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
#import "cheatWindowDelegate.h"

#include <stdlib.h>
#include <string>

#include "../../shared/ram_debug_core.h"
#include "../../shared/ram_search_core.h"

// The window mirrors the Windows dialog: results and the action column on top,
// the search criteria in four group boxes below. All four boxes keep their
// height; only the result list grows when the window is resized.
static const CGFloat kWindowWidth       = 560.0;
static const CGFloat kWindowHeight      = 660.0;
static const CGFloat kMinWindowWidth    = 540.0;
static const CGFloat kMinWindowHeight   = 600.0;

static const CGFloat kPadding           = 12.0;
static const CGFloat kButtonColumnWidth = 140.0;
static const CGFloat kButtonHeight      = 26.0;
static const CGFloat kButtonGap         = 6.0;

static const CGFloat kOptionsHeight     = 282.0;
static const CGFloat kOperatorWidth     = 196.0;
static const CGFloat kDataSizeWidth     = 156.0;
static const CGFloat kColumnGap         = 12.0;
static const CGFloat kCompareHeight     = 130.0;
static const CGFloat kDataRowHeight     = 144.0;
static const CGFloat kRadioHeight       = 20.0;

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
- (void) buildResultListWithWidth:(CGFloat)width height:(CGFloat)height y:(CGFloat)y;
- (void) buildButtonColumnWithWidth:(CGFloat)width height:(CGFloat)height y:(CGFloat)y;
- (void) buildOptionsViewWithWidth:(CGFloat)width;
- (void) buildOperatorBoxWithHeight:(CGFloat)height;
- (void) buildCompareBoxWithFrame:(NSRect)frame;
- (void) buildDataSizeBoxWithFrame:(NSRect)frame;
- (void) buildDataDisplayBoxWithFrame:(NSRect)frame;
- (NSBox *) addGroupBoxWithTitle:(NSString *)title frame:(NSRect)frame;
- (NSButton *) addRadioWithTitle:(NSString *)title tag:(NSInteger)tag frame:(NSRect)frame action:(SEL)action toView:(NSView *)parent;
- (NSButton *) addCheckboxWithTitle:(NSString *)title frame:(NSRect)frame action:(SEL)action toView:(NSView *)parent;
- (NSTextField *) addFieldWithFrame:(NSRect)frame toView:(NSView *)parent;
- (NSTextField *) addSmallLabelWithText:(NSString *)text frame:(NSRect)frame toView:(NSView *)parent;
- (NSButton *) addColumnButtonWithTitle:(NSString *)title frame:(NSRect)frame action:(SEL)action;
- (void) selectRadioWithTag:(NSInteger)tag inView:(NSView *)parent;
- (void) deselectRadioSiblingsOf:(NSButton *)sender;
- (void) startRefreshTimer;
- (void) stopRefreshTimer;
- (void) refreshTimerFired:(NSTimer *)timer;
- (void) updateInterface;
- (void) updateTitle;
- (void) updateOptionFields;
- (void) updateActionButtons;
- (void) syncOptionsFromUI;
- (int32_t) parseValueFromField:(NSTextField *)field;
- (uint32_t) parseAddressFromField:(NSTextField *)field;
- (int32_t) parseCountFromField:(NSTextField *)field;
- (uint32_t) searchValueFromUI;
- (int32_t) parameterFromUI;
- (char) displayTypeCode;
- (char) sizeCode;
- (void) runAutoSearchStep;
- (void) handleAutoSearchExhausted;
- (void) optionsChanged:(id)sender;
- (void) searchTypePicked:(NSButton *)sender;
- (void) operatorPicked:(NSButton *)sender;
- (void) sizePicked:(NSButton *)sender;
- (void) displayPicked:(NSButton *)sender;
- (void) autoSearchPicked:(id)sender;
- (void) searchPressed:(id)sender;
- (void) resetPressed:(id)sender;
- (void) clearChangesPressed:(id)sender;
- (void) undoPressed:(id)sender;
- (void) redoPressed:(id)sender;
- (void) pokePressed:(id)sender;
- (void) addWatchPressed:(id)sender;
- (void) addCheatPressed:(id)sender;
- (void) eliminatePressed:(id)sender;
- (void) postAddWatchNotificationForRow:(NSInteger)row;
- (void) postAddCheatNotificationForRow:(NSInteger)row;
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
	[window setMinSize:NSMakeSize(kMinWindowWidth, kMinWindowHeight)];
	[window setContentMinSize:NSMakeSize(kMinWindowWidth, kMinWindowHeight)];

	self = [super initWithWindow:window];
	[window release];
	if (self == nil)
	{
		return nil;
	}

	_refreshTimer = nil;
	_refreshTick = 0;
	_didReset = NO;
	_autoSearchEnabled = NO;
	_autoSearchAutoRetry = NO;
	_isShowingAutoSearchAlert = NO;
	_radioButtons = [[NSMutableArray alloc] init];

	_searchTypeTag = RAMSearchCore::SEARCH_SPECIFIC_VALUE;
	_operatorTag   = RAMSearchCore::COMPARE_EQUAL;
	_sizeTag       = 0;
	_displayTag    = 0;

	[window setDelegate:self];
	[self buildInterface];
	return self;
}

