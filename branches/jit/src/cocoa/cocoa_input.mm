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

#import "cocoa_input.h"

#import "cocoa_globals.h"
#import "cocoa_mic.h"

#include "../NDSSystem.h"
#undef BOOL


@implementation CocoaDSInput

@synthesize map;
@synthesize deviceCode;
@synthesize deviceName;

- (id)init
{
	return [self initWithDeviceCode:nil name:nil];
}

- (id) initWithDeviceCode:(NSString *)theCode name:(NSString *)theName
{
	self = [super init];
	if (self == nil)
	{
		return self;
	}
	
	map = [[NSMutableDictionary alloc] initWithCapacity:32];
	
	if (theCode == nil)
	{
		deviceCode = [[NSString stringWithFormat:@"0x%08X", rand()] retain];
	}
	else
	{
		deviceCode = [theCode copy];
	}
	
	if (theName == nil)
	{
		deviceName = @"Unknown Device ";
		deviceName = [[deviceName stringByAppendingString:deviceCode] retain];
	}
	else
	{
		deviceName = [theName copy];
	}

	
	return self;
}

- (void)dealloc
{
	self.map = nil;
	self.deviceCode = nil;
	self.deviceName = nil;
	
	[super dealloc];
}

- (void) set:(NSString *)elementCode attributes:(NSDictionary *)mappingAttributes
{
	NSString *keyString = [[self.deviceCode stringByAppendingString:@":"] stringByAppendingString:elementCode];
	
	// Remove any input mappings with the same DS control name as the new mapping.
	// We're doing this check since the current UI doesn't support multiple bindings per control at this time.
	// When the UI is updated to support multiple bindings per control, this check will need to be removed.
	NSArray *mappingKeys = [self.map allKeys];
	for (NSString *key in mappingKeys)
	{
		NSDictionary *inputMapping = (NSDictionary *)[self.map valueForKey:key];
		NSString *dsControlName = (NSString *)[inputMapping valueForKey:@"name"];
		if ([dsControlName isEqualToString:(NSString *)[mappingAttributes valueForKey:@"name"]])
		{
			[self.map setValue:nil forKey:key];
		}
	}
	
	[self.map setValue:mappingAttributes forKey:keyString];
}

- (NSDictionary *) mappingByElementCode:(NSString *)elementCode
{
	NSString *keyString = [[self.deviceCode stringByAppendingString:@":"] stringByAppendingString:elementCode];
	return [self.map valueForKey:keyString];
}

- (void) removeByElementCode:(NSString *)elementCode
{
	NSString *keyString = [[self.deviceCode stringByAppendingString:@":"] stringByAppendingString:elementCode];
	[self.map setValue:nil forKey:keyString];
}

- (void) removeMapping:(NSString *)mappingName
{
	NSArray *mapKeys = [self.map allKeys];
	for(NSString *key in mapKeys)
	{
		if ([(NSString *)[(NSDictionary *)[self.map valueForKey:key] valueForKey:@"name"] isEqual:mappingName])
		{
			[self.map removeObjectForKey:key];
		}
	}
}

+ (NSDictionary *) dictionaryWithMappingAttributes:(NSString *)mappingName
								   useDeviceValues:(BOOL)useDeviceValues
											xPoint:(CGFloat)xPoint
											yPoint:(CGFloat)yPoint
									  integerValue:(NSInteger)integerValue
										floatValue:(float)floatValue
											  data:(NSData *)data
										  selector:(NSString *)selectorString
										 paramType:(NSString *)paramTypeKey
{
	NSDictionary *attributes = nil;
	
	if (mappingName == nil)
	{
		return attributes;
	}
	
	if (useDeviceValues)
	{
		attributes = [NSDictionary dictionaryWithObjectsAndKeys:
					  mappingName, @"name",
					  [NSNumber numberWithBool:useDeviceValues], @"useDeviceValues",
					  nil];
	}
	else
	{
		NSData *tempData = data;
		if (tempData == nil)
		{
			tempData = [NSData data];
		}
		
		attributes = [NSDictionary dictionaryWithObjectsAndKeys:
					  mappingName, @"name",
					  [NSNumber numberWithBool:useDeviceValues], @"useDeviceValues",
					  [NSNumber numberWithFloat:xPoint], @"xPoint",
					  [NSNumber numberWithFloat:yPoint], @"yPoint",
					  [NSNumber numberWithInteger:integerValue], @"integerValue",
					  [NSNumber numberWithFloat:floatValue], @"floatValue",
					  tempData, @"data",
					  selectorString, @"selector",
					  paramTypeKey, @"paramType",
					  nil];
	}
	
	return attributes;
}

