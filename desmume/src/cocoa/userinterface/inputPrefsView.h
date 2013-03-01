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

#import <Cocoa/Cocoa.h>
#import "InputManager.h"

@class CocoaDSController;


@interface InputPrefsView : NSView <InputHIDManagerTarget>
{
	NSWindow *prefWindow;
	InputManager *inputManager;
	NSButton *lastConfigButton;
	NSInteger configInputTargetID;
	NSMutableDictionary *configInputList;
	
	std::tr1::unordered_map<NSInteger, std::string> commandTagMap; // Key = Command ID, Value = Command Tag
	NSDictionary *displayStringBindings;
}

@property (readonly) IBOutlet NSWindow *prefWindow;
@property (readonly) IBOutlet InputManager *inputManager;
@property (assign) NSInteger configInputTargetID;

- (BOOL) handleKeyboardEvent:(NSEvent *)theEvent keyPressed:(BOOL)keyPressed;
- (BOOL) handleMouseButtonEvent:(NSEvent *)mouseEvent buttonPressed:(BOOL)buttonPressed;
- (NSString *) parseMappingDisplayString:(const char *)commandTag;

- (IBAction) inputButtonSet:(id)sender;

@end
