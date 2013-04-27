/*  Copyright (C) 2011 Roger Manuel
 
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

#import <Cocoa/Cocoa.h>


@interface CocoaDSInput : NSObject
{
	bool isStateChanged;
	NSMutableDictionary *property;
}

- (id) init;
- (void) dealloc;
- (void) setIsStateChanged:(bool)state;
- (bool) isStateChanged;
- (void) setInputTime:(NSDate*)inputTime;
- (NSDate*) getInputTime;

@end

@interface CocoaDSButton : CocoaDSInput
{
	
}

- (id) init;
- (void) dealloc;
- (void) setPressed:(bool)inputValue;
- (bool) getPressed;

@end

@interface CocoaDSTouch : CocoaDSInput
{
	
}

- (id) init;
- (void) dealloc;

- (void) setTouching:(bool)inputValue;
- (bool) getTouching;

- (void) setX:(float)inputValue;
- (float) getX;

- (void) setY:(float)inputValue;
- (float) getY;

- (void) setPoint:(NSPoint)inputValue;
- (NSPoint) getPoint;

- (void) setTouchingWithCoords:(bool)isTouching x:(float)xValue y:(float)yValue;
- (void) setTouchingWithPoint:(bool)isTouching point:(NSPoint)inputPoint;

@end

@interface CocoaDSMic : CocoaDSInput
{
	
}

- (id) init;
- (void) dealloc;
- (void) setPressed:(bool)inputValue;
- (bool) getPressed;

@end


@interface CocoaDSController : NSObject
{
	CocoaDSButton *ndsButton_Up;
	CocoaDSButton *ndsButton_Down;
	CocoaDSButton *ndsButton_Left;
	CocoaDSButton *ndsButton_Right;
	CocoaDSButton *ndsButton_A;
	CocoaDSButton *ndsButton_B;
	CocoaDSButton *ndsButton_X;
	CocoaDSButton *ndsButton_Y;
	CocoaDSButton *ndsButton_Select;
	CocoaDSButton *ndsButton_Start;
	CocoaDSButton *ndsButton_R;
	CocoaDSButton *ndsButton_L;
	CocoaDSButton *ndsButton_Debug;
	CocoaDSButton *ndsButton_Lid;
	
	CocoaDSTouch *ndsTouch;
	
	CocoaDSMic *ndsMic;
}

- (id) init;
- (void) dealloc;

- (void) setupAllDSInputs;

//touch screen
- (void)touch:(NSPoint)point;
- (void)releaseTouch;
- (bool) getTouching;
- (NSPoint) getTouchPoint;
- (float) getTouchXCoord;
- (float) getTouchYCoord;

//button input
- (void)pressStart;
- (void)liftStart;
- (bool) getStartPressed;

- (void)pressSelect;
- (void)liftSelect;
- (bool) getSelectPressed;

- (void)pressLeft;
- (void)liftLeft;
- (bool) getLeftPressed;

- (void)pressRight;
- (void)liftRight;
- (bool) getRightPressed;

- (void)pressUp;
- (void)liftUp;
- (bool) getUpPressed;

- (void)pressDown;
- (void)liftDown;
- (bool) getDownPressed;

- (void)pressA;
- (void)liftA;
- (bool) getAPressed;

- (void)pressB;
- (void)liftB;
- (bool) getBPressed;

- (void)pressX;
- (void)liftX;
- (bool) getXPressed;

- (void)pressY;
- (void)liftY;
- (bool) getYPressed;

- (void)pressL;
- (void)liftL;
- (bool) getLPressed;

- (void)pressR;
- (void)liftR;
- (bool) getRPressed;

- (void) pressDebug;
- (void) liftDebug;
- (bool) getDebugPressed;

- (void) pressLid;
- (void) liftLid;
- (bool) getLidPressed;

// Nintendo DS Mic
- (void) pressMic;
- (void) liftMic;
- (bool) getMicPressed;

@end