@end

@implementation CocoaDSController

@synthesize states;
@synthesize cdsMic;
@synthesize inputs;
@synthesize mutexControllerUpdate;

- (id)init
{
	return [self initWithMic:nil];
}

- (id) initWithMic:(CocoaDSMic *)theMic
{
	self = [super init];
	if (self == nil)
	{
		return self;
	}
	
	mutexControllerUpdate = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(mutexControllerUpdate, NULL);
	
	cdsMic = [theMic retain];
	states = [[NSMutableDictionary alloc] init];
	inputs = [[NSMutableDictionary alloc] initWithCapacity:10];
	
	[self initDefaultMappings];
	
	[[NSNotificationCenter defaultCenter] addObserver:self
											 selector:@selector(handleHIDInput:)
												 name:@"org.desmume.DeSmuME.hidInputDetected"
											   object:nil];
	
	return self;
}

- (void)dealloc
{
	[[NSNotificationCenter defaultCenter] removeObserver:self];
	
	self.cdsMic = nil;
	[states release];
	[inputs release];
	
	pthread_mutex_destroy(self.mutexControllerUpdate);
	free(self.mutexControllerUpdate);
	mutexControllerUpdate = nil;
	
	[super dealloc];
}

- (void) initDefaultMappings
{	
	[self initUserDefaultMappings];
	[self initControllerMap];
}

- (BOOL) initUserDefaultMappings
{
	BOOL didChange = NO;
	
	// Check to see if the DefaultKeyMappings.plist files exists.
	NSDictionary *defaultMappings = [NSDictionary dictionaryWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"DefaultKeyMappings" ofType:@"plist"]];
	if (defaultMappings == nil)
	{
		return didChange;
	}
	
	// If the input mappings does not exist in the user's preferences file, then copy all the mappings from the
	// DefaultKeyMappings.plist file to the preferences file. Then we're done.
	NSDictionary *userMappings = [[NSUserDefaults standardUserDefaults] dictionaryForKey:@"Input_ControllerMappings"];
	if (userMappings == nil)
	{
		[[NSUserDefaults standardUserDefaults] setObject:defaultMappings forKey:@"Input_ControllerMappings"];
		didChange = YES;
		return didChange;
	}
	
	// At this point, we need to check every key in the user's preference file and make sure that all the keys
	// exist. The keys that must exist are all listed in the DefaultKeyMappings.plist file.
	NSMutableDictionary *tempUserMappings = [NSMutableDictionary dictionaryWithDictionary:userMappings];
	
	NSArray *inputKeys = [defaultMappings allKeys];
	for(NSString *inputString in inputKeys)
	{
		if ([tempUserMappings objectForKey:inputString] == nil)
		{
			[tempUserMappings setValue:[defaultMappings valueForKey:inputString] forKey:inputString];
			didChange = YES;
		}
	}
	
	// If we had to add a missing key, then we need to copy our temporary dictionary back to the
	// user's preferences file.
	if (didChange)
	{
		[[NSUserDefaults standardUserDefaults] setObject:tempUserMappings forKey:@"Input_ControllerMappings"];
	}
	
	// And we're done.
	return didChange;
}

- (void) initControllerMap
{
	NSDictionary *defaultMappings = [NSDictionary dictionaryWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"DefaultKeyMappings" ofType:@"plist"]];
	if (defaultMappings == nil)
	{
		return;
	}
	
	NSDictionary *userMappings = [[NSUserDefaults standardUserDefaults] dictionaryForKey:@"Input_ControllerMappings"];
	if (userMappings == nil)
	{
		[self initUserDefaultMappings];
	}
	
	NSArray *controlNameList = [defaultMappings allKeys];
	for(NSString *controlName in controlNameList)
	{
		[self.states setValue:[NSMutableDictionary dictionaryWithCapacity:64] forKey:controlName];
		
		NSArray *deviceInfoList = (NSArray *)[userMappings valueForKey:controlName];
		for(NSDictionary *deviceInfo in deviceInfoList)
		{
			[self addMapping:controlName deviceInfo:deviceInfo];
		}
	}
}

