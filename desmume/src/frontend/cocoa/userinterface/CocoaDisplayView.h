/*
	Copyright (C) 2025 DeSmuME team

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

#ifndef _COCOA_DISPLAY_VIEW_H_
#define _COCOA_DISPLAY_VIEW_H_

#import <Cocoa/Cocoa.h>

#import "InputManager.h"
#import "DisplayViewCALayer.h"

#include "../ClientVideoOutput.h"

struct ClientDisplayPresenterProperties;
class MacDisplayViewOutput;
class MacGPUFetchObjectDisplayLink;

@protocol CocoaDisplayViewProtocol <InputHIDManagerTarget, NSObject>

@required
@property (retain) InputManager *inputManager;
@property (readonly, nonatomic) MacDisplayViewOutput *displayViewOutput;
@property (readonly) BOOL canUseShaderBasedFilters;
@property (assign, nonatomic) BOOL allowViewUpdates;
@property (assign) BOOL isHUDVisible;
@property (assign) BOOL isHUDExecutionSpeedVisible;
@property (assign) BOOL isHUDVideoFPSVisible;
@property (assign) BOOL isHUDRender3DFPSVisible;
@property (assign) BOOL isHUDFrameIndexVisible;
@property (assign) BOOL isHUDLagFrameCountVisible;
@property (assign) BOOL isHUDCPULoadAverageVisible;
@property (assign) BOOL isHUDRealTimeClockVisible;
@property (assign) BOOL isHUDInputVisible;
@property (assign) NSColor *hudColorExecutionSpeed;
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
@property (assign) NSInteger displayViewID;
@property (assign) BOOL useVerticalSync;
@property (assign) BOOL videoFiltersPreferGPU;
@property (assign) BOOL sourceDeposterize;
@property (assign) NSInteger outputFilter;
@property (assign) NSInteger pixelScaler;
@property (assign) double scaleFactor;

- (void) setupLayer;
- (void) updateLayerPresenterProperties:(ClientDisplayPresenterProperties &)props scaleFactor:(const double)scaleFactor needFlush:(BOOL)needFlush;
- (void) isPixelScalerSupportedByID:(NSInteger)filterID cpu:(BOOL &)outSupportedOnCPU shader:(BOOL &)outSupportedOnShader;

@end

@interface CocoaDisplayView : NSView<CocoaDisplayViewProtocol>
{
	InputManager *inputManager;
	MacDisplayViewOutput *_dvo;
	CALayer<DisplayViewCALayer> *localLayer;
	NSOpenGLContext *localOGLContext;
}

- (BOOL) handleKeyPress:(NSEvent *)theEvent keyPressed:(BOOL)keyPressed;
- (BOOL) handleMouseButton:(NSEvent *)theEvent buttonPressed:(BOOL)buttonPressed;

- (NSImage *) image;
- (void) handleCopyToPasteboard;

@end

class MacDisplayViewOutput : public ClientDisplayViewOutput
{
private:
	void __InstanceInit(MacGPUFetchObjectDisplayLink *fetchObj);
	
protected:
	MacGPUFetchObjectDisplayLink *_macFetchObj;
	
public:
	MacDisplayViewOutput();
	MacDisplayViewOutput(MacGPUFetchObjectDisplayLink *fetchObj);
	~MacDisplayViewOutput();
	
	virtual MacGPUFetchObjectDisplayLink* GetMacFetchObject();
	virtual void SetMacFetchObject(MacGPUFetchObjectDisplayLink *fetchObj);
	
	virtual void SetVideoProcessingPrefersGPU(bool theState);
	virtual void SetSourceDeposterize(bool theState);
	virtual void SetPixelScalerByID(VideoFilterTypeID filterID);
	
	virtual void HandleCopyToPasteboard();
};

#endif // _COCOA_DISPLAY_VIEW_H_
