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

#import "MemoryViewerWindow.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Private helpers. These need to be visible to the compiler with their real
// return types; otherwise callers that appear earlier in the file assume id.
@interface MemoryHexView (Private)
- (u32) bytesPerUnit;
- (NSUInteger) hexDigitsPerUnit;
- (NSUInteger) unitsPerRow;
- (NSUInteger) rowCountForHeight:(CGFloat)height;
- (CGFloat) dataStartX;
- (CGFloat) unitWidth;
- (CGFloat) asciiStartX;
- (u32) valueAtOffset:(NSUInteger)offset;
- (void) clampViewport;
- (void) notifyViewportChanged;
- (NSString *) cellTextForValue:(u32)value;
- (NSString *) typingTextForCurrentInputWithDigits:(NSUInteger)digits;
- (void) inputHexDigit:(int)digit;
@end

@interface MemoryViewerWindowController (Private)
- (void) buildInterface;
- (void) buildBreakpointBar;
- (void) layoutViews;
- (void) startRefreshTimer;
- (void) stopRefreshTimer;
- (void) refreshTimerFired:(NSTimer *)timer;
- (void) updateAddressField:(u32)address;
- (void) regionChanged:(id)sender;
- (void) viewModeChanged:(id)sender;
- (void) goButtonPressed:(id)sender;
- (void) updateScroller;
- (void) scrollerChanged:(NSScroller *)sender;
- (void) addReadBreakpoint:(id)sender;
- (void) addWriteBreakpoint:(id)sender;
- (void) removeBreakpoint:(id)sender;
- (void) clearBreakpoints:(id)sender;
- (void) updateBreakpointSummary;
- (u32) breakpointAddressFromField;
- (void) loadAddressHistory;
- (void) addAddressToHistory:(u32)address;
- (void) saveAddressHistory;
- (void) exportSelected:(NSPopUpButton *)sender;
- (void) exportTextDump;
- (void) exportRawDump;
- (void) exportFullDump;
- (NSString *) runSavePanelWithDefaultName:(NSString *)name;
- (void) toggleBigEndian:(id)sender;
- (void) toggleFullCharacters:(id)sender;
- (void) updateSelectionLabel:(u32)address;
@end

// NSTextField draws a single line against the top of its frame, so a label that
// matches the height of the 24pt controls next to it ends up sitting high. This
// subclass centres the line itself, which keeps the labels on the same optical
// line as the popup buttons and push buttons in the same row.
@interface MemoryViewerLabel : NSTextField
@end

// Geometry of the hex dump. Everything is expressed in monospace character
// cells so that the layout stays stable when the user changes font sizes.
static const CGFloat kHexOuterMargin	= 10.0;
static const CGFloat kHexHeaderHeight	= 20.0;
static const NSUInteger kHexBytesPerRow	= 16;
static const NSUInteger kHexDefaultRowCount = 16;
static const NSUInteger kHexMaxRowCount	= 256;	// bounded by _memoryBuffer (0x1000 bytes)
static const u32 kHexDefaultWindowSize	= 0x100;

// The window keeps a fixed width so that the hex columns never reflow; only the
// height is user-adjustable, and the dump grows to fill it. The width is sized
// to the actual dump (76 character cells + margins + scroller), not a guess.
static const CGFloat kWindowWidth		= 600.0;
static const CGFloat kToolbarHeight		= 34.0;
static const CGFloat kBreakpointBarHeight	= 34.0;
static const CGFloat kScrollerWidth		= 15.0;

static NSFont *MemoryViewerMonospacedFont(CGFloat size)
{
	NSFont *font = [NSFont userFixedPitchFontOfSize:size];
	if (font == nil)
	{
		font = [NSFont fontWithName:@"Menlo" size:size];
	}
	if (font == nil)
	{
		font = [NSFont fontWithName:@"Courier" size:size];
	}
	
	return font;
}

static int HexDigitValue(unichar c)
{
	if (c >= '0' && c <= '9')
	{
		return (int)(c - '0');
	}
	if (c >= 'A' && c <= 'F')
	{
		return (int)(c - 'A') + 0x0A;
	}
	if (c >= 'a' && c <= 'f')
	{
		return (int)(c - 'a') + 0x0A;
	}
	
	return -1;
}

