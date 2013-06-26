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

#if defined(__ppc__) || defined(__ppc64__)
#include <map>
#else
#include <tr1/unordered_map>
#endif
#include <string>
#include <vector>

#include "mic_ext.h"
#undef BOOL

#define INPUT_HANDLER_STRING_LENGTH 256

enum InputAttributeState
{
	INPUT_ATTRIBUTE_STATE_OFF = 0,
	INPUT_ATTRIBUTE_STATE_ON,
	INPUT_ATTRIBUTE_STATE_MIXED
};

@class EmuControllerDelegate;
@class InputManager;
@class InputHIDManager;

@protocol InputHIDManagerTarget <NSObject>

@required
- (BOOL) handleHIDQueue:(IOHIDQueueRef)hidQueue;

@end

typedef struct
{
	char deviceName[INPUT_HANDLER_STRING_LENGTH];
	char deviceCode[INPUT_HANDLER_STRING_LENGTH];
	char elementName[INPUT_HANDLER_STRING_LENGTH];
	char elementCode[INPUT_HANDLER_STRING_LENGTH];
	
	InputAttributeState state;		// The input state that is sent on command dispatch
	int32_t intCoordX;				// The X-coordinate as an int for commands that require a location
	int32_t intCoordY;				// The Y-coordinate as an int for commands that require a location
	float floatCoordX;				// The X-coordinate as a float for commands that require a location
	float floatCoordY;				// The Y-coordinate as a float for commands that require a location
	float scalar;					// A scalar value for commands that require a scalar
	id sender;						// An object for commands that require an object
} InputAttributes;

typedef struct
{
	char tag[INPUT_HANDLER_STRING_LENGTH];	// A string identifier for these attributes
	SEL selector;					// The selector that is called on command dispatch
	int32_t intValue[4];			// Context dependent int values
	float floatValue[4];			// Context dependent float values
	id object[4];					// Context dependent objects
	
	bool useInputForIntCoord;		// The command will prefer the input device's int coordinate
	bool useInputForFloatCoord;		// The command will prefer the input device's float coordinate
	bool useInputForScalar;			// The command will prefer the input device's scalar
	bool useInputForSender;			// The command will prefer the input device's sender
	
	InputAttributes input;			// The input device's attributes
} CommandAttributes;

typedef std::vector<InputAttributes> InputAttributesList;
typedef std::vector<CommandAttributes> CommandAttributesList;

#if defined(__ppc__) || defined(__ppc64__)
typedef std::map<std::string, CommandAttributes> InputCommandMap; // Key = Input key in deviceCode:elementCode format, Value = CommandAttributes
typedef std::map<std::string, CommandAttributes> CommandAttributesMap; // Key = Command Tag, Value = CommandAttributes
typedef std::map<std::string, SEL> CommandSelectorMap; // Key = Command Tag, Value = Obj-C Selector
typedef std::map<std::string, AudioSampleBlockGenerator> AudioFileSampleGeneratorMap; // Key = File path to audio file, Value = AudioSampleBlockGenerator
#else
typedef std::tr1::unordered_map<std::string, CommandAttributes> InputCommandMap; // Key = Input key in deviceCode:elementCode format, Value = CommandAttributes
typedef std::tr1::unordered_map<std::string, CommandAttributes> CommandAttributesMap; // Key = Command Tag, Value = CommandAttributes
typedef std::tr1::unordered_map<std::string, SEL> CommandSelectorMap; // Key = Command Tag, Value = Obj-C Selector
typedef std::tr1::unordered_map<std::string, AudioSampleBlockGenerator> AudioFileSampleGeneratorMap; // Key = File path to audio file, Value = AudioSampleBlockGenerator
#endif

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
InputAttributes InputAttributesOfHIDValue(IOHIDValueRef hidValueRef, const char *altElementCode, const char *altElementName, bool *altOnState);
InputAttributesList InputListFromHIDValue(IOHIDValueRef hidValueRef);
InputAttributesList InputListFromHatSwitchValue(IOHIDValueRef hidValueRef, bool useEightDirection);