- (void)dealloc
{
	[self stopRefreshTimer];
	[_radioButtons release];
	[super dealloc];
}

#pragma mark Interface construction

- (NSBox *) addGroupBoxWithTitle:(NSString *)title frame:(NSRect)frame
{
	NSBox *box = [[NSBox alloc] initWithFrame:frame];
	[box setBoxType:NSBoxPrimary];
	[box setTitle:title];
	[box setTitleFont:[NSFont systemFontOfSize:11.0]];
	[box setTitlePosition:NSAtTop];
	[_optionsView addSubview:box];
	[box release];
	return box;
}

- (NSButton *) addRadioWithTitle:(NSString *)title tag:(NSInteger)tag frame:(NSRect)frame
                          action:(SEL)action toView:(NSView *)parent
{
	NSButton *button = [[NSButton alloc] initWithFrame:frame];
	[button setButtonType:NSButtonTypeRadio];
	[button setTitle:title];
	[button setAlignment:NSTextAlignmentLeft];
	[button setTag:tag];
	[button setFont:[NSFont systemFontOfSize:12.0]];
	[button setTarget:self];
	[button setAction:action];
	// Rows span the group box, so they follow its width when the window is
	// resized. Rows that share their line with a text field opt out below.
	[button setAutoresizingMask:NSViewWidthSizable];
	[parent addSubview:button];
	[_radioButtons addObject:button];
	[button release];
	return button;
}

- (NSButton *) addCheckboxWithTitle:(NSString *)title frame:(NSRect)frame
                             action:(SEL)action toView:(NSView *)parent
{
	NSButton *button = [[NSButton alloc] initWithFrame:frame];
	[button setButtonType:NSButtonTypeSwitch];
	[button setTitle:title];
	[button setAlignment:NSTextAlignmentLeft];
	[button setFont:[NSFont systemFontOfSize:12.0]];
	[button setTarget:self];
	[button setAction:action];
	[button setAutoresizingMask:NSViewWidthSizable];
	[parent addSubview:button];
	[button release];
	return button;
}

- (NSTextField *) addFieldWithFrame:(NSRect)frame toView:(NSView *)parent
{
	NSTextField *field = [[NSTextField alloc] initWithFrame:frame];
	[field setFont:RAMSearchMonospacedFont(11.0)];
	[parent addSubview:field];
	[field release];
	return field;
}

- (NSTextField *) addSmallLabelWithText:(NSString *)text frame:(NSRect)frame toView:(NSView *)parent
{
	NSTextField *label = [[NSTextField alloc] initWithFrame:frame];
	[label setStringValue:text];
	[label setEditable:NO];
	[label setSelectable:NO];
	[label setBezeled:NO];
	[label setDrawsBackground:NO];
	[label setFont:[NSFont systemFontOfSize:11.0]];
	[label setTextColor:[NSColor secondaryLabelColor]];
	[parent addSubview:label];
	[label release];
	return label;
}

- (NSButton *) addColumnButtonWithTitle:(NSString *)title frame:(NSRect)frame action:(SEL)action
{
	NSButton *button = [[NSButton alloc] initWithFrame:frame];
	[button setTitle:title];
	[button setBezelStyle:NSBezelStyleRounded];
	[button setFont:[NSFont systemFontOfSize:11.0]];
	[button setTarget:self];
	[button setAction:action];
	[button setAutoresizingMask:NSViewMinYMargin];
	[_buttonColumn addSubview:button];
	[button release];
	return button;
}

- (void) selectRadioWithTag:(NSInteger)tag inView:(NSView *)parent
{
	for (NSButton *button in _radioButtons)
	{
		// Radios are hosted by the group box's content view rather than by the
		// box itself, so accept a button one level below it as well.
		NSView *owner = [button superview];
		if ((owner != parent) && ([owner superview] != parent))
		{
			continue;
		}

		if ([button tag] == tag)
		{
			[button setState:NSControlStateValueOn];
		}
	}
}

- (void) deselectRadioSiblingsOf:(NSButton *)sender
{
	for (NSButton *other in _radioButtons)
	{
		if ((other != sender) && ([other superview] == [sender superview]))
		{
			[other setState:NSControlStateValueOff];
		}
	}
}

- (void) buildInterface
{
	const CGFloat contentWidth = kWindowWidth;
	const CGFloat tableWidth = contentWidth - (3.0 * kPadding) - kButtonColumnWidth;
	const CGFloat tableHeight = kWindowHeight - kOptionsHeight - (3.0 * kPadding);
	const CGFloat tableY = kPadding + kOptionsHeight + kPadding;

	[self buildResultListWithWidth:tableWidth height:tableHeight y:tableY];
	[self buildButtonColumnWithWidth:kButtonColumnWidth height:tableHeight y:tableY];
	[self buildOptionsViewWithWidth:(contentWidth - (2.0 * kPadding))];

	[self updateOptionFields];
	[self updateActionButtons];
}