static BOOL ParseHexAddress(NSString *text, u32 *outAddress)
{
	if (text == nil)
	{
		return NO;
	}
	
	NSString *trimmed = [text stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
	if ([trimmed length] >= 1 && ([[trimmed substringFromIndex:[trimmed length] - 1] isEqualToString:@"h"] ||
								  [[trimmed substringFromIndex:[trimmed length] - 1] isEqualToString:@"H"]))
	{
		trimmed = [trimmed substringToIndex:[trimmed length] - 1];
	}
	if ([trimmed hasPrefix:@"0x"] || [trimmed hasPrefix:@"0X"])
	{
		trimmed = [trimmed substringFromIndex:2];
	}
	
	if ([trimmed length] == 0 || [trimmed length] > 8)
	{
		return NO;
	}
	
	u32 value = 0;
	for (NSUInteger i = 0; i < [trimmed length]; i++)
	{
		const int digit = HexDigitValue([trimmed characterAtIndex:i]);
		if (digit < 0)
		{
			return NO;
		}
		value = (value << 4) | (u32)digit;
	}
	
	*outAddress = value;
	return YES;
}

static inline u16 ReadU16LE(const u8 *p)
{
	return (u16)((u16)p[0] | ((u16)p[1] << 8));
}

static inline u32 ReadU32LE(const u8 *p)
{
	return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

static inline u16 SwapBytes16(u16 value)
{
	return (u16)((value >> 8) | (value << 8));
}

static inline u32 SwapBytes32(u32 value)
{
	return ((value & 0x000000FF) << 24) | ((value & 0x0000FF00) << 8) |
	       ((value & 0x00FF0000) >> 8)  | ((value & 0xFF000000) >> 24);
}

#pragma mark -

@implementation MemoryHexView

@synthesize delegate = _delegate;
@synthesize selectionPart = _selectionPart;

- (id)initWithFrame:(NSRect)frame
{
	self = [super initWithFrame:frame];
	if (self == nil)
	{
		return nil;
	}
	
	_region = MEMVIEW_ARM9;
	_viewMode = MemoryViewerViewModeByte;
	_visibleRowCount = kHexDefaultRowCount;
	_windowSize = kHexDefaultWindowSize;
	_address = 0;
	_selectionEnabled = NO;
	_selectionAddress = 0;
	_selectionPart = 0;
	_selectionNewValue = 0;
	_bigEndian = NO;
	_fullCharacters = NO;
	_scrollAccumulator = 0.0;
	memset(_memoryBuffer, 0, sizeof(_memoryBuffer));
	
	_monoFont = [MemoryViewerMonospacedFont(12.0) retain];
	_charWidth = [@"0" sizeWithAttributes:[NSDictionary dictionaryWithObject:_monoFont forKey:NSFontAttributeName]].width;
	if (_charWidth < 1.0)
	{
		_charWidth = 7.0;
	}
	_lineHeight = ceil([_monoFont ascender] - [_monoFont descender] + [_monoFont leading]) + 2.0;
	
	return self;
}

- (void)dealloc
{
	[_monoFont release];
	[super dealloc];
}

- (BOOL)isFlipped
{
	return YES;
}

- (BOOL)isOpaque
{
	return YES;
}

- (BOOL)acceptsFirstResponder
{
	return YES;
}

- (BOOL)becomeFirstResponder
{
	[self setNeedsDisplay:YES];
	return YES;
}

- (BOOL)resignFirstResponder
{
	[self setNeedsDisplay:YES];
	return YES;
}

#pragma mark Virtualised viewport

- (NSUInteger) rowCountForHeight:(CGFloat)height
{
	const CGFloat available = height - kHexHeaderHeight - kHexOuterMargin;
	if (available <= 0.0)
	{
		return 1;
	}
	
	const NSUInteger rows = (NSUInteger)floor(available / _lineHeight);
	return MAX((NSUInteger)1, MIN(rows, kHexMaxRowCount));
}

// Only the rows that actually fit on screen are read out of emulated memory and
// drawn, so growing the window costs a few KB at most, never a full region.
// Scrolling shifts _address instead of materialising anything.
- (void) setFrameSize:(NSSize)newSize
{
	[super setFrameSize:newSize];
	
	if (_monoFont == nil)
	{
		return;
	}
	
	const NSUInteger rows = [self rowCountForHeight:newSize.height];
	if (rows == _visibleRowCount)
	{
		return;
	}
	
	_visibleRowCount = rows;
	_windowSize = (u32)(rows * kHexBytesPerRow);
	
	[self reloadData];
	[self notifyViewportChanged];
}

#pragma mark Layout helpers

- (u32) bytesPerUnit
{
	switch (_viewMode)
	{
		case MemoryViewerViewModeHalfword:	return 2;
		case MemoryViewerViewModeWord:		return 4;
		default:							return 1;
	}
}

- (NSUInteger) hexDigitsPerUnit
{
	switch (_viewMode)
	{
		case MemoryViewerViewModeHalfword:	return 4;
		case MemoryViewerViewModeWord:		return 8;
		default:							return 2;
	}
}

- (NSUInteger) unitsPerRow
{
	return kHexBytesPerRow / [self bytesPerUnit];
}

- (CGFloat) dataStartX
{
	return kHexOuterMargin + ((CGFloat)8 * _charWidth) + ((CGFloat)2 * _charWidth);
}

- (CGFloat) unitWidth
{
	return ((CGFloat)[self hexDigitsPerUnit] + 1.0) * _charWidth;
}

- (CGFloat) asciiStartX
{
	return [self dataStartX] + ((CGFloat)[self unitsPerRow] * [self unitWidth]) + ((CGFloat)2 * _charWidth);
}

- (NSSize) preferredMinimumSize
{
	const CGFloat width = [self asciiStartX] + ((CGFloat)kHexBytesPerRow * _charWidth) + (kHexOuterMargin * 2.0);
	const CGFloat height = kHexHeaderHeight + ((CGFloat)_visibleRowCount * _lineHeight) + kHexOuterMargin;
	
	return NSMakeSize(ceil(width), ceil(height));
}

#pragma mark Property accessors

- (MemoryViewerRegionType) region
{
	return _region;
}

- (void) setRegion:(MemoryViewerRegionType)region
{
	_region = region;
	[self reloadData];
}

- (MemoryViewerViewMode) viewMode
{
	return _viewMode;
}

- (void) setViewMode:(MemoryViewerViewMode)viewMode
{
	_viewMode = viewMode;
	_selectionPart = 0;
	_selectionNewValue = 0;
	[self reloadData];
}

- (u32) address
{
	return _address;
}

- (void) setAddress:(u32)address
{
	_address = address;
	[self notifyViewportChanged];
	[self reloadData];
}

- (void) notifyViewportChanged
{
	if ([_delegate respondsToSelector:@selector(memoryHexView:didChangeViewportAddress:)])
	{
		[_delegate memoryHexView:self didChangeViewportAddress:_address];
	}
}

- (BOOL) selectionEnabled
{
	return _selectionEnabled;
}

- (void) setSelectionEnabled:(BOOL)selectionEnabled
{
	_selectionEnabled = selectionEnabled;
	[self setNeedsDisplay:YES];
}

- (BOOL) bigEndian
{
	return _bigEndian;
}

- (void) setBigEndian:(BOOL)bigEndian
{
	_bigEndian = bigEndian;
	[self reloadData];
}

- (BOOL) fullCharacters
{
	return _fullCharacters;
}

- (void) setFullCharacters:(BOOL)fullCharacters
{
	_fullCharacters = fullCharacters;
	[self setNeedsDisplay:YES];
}

- (NSUInteger) visibleRowCount
{
	return _visibleRowCount;
}

- (u32) windowSize
{
	return _windowSize;
}

- (u32) selectionAddress
{
	return _selectionAddress;
}

- (void) setSelectionAddress:(u32)selectionAddress
{
	[self setSelectionAddress:selectionAddress reveal:NO];
}

#pragma mark Data access

- (void) reloadData
{
	MemoryViewerRead(_region, _address, _windowSize, _memoryBuffer);
	[self setNeedsDisplay:YES];
}

- (void) refresh
{
	[self setNeedsDisplay:YES];
}

- (u32) valueAtOffset:(NSUInteger)offset
{
	switch (_viewMode)
	{
		case MemoryViewerViewModeHalfword:
		{
			const u16 raw = ReadU16LE(&_memoryBuffer[offset]);
			return (u32)((_bigEndian) ? SwapBytes16(raw) : raw);
		}
			
		case MemoryViewerViewModeWord:
		{
			const u32 raw = ReadU32LE(&_memoryBuffer[offset]);
			return (_bigEndian) ? SwapBytes32(raw) : raw;
		}
			
		default:
			return (u32)_memoryBuffer[offset];
	}
}

- (u32) displayedValueAtAddress:(u32)address
{
	u8 bytes[4] = {0};
	const NSUInteger count = [self bytesPerUnit];
	
	MemoryViewerRead(_region, address, (u32)count, bytes);
	if (count < 4)
	{
		memset(&bytes[count], 0, 4 - count);
	}
	
	switch (_viewMode)
	{
		case MemoryViewerViewModeHalfword:
		{
			const u16 raw = ReadU16LE(bytes);
			return (u32)((_bigEndian) ? SwapBytes16(raw) : raw);
		}
			
		case MemoryViewerViewModeWord:
		{
			const u32 raw = ReadU32LE(bytes);
			return (_bigEndian) ? SwapBytes32(raw) : raw;
		}
			
		default:
			return (u32)bytes[0];
	}
}

#pragma mark Scrolling and selection

- (void) clampViewport
{
	const MemoryViewerRegionInfo *info = MemoryViewerRegionGet(_region);
	const u32 base = info->hardwareAddress;
	const u32 lastStart = base + info->size - _windowSize;
	
	if (_address < base)
	{
		_address = base;
	}
	else if (_address > lastStart)
	{
		_address = lastStart & 0xFFFFFFF0;
	}
	
	_address &= 0xFFFFFFF0;
}

- (void) scrollBy:(s32)delta
{
	const MemoryViewerRegionInfo *info = MemoryViewerRegionGet(_region);
	const s64 newAddress = (s64)_address + (s64)delta;
	
	_address = (u32)MAX((s64)info->hardwareAddress, newAddress);
	[self clampViewport];
	
	// A selection scrolled out of the window is no longer meaningful.
	if (_selectionEnabled &&
		(_selectionAddress < _address || _selectionAddress >= _address + _windowSize))
	{
		_selectionEnabled = NO;
		_selectionPart = 0;
		_selectionNewValue = 0;
	}
	
	[self notifyViewportChanged];
	[self reloadData];
}

- (void) scrollWheel:(NSEvent *)event
{
	const CGFloat delta = [event scrollingDeltaY];
	if (delta == 0.0)
	{
		return;
	}
	
	// Trackpads report fractional pixel deltas, so accumulate until a whole
	// line has been scrolled; classic wheels report discrete steps.
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
	[self scrollBy:(s32)(-rows * (NSInteger)kHexBytesPerRow)];
}

- (void) setSelectionAddress:(u32)address reveal:(BOOL)reveal
{
	const MemoryViewerRegionInfo *info = MemoryViewerRegionGet(_region);
	const u32 base = info->hardwareAddress;
	const u32 end = base + info->size;
	
	if (address < base)
	{
		address = base;
	}
	if (address >= end)
	{
		address = end - 1;
	}
	
	_selectionEnabled = YES;
	_selectionAddress = address;
	_selectionPart = 0;
	_selectionNewValue = 0;
	
	if (reveal)
	{
		if (_selectionAddress < _address)
		{
			_address = _selectionAddress & 0xFFFFFFF0;
		}
		else if (_selectionAddress >= _address + _windowSize)
		{
			// Keep the cursor on the last complete row of the window.
			_address = (_selectionAddress & 0xFFFFFFF0) - (_windowSize - kHexBytesPerRow);
		}
		[self clampViewport];
	}
	
	if ([_delegate respondsToSelector:@selector(memoryHexView:didSelectAddress:)])
	{
		[_delegate memoryHexView:self didSelectAddress:_selectionAddress];
	}
	
	[self reloadData];
}

#pragma mark Drawing

- (NSString *) cellTextForValue:(u32)value
{
	switch (_viewMode)
	{
		case MemoryViewerViewModeHalfword:
			return [NSString stringWithFormat:@"%04X", (unsigned)(value & 0xFFFF)];
			
		case MemoryViewerViewModeWord:
			return [NSString stringWithFormat:@"%08X", (unsigned)value];
			
		default:
			return [NSString stringWithFormat:@"%02X", (unsigned)(value & 0xFF)];
	}
}

// While the user is typing a value, show the digits entered so far followed by
// dots for the remaining positions, mirroring the Windows frontend's feedback.
- (NSString *) typingTextForCurrentInputWithDigits:(NSUInteger)digits
{
	if (_selectionPart == 0)
	{
		return nil;
	}
	
	const NSUInteger entered = MIN(_selectionPart, digits);
	const unsigned mask = (unsigned)((1ULL << (entered * 4)) - 1);
	NSMutableString *text = [NSMutableString stringWithFormat:@"%0*X", (int)entered, (unsigned)(_selectionNewValue & mask)];
	
	for (NSUInteger i = entered; i < digits; i++)
	{
		[text appendString:@"."];
	}
	
	return text;
}

- (void) drawRect:(NSRect)dirtyRect
{
	const NSRect bounds = [self bounds];
	
	[[NSColor textBackgroundColor] setFill];
	NSRectFill(bounds);
	
	NSColor *textColor = [NSColor textColor];
	NSColor *dimColor = [NSColor disabledControlTextColor];
	NSColor *highlightColor = [NSColor selectedTextBackgroundColor];
	NSColor *highlightTextColor = [NSColor selectedTextColor];
	
	NSDictionary *headerAttrs = [NSDictionary dictionaryWithObjectsAndKeys:
								 _monoFont, NSFontAttributeName,
								 dimColor, NSForegroundColorAttributeName,
								 nil];
	NSDictionary *cellAttrs = [NSDictionary dictionaryWithObjectsAndKeys:
							   _monoFont, NSFontAttributeName,
							   textColor, NSForegroundColorAttributeName,
							   nil];
	NSDictionary *selectedAttrs = [NSDictionary dictionaryWithObjectsAndKeys:
								   _monoFont, NSFontAttributeName,
								   highlightTextColor, NSForegroundColorAttributeName,
								   nil];
	// Breakpoint cells keep the Windows frontend's red background with black
	// text, which stays readable regardless of the system appearance.
	NSDictionary *breakpointAttrs = [NSDictionary dictionaryWithObjectsAndKeys:
									 _monoFont, NSFontAttributeName,
									 [NSColor blackColor], NSForegroundColorAttributeName,
									 nil];
	const BOOL hasBreakpoints = ((MemoryViewerReadBreakpointCount() + MemoryViewerWriteBreakpointCount()) > 0);
	
	const NSUInteger units = [self unitsPerRow];
	const NSUInteger digits = [self hexDigitsPerUnit];
	const NSUInteger step = [self bytesPerUnit];
	const CGFloat dataX = [self dataStartX];
	const CGFloat unitWidth = [self unitWidth];
	const CGFloat asciiX = [self asciiStartX];
	
	// Column headers.
	for (NSUInteger unit = 0; unit < units; unit++)
	{
		NSString *columnText = [NSString stringWithFormat:@"%02X", (unsigned)(unit * step)];
		[columnText drawAtPoint:NSMakePoint(dataX + ((CGFloat)unit * unitWidth), 2.0) withAttributes:headerAttrs];
	}
	
	[@"0123456789ABCDEF" drawAtPoint:NSMakePoint(asciiX, 2.0) withAttributes:headerAttrs];
	
	// Separator line under the headers.
	[dimColor setFill];
	NSRectFill(NSMakeRect(kHexOuterMargin, kHexHeaderHeight - 2.0, bounds.size.width - (kHexOuterMargin * 2.0), 1.0));
	
	for (NSUInteger row = 0; row < _visibleRowCount; row++)
	{
		const CGFloat y = kHexHeaderHeight + ((CGFloat)row * _lineHeight);
		const u32 rowAddress = _address + (u32)(row * kHexBytesPerRow);
		
		NSString *addressText = [NSString stringWithFormat:@"%08X", (unsigned)rowAddress];
		[addressText drawAtPoint:NSMakePoint(kHexOuterMargin, y) withAttributes:headerAttrs];
		
		for (NSUInteger unit = 0; unit < units; unit++)
		{
			const NSUInteger offset = (row * kHexBytesPerRow) + (unit * step);
			const u32 cellAddress = _address + (u32)offset;
			const u32 value = [self valueAtOffset:offset];
			const CGFloat x = dataX + ((CGFloat)unit * unitWidth);
			const BOOL isSelected = (_selectionEnabled && (_selectionAddress == cellAddress));
			
			if (isSelected)
			{
				[highlightColor setFill];
				NSRectFill(NSMakeRect(x - 1.0, y - 1.0, ((CGFloat)digits * _charWidth) + 2.0, _lineHeight));
				
				NSString *text = [self typingTextForCurrentInputWithDigits:digits];
				if (text == nil)
				{
					text = [self cellTextForValue:value];
				}
				
				[text drawAtPoint:NSMakePoint(x, y) withAttributes:selectedAttrs];
			}
			else
			{
				if (hasBreakpoints && MemoryViewerIsBreakpoint(cellAddress))
				{
					[[NSColor redColor] setFill];
					NSRectFill(NSMakeRect(x - 1.0, y - 1.0, ((CGFloat)digits * _charWidth) + 2.0, _lineHeight));
					[[self cellTextForValue:value] drawAtPoint:NSMakePoint(x, y) withAttributes:breakpointAttrs];
				}
				else
				{
					[[self cellTextForValue:value] drawAtPoint:NSMakePoint(x, y) withAttributes:cellAttrs];
				}
			}
		}
		
		// ASCII column.
		const u8 endChar = (_fullCharacters) ? 0xFF : 0x7F;
		for (NSUInteger i = 0; i < kHexBytesPerRow; i++)
		{
			const u8 value = _memoryBuffer[(row * kHexBytesPerRow) + i];
			NSString *charText = ((value >= 0x20) && (value <= endChar)) ? [NSString stringWithFormat:@"%C", (unichar)value] : @".";
			const CGFloat x = asciiX + ((CGFloat)i * _charWidth);
			
			if (_selectionEnabled && ((_selectionAddress & 0xFFFFFFF0) == rowAddress) &&
				((_selectionAddress & 0x0000000F) == (u32)i))
			{
				[highlightColor setFill];
				NSRectFill(NSMakeRect(x, y - 1.0, _charWidth, _lineHeight));
				[charText drawAtPoint:NSMakePoint(x, y) withAttributes:selectedAttrs];
			}
			else
			{
				[charText drawAtPoint:NSMakePoint(x, y) withAttributes:cellAttrs];
			}
		}
	}
	
	// Focus ring, drawn last so it stays visible over the cells.
	if ([[self window] firstResponder] == self)
	{
		[[NSColor keyboardFocusIndicatorColor] setStroke];
		NSBezierPath *path = [NSBezierPath bezierPathWithRect:NSInsetRect(bounds, 1.0, 1.0)];
		[path setLineWidth:2.0];
		[path stroke];
	}
}

#pragma mark Mouse input

- (void)mouseDown:(NSEvent *)event
{
	[[self window] makeFirstResponder:self];
	
	const NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
	if (point.y < kHexHeaderHeight)
	{
		return;
	}
	
	const NSInteger row = (NSInteger)floor((point.y - kHexHeaderHeight) / _lineHeight);
	if (row < 0 || row >= (NSInteger)_visibleRowCount)
	{
		return;
	}
	
	const CGFloat localX = point.x - [self dataStartX];
	const CGFloat unitWidth = [self unitWidth];
	if (localX < 0.0 || localX >= ((CGFloat)[self unitsPerRow] * unitWidth))
	{
		return;
	}
	
	const NSInteger unit = (NSInteger)floor(localX / unitWidth);
	const u32 cellAddress = _address + (u32)((row * kHexBytesPerRow) + (unit * (NSInteger)[self bytesPerUnit]));
	
	[self setSelectionAddress:cellAddress reveal:NO];
}

#pragma mark Keyboard input

- (void) inputHexDigit:(int)digit
{
	if (!_selectionEnabled)
	{
		NSBeep();
		return;
	}
	
	if (!MemoryViewerIsAvailable(_region, _selectionAddress))
	{
		NSBeep();
		return;
	}
	
	_selectionNewValue = (_selectionNewValue << 4) | (u32)digit;
	_selectionPart++;
	
	if (_selectionPart >= [self hexDigitsPerUnit])
	{
		switch (_viewMode)
		{
			case MemoryViewerViewModeHalfword:
			{
				const u16 value = (_bigEndian) ? SwapBytes16((u16)_selectionNewValue) : (u16)_selectionNewValue;
				MemoryViewerWrite16(_region, _selectionAddress, value);
				break;
			}
				
			case MemoryViewerViewModeWord:
			{
				const u32 value = (_bigEndian) ? SwapBytes32(_selectionNewValue) : _selectionNewValue;
				MemoryViewerWrite32(_region, _selectionAddress, value);
				break;
			}
				
			default:
				MemoryViewerWrite8(_region, _selectionAddress, (u8)_selectionNewValue);
				break;
		}
		
		_selectionPart = 0;
		_selectionNewValue = 0;
		
		const MemoryViewerRegionInfo *info = MemoryViewerRegionGet(_region);
		const u32 nextAddress = _selectionAddress + (u32)[self bytesPerUnit];
		
		if (nextAddress < info->hardwareAddress + info->size)
		{
			[self setSelectionAddress:nextAddress reveal:YES];
			return;
		}
	}
	
	[self reloadData];
}

- (void) keyDown:(NSEvent *)event
{
	NSString *characters = [event charactersIgnoringModifiers];
	if ([characters length] == 0)
	{
		[super keyDown:event];
		return;
	}
	
	const unichar character = [characters characterAtIndex:0];
	const BOOL commandDown = (([event modifierFlags] & NSEventModifierFlagCommand) != 0);
	const s32 step = (s32)[self bytesPerUnit];
	const MemoryViewerRegionInfo *info = MemoryViewerRegionGet(_region);
	
	switch (character)
	{
		case NSLeftArrowFunctionKey:
			[self setSelectionAddress:(_selectionEnabled ? _selectionAddress - (u32)step : _address) reveal:YES];
			return;
			
		case NSRightArrowFunctionKey:
			[self setSelectionAddress:(_selectionEnabled ? _selectionAddress + (u32)step : _address) reveal:YES];
			return;
			
		case NSUpArrowFunctionKey:
			[self setSelectionAddress:(_selectionEnabled ? _selectionAddress - kHexBytesPerRow : _address) reveal:YES];
			return;
			
		case NSDownArrowFunctionKey:
			[self setSelectionAddress:(_selectionEnabled ? _selectionAddress + kHexBytesPerRow : _address) reveal:YES];
			return;
			
		case NSPageUpFunctionKey:
			[self scrollBy:-(s32)_windowSize];
			[self setSelectionAddress:_address reveal:NO];
			return;
			
		case NSPageDownFunctionKey:
			[self scrollBy:(s32)_windowSize];
			[self setSelectionAddress:_address reveal:NO];
			return;
			
		case NSHomeFunctionKey:
			if (commandDown)
			{
				[self setAddress:info->hardwareAddress];
				[self setSelectionAddress:info->hardwareAddress reveal:NO];
			}
			else
			{
				[self setSelectionAddress:_address reveal:NO];
			}
			return;
			
		case NSEndFunctionKey:
			if (commandDown)
			{
				const u32 lastRow = (info->hardwareAddress + info->size - kHexBytesPerRow) & 0xFFFFFFF0;
				[self setAddress:lastRow];
				[self setSelectionAddress:lastRow reveal:NO];
			}
			else
			{
				[self setSelectionAddress:(_address + _windowSize - 1) reveal:NO];
			}
			return;
			
		default:
			break;
	}
	
	const int digit = HexDigitValue(character);
	if (digit >= 0)
	{
		[self inputHexDigit:digit];
		return;
	}
	
	[super keyDown:event];
}

@end

#pragma mark -

@implementation MemoryViewerLabel

- (void) drawRect:(NSRect)dirtyRect
{
	NSString *stringValue = [self stringValue];
	if ([stringValue length] == 0)
	{
		return;
	}
	
	NSFont *font = [self font];
	if (font == nil)
	{
		font = [NSFont systemFontOfSize:[NSFont systemFontSize]];
	}
	
	NSColor *color = [self textColor];
	if (color == nil)
	{
		color = [NSColor textColor];
	}
	
	NSDictionary *attrs = [NSDictionary dictionaryWithObjectsAndKeys:
						   font, NSFontAttributeName,
						   color, NSForegroundColorAttributeName,
						   nil];
	
	NSAttributedString *text = [[[NSAttributedString alloc] initWithString:stringValue attributes:attrs] autorelease];
	const NSRect bounds = [self bounds];
	const NSSize textSize = [text size];
	
	// Centre the glyph box in the control, then let the drawing routine clip or
	// truncate anything wider than the frame.
	NSRect textRect = NSMakeRect(bounds.origin.x,
								 bounds.origin.y + floor((bounds.size.height - textSize.height) * 0.5),
								 bounds.size.width,
								 textSize.height);
	
	[NSGraphicsContext saveGraphicsState];
	[text drawWithRect:textRect
			   options:(NSStringDrawingUsesLineFragmentOrigin | NSStringDrawingTruncatesLastVisibleLine)];
	[NSGraphicsContext restoreGraphicsState];
}

@end

#pragma mark -

@implementation MemoryViewerWindowController

@synthesize hexView = _hexView;

- (id)init
{
	NSRect contentRect = NSMakeRect(0.0, 0.0, kWindowWidth, 500.0);
	NSWindow *window = [[NSWindow alloc] initWithContentRect:contentRect
												   styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
															  NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable)
													 backing:NSBackingStoreBuffered
													   defer:NO];
	if (window == nil)
	{
		[self release];
		return nil;
	}
	
	[window setTitle:@"Memory Viewer"];
	[window setReleasedWhenClosed:NO];
	// Width is locked so the hex columns never reflow; height is free and the
	// dump grows to fill it.
	[window setMinSize:NSMakeSize(kWindowWidth, 300.0)];
	[window setMaxSize:NSMakeSize(kWindowWidth, 4000.0)];
	
	self = [super initWithWindow:window];
	[window release];
	if (self == nil)
	{
		return nil;
	}
	
	_didInitializeRegions = NO;
	[window setDelegate:self];
	[self buildInterface];
	
	return self;
}

- (void)dealloc
{
	[self stopRefreshTimer];
	
	[_hexView setDelegate:nil];
	[_toolbar release];
	[_bpBar release];
	[_regionPopUpButton release];
	[_addressField release];
	[_goButton release];
	[_viewModePopUpButton release];
	[_selectionLabel release];
	[_bpField release];
	[_addReadBPButton release];
	[_addWriteBPButton release];
	[_removeBPButton release];
	[_clearBPButton release];
	[_bpSummaryLabel release];
	[_bigEndianButton release];
	[_fullCharsButton release];
	[_exportButton release];
	[_hexView release];
	[_scroller release];
	
	[super dealloc];
}

- (void) buildInterface
{
	NSWindow *window = [self window];
	NSView *contentView = [window contentView];
	const NSRect contentRect = [contentView bounds];
	
	const CGFloat viewHeight = MAX(120.0, contentRect.size.height - kToolbarHeight - kBreakpointBarHeight);
	
	// Frames are assigned by -layoutViews; autoresizing is deliberately off so
	// that widgets never drift or overlap when the window is resized.
	_hexView = [[MemoryHexView alloc] initWithFrame:NSMakeRect(0.0, kBreakpointBarHeight, kWindowWidth - kScrollerWidth, viewHeight)];
	[_hexView setAutoresizingMask:NSViewNotSizable];
	[_hexView setDelegate:self];
	[contentView addSubview:_hexView];
	
	_scroller = [[NSScroller alloc] initWithFrame:NSMakeRect(kWindowWidth - kScrollerWidth, kBreakpointBarHeight, kScrollerWidth, viewHeight)];
	[_scroller setAutoresizingMask:NSViewNotSizable];
	[_scroller setTarget:self];
	[_scroller setAction:@selector(scrollerChanged:)];
	[contentView addSubview:_scroller];
	
	[self buildBreakpointBar];
	
	_toolbar = [[NSView alloc] initWithFrame:NSMakeRect(0.0, contentRect.size.height - kToolbarHeight, kWindowWidth, kToolbarHeight)];
	[_toolbar setAutoresizingMask:NSViewNotSizable];
	[contentView addSubview:_toolbar];
	
	const CGFloat controlY = floor((kToolbarHeight - 24.0) * 0.5);
	
	_regionPopUpButton = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(8.0, controlY, 92.0, 24.0) pullsDown:NO];
	for (NSUInteger i = 0; i < MEMVIEW_REGION_COUNT; i++)
	{
		[_regionPopUpButton addItemWithTitle:[NSString stringWithUTF8String:MemoryViewerRegionGetName((MemoryViewerRegionType)i)]];
	}
	[_regionPopUpButton setTarget:self];
	[_regionPopUpButton setAction:@selector(regionChanged:)];
	[_toolbar addSubview:_regionPopUpButton];
	
	_addressField = [[NSComboBox alloc] initWithFrame:NSMakeRect(106.0, controlY, 116.0, 24.0)];
	[_addressField setBezeled:YES];
	[_addressField setDrawsBackground:YES];
	[_addressField setEditable:YES];
	[_addressField setSelectable:YES];
	[_addressField setCompletes:NO];
	[_addressField setFont:MemoryViewerMonospacedFont(12.0)];
	[_addressField setTarget:self];
	[_addressField setAction:@selector(goButtonPressed:)];
	[_addressField setToolTip:@"Address to jump to. Pick a previous address from the list."];
	[_toolbar addSubview:_addressField];
	
	_goButton = [[NSButton alloc] initWithFrame:NSMakeRect(226.0, controlY, 38.0, 24.0)];
	[_goButton setBezelStyle:NSBezelStyleRounded];
	[_goButton setTitle:@"Go"];
	[_goButton setTarget:self];
	[_goButton setAction:@selector(goButtonPressed:)];
	[_toolbar addSubview:_goButton];
	
	_viewModePopUpButton = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(268.0, controlY, 92.0, 24.0) pullsDown:NO];
	[_viewModePopUpButton addItemsWithTitles:[NSArray arrayWithObjects:@"Bytes", @"Halfwords", @"Words", nil]];
	[_viewModePopUpButton setTarget:self];
	[_viewModePopUpButton setAction:@selector(viewModeChanged:)];
	[_toolbar addSubview:_viewModePopUpButton];
	
	// Same height and origin as the controls on this row; MemoryViewerLabel
	// centres the text itself instead of letting AppKit anchor it to the top.
	_selectionLabel = [[MemoryViewerLabel alloc] initWithFrame:NSMakeRect(364.0, controlY, 132.0, 24.0)];
	[_selectionLabel setBezeled:NO];
	[_selectionLabel setDrawsBackground:NO];
	[_selectionLabel setEditable:NO];
	[_selectionLabel setSelectable:NO];
	[_selectionLabel setFont:MemoryViewerMonospacedFont(12.0)];
	[_selectionLabel setStringValue:@"00000000h"];
	[_selectionLabel setAutoresizingMask:NSViewNotSizable];
	[_toolbar addSubview:_selectionLabel];
	
	_exportButton = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(kWindowWidth - 96.0, controlY, 88.0, 24.0) pullsDown:YES];
	[_exportButton addItemsWithTitles:[NSArray arrayWithObjects:@"Export", @"Text Dump…", @"Raw Dump…", @"Dump All…", nil]];
	[_exportButton setAutoresizingMask:NSViewNotSizable];
	[_exportButton setToolTip:@"Dump the visible window as text, as raw bytes, or the whole region"];
	[_exportButton setTarget:self];
	[_exportButton setAction:@selector(exportSelected:)];
	[_toolbar addSubview:_exportButton];
	
	[self loadAddressHistory];
	[self layoutViews];
}

