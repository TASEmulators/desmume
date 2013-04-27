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

#import "cocoa_util.h"
#import "cocoa_globals.h"
#import "types.h"
#import <Cocoa/Cocoa.h>

#include <sys/types.h>
#include <sys/sysctl.h>
#include "../version.h"

#undef BOOL


@implementation CocoaDSUtil

static NSDate *distantFutureDate = [[NSDate distantFuture] retain];

+ (void) messageSendOneWay:(NSPort *)sendPort msgID:(NSInteger)msgID
{
	if (sendPort == nil || ![sendPort isValid])
	{
		return;
	}
	
	NSPortMessage *message = [[NSPortMessage alloc] initWithSendPort:sendPort receivePort:nil components:nil];
	[message setMsgid:msgID];
	[message sendBeforeDate:distantFutureDate];
	[message release];
}

+ (void) messageSendOneWayWithMessageComponents:(NSPort *)sendPort msgID:(NSInteger)msgID array:(NSArray *)msgDataArray
{
	if (sendPort == nil || ![sendPort isValid])
	{
		return;
	}
	
	NSPortMessage *message = [[NSPortMessage alloc] initWithSendPort:sendPort receivePort:nil components:msgDataArray];
	[message setMsgid:msgID];
	[message sendBeforeDate:distantFutureDate];
	[message release];
}

+ (void) messageSendOneWayWithData:(NSPort *)sendPort msgID:(NSInteger)msgID data:(NSData *)msgData
{
	if (sendPort == nil || ![sendPort isValid])
	{
		return;
	}
	
	NSArray *messageComponents = [[NSArray alloc] initWithObjects:msgData, nil];
	[CocoaDSUtil messageSendOneWayWithMessageComponents:sendPort msgID:msgID array:messageComponents];
	[messageComponents release];
}

+ (void) messageSendOneWayWithInteger:(NSPort *)sendPort msgID:(NSInteger)msgID integerValue:(NSInteger)integerValue
{
	if (sendPort == nil || ![sendPort isValid])
	{
		return;
	}
	
	NSData *messageData = [[NSData alloc] initWithBytes:&integerValue length:sizeof(NSInteger)];
	[CocoaDSUtil messageSendOneWayWithData:sendPort msgID:msgID data:messageData];
	[messageData release];
}

+ (void) messageSendOneWayWithFloat:(NSPort *)sendPort msgID:(NSInteger)msgID floatValue:(float)floatValue
{
	if (sendPort == nil || ![sendPort isValid])
	{
		return;
	}
	
	NSData *messageData = [[NSData alloc] initWithBytes:&floatValue length:sizeof(float)];
	[CocoaDSUtil messageSendOneWayWithData:sendPort msgID:msgID data:messageData];
	[messageData release];
}

+ (void) messageSendOneWayWithBool:(NSPort *)sendPort msgID:(NSInteger)msgID boolValue:(BOOL)boolValue
{
	if (sendPort == nil || ![sendPort isValid])
	{
		return;
	}
	
	NSData *messageData = [[NSData alloc] initWithBytes:&boolValue length:sizeof(BOOL)];
	[CocoaDSUtil messageSendOneWayWithData:sendPort msgID:msgID data:messageData];
	[messageData release];
}

+ (void) messageSendOneWayWithRect:(NSPort *)sendPort msgID:(NSInteger)msgID rect:(NSRect)rect
{
	if (sendPort == nil || ![sendPort isValid])
	{
		return;
	}
	
	NSData *messageData = [[NSData alloc] initWithBytes:&rect length:sizeof(NSRect)];
	[CocoaDSUtil messageSendOneWayWithData:sendPort msgID:msgID data:messageData];
	[messageData release];
}

+ (NSInteger) getIBActionSenderTag:(id)sender
{
	NSInteger senderTag = 0;
	if ([sender isKindOfClass:[NSButton class]])
	{
		senderTag = [sender tag];
	}
	else if ([sender respondsToSelector:@selector(selectedCell)])
	{
		senderTag = [[sender selectedCell] tag];
	}
	else if ([sender respondsToSelector:@selector(tag)])
	{
		senderTag = [sender tag];
	}
	
	return senderTag;
}

+ (BOOL) getIBActionSenderButtonStateBool:(id)sender
{
	BOOL theState = NO;
	NSInteger buttonState = NSOffState;
	
	if ([sender respondsToSelector:@selector(state)])
	{
		buttonState = [sender state];
	}
	
	if (buttonState == NSOnState)
	{
		theState = YES;
	}
	
	return theState;
}

+ (void) quickDialogUsingTitle:(NSString *)titleText message:(NSString *)messageText
{
	NSRunAlertPanel(titleText, messageText, nil, nil, nil);
}

+ (BOOL) quickYesNoDialogUsingTitle:(NSString *)titleText message:(NSString *)messageText
{
	return NSRunAlertPanel(titleText, messageText, NSLocalizedString(@"Yes", nil), NSLocalizedString(@"No", nil), nil) != 0;
}

+ (NSInteger) appVersionNumeric
{
	return (NSInteger)EMU_DESMUME_VERSION_NUMERIC();
}