- (void) buildResultListWithWidth:(CGFloat)width height:(CGFloat)height y:(CGFloat)y
{
	NSView *contentView = [[self window] contentView];

	_tableScrollView = [[NSScrollView alloc] initWithFrame:NSMakeRect(kPadding, y, width, height)];
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
	// Address needs room for eight hex digits; the other three hold a value of
	// at most four digits, so they stay narrow and the rest of the row is left
	// empty, like the Windows dialog.
	const CGFloat widths[4] = { 87.0, 60.0, 62.0, 68.0 };
	for (NSUInteger i = 0; i < [columns count]; i++)
	{
		NSTableColumn *column = [[[NSTableColumn alloc] initWithIdentifier:[columns objectAtIndex:i]] autorelease];
		[[column headerCell] setStringValue:[columns objectAtIndex:i]];
		[column setWidth:widths[i]];
		[column setMinWidth:50.0];
		[column setEditable:NO];
		[_tableView addTableColumn:column];
	}
	[_tableView setColumnAutoresizingStyle:NSTableViewNoColumnAutoresizing];
}

- (void) buildButtonColumnWithWidth:(CGFloat)width height:(CGFloat)height y:(CGFloat)y
{
	NSView *contentView = [[self window] contentView];

	_buttonColumn = [[NSView alloc] initWithFrame:NSMakeRect(kWindowWidth - kPadding - width, y, width, height)];
	[_buttonColumn setAutoresizingMask:(NSViewMinXMargin | NSViewMinYMargin)];
	[contentView addSubview:_buttonColumn];
	[_buttonColumn release];

	// Same order as the Windows dialog, with Undo/Redo side by side so the
	// column still fits: Search, Reset, Clear Change Counts, Undo, Redo,
	// Eliminate, Poke, Watch, Add Cheat.
	CGFloat cursor = height - kButtonHeight;
	const CGFloat halfWidth = (width - kButtonGap) / 2.0;

	[self addColumnButtonWithTitle:@"Search" frame:NSMakeRect(0.0, cursor, width, kButtonHeight) action:@selector(searchPressed:)];
	cursor -= kButtonHeight + kButtonGap;

	[self addColumnButtonWithTitle:@"Reset" frame:NSMakeRect(0.0, cursor, width, kButtonHeight) action:@selector(resetPressed:)];
	cursor -= kButtonHeight + kButtonGap;

	[self addColumnButtonWithTitle:@"Clear Change Counts" frame:NSMakeRect(0.0, cursor, width, kButtonHeight) action:@selector(clearChangesPressed:)];
	cursor -= kButtonHeight + kButtonGap;

	_undoButton = [self addColumnButtonWithTitle:@"Undo" frame:NSMakeRect(0.0, cursor, halfWidth, kButtonHeight) action:@selector(undoPressed:)];
	_redoButton = [self addColumnButtonWithTitle:@"Redo" frame:NSMakeRect(halfWidth + kButtonGap, cursor, halfWidth, kButtonHeight) action:@selector(redoPressed:)];
	cursor -= kButtonGap;

	// Separator between Undo/Redo and Eliminate. It gets the same amount of air
	// above and below as the buttons get between each other, so it reads as a
	// separator instead of a line squeezed against a button. Buttons are placed
	// by their bottom edge, so the row after the separator has to step down by
	// the extra space the separator needs.
	NSBox *divider = [[[NSBox alloc] initWithFrame:NSMakeRect(0.0, cursor - 1.0, width, 1.0)] autorelease];
	[divider setBoxType:NSBoxSeparator];
	[divider setAutoresizingMask:NSViewMinYMargin];
	[_buttonColumn addSubview:divider];

	cursor -= 1.0 + kButtonGap;
	cursor -= kButtonHeight;

	_eliminateButton = [self addColumnButtonWithTitle:@"Eliminate" frame:NSMakeRect(0.0, cursor, width, kButtonHeight) action:@selector(eliminatePressed:)];
	cursor -= kButtonHeight + kButtonGap;

	_pokeButton = [self addColumnButtonWithTitle:@"Poke" frame:NSMakeRect(0.0, cursor, width, kButtonHeight) action:@selector(pokePressed:)];
	[_pokeButton setToolTip:@"Write the Specific Value into the selected address"];
	cursor -= kButtonHeight + kButtonGap;

	_addWatchButton = [self addColumnButtonWithTitle:@"Watch" frame:NSMakeRect(0.0, cursor, width, kButtonHeight) action:@selector(addWatchPressed:)];
	cursor -= kButtonHeight + kButtonGap;

	_addCheatButton = [self addColumnButtonWithTitle:@"Add Cheat" frame:NSMakeRect(0.0, cursor, width, kButtonHeight) action:@selector(addCheatPressed:)];
}