- (void) buildBreakpointBar
{
	NSView *contentView = [[self window] contentView];
	
	_bpBar = [[NSView alloc] initWithFrame:NSMakeRect(0.0, 0.0, kWindowWidth, kBreakpointBarHeight)];
	[_bpBar setAutoresizingMask:NSViewNotSizable];
	[contentView addSubview:_bpBar];
	
	const CGFloat controlY = floor((kBreakpointBarHeight - 24.0) * 0.5);
	
	NSTextField *caption = [[[MemoryViewerLabel alloc] initWithFrame:NSMakeRect(8.0, controlY, 26.0, 24.0)] autorelease];
	[caption setBezeled:NO];
	[caption setDrawsBackground:NO];
	[caption setEditable:NO];
	[caption setSelectable:NO];
	[caption setStringValue:@"BP:"];
	[_bpBar addSubview:caption];
	
	_bpField = [[NSTextField alloc] initWithFrame:NSMakeRect(36.0, controlY, 86.0, 24.0)];
	[_bpField setBezeled:YES];
	[_bpField setDrawsBackground:YES];
	[_bpField setEditable:YES];
	[_bpField setSelectable:YES];
	[_bpField setFont:MemoryViewerMonospacedFont(12.0)];
	[_bpField setPlaceholderString:@"selected"];
	[_bpField setToolTip:@"Address for the breakpoint buttons. Leave empty to use the selected byte."];
	[_bpBar addSubview:_bpField];
	
	_addReadBPButton = [[NSButton alloc] initWithFrame:NSMakeRect(126.0, controlY, 36.0, 24.0)];
	[_addReadBPButton setBezelStyle:NSBezelStyleRounded];
	[_addReadBPButton setTitle:@"R+"];
	[_addReadBPButton setToolTip:@"Add a read breakpoint (stops the emulator when this address is read)"];
	[_addReadBPButton setTarget:self];
	[_addReadBPButton setAction:@selector(addReadBreakpoint:)];
	[_bpBar addSubview:_addReadBPButton];
	
	_addWriteBPButton = [[NSButton alloc] initWithFrame:NSMakeRect(166.0, controlY, 36.0, 24.0)];
	[_addWriteBPButton setBezelStyle:NSBezelStyleRounded];
	[_addWriteBPButton setTitle:@"W+"];
	[_addWriteBPButton setToolTip:@"Add a write breakpoint (stops the emulator when this address is written)"];
	[_addWriteBPButton setTarget:self];
	[_addWriteBPButton setAction:@selector(addWriteBreakpoint:)];
	[_bpBar addSubview:_addWriteBPButton];
	
	_removeBPButton = [[NSButton alloc] initWithFrame:NSMakeRect(206.0, controlY, 28.0, 24.0)];
	[_removeBPButton setBezelStyle:NSBezelStyleRounded];
	[_removeBPButton setTitle:@"−"];
	[_removeBPButton setToolTip:@"Remove the read and write breakpoint at this address"];
	[_removeBPButton setTarget:self];
	[_removeBPButton setAction:@selector(removeBreakpoint:)];
	[_bpBar addSubview:_removeBPButton];
	
	_clearBPButton = [[NSButton alloc] initWithFrame:NSMakeRect(238.0, controlY, 48.0, 24.0)];
	[_clearBPButton setBezelStyle:NSBezelStyleRounded];
	[_clearBPButton setTitle:@"Clear"];
	[_clearBPButton setToolTip:@"Remove all breakpoints"];
	[_clearBPButton setTarget:self];
	[_clearBPButton setAction:@selector(clearBreakpoints:)];
	[_bpBar addSubview:_clearBPButton];
	
	_bpSummaryLabel = [[MemoryViewerLabel alloc] initWithFrame:NSMakeRect(292.0, controlY, 180.0, 24.0)];
	[_bpSummaryLabel setBezeled:NO];
	[_bpSummaryLabel setDrawsBackground:NO];
	[_bpSummaryLabel setEditable:NO];
	[_bpSummaryLabel setSelectable:NO];
	[_bpSummaryLabel setFont:MemoryViewerMonospacedFont(11.0)];
	[_bpSummaryLabel setLineBreakMode:NSLineBreakByTruncatingTail];
	[_bpSummaryLabel setAutoresizingMask:NSViewNotSizable];
	[_bpBar addSubview:_bpSummaryLabel];
	
	_fullCharsButton = [[NSButton alloc] initWithFrame:NSMakeRect(kWindowWidth - 120.0, controlY + 2.0, 52.0, 20.0)];
	[_fullCharsButton setButtonType:NSButtonTypeSwitch];
	[_fullCharsButton setTitle:@"Full"];
	[_fullCharsButton setFont:[NSFont systemFontOfSize:11.0]];
	[_fullCharsButton setToolTip:@"Show printable characters up to 0xFF instead of 0x7F"];
	[_fullCharsButton setTarget:self];
	[_fullCharsButton setAction:@selector(toggleFullCharacters:)];
	[_fullCharsButton setAutoresizingMask:NSViewNotSizable];
	[_bpBar addSubview:_fullCharsButton];
	
	_bigEndianButton = [[NSButton alloc] initWithFrame:NSMakeRect(kWindowWidth - 60.0, controlY + 2.0, 44.0, 20.0)];
	[_bigEndianButton setButtonType:NSButtonTypeSwitch];
	[_bigEndianButton setTitle:@"BE"];
	[_bigEndianButton setFont:[NSFont systemFontOfSize:11.0]];
	[_bigEndianButton setToolTip:@"Interpret halfwords and words as big-endian"];
	[_bigEndianButton setTarget:self];
	[_bigEndianButton setAction:@selector(toggleBigEndian:)];
	[_bigEndianButton setAutoresizingMask:NSViewNotSizable];
	[_bpBar addSubview:_bigEndianButton];
	
	[self updateBreakpointSummary];
}

