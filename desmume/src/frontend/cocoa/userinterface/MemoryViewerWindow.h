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

#import <Cocoa/Cocoa.h>

#include "MemoryViewerCore.h"

// types.h (pulled in above) redefines BOOL/TRUE/FALSE as macros, which breaks
// the Cocoa definitions. Undo that here, following cocoa_core.h's convention.
#undef BOOL
#undef TRUE
#undef FALSE

typedef enum
{
	MemoryViewerViewModeByte = 0,
	MemoryViewerViewModeHalfword = 1,
	MemoryViewerViewModeWord = 2
} MemoryViewerViewMode;

@class MemoryHexView;

@protocol MemoryHexViewDelegate <NSObject>
- (void) memoryHexView:(MemoryHexView *)view didSelectAddress:(u32)address;
- (void) memoryHexView:(MemoryHexView *)view didChangeViewportAddress:(u32)address;
@end

// Displays a 0x100-byte window of emulated memory as a 16x16 hex dump with an
// ASCII column, and lets the user type hexadecimal digits directly into a cell
// to patch memory. This is the macOS counterpart of the Windows frontend's
// MemView_ViewBox control.
@interface MemoryHexView : NSView
{
	MemoryViewerRegionType _region;
	MemoryViewerViewMode _viewMode;
	u32 _address;

	BOOL _selectionEnabled;
	u32 _selectionAddress;
	u32 _selectionPart;
	u32 _selectionNewValue;
	BOOL _bigEndian;
	BOOL _fullCharacters;

	NSUInteger _visibleRowCount;
	u32 _windowSize;

	u8 _memoryBuffer[0x1000];

	NSFont *_monoFont;
	CGFloat _charWidth;
	CGFloat _lineHeight;
	CGFloat _scrollAccumulator;

	id <MemoryHexViewDelegate> _delegate;
}

@property (assign) MemoryViewerRegionType region;
@property (assign) MemoryViewerViewMode viewMode;
@property (assign) u32 address;
@property (assign) BOOL selectionEnabled;
@property (assign) u32 selectionAddress;
@property (readonly) u32 selectionPart;
@property (assign) BOOL bigEndian;
@property (assign) BOOL fullCharacters;
@property (readonly) NSUInteger visibleRowCount;
@property (readonly) u32 windowSize;
@property (assign) id <MemoryHexViewDelegate> delegate;

// Re-reads the 0x100-byte window from the emulated memory bus and redraws.
- (void) reloadData;
// Redraws using the already cached bytes.
- (void) refresh;

// Moves the visible window by the given number of bytes, clamped to the region.
- (void) scrollBy:(s32)delta;

// Places the cursor on a byte address, optionally scrolling it into view.
- (void) setSelectionAddress:(u32)address reveal:(BOOL)reveal;

- (NSSize) preferredMinimumSize;

// Reads the value at address using the active view mode and byte order. Used
// by the controller to show the floating point interpretation in Words mode.
- (u32) displayedValueAtAddress:(u32)address;

@end

@interface MemoryViewerWindowController : NSWindowController <NSWindowDelegate, MemoryHexViewDelegate>
{
	NSView *_toolbar;
	NSView *_bpBar;
	NSPopUpButton *_regionPopUpButton;
	NSComboBox *_addressField;
	NSButton *_goButton;
	NSPopUpButton *_viewModePopUpButton;
	NSTextField *_selectionLabel;
	NSTextField *_bpField;
	NSButton *_addReadBPButton;
	NSButton *_addWriteBPButton;
	NSButton *_removeBPButton;
	NSButton *_clearBPButton;
	NSTextField *_bpSummaryLabel;
	NSButton *_bigEndianButton;
	NSButton *_fullCharsButton;
	NSPopUpButton *_exportButton;
	MemoryHexView *_hexView;
	NSScroller *_scroller;
	NSTimer *_refreshTimer;
	BOOL _didInitializeRegions;
	BOOL _scrollerIsDragging;
}

@property (readonly) MemoryHexView *hexView;

- (void) showWindowForRegion:(MemoryViewerRegionType)region address:(u32)address;
- (void) selectRegion:(MemoryViewerRegionType)region;
- (void) goToAddress:(u32)address;

@end
