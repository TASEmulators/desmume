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

#import "RAMWatchWindow.h"

#include <string>
#include <vector>

#include "../../shared/ram_debug_core.h"
#include "../../shared/ramwatch_core.h"

// RAM Watch and RAM Search share the same document/core layer. A search window
// can add a result without linking against this controller by posting this
// notification with address/size/type NSNumbers and an optional comment.
NSString * const kRAMWatchAddFromSearchNotification = @"org.desmume.DeSmuME.addRAMWatch";

static const CGFloat kWindowWidth       = 620.0;
static const CGFloat kWindowHeight      = 460.0;
static const CGFloat kSidebarWidth      = 148.0;
static const CGFloat kSidebarPadding    = 12.0;
static const CGFloat kControlHeight     = 26.0;
static const CGFloat kControlSpacing    = 8.0;
static const NSTimeInterval kRefreshInterval = 0.10;

static NSString * const kPrefAutoLoad       = @"RAMWatchAutoLoad";
static NSString * const kPrefSaveWindowPos = @"RAMWatchSaveWindowPos";
static NSString * const kPrefRecentFiles   = @"RAMWatchRecentFiles";
static NSString * const kPrefWindowFrame   = @"RAMWatchWindowFrame";

static NSString *RAMWatchString(const std::string& text)
{
	NSString *result = [NSString stringWithUTF8String:text.c_str()];
	return (result != nil) ? result : @"";
}

static NSFont *RAMWatchMonospacedFont(CGFloat size)
{
	NSFont *font = [NSFont userFixedPitchFontOfSize:size];
	if (font == nil)
	{
		font = [NSFont systemFontOfSize:size];
	}
	return font;
}

#pragma mark - Drop view

@implementation RAMWatchDropView

@synthesize delegate = _delegate;

- (id)initWithFrame:(NSRect)frame
{
	self = [super initWithFrame:frame];
	if (self != nil)
	{
		_delegate = nil;
		[self registerForDraggedTypes:[NSArray arrayWithObject:NSPasteboardTypeFileURL]];
	}
	return self;
}

- (NSArray *)watchFilePathsInPasteboard:(NSPasteboard *)pasteboard
{
	NSArray *urls = [pasteboard readObjectsForClasses:[NSArray arrayWithObject:[NSURL class]] options:nil];
	NSMutableArray *result = [NSMutableArray array];
	for (id item in urls)
	{
		if ([item isKindOfClass:[NSURL class]] && [(NSURL *)item isFileURL])
		{
			NSString *path = [(NSURL *)item path];
			if ([[path pathExtension] caseInsensitiveCompare:@"wch"] == NSOrderedSame)
			{
				[result addObject:path];
			}
		}
	}
	return result;
}

- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
	NSArray *files = [self watchFilePathsInPasteboard:[sender draggingPasteboard]];
	return ([files count] > 0) ? NSDragOperationCopy : NSDragOperationNone;
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
	NSArray *files = [self watchFilePathsInPasteboard:[sender draggingPasteboard]];
	if ([files count] == 0)
	{
		return NO;
	}

	if ([_delegate respondsToSelector:@selector(ramWatchDropView:didReceiveWatchFile:)])
	{
		[_delegate ramWatchDropView:self didReceiveWatchFile:[files objectAtIndex:0]];
	}
	return YES;
}

@end

#pragma mark - Window controller

@interface RAMWatchWindowController (Private)
- (void) buildInterface;
- (void) loadPreferences;
- (void) savePreferences;
- (void) restoreWindowFrame;
- (void) saveWindowFrame;
- (void) updateInterface;
- (void) updateControlStates;
- (void) updateTitle;
- (void) updateStatusLabel;
- (void) startRefreshTimer;
- (void) stopRefreshTimer;
- (void) refreshTimerFired:(NSTimer *)timer;
- (BOOL) confirmDiscardChanges;
- (void) showErrorMessage:(const std::string&)error;
- (NSString *) defaultWatchFileName;
- (void) loadWatchFile:(NSString *)path append:(BOOL)append;
- (void) openPanelForAppend:(BOOL)append;
- (NSPopUpButton *) editorPopUpWithItems:(NSArray *)titles frame:(NSRect)frame;
- (void) presentWatchEditorWithFields:(NSDictionary *)fields index:(NSInteger)index;
- (void) commitWatchEditor;
- (void) clearWatchEditor;
- (NSDictionary *) fieldsForWatchAtIndex:(NSInteger)index;
@end

@implementation RAMWatchWindowController

@synthesize romFileName = _romFileName;

+ (BOOL) shouldAutoLoadOnLaunch
{
	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
	if (![defaults boolForKey:kPrefAutoLoad])
	{
		return NO;
	}
	return ([[defaults arrayForKey:kPrefRecentFiles] count] > 0);
}

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

	[window setTitle:@"RAM Watch"];
	[window setReleasedWhenClosed:NO];
	[window setMinSize:NSMakeSize(520.0, 360.0)];
	[window setContentMinSize:NSMakeSize(520.0, 360.0)];

	self = [super initWithWindow:window];
	[window release];
	if (self == nil)
	{
		return nil;
	}

	_refreshTimer = nil;
	_editorAccessoryView = nil;
	_editingIndex = -1;
	_didLoadPreferences = NO;
	_didAttemptAutoLoad = NO;

	[self loadPreferences];
	[window setDelegate:self];
	[self buildInterface];
	[self restoreWindowFrame];
	[self updateInterface];

	[[NSNotificationCenter defaultCenter] addObserver:self
	                                         selector:@selector(addWatchFromNotification:)
	                                             name:kRAMWatchAddFromSearchNotification
	                                           object:nil];
	return self;
}

