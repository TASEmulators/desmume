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

#import "MovieInfoPreview.h"

#include <stdio.h>
#include <string>

// Pure C++ header: no types.h, so it cannot disturb any Cocoa declaration.
#include "../../shared/movie_info_core.h"

static NSString *NSStr(const std::string &s)
{
	NSString *result = [NSString stringWithUTF8String:s.c_str()];
	return (result != nil) ? result : @"";
}

static NSString *Trimmed(NSString *s)
{
	return [s stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
}

@implementation MovieInfoPreview

- (NSTextField *)newLabelAtX:(CGFloat)x y:(CGFloat)y width:(CGFloat)width title:(NSString *)title
{
	NSTextField *field = [[NSTextField alloc] initWithFrame:NSMakeRect(x, y, width, 16)];
	[field setStringValue:title];
	[field setBezeled:NO];
	[field setDrawsBackground:NO];
	[field setEditable:NO];
	[field setSelectable:NO];
	[field setAlignment:NSRightTextAlignment];
	[field setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
	[view addSubview:field];
	return field;
}

- (NSTextField *)newValueAtX:(CGFloat)x y:(CGFloat)y width:(CGFloat)width
{
	NSTextField *field = [[NSTextField alloc] initWithFrame:NSMakeRect(x, y, width, 16)];
	[field setStringValue:@"-"];
	[field setBezeled:NO];
	[field setDrawsBackground:NO];
	[field setEditable:NO];
	[field setSelectable:YES];
	[field setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
	[field setLineBreakMode:NSLineBreakByTruncatingTail];
	[view addSubview:field];
	return field;
}

- (void)buildView
{
	view = [[NSView alloc] initWithFrame:NSMakeRect(0.0f, 0.0f, 540.0f, 220.0f)];

	NSTextField *title = [self newValueAtX:0.0f y:196.0f width:540.0f];
	[title setStringValue:@"Movie metadata"];
	[title setFont:[NSFont boldSystemFontOfSize:12.0f]];

	// Rows are 20 points apart. The Comments label and its scroll view must
	// stay below the Frames row (they used to overlap it by 8 points).
	[self newLabelAtX:0.0f y:172.0f width:76.0f title:@"Movie ROM"];
	movieROMField = [self newValueAtX:82.0f y:172.0f width:458.0f];

	[self newLabelAtX:0.0f y:152.0f width:76.0f title:@"Your ROM"];
	yourROMField = [self newValueAtX:82.0f y:152.0f width:458.0f];

	[self newLabelAtX:0.0f y:132.0f width:76.0f title:@"Author"];
	authorField = [self newValueAtX:82.0f y:132.0f width:220.0f];
	[self newLabelAtX:310.0f y:132.0f width:60.0f title:@"Recorded"];
	recordDateField = [self newValueAtX:376.0f y:132.0f width:164.0f];

	[self newLabelAtX:0.0f y:112.0f width:76.0f title:@"Core"];
	coreField = [self newValueAtX:82.0f y:112.0f width:458.0f];

	[self newLabelAtX:0.0f y:92.0f width:76.0f title:@"Frames"];
	framesField = [self newValueAtX:82.0f y:92.0f width:100.0f];
	[self newLabelAtX:190.0f y:92.0f width:50.0f title:@"Length"];
	lengthField = [self newValueAtX:246.0f y:92.0f width:110.0f];
	[self newLabelAtX:364.0f y:92.0f width:78.0f title:@"Rerecords"];
	rerecordField = [self newValueAtX:448.0f y:92.0f width:92.0f];

	[self newLabelAtX:0.0f y:56.0f width:76.0f title:@"Comments"];
	NSScrollView *commentsScroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(82.0f, 44.0f, 458.0f, 28.0f)];
	[commentsScroll setBorderType:NSBezelBorder];
	[commentsScroll setHasVerticalScroller:YES];
	commentsView = [[NSTextView alloc] initWithFrame:NSMakeRect(0.0f, 0.0f, 440.0f, 26.0f)];
	[commentsView setEditable:NO];
	[commentsView setSelectable:YES];
	[commentsView setRichText:NO];
	[commentsView setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
	[commentsView setTextContainerInset:NSMakeSize(2.0f, 1.0f)];
	[commentsScroll setDocumentView:commentsView];
	[view addSubview:commentsScroll];
	[commentsScroll release];

	statusField = [self newValueAtX:0.0f y:18.0f width:540.0f];
	[statusField setFont:[NSFont systemFontOfSize:11.0f]];
}

- (id)init
{
	self = [super init];
	if (self == nil)
		return nil;

	lastPath = nil;
	[self buildView];
	[self clearFieldsWithStatus:@"Select a .dsm movie to preview its metadata."];
	return self;
}

- (void)dealloc
{
	[view release];
	[movieROMField release];
	[yourROMField release];
	[authorField release];
	[recordDateField release];
	[coreField release];
	[framesField release];
	[lengthField release];
	[rerecordField release];
	[commentsView release];
	[statusField release];
	[lastPath release];
	[super dealloc];
}

- (NSView *)view
{
	return view;
}

- (void)setField:(NSTextField *)field mismatch:(BOOL)mismatch
{
	[field setTextColor:(mismatch) ? [NSColor redColor] : [NSColor controlTextColor]];
}

- (void)clearFieldsWithStatus:(NSString *)status
{
	[movieROMField setStringValue:@"-"];
	[yourROMField setStringValue:@"-"];
	[authorField setStringValue:@"-"];
	[recordDateField setStringValue:@"-"];
	[coreField setStringValue:@"-"];
	[framesField setStringValue:@"-"];
	[lengthField setStringValue:@"-"];
	[rerecordField setStringValue:@"-"];
	[commentsView setString:@""];
	[self setField:movieROMField mismatch:NO];
	[self setField:yourROMField mismatch:NO];
	[self setField:coreField mismatch:NO];
	[statusField setStringValue:status];
	[statusField setTextColor:[NSColor controlTextColor]];
}

- (void)previewFileURL:(NSURL *)url
{
	if (url == nil || [url isFileURL] == NO)
	{
		[lastPath release];
		lastPath = nil;
		[self clearFieldsWithStatus:@"Select a .dsm movie to preview its metadata."];
		return;
	}

	NSString *path = [url path];
	if (lastPath != nil && [lastPath isEqualToString:path])
		return;  // the open panel fires this for every UI change, not just files
	[lastPath release];
	lastPath = [path copy];

	MovieInfo::Header hdr;
	if (!MovieInfo::ReadHeader(std::string([path UTF8String]), hdr))
	{
		[self clearFieldsWithStatus:@"Not a readable DeSmuME movie (DSM version 1 or 2)."];
		return;
	}

	const MovieInfo::RomIdentity rom = MovieInfo::CurrentRomIdentity();
	const MovieInfo::Assessment a = MovieInfo::Assess(hdr, rom, MovieInfo::CurrentCoreVersion());

	std::string author, description;
	MovieInfo::SplitComments(hdr, author, description);

	const std::string movieName = MovieInfo::BaseName(hdr.romFilename);
	const std::string movieCrc = a.crcComparable
	                            ? hdr.romChecksum : std::string("n/a (legacy)");
	const std::string movieLine = MovieInfo::FormatRomLine("",
	                                hdr.romSerial.empty() ? "-" : hdr.romSerial, movieCrc,
	                                "name=", movieName.empty() ? "-" : MovieInfo::Shorten(movieName, 32));
	const std::string yourLine = MovieInfo::FormatRomLine("",
	                               rom.serial.empty() ? "-" : rom.serial,
	                               MovieInfo::FormatHexCrc(rom.crc), "title=", rom.internalName);

	[movieROMField setStringValue:Trimmed(NSStr(movieLine))];
	[yourROMField setStringValue:Trimmed(NSStr(yourLine))];
	[authorField setStringValue:NSStr(author.empty() ? std::string("-") : author)];
	[commentsView setString:NSStr(description)];
	[recordDateField setStringValue:NSStr(MovieInfo::FormatRecordDate(hdr.recordDate))];
	[coreField setStringValue:NSStr(MovieInfo::CoreVersionText(hdr, a))];
	[lengthField setStringValue:NSStr(MovieInfo::FormatLength(hdr.frames))];

	char buf[64];
	snprintf(buf, sizeof(buf), "%u", hdr.frames);
	[framesField setStringValue:[NSString stringWithUTF8String:buf]];
	snprintf(buf, sizeof(buf), "%u", MovieInfo::ParseUInt(hdr.rerecordCount));
	[rerecordField setStringValue:[NSString stringWithUTF8String:buf]];

	[self setField:movieROMField mismatch:a.romMismatch];
	[self setField:yourROMField mismatch:a.romMismatch];
	[self setField:coreField mismatch:a.coreMismatch];

	if (a.romMismatch || a.coreMismatch)
	{
		[statusField setStringValue:@"ROM or core mismatch - desync expected."];
		[statusField setTextColor:[NSColor redColor]];
	}
	else
	{
		[statusField setStringValue:@"Ready to play."];
		[statusField setTextColor:[NSColor controlTextColor]];
	}
}

- (NSString *)comments
{
	return [commentsView string];
}

- (void)setCommentsEditable:(BOOL)editable
{
	[commentsView setEditable:editable];
	[commentsView setSelectable:YES];
	[[commentsView enclosingScrollView] setBorderType:(editable) ? NSBezelBorder : NSLineBorder];
}

@end