size_t ClearHIDQueue(const IOHIDQueueRef hidQueue);
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
	NSMutableDictionary *inputMappings;
	NSArray *commandTagList;
	NSDictionary *commandIcon;
	
	InputCommandMap commandMap;
	CommandAttributesMap defaultCommandAttributes;
	CommandSelectorMap commandSelector;
	AudioFileSampleGeneratorMap audioFileGenerators;
}

@property (readonly) IBOutlet EmuControllerDelegate *emuControl;
@property (retain) id<InputHIDManagerTarget> hidInputTarget;
@property (readonly) NSMutableDictionary *inputMappings;
@property (readonly) NSArray *commandTagList;
@property (readonly) NSDictionary *commandIcon;

- (void) setMappingsWithMappings:(NSDictionary *)mappings;
- (void) addMappingUsingDeviceInfoDictionary:(NSDictionary *)deviceDict commandAttributes:(const CommandAttributes *)cmdAttr;
- (void) addMappingUsingInputAttributes:(const InputAttributes *)inputAttr commandAttributes:(const CommandAttributes *)cmdAttr;
- (void) addMappingUsingInputList:(const InputAttributesList *)inputList commandAttributes:(const CommandAttributes *)cmdAttr;
- (void) addMappingForIBAction:(const SEL)theSelector commandAttributes:(const CommandAttributes *)cmdAttr;
- (void) addMappingUsingDeviceCode:(const char *)deviceCode elementCode:(const char *)elementCode commandAttributes:(const CommandAttributes *)cmdAttr;

- (void) removeMappingUsingDeviceCode:(const char *)deviceCode elementCode:(const char *)elementCode;
- (void) removeAllMappingsForCommandTag:(const char *)commandTag;

- (CommandAttributesList) generateCommandListUsingInputList:(const InputAttributesList *)inputList;
- (void) dispatchCommandList:(const CommandAttributesList *)cmdList;
- (BOOL) dispatchCommandUsingInputAttributes:(const InputAttributes *)inputAttr;
- (BOOL) dispatchCommandUsingIBAction:(const SEL)theSelector sender:(id)sender;

- (void) writeDefaultsInputMappings;
- (NSString *) commandTagFromInputList:(NSArray *)inputList;
- (SEL) selectorOfCommandTag:(const char *)commandTag;
- (CommandAttributes) defaultCommandAttributesForCommandTag:(const char *)commandTag;

- (CommandAttributes) mappedCommandAttributesOfDeviceCode:(const char *)deviceCode elementCode:(const char *)elementCode;
- (void) setMappedCommandAttributes:(const CommandAttributes *)cmdAttr deviceCode:(const char *)deviceCode elementCode:(const char *)elementCode;
- (void) updateInputSettingsSummaryInDeviceInfoDictionary:(NSMutableDictionary *)deviceInfo commandTag:(const char *)commandTag;

- (OSStatus) loadAudioFileUsingPath:(NSString *)filePath;
- (AudioSampleBlockGenerator *) audioFileGeneratorFromFilePath:(NSString *)filePath;
- (void) updateAudioFileGenerators;

CommandAttributes NewDefaultCommandAttributes(const char *commandTag);
CommandAttributes NewCommandAttributesForSelector(const char *commandTag, const SEL theSelector);
CommandAttributes NewCommandAttributesForDSControl(const char *commandTag, const NSUInteger controlID);
void UpdateCommandAttributesWithDeviceInfoDictionary(CommandAttributes *cmdAttr, NSDictionary *deviceInfo);

NSMutableDictionary* DeviceInfoDictionaryWithCommandAttributes(const CommandAttributes *cmdAttr,
															   NSString *deviceCode,
															   NSString *deviceName,
															   NSString *elementCode,
															   NSString *elementName);

InputAttributesList InputManagerEncodeHIDQueue(const IOHIDQueueRef hidQueue);
InputAttributes InputManagerEncodeKeyboardInput(const unsigned short keyCode, BOOL keyPressed);
InputAttributes InputManagerEncodeMouseButtonInput(const NSInteger buttonNumber, const NSPoint touchLoc, BOOL buttonPressed);
InputAttributes InputManagerEncodeIBAction(const SEL theSelector, id sender);

@end
