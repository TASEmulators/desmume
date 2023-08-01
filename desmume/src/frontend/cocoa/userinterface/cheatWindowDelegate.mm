/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2012-2023 DeSmuME team

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
#import "CheatDatabaseWindowController.h"

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

@synthesize codeEditorFont;
@synthesize bindings;
@synthesize cdsCheats;
@synthesize workingCheat;
@synthesize currentGameCode;
@synthesize currentGameCRC;

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
	
	currentGameCode = nil;
	currentGameCRC = 0;
	workingCheat = nil;
	currentView = nil;
	currentSearchStyleView = nil;
	codeEditorFont = [NSFont fontWithName:@"Monaco" size:13.0];
		
	[bindings setValue:[NSNumber numberWithBool:NO] forKey:@"hasSelection"];
	[bindings setValue:[NSNumber numberWithBool:NO] forKey:@"isRunningSearch"];
	[bindings setValue:[NSNumber numberWithBool:NO] forKey:@"isSearchStarted"];
	[bindings setValue:[NSNumber numberWithInteger:CheatSearchStyle_ExactValue] forKey:@"cheatSearchStyle"];
	[bindings setValue:[NSNumber numberWithInteger:CHEATSEARCH_UNSIGNED] forKey:@"cheatSearchSignType"];
	[bindings setValue:@"Search not started." forKey:@"cheatSearchAddressCount"];
	
	if ([CocoaDSCheatItem iconDirectory] == nil || [CocoaDSCheatItem iconInternalCheat] == nil || [CocoaDSCheatItem iconActionReplay] == nil || [CocoaDSCheatItem iconCodeBreaker] == nil)
	{
		[CocoaDSCheatItem setIconDirectory:[[NSImage imageNamed:@"NSFolder"] retain]];
		[CocoaDSCheatItem setIconInternalCheat:[[NSImage imageNamed:@"NSApplicationIcon"] retain]];
		[CocoaDSCheatItem setIconActionReplay:[[NSImage imageNamed:@"Icon_ActionReplay_128x128"] retain]];
		[CocoaDSCheatItem setIconCodeBreaker:[[NSImage imageNamed:@"Icon_CodeBreaker_128x128"] retain]];
	}
	
	return self;
}

- (void)dealloc
{
	[self setWorkingCheat:nil];
	[self setCdsCheats:nil];
	[self setCurrentGameCode:nil];
	[bindings release];
	
	[super dealloc];
}

- (BOOL) cheatSystemStart:(CocoaDSCheatManager *)cheatManager
{
	BOOL didStartSuccessfully = NO;
	
	if (cheatManager == nil)
	{
		return didStartSuccessfully;
	}
	
	[self setCdsCheats:cheatManager];
	[self setCurrentGameCode:[cheatManager currentGameCode]];
	[self setCurrentGameCRC:[cheatManager currentGameCRC]];
	[cheatManager loadFromMaster];
	[cheatListController setContent:[cheatManager sessionList]];
	
	[self setCheatSearchViewByStyle:CheatSearchStyle_ExactValue];
	[CheatDatabaseWindowController validateWillAddColumnForAllWindows];
	[CheatDatabaseWindowController validateGameTableFontsForAllWindows];
	
	didStartSuccessfully = YES;
	return didStartSuccessfully;
}

- (void) cheatSystemEnd
{
	CocoaDSCheatManager *cheatManager = [self cdsCheats];
	if (cheatManager != nil)
	{
		[cheatManager save];
	}
	
	[cheatListController setContent:nil];
	[self resetSearch:nil];
	[self setCurrentGameCode:nil];
	[self setCurrentGameCRC:0];
	[self setCdsCheats:nil];
	
	[CheatDatabaseWindowController validateWillAddColumnForAllWindows];
	[CheatDatabaseWindowController validateGameTableFontsForAllWindows];
}

- (IBAction) addToList:(id)sender
{
	if ([self cdsCheats] == nil)
	{
		return;
	}
	
	CocoaDSCheatItem *newCheatItem = [[self cdsCheats] newItem];
	if (newCheatItem != nil)
	{
		[cheatListController setContent:[[self cdsCheats] sessionList]];
		[[self cdsCheats] save];
	}
}

