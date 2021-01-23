/*
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


enum TroubleshootingViewID
{
	TROUBLESHOOTING_TECH_SUPPORT_VIEW_ID = 0,
	TROUBLESHOOTING_BUG_REPORT_VIEW_ID
};

@interface TroubleshootingWindowDelegate : NSObject <NSWindowDelegate>
{
	NSView *__weak currentForm;
	
	NSMutableDictionary *bindings;
}

@property (weak) IBOutlet NSObject *dummyObject;
@property (weak) IBOutlet NSWindow *window;
@property (weak) IBOutlet NSObjectController *troubleshootingWindowController;
@property (weak) IBOutlet NSObjectController *romInfoController;
@property (weak) IBOutlet NSObjectController *emuControlController;
@property (weak) IBOutlet NSObjectController *cdsCoreController;
@property (weak) IBOutlet NSView *viewSupportRequest;
@property (weak) IBOutlet NSView *viewBugReport;
@property (weak) IBOutlet NSView *viewFinishedForm;
@property (readonly, strong) NSMutableDictionary *bindings;

- (IBAction) copyRomInfoToTextFields:(id)sender;
- (IBAction) continueToFinalForm:(id)sender;
- (IBAction) backForm:(id)sender;
- (IBAction) copyInfoToPasteboard:(id)sender;
- (IBAction) goToWebpage:(id)sender;

- (void) switchViewByID:(TroubleshootingViewID)viewID;
- (void) clearAllText;
- (void) switchContentView:(NSView *)theView;

@end
