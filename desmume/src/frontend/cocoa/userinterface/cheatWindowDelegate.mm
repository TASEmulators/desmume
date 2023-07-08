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

@synthesize codeEditorFont;
@synthesize bindings;
@synthesize cdsCheats;
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
	
	workingCheat = nil;
	currentView = nil;
	currentSearchStyleView = nil;
	codeEditorFont = [NSFont fontWithName:@"Monaco" size:13.0];
		
	[bindings setValue:[NSNumber numberWithBool:NO] forKey:@"hasSelection"];
	[bindings setValue:[NSNumber numberWithBool:NO] forKey:@"hasItems"];
	[bindings setValue:[NSNumber numberWithBool:NO] forKey:@"isRunningSearch"];
	[bindings setValue:[NSNumber numberWithBool:NO] forKey:@"isSearchStarted"];
	[bindings setValue:[NSNumber numberWithInteger:CHEATSEARCH_SEARCHSTYLE_EXACT_VALUE] forKey:@"cheatSearchStyle"];
	[bindings setValue:[NSNumber numberWithInteger:CHEATSEARCH_UNSIGNED] forKey:@"cheatSearchSignType"];
	[bindings setValue:@"Search not started." forKey:@"cheatSearchAddressCount"];
	
	if ([CocoaDSCheatItem iconInternalCheat] == nil || [CocoaDSCheatItem iconActionReplay] == nil || [CocoaDSCheatItem iconCodeBreaker] == nil)
	{
		[CocoaDSCheatItem setIconInternalCheat:[NSImage imageNamed:@"NSApplicationIcon"]];
		[CocoaDSCheatItem setIconActionReplay:[NSImage imageNamed:@"Icon_ActionReplay_128x128"]];
		[CocoaDSCheatItem setIconCodeBreaker:[NSImage imageNamed:@"Icon_CodeBreaker_128x128"]];
	}
	
	return self;
}

- (void)dealloc
{
	[self setWorkingCheat:nil];
	[self setCdsCheats:nil];
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
	[cheatManager loadFromMaster];
	[cheatListController setContent:[cheatManager sessionList]];
	
	NSMutableDictionary *cheatWindowBindings = (NSMutableDictionary *)[cheatWindowController content];
	[cheatWindowBindings setValue:self forKey:@"cheatWindowDelegateKey"];
	
	NSString *dbFilePath = [[NSUserDefaults standardUserDefaults] stringForKey:@"R4Cheat_DatabasePath"];
	if (dbFilePath != nil)
	{
		[self databaseLoadFromFile:[NSURL fileURLWithPath:dbFilePath]];
	}
	
	[self setCheatSearchViewByStyle:CHEATSEARCH_SEARCHSTYLE_EXACT_VALUE];
	
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
	
	[cheatDatabaseController setContent:nil];
	[cheatListController setContent:nil];
	[self resetSearch:nil];
	[self setCdsCheats:nil];
	
	NSMutableDictionary *cheatWindowBindings = (NSMutableDictionary *)[cheatWindowController content];
	[cheatWindowBindings setValue:@"No ROM loaded." forKey:@"cheatDBTitle"];
	[cheatWindowBindings setValue:@"No ROM loaded." forKey:@"cheatDBDate"];
	[cheatWindowBindings setValue:@"---" forKey:@"cheatDBItemCount"];
	[cheatWindowBindings setValue:nil forKey:@"cheatWindowDelegateKey"];
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
		[bindings setValue:[NSNumber numberWithBool:YES] forKey:@"hasItems"];
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
	else
	{
		[bindings setValue:[NSNumber numberWithBool:NO] forKey:@"hasItems"];
	}
}

- (IBAction) viewDatabase:(id)sender
{
#if defined(MAC_OS_X_VERSION_10_9) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_9)
	if ([window respondsToSelector:@selector(beginSheet:completionHandler:)])
	{
		[window beginSheet:cheatDatabaseSheet
		 completionHandler:^(NSModalResponse response) {
			[self didEndCheatDatabaseSheet:nil returnCode:response contextInfo:nil];
		} ];
	}
	else
#endif
	{
		SILENCE_DEPRECATION_MACOS_10_10( [NSApp beginSheet:cheatDatabaseSheet
											modalForWindow:window
											 modalDelegate:self
											didEndSelector:@selector(didEndCheatDatabaseSheet:returnCode:contextInfo:)
											   contextInfo:nil] );
	}
}

