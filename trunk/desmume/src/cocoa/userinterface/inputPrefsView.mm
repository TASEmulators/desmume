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

#import "inputPrefsView.h"
#import "preferencesWindowDelegate.h"

#import "cocoa_globals.h"
#import "cocoa_input.h"

#define INPUT_HOLD_TIME		0.1		// Time in seconds to hold a button in its on state when mapping an input.

@implementation InputPrefsView

@synthesize prefWindow;
@synthesize inputManager;
@dynamic configInputTargetID;

- (id)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if (self == nil)
	{
		return self;
    }
	
	lastConfigButton = nil;
	configInputTargetID = 0;
	configInputList = [[NSMutableDictionary alloc] initWithCapacity:32];
	
	displayStringBindings = [[NSDictionary alloc] initWithObjectsAndKeys:
							 @"Input_Up",			[NSString stringWithFormat:@"%i", PREF_INPUT_BUTTON_UP],
							 @"Input_Down",			[NSString stringWithFormat:@"%i", PREF_INPUT_BUTTON_DOWN],
							 @"Input_Left",			[NSString stringWithFormat:@"%i", PREF_INPUT_BUTTON_LEFT],
							 @"Input_Right",		[NSString stringWithFormat:@"%i", PREF_INPUT_BUTTON_RIGHT],
							 @"Input_A",			[NSString stringWithFormat:@"%i", PREF_INPUT_BUTTON_A],
							 @"Input_B",			[NSString stringWithFormat:@"%i", PREF_INPUT_BUTTON_B],
							 @"Input_X",			[NSString stringWithFormat:@"%i", PREF_INPUT_BUTTON_X],
							 @"Input_Y",			[NSString stringWithFormat:@"%i", PREF_INPUT_BUTTON_Y],
							 @"Input_L",			[NSString stringWithFormat:@"%i", PREF_INPUT_BUTTON_L],
							 @"Input_R",			[NSString stringWithFormat:@"%i", PREF_INPUT_BUTTON_R],
							 @"Input_Start",		[NSString stringWithFormat:@"%i", PREF_INPUT_BUTTON_START],
							 @"Input_Select",		[NSString stringWithFormat:@"%i", PREF_INPUT_BUTTON_SELECT],
							 @"Input_Microphone",	[NSString stringWithFormat:@"%i", PREF_INPUT_BUTTON_SIM_MIC],
							 @"Input_Lid",			[NSString stringWithFormat:@"%i", PREF_INPUT_BUTTON_LID],
							 @"Input_Debug",		[NSString stringWithFormat:@"%i", PREF_INPUT_BUTTON_DEBUG],
							 @"Input_SpeedHalf",	[NSString stringWithFormat:@"%i", PREF_INPUT_BUTTON_SPEED_HALF],
							 @"Input_SpeedDouble",	[NSString stringWithFormat:@"%i", PREF_INPUT_BUTTON_SPEED_DOUBLE],
							 @"Input_HUD",			[NSString stringWithFormat:@"%i", PREF_INPUT_BUTTON_TOGGLE_HUD],
							 @"Input_Execute",		[NSString stringWithFormat:@"%i", PREF_INPUT_BUTTON_EXECUTE],
							 @"Input_Pause",		[NSString stringWithFormat:@"%i", PREF_INPUT_BUTTON_PAUSE],
							 @"Input_Reset",		[NSString stringWithFormat:@"%i", PREF_INPUT_BUTTON_RESET],
							 @"Input_Touch",		[NSString stringWithFormat:@"%i", PREF_INPUT_BUTTON_TOUCH],
							 nil];
	
	commandTagMap[PREF_INPUT_BUTTON_UP]				= "Up";
	commandTagMap[PREF_INPUT_BUTTON_DOWN]			= "Down";
	commandTagMap[PREF_INPUT_BUTTON_LEFT]			= "Left";
	commandTagMap[PREF_INPUT_BUTTON_RIGHT]			= "Right";
	commandTagMap[PREF_INPUT_BUTTON_A]				= "A";
	commandTagMap[PREF_INPUT_BUTTON_B]				= "B";
	commandTagMap[PREF_INPUT_BUTTON_X]				= "X";
	commandTagMap[PREF_INPUT_BUTTON_Y]				= "Y";
	commandTagMap[PREF_INPUT_BUTTON_L]				= "L";
	commandTagMap[PREF_INPUT_BUTTON_R]				= "R";
	commandTagMap[PREF_INPUT_BUTTON_START]			= "Start";
	commandTagMap[PREF_INPUT_BUTTON_SELECT]			= "Select";
	commandTagMap[PREF_INPUT_BUTTON_SIM_MIC]		= "Microphone";
	commandTagMap[PREF_INPUT_BUTTON_LID]			= "Lid";
	commandTagMap[PREF_INPUT_BUTTON_DEBUG]			= "Debug";
	commandTagMap[PREF_INPUT_BUTTON_SPEED_HALF]		= "Speed Half";
	commandTagMap[PREF_INPUT_BUTTON_SPEED_DOUBLE]	= "Speed Double";
	commandTagMap[PREF_INPUT_BUTTON_TOGGLE_HUD]		= "HUD";
	commandTagMap[PREF_INPUT_BUTTON_EXECUTE]		= "Execute";
	commandTagMap[PREF_INPUT_BUTTON_PAUSE]			= "Pause";
	commandTagMap[PREF_INPUT_BUTTON_RESET]			= "Reset";
	commandTagMap[PREF_INPUT_BUTTON_TOUCH]			= "Touch";
	
    return self;
}