- (void)dealloc
{
	[self stopRefreshTimer];
	[[NSNotificationCenter defaultCenter] removeObserver:self];
	[_editorAccessoryView release];
	[_fileMenu release];
	[_recentMenuItem release];
	[_romFileName release];
	[super dealloc];
}

- (NSButton *)addSidebarButtonWithTitle:(NSString *)title frame:(NSRect)frame action:(SEL)action
{
	NSButton *button = [[NSButton alloc] initWithFrame:frame];
	[button setTitle:title];
	[button setBezelStyle:NSBezelStyleRounded];
	[button setFont:[NSFont systemFontOfSize:11.0]];
	[button setTarget:self];
	[button setAction:action];
	// Keep the controls pinned to the top of the sidebar as the window grows.
	[button setAutoresizingMask:NSViewMinYMargin];
	[_sidebar addSubview:button];
	[button release];
	return button;
}

- (void) buildInterface
{
	NSWindow *window = [self window];
	NSView *contentView = [window contentView];
	const CGFloat tableWidth = kWindowWidth - kSidebarWidth;
	const CGFloat buttonWidth = kSidebarWidth - (2.0 * kSidebarPadding);

	// The drop view sits at the bottom of the stack; controls above it still
	// receive their own mouse events.
	_dropView = [[RAMWatchDropView alloc] initWithFrame:[contentView bounds]];
	[_dropView setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
	[_dropView setDelegate:self];
	[contentView addSubview:_dropView];
	[_dropView release];

	// Left side: the virtual watch list fills all remaining space.
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
	[_tableView setAllowsMultipleSelection:YES];
	[_tableView setAllowsEmptySelection:YES];
	[_tableView setAllowsColumnReordering:NO];
	[_tableView setTarget:self];
	[_tableView setDoubleAction:@selector(editSelectedWatch:)];
	[_tableView setAutoresizingMask:NSViewWidthSizable];
	[_tableScrollView setDocumentView:_tableView];
	[_tableView release];

	NSArray *columns = [NSArray arrayWithObjects:@"Address", @"Value", @"Notes", nil];
	NSArray *widths  = [NSArray arrayWithObjects:[NSNumber numberWithDouble:110.0],
	                                             [NSNumber numberWithDouble:100.0],
	                                             [NSNumber numberWithDouble:tableWidth - 210.0 - 16.0], nil];
	for (NSUInteger i = 0; i < [columns count]; i++)
	{
		NSTableColumn *column = [[[NSTableColumn alloc] initWithIdentifier:[columns objectAtIndex:i]] autorelease];
		[[column headerCell] setStringValue:[columns objectAtIndex:i]];
		[column setWidth:[[widths objectAtIndex:i] doubleValue]];
		[column setMinWidth:60.0];
		[column setEditable:NO];
		if (i == 2)
		{
			[column setResizingMask:NSTableColumnAutoresizingMask];
		}
		[_tableView addTableColumn:column];
	}

	// Right side: a vertical button bar, mirroring the Windows dialog.
	_sidebar = [[NSView alloc] initWithFrame:NSMakeRect(tableWidth, 0.0, kSidebarWidth, kWindowHeight)];
	[_sidebar setAutoresizingMask:(NSViewHeightSizable | NSViewMinXMargin)];
	[contentView addSubview:_sidebar];
	[_sidebar release];

	// Lay out every control from the top down. Pinning all of them to the top
	// (including the status label) keeps them from overlapping when the window
	// is resized shorter; the divider lives in the gap between File and New.
	CGFloat cursor = kWindowHeight - kSidebarPadding;

	_filePopUpButton = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(kSidebarPadding, cursor - kControlHeight, buttonWidth, kControlHeight) pullsDown:YES];
	[_filePopUpButton addItemsWithTitles:[NSArray arrayWithObjects:
		@"File", @"New List", @"Open…", @"Append File…", @"Open Recent",
		@"Save", @"Save As…", @"Auto-load", @"Save Window Position", @"Close", nil]];
	[_filePopUpButton setFont:[NSFont systemFontOfSize:11.0]];
	[_filePopUpButton setAutoresizingMask:NSViewMinYMargin];
	[_sidebar addSubview:_filePopUpButton];
	[_filePopUpButton release];

	_fileMenu = [[_filePopUpButton menu] retain];
	[_fileMenu setDelegate:self];
	NSArray *items = [_fileMenu itemArray];
	[[items objectAtIndex:1] setTarget:self]; [[items objectAtIndex:1] setAction:@selector(newWatchList:)];
	[[items objectAtIndex:2] setTarget:self]; [[items objectAtIndex:2] setAction:@selector(openWatchList:)];
	[[items objectAtIndex:3] setTarget:self]; [[items objectAtIndex:3] setAction:@selector(appendWatchList:)];
	[[items objectAtIndex:5] setTarget:self]; [[items objectAtIndex:5] setAction:@selector(saveWatchList:)];
	[[items objectAtIndex:6] setTarget:self]; [[items objectAtIndex:6] setAction:@selector(saveWatchListAs:)];
	[[items objectAtIndex:7] setTarget:self]; [[items objectAtIndex:7] setAction:@selector(toggleAutoLoad:)];
	[[items objectAtIndex:8] setTarget:self]; [[items objectAtIndex:8] setAction:@selector(toggleSaveWindowPos:)];
	[[items objectAtIndex:9] setTarget:self]; [[items objectAtIndex:9] setAction:@selector(closeWatchWindow:)];

	_recentMenuItem = [[items objectAtIndex:4] retain];
	NSMenu *recentMenu = [[[NSMenu alloc] initWithTitle:@"Open Recent"] autorelease];
	[recentMenu setDelegate:self];
	[_fileMenu setSubmenu:recentMenu forItem:_recentMenuItem];

	cursor -= kControlHeight;

	// Divider strictly inside the gap below the File menu.
	NSBox *divider = [[[NSBox alloc] initWithFrame:NSMakeRect(kSidebarPadding, cursor - kSidebarPadding - 1.0, buttonWidth, 1.0)] autorelease];
	[divider setBoxType:NSBoxSeparator];
	[divider setAutoresizingMask:NSViewMinYMargin];
	[_sidebar addSubview:divider];
	cursor -= (2.0 * kSidebarPadding) + 1.0;

	_newButton       = [self addSidebarButtonWithTitle:@"New"       frame:NSMakeRect(kSidebarPadding, cursor - kControlHeight, buttonWidth, kControlHeight) action:@selector(newWatch:)];
	cursor -= kControlHeight + kControlSpacing;
	_editButton      = [self addSidebarButtonWithTitle:@"Edit"      frame:NSMakeRect(kSidebarPadding, cursor - kControlHeight, buttonWidth, kControlHeight) action:@selector(editSelectedWatch:)];
	cursor -= kControlHeight + kControlSpacing;
	_removeButton    = [self addSidebarButtonWithTitle:@"Remove"    frame:NSMakeRect(kSidebarPadding, cursor - kControlHeight, buttonWidth, kControlHeight) action:@selector(removeSelectedWatches:)];
	cursor -= kControlHeight + kControlSpacing;
	_duplicateButton = [self addSidebarButtonWithTitle:@"Duplicate" frame:NSMakeRect(kSidebarPadding, cursor - kControlHeight, buttonWidth, kControlHeight) action:@selector(duplicateWatch:)];
	cursor -= kControlHeight + kControlSpacing;
	_separatorButton = [self addSidebarButtonWithTitle:@"Separator" frame:NSMakeRect(kSidebarPadding, cursor - kControlHeight, buttonWidth, kControlHeight) action:@selector(addSeparator:)];
	cursor -= kControlHeight + kControlSpacing;

	const CGFloat halfWidth = (buttonWidth - kControlSpacing) / 2.0;
	_moveUpButton   = [self addSidebarButtonWithTitle:@"↑" frame:NSMakeRect(kSidebarPadding, cursor - kControlHeight, halfWidth, kControlHeight) action:@selector(moveWatchUp:)];
	_moveDownButton = [self addSidebarButtonWithTitle:@"↓" frame:NSMakeRect(kSidebarPadding + halfWidth + kControlSpacing, cursor - kControlHeight, halfWidth, kControlHeight) action:@selector(moveWatchDown:)];
	cursor -= kControlHeight + kControlSpacing;

	_statusLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(kSidebarPadding, cursor - 20.0, buttonWidth, 20.0)];
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
}

