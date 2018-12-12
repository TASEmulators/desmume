/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2012-2018 DeSmuME Team
	
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

#import "InputManager.h"
#import "EmuControllerDelegate.h"

#import "cocoa_globals.h"
#import "cocoa_input.h"
#import "cocoa_util.h"

#include <AudioToolbox/AudioToolbox.h>

/*
 Get the symbols for UpdateSystemActivity().
 
 For some reason, in Mac OS v10.5 and earlier, UpdateSystemActivity() is not
 defined for 64-bit, even though it does work on 64-bit systems. So we're going
 to copy the symbols from OSServices/Power.h so that we can use them. This
 solution is better than making an absolute path to the Power.h file, since
 we can't assume that the header file will always be in the same location.
 
 Note that this isn't a problem on 32-bit, or if the target SDK is Mac OS v10.6
 or later.
 */

#if !__LP64__ || MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_5

#include <CoreServices/CoreServices.h>

#else

#ifdef __cplusplus
extern "C"
{
#endif
	
	extern OSErr UpdateSystemActivity(UInt8 activity);
	
	enum
	{
		OverallAct		= 0,
		UsrActivity		= 1,
		NetActivity		= 2,
		HDActivity		= 3,
		IdleActivity	= 4
	};
	
#ifdef __cplusplus
}
#endif

#endif // !__LP64__ || MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_5

#pragma mark -
@implementation InputHIDDevice

@synthesize hidManager;
@synthesize hidDeviceRef;
@dynamic manufacturerName;
@dynamic productName;
@dynamic serialNumber;
@synthesize identifier;
@dynamic supportsForceFeedback;
@dynamic isForceFeedbackEnabled;
@dynamic runLoop;

static NSDictionary *hidUsageTable = nil;

- (id) initWithDevice:(IOHIDDeviceRef)theDevice hidManager:(InputHIDManager *)theHIDManager
{
	self = [super init];
	if(self == nil)
	{
		return self;
	}
	
	if (hidUsageTable == nil)
	{
		hidUsageTable = [[NSDictionary alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"HID_usage_strings" ofType:@"plist"]];
	}
	
	hidManager = [theHIDManager retain];
	hidDeviceRef = theDevice;
	
	hidQueueRef = IOHIDQueueCreate(kCFAllocatorDefault, hidDeviceRef, 10, kIOHIDOptionsTypeNone);
	if (hidQueueRef == NULL)
	{
		[self release];
		return nil;
	}
	
	CFArrayRef elementArray = IOHIDDeviceCopyMatchingElements(hidDeviceRef, NULL, kIOHIDOptionsTypeNone);
	NSEnumerator *enumerator = [(NSArray *)elementArray objectEnumerator];
	IOHIDElementRef hidElement = NULL;
	
	while ((hidElement = (IOHIDElementRef)[enumerator nextObject]))
	{
		IOHIDQueueAddElement(hidQueueRef, hidElement);
	}
	
    CFRelease(elementArray);
	
	// Set up force feedback.
#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_5
	if (IsOSXVersionSupported(10, 6, 0))
	{
		ioService = IOHIDDeviceGetService(hidDeviceRef);
		if (ioService != MACH_PORT_NULL)
		{
			IOObjectRetain(ioService);
		}
	}
	else
#else
	{
		ioService = MACH_PORT_NULL;
		
		CFMutableDictionaryRef matchingDict = IOServiceMatching(kIOHIDDeviceKey);
		if (matchingDict)
		{
			CFStringRef locationKey = CFSTR(kIOHIDLocationIDKey);
			CFTypeRef deviceLocation = IOHIDDeviceGetProperty(hidDeviceRef, locationKey);
			if (deviceLocation != NULL)
			{
				CFDictionaryAddValue(matchingDict, locationKey, deviceLocation);
				
				//This eats a reference to matchingDict, so we don't need a separate release.
				//The result, meanwhile, has a reference count of 1 and must be released by the caller.
				ioService = IOServiceGetMatchingService(kIOMasterPortDefault, matchingDict);
			}
			else
			{
				CFRelease(matchingDict);
			}
		}
	}
#endif
	
	ffDevice = NULL;
	ffEffect = NULL;
	if (ioService != MACH_PORT_NULL && [self supportsForceFeedback])
	{
		HRESULT ffResult = FFCreateDevice(ioService, &ffDevice);
		if (ffDevice != NULL && ffResult != FF_OK)
		{
			FFReleaseDevice(ffDevice);
			ffDevice = NULL;
		}
		
		// Generate the force feedback effect.
		if (ffDevice != NULL)
		{
			DWORD rgdwAxes[1] = {FFJOFS_Y};
			LONG rglDirection[2] = {0};
			
			FFCONSTANTFORCE cf;
			cf.lMagnitude = FF_FFNOMINALMAX;
			
			FFEFFECT newEffect;
			newEffect.dwSize = sizeof(FFEFFECT);
			newEffect.dwFlags = FFEFF_CARTESIAN | FFEFF_OBJECTOFFSETS;
			newEffect.dwDuration = 1000000; // Equivalent to 1 second
			newEffect.dwSamplePeriod = 0;
			newEffect.dwGain = FF_FFNOMINALMAX;
			newEffect.dwTriggerButton = FFEB_NOTRIGGER;
			newEffect.dwTriggerRepeatInterval = 0;
			newEffect.cAxes = 1;
			newEffect.rgdwAxes = rgdwAxes;
			newEffect.rglDirection = rglDirection;
			newEffect.lpEnvelope = NULL;
			newEffect.cbTypeSpecificParams = sizeof(FFCONSTANTFORCE);
			newEffect.lpvTypeSpecificParams = &cf;
			newEffect.dwStartDelay = 0;
			
			FFDeviceCreateEffect(ffDevice, kFFEffectType_ConstantForce_ID, &newEffect, &ffEffect);
			if (ffEffect == NULL)
			{
				FFReleaseDevice(ffDevice);
				ffDevice = NULL;
			}
		}
	}
	
	isForceFeedbackEnabled = (ffDevice != nil);
	if (isForceFeedbackEnabled)
	{
		[self startForceFeedbackAndIterate:RUMBLE_ITERATIONS_ENABLE flags:0];
	}
	
	// Set up the device identifier.
	CFNumberRef cfVendorIDNumber = (CFNumberRef)IOHIDDeviceGetProperty(hidDeviceRef, CFSTR(kIOHIDVendorIDKey));
	CFNumberRef cfProductIDNumber = (CFNumberRef)IOHIDDeviceGetProperty(hidDeviceRef, CFSTR(kIOHIDProductIDKey));
	CFStringRef cfDeviceSerial = (CFStringRef)IOHIDDeviceGetProperty(hidDeviceRef, CFSTR(kIOHIDSerialNumberKey));
	
	if (cfDeviceSerial != nil)
	{
		identifier = [NSString stringWithFormat:@"%d/%d/%@", [(NSNumber *)cfVendorIDNumber intValue], [(NSNumber *)cfProductIDNumber intValue], cfDeviceSerial];
	}
	else
	{
		CFNumberRef cfLocationIDNumber = (CFNumberRef)IOHIDDeviceGetProperty(hidDeviceRef, CFSTR(kIOHIDLocationIDKey));
		identifier = [NSString stringWithFormat:@"%d/%d/0x%08X", [(NSNumber *)cfVendorIDNumber intValue], [(NSNumber *)cfProductIDNumber intValue], [(NSNumber *)cfLocationIDNumber intValue]];
	}
	
	[identifier retain];
	
	spinlockRunLoop = OS_SPINLOCK_INIT;
	[self setRunLoop:[NSRunLoop currentRunLoop]];
	
	return self;
}

- (void)dealloc
{
	[self stopForceFeedback];
	[self stop];
	[self setRunLoop:nil];
	[self setHidManager:nil];
	
	if (hidQueueRef != NULL)
	{
		CFRelease(hidQueueRef);
		hidQueueRef = NULL;
	}
	
	if (ffDevice != NULL)
	{
		FFEffectUnload(ffEffect);
		FFReleaseDevice(ffDevice);
		ffDevice = NULL;
	}
	
	if (ioService != MACH_PORT_NULL)
	{
		IOObjectRelease(ioService);
		ioService = MACH_PORT_NULL;
	}
	
	[identifier release];
	
	[super dealloc];
}

- (NSString *) manufacturerName
{
	return (NSString *)IOHIDDeviceGetProperty([self hidDeviceRef], CFSTR(kIOHIDManufacturerKey));
}

- (NSString *) productName
{
	return (NSString *)IOHIDDeviceGetProperty([self hidDeviceRef], CFSTR(kIOHIDProductKey));
}

- (NSString *) serialNumber
{
	return (NSString *)IOHIDDeviceGetProperty([self hidDeviceRef], CFSTR(kIOHIDSerialNumberKey));
}

- (BOOL) supportsForceFeedback
{
	return (ioService != MACH_PORT_NULL) ? (FFIsForceFeedback(ioService) == FF_OK) : NO;
}

- (void) setIsForceFeedbackEnabled:(BOOL)theState
{
	if (ffDevice != NULL)
	{
		// Enable/disable force feedback by maxing/zeroing out the device gain.
		UInt32 gainValue = (theState) ? FF_FFNOMINALMAX : 0;
		FFDeviceSetForceFeedbackProperty(ffDevice, FFPROP_FFGAIN, &gainValue);
		
		if (theState)
		{
			[self startForceFeedbackAndIterate:RUMBLE_ITERATIONS_ENABLE flags:0];
		}
		else
		{
			[self stopForceFeedback];
		}
		
		isForceFeedbackEnabled = theState;
	}
	else
	{
		isForceFeedbackEnabled = NO;
	}
	
	[self writeDefaults];
}

- (BOOL) isForceFeedbackEnabled
{
	return isForceFeedbackEnabled;
}

- (void) setRunLoop:(NSRunLoop *)theRunLoop
{
	OSSpinLockLock(&spinlockRunLoop);
	
	if (theRunLoop == runLoop)
	{
		OSSpinLockUnlock(&spinlockRunLoop);
		return;
	}
	
	if (theRunLoop == nil)
	{
		IOHIDQueueUnscheduleFromRunLoop(hidQueueRef, [runLoop getCFRunLoop], kCFRunLoopDefaultMode);
	}
	else
	{
		[theRunLoop retain];
		IOHIDQueueRegisterValueAvailableCallback(hidQueueRef, HandleQueueValueAvailableCallback, self);
		IOHIDQueueScheduleWithRunLoop(hidQueueRef, [theRunLoop getCFRunLoop], kCFRunLoopDefaultMode);
	}
	
	[runLoop release];
	runLoop = theRunLoop;
	
	OSSpinLockUnlock(&spinlockRunLoop);
}

- (NSRunLoop *) runLoop
{
	OSSpinLockLock(&spinlockRunLoop);
	NSRunLoop *theRunLoop = runLoop;
	OSSpinLockUnlock(&spinlockRunLoop);
	
	return theRunLoop;
}

- (void) setPropertiesUsingDictionary:(NSDictionary *)theProperties
{
	if (theProperties == nil)
	{
		return;
	}
	
	NSNumber *isFFEnabledNumber = (NSNumber *)[theProperties objectForKey:@"isForceFeedbackEnabled"];
	if (isFFEnabledNumber != nil)
	{
		[self setIsForceFeedbackEnabled:[isFFEnabledNumber boolValue]];
	}
}

- (NSDictionary *) propertiesDictionary
{
	return [NSDictionary dictionaryWithObjectsAndKeys:
			[NSNumber numberWithBool:[self isForceFeedbackEnabled]], @"isForceFeedbackEnabled",
			[self manufacturerName], @"manufacturerName",
			[self productName], @"productName",
			[self serialNumber], @"serialNumber",
			nil];
}

- (void) writeDefaults
{
	NSDictionary *savedInputDeviceDict = [[NSUserDefaults standardUserDefaults] dictionaryForKey:@"Input_SavedDeviceProperties"];
	if (savedInputDeviceDict == nil)
	{
		return;
	}
	
	NSMutableDictionary *newInputDeviceDict = [NSMutableDictionary dictionaryWithDictionary:savedInputDeviceDict];
	[newInputDeviceDict setObject:[self propertiesDictionary] forKey:[self identifier]];
	
	[[NSUserDefaults standardUserDefaults] setObject:newInputDeviceDict forKey:@"Input_SavedDeviceProperties"];
	[[NSUserDefaults standardUserDefaults] synchronize];
}

- (void) start
{
	IOHIDQueueStart(hidQueueRef);
}

- (void) stop
{
	IOHIDQueueStop(hidQueueRef);
}

- (void) startForceFeedbackAndIterate:(UInt32)iterations flags:(UInt32)ffFlags
{
	if (ffDevice != NULL)
	{
		HRESULT ffResult = FFEffectStart(ffEffect, iterations, ffFlags);
		ffResult = ffResult;
	}
}

- (void) stopForceFeedback
{
	if (ffDevice != NULL)
	{
		FFEffectStop(ffEffect);
	}
}

@end

/********************************************************************************************
	InputAttributesOfHIDValue()

	Parses an IOHIDValueRef into an input attributes struct.

	Takes:
		hidValueRef - The IOHIDValueRef to parse.

	Returns:
		A ClientInputDeviceProperties struct with the parsed input device's properties.

	Details:
		None.
 ********************************************************************************************/
