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

#import "DisassemblerWindow.h"

#include <stdio.h>

#include <string>
#include <vector>

#include "../../shared/disassembly_core.h"

// From NDSSystem.h. Repeated here rather than including that header, whose
// types.h drags in BOOL/TRUE/FALSE and breaks Cocoa declarations. u32 is
// unsigned int there, so the signature matches exactly.
extern void NDS_SetDebugBreakHandler(void (*handler)(int cpu, unsigned int address));

// Also from NDSSystem.h: true while the emulation thread is running the CPU.
// Used to decide whether the auto-update timer should chase the PC.
extern volatile bool execute;

// NSNotificationCenter (postNotificationOnMainThreadName:object:userInfo:).
#import "../cocoa_util.h"

NSString * const kDisassemblerBreakNotification = @"org.desmume.DeSmuME.debugBreak";

// Same values as NDS_DEBUG_CPU_* in NDSSystem.h, repeated here because that
// header drags in types.h, whose BOOL/TRUE/FALSE macros break Cocoa.
enum
{
	kDisassemblerCpuARM9 = 0,
	kDisassemblerCpuARM7 = 1
};

static const CGFloat kWindowWidth        = 900.0;
static const CGFloat kWindowHeight       = 640.0;
static const CGFloat kMinimumWidth       = 780.0;
static const CGFloat kMinimumHeight      = 520.0;
static const CGFloat kSidebarWidth       = 288.0;
static const CGFloat kSidebarPadding     = 12.0;
static const CGFloat kRowHeight          = 22.0;
static const CGFloat kControlHeight      = 24.0;
static const NSTimeInterval kRefreshInterval = 0.1;

// Fixed highlight colours: black text stays readable on them in both
// appearances, exactly like the Windows dialog's yellow/red rows.
static NSColor *DisassemblerCurrentRowColor()
{
	return [NSColor colorWithCalibratedRed:1.0 green:0.92 blue:0.35 alpha:1.0];
}

static NSColor *DisassemblerBreakpointRowColor()
{
	return [NSColor colorWithCalibratedRed:1.0 green:0.58 blue:0.58 alpha:1.0];
}

static NSFont *DisassemblerMonospacedFont(CGFloat size)
{
	NSFont *font = [NSFont userFixedPitchFontOfSize:size];
	if (font == nil)
	{
		font = [NSFont systemFontOfSize:size];
	}
	return font;
}

static DisassemblyCore& Core()
{
	return DisassemblyCore::Get();
}

#pragma mark - Scrolling disassembly view

@interface DisassemblerListView : NSView
{
	NSFont *_font;
	CGFloat _lineHeight;
	std::vector<DisassemblyCore::Line> _lines;
	CGFloat _scrollAccumulator;
}

- (void) reloadLines;
- (void) recalcMetrics;
- (NSUInteger) visibleLineCount;

@end

@implementation DisassemblerListView

- (id) initWithFrame:(NSRect)frame
{
	self = [super initWithFrame:frame];
	if (self != nil)
	{
		_font = [DisassemblerMonospacedFont(12.0) retain];
		_scrollAccumulator = 0.0;
		[self recalcMetrics];
	}
	return self;
}

- (void) dealloc
{
	[_font release];
	[super dealloc];
}

- (BOOL) acceptsFirstResponder
{
	return YES;
}

- (BOOL) isFlipped
{
	// Row 0 at the top, like the Windows control.
	return YES;
}

- (void) recalcMetrics
{
	_lineHeight = ceil([_font ascender] - [_font descender] + [_font leading]) + 2.0;
}

- (NSUInteger) visibleLineCount
{
	const CGFloat available = [self bounds].size.height - 4.0;
	if (available <= 0.0)
	{
		return 1;
	}

	const NSUInteger rows = (NSUInteger)(available / _lineHeight);
	return (rows > 0) ? rows : 1;
}

- (void) reloadLines
{
	Core().BuildLines([self visibleLineCount], _lines);
	[self setNeedsDisplay:YES];
}

- (void) drawRect:(NSRect)dirtyRect
{
	[[NSColor textBackgroundColor] setFill];
	NSRectFill([self bounds]);

	const NSRect bounds = [self bounds];
	NSDictionary *normalAttrs = [NSDictionary dictionaryWithObjectsAndKeys:
	                             _font, NSFontAttributeName,
	                             [NSColor textColor], NSForegroundColorAttributeName,
	                             nil];
	NSDictionary *highlightAttrs = [NSDictionary dictionaryWithObjectsAndKeys:
	                                _font, NSFontAttributeName,
	                                [NSColor blackColor], NSForegroundColorAttributeName,
	                                nil];

	for (size_t i = 0; i < _lines.size(); ++i)
	{
		const DisassemblyCore::Line& line = _lines[i];
		const CGFloat y = 2.0 + ((CGFloat)i * _lineHeight);

		NSDictionary *attrs = normalAttrs;
		NSColor *background = nil;

		if (line.Breakpoint)
		{
			background = DisassemblerBreakpointRowColor();
		}
		if (line.Current)
		{
			background = DisassemblerCurrentRowColor();
		}

		if (background != nil)
		{
			[background setFill];
			NSRectFill(NSMakeRect(0.0, y - 1.0, bounds.size.width, _lineHeight));
			attrs = highlightAttrs;
		}

		NSString *text = [NSString stringWithUTF8String:line.Text.c_str()];
		if (text == nil)
		{
			text = @"";
		}
		[text drawAtPoint:NSMakePoint(6.0, y) withAttributes:attrs];
	}
}

