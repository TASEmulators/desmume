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

#import "InputManager.h"
#import "EmuControllerDelegate.h"

#import "cocoa_input.h"
#import "cocoa_util.h"

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
	
	spinlockRunLoop = OS_SPINLOCK_INIT;
	[self setRunLoop:[NSRunLoop currentRunLoop]];
	
	return self;
}

- (void)dealloc
{
	[self stop];
	[self setRunLoop:nil];
	[self setHidManager:nil];
	
	if (hidQueueRef != NULL)
	{
		CFRelease(hidQueueRef);
		hidQueueRef = NULL;
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
		IOHIDQueueRegisterValueAvailableCallback(hidQueueRef, NULL, NULL);
		IOHIDQueueUnscheduleFromRunLoop(hidQueueRef, [runLoop getCFRunLoop], kCFRunLoopDefaultMode);
	}
	else
	{
		[theRunLoop retain];
		IOHIDQueueScheduleWithRunLoop(hidQueueRef, [theRunLoop getCFRunLoop], kCFRunLoopDefaultMode);
		IOHIDQueueRegisterValueAvailableCallback(hidQueueRef, HandleQueueValueAvailableCallback, [self hidManager]);
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

- (void) start
{
	IOHIDQueueStart(hidQueueRef);
}

- (void) stop
{
	IOHIDQueueStop(hidQueueRef);
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

@end

/********************************************************************************************
	InputAttributesOfHIDValue()

	Parses an IOHIDValueRef into an input attributes struct.

	Takes:
		hidValueRef - The IOHIDValueRef to parse.
		altElementCode - An NSString that overrides the default element code.
		altElementName - An NSString that overrides the default element name.
		altOnState - An NSNumber that overrides the default on state.

	Returns:
		An InputAttributes struct with the parsed input attributes.

	Details:
		None.
 ********************************************************************************************/
InputAttributes InputAttributesOfHIDValue(IOHIDValueRef hidValueRef, const char *altElementCode, const char *altElementName, NSNumber *altOnState)
{
	InputAttributes inputAttr;
	
	if (hidValueRef == NULL)
	{
		return inputAttr;
	}
	
	char deviceCodeBuf[256] = {0};
	char deviceNameBuf[256] = {0};
	char elementCodeBuf[256] = {0};
	char elementNameBuf[256] = {0};
	
	IOHIDElementRef hidElementRef = IOHIDValueGetElement(hidValueRef);
	NSInteger elementUsagePage = IOHIDElementGetUsagePage(hidElementRef);
	NSInteger elementUsage = IOHIDElementGetUsage(hidElementRef);
	
	if (altElementCode == NULL)
	{
		snprintf(elementCodeBuf, 256, "0x%04lX/0x%04lX", (long)elementUsagePage, (long)elementUsage);
	}
	else
	{
		strncpy(elementCodeBuf, altElementCode, 256);
	}
	
	if (altElementName == NULL)
	{
		CFStringRef cfElementName = IOHIDElementGetName(hidElementRef);
		if (cfElementName == nil)
		{
			if (elementUsagePage == kHIDPage_Button)
			{
				snprintf(elementNameBuf, 256, "Button %li", (long)elementUsage);
			}
			else if (elementUsagePage == kHIDPage_VendorDefinedStart)
			{
				snprintf(elementNameBuf, 256, "VendorDefined %li", (long)elementUsage);
			}
			else
			{
				NSDictionary *elementUsagePageDict = (NSDictionary *)[hidUsageTable valueForKey:[NSString stringWithFormat:@"0x%04lX", (long)elementUsagePage]];
				
				NSString *elementNameLookup = (NSString *)[elementUsagePageDict valueForKey:[NSString stringWithFormat:@"0x%04lX", (long)elementUsage]];
				if (elementNameLookup != nil)
				{
					strncpy(elementNameBuf, [elementNameLookup cStringUsingEncoding:NSUTF8StringEncoding], 256);
				}
			}
		}
		else
		{
			CFStringGetCString(cfElementName, elementNameBuf, 256, kCFStringEncodingUTF8);
		}
	}
	else
	{
		strncpy(elementNameBuf, altElementName, 256);
	}
	
	IOHIDDeviceRef hidDeviceRef = IOHIDElementGetDevice(hidElementRef);
	
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
		
		snprintf(deviceCodeBuf, 256, "%d/%d/0x%08X", (int)vendorID, (int)productID, (unsigned int)locationID);
	}
	else
	{
		char cfDeviceCodeBuf[256] = {0};
		CFStringGetCString(cfDeviceCode, cfDeviceCodeBuf, 256, kCFStringEncodingUTF8);
		snprintf(deviceCodeBuf, 256, "%d/%d/%s", (int)vendorID, (int)productID, cfDeviceCodeBuf);
	}
	
	CFStringRef cfDeviceName = (CFStringRef)IOHIDDeviceGetProperty(hidDeviceRef, CFSTR(kIOHIDProductKey));
	if (cfDeviceName == nil)
	{
		strncpy(deviceNameBuf, deviceCodeBuf, 256);
	}
	else
	{
		CFStringGetCString(cfDeviceName, deviceNameBuf, 256, kCFStringEncodingUTF8);
	}
	
	BOOL onState = (altOnState == nil) ? GetOnStateFromHIDValueRef(hidValueRef) : [altOnState boolValue];
	CFIndex logicalValue = IOHIDValueGetIntegerValue(hidValueRef);
	
	inputAttr.deviceCode = std::string(deviceCodeBuf);
	inputAttr.deviceName = std::string(deviceNameBuf);
	inputAttr.elementCode = std::string(elementCodeBuf);
	inputAttr.elementName = std::string(elementNameBuf);
	inputAttr.inputState = (onState) ? INPUT_ATTRIBUTE_STATE_ON : INPUT_ATTRIBUTE_STATE_OFF;
	inputAttr.integerValue[0] = (NSInteger)logicalValue;
	inputAttr.floatValue[0] = (CGFloat)logicalValue;
	
	return inputAttr;
}

InputAttributesList InputListFromHIDValue(IOHIDValueRef hidValueRef)
{
	InputAttributesList inputList;
	
	if (hidValueRef == NULL)
	{
		return inputList;
	}
	
	IOHIDElementRef hidElementRef = IOHIDValueGetElement(hidValueRef);
	NSInteger elementUsagePage = IOHIDElementGetUsagePage(hidElementRef);
	NSInteger elementUsage = IOHIDElementGetUsage(hidElementRef);
	
	// IOHIDValueGetIntegerValue() will crash if the value length is too large.
	// Do a bounds check here to prevent crashing. This workaround makes the PS3
	// controller usable, since it returns a length of 39 on some elements.
	if(IOHIDValueGetLength(hidValueRef) > 2)
	{
		return inputList;
	}
	
	NSInteger logicalValue = IOHIDValueGetIntegerValue(hidValueRef);
	NSInteger logicalMin = IOHIDElementGetLogicalMin(hidElementRef);
	NSInteger logicalMax = IOHIDElementGetLogicalMax(hidElementRef);
	
	inputList.resize(2);
	
	if (logicalMin == 0 && logicalMax == 1)
	{
		inputList.push_back(InputAttributesOfHIDValue(hidValueRef, NULL, NULL, nil));
	}
	else
	{
		NSInteger lowerThreshold = ((logicalMax - logicalMin) / 3) + logicalMin;
		NSInteger upperThreshold = (((logicalMax - logicalMin) * 2) / 3) + logicalMin;
		NSNumber *onState = [NSNumber numberWithBool:YES];
		NSNumber *offState = [NSNumber numberWithBool:NO];
		
		char elementCodeLowerThresholdBuf[256] = {0};
		char elementCodeUpperThresholdBuf[256] = {0};
		snprintf(elementCodeLowerThresholdBuf, 256, "0x%04lX/0x%04lX/LowerThreshold", (long)elementUsagePage, (long)elementUsage);
		snprintf(elementCodeUpperThresholdBuf, 256, "0x%04lX/0x%04lX/UpperThreshold", (long)elementUsagePage, (long)elementUsage);
		
		if (logicalValue <= lowerThreshold)
		{
			inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeLowerThresholdBuf, NULL, onState));
			inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeUpperThresholdBuf, NULL, offState));
		}
		else if (logicalValue >= upperThreshold)
		{
			inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeLowerThresholdBuf, NULL, offState));
			inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeUpperThresholdBuf, NULL, onState));
		}
		else
		{
			inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeLowerThresholdBuf, NULL, offState));
			inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeUpperThresholdBuf, NULL, onState));
		}
	}
	
	return inputList;
}

