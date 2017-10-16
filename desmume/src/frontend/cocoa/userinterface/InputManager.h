/*
	Copyright (C) 2013-2017 DeSmuME Team

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

struct ClientCommandAttributes;
struct ClientInputDeviceProperties;
class MacInputDevicePropertiesEncoder;
@class EmuControllerDelegate;
@class InputManager;
@class InputHIDManager;

@protocol InputHIDManagerTarget <NSObject>

@required
- (BOOL) handleHIDQueue:(IOHIDQueueRef)hidQueue hidManager:(InputHIDManager *)hidManager;

@end

typedef std::vector<ClientCommandAttributes> CommandAttributesList;

#if defined(__ppc__) || defined(__ppc64__)
typedef std::map<std::string, ClientCommandAttributes> InputCommandMap; // Key = Input key in deviceCode:elementCode format, Value = ClientCommandAttributes
typedef std::map<std::string, ClientCommandAttributes> CommandAttributesMap; // Key = Command Tag, Value = ClientCommandAttributes
typedef std::map<std::string, AudioSampleBlockGenerator> AudioFileSampleGeneratorMap; // Key = File path to audio file, Value = AudioSampleBlockGenerator
typedef std::map<int32_t, std::string> KeyboardKeyNameMap; // Key = Key code, Value = Key name
#elif !defined(MAC_OS_X_VERSION_10_7) || (MAC_OS_X_VERSION_MIN_REQUIRED <= MAC_OS_X_VERSION_10_6)
typedef std::tr1::unordered_map<std::string, ClientCommandAttributes> InputCommandMap; // Key = Input key in deviceCode:elementCode format, Value = ClientCommandAttributes
typedef std::tr1::unordered_map<std::string, ClientCommandAttributes> CommandAttributesMap; // Key = Command Tag, Value = ClientCommandAttributes
typedef std::tr1::unordered_map<std::string, AudioSampleBlockGenerator> AudioFileSampleGeneratorMap; // Key = File path to audio file, Value = AudioSampleBlockGenerator
typedef std::tr1::unordered_map<int32_t, std::string> KeyboardKeyNameMap; // Key = Key code, Value = Key name
#else
typedef std::unordered_map<std::string, ClientCommandAttributes> InputCommandMap; // Key = Input key in deviceCode:elementCode format, Value = ClientCommandAttributes
typedef std::unordered_map<std::string, ClientCommandAttributes> CommandAttributesMap; // Key = Command Tag, Value = ClientCommandAttributes
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
- (void) addMappingUsingDeviceInfoDictionary:(NSDictionary *)deviceDict commandAttributes:(const ClientCommandAttributes *)cmdAttr;
- (void) addMappingUsingInputAttributes:(const ClientInputDeviceProperties *)inputProperty commandAttributes:(const ClientCommandAttributes *)cmdAttr;
- (void) addMappingUsingInputList:(const ClientInputDevicePropertiesList *)inputPropertyList commandAttributes:(const ClientCommandAttributes *)cmdAttr;
- (void) addMappingForIBAction:(const SEL)theSelector commandAttributes:(const ClientCommandAttributes *)cmdAttr;
- (void) addMappingUsingDeviceCode:(const char *)deviceCode elementCode:(const char *)elementCode commandAttributes:(const ClientCommandAttributes *)cmdAttr;

- (void) removeMappingUsingDeviceCode:(const char *)deviceCode elementCode:(const char *)elementCode;
- (void) removeAllMappingsForCommandTag:(const char *)commandTag;

- (CommandAttributesList) generateCommandListUsingInputList:(const ClientInputDevicePropertiesList *)inputPropertyList;
- (void) dispatchCommandList:(const CommandAttributesList *)cmdList;
- (BOOL) dispatchCommandUsingInputProperties:(const ClientInputDeviceProperties *)inputProperty;
- (BOOL) dispatchCommandUsingIBAction:(const SEL)theSelector sender:(id)sender;

- (void) writeDefaultsInputMappings;
- (NSString *) commandTagFromInputList:(NSArray *)inputList;
- (ClientCommandAttributes) defaultCommandAttributesForCommandTag:(const char *)commandTag;

- (ClientCommandAttributes) mappedCommandAttributesOfDeviceCode:(const char *)deviceCode elementCode:(const char *)elementCode;
- (void) setMappedCommandAttributes:(const ClientCommandAttributes *)cmdAttr deviceCode:(const char *)deviceCode elementCode:(const char *)elementCode;
- (void) updateInputSettingsSummaryInDeviceInfoDictionary:(NSMutableDictionary *)deviceInfo commandTag:(const char *)commandTag;

- (OSStatus) loadAudioFileUsingPath:(NSString *)filePath;
- (AudioSampleBlockGenerator *) audioFileGeneratorFromFilePath:(NSString *)filePath;
- (void) updateAudioFileGenerators;

@end

ClientCommandAttributes NewDefaultCommandAttributes(const char *commandTag);
ClientCommandAttributes NewCommandAttributesWithFunction(const char *commandTag, const ClientCommandDispatcher commandFunc);
ClientCommandAttributes NewCommandAttributesForDSControl(const char *commandTag, const NDSInputID controlID);
void UpdateCommandAttributesWithDeviceInfoDictionary(ClientCommandAttributes *cmdAttr, NSDictionary *deviceInfo);

NSMutableDictionary* DeviceInfoDictionaryWithCommandAttributes(const ClientCommandAttributes *cmdAttr,
															   NSString *deviceCode,
															   NSString *deviceName,
															   NSString *elementCode,
															   NSString *elementName);

void ClientCommandUpdateDSController(const ClientCommandAttributes &cmdAttr, void *dispatcherObject);
void ClientCommandUpdateDSControllerWithTurbo(const ClientCommandAttributes &cmdAttr, void *dispatcherObject);
void ClientCommandUpdateDSTouch(const ClientCommandAttributes &cmdAttr, void *dispatcherObject);
void ClientCommandUpdateDSMicrophone(const ClientCommandAttributes &cmdAttr, void *dispatcherObject);
void ClientCommandUpdateDSPaddle(const ClientCommandAttributes &cmdAttr, void *dispatcherObject);
void ClientCommandAutoholdSet(const ClientCommandAttributes &cmdAttr, void *dispatcherObject);
void ClientCommandAutoholdClear(const ClientCommandAttributes &cmdAttr, void *dispatcherObject);
void ClientCommandLoadEmuSaveStateSlot(const ClientCommandAttributes &cmdAttr, void *dispatcherObject);
void ClientCommandSaveEmuSaveStateSlot(const ClientCommandAttributes &cmdAttr, void *dispatcherObject);
void ClientCommandCopyScreen(const ClientCommandAttributes &cmdAttr, void *dispatcherObject);
void ClientCommandRotateDisplayRelative(const ClientCommandAttributes &cmdAttr, void *dispatcherObject);
void ClientCommandToggleAllDisplays(const ClientCommandAttributes &cmdAttr, void *dispatcherObject);
void ClientCommandHoldToggleSpeedScalar(const ClientCommandAttributes &cmdAttr, void *dispatcherObject);
void ClientCommandToggleSpeedLimiter(const ClientCommandAttributes &cmdAttr, void *dispatcherObject);
void ClientCommandToggleAutoFrameSkip(const ClientCommandAttributes &cmdAttr, void *dispatcherObject);
void ClientCommandToggleCheats(const ClientCommandAttributes &cmdAttr, void *dispatcherObject);
void ClientCommandToggleExecutePause(const ClientCommandAttributes &cmdAttr, void *dispatcherObject);
void ClientCommandCoreExecute(const ClientCommandAttributes &cmdAttr, void *dispatcherObject);
void ClientCommandCorePause(const ClientCommandAttributes &cmdAttr, void *dispatcherObject);
void ClientCommandFrameAdvance(const ClientCommandAttributes &cmdAttr, void *dispatcherObject);
void ClientCommandFrameJump(const ClientCommandAttributes &cmdAttr, void *dispatcherObject);
void ClientCommandReset(const ClientCommandAttributes &cmdAttr, void *dispatcherObject);
void ClientCommandToggleMute(const ClientCommandAttributes &cmdAttr, void *dispatcherObject);
void ClientCommandToggleGPUState(const ClientCommandAttributes &cmdAttr, void *dispatcherObject);

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