- (void) mouseDown:(NSEvent *)event
{
	[[self window] makeFirstResponder:self];
}

- (void) scrollWheel:(NSEvent *)event
{
	const CGFloat delta = [event scrollingDeltaY];
	if (delta == 0.0)
	{
		return;
	}

	if ([event hasPreciseScrollingDeltas])
	{
		_scrollAccumulator += delta;
	}
	else
	{
		_scrollAccumulator += delta * _lineHeight;
	}

	const NSInteger rows = (NSInteger)(_scrollAccumulator / _lineHeight);
	if (rows == 0)
	{
		return;
	}

	_scrollAccumulator -= (CGFloat)rows * _lineHeight;

	// Scrolling down (negative delta) moves the view towards higher addresses.
	Core().ScrollLines((int)-rows);
	[self reloadLines];
}

- (void) keyDown:(NSEvent *)event
{
	const unichar key = [[event charactersIgnoringModifiers] length] > 0
	                  ? [[event charactersIgnoringModifiers] characterAtIndex:0] : 0;

	switch (key)
	{
		case NSUpArrowFunctionKey:
			Core().ScrollLines(-1);
			break;
		case NSDownArrowFunctionKey:
			Core().ScrollLines(1);
			break;
		case NSPageUpFunctionKey:
			Core().ScrollPages(-1, [self visibleLineCount]);
			break;
		case NSPageDownFunctionKey:
			Core().ScrollPages(1, [self visibleLineCount]);
			break;
		case NSHomeFunctionKey:
			Core().FollowPC();
			break;
		default:
			[super keyDown:event];
			return;
	}

	[self reloadLines];
}

- (void) setFrameSize:(NSSize)newSize
{
	[super setFrameSize:newSize];
	[self reloadLines];
}

@end

#pragma mark - Window controller

@interface DisassemblerWindowController (Private)
- (void) buildInterface;
- (NSTextField *) labelWithText:(NSString *)text frame:(NSRect)frame;
- (NSTextField *) fieldWithFrame:(NSRect)frame editable:(BOOL)editable;
- (NSButton *) buttonWithTitle:(NSString *)title frame:(NSRect)frame action:(SEL)action;
- (void) refreshInterface;
- (void) refreshRegisters;
- (void) refreshBreakpoints;
- (BOOL) isEditingRegister;
- (void) startRefreshTimer;
- (void) stopRefreshTimer;
- (void) refreshTimerFired:(NSTimer *)timer;
- (void) syncControlsFromCore;
@end

@implementation DisassemblerWindowController

@synthesize delegate = _delegate;

+ (void) installDebugBreakHandler
{
	// Called from the emulation thread, so hop to the main thread before any
	// UI work happens.
	NDS_SetDebugBreakHandler([](int cpu, unsigned int address)
	{
		NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
		NSDictionary *info = [[NSDictionary alloc] initWithObjectsAndKeys:
		                      [NSNumber numberWithInt:cpu], @"cpu",
		                      [NSNumber numberWithUnsignedInt:(unsigned int)address], @"address",
		                      nil];
		[[NSNotificationCenter defaultCenter] postNotificationOnMainThreadName:kDisassemblerBreakNotification
		                                                                object:nil
		                                                              userInfo:info];
		[info release];
		[pool release];
	});
}

- (id) init
{
	NSRect contentRect = NSMakeRect(0.0, 0.0, kWindowWidth, kWindowHeight);
	NSWindow *window = [[NSWindow alloc] initWithContentRect:contentRect
	                                               styleMask:(NSWindowStyleMaskTitled |
	                                                          NSWindowStyleMaskClosable |
	                                                          NSWindowStyleMaskMiniaturizable |
	                                                          NSWindowStyleMaskResizable)
	                                                 backing:NSBackingStoreBuffered
	                                                   defer:NO];
	if (window == nil)
	{
		[self release];
		return nil;
	}

	[window setTitle:@"Disassembler"];
	[window setReleasedWhenClosed:NO];
	[window setMinSize:NSMakeSize(kMinimumWidth, kMinimumHeight)];
	[window setContentMinSize:NSMakeSize(kMinimumWidth, kMinimumHeight)];

	self = [super initWithWindow:window];
	[window release];
	if (self == nil)
	{
		return nil;
	}

	_delegate = nil;
	_registerFields = [[NSMutableArray alloc] init];
	_autoUpdate = YES;
	_autoUpdateFrames = 1;
	_syncingControls = NO;
	_refreshTimer = nil;

	[window setDelegate:self];
	[self buildInterface];

	return self;
}

