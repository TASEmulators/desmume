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

@class CocoaDSController;

@interface InputPrefProperties : NSObject
{
	NSString *commandTag;
	NSImage *icon;
	NSWindow *settingsSheet;
}

@property (retain) NSString *commandTag;
@property (retain) NSImage *icon;
@property (retain) NSWindow *settingsSheet;

- (id) initWithCommandTag:(NSString *)theCommandTag icon:(NSImage *)theIcon sheet:(NSWindow *)theSheet;

@end

#pragma mark -

@interface InputPrefsView : NSView <InputHIDManagerTarget, NSOutlineViewDelegate, NSOutlineViewDataSource>
{
	NSWindow *prefWindow;
	NSOutlineView *inputPrefOutlineView;
	NSObjectController *inputSettingsController;
	
	NSWindow *inputSettingsMicrophone;
	NSWindow *inputSettingsTouch;
	NSWindow *inputSettingsLoadStateSlot;
	NSWindow *inputSettingsSaveStateSlot;
	NSWindow *inputSettingsSetSpeedLimit;
	NSWindow *inputSettingsGPUState;
	
	InputManager *inputManager;
	NSString *configInputTargetID;
	NSMutableDictionary *configInputList;
	
	NSDictionary *inputPrefProperties;
	NSDictionary *inputSettingsMappings;
	NSArray *commandTagList;
}

@property (readonly) IBOutlet NSWindow *prefWindow;
@property (readonly) IBOutlet NSOutlineView *inputPrefOutlineView;
@property (readonly) IBOutlet NSObjectController *inputSettingsController;

@property (readonly) IBOutlet NSWindow *inputSettingsMicrophone;
@property (readonly) IBOutlet NSWindow *inputSettingsTouch;
@property (readonly) IBOutlet NSWindow *inputSettingsLoadStateSlot;
@property (readonly) IBOutlet NSWindow *inputSettingsSaveStateSlot;
@property (readonly) IBOutlet NSWindow *inputSettingsSetSpeedLimit;
@property (readonly) IBOutlet NSWindow *inputSettingsGPUState;

@property (readonly) IBOutlet InputManager *inputManager;
@property (retain) NSString *configInputTargetID;

- (void) initSettingsSheets;
- (NSString *) commandTagFromInputList:(NSArray *)inputList;

- (BOOL) handleKeyboardEvent:(NSEvent *)theEvent keyPressed:(BOOL)keyPressed;
- (BOOL) handleMouseButtonEvent:(NSEvent *)mouseEvent buttonPressed:(BOOL)buttonPressed;
- (BOOL) addMappingUsingInputAttributes:(const InputAttributes *)inputAttr commandTag:(NSString *)commandTag;
- (void) setMappingUsingDeviceInfoDictionary:(NSMutableDictionary *)deviceInfo;
- (void) didEndSettingsSheet:(NSWindow *)sheet returnCode:(NSInteger)returnCode contextInfo:(void *)contextInfo;

- (IBAction) setInputAdd:(id)sender;
- (IBAction) removeInput:(id)sender;
- (IBAction) changeSpeed:(id)sender;
- (IBAction) showSettingsSheet:(id)sender;
- (IBAction) closeSettingsSheet:(id)sender;

@end
