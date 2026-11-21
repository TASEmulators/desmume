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

#import "MovieRecordWindowController.h"

#include <string>

#include "../../shared/movie_info_core.h"

static NSString *NSStr(const std::string &s)
{
	NSString *result = [NSString stringWithUTF8String:s.c_str()];
	return (result != nil) ? result : @"";
}

@implementation MovieRecordWindowController

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

- (NSDatePicker *)datePickerAt:(NSRect)frame date:(NSDate *)date timeOnly:(BOOL)timeOnly
{
	NSDatePicker *picker = [[NSDatePicker alloc] initWithFrame:frame];
	[picker setDatePickerStyle:NSTextFieldDatePickerStyle];
	[picker setDatePickerElements:(timeOnly) ? NSHourMinuteDatePickerElementFlag
	                                          : NSYearMonthDayDatePickerElementFlag];
	[picker setDateValue:date];
	[[[self window] contentView] addSubview:picker];
	return picker;
}

#pragma mark - Construction

- (NSDate *)dateFromStamp:(const MovieInfo::RTCStamp &)stamp
{
	NSDateComponents *parts = [[[NSDateComponents alloc] init] autorelease];
	[parts setYear:stamp.year];
	[parts setMonth:stamp.month];
	[parts setDay:stamp.day];
	[parts setHour:stamp.hour];
	[parts setMinute:stamp.minute];
	[parts setSecond:stamp.second];
	return [[NSCalendar currentCalendar] dateFromComponents:parts];
}