- (void) loadAddressHistory
{
	NSArray *history = [[NSUserDefaults standardUserDefaults] arrayForKey:@"MemoryViewerAddressHistory"];
	for (NSString *entry in history)
	{
		if ([entry isKindOfClass:[NSString class]] && [entry length] > 0)
		{
			[_addressField addItemWithObjectValue:entry];
		}
	}
}

- (void) addAddressToHistory:(u32)address
{
	static const NSInteger kMaxHistoryEntries = 20;
	
	NSString *entry = [NSString stringWithFormat:@"%08X", (unsigned)address];
	const NSInteger existingIndex = [_addressField indexOfItemWithObjectValue:entry];
	if (existingIndex != NSNotFound)
	{
		[_addressField removeItemAtIndex:existingIndex];
	}
	
	[_addressField insertItemWithObjectValue:entry atIndex:0];
	
	while ([_addressField numberOfItems] > kMaxHistoryEntries)
	{
		[_addressField removeItemAtIndex:[_addressField numberOfItems] - 1];
	}
	
	[_addressField setStringValue:entry];
	[self saveAddressHistory];
}

- (void) saveAddressHistory
{
	NSMutableArray *history = [NSMutableArray arrayWithCapacity:(NSUInteger)[_addressField numberOfItems]];
	for (NSInteger i = 0; i < [_addressField numberOfItems]; i++)
	{
		[history addObject:[_addressField itemObjectValueAtIndex:i]];
	}
	
	[[NSUserDefaults standardUserDefaults] setObject:history forKey:@"MemoryViewerAddressHistory"];
}

