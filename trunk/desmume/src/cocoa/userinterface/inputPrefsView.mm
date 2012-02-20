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

#import "inputPrefsView.h"
#import "preferencesWindowDelegate.h"

#import "cocoa_globals.h"
#import "cocoa_input.h"

#define INPUT_HOLD_TIME		0.1		// Time in seconds to hold a button in its on state when mapping an input.

@implementation InputPrefsView

@synthesize prefWindow;
@synthesize configInput;
@synthesize cdsController;

- (id)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if (self == nil)
	{
		return self;
    }
	
	lastConfigButton = nil;
	configInput = 0;
	configInputList = [[NSMutableDictionary alloc] initWithCapacity:32];
	keyNameTable = [[NSDictionary alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"KeyNames" ofType:@"plist"]];
	cdsController = nil;
	
	[[NSNotificationCenter defaultCenter] addObserver:self
											 selector:@selector(handleHIDInput:)
												 name:@"org.desmume.DeSmuME.hidInputDetected"
											   object:nil];
	
    return self;
}

- (void)dealloc
{
	[[NSNotificationCenter defaultCenter] removeObserver:self];
	
	[configInputList release];
	[keyNameTable release];
	
	[super dealloc];
}

- (void)keyDown:(NSEvent *)theEvent
{
	NSString *elementCode = [NSString stringWithFormat:@"%d", [theEvent keyCode]];
	NSString *elementName = (NSString *)[keyNameTable valueForKey:elementCode];
	
	if (configInput != 0)
	{
		[self addMappingById:configInput deviceCode:@"NSEventKeyboard" deviceName:@"Keyboard" elementCode:elementCode elementName:elementName];
		[self inputButtonCancelConfig];
	}
}

- (void)mouseDown:(NSEvent *)theEvent
{
	BOOL isHandled = [self handleMouseDown:theEvent];
	if (!isHandled)
	{
		[super mouseDown:theEvent];
	}
}

- (void)mouseDragged:(NSEvent *)theEvent
{
	[self mouseDown:theEvent];
}

- (void)rightMouseDown:(NSEvent *)theEvent
{
	BOOL isHandled = [self handleMouseDown:theEvent];
	if (!isHandled)
	{
		[super rightMouseDown:theEvent];
	}
}

- (void)rightMouseDragged:(NSEvent *)theEvent
{
	[self rightMouseDown:theEvent];
}

- (void)otherMouseDown:(NSEvent *)theEvent
{
	BOOL isHandled = [self handleMouseDown:theEvent];
	if (!isHandled)
	{
		[super otherMouseDown:theEvent];
	}
}

- (void)otherMouseDragged:(NSEvent *)theEvent
{
	[self otherMouseDown:theEvent];
}

- (BOOL) handleMouseDown:(NSEvent *)mouseEvent
{
	BOOL isHandled = NO;
	NSString *elementCode = [NSString stringWithFormat:@"%i", [mouseEvent buttonNumber]];
	NSString *elementName = [NSString stringWithFormat:@"Button %i", [mouseEvent buttonNumber]];
	
	switch ([mouseEvent buttonNumber])
	{
		case kCGMouseButtonLeft:
			elementName = @"Primary Button";
			break;
			
		case kCGMouseButtonRight:
			elementName = @"Secondary Button";
			break;
			
		case kCGMouseButtonCenter:
			elementName = @"Center Button";
			break;
			
		default:
			break;
	}
	
	if (configInput != 0)
	{
		[self addMappingById:configInput deviceCode:@"NSEventMouse" deviceName:@"Mouse" elementCode:elementCode elementName:elementName];
		[self inputButtonCancelConfig];
		isHandled = YES;
	}
	
	return isHandled;
}

