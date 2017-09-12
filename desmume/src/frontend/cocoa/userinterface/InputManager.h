/*
	Copyright (C) 2013-2015 DeSmuME Team

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
#include <ForceFeedback/ForceFeedback.h>

#if defined(__ppc__) || defined(__ppc64__)
#include <map>
#elif !defined(MAC_OS_X_VERSION_10_7) || (MAC_OS_X_VERSION_MIN_REQUIRED <= MAC_OS_X_VERSION_10_6)
#include <tr1/unordered_map>
#else
#include <unordered_map>
#endif
#include <string>
#include <vector>

#include "../audiosamplegenerator.h"
#include "../ClientInputHandler.h"

struct ClientInputDeviceProperties;
class MacInputDevicePropertiesEncoder;
@class EmuControllerDelegate;
@class InputManager;
@class InputHIDManager;

@protocol InputHIDManagerTarget <NSObject>

@required
- (BOOL) handleHIDQueue:(IOHIDQueueRef)hidQueue hidManager:(InputHIDManager *)hidManager;

@end

typedef struct
{
	char tag[INPUT_HANDLER_STRING_LENGTH];	// A string identifier for these attributes
	SEL selector;						// The selector that is called on command dispatch
	int32_t intValue[4];				// Context dependent int values
	float floatValue[4];				// Context dependent float values
	void *object[4];					// Context dependent objects
	
	bool useInputForIntCoord;			// The command will prefer the input device's int coordinate
	bool useInputForFloatCoord;			// The command will prefer the input device's float coordinate
	bool useInputForScalar;				// The command will prefer the input device's scalar
	bool useInputForSender;				// The command will prefer the input device's sender
	
	ClientInputDeviceProperties input;	// The input device's properties
	bool allowAnalogInput;				// Flag for allowing a command to accept analog inputs
} CommandAttributes;

typedef std::vector<CommandAttributes> CommandAttributesList;

#if defined(__ppc__) || defined(__ppc64__)
typedef std::map<std::string, CommandAttributes> InputCommandMap; // Key = Input key in deviceCode:elementCode format, Value = CommandAttributes
typedef std::map<std::string, CommandAttributes> CommandAttributesMap; // Key = Command Tag, Value = CommandAttributes
typedef std::map<std::string, SEL> CommandSelectorMap; // Key = Command Tag, Value = Obj-C Selector
typedef std::map<std::string, AudioSampleBlockGenerator> AudioFileSampleGeneratorMap; // Key = File path to audio file, Value = AudioSampleBlockGenerator
typedef std::map<int32_t, std::string> KeyboardKeyNameMap; // Key = Key code, Value = Key name
#elif !defined(MAC_OS_X_VERSION_10_7) || (MAC_OS_X_VERSION_MIN_REQUIRED <= MAC_OS_X_VERSION_10_6)
typedef std::tr1::unordered_map<std::string, CommandAttributes> InputCommandMap; // Key = Input key in deviceCode:elementCode format, Value = CommandAttributes
typedef std::tr1::unordered_map<std::string, CommandAttributes> CommandAttributesMap; // Key = Command Tag, Value = CommandAttributes
typedef std::tr1::unordered_map<std::string, SEL> CommandSelectorMap; // Key = Command Tag, Value = Obj-C Selector
typedef std::tr1::unordered_map<std::string, AudioSampleBlockGenerator> AudioFileSampleGeneratorMap; // Key = File path to audio file, Value = AudioSampleBlockGenerator
typedef std::tr1::unordered_map<int32_t, std::string> KeyboardKeyNameMap; // Key = Key code, Value = Key name
#else
typedef std::unordered_map<std::string, CommandAttributes> InputCommandMap; // Key = Input key in deviceCode:elementCode format, Value = CommandAttributes
typedef std::unordered_map<std::string, CommandAttributes> CommandAttributesMap; // Key = Command Tag, Value = CommandAttributes
typedef std::unordered_map<std::string, SEL> CommandSelectorMap; // Key = Command Tag, Value = Obj-C Selector
typedef std::unordered_map<std::string, AudioSampleBlockGenerator> AudioFileSampleGeneratorMap; // Key = File path to audio file, Value = AudioSampleBlockGenerator
typedef std::unordered_map<int32_t, std::string> KeyboardKeyNameMap; // Key = Key code, Value = Key name
#endif

#pragma mark -
@interface InputHIDDevice : NSObject
{
	InputHIDManager *hidManager;
	IOHIDDeviceRef hidDeviceRef;
	IOHIDQueueRef hidQueueRef;
	
	NSString *identifier;
	
	io_service_t ioService;
	FFDeviceObjectReference ffDevice;
	FFEffectObjectReference ffEffect;
	BOOL supportsForceFeedback;
	BOOL isForceFeedbackEnabled;
	
	NSRunLoop *runLoop;
	OSSpinLock spinlockRunLoop;
}

@property (retain) InputHIDManager *hidManager;
@property (readonly) IOHIDDeviceRef hidDeviceRef;
@property (readonly) NSString *manufacturerName;
@property (readonly) NSString *productName;
@property (readonly) NSString *serialNumber;
@property (readonly) NSString *identifier;
@property (readonly) BOOL supportsForceFeedback;
@property (assign) BOOL isForceFeedbackEnabled;
@property (retain) NSRunLoop *runLoop;

- (id) initWithDevice:(IOHIDDeviceRef)theDevice hidManager:(InputHIDManager *)theHIDManager;

- (void) setPropertiesUsingDictionary:(NSDictionary *)theProperties;
- (NSDictionary *) propertiesDictionary;
- (void) writeDefaults;

- (void) start;
- (void) stop;

- (void) startForceFeedbackAndIterate:(UInt32)iterations flags:(UInt32)ffFlags;
- (void) stopForceFeedback;

@end

bool InputElementCodeFromHIDElement(const IOHIDElementRef hidElementRef, char *charBuffer);
bool InputElementNameFromHIDElement(const IOHIDElementRef hidElementRef, char *charBuffer);
bool InputDeviceCodeFromHIDDevice(const IOHIDDeviceRef hidDeviceRef, char *charBuffer);
bool InputDeviceNameFromHIDDevice(const IOHIDDeviceRef hidDeviceRef, char *charBuffer);
ClientInputDeviceProperties InputAttributesOfHIDValue(IOHIDValueRef hidValueRef);
ClientInputDevicePropertiesList InputListFromHIDValue(IOHIDValueRef hidValueRef, InputManager *inputManager, bool forceDigitalInput);

size_t ClearHIDQueue(const IOHIDQueueRef hidQueue);
void HandleQueueValueAvailableCallback(void *inContext, IOReturn inResult, void *inSender);

#pragma mark -
@interface InputHIDManager : NSObject
{
	InputManager *inputManager;
	IOHIDManagerRef hidManagerRef;
	NSRunLoop *runLoop;
	NSArrayController *deviceListController;
	id<InputHIDManagerTarget> target;
	
	OSSpinLock spinlockRunLoop;
}

@property (retain) NSArrayController *deviceListController;
@property (retain) InputManager *inputManager;
@property (readonly) IOHIDManagerRef hidManagerRef;
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
	MacInputDevicePropertiesEncoder *inputEncoder;
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
@property (readonly) MacInputDevicePropertiesEncoder *inputEncoder;
@property (retain) id<InputHIDManagerTarget> hidInputTarget;
@property (readonly) InputHIDManager *hidManager;
@property (readonly) NSMutableDictionary *inputMappings;
@property (readonly) NSArray *commandTagList;
@property (readonly) NSDictionary *commandIcon;

- (void) setMappingsWithMappings:(NSDictionary *)mappings;
- (void) addMappingUsingDeviceInfoDictionary:(NSDictionary *)deviceDict commandAttributes:(const CommandAttributes *)cmdAttr;
- (void) addMappingUsingInputAttributes:(const ClientInputDeviceProperties *)inputProperty commandAttributes:(const CommandAttributes *)cmdAttr;
- (void) addMappingUsingInputList:(const ClientInputDevicePropertiesList *)inputPropertyList commandAttributes:(const CommandAttributes *)cmdAttr;
- (void) addMappingForIBAction:(const SEL)theSelector commandAttributes:(const CommandAttributes *)cmdAttr;
- (void) addMappingUsingDeviceCode:(const char *)deviceCode elementCode:(const char *)elementCode commandAttributes:(const CommandAttributes *)cmdAttr;

- (void) removeMappingUsingDeviceCode:(const char *)deviceCode elementCode:(const char *)elementCode;
- (void) removeAllMappingsForCommandTag:(const char *)commandTag;

- (CommandAttributesList) generateCommandListUsingInputList:(const ClientInputDevicePropertiesList *)inputPropertyList;
- (void) dispatchCommandList:(const CommandAttributesList *)cmdList;
- (BOOL) dispatchCommandUsingInputProperties:(const ClientInputDeviceProperties *)inputProperty;
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

@end

CommandAttributes NewDefaultCommandAttributes(const char *commandTag);
CommandAttributes NewCommandAttributesForSelector(const char *commandTag, const SEL theSelector);
CommandAttributes NewCommandAttributesForDSControl(const char *commandTag, const NDSInputID controlID);
void UpdateCommandAttributesWithDeviceInfoDictionary(CommandAttributes *cmdAttr, NSDictionary *deviceInfo);

NSMutableDictionary* DeviceInfoDictionaryWithCommandAttributes(const CommandAttributes *cmdAttr,
															   NSString *deviceCode,
															   NSString *deviceName,
															   NSString *elementCode,
															   NSString *elementName);

class MacInputDevicePropertiesEncoder : public ClientInputDevicePropertiesEncoder
{
private:
	KeyboardKeyNameMap _keyboardNameTable;
	
public:
	MacInputDevicePropertiesEncoder();
	
	virtual ClientInputDeviceProperties EncodeKeyboardInput(const int32_t keyCode, bool keyPressed);
	virtual ClientInputDeviceProperties EncodeMouseInput(const int32_t buttonNumber, float touchLocX, float touchLocY, bool buttonPressed);
	virtual ClientInputDeviceProperties EncodeIBAction(const SEL theSelector, id sender);
	virtual ClientInputDevicePropertiesList EncodeHIDQueue(const IOHIDQueueRef hidQueue, InputManager *inputManager, bool forceDigitalInput);
};