#pragma mark Export

- (NSString *) runSavePanelWithDefaultName:(NSString *)name
{
	NSSavePanel *panel = [NSSavePanel savePanel];
	[panel setNameFieldStringValue:name];
	[panel setCanCreateDirectories:YES];
	
	if ([panel runModal] != NSModalResponseOK)
	{
		return nil;
	}
	
	return [[panel URL] path];
}

- (void) exportSelected:(NSPopUpButton *)sender
{
	const NSInteger index = [sender indexOfSelectedItem];
	[sender selectItemAtIndex:0];
	
	switch (index)
	{
		case 1: [self exportTextDump]; break;
		case 2: [self exportRawDump]; break;
		case 3: [self exportFullDump]; break;
		default: break;
	}
}

- (void) exportTextDump
{
	const MemoryViewerRegionType region = [_hexView region];
	const u32 address = [_hexView address];
	const MemoryViewerViewMode mode = [_hexView viewMode];
	
	NSString *path = [self runSavePanelWithDefaultName:[NSString stringWithFormat:@"memory_%08X.txt", (unsigned)address]];
	if (path == nil)
	{
		return;
	}
	
	const u32 windowSize = [_hexView windowSize];
	const NSUInteger rowCount = [_hexView visibleRowCount];
	
	u8 *buffer = (u8 *)malloc(windowSize);
	if (buffer == NULL)
	{
		NSBeep();
		return;
	}
	MemoryViewerRead(region, address, windowSize, buffer);
	
	// Append, matching the Windows frontend, so repeated dumps accumulate.
	FILE *file = fopen([path fileSystemRepresentation], "a");
	if (file == NULL)
	{
		free(buffer);
		NSBeep();
		return;
	}
	
	for (NSUInteger line = 0; line < rowCount; line++)
	{
		const NSUInteger rowOffset = line * kHexBytesPerRow;
		fprintf(file, "%08X\t\t", (unsigned)(address + (u32)rowOffset));
		
		switch (mode)
		{
			case MemoryViewerViewModeHalfword:
				for (NSUInteger i = 0; i < kHexBytesPerRow; i += 2)
				{
					fprintf(file, "%04X ", (unsigned)ReadU16LE(&buffer[rowOffset + i]));
				}
				fprintf(file, "\t\t");
				break;
				
			case MemoryViewerViewModeWord:
				for (NSUInteger i = 0; i < kHexBytesPerRow; i += 4)
				{
					fprintf(file, "%08X ", (unsigned)ReadU32LE(&buffer[rowOffset + i]));
				}
				fprintf(file, "\t\t\t");
				break;
				
			default:
				for (NSUInteger i = 0; i < kHexBytesPerRow; i++)
				{
					fprintf(file, "%02X ", buffer[rowOffset + i]);
				}
				fprintf(file, "\t");
				break;
		}
		
		for (NSUInteger i = 0; i < kHexBytesPerRow; i++)
		{
			const u8 value = buffer[rowOffset + i];
			fputc(((value >= 0x20) && (value <= 0x7F)) ? value : '.', file);
		}
		fputc('\n', file);
	}
	
	fclose(file);
	free(buffer);
}