- (void) addMappingById:(NSInteger)dsControlID deviceCode:(NSString *)deviceCode deviceName:(NSString *)deviceName elementCode:(NSString *)elementCode elementName:(NSString *)elementName
{
	NSString *dsControlKey = nil;
	NSString *displayBindingsKey = nil;
	
	switch (dsControlID)
	{
		case PREF_INPUT_BUTTON_UP:
			dsControlKey = @"Up";
			displayBindingsKey = @"Input_Up";
			break;
			
		case PREF_INPUT_BUTTON_DOWN:
			dsControlKey = @"Down";
			displayBindingsKey = @"Input_Down";
			break;
			
		case PREF_INPUT_BUTTON_LEFT:
			dsControlKey = @"Left";
			displayBindingsKey = @"Input_Left";
			break;
			
		case PREF_INPUT_BUTTON_RIGHT:
			dsControlKey = @"Right";
			displayBindingsKey = @"Input_Right";
			break;
			
		case PREF_INPUT_BUTTON_A:
			dsControlKey = @"A";
			displayBindingsKey = @"Input_A";
			break;
			
		case PREF_INPUT_BUTTON_B:
			dsControlKey = @"B";
			displayBindingsKey = @"Input_B";
			break;
			
		case PREF_INPUT_BUTTON_X:
			dsControlKey = @"X";
			displayBindingsKey = @"Input_X";
			break;
			
		case PREF_INPUT_BUTTON_Y:
			dsControlKey = @"Y";
			displayBindingsKey = @"Input_Y";
			break;
			
		case PREF_INPUT_BUTTON_L:
			dsControlKey = @"L";
			displayBindingsKey = @"Input_L";
			break;
			
		case PREF_INPUT_BUTTON_R:
			dsControlKey = @"R";
			displayBindingsKey = @"Input_R";
			break;
			
		case PREF_INPUT_BUTTON_START:
			dsControlKey = @"Start";
			displayBindingsKey = @"Input_Start";
			break;
			
		case PREF_INPUT_BUTTON_SELECT:
			dsControlKey = @"Select";
			displayBindingsKey = @"Input_Select";
			break;
			
		case PREF_INPUT_BUTTON_SIM_MIC:
			dsControlKey = @"Microphone";
			displayBindingsKey = @"Input_Microphone";
			break;
			
		case PREF_INPUT_BUTTON_LID:
			dsControlKey = @"Lid";
			displayBindingsKey = @"Input_Lid";
			break;
			
		case PREF_INPUT_BUTTON_DEBUG:
			dsControlKey = @"Debug";
			displayBindingsKey = @"Input_Debug";
			break;
			
		case PREF_INPUT_BUTTON_SPEED_HALF:
			dsControlKey = @"Speed Half";
			displayBindingsKey = @"Input_SpeedHalf";
			break;
			
		case PREF_INPUT_BUTTON_SPEED_DOUBLE:
			dsControlKey = @"Speed Double";
			displayBindingsKey = @"Input_SpeedDouble";
			break;
			
		case PREF_INPUT_BUTTON_TOGGLE_HUD:
			dsControlKey = @"HUD";
			displayBindingsKey = @"Input_HUD";
			break;
			
		case PREF_INPUT_BUTTON_EXECUTE:
			dsControlKey = @"Execute";
			displayBindingsKey = @"Input_Execute";
			break;
			
		case PREF_INPUT_BUTTON_PAUSE:
			dsControlKey = @"Pause";
			displayBindingsKey = @"Input_Pause";
			break;
			
		case PREF_INPUT_BUTTON_RESET:
			dsControlKey = @"Reset";
			displayBindingsKey = @"Input_Reset";
			break;
			
		case PREF_INPUT_BUTTON_TOUCH:
			dsControlKey = @"Touch";
			break;
			
		default:
			return;
			break;
	}
	
	if (dsControlKey != nil)
	{
		[self addMappingByKey:dsControlKey deviceCode:deviceCode deviceName:deviceName elementCode:elementCode elementName:elementName];
		if (dsControlID != PREF_INPUT_BUTTON_TOUCH)
		{
			NSMutableDictionary *prefWindowBindings = [(PreferencesWindowDelegate *)[prefWindow delegate] bindings];
			[prefWindowBindings setValue:[self parseMappingDisplayString:dsControlKey] forKey:displayBindingsKey];
		}
	}
}

- (void) addMappingByKey:(NSString *)dsControlKey deviceCode:(NSString *)deviceCode deviceName:(NSString *)deviceName elementCode:(NSString *)elementCode elementName:(NSString *)elementName
{
	if (deviceCode == nil || elementCode == nil)
	{
		return;
	}
	
	if (deviceName == nil)
	{
		deviceName = deviceCode;
	}
	
	if (elementName == nil)
	{
		elementName = elementCode;
	}
	
	BOOL useDeviceValues = NO;
	if ([deviceCode isEqualToString:@"NSEventMouse"])
	{
		useDeviceValues = YES;
	}
	
	NSDictionary *deviceInfo = [NSDictionary dictionaryWithObjectsAndKeys:
								deviceCode, @"deviceCode",
								deviceName, @"deviceName",
								elementCode, @"elementCode",
								elementName, @"elementName",
								[NSNumber numberWithBool:useDeviceValues], @"useDeviceValues",
								nil];
	
	[self addMappingByKey:dsControlKey deviceInfo:deviceInfo];
}

