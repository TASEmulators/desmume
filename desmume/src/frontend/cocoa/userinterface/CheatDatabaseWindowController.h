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

#import <Cocoa/Cocoa.h>

@class CocoaDSCheatDatabase;
@class CheatWindowDelegate;
@class CocoaDSCheatDBGame;

#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_5
@interface CheatDatabaseWindowController : NSWindowController <NSWindowDelegate, NSTableViewDelegate>
#else
@interface CheatDatabaseWindowController : NSWindowController
#endif
{
	NSObject *dummyObject;
	NSWindow *errorSheet;
	NSString *defaultWindowTitle;
	NSArrayController *gameListController;
	NSTreeController *entryListController;
	NSSplitView *splitView;
	NSTableView *gameTable;
	NSOutlineView *entryOutline;
	NSFont *codeViewerFont;
	
	CheatWindowDelegate *cheatManagerDelegate;
	CocoaDSCheatDatabase *database;
	
	NSString *filePath;
	BOOL isFileLoading;
	BOOL isCurrentGameFound;
	BOOL isSelectedGameTheCurrentGame;
	NSInteger currentGameTableRowIndex;
	NSString *currentGameIndexString;
	NSString *currentGameSerial;
	NSUInteger currentGameCRC;
	BOOL isCompatibilityCheckIgnored;
	BOOL isOptionWarningSilenced;
	
	NSString *errorMajorString;
	NSString *errorMinorString;
	
	NSDictionary *initProperties;
}

@property (readonly) IBOutlet NSObject *dummyObject;
@property (readonly) IBOutlet NSWindow *errorSheet;
@property (readonly) IBOutlet NSArrayController *gameListController;
@property (readonly) IBOutlet NSTreeController *entryListController;
@property (readonly) IBOutlet NSSplitView *splitView;
@property (readonly) IBOutlet NSTableView *gameTable;
@property (readonly) IBOutlet NSOutlineView *entryOutline;

@property (retain) CheatWindowDelegate *cheatManagerDelegate;
@property (assign) NSFont *codeViewerFont;
@property (retain, nonatomic) CocoaDSCheatDatabase *database;
@property (readonly) NSString *filePath;
@property (readonly) NSString *databaseFormatString;
@property (readonly) NSInteger gameCount;
@property (readonly) NSString *isEncryptedString;
@property (assign) BOOL isFileLoading;
@property (assign) BOOL isCurrentGameFound;
@property (assign) BOOL isSelectedGameTheCurrentGame;
@property (retain, nonatomic) NSString *currentGameSerial;
@property (assign, nonatomic) NSUInteger currentGameCRC;
@property (readonly, nonatomic) NSString *currentGameCRCString;
@property (assign, nonatomic) BOOL isCompatibilityCheckIgnored;
@property (assign) BOOL isOptionWarningSilenced;

@property (assign) NSString *errorMajorString;
@property (assign) NSString *errorMinorString;

- (id) initWithWindowNibName:(NSString *)windowNibName delegate:(CheatWindowDelegate *)theDelegate;
- (void) loadFileStart:(NSURL *)theURL;
- (void) loadFileOnThread:(id)object;
- (void) loadFileDidFinish:(NSNotification *)aNotification;
- (void) updateWindow;
+ (void) setCurrentGameForAllWindowsSerial:(NSString *)serialString crc:(NSUInteger)crc;
- (void) validateGameTableFonts;
- (BOOL) validateWillAddColumn;
- (void) showErrorSheet:(NSInteger)errorCode;
- (void) didEndErrorSheet:(NSWindow *)sheet returnCode:(NSInteger)returnCode contextInfo:(void *)contextInfo;

- (IBAction) openFile:(id)sender;
- (IBAction) selectAll:(id)sender;
- (IBAction) selectNone:(id)sender;
- (IBAction) addSelected:(id)sender;
- (IBAction) selectCurrentGame:(id)sender;
- (IBAction) closeErrorSheet:(id)sender;

@end
