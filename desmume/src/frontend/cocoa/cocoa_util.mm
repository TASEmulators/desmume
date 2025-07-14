/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2012-2025 DeSmuME team

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
	
#if defined(MAC_OS_X_VERSION_10_14) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_14)
	NSControlStateValue buttonState = GUI_STATE_OFF;
#else
	NSInteger buttonState = GUI_STATE_OFF;
#endif
	
	if ([sender respondsToSelector:@selector(state)])
	{
		buttonState = [sender state];
	}
	
	if (buttonState == GUI_STATE_ON)
	{
		theState = YES;
	}
	
	return theState;
}

+ (void) endSheet:(NSWindow *)sheet returnCode:(NSInteger)code
{
	if (sheet == nil)
	{
		return;
	}
	
#if defined(MAC_OS_X_VERSION_10_9) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_9)
	if ([sheet respondsToSelector:@selector(sheetParent)] && [[sheet sheetParent] respondsToSelector:@selector(endSheet:returnCode:)])
	{
		[[sheet sheetParent] endSheet:sheet returnCode:code];
	}
	else
#endif
	{
		[NSApp endSheet:sheet returnCode:code];
	}
}

+ (NSColor *) NSColorFromRGBA8888:(uint32_t)theColor
{
	const CGFloat r = (CGFloat)((theColor >>  0) & 0xFF) / 255.0f;
	const CGFloat g = (CGFloat)((theColor >>  8) & 0xFF) / 255.0f;
	const CGFloat b = (CGFloat)((theColor >> 16) & 0xFF) / 255.0f;
	const CGFloat a = (CGFloat)((theColor >> 24) & 0xFF) / 255.0f;
	
	return [NSColor colorWithDeviceRed:r green:g blue:b alpha:a];
}

+ (uint32_t) RGBA8888FromNSColor:(NSColor *)theColor
{
	NSInteger numberOfComponents = [theColor numberOfComponents];
	if ( ([theColor colorSpace] != [NSColorSpace deviceRGBColorSpace]) || (numberOfComponents != 4) )
	{
		theColor = [theColor colorUsingColorSpace:[NSColorSpace deviceRGBColorSpace]];
		numberOfComponents = [theColor numberOfComponents];
		
		if ( (theColor == nil) || (numberOfComponents != 4) )
		{
			return 0x00000000;
		}
	}
	
	// Incoming NSColor objects should be under the deviceRGBColorSpace.
	// This is typically the case for macOS v10.7 Lion and later. However,
	// Leopard and Snow Leopard may use an NSCustomColorSpace that just so
	// happens to be compatible with deviceRGBColorSpace.
	//
	// Unfortunately, calling [NSColor getRed:green:blue:alpha] in Leopard
	// or Snow Leopard strictly wants the NSColor to be under
	// deviceRGBColorSpace and no other, lest an exception is raised with
	// the method call.
	//
	// Therefore, for compatibility purposes, we'll use the method call
	// [NSColor getComponents:], which has looser requirements and works
	// with all NSColor objects that are compatible with deviceRGBColorSpace.
	
	CGFloat colorComponent[4];
	[theColor getComponents:colorComponent];
	
	return (((uint32_t)(colorComponent[0] * 255.0f)) <<  0) |
	       (((uint32_t)(colorComponent[1] * 255.0f)) <<  8) |
	       (((uint32_t)(colorComponent[2] * 255.0f)) << 16) |
	       (((uint32_t)(colorComponent[3] * 255.0f)) << 24);
}

+ (NSString *) filePathFromCPath:(const char *)cPath
{
	if (cPath == NULL)
	{
		return nil;
	}
	
	return [[NSFileManager defaultManager] stringWithFileSystemRepresentation:cPath length:strlen(cPath)];
}

+ (NSURL *) fileURLFromCPath:(const char *)cPath
{
	return [NSURL fileURLWithPath:[CocoaDSUtil filePathFromCPath:cPath]];
}

+ (const char *) cPathFromFilePath:(NSString *)filePath
{
	return (filePath != nil) ? [filePath fileSystemRepresentation] : NULL;
}

