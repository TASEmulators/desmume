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
		altElementCode - A char buffer that overrides the default element code.
		altElementName - A char buffer that overrides the default element name.
		altOnState - A pointer to a bool value that overrides the default on state.

	Returns:
		An InputAttributes struct with the parsed input attributes.

	Details:
		None.
 ********************************************************************************************/
InputAttributes InputAttributesOfHIDValue(IOHIDValueRef hidValueRef, const char *altElementCode, const char *altElementName, bool *altOnState)
{
	InputAttributes inputAttr;
	
	if (hidValueRef == NULL)
	{
		return inputAttr;
	}
	
	IOHIDElementRef hidElementRef = IOHIDValueGetElement(hidValueRef);
	NSInteger elementUsagePage = IOHIDElementGetUsagePage(hidElementRef);
	NSInteger elementUsage = IOHIDElementGetUsage(hidElementRef);
	
	if (altElementCode == NULL)
	{
		snprintf(inputAttr.elementCode, INPUT_HANDLER_STRING_LENGTH, "0x%04lX/0x%04lX", (long)elementUsagePage, (long)elementUsage);
	}
	else
	{
		strncpy(inputAttr.elementCode, altElementCode, INPUT_HANDLER_STRING_LENGTH);
	}
	
	if (altElementName == NULL)
	{
		CFStringRef cfElementName = IOHIDElementGetName(hidElementRef);
		if (cfElementName == nil)
		{
			if (elementUsagePage == kHIDPage_Button)
			{
				snprintf(inputAttr.elementName, INPUT_HANDLER_STRING_LENGTH, "Button %li", (long)elementUsage);
			}
			else if (elementUsagePage == kHIDPage_VendorDefinedStart)
			{
				snprintf(inputAttr.elementName, INPUT_HANDLER_STRING_LENGTH, "VendorDefined %li", (long)elementUsage);
			}
			else
			{
				NSDictionary *elementUsagePageDict = (NSDictionary *)[hidUsageTable valueForKey:[NSString stringWithFormat:@"0x%04lX", (long)elementUsagePage]];
				
				NSString *elementNameLookup = (NSString *)[elementUsagePageDict valueForKey:[NSString stringWithFormat:@"0x%04lX", (long)elementUsage]];
				if (elementNameLookup != nil)
				{
					strncpy(inputAttr.elementName, [elementNameLookup cStringUsingEncoding:NSUTF8StringEncoding], 256);
				}
			}
		}
		else
		{
			CFStringGetCString(cfElementName, inputAttr.elementName, INPUT_HANDLER_STRING_LENGTH, kCFStringEncodingUTF8);
		}
	}
	else
	{
		strncpy(inputAttr.elementName, altElementName, INPUT_HANDLER_STRING_LENGTH);
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
		
		snprintf(inputAttr.deviceCode, INPUT_HANDLER_STRING_LENGTH, "%d/%d/0x%08X", (int)vendorID, (int)productID, (unsigned int)locationID);
	}
	else
	{
		char cfDeviceCodeBuf[256] = {0};
		CFStringGetCString(cfDeviceCode, cfDeviceCodeBuf, 256, kCFStringEncodingUTF8);
		snprintf(inputAttr.deviceCode, INPUT_HANDLER_STRING_LENGTH, "%d/%d/%s", (int)vendorID, (int)productID, cfDeviceCodeBuf);
	}
	
	CFStringRef cfDeviceName = (CFStringRef)IOHIDDeviceGetProperty(hidDeviceRef, CFSTR(kIOHIDProductKey));
	if (cfDeviceName == nil)
	{
		strncpy(inputAttr.deviceName, inputAttr.deviceCode, INPUT_HANDLER_STRING_LENGTH);
	}
	else
	{
		CFStringGetCString(cfDeviceName, inputAttr.deviceName, INPUT_HANDLER_STRING_LENGTH, kCFStringEncodingUTF8);
	}
	
	bool onState = (altOnState == NULL) ? GetOnStateFromHIDValueRef(hidValueRef) : *altOnState;
	CFIndex logicalValue = IOHIDValueGetIntegerValue(hidValueRef);
	NSInteger logicalMin = IOHIDElementGetLogicalMin(hidElementRef);
	NSInteger logicalMax = IOHIDElementGetLogicalMax(hidElementRef);
	
	inputAttr.state			= (onState) ? INPUT_ATTRIBUTE_STATE_ON : INPUT_ATTRIBUTE_STATE_OFF;
	inputAttr.intCoordX		= 0;
	inputAttr.intCoordY		= 0;
	inputAttr.floatCoordX	= 0.0f;
	inputAttr.floatCoordY	= 0.0f;
	inputAttr.scalar		= (float)(logicalValue - logicalMin) / (float)(logicalMax - logicalMin);
	inputAttr.sender		= nil;
	
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
		inputList.push_back(InputAttributesOfHIDValue(hidValueRef, NULL, NULL, NULL));
	}
	else
	{
		NSInteger lowerThreshold = ((logicalMax - logicalMin) / 3) + logicalMin;
		NSInteger upperThreshold = (((logicalMax - logicalMin) * 2) / 3) + logicalMin;
		bool onState = true;
		bool offState = false;
		
		char elementCodeLowerThresholdBuf[256] = {0};
		char elementCodeUpperThresholdBuf[256] = {0};
		snprintf(elementCodeLowerThresholdBuf, 256, "0x%04lX/0x%04lX/LowerThreshold", (long)elementUsagePage, (long)elementUsage);
		snprintf(elementCodeUpperThresholdBuf, 256, "0x%04lX/0x%04lX/UpperThreshold", (long)elementUsagePage, (long)elementUsage);
		
		if (logicalValue <= lowerThreshold)
		{
			inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeLowerThresholdBuf, NULL, &onState));
			inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeUpperThresholdBuf, NULL, &offState));
		}
		else if (logicalValue >= upperThreshold)
		{
			inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeLowerThresholdBuf, NULL, &offState));
			inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeUpperThresholdBuf, NULL, &onState));
		}
		else
		{
			inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeLowerThresholdBuf, NULL, &offState));
			inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeUpperThresholdBuf, NULL, &offState));
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
	bool onState = true;
	bool offState = false;
	
	char elementCodeFourWay[4][256];
	for (size_t i = 0; i < 4; i++)
	{
		snprintf(elementCodeFourWay[i], 256, "0x%04lX/0x%04lX/%d-FourDirection", (long)elementUsagePage, (long)elementUsage, (unsigned int)i);
	}
	
	const char *elementNameFourWay[4] = {
		"Hatswitch - Up",
		"Hatswitch - Right",
		"Hatswitch - Down",
		"Hatswitch - Left" };
	
	char elementCodeEightWay[8][256];
	for (size_t i = 0; i < 8; i++)
	{
		snprintf(elementCodeEightWay[i], 256, "0x%04lX/0x%04lX/%d-EightDirection", (long)elementUsagePage, (long)elementUsage, (unsigned int)i);
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
		for (size_t i = 0; i <= (size_t)logicalMax; i++)
		{
			inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeFourWay[i], elementNameFourWay[i], (i == (size_t)logicalValue) ? &onState : &offState));
		}
	}
	else if (logicalMax == 7)
	{
		if (useEightDirection)
		{
			for (size_t i = 0; i <= (size_t)logicalMax; i++)
			{
				inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[i], elementNameEightWay[i], (i == (size_t)logicalValue) ? &onState : &offState));
			}
		}
		else
		{
			switch (logicalValue)
			{
				case 0:
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[0], elementNameEightWay[0], &onState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[2], elementNameEightWay[2], &offState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[4], elementNameEightWay[4], &offState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[6], elementNameEightWay[6], &offState));
					break;
					
				case 1:
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[0], elementNameEightWay[0], &onState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[2], elementNameEightWay[2], &onState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[4], elementNameEightWay[4], &offState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[6], elementNameEightWay[6], &offState));
					break;
					
				case 2:
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[0], elementNameEightWay[0], &offState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[2], elementNameEightWay[2], &onState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[4], elementNameEightWay[4], &offState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[6], elementNameEightWay[6], &offState));
					break;
					
				case 3:
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[0], elementNameEightWay[0], &offState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[2], elementNameEightWay[2], &onState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[4], elementNameEightWay[4], &onState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[6], elementNameEightWay[6], &offState));
					break;
					
				case 4:
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[0], elementNameEightWay[0], &offState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[2], elementNameEightWay[2], &offState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[4], elementNameEightWay[4], &onState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[6], elementNameEightWay[6], &offState));
					break;
					
				case 5:
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[0], elementNameEightWay[0], &offState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[2], elementNameEightWay[2], &offState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[4], elementNameEightWay[4], &onState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[6], elementNameEightWay[6], &onState));
					break;
					
				case 6:
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[0], elementNameEightWay[0], &offState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[2], elementNameEightWay[2], &offState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[4], elementNameEightWay[4], &offState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[6], elementNameEightWay[6], &onState));
					break;
					
				case 7:
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[0], elementNameEightWay[0], &onState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[2], elementNameEightWay[2], &offState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[4], elementNameEightWay[4], &offState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[6], elementNameEightWay[6], &onState));
					break;
					
				default:
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[0], elementNameEightWay[0], &offState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[2], elementNameEightWay[2], &offState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[4], elementNameEightWay[4], &offState));
					inputList.push_back(InputAttributesOfHIDValue(hidValueRef, elementCodeEightWay[6], elementNameEightWay[6], &offState));
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
	InputHIDManager *hidManager = (InputHIDManager *)inContext;
	IOHIDQueueRef hidQueue = (IOHIDQueueRef)inSender;
	id<InputHIDManagerTarget> target = [hidManager target];
	
	if (target != nil)
	{
		NSAutoreleasePool *tempPool = [[NSAutoreleasePool alloc] init];
		[[hidManager target] handleHIDQueue:hidQueue];
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
@synthesize inputMappings;
@synthesize commandTagList;
@synthesize commandIcon;

#if defined(__ppc__) || defined(__ppc64__)
static std::map<unsigned short, std::string> keyboardNameTable; // Key = Key code, Value = Key name
#else
static std::tr1::unordered_map<unsigned short, std::string> keyboardNameTable; // Key = Key code, Value = Key name
#endif

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
	inputMappings = [[NSMutableDictionary alloc] initWithCapacity:128];
	commandTagList = [[[NSDictionary alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"DefaultKeyMappings" ofType:@"plist"]] valueForKey:@"CommandTagList"];
	
	// Initialize the icons associated with each command.
	commandIcon = [[NSDictionary alloc] initWithObjectsAndKeys:
				   [[[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_DSButtonSelect_420x420" ofType:@"png"]] autorelease],		@"UNKNOWN COMMAND",
				   [[[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_ArrowUp_420x420" ofType:@"png"]] autorelease],			@"Up",
				   [[[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_ArrowDown_420x420" ofType:@"png"]] autorelease],			@"Down",
				   [[[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_ArrowLeft_420x420" ofType:@"png"]] autorelease],			@"Left",
				   [[[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_ArrowRight_420x420" ofType:@"png"]] autorelease],			@"Right",
				   [[[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_DSButtonA_420x420" ofType:@"png"]] autorelease],			@"A",
				   [[[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_DSButtonB_420x420" ofType:@"png"]] autorelease],			@"B",
				   [[[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_DSButtonX_420x420" ofType:@"png"]] autorelease],			@"X",
				   [[[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_DSButtonY_420x420" ofType:@"png"]] autorelease],			@"Y",
				   [[[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_DSButtonL_420x420" ofType:@"png"]] autorelease],			@"L",
				   [[[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_DSButtonR_420x420" ofType:@"png"]] autorelease],			@"R",
				   [[[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_DSButtonStart_420x420" ofType:@"png"]] autorelease],		@"Start",
				   [[[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_DSButtonSelect_420x420" ofType:@"png"]] autorelease],		@"Select",
				   [[[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_Microphone_420x420" ofType:@"png"]] autorelease],			@"Microphone",
				   [[[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_RotateCCW_420x420" ofType:@"png"]] autorelease],			@"Rotate Display Left",
				   [[[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_RotateCW_420x420" ofType:@"png"]] autorelease],			@"Rotate Display Right",
				   [[[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_ShowHUD_420x420" ofType:@"png"]] autorelease],			@"HUD",
				   [[[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_Execute_420x420" ofType:@"png"]] autorelease],			@"Execute",
				   [[[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_Pause_420x420" ofType:@"png"]] autorelease],				@"Pause",
				   [[[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_Execute_420x420" ofType:@"png"]] autorelease],			@"Execute/Pause",
				   [[[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_Reset_420x420" ofType:@"png"]] autorelease],				@"Reset",
				   [[[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_DSButtonSelect_420x420" ofType:@"png"]] autorelease],		@"Touch",
				   [[[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Icon_VolumeMute_16x16" ofType:@"png"]] autorelease],			@"Mute/Unmute",
				   nil];
	
	// Initialize the selectors used for each command tag. (Do this in code rather than in an external file.)
	commandSelector["Up"]						= @selector(cmdUpdateDSController:);
	commandSelector["Down"]						= @selector(cmdUpdateDSController:);
	commandSelector["Right"]					= @selector(cmdUpdateDSController:);
	commandSelector["Left"]						= @selector(cmdUpdateDSController:);
	commandSelector["A"]						= @selector(cmdUpdateDSController:);
	commandSelector["B"]						= @selector(cmdUpdateDSController:);
	commandSelector["X"]						= @selector(cmdUpdateDSController:);
	commandSelector["Y"]						= @selector(cmdUpdateDSController:);
	commandSelector["L"]						= @selector(cmdUpdateDSController:);
	commandSelector["R"]						= @selector(cmdUpdateDSController:);
	commandSelector["Start"]					= @selector(cmdUpdateDSController:);
	commandSelector["Select"]					= @selector(cmdUpdateDSController:);
	commandSelector["Touch"]					= @selector(cmdUpdateDSController:);
	commandSelector["Microphone"]				= @selector(cmdUpdateDSController:);
	commandSelector["Debug"]					= @selector(cmdUpdateDSController:);
	commandSelector["Lid"]						= @selector(cmdUpdateDSController:);
	
	commandSelector["Load State Slot"]			= @selector(cmdLoadEmuSaveStateSlot:);
	commandSelector["Save State Slot"]			= @selector(cmdSaveEmuSaveStateSlot:);
	commandSelector["Copy Screen"]				= @selector(cmdCopyScreen:);
	commandSelector["Rotate Display Relative"]	= @selector(cmdRotateDisplayRelative:);
	commandSelector["Set Speed"]				= @selector(cmdHoldToggleSpeedScalar:);
	commandSelector["Enable/Disable Speed Limiter"] = @selector(cmdToggleSpeedLimiter:);
	commandSelector["Enable/Disable Auto Frame Skip"] = @selector(cmdToggleAutoFrameSkip:);
	commandSelector["Enable/Disable Cheats"]	= @selector(cmdToggleCheats:);
	commandSelector["Execute/Pause"]			= @selector(cmdToggleExecutePause:);
	commandSelector["Reset"]					= @selector(cmdReset:);
	commandSelector["Mute/Unmute"]				= @selector(cmdToggleMute:);
	commandSelector["Enable/Disable GPU State"]	= @selector(cmdToggleGPUState:);
	
	// Generate the default command attributes for each command tag. (Do this in code rather than in an external file.)
	CommandAttributes cmdDSControlRight			= NewCommandAttributesForDSControl("Right", DSControllerState_Right);
	CommandAttributes cmdDSControlLeft			= NewCommandAttributesForDSControl("Left", DSControllerState_Left);
	CommandAttributes cmdDSControlDown			= NewCommandAttributesForDSControl("Down", DSControllerState_Down);
	CommandAttributes cmdDSControlUp			= NewCommandAttributesForDSControl("Up", DSControllerState_Up);
	CommandAttributes cmdDSControlSelect		= NewCommandAttributesForDSControl("Select", DSControllerState_Select);
	CommandAttributes cmdDSControlStart			= NewCommandAttributesForDSControl("Start", DSControllerState_Start);
	CommandAttributes cmdDSControlB				= NewCommandAttributesForDSControl("B", DSControllerState_B);
	CommandAttributes cmdDSControlA				= NewCommandAttributesForDSControl("A", DSControllerState_A);
	CommandAttributes cmdDSControlY				= NewCommandAttributesForDSControl("Y", DSControllerState_Y);
	CommandAttributes cmdDSControlX				= NewCommandAttributesForDSControl("X", DSControllerState_X);
	CommandAttributes cmdDSControlL				= NewCommandAttributesForDSControl("L", DSControllerState_L);
	CommandAttributes cmdDSControlR				= NewCommandAttributesForDSControl("R", DSControllerState_R);
	CommandAttributes cmdDSControlDebug			= NewCommandAttributesForDSControl("Debug", DSControllerState_Debug);
	CommandAttributes cmdDSControlLid			= NewCommandAttributesForDSControl("Lid", DSControllerState_Lid);
	
	CommandAttributes cmdDSControlTouch			= NewCommandAttributesForDSControl("Touch", DSControllerState_Touch);	
	cmdDSControlTouch.useInputForIntCoord = true;
	
	CommandAttributes cmdDSControlMic			= NewCommandAttributesForDSControl("Microphone", DSControllerState_Microphone);
	cmdDSControlMic.intValue[1] = MICMODE_INTERNAL_NOISE;
	cmdDSControlMic.floatValue[0] = 250.0f;
	
	CommandAttributes cmdLoadEmuSaveStateSlot	= NewCommandAttributesForSelector("Load State Slot", commandSelector["Load State Slot"]);
	CommandAttributes cmdSaveEmuSaveStateSlot	= NewCommandAttributesForSelector("Save State Slot", commandSelector["Save State Slot"]);	
	CommandAttributes cmdCopyScreen				= NewCommandAttributesForSelector("Copy Screen", commandSelector["Copy Screen"]);
	
	CommandAttributes cmdRotateDisplayRelative	= NewCommandAttributesForSelector("Rotate Display Relative", commandSelector["Rotate Display Relative"]);
	cmdRotateDisplayRelative.intValue[0] = 90;
	
	CommandAttributes cmdRotateDisplayLeft		= NewCommandAttributesForSelector("Rotate Display Left", commandSelector["Rotate Display Relative"]);
	cmdRotateDisplayLeft.intValue[0] = -90;
	
	CommandAttributes cmdRotateDisplayRight		= NewCommandAttributesForSelector("Rotate Display Right", commandSelector["Rotate Display Relative"]);
	cmdRotateDisplayRight.intValue[0] = 90;
	
	CommandAttributes cmdToggleSpeed			= NewCommandAttributesForSelector("Set Speed", commandSelector["Set Speed"]);
	cmdToggleSpeed.floatValue[0] = 1.0f;
	
	CommandAttributes cmdToggleSpeedLimiter		= NewCommandAttributesForSelector("Enable/Disable Speed Limiter", commandSelector["Enable/Disable Speed Limiter"]);
	CommandAttributes cmdToggleAutoFrameSkip	= NewCommandAttributesForSelector("Enable/Disable Auto Frame Skip", commandSelector["Enable/Disable Auto Frame Skip"]);
	CommandAttributes cmdToggleCheats			= NewCommandAttributesForSelector("Enable/Disable Cheats", commandSelector["Enable/Disable Cheats"]);
	CommandAttributes cmdToggleExecutePause		= NewCommandAttributesForSelector("Execute/Pause", commandSelector["Execute/Pause"]);
	CommandAttributes cmdReset					= NewCommandAttributesForSelector("Reset", commandSelector["Reset"]);
	CommandAttributes cmdToggleMute				= NewCommandAttributesForSelector("Mute/Unmute", commandSelector["Mute/Unmute"]);
	CommandAttributes cmdToggleGPUState			= NewCommandAttributesForSelector("Enable/Disable GPU State", commandSelector["Enable/Disable GPU State"]);
	
	defaultCommandAttributes["Up"]				= cmdDSControlUp;
	defaultCommandAttributes["Down"]			= cmdDSControlDown;
	defaultCommandAttributes["Right"]			= cmdDSControlRight;
	defaultCommandAttributes["Left"]			= cmdDSControlLeft;
	defaultCommandAttributes["A"]				= cmdDSControlA;
	defaultCommandAttributes["B"]				= cmdDSControlB;
	defaultCommandAttributes["X"]				= cmdDSControlX;
	defaultCommandAttributes["Y"]				= cmdDSControlY;
	defaultCommandAttributes["L"]				= cmdDSControlL;
	defaultCommandAttributes["R"]				= cmdDSControlR;
	defaultCommandAttributes["Start"]			= cmdDSControlStart;
	defaultCommandAttributes["Select"]			= cmdDSControlSelect;
	defaultCommandAttributes["Touch"]			= cmdDSControlTouch;
	defaultCommandAttributes["Microphone"]		= cmdDSControlMic;
	defaultCommandAttributes["Debug"]			= cmdDSControlDebug;
	defaultCommandAttributes["Lid"]				= cmdDSControlLid;
	
	defaultCommandAttributes["Load State Slot"] = cmdLoadEmuSaveStateSlot;
	defaultCommandAttributes["Save State Slot"] = cmdSaveEmuSaveStateSlot;
	defaultCommandAttributes["Copy Screen"]		= cmdCopyScreen;
	defaultCommandAttributes["Rotate Display Left"] = cmdRotateDisplayLeft;
	defaultCommandAttributes["Rotate Display Right"] = cmdRotateDisplayRight;
	defaultCommandAttributes["Set Speed"]		= cmdToggleSpeed;
	defaultCommandAttributes["Enable/Disable Speed Limiter"] = cmdToggleSpeedLimiter;
	defaultCommandAttributes["Enable/Disable Auto Frame Skip"] = cmdToggleAutoFrameSkip;
	defaultCommandAttributes["Enable/Disable Cheats"] = cmdToggleCheats;
	defaultCommandAttributes["Execute/Pause"]	= cmdToggleExecutePause;
	defaultCommandAttributes["Reset"]			= cmdReset;
	defaultCommandAttributes["Mute/Unmute"]		= cmdToggleMute;
	defaultCommandAttributes["Enable/Disable GPU State"] = cmdToggleGPUState;
	
	// Map all IBActions (the target object is an EmuControllerDelegate)
	[self addMappingForIBAction:@selector(loadEmuSaveStateSlot:) commandAttributes:&cmdLoadEmuSaveStateSlot];
	[self addMappingForIBAction:@selector(saveEmuSaveStateSlot:) commandAttributes:&cmdSaveEmuSaveStateSlot];
	[self addMappingForIBAction:@selector(copy:) commandAttributes:&cmdCopyScreen];
	[self addMappingForIBAction:@selector(changeRotationRelative:) commandAttributes:&cmdRotateDisplayRelative];
	[self addMappingForIBAction:@selector(toggleSpeedLimiter:) commandAttributes:&cmdToggleSpeedLimiter];
	[self addMappingForIBAction:@selector(toggleAutoFrameSkip:) commandAttributes:&cmdToggleAutoFrameSkip];
	[self addMappingForIBAction:@selector(toggleCheats:) commandAttributes:&cmdToggleCheats];
	[self addMappingForIBAction:@selector(toggleExecutePause:) commandAttributes:&cmdToggleExecutePause];
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
		
		for(NSDictionary *deviceInfo in deviceInfoList)
		{
			const char *cmdTag = [commandTag cStringUsingEncoding:NSUTF8StringEncoding];
			CommandAttributes cmdAttr = defaultCommandAttributes[cmdTag];
			UpdateCommandAttributesWithDeviceInfoDictionary(&cmdAttr, deviceInfo);
			
			// Force DS control commands to use IDs from code instead of from the file.
			// (In other words, we can't trust an external file with this information since
			// IDs might desync if the DS Control ID enumeration changes.)
			if (cmdAttr.selector == @selector(cmdUpdateDSController:))
			{
				cmdAttr.intValue[0] = defaultCommandAttributes[cmdTag].intValue[0];
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

- (void) addMappingUsingDeviceInfoDictionary:(NSDictionary *)deviceInfo commandAttributes:(const CommandAttributes *)cmdAttr
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

- (void) addMappingUsingInputAttributes:(const InputAttributes *)inputAttr commandAttributes:(const CommandAttributes *)cmdAttr
{
	if (inputAttr == NULL)
	{
		return;
	}
	
	NSMutableDictionary *deviceInfo = DeviceInfoDictionaryWithCommandAttributes(cmdAttr,
																				[NSString stringWithCString:inputAttr->deviceCode encoding:NSUTF8StringEncoding],
																				[NSString stringWithCString:inputAttr->deviceName encoding:NSUTF8StringEncoding],
																				[NSString stringWithCString:inputAttr->elementCode encoding:NSUTF8StringEncoding],
																				[NSString stringWithCString:inputAttr->elementName encoding:NSUTF8StringEncoding]);
	
	[self addMappingUsingDeviceInfoDictionary:deviceInfo commandAttributes:cmdAttr];
}

- (void) addMappingUsingInputList:(const InputAttributesList *)inputList commandAttributes:(const CommandAttributes *)cmdAttr
{
	if (inputList == NULL)
	{
		return;
	}
	
	const size_t inputCount = inputList->size();
	
	for (size_t i = 0; i < inputCount; i++)
	{
		const InputAttributes &inputAttr = (*inputList)[i];
		if (inputAttr.state != INPUT_ATTRIBUTE_STATE_ON)
		{
			continue;
		}
		
		[self addMappingUsingInputAttributes:&inputAttr commandAttributes:cmdAttr];
	}
}

- (void) addMappingForIBAction:(const SEL)theSelector commandAttributes:(const CommandAttributes *)cmdAttr
{
	if (theSelector == nil)
	{
		return;
	}
	
	CommandAttributes IBActionCmdAttr = *cmdAttr;
	IBActionCmdAttr.useInputForSender = true;
	
	[self addMappingUsingDeviceCode:"IBAction" elementCode:sel_getName(theSelector) commandAttributes:&IBActionCmdAttr];
}

- (void) addMappingUsingDeviceCode:(const char *)deviceCode elementCode:(const char *)elementCode commandAttributes:(const CommandAttributes *)cmdAttr
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

- (CommandAttributesList) generateCommandListUsingInputList:(const InputAttributesList *)inputList
{
	CommandAttributesList cmdList;
	const size_t inputCount = inputList->size();
	
	for (size_t i = 0; i < inputCount; i++)
	{
		const InputAttributes &inputAttr = (*inputList)[i];
		
		// All inputs require a device code and element code for mapping. If one or both are
		// not present, reject the input.
		if (inputAttr.deviceCode[0] == '\0' || inputAttr.elementCode[0] == '\0')
		{
			continue;
		}
		
		// Look up the command attributes using the input key.
		const std::string inputKey	= std::string(inputAttr.deviceCode) + ":" + std::string(inputAttr.elementCode);
		CommandAttributes cmdAttr	= commandMap[inputKey];
		if (cmdAttr.tag[0] == '\0' || cmdAttr.selector == nil)
		{
			continue;
		}
		
		cmdAttr.input = inputAttr; // Copy the input state to the command attributes.
		cmdList.push_back(cmdAttr); // Add the command attributes to the list.
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
			NSValue *cmdObject = [[NSValue alloc] initWithBytes:&cmdAttr objCType:@encode(CommandAttributes)];
			[emuControl performSelector:cmdAttr.selector withObject:cmdObject];
			[cmdObject release];
		}
	}
}

- (BOOL) dispatchCommandUsingInputAttributes:(const InputAttributes *)inputAttr
{
	BOOL didCommandDispatch = NO;
	
	// All inputs require a device code and element code for mapping. If one or both are
	// not present, reject the input.
	if (inputAttr->deviceCode[0] == '\0' || inputAttr->elementCode[0] == '\0')
	{
		return didCommandDispatch;
	}
	
	// Look up the command key using the input key.
	const std::string inputKey	= std::string(inputAttr->deviceCode) + ":" + std::string(inputAttr->elementCode);
	CommandAttributes cmdAttr	= commandMap[inputKey];
	if (cmdAttr.tag[0] == '\0' || cmdAttr.selector == nil)
	{
		return didCommandDispatch;
	}
	
	// Copy the input state to the command attributes.
	cmdAttr.input = *inputAttr;
	
	if ([emuControl respondsToSelector:cmdAttr.selector])
	{
		NSValue *cmdObject = [[NSValue alloc] initWithBytes:&cmdAttr objCType:@encode(CommandAttributes)];
		[emuControl performSelector:cmdAttr.selector withObject:cmdObject];
		[cmdObject release];
	}
	
	didCommandDispatch = YES;
	return didCommandDispatch;
}

- (BOOL) dispatchCommandUsingIBAction:(const SEL)theSelector sender:(id)sender
{
	const InputAttributes inputAttr = InputManagerEncodeIBAction(theSelector, sender);
	return [self dispatchCommandUsingInputAttributes:&inputAttr];
}

- (void) writeDefaultsInputMappings
{
	[[NSUserDefaults standardUserDefaults] setObject:[self inputMappings] forKey:@"Input_ControllerMappings"];
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

- (SEL) selectorOfCommandTag:(const char *)commandTag
{
	return commandSelector[commandTag];
}

- (CommandAttributes) defaultCommandAttributesForCommandTag:(const char *)commandTag
{
	return defaultCommandAttributes[commandTag];
}

- (CommandAttributes) mappedCommandAttributesOfDeviceCode:(const char *)deviceCode elementCode:(const char *)elementCode
{
	const std::string inputKey = std::string(deviceCode) + ":" + std::string(elementCode);
	return commandMap[inputKey];
}

- (void) setMappedCommandAttributes:(const CommandAttributes *)cmdAttr deviceCode:(const char *)deviceCode elementCode:(const char *)elementCode
{
	const std::string inputKey = std::string(deviceCode) + ":" + std::string(elementCode);
	commandMap[inputKey] = *cmdAttr;
}

- (void) updateInputSettingsSummaryInDeviceInfoDictionary:(NSMutableDictionary *)deviceInfo commandTag:(const char *)commandTag
{
	NSString *inputSummary = nil;
	
	if (strncmp(commandTag, "Touch", INPUT_HANDLER_STRING_LENGTH) == 0)
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
		const NSInteger micMode = [(NSNumber *)[deviceInfo valueForKey:@"intValue1"] integerValue];
		switch (micMode)
		{
			case MICMODE_NONE:
				inputSummary = NSSTRING_INPUTPREF_MIC_NONE;
				break;
				
			case MICMODE_INTERNAL_NOISE:
				inputSummary = NSSTRING_INPUTPREF_MIC_INTERNAL_NOISE;
				break;
				
			case MICMODE_AUDIO_FILE:
				inputSummary = (NSString *)[deviceInfo valueForKey:@"object1"];
				if (inputSummary == nil)
				{
					inputSummary = NSSTRING_INPUTPREF_MIC_AUDIO_FILE_NONE_SELECTED;
				}
				break;
				
			case MICMODE_WHITE_NOISE:
				inputSummary = NSSTRING_INPUTPREF_MIC_WHITE_NOISE;
				break;
				
			case MICMODE_SINE_WAVE:
				inputSummary = [NSString stringWithFormat:NSSTRING_INPUTPREF_MIC_SINE_WAVE, [(NSNumber *)[deviceInfo valueForKey:@"floatValue0"] floatValue]];
				break;
				
			case MICMODE_PHYSICAL:
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
	outputFormat.mBytesPerPacket = 1;
	outputFormat.mFramesPerPacket = 1;
	outputFormat.mBytesPerFrame = 1;
	outputFormat.mChannelsPerFrame = 1;
	outputFormat.mBitsPerChannel = 8;
	
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
	u8 *buffer = theGenerator.allocate(bufferSize);
	
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
	for (SInt64 i = 0; i < bufferSize; i++)
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

CommandAttributes NewDefaultCommandAttributes(const char *commandTag)
{
	CommandAttributes cmdAttr;
	
	strncpy(cmdAttr.tag, commandTag, INPUT_HANDLER_STRING_LENGTH);
	cmdAttr.selector				= nil;
	cmdAttr.intValue[0]				= 0;
	cmdAttr.intValue[1]				= 0;
	cmdAttr.intValue[2]				= 0;
	cmdAttr.intValue[3]				= 0;
	cmdAttr.floatValue[0]			= 0;
	cmdAttr.floatValue[1]			= 0;
	cmdAttr.floatValue[2]			= 0;
	cmdAttr.floatValue[3]			= 0;
	cmdAttr.object[0]				= nil;
	cmdAttr.object[1]				= nil;
	cmdAttr.object[2]				= nil;
	cmdAttr.object[3]				= nil;
	
	cmdAttr.useInputForIntCoord		= false;
	cmdAttr.useInputForFloatCoord	= false;
	cmdAttr.useInputForScalar		= false;
	cmdAttr.useInputForSender		= false;
	
	return cmdAttr;
}

CommandAttributes NewCommandAttributesForSelector(const char *commandTag, const SEL theSelector)
{
	CommandAttributes cmdAttr = NewDefaultCommandAttributes(commandTag);
	cmdAttr.selector = theSelector;
	
	return cmdAttr;
}

CommandAttributes NewCommandAttributesForDSControl(const char *commandTag, const NSUInteger controlID)
{
	CommandAttributes cmdAttr = NewCommandAttributesForSelector(commandTag, @selector(cmdUpdateDSController:));
	cmdAttr.intValue[0] = controlID;
	cmdAttr.floatValue[0] = 250.0f;
	
	return cmdAttr;
}

void UpdateCommandAttributesWithDeviceInfoDictionary(CommandAttributes *cmdAttr, NSDictionary *deviceInfo)
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
	NSNumber *useInputForSender		= (NSNumber *)[deviceInfo valueForKey:@"useInputForSender"];
	
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
	if (useInputForSender != nil)		cmdAttr->useInputForSender = [useInputForSender boolValue];
	
	cmdAttr->object[0] = object0;
	cmdAttr->object[1] = object1;
	cmdAttr->object[2] = object2;
	cmdAttr->object[3] = object3;
}

NSMutableDictionary* DeviceInfoDictionaryWithCommandAttributes(const CommandAttributes *cmdAttr,
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
										  [NSNumber numberWithBool:cmdAttr->useInputForSender],			@"useInputForSender",
										  nil];
	
	// Set the object references last since these could be nil.
	[newDeviceInfo setValue:cmdAttr->object[0] forKey:@"object0"];
	[newDeviceInfo setValue:cmdAttr->object[1] forKey:@"object1"];
	[newDeviceInfo setValue:cmdAttr->object[2] forKey:@"object2"];
	[newDeviceInfo setValue:cmdAttr->object[3] forKey:@"object3"];
	
	return newDeviceInfo;
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
		
		const size_t hidInputCount = hidInputList.size();
		for (size_t i = 0; i < hidInputCount; i++)
		{
			if (hidInputList[i].deviceCode[0] == '\0' || hidInputList[i].elementCode[0] == '\0')
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
	
	InputAttributes inputAttr;
	strncpy(inputAttr.deviceCode, "NSEventKeyboard", INPUT_HANDLER_STRING_LENGTH);
	strncpy(inputAttr.deviceName, "Keyboard", INPUT_HANDLER_STRING_LENGTH);
	snprintf(inputAttr.elementCode, INPUT_HANDLER_STRING_LENGTH, "%d", keyCode);
	strncpy(inputAttr.elementName, (elementName.empty()) ? inputAttr.elementCode : elementName.c_str(), INPUT_HANDLER_STRING_LENGTH);
	
	inputAttr.state			= (keyPressed) ? INPUT_ATTRIBUTE_STATE_ON : INPUT_ATTRIBUTE_STATE_OFF;
	inputAttr.intCoordX		= 0;
	inputAttr.intCoordY		= 0;
	inputAttr.floatCoordX	= 0.0f;
	inputAttr.floatCoordY	= 0.0f;
	inputAttr.scalar		= (keyPressed) ? 1.0f : 0.0f;
	inputAttr.sender		= nil;
	
	return inputAttr;
}

InputAttributes InputManagerEncodeMouseButtonInput(const NSInteger buttonNumber, const NSPoint touchLoc, BOOL buttonPressed)
{
	InputAttributes inputAttr;
	strncpy(inputAttr.deviceCode, "NSEventMouse", INPUT_HANDLER_STRING_LENGTH);
	strncpy(inputAttr.deviceName, "Mouse", INPUT_HANDLER_STRING_LENGTH);
	snprintf(inputAttr.elementCode, INPUT_HANDLER_STRING_LENGTH, "%i", (const int)buttonNumber);
	
	switch (buttonNumber)
	{
		case kCGMouseButtonLeft:
			strncpy(inputAttr.elementName, "Primary Button", INPUT_HANDLER_STRING_LENGTH);
			break;
			
		case kCGMouseButtonRight:
			strncpy(inputAttr.elementName, "Secondary Button", INPUT_HANDLER_STRING_LENGTH);
			break;
			
		case kCGMouseButtonCenter:
			strncpy(inputAttr.elementName, "Center Button", INPUT_HANDLER_STRING_LENGTH);
			break;
			
		default:
			snprintf(inputAttr.elementName, INPUT_HANDLER_STRING_LENGTH, "Button %i", (const int)buttonNumber);
			break;
	}
	
	inputAttr.state			= (buttonPressed) ? INPUT_ATTRIBUTE_STATE_ON : INPUT_ATTRIBUTE_STATE_OFF;
	inputAttr.intCoordX		= (int32_t)touchLoc.x;
	inputAttr.intCoordY		= (int32_t)touchLoc.y;
	inputAttr.floatCoordX	= touchLoc.x;
	inputAttr.floatCoordY	= touchLoc.y;
	inputAttr.scalar		= (buttonPressed) ? 1.0f : 0.0f;
	inputAttr.sender		= nil;
	
	return inputAttr;
}

InputAttributes InputManagerEncodeIBAction(const SEL theSelector, id sender)
{
	InputAttributes inputAttr;
	strncpy(inputAttr.deviceCode, "IBAction", INPUT_HANDLER_STRING_LENGTH);
	strncpy(inputAttr.deviceName, "Application", INPUT_HANDLER_STRING_LENGTH);
	strncpy(inputAttr.elementCode, sel_getName(theSelector), INPUT_HANDLER_STRING_LENGTH);
	strncpy(inputAttr.elementName, inputAttr.elementCode, INPUT_HANDLER_STRING_LENGTH);
	
	inputAttr.state			= INPUT_ATTRIBUTE_STATE_ON;
	inputAttr.intCoordX		= 0;
	inputAttr.intCoordY		= 0;
	inputAttr.floatCoordX	= 0.0f;
	inputAttr.floatCoordY	= 0.0f;
	inputAttr.scalar		= 1.0f;
	inputAttr.sender		= sender;
	
	return inputAttr;
}

@end