- (void)dealloc
{
	[configInputList release];
	[displayStringBindings release];
	
	[super dealloc];
}

#pragma mark Dynamic Properties
- (void) setConfigInputTargetID:(NSInteger)targetID
{
	if (targetID == 0)
	{
		[lastConfigButton setState:NSOffState];
		lastConfigButton = nil;
	}
	
	configInputTargetID = targetID;
	[[self inputManager] setHidInputTarget:(targetID == 0) ? nil : self];
}

- (NSInteger) configInputTargetID
{
	return configInputTargetID;
}

#pragma mark Class Methods

- (BOOL) handleKeyboardEvent:(NSEvent *)theEvent keyPressed:(BOOL)keyPressed
{
	BOOL isHandled = NO;
	
	if ([self configInputTargetID] == 0)
	{
		return isHandled;
	}
	
	std::string commandTag = commandTagMap[[self configInputTargetID]];
	if (commandTag.empty())
	{
		return isHandled;
	}
	
	InputAttributes inputAttr = InputManagerEncodeKeyboardInput([theEvent keyCode], keyPressed);
	[inputManager addMappingUsingInputAttributes:&inputAttr commandTag:commandTag.c_str()];
	[inputManager writeUserDefaultsMappingUsingInputAttributes:&inputAttr commandTag:commandTag.c_str()];
	
	NSMutableDictionary *prefWindowBindings = [(PreferencesWindowDelegate *)[prefWindow delegate] bindings];
	NSString *displayBinding = (NSString *)[displayStringBindings valueForKey:[NSString stringWithFormat:@"%i", [self configInputTargetID]]];
	[prefWindowBindings setValue:[self parseMappingDisplayString:commandTag.c_str()] forKey:displayBinding];
	
	[self setConfigInputTargetID:0];
	
	isHandled = YES;
	return isHandled;
}

- (BOOL) handleMouseButtonEvent:(NSEvent *)mouseEvent buttonPressed:(BOOL)buttonPressed
{
	BOOL isHandled = NO;
	
	if ([self configInputTargetID] == 0)
	{
		return isHandled;
	}
	
	std::string commandTag = commandTagMap[[self configInputTargetID]];
	if (commandTag.empty())
	{
		return isHandled;
	}
	
	InputAttributes inputAttr = InputManagerEncodeMouseButtonInput([mouseEvent buttonNumber], NSMakePoint(0.0f, 0.0f), buttonPressed);
	[inputManager addMappingUsingInputAttributes:&inputAttr commandTag:commandTag.c_str()];
	[inputManager writeUserDefaultsMappingUsingInputAttributes:&inputAttr commandTag:commandTag.c_str()];
	
	NSMutableDictionary *prefWindowBindings = [(PreferencesWindowDelegate *)[prefWindow delegate] bindings];
	NSString *displayBinding = (NSString *)[displayStringBindings valueForKey:[NSString stringWithFormat:@"%i", [self configInputTargetID]]];
	[prefWindowBindings setValue:[self parseMappingDisplayString:commandTag.c_str()] forKey:displayBinding];
	
	[self setConfigInputTargetID:0];
	
	isHandled = YES;
	return isHandled;
}

