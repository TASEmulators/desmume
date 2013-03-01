/*
	Copyright (C) 2013 DeSmuME team

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
#include <libkern/OSAtomic.h>
#include <IOKit/hid/IOHIDManager.h>

#include <tr1/unordered_map>
#include <string>
#include <vector>

#define INPUT_HANDLER_MAX_ATTRIBUTE_VALUES 4

enum
{
	INPUT_ATTRIBUTE_STATE_OFF = 0,
	INPUT_ATTRIBUTE_STATE_ON,
	INPUT_ATTRIBUTE_STATE_MIXED
};

@class EmuControllerDelegate;
@class CocoaDSController;
@class InputManager;
@class InputHIDManager;

@protocol InputHIDManagerTarget <NSObject>

@required
- (BOOL) handleHIDQueue:(IOHIDQueueRef)hidQueue;

@end

typedef struct
{
	std::string deviceName;
	std::string deviceCode;
	std::string elementName;
	std::string elementCode;
	
	NSInteger inputState;
	
	NSInteger integerValue[INPUT_HANDLER_MAX_ATTRIBUTE_VALUES];
	CGFloat floatValue[INPUT_HANDLER_MAX_ATTRIBUTE_VALUES];
	id object[INPUT_HANDLER_MAX_ATTRIBUTE_VALUES];
} InputAttributes;

typedef struct
{
	SEL selector;
	NSInteger commandID;
	NSInteger inputState;
	
	NSInteger integerValue[INPUT_HANDLER_MAX_ATTRIBUTE_VALUES];
	BOOL useIntegerInputValue[INPUT_HANDLER_MAX_ATTRIBUTE_VALUES];
	
	CGFloat floatValue[INPUT_HANDLER_MAX_ATTRIBUTE_VALUES];
	BOOL useFloatInputValue[INPUT_HANDLER_MAX_ATTRIBUTE_VALUES];
	
	id object[INPUT_HANDLER_MAX_ATTRIBUTE_VALUES];
	BOOL useObjectInputValue[INPUT_HANDLER_MAX_ATTRIBUTE_VALUES];
} CommandAttributes;

typedef std::vector<InputAttributes> InputAttributesList;
typedef std::vector<CommandAttributes> CommandAttributesList;
typedef std::tr1::unordered_map<std::string, std::string> CommandTagMap; // Key = Input key in deviceCode:elementCode format, Value = Command Tag
typedef std::tr1::unordered_map<std::string, CommandAttributes> CommandAttributesMap; // Key = Command Tag, Value = CommandAttributes

#pragma mark -
@interface InputHIDDevice : NSObject
{
	InputHIDManager *hidManager;
	IOHIDDeviceRef hidDeviceRef;
	IOHIDQueueRef hidQueueRef;
	NSRunLoop *runLoop;
	OSSpinLock spinlockRunLoop;
}

@property (retain) InputHIDManager *hidManager;
@property (readonly) IOHIDDeviceRef hidDeviceRef;
@property (retain) NSRunLoop *runLoop;

- (id) initWithDevice:(IOHIDDeviceRef)theDevice hidManager:(InputHIDManager *)theHIDManager;

- (void) start;
- (void) stop;

- (NSString *) manufacturerName;
- (NSString *) productName;
- (NSString *) serialNumber;

@end

BOOL GetOnStateFromHIDValueRef(IOHIDValueRef hidValueRef);
InputAttributes InputAttributesOfHIDValue(IOHIDValueRef hidValueRef, const char *altElementCode, const char *altElementName, NSNumber *altOnState);
InputAttributesList InputListFromHIDValue(IOHIDValueRef hidValueRef);
InputAttributesList InputListFromHatSwitchValue(IOHIDValueRef hidValueRef, bool useEightDirection);

unsigned int ClearHIDQueue(const IOHIDQueueRef hidQueue);
void HandleQueueValueAvailableCallback(void *inContext, IOReturn inResult, void *inSender);

#pragma mark -
@interface InputHIDManager : NSObject
{
	InputManager *inputManager;
	IOHIDManagerRef hidManagerRef;
	NSRunLoop *runLoop;
	NSMutableSet *deviceList;
	id<InputHIDManagerTarget> target;
	
	OSSpinLock spinlockRunLoop;
}

@property (retain) InputManager *inputManager;
@property (readonly) IOHIDManagerRef hidManagerRef;
@property (readonly) NSMutableSet *deviceList;
@property (assign) id target;
@property (retain) NSRunLoop *runLoop;

- (id) initWithInputManager:(InputManager *)theInputManager;

@end

void HandleDeviceMatchingCallback(void *inContext, IOReturn inResult, void *inSender, IOHIDDeviceRef inIOHIDDeviceRef);
void HandleDeviceRemovalCallback(void *inContext, IOReturn inResult, void *inSender, IOHIDDeviceRef inIOHIDDeviceRef);

#pragma mark -
@interface InputManager : NSObject
{
	EmuControllerDelegate *emuControl;
	id<InputHIDManagerTarget> hidInputTarget;
	InputHIDManager *hidManager;
		
	CommandTagMap commandMap;
	CommandAttributesMap commandMasterList;
}

@property (assign) IBOutlet EmuControllerDelegate *emuControl;
@property (retain) id<InputHIDManagerTarget> hidInputTarget;

- (void) addMappingsUsingUserDefaults;
- (void) addMappingUsingDeviceInfoDictionary:(NSDictionary *)deviceDict commandTag:(const char *)commandTag;
- (void) addMappingUsingInputAttributes:(const InputAttributes *)inputAttr commandTag:(const char *)commandTag;
- (void) addMappingUsingInputList:(const InputAttributesList *)inputList commandTag:(const char *)commandTag;
- (void) addMappingForIBAction:(const SEL)theSelector commandTag:(const char *)commandTag;
- (void) addMappingUsingDeviceCode:(const char *)deviceCode elementCode:(const char *)elementCode commandTag:(const char *)commandTag;

- (void) removeMappingUsingDeviceCode:(const char *)deviceCode elementCode:(const char *)elementCode;
- (void) removeAllMappingsOfCommandTag:(const char *)commandTag;

- (CommandAttributesList) generateCommandListUsingInputList:(const InputAttributesList *)inputList;
- (void) dispatchCommandList:(const CommandAttributesList *)cmdList;
- (BOOL) dispatchCommandUsingInputAttributes:(const InputAttributes *)inputAttr;
- (BOOL) dispatchCommandUsingIBAction:(const SEL)theSelector tag:(const NSInteger)tagValue;

- (void) writeUserDefaultsMappingUsingInputAttributes:(const InputAttributes *)inputAttr commandTag:(const char *)commandTag;

InputAttributesList InputManagerEncodeHIDQueue(const IOHIDQueueRef hidQueue);
InputAttributes InputManagerEncodeKeyboardInput(const unsigned short keyCode, BOOL keyPressed);
InputAttributes InputManagerEncodeMouseButtonInput(const NSInteger buttonNumber, const NSPoint touchLoc, BOOL buttonPressed);
InputAttributes InputManagerEncodeIBAction(const SEL theSelector, const NSInteger tagValue);

@end
