/*
	Copyright (C) 2013-2026 DeSmuME team

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

#ifndef _COCOA_GRAPHICS_CONTROLLER_H_
#define _COCOA_GRAPHICS_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "utilities.h"
#include "ClientGraphicsControl.h"

#ifdef BOOL
#undef BOOL
#endif


class MacGraphicsControl : public ClientGraphicsControl
{
protected:
	virtual void _UpdateEngineClientProperties();
	
public:
	MacGraphicsControl(bool useMetalGraphics);
	virtual ~MacGraphicsControl();
	
	virtual void Set3DEngineByID(int engineID);
	virtual void SetEngine3DClientIndex(int clientListIndex);
	virtual int GetEngine3DClientIndex();
	
	virtual void DidFrameBegin(const size_t line, const bool isFrameSkipRequested, const size_t pageCount, u8 &selectedBufferIndexInOut);
	virtual void DidFrameEnd(bool isFrameSkipped, const NDSDisplayInfo &latestDisplayInfo);
	
	void ClearWithColor(const uint16_t colorBGRA5551);
	
	// GPUEventHandlerDefault methods
	virtual void DidApplyGPUSettingsBegin();
	virtual void DidApplyGPUSettingsEnd();
};

@interface CocoaGraphicsController : NSObjectController
{
	MacGraphicsControl *_graphicsControl;
	NSString *_engine3DClientName;
	NSString *_msaaSizeString;
	UInt32 gpuStateFlags;
	apple_unfairlock_t _unfairlockGpuState;
}

@property (assign) UInt32 gpuStateFlags;
@property (assign) Client2DGraphicsSettings gpu2DSettings;
@property (assign) NSUInteger gpuScale;
@property (assign) NSUInteger gpuColorFormat;

@property (assign) BOOL layerMainGPU;
@property (assign) BOOL layerMainBG0;
@property (assign) BOOL layerMainBG1;
@property (assign) BOOL layerMainBG2;
@property (assign) BOOL layerMainBG3;
@property (assign) BOOL layerMainOBJ;
@property (assign) BOOL layerSubGPU;
@property (assign) BOOL layerSubBG0;
@property (assign) BOOL layerSubBG1;
@property (assign) BOOL layerSubBG2;
@property (assign) BOOL layerSubBG3;
@property (assign) BOOL layerSubOBJ;

@property (assign) NSInteger engine3DClientIndex;
@property (readonly) NSInteger engine3DInternalIDApplied;
@property (readonly) NSInteger engine3DClientIDApplied;
@property (readonly, nonatomic, getter=engine3DClientNameApplied) NSString *_engine3DClientName;
@property (assign) BOOL highPrecisionColorInterpolationEnable;
@property (assign) BOOL edgeMarkEnable;
@property (assign) BOOL fogEnable;
@property (assign) BOOL textureEnable;
@property (assign) NSUInteger render3DThreads;
@property (assign) BOOL lineHackEnable;
@property (readonly) NSUInteger maxMultisampleSize;
@property (assign) NSUInteger multisampleSize;
@property (readonly, nonatomic, getter=multisampleSizeString) NSString *_msaaSizeString;
@property (assign) BOOL textureDeposterizeEnable;
@property (assign) BOOL textureSmoothingEnable;
@property (assign) NSUInteger textureScalingFactor;
@property (assign) BOOL fragmentSamplingHackEnable;
@property (assign) BOOL shadowPolygonEnable;
@property (assign) BOOL specialZeroAlphaBlendingEnable;
@property (assign) BOOL ndsDepthCalculationEnable;
@property (assign) BOOL depthLEqualPolygonFacingEnable;

@property (readonly, nonatomic) GPUClientFetchObject *fetchObject;

- (id) initWithGraphicsControl:(MacGraphicsControl *)graphicsControl;
- (BOOL) gpuStateByBit:(const UInt32)stateBit;
- (void) clearWithColor:(const uint16_t)colorBGRA5551;

@end

#endif // _COCOA_GRAPHICS_CONTROLLER_H_
