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

#import "cocoa_input_legacy.h"
#include "../NDSSystem.h"


@implementation CocoaDSInput

- (id) init
{
	isStateChanged = false;
	
	property = [[NSMutableDictionary alloc] init];
	[property setValue:[NSDate date] forKey:@"inputTime"];
	
	return self;
}

- (void) dealloc
{
	[property release];
	
	[super dealloc];
}

- (void) setIsStateChanged:(bool)state
{
	isStateChanged = state;
}

- (bool) isStateChanged
{
	return isStateChanged;
}

- (void) setInputTime:(NSDate*)inputTime
{
	[property setValue:inputTime forKey:@"inputTime"];
}

- (NSDate*) getInputTime
{
	return [property objectForKey:@"inputTime"];
}

@end

@implementation CocoaDSButton

- (id) init
{
	[super init];
	[property setValue:[NSNumber numberWithBool:false] forKey:@"press"];
	
	return self;
}

- (void) dealloc
{
	[super dealloc];
}

- (void) setPressed:(bool)inputValue
{
	// Check for state change and set the flag appropriately.
	if (inputValue != [self getPressed])
	{
		isStateChanged = true;
	}
	else
	{
		isStateChanged = false;
	}
	
	[property setValue:[NSDate date] forKey:@"inputTime"];
	[property setValue:[NSNumber numberWithBool:inputValue] forKey:@"press"];
}

- (bool) getPressed
{
	return [[property objectForKey:@"press"] boolValue];
}

@end

@implementation CocoaDSTouch

- (id) init
{
	[super init];
	[property setValue:[NSNumber numberWithBool:false] forKey:@"touch"];
	[property setValue:[NSNumber numberWithFloat:0.0] forKey:@"x"];
	[property setValue:[NSNumber numberWithFloat:0.0] forKey:@"y"];
	
	return self;
}

- (void) dealloc
{
	[super dealloc];
}

- (void) setTouching:(bool)inputValue
{
	// Check for state change and set the flag appropriately.
	if (inputValue != [self getTouching])
	{
		isStateChanged = true;
	}
	else
	{
		isStateChanged = false;
	}
	
	[property setValue:[NSDate date] forKey:@"inputTime"];
	[property setValue:[NSNumber numberWithBool:inputValue] forKey:@"touch"];
}

- (bool) getTouching
{
	return [[property objectForKey:@"touch"] boolValue];
}

- (void) setX:(float)inputValue
{
	// Check for state change and set the flag appropriately.
	if (inputValue != [self getX])
	{
		isStateChanged = true;
	}
	else
	{
		isStateChanged = false;
	}
	
	[property setValue:[NSDate date] forKey:@"inputTime"];
	[property setValue:[NSNumber numberWithFloat:inputValue] forKey:@"x"];
}

- (float) getX
{
	return [[property objectForKey:@"x"] floatValue];
}

- (void) setY:(float)inputValue
{
	// Check for state change and set the flag appropriately.
	if (inputValue != [self getY])
	{
		isStateChanged = true;
	}
	else
	{
		isStateChanged = false;
	}
	
	[property setValue:[NSDate date] forKey:@"inputTime"];
	[property setValue:[NSNumber numberWithFloat:inputValue] forKey:@"y"];
}

- (float) getY
{
	return [[property objectForKey:@"y"] floatValue];
}

- (void) setPoint:(NSPoint)inputValue
{
	// Check for state change and set the flag appropriately.
	if (inputValue.x != [self getX] || inputValue.y != [self getY])
	{
		isStateChanged = true;
	}
	else
	{
		isStateChanged = false;
	}
	
	[self setX:inputValue.x];
	[self setY:inputValue.y];
}

- (NSPoint) getPoint
{
	NSPoint outPoint = {[self getX], [self getY]};
	return outPoint;
}

