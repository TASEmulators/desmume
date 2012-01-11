/*
	Copyright (C) 2007 Jeff Bland
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

#import "input_legacy.h"
#import "cocoa_input_legacy.h"
#import "preferences_legacy.h"

static unsigned char utf8_return = 0x0D;
static unsigned char utf8_right[3] = { 0xEF, 0x9C, 0x83 };
static unsigned char utf8_up[3] = { 0xEF, 0x9C, 0x80 };
static unsigned char utf8_down[3] = { 0xEF, 0x9C, 0x81 };
static unsigned char utf8_left[3] = { 0xEF, 0x9C, 0x82 };

//

@implementation ControlsDelegate
+ (id)sharedObject
{
	static ControlsDelegate* object = nil;
	if(!object)object = [[ControlsDelegate alloc] init];
	return object;
}
- (void)bindingForKeyA:(id)sender      { [[NSUserDefaults standardUserDefaults] setValue:[[sender selectedItem] representedObject] forKey:PREF_KEY_A]; }
- (void)bindingForKeyB:(id)sender      { [[NSUserDefaults standardUserDefaults] setValue:[[sender selectedItem] representedObject] forKey:PREF_KEY_B]; }
- (void)bindingForKeyX:(id)sender      { [[NSUserDefaults standardUserDefaults] setValue:[[sender selectedItem] representedObject] forKey:PREF_KEY_X]; }
- (void)bindingForKeyY:(id)sender      { [[NSUserDefaults standardUserDefaults] setValue:[[sender selectedItem] representedObject] forKey:PREF_KEY_Y]; }
- (void)bindingForKeyL:(id)sender      { [[NSUserDefaults standardUserDefaults] setValue:[[sender selectedItem] representedObject] forKey:PREF_KEY_L]; }
- (void)bindingForKeyR:(id)sender      { [[NSUserDefaults standardUserDefaults] setValue:[[sender selectedItem] representedObject] forKey:PREF_KEY_R]; }
- (void)bindingForKeyUp:(id)sender     { [[NSUserDefaults standardUserDefaults] setValue:[[sender selectedItem] representedObject] forKey:PREF_KEY_UP]; }
- (void)bindingForKeyDown:(id)sender   { [[NSUserDefaults standardUserDefaults] setValue:[[sender selectedItem] representedObject] forKey:PREF_KEY_DOWN]; }
- (void)bindingForKeyLeft:(id)sender   { [[NSUserDefaults standardUserDefaults] setValue:[[sender selectedItem] representedObject] forKey:PREF_KEY_LEFT]; }
- (void)bindingForKeyRight:(id)sender  { [[NSUserDefaults standardUserDefaults] setValue:[[sender selectedItem] representedObject] forKey:PREF_KEY_RIGHT]; }
- (void)bindingForKeyStart:(id)sender  { [[NSUserDefaults standardUserDefaults] setValue:[[sender selectedItem] representedObject] forKey:PREF_KEY_START]; }
- (void)bindingForKeySelect:(id)sender { [[NSUserDefaults standardUserDefaults] setValue:[[sender selectedItem] representedObject] forKey:PREF_KEY_SELECT]; }
@end

//

@implementation InputHandler

//Class functions ---------------------------------

+ (NSView*)createPreferencesView:(float)width
{
	NSArray *keys = [NSArray arrayWithObjects:
		@"0", @"1", @"2", @"3", @"4", @"5", @"6", @"7", @"8", @"9",
		@"A", @"B", @"C", @"D", @"E", @"F", @"G", @"H", @"I", @"J", @"K", @"L",
		@"M", @"N",	@"O", @"P", @"Q", @"R", @"S", @"T", @"V", @"W", @"X", @"Y", @"Z",
		@",", @"<", @".", @">", @"/", @"?", @";", @":", @"'", @"\"", @"[", @"{", @"]", @"}", @"\\", @"|",   
		@"Up Key"   , @"Down Key" , @"Left Key" ,
		@"Right Key", @"Space Bar", @"Enter Key",
		nil];

	NSArray *objects = [NSArray arrayWithObjects:
		@"0" , @"1" , @"2" , @"3" , @"4" , @"5" , @"6" , @"7" , @"8" , @"9",
		@"aA", @"bB", @"cC", @"dD", @"eE", @"fF", @"gG", @"hH", @"iI", @"jJ", @"kK", @"lL",
		@"mM", @"nN", @"oO", @"pP", @"qQ", @"rR", @"sS", @"tT", @"vV", @"wW", @"xX", @"yY", @"zZ",
		@",", @"<", @".", @">", @"/", @"?", @";", @":", @"'", @"\"", @"[", @"{", @"]", @"}", @"\\", @"|",   
		[[[NSString alloc] initWithBytesNoCopy:utf8_up length:3 encoding:NSUTF8StringEncoding freeWhenDone:NO] autorelease],
		[[[NSString alloc] initWithBytesNoCopy:utf8_down length:3 encoding:NSUTF8StringEncoding freeWhenDone:NO] autorelease],
		[[[NSString alloc] initWithBytesNoCopy:utf8_left length:3 encoding:NSUTF8StringEncoding freeWhenDone:NO] autorelease],
		[[[NSString alloc] initWithBytesNoCopy:utf8_right length:3 encoding:NSUTF8StringEncoding freeWhenDone:NO] autorelease],
		@" ",
		[[[NSString alloc] initWithBytesNoCopy:&utf8_return length:1 encoding:NSUTF8StringEncoding freeWhenDone:NO] autorelease],
		nil];
	
	NSDictionary *keyboardMap = [[NSDictionary alloc] initWithObjects:objects forKeys:keys];

	NSDictionary *controls_options = [NSDictionary dictionaryWithObjectsAndKeys:

		[NSArray arrayWithObjects:@"Dictionary", [NSData dataWithBytes:&@selector(bindingForKeyA:) length:sizeof(SEL)], keyboardMap , nil] , PREF_KEY_A,
		[NSArray arrayWithObjects:@"Dictionary", [NSData dataWithBytes:&@selector(bindingForKeyB:) length:sizeof(SEL)], keyboardMap , nil] , PREF_KEY_B,
		[NSArray arrayWithObjects:@"Dictionary", [NSData dataWithBytes:&@selector(bindingForKeyX:) length:sizeof(SEL)], keyboardMap , nil] , PREF_KEY_X,
		[NSArray arrayWithObjects:@"Dictionary", [NSData dataWithBytes:&@selector(bindingForKeyY:) length:sizeof(SEL)], keyboardMap , nil] , PREF_KEY_Y,
		[NSArray arrayWithObjects:@"Dictionary", [NSData dataWithBytes:&@selector(bindingForKeyL:) length:sizeof(SEL)], keyboardMap , nil] , PREF_KEY_L,
		[NSArray arrayWithObjects:@"Dictionary", [NSData dataWithBytes:&@selector(bindingForKeyR:) length:sizeof(SEL)], keyboardMap , nil] , PREF_KEY_R,
		[NSArray arrayWithObjects:@"Dictionary", [NSData dataWithBytes:&@selector(bindingForKeyUp:) length:sizeof(SEL)], keyboardMap , nil] , PREF_KEY_UP,
		[NSArray arrayWithObjects:@"Dictionary", [NSData dataWithBytes:&@selector(bindingForKeyDown:) length:sizeof(SEL)], keyboardMap , nil] , PREF_KEY_DOWN,			
		[NSArray arrayWithObjects:@"Dictionary", [NSData dataWithBytes:&@selector(bindingForKeyLeft:) length:sizeof(SEL)], keyboardMap , nil] , PREF_KEY_LEFT,
		[NSArray arrayWithObjects:@"Dictionary", [NSData dataWithBytes:&@selector(bindingForKeyRight:) length:sizeof(SEL)], keyboardMap , nil] , PREF_KEY_RIGHT,			
		[NSArray arrayWithObjects:@"Dictionary", [NSData dataWithBytes:&@selector(bindingForKeyStart:) length:sizeof(SEL)], keyboardMap , nil] , PREF_KEY_START,
		[NSArray arrayWithObjects:@"Dictionary", [NSData dataWithBytes:&@selector(bindingForKeySelect:) length:sizeof(SEL)], keyboardMap , nil] , PREF_KEY_SELECT,				

		nil];
	
	return createPreferencesView(@"Use the popup buttons on the right to change settings", controls_options, [ControlsDelegate sharedObject]);
}

+ (NSDictionary*)appDefaults
{
	return  [NSDictionary dictionaryWithObjectsAndKeys:
	@"vV", PREF_KEY_A,
	@"bB", PREF_KEY_B,
	@"gG", PREF_KEY_X,
	@"hH", PREF_KEY_Y,
	@"cC", PREF_KEY_L,
	@"nN", PREF_KEY_R,
	@" ", PREF_KEY_SELECT,
	[[[NSString alloc] initWithBytesNoCopy:utf8_up length:3 encoding:NSUTF8StringEncoding freeWhenDone:NO] autorelease], PREF_KEY_UP,
	[[[NSString alloc] initWithBytesNoCopy:utf8_down length:3 encoding:NSUTF8StringEncoding freeWhenDone:NO] autorelease], PREF_KEY_DOWN,
	[[[NSString alloc] initWithBytesNoCopy:utf8_left length:3 encoding:NSUTF8StringEncoding freeWhenDone:NO] autorelease], PREF_KEY_LEFT,
	[[[NSString alloc] initWithBytesNoCopy:utf8_right length:3 encoding:NSUTF8StringEncoding freeWhenDone:NO] autorelease], PREF_KEY_RIGHT,
	[[[NSString alloc] initWithBytesNoCopy:&utf8_return length:1 encoding:NSUTF8StringEncoding freeWhenDone:NO] autorelease], PREF_KEY_START,
	nil];
}

//Member Functions -----------------------------------

- (id)init
{
	//make sure we go through through the designated init function
	[self doesNotRecognizeSelector:_cmd];
	return nil;
}

- (id) initWithCdsController:(CocoaDSController *)theController
{
	self = [super init];
	if(self == nil)
	{
		return nil;
	}
	
	cdsController = theController;
	[cdsController retain];
	
	return self;
}

- (void)dealloc
{
	[cdsController release];
	
	[super dealloc];
}

- (void) setCdsController:(CocoaDSController *)theController
{
	[cdsController release];
	cdsController = theController;
	[cdsController retain];
}

- (CocoaDSController *) cdsController
{
	return cdsController;
}

- (void)keyDown:(NSEvent*)event
{
	if([event isARepeat])return;

	NSUserDefaults *settings = [NSUserDefaults standardUserDefaults];
	NSString *chars = [event characters];
	
	     if(testKey(chars, [settings stringForKey:PREF_KEY_A     ]))[cdsController pressA];
	else if(testKey(chars, [settings stringForKey:PREF_KEY_B     ]))[cdsController pressB];
	else if(testKey(chars, [settings stringForKey:PREF_KEY_SELECT]))[cdsController pressSelect];
	else if(testKey(chars, [settings stringForKey:PREF_KEY_START ]))[cdsController pressStart];
	else if(testKey(chars, [settings stringForKey:PREF_KEY_RIGHT ]))[cdsController pressRight];
	else if(testKey(chars, [settings stringForKey:PREF_KEY_LEFT  ]))[cdsController pressLeft];
	else if(testKey(chars, [settings stringForKey:PREF_KEY_UP    ]))[cdsController pressUp];
	else if(testKey(chars, [settings stringForKey:PREF_KEY_DOWN  ]))[cdsController pressDown];
	else if(testKey(chars, [settings stringForKey:PREF_KEY_R     ]))[cdsController pressR];
	else if(testKey(chars, [settings stringForKey:PREF_KEY_L     ]))[cdsController pressL];
	else if(testKey(chars, [settings stringForKey:PREF_KEY_X     ]))[cdsController pressX];
	else if(testKey(chars, [settings stringForKey:PREF_KEY_Y     ]))[cdsController pressY];
}

- (void)keyUp:(NSEvent*)event
{
	NSUserDefaults *settings = [NSUserDefaults standardUserDefaults];
	NSString *chars = [event characters];
	
	     if(testKey(chars, [settings stringForKey:PREF_KEY_A     ]))[cdsController liftA];
	else if(testKey(chars, [settings stringForKey:PREF_KEY_B     ]))[cdsController liftB];
	else if(testKey(chars, [settings stringForKey:PREF_KEY_SELECT]))[cdsController liftSelect];
	else if(testKey(chars, [settings stringForKey:PREF_KEY_START ]))[cdsController liftStart];
	else if(testKey(chars, [settings stringForKey:PREF_KEY_RIGHT ]))[cdsController liftRight];
	else if(testKey(chars, [settings stringForKey:PREF_KEY_LEFT  ]))[cdsController liftLeft];
	else if(testKey(chars, [settings stringForKey:PREF_KEY_UP    ]))[cdsController liftUp];
	else if(testKey(chars, [settings stringForKey:PREF_KEY_DOWN  ]))[cdsController liftDown];
	else if(testKey(chars, [settings stringForKey:PREF_KEY_R     ]))[cdsController liftR];
	else if(testKey(chars, [settings stringForKey:PREF_KEY_L     ]))[cdsController liftL];
	else if(testKey(chars, [settings stringForKey:PREF_KEY_X     ]))[cdsController liftX];
	else if(testKey(chars, [settings stringForKey:PREF_KEY_Y     ]))[cdsController liftY];
}

@end

inline int testKey(NSString *chars_pressed, NSString *chars_for_key)
{
	//Checks for common characters in chars_pressed and chars_for_key
	
	unichar *buffer1 = (unichar*)malloc([chars_pressed length] * sizeof(unichar));
	unichar *buffer2 = (unichar*)malloc([chars_for_key length] * sizeof(unichar));
	if(!buffer1 || !buffer2)return 0;
	
	[chars_pressed getCharacters:buffer1];
	[chars_for_key getCharacters:buffer2];
	
	int i1, i2;
	for(i1 = 0; i1 < [chars_pressed length]; i1++)
		for(i2 = 0; i2 < [chars_for_key length]; i2++)
			if(buffer1[i1] == buffer2[i2])
			{
				free(buffer1);
				free(buffer2);
				return 1;
			}
	
	free(buffer1);
	free(buffer2);
	return 0;	
}