- (IBAction) setInternalCheatValue:(id)sender
{
	// Force end of editing of any text fields.
	[window makeFirstResponder:nil];
	
	[[self cdsCheats] applyInternalCheat:[self workingCheat]];
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
	if (searchStyle == CHEATSEARCH_SEARCHSTYLE_COMPARATIVE)
	{
		[self setCheatSearchViewByStyle:CHEATSEARCH_SEARCHSTYLE_COMPARATIVE];
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

- (void) databaseLoadFromFile:(NSURL *)fileURL
{
	CocoaDSCheatManager *cheatManager = [self cdsCheats];
	NSMutableDictionary *cheatWindowBindings = (NSMutableDictionary *)[cheatWindowController content];
	
	if ( (fileURL == nil) || (cheatManager == nil) || (cheatWindowBindings == nil) )
	{
		return;
	}
	
	NSInteger error = 0;
	NSMutableArray *dbList = [cheatManager cheatListFromDatabase:fileURL errorCode:&error];
	if (dbList != nil)
	{
		[cheatDatabaseController setContent:dbList];
		
		NSString *titleString = [cheatManager databaseTitle];
		NSString *dateString = [cheatManager databaseDate];
		
		[cheatWindowBindings setValue:titleString forKey:@"cheatDBTitle"];
		[cheatWindowBindings setValue:dateString forKey:@"cheatDBDate"];
		[cheatWindowBindings setValue:[NSString stringWithFormat:@"%ld", (unsigned long)[dbList count]] forKey:@"cheatDBItemCount"];
	}
	else
	{
		[cheatWindowBindings setValue:@"---" forKey:@"cheatDBItemCount"];
		
		switch (error)
		{
			case CHEATEXPORT_ERROR_FILE_NOT_FOUND:
				NSLog(@"R4 Cheat Database read failed! Could not load the database file!");
				[cheatWindowBindings setValue:@"Database not loaded." forKey:@"cheatDBTitle"];
				[cheatWindowBindings setValue:@"CANNOT LOAD FILE" forKey:@"cheatDBDate"];
				break;
				
			case CHEATEXPORT_ERROR_WRONG_FILE_FORMAT:
				NSLog(@"R4 Cheat Database read failed! Wrong file format!");
				[cheatWindowBindings setValue:@"Database load error." forKey:@"cheatDBTitle"];
				[cheatWindowBindings setValue:@"FAILED TO LOAD FILE" forKey:@"cheatDBDate"];
				break;
				
			case CHEATEXPORT_ERROR_SERIAL_NOT_FOUND:
				NSLog(@"R4 Cheat Database read failed! Could not find the serial number for this game in the database!");
				[cheatWindowBindings setValue:@"ROM not found in database." forKey:@"cheatDBTitle"];
				[cheatWindowBindings setValue:@"ROM not found." forKey:@"cheatDBDate"];
				break;
				
			case CHEATEXPORT_ERROR_EXPORT_FAILED:
				NSLog(@"R4 Cheat Database read failed! Could not read the database file!");
				[cheatWindowBindings setValue:@"Database read error." forKey:@"cheatDBTitle"];
				[cheatWindowBindings setValue:@"CANNOT READ FILE" forKey:@"cheatDBDate"];
				break;
				
			default:
				break;
		}
	}
}

- (void) addSelectedFromCheatDatabase
{
	CocoaDSCheatManager *cheatManager = [self cdsCheats];
	if (cheatManager == nil)
	{
		return;
	}
	
	const NSInteger addedItemCount = [cheatManager databaseAddSelected];
	if (addedItemCount > 0)
	{
		[cheatListController setContent:[[self cdsCheats] sessionList]];
		[[self cdsCheats] save];
		[bindings setValue:[NSNumber numberWithBool:YES] forKey:@"hasItems"];
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
		[cheatItem setWillAdd:YES];
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
		[cheatItem setWillAdd:NO];
	}
}

- (IBAction) closeCheatDatabaseSheet:(id)sender
{
	NSWindow *sheet = [(NSControl *)sender window];
	const NSInteger code = [(NSControl *)sender tag];
	
	[CocoaDSUtil endSheet:sheet returnCode:code];
}

- (void) didEndCheatDatabaseSheet:(NSWindow *)sheet returnCode:(NSInteger)returnCode contextInfo:(void *)contextInfo
{
	[sheet orderOut:self];
	
	switch (returnCode)
	{
		case GUI_RESPONSE_CANCEL:
			return;
			
		case GUI_RESPONSE_OK:
			[self addSelectedFromCheatDatabase];
			break;
			
		default:
			break;
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
