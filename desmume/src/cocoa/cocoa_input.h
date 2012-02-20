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
#include <pthread.h>

@class CocoaDSMic;


@interface CocoaDSInput : NSObject
{
	NSMutableDictionary *map;
	NSString *deviceCode;
	NSString *deviceName;
}

@property (retain) NSMutableDictionary *map;
@property (copy) NSString *deviceCode;
@property (copy) NSString *deviceName;

- (id) initWithDeviceCode:(NSString *)theCode name:(NSString *)theName;

- (void) set:(NSString *)elementCode attributes:(NSDictionary *)mappingAttributes;
- (NSDictionary *) mappingByElementCode:(NSString *)elementCode;
- (void) removeByElementCode:(NSString *)elementCode;
- (void) removeMapping:(NSString *)mappingName;

+ (NSDictionary *) dictionaryWithMappingAttributes:(NSString *)mappingName
								   useDeviceValues:(BOOL)useDeviceValues
											xPoint:(CGFloat)xPoint
											yPoint:(CGFloat)yPoint
									  integerValue:(NSInteger)integerValue
										floatValue:(float)floatValue
											  data:(NSData *)data
										  selector:(NSString *)selectorString
										 paramType:(NSString *)paramTypeKey;

@end


@interface CocoaDSController : NSObject
{
	NSMutableDictionary *states;
	CocoaDSMic *cdsMic;
	pthread_mutex_t *mutexControllerUpdate;
	NSMutableDictionary *inputs;
}

@property (readonly) NSMutableDictionary *states;
@property (retain) CocoaDSMic *cdsMic;
@property (readonly) NSMutableDictionary *inputs;
@property (readonly) pthread_mutex_t *mutexControllerUpdate;

- (id) initWithMic:(CocoaDSMic *)theMic;

- (void) initDefaultMappings;
- (BOOL) initUserDefaultMappings;
- (void) initControllerMap;

- (void) addMapping:(NSString *)controlName deviceInfo:(NSDictionary *)deviceInfo;

- (BOOL) setStateWithInput:(NSDictionary *)inputAttributes;
- (BOOL) setStateWithMultipleInputs:(NSArray *)inputAttributesList;
- (NSDictionary *) mappingByEvent:(NSEvent *)event;
- (BOOL) updateController:(NSArray *)inputStates;

- (NSDate *) inputTime:(NSString *)controlName;
- (bool) isInputPressed:(NSString *)controlName;
- (NSPoint) inputLocation:(NSString *)controlName;
- (unsigned char) inputSoundSample:(NSString *)controlName;

- (void) setSoundInputMode:(NSInteger)inputMode;
- (void) update;
- (void) handleHIDInput:(NSNotification *)aNotification;

@end
