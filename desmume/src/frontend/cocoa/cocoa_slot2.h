/*
	Copyright (C) 2014 DeSmuME Team

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
#include "../../slot2.h"
#undef BOOL

@interface CocoaDSSlot2Device : NSObject
{
	ISlot2Interface *device;
	BOOL enabled;
}

@property (readonly, copy) NSString *name;
@property (readonly, copy) NSString *description;
@property (readonly) NSInteger deviceID;
@property (readonly) NDS_SLOT2_TYPE type;
@property (assign) BOOL enabled;

- (id) initWithDeviceData:(ISlot2Interface *)deviceData;

@end

@interface CocoaDSSlot2Manager : NSObject
{
	NSMutableArray *deviceList;
	CocoaDSSlot2Device *currentDevice;
	NSString *slot2StatusText;
}

@property (readonly, strong) NSMutableArray *deviceList;
@property (nonatomic, strong) CocoaDSSlot2Device *currentDevice;
@property (copy) NSString *slot2StatusText;
@property (copy) NSURL *mpcfFileSearchURL;
@property (copy) NSURL *gbaCartridgeURL;
@property (copy) NSURL *gbaSRamURL;
@property (readonly) BOOL doesGbaCartridgeSaveExist;

- (CocoaDSSlot2Device *) autoSelectedDevice;
- (NSString *) autoSelectedDeviceName;
- (CocoaDSSlot2Device *) findDeviceByType:(NDS_SLOT2_TYPE)theType;
- (void) setDeviceByType:(NDS_SLOT2_TYPE)theType;
- (void) updateDeviceList;
- (void) updateStatus;

@end

// Force Feedback
void OSXSendForceFeedbackState(bool enable);
