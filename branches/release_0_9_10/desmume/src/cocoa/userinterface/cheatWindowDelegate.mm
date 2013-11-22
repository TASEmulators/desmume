/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2012 DeSmuME team

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

#import "cheatWindowDelegate.h"

#import "cocoa_globals.h"
#import "cocoa_cheat.h"
#import "cocoa_util.h"


@implementation CheatWindowDelegate

@dynamic dummyObject;
@synthesize window;
@synthesize cheatConfigBox;
@synthesize cheatSearchView;
@synthesize cheatListTable;
@synthesize cheatSearchListTable;
@synthesize cheatListController;
@synthesize cheatSearchListController;
@synthesize cheatDatabaseController;
@synthesize cheatWindowController;
@synthesize cheatSelectedItemController;

@synthesize viewConfigureNoSelection;
@synthesize viewConfigureInternalCheat;
@synthesize viewConfigureActionReplayCheat;
@synthesize viewConfigureCodeBreakerCheat;
@synthesize viewSearchNoSelection;
@synthesize viewSearchExactValue;
@synthesize viewSearchComparativeStart;
@synthesize viewSearchComparativeContinue;

@synthesize searchField;

@synthesize cheatDatabaseSheet;

@synthesize untitledCount;
@synthesize bindings;
@synthesize cdsCheats;
@synthesize cdsCheatSearch;
@synthesize workingCheat;

- (id)init
{
	self = [super init];
	if(self == nil)
	{
		return nil;
	}
	
	bindings = [[NSMutableDictionary alloc] init];
	if (bindings == nil)
	{
		[self release];
		self = nil;
		return self;
	}
	
	cdsCheatSearch = [[CocoaDSCheatSearch alloc] init];
	if (cdsCheatSearch == nil)
	{
		[bindings release];
		[self release];
		self = nil;
		return self;
	}
	
	workingCheat = nil;
	currentView = nil;
	currentSearchStyleView = nil;
	untitledCount = 0;
		
	[bindings setValue:[NSNumber numberWithBool:NO] forKey:@"hasSelection"];
	[bindings setValue:[NSNumber numberWithBool:NO] forKey:@"hasItems"];
	[bindings setValue:[NSNumber numberWithBool:NO] forKey:@"isRunningSearch"];
	[bindings setValue:[NSNumber numberWithBool:NO] forKey:@"isSearchStarted"];
	[bindings setValue:[NSNumber numberWithInteger:CHEATSEARCH_SEARCHSTYLE_EXACT_VALUE] forKey:@"cheatSearchStyle"];
	[bindings setValue:[NSNumber numberWithInteger:CHEATSEARCH_UNSIGNED] forKey:@"cheatSearchSignType"];
	[bindings setValue:@"Search not started." forKey:@"cheatSearchAddressCount"];
	
	return self;
}

- (void)dealloc
{	
	self.workingCheat = nil;
	self.cdsCheats = nil;
	[cdsCheatSearch release];
	[bindings release];
	
	[super dealloc];
}

- (IBAction) addToList:(id)sender
{
	if (self.cdsCheats == nil)
	{
		return;
	}
	
	NSString *untitledString = nil;
	
	self.untitledCount++;
	if (self.untitledCount > 1)
	{
		untitledString = [NSString stringWithFormat:@"Untitled %ld", (unsigned long)self.untitledCount];
	}
	else
	{
		untitledString = @"Untitled";
	}
	
	CocoaDSCheatItem *newCheatItem = [[[CocoaDSCheatItem alloc] init] autorelease];
	newCheatItem.cheatType = CHEAT_TYPE_INTERNAL;
	newCheatItem.description = untitledString;
	
	[cheatListController addObject:newCheatItem];
	[self.cdsCheats add:newCheatItem];
	[self.cdsCheats save];
	
	[bindings setValue:[NSNumber numberWithBool:YES] forKey:@"hasItems"];
}

