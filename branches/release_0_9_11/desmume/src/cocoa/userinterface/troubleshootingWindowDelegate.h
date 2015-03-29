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

#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_5
@interface TroubleshootingWindowDelegate : NSObject <NSWindowDelegate>
#else
@interface TroubleshootingWindowDelegate : NSObject
#endif
{
	NSObject *dummyObject;
	NSWindow *window;
	NSObjectController *troubleshootingWindowController;
	NSObjectController *romInfoController;
	NSObjectController *emuControlController;
	NSObjectController *cdsCoreController;
	
	NSView *viewSupportRequest;
	NSView *viewBugReport;
	NSView *viewFinishedForm;
	
	NSView *currentForm;
	
	NSMutableDictionary *bindings;
}

@property (readonly) IBOutlet NSObject *dummyObject;
@property (readonly) IBOutlet NSWindow *window;
@property (readonly) IBOutlet NSObjectController *troubleshootingWindowController;
@property (readonly) IBOutlet NSObjectController *romInfoController;
@property (readonly) IBOutlet NSObjectController *emuControlController;
@property (readonly) IBOutlet NSObjectController *cdsCoreController;
@property (readonly) IBOutlet NSView *viewSupportRequest;
@property (readonly) IBOutlet NSView *viewBugReport;
@property (readonly) IBOutlet NSView *viewFinishedForm;
@property (readonly) NSMutableDictionary *bindings;

- (IBAction) copyRomInfoToTextFields:(id)sender;
- (IBAction) continueToFinalForm:(id)sender;
- (IBAction) backForm:(id)sender;
- (IBAction) copyInfoToPasteboard:(id)sender;
- (IBAction) goToWebpage:(id)sender;

- (void) switchViewByID:(TroubleshootingViewID)viewID;
- (void) clearAllText;
- (void) switchContentView:(NSView *)theView;

@end
