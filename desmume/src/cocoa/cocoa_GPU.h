/*
	Copyright (C) 2013-2015 DeSmuME team

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

#import <Foundation/Foundation.h>
#include <pthread.h>
#include <libkern/OSAtomic.h>


class GPUEventHandlerOSX;

@interface CocoaDSGPU : NSObject
{
	UInt32 gpuStateFlags;
	uint8_t _gpuScale;
	BOOL isCPUCoreCountAuto;
	BOOL _needRestoreRender3DLock;
	
	OSSpinLock spinlockGpuState;
	GPUEventHandlerOSX *gpuEvent;
}

@property (assign) UInt32 gpuStateFlags;
@property (assign) NSSize gpuDimensions;
@property (assign) NSUInteger gpuScale;
@property (readonly) pthread_rwlock_t *gpuFrameRWLock;

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

@property (assign) NSInteger render3DRenderingEngine;
@property (assign) BOOL render3DHighPrecisionColorInterpolation;
@property (assign) BOOL render3DEdgeMarking;
@property (assign) BOOL render3DFog;
@property (assign) BOOL render3DTextures;
@property (assign) NSUInteger render3DDepthComparisonThreshold;
@property (assign) NSUInteger render3DThreads;
@property (assign) BOOL render3DLineHack;
@property (assign) BOOL render3DMultisample;
@property (assign) BOOL render3DFragmentSamplingHack;

- (void) setOutputList:(NSMutableArray *)theOutputList mutexPtr:(pthread_mutex_t *)theMutex;
- (BOOL) gpuStateByBit:(const UInt32)stateBit;
- (NSString *) render3DRenderingEngineString;
- (void) clearWithColor:(const uint16_t)colorBGRA5551;
- (void) respondToPauseState:(BOOL)isPaused;

@end

#ifdef __cplusplus
extern "C"
{
#endif

bool OSXOpenGLRendererInit();
bool OSXOpenGLRendererBegin();
void OSXOpenGLRendererEnd();
bool OSXOpenGLRendererFramebufferDidResize(size_t w, size_t h);

bool CreateOpenGLRenderer();
void DestroyOpenGLRenderer();
void RequestOpenGLRenderer_3_2(bool request_3_2);
void SetOpenGLRendererFunctions(bool (*initFunction)(),
								bool (*beginOGLFunction)(),
								void (*endOGLFunction)(),
								bool (*resizeOGLFunction)(size_t w, size_t h));

#ifdef __cplusplus
}
#endif
