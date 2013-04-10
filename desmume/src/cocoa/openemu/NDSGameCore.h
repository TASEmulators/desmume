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
#import "OENDSSystemResponderClient.h"
#include <libkern/OSAtomic.h>

@class CocoaDSFirmware;
@class CocoaDSController;


@interface NDSGameCore : OEGameCore
{
	NSPoint touchLocation;
	CocoaDSController *cdsController;
	CocoaDSFirmware *firmware;
	NSInteger micMode;
	NSInteger displayMode;
	OEIntRect displayRect;
	NSInteger inputID[OENDSButtonCount]; // Key = OpenEmu's input ID, Value = DeSmuME's input ID
	
	OSSpinLock spinlockDisplayMode;
}

@property (retain) CocoaDSController *cdsController;
@property (retain) CocoaDSFirmware *firmware;
@property (assign) NSInteger micMode;
@property (assign) NSInteger displayMode;

@end
