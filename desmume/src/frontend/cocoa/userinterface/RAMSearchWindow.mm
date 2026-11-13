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

#import "RAMSearchWindow.h"
#import "RAMWatchWindow.h"

#include <stdlib.h>
#include <string>

#include "../../shared/ram_debug_core.h"
#include "../../shared/ram_search_core.h"

static const CGFloat kWindowWidth       = 760.0;
static const CGFloat kWindowHeight      = 480.0;
static const CGFloat kSidebarWidth      = 220.0;
static const CGFloat kSidebarPadding    = 12.0;
static const CGFloat kLabelWidth        = 76.0;
static const CGFloat kControlHeight     = 24.0;
static const CGFloat kRowSpacing        = 32.0;

static const char kSizeCodes[3]      = { 'b', 'w', 'd' };
static const char kDisplayCodes[4]   = { 's', 'u', 'h', 'f' };

static NSString *RAMSearchString(const std::string& text)
{
	NSString *result = [NSString stringWithUTF8String:text.c_str()];
	return (result != nil) ? result : @"";
}

static NSFont *RAMSearchMonospacedFont(CGFloat size)
{
	NSFont *font = [NSFont userFixedPitchFontOfSize:size];
	return (font != nil) ? font : [NSFont systemFontOfSize:size];
}

@interface RAMSearchWindowController (Private)
- (void) buildInterface;
- (void) startRefreshTimer;
- (void) stopRefreshTimer;
- (void) refreshTimerFired:(NSTimer *)timer;
- (void) updateInterface;
- (void) updateStatusLabel;
- (void) updateParameterField;
- (void) syncOptionsFromUI;
- (int32_t) parseValueFromField:(NSTextField *)field;
- (char) displayTypeCode;
- (char) sizeCode;
- (void) postAddWatchNotificationForRow:(NSInteger)row;
- (NSTextField *) addLabelWithText:(NSString *)text atY:(CGFloat)y;
- (NSPopUpButton *) addPopUpWithItems:(NSArray *)titles atY:(CGFloat)y;
- (NSTextField *) addTextFieldAtY:(CGFloat)y;
- (NSButton *) addCheckboxWithTitle:(NSString *)title atY:(CGFloat)y action:(SEL)action;
- (NSButton *) addFullWidthButtonWithTitle:(NSString *)title atY:(CGFloat)y action:(SEL)action;
@end

@implementation RAMSearchWindowController

- (id)init
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

	[window setTitle:@"RAM Search"];
	[window setReleasedWhenClosed:NO];
	[window setMinSize:NSMakeSize(640.0, 380.0)];
	[window setContentMinSize:NSMakeSize(640.0, 380.0)];

	self = [super initWithWindow:window];
	[window release];
	if (self == nil)
	{
		return nil;
	}

	_refreshTimer = nil;
	_refreshTick = 0;
	_didReset = NO;

	[window setDelegate:self];
	[self buildInterface];
	return self;
}

- (void)dealloc
{
	[self stopRefreshTimer];
	[super dealloc];
}

#pragma mark Interface construction

- (NSTextField *) addLabelWithText:(NSString *)text atY:(CGFloat)y
{
	NSTextField *label = [[NSTextField alloc] initWithFrame:NSMakeRect(kSidebarPadding, y, kLabelWidth, 18.0)];
	[label setStringValue:text];
	[label setEditable:NO];
	[label setBezeled:NO];
	[label setDrawsBackground:NO];
	[label setFont:[NSFont systemFontOfSize:11.0]];
	[label setTextColor:[NSColor secondaryLabelColor]];
	[label setAutoresizingMask:NSViewMinYMargin];
	[_sidebar addSubview:label];
	[label release];
	return label;
}

- (NSPopUpButton *) addPopUpWithItems:(NSArray *)titles atY:(CGFloat)y
{
	const CGFloat width = kSidebarWidth - (2.0 * kSidebarPadding) - kLabelWidth;
	NSPopUpButton *button = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(kSidebarPadding + kLabelWidth, y, width, kControlHeight) pullsDown:NO];
	[button addItemsWithTitles:titles];
	[button setFont:[NSFont systemFontOfSize:11.0]];
	[button setTarget:self];
	[button setAction:@selector(optionsChanged:)];
	[button setAutoresizingMask:NSViewMinYMargin];
	[_sidebar addSubview:button];
	[button release];
	return button;
}

