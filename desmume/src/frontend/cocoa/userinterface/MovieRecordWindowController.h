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

// Cocoa host for the Record Movie dialog, mirroring Windows IDD_RECORDMOVIE.
// File-name generation, extension handling and the current emulation flags
// come from shared/movie_info_core; the host only supplies widgets, the two
// file panels and preference/path persistence. A GTK host can do the same
// with a GtkDialog.

#import <Cocoa/Cocoa.h>

@interface MovieRecordWindowController : NSWindowController <NSWindowDelegate>
{
	NSTextField *pathField;
	NSButton *browseButton;
	NSTextField *authorField;
	NSTextField *commentField;
	NSDatePicker *datePicker;
	NSDatePicker *timePicker;
	NSTextField *romInfoField;

	NSButton *radioPower;
	NSButton *radioSram;
	NSButton *radioSnapshot;
	NSTextField *sramField;
	NSButton *sramBrowseButton;
	NSButton *biosExternal;
	NSButton *biosSwi;
	NSButton *firmwareExternal;
	NSButton *bootFromFirmware;
	NSButton *okButton;

	NSString *defaultMovieName;
	NSString *moviesDirectory;
	id target;
	SEL action;
}

- (void)setDefaultMovieName:(NSString *)name;
- (void)setMoviesDirectory:(NSString *)directory;
- (void)setTarget:(id)theTarget action:(SEL)theAction;

- (NSInteger)runModal;

- (NSString *)moviePath;
- (NSString *)author;
- (NSString *)descriptionText;
- (NSString *)sramPath;
- (int)startFrom;
- (NSDate *)rtcDate;

@end
