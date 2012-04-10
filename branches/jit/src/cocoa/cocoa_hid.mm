/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2012 DeSmuME team

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

#import "cocoa_hid.h"
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

#if !__LP64__ || MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_5

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
	
#endif // !__LP64__ || MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_5

@implementation CocoaHIDDevice

@synthesize hidDeviceRef;
@dynamic runLoop;

static NSDictionary *hidUsageTable = nil;

- (id)init
{
	return [self initWithDevice:nil];
}

- (id) initWithDevice:(IOHIDDeviceRef)theDevice
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
	
	self.runLoop = nil;
	
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
		IOHIDQueueRegisterValueAvailableCallback(hidQueueRef, HandleQueueValueAvailableCallback, self);
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
	return [CocoaHIDDevice manufacturerNameFromHIDDevice:self];
}

- (NSString *) productName
{
	return [CocoaHIDDevice productNameFromHIDDevice:self];
}

- (NSString *) serialNumber
{
	return [CocoaHIDDevice serialNumberFromHIDDevice:self];
}

/********************************************************************************************
	inputAttributesOfHIDValue:altElementCode:altElementName:inputState:

	Parses an IOHIDValueRef to return an input attributes NSDictionary.

	Takes:
		hidValueRef - The IOHIDValueRef to parse.
		altElementCode - An NSString that overrides the default element code.
		altElementName - An NSString that overrides the default element name.
		altOnState - An NSNumber that overrides the default on state.

	Returns:
		An input attributes NSDictionary.

	Details:
		None.
 ********************************************************************************************/
+ (NSDictionary *) inputAttributesOfHIDValue:(IOHIDValueRef)hidValueRef altElementCode:(NSString *)altElementCode altElementName:(NSString *)altElementName inputState:(NSNumber *)altOnState
{
	if (hidValueRef == NULL)
	{
		return nil;
	}
	
	IOHIDElementRef hidElementRef = IOHIDValueGetElement(hidValueRef);
	NSInteger elementUsagePage = IOHIDElementGetUsagePage(hidElementRef);
	NSInteger elementUsage = IOHIDElementGetUsage(hidElementRef);
	
	NSString *elementCode = nil;
	if (altElementCode == nil)
	{
		elementCode = [NSString stringWithFormat:@"0x%04X/0x%04X", elementUsagePage, elementUsage];
	}
	else
	{
		elementCode = altElementCode;
	}
	
	NSString *elementName = nil;
	if (altElementName == nil)
	{
		
		CFStringRef cfElementName = IOHIDElementGetName(hidElementRef);
		if (cfElementName == nil)
		{
			if (elementUsagePage == kHIDPage_Button)
			{
				elementName = [NSString stringWithFormat:@"Button %i", elementUsage];
			}
			else if (elementUsagePage == kHIDPage_VendorDefinedStart)
			{
				elementName = [NSString stringWithFormat:@"VendorDefined %i", elementUsage];
			}
			else
			{
				NSDictionary *elementUsagePageDict = (NSDictionary *)[hidUsageTable valueForKey:[NSString stringWithFormat:@"0x%04X", elementUsagePage]];
				elementName = (NSString *)[elementUsagePageDict valueForKey:[NSString stringWithFormat:@"0x%04X", elementUsage]];
			}
		}
		else
		{
			elementName = [NSString stringWithString:(NSString *)cfElementName];
		}
	}
	else
	{
		elementName = altElementName;
	}
	
	IOHIDDeviceRef hidDeviceRef = IOHIDElementGetDevice(hidElementRef);
	
	CFNumberRef cfVendorIDNumber = (CFNumberRef)IOHIDDeviceGetProperty(hidDeviceRef, CFSTR(kIOHIDVendorIDKey));
	CFNumberRef cfProductIDNumber = (CFNumberRef)IOHIDDeviceGetProperty(hidDeviceRef, CFSTR(kIOHIDProductIDKey));
	UInt32 vendorID = [(NSNumber *)cfVendorIDNumber integerValue];
	UInt32 productID = [(NSNumber *)cfProductIDNumber integerValue];
	
	NSString *deviceCode = [NSString stringWithFormat:@"%d/%d/", vendorID, productID];
	CFStringRef cfDeviceCode = (CFStringRef)IOHIDDeviceGetProperty(hidDeviceRef, CFSTR(kIOHIDSerialNumberKey));
	if (cfDeviceCode == nil)
	{
		CFNumberRef cfLocationIDNumber = (CFNumberRef)IOHIDDeviceGetProperty(hidDeviceRef, CFSTR(kIOHIDLocationIDKey));
		UInt32 locationID = [(NSNumber *)cfLocationIDNumber integerValue];
		deviceCode = [deviceCode stringByAppendingFormat:@"0x%08X", locationID];
	}
	else
	{
		deviceCode = [deviceCode stringByAppendingString:(NSString *)cfDeviceCode];
	}
	
	NSString *deviceName = nil;
	CFStringRef cfDeviceName = (CFStringRef)IOHIDDeviceGetProperty(hidDeviceRef, CFSTR(kIOHIDProductKey));
	if (cfDeviceName == nil)
	{
		deviceName = deviceCode;
	}
	else
	{
		deviceName = [NSString stringWithString:(NSString *)cfDeviceName];
	}
	
	NSNumber *onState = nil;
	if (altOnState == nil)
	{
		onState = [NSNumber numberWithBool:[CocoaHIDDevice onStateFromHIDValue:hidValueRef]];
	}
	else
	{
		onState = altOnState;
	}
	
	NSInteger logicalValue = IOHIDValueGetIntegerValue(hidValueRef);
	
	return [NSDictionary dictionaryWithObjectsAndKeys:
			deviceCode, @"deviceCode",
			deviceName, @"deviceName",
			elementCode, @"elementCode",
			elementName, @"elementName",
			onState, @"on",
			[NSNumber numberWithInteger:logicalValue], @"integerValue",
			[NSNumber numberWithFloat:(float)logicalValue], @"floatValue",
			nil];
}

