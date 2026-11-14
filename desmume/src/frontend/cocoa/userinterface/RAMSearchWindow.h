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
// ../../shared/ram_search_core.cpp. The window follows the Windows dialog: the
// result list sits top-left, the action buttons run down its right edge, and
// the search criteria live in four group boxes underneath. Row actions mirror
// the Windows dialog as well: Watch inserts into RAM Watch, Add Cheat creates an
// internal cheat in the Cheat Manager, Poke writes a value, Eliminate drops a
// row, and Undo/Redo step through searches.

#import <Cocoa/Cocoa.h>

@interface RAMSearchWindowController : NSWindowController <NSWindowDelegate,
                                                          NSTableViewDataSource,
                                                          NSTableViewDelegate>
{
	NSScrollView *_tableScrollView;
	NSTableView *_tableView;
	NSView *_buttonColumn;
	NSView *_optionsView;

	// Comparison Operator group
	NSTextField *_differentByField;
	NSTextField *_moduloField;

	// Compare To / By group
	NSTextField *_specificValueField;
	NSTextField *_specificAddressField;
	NSTextField *_changeCountField;

	// Data Size group
	NSButton *_misalignButton;
	NSButton *_bigEndianButton;

	// Data Type / Display group
	NSButton *_autoSearchButton;

	// Action buttons that need an enabled state
	NSButton *_undoButton;
	NSButton *_redoButton;
	NSButton *_eliminateButton;
	NSButton *_pokeButton;
	NSButton *_addWatchButton;
	NSButton *_addCheatButton;

	// Every radio button in the window, in creation order. AppKit lets a
	// button's type be set but not read back, so the list is what tells the
	// four radio groups apart from the checkboxes that share a group box.
	NSMutableArray *_radioButtons;

	// Selected tags of the four radio groups
	NSInteger _searchTypeTag;
	NSInteger _operatorTag;
	NSInteger _sizeTag;
	NSInteger _displayTag;

	NSTimer *_refreshTimer;
	NSUInteger _refreshTick;
	BOOL _didReset;

	BOOL _autoSearchEnabled;
	BOOL _autoSearchAutoRetry;
	BOOL _isShowingAutoSearchAlert;
}

- (void) showRAMSearchWindow;

@end