ClientInputDeviceProperties InputAttributesOfHIDValue(IOHIDValueRef hidValueRef)
{
	ClientInputDeviceProperties inputProperty;
	
	if (hidValueRef == NULL)
	{
		return inputProperty;
	}
	
	const IOHIDElementRef hidElementRef = IOHIDValueGetElement(hidValueRef);
	const IOHIDDeviceRef hidDeviceRef = IOHIDElementGetDevice(hidElementRef);
	
	InputElementCodeFromHIDElement(hidElementRef, inputProperty.elementCode);
	InputDeviceCodeFromHIDDevice(hidDeviceRef, inputProperty.deviceCode);
	
	strncpy(inputProperty.elementName, inputProperty.elementCode, INPUT_HANDLER_STRING_LENGTH);
	InputElementNameFromHIDElement(hidElementRef, inputProperty.elementName);
	
	strncpy(inputProperty.deviceName, inputProperty.deviceCode, INPUT_HANDLER_STRING_LENGTH);
	InputDeviceNameFromHIDDevice(hidDeviceRef, inputProperty.deviceName);
		
	const NSInteger logicalValue = IOHIDValueGetIntegerValue(hidValueRef);
	const NSInteger logicalMin = IOHIDElementGetLogicalMin(hidElementRef);
	const NSInteger logicalMax = IOHIDElementGetLogicalMax(hidElementRef);
	const NSInteger elementType = IOHIDElementGetType(hidElementRef);
	const NSInteger elementUsage = IOHIDElementGetUsage(hidElementRef);
	
	inputProperty.isAnalog		= (elementType != kIOHIDElementTypeInput_Button) && !(logicalMin == 0 && logicalMax == 1);
	inputProperty.intCoordX		= 0;
	inputProperty.intCoordY		= 0;
	inputProperty.floatCoordX	= 0.0f;
	inputProperty.floatCoordY	= 0.0f;
	inputProperty.scalar		= (float)(logicalValue - logicalMin) / (float)(logicalMax - logicalMin);
	inputProperty.object		= NULL;
	
	if (!inputProperty.isAnalog)
	{
		inputProperty.state	= (logicalValue == 1) ? ClientInputDeviceState_On : ClientInputDeviceState_Off;
	}
	else if (elementUsage == kHIDUsage_GD_Hatswitch)
	{
		// For hatswitch inputs, use the intCoord fields to store the axis information.
		inputProperty.state = (logicalValue >= logicalMin && logicalValue <= logicalMax) ? ClientInputDeviceState_On : ClientInputDeviceState_Off;
		if (inputProperty.state == ClientInputDeviceState_On)
		{
			struct IntCoord
			{
				int x;
				int y;
			};
			
			static const IntCoord coords4Way[4] = {
				{0, -1},	// Up
				{1, 0},		// Right
				{0, 1},		// Down
				{-1, 0}		// Left
			};
			
			static const IntCoord coords8Way[8] = {
				{0, -1},	// Up
				{1, -1},	// Up/Right
				{1, 0},		// Right
				{1, 1},		// Down/Right
				{0, 1},		// Down
				{-1, 1},	// Down/Left
				{-1, 0},	// Left
				{-1, -1}	// Up/Left
			};
			
			if (logicalMax == 3) // For a 4-way hatswitch
			{
				inputProperty.intCoordX		= coords4Way[logicalValue].x;
				inputProperty.intCoordY		= coords4Way[logicalValue].y;
			}
			else if (logicalMax == 7) // For an 8-way hatswitch
			{
				inputProperty.intCoordX		= coords8Way[logicalValue].x;
				inputProperty.intCoordY		= coords8Way[logicalValue].y;
			}
		}
	}
	else // Some generic analog input
	{
		inputProperty.state	= (inputProperty.scalar <= 0.30f || inputProperty.scalar >= 0.7f) ? ClientInputDeviceState_On : ClientInputDeviceState_Off;
	}
	
	return inputProperty;
}

ClientInputDevicePropertiesList InputListFromHIDValue(IOHIDValueRef hidValueRef, InputManager *inputManager, bool forceDigitalInput)
{
	ClientInputDevicePropertiesList inputPropertyList;
	
	if (hidValueRef == NULL)
	{
		return inputPropertyList;
	}
	
	// IOHIDValueGetIntegerValue() will crash if the value length is too large.
	// Do a bounds check here to prevent crashing. This workaround makes the PS3
	// controller usable, since it returns a length of 39 on some elements.
	if(IOHIDValueGetLength(hidValueRef) > 2)
	{
		return inputPropertyList;
	}
	
	ClientInputDeviceProperties inputProperty = InputAttributesOfHIDValue(hidValueRef);
	if (inputProperty.deviceCode[0] == '\0' || inputProperty.elementCode[0] == '\0')
	{
		return inputPropertyList;
	}
	
	if (!inputProperty.isAnalog)
	{
		inputPropertyList.push_back(inputProperty);
	}
	else
	{
		const IOHIDElementRef hidElementRef = IOHIDValueGetElement(hidValueRef);
		const NSInteger elementUsage = IOHIDElementGetUsage(hidElementRef);
		
		if (elementUsage == kHIDUsage_GD_Hatswitch)
		{
			ClientInputDeviceProperties hatUp = inputProperty;
			hatUp.isAnalog = false;
			strncat(hatUp.elementName, "/Up", 4);
			strncat(hatUp.elementCode, "/Up", 4);
			
			ClientInputDeviceProperties hatRight = inputProperty;
			hatRight.isAnalog = false;
			strncat(hatRight.elementName, "/Right", 7);
			strncat(hatRight.elementCode, "/Right", 7);
			
			ClientInputDeviceProperties hatDown = inputProperty;
			hatDown.isAnalog = false;
			strncat(hatDown.elementName, "/Down", 6);
			strncat(hatDown.elementCode, "/Down", 6);
			
			ClientInputDeviceProperties hatLeft = inputProperty;
			hatLeft.isAnalog = false;
			strncat(hatLeft.elementName, "/Left", 6);
			strncat(hatLeft.elementCode, "/Left", 6);
			
			if (inputProperty.intCoordX == -1)
			{
				hatRight.state = ClientInputDeviceState_Off;
				hatLeft.state = ClientInputDeviceState_On;
			}
			else if (inputProperty.intCoordX == 1)
			{
				hatRight.state = ClientInputDeviceState_On;
				hatLeft.state = ClientInputDeviceState_Off;
			}
			else
			{
				hatRight.state = ClientInputDeviceState_Off;
				hatLeft.state = ClientInputDeviceState_Off;
			}
			
			if (inputProperty.intCoordY == -1)
			{
				hatDown.state = ClientInputDeviceState_Off;
				hatUp.state = ClientInputDeviceState_On;
			}
			else if (inputProperty.intCoordY == 1)
			{
				hatDown.state = ClientInputDeviceState_On;
				hatUp.state = ClientInputDeviceState_Off;
			}
			else
			{
				hatDown.state = ClientInputDeviceState_Off;
				hatUp.state = ClientInputDeviceState_Off;
			}
			
			inputPropertyList.resize(4);
			inputPropertyList.push_back(hatUp);
			inputPropertyList.push_back(hatRight);
			inputPropertyList.push_back(hatDown);
			inputPropertyList.push_back(hatLeft);
		}
		else
		{
			ClientCommandAttributes cmdAttr = [inputManager mappedCommandAttributesOfDeviceCode:inputProperty.deviceCode elementCode:inputProperty.elementCode];
			if (cmdAttr.tag[0] == '\0' || cmdAttr.dispatchFunction == NULL)
			{
				std::string tempElementCode = std::string(inputProperty.elementCode) + "/LowerThreshold";
				cmdAttr = [inputManager mappedCommandAttributesOfDeviceCode:inputProperty.deviceCode elementCode:tempElementCode.c_str()];
				if (cmdAttr.tag[0] == '\0' || cmdAttr.dispatchFunction == NULL)
				{
					tempElementCode = std::string(inputProperty.elementCode) + "/UpperThreshold";
					cmdAttr = [inputManager mappedCommandAttributesOfDeviceCode:inputProperty.deviceCode elementCode:tempElementCode.c_str()];
				}
			}
			
			const bool useAnalog = (cmdAttr.tag[0] == '\0' || cmdAttr.dispatchFunction == NULL) ? !forceDigitalInput : (!forceDigitalInput && cmdAttr.allowAnalogInput);
			
			if (useAnalog)
			{
				inputPropertyList.push_back(inputProperty);
			}
			else
			{
				ClientInputDeviceProperties loInputProperty = inputProperty;
				loInputProperty.isAnalog = false;
				strncat(loInputProperty.elementName, "-", 2);
				strncat(loInputProperty.elementCode, "/LowerThreshold", 16);
				
				ClientInputDeviceProperties hiInputProperty = inputProperty;
				hiInputProperty.isAnalog = false;
				strncat(hiInputProperty.elementName, "+", 2);
				strncat(hiInputProperty.elementCode, "/UpperThreshold", 16);
				
				if (loInputProperty.scalar <= 0.30f)
				{
					loInputProperty.state = ClientInputDeviceState_On;
					hiInputProperty.state = ClientInputDeviceState_Off;
				}
				else if (loInputProperty.scalar >= 0.70f)
				{
					loInputProperty.state = ClientInputDeviceState_Off;
					hiInputProperty.state = ClientInputDeviceState_On;
				}
				else
				{
					loInputProperty.state = ClientInputDeviceState_Off;
					hiInputProperty.state = ClientInputDeviceState_Off;
				}
				
				inputPropertyList.resize(2);
				inputPropertyList.push_back(loInputProperty);
				inputPropertyList.push_back(hiInputProperty);
			}
		}
	}
	
	return inputPropertyList;
}

bool InputElementCodeFromHIDElement(const IOHIDElementRef hidElementRef, char *charBuffer)
{
	NSInteger elementUsagePage = IOHIDElementGetUsagePage(hidElementRef);
	NSInteger elementUsage = IOHIDElementGetUsage(hidElementRef);
	snprintf(charBuffer, INPUT_HANDLER_STRING_LENGTH, "0x%04lX/0x%04lX", (long)elementUsagePage, (long)elementUsage);
	
	return true;
}

bool InputElementNameFromHIDElement(const IOHIDElementRef hidElementRef, char *charBuffer)
{
	CFStringRef cfElementName = IOHIDElementGetName(hidElementRef);
	bool propertyExists = (cfElementName != nil);
	
	if (propertyExists)
	{
		CFStringGetCString(cfElementName, charBuffer, INPUT_HANDLER_STRING_LENGTH, kCFStringEncodingUTF8);
		return propertyExists;
	}
	
	// If the name property is not present, then generate a name ourselves.
	NSInteger elementUsagePage = IOHIDElementGetUsagePage(hidElementRef);
	NSInteger elementUsage = IOHIDElementGetUsage(hidElementRef);
	
	if (elementUsage == kHIDUsage_GD_Hatswitch)
	{
		strncpy(charBuffer, "Hatswitch", INPUT_HANDLER_STRING_LENGTH);
		propertyExists = true;
		return propertyExists;
	}
	else if (elementUsagePage == kHIDPage_Button)
	{
		snprintf(charBuffer, INPUT_HANDLER_STRING_LENGTH, "Button %li", (long)elementUsage);
		propertyExists = true;
		return propertyExists;
	}
	else if (elementUsagePage == kHIDPage_VendorDefinedStart)
	{
		snprintf(charBuffer, INPUT_HANDLER_STRING_LENGTH, "VendorDefined %li", (long)elementUsage);
		propertyExists = true;
		return propertyExists;
	}
	else
	{
		// Only look up the HID Usage Table as a last resort, since this can be relatively slow.
		NSDictionary *elementUsagePageDict = (NSDictionary *)[hidUsageTable valueForKey:[NSString stringWithFormat:@"0x%04lX", (long)elementUsagePage]];
		NSString *elementNameLookup = (NSString *)[elementUsagePageDict valueForKey:[NSString stringWithFormat:@"0x%04lX", (long)elementUsage]];
		propertyExists = (elementNameLookup != nil);
		
		if (propertyExists)
		{
			strncpy(charBuffer, [elementNameLookup cStringUsingEncoding:NSUTF8StringEncoding], INPUT_HANDLER_STRING_LENGTH);
			return propertyExists;
		}
	}
	
	return propertyExists;
}

bool InputDeviceCodeFromHIDDevice(const IOHIDDeviceRef hidDeviceRef, char *charBuffer)
{
	CFNumberRef cfVendorIDNumber = (CFNumberRef)IOHIDDeviceGetProperty(hidDeviceRef, CFSTR(kIOHIDVendorIDKey));
	SInt32 vendorID = 0;
	CFNumberGetValue(cfVendorIDNumber, kCFNumberSInt32Type, &vendorID);
	
	CFNumberRef cfProductIDNumber = (CFNumberRef)IOHIDDeviceGetProperty(hidDeviceRef, CFSTR(kIOHIDProductIDKey));
	SInt32 productID = 0;
	CFNumberGetValue(cfProductIDNumber, kCFNumberSInt32Type, &productID);
	
	CFStringRef cfDeviceCode = (CFStringRef)IOHIDDeviceGetProperty(hidDeviceRef, CFSTR(kIOHIDSerialNumberKey));
	if (cfDeviceCode == nil)
	{
		CFNumberRef cfLocationIDNumber = (CFNumberRef)IOHIDDeviceGetProperty(hidDeviceRef, CFSTR(kIOHIDLocationIDKey));
		SInt32 locationID = 0;
		CFNumberGetValue(cfLocationIDNumber, kCFNumberSInt32Type, &locationID);
		
		snprintf(charBuffer, INPUT_HANDLER_STRING_LENGTH, "%d/%d/0x%08X", (int)vendorID, (int)productID, (unsigned int)locationID);
	}
	else
	{
		char cfDeviceCodeBuf[256] = {0};
		CFStringGetCString(cfDeviceCode, cfDeviceCodeBuf, 256, kCFStringEncodingUTF8);
		snprintf(charBuffer, INPUT_HANDLER_STRING_LENGTH, "%d/%d/%s", (int)vendorID, (int)productID, cfDeviceCodeBuf);
	}
	
	return true;
}