- (IBAction) removeFromList:(id)sender
{
	NSMutableArray *cheatList = (NSMutableArray *)[cheatListController content];
	if (cdsCheats == nil || cheatList == nil)
	{
		return;
	}
	
	NSUInteger selectionIndex = [cheatListController selectionIndex];
	if (selectionIndex == NSNotFound)
	{
		return;
	}
	
	NSIndexSet *indexSet = [NSIndexSet indexSetWithIndex:selectionIndex];
	
	NSArray *selectedObjects = [cheatListController selectedObjects];
	CocoaDSCheatItem *selectedCheat = (CocoaDSCheatItem *)[selectedObjects objectAtIndex:0];
	[self.cdsCheats remove:selectedCheat];
	[cheatListController removeObject:selectedCheat];
	
	[self.cdsCheats save];
	[cheatListTable deselectAll:sender];
	
	NSUInteger cheatCount = [cheatList count];
	if (cheatCount > 0)
	{
		if (selectionIndex >= cheatCount)
		{
			selectionIndex--;
			indexSet = [NSIndexSet indexSetWithIndex:selectionIndex];
		}
		
		[cheatListTable selectRowIndexes:indexSet byExtendingSelection:NO];
	}
	else
	{
		[bindings setValue:[NSNumber numberWithBool:NO] forKey:@"hasItems"];
	}
}

- (IBAction) viewDatabase:(id)sender
{
	[NSApp beginSheet:cheatDatabaseSheet
	   modalForWindow:window
		modalDelegate:self
	   didEndSelector:@selector(didEndCheatDatabaseSheet:returnCode:contextInfo:)
		  contextInfo:nil];
}

- (IBAction) setInternalCheatValue:(id)sender
{
	// Force end of editing of any text fields.
	[window makeFirstResponder:nil];
	
	[self.cdsCheats applyInternalCheat:self.workingCheat];
}

- (IBAction) applyConfiguration:(id)sender
{
	if (self.workingCheat == nil)
	{
		return;
	}
	
	// Force end of editing of any text fields.
	[window makeFirstResponder:nil];
	
	[self.workingCheat mergeToParent];
	
	BOOL result = [self.cdsCheats update:self.workingCheat.parent];
	if (result)
	{
		[self.cdsCheats save];
	}
	else
	{
		// TODO: Display an error sheet saying that the cheat applying failed.
	}
}

- (IBAction) selectCheatType:(id)sender
{
	NSInteger cheatTypeID = [CocoaDSUtil getIBActionSenderTag:sender];
	CocoaDSCheatItem *cheatItem = [cheatSelectedItemController content];
	
	cheatItem.cheatType = cheatTypeID;
	
	[self setCheatConfigViewByType:cheatTypeID];
}

- (IBAction) selectCheatSearchStyle:(id)sender
{
	NSInteger searchStyle = [CocoaDSUtil getIBActionSenderTag:sender];
	[self.bindings setValue:[NSNumber numberWithInteger:searchStyle] forKey:@"cheatSearchStyle"];
	[self setCheatSearchViewByStyle:searchStyle];
}

- (IBAction) runExactValueSearch:(id)sender
{
	if (self.workingCheat == nil)
	{
		return;
	}
	
	[bindings setValue:[NSNumber numberWithBool:YES] forKey:@"isSearchStarted"];
	[bindings setValue:[NSNumber numberWithBool:YES] forKey:@"isRunningSearch"];
	
	NSInteger value = [searchField integerValue];
	UInt8 byteSize = self.workingCheat.bytes;
	NSInteger signType = [(NSNumber *)[self.bindings valueForKey:@"cheatSearchSignType"] integerValue];
	NSUInteger addressCount = [cdsCheatSearch runExactValueSearch:value byteSize:byteSize signType:signType];
	[bindings setValue:[NSNumber numberWithUnsignedInteger:addressCount] forKey:@"cheatSearchAddressCount"];
	[cheatSearchListController setContent:cdsCheatSearch.addressList];
	
	[bindings setValue:[NSNumber numberWithBool:NO] forKey:@"isRunningSearch"];
	 
}