- (void) buildOptionsViewWithWidth:(CGFloat)width
{
	NSView *contentView = [[self window] contentView];

	_optionsView = [[NSView alloc] initWithFrame:NSMakeRect(kPadding, kPadding, width, kOptionsHeight)];
	[_optionsView setAutoresizingMask:(NSViewWidthSizable | NSViewMaxYMargin)];
	[contentView addSubview:_optionsView];
	[_optionsView release];

	[self buildOperatorBoxWithHeight:kOptionsHeight];

	const CGFloat rightX = kOperatorWidth + kColumnGap;
	const CGFloat rightWidth = width - rightX;

	[self buildCompareBoxWithFrame:NSMakeRect(rightX, kOptionsHeight - kCompareHeight, rightWidth, kCompareHeight)];
	[self buildDataSizeBoxWithFrame:NSMakeRect(rightX, 0.0, kDataSizeWidth, kDataRowHeight)];
	[self buildDataDisplayBoxWithFrame:NSMakeRect(rightX + kDataSizeWidth + kColumnGap, 0.0,
	                                              width - (rightX + kDataSizeWidth + kColumnGap), kDataRowHeight)];
}

- (void) buildOperatorBoxWithHeight:(CGFloat)height
{
	NSBox *box = [self addGroupBoxWithTitle:@"Comparison Operator"
	                                  frame:NSMakeRect(0.0, 0.0, kOperatorWidth, height)];
	const CGFloat rowX = 12.0;
	const CGFloat rowWidth = kOperatorWidth - 24.0;
	CGFloat rowY = height - 34.0 - kRadioHeight;
	const CGFloat rowStep = 30.0;

	[self addRadioWithTitle:@"Less Than" tag:RAMSearchCore::COMPARE_LESS_THAN
	                  frame:NSMakeRect(rowX, rowY, rowWidth, kRadioHeight) action:@selector(operatorPicked:) toView:box];
	rowY -= rowStep;
	[self addRadioWithTitle:@"Greater Than" tag:RAMSearchCore::COMPARE_GREATER_THAN
	                  frame:NSMakeRect(rowX, rowY, rowWidth, kRadioHeight) action:@selector(operatorPicked:) toView:box];
	rowY -= rowStep;
	[self addRadioWithTitle:@"Less Than or Equal To" tag:RAMSearchCore::COMPARE_LESS_OR_EQUAL
	                  frame:NSMakeRect(rowX, rowY, rowWidth, kRadioHeight) action:@selector(operatorPicked:) toView:box];
	rowY -= rowStep;
	[self addRadioWithTitle:@"Greater Than or Equal To" tag:RAMSearchCore::COMPARE_GREATER_OR_EQUAL
	                  frame:NSMakeRect(rowX, rowY, rowWidth, kRadioHeight) action:@selector(operatorPicked:) toView:box];
	rowY -= rowStep;
	[self addRadioWithTitle:@"Equal To" tag:RAMSearchCore::COMPARE_EQUAL
	                  frame:NSMakeRect(rowX, rowY, rowWidth, kRadioHeight) action:@selector(operatorPicked:) toView:box];
	rowY -= rowStep;
	[self addRadioWithTitle:@"Not Equal To" tag:RAMSearchCore::COMPARE_NOT_EQUAL
	                  frame:NSMakeRect(rowX, rowY, rowWidth, kRadioHeight) action:@selector(operatorPicked:) toView:box];
	rowY -= rowStep;

	NSButton *differentByRadio = [self addRadioWithTitle:@"Different By:" tag:RAMSearchCore::COMPARE_DIFFERENT_BY
	                                               frame:NSMakeRect(rowX, rowY, 100.0, kRadioHeight) action:@selector(operatorPicked:) toView:box];
	[differentByRadio setAutoresizingMask:NSViewNotSizable];
	_differentByField = [self addFieldWithFrame:NSMakeRect(rowX + 104.0, rowY - 1.0, rowWidth - 104.0, 22.0) toView:box];
	[_differentByField setStringValue:@"0"];
	rowY -= rowStep;

	NSButton *moduloRadio = [self addRadioWithTitle:@"Modulo" tag:RAMSearchCore::COMPARE_MODULO
	                                          frame:NSMakeRect(rowX, rowY, 70.0, kRadioHeight) action:@selector(operatorPicked:) toView:box];
	[moduloRadio setAutoresizingMask:NSViewNotSizable];
	_moduloField = [self addFieldWithFrame:NSMakeRect(rowX + 74.0, rowY - 1.0, 60.0, 22.0) toView:box];
	[_moduloField setStringValue:@"0"];
	[self addSmallLabelWithText:@"Is" frame:NSMakeRect(rowX + 142.0, rowY + 2.0, 22.0, 16.0) toView:box];

	[self selectRadioWithTag:_operatorTag inView:box];
}

