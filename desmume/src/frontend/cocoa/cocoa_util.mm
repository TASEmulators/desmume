/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2012-2017 DeSmuME team

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
#include "../../version.h"

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
	if ([sender isKindOfClass:[NSButton class]] ||
		[sender isKindOfClass:[NSMenuItem class]])
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

+ (NSColor *) NSColorFromRGBA8888:(uint32_t)theColor
{
#ifdef MSB_FIRST
	const CGFloat a = (CGFloat)((theColor >>  0) & 0xFF) / 255.0f;
	const CGFloat b = (CGFloat)((theColor >>  8) & 0xFF) / 255.0f;
	const CGFloat g = (CGFloat)((theColor >> 16) & 0xFF) / 255.0f;
	const CGFloat r = (CGFloat)((theColor >> 24) & 0xFF) / 255.0f;
#else
	const CGFloat r = (CGFloat)((theColor >>  0) & 0xFF) / 255.0f;
	const CGFloat g = (CGFloat)((theColor >>  8) & 0xFF) / 255.0f;
	const CGFloat b = (CGFloat)((theColor >> 16) & 0xFF) / 255.0f;
	const CGFloat a = (CGFloat)((theColor >> 24) & 0xFF) / 255.0f;
#endif
	
	return [NSColor colorWithDeviceRed:r green:g blue:b alpha:a];
}

+ (uint32_t) RGBA8888FromNSColor:(NSColor *)theColor
{
	if (![[theColor colorSpaceName] isEqualToString:NSDeviceRGBColorSpace])
	{
		theColor = [theColor colorUsingColorSpaceName:NSDeviceRGBColorSpace];
		if (theColor == nil)
		{
			return 0x00000000;
		}
	}
	
	CGFloat r, g, b, a;
	[theColor getRed:&r green:&g blue:&b alpha:&a];
	
#ifdef MSB_FIRST
	return (((uint32_t)(a * 255.0f)) <<  0) |
	       (((uint32_t)(b * 255.0f)) <<  8) |
	       (((uint32_t)(g * 255.0f)) << 16) |
	       (((uint32_t)(r * 255.0f)) << 24);
#else
	return (((uint32_t)(r * 255.0f)) <<  0) |
	       (((uint32_t)(g * 255.0f)) <<  8) |
	       (((uint32_t)(b * 255.0f)) << 16) |
	       (((uint32_t)(a * 255.0f)) << 24);
#endif
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
@dynamic idle;
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
	_idleState = NO;
	autoreleaseInterval = interval;
	
	spinlockIdle = OS_SPINLOCK_INIT;
	
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

- (void) setIdle:(BOOL)theState
{
	OSSpinLockLock(&spinlockIdle);
	_idleState = theState;
	OSSpinLockUnlock(&spinlockIdle);
}

- (BOOL) idle
{
	OSSpinLockLock(&spinlockIdle);
	const BOOL theState = _idleState;
	OSSpinLockUnlock(&spinlockIdle);
	
	return theState;
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
	[self setIdle:NO];
	
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

@implementation DirectoryURLDragDestTextField

- (id)initWithCoder:(NSCoder *)coder
{
	self = [super initWithCoder:coder];
	if (self == nil)
	{
		return self;
	}
	
	[self registerForDraggedTypes:[NSArray arrayWithObjects: NSURLPboardType, nil]];
	
	return self;
}

#pragma mark NSDraggingDestination Protocol
- (BOOL)wantsPeriodicDraggingUpdates
{
	return NO;
}

- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
	NSDragOperation dragOp = NSDragOperationNone;
	NSPasteboard *pboard = [sender draggingPasteboard];
	
	if ([[pboard types] containsObject:NSURLPboardType])
	{
		NSURL *fileURL = [NSURL URLFromPasteboard:pboard];
		NSString *filePath = [fileURL path];
		
		NSFileManager *fileManager = [[NSFileManager alloc] init];
		BOOL isDir;
		BOOL dirExists = [fileManager fileExistsAtPath:filePath isDirectory:&isDir];
		
		if (dirExists && isDir)
		{
			dragOp = NSDragOperationLink;
		}
		
		[fileManager release];
	}
	
	return dragOp;
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
	NSWindow *window = [self window];
	id <DirectoryURLDragDestTextFieldProtocol> delegate = (id <DirectoryURLDragDestTextFieldProtocol>)[window delegate];
	NSPasteboard *pboard = [sender draggingPasteboard];
	NSString *filePath = NULL;
	
	if ([[pboard types] containsObject:NSURLPboardType])
	{
		NSURL *fileURL = [NSURL URLFromPasteboard:pboard];
		filePath = [fileURL path];
	}
	
	if (filePath != NULL)
	{
		[delegate assignDirectoryPath:filePath textField:self];
	}
	
	return YES;
}

@end

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

@implementation RGBA8888ToNSColorValueTransformer

+ (Class)transformedValueClass
{
	return [NSColor class];
}

+ (BOOL)allowsReverseTransformation
{
	return YES;
}

- (id)transformedValue:(id)value
{
	if (value == nil)
	{
		return nil;
	}
	
	uint32_t color32 = 0xFFFFFFFF;
	
	if ([value respondsToSelector:@selector(unsignedIntegerValue)])
	{
		color32 = (uint32_t)[value unsignedIntegerValue];
	}
	else if ([value respondsToSelector:@selector(unsignedIntValue)])
	{
		color32 = (uint32_t)[value unsignedIntValue];
	}
	else if ([value respondsToSelector:@selector(integerValue)])
	{
		color32 = (uint32_t)[value integerValue];
	}
	else if ([value respondsToSelector:@selector(intValue)])
	{
		color32 = (uint32_t)[value intValue];
	}
	else
	{
		[NSException raise:NSInternalInconsistencyException format:@"Value (%@) does not respond to -unsignedIntegerValue, -unsignedIntValue, -integerValue or -intValue.", [value class]];
	}
	
	color32 = LE_TO_LOCAL_32(color32);
	return [CocoaDSUtil NSColorFromRGBA8888:color32];
}

- (id)reverseTransformedValue:(id)value
{
	uint32_t color32 = 0xFFFFFFFF;
	
	if (value == nil)
	{
		return nil;
	}
	else if (![value isKindOfClass:[NSColor class]])
	{
		NSLog(@"-reverseTransformedValue is %@", [value class]);
		return [NSNumber numberWithUnsignedInteger:color32];
	}
	
	color32 = [CocoaDSUtil RGBA8888FromNSColor:(NSColor *)value];
	color32 = LE_TO_LOCAL_32(color32);
	return [NSNumber numberWithUnsignedInteger:color32];
}

@end