- (IBAction) runComparativeSearch:(id)sender
{
	if (self.workingCheat == nil)
	{
		return;
	}
	
	[bindings setValue:[NSNumber numberWithBool:YES] forKey:@"isSearchStarted"];
	[bindings setValue:[NSNumber numberWithBool:YES] forKey:@"isRunningSearch"];
	
	if (cdsCheatSearch.searchCount == 0)
	{
		[bindings setValue:@"Running initial search..." forKey:@"cheatSearchAddressCount"];
		[window displayIfNeeded];
	}
	
	NSInteger compSearchTypeID = [CocoaDSUtil getIBActionSenderTag:sender];
	UInt8 byteSize = self.workingCheat.bytes;
	NSInteger signType = [(NSNumber *)[self.bindings valueForKey:@"cheatSearchSignType"] integerValue];
	NSUInteger addressCount = [cdsCheatSearch runComparativeSearch:compSearchTypeID byteSize:byteSize signType:signType];
	[bindings setValue:[NSNumber numberWithUnsignedInteger:addressCount] forKey:@"cheatSearchAddressCount"];
	[cheatSearchListController setContent:cdsCheatSearch.addressList];
	
	NSInteger searchStyle = [(NSNumber *)[self.bindings valueForKey:@"cheatSearchStyle"] integerValue];
	if (searchStyle == CHEATSEARCH_SEARCHSTYLE_COMPARATIVE && cdsCheatSearch.searchCount == 1)
	{
		[self setCheatSearchViewByStyle:CHEATSEARCH_SEARCHSTYLE_COMPARATIVE];
		[bindings setValue:@"Search started!" forKey:@"cheatSearchAddressCount"];
	}
	
	[bindings setValue:[NSNumber numberWithBool:NO] forKey:@"isRunningSearch"];
	 
}

- (void) searchDidFinish:(NSNotification *)aNotification
{
	CocoaDSCheatSearch *searcher = [aNotification object];
	NSInteger addressCount = 0;
	
	if (searcher != nil)
	{
		addressCount = searcher.addressList.count;
		[bindings setValue:[NSNumber numberWithUnsignedInteger:addressCount] forKey:@"cheatSearchAddressCount"];
		[cheatSearchListController setContent:searcher.addressList];
		
		NSInteger searchStyle = [(NSNumber *)[self.bindings valueForKey:@"cheatSearchStyle"] integerValue];
		if (searchStyle == CHEATSEARCH_SEARCHSTYLE_COMPARATIVE && searcher.searchCount == 1)
		{
			[self setCheatSearchViewByStyle:CHEATSEARCH_SEARCHSTYLE_COMPARATIVE];
			[bindings setValue:@"Search started!" forKey:@"cheatSearchAddressCount"];
		}
	}
	
	[bindings setValue:[NSNumber numberWithBool:NO] forKey:@"isRunningSearch"];
}

- (IBAction) resetSearch:(id)sender
{
	[cheatSearchListController setContent:nil];
	[cdsCheatSearch reset];
	[self.bindings setValue:nil forKey:@"cheatSearchSearchValue"];
	[self.bindings setValue:@"Search not started." forKey:@"cheatSearchAddressCount"];
	[self setCheatSearchViewByStyle:[(NSNumber *)[self.bindings valueForKey:@"cheatSearchStyle"] integerValue]];
	[bindings setValue:[NSNumber numberWithBool:NO] forKey:@"isSearchStarted"];
}

- (void) setCheatConfigViewByType:(NSInteger)cheatTypeID
{
	NSView *newView = nil;
	
	if (currentView == nil)
	{
		currentView = viewConfigureNoSelection;
	}
	
	switch (cheatTypeID)
	{
		case CHEAT_TYPE_INTERNAL:
			newView = viewConfigureInternalCheat;
			break;
			
		case CHEAT_TYPE_ACTION_REPLAY:
			newView = viewConfigureActionReplayCheat;
			break;
			
		case CHEAT_TYPE_CODE_BREAKER:
			newView = viewConfigureCodeBreakerCheat;
			break;
			
		default:
			break;
	}
	
	if (newView != nil)
	{
		NSRect frameRect = [currentView frame];
		[currentView retain];
		[cheatConfigBox replaceSubview:currentView with:newView];
		currentView = newView;
		[currentView setFrame:frameRect];
	}
}

- (void) setCheatSearchViewByStyle:(NSInteger)searchStyleID
{
	NSView *newView = nil;
	
	if (currentSearchStyleView == nil)
	{
		currentSearchStyleView = viewSearchNoSelection;
	}
	
	switch (searchStyleID)
	{
		case CHEATSEARCH_SEARCHSTYLE_EXACT_VALUE:
			newView = viewSearchExactValue;
			break;
			
		case CHEATSEARCH_SEARCHSTYLE_COMPARATIVE:
			if (cdsCheatSearch.searchCount == 0)
			{
				newView = viewSearchComparativeStart;
			}
			else
			{
				newView = viewSearchComparativeContinue;
			}
			break;
			
		default:
			break;
	}
	
	if (newView != nil)
	{
		NSRect frameRect = [currentSearchStyleView frame];
		[currentSearchStyleView retain];
		[cheatSearchView replaceSubview:currentSearchStyleView with:newView];
		currentSearchStyleView = newView;
		[currentSearchStyleView setFrame:frameRect];
	}
}

