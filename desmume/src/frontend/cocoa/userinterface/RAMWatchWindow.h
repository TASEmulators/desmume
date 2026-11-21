/*
	Copyright (C) 2006-2026 DeSmuME team

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

// Cocoa host for RAM Watch. All document logic lives in
// ../../shared/ramwatch_core.cpp, which is also used by the Windows frontend.
// This file is UI only: table view, buttons, sheets, panels and preferences.

#import <Cocoa/Cocoa.h>
#include <stdint.h>

// Post this notification (userInfo: address/size/type NSNumbers, optional
// comment NSString) to add a RAM Search result to the watch list. The upcoming
// RAM Search port uses this instead of depending on the window controller.
extern NSString * const kRAMWatchAddFromSearchNotification;

@class RAMWatchDropView;

@protocol RAMWatchDropViewDelegate <NSObject>
- (void) ramWatchDropView:(RAMWatchDropView *)view didReceiveWatchFile:(NSString *)path;
@end

@interface RAMWatchDropView : NSView
{
	id <RAMWatchDropViewDelegate> _delegate;
}
@property (assign) id <RAMWatchDropViewDelegate> delegate;
@end

@interface RAMWatchWindowController : NSWindowController <NSWindowDelegate,
                                                          NSTableViewDataSource,
                                                          NSTableViewDelegate,
                                                          NSMenuDelegate,
                                                          RAMWatchDropViewDelegate>
{
	NSScrollView *_tableScrollView;
	NSTableView *_tableView;
	NSView *_sidebar;
	NSPopUpButton *_filePopUpButton;
	NSMenu *_fileMenu;
	NSMenuItem *_recentMenuItem;
	NSButton *_newButton;
	NSButton *_editButton;
	NSButton *_removeButton;
	NSButton *_duplicateButton;
	NSButton *_separatorButton;
	NSButton *_moveUpButton;
	NSButton *_moveDownButton;
	NSTextField *_statusLabel;
	RAMWatchDropView *_dropView;
	NSTimer *_refreshTimer;

	// Controls of the currently presented watch editor sheet (MRC).
	NSView *_editorAccessoryView;
	NSTextField *_editorAddressField;
	NSTextField *_editorCommentField;
	NSPopUpButton *_editorSizePopUp;
	NSPopUpButton *_editorTypePopUp;
	NSInteger _editingIndex; // -1 while creating a new watch

	BOOL _didLoadPreferences;
	BOOL _didAttemptAutoLoad;
	NSString *_romFileName;
}

// Base name of the loaded ROM, used to suggest "<ROM>.wch" in Save As.
@property (copy) NSString *romFileName;

// True when Auto-load is enabled and there is a recent list to reopen. The
// application delegate uses this right after launch.
+ (BOOL) shouldAutoLoadOnLaunch;

// Shows the window, starts the refresh timer and applies Auto-load once.
- (void) showRAMWatchWindow;

// Integration point for the upcoming RAM Search port: a search result can be
// turned into a watch without knowing anything about this window. A matching
// NSNotification (@"org.desmume.DeSmuME.addRAMWatch") is also observed.
- (BOOL) addWatchFromSearchResult:(uint32_t)address
                             size:(char)sizeCode
                             type:(char)typeCode
                          comment:(NSString *)comment;

@end