+ (const char *) cPathFromFileURL:(NSURL *)fileURL
{
	return (fileURL != nil) ? [CocoaDSUtil cPathFromFilePath:[fileURL path]] : NULL;
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

+ (BOOL) determineDarkModeAppearance
{
	const NSInteger appAppearanceMode = [[NSUserDefaults standardUserDefaults] integerForKey:@"Debug_AppAppearanceMode"];
	BOOL darkModeState = NO; // Default to Light Mode appearance
	
	if ( (appAppearanceMode == APP_APPEARANCEMODE_AUTOMATIC) && IsOSXVersionSupported(10, 10, 0) )
	{
		// We're doing a Yosemite-style check for Dark Mode by reading the AppleInterfaceStyle key from NSGlobalDomain.
		//
		// Apple recommends using [[NSView effectiveAppearance] name] to check for Dark Mode, which requires Mojave.
		// While this Mojave method may be the "correct" method according to Apple, we would be forcing the user to
		// run Mojave or later, with this method only being useful if DeSmuME's GUI uses a mix of both Light Mode and
		// Dark Mode windows. We want to keep DeSmuME's GUI simple by only using a single appearance mode for the
		// entire app, and so the Mojave method gives no benefits to DeSmuME over the Yosemite method, and because we
		// don't want DeSmuME to be limited to running only on the more advanced versions of macOS.
		//
		// And so we call this method in response to the "AppleInterfaceThemeChangedNotification" notification from
		// NSDistributedNotificationCenter. Doing this causes a serious technical problem to occur for the Mojave
		// method, in which macOS Mojave and Cataline experience a race condition if you try to call
		// [[NSView effectiveAppearance] name] when the user switches appearance modes in System Preferences. This
		// race condition causes an unreliable read of the Dark Mode state on Mojave and Catalina.
		//
		// With the aforementioned reliability issue being the final straw on top of the previously mentioned reasons,
		// this makes the Yosemite method for checking the Dark Mode state the correct choice.
		const NSDictionary *globalPersistentDomain = [[NSUserDefaults standardUserDefaults] persistentDomainForName:NSGlobalDomain];
		const NSString *interfaceStyle = [globalPersistentDomain valueForKey:@"AppleInterfaceStyle"];
		darkModeState = (interfaceStyle != nil) && [interfaceStyle isEqualToString:@"Dark"] && IsOSXVersionSupported(10, 14, 0);
	}
	else if (appAppearanceMode == APP_APPEARANCEMODE_DARK)
	{
		darkModeState = YES; // Force Dark Mode appearance
	}
	
	return darkModeState;
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
	
#if HAVE_OSAVAILABLE && defined(MAC_OS_X_VERSION_10_13) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_13)
	// We need to use @available here when compiling against macOS v10.13 SDK and later in order to
	// silence a silly warning.
	if (@available(macOS 10_13, *))
#endif
	{
		[self registerForDraggedTypes:[NSArray arrayWithObjects:PASTEBOARDTYPE_URL, nil]];
	}
	
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
	BOOL pboardHasURL = NO;
	
#if HAVE_OSAVAILABLE && defined(MAC_OS_X_VERSION_10_13) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_13)
	// We need to use @available here when compiling against macOS v10.13 SDK and later in order to
	// silence a silly warning.
	if (@available(macOS 10_13, *))
#endif
	{
		pboardHasURL = [[pboard types] containsObject:PASTEBOARDTYPE_URL];
	}
	
	if (pboardHasURL)
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
	BOOL pboardHasURL = NO;
	NSString *filePath = NULL;
	
#if HAVE_OSAVAILABLE && defined(MAC_OS_X_VERSION_10_13) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_13)
	// We need to use @available here when compiling against macOS v10.13 SDK and later in order to
	// silence a silly warning.
	if (@available(macOS 10_13, *))
#endif
	{
		pboardHasURL = [[pboard types] containsObject:PASTEBOARDTYPE_URL];
	}
	
	if (pboardHasURL)
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
	return [NSNumber numberWithUnsignedInteger:color32];
}

@end
