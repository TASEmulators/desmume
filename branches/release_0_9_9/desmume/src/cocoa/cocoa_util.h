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

#import <Foundation/Foundation.h>
#include "utilities.h"

#if MAC_OS_X_VERSION_MAX_ALLOWED <= MAC_OS_X_VERSION_10_4
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

+ (NSInteger) appVersionNumeric;
+ (NSString *) appInternalVersionString;
+ (NSString *) appInternalNameAndVersionString;
+ (NSString *) appCompilerDetailString;

+ (NSString *) operatingSystemString;
+ (NSString *) modelIdentifierString;

@end

#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_4

#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_5
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

@property (assign) NSThread *thread;
@property (assign) BOOL threadExit;
@property (assign) NSTimeInterval autoreleaseInterval;
@property (assign) NSPort *sendPort;
@property (assign) NSPort *receivePort;

- (id) initWithAutoreleaseInterval:(NSTimeInterval)interval;
- (void) runThread:(id)object;
- (void) forceThreadExit;

@end

#endif

@interface NSNotificationCenter (MainThread)

- (void)postNotificationOnMainThread:(NSNotification *)notification;
- (void)postNotificationOnMainThreadName:(NSString *)aName object:(id)anObject;
- (void)postNotificationOnMainThreadName:(NSString *)aName object:(id)anObject userInfo:(NSDictionary *)aUserInfo;

@end
