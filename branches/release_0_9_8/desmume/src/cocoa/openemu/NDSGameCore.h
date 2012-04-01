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
#import <OpenEmuBase/OEGameCore.h>
#include <libkern/OSAtomic.h>

@class CocoaDSFirmware;
@class CocoaDSMic;


@interface NDSGameCore : OEGameCore
{
	bool *input;
	bool isTouchPressed;
	OEIntPoint touchLocation;
	CocoaDSFirmware *firmware;
	CocoaDSMic *microphone;
	NSInteger displayMode;
	OEIntRect displayRect;
	
	OSSpinLock spinlockDisplayMode;
}

@property (retain) CocoaDSFirmware *firmware;
@property (retain) CocoaDSMic *microphone;
@property (assign) NSInteger displayMode;

@end