InputAttributesList InputListFromHatSwitchValue(IOHIDValueRef hidValueRef, bool useEightDirection)
{
	InputAttributesList inputList;
	
	if (hidValueRef == NULL)
	{
		return inputList;
	}
	
	IOHIDElementRef hidElementRef = IOHIDValueGetElement(hidValueRef);
	NSInteger elementUsagePage = IOHIDElementGetUsagePage(hidElementRef);
	NSInteger elementUsage = IOHIDElementGetUsage(hidElementRef);
	
	if (elementUsage != kHIDUsage_GD_Hatswitch)
	{
		return inputList;
	}
	
	inputList.resize(8);
	NSInteger logicalMax = IOHIDElementGetLogicalMax(hidElementRef);
	NSInteger logicalValue = IOHIDValueGetIntegerValue(hidValueRef);
	
	NSNumber *onState = [NSNumber numberWithBool:YES];
	NSNumber *offState = [NSNumber numberWithBool:NO];
	
	char elementCodeFourWay[4][256];
	for (unsigned int i = 0; i < 4; i++)
	{
		snprintf(elementCodeFourWay[i], 256, "0x%04lX/0x%04lX/%d-FourDirection", (long)elementUsagePage, (long)elementUsage, i);
	}
	
	const char *elementNameFourWay[4] = {
		"Hatswitch - Up",
		"Hatswitch - Right",
		"Hatswitch - Down",
		"Hatswitch - Left" };
	
	char elementCodeEightWay[8][256];
	for (unsigned int i = 0; i < 8; i++)
	{
		snprintf(elementCodeEightWay[i], 256, "0x%04lX/0x%04lX/%d-EightDirection", (long)elementUsagePage, (long)elementUsage, i);
	}
	
	const char *elementNameEightWay[8] = {
		"Hatswitch - Up",
		"Hatswitch - Up/Right",
		"Hatswitch - Right",
		"Hatswitch - Down/Right",
		"Hatswitch - Down",
		"Hatswitch - Down/Left",
		"Hatswitch - Left",
		"Hatswitch - Up/Left" };
	
	if (logicalMax == 3)
	{
		for (unsigned int i = 0; i <= (unsigned int)logicalMax; i++)
		{
			inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeFourWay[i], elementNameFourWay[i], (i == (unsigned int)logicalValue) ? onState : offState));
		}
	}
	else if (logicalMax == 7)
	{
		if (useEightDirection)
		{
			for (unsigned int i = 0; i <= (unsigned int)logicalMax; i++)
			{
				inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[i], elementNameEightWay[i], (i == (unsigned int)logicalValue) ? onState : offState));
			}
		}
		else
		{
			switch (logicalValue)
			{
				case 0:
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[0], elementNameEightWay[0], onState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[2], elementNameEightWay[2], offState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[4], elementNameEightWay[4], offState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[6], elementNameEightWay[6], offState));
					break;
					
				case 1:
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[0], elementNameEightWay[0], onState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[2], elementNameEightWay[2], onState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[4], elementNameEightWay[4], offState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[6], elementNameEightWay[6], offState));
					break;
					
				case 2:
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[0], elementNameEightWay[0], offState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[2], elementNameEightWay[2], onState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[4], elementNameEightWay[4], offState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[6], elementNameEightWay[6], offState));
					break;
					
				case 3:
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[0], elementNameEightWay[0], offState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[2], elementNameEightWay[2], onState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[4], elementNameEightWay[4], onState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[6], elementNameEightWay[6], offState));
					break;
					
				case 4:
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[0], elementNameEightWay[0], offState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[2], elementNameEightWay[2], offState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[4], elementNameEightWay[4], onState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[6], elementNameEightWay[6], offState));
					break;
					
				case 5:
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[0], elementNameEightWay[0], offState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[2], elementNameEightWay[2], offState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[4], elementNameEightWay[4], onState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[6], elementNameEightWay[6], onState));
					break;
					
				case 6:
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[0], elementNameEightWay[0], offState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[2], elementNameEightWay[2], offState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[4], elementNameEightWay[4], offState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[6], elementNameEightWay[6], onState));
					break;
					
				case 7:
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[0], elementNameEightWay[0], onState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[2], elementNameEightWay[2], offState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[4], elementNameEightWay[4], offState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[6], elementNameEightWay[6], onState));
					break;
					
				default:
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[0], elementNameEightWay[0], offState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[2], elementNameEightWay[2], offState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[4], elementNameEightWay[4], offState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[6], elementNameEightWay[6], offState));
					break;
			}
		}
	}
	
	return inputList;
}