- (void) buildCompareBoxWithFrame:(NSRect)frame
{
	NSBox *box = [self addGroupBoxWithTitle:@"Compare To / By" frame:frame];
	[box setAutoresizingMask:NSViewWidthSizable];

	const CGFloat rowX = 12.0;
	CGFloat rowY = frame.size.height - 34.0 - kRadioHeight;
	const CGFloat rowStep = 22.0;
	const CGFloat fieldWidth = frame.size.width - rowX - 158.0 - 21.0;

	// The radios keep their width: each one shares its line with a field, and
	// the fields are what stretches with the box.
	NSButton *radio = [self addRadioWithTitle:@"Previous Value" tag:RAMSearchCore::SEARCH_RELATIVE_VALUE
	                                    frame:NSMakeRect(rowX, rowY, 150.0, kRadioHeight) action:@selector(searchTypePicked:) toView:box];
	[radio setAutoresizingMask:NSViewNotSizable];
	rowY -= rowStep;
	radio = [self addRadioWithTitle:@"Specific Value:" tag:RAMSearchCore::SEARCH_SPECIFIC_VALUE
	                          frame:NSMakeRect(rowX, rowY, 150.0, kRadioHeight) action:@selector(searchTypePicked:) toView:box];
	[radio setAutoresizingMask:NSViewNotSizable];
	_specificValueField = [self addFieldWithFrame:NSMakeRect(rowX + 158.0, rowY, fieldWidth, 20.0) toView:box];
	[_specificValueField setAutoresizingMask:NSViewWidthSizable];
	[_specificValueField setStringValue:@"0"];
	rowY -= rowStep;
	radio = [self addRadioWithTitle:@"Specific Address:" tag:RAMSearchCore::SEARCH_SPECIFIC_ADDRESS
	                          frame:NSMakeRect(rowX, rowY, 150.0, kRadioHeight) action:@selector(searchTypePicked:) toView:box];
	[radio setAutoresizingMask:NSViewNotSizable];
	_specificAddressField = [self addFieldWithFrame:NSMakeRect(rowX + 158.0, rowY, fieldWidth, 20.0) toView:box];
	[_specificAddressField setAutoresizingMask:NSViewWidthSizable];
	[_specificAddressField setStringValue:@"02000000"];
	rowY -= rowStep;
	radio = [self addRadioWithTitle:@"Number of Changes:" tag:RAMSearchCore::SEARCH_NUMBER_OF_CHANGES
	                          frame:NSMakeRect(rowX, rowY, 150.0, kRadioHeight) action:@selector(searchTypePicked:) toView:box];
	[radio setAutoresizingMask:NSViewNotSizable];
	_changeCountField = [self addFieldWithFrame:NSMakeRect(rowX + 158.0, rowY, fieldWidth, 20.0) toView:box];
	[_changeCountField setAutoresizingMask:NSViewWidthSizable];
	[_changeCountField setStringValue:@"0"];

	[self selectRadioWithTag:_searchTypeTag inView:box];
}

- (void) buildDataSizeBoxWithFrame:(NSRect)frame
{
	NSBox *box = [self addGroupBoxWithTitle:@"Data Size" frame:frame];
	const CGFloat rowX = 12.0;
	const CGFloat rowWidth = frame.size.width - 24.0;
	CGFloat rowY = frame.size.height - 34.0 - kRadioHeight;
	const CGFloat rowStep = 22.0;

	[self addRadioWithTitle:@"1 byte" tag:0 frame:NSMakeRect(rowX, rowY, rowWidth, kRadioHeight)
	                 action:@selector(sizePicked:) toView:box];
	rowY -= rowStep;
	[self addRadioWithTitle:@"2 bytes" tag:1 frame:NSMakeRect(rowX, rowY, rowWidth, kRadioHeight)
	                 action:@selector(sizePicked:) toView:box];
	rowY -= rowStep;
	[self addRadioWithTitle:@"4 bytes" tag:2 frame:NSMakeRect(rowX, rowY, rowWidth, kRadioHeight)
	                 action:@selector(sizePicked:) toView:box];
	rowY -= rowStep;

	_misalignButton = [self addCheckboxWithTitle:@"Check Misaligned" frame:NSMakeRect(rowX, rowY, rowWidth, kRadioHeight)
	                                      action:@selector(optionsChanged:) toView:box];
	rowY -= rowStep;
	_bigEndianButton = [self addCheckboxWithTitle:@"Big-endian values" frame:NSMakeRect(rowX, rowY, rowWidth, kRadioHeight)
	                                        action:@selector(optionsChanged:) toView:box];
	// The DS stores multi-byte values little endian, which is what an unchecked
	// box means, so big endian starts off.

	[self selectRadioWithTag:_sizeTag inView:box];
}