#pragma mark Preferences

- (void) loadPreferences
{
	if (_didLoadPreferences)
	{
		return;
	}
	_didLoadPreferences = YES;

	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
	RAMWatchCore& core = RAMWatchCore::Get();
	core.SetAutoLoad([defaults boolForKey:kPrefAutoLoad]);
	core.SetSaveWindowPos([defaults boolForKey:kPrefSaveWindowPos]);

	NSArray *recent = [defaults arrayForKey:kPrefRecentFiles];
	std::vector<std::string> files;
	for (id path in recent)
	{
		if ([path isKindOfClass:[NSString class]] && [(NSString *)path length] > 0)
		{
			files.push_back([(NSString *)path UTF8String]);
		}
	}
	core.SetRecentFiles(files);
}

- (void) savePreferences
{
	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
	const RAMWatchCore& core = RAMWatchCore::Get();
	[defaults setBool:(core.AutoLoad() ? YES : NO) forKey:kPrefAutoLoad];
	[defaults setBool:(core.SaveWindowPos() ? YES : NO) forKey:kPrefSaveWindowPos];

	const std::vector<std::string>& recent = core.RecentFiles();
	NSMutableArray *recentArray = [NSMutableArray arrayWithCapacity:recent.size()];
	for (size_t i = 0; i < recent.size(); i++)
	{
		[recentArray addObject:RAMWatchString(recent[i])];
	}
	[defaults setObject:recentArray forKey:kPrefRecentFiles];
}

