/*
	Copyright (C) 2017 DeSmuME team
 
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

#ifndef _MAC_SCREENSHOTCAPTURETOOL_H_
#define _MAC_SCREENSHOTCAPTURETOOL_H_

#import <Cocoa/Cocoa.h>

#include <string>
#include "../ClientDisplayView.h"
#include "../cocoa_util.h"

#ifdef BOOL
#undef BOOL
#endif

@class MacClientSharedObject;

#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_5
@interface MacScreenshotCaptureToolDelegate : NSObject <NSWindowDelegate, DirectoryURLDragDestTextFieldProtocol>
#else
@interface MacScreenshotCaptureToolDelegate : NSObject <DirectoryURLDragDestTextFieldProtocol>
#endif
{
	NSObject *dummyObject;
	NSWindow *window;
	DirectoryURLDragDestTextField *saveDirectoryPathTextField;
	
	MacClientSharedObject *sharedData;
	
	NSString *saveDirectoryPath;
	NSString *romName;
	NSBitmapFormat fileFormat;
	NSInteger displayMode;
	NSInteger displayLayout;
	NSInteger displayOrder;
	NSInteger displaySeparation;
	NSInteger displayScale;
	NSInteger displayRotation;
	BOOL useDeposterize;
	NSInteger outputFilterID;
	NSInteger pixelScalerID;
}

@property (readonly) IBOutlet NSObject *dummyObject;
@property (readonly) IBOutlet NSWindow *window;
@property (readonly) IBOutlet DirectoryURLDragDestTextField *saveDirectoryPathTextField;

@property (retain) MacClientSharedObject *sharedData;

@property (copy) NSString *saveDirectoryPath;
@property (copy) NSString *romName;
@property (assign) NSBitmapFormat fileFormat;
@property (assign) NSInteger displayMode;
@property (assign) NSInteger displayLayout;
@property (assign) NSInteger displayOrder;
@property (assign) NSInteger displaySeparation;
@property (assign) NSInteger displayScale;
@property (assign) NSInteger displayRotation;
@property (assign) BOOL useDeposterize;
@property (assign) NSInteger outputFilterID;
@property (assign) NSInteger pixelScalerID;

- (IBAction) chooseDirectoryPath:(id)sender;
- (IBAction) takeScreenshot:(id)sender;

- (void) chooseDirectoryPathDidEnd:(NSOpenPanel *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo;

- (void) readUserDefaults;
- (void) writeUserDefaults;

@end

class MacScreenshotCaptureToolParams
{
public:
	MacClientSharedObject *sharedData;
	
	NSBitmapImageFileType fileFormat;
	std::string savePath;
	std::string romName;
	bool useDeposterize;
	OutputFilterTypeID outputFilterID;
	VideoFilterTypeID pixelScalerID;
	ClientDisplayPresenterProperties cdpProperty;
};

static void* RunFileWriteThread(void *arg);

#endif // _MAC_SCREENSHOTCAPTURETOOL_H_