- (void) dealloc
{
	[self stopRefreshTimer];
	[_registerFields release];
	[_pcField release];
	[super dealloc];
}

#pragma mark Interface construction

- (NSTextField *) labelWithText:(NSString *)text frame:(NSRect)frame
{
	NSTextField *label = [[NSTextField alloc] initWithFrame:frame];
	[label setStringValue:text];
	[label setBezeled:NO];
	[label setDrawsBackground:NO];
	[label setEditable:NO];
	[label setSelectable:NO];
	[label setFont:[NSFont systemFontOfSize:11.0]];
	return [label autorelease];
}

- (NSTextField *) fieldWithFrame:(NSRect)frame editable:(BOOL)editable
{
	NSTextField *field = [[NSTextField alloc] initWithFrame:frame];
	[field setFont:DisassemblerMonospacedFont(11.0)];
	[field setAlignment:NSTextAlignmentRight];
	[field setEditable:editable];
	[field setSelectable:YES];
	[field setBezeled:editable];
	[field setDrawsBackground:editable];
	return [field autorelease];
}

- (NSButton *) buttonWithTitle:(NSString *)title frame:(NSRect)frame action:(SEL)action
{
	NSButton *button = [[NSButton alloc] initWithFrame:frame];
	[button setTitle:title];
	[button setBezelStyle:NSBezelStyleRounded];
	[button setFont:[NSFont systemFontOfSize:11.0]];
	[button setTarget:self];
	[button setAction:action];
	return [button autorelease];
}