- (void) exportRawDump
{
	const MemoryViewerRegionType region = [_hexView region];
	const u32 address = [_hexView address];
	
	NSString *path = [self runSavePanelWithDefaultName:[NSString stringWithFormat:@"memory_%08X.bin", (unsigned)address]];
	if (path == nil)
	{
		return;
	}
	
	const u32 windowSize = [_hexView windowSize];
	u8 *buffer = (u8 *)malloc(windowSize);
	if (buffer == NULL)
	{
		NSBeep();
		return;
	}
	MemoryViewerRead(region, address, windowSize, buffer);
	
	FILE *file = fopen([path fileSystemRepresentation], "ab");
	if (file == NULL)
	{
		free(buffer);
		NSBeep();
		return;
	}
	
	fwrite(buffer, 1, windowSize, file);
	fclose(file);
	free(buffer);
}

- (void) exportFullDump
{
	const MemoryViewerRegionType region = [_hexView region];
	const u32 total = MemoryViewerRegionDumpSize(region);
	
	if (total == 0)
	{
		NSAlert *alert = [[[NSAlert alloc] init] autorelease];
		[alert setMessageText:@"No data to dump"];
		[alert setInformativeText:@"The \"Full\" region spans the entire address space, and \"CartROM\" is empty without a loaded ROM. Select ARM9, ARM7 or Firmware instead."];
		[alert runModal];
		return;
	}
	
	const MemoryViewerRegionInfo *info = MemoryViewerRegionGet(region);
	const u32 base = (region == MEMVIEW_ROM) ? 0 : info->hardwareAddress;
	
	NSString *path = [self runSavePanelWithDefaultName:[NSString stringWithFormat:@"%s_%08X.bin", info->name, (unsigned)base]];
	if (path == nil)
	{
		return;
	}
	
	FILE *file = fopen([path fileSystemRepresentation], "wb");
	if (file == NULL)
	{
		NSBeep();
		return;
	}
	
	const u32 blockSize = 0x1000;
	u8 *buffer = (u8 *)malloc(blockSize);
	if (buffer != NULL)
	{
		for (u32 offset = 0; offset < total; offset += blockSize)
		{
			const u32 chunk = MIN(blockSize, total - offset);
			MemoryViewerRead(region, base + offset, chunk, buffer);
			fwrite(buffer, 1, chunk, file);
		}
		
		free(buffer);
	}
	
	fclose(file);
}

