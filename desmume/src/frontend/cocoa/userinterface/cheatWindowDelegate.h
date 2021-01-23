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


@interface CheatWindowDelegate : NSObject <NSWindowDelegate, NSTableViewDelegate>
{
	NSView *__weak currentView;
	NSView *__weak currentSearchStyleView;
	
	NSUInteger untitledCount;
	
	NSMutableDictionary *bindings;
	CocoaDSCheatItem *workingCheat;
	CocoaDSCheatManager *cdsCheats;
	CocoaDSCheatSearch *cdsCheatSearch;
}

@property (weak) IBOutlet NSObject *dummyObject;
@property (weak) IBOutlet NSWindow *window;
@property (weak) IBOutlet NSBox *cheatConfigBox;
@property (weak) IBOutlet NSView *cheatSearchView;
@property (weak) IBOutlet NSTableView *cheatListTable;
@property (weak) IBOutlet NSTableView *cheatSearchListTable;
@property (weak) IBOutlet NSArrayController *cheatListController;
@property (weak) IBOutlet NSArrayController *cheatSearchListController;
@property (weak) IBOutlet NSArrayController *cheatDatabaseController;
@property (weak) IBOutlet NSObjectController *cheatWindowController;
@property (weak) IBOutlet NSObjectController *cheatSelectedItemController;

@property (weak) IBOutlet NSView *viewConfigureNoSelection;
@property (weak) IBOutlet NSView *viewConfigureInternalCheat;
@property (weak) IBOutlet NSView *viewConfigureActionReplayCheat;
@property (weak) IBOutlet NSView *viewConfigureCodeBreakerCheat;

@property (weak) IBOutlet NSView *viewSearchNoSelection;
@property (weak) IBOutlet NSView *viewSearchExactValue;
@property (weak) IBOutlet NSView *viewSearchComparativeStart;
@property (weak) IBOutlet NSView *viewSearchComparativeContinue;

@property (weak) IBOutlet NSSearchField *searchField;

@property (weak) IBOutlet NSWindow *cheatDatabaseSheet;

@property (assign) NSUInteger untitledCount;
@property (readonly, strong) NSMutableDictionary *bindings;
@property (strong) CocoaDSCheatItem *workingCheat;
@property (strong) CocoaDSCheatManager *cdsCheats;
@property (readonly, strong) CocoaDSCheatSearch *cdsCheatSearch;

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

@end
