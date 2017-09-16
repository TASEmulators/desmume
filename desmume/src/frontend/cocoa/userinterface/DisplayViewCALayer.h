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

#ifndef _DISPLAYVIEWCALAYER_H
#define _DISPLAYVIEWCALAYER_H

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>

#import "InputManager.h"

@class CocoaDSDisplayVideo;
@class MacClientSharedObject;
class ClientDisplay3DView;

@protocol DisplayViewCALayer <NSObject>

@required
- (ClientDisplay3DView *) clientDisplay3DView;

@end

@protocol CocoaDisplayViewProtocol <InputHIDManagerTarget, NSObject>

@required
@property (retain) InputManager *inputManager;
@property (retain) CocoaDSDisplayVideo *cdsVideoOutput;
@property (readonly, nonatomic) ClientDisplay3DView *clientDisplay3DView;
@property (readonly) BOOL canUseShaderBasedFilters;
@property (assign, nonatomic) BOOL allowViewUpdates;
@property (assign) BOOL isHUDVisible;
@property (assign) BOOL isHUDVideoFPSVisible;
@property (assign) BOOL isHUDRender3DFPSVisible;
@property (assign) BOOL isHUDFrameIndexVisible;
@property (assign) BOOL isHUDLagFrameCountVisible;
@property (assign) BOOL isHUDCPULoadAverageVisible;
@property (assign) BOOL isHUDRealTimeClockVisible;
@property (assign) BOOL isHUDInputVisible;
@property (assign) NSColor *hudColorVideoFPS;
@property (assign) NSColor *hudColorRender3DFPS;
@property (assign) NSColor *hudColorFrameIndex;
@property (assign) NSColor *hudColorLagFrameCount;
@property (assign) NSColor *hudColorCPULoadAverage;
@property (assign) NSColor *hudColorRTC;
@property (assign) NSColor *hudColorInputPendingAndApplied;
@property (assign) NSColor *hudColorInputAppliedOnly;
@property (assign) NSColor *hudColorInputPendingOnly;
@property (assign) NSInteger displayMainVideoSource;
@property (assign) NSInteger displayTouchVideoSource;
@property (assign) BOOL useVerticalSync;
@property (assign) BOOL videoFiltersPreferGPU;
@property (assign) BOOL sourceDeposterize;
@property (assign) NSInteger outputFilter;
@property (assign) NSInteger pixelScaler;

- (void) setupLayer;
- (void) requestScreenshot:(NSURL *)fileURL fileType:(NSBitmapImageFileType)fileType;

@end

class DisplayViewCALayerInterface
{
private:
	NSView *_nsView;
	CALayer<DisplayViewCALayer> *_frontendLayer;
	bool _willRenderToCALayer;
	
	MacClientSharedObject *_sharedData;
	
public:
	DisplayViewCALayerInterface();
	
	NSView* GetNSView() const;
	void SetNSView(NSView *theView);
	
	CALayer<DisplayViewCALayer>* GetFrontendLayer() const;
	void SetFrontendLayer(CALayer<DisplayViewCALayer> *layer);
	void CALayerDisplay();
	
	bool GetRenderToCALayer() const;
	void SetRenderToCALayer(const bool renderToLayer);
	
	MacClientSharedObject* GetSharedData();
	void SetSharedData(MacClientSharedObject *sharedObject);
};

#endif // _DISPLAYVIEWCALAYER_H
