/*
	Copyright (C) 2023 DeSmuME team

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

#import "CheatDatabaseWindowController.h"
#import "cheatWindowDelegate.h"
#import "../cocoa_globals.h"
#import "../cocoa_cheat.h"
#import "../cocoa_util.h"

NSMutableArray *cheatDatabaseWindowList = nil;

@implementation CheatDatabaseWindowController

@synthesize dummyObject;
@synthesize errorSheet;
@synthesize gameListController;
@synthesize entryListController;
@synthesize splitView;
@synthesize gameTable;
@synthesize entryOutline;

@synthesize cheatManagerDelegate;
@synthesize codeViewerFont;
@dynamic database;
@dynamic filePath;
@dynamic databaseFormatString;
@dynamic gameCount;
@dynamic isEncryptedString;
@synthesize isFileLoading;
@synthesize isCurrentGameFound;
@synthesize isSelectedGameTheCurrentGame;

@synthesize errorMajorString;
@synthesize errorMinorString;

- (id) initWithWindowNibName:(NSString *)windowNibName delegate:(CheatWindowDelegate *)theDelegate
{
	self = [super initWithWindowNibName:windowNibName];
	if (self == nil)
	{
		return self;
	}
	
	cheatManagerDelegate = [theDelegate retain];
	codeViewerFont = [NSFont fontWithName:@"Monaco" size:13.0];
	
	dummyObject = nil;
	defaultWindowTitle = [[NSString alloc] initWithString:@"Cheat Database Viewer"];
	database = nil;
	isFileLoading = NO;
	isCurrentGameFound = NO;
	isSelectedGameTheCurrentGame = NO;
	currentGameIndexString = [[NSString alloc] initWithString:@"NSNotFound"];
	currentGameTableRowIndex = NSNotFound;
	errorMajorString = @"No error has occurred!";
	errorMinorString = @"This is just a placeholder message for initialization purposes.";
	
	if (cheatDatabaseWindowList == nil)
	{
		cheatDatabaseWindowList = [[NSMutableArray alloc] initWithObjects:self, nil];
	}
	else
	{
		[cheatDatabaseWindowList addObject:self];
	}
	
	return self;
}

- (void)dealloc
{
	[self setCheatManagerDelegate:nil];
	[self setDatabase:nil];
	[currentGameIndexString release];
	[defaultWindowTitle release];
	
	[super dealloc];
}

- (void) loadFileStart:(NSURL *)theURL
{
	if (theURL == nil)
	{
		return;
	}
	
	// First check if another cheat database window has already opened the file at this URL.
	CheatDatabaseWindowController *foundWindowController = nil;
	for (CheatDatabaseWindowController *windowController in cheatDatabaseWindowList)
	{
		NSURL *databaseURL = [[windowController database] lastFileURL];
		NSString *foundDatabaseFilePath = [databaseURL path];
		
		if ( (foundDatabaseFilePath != nil) && ([foundDatabaseFilePath isEqualToString:[theURL path]]) )
		{
			foundWindowController = windowController;
			break;
		}
	}
	
	if (foundWindowController != nil)
	{
		// If the file is already open, then simply assign that database file to this window.
		[self setDatabase:[foundWindowController database]];
		[self updateWindow];
	}
	else
	{
		// If the file is not open, then we need to open it now. Let's do this on a separate
		// thread so that we don't lock up the main thread.
		[self setIsFileLoading:YES];
		[self setDatabase:nil];
		
		NSString *threadNamePrefix = @"org.desmume.DeSmuME.loadDatabaseDidFinish_";
		NSString *fullThreadName = [threadNamePrefix stringByAppendingString:[theURL absoluteString]];
		
		[[NSNotificationCenter defaultCenter] addObserver:self
												 selector:@selector(loadFileDidFinish:)
													 name:fullThreadName
												   object:nil];
		
		[theURL retain];
		[NSThread detachNewThreadSelector:@selector(loadFileOnThread:) toTarget:self withObject:theURL];
	}
}

- (void) loadFileOnThread:(id)object
{
	NSAutoreleasePool *threadPool = [[NSAutoreleasePool alloc] init];
	
	NSURL *workingURL = (NSURL *)object;
	NSString *threadNamePrefix = @"org.desmume.DeSmuME.loadDatabaseDidFinish_";
	NSString *fullThreadName = [threadNamePrefix stringByAppendingString:[workingURL absoluteString]];
	
	CheatSystemError error = CheatSystemError_NoError;
	CocoaDSCheatDatabase *newDatabase = [[CocoaDSCheatDatabase alloc] initWithFileURL:workingURL error:&error];
	
	NSDictionary *userInfo = [[NSDictionary alloc] initWithObjectsAndKeys:
							  workingURL, @"URL",
							  [NSNumber numberWithInteger:(NSInteger)error], @"ErrorCode",
							  nil];
	[[NSNotificationCenter defaultCenter] postNotificationOnMainThreadName:fullThreadName object:newDatabase userInfo:userInfo];
	
	[threadPool release];
}

- (void) loadFileDidFinish:(NSNotification *)aNotification
{
	CocoaDSCheatDatabase *newDatabase = [aNotification object];
	NSDictionary *userInfo = [aNotification userInfo];
	NSURL *workingURL = (NSURL *)[userInfo valueForKey:@"URL"];
	CheatSystemError errorCode = (CheatSystemError)[(NSNumber *)[userInfo valueForKey:@"ErrorCode"] integerValue];
	
	NSString *threadNamePrefix = @"org.desmume.DeSmuME.loadDatabaseDidFinish_";
	NSString *fullThreadName = [threadNamePrefix stringByAppendingString:[workingURL absoluteString]];
	[[NSNotificationCenter defaultCenter] removeObserver:self name:fullThreadName object:nil];
	
	[self setIsFileLoading:NO];
	[self setDatabase:[newDatabase autorelease]];
	[self updateWindow];
	
	if (errorCode != CheatSystemError_NoError)
	{
		[self showErrorSheet:errorCode];
	}
	
	if (database == nil)
	{
		return;
	}
	
	// Begin the generation of the cheat database recents menu.
	NSString *legacyFilePath = [[NSUserDefaults standardUserDefaults] stringForKey:@"R4Cheat_DatabasePath"];
	BOOL useLegacyFilePath = ( (legacyFilePath != nil) && ([legacyFilePath length] > 0) );
	
	NSArray *dbRecentsList = [[NSUserDefaults standardUserDefaults] arrayForKey:@"CheatDatabase_RecentFilePath"];
	NSMutableArray *newRecentsList = [NSMutableArray arrayWithCapacity:[dbRecentsList count] + 1];
	
	if (useLegacyFilePath)
	{
		// We need to check if the legacy file path also exists in the recents list.
		// If it does, then the recents list version takes priority.
		for (NSDictionary *dbRecentItem in dbRecentsList)
		{
			NSString *dbRecentItemFilePath = (NSString *)[dbRecentItem valueForKey:@"FilePath"];
			if ([dbRecentItemFilePath isEqualToString:legacyFilePath])
			{
				useLegacyFilePath = NO;
				break;
			}
		}
	}
	
	if (useLegacyFilePath)
	{
		// The legacy file path must always be the first entry of the recents list.
		NSDictionary *legacyRecentItem = [NSDictionary dictionaryWithObjectsAndKeys:legacyFilePath, @"FilePath",
										  [legacyFilePath lastPathComponent], @"FileName",
										  nil];
		[newRecentsList addObject:legacyRecentItem];
	}
	
	// Next, we need to add back all of the recent items in the same order in which
	// they appear in user defaults, with the exception of our newest item.
	NSString *newFilePath = [[database lastFileURL] path];
	for (NSDictionary *dbRecentItem in dbRecentsList)
	{
		NSString *dbRecentItemFilePath = (NSString *)[dbRecentItem valueForKey:@"FilePath"];
		if ( ![newFilePath isEqualToString:dbRecentItemFilePath] )
		{
			[newRecentsList addObject:dbRecentItem];
		}
	}
	
	// Create our new recent item...
	NSDictionary *newRecentItem = [NSDictionary dictionaryWithObjectsAndKeys:newFilePath, @"FilePath",
								   [newFilePath lastPathComponent], @"FileName",
								   [NSDate date], @"AddedDate",
								   [[self window] stringWithSavedFrame], @"WindowFrame",
								   [NSNumber numberWithFloat:[[[splitView subviews] objectAtIndex:0] frame].size.height], @"WindowSplitViewDividerPosition",
								   nil];
	
	// ...and then add the newest recent item, ensuring that it is always last in the list.
	[newRecentsList addObject:newRecentItem];
	
	// We're done generating the new recent items list, so write it back to user defaults, and then
	// send a notification that UI elements needs to be updated.
	[[NSUserDefaults standardUserDefaults] setObject:newRecentsList forKey:@"CheatDatabase_RecentFilePath"];
	[[NSNotificationCenter defaultCenter] postNotificationOnMainThreadName:@"org.desmume.DeSmuME.updateCheatDatabaseRecentsMenu" object:[newRecentsList retain] userInfo:nil];
}

- (void) updateWindow
{
	if ([self database] == nil)
	{
		[[self window] setTitle:defaultWindowTitle];
	}
	else
	{
		[[self window] setTitle:[database description]];
	}
	
	[[self window] setRepresentedURL:[database lastFileURL]];
	[gameListController setContent:[database gameList]];
	
	NSIndexSet *selectedRows = [gameTable selectedRowIndexes];
	[gameTable deselectAll:nil];
	[gameTable selectRowIndexes:selectedRows byExtendingSelection:NO];
	
	[self validateGameTableFonts];
	[self selectCurrentGame:nil];
}

- (void) validateGameTableFonts
{
	CheatWindowDelegate *delegate = [self cheatManagerDelegate];
	CocoaDSCheatManager *cheatManager = [delegate cdsCheats];
	
	if ( (delegate == nil) || (cheatManager == nil) )
	{
		currentGameTableRowIndex = NSNotFound;
		[currentGameIndexString release];
		currentGameIndexString = [[NSString alloc] initWithString:@"NSNotFound"];
		[self setIsCurrentGameFound:NO];
		return;
	}
	
	NSString *currentGameCode = [cheatManager currentGameCode];
	const NSUInteger currentGameCRC = [cheatManager currentGameCRC];
	
	for (CocoaDSCheatDBGame *game in [gameListController content])
	{
		if ( ([game crc] == currentGameCRC) && ([[game serial] isEqualToString:currentGameCode]) )
		{
			[currentGameIndexString release];
			currentGameIndexString = [[NSString alloc] initWithFormat:@"%llu", (unsigned long long)[game index]];
			[self setIsCurrentGameFound:YES];
			break;
		}
	}
}

+ (void) validateGameTableFontsForAllWindows
{
	if (cheatDatabaseWindowList == nil)
	{
		return;
	}
	
	for (CheatDatabaseWindowController *windowController in cheatDatabaseWindowList)
	{
		[windowController validateGameTableFonts];
		[[windowController gameTable] setNeedsDisplay];
	}
}

- (BOOL) validateWillAddColumn
{
	BOOL showWillAddColumn = NO;
	
	CheatWindowDelegate *delegate = [self cheatManagerDelegate];
	CocoaDSCheatManager *cheatManager = [delegate cdsCheats];
	
	NSArray *selectedObjects = [gameListController selectedObjects];
	if ( (selectedObjects == nil) || ([selectedObjects count] == 0) )
	{
		return showWillAddColumn;
	}
	
	CocoaDSCheatDBGame *selectedGame = [selectedObjects objectAtIndex:0];
	
	if ( (delegate != nil) && (cheatManager != nil) && ([selectedGame serial] != nil) )
	{
		showWillAddColumn = ([[selectedGame serial] isEqualToString:[cheatManager currentGameCode]]) && ([selectedGame crc] == [cheatManager currentGameCRC]);
	}
	
	NSTableColumn *willAddColumn = [entryOutline tableColumnWithIdentifier:@"willAdd"];
	[willAddColumn setHidden:!showWillAddColumn];
	
	[self setIsSelectedGameTheCurrentGame:showWillAddColumn];
	
	return showWillAddColumn;
}

+ (void) validateWillAddColumnForAllWindows
{
	if (cheatDatabaseWindowList == nil)
	{
		return;
	}
	
	for (CheatDatabaseWindowController *windowController in cheatDatabaseWindowList)
	{
		[windowController validateWillAddColumn];
	}
}

- (void) showErrorSheet:(NSInteger)errorCode
{
	switch (errorCode)
	{
		case CheatSystemError_NoError:
			[self setErrorMajorString:@"No error has occurred."];
			[self setErrorMinorString:@"This message is a placeholder. You are seeing this error as a test for this app's error handling.\n\nError Code: %i"];
			break;
			
		case CheatSystemError_FileOpenFailed:
			[self setErrorMajorString:@"Failed to open file."];
			[self setErrorMinorString:[NSString stringWithFormat:@"The system could not open the cheat database file. This problem is usually because another app is using it, or because the file permissions disallow read access.\n\nError Code: %i", (int)errorCode]];
			break;
			
		case CheatSystemError_FileFormatInvalid:
			[self setErrorMajorString:@"Invalid file format."];
			[self setErrorMinorString:[NSString stringWithFormat:@"DeSmuME could not recognize the file format of the cheat database file. Currently, DeSmuME only recognizes the R4 file format. It is also possible that the file data is corrupted.\n\nError Code: %i", (int)errorCode]];
			break;
			
		case CheatSystemError_GameNotFound:
		{
			CheatWindowDelegate *delegate = [self cheatManagerDelegate];
			CocoaDSCheatManager *cheatManager = [delegate cdsCheats];
			[self setErrorMajorString:@"Current game not found in database."];
			[self setErrorMinorString:[NSString stringWithFormat:@"The current game (Serial='%@', CRC=%llu) could not be found in the cheat database.\n\nError Code: %i", [cheatManager currentGameCode], (unsigned long long)[cheatManager currentGameCRC], (int)errorCode]];
			break;
		}
			
		case CheatSystemError_LoadEntryError:
			[self setErrorMajorString:@"Could not read cheat entries."];
			[self setErrorMinorString:[NSString stringWithFormat:@"The entry data for the selected game could not be read. This is usually due to file data corruption.\n\nError Code: %i", (int)errorCode]];
			break;
			
		case CheatSystemError_FileDoesNotExist:
			[self setErrorMajorString:@"The file does not exist."];
			[self setErrorMinorString:[NSString stringWithFormat:@"If this file was selected from the Recents Menu, then it has been removed.\n\nError Code: %i", (int)errorCode]];
			break;
			
		default:
			[self setErrorMajorString:@"An unknown error has occurred."];
			[self setErrorMinorString:[NSString stringWithFormat:@"Error Code: %i", (int)errorCode]];
			break;
	}
	
#if defined(MAC_OS_X_VERSION_10_9) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_9)
	if ([[self window] respondsToSelector:@selector(beginSheet:completionHandler:)])
	{
		[[self window] beginSheet:errorSheet
		 completionHandler:^(NSModalResponse response) {
			[self didEndErrorSheet:nil returnCode:response contextInfo:nil];
		} ];
	}
	else
#endif
	{
		SILENCE_DEPRECATION_MACOS_10_10( [NSApp beginSheet:errorSheet
											modalForWindow:[self window]
											 modalDelegate:self
											didEndSelector:@selector(didEndErrorSheet:returnCode:contextInfo:)
											   contextInfo:nil] );
	}
}

- (void) didEndErrorSheet:(NSWindow *)sheet returnCode:(NSInteger)returnCode contextInfo:(void *)contextInfo
{
	[sheet orderOut:self];
}

#pragma mark -
#pragma mark Dynamic Properties

- (void) setDatabase:(CocoaDSCheatDatabase *)theDatabase
{
	[self willChangeValueForKey:@"filePath"];
	[self willChangeValueForKey:@"databaseFormatString"];
	[self willChangeValueForKey:@"gameCount"];
	[self willChangeValueForKey:@"isEncryptedString"];
	
	[theDatabase retain];
	[database release];
	database = theDatabase;
	
	[self didChangeValueForKey:@"filePath"];
	[self didChangeValueForKey:@"databaseFormatString"];
	[self didChangeValueForKey:@"gameCount"];
	[self didChangeValueForKey:@"isEncryptedString"];
}

- (CocoaDSCheatDatabase *) database
{
	return database;
}

- (NSString *) filePath
{
	if ( (database != nil) && ([database lastFileURL] != nil) )
	{
		return [[database lastFileURL] path];
	}
	else if ([self isFileLoading])
	{
		return @"Loading database file...";
	}
	
	return @"No database file loaded.";
}

- (NSString *) databaseFormatString
{
	if (database != nil)
	{
		return [database formatString];
	}
	
	return @"---";
}

- (NSInteger) gameCount
{
	if (database != nil)
	{
		return [[database gameList] count];
	}
	
	return 0;
}

- (NSString *) isEncryptedString
{
	if (database != nil)
	{
		return [database isEncrypted] ? @"Yes" : @"No";
	}
	
	return @"---";
}

#pragma mark -
#pragma mark IBActions

- (IBAction) openFile:(id)sender
{
	NSOpenPanel *panel = [NSOpenPanel openPanel];
	[panel setCanChooseDirectories:NO];
	[panel setCanChooseFiles:YES];
	[panel setResolvesAliases:YES];
	[panel setAllowsMultipleSelection:NO];
	[panel setTitle:NSSTRING_TITLE_OPEN_CHEAT_DB_PANEL];
	NSArray *fileTypes = [NSArray arrayWithObjects:@FILE_EXT_R4_CHEAT_DB, nil];
	
	// The NSOpenPanel/NSSavePanel method -(void)beginSheetForDirectory:file:types:modalForWindow:modalDelegate:didEndSelector:contextInfo
	// is deprecated in Mac OS X v10.6.
#if MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_5
	if (IsOSXVersionSupported(10, 6, 0))
	{
		[panel setAllowedFileTypes:fileTypes];
		[panel beginSheetModalForWindow:[self window]
					  completionHandler:^(NSInteger result) {
						  [self chooseCheatDatabaseDidEnd:panel returnCode:(int)result contextInfo:nil];
					  } ];
	}
	else
#endif
	{
		SILENCE_DEPRECATION_MACOS_10_6( [panel beginSheetForDirectory:nil
																 file:nil
																types:fileTypes
													   modalForWindow:[self window]
														modalDelegate:self
													   didEndSelector:@selector(chooseCheatDatabaseDidEnd:returnCode:contextInfo:)
														  contextInfo:nil] );
	}
}

- (void) chooseCheatDatabaseDidEnd:(NSOpenPanel *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
	[sheet orderOut:self];
	
	if (returnCode == GUI_RESPONSE_CANCEL)
	{
		return;
	}
	
	NSURL *selectedFileURL = [[sheet URLs] lastObject]; //hopefully also the first object
	[self loadFileStart:selectedFileURL];
}

- (IBAction) selectAll:(id)sender
{
	NSMutableArray *entryTree = [entryListController content];
	if (entryTree == nil)
	{
		return;
	}
	
	for (CocoaDSCheatDBEntry *entry in entryTree)
	{
		[entry setWillAdd:YES];
	}
	
	[entryOutline setNeedsDisplay];
}

- (IBAction) selectNone:(id)sender
{
	NSMutableArray *entryTree = [entryListController content];
	if (entryTree == nil)
	{
		return;
	}
	
	for (CocoaDSCheatDBEntry *entry in entryTree)
	{
		[entry setWillAdd:NO];
	}
	
	[entryOutline setNeedsDisplay];
}

- (IBAction) addSelected:(id)sender
{
	CheatWindowDelegate *delegate = [self cheatManagerDelegate];
	if (delegate == nil)
	{
		return;
	}
	
	CocoaDSCheatManager *cheatManager = [delegate cdsCheats];
	if (cheatManager == nil)
	{
		return;
	}
	
	NSString *currentGameCode = [cheatManager currentGameCode];
	NSUInteger currentGameCRC = [cheatManager currentGameCRC];
	if ( (currentGameCode == nil) || (currentGameCRC == 0) )
	{
		return;
	}
	
	NSMutableArray *entryTree = [entryListController content];
	if (entryTree == nil)
	{
		return;
	}
	
	NSInteger selectedIndex = [gameTable selectedRow];
	CocoaDSCheatDBGame *selectedGame = (CocoaDSCheatDBGame *)[[gameListController arrangedObjects] objectAtIndex:selectedIndex];
	
	if ( (![[selectedGame serial] isEqualToString:currentGameCode]) || ([selectedGame crc] != currentGameCRC) )
	{
		return;
	}
	
	CocoaDSCheatDBEntry *rootEntry = [selectedGame entryRoot];
	if (rootEntry == nil)
	{
		return;
	}
	
	const NSInteger addedItemCount = [cheatManager databaseAddSelectedInEntry:[selectedGame entryRoot]];
	if (addedItemCount > 0)
	{
		[[delegate cheatListController] setContent:[cheatManager sessionList]];
		[cheatManager save];
	}
}

- (IBAction) selectCurrentGame:(id)sender
{
	CheatWindowDelegate *delegate = [self cheatManagerDelegate];
	if (delegate == nil)
	{
		return;
	}
	
	CocoaDSCheatManager *cheatManager = [delegate cdsCheats];
	if (cheatManager == nil)
	{
		return;
	}
	
	NSString *currentGameCode = [cheatManager currentGameCode];
	NSUInteger currentGameCRC = [cheatManager currentGameCRC];
	if ( (currentGameCode == nil) || (currentGameCRC == 0) )
	{
		return;
	}
	
	NSUInteger selectionIndex = NSNotFound;
	
	NSArray *arrangedObjects = (NSArray *)[gameListController arrangedObjects];
	for (CocoaDSCheatDBGame *game in arrangedObjects)
	{
		if ( ([game crc] == currentGameCRC) && ([[game serial] isEqualToString:currentGameCode]) )
		{
			selectionIndex = [arrangedObjects indexOfObject:game];
			NSIndexSet *indexSet = [NSIndexSet indexSetWithIndex:selectionIndex];
			[gameTable selectRowIndexes:indexSet byExtendingSelection:NO];
			[gameTable scrollRowToVisible:selectionIndex];
			break;
		}
	}
}

- (IBAction) closeErrorSheet:(id)sender
{
	NSWindow *sheet = [(NSControl *)sender window];
	const NSInteger code = [(NSControl *)sender tag];
	
	[CocoaDSUtil endSheet:sheet returnCode:code];
}

#pragma mark -
#pragma mark NSWindowDelegate Protocol

- (void)windowDidLoad
{
	// Save a copy of the default window title before we replace it
	// with the database file's description.
	NSString *oldDefaultWindowTitle = defaultWindowTitle;
	defaultWindowTitle = [[[self window] title] copy];
	[oldDefaultWindowTitle release];
}

- (void)windowWillClose:(NSNotification *)notification
{
	NSArray *userDefaultsRecentsList = [[NSUserDefaults standardUserDefaults] arrayForKey:@"CheatDatabase_RecentFilePath"];
	if ( (userDefaultsRecentsList != nil) && ([userDefaultsRecentsList count] > 0) )
	{
		NSMutableArray *dbRecentsList = [NSMutableArray arrayWithCapacity:[userDefaultsRecentsList count]];
		
		for (NSDictionary *recentItem in userDefaultsRecentsList)
		{
			NSString *thisFilePath = [[database lastFileURL] path];
			NSString *recentItemPath = (NSString *)[recentItem objectForKey:@"FilePath"];
			
			if ( (thisFilePath != nil) && ([recentItemPath isEqualToString:thisFilePath]) )
			{
				NSMutableDictionary *newRecentItem = [NSMutableDictionary dictionaryWithDictionary:recentItem];
				[newRecentItem setObject:[[self window] stringWithSavedFrame] forKey:@"WindowFrame"];
				[newRecentItem setObject:[NSNumber numberWithFloat:[[[splitView subviews] objectAtIndex:0] frame].size.height] forKey:@"WindowSplitViewDividerPosition"];
				
				[dbRecentsList addObject:newRecentItem];
			}
			else
			{
				[dbRecentsList addObject:recentItem];
			}
		}
		
		[[NSUserDefaults standardUserDefaults] setObject:dbRecentsList forKey:@"CheatDatabase_RecentFilePath"];
	}
	
	[cheatDatabaseWindowList removeObject:self];
	[self release];
}

#pragma mark -
#pragma mark NSTableViewDelegate Protocol

- (void)tableViewSelectionDidChange:(NSNotification *)aNotification
{
	NSTableView *table = (NSTableView *)[aNotification object];
	NSInteger rowIndex = [table selectedRow];
	
	if (table == gameTable)
	{
		if (rowIndex >= 0)
		{
			NSArray *selectedObjects = [gameListController selectedObjects];
			CocoaDSCheatDBGame *selectedGame = [selectedObjects objectAtIndex:0];
			CocoaDSCheatDBEntry *entryRoot = [database loadGameEntry:selectedGame];
			[self validateWillAddColumn];
			[entryListController setContent:[entryRoot child]];
			
			if (entryRoot == nil)
			{
				[self showErrorSheet:CheatSystemError_LoadEntryError];
			}
		}
		else
		{
			[entryListController setContent:nil];
		}
	}
}

- (void)tableView:(NSTableView *)tableView willDisplayCell:(id)cell forTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row
{
	NSString *cellString = [cell stringValue];
	if ( (cellString != nil) && [cellString isEqualToString:currentGameIndexString] )
	{
		currentGameTableRowIndex = row;
	}
	
	if ( (cellString != nil) && (row == currentGameTableRowIndex) )
	{
		[cell setFont:[NSFont boldSystemFontOfSize:[NSFont smallSystemFontSize]]];
	}
	else
	{
		[cell setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
		currentGameTableRowIndex = NSNotFound;
	}
}

@end