- (void) restoreWindowFrame
{
	if (!RAMWatchCore::Get().SaveWindowPos())
	{
		return;
	}

	NSString *frameString = [[NSUserDefaults standardUserDefaults] stringForKey:kPrefWindowFrame];
	if ([frameString length] == 0)
	{
		return;
	}

	NSRect frame = NSRectFromString(frameString);
	if (NSIsEmptyRect(frame))
	{
		return;
	}
	const NSSize minSize = [[self window] minSize];
	if (frame.size.width < minSize.width)  frame.size.width = minSize.width;
	if (frame.size.height < minSize.height) frame.size.height = minSize.height;
	// Reject frames that ended up completely off screen.
	if (!NSIntersectsRect(frame, [[NSScreen mainScreen] visibleFrame]))
	{
		return;
	}
	[[self window] setFrame:frame display:NO];
}

- (void) saveWindowFrame
{
	if (!RAMWatchCore::Get().SaveWindowPos())
	{
		return;
	}
	[[NSUserDefaults standardUserDefaults] setObject:NSStringFromRect([[self window] frame])
	                                          forKey:kPrefWindowFrame];
}

#pragma mark Window lifecycle

- (void) showRAMWatchWindow
{
	[self loadPreferences];

	if (!_didAttemptAutoLoad)
	{
		_didAttemptAutoLoad = YES;
		RAMWatchCore& core = RAMWatchCore::Get();
		if (core.AutoLoad() && (core.Count() == 0) && !core.RecentFiles().empty())
		{
			std::string error;
			core.OpenRecentFile(0, error);
		}
	}

	[self updateInterface];
	[self showWindow:self];
	[[self window] makeKeyAndOrderFront:self];
	[self startRefreshTimer];
}

- (BOOL) windowShouldClose:(id)sender
{
	if (![self confirmDiscardChanges])
	{
		return NO;
	}
	[self stopRefreshTimer];
	return YES;
}

- (void) windowWillClose:(NSNotification *)notification
{
	[self savePreferences];
	[self saveWindowFrame];
	[self stopRefreshTimer];
}

- (void) windowDidMove:(NSNotification *)notification
{
	[self saveWindowFrame];
}

- (void) windowDidResize:(NSNotification *)notification
{
	[self saveWindowFrame];
}