+ (NSString *) manufacturerNameFromHIDDevice:(CocoaHIDDevice *)hidDevice
{
	return (NSString *)IOHIDDeviceGetProperty(hidDevice.hidDeviceRef, CFSTR(kIOHIDManufacturerKey));
}

+ (NSString *) productNameFromHIDDevice:(CocoaHIDDevice *)hidDevice
{
	return (NSString *)IOHIDDeviceGetProperty(hidDevice.hidDeviceRef, CFSTR(kIOHIDProductKey));
}

+ (NSString *) serialNumberFromHIDDevice:(CocoaHIDDevice *)hidDevice
{
	return (NSString *)IOHIDDeviceGetProperty(hidDevice.hidDeviceRef, CFSTR(kIOHIDSerialNumberKey));
}

+ (NSMutableArray *) inputArrayFromHIDValue:(IOHIDValueRef)hidValueRef
{
	NSMutableArray *inputAttributesList = nil;
	
	if (hidValueRef == NULL)
	{
		return inputAttributesList;
	}
	
	IOHIDElementRef hidElementRef = IOHIDValueGetElement(hidValueRef);
	NSInteger elementUsagePage = IOHIDElementGetUsagePage(hidElementRef);
	NSInteger elementUsage = IOHIDElementGetUsage(hidElementRef);
	
	// IOHIDValueGetIntegerValue() will crash if the value length is too large.
	// Do a bounds check here to prevent crashing. This workaround makes the PS3
	// controller usable, since it returns a length of 39 on some elements.
	if(IOHIDValueGetLength(hidValueRef) > 2)
	{
		return inputAttributesList;
	}
	
	NSInteger logicalValue = IOHIDValueGetIntegerValue(hidValueRef);
	NSInteger logicalMin = IOHIDElementGetLogicalMin(hidElementRef);
	NSInteger logicalMax = IOHIDElementGetLogicalMax(hidElementRef);
	
	inputAttributesList = [NSMutableArray arrayWithCapacity:2];
	
	if (logicalMin == 0 && logicalMax == 1)
	{
		[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:nil altElementName:nil inputState:nil]];
	}
	else
	{
		NSInteger lowerThreshold = ((logicalMax - logicalMin) / 3) + logicalMin;
		NSInteger upperThreshold = (((logicalMax - logicalMin) * 2) / 3) + logicalMin;
		NSNumber *onState = [NSNumber numberWithBool:YES];
		NSNumber *offState = [NSNumber numberWithBool:NO];
		NSString *elementCodeLowerThreshold = [NSString stringWithFormat:@"0x%04X/0x%04X/LowerThreshold", elementUsagePage, elementUsage];
		NSString *elementCodeUpperThreshold = [NSString stringWithFormat:@"0x%04X/0x%04X/UpperThreshold", elementUsagePage, elementUsage];
		
		if (logicalValue <= lowerThreshold)
		{
			[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeLowerThreshold altElementName:nil inputState:onState]];
			[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeUpperThreshold altElementName:nil inputState:offState]];
		}
		else if (logicalValue >= upperThreshold)
		{
			[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeLowerThreshold altElementName:nil inputState:offState]];
			[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeUpperThreshold altElementName:nil inputState:onState]];
		}
		else
		{
			[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeLowerThreshold altElementName:nil inputState:offState]];
			[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeUpperThreshold altElementName:nil inputState:offState]];
		}
	}
	
	return inputAttributesList;
}

