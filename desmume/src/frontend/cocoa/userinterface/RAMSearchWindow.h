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

// Cocoa host for RAM Search (core feature set). The search engine lives in
// ../../shared/ram_search_core.cpp. Advanced operations from the Windows
// dialog (Eliminate, Poke, Undo/Redo, Auto-search, Add Cheat) are intentionally
// left for a later pass.

#import <Cocoa/Cocoa.h>

@interface RAMSearchWindowController : NSWindowController <NSWindowDelegate,
                                                          NSTableViewDataSource,
                                                          NSTableViewDelegate>
{
	NSScrollView *_tableScrollView;
	NSTableView *_tableView;
	NSView *_sidebar;

	NSPopUpButton *_searchTypePopUp;
	NSPopUpButton *_operatorPopUp;
	NSPopUpButton *_sizePopUp;
	NSPopUpButton *_displayPopUp;
	NSTextField *_valueField;
	NSTextField *_parameterField;
	NSButton *_misalignButton;
	NSButton *_littleEndianButton;
	NSButton *_searchButton;
	NSButton *_resetButton;
	NSButton *_addWatchButton;
	NSTextField *_statusLabel;

	NSTimer *_refreshTimer;
	NSUInteger _refreshTick;
	BOOL _didReset;
}

- (void) showRAMSearchWindow;

@end
