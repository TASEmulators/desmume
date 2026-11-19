/*
	Copyright (C) 2008-2026 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
*/

#import "MoviePlayWindowController.h"
#import "MovieInfoPreview.h"

#include <stdio.h>
#include <string>

#include "../../shared/movie_info_core.h"

static NSString *NSStr(const std::string &s)
{
	NSString *result = [NSString stringWithUTF8String:s.c_str()];
	return (result != nil) ? result : @"";
}

@implementation MoviePlayWindowController

#pragma mark - Widget helpers

- (NSTextField *)labelAt:(NSRect)frame text:(NSString *)text right:(BOOL)right
{
	NSTextField *field = [[NSTextField alloc] initWithFrame:frame];
	[field setStringValue:text];
	[field setBezeled:NO];
	[field setDrawsBackground:NO];
	[field setEditable:NO];
	[field setSelectable:NO];
	[field setAlignment:(right) ? NSRightTextAlignment : NSLeftTextAlignment];
	[field setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
	[[[self window] contentView] addSubview:field];
	return field;
}

- (NSTextField *)fieldAt:(NSRect)frame
{
	NSTextField *field = [[NSTextField alloc] initWithFrame:frame];
	[field setBezeled:YES];
	[field setDrawsBackground:YES];
	[field setEditable:YES];
	[field setSelectable:YES];
	[field setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
	[[[self window] contentView] addSubview:field];
	return field;
}

- (NSButton *)buttonAt:(NSRect)frame title:(NSString *)title checkbox:(BOOL)checkbox
{
	NSButton *button = [[NSButton alloc] initWithFrame:frame];
	[button setTitle:title];
	[button setButtonType:(checkbox) ? NSSwitchButton : NSRadioButton];
	[button setBezelStyle:NSRoundedBezelStyle];
	[button setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
	[button setState:NSOffState];
	[[[self window] contentView] addSubview:button];
	return button;
}

- (NSBox *)groupAt:(NSRect)frame title:(NSString *)title
{
	NSBox *box = [[NSBox alloc] initWithFrame:frame];
	[box setTitle:title];
	[box setBoxType:NSBoxPrimary];
	[box setTitlePosition:NSAtTop];
	[[[self window] contentView] addSubview:box];
	[box release];
	return box;
}

#pragma mark - Construction

- (id)init
{
	NSWindow *window = [[[NSWindow alloc] initWithContentRect:NSMakeRect(0.0f, 0.0f, 620.0f, 520.0f)
	                                                styleMask:NSTitledWindowMask | NSClosableWindowMask
	                                                  backing:NSBackingStoreBuffered
	                                                    defer:NO] autorelease];
	self = [super initWithWindow:window];
	if (self == nil)
		return nil;

	[window setTitle:@"Play Movie"];
	[window setDelegate:self];

	NSView *content = [window contentView];

	// Movie file row, matching the Windows dialog's layout order.
	[self labelAt:NSMakeRect(8.0f, 490.0f, 70.0f, 16.0f) text:@"Movie file:" right:YES];
	pathField = [self fieldAt:NSMakeRect(84.0f, 488.0f, 400.0f, 19.0f)];
	refreshButton = [[NSButton alloc] initWithFrame:NSMakeRect(490.0f, 487.0f, 60.0f, 21.0f)];
	[refreshButton setTitle:@"Refresh"];
	[refreshButton setBezelStyle:NSRoundedBezelStyle];
	[refreshButton setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
	[refreshButton setTarget:self];
	[refreshButton setAction:@selector(refresh:)];
	[content addSubview:refreshButton];
	browseButton = [[NSButton alloc] initWithFrame:NSMakeRect(550.0f, 487.0f, 61.0f, 21.0f)];
	[browseButton setTitle:@"Browse"];
	[browseButton setBezelStyle:NSRoundedBezelStyle];
	[browseButton setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
	[browseButton setTarget:self];
	[browseButton setAction:@selector(browse:)];
	[content addSubview:browseButton];

	readOnlyButton = [self buttonAt:NSMakeRect(84.0f, 464.0f, 200.0f, 18.0f)
	                          title:@"Open read-only" checkbox:YES];
	[readOnlyButton setState:NSOnState];
	[readOnlyButton setTarget:self];
	[readOnlyButton setAction:@selector(readOnlyChanged:)];

	// Metadata area: the shared core fills every field, including the two
	// red mismatch highlights. The Comment text view is editable when the
	// movie is opened read-write.
	preview = [[MovieInfoPreview alloc] init];
	[[preview view] setFrame:NSMakeRect(10.0f, 240.0f, 600.0f, 220.0f)];
	[content addSubview:[preview view]];

	pauseCheck = [self buttonAt:NSMakeRect(84.0f, 224.0f, 120.0f, 18.0f)
	                      title:@"Pause at frame:" checkbox:YES];
	[pauseCheck setTarget:self];
	[pauseCheck setAction:@selector(pauseCheckChanged:)];
	pauseField = [self fieldAt:NSMakeRect(210.0f, 223.0f, 80.0f, 19.0f)];
	[pauseField setEnabled:NO];

	// Recording options (read-only in playback).
	[self groupAt:NSMakeRect(10.0f, 98.0f, 180.0f, 112.0f) title:@"Recording options"];
	radioPower = [self buttonAt:NSMakeRect(20.0f, 175.0f, 160.0f, 18.0f)
	                      title:@"From power on" checkbox:NO];
	radioSram = [self buttonAt:NSMakeRect(20.0f, 152.0f, 160.0f, 18.0f)
	                     title:@"From reset/SRAM" checkbox:NO];
	radioSnapshot = [self buttonAt:NSMakeRect(20.0f, 129.0f, 160.0f, 18.0f)
	                         title:@"From snapshot" checkbox:NO];
	[radioPower setEnabled:NO];
	[radioSram setEnabled:NO];
	[radioSnapshot setEnabled:NO];

	// Console / firmware.
	[self groupAt:NSMakeRect(200.0f, 98.0f, 200.0f, 112.0f) title:@"Console / firmware"];
	[self labelAt:NSMakeRect(206.0f, 175.0f, 62.0f, 16.0f) text:@"System:" right:YES];
	[self labelAt:NSMakeRect(272.0f, 175.0f, 120.0f, 16.0f) text:@"Nintendo DS" right:NO];
	[self labelAt:NSMakeRect(206.0f, 153.0f, 62.0f, 16.0f) text:@"Firmware:" right:YES];
	firmwareField = [self labelAt:NSMakeRect(272.0f, 153.0f, 120.0f, 16.0f) text:@"-" right:NO];
	[self labelAt:NSMakeRect(206.0f, 131.0f, 62.0f, 16.0f) text:@"Language:" right:YES];
	languageField = [self labelAt:NSMakeRect(272.0f, 131.0f, 120.0f, 16.0f) text:@"-" right:NO];
	[self labelAt:NSMakeRect(206.0f, 109.0f, 62.0f, 16.0f) text:@"Nickname:" right:YES];
	nicknameField = [self labelAt:NSMakeRect(272.0f, 109.0f, 120.0f, 16.0f) text:@"-" right:NO];

	// BIOS flags.
	[self groupAt:NSMakeRect(410.0f, 98.0f, 200.0f, 112.0f) title:@"BIOS settings"];
	biosExternal = [self buttonAt:NSMakeRect(418.0f, 175.0f, 184.0f, 18.0f)
	                         title:@"External BIOS" checkbox:YES];
	biosSwi = [self buttonAt:NSMakeRect(418.0f, 153.0f, 184.0f, 18.0f)
	                    title:@"SWI from BIOS" checkbox:YES];
	firmwareExternal = [self buttonAt:NSMakeRect(418.0f, 131.0f, 184.0f, 18.0f)
	                              title:@"External firmware" checkbox:YES];
	bootFromFirmware = [self buttonAt:NSMakeRect(418.0f, 109.0f, 184.0f, 18.0f)
	                              title:@"Boot from firmware" checkbox:YES];
	[biosExternal setEnabled:NO];
	[biosSwi setEnabled:NO];
	[firmwareExternal setEnabled:NO];
	[bootFromFirmware setEnabled:NO];

	[self labelAt:NSMakeRect(10.0f, 46.0f, 470.0f, 42.0f)
	         text:@"Press OK to start playback.\nNote: this will erase or overwrite the game's SRAM,\nand may change other emulator settings."
	        right:NO];

	okButton = [[NSButton alloc] initWithFrame:NSMakeRect(490.0f, 20.0f, 60.0f, 22.0f)];
	[okButton setTitle:@"OK"];
	[okButton setBezelStyle:NSRoundedBezelStyle];
	[okButton setTarget:self];
	[okButton setAction:@selector(accept:)];
	[content addSubview:okButton];
	NSButton *cancelButton = [[NSButton alloc] initWithFrame:NSMakeRect(556.0f, 20.0f, 55.0f, 22.0f)];
	[cancelButton setTitle:@"Cancel"];
	[cancelButton setBezelStyle:NSRoundedBezelStyle];
	[cancelButton setTarget:self];
	[cancelButton setAction:@selector(cancel:)];
	[content addSubview:cancelButton];
	[cancelButton release];

	[window setDefaultButtonCell:[okButton cell]];
	return self;
}

- (void)dealloc
{
	[pathField release];
	[readOnlyButton release];
	[refreshButton release];
	[browseButton release];
	[pauseCheck release];
	[pauseField release];
	[okButton release];
	[preview release];
	[radioPower release];
	[radioSram release];
	[radioSnapshot release];
	[firmwareField release];
	[languageField release];
	[nicknameField release];
	[biosExternal release];
	[biosSwi release];
	[firmwareExternal release];
	[bootFromFirmware release];
	[defaultMovieName release];
	[moviesDirectory release];
	[originalDescription release];
	[requestPath release];
	[super dealloc];
}

#pragma mark - Configuration

- (void)setDefaultMovieName:(NSString *)name
{
	[defaultMovieName release];
	defaultMovieName = [name copy];
	[pathField setStringValue:[defaultMovieName stringByAppendingPathExtension:@"dsm"]];
}

- (void)setMoviesDirectory:(NSString *)directory
{
	[moviesDirectory release];
	moviesDirectory = [directory copy];
}

- (void)setTarget:(id)theTarget action:(SEL)theAction
{
	target = theTarget;
	action = theAction;
}

- (NSString *)moviePath
{
	NSString *text = [pathField stringValue];
	if ([text length] == 0)
		return nil;

	if ([[text pathExtension] length] == 0)
		text = [text stringByAppendingPathExtension:@"dsm"];

	// A bare name is resolved against the Movies directory, just like the
	// Windows dialog resolves against File > Path Settings > Movies.
	if ([text rangeOfString:@"/"].location == NSNotFound)
	{
		NSString *directory = moviesDirectory;
		if ([directory length] == 0)
			directory = [NSHomeDirectory() stringByAppendingPathComponent:@"Movies"];
		text = [directory stringByAppendingPathComponent:text];
	}
	return text;
}

- (BOOL)readOnly
{
	return requestReadOnly;
}

- (int)pauseFrame
{
	return requestPauseFrame;
}

#pragma mark - Preview refresh

- (void)fillGroupsFromHeader:(const MovieInfo::Header &)hdr
{
	const BOOL fromSnapshot = (hdr.savestate == "1");
	const BOOL fromSram = (!fromSnapshot && hdr.hasSram);
	[radioPower setState:(!fromSnapshot && !fromSram) ? NSOnState : NSOffState];
	[radioSram setState:(fromSram) ? NSOnState : NSOffState];
	[radioSnapshot setState:(fromSnapshot) ? NSOnState : NSOffState];

	[firmwareField setStringValue:NSStr(MovieInfo::DescribeFirmware(hdr.useExtFirmware))];
	[languageField setStringValue:NSStr(MovieInfo::DescribeLanguage(hdr.firmLanguage))];
	[nicknameField setStringValue:NSStr(hdr.firmNickname.empty() ? std::string("-") : hdr.firmNickname)];

	[biosExternal setState:(atoi(hdr.useExtBios.c_str()) != 0) ? NSOnState : NSOffState];
	[biosSwi setState:(atoi(hdr.swiFromBios.c_str()) != 0) ? NSOnState : NSOffState];
	[firmwareExternal setState:(atoi(hdr.useExtFirmware.c_str()) != 0) ? NSOnState : NSOffState];
	[bootFromFirmware setState:(atoi(hdr.bootFromFirmware.c_str()) != 0) ? NSOnState : NSOffState];
}

- (void)clearGroups
{
	[radioPower setState:NSOffState];
	[radioSram setState:NSOffState];
	[radioSnapshot setState:NSOffState];
	[firmwareField setStringValue:@"-"];
	[languageField setStringValue:@"-"];
	[nicknameField setStringValue:@"-"];
	[biosExternal setState:NSOffState];
	[biosSwi setState:NSOffState];
	[firmwareExternal setState:NSOffState];
	[bootFromFirmware setState:NSOffState];
}

- (IBAction)refresh:(id)sender
{
	NSString *path = [self moviePath];
	NSFileManager *files = [NSFileManager defaultManager];
	const BOOL readable = (path != nil) && [files isReadableFileAtPath:path];
	const BOOL writable = (path != nil) && [files isWritableFileAtPath:path];

	[okButton setEnabled:readable];
	if (!writable)
	{
		[readOnlyButton setState:NSOnState];
		[readOnlyButton setEnabled:NO];
	}
	else
	{
		[readOnlyButton setEnabled:YES];
	}
	[preview setCommentsEditable:([readOnlyButton state] != NSOnState)];
	[preview previewFileURL:(readable) ? [NSURL fileURLWithPath:path] : nil];

	[originalDescription release];
	originalDescription = [[preview comments] copy];

	if (!readable)
	{
		[self clearGroups];
		return;
	}

	MovieInfo::Header hdr;
	if (!MovieInfo::ReadHeader(std::string([path UTF8String]), hdr))
	{
		[self clearGroups];
		return;
	}
	[self fillGroupsFromHeader:hdr];
}

#pragma mark - Actions

- (IBAction)browse:(id)sender
{
	NSOpenPanel *panel = [NSOpenPanel openPanel];
	[panel setCanChooseFiles:YES];
	[panel setCanChooseDirectories:NO];
	[panel setAllowsMultipleSelection:NO];
	[panel setTitle:@"Play Movie from File"];
	[panel setAllowedFileTypes:[NSArray arrayWithObject:@"dsm"]];

	if ([panel runModal] != NSOKButton)
		return;

	NSURL *selected = [[panel URLs] lastObject];
	if (selected == nil)
		return;
	[pathField setStringValue:[selected path]];
	[self refresh:self];
}

- (IBAction)readOnlyChanged:(id)sender
{
	[preview setCommentsEditable:([readOnlyButton state] != NSOnState)];
}

- (IBAction)pauseCheckChanged:(id)sender
{
	const BOOL enabled = ([pauseCheck state] == NSOnState);
	[pauseField setEnabled:enabled];
	if (!enabled)
	{
		[pauseField setStringValue:@""];
		return;
	}

	NSString *path = [self moviePath];
	MovieInfo::Header hdr;
	if (path != nil && MovieInfo::ReadHeader(std::string([path UTF8String]), hdr))
	{
		char text[32];
		snprintf(text, sizeof(text), "%u", hdr.frames);
		[pauseField setStringValue:[NSString stringWithUTF8String:text]];
	}
}

- (IBAction)accept:(id)sender
{
	NSString *path = [self moviePath];
	if (path == nil || ![[NSFileManager defaultManager] isReadableFileAtPath:path])
		return;

	const BOOL readOnly = ([readOnlyButton state] == NSOnState);
	const std::string cppPath([path UTF8String]);

	// Only the comment lines are rewritten; the author line, unknown keys and
	// every frame byte are preserved by the shared core.
	if (!readOnly)
	{
		NSString *description = [preview comments];
		if (![description isEqualToString:originalDescription])
		{
			MovieInfo::Header hdr;
			if (MovieInfo::ReadHeader(cppPath, hdr) &&
			    !MovieInfo::WriteComments(cppPath, hdr, std::string([description UTF8String])))
			{
				NSRunAlertPanel(@"Play Movie",
				                @"The comment could not be saved. Playback will continue with the original file.",
				                @"OK", nil, nil);
			}
		}
	}

	[requestPath release];
	requestPath = [path copy];
	requestReadOnly = readOnly;
	requestPauseFrame = MovieInfo::ParsePauseFrame(std::string([[pauseField stringValue] UTF8String]),
	                                                [pauseCheck state] == NSOnState);

	if (target != nil && action != NULL)
		[target performSelector:action withObject:self];

	[NSApp stopModalWithCode:NSOKButton];
	[[self window] close];
}

- (IBAction)cancel:(id)sender
{
	if ([NSApp modalWindow] == [self window])
		[NSApp stopModalWithCode:NSCancelButton];
	[[self window] close];
}

- (BOOL)windowShouldClose:(id)sender
{
	// Stop the modal session here too, so the red close button behaves like
	// Cancel. Do not call -close from here: that would recurse.
	if ([NSApp modalWindow] == [self window])
		[NSApp stopModalWithCode:NSCancelButton];
	return YES;
}

- (NSInteger)runModal
{
	[self refresh:self];
	[self showWindow:self];
	return [NSApp runModalForWindow:[self window]];
}

@end