- (NSString *) parseMappingDisplayString:(const char *)commandTag
{
	NSDictionary *userMappings = [[NSUserDefaults standardUserDefaults] dictionaryForKey:@"Input_ControllerMappings"];
	NSArray *mappingList = (NSArray *)[userMappings valueForKey:[NSString stringWithCString:commandTag encoding:NSUTF8StringEncoding]];
	NSDictionary *mapping = (NSDictionary *)[mappingList objectAtIndex:0];
	NSString *deviceName = (NSString *)[mapping valueForKey:@"deviceName"];
	NSString *elementName = (NSString *)[mapping valueForKey:@"elementName"];
	
	NSString *displayString = [NSString stringWithString:deviceName];
	displayString = [displayString stringByAppendingString:@": "];
	displayString = [displayString stringByAppendingString:elementName];
	
	return displayString;
}

#pragma mark InputHIDManagerTarget Protocol
- (BOOL) handleHIDQueue:(IOHIDQueueRef)hidQueue
{
	BOOL isHandled = NO;
	
	if ([self configInputTargetID] == 0)
	{
		ClearHIDQueue(hidQueue);
		return isHandled;
	}
	
	InputAttributesList inputList = InputManagerEncodeHIDQueue(hidQueue);
	const size_t inputCount = inputList.size();
	
	for (unsigned int i = 0; i < inputCount; i++)
	{
		const InputAttributes &inputAttr = inputList[i];
		const std::string inputKey = inputAttr.deviceCode + ":" + inputAttr.elementCode;
		NSString *inputKeyStr = [NSString stringWithCString:inputKey.c_str() encoding:NSUTF8StringEncoding];
		NSDate *inputOnDate = [configInputList valueForKey:inputKeyStr];
		
		if (inputAttr.inputState == INPUT_ATTRIBUTE_STATE_ON)
		{
			if (inputOnDate == nil)
			{
				[configInputList setValue:[NSDate date] forKey:inputKeyStr];
			}
		}
		else
		{
			if (inputOnDate != nil)
			{
				if (([inputOnDate timeIntervalSinceNow] * -1.0) < INPUT_HOLD_TIME)
				{
					// If the button isn't held for at least INPUT_HOLD_TIME seconds, then reject the input.
					[configInputList setValue:nil forKey:inputKeyStr];
				}
				else
				{
					std::string commandTag = commandTagMap[[self configInputTargetID]];
					if (commandTag.empty())
					{
						continue;
					}
					
					// Add the input mapping.
					[inputManager addMappingUsingInputAttributes:&inputAttr commandTag:commandTag.c_str()];
					[inputManager writeUserDefaultsMappingUsingInputAttributes:&inputAttr commandTag:commandTag.c_str()];
					
					NSMutableDictionary *prefWindowBindings = [(PreferencesWindowDelegate *)[prefWindow delegate] bindings];
					NSString *displayBinding = (NSString *)[displayStringBindings valueForKey:[NSString stringWithFormat:@"%i", [self configInputTargetID]]];
					[prefWindowBindings setValue:[self parseMappingDisplayString:commandTag.c_str()] forKey:displayBinding];
					
					[self setConfigInputTargetID:0];
					break;
				}
			}
		}
	}
	
	isHandled = YES;
	return isHandled;
}

#pragma mark NSResponder Methods

- (void)keyDown:(NSEvent *)theEvent
{
	BOOL isHandled = [self handleKeyboardEvent:theEvent keyPressed:YES];
	if (!isHandled)
	{
		[super keyDown:theEvent];
	}
}

- (void)mouseDown:(NSEvent *)theEvent
{
	BOOL isHandled = [self handleMouseButtonEvent:theEvent buttonPressed:YES];
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
	BOOL isHandled = [self handleMouseButtonEvent:theEvent buttonPressed:YES];
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
	BOOL isHandled = [self handleMouseButtonEvent:theEvent buttonPressed:YES];
	if (!isHandled)
	{
		[super otherMouseDown:theEvent];
	}
}

- (void)otherMouseDragged:(NSEvent *)theEvent
{
	[self otherMouseDown:theEvent];
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

#pragma mark IBAction Methods

- (IBAction) inputButtonSet:(id)sender
{
	NSButton *theButton = (NSButton *)sender;
	
	if ([self configInputTargetID] != 0 && lastConfigButton != theButton)
	{
		[lastConfigButton setState:NSOffState];
	}
	
	if ([theButton state] == NSOnState)
	{
		lastConfigButton = theButton;
		[configInputList removeAllObjects];
		[self setConfigInputTargetID:[theButton tag]];
	}
	else
	{
		[self setConfigInputTargetID:0];
	}
}

@end
