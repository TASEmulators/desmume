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

#if MAC_OS_X_VERSION_MIN_REQUIRED <= MAC_OS_X_VERSION_10_4
	#include "macosx_10_4_compat.h"
#endif


@interface CocoaDSUtil : NSObject
{
	
}

+ (void) messageSendOneWay:(NSPort *)sendPort msgID:(NSInteger)msgID;
+ (void) messageSendOneWayWithMessageComponents:(NSPort *)sendPort msgID:(NSInteger)msgID array:(NSArray *)msgDataArray;
+ (void) messageSendOneWayWithData:(NSPort *)sendPort msgID:(NSInteger)msgID data:(NSData *)msgData;
+ (void) messageSendOneWayWithInteger:(NSPort *)sendPort msgID:(NSInteger)msgID integerValue:(NSInteger)integerValue;
+ (void) messageSendOneWayWithFloat:(NSPort *)sendPort msgID:(NSInteger)msgID floatValue:(float)floatValue;
+ (void) messageSendOneWayWithBool:(NSPort *)sendPort msgID:(NSInteger)msgID boolValue:(BOOL)boolValue;
+ (void) messageSendOneWayWithRect:(NSPort *)sendPort msgID:(NSInteger)msgID rect:(NSRect)rect;
+ (NSInteger) getIBActionSenderTag:(id)sender;
+ (BOOL) getIBActionSenderButtonStateBool:(id)sender;

+ (void) quickDialogUsingTitle:(NSString *)titleText message:(NSString *)messageText;
+ (BOOL) quickYesNoDialogUsingTitle:(NSString *)titleText message:(NSString *)messageText;

@end

#if MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_4

#if MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_5
@interface CocoaDSThread : NSObject <NSPortDelegate>
#else
@interface CocoaDSThread : NSObject
#endif
{
	NSThread *thread;
	BOOL threadExit;
	NSTimeInterval autoreleaseInterval;
	NSPort *sendPort;
	NSPort *receivePort;	
}

@property (retain) NSThread *thread;
@property (assign) BOOL threadExit;
@property (assign) NSTimeInterval autoreleaseInterval;
@property (retain) NSPort *sendPort;
@property (readonly) NSPort *receivePort;

- (id) initWithAutoreleaseInterval:(NSTimeInterval)interval;
- (void) runThread:(id)object;

@end

#endif

@interface NSNotificationCenter (MainThread)

- (void)postNotificationOnMainThread:(NSNotification *)notification;
- (void)postNotificationOnMainThreadName:(NSString *)aName object:(id)anObject;
- (void)postNotificationOnMainThreadName:(NSString *)aName object:(id)anObject userInfo:(NSDictionary *)aUserInfo;

@end

#ifdef __cplusplus
extern "C"
{
#endif

uint32_t RGBA5551ToRGBA8888(const uint16_t color16);
uint32_t RGBA8888ToRGB888(const uint32_t color24);
void RGBA5551ToRGBA8888Buffer(const uint16_t *__restrict__ srcBuffer, uint32_t *__restrict__ destBuffer, unsigned int numberPixels);
void RGB888ToRGBA8888Buffer(const uint32_t *__restrict__ srcBuffer, uint32_t *__restrict__ destBuffer, unsigned int numberPixels);
NSSize GetTransformedBounds(NSSize normalBounds, double scalar, double angleDegrees);
double GetMaxScalarInBounds(double normalBoundsWidth, double normalBoundsHeight, double keepInBoundsWidth, double keepInBoundsHeight);
NSPoint GetNormalPointFromTransformedPoint(NSPoint transformedPt, NSSize normalBounds, NSSize transformBounds, double scalar, double angleDegrees);
uint32_t GetNearestPositivePOT(uint32_t value);

#ifdef __cplusplus
}
#endif