- (IBAction) selectAllCheatsInDatabase:(id)sender
{
	NSMutableArray *dbList = [cheatDatabaseController content];
	if (dbList == nil)
	{
		return;
	}
	
	for (CocoaDSCheatItem *cheatItem in dbList)
	{
		cheatItem.willAdd = YES;
	}
}

- (IBAction) selectNoneCheatsInDatabase:(id)sender
{
	NSMutableArray *dbList = [cheatDatabaseController content];
	if (dbList == nil)
	{
		return;
	}
	
	for (CocoaDSCheatItem *cheatItem in dbList)
	{
		cheatItem.willAdd = NO;
	}
}

- (void) addSelectedFromCheatDatabase
{
	NSMutableArray *dbList = [cheatDatabaseController content];
	if (dbList == nil)
	{
		return;
	}
	
	for (CocoaDSCheatItem *cheatItem in dbList)
	{
		if (cheatItem.willAdd)
		{
			CocoaDSCheatItem *newCheatItem = [[[CocoaDSCheatItem alloc] initWithCheatData:cheatItem.data] autorelease];
			[newCheatItem retainData];
			[cheatListController addObject:newCheatItem];
			[self.cdsCheats add:newCheatItem];
		}
	}
	
	if ([dbList count] > 0)
	{
		[self.cdsCheats save];
		[bindings setValue:[NSNumber numberWithBool:YES] forKey:@"hasItems"];
	}
}

- (IBAction) closeCheatDatabaseSheet:(id)sender
{
	NSWindow *sheet = [(NSControl *)sender window];
	NSInteger code = [(NSControl *)sender tag];
	
    [NSApp endSheet:sheet returnCode:code];
}

- (void) didEndCheatDatabaseSheet:(NSWindow *)sheet returnCode:(NSInteger)returnCode contextInfo:(void *)contextInfo
{
	[sheet orderOut:self];
	
	switch (returnCode)
	{
		case NSCancelButton:
			return;
			break;
			
		case NSOKButton:
			[self addSelectedFromCheatDatabase];
			break;
			
		default:
			break;
	}
}

- (void)windowDidBecomeKey:(NSNotification *)notification
{
	[cheatWindowController setContent:self.bindings];
}

- (void)tableViewSelectionDidChange:(NSNotification *)aNotification
{
	NSTableView *table = (NSTableView *)[aNotification object];
	NSInteger rowIndex = [table selectedRow];
	
	if (table == self.cheatListTable)
	{
		if (rowIndex >= 0)
		{
			NSArray *selectedObjects = [cheatListController selectedObjects];
			CocoaDSCheatItem *selectedCheat = [selectedObjects objectAtIndex:0];
			self.workingCheat = [selectedCheat createWorkingCopy];
			[cheatSelectedItemController setContent:self.workingCheat];
			
			[self setCheatConfigViewByType:selectedCheat.cheatType];
			[bindings setValue:[NSNumber numberWithBool:YES] forKey:@"hasSelection"];
		}
		else
		{
			if (self.workingCheat != nil)
			{
				[self.workingCheat.parent destroyWorkingCopy];
			}
			
			[cheatSelectedItemController setContent:nil];
			self.workingCheat = nil;
			
			NSRect frameRect = [currentView frame];
			[currentView retain];
			[cheatConfigBox replaceSubview:currentView with:viewConfigureNoSelection];
			currentView = viewConfigureNoSelection;
			[currentView setFrame:frameRect];
			
			[bindings setValue:[NSNumber numberWithBool:NO] forKey:@"hasSelection"];
		}
	}
	else if (table == self.cheatSearchListTable)
	{
		if (rowIndex >= 0)
		{
			NSArray *selectedObjects = [cheatSearchListController selectedObjects];
			NSMutableDictionary *selectedAddress = [selectedObjects objectAtIndex:0];
			NSString *addressString = [(NSString *)[selectedAddress valueForKey:@"addressString"] substringFromIndex:4];
			
			if (self.workingCheat != nil)
			{
				self.workingCheat.memAddressSixDigitString = addressString;
			}
		}
	}
}

@end