- (void) buildDataDisplayBoxWithFrame:(NSRect)frame
{
	NSBox *box = [self addGroupBoxWithTitle:@"Data Type / Display" frame:frame];
	[box setAutoresizingMask:NSViewWidthSizable];

	const CGFloat rowX = 12.0;
	const CGFloat rowWidth = frame.size.width - 24.0;
	CGFloat rowY = frame.size.height - 34.0 - kRadioHeight;
	const CGFloat rowStep = 22.0;

	[self addRadioWithTitle:@"Signed" tag:RAMSearchCore::VALUE_SIGNED frame:NSMakeRect(rowX, rowY, rowWidth, kRadioHeight)
	                 action:@selector(displayPicked:) toView:box];
	rowY -= rowStep;
	[self addRadioWithTitle:@"Unsigned" tag:RAMSearchCore::VALUE_UNSIGNED frame:NSMakeRect(rowX, rowY, rowWidth, kRadioHeight)
	                 action:@selector(displayPicked:) toView:box];
	rowY -= rowStep;
	[self addRadioWithTitle:@"Hexadecimal" tag:RAMSearchCore::VALUE_HEX frame:NSMakeRect(rowX, rowY, rowWidth, kRadioHeight)
	                 action:@selector(displayPicked:) toView:box];
	rowY -= rowStep;
	NSButton *fixedRadio = [self addRadioWithTitle:@"20.12 Fixed" tag:RAMSearchCore::VALUE_FIXED_20_12 frame:NSMakeRect(rowX, rowY, rowWidth, kRadioHeight)
	                                       action:@selector(displayPicked:) toView:box];
	[fixedRadio setToolTip:@"Fixed point with 20 integer bits and 12 fraction bits: "
	                       @"1.0 is stored as 4096, so the value is shown divided by 4096."];
	rowY -= rowStep;

	_autoSearchButton = [self addCheckboxWithTitle:@"Autosearch" frame:NSMakeRect(rowX, rowY, rowWidth, kRadioHeight)
	                                        action:@selector(autoSearchPicked:) toView:box];

	[self selectRadioWithTag:_displayTag inView:box];
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
	if (![[self window] isVisible] || _isShowingAutoSearchAlert)
	{
		return;
	}

	RAMSearchCore& core = RAMSearchCore::Get();
	const size_t count = core.ResultCount();
	if ((count == 0) && !_autoSearchEnabled)
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

	if (_autoSearchEnabled)
	{
		[self runAutoSearchStep];
	}

	NSRange visible = [_tableView rowsInRect:[_tableView visibleRect]];
	if (visible.length == 0)
	{
		return;
	}
	NSIndexSet *columns = [NSIndexSet indexSetWithIndexesInRange:NSMakeRange(1, 3)];
	[_tableView reloadDataForRowIndexes:[NSIndexSet indexSetWithIndexesInRange:visible]
	                    columnIndexes:columns];
}

#pragma mark Autosearch

- (void) runAutoSearchStep
{
	RAMSearchCore& core = RAMSearchCore::Get();
	if (core.ResultCount() == 0)
	{
		[self handleAutoSearchExhausted];
		return;
	}

	[self syncOptionsFromUI];

	const size_t before = core.ResultCount();
	core.Search();
	if (core.ResultCount() == before)
	{
		return;
	}

	[_tableView reloadData];
	[self updateTitle];
	[self updateActionButtons];
}

- (void) handleAutoSearchExhausted
{
	RAMSearchCore& core = RAMSearchCore::Get();

	if (_autoSearchAutoRetry)
	{
		core.Reset();
		[_tableView reloadData];
		[self updateTitle];
		[self updateActionButtons];
		return;
	}

	// The alert runs its own modal loop, in which this timer keeps firing.
	_isShowingAutoSearchAlert = YES;

	NSAlert *alert = [[[NSAlert alloc] init] autorelease];
	[alert setMessageText:@"Autosearch — out of results."];
	[alert setInformativeText:@"Retry resets the search once and keeps autosearching. "
	                           @"Ignore resets whenever necessary without asking again. "
	                           @"Stop turns autosearch off."];
	[alert addButtonWithTitle:@"Retry"];
	[alert addButtonWithTitle:@"Ignore"];
	[alert addButtonWithTitle:@"Stop"];

	const NSModalResponse response = [alert runModal];
	_isShowingAutoSearchAlert = NO;

	if (response == NSAlertThirdButtonReturn)
	{
		[_autoSearchButton setState:NSControlStateValueOff];
		_autoSearchEnabled = NO;
		_autoSearchAutoRetry = NO;
	}
	else
	{
		if (response == NSAlertSecondButtonReturn)
		{
			_autoSearchAutoRetry = YES;
		}
		core.Reset();
	}

	[_tableView reloadData];
	[self updateTitle];
	[self updateActionButtons];
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
	[self updateActionButtons];
}

#pragma mark Options

- (char) sizeCode
{
	return kSizeCodes[(_sizeTag >= 0 && _sizeTag < 3) ? _sizeTag : 0];
}

- (char) displayTypeCode
{
	return kDisplayCodes[(_displayTag >= 0 && _displayTag < 4) ? _displayTag : 0];
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
	const bool hasHexPrefix = ([text rangeOfString:@"0x" options:NSCaseInsensitiveSearch].location != NSNotFound);
	const int base = ((_displayTag == RAMSearchCore::VALUE_HEX) || hasHexPrefix) ? 16 : 10;
	return (int32_t)strtol(cString, 0, base);
}

