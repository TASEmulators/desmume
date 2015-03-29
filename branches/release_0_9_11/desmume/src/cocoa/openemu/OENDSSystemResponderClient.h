/*
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

@protocol OESystemResponderClient;

typedef enum _OENDSButton
{
	OENDSButtonUp,
	OENDSButtonDown,
	OENDSButtonLeft,
	OENDSButtonRight,
	OENDSButtonA,
	OENDSButtonB,
	OENDSButtonX,
    OENDSButtonY,
	OENDSButtonL,
    OENDSButtonR,
    OENDSButtonStart,
	OENDSButtonSelect,
    OENDSButtonMicrophone,
	OENDSButtonLid,
	OENDSButtonDebug,
	OENDSButtonCount
} OENDSButton;

@protocol OENDSSystemResponderClient <OESystemResponderClient, NSObject>

- (oneway void)didPushNDSButton:(OENDSButton)button forPlayer:(NSUInteger)player;
- (oneway void)didReleaseNDSButton:(OENDSButton)button forPlayer:(NSUInteger)player;
- (oneway void)didTouchScreenPoint:(OEIntPoint)aPoint;
- (oneway void)didReleaseTouch;

@end
