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

#import <Cocoa/Cocoa.h>

@class CocoaDSCheatItem;
@class CocoaDSCheatManager;
@class CocoaDSCheatSearch;
@class CocoaDSCheatSearchParams;


#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_5
@interface CheatWindowDelegate : NSObject <NSWindowDelegate, NSTableViewDelegate>
#else
@interface CheatWindowDelegate : NSObject
#endif
{
	NSObject *dummyObject;
	NSWindow *window;
	NSBox *cheatConfigBox;
	NSView *cheatSearchView;
	NSTableView *cheatListTable;
	NSTableView *cheatSearchListTable;
	NSArrayController *cheatListController;
	NSArrayController *cheatSearchListController;
	NSArrayController *cheatDatabaseController;
	NSObjectController *cheatWindowController;
	NSObjectController *cheatSelectedItemController;
	
	NSView *currentView;
	NSView *viewConfigureNoSelection;
	NSView *viewConfigureInternalCheat;
	NSView *viewConfigureActionReplayCheat;
	NSView *viewConfigureCodeBreakerCheat;
	NSView *currentSearchStyleView;
	NSView *viewSearchNoSelection;
	NSView *viewSearchExactValue;
	NSView *viewSearchComparativeStart;
	NSView *viewSearchComparativeContinue;
	
	NSSearchField *searchField;
	
	NSWindow *cheatDatabaseSheet;
	
	NSUInteger untitledCount;
	
	NSMutableDictionary *bindings;
	CocoaDSCheatItem *workingCheat;
	CocoaDSCheatManager *cdsCheats;
	CocoaDSCheatSearch *cdsCheatSearch;
}

@property (assign) IBOutlet NSObject *dummyObject;
@property (readonly) IBOutlet NSWindow *window;
@property (readonly) IBOutlet NSBox *cheatConfigBox;
@property (readonly) IBOutlet NSView *cheatSearchView;
@property (readonly) IBOutlet NSTableView *cheatListTable;
@property (readonly) IBOutlet NSTableView *cheatSearchListTable;
@property (readonly) IBOutlet NSArrayController *cheatListController;
@property (readonly) IBOutlet NSArrayController *cheatSearchListController;
@property (readonly) IBOutlet NSArrayController *cheatDatabaseController;
@property (readonly) IBOutlet NSObjectController *cheatWindowController;
@property (readonly) IBOutlet NSObjectController *cheatSelectedItemController;

@property (readonly) IBOutlet NSView *viewConfigureNoSelection;
@property (readonly) IBOutlet NSView *viewConfigureInternalCheat;
@property (readonly) IBOutlet NSView *viewConfigureActionReplayCheat;
@property (readonly) IBOutlet NSView *viewConfigureCodeBreakerCheat;

@property (readonly) IBOutlet NSView *viewSearchNoSelection;
@property (readonly) IBOutlet NSView *viewSearchExactValue;
@property (readonly) IBOutlet NSView *viewSearchComparativeStart;
@property (readonly) IBOutlet NSView *viewSearchComparativeContinue;

@property (readonly) IBOutlet NSSearchField *searchField;

@property (readonly) IBOutlet NSWindow *cheatDatabaseSheet;

@property (assign) NSUInteger untitledCount;
@property (readonly) NSMutableDictionary *bindings;
@property (retain) CocoaDSCheatItem *workingCheat;
@property (retain) CocoaDSCheatManager *cdsCheats;
@property (readonly) CocoaDSCheatSearch *cdsCheatSearch;

- (IBAction) addToList:(id)sender;
- (IBAction) removeFromList:(id)sender;
- (IBAction) viewDatabase:(id)sender;
- (IBAction) setInternalCheatValue:(id)sender;
- (IBAction) applyConfiguration:(id)sender;
- (IBAction) selectCheatType:(id)sender;

- (IBAction) selectCheatSearchStyle:(id)sender;
- (IBAction) runExactValueSearch:(id)sender;
- (IBAction) runComparativeSearch:(id)sender;
- (void) searchDidFinish:(NSNotification *)aNotification;
- (IBAction) resetSearch:(id)sender;

- (void) setCheatConfigViewByType:(NSInteger)cheatTypeID;
- (void) setCheatSearchViewByStyle:(NSInteger)searchStyleID;

- (IBAction) selectAllCheatsInDatabase:(id)sender;
- (IBAction) selectNoneCheatsInDatabase:(id)sender;
- (void) addSelectedFromCheatDatabase;
- (IBAction) closeCheatDatabaseSheet:(id)sender;
- (void) didEndCheatDatabaseSheet:(NSWindow *)sheet returnCode:(NSInteger)returnCode contextInfo:(void *)contextInfo;

@end