- (void) setTouchingWithCoords:(bool)isTouching x:(float)xValue y:(float)yValue
{
	// Check for state change and set the flag appropriately.
	if (isTouching != [self getTouching] ||
		xValue != [self getX] ||
		yValue != [self getY])
	{
		isStateChanged = true;
	}
	else
	{
		isStateChanged = false;
	}
	
	[property setValue:[NSDate date] forKey:@"inputTime"];
	[property setValue:[NSNumber numberWithBool:isTouching] forKey:@"touch"];
	[property setValue:[NSNumber numberWithFloat:xValue] forKey:@"x"];
	[property setValue:[NSNumber numberWithFloat:yValue] forKey:@"y"];
}

- (void) setTouchingWithPoint:(bool)isTouching point:(NSPoint)inputPoint
{
	[self setTouchingWithCoords:isTouching x:inputPoint.x y:inputPoint.y];
}

@end

@implementation CocoaDSMic

- (id) init
{
	[super init];
	[property setValue:[NSNumber numberWithBool:false] forKey:@"press"];
	
	return self;
}

- (void) dealloc
{
	[super dealloc];
}

- (void) setPressed:(bool)inputValue
{
	[property setValue:[NSDate date] forKey:@"inputTime"];
	[property setValue:[NSNumber numberWithBool:inputValue] forKey:@"press"];
}

- (bool) getPressed
{
	return [[property objectForKey:@"press"] boolValue];
}

@end


@implementation CocoaDSController

- (id) init
{
	ndsButton_Up		= [[CocoaDSButton alloc] init];
	ndsButton_Down		= [[CocoaDSButton alloc] init];
	ndsButton_Left		= [[CocoaDSButton alloc] init];
	ndsButton_Right		= [[CocoaDSButton alloc] init];
	ndsButton_A			= [[CocoaDSButton alloc] init];
	ndsButton_B			= [[CocoaDSButton alloc] init];
	ndsButton_X			= [[CocoaDSButton alloc] init];
	ndsButton_Y			= [[CocoaDSButton alloc] init];
	ndsButton_Select	= [[CocoaDSButton alloc] init];
	ndsButton_Start		= [[CocoaDSButton alloc] init];
	ndsButton_R			= [[CocoaDSButton alloc] init];
	ndsButton_L			= [[CocoaDSButton alloc] init];
	ndsButton_Debug		= [[CocoaDSButton alloc] init];
	ndsButton_Lid		= [[CocoaDSButton alloc] init];
	
	ndsTouch			= [[CocoaDSTouch alloc] init];
	
	ndsMic				= [[CocoaDSMic alloc] init];
	
	return self;
}

- (void) dealloc
{
	[ndsButton_Up release];
	[ndsButton_Down release];
	[ndsButton_Left release];
	[ndsButton_Right release];
	[ndsButton_A release];
	[ndsButton_B release];
	[ndsButton_X release];
	[ndsButton_Y release];
	[ndsButton_Select release];
	[ndsButton_Start release];
	[ndsButton_R release];
	[ndsButton_L release];
	[ndsButton_Debug release];
	[ndsButton_Lid release];
	[ndsTouch release];
	[ndsMic release];
	
	[super dealloc];
}

- (void) setupAllDSInputs
{
	// Setup the DS pad.
	NDS_setPad([self getRightPressed],
			   [self getLeftPressed],
			   [self getDownPressed],
			   [self getUpPressed],
			   [self getSelectPressed],
			   [self getStartPressed],
			   [self getBPressed],
			   [self getAPressed],
			   [self getYPressed],
			   [self getXPressed],
			   [self getLPressed],
			   [self getRPressed],
			   [self getDebugPressed],
			   [self getLidPressed]);
	
	// Setup the DS touch pad.
	if ([self getTouching])
	{
		NDS_setTouchPos([self getTouchXCoord], [self getTouchYCoord]);
	}
	else
	{
		NDS_releaseTouch();
	}
	
	// Setup the DS mic.
	NDS_setMic([self getMicPressed]);
}

- (void)touch:(NSPoint)point
{
	[ndsTouch setTouchingWithPoint:true point:point];
}