- (IBAction) removeFromList:(id)sender
{
	CocoaDSCheatManager *cheatManager = [self cdsCheats];
	if (cheatManager == nil)
	{
		return;
	}
	
	NSUInteger selectionIndex = [cheatListController selectionIndex];
	if (selectionIndex == NSNotFound)
	{
		return;
	}
	
	NSArray *selectedObjects = [cheatListController selectedObjects];
	CocoaDSCheatItem *selectedCheat = (CocoaDSCheatItem *)[selectedObjects objectAtIndex:0];
	[cheatManager remove:selectedCheat];
	
	[cheatListController setContent:[cheatManager sessionList]];
	[cheatManager save];
	[cheatListTable deselectAll:sender];
	
	NSUInteger cheatCount = [cheatManager itemTotalCount];
	if (cheatCount > 0)
	{
		NSIndexSet *indexSet = [NSIndexSet indexSetWithIndex:selectionIndex];
		
		if (selectionIndex >= cheatCount)
		{
			selectionIndex--;
			indexSet = [NSIndexSet indexSetWithIndex:selectionIndex];
		}
		
		[cheatListTable selectRowIndexes:indexSet byExtendingSelection:NO];
	}
}

- (IBAction) removeAllFromList:(id)sender
{
	CocoaDSCheatManager *cheatManager = [self cdsCheats];
	if (cheatManager == nil)
	{
		return;
	}
	
	[cheatListTable deselectAll:sender];
	
	NSUInteger itemCount = [[cheatManager sessionList] count];
	
	for (NSUInteger i = 0; i < itemCount; i++)
	{
		[cheatManager removeAtIndex:0];
	}
	
	[cheatListController setContent:[cheatManager sessionList]];
	[cheatManager save];
}

- (IBAction) setInternalCheatValue:(id)sender
{
	// Force end of editing of any text fields.
	[window makeFirstResponder:nil];
	
	[[self cdsCheats] directWriteInternalCheat:[self workingCheat]];
}

- (IBAction) applyConfiguration:(id)sender
{
	if ([self workingCheat] == nil)
	{
		return;
	}
	
	// Force end of editing of any text fields.
	[window makeFirstResponder:nil];
	
	[[self workingCheat] mergeToParent];
	[[self cdsCheats] save];
}

- (IBAction) selectCheatType:(id)sender
{
	NSInteger cheatTypeID = [CocoaDSUtil getIBActionSenderTag:sender];
	CocoaDSCheatItem *cheatItem = [cheatSelectedItemController content];
	
	[window makeFirstResponder:nil]; // Force end of editing of any text fields.
	[cheatItem setCheatType:cheatTypeID];
	
	[self setCheatConfigViewByType:cheatTypeID];
}

- (IBAction) selectCheatSearchStyle:(id)sender
{
	NSInteger searchStyle = [CocoaDSUtil getIBActionSenderTag:sender];
	[bindings setValue:[NSNumber numberWithInteger:searchStyle] forKey:@"cheatSearchStyle"];
	[self setCheatSearchViewByStyle:searchStyle];
}

- (IBAction) runExactValueSearch:(id)sender
{
	if ([self workingCheat] == nil)
	{
		return;
	}
	
	[bindings setValue:[NSNumber numberWithBool:YES] forKey:@"isSearchStarted"];
	[bindings setValue:[NSNumber numberWithBool:YES] forKey:@"isRunningSearch"];
	
	NSInteger value = [searchField integerValue];
	UInt8 byteSize = [[self workingCheat] bytes];
	NSInteger signType = [(NSNumber *)[bindings valueForKey:@"cheatSearchSignType"] integerValue];
	
	NSUInteger resultsCount = [cdsCheats runExactValueSearch:value byteSize:byteSize signType:signType];
	
	[bindings setValue:[NSNumber numberWithUnsignedInteger:resultsCount] forKey:@"cheatSearchAddressCount"];
	[cheatSearchListController setContent:[[self cdsCheats] searchResultsList]];
	
	[bindings setValue:[NSNumber numberWithBool:NO] forKey:@"isRunningSearch"];
}