#pragma mark Window lifecycle

// Deterministic layout: every resize recomputes all four regions from scratch,
// so widgets can never drift or overlap.
- (void) layoutViews
{
	NSView *contentView = [[self window] contentView];
	const NSRect bounds = [contentView bounds];
	const CGFloat width = bounds.size.width;
	const CGFloat height = bounds.size.height;
	const CGFloat viewHeight = MAX(120.0, height - kToolbarHeight - kBreakpointBarHeight);
	
	[_toolbar setFrame:NSMakeRect(0.0, height - kToolbarHeight, width, kToolbarHeight)];
	[_bpBar setFrame:NSMakeRect(0.0, 0.0, width, kBreakpointBarHeight)];
	[_hexView setFrame:NSMakeRect(0.0, kBreakpointBarHeight, width - kScrollerWidth, viewHeight)];
	[_scroller setFrame:NSMakeRect(width - kScrollerWidth, kBreakpointBarHeight, kScrollerWidth, viewHeight)];
}

- (void) windowDidResize:(NSNotification *)notification
{
	[self layoutViews];
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
	
	[_hexView reloadData];
}

#pragma mark Actions

- (void) updateAddressField:(u32)address
{
	[_addressField setStringValue:[NSString stringWithFormat:@"%08X", (unsigned)address]];
}

- (void) updateScroller
{
	if (_scrollerIsDragging)
	{
		return;
	}
	
	const MemoryViewerRegionInfo *info = MemoryViewerRegionGet([_hexView region]);
	const u32 base = info->hardwareAddress;
	const u32 size = info->size;
	const u32 windowSize = [_hexView windowSize];
	
	if (size <= windowSize)
	{
		[_scroller setEnabled:NO];
		[_scroller setKnobProportion:1.0];
		[_scroller setDoubleValue:0.0];
		return;
	}
	
	const u32 span = size - windowSize;
	double value = (double)([_hexView address] - base) / (double)span;
	value = MAX(0.0, MIN(1.0, value));
	
	// Keep the knob visible even for multi-gigabyte regions such as the
	// cartridge dump, where the exact proportion is vanishingly small.
	const CGFloat proportion = (CGFloat)MAX(0.0001, (double)windowSize / (double)size);
	
	[_scroller setEnabled:YES];
	[_scroller setKnobProportion:proportion];
	[_scroller setDoubleValue:value];
}

- (void) scrollerChanged:(NSScroller *)sender
{
	const MemoryViewerRegionInfo *info = MemoryViewerRegionGet([_hexView region]);
	const u32 base = info->hardwareAddress;
	const u32 size = info->size;
	const u32 windowSize = [_hexView windowSize];
	
	if (size <= windowSize)
	{
		return;
	}
	
	const u32 span = size - windowSize;
	const double value = [sender doubleValue];
	
	u32 newAddress = base + (u32)(value * (double)span);
	newAddress &= 0xFFFFFFF0;
	
	_scrollerIsDragging = YES;
	[_hexView setAddress:newAddress];
	_scrollerIsDragging = NO;
	
	[self updateAddressField:newAddress];
}

#pragma mark Breakpoints

- (u32) breakpointAddressFromField
{
	u32 address = 0;
	if (ParseHexAddress([_bpField stringValue], &address))
	{
		return address;
	}
	
	// Empty or invalid field: fall back to whatever the user has selected.
	if ([_hexView selectionEnabled])
	{
		return [_hexView selectionAddress];
	}
	
	return [_hexView address];
}

- (void) addReadBreakpoint:(id)sender
{
	const u32 address = [self breakpointAddressFromField];
	MemoryViewerAddReadBreakpoint(address);
	
	[_bpField setStringValue:[NSString stringWithFormat:@"%08X", (unsigned)address]];
	[self updateBreakpointSummary];
	[_hexView refresh];
}