- (NSTextField *) addTextFieldAtY:(CGFloat)y
{
	const CGFloat width = kSidebarWidth - (2.0 * kSidebarPadding) - kLabelWidth;
	NSTextField *field = [[NSTextField alloc] initWithFrame:NSMakeRect(kSidebarPadding + kLabelWidth, y, width, kControlHeight)];
	[field setFont:RAMSearchMonospacedFont(11.0)];
	[field setAutoresizingMask:NSViewMinYMargin];
	[_sidebar addSubview:field];
	[field release];
	return field;
}

- (NSButton *) addCheckboxWithTitle:(NSString *)title atY:(CGFloat)y action:(SEL)action
{
	const CGFloat width = kSidebarWidth - (2.0 * kSidebarPadding);
	NSButton *button = [[NSButton alloc] initWithFrame:NSMakeRect(kSidebarPadding, y, width, 22.0)];
	[button setButtonType:NSButtonTypeSwitch];
	[button setTitle:title];
	[button setFont:[NSFont systemFontOfSize:11.0]];
	[button setTarget:self];
	[button setAction:action];
	[button setAutoresizingMask:NSViewMinYMargin];
	[_sidebar addSubview:button];
	[button release];
	return button;
}

- (NSButton *) addFullWidthButtonWithTitle:(NSString *)title atY:(CGFloat)y action:(SEL)action
{
	const CGFloat width = kSidebarWidth - (2.0 * kSidebarPadding);
	NSButton *button = [[NSButton alloc] initWithFrame:NSMakeRect(kSidebarPadding, y, width, 26.0)];
	[button setTitle:title];
	[button setBezelStyle:NSBezelStyleRounded];
	[button setFont:[NSFont systemFontOfSize:11.0]];
	[button setTarget:self];
	[button setAction:action];
	[button setAutoresizingMask:NSViewMinYMargin];
	[_sidebar addSubview:button];
	[button release];
	return button;
}