- (id)init
{
	NSWindow *window = [[[NSWindow alloc] initWithContentRect:NSMakeRect(0.0f, 0.0f, 620.0f, 360.0f)
	                                                styleMask:NSTitledWindowMask | NSClosableWindowMask
	                                                  backing:NSBackingStoreBuffered
	                                                    defer:NO] autorelease];
	self = [super initWithWindow:window];
	if (self == nil)
		return nil;

	[window setTitle:@"Record Movie"];
	[window setDelegate:self];

	[self labelAt:NSMakeRect(8.0f, 326.0f, 70.0f, 16.0f) text:@"Movie file:" right:YES];
	pathField = [self fieldAt:NSMakeRect(84.0f, 324.0f, 460.0f, 19.0f)];
	browseButton = [[NSButton alloc] initWithFrame:NSMakeRect(550.0f, 323.0f, 61.0f, 21.0f)];
	[browseButton setTitle:@"Browse"];
	[browseButton setBezelStyle:NSRoundedBezelStyle];
	[browseButton setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
	[browseButton setTarget:self];
	[browseButton setAction:@selector(browseMovie:)];
	[[window contentView] addSubview:browseButton];

	[self labelAt:NSMakeRect(8.0f, 298.0f, 70.0f, 16.0f) text:@"Author:" right:YES];
	authorField = [self fieldAt:NSMakeRect(84.0f, 296.0f, 527.0f, 19.0f)];
	// Match Windows, which seeds the DSM author with GetUserNameW() (the
	// account/login name), not the user's full name. NSFullUserName() would
	// give a different value on macOS (and can carry a leading space).
	[authorField setStringValue:[[NSUserName() description] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]]];

	[self labelAt:NSMakeRect(8.0f, 272.0f, 70.0f, 16.0f) text:@"Comment:" right:YES];
	commentField = [self fieldAt:NSMakeRect(84.0f, 270.0f, 527.0f, 19.0f)];

	[self labelAt:NSMakeRect(8.0f, 246.0f, 70.0f, 16.0f) text:@"Start RTC:" right:YES];
	NSDate *defaultDate = [self dateFromStamp:MovieInfo::DefaultRTCStamp()];
	datePicker = [self datePickerAt:NSMakeRect(84.0f, 243.0f, 200.0f, 22.0f) date:defaultDate timeOnly:NO];
	timePicker = [self datePickerAt:NSMakeRect(292.0f, 243.0f, 120.0f, 22.0f) date:defaultDate timeOnly:YES];

	romInfoField = [self labelAt:NSMakeRect(84.0f, 219.0f, 527.0f, 16.0f) text:@"" right:NO];
	const MovieInfo::RomIdentity rom = MovieInfo::CurrentRomIdentity();
	[romInfoField setStringValue:NSStr(MovieInfo::FormatRomLine("Your ROM:  ",
	                                    rom.serial, MovieInfo::FormatHexCrc(rom.crc),
	                                    "title=", rom.internalName))];

	[self groupAt:NSMakeRect(8.0f, 88.0f, 290.0f, 120.0f) title:@"Recording options"];
	radioPower = [self buttonAt:NSMakeRect(18.0f, 170.0f, 270.0f, 18.0f)
	                      title:@"From power on (cold boot)" checkbox:NO];
	radioSram = [self buttonAt:NSMakeRect(18.0f, 146.0f, 270.0f, 18.0f)
	                     title:@"From reset / SRAM" checkbox:NO];
	radioSnapshot = [self buttonAt:NSMakeRect(18.0f, 122.0f, 270.0f, 18.0f)
	                         title:@"From current state (snapshot)" checkbox:NO];
	[radioPower setState:NSOnState];
	[radioPower setTarget:self];
	[radioPower setAction:@selector(startFromChanged:)];
	[radioSram setTarget:self];
	[radioSram setAction:@selector(startFromChanged:)];
	[radioSnapshot setTarget:self];
	[radioSnapshot setAction:@selector(startFromChanged:)];

	[self labelAt:NSMakeRect(18.0f, 98.0f, 40.0f, 16.0f) text:@"SRAM:" right:YES];
	sramField = [self fieldAt:NSMakeRect(62.0f, 96.0f, 192.0f, 19.0f)];
	sramBrowseButton = [[NSButton alloc] initWithFrame:NSMakeRect(260.0f, 95.0f, 28.0f, 21.0f)];
	[sramBrowseButton setTitle:@"…"];
	[sramBrowseButton setBezelStyle:NSRoundedBezelStyle];
	[sramBrowseButton setTarget:self];
	[sramBrowseButton setAction:@selector(browseSram:)];
	[[window contentView] addSubview:sramBrowseButton];

	[self groupAt:NSMakeRect(308.0f, 88.0f, 304.0f, 120.0f) title:@"Emulation settings (read-only)"];
	const MovieInfo::EmulationFlags flags = MovieInfo::CurrentEmulationFlags();
	biosExternal = [self buttonAt:NSMakeRect(318.0f, 170.0f, 284.0f, 18.0f)
	                         title:@"External BIOS" checkbox:YES];
	biosSwi = [self buttonAt:NSMakeRect(318.0f, 146.0f, 284.0f, 18.0f)
	                    title:@"SWI from BIOS" checkbox:YES];
	firmwareExternal = [self buttonAt:NSMakeRect(318.0f, 122.0f, 284.0f, 18.0f)
	                              title:@"External firmware" checkbox:YES];
	bootFromFirmware = [self buttonAt:NSMakeRect(318.0f, 98.0f, 284.0f, 18.0f)
	                              title:@"Boot from firmware" checkbox:YES];
	[biosExternal setState:(flags.useExtBios) ? NSOnState : NSOffState];
	[biosSwi setState:(flags.swiFromBios) ? NSOnState : NSOffState];
	[firmwareExternal setState:(flags.useExtFirmware) ? NSOnState : NSOffState];
	[bootFromFirmware setState:(flags.bootFromFirmware) ? NSOnState : NSOffState];
	[biosExternal setEnabled:NO];
	[biosSwi setEnabled:NO];
	[firmwareExternal setEnabled:NO];
	[bootFromFirmware setEnabled:NO];

	[self labelAt:NSMakeRect(8.0f, 58.0f, 500.0f, 16.0f)
	         text:@"OK starts recording. The movie stores the current emulation settings." right:NO];

	okButton = [[NSButton alloc] initWithFrame:NSMakeRect(490.0f, 16.0f, 60.0f, 22.0f)];
	[okButton setTitle:@"OK"];
	[okButton setBezelStyle:NSRoundedBezelStyle];
	[okButton setTarget:self];
	[okButton setAction:@selector(accept:)];
	[[window contentView] addSubview:okButton];
	NSButton *cancelButton = [[NSButton alloc] initWithFrame:NSMakeRect(556.0f, 16.0f, 55.0f, 22.0f)];
	[cancelButton setTitle:@"Cancel"];
	[cancelButton setBezelStyle:NSRoundedBezelStyle];
	[cancelButton setTarget:self];
	[cancelButton setAction:@selector(cancel:)];
	[[window contentView] addSubview:cancelButton];
	[cancelButton release];

	[window setDefaultButtonCell:[okButton cell]];
	[self ensureMoviesDirectory];
	[self startFromChanged:nil];
	return self;
}

- (void)dealloc
{
	[pathField release];
	[browseButton release];
	[authorField release];
	[commentField release];
	[datePicker release];
	[timePicker release];
	[romInfoField release];
	[radioPower release];
	[radioSram release];
	[radioSnapshot release];
	[sramField release];
	[sramBrowseButton release];
	[biosExternal release];
	[biosSwi release];
	[firmwareExternal release];
	[bootFromFirmware release];
	[okButton release];
	[defaultMovieName release];
	[moviesDirectory release];
	[super dealloc];
}

#pragma mark - Configuration

- (void)setDefaultMovieName:(NSString *)name
{
	[defaultMovieName release];
	defaultMovieName = [name copy];
	NSString *directory = ([moviesDirectory length] > 0)
	                        ? moviesDirectory
	                        : [NSHomeDirectory() stringByAppendingPathComponent:@"Movies"];
	const std::string leaf = MovieInfo::DefaultMovieFileName(
	                              std::string([defaultMovieName UTF8String]),
	                              std::string([directory UTF8String]), true);
	[pathField setStringValue:NSStr(leaf)];
	[self updateOK];
}