BOOL GetOnStateFromHIDValueRef(IOHIDValueRef hidValueRef)
{
	BOOL onState = NO;
	
	if (hidValueRef == nil)
	{
		return onState;
	}
	
	IOHIDElementRef hidElementRef = IOHIDValueGetElement(hidValueRef);
	NSInteger logicalValue = IOHIDValueGetIntegerValue(hidValueRef);
	NSInteger logicalMin = IOHIDElementGetLogicalMin(hidElementRef);
	NSInteger logicalMax = IOHIDElementGetLogicalMax(hidElementRef);
	NSInteger lowerThreshold = ((logicalMax - logicalMin) / 4) + logicalMin;
	NSInteger upperThreshold = (((logicalMax - logicalMin) * 3) / 4) + logicalMin;
	
	NSInteger elementType = IOHIDElementGetType(hidElementRef);
	switch (elementType)
	{
		case kIOHIDElementTypeInput_Misc:
		{
			if (logicalMin == 0 && logicalMax == 1)
			{
				if (logicalValue == 1)
				{
					onState = YES;
				}
			}
			else
			{
				if (logicalValue <= lowerThreshold || logicalValue >= upperThreshold)
				{
					onState = YES;
				}
			}
			break;
		}
			
		case kIOHIDElementTypeInput_Button:
		{
			if (logicalValue == 1)
			{
				onState = YES;
			}
			break;
		}
			
		case kIOHIDElementTypeInput_Axis:
		{
			if (logicalMin == 0 && logicalMax == 1)
			{
				if (logicalValue == 1)
				{
					onState = YES;
				}
			}
			else
			{
				if (logicalValue <= lowerThreshold || logicalValue >= upperThreshold)
				{
					onState = YES;
				}
			}
			break;
		}
			
		default:
			break;
	}
	
	return onState;
}