- (void)releaseTouch
{
	[ndsTouch setTouching:false];
}

- (bool) getTouching
{
	return [ndsTouch getTouching];
}

- (NSPoint) getTouchPoint
{
	return [ndsTouch getPoint];
}

- (float) getTouchXCoord
{
	return [ndsTouch getX];
}

- (float) getTouchYCoord
{
	return [ndsTouch getY];
}

- (void)pressStart
{
	[ndsButton_Start setPressed:true];
}

- (void)liftStart
{
	[ndsButton_Start setPressed:false];
}

- (bool) getStartPressed
{
	return [ndsButton_Start getPressed];
}

- (void)pressSelect
{
	[ndsButton_Select setPressed:true];
}

- (void)liftSelect
{
	[ndsButton_Select setPressed:false];
}

- (bool) getSelectPressed
{
	return [ndsButton_Select getPressed];
}

- (void)pressLeft
{
	[ndsButton_Left setPressed:true];
}

- (void)liftLeft
{
	[ndsButton_Left setPressed:false];
}

- (bool) getLeftPressed
{
	return [ndsButton_Left getPressed];
}

- (void)pressRight
{
	[ndsButton_Right setPressed:true];
}

- (void)liftRight
{
	[ndsButton_Right setPressed:false];
}

- (bool) getRightPressed
{
	return [ndsButton_Right getPressed];
}

- (void)pressUp
{
	[ndsButton_Up setPressed:true];
}

- (void)liftUp
{
	[ndsButton_Up setPressed:false];
}

- (bool) getUpPressed
{
	return [ndsButton_Up getPressed];
}

- (void)pressDown
{
	[ndsButton_Down setPressed:true];
}

- (void)liftDown
{
	[ndsButton_Down setPressed:false];
}

- (bool) getDownPressed
{
	return [ndsButton_Down getPressed];
}

- (void)pressA
{
	[ndsButton_A setPressed:true];
}

- (void)liftA
{
	[ndsButton_A setPressed:false];
}

- (bool) getAPressed
{
	return [ndsButton_A getPressed];
}

- (void)pressB
{
	[ndsButton_B setPressed:true];
}

- (void)liftB
{
	[ndsButton_B setPressed:false];
}

- (bool) getBPressed
{
	return [ndsButton_B getPressed];
}

- (void)pressX
{
	[ndsButton_X setPressed:true];
}

- (void)liftX
{
	[ndsButton_X setPressed:false];
}

- (bool) getXPressed
{
	return [ndsButton_X getPressed];
}

- (void)pressY
{
	[ndsButton_Y setPressed:true];
}

- (void)liftY
{
	[ndsButton_Y setPressed:false];
}

- (bool) getYPressed
{
	return [ndsButton_Y getPressed];
}

- (void)pressL
{
	[ndsButton_L setPressed:true];
}

- (void)liftL
{
	[ndsButton_L setPressed:false];
}

- (bool) getLPressed
{
	return [ndsButton_L getPressed];
}

- (void)pressR
{
	[ndsButton_R setPressed:true];
}

- (void)liftR
{
	[ndsButton_R setPressed:false];
}

- (bool) getRPressed
{
	return [ndsButton_R getPressed];
}

- (void) pressDebug
{
	[ndsButton_Debug setPressed:true];
}

- (void) liftDebug
{
	[ndsButton_Debug setPressed:false];
}

- (bool) getDebugPressed
{
	return [ndsButton_Debug getPressed];
}

- (void) pressLid
{
	[ndsButton_Lid setPressed:true];
}

- (void) liftLid
{
	[ndsButton_Lid setPressed:false];
}

- (bool) getLidPressed
{
	return [ndsButton_Lid getPressed];
}

- (void) pressMic
{
	[ndsMic setPressed:true];
}

- (void) liftMic
{
	[ndsMic setPressed:false];
}

- (bool) getMicPressed
{
	return [ndsMic getPressed];
}

@end