- (void) buildInterface
{
	NSView *contentView = [[self window] contentView];
	const CGFloat tableWidth = kWindowWidth - kSidebarWidth;

	_tableScrollView = [[NSScrollView alloc] initWithFrame:NSMakeRect(0.0, 0.0, tableWidth, kWindowHeight)];
	[_tableScrollView setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
	[_tableScrollView setHasVerticalScroller:YES];
	[_tableScrollView setHasHorizontalScroller:NO];
	[_tableScrollView setBorderType:NSBezelBorder];
	[contentView addSubview:_tableScrollView];
	[_tableScrollView release];

	_tableView = [[NSTableView alloc] initWithFrame:NSZeroRect];
	[_tableView setDataSource:self];
	[_tableView setDelegate:self];
	[_tableView setRowHeight:19.0];
	[_tableView setUsesAlternatingRowBackgroundColors:YES];
	[_tableView setAllowsMultipleSelection:NO];
	[_tableView setAllowsColumnReordering:NO];
	[_tableView setTarget:self];
	[_tableView setDoubleAction:@selector(addWatchPressed:)];
	[_tableView setAutoresizingMask:NSViewWidthSizable];
	[_tableScrollView setDocumentView:_tableView];
	[_tableView release];

	NSArray *columns = [NSArray arrayWithObjects:@"Address", @"Value", @"Previous", @"Changes", nil];
	NSArray *widths  = [NSArray arrayWithObjects:[NSNumber numberWithDouble:120.0],
	                                             [NSNumber numberWithDouble:110.0],
	                                             [NSNumber numberWithDouble:110.0],
	                                             [NSNumber numberWithDouble:90.0], nil];
	for (NSUInteger i = 0; i < [columns count]; i++)
	{
		NSTableColumn *column = [[[NSTableColumn alloc] initWithIdentifier:[columns objectAtIndex:i]] autorelease];
		[[column headerCell] setStringValue:[columns objectAtIndex:i]];
		[column setWidth:[[widths objectAtIndex:i] doubleValue]];
		[column setMinWidth:60.0];
		[column setEditable:NO];
		if (i == 3)
		{
			[column setResizingMask:NSTableColumnAutoresizingMask];
		}
		[_tableView addTableColumn:column];
	}

	_sidebar = [[NSView alloc] initWithFrame:NSMakeRect(tableWidth, 0.0, kSidebarWidth, kWindowHeight)];
	[_sidebar setAutoresizingMask:(NSViewHeightSizable | NSViewMinXMargin)];
	[contentView addSubview:_sidebar];
	[_sidebar release];

	CGFloat cursor = kWindowHeight - kSidebarPadding;

	[self addLabelWithText:@"Search Type" atY:cursor - 18.0];
	_searchTypePopUp = [self addPopUpWithItems:[NSArray arrayWithObjects:
		@"Specific Value", @"Relative to Previous", @"Specific Address", @"Number of Changes", nil]
	                                      atY:cursor - kControlHeight];
	cursor -= kRowSpacing;

	[self addLabelWithText:@"Comparison" atY:cursor - 18.0];
	_operatorPopUp = [self addPopUpWithItems:[NSArray arrayWithObjects:
		@"= Equal", @"≠ Not Equal", @"< Less", @"> Greater", @"≤ Less or Equal",
		@"≥ Greater or Equal", @"Different By", @"Modulo Is", nil]
	                                    atY:cursor - kControlHeight];
	[_operatorPopUp setAction:@selector(operatorChanged:)];
	cursor -= kRowSpacing;

	[self addLabelWithText:@"Value" atY:cursor - 18.0];
	_valueField = [self addTextFieldAtY:cursor - kControlHeight];
	[_valueField setStringValue:@"0"];
	cursor -= kRowSpacing;

	[self addLabelWithText:@"Parameter" atY:cursor - 18.0];
	_parameterField = [self addTextFieldAtY:cursor - kControlHeight];
	[_parameterField setStringValue:@"0"];
	cursor -= kRowSpacing;

	[self addLabelWithText:@"Size" atY:cursor - 18.0];
	_sizePopUp = [self addPopUpWithItems:[NSArray arrayWithObjects:
		@"1 Byte", @"2 Bytes", @"4 Bytes", nil]
	                              atY:cursor - kControlHeight];
	cursor -= kRowSpacing;

	[self addLabelWithText:@"Display" atY:cursor - 18.0];
	_displayPopUp = [self addPopUpWithItems:[NSArray arrayWithObjects:
		@"Signed", @"Unsigned", @"Hexadecimal", @"20.12 Fixed", nil]
	                                  atY:cursor - kControlHeight];
	cursor -= kRowSpacing;

	_misalignButton = [self addCheckboxWithTitle:@"Allow misaligned addresses"
	                                        atY:cursor - 22.0
	                                      action:@selector(optionsChanged:)];
	cursor -= 28.0;
	_littleEndianButton = [self addCheckboxWithTitle:@"Little endian values"
	                                             atY:cursor - 22.0
	                                           action:@selector(optionsChanged:)];
	cursor -= 32.0;

	_searchButton = [self addFullWidthButtonWithTitle:@"Search" atY:cursor - 26.0 action:@selector(searchPressed:)];
	cursor -= 34.0;
	_resetButton = [self addFullWidthButtonWithTitle:@"Reset" atY:cursor - 26.0 action:@selector(resetPressed:)];
	cursor -= 34.0;
	_addWatchButton = [self addFullWidthButtonWithTitle:@"Add Watch" atY:cursor - 26.0 action:@selector(addWatchPressed:)];
	cursor -= 36.0;

	_statusLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(kSidebarPadding, cursor - 20.0,
	                                                             kSidebarWidth - (2.0 * kSidebarPadding), 20.0)];
	[_statusLabel setEditable:NO];
	[_statusLabel setSelectable:NO];
	[_statusLabel setBezeled:NO];
	[_statusLabel setDrawsBackground:NO];
	[_statusLabel setAlignment:NSTextAlignmentCenter];
	[_statusLabel setFont:[NSFont systemFontOfSize:11.0]];
	[_statusLabel setTextColor:[NSColor secondaryLabelColor]];
	[_statusLabel setAutoresizingMask:NSViewMinYMargin];
	[_sidebar addSubview:_statusLabel];
	[_statusLabel release];

	[self updateParameterField];
	[self updateStatusLabel];
}