unsigned int ClearHIDQueue(const IOHIDQueueRef hidQueue)
{
	unsigned int hidInputClearCount = 0;
	
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
	InputHIDManager *hidManager = (InputHIDManager *)inContext;
	IOHIDQueueRef hidQueue = (IOHIDQueueRef)inSender;
	id<InputHIDManagerTarget> target = [hidManager target];
	
	if (target != nil)
	{
		[[hidManager target] handleHIDQueue:hidQueue];
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
@synthesize deviceList;
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
	inputManager = [theInputManager retain];
	
	hidManagerRef = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
	if (hidManagerRef == NULL)
	{
		[self release];
		return nil;
	}
	
	deviceList = [[NSMutableSet alloc] initWithCapacity:32];
	
	CFMutableDictionaryRef cfJoystickMatcher = CFDictionaryCreateMutable(kCFAllocatorDefault, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFDictionarySetValue(cfJoystickMatcher, CFSTR(kIOHIDDeviceUsagePageKey), (CFNumberRef)[NSNumber numberWithInteger:kHIDPage_GenericDesktop]);
	CFDictionarySetValue(cfJoystickMatcher, CFSTR(kIOHIDDeviceUsageKey), (CFNumberRef)[NSNumber numberWithInteger:kHIDUsage_GD_Joystick]);
	
	CFMutableDictionaryRef cfGamepadMatcher = CFDictionaryCreateMutable(kCFAllocatorDefault, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFDictionarySetValue(cfGamepadMatcher, CFSTR(kIOHIDDeviceUsagePageKey), (CFNumberRef)[NSNumber numberWithInteger:kHIDPage_GenericDesktop]);
	CFDictionarySetValue(cfGamepadMatcher, CFSTR(kIOHIDDeviceUsageKey), (CFNumberRef)[NSNumber numberWithInteger:kHIDUsage_GD_GamePad]);
	
	NSArray *matcherArray = [[NSArray alloc] initWithObjects:(NSMutableDictionary *)cfJoystickMatcher, (NSMutableDictionary *)cfGamepadMatcher, nil];
	
	IOHIDManagerSetDeviceMatchingMultiple(hidManagerRef, (CFArrayRef)matcherArray);
	
	[matcherArray release];
	CFRelease(cfJoystickMatcher);
	CFRelease(cfGamepadMatcher);
	
	spinlockRunLoop = OS_SPINLOCK_INIT;
	[self setRunLoop:[NSRunLoop currentRunLoop]];
	
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
	[self setInputManager:nil];
	[self setTarget:nil];
	
	[deviceList release];
	
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
	[[hidManager deviceList] addObject:newDevice];
	[newDevice start];
}

void HandleDeviceRemovalCallback(void *inContext, IOReturn inResult, void *inSender, IOHIDDeviceRef inIOHIDDeviceRef)
{
	InputHIDManager *hidManager = (InputHIDManager *)inContext;
	
	for (InputHIDDevice *hidDevice in [hidManager deviceList])
	{
		if ([hidDevice hidDeviceRef] == inIOHIDDeviceRef)
		{
			[[hidManager deviceList] removeObject:hidDevice];
			break;
		}
	}
}

#pragma mark -
@implementation InputManager

@synthesize emuControl;
@dynamic hidInputTarget;

static std::tr1::unordered_map<unsigned short, std::string> keyboardNameTable; // Key = Key code, Value = Key name

- (id)init
{
	self = [super init];
	if (self == nil)
	{
		return self;
	}
	
	if (keyboardNameTable.empty())
	{
		NSDictionary *keyboardNameDict = [[NSDictionary alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"KeyNames" ofType:@"plist"]];
		NSArray *keyboardDictKeyList = [keyboardNameDict allKeys];
		
		for (NSString *dKey in keyboardDictKeyList)
		{
			unsigned short keyCode = (unsigned short)[dKey intValue];
			keyboardNameTable[keyCode] = std::string([(NSString *)[keyboardNameDict valueForKey:dKey] cStringUsingEncoding:NSUTF8StringEncoding]);
		}
	}
	
	hidManager = [[InputHIDManager alloc] initWithInputManager:self];
	
	// Initialize the master command list.
	CommandAttributes cmdDSControlRight		= {@selector(cmdUpdateDSController:), DSControllerState_Right, INPUT_ATTRIBUTE_STATE_OFF, {0, 0, 0, 0}, {NO, NO, NO, NO}, {0.0f, 0.0f, 0.0f, 0.0f}, {NO, NO, NO, NO}, {NULL, NULL, NULL, NULL}, {NO, NO, NO, NO}};
	CommandAttributes cmdDSControlLeft		= {@selector(cmdUpdateDSController:), DSControllerState_Left, INPUT_ATTRIBUTE_STATE_OFF, {0, 0, 0, 0}, {NO, NO, NO, NO}, {0.0f, 0.0f, 0.0f, 0.0f}, {NO, NO, NO, NO}, {NULL, NULL, NULL, NULL}, {NO, NO, NO, NO}};
	CommandAttributes cmdDSControlDown		= {@selector(cmdUpdateDSController:), DSControllerState_Down, INPUT_ATTRIBUTE_STATE_OFF, {0, 0, 0, 0}, {NO, NO, NO, NO}, {0.0f, 0.0f, 0.0f, 0.0f}, {NO, NO, NO, NO}, {NULL, NULL, NULL, NULL}, {NO, NO, NO, NO}};
	CommandAttributes cmdDSControlUp		= {@selector(cmdUpdateDSController:), DSControllerState_Up, INPUT_ATTRIBUTE_STATE_OFF, {0, 0, 0, 0}, {NO, NO, NO, NO}, {0.0f, 0.0f, 0.0f, 0.0f}, {NO, NO, NO, NO}, {NULL, NULL, NULL, NULL}, {NO, NO, NO, NO}};
	CommandAttributes cmdDSControlSelect	= {@selector(cmdUpdateDSController:), DSControllerState_Select, INPUT_ATTRIBUTE_STATE_OFF, {0, 0, 0, 0}, {NO, NO, NO, NO}, {0.0f, 0.0f, 0.0f, 0.0f}, {NO, NO, NO, NO}, {NULL, NULL, NULL, NULL}, {NO, NO, NO, NO}};
	CommandAttributes cmdDSControlStart		= {@selector(cmdUpdateDSController:), DSControllerState_Start, INPUT_ATTRIBUTE_STATE_OFF, {0, 0, 0, 0}, {NO, NO, NO, NO}, {0.0f, 0.0f, 0.0f, 0.0f}, {NO, NO, NO, NO}, {NULL, NULL, NULL, NULL}, {NO, NO, NO, NO}};
	CommandAttributes cmdDSControlB			= {@selector(cmdUpdateDSController:), DSControllerState_B, INPUT_ATTRIBUTE_STATE_OFF, {0, 0, 0, 0}, {NO, NO, NO, NO}, {0.0f, 0.0f, 0.0f, 0.0f}, {NO, NO, NO, NO}, {NULL, NULL, NULL, NULL}, {NO, NO, NO, NO}};
	CommandAttributes cmdDSControlA			= {@selector(cmdUpdateDSController:), DSControllerState_A, INPUT_ATTRIBUTE_STATE_OFF, {0, 0, 0, 0}, {NO, NO, NO, NO}, {0.0f, 0.0f, 0.0f, 0.0f}, {NO, NO, NO, NO}, {NULL, NULL, NULL, NULL}, {NO, NO, NO, NO}};
	CommandAttributes cmdDSControlY			= {@selector(cmdUpdateDSController:), DSControllerState_Y, INPUT_ATTRIBUTE_STATE_OFF, {0, 0, 0, 0}, {NO, NO, NO, NO}, {0.0f, 0.0f, 0.0f, 0.0f}, {NO, NO, NO, NO}, {NULL, NULL, NULL, NULL}, {NO, NO, NO, NO}};
	CommandAttributes cmdDSControlX			= {@selector(cmdUpdateDSController:), DSControllerState_X, INPUT_ATTRIBUTE_STATE_OFF, {0, 0, 0, 0}, {NO, NO, NO, NO}, {0.0f, 0.0f, 0.0f, 0.0f}, {NO, NO, NO, NO}, {NULL, NULL, NULL, NULL}, {NO, NO, NO, NO}};
	CommandAttributes cmdDSControlL			= {@selector(cmdUpdateDSController:), DSControllerState_L, INPUT_ATTRIBUTE_STATE_OFF, {0, 0, 0, 0}, {NO, NO, NO, NO}, {0.0f, 0.0f, 0.0f, 0.0f}, {NO, NO, NO, NO}, {NULL, NULL, NULL, NULL}, {NO, NO, NO, NO}};
	CommandAttributes cmdDSControlR			= {@selector(cmdUpdateDSController:), DSControllerState_R, INPUT_ATTRIBUTE_STATE_OFF, {0, 0, 0, 0}, {NO, NO, NO, NO}, {0.0f, 0.0f, 0.0f, 0.0f}, {NO, NO, NO, NO}, {NULL, NULL, NULL, NULL}, {NO, NO, NO, NO}};
	CommandAttributes cmdDSControlDebug		= {@selector(cmdUpdateDSController:), DSControllerState_Debug, INPUT_ATTRIBUTE_STATE_OFF, {0, 0, 0, 0}, {NO, NO, NO, NO}, {0.0f, 0.0f, 0.0f, 0.0f}, {NO, NO, NO, NO}, {NULL, NULL, NULL, NULL}, {NO, NO, NO, NO}};
	CommandAttributes cmdDSControlLid		= {@selector(cmdUpdateDSController:), DSControllerState_Lid, INPUT_ATTRIBUTE_STATE_OFF, {0, 0, 0, 0}, {NO, NO, NO, NO}, {0.0f, 0.0f, 0.0f, 0.0f}, {NO, NO, NO, NO}, {NULL, NULL, NULL, NULL}, {NO, NO, NO, NO}};
	CommandAttributes cmdDSControlTouch		= {@selector(cmdUpdateDSController:), DSControllerState_Touch, INPUT_ATTRIBUTE_STATE_OFF, {0, 0, 0, 0}, {NO, NO, NO, NO}, {0.0f, 0.0f, 0.0f, 0.0f}, {YES, YES, NO, NO}, {NULL, NULL, NULL, NULL}, {NO, NO, NO, NO}};
	CommandAttributes cmdDSControlMic		= {@selector(cmdUpdateDSController:), DSControllerState_Microphone, INPUT_ATTRIBUTE_STATE_OFF, {0, 0, 0, 0}, {NO, NO, NO, NO}, {0.0f, 0.0f, 0.0f, 0.0f}, {NO, NO, NO, NO}, {NULL, NULL, NULL, NULL}, {NO, NO, NO, NO}};
	
	CommandAttributes cmdLoadEmuSaveStateSlot = {@selector(cmdLoadEmuSaveStateSlot:), 0, INPUT_ATTRIBUTE_STATE_OFF, {0, 0, 0, 0}, {YES, NO, NO, NO}, {0.0f, 0.0f, 0.0f, 0.0f}, {NO, NO, NO, NO}, {NULL, NULL, NULL, NULL}, {NO, NO, NO, NO}};
	CommandAttributes cmdSaveEmuSaveStateSlot = {@selector(cmdSaveEmuSaveStateSlot:), 0, INPUT_ATTRIBUTE_STATE_OFF, {0, 0, 0, 0}, {YES, NO, NO, NO}, {0.0f, 0.0f, 0.0f, 0.0f}, {NO, NO, NO, NO}, {NULL, NULL, NULL, NULL}, {NO, NO, NO, NO}};
	CommandAttributes cmdCopyScreen			= {@selector(cmdCopyScreen:), 0, INPUT_ATTRIBUTE_STATE_OFF, {0, 0, 0, 0}, {NO, NO, NO, NO}, {0.0f, 0.0f, 0.0f, 0.0f}, {NO, NO, NO, NO}, {NULL, NULL, NULL, NULL}, {NO, NO, NO, NO}};
	CommandAttributes cmdToggleSpeedHalf	= {@selector(cmdToggleSpeedScalar:), 0, INPUT_ATTRIBUTE_STATE_OFF, {0, 0, 0, 0}, {NO, NO, NO, NO}, {0.5f, 0.0f, 0.0f, 0.0f}, {NO, NO, NO, NO}, {NULL, NULL, NULL, NULL}, {NO, NO, NO, NO}};
	CommandAttributes cmdToggleSpeedDouble	= {@selector(cmdToggleSpeedScalar:), 0, INPUT_ATTRIBUTE_STATE_OFF, {0, 0, 0, 0}, {NO, NO, NO, NO}, {2.0f, 0.0f, 0.0f, 0.0f}, {NO, NO, NO, NO}, {NULL, NULL, NULL, NULL}, {NO, NO, NO, NO}};
	CommandAttributes cmdToggleSpeedLimiter	= {@selector(cmdToggleSpeedLimiter:), 0, INPUT_ATTRIBUTE_STATE_OFF, {0, 0, 0, 0}, {NO, NO, NO, NO}, {0.0f, 0.0f, 0.0f, 0.0f}, {NO, NO, NO, NO}, {NULL, NULL, NULL, NULL}, {NO, NO, NO, NO}};
	CommandAttributes cmdToggleAutoFrameSkip = {@selector(cmdToggleAutoFrameSkip:), 0, INPUT_ATTRIBUTE_STATE_OFF, {0, 0, 0, 0}, {NO, NO, NO, NO}, {0.0f, 0.0f, 0.0f, 0.0f}, {NO, NO, NO, NO}, {NULL, NULL, NULL, NULL}, {NO, NO, NO, NO}};
	CommandAttributes cmdToggleCheats		= {@selector(cmdToggleCheats:), 0, INPUT_ATTRIBUTE_STATE_OFF, {0, 0, 0, 0}, {NO, NO, NO, NO}, {0.0f, 0.0f, 0.0f, 0.0f}, {NO, NO, NO, NO}, {NULL, NULL, NULL, NULL}, {NO, NO, NO, NO}};
	CommandAttributes cmdToggleExecutePause	= {@selector(cmdToggleExecutePause:), 0, INPUT_ATTRIBUTE_STATE_OFF, {0, 0, 0, 0}, {NO, NO, NO, NO}, {0.0f, 0.0f, 0.0f, 0.0f}, {NO, NO, NO, NO}, {NULL, NULL, NULL, NULL}, {NO, NO, NO, NO}};
	CommandAttributes cmdReset				= {@selector(cmdReset:), 0, INPUT_ATTRIBUTE_STATE_OFF, {0, 0, 0, 0}, {NO, NO, NO, NO}, {0.0f, 0.0f, 0.0f, 0.0f}, {NO, NO, NO, NO}, {NULL, NULL, NULL, NULL}, {NO, NO, NO, NO}};
	CommandAttributes cmdToggleGPUState		= {@selector(cmdToggleGPUState:), 0, INPUT_ATTRIBUTE_STATE_OFF, {0, 0, 0, 0}, {YES, NO, NO, NO}, {0.0f, 0.0f, 0.0f, 0.0f}, {NO, NO, NO, NO}, {NULL, NULL, NULL, NULL}, {NO, NO, NO, NO}};
	
	commandMasterList["Up"]			= cmdDSControlUp;
	commandMasterList["Down"]		= cmdDSControlDown;
	commandMasterList["Right"]		= cmdDSControlRight;
	commandMasterList["Left"]		= cmdDSControlLeft;
	commandMasterList["A"]			= cmdDSControlA;
	commandMasterList["B"]			= cmdDSControlB;
	commandMasterList["X"]			= cmdDSControlX;
	commandMasterList["Y"]			= cmdDSControlY;
	commandMasterList["L"]			= cmdDSControlL;
	commandMasterList["R"]			= cmdDSControlR;
	commandMasterList["Start"]		= cmdDSControlStart;
	commandMasterList["Select"]		= cmdDSControlSelect;
	commandMasterList["Touch"]		= cmdDSControlTouch;
	commandMasterList["Microphone"]	= cmdDSControlMic;
	commandMasterList["Debug"]		= cmdDSControlDebug;
	commandMasterList["Lid"]		= cmdDSControlLid;
	
	commandMasterList["Load State Slot"] = cmdLoadEmuSaveStateSlot;
	commandMasterList["Save State Slot"] = cmdSaveEmuSaveStateSlot;
	commandMasterList["Copy Screen"]	= cmdCopyScreen;
	commandMasterList["Speed Half"]		= cmdToggleSpeedHalf;
	commandMasterList["Speed Double"]	= cmdToggleSpeedDouble;
	commandMasterList["Enable/Disable Speed Limiter"] = cmdToggleSpeedLimiter;
	commandMasterList["Enable/Disable Auto Frame Skip"] = cmdToggleAutoFrameSkip;
	commandMasterList["Enable/Disable Cheats"] = cmdToggleCheats;
	commandMasterList["Execute/Pause"]	= cmdToggleExecutePause;
	commandMasterList["Reset"]			= cmdReset;
	commandMasterList["Enable/Disable GPU State"] = cmdToggleGPUState;
	
	[self addMappingForIBAction:@selector(loadEmuSaveStateSlot:) commandTag:"Load State Slot"];
	[self addMappingForIBAction:@selector(saveEmuSaveStateSlot:) commandTag:"Save State Slot"];
	[self addMappingForIBAction:@selector(toggleSpeedHalf:) commandTag:"Speed Half"];
	[self addMappingForIBAction:@selector(toggleSpeedDouble:) commandTag:"Speed Double"];
	[self addMappingForIBAction:@selector(toggleSpeedLimiter:) commandTag:"Enable/Disable Speed Limiter"];
	[self addMappingForIBAction:@selector(toggleAutoFrameSkip:) commandTag:"Enable/Disable Auto Frame Skip"];
	[self addMappingForIBAction:@selector(toggleCheats:) commandTag:"Enable/Disable Cheats"];
	[self addMappingForIBAction:@selector(toggleExecutePause:) commandTag:"Execute/Pause"];
	[self addMappingForIBAction:@selector(reset:) commandTag:"Reset"];
	[self addMappingForIBAction:@selector(cmdToggleGPUState:) commandTag:"Enable/Disable GPU State"];
	
	return self;
}

- (void)dealloc
{	
	[hidManager release];
	
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

- (void) addMappingsUsingUserDefaults
{
	// Check to see if the DefaultKeyMappings.plist files exists.
	NSDictionary *defaultMappings = [NSDictionary dictionaryWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"DefaultKeyMappings" ofType:@"plist"]];
	if (defaultMappings == nil)
	{
		return;
	}
	
	NSDictionary *userMappings = [[NSUserDefaults standardUserDefaults] dictionaryForKey:@"Input_ControllerMappings"];
	if (userMappings == nil)
	{
		// If the input mappings does not exist in the user's preferences file, then copy all the mappings from the
		// DefaultKeyMappings.plist file to the preferences file.
		[[NSUserDefaults standardUserDefaults] setObject:defaultMappings forKey:@"Input_ControllerMappings"];
	}
	else
	{
		// At this point, we need to check every key in the user's preference file and make sure that all the keys
		// exist. The keys that must exist are all listed in the DefaultKeyMappings.plist file.
		BOOL userMappingsDidChange = NO;
		NSMutableDictionary *tempUserMappings = [NSMutableDictionary dictionaryWithDictionary:userMappings];
		
		NSArray *inputKeys = [defaultMappings allKeys];
		for(NSString *inputString in inputKeys)
		{
			if ([tempUserMappings objectForKey:inputString] == nil)
			{
				[tempUserMappings setValue:[defaultMappings valueForKey:inputString] forKey:inputString];
				userMappingsDidChange = YES;
			}
		}
		
		// If we had to add a missing key, then we need to copy our temporary dictionary back to the
		// user's preferences file.
		if (userMappingsDidChange)
		{
			[[NSUserDefaults standardUserDefaults] setObject:tempUserMappings forKey:@"Input_ControllerMappings"];
		}
	}
	
	NSArray *commandTagList = [defaultMappings allKeys];
	for(NSString *commandTag in commandTagList)
	{
		NSArray *deviceInfoList = (NSArray *)[userMappings valueForKey:commandTag];
		for(NSDictionary *deviceInfo in deviceInfoList)
		{
			[self addMappingUsingDeviceInfoDictionary:deviceInfo commandTag:[commandTag cStringUsingEncoding:NSUTF8StringEncoding]];
		}
	}
}

- (void) addMappingUsingDeviceInfoDictionary:(NSDictionary *)deviceInfo commandTag:(const char *)commandTag
{
	NSString *deviceCode = (NSString *)[deviceInfo valueForKey:@"deviceCode"];
	NSString *elementCode = (NSString *)[deviceInfo valueForKey:@"elementCode"];
	
	if (deviceCode == nil || elementCode == nil || commandTag == NULL)
	{
		return;
	}
	
	[self addMappingUsingDeviceCode:[deviceCode cStringUsingEncoding:NSUTF8StringEncoding] elementCode:[elementCode cStringUsingEncoding:NSUTF8StringEncoding] commandTag:commandTag];
}

- (void) addMappingUsingInputAttributes:(const InputAttributes *)inputAttr commandTag:(const char *)commandTag
{
	if (inputAttr == NULL || commandTag == NULL)
	{
		return;
	}
	
	[self addMappingUsingDeviceCode:inputAttr->deviceCode.c_str() elementCode:inputAttr->elementCode.c_str() commandTag:commandTag];
}

- (void) addMappingUsingInputList:(const InputAttributesList *)inputList commandTag:(const char *)commandTag
{
	if (inputList == NULL || commandTag == NULL)
	{
		return;
	}
	
	const size_t inputCount = inputList->size();
	
	for (unsigned int i = 0; i < inputCount; i++)
	{
		const InputAttributes &inputAttr = (*inputList)[i];
		
		if (inputAttr.inputState != INPUT_ATTRIBUTE_STATE_ON)
		{
			continue;
		}
		
		[self addMappingUsingInputAttributes:&inputAttr commandTag:commandTag];
	}
}

- (void) addMappingForIBAction:(const SEL)theSelector commandTag:(const char *)commandTag
{
	if (theSelector == nil || commandTag == NULL)
	{
		return;
	}
	
	[self addMappingUsingDeviceCode:"IBAction" elementCode:sel_getName(theSelector) commandTag:commandTag];
}

- (void) addMappingUsingDeviceCode:(const char *)deviceCode elementCode:(const char *)elementCode commandTag:(const char *)commandTag
{
	if (deviceCode == NULL || elementCode == NULL || commandTag == NULL)
	{
		return;
	}
	
	// Remove all mappings to commandTag in order to force a 1:1 mapping (except for IBAction mappings).
	// TODO: Remove this restriction when the code supports multiple inputs being mapped to the same command tag.
	[self removeAllMappingsOfCommandTag:commandTag];
	
	const std::string inputKey = std::string(deviceCode) + ":" + std::string(elementCode);
	commandMap[inputKey] = std::string(commandTag);
}

- (void) removeMappingUsingDeviceCode:(const char *)deviceCode elementCode:(const char *)elementCode
{
	if (deviceCode == NULL || elementCode == NULL)
	{
		return;
	}
	
	const std::string inputKey = std::string(deviceCode) + ":" + std::string(elementCode);
	
	CommandTagMap::iterator it = commandMap.find(inputKey);
	if (it != commandMap.end())
	{
		commandMap.erase(it);
	}
}

- (void) removeAllMappingsOfCommandTag:(const char *)commandTag
{
	if (commandTag == NULL)
	{
		return;
	}
		
	// This loop removes all mappings to commandTag, with the exception of IBAction mappings.
	for (CommandTagMap::iterator it=commandMap.begin(); it!=commandMap.end(); ++it)
	{
		if (it->first.find("IBAction") == std::string::npos && it->second == std::string(commandTag))
		{
			commandMap.erase(it);
		}
	}
}

- (CommandAttributesList) generateCommandListUsingInputList:(const InputAttributesList *)inputList
{
	CommandAttributesList cmdList;
	size_t inputCount = inputList->size();
	
	for (unsigned int i = 0; i < inputCount; i++)
	{
		const InputAttributes &inputAttr = (*inputList)[i];
		
		// All inputs require a device code and element code for mapping. If one or both are
		// not present, reject the input.
		if (inputAttr.deviceCode.empty() || inputAttr.elementCode.empty())
		{
			continue;
		}
		
		// Look up the command key using the input key.
		const std::string inputKey = inputAttr.deviceCode + ":" + inputAttr.elementCode;
		const std::string commandTag = commandMap[inputKey];
		
		if (commandTag.empty())
		{
			continue;
		}
		
		// Look up the command attributes using the command key.
		CommandAttributes cmdAttr = commandMasterList[commandTag];
		cmdAttr.inputState = inputAttr.inputState;
		
		// Override command values with input values as necessary.
		for (unsigned int j = 0; j < INPUT_HANDLER_MAX_ATTRIBUTE_VALUES; j++)
		{
			if (cmdAttr.useIntegerInputValue[j])
			{
				cmdAttr.integerValue[j] = inputAttr.integerValue[j];
			}
			
			if (cmdAttr.useFloatInputValue[j])
			{
				cmdAttr.floatValue[j] = inputAttr.floatValue[j];
			}
			
			if (cmdAttr.useObjectInputValue[j])
			{
				cmdAttr.object[j] = inputAttr.object[j];
			}
		}
		
		cmdList.push_back(cmdAttr);
	}
	
	return cmdList;
}

- (void) dispatchCommandList:(const CommandAttributesList *)cmdList
{
	size_t cmdCount = cmdList->size();
	for (size_t i = 0; i < cmdCount; i++)
	{
		const CommandAttributes &cmdAttr = (*cmdList)[i];
		
		if ([emuControl respondsToSelector:cmdAttr.selector])
		{
			[emuControl performSelector:cmdAttr.selector withObject:[NSValue valueWithBytes:&cmdAttr objCType:@encode(CommandAttributes)]];
		}
	}
}

- (BOOL) dispatchCommandUsingInputAttributes:(const InputAttributes *)inputAttr
{
	BOOL didCommandDispatch = NO;
	
	// All inputs require a device code and element code for mapping. If one or both are
	// not present, reject the input.
	if (inputAttr->deviceCode.empty() || inputAttr->elementCode.empty())
	{
		return didCommandDispatch;
	}
	
	// Look up the command key using the input key.
	const std::string inputKey = inputAttr->deviceCode + ":" + inputAttr->elementCode;
	const std::string commandTag = commandMap[inputKey];
	
	if (commandTag.empty())
	{
		return didCommandDispatch;
	}
	
	// Look up the command attributes using the command key.
	CommandAttributes cmdAttr = commandMasterList[commandTag];
	cmdAttr.inputState = inputAttr->inputState;
	
	// Override command values with input values as necessary.
	for (unsigned int j = 0; j < INPUT_HANDLER_MAX_ATTRIBUTE_VALUES; j++)
	{
		if (cmdAttr.useIntegerInputValue[j])
		{
			cmdAttr.integerValue[j] = inputAttr->integerValue[j];
		}
		
		if (cmdAttr.useFloatInputValue[j])
		{
			cmdAttr.floatValue[j] = inputAttr->floatValue[j];
		}
		
		if (cmdAttr.useObjectInputValue[j])
		{
			cmdAttr.object[j] = inputAttr->object[j];
		}
	}
	
	if ([emuControl respondsToSelector:cmdAttr.selector])
	{
		[emuControl performSelector:cmdAttr.selector withObject:[NSValue valueWithBytes:&cmdAttr objCType:@encode(CommandAttributes)]];
	}
	
	didCommandDispatch = YES;
	return didCommandDispatch;
}

- (BOOL) dispatchCommandUsingIBAction:(const SEL)theSelector tag:(NSInteger)tagValue
{
	InputAttributes inputAttr = InputManagerEncodeIBAction(theSelector, tagValue);
	return [self dispatchCommandUsingInputAttributes:&inputAttr];
}

- (void) writeUserDefaultsMappingUsingInputAttributes:(const InputAttributes *)inputAttr commandTag:(const char *)commandTag
{
	NSString *deviceCode = [NSString stringWithCString:inputAttr->deviceCode.c_str() encoding:NSUTF8StringEncoding];
	NSString *deviceName = [NSString stringWithCString:inputAttr->deviceName.c_str() encoding:NSUTF8StringEncoding];
	NSString *elementCode = [NSString stringWithCString:inputAttr->elementCode.c_str() encoding:NSUTF8StringEncoding];
	NSString *elementName = [NSString stringWithCString:inputAttr->elementName.c_str() encoding:NSUTF8StringEncoding];
	NSString *commandTagString = [NSString stringWithCString:commandTag encoding:NSUTF8StringEncoding];
	
	NSDictionary *deviceInfo = [NSDictionary dictionaryWithObjectsAndKeys:
								deviceCode, @"deviceCode",
								deviceName, @"deviceName",
								elementCode, @"elementCode",
								elementName, @"elementName",
								nil];
	
	NSMutableDictionary *tempUserMappings = [NSMutableDictionary dictionaryWithDictionary:[[NSUserDefaults standardUserDefaults] dictionaryForKey:@"Input_ControllerMappings"]];
	[tempUserMappings setValue:[NSArray arrayWithObject:deviceInfo] forKey:commandTagString];
	[[NSUserDefaults standardUserDefaults] setValue:tempUserMappings forKey:@"Input_ControllerMappings"];
}

InputAttributesList InputManagerEncodeHIDQueue(const IOHIDQueueRef hidQueue)
{
	InputAttributesList inputList;
	if (hidQueue == nil)
	{
		return inputList;
	}
	
	do
	{
		IOHIDValueRef hidValueRef = IOHIDQueueCopyNextValueWithTimeout(hidQueue, 0.0);
		if (hidValueRef == NULL)
		{
			break;
		}
		
		InputAttributesList hidInputList = InputListFromHatSwitchValue(hidValueRef, false);
		if (hidInputList.empty())
		{
			hidInputList = InputListFromHIDValue(hidValueRef);
		}
		
		size_t hidInputCount = hidInputList.size();
		for (unsigned int i = 0; i < hidInputCount; i++)
		{
			if (hidInputList[i].deviceCode.empty() || hidInputList[i].elementCode.empty())
			{
				continue;
			}
			
			inputList.push_back(hidInputList[i]);
		}
		
		CFRelease(hidValueRef);
	} while (1);
	
	if (!inputList.empty())
	{
		// HID input devices don't register events, so we need to manually prevent
		// sleep and screensaver whenever we detect an input.
		UpdateSystemActivity(UsrActivity);
	}
	
	return inputList;
}

InputAttributes InputManagerEncodeKeyboardInput(const unsigned short keyCode, BOOL keyPressed)
{
	std::string elementName = keyboardNameTable[keyCode];
	char elementCodeBuf[256] = {0};
	snprintf(elementCodeBuf, 256, "%d", keyCode);
	
	InputAttributes inputAttr;
	inputAttr.deviceCode = "NSEventKeyboard";
	inputAttr.deviceName = "Keyboard";
	inputAttr.elementCode = std::string(elementCodeBuf);
	inputAttr.elementName = (elementName.empty()) ? inputAttr.elementCode : elementName;
	inputAttr.inputState = (keyPressed) ? INPUT_ATTRIBUTE_STATE_ON : INPUT_ATTRIBUTE_STATE_OFF;
	
	return inputAttr;
}

InputAttributes InputManagerEncodeMouseButtonInput(const NSInteger buttonNumber, const NSPoint touchLoc, BOOL buttonPressed)
{
	char elementNameBuf[256] = {0};
	snprintf(elementNameBuf, 256, "Button %i", (int)buttonNumber);
	
	char elementCodeBuf[256] = {0};
	snprintf(elementCodeBuf, 256, "%i", (int)buttonNumber);
	
	switch (buttonNumber)
	{
		case kCGMouseButtonLeft:
			strncpy(elementNameBuf, "Primary Button", 256);
			break;
			
		case kCGMouseButtonRight:
			strncpy(elementNameBuf, "Secondary Button", 256);
			break;
			
		case kCGMouseButtonCenter:
			strncpy(elementNameBuf, "Center Button", 256);
			break;
			
		default:
			break;
	}
	
	InputAttributes inputAttr;
	inputAttr.deviceCode = "NSEventMouse";
	inputAttr.deviceName = "Mouse";
	inputAttr.elementCode = std::string(elementCodeBuf);
	inputAttr.elementName = std::string(elementNameBuf);
	inputAttr.inputState = (buttonPressed) ? INPUT_ATTRIBUTE_STATE_ON : INPUT_ATTRIBUTE_STATE_OFF;
	inputAttr.floatValue[0] = touchLoc.x;
	inputAttr.floatValue[1] = touchLoc.y;
	
	return inputAttr;
}

InputAttributes InputManagerEncodeIBAction(const SEL theSelector, const NSInteger tagValue)
{
	InputAttributes inputAttr;
	inputAttr.deviceCode = "IBAction";
	inputAttr.deviceName = "Application";
	inputAttr.elementCode = std::string(sel_getName(theSelector));
	inputAttr.elementName = inputAttr.elementCode;
	inputAttr.integerValue[0] = tagValue;
	
	return inputAttr;
}

@end
