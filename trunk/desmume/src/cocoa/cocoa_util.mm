/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2013 DeSmuME team

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

#include <Accelerate/Accelerate.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include "../version.h"

#undef BOOL


@implementation CocoaDSUtil

static NSDate *distantFutureDate = [[NSDate distantFuture] retain];

+ (void) messageSendOneWay:(NSPort *)sendPort msgID:(NSInteger)msgID
{
	NSPortMessage *message = [[NSPortMessage alloc] initWithSendPort:sendPort receivePort:nil components:nil];
	[message setMsgid:msgID];
	[message sendBeforeDate:distantFutureDate];
	[message release];
}

+ (void) messageSendOneWayWithMessageComponents:(NSPort *)sendPort msgID:(NSInteger)msgID array:(NSArray *)msgDataArray
{
	NSPortMessage *message = [[NSPortMessage alloc] initWithSendPort:sendPort receivePort:nil components:msgDataArray];
	[message setMsgid:msgID];
	[message sendBeforeDate:distantFutureDate];
	[message release];
}

+ (void) messageSendOneWayWithData:(NSPort *)sendPort msgID:(NSInteger)msgID data:(NSData *)msgData
{
	NSArray *messageComponents = [[NSArray alloc] initWithObjects:msgData, nil];
	[CocoaDSUtil messageSendOneWayWithMessageComponents:sendPort msgID:msgID array:messageComponents];
	[messageComponents release];
}

+ (void) messageSendOneWayWithInteger:(NSPort *)sendPort msgID:(NSInteger)msgID integerValue:(NSInteger)integerValue
{
	NSData *messageData = [[NSData alloc] initWithBytes:&integerValue length:sizeof(NSInteger)];
	[CocoaDSUtil messageSendOneWayWithData:sendPort msgID:msgID data:messageData];
	[messageData release];
}

+ (void) messageSendOneWayWithFloat:(NSPort *)sendPort msgID:(NSInteger)msgID floatValue:(float)floatValue
{
	NSData *messageData = [[NSData alloc] initWithBytes:&floatValue length:sizeof(float)];
	[CocoaDSUtil messageSendOneWayWithData:sendPort msgID:msgID data:messageData];
	[messageData release];
}

+ (void) messageSendOneWayWithBool:(NSPort *)sendPort msgID:(NSInteger)msgID boolValue:(BOOL)boolValue
{
	NSData *messageData = [[NSData alloc] initWithBytes:&boolValue length:sizeof(BOOL)];
	[CocoaDSUtil messageSendOneWayWithData:sendPort msgID:msgID data:messageData];
	[messageData release];
}