+ (NSMutableArray *) inputArrayFromHatSwitchValue:(IOHIDValueRef)hidValueRef useEightDirection:(BOOL)useEightDirection
{
	NSMutableArray *inputAttributesList = nil;
	
	if (hidValueRef == NULL)
	{
		return inputAttributesList;
	}
	
	IOHIDElementRef hidElementRef = IOHIDValueGetElement(hidValueRef);
	NSInteger elementUsagePage = IOHIDElementGetUsagePage(hidElementRef);
	NSInteger elementUsage = IOHIDElementGetUsage(hidElementRef);
	
	if (elementUsage != kHIDUsage_GD_Hatswitch)
	{
		return inputAttributesList;
	}
	
	inputAttributesList = [NSMutableArray arrayWithCapacity:8];
	NSInteger logicalMax = IOHIDElementGetLogicalMax(hidElementRef);
	NSInteger logicalValue = IOHIDValueGetIntegerValue(hidValueRef);
	
	NSNumber *onState = [NSNumber numberWithBool:YES];
	NSNumber *offState = [NSNumber numberWithBool:NO];
	NSString *elementCodeFourWayUp = [NSString stringWithFormat:@"0x%04X/0x%04X/%d-FourDirection", elementUsagePage, elementUsage, 0];
	NSString *elementCodeFourWayRight = [NSString stringWithFormat:@"0x%04X/0x%04X/%d-FourDirection", elementUsagePage, elementUsage, 1];
	NSString *elementCodeFourWayDown = [NSString stringWithFormat:@"0x%04X/0x%04X/%d-FourDirection", elementUsagePage, elementUsage, 2];
	NSString *elementCodeFourWayLeft = [NSString stringWithFormat:@"0x%04X/0x%04X/%d-FourDirection", elementUsagePage, elementUsage, 3];
	NSString *elementCodeEightWayUp = [NSString stringWithFormat:@"0x%04X/0x%04X/%d-EightDirection", elementUsagePage, elementUsage, 0];
	NSString *elementCodeEightWayUpRight = [NSString stringWithFormat:@"0x%04X/0x%04X/%d-EightDirection", elementUsagePage, elementUsage, 1];
	NSString *elementCodeEightWayRight = [NSString stringWithFormat:@"0x%04X/0x%04X/%d-EightDirection", elementUsagePage, elementUsage, 2];
	NSString *elementCodeEightWayDownRight = [NSString stringWithFormat:@"0x%04X/0x%04X/%d-EightDirection", elementUsagePage, elementUsage, 3];
	NSString *elementCodeEightWayDown = [NSString stringWithFormat:@"0x%04X/0x%04X/%d-EightDirection", elementUsagePage, elementUsage, 4];
	NSString *elementCodeEightWayDownLeft = [NSString stringWithFormat:@"0x%04X/0x%04X/%d-EightDirection", elementUsagePage, elementUsage, 5];
	NSString *elementCodeEightWayLeft = [NSString stringWithFormat:@"0x%04X/0x%04X/%d-EightDirection", elementUsagePage, elementUsage, 6];
	NSString *elementCodeEightWayUpLeft = [NSString stringWithFormat:@"0x%04X/0x%04X/%d-EightDirection", elementUsagePage, elementUsage, 7];
	
	if (logicalMax == 3)
	{
		switch (logicalValue)
		{
			case 0:
				[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeFourWayUp altElementName:@"Hatswitch - Up" inputState:onState]];
				[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeFourWayRight altElementName:@"Hatswitch - Right" inputState:offState]];
				[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeFourWayDown altElementName:@"Hatswitch - Down" inputState:offState]];
				[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeFourWayLeft altElementName:@"Hatswitch - Left" inputState:offState]];
				break;
				
			case 1:
				[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeFourWayUp altElementName:@"Hatswitch - Up" inputState:offState]];
				[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeFourWayRight altElementName:@"Hatswitch - Right" inputState:onState]];
				[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeFourWayDown altElementName:@"Hatswitch - Down" inputState:offState]];
				[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeFourWayLeft altElementName:@"Hatswitch - Left" inputState:offState]];
				break;
				
			case 2:
				[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeFourWayUp altElementName:@"Hatswitch - Up" inputState:offState]];
				[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeFourWayRight altElementName:@"Hatswitch - Right" inputState:offState]];
				[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeFourWayDown altElementName:@"Hatswitch - Down" inputState:onState]];
				[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeFourWayLeft altElementName:@"Hatswitch - Left" inputState:offState]];
				break;
				
			case 3:
				[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeFourWayUp altElementName:@"Hatswitch - Up" inputState:offState]];
				[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeFourWayRight altElementName:@"Hatswitch - Right" inputState:offState]];
				[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeFourWayDown altElementName:@"Hatswitch - Down" inputState:offState]];
				[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeFourWayLeft altElementName:@"Hatswitch - Left" inputState:onState]];
				break;
				
			default:
				[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeFourWayUp altElementName:@"Hatswitch - Up" inputState:offState]];
				[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeFourWayRight altElementName:@"Hatswitch - Right" inputState:offState]];
				[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeFourWayDown altElementName:@"Hatswitch - Down" inputState:offState]];
				[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeFourWayLeft altElementName:@"Hatswitch - Left" inputState:offState]];
				break;
		}
	}
	else if (logicalMax == 7)
	{
		if (useEightDirection)
		{
			switch (logicalValue)
			{
				case 0:
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayUp altElementName:@"Hatswitch - Up" inputState:onState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayUpRight altElementName:@"Hatswitch - Up/Right" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayRight altElementName:@"Hatswitch - Right" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayDownRight altElementName:@"Hatswitch - Down/Right" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayDown altElementName:@"Hatswitch - Down" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayDownLeft altElementName:@"Hatswitch - Down/Left" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayLeft altElementName:@"Hatswitch - Left" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayUpLeft altElementName:@"Hatswitch - Up/Left" inputState:offState]];
					break;
					
				case 1:
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayUp altElementName:@"Hatswitch - Up" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayUpRight altElementName:@"Hatswitch - Up/Right" inputState:onState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayRight altElementName:@"Hatswitch - Right" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayDownRight altElementName:@"Hatswitch - Down/Right" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayDown altElementName:@"Hatswitch - Down" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayDownLeft altElementName:@"Hatswitch - Down/Left" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayLeft altElementName:@"Hatswitch - Left" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayUpLeft altElementName:@"Hatswitch - Up/Left" inputState:offState]];
					break;
					
				case 2:
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayUp altElementName:@"Hatswitch - Up" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayUpRight altElementName:@"Hatswitch - Up/Right" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayRight altElementName:@"Hatswitch - Right" inputState:onState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayDownRight altElementName:@"Hatswitch - Down/Right" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayDown altElementName:@"Hatswitch - Down" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayDownLeft altElementName:@"Hatswitch - Down/Left" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayLeft altElementName:@"Hatswitch - Left" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayUpLeft altElementName:@"Hatswitch - Up/Left" inputState:offState]];
					break;
					
				case 3:
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayUp altElementName:@"Hatswitch - Up" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayUpRight altElementName:@"Hatswitch - Up/Right" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayRight altElementName:@"Hatswitch - Right" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayDownRight altElementName:@"Hatswitch - Down/Right" inputState:onState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayDown altElementName:@"Hatswitch - Down" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayDownLeft altElementName:@"Hatswitch - Down/Left" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayLeft altElementName:@"Hatswitch - Left" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayUpLeft altElementName:@"Hatswitch - Up/Left" inputState:offState]];
					break;
					
				case 4:
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayUp altElementName:@"Hatswitch - Up" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayUpRight altElementName:@"Hatswitch - Up/Right" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayRight altElementName:@"Hatswitch - Right" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayDownRight altElementName:@"Hatswitch - Down/Right" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayDown altElementName:@"Hatswitch - Down" inputState:onState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayDownLeft altElementName:@"Hatswitch - Down/Left" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayLeft altElementName:@"Hatswitch - Left" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayUpLeft altElementName:@"Hatswitch - Up/Left" inputState:offState]];
					break;
					
				case 5:
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayUp altElementName:@"Hatswitch - Up" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayUpRight altElementName:@"Hatswitch - Up/Right" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayRight altElementName:@"Hatswitch - Right" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayDownRight altElementName:@"Hatswitch - Down/Right" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayDown altElementName:@"Hatswitch - Down" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayDownLeft altElementName:@"Hatswitch - Down/Left" inputState:onState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayLeft altElementName:@"Hatswitch - Left" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayUpLeft altElementName:@"Hatswitch - Up/Left" inputState:offState]];
					break;
					
				case 6:
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayUp altElementName:@"Hatswitch - Up" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayUpRight altElementName:@"Hatswitch - Up/Right" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayRight altElementName:@"Hatswitch - Right" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayDownRight altElementName:@"Hatswitch - Down/Right" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayDown altElementName:@"Hatswitch - Down" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayDownLeft altElementName:@"Hatswitch - Down/Left" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayLeft altElementName:@"Hatswitch - Left" inputState:onState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayUpLeft altElementName:@"Hatswitch - Up/Left" inputState:offState]];
					break;
					
				case 7:
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayUp altElementName:@"Hatswitch - Up" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayUpRight altElementName:@"Hatswitch - Up/Right" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayRight altElementName:@"Hatswitch - Right" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayDownRight altElementName:@"Hatswitch - Down/Right" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayDown altElementName:@"Hatswitch - Down" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayDownLeft altElementName:@"Hatswitch - Down/Left" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayLeft altElementName:@"Hatswitch - Left" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayUpLeft altElementName:@"Hatswitch - Up/Left" inputState:onState]];
					break;
					
				default:
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayUp altElementName:@"Hatswitch - Up" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayUpRight altElementName:@"Hatswitch - Up/Right" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayRight altElementName:@"Hatswitch - Right" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayDownRight altElementName:@"Hatswitch - Down/Right" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayDown altElementName:@"Hatswitch - Down" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayDownLeft altElementName:@"Hatswitch - Down/Left" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayLeft altElementName:@"Hatswitch - Left" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayUpLeft altElementName:@"Hatswitch - Up/Left" inputState:offState]];
					break;
			}
		}
		else
		{
			switch (logicalValue)
			{
				case 0:
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayUp altElementName:@"Hatswitch - Up" inputState:onState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayRight altElementName:@"Hatswitch - Right" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayDown altElementName:@"Hatswitch - Down" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayLeft altElementName:@"Hatswitch - Left" inputState:offState]];
					break;
					
				case 1:
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayUp altElementName:@"Hatswitch - Up" inputState:onState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayRight altElementName:@"Hatswitch - Right" inputState:onState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayDown altElementName:@"Hatswitch - Down" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayLeft altElementName:@"Hatswitch - Left" inputState:offState]];
					break;
					
				case 2:
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayUp altElementName:@"Hatswitch - Up" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayRight altElementName:@"Hatswitch - Right" inputState:onState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayDown altElementName:@"Hatswitch - Down" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayLeft altElementName:@"Hatswitch - Left" inputState:offState]];
					break;
					
				case 3:
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayUp altElementName:@"Hatswitch - Up" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayRight altElementName:@"Hatswitch - Right" inputState:onState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayDown altElementName:@"Hatswitch - Down" inputState:onState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayLeft altElementName:@"Hatswitch - Left" inputState:offState]];
					break;
					
				case 4:
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayUp altElementName:@"Hatswitch - Up" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayRight altElementName:@"Hatswitch - Right" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayDown altElementName:@"Hatswitch - Down" inputState:onState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayLeft altElementName:@"Hatswitch - Left" inputState:offState]];
					break;
					
				case 5:
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayUp altElementName:@"Hatswitch - Up" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayRight altElementName:@"Hatswitch - Right" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayDown altElementName:@"Hatswitch - Down" inputState:onState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayLeft altElementName:@"Hatswitch - Left" inputState:onState]];
					break;
					
				case 6:
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayUp altElementName:@"Hatswitch - Up" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayRight altElementName:@"Hatswitch - Right" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayDown altElementName:@"Hatswitch - Down" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayLeft altElementName:@"Hatswitch - Left" inputState:onState]];
					break;
					
				case 7:
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayUp altElementName:@"Hatswitch - Up" inputState:onState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayRight altElementName:@"Hatswitch - Right" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayDown altElementName:@"Hatswitch - Down" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayLeft altElementName:@"Hatswitch - Left" inputState:onState]];
					break;
					
				default:
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayUp altElementName:@"Hatswitch - Up" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayRight altElementName:@"Hatswitch - Right" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayDown altElementName:@"Hatswitch - Down" inputState:offState]];
					[inputAttributesList addObject:[CocoaHIDDevice inputAttributesOfHIDValue:hidValueRef altElementCode:elementCodeEightWayLeft altElementName:@"Hatswitch - Left" inputState:offState]];
					break;
			}
		}
	}
	
	return inputAttributesList;
}