- (void) startRefreshTimer
{
	if (_refreshTimer != nil)
	{
		return;
	}
	_refreshTimer = [[NSTimer timerWithTimeInterval:kRefreshInterval
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

	std::vector<bool> changed;
	if (!RAMWatchCore::Get().RefreshValues(changed))
	{
		return;
	}

	// Only redraw changed rows that are actually on screen, matching the
	// owner-data optimization of the Windows list view.
	NSRange visible = [_tableView rowsInRect:[_tableView visibleRect]];
	NSMutableIndexSet *rows = [NSMutableIndexSet indexSet];
	for (NSUInteger i = visible.location; (i < NSMaxRange(visible)) && (i < changed.size()); i++)
	{
		if (changed[i])
		{
			[rows addIndex:i];
		}
	}
	if ([rows count] > 0)
	{
		[_tableView reloadDataForRowIndexes:rows columnIndexes:[NSIndexSet indexSetWithIndex:1]];
	}
}

#pragma mark Table view

- (NSInteger) numberOfRowsInTableView:(NSTableView *)tableView
{
	return (NSInteger)RAMWatchCore::Get().Count();
}

- (id) tableView:(NSTableView *)tableView objectValueForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row
{
	const RAMWatchCore& core = RAMWatchCore::Get();
	if ((row < 0) || ((size_t)row >= core.Count()))
	{
		return @"";
	}

	NSString *columnID = [tableColumn identifier];
	if (core.IsSeparator((size_t)row))
	{
		return [columnID isEqualToString:@"Notes"] ? @"────────────────────────────" : @"";
	}

	if ([columnID isEqualToString:@"Address"])
	{
		return RAMWatchString(RAMWatchCore::DisplayAddress(core.At((size_t)row)));
	}
	if ([columnID isEqualToString:@"Value"])
	{
		return RAMWatchString(core.DisplayValueAt((size_t)row));
	}
	return RAMWatchString(core.At((size_t)row).Comment);
}

- (void) tableView:(NSTableView *)tableView willDisplayCell:(id)cell
    forTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row
{
	if (![cell isKindOfClass:[NSTextFieldCell class]])
	{
		return;
	}

	NSString *columnID = [tableColumn identifier];
	const RAMWatchCore& core = RAMWatchCore::Get();
	const BOOL separator = ((row >= 0) && ((size_t)row < core.Count()) && core.IsSeparator((size_t)row));

	if ([columnID isEqualToString:@"Notes"])
	{
		[cell setFont:[NSFont systemFontOfSize:12.0]];
	}
	else
	{
		[cell setFont:RAMWatchMonospacedFont(12.0)];
	}
	[cell setTextColor:separator ? [NSColor secondaryLabelColor] : [NSColor labelColor]];
}

- (void) tableViewSelectionDidChange:(NSNotification *)notification
{
	[self updateControlStates];
}

#pragma mark Interface state

- (void) updateInterface
{
	[_tableView reloadData];
	[self updateTitle];
	[self updateStatusLabel];
	[self updateControlStates];
}

- (void) updateTitle
{
	const RAMWatchCore& core = RAMWatchCore::Get();
	NSString *title = @"RAM Watch";
	NSString *file = RAMWatchString(core.CurrentFile());

	if ([file length] > 0)
	{
		title = [NSString stringWithFormat:@"RAM Watch — %@", [file lastPathComponent]];
	}
	if (core.IsDirty())
	{
		title = [title stringByAppendingString:@" *"];
	}
	[[self window] setTitle:title];
}

- (void) updateStatusLabel
{
	const RAMWatchCore& core = RAMWatchCore::Get();
	[_statusLabel setStringValue:[NSString stringWithFormat:@"%lu watch%@",
	                              (unsigned long)core.Count(),
	                              (core.Count() == 1) ? @"" : @"es"]];
}

- (void) updateControlStates
{
	const NSInteger selected = [_tableView selectedRow];
	const NSUInteger selectedCount = [[_tableView selectedRowIndexes] count];
	const RAMWatchCore& core = RAMWatchCore::Get();
	const BOOL canAdd = (core.Count() < RAMWatchCore::kMaxWatchCount);

	[_newButton setEnabled:canAdd];
	[_editButton setEnabled:(selectedCount == 1)];
	[_removeButton setEnabled:(selectedCount >= 1)];
	[_duplicateButton setEnabled:(selectedCount == 1) && canAdd];
	[_separatorButton setEnabled:canAdd];
	[_moveUpButton setEnabled:(selectedCount == 1) && (selected > 0)];
	[_moveDownButton setEnabled:(selectedCount == 1) && (selected >= 0) && ((size_t)selected + 1 < core.Count())];
}

#pragma mark Menu

- (void) menuNeedsUpdate:(NSMenu *)menu
{
	const RAMWatchCore& core = RAMWatchCore::Get();

	if (menu == _fileMenu)
	{
		NSArray *items = [menu itemArray];
		[[items objectAtIndex:7] setState:core.AutoLoad() ? NSControlStateValueOn : NSControlStateValueOff];
		[[items objectAtIndex:8] setState:core.SaveWindowPos() ? NSControlStateValueOn : NSControlStateValueOff];
		return;
	}

	// Recent-files submenu is rebuilt on every open so it mirrors the core.
	[menu removeAllItems];
	const std::vector<std::string>& recent = core.RecentFiles();
	if (recent.empty())
	{
		NSMenuItem *none = [[[NSMenuItem alloc] initWithTitle:@"No Recent Files"
		                                               action:NULL
		                                        keyEquivalent:@""] autorelease];
		[none setEnabled:NO];
		[menu addItem:none];
		return;
	}

	for (size_t i = 0; i < recent.size(); i++)
	{
		NSString *path = RAMWatchString(recent[i]);
		NSString *title = [NSString stringWithFormat:@"%lu. %@", (unsigned long)(i + 1), [path lastPathComponent]];
		NSMenuItem *item = [[[NSMenuItem alloc] initWithTitle:title
		                                               action:@selector(openRecentWatchList:)
		                                        keyEquivalent:@""] autorelease];
		[item setTarget:self];
		[item setTag:(NSInteger)i];
		[item setToolTip:path];
		[menu addItem:item];
	}
}

#pragma mark File operations

- (void) showErrorMessage:(const std::string&)error
{
	NSAlert *alert = [[[NSAlert alloc] init] autorelease];
	[alert setMessageText:@"RAM Watch"];
	[alert setInformativeText:(!error.empty()) ? RAMWatchString(error) : @"An unknown error occurred."];
	[alert addButtonWithTitle:@"OK"];
	[alert runModal];
}

- (BOOL) confirmDiscardChanges
{
	RAMWatchCore& core = RAMWatchCore::Get();
	if (!core.IsDirty())
	{
		return YES;
	}

	NSAlert *alert = [[[NSAlert alloc] init] autorelease];
	[alert setMessageText:@"Save changes to the watch list?"];
	[alert setInformativeText:@"Your changes will be lost if you don't save them."];
	[alert addButtonWithTitle:@"Save"];
	[alert addButtonWithTitle:@"Don't Save"];
	[alert addButtonWithTitle:@"Cancel"];

	const NSModalResponse response = [alert runModal];
	if (response == NSAlertFirstButtonReturn)
	{
		return [self saveWatchList:self];
	}
	if (response == NSAlertSecondButtonReturn)
	{
		core.SetDirty(NO);
		return YES;
	}
	return NO;
}

- (NSString *) defaultWatchFileName
{
	const std::string& current = RAMWatchCore::Get().CurrentFile();
	if (!current.empty())
	{
		return [RAMWatchString(current) lastPathComponent];
	}
	if ([_romFileName length] > 0)
	{
		return [_romFileName stringByAppendingPathExtension:@"wch"];
	}
	return @"Untitled.wch";
}

- (void) newWatchList:(id)sender
{
	if (![self confirmDiscardChanges])
	{
		return;
	}
	RAMWatchCore::Get().Clear();
	[self updateInterface];
	[self savePreferences];
}

- (void) openWatchList:(id)sender
{
	[self openPanelForAppend:NO];
}

- (void) appendWatchList:(id)sender
{
	[self openPanelForAppend:YES];
}

- (void) openPanelForAppend:(BOOL)append
{
	if (!append && ![self confirmDiscardChanges])
	{
		return;
	}

	NSOpenPanel *panel = [NSOpenPanel openPanel];
	[panel setCanChooseFiles:YES];
	[panel setCanChooseDirectories:NO];
	[panel setAllowsMultipleSelection:NO];
	[panel setAllowedFileTypes:[NSArray arrayWithObject:@"wch"]];
	[panel setMessage:append ? @"Choose a watch list to append." : @"Choose a watch list to open."];

	if ([panel runModal] != NSModalResponseOK)
	{
		return;
	}
	NSString *path = [[panel URL] path];
	if ([path length] > 0)
	{
		[self loadWatchFile:path append:append];
	}
}

- (void) loadWatchFile:(NSString *)path append:(BOOL)append
{
	std::string error;
	if (!RAMWatchCore::Get().LoadFile([path UTF8String], append, error))
	{
		[self showErrorMessage:error];
		return;
	}
	[self savePreferences];
	[self updateInterface];
}

- (BOOL) saveWatchList:(id)sender
{
	RAMWatchCore& core = RAMWatchCore::Get();
	if (!core.CurrentFile().empty())
	{
		std::string error;
		if (!core.SaveCurrentFile(error))
		{
			[self showErrorMessage:error];
			return NO;
		}
		[self savePreferences];
		[self updateInterface];
		return YES;
	}
	return [self saveWatchListAs:sender];
}

- (BOOL) saveWatchListAs:(id)sender
{
	NSSavePanel *panel = [NSSavePanel savePanel];
	[panel setAllowedFileTypes:[NSArray arrayWithObject:@"wch"]];
	[panel setCanCreateDirectories:YES];
	[panel setNameFieldStringValue:[self defaultWatchFileName]];
	[panel setMessage:@"Save the current watch list."];

	if ([panel runModal] != NSModalResponseOK)
	{
		return NO;
	}
	NSString *path = [[panel URL] path];
	if ([path length] == 0)
	{
		return NO;
	}

	std::string error;
	if (!RAMWatchCore::Get().SaveAsFile([path UTF8String], error))
	{
		[self showErrorMessage:error];
		return NO;
	}
	[self savePreferences];
	[self updateInterface];
	return YES;
}

- (void) openRecentWatchList:(id)sender
{
	const NSInteger index = [sender tag];
	if (index < 0)
	{
		return;
	}
	if (![self confirmDiscardChanges])
	{
		return;
	}

	std::string error;
	if (!RAMWatchCore::Get().OpenRecentFile((size_t)index, error))
	{
		if (!error.empty())
		{
			[self showErrorMessage:error];
		}
	}
	[self savePreferences];
	[self updateInterface];
}

- (void) toggleAutoLoad:(id)sender
{
	RAMWatchCore& core = RAMWatchCore::Get();
	core.SetAutoLoad(!core.AutoLoad());
	[self savePreferences];
}

- (void) toggleSaveWindowPos:(id)sender
{
	RAMWatchCore& core = RAMWatchCore::Get();
	core.SetSaveWindowPos(!core.SaveWindowPos());
	if (core.SaveWindowPos())
	{
		[self saveWindowFrame];
	}
	[self savePreferences];
}

- (void) closeWatchWindow:(id)sender
{
	[[self window] performClose:self];
}

#pragma mark Watch editing

- (NSDictionary *) fieldsForWatchAtIndex:(NSInteger)index
{
	uint32_t address = 0;
	char sizeCode = 'b';
	char typeCode = 's';
	NSString *comment = @"";

	if (index >= 0 && (size_t)index < RAMWatchCore::Get().Count())
	{
		const RAMWatchCore::Watch& watch = RAMWatchCore::Get().At((size_t)index);
		address = watch.Address;
		sizeCode = watch.Size;
		typeCode = watch.Type;
		comment = RAMWatchString(watch.Comment);
	}

	return [NSDictionary dictionaryWithObjectsAndKeys:
		[NSNumber numberWithUnsignedInt:address], @"address",
		[NSNumber numberWithChar:sizeCode], @"size",
		[NSNumber numberWithChar:typeCode], @"type",
		comment, @"comment", nil];
}

- (void) newWatch:(id)sender
{
	if (RAMWatchCore::Get().Count() >= RAMWatchCore::kMaxWatchCount)
	{
		return;
	}
	[self presentWatchEditorWithFields:[self fieldsForWatchAtIndex:-1] index:-1];
}

- (void) editSelectedWatch:(id)sender
{
	const NSInteger row = [_tableView selectedRow];
	if ((row < 0) || ((size_t)row >= RAMWatchCore::Get().Count()))
	{
		return;
	}
	if (RAMWatchCore::Get().IsSeparator((size_t)row))
	{
		return;
	}
	[self presentWatchEditorWithFields:[self fieldsForWatchAtIndex:row] index:row];
}

- (void) duplicateWatch:(id)sender
{
	const NSInteger row = [_tableView selectedRow];
	if ((row < 0) || ((size_t)row >= RAMWatchCore::Get().Count()))
	{
		return;
	}
	// Like the Windows Duplicate command, the editor opens prefilled so the
	// user can change the address or type before inserting.
	[self presentWatchEditorWithFields:[self fieldsForWatchAtIndex:row] index:-1];
}

- (void) removeSelectedWatches:(id)sender
{
	NSIndexSet *selected = [[_tableView selectedRowIndexes] copy];
	RAMWatchCore& core = RAMWatchCore::Get();
	NSUInteger index = [selected firstIndex];
	NSMutableIndexSet *toRemove = [NSMutableIndexSet indexSet];
	while (index != NSNotFound)
	{
		if (index < core.Count())
		{
			[toRemove addIndex:index];
		}
		index = [selected indexGreaterThanIndex:index];
	}
	[selected release];

	// Remove back-to-front so indexes stay valid.
	index = [toRemove lastIndex];
	while (index != NSNotFound)
	{
		core.Remove((size_t)index);
		if (index == 0)
		{
			break;
		}
		index = [toRemove indexLessThanIndex:index];
	}

	[_tableView deselectAll:self];
	[self updateInterface];
}

- (void) addSeparator:(id)sender
{
	RAMWatchCore::Get().AddSeparator();
	[self updateInterface];
}

- (void) moveWatchUp:(id)sender
{
	const NSInteger row = [_tableView selectedRow];
	if (row <= 0 || !RAMWatchCore::Get().MoveUp((size_t)row))
	{
		return;
	}
	[_tableView selectRowIndexes:[NSIndexSet indexSetWithIndex:(NSUInteger)(row - 1)] byExtendingSelection:NO];
	[self updateInterface];
}

- (void) moveWatchDown:(id)sender
{
	const NSInteger row = [_tableView selectedRow];
	if (row < 0 || !RAMWatchCore::Get().MoveDown((size_t)row))
	{
		return;
	}
	[_tableView selectRowIndexes:[NSIndexSet indexSetWithIndex:(NSUInteger)(row + 1)] byExtendingSelection:NO];
	[self updateInterface];
}

- (NSPopUpButton *) editorPopUpWithItems:(NSArray *)titles frame:(NSRect)frame
{
	NSPopUpButton *button = [[[NSPopUpButton alloc] initWithFrame:frame pullsDown:NO] autorelease];
	[button addItemsWithTitles:titles];
	[button setFont:[NSFont systemFontOfSize:12.0]];
	return button;
}

- (void) presentWatchEditorWithFields:(NSDictionary *)fields index:(NSInteger)index
{
	if (_editorAccessoryView != nil)
	{
		return; // a sheet is already up
	}
	_editingIndex = index;

	NSView *accessory = [[[NSView alloc] initWithFrame:NSMakeRect(0.0, 0.0, 340.0, 132.0)] autorelease];

	NSTextField *addressLabel = [[[NSTextField alloc] initWithFrame:NSMakeRect(0.0, 104.0, 80.0, 20.0)] autorelease];
	[addressLabel setStringValue:@"Address:"];
	[addressLabel setEditable:NO]; [addressLabel setBezeled:NO]; [addressLabel setDrawsBackground:NO];
	[accessory addSubview:addressLabel];

	_editorAddressField = [[[NSTextField alloc] initWithFrame:NSMakeRect(84.0, 102.0, 120.0, 24.0)] autorelease];
	[_editorAddressField setStringValue:[NSString stringWithFormat:@"%08X",
	                                     (unsigned)[[fields objectForKey:@"address"] unsignedIntValue]]];
	[_editorAddressField setFont:RAMWatchMonospacedFont(12.0)];
	[accessory addSubview:_editorAddressField];

	_editorSizePopUp = [self editorPopUpWithItems:[NSArray arrayWithObjects:@"1 Byte", @"2 Bytes", @"4 Bytes", nil]
	                                        frame:NSMakeRect(212.0, 102.0, 124.0, 24.0)];
	const char sizeCode = [[fields objectForKey:@"size"] charValue];
	[_editorSizePopUp selectItemAtIndex:(sizeCode == 'w') ? 1 : (sizeCode == 'd') ? 2 : 0];
	[accessory addSubview:_editorSizePopUp];

	_editorTypePopUp = [self editorPopUpWithItems:[NSArray arrayWithObjects:@"Signed", @"Unsigned", @"Hex", @"20.12 Fixed", nil]
	                                        frame:NSMakeRect(84.0, 70.0, 252.0, 24.0)];
	const char typeCode = [[fields objectForKey:@"type"] charValue];
	const NSInteger typeIndex = (typeCode == 'u') ? 1 : (typeCode == 'h') ? 2 : (typeCode == 'f') ? 3 : 0;
	[_editorTypePopUp selectItemAtIndex:typeIndex];
	[accessory addSubview:_editorTypePopUp];

	NSTextField *commentLabel = [[[NSTextField alloc] initWithFrame:NSMakeRect(0.0, 42.0, 80.0, 20.0)] autorelease];
	[commentLabel setStringValue:@"Notes:"];
	[commentLabel setEditable:NO]; [commentLabel setBezeled:NO]; [commentLabel setDrawsBackground:NO];
	[accessory addSubview:commentLabel];

	_editorCommentField = [[[NSTextField alloc] initWithFrame:NSMakeRect(84.0, 40.0, 252.0, 24.0)] autorelease];
	[_editorCommentField setStringValue:[fields objectForKey:@"comment"]];
	[accessory addSubview:_editorCommentField];

	NSTextField *hint = [[[NSTextField alloc] initWithFrame:NSMakeRect(84.0, 8.0, 252.0, 28.0)] autorelease];
	[hint setStringValue:@"Hexadecimal address, e.g. 02000000"];
	[hint setEditable:NO]; [hint setBezeled:NO]; [hint setDrawsBackground:NO];
	[hint setFont:[NSFont systemFontOfSize:10.0]];
	[hint setTextColor:[NSColor secondaryLabelColor]];
	[accessory addSubview:hint];

	_editorAccessoryView = [accessory retain];

	NSAlert *alert = [[[NSAlert alloc] init] autorelease];
	[alert setMessageText:(index < 0) ? @"New Watch" : @"Edit Watch"];
	[alert setInformativeText:@"Choose the address, size and display type."];
	[alert addButtonWithTitle:@"OK"];
	[alert addButtonWithTitle:@"Cancel"];
	[alert setAccessoryView:accessory];

	[alert beginSheetModalForWindow:[self window]
	              completionHandler:^(NSModalResponse response) {
	                  if (response == NSAlertFirstButtonReturn)
	                  {
	                      [self commitWatchEditor];
	                  }
	                  [self clearWatchEditor];
	              }];
}

- (void) commitWatchEditor
{
	bool valid = false;
	uint32_t address = RAMDebugCore::ParseAddress([[_editorAddressField stringValue] UTF8String], &valid);
	RAMDebugCore::NormalizeAddress(address);

	if (!valid || !RAMDebugCore::IsRAMAddressValid(address))
	{
		NSBeep();
		[self showErrorMessage:"Invalid Address"];
		return;
	}

	const NSInteger sizeIndex = [_editorSizePopUp indexOfSelectedItem];
	const NSInteger typeIndex = [_editorTypePopUp indexOfSelectedItem];
	const char sizeCodes[3] = { 'b', 'w', 'd' };
	const char typeCodes[4] = { 's', 'u', 'h', 'f' };

	RAMWatchCore::Watch watch;
	watch.Address = address;
	watch.Size = sizeCodes[(sizeIndex >= 0 && sizeIndex < 3) ? sizeIndex : 0];
	watch.Type = typeCodes[(typeIndex >= 0 && typeIndex < 4) ? typeIndex : 0];
	watch.WrongEndian = false;
	const char *comment = [[_editorCommentField stringValue] UTF8String];
	watch.Comment = (comment != 0) ? comment : "";

	RAMWatchCore& core = RAMWatchCore::Get();
	const BOOL inserted = (_editingIndex >= 0)
	                    ? core.Replace((size_t)_editingIndex, watch)
	                    : core.Insert(watch);
	if (!inserted)
	{
		[self showErrorMessage:"A watch with this address, size and type already exists, or the list is full."];
		return;
	}
	[self updateInterface];
}

- (void) clearWatchEditor
{
	[_editorAccessoryView release];
	_editorAccessoryView = nil;
	_editorAddressField = nil;
	_editorCommentField = nil;
	_editorSizePopUp = nil;
	_editorTypePopUp = nil;
	_editingIndex = -1;
}

#pragma mark Drag and drop / RAM Search integration

- (void) ramWatchDropView:(RAMWatchDropView *)view didReceiveWatchFile:(NSString *)path
{
	if (![self confirmDiscardChanges])
	{
		return;
	}
	[self loadWatchFile:path append:NO];
}

- (void) addWatchFromNotification:(NSNotification *)notification
{
	NSDictionary *info = [notification userInfo];
	if (info == nil)
	{
		return;
	}
	id address = [info objectForKey:@"address"];
	id size = [info objectForKey:@"size"];
	id type = [info objectForKey:@"type"];
	NSString *comment = [info objectForKey:@"comment"];
	if (![address isKindOfClass:[NSNumber class]] ||
	    ![size isKindOfClass:[NSNumber class]] ||
	    ![type isKindOfClass:[NSNumber class]])
	{
		return;
	}

	[self addWatchFromSearchResult:[address unsignedIntValue]
	                          size:(char)[size intValue]
	                          type:(char)[type intValue]
	                       comment:(comment != nil) ? comment : @""];
}

- (BOOL) addWatchFromSearchResult:(uint32_t)address
                             size:(char)sizeCode
                             type:(char)typeCode
                          comment:(NSString *)comment
{
	const char *cComment = [comment UTF8String];
	const BOOL inserted = RAMWatchCore::Get().InsertFromSearch(address, sizeCode, typeCode,
	                                                           (cComment != 0) ? cComment : "");
	[self updateInterface];
	if (inserted)
	{
		[self showRAMWatchWindow];
	}
	return inserted;
}

@end