bool InputDeviceNameFromHIDDevice(const IOHIDDeviceRef hidDeviceRef, char *charBuffer)
{
	CFStringRef cfDeviceName = (CFStringRef)IOHIDDeviceGetProperty(hidDeviceRef, CFSTR(kIOHIDProductKey));
	bool propertyExists = (cfDeviceName != nil);
	
	if (propertyExists)
	{
		CFStringGetCString(cfDeviceName, charBuffer, INPUT_HANDLER_STRING_LENGTH, kCFStringEncodingUTF8);
	}
	else
	{
		strncpy(charBuffer, "Unknown Device", INPUT_HANDLER_STRING_LENGTH);
	}
	
	return propertyExists;
}

size_t ClearHIDQueue(const IOHIDQueueRef hidQueue)
{
	size_t hidInputClearCount = 0;
	
	if (hidQueue == nil)
	{
		return hidInputClearCount;
	}
	
	do
	{
		IOHIDValueRef hidValueRef = IOHIDQueueCopyNextValueWithTimeout(hidQueue, 0.0);
		if (hidValueRef == NULL)
		{
			break;
		}
		
		CFRelease(hidValueRef);
		hidInputClearCount++;
	} while (1);
	
	if (hidInputClearCount > 0)
	{
		// HID input devices don't register events, so we need to manually prevent
		// sleep and screensaver whenever we detect an input.
		UpdateSystemActivity(UsrActivity);
	}
	
	return hidInputClearCount;
}

void HandleQueueValueAvailableCallback(void *inContext, IOReturn inResult, void *inSender)
{
	InputHIDDevice *hidDevice = (InputHIDDevice *)inContext;
	InputHIDManager *hidManager = [hidDevice hidManager];
	IOHIDQueueRef hidQueue = (IOHIDQueueRef)inSender;
	id<InputHIDManagerTarget> target = [hidManager target];
	
	if (target != nil)
	{
		NSAutoreleasePool *tempPool = [[NSAutoreleasePool alloc] init];
		[[hidManager target] handleHIDQueue:hidQueue hidManager:hidManager];
		[tempPool release];
	}
	else
	{
		// We must make sure the HID queue is emptied or else HID input will stall.
		ClearHIDQueue(hidQueue);
	}
}

#pragma mark -
@implementation InputHIDManager

@synthesize inputManager;
@synthesize hidManagerRef;
@synthesize deviceListController;
@synthesize target;
@dynamic runLoop;