- (void) buildInterface
{
	NSWindow *window = [self window];
	NSView *contentView = [window contentView];
	const NSRect contentBounds = [contentView bounds];
	const CGFloat contentWidth = contentBounds.size.width;
	const CGFloat contentHeight = contentBounds.size.height;

	// ---- Top control row ------------------------------------------------
	const CGFloat topY = contentHeight - kControlHeight - 8.0;

	NSTextField *cpuLabel = [self labelWithText:@"CPU:" frame:NSMakeRect(12.0, topY + 5.0, 34.0, 16.0)];
	[cpuLabel setAutoresizingMask:NSViewMinYMargin];
	[contentView addSubview:cpuLabel];

	_cpuPopUp = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(48.0, topY, 86.0, kControlHeight) pullsDown:NO];
	[_cpuPopUp addItemsWithTitles:[NSArray arrayWithObjects:@"ARM9", @"ARM7", nil]];
	[_cpuPopUp setFont:[NSFont systemFontOfSize:11.0]];
	[_cpuPopUp setTarget:self];
	[_cpuPopUp setAction:@selector(cpuChanged:)];
	[_cpuPopUp setAutoresizingMask:NSViewMinYMargin];
	[contentView addSubview:_cpuPopUp];
	[_cpuPopUp release];

	NSTextField *modeLabel = [self labelWithText:@"Mode:" frame:NSMakeRect(146.0, topY + 5.0, 40.0, 16.0)];
	[modeLabel setAutoresizingMask:NSViewMinYMargin];
	[contentView addSubview:modeLabel];

	_modePopUp = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(188.0, topY, 96.0, kControlHeight) pullsDown:NO];
	[_modePopUp addItemsWithTitles:[NSArray arrayWithObjects:@"Auto", @"ARM", @"Thumb", nil]];
	[_modePopUp setFont:[NSFont systemFontOfSize:11.0]];
	[_modePopUp setTarget:self];
	[_modePopUp setAction:@selector(modeChanged:)];
	[_modePopUp setAutoresizingMask:NSViewMinYMargin];
	[contentView addSubview:_modePopUp];
	[_modePopUp release];

	NSTextField *goLabel = [self labelWithText:@"Go to:" frame:NSMakeRect(298.0, topY + 5.0, 44.0, 16.0)];
	[goLabel setAutoresizingMask:NSViewMinYMargin];
	[contentView addSubview:goLabel];

	_goField = [[NSTextField alloc] initWithFrame:NSMakeRect(344.0, topY, 84.0, kControlHeight)];
	[_goField setFont:DisassemblerMonospacedFont(11.0)];
	[_goField setPlaceholderString:@"02000000"];
	[_goField setTarget:self];
	[_goField setAction:@selector(goPressed:)];
	[_goField setAutoresizingMask:NSViewMinYMargin];
	[contentView addSubview:_goField];
	[_goField release];

	NSButton *goButton = [self buttonWithTitle:@"Go" frame:NSMakeRect(432.0, topY, 40.0, kControlHeight) action:@selector(goPressed:)];
	[goButton setAutoresizingMask:NSViewMinYMargin];
	[contentView addSubview:goButton];

	_autoUpdateCheckBox = [[NSButton alloc] initWithFrame:NSMakeRect(484.0, topY + 3.0, 108.0, 18.0)];
	[_autoUpdateCheckBox setButtonType:NSButtonTypeSwitch];
	[_autoUpdateCheckBox setTitle:@"Auto-update"];
	[_autoUpdateCheckBox setFont:[NSFont systemFontOfSize:11.0]];
	[_autoUpdateCheckBox setState:NSControlStateValueOn];
	[_autoUpdateCheckBox setTarget:self];
	[_autoUpdateCheckBox setAction:@selector(autoUpdateChanged:)];
	[_autoUpdateCheckBox setAutoresizingMask:NSViewMinYMargin];
	[contentView addSubview:_autoUpdateCheckBox];
	[_autoUpdateCheckBox release];

	_autoUpdateStepper = [[NSStepper alloc] initWithFrame:NSMakeRect(596.0, topY + 1.0, 16.0, kControlHeight)];
	[_autoUpdateStepper setMinValue:1.0];
	[_autoUpdateStepper setMaxValue:99.0];
	[_autoUpdateStepper setIncrement:1.0];
	[_autoUpdateStepper setIntegerValue:(NSInteger)_autoUpdateFrames];
	[_autoUpdateStepper setTarget:self];
	[_autoUpdateStepper setAction:@selector(stepperChanged:)];
	[_autoUpdateStepper setAutoresizingMask:NSViewMinYMargin];
	[contentView addSubview:_autoUpdateStepper];
	[_autoUpdateStepper release];

	_autoUpdateFramesLabel = [self labelWithText:@"1 frame" frame:NSMakeRect(616.0, topY + 5.0, 70.0, 16.0)];
	[_autoUpdateFramesLabel setAutoresizingMask:NSViewMinYMargin];
	[contentView addSubview:_autoUpdateFramesLabel];

	// ---- Disassembly list ------------------------------------------------
	// Keep the list top just below the control row; the old formula left a
	// gap that was almost a full text row high.
	const CGFloat listWidth = contentWidth - kSidebarWidth - 24.0;
	const CGFloat listTop = topY - 8.0;
	const CGFloat listHeight = listTop - 34.0;

	DisassemblerListView *listView = [[DisassemblerListView alloc] initWithFrame:NSMakeRect(12.0, 34.0, listWidth, listHeight)];
	[listView setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
	[contentView addSubview:listView];
	_listView = listView;
	[listView release];

	// ---- Bottom buttons ---------------------------------------------------
	NSButton *stepButton = [self buttonWithTitle:@"Step" frame:NSMakeRect(12.0, 6.0, 62.0, kControlHeight) action:@selector(stepPressed:)];
	[stepButton setAutoresizingMask:NSViewMaxYMargin];
	[contentView addSubview:stepButton];

	NSButton *stepOverButton = [self buttonWithTitle:@"Step Over" frame:NSMakeRect(78.0, 6.0, 86.0, kControlHeight) action:@selector(stepOverPressed:)];
	[stepOverButton setAutoresizingMask:NSViewMaxYMargin];
	[contentView addSubview:stepOverButton];

	NSButton *continueButton = [self buttonWithTitle:@"Continue" frame:NSMakeRect(168.0, 6.0, 80.0, kControlHeight) action:@selector(continuePressed:)];
	[continueButton setAutoresizingMask:NSViewMaxYMargin];
	[contentView addSubview:continueButton];

	NSButton *runToReturnButton = [self buttonWithTitle:@"Run To Return" frame:NSMakeRect(252.0, 6.0, 110.0, kControlHeight) action:@selector(runToReturnPressed:)];
	[runToReturnButton setAutoresizingMask:NSViewMaxYMargin];
	[contentView addSubview:runToReturnButton];

	NSButton *refreshButton = [self buttonWithTitle:@"Refresh" frame:NSMakeRect(366.0, 6.0, 74.0, kControlHeight) action:@selector(refreshPressed:)];
	[refreshButton setAutoresizingMask:NSViewMaxYMargin];
	[contentView addSubview:refreshButton];

	// ---- Sidebar ----------------------------------------------------------
	const CGFloat sidebarX = contentWidth - kSidebarWidth + kSidebarPadding;
	const CGFloat fieldWidth = 84.0;
	const CGFloat columnWidth = 132.0;
	// The control row spans the whole window, so the sidebar must start below
	// it or the "1 frame" label overlaps the Registers header.
	const CGFloat registerTop = topY - 36.0;

	// The sidebar is anchored to the right edge, so every control in it keeps
	// its right margin when the window is resized.
	NSTextField *registersHeader = [self labelWithText:@"Registers" frame:NSMakeRect(sidebarX, registerTop + 8.0, 100.0, 16.0)];
	[registersHeader setFont:[NSFont boldSystemFontOfSize:11.0]];
	[registersHeader setAutoresizingMask:(NSViewMinXMargin | NSViewMinYMargin)];
	[contentView addSubview:registersHeader];

	for (int i = 0; i < 16; ++i)
	{
		const int column = i / 8;
		const int row = i % 8;
		const CGFloat x = sidebarX + ((CGFloat)column * columnWidth);
		const CGFloat y = registerTop - ((CGFloat)(row + 1) * kRowHeight);

		char name[8];
		if (i == 13)      snprintf(name, sizeof(name), "SP");
		else if (i == 14) snprintf(name, sizeof(name), "LR");
		else if (i == 15) snprintf(name, sizeof(name), "PC");
		else              snprintf(name, sizeof(name), "R%d", i);

		NSTextField *label = [self labelWithText:[NSString stringWithFormat:@"%s", name]
		                                   frame:NSMakeRect(x, y + 3.0, 28.0, 16.0)];
		[label setFont:DisassemblerMonospacedFont(11.0)];
		[label setAutoresizingMask:(NSViewMinXMargin | NSViewMinYMargin)];
		[contentView addSubview:label];

		NSTextField *field = [self fieldWithFrame:NSMakeRect(x + 30.0, y, fieldWidth, 20.0) editable:(i < 15)];
		[field setTag:i];
		[field setStringValue:@"00000000"];
		[field setAutoresizingMask:(NSViewMinXMargin | NSViewMinYMargin)];
		[contentView addSubview:field];

		if (i < 15)
		{
			[_registerFields addObject:field];
		}
		else
		{
			_pcField = [field retain];
		}
	}

	const CGFloat flagsY = registerTop - (9.0 * kRowHeight);

	NSTextField *cpsrLabel = [self labelWithText:@"CPSR" frame:NSMakeRect(sidebarX, flagsY + 3.0, 40.0, 16.0)];
	[cpsrLabel setAutoresizingMask:(NSViewMinXMargin | NSViewMinYMargin)];
	[contentView addSubview:cpsrLabel];

	_cpsrFlagsField = [self fieldWithFrame:NSMakeRect(sidebarX + 44.0, flagsY, 150.0, 20.0) editable:NO];
	[_cpsrFlagsField setAlignment:NSTextAlignmentLeft];
	[_cpsrFlagsField setAutoresizingMask:(NSViewMinXMargin | NSViewMinYMargin)];
	[contentView addSubview:_cpsrFlagsField];

	NSTextField *modeValueLabel = [self labelWithText:@"Mode" frame:NSMakeRect(sidebarX, flagsY - kRowHeight + 3.0, 40.0, 16.0)];
	[modeValueLabel setAutoresizingMask:(NSViewMinXMargin | NSViewMinYMargin)];
	[contentView addSubview:modeValueLabel];

	_modeValueField = [self fieldWithFrame:NSMakeRect(sidebarX + 44.0, flagsY - kRowHeight, 150.0, 20.0) editable:NO];
	[_modeValueField setAlignment:NSTextAlignmentLeft];
	[_modeValueField setAutoresizingMask:(NSViewMinXMargin | NSViewMinYMargin)];
	[contentView addSubview:_modeValueField];

	NSTextField *spsrLabel = [self labelWithText:@"SPSR" frame:NSMakeRect(sidebarX, flagsY - (2.0 * kRowHeight) + 3.0, 40.0, 16.0)];
	[spsrLabel setAutoresizingMask:(NSViewMinXMargin | NSViewMinYMargin)];
	[contentView addSubview:spsrLabel];

	_spsrValueField = [self fieldWithFrame:NSMakeRect(sidebarX + 44.0, flagsY - (2.0 * kRowHeight), 150.0, 20.0) editable:NO];
	[_spsrValueField setAlignment:NSTextAlignmentLeft];
	[_spsrValueField setAutoresizingMask:(NSViewMinXMargin | NSViewMinYMargin)];
	[contentView addSubview:_spsrValueField];

	_updateRegistersButton = [self buttonWithTitle:@"Update Registers"
	                                        frame:NSMakeRect(sidebarX, flagsY - (3.0 * kRowHeight) - 2.0, 194.0, kControlHeight)
	                                       action:@selector(updateRegistersPressed:)];
	[_updateRegistersButton setAutoresizingMask:(NSViewMinXMargin | NSViewMinYMargin)];
	[contentView addSubview:_updateRegistersButton];

	// ---- Breakpoints ------------------------------------------------------
	const CGFloat breakpointTop = flagsY - (3.0 * kRowHeight) - 34.0;

	NSTextField *breakpointsHeader = [self labelWithText:@"Breakpoints" frame:NSMakeRect(sidebarX, breakpointTop, 120.0, 16.0)];
	[breakpointsHeader setFont:[NSFont boldSystemFontOfSize:11.0]];
	[breakpointsHeader setAutoresizingMask:(NSViewMinXMargin | NSViewMinYMargin)];
	[contentView addSubview:breakpointsHeader];

	const CGFloat tableHeight = breakpointTop - 96.0;
	_breakpointScrollView = [[NSScrollView alloc] initWithFrame:NSMakeRect(sidebarX, 70.0, 194.0, (tableHeight > 60.0) ? tableHeight : 60.0)];
	[_breakpointScrollView setHasVerticalScroller:YES];
	[_breakpointScrollView setBorderType:NSBezelBorder];
	// Keep the bottom edge above the Address/Add/Remove row. HeightSizable
	// alone lets the table grow upwards when the window is enlarged and keeps
	// its bottom margin fixed when it is shrunk.
	[_breakpointScrollView setAutoresizingMask:(NSViewMinXMargin | NSViewHeightSizable)];

	_breakpointTable = [[NSTableView alloc] initWithFrame:[_breakpointScrollView bounds]];
	[_breakpointTable setAllowsMultipleSelection:NO];
	[_breakpointTable setUsesAlternatingRowBackgroundColors:NO];
	[_breakpointTable setRowHeight:18.0];
	[_breakpointTable setDataSource:self];
	[_breakpointTable setDelegate:self];

	NSTableColumn *addressColumn = [[NSTableColumn alloc] initWithIdentifier:@"address"];
	[addressColumn setWidth:150.0];
	[[addressColumn headerCell] setStringValue:@"Address"];
	[_breakpointTable addTableColumn:addressColumn];
	[addressColumn release];
	[_breakpointTable setHeaderView:nil];

	[_breakpointScrollView setDocumentView:_breakpointTable];
	[_breakpointTable release];
	[contentView addSubview:_breakpointScrollView];
	[_breakpointScrollView release];

	_breakpointField = [[NSTextField alloc] initWithFrame:NSMakeRect(sidebarX, 40.0, 110.0, 20.0)];
	[_breakpointField setFont:DisassemblerMonospacedFont(11.0)];
	[_breakpointField setPlaceholderString:@"Address"];
	[_breakpointField setTarget:self];
	[_breakpointField setAction:@selector(addBreakpointPressed:)];
	[_breakpointField setAutoresizingMask:(NSViewMinXMargin | NSViewMaxYMargin)];
	[contentView addSubview:_breakpointField];
	[_breakpointField release];

	NSButton *addButton = [self buttonWithTitle:@"Add" frame:NSMakeRect(sidebarX + 114.0, 37.0, 50.0, kControlHeight) action:@selector(addBreakpointPressed:)];
	[addButton setAutoresizingMask:(NSViewMinXMargin | NSViewMaxYMargin)];
	[contentView addSubview:addButton];

	NSButton *removeButton = [self buttonWithTitle:@"Remove" frame:NSMakeRect(sidebarX + 166.0, 37.0, 70.0, kControlHeight) action:@selector(removeBreakpointPressed:)];
	[removeButton setAutoresizingMask:(NSViewMinXMargin | NSViewMaxYMargin)];
	[contentView addSubview:removeButton];

	[self syncControlsFromCore];
	[listView reloadLines];
	[self refreshRegisters];
	[self refreshBreakpoints];
}