- (void) addMapping:(NSString *)controlName deviceInfo:(NSDictionary *)deviceInfo
{
	if (controlName == nil)
	{
		return;
	}
	
	NSString *deviceCode = (NSString *)[deviceInfo valueForKey:@"deviceCode"];
	NSString *elementCode = (NSString *)[deviceInfo valueForKey:@"elementCode"];
	
	if (deviceCode == nil || elementCode == nil)
	{
		return;
	}
	
	NSDictionary *mappingAttributes = [CocoaDSInput dictionaryWithMappingAttributes:controlName
																	useDeviceValues:[(NSNumber *)[deviceInfo valueForKey:@"useDeviceValues"] boolValue]
																			 xPoint:[(NSNumber *)[deviceInfo valueForKey:@"xPoint"] floatValue]
																			 yPoint:[(NSNumber *)[deviceInfo valueForKey:@"yPoint"] floatValue]
																	   integerValue:[(NSNumber *)[deviceInfo valueForKey:@"integerValue"] integerValue]
																		 floatValue:[(NSNumber *)[deviceInfo valueForKey:@"floatValue"] floatValue]
																			   data:(NSData *)[deviceInfo valueForKey:@"data"]
																		   selector:(NSString *)[deviceInfo valueForKey:@"selector"]
																		  paramType:(NSString *)[deviceInfo valueForKey:@"paramType"]];
	
	if (mappingAttributes == nil)
	{
		return;
	}
	
	CocoaDSInput *input = (CocoaDSInput *)[self.inputs valueForKey:deviceCode];
	if (input == nil)
	{
		NSString *deviceName = (NSString *)[deviceInfo valueForKey:@"deviceName"];
		if (deviceName == nil)
		{
			deviceName = deviceCode;
		}
		
		input = [[[CocoaDSInput alloc] initWithDeviceCode:deviceCode name:deviceName] autorelease];
		[self.inputs setValue:input forKey:deviceCode];
	}
	
	[input set:elementCode attributes:mappingAttributes];
}

- (BOOL) setStateWithInput:(NSDictionary *)inputAttributes
{
	BOOL inputChanged = NO;
	
	if (inputAttributes == nil)
	{
		return inputChanged;
	}
	
	NSMutableArray *attributesList = [NSMutableArray arrayWithObject:inputAttributes];
	
	return [self setStateWithMultipleInputs:attributesList];
}

