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

@class CocoaDSController;


@interface InputPrefsView : NSView
{
	NSWindow *prefWindow;
	NSButton *lastConfigButton;
	NSInteger configInput;
	NSMutableDictionary *configInputList;
	NSDictionary *keyNameTable;
	
	CocoaDSController *cdsController;
}

@property (readonly) IBOutlet NSWindow *prefWindow;
@property (assign) NSInteger configInput;
@property (retain) CocoaDSController *cdsController;

- (BOOL) handleMouseDown:(NSEvent *)mouseEvent;
- (void) addMappingById:(NSInteger)dsControlID deviceCode:(NSString *)deviceCode deviceName:(NSString *)deviceName elementCode:(NSString *)elementCode elementName:(NSString *)elementName;
- (void) addMappingByKey:(NSString *)dsControlKey deviceCode:(NSString *)deviceCode deviceName:(NSString *)deviceName elementCode:(NSString *)elementCode elementName:(NSString *)elementName;
- (void) addMappingByKey:(NSString *)dsControlKey deviceInfo:(NSDictionary *)deviceInfo;
- (NSString *) parseMappingDisplayString:(NSString *)keyString;
- (IBAction) inputButtonSet:(id)sender;
- (void) inputButtonCancelConfig;
- (void) handleHIDInput:(NSNotification *)aNotification;

@end