#pragma mark Refresh

- (void) syncControlsFromCore
{
	_syncingControls = YES;

	const NSInteger cpu = (NSInteger)Core().GetCpu();
	[_cpuPopUp selectItemAtIndex:(cpu == kDisassemblerCpuARM7) ? 1 : 0];

	NSInteger mode = 0;
	switch (Core().GetViewMode())
	{
		case DisassemblyCore::ViewMode_ARM:   mode = 1; break;
		case DisassemblyCore::ViewMode_Thumb: mode = 2; break;
		case DisassemblyCore::ViewMode_Auto:
		default:                              mode = 0; break;
	}
	[_modePopUp selectItemAtIndex:mode];

	[_autoUpdateCheckBox setState:_autoUpdate ? NSControlStateValueOn : NSControlStateValueOff];
	[_autoUpdateStepper setIntegerValue:(NSInteger)_autoUpdateFrames];
	[_autoUpdateFramesLabel setStringValue:[NSString stringWithFormat:@"%lu frame%s",
	                                        (unsigned long)_autoUpdateFrames,
	                                        (_autoUpdateFrames == 1) ? "" : "s"]];

	_syncingControls = NO;
}

- (BOOL) isEditingRegister
{
	NSResponder *responder = [[self window] firstResponder];
	if (![responder isKindOfClass:[NSTextView class]])
	{
		return NO;
	}

	id fieldEditorDelegate = [(NSTextView *)responder delegate];
	for (NSTextField *field in _registerFields)
	{
		if (field == fieldEditorDelegate)
		{
			return YES;
		}
	}
	return NO;
}

