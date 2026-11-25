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

// Cocoa host for the disassembler. Decoding, address navigation, ARM/Thumb
// mode, register snapshots and the breakpoint list live in
// ../../shared/disassembly_core.cpp and are shared with the Windows and GTK3
// frontends; this file is UI only.

#import <Cocoa/Cocoa.h>
#include <stdint.h>

// Posted on the main thread when the core stops on a breakpoint. userInfo:
// "cpu" (NSNumber, NDS_DEBUG_CPU_ARM9/ARM7) and "address" (NSNumber).
extern NSString * const kDisassemblerBreakNotification;

@class DisassemblerWindowController;

// The window cannot resume the emulator by itself: CocoaDSCore owns the
// execution state machine and lives behind EmuControllerDelegate.
@protocol DisassemblerWindowDelegate <NSObject>
- (void) disassemblerWindowDidRequestResume:(DisassemblerWindowController *)controller;
@end

@interface DisassemblerWindowController : NSWindowController <NSWindowDelegate, NSTableViewDataSource, NSTableViewDelegate>
{
	id <DisassemblerWindowDelegate> _delegate;

	NSPopUpButton *_cpuPopUp;
	NSPopUpButton *_modePopUp;
	NSTextField *_goField;
	NSButton *_autoUpdateCheckBox;
	NSStepper *_autoUpdateStepper;
	NSTextField *_autoUpdateFramesLabel;

	NSView *_listView;

	NSMutableArray *_registerFields;
	NSTextField *_pcField;
	NSTextField *_cpsrFlagsField;
	NSTextField *_modeValueField;
	NSTextField *_spsrValueField;
	NSButton *_updateRegistersButton;

	NSTableView *_breakpointTable;
	NSScrollView *_breakpointScrollView;
	NSTextField *_breakpointField;

	NSTimer *_refreshTimer;
	BOOL _autoUpdate;
	NSUInteger _autoUpdateFrames;
	BOOL _syncingControls;
}

@property (assign) id <DisassemblerWindowDelegate> delegate;

// Installs the core's breakpoint hook. Called once at launch; the hook posts
// kDisassemblerBreakNotification on the main thread.
+ (void) installDebugBreakHandler;

// Shows the window and brings it to the given address / CPU.
- (void) showDisassemblerWindow;
- (void) showAddress:(uint32_t)address cpu:(int)cpu;

@end