- (uint32_t) parseAddressFromField:(NSTextField *)field
{
	NSString *text = [[field stringValue] stringByTrimmingCharactersInSet:
	                  [NSCharacterSet whitespaceAndNewlineCharacterSet]];
	if ([text length] == 0)
	{
		return 0;
	}

	const char *cString = [text UTF8String];
	if (([text length] > 2) && ([text characterAtIndex:0] == '0') &&
	    (([text characterAtIndex:1] == 'x') || ([text characterAtIndex:1] == 'X')))
	{
		cString += 2;
	}
	return (uint32_t)strtoul(cString, 0, 16);
}

- (int32_t) parseCountFromField:(NSTextField *)field
{
	NSString *text = [[field stringValue] stringByTrimmingCharactersInSet:
	                  [NSCharacterSet whitespaceAndNewlineCharacterSet]];
	if ([text length] == 0)
	{
		return 0;
	}
	return (int32_t)strtol([text UTF8String], 0, 10);
}

- (uint32_t) searchValueFromUI
{
	switch (_searchTypeTag)
	{
		case RAMSearchCore::SEARCH_SPECIFIC_VALUE:
			return (uint32_t)[self parseValueFromField:_specificValueField];

		case RAMSearchCore::SEARCH_SPECIFIC_ADDRESS:
			return [self parseAddressFromField:_specificAddressField];

		case RAMSearchCore::SEARCH_NUMBER_OF_CHANGES:
			return (uint32_t)[self parseCountFromField:_changeCountField];

		default:
			return 0; // "Previous Value" compares against the old value only.
	}
}

- (int32_t) parameterFromUI
{
	switch (_operatorTag)
	{
		case RAMSearchCore::COMPARE_DIFFERENT_BY:
			return [self parseValueFromField:_differentByField];

		case RAMSearchCore::COMPARE_MODULO:
			return [self parseValueFromField:_moduloField];

		default:
			return 0;
	}
}

- (void) updateOptionFields
{
	[_specificValueField setEnabled:(_searchTypeTag == RAMSearchCore::SEARCH_SPECIFIC_VALUE)];
	[_specificAddressField setEnabled:(_searchTypeTag == RAMSearchCore::SEARCH_SPECIFIC_ADDRESS)];
	[_changeCountField setEnabled:(_searchTypeTag == RAMSearchCore::SEARCH_NUMBER_OF_CHANGES)];

	[_differentByField setEnabled:(_operatorTag == RAMSearchCore::COMPARE_DIFFERENT_BY)];
	[_moduloField setEnabled:(_operatorTag == RAMSearchCore::COMPARE_MODULO)];
}

- (void) syncOptionsFromUI
{
	RAMSearchCore& core = RAMSearchCore::Get();
	core.SetSize([self sizeCode]);
	core.SetMisaligned([_misalignButton state] == NSControlStateValueOn);
	// The box asks for big endian, and the core stores the opposite.
	core.SetLittleEndian([_bigEndianButton state] != NSControlStateValueOn);
	core.SetValueType((RAMSearchCore::ValueType)_displayTag);
	core.SetSearchType((RAMSearchCore::SearchType)_searchTypeTag);
	core.SetCompareOperator((RAMSearchCore::CompareOperator)_operatorTag);
	core.SetSearchValue((int32_t)[self searchValueFromUI]);
	core.SetParameter([self parameterFromUI]);
}

- (void) optionsChanged:(id)sender
{
	[self syncOptionsFromUI];
	[self updateOptionFields];
	[self updateInterface];
}

- (void) searchTypePicked:(NSButton *)sender
{
	_searchTypeTag = [sender tag];
	[self deselectRadioSiblingsOf:sender];
	[self optionsChanged:sender];
}

- (void) operatorPicked:(NSButton *)sender
{
	_operatorTag = [sender tag];
	[self deselectRadioSiblingsOf:sender];
	[self optionsChanged:sender];
}

- (void) sizePicked:(NSButton *)sender
{
	_sizeTag = [sender tag];
	[self deselectRadioSiblingsOf:sender];
	[self optionsChanged:sender];
}

- (void) displayPicked:(NSButton *)sender
{
	_displayTag = [sender tag];
	[self deselectRadioSiblingsOf:sender];
	[self optionsChanged:sender];
}