- (void) refreshRegisters
{
	const DisassemblyCore::Registers regs = DisassemblyCore::ReadCpuRegisters(Core().GetCpu());

	if (![self isEditingRegister])
	{
		for (NSTextField *field in _registerFields)
		{
			const NSInteger index = [field tag];
			[field setStringValue:[NSString stringWithFormat:@"%08X", (unsigned int)regs.R[index]]];
		}
	}

	[_pcField setStringValue:[NSString stringWithFormat:@"%08X", (unsigned int)regs.R[15]]];

	// CPSR flags: set flags in red, clear ones in grey.
	NSMutableAttributedString *flags = [[NSMutableAttributedString alloc] init];
	NSArray *letters = [NSArray arrayWithObjects:@"N", @"Z", @"C", @"V", @"Q", @"I", @"F", @"T", nil];
	const bool values[8] = { regs.N, regs.Z, regs.C, regs.V, regs.Q, regs.I, regs.F, regs.T };
	for (NSUInteger i = 0; i < [letters count]; ++i)
	{
		NSString *letter = [NSString stringWithFormat:@"%@ ", [letters objectAtIndex:i]];
		NSColor *color = values[i] ? [NSColor systemRedColor] : [NSColor disabledControlTextColor];
		NSDictionary *attrs = [NSDictionary dictionaryWithObjectsAndKeys:
		                       DisassemblerMonospacedFont(11.0), NSFontAttributeName,
		                       color, NSForegroundColorAttributeName,
		                       nil];
		[flags appendAttributedString:[[[NSAttributedString alloc] initWithString:letter attributes:attrs] autorelease]];
	}

	NSMutableAttributedString *result = [[NSMutableAttributedString alloc] init];
	NSDictionary *prefixAttrs = [NSDictionary dictionaryWithObjectsAndKeys:
	                             DisassemblerMonospacedFont(11.0), NSFontAttributeName,
	                             [NSColor secondaryLabelColor], NSForegroundColorAttributeName,
	                             nil];
	[result appendAttributedString:[[[NSAttributedString alloc] initWithString:[NSString stringWithFormat:@"%08X  ", (unsigned int)regs.CPSR]
	                                                                attributes:prefixAttrs] autorelease]];
	[result appendAttributedString:flags];
	[_cpsrFlagsField setAttributedStringValue:result];
	[result release];
	[flags release];

	const unsigned int modeIndex = regs.Mode & 0x1F;

	[_modeValueField setStringValue:[NSString stringWithFormat:@"%02X %s", modeIndex, DisassemblyCore::CpuModeName(modeIndex)]];
	[_spsrValueField setStringValue:[NSString stringWithFormat:@"%08X", (unsigned int)regs.SPSR]];
}