- (IBAction) runComparativeSearch:(id)sender
{
	if ([self workingCheat] == nil)
	{
		return;
	}
	
	[bindings setValue:[NSNumber numberWithBool:YES] forKey:@"isSearchStarted"];
	[bindings setValue:[NSNumber numberWithBool:YES] forKey:@"isRunningSearch"];
	
	const BOOL wasSearchAlreadyStarted = [cdsCheats searchDidStart];
	if (!wasSearchAlreadyStarted)
	{
		[bindings setValue:@"Running initial search..." forKey:@"cheatSearchAddressCount"];
		[window displayIfNeeded];
	}
	
	NSInteger compSearchTypeID = [CocoaDSUtil getIBActionSenderTag:sender];
	UInt8 byteSize = [[self workingCheat] bytes];
	NSInteger signType = [(NSNumber *)[bindings valueForKey:@"cheatSearchSignType"] integerValue];
	
	NSUInteger newResultsCount = [cdsCheats runComparativeSearch:compSearchTypeID byteSize:byteSize signType:signType];
	[cheatSearchListController setContent:[[self cdsCheats] searchResultsList]];
	
	NSInteger searchStyle = [(NSNumber *)[bindings valueForKey:@"cheatSearchStyle"] integerValue];
	if (searchStyle == CheatSearchStyle_Comparative)
	{
		[self setCheatSearchViewByStyle:CheatSearchStyle_Comparative];
	}
	
	if (!wasSearchAlreadyStarted)
	{
		[bindings setValue:@"Search started!" forKey:@"cheatSearchAddressCount"];
	}
	else
	{
		[bindings setValue:[NSNumber numberWithUnsignedInteger:newResultsCount] forKey:@"cheatSearchAddressCount"];
	}
	
	[bindings setValue:[NSNumber numberWithBool:NO] forKey:@"isRunningSearch"];
}

- (IBAction) resetSearch:(id)sender
{
	[[self cdsCheats] searchReset];
	[cheatSearchListController setContent:[[self cdsCheats] searchResultsList]];
	[bindings setValue:nil forKey:@"cheatSearchSearchValue"];
	[bindings setValue:@"Search not started." forKey:@"cheatSearchAddressCount"];
	[self setCheatSearchViewByStyle:[(NSNumber *)[bindings valueForKey:@"cheatSearchStyle"] integerValue]];
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
		case CheatType_Internal:
			newView = viewConfigureInternalCheat;
			break;
			
		case CheatType_ActionReplay:
			newView = viewConfigureActionReplayCheat;
			break;
			
		case CheatType_CodeBreaker:
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
		case CheatSearchStyle_ExactValue:
			newView = viewSearchExactValue;
			break;
			
		case CheatSearchStyle_Comparative:
			if ([cdsCheats searchDidStart] == 0)
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

- (void)windowDidBecomeKey:(NSNotification *)notification
{
	[cheatWindowController setContent:bindings];
}

- (void)tableViewSelectionDidChange:(NSNotification *)aNotification
{
	NSTableView *table = (NSTableView *)[aNotification object];
	NSInteger rowIndex = [table selectedRow];
	
	if (table == [self cheatListTable])
	{
		if (rowIndex >= 0)
		{
			NSArray *selectedObjects = [cheatListController selectedObjects];
			CocoaDSCheatItem *selectedCheat = [selectedObjects objectAtIndex:0];
			[self setWorkingCheat:[selectedCheat createWorkingCopy]];
			[cheatSelectedItemController setContent:[self workingCheat]];
			
			[self setCheatConfigViewByType:[selectedCheat cheatType]];
			[bindings setValue:[NSNumber numberWithBool:YES] forKey:@"hasSelection"];
		}
		else
		{
			if ([self workingCheat] != nil)
			{
				[[[self workingCheat] parent] destroyWorkingCopy];
			}
			
			[cheatSelectedItemController setContent:nil];
			[self setWorkingCheat:nil];
			
			NSRect frameRect = [currentView frame];
			[currentView retain];
			[cheatConfigBox replaceSubview:currentView with:viewConfigureNoSelection];
			currentView = viewConfigureNoSelection;
			[currentView setFrame:frameRect];
			
			[bindings setValue:[NSNumber numberWithBool:NO] forKey:@"hasSelection"];
		}
	}
	else if (table == [self cheatSearchListTable])
	{
		if (rowIndex >= 0)
		{
			NSArray *selectedObjects = [cheatSearchListController selectedObjects];
			NSMutableDictionary *selectedAddress = [selectedObjects objectAtIndex:0];
			NSString *addressString = [(NSString *)[selectedAddress valueForKey:@"addressString"] substringFromIndex:4];
			
			if ([self workingCheat] != nil)
			{
				[[self workingCheat] setMemAddressSixDigitString:addressString];
			}
		}
	}
}

@end