- (id) initWithInputManager:(InputManager *)theInputManager
{
	self = [super init];
	if(self == nil)
	{
		return self;
	}
	
	target = nil;
	deviceListController = nil;
	inputManager = [theInputManager retain];
	
	hidManagerRef = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
	if (hidManagerRef == NULL)
	{
		[self release];
		return nil;
	}
		
	CFMutableDictionaryRef cfJoystickMatcher = CFDictionaryCreateMutable(kCFAllocatorDefault, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFDictionarySetValue(cfJoystickMatcher, CFSTR(kIOHIDDeviceUsagePageKey), (CFNumberRef)[NSNumber numberWithInteger:kHIDPage_GenericDesktop]);
	CFDictionarySetValue(cfJoystickMatcher, CFSTR(kIOHIDDeviceUsageKey), (CFNumberRef)[NSNumber numberWithInteger:kHIDUsage_GD_Joystick]);
	
	CFMutableDictionaryRef cfGamepadMatcher = CFDictionaryCreateMutable(kCFAllocatorDefault, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFDictionarySetValue(cfGamepadMatcher, CFSTR(kIOHIDDeviceUsagePageKey), (CFNumberRef)[NSNumber numberWithInteger:kHIDPage_GenericDesktop]);
	CFDictionarySetValue(cfGamepadMatcher, CFSTR(kIOHIDDeviceUsageKey), (CFNumberRef)[NSNumber numberWithInteger:kHIDUsage_GD_GamePad]);
	
	CFMutableDictionaryRef cfGenericControllerMatcher = CFDictionaryCreateMutable(kCFAllocatorDefault, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFDictionarySetValue(cfGenericControllerMatcher, CFSTR(kIOHIDDeviceUsagePageKey), (CFNumberRef)[NSNumber numberWithInteger:kHIDPage_GenericDesktop]);
	CFDictionarySetValue(cfGenericControllerMatcher, CFSTR(kIOHIDDeviceUsageKey), (CFNumberRef)[NSNumber numberWithInteger:kHIDUsage_GD_MultiAxisController]);
	
	NSArray *matcherArray = [[NSArray alloc] initWithObjects:(NSMutableDictionary *)cfJoystickMatcher, (NSMutableDictionary *)cfGamepadMatcher, (NSMutableDictionary *)cfGenericControllerMatcher, nil];
	
	IOHIDManagerSetDeviceMatchingMultiple(hidManagerRef, (CFArrayRef)matcherArray);
	
	[matcherArray release];
	CFRelease(cfJoystickMatcher);
	CFRelease(cfGamepadMatcher);
	CFRelease(cfGenericControllerMatcher);
	
	spinlockRunLoop = OS_SPINLOCK_INIT;
	
	IOReturn result = IOHIDManagerOpen(hidManagerRef, kIOHIDOptionsTypeNone);
	if (result != kIOReturnSuccess)
	{
		[self release];
		return nil;
	}
	
	return self;
}

- (void)dealloc
{
	[self setRunLoop:nil];
	[self setDeviceListController:nil];
	[self setInputManager:nil];
	[self setTarget:nil];
		
	if (hidManagerRef != NULL)
	{
		IOHIDManagerClose(hidManagerRef, 0);
		CFRelease(hidManagerRef);
		hidManagerRef = NULL;
	}
	
	[super dealloc];
}

- (void) setRunLoop:(NSRunLoop *)theRunLoop
{
	OSSpinLockLock(&spinlockRunLoop);
	
	if (theRunLoop == runLoop)
	{
		OSSpinLockUnlock(&spinlockRunLoop);
		return;
	}
	
	if (theRunLoop == nil)
	{
		IOHIDManagerRegisterDeviceMatchingCallback(hidManagerRef, NULL, NULL);
		IOHIDManagerRegisterDeviceRemovalCallback(hidManagerRef, NULL, NULL);
		IOHIDManagerUnscheduleFromRunLoop(hidManagerRef, [runLoop getCFRunLoop], kCFRunLoopDefaultMode);
	}
	else
	{
		[theRunLoop retain];
		IOHIDManagerScheduleWithRunLoop(hidManagerRef, [theRunLoop getCFRunLoop], kCFRunLoopDefaultMode);
		IOHIDManagerRegisterDeviceMatchingCallback(hidManagerRef, HandleDeviceMatchingCallback, self);
		IOHIDManagerRegisterDeviceRemovalCallback(hidManagerRef, HandleDeviceRemovalCallback, self);
	}
	
	[runLoop release];
	runLoop = theRunLoop;
	
	OSSpinLockUnlock(&spinlockRunLoop);
}

- (NSRunLoop *) runLoop
{
	OSSpinLockLock(&spinlockRunLoop);
	NSRunLoop *theRunLoop = runLoop;
	OSSpinLockUnlock(&spinlockRunLoop);
	
	return theRunLoop;
}

@end

void HandleDeviceMatchingCallback(void *inContext, IOReturn inResult, void *inSender, IOHIDDeviceRef inIOHIDDeviceRef)
{
	InputHIDManager *hidManager = (InputHIDManager *)inContext;
	InputHIDDevice *newDevice = [[[InputHIDDevice alloc] initWithDevice:inIOHIDDeviceRef hidManager:hidManager] autorelease];
	[[hidManager deviceListController] addObject:newDevice];
	
	NSDictionary *savedInputDeviceDict = [[NSUserDefaults standardUserDefaults] dictionaryForKey:@"Input_SavedDeviceProperties"];
	NSDictionary *devicePropertiesDict = (NSDictionary *)[savedInputDeviceDict objectForKey:[newDevice identifier]];
	
	if (devicePropertiesDict != nil)
	{
		[newDevice setPropertiesUsingDictionary:devicePropertiesDict];
	}
	else
	{
		[newDevice writeDefaults];
	}
	
	[newDevice start];
}

void HandleDeviceRemovalCallback(void *inContext, IOReturn inResult, void *inSender, IOHIDDeviceRef inIOHIDDeviceRef)
{
	InputHIDManager *hidManager = (InputHIDManager *)inContext;
	NSArray *hidDeviceList = [[hidManager deviceListController] arrangedObjects];
	
	for (InputHIDDevice *hidDevice in hidDeviceList)
	{
		if ([hidDevice hidDeviceRef] == inIOHIDDeviceRef)
		{
			[hidDevice stopForceFeedback];
			[[hidManager deviceListController] removeObject:hidDevice];
			break;
		}
	}
}

#pragma mark -
@implementation InputManager

@synthesize emuControl;
@synthesize inputEncoder;
@dynamic hidInputTarget;
@synthesize hidManager;
@synthesize inputMappings;
@synthesize commandTagList;
@synthesize commandIcon;

- (id)init
{
	self = [super init];
	if (self == nil)
	{
		return self;
	}
	
	inputEncoder = new MacInputDevicePropertiesEncoder;
	hidManager = [[InputHIDManager alloc] initWithInputManager:self];
	inputMappings = [[NSMutableDictionary alloc] initWithCapacity:128];
	commandTagList = [[[NSDictionary alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"DefaultKeyMappings" ofType:@"plist"]] valueForKey:@"CommandTagList"];
	
	// Initialize the icons associated with each command.
	commandIcon = [[NSDictionary alloc] initWithObjectsAndKeys:
				   [NSImage imageNamed:@"Icon_DSButtonSelect_420x420"],				@"UNKNOWN COMMAND",
				   [NSImage imageNamed:@"Icon_ArrowUp_420x420"],					@"Up",
				   [NSImage imageNamed:@"Icon_ArrowDown_420x420"],					@"Down",
				   [NSImage imageNamed:@"Icon_ArrowLeft_420x420"],					@"Left",
				   [NSImage imageNamed:@"Icon_ArrowRight_420x420"],					@"Right",
				   [NSImage imageNamed:@"Icon_DSButtonA_420x420"],					@"A",
				   [NSImage imageNamed:@"Icon_DSButtonB_420x420"],					@"B",
				   [NSImage imageNamed:@"Icon_DSButtonX_420x420"],					@"X",
				   [NSImage imageNamed:@"Icon_DSButtonY_420x420"],					@"Y",
				   [NSImage imageNamed:@"Icon_DSButtonL_420x420"],					@"L",
				   [NSImage imageNamed:@"Icon_DSButtonR_420x420"],					@"R",
				   [NSImage imageNamed:@"Icon_DSButtonStart_420x420"],				@"Start",
				   [NSImage imageNamed:@"Icon_DSButtonSelect_420x420"],				@"Select",
				   [NSImage imageNamed:@"Icon_MicrophoneBlueGlow_256x256"],			@"Microphone",
				   [NSImage imageNamed:@"Icon_GuitarGrip_Button_Green_512x512"],	@"Guitar Grip: Green",
				   [NSImage imageNamed:@"Icon_GuitarGrip_Button_Red_512x512"],		@"Guitar Grip: Red",
				   [NSImage imageNamed:@"Icon_GuitarGrip_Button_Yellow_512x512"],	@"Guitar Grip: Yellow",
				   [NSImage imageNamed:@"Icon_GuitarGrip_Button_Blue_512x512"],		@"Guitar Grip: Blue",
				   [NSImage imageNamed:@"Icon_Piano_256x256"],						@"Piano: C",
				   [NSImage imageNamed:@"Icon_Piano_256x256"],						@"Piano: C#",
				   [NSImage imageNamed:@"Icon_Piano_256x256"],						@"Piano: D",
				   [NSImage imageNamed:@"Icon_Piano_256x256"],						@"Piano: D#",
				   [NSImage imageNamed:@"Icon_Piano_256x256"],						@"Piano: E",
				   [NSImage imageNamed:@"Icon_Piano_256x256"],						@"Piano: F",
				   [NSImage imageNamed:@"Icon_Piano_256x256"],						@"Piano: F#",
				   [NSImage imageNamed:@"Icon_Piano_256x256"],						@"Piano: G",
				   [NSImage imageNamed:@"Icon_Piano_256x256"],						@"Piano: G#",
				   [NSImage imageNamed:@"Icon_Piano_256x256"],						@"Piano: A",
				   [NSImage imageNamed:@"Icon_Piano_256x256"],						@"Piano: A#",
				   [NSImage imageNamed:@"Icon_Piano_256x256"],						@"Piano: B",
				   [NSImage imageNamed:@"Icon_Piano_256x256"],						@"Piano: High C",
				   [NSImage imageNamed:@"Icon_PaddleKnob_256x256"],					@"Paddle",
				   [NSImage imageNamed:@"Icon_AutoholdSet_420x420"],				@"Autohold - Set",
				   [NSImage imageNamed:@"Icon_AutoholdClear_420x420"],				@"Autohold - Clear",
				   [NSImage imageNamed:@"Icon_DisplayToggle_420x420"],				@"Toggle All Displays",
				   [NSImage imageNamed:@"Icon_RotateCCW_420x420"],					@"Rotate Display Left",
				   [NSImage imageNamed:@"Icon_RotateCW_420x420"],					@"Rotate Display Right",
				   [NSImage imageNamed:@"Icon_ShowHUD_420x420"],					@"HUD",
				   [NSImage imageNamed:@"Icon_Execute_420x420"],					@"Execute",
				   [NSImage imageNamed:@"Icon_Pause_420x420"],						@"Pause",
				   [NSImage imageNamed:@"Icon_Execute_420x420"],					@"Execute/Pause",
				   [NSImage imageNamed:@"Icon_FrameAdvance_420x420"],				@"Frame Advance",
				   [NSImage imageNamed:@"Icon_FrameJump_420x420"],					@"Frame Jump",
				   [NSImage imageNamed:@"Icon_Reset_420x420"],						@"Reset",
				   [NSImage imageNamed:@"Icon_DSButtonSelect_420x420"],				@"Touch",
				   [NSImage imageNamed:@"Icon_VolumeMute_16x16"],					@"Mute/Unmute",
				   nil];
	
	// Generate the default command attributes for each command tag. (Do this in code rather than in an external file.)
	ClientCommandAttributes cmdDSControlRight					= NewCommandAttributesForDSControl("Right", NDSInputID_Right);
	ClientCommandAttributes cmdDSControlLeft					= NewCommandAttributesForDSControl("Left", NDSInputID_Left);
	ClientCommandAttributes cmdDSControlDown					= NewCommandAttributesForDSControl("Down", NDSInputID_Down);
	ClientCommandAttributes cmdDSControlUp						= NewCommandAttributesForDSControl("Up", NDSInputID_Up);
	ClientCommandAttributes cmdDSControlSelect					= NewCommandAttributesForDSControl("Select", NDSInputID_Select);
	ClientCommandAttributes cmdDSControlStart					= NewCommandAttributesForDSControl("Start", NDSInputID_Start);
	ClientCommandAttributes cmdDSControlB						= NewCommandAttributesForDSControl("B", NDSInputID_B);
	ClientCommandAttributes cmdDSControlA						= NewCommandAttributesForDSControl("A", NDSInputID_A);
	ClientCommandAttributes cmdDSControlY						= NewCommandAttributesForDSControl("Y", NDSInputID_Y);
	ClientCommandAttributes cmdDSControlX						= NewCommandAttributesForDSControl("X", NDSInputID_X);
	ClientCommandAttributes cmdDSControlL						= NewCommandAttributesForDSControl("L", NDSInputID_L);
	ClientCommandAttributes cmdDSControlR						= NewCommandAttributesForDSControl("R", NDSInputID_R);
	ClientCommandAttributes cmdDSControlDebug					= NewCommandAttributesForDSControl("Debug", NDSInputID_Debug);
	ClientCommandAttributes cmdDSControlLid						= NewCommandAttributesForDSControl("Lid", NDSInputID_Lid);
	
	ClientCommandAttributes cmdDSControlTouch					= NewCommandAttributesForDSControl("Touch", NDSInputID_Touch);
	cmdDSControlTouch.useInputForIntCoord = true;
	
	ClientCommandAttributes cmdDSControlMic						= NewCommandAttributesForDSControl("Microphone", NDSInputID_Microphone);
	cmdDSControlMic.intValue[1] = MicrophoneMode_InternalNoise;
	cmdDSControlMic.floatValue[0] = 250.0f;
	
	ClientCommandAttributes cmdGuitarGripGreen					= NewCommandAttributesForDSControl("Guitar Grip: Green", NDSInputID_GuitarGrip_Green);
	ClientCommandAttributes cmdGuitarGripRed					= NewCommandAttributesForDSControl("Guitar Grip: Red", NDSInputID_GuitarGrip_Red);
	ClientCommandAttributes cmdGuitarGripYellow					= NewCommandAttributesForDSControl("Guitar Grip: Yellow", NDSInputID_GuitarGrip_Yellow);
	ClientCommandAttributes cmdGuitarGripBlue					= NewCommandAttributesForDSControl("Guitar Grip: Blue", NDSInputID_GuitarGrip_Blue);
	ClientCommandAttributes cmdPianoC							= NewCommandAttributesForDSControl("Piano: C", NDSInputID_Piano_C);
	ClientCommandAttributes cmdPianoCSharp						= NewCommandAttributesForDSControl("Piano: C#", NDSInputID_Piano_CSharp);
	ClientCommandAttributes cmdPianoD							= NewCommandAttributesForDSControl("Piano: D", NDSInputID_Piano_D);
	ClientCommandAttributes cmdPianoDSharp						= NewCommandAttributesForDSControl("Piano: D#", NDSInputID_Piano_DSharp);
	ClientCommandAttributes cmdPianoE							= NewCommandAttributesForDSControl("Piano: E", NDSInputID_Piano_E);
	ClientCommandAttributes cmdPianoF							= NewCommandAttributesForDSControl("Piano: F", NDSInputID_Piano_F);
	ClientCommandAttributes cmdPianoFSharp						= NewCommandAttributesForDSControl("Piano: F#", NDSInputID_Piano_FSharp);
	ClientCommandAttributes cmdPianoG							= NewCommandAttributesForDSControl("Piano: G", NDSInputID_Piano_G);
	ClientCommandAttributes cmdPianoGSharp						= NewCommandAttributesForDSControl("Piano: G#", NDSInputID_Piano_GSharp);
	ClientCommandAttributes cmdPianoA							= NewCommandAttributesForDSControl("Piano: A", NDSInputID_Piano_A);
	ClientCommandAttributes cmdPianoASharp						= NewCommandAttributesForDSControl("Piano: A#", NDSInputID_Piano_ASharp);
	ClientCommandAttributes cmdPianoB							= NewCommandAttributesForDSControl("Piano: B", NDSInputID_Piano_B);
	ClientCommandAttributes cmdPianoHighC						= NewCommandAttributesForDSControl("Piano: High C", NDSInputID_Piano_HighC);
	
	ClientCommandAttributes cmdPaddle							= NewCommandAttributesForDSControl("Paddle", NDSInputID_Paddle);
	cmdPaddle.allowAnalogInput = true;
	cmdPaddle.intValue[1] = 0;
	cmdPaddle.floatValue[0] = 10.0f;
	
	ClientCommandAttributes cmdAutoholdSet						= NewCommandAttributesWithFunction("Autohold - Set", &ClientCommandAutoholdSet);
	ClientCommandAttributes cmdAutoholdClear					= NewCommandAttributesWithFunction("Autohold - Clear", &ClientCommandAutoholdClear);
	ClientCommandAttributes cmdLoadEmuSaveStateSlot				= NewCommandAttributesWithFunction("Load State Slot", &ClientCommandLoadEmuSaveStateSlot);
	ClientCommandAttributes cmdSaveEmuSaveStateSlot				= NewCommandAttributesWithFunction("Save State Slot", &ClientCommandSaveEmuSaveStateSlot);
	ClientCommandAttributes cmdCopyScreen						= NewCommandAttributesWithFunction("Copy Screen", &ClientCommandCopyScreen);
	
	ClientCommandAttributes cmdRotateDisplayRelative			= NewCommandAttributesWithFunction("Rotate Display Relative", &ClientCommandRotateDisplayRelative);
	cmdRotateDisplayRelative.intValue[0] = 90;
	
	ClientCommandAttributes cmdRotateDisplayLeft				= NewCommandAttributesWithFunction("Rotate Display Left", &ClientCommandRotateDisplayRelative);
	cmdRotateDisplayLeft.intValue[0] = -90;
	
	ClientCommandAttributes cmdRotateDisplayRight				= NewCommandAttributesWithFunction("Rotate Display Right", &ClientCommandRotateDisplayRelative);
	cmdRotateDisplayRight.intValue[0] = 90;
	
	ClientCommandAttributes cmdToggleAllDisplays				= NewCommandAttributesWithFunction("Toggle All Displays", &ClientCommandToggleAllDisplays);
	
	ClientCommandAttributes cmdToggleSpeed						= NewCommandAttributesWithFunction("Set Speed", &ClientCommandHoldToggleSpeedScalar);
	cmdToggleSpeed.floatValue[0] = 1.0f;
	
	ClientCommandAttributes cmdToggleSpeedLimiter				= NewCommandAttributesWithFunction("Enable/Disable Speed Limiter", &ClientCommandToggleSpeedLimiter);
	ClientCommandAttributes cmdToggleAutoFrameSkip				= NewCommandAttributesWithFunction("Enable/Disable Auto Frame Skip", &ClientCommandToggleAutoFrameSkip);
	ClientCommandAttributes cmdToggleCheats						= NewCommandAttributesWithFunction("Enable/Disable Cheats", &ClientCommandToggleCheats);
	ClientCommandAttributes cmdCoreExecute						= NewCommandAttributesWithFunction("Execute", &ClientCommandCoreExecute);
	ClientCommandAttributes cmdCorePause						= NewCommandAttributesWithFunction("Pause", &ClientCommandCorePause);
	ClientCommandAttributes cmdToggleExecutePause				= NewCommandAttributesWithFunction("Execute/Pause", &ClientCommandToggleExecutePause);
	ClientCommandAttributes cmdFrameAdvance						= NewCommandAttributesWithFunction("Frame Advance", &ClientCommandFrameAdvance);
	ClientCommandAttributes cmdFrameJump						= NewCommandAttributesWithFunction("Frame Jump", &ClientCommandFrameJump);
	ClientCommandAttributes cmdReset							= NewCommandAttributesWithFunction("Reset", &ClientCommandReset);
	ClientCommandAttributes cmdToggleMute						= NewCommandAttributesWithFunction("Mute/Unmute", &ClientCommandToggleMute);
	ClientCommandAttributes cmdToggleGPUState					= NewCommandAttributesWithFunction("Enable/Disable GPU State", &ClientCommandToggleGPUState);
	
	defaultCommandAttributes["Up"]								= cmdDSControlUp;
	defaultCommandAttributes["Down"]							= cmdDSControlDown;
	defaultCommandAttributes["Right"]							= cmdDSControlRight;
	defaultCommandAttributes["Left"]							= cmdDSControlLeft;
	defaultCommandAttributes["A"]								= cmdDSControlA;
	defaultCommandAttributes["B"]								= cmdDSControlB;
	defaultCommandAttributes["X"]								= cmdDSControlX;
	defaultCommandAttributes["Y"]								= cmdDSControlY;
	defaultCommandAttributes["L"]								= cmdDSControlL;
	defaultCommandAttributes["R"]								= cmdDSControlR;
	defaultCommandAttributes["Start"]							= cmdDSControlStart;
	defaultCommandAttributes["Select"]							= cmdDSControlSelect;
	defaultCommandAttributes["Touch"]							= cmdDSControlTouch;
	defaultCommandAttributes["Microphone"]						= cmdDSControlMic;
	defaultCommandAttributes["Debug"]							= cmdDSControlDebug;
	defaultCommandAttributes["Lid"]								= cmdDSControlLid;
	
	defaultCommandAttributes["Guitar Grip: Green"]				= cmdGuitarGripGreen;
	defaultCommandAttributes["Guitar Grip: Red"]				= cmdGuitarGripRed;
	defaultCommandAttributes["Guitar Grip: Yellow"]				= cmdGuitarGripYellow;
	defaultCommandAttributes["Guitar Grip: Blue"]				= cmdGuitarGripBlue;
	defaultCommandAttributes["Piano: C"]						= cmdPianoC;
	defaultCommandAttributes["Piano: C#"]						= cmdPianoCSharp;
	defaultCommandAttributes["Piano: D"]						= cmdPianoD;
	defaultCommandAttributes["Piano: D#"]						= cmdPianoDSharp;
	defaultCommandAttributes["Piano: E"]						= cmdPianoE;
	defaultCommandAttributes["Piano: F"]						= cmdPianoF;
	defaultCommandAttributes["Piano: F#"]						= cmdPianoFSharp;
	defaultCommandAttributes["Piano: G"]						= cmdPianoG;
	defaultCommandAttributes["Piano: G#"]						= cmdPianoGSharp;
	defaultCommandAttributes["Piano: A"]						= cmdPianoA;
	defaultCommandAttributes["Piano: A#"]						= cmdPianoASharp;
	defaultCommandAttributes["Piano: B"]						= cmdPianoB;
	defaultCommandAttributes["Piano: High C"]					= cmdPianoHighC;
	defaultCommandAttributes["Paddle"]							= cmdPaddle;
	
	defaultCommandAttributes["Autohold - Set"]					= cmdAutoholdSet;
	defaultCommandAttributes["Autohold - Clear"]				= cmdAutoholdClear;
	defaultCommandAttributes["Load State Slot"]					= cmdLoadEmuSaveStateSlot;
	defaultCommandAttributes["Save State Slot"]					= cmdSaveEmuSaveStateSlot;
	defaultCommandAttributes["Copy Screen"]						= cmdCopyScreen;
	defaultCommandAttributes["Rotate Display Left"]				= cmdRotateDisplayLeft;
	defaultCommandAttributes["Rotate Display Right"]			= cmdRotateDisplayRight;
	defaultCommandAttributes["Toggle All Displays"]				= cmdToggleAllDisplays;
	defaultCommandAttributes["Set Speed"]						= cmdToggleSpeed;
	defaultCommandAttributes["Enable/Disable Speed Limiter"]	= cmdToggleSpeedLimiter;
	defaultCommandAttributes["Enable/Disable Auto Frame Skip"]	= cmdToggleAutoFrameSkip;
	defaultCommandAttributes["Enable/Disable Cheats"]			= cmdToggleCheats;
	defaultCommandAttributes["Execute"]							= cmdCoreExecute;
	defaultCommandAttributes["Pause"]							= cmdCorePause;
	defaultCommandAttributes["Execute/Pause"]					= cmdToggleExecutePause;
	defaultCommandAttributes["Frame Advance"]					= cmdFrameAdvance;
	defaultCommandAttributes["Frame Jump"]						= cmdFrameJump;
	defaultCommandAttributes["Reset"]							= cmdReset;
	defaultCommandAttributes["Mute/Unmute"]						= cmdToggleMute;
	defaultCommandAttributes["Enable/Disable GPU State"]		= cmdToggleGPUState;
	
	// Map all IBActions (the target object is an EmuControllerDelegate)
	[self addMappingForIBAction:@selector(autoholdSet:) commandAttributes:&cmdAutoholdSet];
	[self addMappingForIBAction:@selector(autoholdClear:) commandAttributes:&cmdAutoholdClear];
	[self addMappingForIBAction:@selector(loadEmuSaveStateSlot:) commandAttributes:&cmdLoadEmuSaveStateSlot];
	[self addMappingForIBAction:@selector(saveEmuSaveStateSlot:) commandAttributes:&cmdSaveEmuSaveStateSlot];
	[self addMappingForIBAction:@selector(copy:) commandAttributes:&cmdCopyScreen];
	[self addMappingForIBAction:@selector(toggleAllDisplays:) commandAttributes:&cmdToggleAllDisplays];
	[self addMappingForIBAction:@selector(changeRotationRelative:) commandAttributes:&cmdRotateDisplayRelative];
	[self addMappingForIBAction:@selector(toggleSpeedLimiter:) commandAttributes:&cmdToggleSpeedLimiter];
	[self addMappingForIBAction:@selector(toggleAutoFrameSkip:) commandAttributes:&cmdToggleAutoFrameSkip];
	[self addMappingForIBAction:@selector(toggleCheats:) commandAttributes:&cmdToggleCheats];
	[self addMappingForIBAction:@selector(coreExecute:) commandAttributes:&cmdCoreExecute];
	[self addMappingForIBAction:@selector(corePause:) commandAttributes:&cmdCorePause];
	[self addMappingForIBAction:@selector(toggleExecutePause:) commandAttributes:&cmdToggleExecutePause];
	[self addMappingForIBAction:@selector(frameAdvance:) commandAttributes:&cmdFrameAdvance];
	[self addMappingForIBAction:@selector(frameJump:) commandAttributes:&cmdFrameJump];
	[self addMappingForIBAction:@selector(reset:) commandAttributes:&cmdReset];
	[self addMappingForIBAction:@selector(toggleGPUState:) commandAttributes:&cmdToggleGPUState];
	
	return self;
}

- (void)dealloc
{	
	[hidManager release];
	[inputMappings release];
	[commandTagList release];
	[commandIcon release];
	
	delete inputEncoder;
	
	[super dealloc];
}

#pragma mark Dynamic Properties

- (void) setHidInputTarget:(id<InputHIDManagerTarget>)theTarget
{
	[hidManager setTarget:theTarget];
}

- (id<InputHIDManagerTarget>) hidInputTarget
{
	return [hidManager target];
}

#pragma mark Class Methods

- (void) setMappingsWithMappings:(NSDictionary *)mappings
{
	if (mappings == nil || commandTagList == nil)
	{
		return;
	}
	
	// Add the input mappings, filling in missing data as needed.
	for (NSString *commandTag in commandTagList)
	{
		NSArray *deviceInfoList = (NSArray *)[mappings valueForKey:commandTag];
		
		if ([[self inputMappings] valueForKey:commandTag] == nil)
		{
			[[self inputMappings] setObject:[NSMutableArray arrayWithCapacity:4] forKey:commandTag];
		}
		else
		{
			[self removeAllMappingsForCommandTag:[commandTag cStringUsingEncoding:NSUTF8StringEncoding]];
		}
		
		for (NSDictionary *deviceInfo in deviceInfoList)
		{
			const char *cmdTag = [commandTag cStringUsingEncoding:NSUTF8StringEncoding];
			ClientCommandAttributes cmdAttr = defaultCommandAttributes[cmdTag];
			UpdateCommandAttributesWithDeviceInfoDictionary(&cmdAttr, deviceInfo);
			
			// Force DS control commands to use IDs from code instead of from the file.
			// (In other words, we can't trust an external file with this information since
			// IDs might desync if the DS Control ID enumeration changes.)
			if (cmdAttr.dispatchFunction == &ClientCommandUpdateDSController ||
				cmdAttr.dispatchFunction == &ClientCommandUpdateDSControllerWithTurbo ||
				cmdAttr.dispatchFunction == &ClientCommandUpdateDSTouch ||
				cmdAttr.dispatchFunction == &ClientCommandUpdateDSMicrophone ||
				cmdAttr.dispatchFunction == &ClientCommandUpdateDSPaddle)
			{
				cmdAttr.intValue[0] = defaultCommandAttributes[cmdTag].intValue[0];
			}
			
			if (cmdAttr.dispatchFunction == &ClientCommandUpdateDSControllerWithTurbo)
			{
				if ((cmdAttr.intValue[2] == 0) || (cmdAttr.intValue[3] == 0))
				{
					cmdAttr.intValue[2] = defaultCommandAttributes[cmdTag].intValue[2];
					cmdAttr.intValue[3] = defaultCommandAttributes[cmdTag].intValue[3];
				}
			}
			
			// Copy all command attributes into a new deviceInfo dictionary.
			NSMutableDictionary *newDeviceInfo = DeviceInfoDictionaryWithCommandAttributes(&cmdAttr,
																						   [deviceInfo valueForKey:@"deviceCode"],
																						   [deviceInfo valueForKey:@"deviceName"],
																						   [deviceInfo valueForKey:@"elementCode"],
																						   [deviceInfo valueForKey:@"elementName"]);
			
			[self addMappingUsingDeviceInfoDictionary:newDeviceInfo commandAttributes:&cmdAttr];
		}
	}
	
	[self updateAudioFileGenerators];
}

- (void) addMappingUsingDeviceInfoDictionary:(NSDictionary *)deviceInfo commandAttributes:(const ClientCommandAttributes *)cmdAttr
{
	NSString *deviceCode = (NSString *)[deviceInfo valueForKey:@"deviceCode"];
	NSString *elementCode = (NSString *)[deviceInfo valueForKey:@"elementCode"];
	
	if (deviceCode == nil || elementCode == nil || cmdAttr == NULL)
	{
		return;
	}
	
	[self addMappingUsingDeviceCode:[deviceCode cStringUsingEncoding:NSUTF8StringEncoding]
						elementCode:[elementCode cStringUsingEncoding:NSUTF8StringEncoding]
				  commandAttributes:cmdAttr];
	
	// Save the input device info for the user defaults.
	NSString *commandTagString = [NSString stringWithCString:cmdAttr->tag encoding:NSUTF8StringEncoding];
	NSMutableArray *inputList = (NSMutableArray *)[[self inputMappings] valueForKey:commandTagString];
	NSMutableDictionary *newDeviceInfo = [NSMutableDictionary dictionaryWithDictionary:deviceInfo];
	
	[self updateInputSettingsSummaryInDeviceInfoDictionary:newDeviceInfo commandTag:cmdAttr->tag];
	
	if (inputList == nil)
	{
		inputList = [NSMutableArray arrayWithObject:newDeviceInfo];
		[[self inputMappings] setObject:inputList forKey:commandTagString];
	}
	else
	{
		[inputList addObject:newDeviceInfo];
	}
}

- (void) addMappingUsingInputAttributes:(const ClientInputDeviceProperties *)inputProperty commandAttributes:(const ClientCommandAttributes *)cmdAttr
{
	if (inputProperty == NULL)
	{
		return;
	}
	
	NSMutableDictionary *deviceInfo = DeviceInfoDictionaryWithCommandAttributes(cmdAttr,
																				[NSString stringWithCString:inputProperty->deviceCode encoding:NSUTF8StringEncoding],
																				[NSString stringWithCString:inputProperty->deviceName encoding:NSUTF8StringEncoding],
																				[NSString stringWithCString:inputProperty->elementCode encoding:NSUTF8StringEncoding],
																				[NSString stringWithCString:inputProperty->elementName encoding:NSUTF8StringEncoding]);
	
	[deviceInfo setValue:[NSNumber numberWithBool:(cmdAttr->allowAnalogInput) ? inputProperty->isAnalog : NO] forKey:@"isInputAnalog"];
	[self addMappingUsingDeviceInfoDictionary:deviceInfo commandAttributes:cmdAttr];
}

- (void) addMappingUsingInputList:(const ClientInputDevicePropertiesList *)inputPropertyList commandAttributes:(const ClientCommandAttributes *)cmdAttr
{
	if (inputPropertyList == NULL)
	{
		return;
	}
	
	const size_t inputCount = inputPropertyList->size();
	
	for (size_t i = 0; i < inputCount; i++)
	{
		const ClientInputDeviceProperties &inputProperty = (*inputPropertyList)[i];
		if (inputProperty.state != ClientInputDeviceState_On)
		{
			continue;
		}
		
		[self addMappingUsingInputAttributes:&inputProperty commandAttributes:cmdAttr];
	}
}

- (void) addMappingForIBAction:(const SEL)theSelector commandAttributes:(const ClientCommandAttributes *)cmdAttr
{
	if (theSelector == nil)
	{
		return;
	}
	
	ClientCommandAttributes IBActionCmdAttr = *cmdAttr;
	IBActionCmdAttr.useInputForObject = true;
	
	[self addMappingUsingDeviceCode:"IBAction" elementCode:sel_getName(theSelector) commandAttributes:&IBActionCmdAttr];
}

- (void) addMappingUsingDeviceCode:(const char *)deviceCode elementCode:(const char *)elementCode commandAttributes:(const ClientCommandAttributes *)cmdAttr
{
	if (deviceCode == NULL || elementCode == NULL || cmdAttr == NULL)
	{
		return;
	}
	
	// Remove all previous instances of this particular input device. (We will not be supporting
	// many-to-many mappings at this time.
	[self removeMappingUsingDeviceCode:deviceCode elementCode:elementCode];
	
	// Map the input.
	[self setMappedCommandAttributes:cmdAttr deviceCode:deviceCode elementCode:elementCode];
}

- (void) removeMappingUsingDeviceCode:(const char *)deviceCode elementCode:(const char *)elementCode
{
	if (deviceCode == NULL || elementCode == NULL)
	{
		return;
	}
	
	const std::string inputKey = std::string(deviceCode) + ":" + std::string(elementCode);
	commandMap.erase(inputKey);
	
	for (NSString *inputCommandTag in inputMappings)
	{
		NSMutableArray *inputList = (NSMutableArray *)[inputMappings valueForKey:inputCommandTag];
		NSMutableArray *inputRemovalList = [NSMutableArray arrayWithCapacity:1];
		
		for (NSDictionary *inputDeviceInfo in inputList)
		{
			NSString *deviceCodeString = [NSString stringWithCString:deviceCode encoding:NSUTF8StringEncoding];
			NSString *elementCodeString = [NSString stringWithCString:elementCode encoding:NSUTF8StringEncoding];
			NSString *inputDeviceCode = (NSString *)[inputDeviceInfo valueForKey:@"deviceCode"];
			NSString *inputElementCode = (NSString *)[inputDeviceInfo valueForKey:@"elementCode"];
			
			if ([inputDeviceCode isEqualToString:deviceCodeString] && [inputElementCode isEqualToString:elementCodeString])
			{
				[inputRemovalList addObject:inputDeviceInfo];
			}
		}
		
		[inputList removeObjectsInArray:inputRemovalList];
	}
}

- (void) removeAllMappingsForCommandTag:(const char *)cmdTag
{
	if (cmdTag == NULL)
	{
		return;
	}
	
	// This loop removes all mappings to commandTag, with the exception of IBAction mappings.
	for (InputCommandMap::iterator it=commandMap.begin(); it!=commandMap.end(); ++it)
	{
		if (it->first.find("IBAction") == std::string::npos && strncmp(it->second.tag, cmdTag, INPUT_HANDLER_STRING_LENGTH) == 0)
		{
			commandMap.erase(it);
		}
	}
	
	NSString *commandTag = [NSString stringWithCString:cmdTag encoding:NSUTF8StringEncoding];
	NSMutableArray *inputList = (NSMutableArray *)[[self inputMappings] valueForKey:commandTag];
	[inputList removeAllObjects];
}

- (CommandAttributesList) generateCommandListUsingInputList:(const ClientInputDevicePropertiesList *)inputPropertyList
{
	CommandAttributesList cmdList;
	const size_t inputCount = inputPropertyList->size();
	
	for (size_t i = 0; i < inputCount; i++)
	{
		const ClientInputDeviceProperties &inputProperty = (*inputPropertyList)[i];
		
		// All inputs require a device code and element code for mapping. If one or both are
		// not present, reject the input.
		if (inputProperty.deviceCode[0] == '\0' || inputProperty.elementCode[0] == '\0')
		{
			continue;
		}
		
		// Look up the command attributes using the input key.
		const std::string inputKey	= std::string(inputProperty.deviceCode) + ":" + std::string(inputProperty.elementCode);
		ClientCommandAttributes cmdAttr	= commandMap[inputKey];
		if (cmdAttr.tag[0] == '\0' || cmdAttr.dispatchFunction == NULL)
		{
			continue;
		}
		
		cmdAttr.input = inputProperty; // Copy the input state to the command attributes.
		cmdList.push_back(cmdAttr); // Add the command attributes to the list.
	}
	
	return cmdList;
}

- (void) dispatchCommandList:(const CommandAttributesList *)cmdList
{
	size_t cmdCount = cmdList->size();
	for (size_t i = 0; i < cmdCount; i++)
	{
		const ClientCommandAttributes &cmdAttr = (*cmdList)[i];
		cmdAttr.dispatchFunction(cmdAttr, emuControl);
	}
}

- (BOOL) dispatchCommandUsingInputProperties:(const ClientInputDeviceProperties *)inputProperty
{
	BOOL didCommandDispatch = NO;
	
	// All inputs require a device code and element code for mapping. If one or both are
	// not present, reject the input.
	if (inputProperty->deviceCode[0] == '\0' || inputProperty->elementCode[0] == '\0')
	{
		return didCommandDispatch;
	}
	
	// Look up the command key using the input key.
	const std::string inputKey	= std::string(inputProperty->deviceCode) + ":" + std::string(inputProperty->elementCode);
	ClientCommandAttributes cmdAttr	= commandMap[inputKey];
	if (cmdAttr.tag[0] == '\0' || cmdAttr.dispatchFunction == NULL)
	{
		return didCommandDispatch;
	}
	
	// Copy the input state to the command attributes.
	cmdAttr.input = *inputProperty;
	
	// Call this command's dispatch function.
	cmdAttr.dispatchFunction(cmdAttr, emuControl);
	
	didCommandDispatch = YES;
	return didCommandDispatch;
}

- (BOOL) dispatchCommandUsingIBAction:(const SEL)theSelector sender:(id)sender
{
	const ClientInputDeviceProperties inputProperty = inputEncoder->EncodeIBAction(theSelector, sender);
	return [self dispatchCommandUsingInputProperties:&inputProperty];
}

- (void) writeDefaultsInputMappings
{
	[[NSUserDefaults standardUserDefaults] setObject:[self inputMappings] forKey:@"Input_ControllerMappings"];
	[[NSUserDefaults standardUserDefaults] synchronize];
}

- (NSString *) commandTagFromInputList:(NSArray *)inputList
{
	NSString *commandTag = nil;
	if (inputList == nil)
	{
		return commandTag;
	}
	
	for (NSString *tag in inputMappings)
	{
		if (inputList == [inputMappings valueForKey:tag])
		{
			commandTag = tag;
			break;
		}
	}
	
	return commandTag;
}

- (ClientCommandAttributes) defaultCommandAttributesForCommandTag:(const char *)commandTag
{
	return defaultCommandAttributes[commandTag];
}

- (ClientCommandAttributes) mappedCommandAttributesOfDeviceCode:(const char *)deviceCode elementCode:(const char *)elementCode
{
	const std::string inputKey = std::string(deviceCode) + ":" + std::string(elementCode);
	return commandMap[inputKey];
}

- (void) setMappedCommandAttributes:(const ClientCommandAttributes *)cmdAttr deviceCode:(const char *)deviceCode elementCode:(const char *)elementCode
{
	const std::string inputKey = std::string(deviceCode) + ":" + std::string(elementCode);
	commandMap[inputKey] = *cmdAttr;
}

- (void) updateInputSettingsSummaryInDeviceInfoDictionary:(NSMutableDictionary *)deviceInfo commandTag:(const char *)commandTag
{
	NSString *inputSummary = nil;
	
	if ((strncmp(commandTag, "Up", INPUT_HANDLER_STRING_LENGTH) == 0) ||
		(strncmp(commandTag, "Down", INPUT_HANDLER_STRING_LENGTH) == 0) ||
		(strncmp(commandTag, "Left", INPUT_HANDLER_STRING_LENGTH) == 0) ||
		(strncmp(commandTag, "Right", INPUT_HANDLER_STRING_LENGTH) == 0) ||
		(strncmp(commandTag, "A", INPUT_HANDLER_STRING_LENGTH) == 0) ||
		(strncmp(commandTag, "B", INPUT_HANDLER_STRING_LENGTH) == 0) ||
		(strncmp(commandTag, "X", INPUT_HANDLER_STRING_LENGTH) == 0) ||
		(strncmp(commandTag, "Y", INPUT_HANDLER_STRING_LENGTH) == 0) ||
		(strncmp(commandTag, "L", INPUT_HANDLER_STRING_LENGTH) == 0) ||
		(strncmp(commandTag, "R", INPUT_HANDLER_STRING_LENGTH) == 0) ||
		(strncmp(commandTag, "Start", INPUT_HANDLER_STRING_LENGTH) == 0) ||
		(strncmp(commandTag, "Select", INPUT_HANDLER_STRING_LENGTH) == 0))
	{
		const BOOL isTurbo = [(NSNumber *)[deviceInfo valueForKey:@"intValue1"] boolValue];
		inputSummary = [NSString stringWithFormat:@"Turbo: %@", (isTurbo) ? @"Yes" : @"No"];
	}
	else if (strncmp(commandTag, "Touch", INPUT_HANDLER_STRING_LENGTH) == 0)
	{
		const BOOL useInputForIntCoord = [(NSNumber *)[deviceInfo valueForKey:@"useInputForIntCoord"] boolValue];
		if (useInputForIntCoord)
		{
			inputSummary = NSSTRING_INPUTPREF_USE_DEVICE_COORDINATES;
		}
		else
		{
			const int xCoord = (int)[(NSNumber *)[deviceInfo valueForKey:@"intValue1"] intValue];
			const int yCoord = (int)[(NSNumber *)[deviceInfo valueForKey:@"intValue2"] intValue];
			inputSummary = [NSString stringWithFormat:@"X:%i Y:%i", xCoord, yCoord];
		}
	}
	else if (strncmp(commandTag, "Microphone", INPUT_HANDLER_STRING_LENGTH) == 0)
	{
		const MicrophoneMode micMode = (MicrophoneMode)[(NSNumber *)[deviceInfo valueForKey:@"intValue1"] integerValue];
		switch (micMode)
		{
			case MicrophoneMode_None:
				inputSummary = NSSTRING_INPUTPREF_MIC_NONE;
				break;
				
			case MicrophoneMode_InternalNoise:
				inputSummary = NSSTRING_INPUTPREF_MIC_INTERNAL_NOISE;
				break;
				
			case MicrophoneMode_AudioFile:
				inputSummary = (NSString *)[deviceInfo valueForKey:@"object1"];
				if (inputSummary == nil)
				{
					inputSummary = NSSTRING_INPUTPREF_MIC_AUDIO_FILE_NONE_SELECTED;
				}
				break;
				
			case MicrophoneMode_WhiteNoise:
				inputSummary = NSSTRING_INPUTPREF_MIC_WHITE_NOISE;
				break;
				
			case MicrophoneMode_SineWave:
				inputSummary = [NSString stringWithFormat:NSSTRING_INPUTPREF_MIC_SINE_WAVE, [(NSNumber *)[deviceInfo valueForKey:@"floatValue0"] floatValue]];
				break;
				
			case MicrophoneMode_Physical:
				inputSummary = @"Physical:";
				break;
				
			default:
				break;
		}
	}
	else if (strncmp(commandTag, "Load State Slot", INPUT_HANDLER_STRING_LENGTH) == 0)
	{
		const NSInteger slotNumber = [(NSNumber *)[deviceInfo valueForKey:@"intValue0"] integerValue] + 1;
		inputSummary = [NSString stringWithFormat:NSSTRING_TITLE_SLOT_NUMBER, slotNumber];
	}
	else if (strncmp(commandTag, "Save State Slot", INPUT_HANDLER_STRING_LENGTH) == 0)
	{
		const NSInteger slotNumber = [(NSNumber *)[deviceInfo valueForKey:@"intValue0"] integerValue] + 1;
		inputSummary = [NSString stringWithFormat:NSSTRING_TITLE_SLOT_NUMBER, slotNumber];
	}
	else if (strncmp(commandTag, "Set Speed", INPUT_HANDLER_STRING_LENGTH) == 0)
	{
		const float speedScalar = [(NSNumber *)[deviceInfo valueForKey:@"floatValue0"] floatValue];
		inputSummary = [NSString stringWithFormat:NSSTRING_INPUTPREF_SPEED_SCALAR, speedScalar];
	}
	else if (strncmp(commandTag, "Enable/Disable GPU State", INPUT_HANDLER_STRING_LENGTH) == 0)
	{
		const NSInteger gpuStateID = [(NSNumber *)[deviceInfo valueForKey:@"intValue0"] integerValue];
		switch (gpuStateID)
		{
			case 0:
				inputSummary = NSSTRING_INPUTPREF_GPU_STATE_ALL_MAIN;
				break;
				
			case 1:
				inputSummary = @"Main BG0";
				break;
				
			case 2:
				inputSummary = @"Main BG1";
				break;
				
			case 3:
				inputSummary = @"Main BG2";
				break;
				
			case 4:
				inputSummary = @"Main BG3";
				break;
				
			case 5:
				inputSummary = @"Main OBJ";
				break;
				
			case 6:
				inputSummary = NSSTRING_INPUTPREF_GPU_STATE_ALL_SUB;
				break;
				
			case 7:
				inputSummary = @"Sub BG0";
				break;
				
			case 8:
				inputSummary = @"Sub BG1";
				break;
				
			case 9:
				inputSummary = @"Sub BG2";
				break;
				
			case 10:
				inputSummary = @"Sub BG3";
				break;
				
			case 11:
				inputSummary = @"Sub OBJ";
				break;
				
			default:
				break;
		}
	}
	else if (strncmp(commandTag, "Paddle", INPUT_HANDLER_STRING_LENGTH) == 0)
	{
		const BOOL isInputAnalog = [(NSNumber *)[deviceInfo valueForKey:@"isInputAnalog"] boolValue];
		
		if (isInputAnalog)
		{
			const float paddleSensitivity = [(NSNumber *)[deviceInfo valueForKey:@"floatValue0"] floatValue];
			inputSummary = [NSString stringWithFormat:@"Paddle Sensitivity: %1.1f", paddleSensitivity];
		}
		else
		{
			const NSInteger paddleAdjust = [(NSNumber *)[deviceInfo valueForKey:@"intValue1"] integerValue];
			if (paddleAdjust > 0)
			{
				inputSummary = [NSString stringWithFormat:@"Paddle Adjust: +%ld", (long)paddleAdjust];
			}
			else
			{
				inputSummary = [NSString stringWithFormat:@"Paddle Adjust: %ld", (long)paddleAdjust];
			}
		}
	}
	
	if (inputSummary == nil)
	{
		inputSummary = @"";
	}
	
	[deviceInfo setObject:inputSummary forKey:@"inputSettingsSummary"];
}

- (OSStatus) loadAudioFileUsingPath:(NSString *)filePath
{
	OSStatus error = noErr;
	
	if (filePath == nil)
	{
		error = 1;
		return error;
	}
	
	// Check if the audio file is already loaded. If it is, don't load it again.
	std::string filePathStr = std::string([filePath cStringUsingEncoding:NSUTF8StringEncoding]);
	for (AudioFileSampleGeneratorMap::iterator it=audioFileGenerators.begin(); it!=audioFileGenerators.end(); ++it)
	{
		if (it->first.find(filePathStr) != std::string::npos)
		{
			return error;
		}
	}
	
	// Open the audio file using the file URL.
	NSURL *fileURL = [NSURL fileURLWithPath:filePath isDirectory:NO];
	if (fileURL == nil)
	{
		error = 1;
		return error;
	}
	
	ExtAudioFileRef audioFile;
	error = ExtAudioFileOpenURL((CFURLRef)fileURL, &audioFile);
	if (error != noErr)
	{
		return error;
	}
	
	// Create an ASBD of the DS mic audio format.
	AudioStreamBasicDescription outputFormat;
	outputFormat.mSampleRate = MIC_SAMPLE_RATE;
	outputFormat.mFormatID = kAudioFormatLinearPCM;
	outputFormat.mFormatFlags = kAudioFormatFlagIsPacked;
	outputFormat.mBytesPerPacket = MIC_SAMPLE_SIZE;
	outputFormat.mFramesPerPacket = 1;
	outputFormat.mBytesPerFrame = MIC_SAMPLE_SIZE;
	outputFormat.mChannelsPerFrame = MIC_NUMBER_CHANNELS;
	outputFormat.mBitsPerChannel = MIC_SAMPLE_RESOLUTION;
	
	error = ExtAudioFileSetProperty(audioFile, kExtAudioFileProperty_ClientDataFormat, sizeof(outputFormat), &outputFormat);
	if (error != noErr)
	{
		return error;
	}
	
	AudioStreamBasicDescription inputFormat;
	UInt32 propertySize = sizeof(inputFormat);
	
	error = ExtAudioFileGetProperty(audioFile, kExtAudioFileProperty_FileDataFormat, &propertySize, &inputFormat);
	if (error != noErr)
	{
		return error;
	}
	
	SInt64 fileLengthFrames = 0;
	propertySize = sizeof(fileLengthFrames);
	
	error = ExtAudioFileGetProperty(audioFile, kExtAudioFileProperty_FileLengthFrames, &propertySize, &fileLengthFrames);
	if (error != noErr)
	{
		return error;
	}
	
	// Create a new audio file generator.
	audioFileGenerators[filePathStr] = AudioSampleBlockGenerator();
	AudioSampleBlockGenerator &theGenerator = audioFileGenerators[filePathStr];
	const size_t readSize = 32 * 1024;
	const size_t bufferSize = (size_t)((double)(outputFormat.mSampleRate / inputFormat.mSampleRate) * (double)fileLengthFrames) + readSize;
	uint8_t *buffer = theGenerator.allocate(bufferSize);
	
	// Read the audio file and fill the generator's buffer.
	AudioBufferList convertedData;
	convertedData.mNumberBuffers = 1;
	convertedData.mBuffers[0].mNumberChannels = outputFormat.mChannelsPerFrame;
	convertedData.mBuffers[0].mDataByteSize = readSize;
	convertedData.mBuffers[0].mData = buffer;
	
	UInt32 readFrames = readSize;
	while (readFrames > 0)
	{
		ExtAudioFileRead(audioFile, &readFrames, &convertedData);
		buffer += readFrames;
		convertedData.mBuffers[0].mData = buffer;
	}
	
	// Close the audio file.
	ExtAudioFileDispose(audioFile);
	
	// Convert the audio buffer to 7-bit unsigned PCM.
	buffer = theGenerator.getBuffer();
	for (size_t i = 0; i < bufferSize; i++)
	{
		*(buffer+i) >>= 1;
	}
	
	return error;
}

- (AudioSampleBlockGenerator *) audioFileGeneratorFromFilePath:(NSString *)filePath
{
	BOOL isAudioFileLoaded = NO;
	
	if (filePath == nil)
	{
		return NULL;
	}
	
	std::string filePathStr = std::string([filePath cStringUsingEncoding:NSUTF8StringEncoding]);
	for (AudioFileSampleGeneratorMap::iterator it=audioFileGenerators.begin(); it!=audioFileGenerators.end(); ++it)
	{
		if (it->first.find(filePathStr) != std::string::npos)
		{
			isAudioFileLoaded = YES;
			break;
		}
	}
	
	return (isAudioFileLoaded) ? &audioFileGenerators[filePathStr] : NULL;
}

- (void) updateAudioFileGenerators
{
	NSMutableArray *inputList = (NSMutableArray *)[inputMappings valueForKey:@"Microphone"];
	
	// Load any unloaded audio files
	for (NSMutableArray *deviceInfo in inputList)
	{
		NSString *filePath = (NSString *)[deviceInfo valueForKey:@"object0"];
		[self loadAudioFileUsingPath:filePath];
	}
	
	// Unload any orphaned audio files
	for (AudioFileSampleGeneratorMap::iterator it=audioFileGenerators.begin(); it!=audioFileGenerators.end(); ++it)
	{
		BOOL didFindKey = NO;
		NSString *audioGeneratorKey = [NSString stringWithCString:it->first.c_str() encoding:NSUTF8StringEncoding];
		
		for (NSMutableDictionary *deviceInfo in inputList)
		{
			NSString *deviceAudioFilePath = (NSString *)[deviceInfo valueForKey:@"object0"];
			if ([audioGeneratorKey isEqualToString:deviceAudioFilePath])
			{
				didFindKey = YES;
				break;
			}
		}
		
		if (!didFindKey)
		{
			AudioSampleBlockGenerator *selectedGenerator = [emuControl selectedAudioFileGenerator];
			if (selectedGenerator == &it->second)
			{
				[emuControl setSelectedAudioFileGenerator:NULL];
			}
			
			audioFileGenerators.erase(it);
		}
	}
}

@end

ClientCommandAttributes NewDefaultCommandAttributes(const char *commandTag)
{
	ClientCommandAttributes cmdAttr;
	
	strncpy(cmdAttr.tag, commandTag, INPUT_HANDLER_STRING_LENGTH);
	cmdAttr.dispatchFunction		= NULL;
	cmdAttr.intValue[0]				= 0;
	cmdAttr.intValue[1]				= 0;
	cmdAttr.intValue[2]				= 0;
	cmdAttr.intValue[3]				= 0;
	cmdAttr.floatValue[0]			= 0;
	cmdAttr.floatValue[1]			= 0;
	cmdAttr.floatValue[2]			= 0;
	cmdAttr.floatValue[3]			= 0;
	cmdAttr.object[0]				= NULL;
	cmdAttr.object[1]				= NULL;
	cmdAttr.object[2]				= NULL;
	cmdAttr.object[3]				= NULL;
	
	cmdAttr.useInputForIntCoord		= false;
	cmdAttr.useInputForFloatCoord	= false;
	cmdAttr.useInputForScalar		= false;
	cmdAttr.useInputForObject		= false;
	cmdAttr.allowAnalogInput		= false;
	
	return cmdAttr;
}

ClientCommandAttributes NewCommandAttributesWithFunction(const char *commandTag, const ClientCommandDispatcher commandFunc)
{
	ClientCommandAttributes cmdAttr = NewDefaultCommandAttributes(commandTag);
	cmdAttr.dispatchFunction = commandFunc;
	
	return cmdAttr;
}

ClientCommandAttributes NewCommandAttributesForDSControl(const char *commandTag, const NDSInputID controlID)
{
	ClientCommandAttributes cmdAttr = NewCommandAttributesWithFunction(commandTag, &ClientCommandUpdateDSController);
	
	switch (controlID)
	{
		case NDSInputID_Right:
		case NDSInputID_Left:
		case NDSInputID_Down:
		case NDSInputID_Up:
		case NDSInputID_Select:
		case NDSInputID_Start:
		case NDSInputID_B:
		case NDSInputID_A:
		case NDSInputID_Y:
		case NDSInputID_X:
		case NDSInputID_L:
		case NDSInputID_R:
			cmdAttr.dispatchFunction = &ClientCommandUpdateDSControllerWithTurbo;
			cmdAttr.intValue[2] = 0x33333333;
			cmdAttr.intValue[3] = 4;
			break;
			
		case NDSInputID_Touch:
			cmdAttr.dispatchFunction = &ClientCommandUpdateDSTouch;
			break;
			
		case NDSInputID_Microphone:
			cmdAttr.dispatchFunction = &ClientCommandUpdateDSMicrophone;
			break;
			
		case NDSInputID_Paddle:
			cmdAttr.dispatchFunction = &ClientCommandUpdateDSPaddle;
			break;
			
		default:
			break;
	}
	
	cmdAttr.intValue[0] = controlID;
	cmdAttr.intValue[1] = NO;
	
	return cmdAttr;
}

void UpdateCommandAttributesWithDeviceInfoDictionary(ClientCommandAttributes *cmdAttr, NSDictionary *deviceInfo)
{
	if (cmdAttr == NULL || deviceInfo == nil)
	{
		return;
	}
	
	NSNumber *intValue0				= (NSNumber *)[deviceInfo valueForKey:@"intValue0"];
	NSNumber *intValue1				= (NSNumber *)[deviceInfo valueForKey:@"intValue1"];
	NSNumber *intValue2				= (NSNumber *)[deviceInfo valueForKey:@"intValue2"];
	NSNumber *intValue3				= (NSNumber *)[deviceInfo valueForKey:@"intValue3"];
	NSNumber *floatValue0			= (NSNumber *)[deviceInfo valueForKey:@"floatValue0"];
	NSNumber *floatValue1			= (NSNumber *)[deviceInfo valueForKey:@"floatValue1"];
	NSNumber *floatValue2			= (NSNumber *)[deviceInfo valueForKey:@"floatValue2"];
	NSNumber *floatValue3			= (NSNumber *)[deviceInfo valueForKey:@"floatValue3"];
	NSObject *object0				= [deviceInfo valueForKey:@"object0"];
	NSObject *object1				= [deviceInfo valueForKey:@"object1"];
	NSObject *object2				= [deviceInfo valueForKey:@"object2"];
	NSObject *object3				= [deviceInfo valueForKey:@"object3"];
	NSNumber *useInputForIntCoord	= (NSNumber *)[deviceInfo valueForKey:@"useInputForIntCoord"];
	NSNumber *useInputForFloatCoord	= (NSNumber *)[deviceInfo valueForKey:@"useInputForFloatCoord"];
	NSNumber *useInputForScalar		= (NSNumber *)[deviceInfo valueForKey:@"useInputForScalar"];
	NSNumber *useInputForObject		= (NSNumber *)[deviceInfo valueForKey:@"useInputForSender"];
	NSNumber *isInputAnalog			= (NSNumber *)[deviceInfo valueForKey:@"isInputAnalog"];
	
	if (intValue0 != nil)				cmdAttr->intValue[0] = [intValue0 intValue];
	if (intValue1 != nil)				cmdAttr->intValue[1] = [intValue1 intValue];
	if (intValue2 != nil)				cmdAttr->intValue[2] = [intValue2 intValue];
	if (intValue3 != nil)				cmdAttr->intValue[3] = [intValue3 intValue];
	if (floatValue0 != nil)				cmdAttr->floatValue[0] = [floatValue0 floatValue];
	if (floatValue1 != nil)				cmdAttr->floatValue[1] = [floatValue1 floatValue];
	if (floatValue2 != nil)				cmdAttr->floatValue[2] = [floatValue2 floatValue];
	if (floatValue3 != nil)				cmdAttr->floatValue[3] = [floatValue3 floatValue];				
	if (useInputForIntCoord != nil)		cmdAttr->useInputForIntCoord = [useInputForIntCoord boolValue];
	if (useInputForFloatCoord != nil)	cmdAttr->useInputForFloatCoord = [useInputForFloatCoord boolValue];
	if (useInputForScalar != nil)		cmdAttr->useInputForScalar = [useInputForScalar boolValue];
	if (useInputForObject != nil)		cmdAttr->useInputForObject = [useInputForObject boolValue];
	if (isInputAnalog != nil)			cmdAttr->allowAnalogInput = [isInputAnalog boolValue];
	
	cmdAttr->object[0] = object0;
	cmdAttr->object[1] = object1;
	cmdAttr->object[2] = object2;
	cmdAttr->object[3] = object3;
}

NSMutableDictionary* DeviceInfoDictionaryWithCommandAttributes(const ClientCommandAttributes *cmdAttr,
															   NSString *deviceCode,
															   NSString *deviceName,
															   NSString *elementCode,
															   NSString *elementName)
{
	if (cmdAttr == NULL ||
		deviceCode == nil ||
		deviceName == nil ||
		elementCode == nil ||
		elementName == nil)
	{
		return nil;
	}
	
	NSString *deviceInfoSummary = [[deviceName stringByAppendingString:@": "] stringByAppendingString:elementName];
	
	NSMutableDictionary *newDeviceInfo = [NSMutableDictionary dictionaryWithObjectsAndKeys:
										  deviceCode,													@"deviceCode",
										  deviceName,													@"deviceName",
										  elementCode,													@"elementCode",
										  elementName,													@"elementName",
										  [NSNumber numberWithBool:cmdAttr->allowAnalogInput],			@"isInputAnalog",
										  deviceInfoSummary,											@"deviceInfoSummary",
										  @"",															@"inputSettingsSummary",
										  [NSNumber numberWithInt:cmdAttr->intValue[0]],				@"intValue0",
										  [NSNumber numberWithInt:cmdAttr->intValue[1]],				@"intValue1",
										  [NSNumber numberWithInt:cmdAttr->intValue[2]],				@"intValue2",
										  [NSNumber numberWithInt:cmdAttr->intValue[3]],				@"intValue3",
										  [NSNumber numberWithFloat:cmdAttr->floatValue[0]],			@"floatValue0",
										  [NSNumber numberWithFloat:cmdAttr->floatValue[1]],			@"floatValue1",
										  [NSNumber numberWithFloat:cmdAttr->floatValue[2]],			@"floatValue2",
										  [NSNumber numberWithFloat:cmdAttr->floatValue[3]],			@"floatValue3",
										  [NSNumber numberWithBool:cmdAttr->useInputForIntCoord],		@"useInputForIntCoord",
										  [NSNumber numberWithBool:cmdAttr->useInputForFloatCoord],		@"useInputForFloatCoord",
										  [NSNumber numberWithBool:cmdAttr->useInputForScalar],			@"useInputForScalar",
										  [NSNumber numberWithBool:cmdAttr->useInputForObject],			@"useInputForSender",
										  nil];
	
	// Set the object references last since these could be nil.
	[newDeviceInfo setValue:(id)cmdAttr->object[0] forKey:@"object0"];
	[newDeviceInfo setValue:(id)cmdAttr->object[1] forKey:@"object1"];
	[newDeviceInfo setValue:(id)cmdAttr->object[2] forKey:@"object2"];
	[newDeviceInfo setValue:(id)cmdAttr->object[3] forKey:@"object3"];
	
	return newDeviceInfo;
}

void ClientCommandUpdateDSController(const ClientCommandAttributes &cmdAttr, void *dispatcherObject)
{
	EmuControllerDelegate *emuControl = (EmuControllerDelegate *)dispatcherObject;
	[emuControl cmdUpdateDSController:cmdAttr];
}

void ClientCommandUpdateDSControllerWithTurbo(const ClientCommandAttributes &cmdAttr, void *dispatcherObject)
{
	EmuControllerDelegate *emuControl = (EmuControllerDelegate *)dispatcherObject;
	[emuControl cmdUpdateDSControllerWithTurbo:cmdAttr];
}

void ClientCommandUpdateDSTouch(const ClientCommandAttributes &cmdAttr, void *dispatcherObject)
{
	EmuControllerDelegate *emuControl = (EmuControllerDelegate *)dispatcherObject;
	[emuControl cmdUpdateDSTouch:cmdAttr];
}

void ClientCommandUpdateDSMicrophone(const ClientCommandAttributes &cmdAttr, void *dispatcherObject)
{
	EmuControllerDelegate *emuControl = (EmuControllerDelegate *)dispatcherObject;
	[emuControl cmdUpdateDSMicrophone:cmdAttr];
}

void ClientCommandUpdateDSPaddle(const ClientCommandAttributes &cmdAttr, void *dispatcherObject)
{
	EmuControllerDelegate *emuControl = (EmuControllerDelegate *)dispatcherObject;
	[emuControl cmdUpdateDSPaddle:cmdAttr];
}

void ClientCommandAutoholdSet(const ClientCommandAttributes &cmdAttr, void *dispatcherObject)
{
	EmuControllerDelegate *emuControl = (EmuControllerDelegate *)dispatcherObject;
	[emuControl cmdAutoholdSet:cmdAttr];
}

void ClientCommandAutoholdClear(const ClientCommandAttributes &cmdAttr, void *dispatcherObject)
{
	EmuControllerDelegate *emuControl = (EmuControllerDelegate *)dispatcherObject;
	[emuControl cmdAutoholdClear:cmdAttr];
}

void ClientCommandLoadEmuSaveStateSlot(const ClientCommandAttributes &cmdAttr, void *dispatcherObject)
{
	EmuControllerDelegate *emuControl = (EmuControllerDelegate *)dispatcherObject;
	[emuControl cmdLoadEmuSaveStateSlot:cmdAttr];
}

void ClientCommandSaveEmuSaveStateSlot(const ClientCommandAttributes &cmdAttr, void *dispatcherObject)
{
	EmuControllerDelegate *emuControl = (EmuControllerDelegate *)dispatcherObject;
	[emuControl cmdSaveEmuSaveStateSlot:cmdAttr];
}

void ClientCommandCopyScreen(const ClientCommandAttributes &cmdAttr, void *dispatcherObject)
{
	EmuControllerDelegate *emuControl = (EmuControllerDelegate *)dispatcherObject;
	[emuControl cmdCopyScreen:cmdAttr];
}

void ClientCommandRotateDisplayRelative(const ClientCommandAttributes &cmdAttr, void *dispatcherObject)
{
	EmuControllerDelegate *emuControl = (EmuControllerDelegate *)dispatcherObject;
	[emuControl cmdRotateDisplayRelative:cmdAttr];
}

void ClientCommandToggleAllDisplays(const ClientCommandAttributes &cmdAttr, void *dispatcherObject)
{
	EmuControllerDelegate *emuControl = (EmuControllerDelegate *)dispatcherObject;
	[emuControl cmdToggleAllDisplays:cmdAttr];
}

void ClientCommandHoldToggleSpeedScalar(const ClientCommandAttributes &cmdAttr, void *dispatcherObject)
{
	EmuControllerDelegate *emuControl = (EmuControllerDelegate *)dispatcherObject;
	[emuControl cmdHoldToggleSpeedScalar:cmdAttr];
}

void ClientCommandToggleSpeedLimiter(const ClientCommandAttributes &cmdAttr, void *dispatcherObject)
{
	EmuControllerDelegate *emuControl = (EmuControllerDelegate *)dispatcherObject;
	[emuControl cmdToggleSpeedLimiter:cmdAttr];
}

void ClientCommandToggleAutoFrameSkip(const ClientCommandAttributes &cmdAttr, void *dispatcherObject)
{
	EmuControllerDelegate *emuControl = (EmuControllerDelegate *)dispatcherObject;
	[emuControl cmdToggleAutoFrameSkip:cmdAttr];
}

void ClientCommandToggleCheats(const ClientCommandAttributes &cmdAttr, void *dispatcherObject)
{
	EmuControllerDelegate *emuControl = (EmuControllerDelegate *)dispatcherObject;
	[emuControl cmdToggleCheats:cmdAttr];
}

void ClientCommandToggleExecutePause(const ClientCommandAttributes &cmdAttr, void *dispatcherObject)
{
	EmuControllerDelegate *emuControl = (EmuControllerDelegate *)dispatcherObject;
	[emuControl cmdToggleExecutePause:cmdAttr];
}

void ClientCommandCoreExecute(const ClientCommandAttributes &cmdAttr, void *dispatcherObject)
{
	EmuControllerDelegate *emuControl = (EmuControllerDelegate *)dispatcherObject;
	[emuControl cmdCoreExecute:cmdAttr];
}

void ClientCommandCorePause(const ClientCommandAttributes &cmdAttr, void *dispatcherObject)
{
	EmuControllerDelegate *emuControl = (EmuControllerDelegate *)dispatcherObject;
	[emuControl cmdCorePause:cmdAttr];
}

void ClientCommandFrameAdvance(const ClientCommandAttributes &cmdAttr, void *dispatcherObject)
{
	EmuControllerDelegate *emuControl = (EmuControllerDelegate *)dispatcherObject;
	[emuControl cmdFrameAdvance:cmdAttr];
}

void ClientCommandFrameJump(const ClientCommandAttributes &cmdAttr, void *dispatcherObject)
{
	EmuControllerDelegate *emuControl = (EmuControllerDelegate *)dispatcherObject;
	[emuControl cmdFrameJump:cmdAttr];
}

void ClientCommandReset(const ClientCommandAttributes &cmdAttr, void *dispatcherObject)
{
	EmuControllerDelegate *emuControl = (EmuControllerDelegate *)dispatcherObject;
	[emuControl cmdReset:cmdAttr];
}

void ClientCommandToggleMute(const ClientCommandAttributes &cmdAttr, void *dispatcherObject)
{
	EmuControllerDelegate *emuControl = (EmuControllerDelegate *)dispatcherObject;
	[emuControl cmdToggleMute:cmdAttr];
}

void ClientCommandToggleGPUState(const ClientCommandAttributes &cmdAttr, void *dispatcherObject)
{
	EmuControllerDelegate *emuControl = (EmuControllerDelegate *)dispatcherObject;
	[emuControl cmdToggleGPUState:cmdAttr];
}

MacInputDevicePropertiesEncoder::MacInputDevicePropertiesEncoder()
{
	NSDictionary *keyboardNameDict = [[NSDictionary alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"KeyNames" ofType:@"plist"]];
	NSArray *keyboardDictKeyList = [keyboardNameDict allKeys];
	
	for (NSString *dKey in keyboardDictKeyList)
	{
		int32_t keyCode = (int32_t)[dKey intValue];
		_keyboardNameTable[keyCode] = std::string([(NSString *)[keyboardNameDict valueForKey:dKey] cStringUsingEncoding:NSUTF8StringEncoding]);
	}
}

ClientInputDeviceProperties MacInputDevicePropertiesEncoder::EncodeKeyboardInput(const int32_t keyCode, bool keyPressed)
{
	std::string elementName = this->_keyboardNameTable[keyCode];
	
	ClientInputDeviceProperties inputProperty;
	strncpy(inputProperty.deviceCode, "NSEventKeyboard", INPUT_HANDLER_STRING_LENGTH);
	strncpy(inputProperty.deviceName, "Keyboard", INPUT_HANDLER_STRING_LENGTH);
	snprintf(inputProperty.elementCode, INPUT_HANDLER_STRING_LENGTH, "%i", (int)keyCode);
	strncpy(inputProperty.elementName, (elementName.empty()) ? inputProperty.elementCode : elementName.c_str(), INPUT_HANDLER_STRING_LENGTH);
	
	inputProperty.isAnalog		= false;
	inputProperty.state			= (keyPressed) ? ClientInputDeviceState_On : ClientInputDeviceState_Off;
	inputProperty.intCoordX		= 0;
	inputProperty.intCoordY		= 0;
	inputProperty.floatCoordX	= 0.0f;
	inputProperty.floatCoordY	= 0.0f;
	inputProperty.scalar		= (keyPressed) ? 1.0f : 0.0f;
	inputProperty.object		= NULL;
	
	return inputProperty;
}

ClientInputDeviceProperties MacInputDevicePropertiesEncoder::EncodeMouseInput(const int32_t buttonNumber, float touchLocX, float touchLocY, bool buttonPressed)
{
	ClientInputDeviceProperties inputProperty;
	strncpy(inputProperty.deviceCode, "NSEventMouse", INPUT_HANDLER_STRING_LENGTH);
	strncpy(inputProperty.deviceName, "Mouse", INPUT_HANDLER_STRING_LENGTH);
	snprintf(inputProperty.elementCode, INPUT_HANDLER_STRING_LENGTH, "%i", (int)buttonNumber);
	
	switch (buttonNumber)
	{
		case kCGMouseButtonLeft:
			strncpy(inputProperty.elementName, "Primary Button", INPUT_HANDLER_STRING_LENGTH);
			break;
			
		case kCGMouseButtonRight:
			strncpy(inputProperty.elementName, "Secondary Button", INPUT_HANDLER_STRING_LENGTH);
			break;
			
		case kCGMouseButtonCenter:
			strncpy(inputProperty.elementName, "Center Button", INPUT_HANDLER_STRING_LENGTH);
			break;
			
		default:
			snprintf(inputProperty.elementName, INPUT_HANDLER_STRING_LENGTH, "Button %i", (int)buttonNumber);
			break;
	}
	
	inputProperty.isAnalog		= false;
	inputProperty.state			= (buttonPressed) ? ClientInputDeviceState_On : ClientInputDeviceState_Off;
	inputProperty.intCoordX		= (int32_t)touchLocX;
	inputProperty.intCoordY		= (int32_t)touchLocY;
	inputProperty.floatCoordX	= touchLocX;
	inputProperty.floatCoordY	= touchLocY;
	inputProperty.scalar		= (buttonPressed) ? 1.0f : 0.0f;
	inputProperty.object		= NULL;
	
	return inputProperty;
}

ClientInputDeviceProperties MacInputDevicePropertiesEncoder::EncodeIBAction(const SEL theSelector, id sender)
{
	ClientInputDeviceProperties inputProperty;
	strncpy(inputProperty.deviceCode, "IBAction", INPUT_HANDLER_STRING_LENGTH);
	strncpy(inputProperty.deviceName, "Application", INPUT_HANDLER_STRING_LENGTH);
	strncpy(inputProperty.elementCode, sel_getName(theSelector), INPUT_HANDLER_STRING_LENGTH);
	strncpy(inputProperty.elementName, inputProperty.elementCode, INPUT_HANDLER_STRING_LENGTH);
	
	inputProperty.isAnalog		= false;
	inputProperty.state			= ClientInputDeviceState_On;
	inputProperty.intCoordX		= 0;
	inputProperty.intCoordY		= 0;
	inputProperty.floatCoordX	= 0.0f;
	inputProperty.floatCoordY	= 0.0f;
	inputProperty.scalar		= 1.0f;
	inputProperty.object		= sender;
	
	return inputProperty;
}

ClientInputDevicePropertiesList MacInputDevicePropertiesEncoder::EncodeHIDQueue(const IOHIDQueueRef hidQueue, InputManager *inputManager, bool forceDigitalInput)
{
	ClientInputDevicePropertiesList inputPropertyList;
	if (hidQueue == nil)
	{
		return inputPropertyList;
	}
	
	do
	{
		IOHIDValueRef hidValueRef = IOHIDQueueCopyNextValueWithTimeout(hidQueue, 0.0);
		if (hidValueRef == NULL)
		{
			break;
		}
		
		ClientInputDevicePropertiesList hidInputList = InputListFromHIDValue(hidValueRef, inputManager, forceDigitalInput);
		const size_t hidInputCount = hidInputList.size();
		for (size_t i = 0; i < hidInputCount; i++)
		{
			inputPropertyList.push_back(hidInputList[i]);
		}
		
		CFRelease(hidValueRef);
	} while (1);
	
	if (!inputPropertyList.empty())
	{
		// HID input devices don't register events, so we need to manually prevent
		// sleep and screensaver whenever we detect an input.
		UpdateSystemActivity(UsrActivity);
	}
	
	return inputPropertyList;
}