- (void) refreshBreakpoints
{
	[_breakpointTable reloadData];

	const NSInteger count = (NSInteger)Core().BreakpointCount();
	if (count > 0)
	{
		[_breakpointTable selectRowIndexes:[NSIndexSet indexSetWithIndex:(NSUInteger)(count - 1)] byExtendingSelection:NO];
	}
}

- (void) refreshInterface
{
	[self syncControlsFromCore];
	[self refreshRegisters];
	[self refreshBreakpoints];
	[(DisassemblerListView *)_listView reloadLines];
}

- (void) startRefreshTimer
{
	[self stopRefreshTimer];
	_refreshTimer = [NSTimer timerWithTimeInterval:kRefreshInterval
	                                        target:self
	                                      selector:@selector(refreshTimerFired:)
	                                      userInfo:nil
	                                       repeats:YES];
	[[NSRunLoop currentRunLoop] addTimer:_refreshTimer forMode:NSRunLoopCommonModes];
}

- (void) stopRefreshTimer
{
	[_refreshTimer invalidate];
	_refreshTimer = nil;
}

- (void) refreshTimerFired:(NSTimer *)timer
{
	// "Auto-update" means "keep the view glued to the PC while the CPU runs".
	// Once emulation is paused the PC cannot move, so re-anchoring would only
	// undo the user's scrolling; the list is refreshed by the individual
	// actions (step, Go to, breakpoint hit) instead.
	if (!_autoUpdate)
	{
		return;
	}

	if (execute)
	{
		Core().FollowPC();
	}

	[(DisassemblerListView *)_listView reloadLines];
	[self refreshRegisters];
}

#pragma mark Actions

- (void) cpuChanged:(id)sender
{
	if (_syncingControls)
	{
		return;
	}

	const NSInteger selection = [_cpuPopUp indexOfSelectedItem];
	Core().SetCpu((selection == 1) ? DisassemblyCore::CPU_ARM7 : DisassemblyCore::CPU_ARM9);

	// Each CPU keeps its own view state; re-align in case the new CPU is in a
	// different ARM/Thumb mode.
	Core().SetAnchorAddress(Core().AnchorAddress());
	[self refreshInterface];
}

