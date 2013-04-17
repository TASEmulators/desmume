/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2012-2013 DeSmuME team

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
#import "InputManager.h"

@class InputProfileController;


#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_5
@interface InputPrefsView : NSView <InputHIDManagerTarget, NSOutlineViewDelegate, NSOutlineViewDataSource>
#else
@interface InputPrefsView : NSView <InputHIDManagerTarget>
#endif
{
	NSObject *dummyObject;
	NSWindow *prefWindow;
	NSPopUpButton *inputProfileMenu;
	NSButton *inputProfilePreviousButton;
	NSButton *inputProfileNextButton;
	NSOutlineView *inputPrefOutlineView;
	NSObjectController *inputSettingsController;
	InputProfileController *inputProfileController;
	
	NSWindow *inputProfileSheet;
	NSWindow *inputProfileRenameSheet;
	NSWindow *inputSettingsMicrophone;
	NSWindow *inputSettingsTouch;
	NSWindow *inputSettingsLoadStateSlot;
	NSWindow *inputSettingsSaveStateSlot;
	NSWindow *inputSettingsSetSpeedLimit;
	NSWindow *inputSettingsGPUState;
	
	InputManager *inputManager;
	NSString *configInputTargetID;
	NSMutableDictionary *configInputList;
	NSMutableDictionary *inputSettingsInEdit;
	
	NSDictionary *inputSettingsMappings;
	
	NSUInteger _defaultProfileListCount;
	NSMutableArray *defaultProfilesList;
	NSMutableArray *savedProfilesList;
}

@property (readonly) IBOutlet NSObject *dummyObject;
@property (readonly) IBOutlet NSWindow *prefWindow;
@property (readonly) IBOutlet NSPopUpButton *inputProfileMenu;
@property (readonly) IBOutlet NSButton *inputProfilePreviousButton;
@property (readonly) IBOutlet NSButton *inputProfileNextButton;
@property (readonly) IBOutlet NSOutlineView *inputPrefOutlineView;
@property (readonly) IBOutlet NSObjectController *inputSettingsController;
@property (readonly) IBOutlet InputProfileController *inputProfileController;

@property (readonly) IBOutlet NSWindow *inputProfileSheet;
@property (readonly) IBOutlet NSWindow *inputProfileRenameSheet;
@property (readonly) IBOutlet NSWindow *inputSettingsMicrophone;
@property (readonly) IBOutlet NSWindow *inputSettingsTouch;
@property (readonly) IBOutlet NSWindow *inputSettingsLoadStateSlot;
@property (readonly) IBOutlet NSWindow *inputSettingsSaveStateSlot;
@property (readonly) IBOutlet NSWindow *inputSettingsSetSpeedLimit;
@property (readonly) IBOutlet NSWindow *inputSettingsGPUState;

@property (readonly) IBOutlet InputManager *inputManager;
@property (retain) NSString *configInputTargetID;
@property (retain) NSMutableDictionary *inputSettingsInEdit;

- (void) initSettingsSheets;
- (void) populateInputProfileMenu;

- (BOOL) handleKeyboardEvent:(NSEvent *)theEvent keyPressed:(BOOL)keyPressed;
- (BOOL) handleMouseButtonEvent:(NSEvent *)mouseEvent buttonPressed:(BOOL)buttonPressed;
- (BOOL) addMappingUsingInputAttributes:(const InputAttributes *)inputAttr commandTag:(NSString *)commandTag;
- (void) setMappingUsingDeviceInfoDictionary:(NSMutableDictionary *)deviceInfo;
- (BOOL) doesProfileNameExist:(NSString *)profileName;
- (void) updateSelectedProfileName;
- (void) didEndSettingsSheet:(NSWindow *)sheet returnCode:(NSInteger)returnCode contextInfo:(void *)contextInfo;
- (void) didEndProfileSheet:(NSWindow *)sheet returnCode:(NSInteger)returnCode contextInfo:(void *)contextInfo;
- (void) didEndProfileRenameSheet:(NSWindow *)sheet returnCode:(NSInteger)returnCode contextInfo:(void *)contextInfo;

- (IBAction) setInputAdd:(id)sender;
- (IBAction) removeInput:(id)sender;
- (IBAction) changeSpeed:(id)sender;
- (IBAction) showSettingsSheet:(id)sender;
- (IBAction) closeSettingsSheet:(id)sender;

- (IBAction) profileNew:(id)sender;
- (IBAction) profileView:(id)sender;
- (IBAction) profileApply:(id)sender;
- (IBAction) profileRename:(id)sender;
- (IBAction) profileSave:(id)sender;
- (IBAction) profileDelete:(id)sender;
- (IBAction) profileSelect:(id)sender;
- (IBAction) closeProfileSheet:(id)sender;
- (IBAction) closeProfileRenameSheet:(id)sender;

- (IBAction) audioFileChoose:(id)sender;
- (IBAction) audioFileChooseNone:(id)sender;

@end
