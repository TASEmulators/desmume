/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2012-2018 DeSmuME team

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

+ (uint32_t) hostIP4AddressAsUInt32
{
	uint32_t ip4Address_u32 = 0;
	
	NSNumberFormatter *numberFormatter = [[NSNumberFormatter alloc] init];
	[numberFormatter setAllowsFloats:NO];
	
	NSArray *ipAddresses = [[NSHost hostWithName:[[NSHost currentHost] name]] addresses];
	for (NSString *ipAddress in ipAddresses)
	{
		if ([ipAddress isEqualToString:@"127.0.0.1"])
		{
			continue;
		}
		else
		{
			NSArray *ipParts = [ipAddress componentsSeparatedByString:@"."];
			
			if ([ipParts count] != 4)
			{
				continue;
			}
			else
			{
				NSNumber *ipPartNumber[4] = {
					[numberFormatter numberFromString:(NSString *)[ipParts objectAtIndex:0]],
					[numberFormatter numberFromString:(NSString *)[ipParts objectAtIndex:1]],
					[numberFormatter numberFromString:(NSString *)[ipParts objectAtIndex:2]],
					[numberFormatter numberFromString:(NSString *)[ipParts objectAtIndex:3]]
				};
				
				if ( (ipPartNumber[0] == nil) || (ipPartNumber[1] == nil) || (ipPartNumber[2] == nil) || (ipPartNumber[3] == nil) )
				{
					continue;
				}
				else
				{
					const uint8_t ipPart_u8[4] = {
						[ipPartNumber[0] unsignedCharValue],
						[ipPartNumber[1] unsignedCharValue],
						[ipPartNumber[2] unsignedCharValue],
						[ipPartNumber[3] unsignedCharValue]
					};
					
					ip4Address_u32 = (ipPart_u8[0]) | (ipPart_u8[1] << 8) | (ipPart_u8[2] << 16) | (ipPart_u8[3] << 24);
					break;
				}
			}
		}
	}
	
	[numberFormatter release];
	return ip4Address_u32;
}

@end

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
