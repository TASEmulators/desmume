/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2012-2014 DeSmuME Team

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


@interface InputPrefsView : NSView <InputHIDManagerTarget, NSOutlineViewDelegate, NSOutlineViewDataSource>
{
	NSObject *__unsafe_unretained dummyObject;
	NSWindow *__weak prefWindow;
	NSPopUpButton *__weak inputProfileMenu;
	NSButton *__weak inputProfilePreviousButton;
	NSButton *__weak inputProfileNextButton;
	NSOutlineView *__weak inputPrefOutlineView;
	NSObjectController *__weak inputSettingsController;
	InputProfileController *__weak inputProfileController;
	
	NSWindow *__weak inputProfileSheet;
	NSWindow *__weak inputProfileRenameSheet;
	NSWindow *__weak inputSettingsNDSInput;
	NSWindow *__weak inputSettingsMicrophone;
	NSWindow *__weak inputSettingsTouch;
	NSWindow *__weak inputSettingsLoadStateSlot;
	NSWindow *__weak inputSettingsSaveStateSlot;
	NSWindow *__weak inputSettingsSetSpeedLimit;
	NSWindow *__weak inputSettingsGPUState;
	NSWindow *__weak inputSettingsPaddleController;
	
	NSSegmentedControl *__weak turboPatternControl;
	
	InputManager *__weak inputManager;
	NSString *configInputTargetID;
	NSMutableDictionary *configInputList;
	NSMutableDictionary *inputSettingsInEdit;
	
	NSDictionary *inputSettingsMappings;
	
	NSUInteger _defaultProfileListCount;
	NSMutableArray *defaultProfilesList;
	NSMutableArray *savedProfilesList;
}

@property (unsafe_unretained, readonly) IBOutlet NSObject *dummyObject;
@property (weak, readonly) IBOutlet NSWindow *prefWindow;
@property (weak, readonly) IBOutlet NSPopUpButton *inputProfileMenu;
@property (weak, readonly) IBOutlet NSButton *inputProfilePreviousButton;
@property (weak, readonly) IBOutlet NSButton *inputProfileNextButton;
@property (weak, readonly) IBOutlet NSOutlineView *inputPrefOutlineView;
@property (weak, readonly) IBOutlet NSObjectController *inputSettingsController;
@property (weak, readonly) IBOutlet InputProfileController *inputProfileController;

@property (weak, readonly) IBOutlet NSWindow *inputProfileSheet;
@property (weak, readonly) IBOutlet NSWindow *inputProfileRenameSheet;
@property (weak, readonly) IBOutlet NSWindow *inputSettingsNDSInput;
@property (weak, readonly) IBOutlet NSWindow *inputSettingsMicrophone;
@property (weak, readonly) IBOutlet NSWindow *inputSettingsTouch;
@property (weak, readonly) IBOutlet NSWindow *inputSettingsLoadStateSlot;
@property (weak, readonly) IBOutlet NSWindow *inputSettingsSaveStateSlot;
@property (weak, readonly) IBOutlet NSWindow *inputSettingsSetSpeedLimit;
@property (weak, readonly) IBOutlet NSWindow *inputSettingsGPUState;
@property (weak, readonly) IBOutlet NSWindow *inputSettingsPaddleController;

@property (weak, readonly) IBOutlet NSSegmentedControl *turboPatternControl;

@property (weak, readonly) IBOutlet InputManager *inputManager;
@property (strong) NSString *configInputTargetID;
@property (strong) NSMutableDictionary *inputSettingsInEdit;

- (void) initSettingsSheets;
- (void) loadSavedProfilesList;
- (void) populateInputProfileMenu;

- (BOOL) handleKeyboardEvent:(NSEvent *)theEvent keyPressed:(BOOL)keyPressed;
- (BOOL) handleMouseButtonEvent:(NSEvent *)mouseEvent buttonPressed:(BOOL)buttonPressed;
- (BOOL) addMappingUsingInputAttributes:(const ClientInputDeviceProperties *)inputProperty commandTag:(NSString *)commandTag;
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

- (IBAction) updateCustomTurboPatternControls:(id)sender;
- (IBAction) setTurboPatternBits:(id)sender;
- (IBAction) setTurboPatternUsingTag:(id)sender;

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