- (void) addMappingByKey:(NSString *)dsControlKey deviceInfo:(NSDictionary *)deviceInfo
{
	[cdsController addMapping:dsControlKey deviceInfo:deviceInfo];
	
	NSMutableDictionary *tempUserMappings = [NSMutableDictionary dictionaryWithDictionary:[[NSUserDefaults standardUserDefaults] dictionaryForKey:@"Input_ControllerMappings"]];
	[tempUserMappings setValue:[NSArray arrayWithObject:deviceInfo] forKey:dsControlKey];
	[[NSUserDefaults standardUserDefaults] setValue:tempUserMappings forKey:@"Input_ControllerMappings"];
}

- (NSString *) parseMappingDisplayString:(NSString *)keyString
{
	NSDictionary *userMappings = [[NSUserDefaults standardUserDefaults] dictionaryForKey:@"Input_ControllerMappings"];
	NSArray *mappingList = (NSArray *)[userMappings valueForKey:keyString];
	NSDictionary *mapping = (NSDictionary *)[mappingList objectAtIndex:0];
	NSString *deviceName = (NSString *)[mapping valueForKey:@"deviceName"];
	NSString *elementName = (NSString *)[mapping valueForKey:@"elementName"];

	NSString *displayString = [NSString stringWithString:deviceName];
	displayString = [displayString stringByAppendingString:@": "];
	displayString = [displayString stringByAppendingString:elementName];
	
	return displayString;
}

- (IBAction) inputButtonSet:(id)sender
{
	NSButton *theButton = (NSButton *)sender;
	
	if (configInput && lastConfigButton != theButton)
	{
		[lastConfigButton setState:NSOffState];
	}
	
	if ([theButton state] == NSOnState)
	{
		lastConfigButton = theButton;
		[configInputList removeAllObjects];
		configInput = [theButton tag];
	}
	else
	{
		[self inputButtonCancelConfig];
	}

}

- (void) inputButtonCancelConfig
{
	if (lastConfigButton != nil)
	{
		[lastConfigButton setState:NSOffState];
		lastConfigButton = nil;
	}
	
	configInput = 0;
}

- (BOOL)acceptsFirstResponder
{
	return YES;
}

- (BOOL)becomeFirstResponder
{
	return YES;
}

- (BOOL)resignFirstResponder
{
	return YES;
}

- (void) handleHIDInput:(NSNotification *)aNotification
{
	if (configInput == 0)
	{
		return;
	}
	
	NSArray *inputPropertiesList = (NSArray *)[aNotification object];
	BOOL inputOn = NO;
	
	for (NSDictionary *inputProperties in inputPropertiesList)
	{
		NSNumber *onState = (NSNumber *)[inputProperties valueForKey:@"on"];
		if (onState != nil)
		{
			NSString *deviceCode = (NSString *)[inputProperties valueForKey:@"deviceCode"];
			NSString *elementCode = (NSString *)[inputProperties valueForKey:@"elementCode"];
			NSString *deviceElementCode = [[deviceCode stringByAppendingString:@":"] stringByAppendingString:elementCode];
			NSDate *inputOnDate = [configInputList valueForKey:deviceElementCode];
			
			inputOn = [onState boolValue];
			if (inputOn)
			{
				if (inputOnDate == nil)
				{
					[configInputList setValue:[NSDate date] forKey:deviceElementCode];
				}
			}
			else
			{
				if (inputOnDate != nil)
				{
					if (([inputOnDate timeIntervalSinceNow] * -1.0) < INPUT_HOLD_TIME)
					{
						// If the button isn't held for at least INPUT_HOLD_TIME seconds, then reject the input.
						[configInputList setValue:nil forKey:deviceElementCode];
					}
					else
					{
						// Add the input mapping.
						NSString *deviceName = (NSString *)[inputProperties valueForKey:@"deviceName"];
						NSString *elementName = (NSString *)[inputProperties valueForKey:@"elementName"];
						
						[self addMappingById:configInput deviceCode:deviceCode deviceName:deviceName elementCode:elementCode elementName:elementName];
						[self inputButtonCancelConfig];
						break;
					}
				}
			}
		}
	}
}

@end