- (void) addWriteBreakpoint:(id)sender
{
	const u32 address = [self breakpointAddressFromField];
	MemoryViewerAddWriteBreakpoint(address);
	
	[_bpField setStringValue:[NSString stringWithFormat:@"%08X", (unsigned)address]];
	[self updateBreakpointSummary];
	[_hexView refresh];
}

- (void) removeBreakpoint:(id)sender
{
	const u32 address = [self breakpointAddressFromField];
	MemoryViewerRemoveReadBreakpoint(address);
	MemoryViewerRemoveWriteBreakpoint(address);
	
	[self updateBreakpointSummary];
	[_hexView refresh];
}

- (void) clearBreakpoints:(id)sender
{
	MemoryViewerClearReadBreakpoints();
	MemoryViewerClearWriteBreakpoints();
	
	[self updateBreakpointSummary];
	[_hexView refresh];
}

- (void) updateBreakpointSummary
{
	const NSUInteger readCount = MemoryViewerReadBreakpointCount();
	const NSUInteger writeCount = MemoryViewerWriteBreakpointCount();
	const NSUInteger maxShown = 4;
	
	if (readCount == 0 && writeCount == 0)
	{
		[_bpSummaryLabel setStringValue:@"No breakpoints"];
		[_bpSummaryLabel setToolTip:@"No breakpoints"];
		[_clearBPButton setEnabled:NO];
		[_removeBPButton setEnabled:NO];
		return;
	}
	
	NSMutableString *summary = [NSMutableString string];
	
	if (readCount > 0)
	{
		[summary appendFormat:@"R[%lu]:", (unsigned long)readCount];
		for (NSUInteger i = 0; i < MIN(readCount, maxShown); i++)
		{
			[summary appendFormat:@" %08X", (unsigned)MemoryViewerReadBreakpointAt(i)];
		}
		if (readCount > maxShown)
		{
			[summary appendFormat:@" +%lu", (unsigned long)(readCount - maxShown)];
		}
	}
	
	if (writeCount > 0)
	{
		if (readCount > 0)
		{
			[summary appendString:@"   "];
		}
		[summary appendFormat:@"W[%lu]:", (unsigned long)writeCount];
		for (NSUInteger i = 0; i < MIN(writeCount, maxShown); i++)
		{
			[summary appendFormat:@" %08X", (unsigned)MemoryViewerWriteBreakpointAt(i)];
		}
		if (writeCount > maxShown)
		{
			[summary appendFormat:@" +%lu", (unsigned long)(writeCount - maxShown)];
		}
	}
	
	[_bpSummaryLabel setStringValue:summary];
	[_bpSummaryLabel setToolTip:summary];
	[_clearBPButton setEnabled:YES];
	[_removeBPButton setEnabled:YES];
}

- (void) selectRegion:(MemoryViewerRegionType)region
{
	if (region < 0 || region >= MEMVIEW_REGION_COUNT)
	{
		return;
	}
	
	const MemoryViewerRegionInfo *info = MemoryViewerRegionGet(region);
	
	[_regionPopUpButton selectItemAtIndex:(NSInteger)region];
	[_hexView setRegion:region];
	[_hexView setViewMode:(MemoryViewerViewMode)[_viewModePopUpButton indexOfSelectedItem]];
	[_hexView setAddress:info->hardwareAddress];
	[_hexView setSelectionAddress:info->hardwareAddress reveal:NO];
	
	[[self window] setTitle:[NSString stringWithUTF8String:info->longName]];
	[self updateAddressField:info->hardwareAddress];
	[self updateScroller];
}

- (void) regionChanged:(id)sender
{
	const NSInteger index = [_regionPopUpButton indexOfSelectedItem];
	if (index < 0 || index >= (NSInteger)MEMVIEW_REGION_COUNT)
	{
		return;
	}
	
	[self selectRegion:(MemoryViewerRegionType)index];
}

- (void) viewModeChanged:(id)sender
{
	const NSInteger index = [_viewModePopUpButton indexOfSelectedItem];
	if (index < 0 || index > 2)
	{
		return;
	}
	
	[_hexView setViewMode:(MemoryViewerViewMode)index];
	
	// Re-align the cursor to the start of the new unit size.
	if ([_hexView selectionEnabled])
	{
		const u32 alignment = (u32)[_hexView bytesPerUnit];
		[_hexView setSelectionAddress:([_hexView selectionAddress] & ~(alignment - 1)) reveal:NO];
	}
	
	[self updateSelectionLabel:[_hexView selectionAddress]];
}

- (void) goToAddress:(u32)address
{
	const MemoryViewerRegionType region = [_hexView region];
	
	// ARM9/ARM7 memory is mirrored in 16MB blocks; jumping to another block
	// moves the region's base address, exactly like the Windows frontend.
	if (region == MEMVIEW_ARM9 || region == MEMVIEW_ARM7)
	{
		MemoryViewerRegionSetBaseAddress(region, address & 0xFF000000);
	}
	
	const MemoryViewerRegionInfo *info = MemoryViewerRegionGet(region);
	const u32 base = info->hardwareAddress;
	const u32 lastStart = base + info->size - [_hexView windowSize];
	
	u32 viewAddress = address & 0xFFFFFFF0;
	if (viewAddress < base)
	{
		viewAddress = base;
	}
	if (viewAddress > lastStart)
	{
		viewAddress = lastStart & 0xFFFFFFF0;
	}
	
	[_hexView setAddress:viewAddress];
	[_hexView setSelectionAddress:(viewAddress | (address & 0x0000000F)) reveal:NO];
	[self updateAddressField:viewAddress];
}

- (void) goButtonPressed:(id)sender
{
	u32 address = 0;
	if (!ParseHexAddress([_addressField stringValue], &address))
	{
		NSBeep();
		[self updateAddressField:[_hexView address]];
		return;
	}
	
	[self goToAddress:address];
	[self addAddressToHistory:address];
}

#pragma mark Public API

- (void) showWindowForRegion:(MemoryViewerRegionType)region address:(u32)address
{
	if (!_didInitializeRegions)
	{
		MemoryViewerResetRegions();
		_didInitializeRegions = YES;
	}
	
	[self selectRegion:region];
	
	if (address != 0xFFFFFFFF)
	{
		[self goToAddress:address];
	}
	
	[self showWindow:self];
	[[self window] makeKeyAndOrderFront:self];
	[self startRefreshTimer];
	[[self window] makeFirstResponder:_hexView];
}

#pragma mark MemoryHexViewDelegate

- (void) memoryHexView:(MemoryHexView *)view didSelectAddress:(u32)address
{
	[self updateSelectionLabel:address];
}

- (void) updateSelectionLabel:(u32)address
{
	NSString *text = [NSString stringWithFormat:@"%08Xh", (unsigned)address];
	
	// The Windows frontend shows the Q16.16 fixed point reading of the selected
	// word in Words mode; keep that behaviour here.
	if (([_hexView viewMode] == MemoryViewerViewModeWord) && [_hexView selectionEnabled])
	{
		const u32 raw = [_hexView displayedValueAtAddress:address];
		text = [text stringByAppendingFormat:@"  %.4f", (double)(s32)raw / 4096.0];
	}
	
	[_selectionLabel setStringValue:text];
}

- (void) toggleBigEndian:(id)sender
{
	[_hexView setBigEndian:([_bigEndianButton state] == NSControlStateValueOn)];
	[self updateSelectionLabel:[_hexView selectionAddress]];
}

- (void) toggleFullCharacters:(id)sender
{
	[_hexView setFullCharacters:([_fullCharsButton state] == NSControlStateValueOn)];
}

- (void) memoryHexView:(MemoryHexView *)view didChangeViewportAddress:(u32)address
{
	[self updateScroller];
	[self updateAddressField:address];
}

@end