#pragma mark Window lifecycle

- (void) showRAMSearchWindow
{
	if (!_didReset)
	{
		_didReset = YES;
		[self syncOptionsFromUI];
		RAMSearchCore::Get().Reset();
		[self updateInterface];
	}

	[self showWindow:self];
	[[self window] makeKeyAndOrderFront:self];
	[self startRefreshTimer];
}

- (void) windowWillClose:(NSNotification *)notification
{
	[self stopRefreshTimer];
}

- (void) startRefreshTimer
{
	if (_refreshTimer != nil)
	{
		return;
	}
	_refreshTimer = [[NSTimer timerWithTimeInterval:0.15
	                                         target:self
	                                       selector:@selector(refreshTimerFired:)
	                                       userInfo:nil
	                                        repeats:YES] retain];
	[[NSRunLoop currentRunLoop] addTimer:_refreshTimer forMode:NSRunLoopCommonModes];
}

- (void) stopRefreshTimer
{
	if (_refreshTimer == nil)
	{
		return;
	}
	[_refreshTimer invalidate];
	[_refreshTimer release];
	_refreshTimer = nil;
}

- (void) refreshTimerFired:(NSTimer *)timer
{
	if (![[self window] isVisible])
	{
		return;
	}

	RAMSearchCore& core = RAMSearchCore::Get();
	const size_t count = core.ResultCount();
	if (count == 0)
	{
		return;
	}

	// A first search can hold four million candidates; refresh those less
	// often so the UI thread is not spending all its time walking RAM.
	_refreshTick++;
	const NSUInteger interval = (count > 1000000) ? 6 : (count > 200000) ? 2 : 1;
	if ((_refreshTick % interval) != 0)
	{
		return;
	}

	core.SignalNewFrame();
	NSRange visible = [_tableView rowsInRect:[_tableView visibleRect]];
	if (visible.length == 0)
	{
		return;
	}
	NSIndexSet *columns = [NSIndexSet indexSetWithIndexesInRange:NSMakeRange(1, 3)];
	[_tableView reloadDataForRowIndexes:[NSIndexSet indexSetWithIndexesInRange:visible]
	                    columnIndexes:columns];
}

#pragma mark Table view

- (NSInteger) numberOfRowsInTableView:(NSTableView *)tableView
{
	return (NSInteger)RAMSearchCore::Get().ResultCount();
}

- (id) tableView:(NSTableView *)tableView objectValueForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row
{
	RAMSearchCore& core = RAMSearchCore::Get();
	if ((row < 0) || ((size_t)row >= core.ResultCount()))
	{
		return @"";
	}

	const RAMSearchCore::Result result = core.ResultAt((size_t)row);
	const char sizeCode = [self sizeCode];
	const char typeCode = [self displayTypeCode];
	NSString *columnID = [tableColumn identifier];

	if ([columnID isEqualToString:@"Address"])
	{
		return RAMSearchString(RAMDebugCore::FormatAddress(result.address));
	}
	if ([columnID isEqualToString:@"Value"])
	{
		return RAMSearchString(RAMDebugCore::FormatValue(sizeCode, typeCode, result.currentValue));
	}
	if ([columnID isEqualToString:@"Previous"])
	{
		return RAMSearchString(RAMDebugCore::FormatValue(sizeCode, typeCode, result.previousValue));
	}
	return [NSString stringWithFormat:@"%u", (unsigned)result.changeCount];
}

- (void) tableView:(NSTableView *)tableView willDisplayCell:(id)cell
    forTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row
{
	if ([cell isKindOfClass:[NSTextFieldCell class]])
	{
		[cell setFont:RAMSearchMonospacedFont(12.0)];
		[cell setTextColor:[NSColor labelColor]];
	}
}

- (void) tableViewSelectionDidChange:(NSNotification *)notification
{
	[_addWatchButton setEnabled:([_tableView selectedRow] >= 0)];
}

#pragma mark Options

- (char) sizeCode
{
	const NSInteger index = [_sizePopUp indexOfSelectedItem];
	return kSizeCodes[(index >= 0 && index < 3) ? index : 0];
}

- (char) displayTypeCode
{
	const NSInteger index = [_displayPopUp indexOfSelectedItem];
	return kDisplayCodes[(index >= 0 && index < 4) ? index : 0];
}