- (BOOL) setStateWithMultipleInputs:(NSArray *)inputAttributesList
{
	BOOL inputChanged = NO;
	NSMutableArray *inputStates = [NSMutableArray arrayWithCapacity:8];
	
	if (inputAttributesList == nil || [inputAttributesList count] <= 0)
	{
		return inputChanged;
	}
	
	for (NSDictionary *attr in inputAttributesList)
	{
		NSString *deviceCode = (NSString *)[attr valueForKey:@"deviceCode"];
		if (deviceCode == nil)
		{
			continue;
		}
		
		NSString *elementCode = (NSString *)[attr valueForKey:@"elementCode"];
		if (elementCode == nil)
		{
			continue;
		}
		
		CocoaDSInput *input = (CocoaDSInput *)[self.inputs valueForKey:deviceCode];
		if (input == nil)
		{
			continue;
		}
		
		NSDictionary *mappingAttributes = [input mappingByElementCode:elementCode];
		if (mappingAttributes == nil)
		{
			continue;
		}
		
		NSString *controlName = (NSString *)[mappingAttributes valueForKey:@"name"];
		if (controlName == nil)
		{
			continue;
		}
		
		NSMutableDictionary *inputState = [NSMutableDictionary dictionaryWithObjectsAndKeys:
										   controlName, @"name",
										   [attr valueForKey:@"on"], @"on",
										   nil];
		
		NSNumber *xPointNumber = nil;
		NSNumber *yPointNumber = nil;
		NSNumber *integerNumber = nil;
		NSNumber *floatNumber = nil;
		NSData *inputData = nil;
		NSString *selectorString = nil;
		NSString *paramTypeKey = nil;
		
		NSNumber *useDeviceValuesNumber = (NSNumber *)[mappingAttributes valueForKey:@"useDeviceValues"];
		if (useDeviceValuesNumber == nil || ![useDeviceValuesNumber boolValue]) // Not reading from device values, use values from mapping attributes
		{
			xPointNumber = (NSNumber *)[mappingAttributes valueForKey:@"pointX"];
			yPointNumber = (NSNumber *)[mappingAttributes valueForKey:@"pointY"];
			integerNumber = (NSNumber *)[mappingAttributes valueForKey:@"integerValue"];
			floatNumber = (NSNumber *)[mappingAttributes valueForKey:@"floatValue"];
			inputData = (NSData *)[mappingAttributes valueForKey:@"data"];
			selectorString = (NSString *)[mappingAttributes valueForKey:@"selector"];
			paramTypeKey = (NSString *)[mappingAttributes valueForKey:@"paramType"];
		}
		else // Reading from device values, use values from input attributes
		{
			xPointNumber = (NSNumber *)[attr valueForKey:@"pointX"];
			yPointNumber = (NSNumber *)[attr valueForKey:@"pointY"];
			integerNumber = (NSNumber *)[attr valueForKey:@"integerValue"];
			floatNumber = (NSNumber *)[attr valueForKey:@"floatValue"];
			inputData = (NSData *)[attr valueForKey:@"data"];
		}
		
		if (xPointNumber == nil)
		{
			xPointNumber = [NSNumber numberWithFloat:0.0f];
		}
		
		if (yPointNumber == nil)
		{
			yPointNumber = [NSNumber numberWithFloat:0.0f];
		}
		
		if (integerNumber == nil)
		{
			integerNumber = [NSNumber numberWithInteger:0];
		}
		
		if (floatNumber == nil)
		{
			floatNumber = [NSNumber numberWithFloat:0.0f];
		}
		
		[inputState setValue:xPointNumber forKey:@"pointX"];
		[inputState setValue:yPointNumber forKey:@"pointY"];
		[inputState setValue:integerNumber forKey:@"integerValue"];
		[inputState setValue:floatNumber forKey:@"floatValue"];
		[inputState setValue:inputData forKey:@"data"];
		
		[inputStates addObject:inputState];
	}
	
	inputChanged = [self updateController:inputStates];
	
	return inputChanged;
}

- (NSDictionary *) mappingByEvent:(NSEvent *)event
{
	CocoaDSInput *input = nil;
	NSString *elementCode = nil;
	NSEventType eventType = [event type];
	
	switch (eventType)
	{
		case NSKeyDown:
		case NSKeyUp:
			input = (CocoaDSInput *)[self.inputs valueForKey:@"NSEventKeyboard"];
			elementCode = [NSString stringWithFormat:@"%d", [event keyCode]];
			break;
			
		case NSLeftMouseDown:
		case NSRightMouseDown:
		case NSOtherMouseDown:
		case NSLeftMouseUp:
		case NSRightMouseUp:
		case NSOtherMouseUp:
		case NSLeftMouseDragged:
		case NSRightMouseDragged:
		case NSOtherMouseDragged:
			input = (CocoaDSInput *)[self.inputs valueForKey:@"NSEventMouse"];
			elementCode = [NSString stringWithFormat:@"%i", [event buttonNumber]];
			break;
			
		default:
			break;
	}
	
	return [input mappingByElementCode:elementCode];
}

- (BOOL) updateController:(NSArray *)inputStates
{
	BOOL result = NO;
	
	if (inputStates == nil)
	{
		return result;
	}
	
	pthread_mutex_lock(self.mutexControllerUpdate);
	
	for (NSDictionary *iState in inputStates)
	{
		NSString *controlName = (NSString *)[iState valueForKey:@"name"];
		if (controlName == nil)
		{
			continue;
		}
		
		NSMutableDictionary *controlState = (NSMutableDictionary *)[self.states valueForKey:controlName];
		[controlState setDictionary:iState];
		[controlState setValue:[NSDate date] forKey:@"time"];
		
		result = YES;
	}
	
	pthread_mutex_unlock(self.mutexControllerUpdate);
	
	return result;
}