- (void) autoSearchPicked:(id)sender
{
	_autoSearchEnabled = ([_autoSearchButton state] == NSControlStateValueOn);
	_autoSearchAutoRetry = NO;

	// Checking the box runs a search right away, like the Windows dialog.
	if (_autoSearchEnabled)
	{
		[self searchPressed:sender];
	}
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

- (void) clearChangesPressed:(id)sender
{
	RAMSearchCore::Get().ClearChangeCounts();
	[self updateInterface];
}

- (void) undoPressed:(id)sender
{
	if (!RAMSearchCore::Get().Undo())
	{
		NSBeep();
		return;
	}
	[_tableView deselectAll:self];
	[self updateInterface];
}

- (void) redoPressed:(id)sender
{
	if (!RAMSearchCore::Get().Redo())
	{
		NSBeep();
		return;
	}
	[_tableView deselectAll:self];
	[self updateInterface];
}

- (void) pokePressed:(id)sender
{
	RAMSearchCore& core = RAMSearchCore::Get();
	const NSInteger row = [_tableView selectedRow];
	if ((row < 0) || ((size_t)row >= core.ResultCount()))
	{
		return;
	}

	const RAMSearchCore::Result result = core.ResultAt((size_t)row);
	const int sizeInBytes = RAMDebugCore::SizeInBytes([self sizeCode]);
	const uint32_t value = (uint32_t)[self parseValueFromField:_specificValueField];

	if (!RAMDebugCore::WriteValue(result.address, value, sizeInBytes))
	{
		NSBeep();
		return;
	}

	// Refresh the row immediately so the write is visible without waiting for
	// the next tick.
	core.SignalNewFrame();
	[_tableView reloadDataForRowIndexes:[NSIndexSet indexSetWithIndex:(NSUInteger)row]
	                    columnIndexes:[NSIndexSet indexSetWithIndexesInRange:NSMakeRange(1, 3)]];
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

- (void) postAddCheatNotificationForRow:(NSInteger)row
{
	RAMSearchCore& core = RAMSearchCore::Get();
	if ((row < 0) || ((size_t)row >= core.ResultCount()))
	{
		return;
	}

	const RAMSearchCore::Result result = core.ResultAt((size_t)row);

	// Internal cheats are Action Replay constant writes into main RAM
	// (0x02xxxxxx), so anything else would silently freeze the wrong address.
	if ((result.address < RAMDebugCore::kMainMemAddress) ||
	    (result.address >= (RAMDebugCore::kMainMemAddress + RAMDebugCore::kMainMemSize)))
	{
		NSBeep();

		NSAlert *alert = [[[NSAlert alloc] init] autorelease];
		[alert setMessageText:@"Add Cheat"];
		[alert setInformativeText:@"Internal cheats can only target main RAM "
		                           @"(0x02000000 - 0x023FFFFF). The selected result is not in main RAM."];
		[alert addButtonWithTitle:@"OK"];
		[alert runModal];
		return;
	}

	// Take the value exactly as the result list is showing it, like the
	// Windows dialog does, rather than re-reading the bus with a different
	// endianness convention.
	const int sizeInBytes = RAMDebugCore::SizeInBytes([self sizeCode]);
	const uint32_t sizeMask = (sizeInBytes >= 4) ? 0xFFFFFFFFu : ((1u << (8 * sizeInBytes)) - 1u);
	const uint32_t value = result.currentValue & sizeMask;

	NSDictionary *info = [NSDictionary dictionaryWithObjectsAndKeys:
		[NSNumber numberWithUnsignedInt:result.address], @"address",
		[NSNumber numberWithUnsignedInt:(unsigned int)sizeInBytes], @"bytes",
		[NSNumber numberWithUnsignedInt:value], @"value",
		@"", @"description", nil];
	[[NSNotificationCenter defaultCenter] postNotificationName:kCheatManagerAddInternalCheatNotification
	                                                    object:self
	                                                  userInfo:info];
}

- (void) addCheatPressed:(id)sender
{
	[self postAddCheatNotificationForRow:[_tableView selectedRow]];
}

- (void) eliminatePressed:(id)sender
{
	const NSInteger row = [_tableView selectedRow];
	if (row < 0)
	{
		return;
	}

	RAMSearchCore& core = RAMSearchCore::Get();
	if (!core.Eliminate((size_t)row))
	{
		return;
	}

	[self updateInterface];

	// Keep the selection where the removed row used to be so that clicking
	// Eliminate repeatedly walks down the list.
	const NSInteger count = (NSInteger)core.ResultCount();
	if (count > 0)
	{
		const NSInteger nextRow = (row < count) ? row : (count - 1);
		[_tableView selectRowIndexes:[NSIndexSet indexSetWithIndex:(NSUInteger)nextRow]
		        byExtendingSelection:NO];
		[_tableView scrollRowToVisible:nextRow];
	}

	[self updateActionButtons];
}

#pragma mark State

- (void) updateInterface
{
	[_tableView reloadData];
	[self updateTitle];
	[self updateActionButtons];
}

- (void) updateTitle
{
	const size_t count = RAMSearchCore::Get().ResultCount();
	NSString *title = (count == 1)
	                ? @"RAM Search — 1 result"
	                : [NSString stringWithFormat:@"RAM Search — %lu results", (unsigned long)count];
	[[self window] setTitle:title];
}

- (void) updateActionButtons
{
	const NSInteger row = [_tableView selectedRow];
	const RAMSearchCore& core = RAMSearchCore::Get();
	const BOOL hasSelection = (row >= 0) && ((size_t)row < core.ResultCount());

	[_addWatchButton setEnabled:hasSelection];
	[_addCheatButton setEnabled:hasSelection];
	[_eliminateButton setEnabled:hasSelection];
	[_pokeButton setEnabled:hasSelection];
	[_undoButton setEnabled:core.CanUndo()];
	[_redoButton setEnabled:core.CanRedo()];
}

@end