+ (BOOL) onStateFromHIDValue:(IOHIDValueRef)hidValueRef
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

@end


@implementation CocoaHIDManager

@synthesize hidManagerRef;
@synthesize deviceList;
@dynamic runLoop;

- (id)init
{
	self = [super init];
	if(self == nil)
	{
		return self;
	}
	
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
	self.runLoop = nil;
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
	CocoaHIDManager *hidManager = (CocoaHIDManager *)inContext;
	CocoaHIDDevice *newDevice = [[[CocoaHIDDevice alloc] initWithDevice:inIOHIDDeviceRef] autorelease];
	[hidManager.deviceList addObject:newDevice];
	[newDevice start];
}

void HandleDeviceRemovalCallback(void *inContext, IOReturn inResult, void *inSender, IOHIDDeviceRef inIOHIDDeviceRef)
{
	CocoaHIDManager *hidManager = (CocoaHIDManager *)inContext;
	
	for (CocoaHIDDevice *hidDevice in hidManager.deviceList)
	{
		if (hidDevice.hidDeviceRef == inIOHIDDeviceRef)
		{
			[hidManager.deviceList removeObject:hidDevice];
			break;
		}
	}
}

void HandleQueueValueAvailableCallback(void *inContext, IOReturn inResult, void *inSender)
{
	IOHIDQueueRef hidQueue = (IOHIDQueueRef)inSender;
	NSMutableArray *inputAttributesList = nil;
	NSAutoreleasePool *hidPool = [[NSAutoreleasePool alloc] init];
	
	do
	{
		IOHIDValueRef hidValueRef = IOHIDQueueCopyNextValueWithTimeout(hidQueue, 0.0);
		if (hidValueRef == NULL)
		{
			break;
		}
		
		NSMutableArray *hatSwitchInput = [CocoaHIDDevice inputArrayFromHatSwitchValue:hidValueRef useEightDirection:NO];
		if (hatSwitchInput != nil)
		{
			inputAttributesList = hatSwitchInput;
		}
		else
		{
			inputAttributesList = [CocoaHIDDevice inputArrayFromHIDValue:hidValueRef];
		}
		
		if (inputAttributesList != nil)
		{
			// HID input devices don't register events, so we need to manually prevent
			// sleep and screensaver whenever we detect an input.
			UpdateSystemActivity(UsrActivity);
			
			[[NSNotificationCenter defaultCenter] postNotificationOnMainThreadName:@"org.desmume.DeSmuME.hidInputDetected" object:inputAttributesList userInfo:nil];
		}
		
		CFRelease(hidValueRef);
	} while (1);
	
	[hidPool release];
}
