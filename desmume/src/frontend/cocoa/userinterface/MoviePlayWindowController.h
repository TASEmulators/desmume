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

// Cocoa host for the Play Movie dialog. All decisions (header parsing, ROM /
// core mismatch, pause-frame validation and comment rewriting) are made by
// MovieInfo in src/frontend/shared; this class only owns the widgets, the
// NSOpenPanel used by Browse and the modal session. A GTK host can implement
// the same contract with GtkDialog and reuse the shared layer unchanged.

#import <Cocoa/Cocoa.h>

@class MovieInfoPreview;

@interface MoviePlayWindowController : NSWindowController <NSWindowDelegate>
{
	NSTextField *pathField;
	NSButton *readOnlyButton;
	NSButton *refreshButton;
	NSButton *browseButton;
	NSButton *pauseCheck;
	NSTextField *pauseField;
	NSButton *okButton;
	MovieInfoPreview *preview;

	NSButton *radioPower;
	NSButton *radioSram;
	NSButton *radioSnapshot;
	NSTextField *firmwareField;
	NSTextField *languageField;
	NSTextField *nicknameField;
	NSButton *biosExternal;
	NSButton *biosSwi;
	NSButton *firmwareExternal;
	NSButton *bootFromFirmware;

	NSString *defaultMovieName;
	NSString *moviesDirectory;
	NSString *originalDescription;
	NSString *requestPath;
	BOOL requestReadOnly;
	int requestPauseFrame;
	id target;
	SEL action;
}

// Called once before runModal.
- (void)setDefaultMovieName:(NSString *)name;
- (void)setMoviesDirectory:(NSString *)directory;
- (void)setTarget:(id)theTarget action:(SEL)theAction;

- (NSInteger)runModal;

// The values the host needs to actually start playback.
- (NSString *)moviePath;
- (BOOL)readOnly;
- (int)pauseFrame;

@end