- (void) modeChanged:(id)sender
{
	if (_syncingControls)
	{
		return;
	}

	switch ([_modePopUp indexOfSelectedItem])
	{
		case 1:  Core().SetViewMode(DisassemblyCore::ViewMode_ARM); break;
		case 2:  Core().SetViewMode(DisassemblyCore::ViewMode_Thumb); break;
		default: Core().SetViewMode(DisassemblyCore::ViewMode_Auto); break;
	}
	[self refreshInterface];
}

- (void) goPressed:(id)sender
{
	uint32_t address = 0;
	if (DisassemblyCore::ParseHex32([[_goField stringValue] UTF8String], address))
	{
		Core().SetAnchorAddress(address);
		[(DisassemblerListView *)_listView reloadLines];
	}
}

- (void) refreshPressed:(id)sender
{
	Core().FollowPC();
	[self refreshInterface];
}

- (void) autoUpdateChanged:(id)sender
{
	_autoUpdate = ([_autoUpdateCheckBox state] == NSControlStateValueOn);
	if (_autoUpdate)
	{
		Core().FollowPC();
		[(DisassemblerListView *)_listView reloadLines];
	}
}

- (void) stepperChanged:(id)sender
{
	_autoUpdateFrames = (NSUInteger)[_autoUpdateStepper integerValue];
	if (_autoUpdateFrames == 0)
	{
		_autoUpdateFrames = 1;
	}
	[_autoUpdateFramesLabel setStringValue:[NSString stringWithFormat:@"%lu frame%s",
	                                        (unsigned long)_autoUpdateFrames,
	                                        (_autoUpdateFrames == 1) ? "" : "s"]];
}

- (void) stepPressed:(id)sender
{
	Core().StepInstruction();
	[self requestResume];
}

- (void) stepOverPressed:(id)sender
{
	Core().StepOver();
	[self requestResume];
}

- (void) continuePressed:(id)sender
{
	Core().Continue();
	[self requestResume];
}

- (void) runToReturnPressed:(id)sender
{
	Core().RunToReturn();
	[self requestResume];
}

- (void) requestResume
{
	if ([_delegate respondsToSelector:@selector(disassemblerWindowDidRequestResume:)])
	{
		[_delegate disassemblerWindowDidRequestResume:self];
	}

	[self refreshRegisters];
	[(DisassemblerListView *)_listView reloadLines];
}

- (void) updateRegistersPressed:(id)sender
{
	for (NSTextField *field in _registerFields)
	{
		uint32_t value = 0;
		if (DisassemblyCore::ParseHex32([[field stringValue] UTF8String], value))
		{
			Core().WriteRegister((unsigned int)[field tag], value);
		}
	}

	[self refreshRegisters];
	[(DisassemblerListView *)_listView reloadLines];
}

- (void) addBreakpointPressed:(id)sender
{
	uint32_t address = 0;
	if (DisassemblyCore::ParseHex32([[_breakpointField stringValue] UTF8String], address))
	{
		if (Core().AddBreakpoint(address))
		{
			[_breakpointField setStringValue:@""];
		}
	}

	[self refreshBreakpoints];
	[(DisassemblerListView *)_listView reloadLines];
}

- (void) removeBreakpointPressed:(id)sender
{
	const NSInteger row = [_breakpointTable selectedRow];
	if (row >= 0)
	{
		Core().RemoveBreakpointAt((size_t)row);
	}

	[self refreshBreakpoints];
	[(DisassemblerListView *)_listView reloadLines];
}

#pragma mark Breakpoint table

- (NSInteger) numberOfRowsInTableView:(NSTableView *)tableView
{
	return (NSInteger)Core().BreakpointCount();
}

- (id) tableView:(NSTableView *)tableView objectValueForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row
{
	const uint32_t address = Core().BreakpointAt((size_t)row);
	return [NSString stringWithFormat:@"%08X", (unsigned int)address];
}

#pragma mark Window

- (void) showDisassemblerWindow
{
	[self showWindow:self];
	[[self window] makeKeyAndOrderFront:self];
	[self syncControlsFromCore];

	if (Core().AnchorAddress() == 0)
	{
		Core().FollowPC();
	}

	[(DisassemblerListView *)_listView reloadLines];
	[self refreshRegisters];
	[self refreshBreakpoints];
	[self startRefreshTimer];
}

- (void) showAddress:(uint32_t)address cpu:(int)cpu
{
	Core().SetCpu((cpu == kDisassemblerCpuARM7) ? DisassemblyCore::CPU_ARM7 : DisassemblyCore::CPU_ARM9);
	Core().SetAnchorAddress(address);

	[self showDisassemblerWindow];
}

- (void) windowWillClose:(NSNotification *)notification
{
	[self stopRefreshTimer];
}

@end
