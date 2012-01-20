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

#import <Cocoa/Cocoa.h>
#include <IOKit/hid/IOHIDManager.h>


@interface CocoaHIDDevice : NSObject
{
	IOHIDDeviceRef hidDeviceRef;
	IOHIDQueueRef hidQueueRef;
	NSRunLoop *runLoop;
}

@property (readonly) IOHIDDeviceRef hidDeviceRef;
@property (assign) NSRunLoop *runLoop;

- (id) initWithDevice:(IOHIDDeviceRef)theDevice;

- (void) start;
- (void) stop;

- (NSString *) manufacturerName;
- (NSString *) productName;
- (NSString *) serialNumber;

+ (NSString *) manufacturerNameFromHIDDevice:(CocoaHIDDevice *)hidDevice;
+ (NSString *) productNameFromHIDDevice:(CocoaHIDDevice *)hidDevice;
+ (NSString *) serialNumberFromHIDDevice:(CocoaHIDDevice *)hidDevice;

+ (NSMutableArray *) inputArrayFromHIDValue:(IOHIDValueRef)hidValueRef;
+ (NSDictionary *) inputAttributesOfHIDValue:(IOHIDValueRef)hidValueRef altElementCode:(NSString *)altElementCode altElementName:(NSString *)altElementName inputState:(NSNumber *)altOnState;
+ (NSMutableArray *) inputArrayFromHatSwitchValue:(IOHIDValueRef)hidValueRef useEightDirection:(BOOL)useEightDirection;
+ (BOOL) onStateFromHIDValue:(IOHIDValueRef)hidValueRef;

@end


@interface CocoaHIDManager : NSObject
{
	IOHIDManagerRef hidManagerRef;
	NSRunLoop *runLoop;
	NSMutableSet *deviceList;
}

@property (readonly) IOHIDManagerRef hidManagerRef;
@property (readonly) NSMutableSet *deviceList;
@property (assign) NSRunLoop *runLoop;

@end

void HandleDeviceMatchingCallback(void *inContext, IOReturn inResult, void *inSender, IOHIDDeviceRef inIOHIDDeviceRef);
void HandleDeviceRemovalCallback(void *inContext, IOReturn inResult, void *inSender, IOHIDDeviceRef inIOHIDDeviceRef);
void HandleQueueValueAvailableCallback(void *inContext, IOReturn inResult, void *inSender);
