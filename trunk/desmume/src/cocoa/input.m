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

//Default key config (based on the windows version)
unsigned short ds_up     = 126; //up arrow
unsigned short ds_down   = 125; //down arrow
unsigned short ds_left   = 123; //left arrow
unsigned short ds_right  = 124; //right arrrow
unsigned short ds_a      = 9  ; //v
unsigned short ds_b      = 11 ; //b
unsigned short ds_x      = 5  ; //g
unsigned short ds_y      = 4  ; //h
unsigned short ds_l      = 8  ; //c
unsigned short ds_r      = 45 ; //n
unsigned short ds_select = 49 ; //space bar
unsigned short ds_start  = 36 ; //enter

//
unsigned short save_slot_1  = 122; //F1
unsigned short save_slot_2  = 120; //F2
unsigned short save_slot_3  =  99; //F3
unsigned short save_slot_4  = 118; //F4
unsigned short save_slot_5  =  96; //F5
unsigned short save_slot_6  =  97; //F6
unsigned short save_slot_7  =  98; //F7
unsigned short save_slot_8  = 100; //F8
unsigned short save_slot_9  = 101; //F9
unsigned short save_slot_10 = 109; //F10
//unsigned short save_slot_11 = 103; //F11
//unsigned short save_slot_12 = 111; //F12

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

	unsigned short keycode = [event keyCode];

	if(keycode == ds_a)[my_ds pressA];
	else if(keycode == ds_b)[my_ds pressB];
	else if(keycode == ds_select)[my_ds pressSelect];
	else if(keycode == ds_start)[my_ds pressStart];
	else if(keycode == ds_right)[my_ds pressRight];
	else if(keycode == ds_left)[my_ds pressLeft];
	else if(keycode == ds_up)[my_ds pressUp];
	else if(keycode == ds_down)[my_ds pressDown];
	else if(keycode == ds_r)[my_ds pressR];
	else if(keycode == ds_l)[my_ds pressL];
	else if(keycode == ds_x)[my_ds pressX];
	else if(keycode == ds_y)[my_ds pressY];
}

- (void)keyUp:(NSEvent*)event
{
	unsigned short keycode = [event keyCode];

	if(keycode == ds_a)[my_ds liftA];
	else if(keycode == ds_b)[my_ds liftB];
	else if(keycode == ds_select)[my_ds liftSelect];
	else if(keycode == ds_start)[my_ds liftStart];
	else if(keycode == ds_right)[my_ds liftRight];
	else if(keycode == ds_left)[my_ds liftLeft];
	else if(keycode == ds_up)[my_ds liftUp];
	else if(keycode == ds_down)[my_ds liftDown];
	else if(keycode == ds_r)[my_ds liftR];
	else if(keycode == ds_l)[my_ds liftL];
	else if(keycode == ds_x)[my_ds liftX];
	else if(keycode == ds_y)[my_ds liftY];
/*
	else if(keycode == save_slot_1)
	{
		if([event modifierFlags] & NSShiftKeyMask)
			savestate_slot(1);
		else
			loadstate_slot(1);
	}

	else if(keycode == save_slot_2)
	{
		if([event modifierFlags] & NSShiftKeyMask)
			savestate_slot(2);
		else
			loadstate_slot(2);
	}

	else if(keycode == save_slot_3)
	{
		if([event modifierFlags] & NSShiftKeyMask)
			savestate_slot(3);
		else
			loadstate_slot(3);
	}

	else if(keycode == save_slot_4)
	{
		if([event modifierFlags] & NSShiftKeyMask)
			savestate_slot(4);
		else
			loadstate_slot(4);
	}

	else if(keycode == save_slot_5)
	{
		if([event modifierFlags] & NSShiftKeyMask)
			savestate_slot(5);
		else
			loadstate_slot(5);
	}

	else if(keycode == save_slot_6)
	{
		if([event modifierFlags] & NSShiftKeyMask)
			savestate_slot(6);
		else
			loadstate_slot(6);
	}

	else if(keycode == save_slot_7)
	{
		if([event modifierFlags] & NSShiftKeyMask)
			savestate_slot(7);
		else
			loadstate_slot(7);
	}

	else if(keycode == save_slot_8)
	{
		if([event modifierFlags] & NSShiftKeyMask)
			savestate_slot(8);
		else
			loadstate_slot(8);
	}

	else if(keycode == save_slot_9)
	{
		if([event modifierFlags] & NSShiftKeyMask)
			savestate_slot(9);
		else
			loadstate_slot(9);
	}

	else if(keycode == save_slot_10)
	{
		if([event modifierFlags] & NSShiftKeyMask)
			savestate_slot(10);
		else
			loadstate_slot(10);
	}
*/
}

- (void)mouseDown:(NSEvent*)event
{
	[my_ds touch:[my_ds windowPointToDSCoords:[event locationInWindow]]];
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

