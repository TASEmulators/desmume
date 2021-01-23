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
	NSString *configInputTargetID;
	NSMutableDictionary *configInputList;
	NSMutableDictionary *inputSettingsInEdit;
	
	NSDictionary *inputSettingsMappings;
	
	NSUInteger _defaultProfileListCount;
	NSMutableArray *defaultProfilesList;
	NSMutableArray *savedProfilesList;
}

@property (weak) IBOutlet NSObject *dummyObject;
@property (weak) IBOutlet NSWindow *prefWindow;
@property (weak) IBOutlet NSPopUpButton *inputProfileMenu;
@property (weak) IBOutlet NSButton *inputProfilePreviousButton;
@property (weak) IBOutlet NSButton *inputProfileNextButton;
@property (weak) IBOutlet NSOutlineView *inputPrefOutlineView;
@property (weak) IBOutlet NSObjectController *inputSettingsController;
@property (weak) IBOutlet InputProfileController *inputProfileController;

@property (weak) IBOutlet NSWindow *inputProfileSheet;
@property (weak) IBOutlet NSWindow *inputProfileRenameSheet;
@property (weak) IBOutlet NSWindow *inputSettingsNDSInput;
@property (weak) IBOutlet NSWindow *inputSettingsMicrophone;
@property (weak) IBOutlet NSWindow *inputSettingsTouch;
@property (weak) IBOutlet NSWindow *inputSettingsLoadStateSlot;
@property (weak) IBOutlet NSWindow *inputSettingsSaveStateSlot;
@property (weak) IBOutlet NSWindow *inputSettingsSetSpeedLimit;
@property (weak) IBOutlet NSWindow *inputSettingsGPUState;
@property (weak) IBOutlet NSWindow *inputSettingsPaddleController;

@property (weak) IBOutlet NSSegmentedControl *turboPatternControl;

@property (weak) IBOutlet InputManager *inputManager;
@property (nonatomic, copy) NSString *configInputTargetID;
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