+ (NSString *) appInternalVersionString
{
	return [NSString stringWithCString:EMU_DESMUME_VERSION_STRING() encoding:NSUTF8StringEncoding];
}

+ (NSString *) appInternalNameAndVersionString
{
	return [NSString stringWithCString:EMU_DESMUME_NAME_AND_VERSION() encoding:NSUTF8StringEncoding];
}

+ (NSString *) appCompilerDetailString
{
	return [NSString stringWithCString:EMU_DESMUME_COMPILER_DETAIL() encoding:NSUTF8StringEncoding];
}

+ (NSString *) operatingSystemString
{
	NSDictionary *systemDict = [NSDictionary dictionaryWithContentsOfFile:@"/System/Library/CoreServices/SystemVersion.plist"];
	
	NSString *productString = (NSString *)[systemDict objectForKey:@"ProductName"];
	NSString *versionString = (NSString *)[systemDict objectForKey:@"ProductVersion"];
	NSString *buildString = (NSString *)[systemDict objectForKey:@"ProductBuildVersion"];
	
	return [[[[[productString stringByAppendingString:@" v"] stringByAppendingString:versionString] stringByAppendingString:@" ("] stringByAppendingString:buildString] stringByAppendingString:@")"];
}

+ (NSString *) modelIdentifierString
{
	NSString *modelIdentifierStr = @"";
	size_t stringLength = 0;
	char *modelCString = NULL;
	
	sysctlbyname("hw.model", NULL, &stringLength, NULL, 0);
	if (stringLength == 0)
	{
		return modelIdentifierStr;
	}
	
	modelCString = (char *)malloc(stringLength * sizeof(char));
	sysctlbyname("hw.model", modelCString, &stringLength, NULL, 0);
	modelIdentifierStr = [NSString stringWithCString:modelCString encoding:NSUTF8StringEncoding];
	free(modelCString);
	
	return modelIdentifierStr;
}

@end

#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_4
@implementation CocoaDSThread

@synthesize thread;
@synthesize threadExit;
@synthesize autoreleaseInterval;
@synthesize sendPort;
@synthesize receivePort;

- (id)init
{	
	return [self initWithAutoreleaseInterval:15.0];
}

- (id) initWithAutoreleaseInterval:(NSTimeInterval)interval
{
	// Set up thread info.
	thread = nil;
	threadExit = NO;
	autoreleaseInterval = interval;
	
	// Set up thread ports.
	sendPort = [[NSPort port] retain];
	[sendPort setDelegate:self];
	receivePort = [[NSPort port] retain];
	[receivePort setDelegate:self];
	
	return self;
}

- (void)dealloc
{
	[self forceThreadExit];
	
	[super dealloc];
}

- (void) runThread:(id)object
{
	NSAutoreleasePool *threadPool = [[NSAutoreleasePool alloc] init];
	NSRunLoop *runLoop = [NSRunLoop currentRunLoop];
	
	[runLoop addPort:[self receivePort] forMode:NSDefaultRunLoopMode];
	[self setThread:[NSThread currentThread]];
	
	do
	{
		NSAutoreleasePool *runLoopPool = [[NSAutoreleasePool alloc] init];
		NSDate *runDate = [[NSDate alloc] initWithTimeIntervalSinceNow:[self autoreleaseInterval]];
		[runLoop runUntilDate:runDate];
		[runDate release];
		[runLoopPool release];
	} while (![self threadExit]);
	
	NSPort *tempSendPort = [self sendPort];
	[self setSendPort:nil];
	[tempSendPort invalidate];
	[tempSendPort release];
	
	NSPort *tempReceivePort = [self receivePort];
	[self setReceivePort:nil];
	[tempReceivePort invalidate];
	[tempReceivePort release];
	
	[threadPool release];
	[self setThread:nil];
}

- (void) forceThreadExit
{
	if ([self thread] == nil)
	{
		return;
	}
	
	[self setThreadExit:YES];
	
	// Wait until the thread has shut down.
	while ([self thread] != nil)
	{
		[NSThread sleepUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
	}
}

- (void)handlePortMessage:(NSPortMessage *)portMessage
{
	NSInteger message = (NSInteger)[portMessage msgid];
	
	switch (message)
	{
		case MESSAGE_EXIT_THREAD:
			[self setThreadExit:YES];
			break;
			
		default:
			break;
	}
}

@end
#endif

@implementation NSNotificationCenter (MainThread)

- (void)postNotificationOnMainThread:(NSNotification *)notification
{
	[self performSelectorOnMainThread:@selector(postNotification:) withObject:notification waitUntilDone:YES];
}

- (void)postNotificationOnMainThreadName:(NSString *)aName object:(id)anObject
{
	NSNotification *notification = [NSNotification notificationWithName:aName object:anObject];
	[self postNotificationOnMainThread:notification];
}

- (void)postNotificationOnMainThreadName:(NSString *)aName object:(id)anObject userInfo:(NSDictionary *)aUserInfo
{
	NSNotification *notification = [NSNotification notificationWithName:aName object:anObject userInfo:aUserInfo];
	[self postNotificationOnMainThread:notification];
}

@end