- (NSDate *) inputTime:(NSString *)controlName
{
	NSDate *inputTime = nil;
	
	NSMutableDictionary *properties = [self.states objectForKey:controlName];
	inputTime = [properties objectForKey:@"time"];
	if (inputTime == nil)
	{
		return inputTime;
	}
	
	return inputTime;
}

- (bool) isInputPressed:(NSString *)controlName
{
	bool result = false;
	
	NSMutableDictionary *controlState = (NSMutableDictionary *)[self.states valueForKey:controlName];
	NSNumber *pressValue = (NSNumber *)[controlState valueForKey:@"on"];
	if (pressValue == nil)
	{
		return result;
	}
	
	result = [pressValue boolValue];
	
	return result;
}

- (NSPoint) inputLocation:(NSString *)controlName
{
	NSPoint outPoint = {0.0f, 0.0f};
	NSMutableDictionary *properties = [self.states objectForKey:controlName];
	
	NSNumber *xValue = [properties objectForKey:@"pointX"];
	if (xValue != nil)
	{
		outPoint.x = [xValue floatValue];
	}
	
	NSNumber *yValue = [properties objectForKey:@"pointY"];
	if (yValue != nil)
	{
		outPoint.y = [yValue floatValue];
	}
	
	return outPoint;
}

- (unsigned char) inputSoundSample:(NSString *)controlName
{
	unsigned char sampleValue = 0;
	
	NSMutableDictionary *properties = [self.states objectForKey:controlName];
	NSNumber *sampleValueObj = [properties objectForKey:@"Sound Sample"];
	if (sampleValueObj == nil)
	{
		return sampleValue;
	}
	
	sampleValue = [sampleValueObj unsignedCharValue];
	
	return sampleValue;
}

- (void) setSoundInputMode:(NSInteger)inputMode
{
	self.cdsMic.mode = inputMode;
}

- (void) update
{
	pthread_mutex_lock(self.mutexControllerUpdate);
	
	// Setup the DS pad.
	NDS_setPad([self isInputPressed:@"Right"],
			   [self isInputPressed:@"Left"],
			   [self isInputPressed:@"Down"],
			   [self isInputPressed:@"Up"],
			   [self isInputPressed:@"Select"],
			   [self isInputPressed:@"Start"],
			   [self isInputPressed:@"B"],
			   [self isInputPressed:@"A"],
			   [self isInputPressed:@"Y"],
			   [self isInputPressed:@"X"],
			   [self isInputPressed:@"L"],
			   [self isInputPressed:@"R"],
			   [self isInputPressed:@"Debug"],
			   [self isInputPressed:@"Lid"]);
	
	// Setup the DS touch pad.
	if ([self isInputPressed:@"Touch"])
	{
		NSPoint touchLocation = [self inputLocation:@"Touch"];
		NDS_setTouchPos((u16)touchLocation.x, (u16)touchLocation.y);
	}
	else
	{
		NDS_releaseTouch();
	}
	
	// Setup the DS mic.
	bool isMicPressed = [self isInputPressed:@"Microphone"];
	NDS_setMic(isMicPressed);
	
	pthread_mutex_unlock(self.mutexControllerUpdate);
	
	if (isMicPressed)
	{
		if (self.cdsMic.mode == MICMODE_NONE)
		{
			[self.cdsMic fillWithNullSamples];
		}
		else if (self.cdsMic.mode == MICMODE_INTERNAL_NOISE)
		{
			[self.cdsMic fillWithInternalNoise];
		}
		else if (self.cdsMic.mode == MICMODE_WHITE_NOISE)
		{
			[self.cdsMic fillWithWhiteNoise];
		}
		else if (self.cdsMic.mode == MICMODE_SOUND_FILE)
		{
			// TODO: Need to implement. Does nothing for now.
		}
	}
}

- (void) handleHIDInput:(NSNotification *)aNotification
{
	NSArray *inputPropertiesList = (NSArray *)[aNotification object];
	
	[self setStateWithMultipleInputs:inputPropertiesList];
}

@end