- (void)setMoviesDirectory:(NSString *)directory
{
	[moviesDirectory release];
	moviesDirectory = [directory copy];
	[self ensureMoviesDirectory];
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
	text = NSStr(MovieInfo::EnsureDsmExtension(std::string([text UTF8String])));

	if ([text rangeOfString:@"/"].location == NSNotFound)
	{
		NSString *directory = ([moviesDirectory length] > 0)
		                        ? moviesDirectory
		                        : [NSHomeDirectory() stringByAppendingPathComponent:@"Movies"];
		text = [directory stringByAppendingPathComponent:text];
	}
	return text;
}

- (NSString *)author
{
	return [authorField stringValue];
}

- (NSString *)descriptionText
{
	return [commentField stringValue];
}

- (NSString *)sramPath
{
	return ([radioSram state] == NSOnState) ? [sramField stringValue] : @"";
}

- (int)startFrom
{
	if ([radioSram state] == NSOnState)
		return MovieInfo::kStartSram;
	if ([radioSnapshot state] == NSOnState)
		return MovieInfo::kStartSavestate;
	return MovieInfo::kStartBlank;
}

- (NSDate *)rtcDate
{
	NSCalendar *calendar = [NSCalendar currentCalendar];
	NSCalendarUnit units = NSCalendarUnitYear | NSCalendarUnitMonth | NSCalendarUnitDay;
	NSDateComponents *parts = [calendar components:units fromDate:[datePicker dateValue]];
	units = NSCalendarUnitHour | NSCalendarUnitMinute | NSCalendarUnitSecond;
	NSDateComponents *time = [calendar components:units fromDate:[timePicker dateValue]];
	[parts setHour:[time hour]];
	[parts setMinute:[time minute]];
	[parts setSecond:[time second]];
	return [calendar dateFromComponents:parts];
}

#pragma mark - Actions

- (void)ensureMoviesDirectory
{
	NSString *directory = ([moviesDirectory length] > 0)
	                        ? moviesDirectory
	                        : [NSHomeDirectory() stringByAppendingPathComponent:@"Movies"];
	NSFileManager *files = [NSFileManager defaultManager];
	BOOL isDirectory = NO;
	if ([files fileExistsAtPath:directory isDirectory:&isDirectory] && isDirectory)
		return;
	[files createDirectoryAtPath:directory withIntermediateDirectories:YES
	                 attributes:nil error:NULL];
}

// One predicate shared by the OK button and -accept:, so the button can never
// look enabled while -accept: silently refuses to record.
- (BOOL)canWriteMovieAtPath:(NSString *)path
{
	if ([path length] == 0)
		return NO;
	NSFileManager *files = [NSFileManager defaultManager];
	NSString *parent = [path stringByDeletingLastPathComponent];
	BOOL isDirectory = NO;
	if (![files fileExistsAtPath:parent isDirectory:&isDirectory] || !isDirectory)
		return NO;
	if (![files isWritableFileAtPath:parent])
		return NO;
	if ([files fileExistsAtPath:path] && ![files isWritableFileAtPath:path])
		return NO;
	return YES;
}

- (void)updateOK
{
	[okButton setEnabled:[self canWriteMovieAtPath:[self moviePath]]];
}

- (IBAction)startFromChanged:(id)sender
{
	const BOOL useSram = ([radioSram state] == NSOnState);
	[sramField setEnabled:useSram];
	[sramBrowseButton setEnabled:useSram];
}

- (IBAction)browseMovie:(id)sender
{
	NSSavePanel *panel = [NSSavePanel savePanel];
	[panel setTitle:@"Record a new movie"];
	[panel setAllowedFileTypes:[NSArray arrayWithObject:@"dsm"]];
	NSString *path = [self moviePath];
	if ([path length] > 0)
		[panel setNameFieldStringValue:[path lastPathComponent]];

	if ([panel runModal] != NSOKButton)
		return;
	[pathField setStringValue:[[panel URL] path]];
	[self updateOK];
}

- (IBAction)browseSram:(id)sender
{
	NSOpenPanel *panel = [NSOpenPanel openPanel];
	[panel setTitle:@"Choose SRAM"];
	[panel setAllowedFileTypes:[NSArray arrayWithObject:@"dsv"]];
	if ([panel runModal] != NSOKButton)
		return;
	[sramField setStringValue:[[[panel URLs] lastObject] path]];
}

- (IBAction)accept:(id)sender
{
	NSString *path = [self moviePath];
	if (![self canWriteMovieAtPath:path])
		return;

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
	if ([NSApp modalWindow] == [self window])
		[NSApp stopModalWithCode:NSCancelButton];
	return YES;
}

- (NSInteger)runModal
{
	[self updateOK];
	[self showWindow:self];
	return [NSApp runModalForWindow:[self window]];
}

@end
