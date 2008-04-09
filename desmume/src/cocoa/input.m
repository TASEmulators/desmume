/*  Copyright (C) 2007 Jeff Bland

	This file is part of DeSmuME

	DeSmuME is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	DeSmuME is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with DeSmuME; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#import "input.h"
#import "main_window.h"
#import "preferences.h"

@implementation InputHandler

//
- (id)initWithWindow:(VideoOutputWindow*)nds
{
	self = [super init];

	my_ds = nds;
	[my_ds retain];

	return self;
}

- (void)dealloc
{
	[my_ds release];
	[super dealloc];
}

- (void)keyDown:(NSEvent*)event
{
	if([event isARepeat])return;
	
	NSUserDefaults *settings = [NSUserDefaults standardUserDefaults];
	NSString *chars = [event characters];

	     if([chars rangeOfString:[settings stringForKey:PREF_KEY_A     ]].location!=NSNotFound)[my_ds pressA];
	else if([chars rangeOfString:[settings stringForKey:PREF_KEY_B     ]].location!=NSNotFound)[my_ds pressB];
	else if([chars rangeOfString:[settings stringForKey:PREF_KEY_SELECT]].location!=NSNotFound)[my_ds pressSelect];
	else if([chars rangeOfString:[settings stringForKey:PREF_KEY_START ]].location!=NSNotFound)[my_ds pressStart];
	else if([chars rangeOfString:[settings stringForKey:PREF_KEY_RIGHT ]].location!=NSNotFound)[my_ds pressRight];
	else if([chars rangeOfString:[settings stringForKey:PREF_KEY_LEFT  ]].location!=NSNotFound)[my_ds pressLeft];
	else if([chars rangeOfString:[settings stringForKey:PREF_KEY_UP    ]].location!=NSNotFound)[my_ds pressUp];
	else if([chars rangeOfString:[settings stringForKey:PREF_KEY_DOWN  ]].location!=NSNotFound)[my_ds pressDown];
	else if([chars rangeOfString:[settings stringForKey:PREF_KEY_R     ]].location!=NSNotFound)[my_ds pressR];
	else if([chars rangeOfString:[settings stringForKey:PREF_KEY_L     ]].location!=NSNotFound)[my_ds pressL];
	else if([chars rangeOfString:[settings stringForKey:PREF_KEY_X     ]].location!=NSNotFound)[my_ds pressX];
	else if([chars rangeOfString:[settings stringForKey:PREF_KEY_Y     ]].location!=NSNotFound)[my_ds pressY];
}

- (void)keyUp:(NSEvent*)event
{
	NSUserDefaults *settings = [NSUserDefaults standardUserDefaults];
	NSString *chars = [event characters];

	     if([chars rangeOfString:[settings stringForKey:PREF_KEY_A     ]].location!=NSNotFound)[my_ds liftA];
	else if([chars rangeOfString:[settings stringForKey:PREF_KEY_B     ]].location!=NSNotFound)[my_ds liftB];
	else if([chars rangeOfString:[settings stringForKey:PREF_KEY_SELECT]].location!=NSNotFound)[my_ds liftSelect];
	else if([chars rangeOfString:[settings stringForKey:PREF_KEY_START ]].location!=NSNotFound)[my_ds liftStart];
	else if([chars rangeOfString:[settings stringForKey:PREF_KEY_RIGHT ]].location!=NSNotFound)[my_ds liftRight];
	else if([chars rangeOfString:[settings stringForKey:PREF_KEY_LEFT  ]].location!=NSNotFound)[my_ds liftLeft];
	else if([chars rangeOfString:[settings stringForKey:PREF_KEY_UP    ]].location!=NSNotFound)[my_ds liftUp];
	else if([chars rangeOfString:[settings stringForKey:PREF_KEY_DOWN  ]].location!=NSNotFound)[my_ds liftDown];
	else if([chars rangeOfString:[settings stringForKey:PREF_KEY_R     ]].location!=NSNotFound)[my_ds liftR];
	else if([chars rangeOfString:[settings stringForKey:PREF_KEY_L     ]].location!=NSNotFound)[my_ds liftL];
	else if([chars rangeOfString:[settings stringForKey:PREF_KEY_X     ]].location!=NSNotFound)[my_ds liftX];
	else if([chars rangeOfString:[settings stringForKey:PREF_KEY_Y     ]].location!=NSNotFound)[my_ds liftY];
}

- (void)mouseDown:(NSEvent*)event
{
	NSPoint temp = [my_ds windowPointToDSCoords:[event locationInWindow]];
	if(temp.x >= 0 && temp.y>=0)[my_ds touch:temp];
}

- (void)mouseDragged:(NSEvent*)event
{
	[self mouseDown:event];
}

- (void)mouseUp:(NSEvent*)event
{
	[my_ds releaseTouch];
}

@end