+ (void) messageSendOneWayWithRect:(NSPort *)sendPort msgID:(NSInteger)msgID rect:(NSRect)rect
{
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

+ (BOOL) OSVersionCheckMajor:(NSUInteger)checkMajor minor:(NSUInteger)checkMinor revision:(NSUInteger)checkRevision
{
	BOOL result = NO;
	
	NSDictionary *systemDict = [NSDictionary dictionaryWithContentsOfFile:@"/System/Library/CoreServices/SystemVersion.plist"];
	NSString *versionString = (NSString *)[systemDict objectForKey:@"ProductVersion"];
	const char *versionCString = [versionString cStringUsingEncoding:NSUTF8StringEncoding];
	
	unsigned int OSMajor = 0;
	unsigned int OSMinor = 0;
	unsigned int OSRevision = 0;
	sscanf(versionCString, "%u.%u.%u", &OSMajor, &OSMinor, &OSRevision);
	
	if ((OSMajor > checkMajor) ||
		(OSMajor >= checkMajor && OSMinor > checkMinor) ||
		(OSMajor >= checkMajor && OSMinor >= checkMinor && OSRevision >= checkRevision) )
	{
		result = YES;
	}
	
	return result;
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
	// Exit the thread.
	if (self.thread != nil)
	{
		// Tell the thread to shut down.
		self.threadExit = YES;
		
		// Wait until the thread has shut down.
		while (self.thread != nil)
		{
			[NSThread sleepUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
		}
	}
	
	self.sendPort = nil;
	[receivePort release];
	
	[super dealloc];
}

- (void) runThread:(id)object
{
	NSAutoreleasePool *threadPool = [[NSAutoreleasePool alloc] init];
	NSRunLoop *runLoop = [[NSRunLoop currentRunLoop] retain];
	
	[runLoop addPort:self.receivePort forMode:NSDefaultRunLoopMode];
	self.thread = [NSThread currentThread];
	
	do
	{
		NSAutoreleasePool *runLoopPool = [[NSAutoreleasePool alloc] init];
		NSDate *runDate = [[NSDate alloc] initWithTimeIntervalSinceNow:self.autoreleaseInterval];
		[runLoop runUntilDate:runDate];
		[runDate release];
		[runLoopPool release];
	} while (!self.threadExit);
	
	self.thread = nil;
	[runLoop release];
	
	[threadPool release];
}

- (void)handlePortMessage:(NSPortMessage *)portMessage
{
	NSInteger message = (NSInteger)[portMessage msgid];
	
	switch (message)
	{
		case MESSAGE_EXIT_THREAD:
			self.threadExit = YES;
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

/********************************************************************************************
	RGBA5551ToRGBA8888() - INLINE

	Converts a color from 16-bit RGBA5551 format into 32-bit RGBA8888 format.

	Takes:
		color16 - The pixel in 16-bit RGBA5551 format.

	Returns:
		A 32-bit unsigned integer containing the RGBA8888 formatted color.

	Details:
		The input and output pixels are expected to have little-endian byte order.
 ********************************************************************************************/
FORCEINLINE uint32_t RGBA5551ToRGBA8888(const uint16_t color16)
{
	return	((color16 & 0x001F) << 3) |
			((color16 & 0x03E0) << 6) |
			((color16 & 0x7C00) << 9) |
			0xFF000000;
}

/********************************************************************************************
	RGB888ToRGBA8888() - INLINE

	Converts a color from 24-bit RGB888 format into 32-bit RGBA8888 format.

	Takes:
		color24 - The pixel in 24-bit RGB888 format.

	Returns:
		A 32-bit unsigned integer containing the RGBA8888 formatted color.

	Details:
		The input and output pixels are expected to have little-endian byte order.
 ********************************************************************************************/
FORCEINLINE uint32_t RGB888ToRGBA8888(const uint32_t color24)
{
	return color24 | 0xFF000000;
}

/********************************************************************************************
	RGBA5551ToRGBA8888Buffer()

	Copies a 16-bit RGBA5551 pixel buffer into a 32-bit RGBA8888 pixel buffer.

	Takes:
		srcBuffer - Pointer to the source 16-bit RGBA5551 pixel buffer.
		
		destBuffer - Pointer to the destination 32-bit RGBA8888 pixel buffer.
		
		numberPixels - The number of pixels to copy.

	Returns:
		Nothing.

	Details:
		The source and destination pixels are expected to have little-endian byte order.
		Also, it is the caller's responsibility to ensure that the source and destination
		buffers are large enough to accomodate the requested number of pixels.
 ********************************************************************************************/
void RGBA5551ToRGBA8888Buffer(const uint16_t *__restrict__ srcBuffer, uint32_t *__restrict__ destBuffer, unsigned int numberPixels)
{
	const uint32_t *__restrict__ destBufferEnd = destBuffer + numberPixels;
	
	while (destBuffer < destBufferEnd)
	{
		*destBuffer++ = RGBA5551ToRGBA8888(*srcBuffer++);
	}
}

/********************************************************************************************
	RGB888ToRGBA8888Buffer()

	Copies a 24-bit RGB888 pixel buffer into a 32-bit RGBA8888 pixel buffer.

	Takes:
		srcBuffer - Pointer to the source 24-bit RGB888 pixel buffer.
		
		destBuffer - Pointer to the destination 32-bit RGBA8888 pixel buffer.
		
		numberPixels - The number of pixels to copy.

	Returns:
		Nothing.

	Details:
		The source and destination pixels are expected to have little-endian byte order.
		Also, it is the caller's responsibility to ensure that the source and destination
		buffers are large enough to accomodate the requested number of pixels.
 ********************************************************************************************/
void RGB888ToRGBA8888Buffer(const uint32_t *__restrict__ srcBuffer, uint32_t *__restrict__ destBuffer, unsigned int numberPixels)
{
	const uint32_t *__restrict__ destBufferEnd = destBuffer + numberPixels;
	
	while (destBuffer < destBufferEnd)
	{
		*destBuffer++ = RGB888ToRGBA8888(*srcBuffer++);
	}
}

/********************************************************************************************
	GetTransformedBounds()

	Returns the bounds of a normalized 2D surface using affine transformations.

	Takes:
		normalBounds - The rectangular bounds of the normal 2D surface.
		
		scalar - The scalar used to transform the 2D surface.
		
		angleDegrees - The rotation angle, in degrees, to transform the 2D surface.

	Returns:
		The bounds of a normalized 2D surface using affine transformations.

	Details:
		The returned bounds is always a normal rectangle. Ignoring the scaling, the
		returned bounds will always be at its smallest when the angle is at 0, 90, 180,
		or 270 degrees, and largest when the angle is at 45, 135, 225, or 315 degrees.
 ********************************************************************************************/
NSSize GetTransformedBounds(NSSize normalBounds, double scalar, double angleDegrees)
{
	const double angleRadians = angleDegrees * (M_PI/180.0);
	
	// The points are as follows:
	//
	// (x[3], y[3])    (x[2], y[2])
	//
	//
	//
	// (x[0], y[0])    (x[1], y[1])
	
	// Do our scale and rotate transformations.
#ifdef __ACCELERATE__
	
	// Note that although we only need to calculate 3 points, we include 4 points
	// here because Accelerate prefers 16-byte alignment.
	double x[] = {0.0, normalBounds.width, normalBounds.width, 0.0};
	double y[] = {0.0, 0.0, normalBounds.height, normalBounds.height};
	
	cblas_drot(4, x, 1, y, 1, scalar * cos(angleRadians), scalar * sin(angleRadians));
	
#else // Keep a C-version of this transformation for reference purposes.
	
	const double w = scalar * normalBounds.width;
	const double h = scalar * normalBounds.height;
	const double d = hypot(w, h);
	const double dAngle = atan2(h, w);
	
	const double px = w * cos(angleRadians);
	const double py = w * sin(angleRadians);
	const double qx = d * cos(dAngle + angleRadians);
	const double qy = d * sin(dAngle + angleRadians);
	const double rx = h * cos((M_PI/2.0) + angleRadians);
	const double ry = h * sin((M_PI/2.0) + angleRadians);
	
	const double x[] = {0.0, px, qx, rx};
	const double y[] = {0.0, py, qy, ry};
	
#endif
	
	// Determine the transformed width, which is dependent on the location of
	// the x-coordinate of point (x[2], y[2]).
	NSSize transformBounds = {0.0, 0.0};
	
	if (x[2] > 0.0)
	{
		if (x[2] < x[3])
		{
			transformBounds.width = x[3] - x[1];
		}
		else if (x[2] < x[1])
		{
			transformBounds.width = x[1] - x[3];
		}
		else
		{
			transformBounds.width = x[2];
		}
	}
	else if (x[2] < 0.0)
	{
		if (x[2] > x[3])
		{
			transformBounds.width = -(x[3] - x[1]);
		}
		else if (x[2] > x[1])
		{
			transformBounds.width = -(x[1] - x[3]);
		}
		else
		{
			transformBounds.width = -x[2];
		}
	}
	else
	{
		transformBounds.width = abs(x[1] - x[3]);
	}
	
	// Determine the transformed height, which is dependent on the location of
	// the y-coordinate of point (x[2], y[2]).
	if (y[2] > 0.0)
	{
		if (y[2] < y[3])
		{
			transformBounds.height = y[3] - y[1];
		}
		else if (y[2] < y[1])
		{
			transformBounds.height = y[1] - y[3];
		}
		else
		{
			transformBounds.height = y[2];
		}
	}
	else if (y[2] < 0.0)
	{
		if (y[2] > y[3])
		{
			transformBounds.height = -(y[3] - y[1]);
		}
		else if (y[2] > y[1])
		{
			transformBounds.height = -(y[1] - y[3]);
		}
		else
		{
			transformBounds.height = -y[2];
		}
	}
	else
	{
		transformBounds.height = abs(y[3] - y[1]);
	}
	
	return transformBounds;
}

/********************************************************************************************
	GetMaxScalarInBounds()

	Returns the maximum scalar that a rectangle can grow, while maintaining its aspect
	ratio, within a boundary.

	Takes:
		normalBoundsWidth - The rectangular width of the normal 2D surface.
		
		normalBoundsHeight - The rectangular height of the normal 2D surface.
		
		keepInBoundsWidth - The rectangular width of the keep in 2D surface.
		
		keepInBoundsHeight - The rectangular height of the keep in 2D surface.

	Returns:
		The maximum scalar that a rectangle can grow, while maintaining its aspect ratio,
		within a boundary.

	Details:
		If keepInBoundsWidth or keepInBoundsHeight are less than or equal to zero, the
		returned scalar will be zero.
 ********************************************************************************************/
double GetMaxScalarInBounds(double normalBoundsWidth, double normalBoundsHeight, double keepInBoundsWidth, double keepInBoundsHeight)
{
	const double maxX = (normalBoundsWidth <= 0.0) ? 0.0 : keepInBoundsWidth / normalBoundsWidth;
	const double maxY = (normalBoundsHeight <= 0.0) ? 0.0 : keepInBoundsHeight / normalBoundsHeight;
	
	return (maxX <= maxY) ? maxX : maxY;
}

/********************************************************************************************
	GetNormalPointFromTransformedPoint()

	Returns a normalized point from a point from a 2D transformed surface.

	Takes:
		transformedPt - A point as it exists on a 2D transformed surface.
		
		normalBounds - The rectangular bounds of the normal 2D surface.
		
		transformedBounds - The rectangular bounds of the transformed 2D surface.
		
		scalar - The scalar used on the transformed 2D surface.
		
		angleDegrees - The rotation angle, in degrees, of the transformed 2D surface.

	Returns:
		A normalized point from a point from a 2D transformed surface.

	Details:
		It may help to call GetTransformedBounds() for the transformBounds parameter.
 ********************************************************************************************/
NSPoint GetNormalPointFromTransformedPoint(NSPoint transformedPt, NSSize normalBounds, NSSize transformBounds, double scalar, double angleDegrees)
{
	const double angleRadians = angleDegrees * (M_PI/180.0);
	
	// Get the coordinates of the transformed point and translate the coordinate
	// system so that the origin becomes the center.
	const double transformedX = transformedPt.x - (transformBounds.width / 2.0);
	const double transformedY = transformedPt.y - (transformBounds.height / 2.0);
	
	// Perform rect-polar conversion.
	
	// Get the radius r with respect to the origin.
	const double r = hypot(transformedX, transformedY);
	
	// Get the angle theta with respect to the origin.
	double theta = 0.0;
	
	if (transformedX == 0.0)
	{
		if (transformedY > 0.0)
		{
			theta = M_PI / 2.0;
		}
		else if (transformedY < 0.0)
		{
			theta = M_PI * 1.5;
		}
	}
	else if (transformedX < 0.0)
	{
		theta = M_PI - atan2(transformedY, -transformedX);
	}
	else if (transformedY < 0.0)
	{
		theta = atan2(transformedY, transformedX) + (M_PI * 2.0);
	}
	else
	{
		theta = atan2(transformedY, transformedX);
	}
	
	// Get the normalized angle and use it to rotate about the origin.
	// Then do polar-rect conversion and translate back to transformed coordinates
	// with a 0 degree rotation.
	const double normalizedAngle = theta - angleRadians;
	const double normalizedX = (r * cos(normalizedAngle)) + (normalBounds.width * scalar / 2.0);
	const double normalizedY = (r * sin(normalizedAngle)) + (normalBounds.height * scalar / 2.0);
	
	// Scale the location to get a one-to-one correlation to normal coordinates.
	const NSPoint normalizedPt = { (CGFloat)(normalizedX / scalar), (CGFloat)(normalizedY / scalar) };
	
	return normalizedPt;
}

/********************************************************************************************
	GetNearestPositivePOT()

	Returns the next highest power of two of a 32-bit integer value.
	
	Takes:
		value - A 32-bit integer value.

	Returns:
		A 32-bit integer with the next highest power of two compared to the input value.

	Details:
		If the input value is already a power of two, this function returns the same
		value.
 ********************************************************************************************/
uint32_t GetNearestPositivePOT(uint32_t value)
{
	value--;
	value |= value >> 1;
	value |= value >> 2;
	value |= value >> 4;
	value |= value >> 8;
	value |= value >> 16;
	value++;
	
	return value;
}
