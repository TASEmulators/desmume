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

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#include <libkern/OSAtomic.h>
#include "utilities.h"

NS_ASSUME_NONNULL_BEGIN

@interface CocoaDSUtil : NSObject

+ (NSInteger) getIBActionSenderTag:(id)sender;
+ (BOOL) getIBActionSenderButtonStateBool:(id)sender;

+ (NSColor *) NSColorFromRGBA8888:(uint32_t)theColor;
+ (uint32_t) RGBA8888FromNSColor:(NSColor *)theColor;

@property (class, readonly) NSInteger appVersionNumeric;
@property (class, readonly, copy) NSString *appInternalVersionString;
@property (class, readonly, copy) NSString *appInternalNameAndVersionString;
@property (class, readonly, copy) NSString *appCompilerDetailString;

@property (class, readonly, copy) NSString *operatingSystemString;
@property (class, readonly, copy) NSString *modelIdentifierString;
@property (class, readonly) uint32_t hostIP4AddressAsUInt32;

@end

@protocol DirectoryURLDragDestTextFieldProtocol <NSObject>

@required
- (void) assignDirectoryPath:(NSString *)dirPath textField:(NSTextField *)textField;

@end

/// Subclass NSTextField to override NSDraggingDestination methods for assigning directory paths using drag-and-drop
@interface DirectoryURLDragDestTextField : NSTextField

@end

@interface NSNotificationCenter (MainThread)

- (void)postNotificationOnMainThread:(NSNotification *)notification;
- (void)postNotificationOnMainThreadName:(NSNotificationName)aName object:(nullable id)anObject;
- (void)postNotificationOnMainThreadName:(NSNotificationName)aName object:(nullable id)anObject userInfo:(nullable NSDictionary *)aUserInfo;

@end

@interface RGBA8888ToNSColorValueTransformer : NSValueTransformer

@end

NS_ASSUME_NONNULL_END