- (int32_t) parseValueFromField:(NSTextField *)field
{
	NSString *text = [[field stringValue] stringByTrimmingCharactersInSet:
	                  [NSCharacterSet whitespaceAndNewlineCharacterSet]];
	if ([text length] == 0)
	{
		return 0;
	}

	const char *cString = [text UTF8String];
	const RAMSearchCore::ValueType type = (RAMSearchCore::ValueType)[_displayPopUp indexOfSelectedItem];
	const bool hasHexPrefix = ([text rangeOfString:@"0x" options:NSCaseInsensitiveSearch].location != NSNotFound);
	const int base = ((type == RAMSearchCore::VALUE_HEX) || hasHexPrefix) ? 16 : 10;
	return (int32_t)strtol(cString, 0, base);
}

- (void) updateParameterField
{
	const NSInteger op = [_operatorPopUp indexOfSelectedItem];
	const BOOL needed = (op == RAMSearchCore::COMPARE_DIFFERENT_BY) ||
	                    (op == RAMSearchCore::COMPARE_MODULO);
	[_parameterField setEnabled:needed];
}

- (void) syncOptionsFromUI
{
	RAMSearchCore& core = RAMSearchCore::Get();
	core.SetSize([self sizeCode]);
	core.SetMisaligned([_misalignButton state] == NSControlStateValueOn);
	core.SetLittleEndian([_littleEndianButton state] == NSControlStateValueOn);
	core.SetValueType((RAMSearchCore::ValueType)[_displayPopUp indexOfSelectedItem]);
	core.SetSearchType((RAMSearchCore::SearchType)[_searchTypePopUp indexOfSelectedItem]);
	core.SetCompareOperator((RAMSearchCore::CompareOperator)[_operatorPopUp indexOfSelectedItem]);
	core.SetSearchValue([self parseValueFromField:_valueField]);
	core.SetParameter([self parseValueFromField:_parameterField]);
}

- (void) optionsChanged:(id)sender
{
	[self syncOptionsFromUI];
	[self updateParameterField];
	[self updateInterface];
}

- (void) operatorChanged:(id)sender
{
	[self updateParameterField];
	[self syncOptionsFromUI];
}

#pragma mark Actions

- (void) searchPressed:(id)sender
{
	[self syncOptionsFromUI];
	RAMSearchCore::Get().Search();
	[self updateInterface];
}

- (void) resetPressed:(id)sender
{
	[self syncOptionsFromUI];
	RAMSearchCore::Get().Reset();
	[self updateInterface];
}

- (void) postAddWatchNotificationForRow:(NSInteger)row
{
	RAMSearchCore& core = RAMSearchCore::Get();
	if ((row < 0) || ((size_t)row >= core.ResultCount()))
	{
		return;
	}

	const RAMSearchCore::Result result = core.ResultAt((size_t)row);
	NSDictionary *info = [NSDictionary dictionaryWithObjectsAndKeys:
		[NSNumber numberWithUnsignedInt:result.address], @"address",
		[NSNumber numberWithChar:[self sizeCode]], @"size",
		[NSNumber numberWithChar:[self displayTypeCode]], @"type",
		@"", @"comment", nil];
	[[NSNotificationCenter defaultCenter] postNotificationName:kRAMWatchAddFromSearchNotification
	                                                    object:self
	                                                  userInfo:info];
}

- (void) addWatchPressed:(id)sender
{
	[self postAddWatchNotificationForRow:[_tableView selectedRow]];
}

#pragma mark State

- (void) updateInterface
{
	[_tableView reloadData];
	[self updateStatusLabel];
	[_addWatchButton setEnabled:([_tableView selectedRow] >= 0)];
}

- (void) updateStatusLabel
{
	const size_t count = RAMSearchCore::Get().ResultCount();
	if (count > 1000000)
	{
		[_statusLabel setStringValue:[NSString stringWithFormat:@"%lu results (new search)",
		                              (unsigned long)count]];
	}
	else
	{
		[_statusLabel setStringValue:[NSString stringWithFormat:@"%lu result%@",
		                              (unsigned long)count, (count == 1) ? @"" : @"s"]];
	}
}

@end
