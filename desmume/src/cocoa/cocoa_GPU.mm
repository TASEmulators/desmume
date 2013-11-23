/*
	Copyright (C) 2013 DeSmuME team

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

#import "cocoa_GPU.h"
#import "cocoa_globals.h"
#include "utilities.h"

#include "../NDSSystem.h"
#include "../GPU.h"
#include "../rasterize.h"

#ifdef MAC_OS_X_VERSION_10_7
#include "../OGLRender_3_2.h"
#else
#include "../OGLRender.h"
#endif

#include <OpenGL/OpenGL.h>

#undef BOOL

GPU3DInterface *core3DList[] = {
	&gpu3DNull,
	&gpu3DRasterize,
	&gpu3Dgl,
	NULL
};

@implementation CocoaDSGPU

@dynamic gpuStateFlags;
@synthesize mutexProducer;

@dynamic layerMainGPU;
@dynamic layerMainBG0;
@dynamic layerMainBG1;
@dynamic layerMainBG2;
@dynamic layerMainBG3;
@dynamic layerMainOBJ;
@dynamic layerSubGPU;
@dynamic layerSubBG0;
@dynamic layerSubBG1;
@dynamic layerSubBG2;
@dynamic layerSubBG3;
@dynamic layerSubOBJ;

@dynamic render3DRenderingEngine;
@dynamic render3DHighPrecisionColorInterpolation;
@dynamic render3DEdgeMarking;
@dynamic render3DFog;
@dynamic render3DTextures;
@dynamic render3DDepthComparisonThreshold;
@dynamic render3DThreads;
@dynamic render3DLineHack;
@dynamic render3DMultisample;


- (id)init
{
	self = [super init];
	if (self == nil)
	{
		return self;
	}
	
	spinlockGpuState = OS_SPINLOCK_INIT;
	mutexProducer = NULL;
	
	gpuStateFlags	= GPUSTATE_MAIN_GPU_MASK |
					  GPUSTATE_MAIN_BG0_MASK |
					  GPUSTATE_MAIN_BG1_MASK |
					  GPUSTATE_MAIN_BG2_MASK |
					  GPUSTATE_MAIN_BG3_MASK |
					  GPUSTATE_MAIN_OBJ_MASK |
					  GPUSTATE_SUB_GPU_MASK |
					  GPUSTATE_SUB_BG0_MASK |
					  GPUSTATE_SUB_BG1_MASK |
					  GPUSTATE_SUB_BG2_MASK |
					  GPUSTATE_SUB_BG3_MASK |
					  GPUSTATE_SUB_OBJ_MASK;
	
	isCPUCoreCountAuto = NO;
	
	SetOpenGLRendererFunctions(&OSXOpenGLRendererInit,
							   &OSXOpenGLRendererBegin,
							   &OSXOpenGLRendererEnd);
	
	return self;
}

- (void)dealloc
{
	DestroyOpenGLRenderer();
	
	[super dealloc];
}

- (void) setGpuStateFlags:(UInt32)flags
{
	OSSpinLockLock(&spinlockGpuState);
	gpuStateFlags = flags;
	OSSpinLockUnlock(&spinlockGpuState);
	
	[self setLayerMainGPU:((flags & GPUSTATE_MAIN_GPU_MASK) != 0)];
	[self setLayerMainBG0:((flags & GPUSTATE_MAIN_BG0_MASK) != 0)];
	[self setLayerMainBG1:((flags & GPUSTATE_MAIN_BG1_MASK) != 0)];
	[self setLayerMainBG2:((flags & GPUSTATE_MAIN_BG2_MASK) != 0)];
	[self setLayerMainBG3:((flags & GPUSTATE_MAIN_BG3_MASK) != 0)];
	[self setLayerMainOBJ:((flags & GPUSTATE_MAIN_OBJ_MASK) != 0)];
	
	[self setLayerSubGPU:((flags & GPUSTATE_SUB_GPU_MASK) != 0)];
	[self setLayerSubBG0:((flags & GPUSTATE_SUB_BG0_MASK) != 0)];
	[self setLayerSubBG1:((flags & GPUSTATE_SUB_BG1_MASK) != 0)];
	[self setLayerSubBG2:((flags & GPUSTATE_SUB_BG2_MASK) != 0)];
	[self setLayerSubBG3:((flags & GPUSTATE_SUB_BG3_MASK) != 0)];
	[self setLayerSubOBJ:((flags & GPUSTATE_SUB_OBJ_MASK) != 0)];
}

- (UInt32) gpuStateFlags
{
	OSSpinLockLock(&spinlockGpuState);
	const UInt32 flags = gpuStateFlags;
	OSSpinLockUnlock(&spinlockGpuState);
	
	return flags;
}

- (void) setRender3DRenderingEngine:(NSInteger)methodID
{
	pthread_mutex_lock(self.mutexProducer);
	NDS_3D_ChangeCore(methodID);
	pthread_mutex_unlock(self.mutexProducer);
}

- (NSInteger) render3DRenderingEngine
{
	pthread_mutex_lock(self.mutexProducer);
	const NSInteger methodID = (NSInteger)cur3DCore;
	pthread_mutex_unlock(self.mutexProducer);
	
	return methodID;
}

- (void) setRender3DHighPrecisionColorInterpolation:(BOOL)state
{
	pthread_mutex_lock(self.mutexProducer);
	CommonSettings.GFX3D_HighResolutionInterpolateColor = state ? true : false;
	pthread_mutex_unlock(self.mutexProducer);
}

- (BOOL) render3DHighPrecisionColorInterpolation
{
	pthread_mutex_lock(self.mutexProducer);
	const BOOL state = CommonSettings.GFX3D_HighResolutionInterpolateColor ? YES : NO;
	pthread_mutex_unlock(self.mutexProducer);
	
	return state;
}

- (void) setRender3DEdgeMarking:(BOOL)state
{
	pthread_mutex_lock(self.mutexProducer);
	CommonSettings.GFX3D_EdgeMark = state ? true : false;
	pthread_mutex_unlock(self.mutexProducer);
}

- (BOOL) render3DEdgeMarking
{
	pthread_mutex_lock(self.mutexProducer);
	const BOOL state = CommonSettings.GFX3D_EdgeMark ? YES : NO;
	pthread_mutex_unlock(self.mutexProducer);
	
	return state;
}

- (void) setRender3DFog:(BOOL)state
{
	pthread_mutex_lock(self.mutexProducer);
	CommonSettings.GFX3D_Fog = state ? true : false;
	pthread_mutex_unlock(self.mutexProducer);
}

- (BOOL) render3DFog
{
	pthread_mutex_lock(self.mutexProducer);
	const BOOL state = CommonSettings.GFX3D_Fog ? YES : NO;
	pthread_mutex_unlock(self.mutexProducer);
	
	return state;
}

- (void) setRender3DTextures:(BOOL)state
{
	pthread_mutex_lock(self.mutexProducer);
	CommonSettings.GFX3D_Texture = state ? true : false;
	pthread_mutex_unlock(self.mutexProducer);
}

- (BOOL) render3DTextures
{
	pthread_mutex_lock(self.mutexProducer);
	const BOOL state = CommonSettings.GFX3D_Texture ? YES : NO;
	pthread_mutex_unlock(self.mutexProducer);
	
	return state;
}

- (void) setRender3DDepthComparisonThreshold:(NSUInteger)threshold
{
	pthread_mutex_lock(self.mutexProducer);
	CommonSettings.GFX3D_Zelda_Shadow_Depth_Hack = threshold;
	pthread_mutex_unlock(self.mutexProducer);
}

- (NSUInteger) render3DDepthComparisonThreshold
{
	pthread_mutex_lock(self.mutexProducer);
	const NSUInteger threshold = (NSUInteger)CommonSettings.GFX3D_Zelda_Shadow_Depth_Hack;
	pthread_mutex_unlock(self.mutexProducer);
	
	return threshold;
}

- (void) setRender3DThreads:(NSUInteger)numberThreads
{
	NSUInteger numberCores = [[NSProcessInfo processInfo] activeProcessorCount];
	if (numberThreads == 0)
	{
		isCPUCoreCountAuto = YES;
		
		if (numberCores >= 8)
		{
			numberCores = 8;
		}
		else if (numberCores >= 4)
		{
			numberCores = 4;
		}
		else if (numberCores >= 2)
		{
			numberCores = 2;
		}
		else
		{
			numberCores = 1;
		}
	}
	else
	{
		isCPUCoreCountAuto = NO;
		numberCores = numberThreads;
	}
	
	const NSInteger renderingEngineID = [self render3DRenderingEngine];
	
	pthread_mutex_lock(self.mutexProducer);
	
	CommonSettings.num_cores = numberCores;
	
	if (renderingEngineID == CORE3DLIST_SWRASTERIZE || renderingEngineID == CORE3DLIST_OPENGL)
	{
		NDS_3D_ChangeCore(renderingEngineID);
	}
	
	pthread_mutex_unlock(self.mutexProducer);
}

- (NSUInteger) render3DThreads
{
	pthread_mutex_lock(self.mutexProducer);
	const NSUInteger numberThreads = isCPUCoreCountAuto ? 0 : (NSUInteger)CommonSettings.num_cores;
	pthread_mutex_unlock(self.mutexProducer);
	
	return numberThreads;
}

- (void) setRender3DLineHack:(BOOL)state
{
	pthread_mutex_lock(self.mutexProducer);
	CommonSettings.GFX3D_LineHack = state ? true : false;
	pthread_mutex_unlock(self.mutexProducer);
}

- (BOOL) render3DLineHack
{
	pthread_mutex_lock(self.mutexProducer);
	const BOOL state = CommonSettings.GFX3D_LineHack ? YES : NO;
	pthread_mutex_unlock(self.mutexProducer);
	
	return state;
}

- (void) setRender3DMultisample:(BOOL)state
{
	pthread_mutex_lock(self.mutexProducer);
	CommonSettings.GFX3D_Renderer_Multisample = state ? true : false;
	pthread_mutex_unlock(self.mutexProducer);
}

- (BOOL) render3DMultisample
{
	pthread_mutex_lock(self.mutexProducer);
	const BOOL state = CommonSettings.GFX3D_Renderer_Multisample ? YES : NO;
	pthread_mutex_unlock(self.mutexProducer);
	
	return state;
}

- (void) setLayerMainGPU:(BOOL)gpuState
{
	pthread_mutex_lock(self.mutexProducer);
	SetGPUDisplayState(DS_GPU_TYPE_MAIN, (gpuState) ? true : false);
	pthread_mutex_unlock(self.mutexProducer);
	
	OSSpinLockLock(&spinlockGpuState);
	gpuStateFlags = (gpuState) ? (gpuStateFlags | GPUSTATE_MAIN_GPU_MASK) : (gpuStateFlags & ~GPUSTATE_MAIN_GPU_MASK);
	OSSpinLockUnlock(&spinlockGpuState);
}

- (BOOL) layerMainGPU
{
	pthread_mutex_lock(self.mutexProducer);
	const BOOL gpuState = GetGPUDisplayState(DS_GPU_TYPE_MAIN) ? YES : NO;
	pthread_mutex_unlock(self.mutexProducer);
	
	return gpuState;
}

- (void) setLayerMainBG0:(BOOL)layerState
{
	pthread_mutex_lock(self.mutexProducer);
	SetGPULayerState(DS_GPU_TYPE_MAIN, 0, (layerState) ? true : false);
	pthread_mutex_unlock(self.mutexProducer);
	
	OSSpinLockLock(&spinlockGpuState);
	gpuStateFlags = (layerState) ? (gpuStateFlags | GPUSTATE_MAIN_BG0_MASK) : (gpuStateFlags & ~GPUSTATE_MAIN_BG0_MASK);
	OSSpinLockUnlock(&spinlockGpuState);
}

- (BOOL) layerMainBG0
{
	pthread_mutex_lock(self.mutexProducer);
	const BOOL layerState = GetGPULayerState(DS_GPU_TYPE_MAIN, 0) ? YES : NO;
	pthread_mutex_unlock(self.mutexProducer);
	
	return layerState;
}

- (void) setLayerMainBG1:(BOOL)layerState
{
	pthread_mutex_lock(self.mutexProducer);
	SetGPULayerState(DS_GPU_TYPE_MAIN, 1, (layerState) ? true : false);
	pthread_mutex_unlock(self.mutexProducer);
	
	OSSpinLockLock(&spinlockGpuState);
	gpuStateFlags = (layerState) ? (gpuStateFlags | GPUSTATE_MAIN_BG1_MASK) : (gpuStateFlags & ~GPUSTATE_MAIN_BG1_MASK);
	OSSpinLockUnlock(&spinlockGpuState);
}

- (BOOL) layerMainBG1
{
	pthread_mutex_lock(self.mutexProducer);
	const BOOL layerState = GetGPULayerState(DS_GPU_TYPE_MAIN, 1) ? YES : NO;
	pthread_mutex_unlock(self.mutexProducer);
	
	return layerState;
}

- (void) setLayerMainBG2:(BOOL)layerState
{
	pthread_mutex_lock(self.mutexProducer);
	SetGPULayerState(DS_GPU_TYPE_MAIN, 2, (layerState) ? true : false);
	pthread_mutex_unlock(self.mutexProducer);
	
	OSSpinLockLock(&spinlockGpuState);
	gpuStateFlags = (layerState) ? (gpuStateFlags | GPUSTATE_MAIN_BG2_MASK) : (gpuStateFlags & ~GPUSTATE_MAIN_BG2_MASK);
	OSSpinLockUnlock(&spinlockGpuState);
}

- (BOOL) layerMainBG2
{
	pthread_mutex_lock(self.mutexProducer);
	const BOOL layerState = GetGPULayerState(DS_GPU_TYPE_MAIN, 2) ? YES : NO;
	pthread_mutex_unlock(self.mutexProducer);
	
	return layerState;
}

- (void) setLayerMainBG3:(BOOL)layerState
{
	pthread_mutex_lock(self.mutexProducer);
	SetGPULayerState(DS_GPU_TYPE_MAIN, 3, (layerState) ? true : false);
	pthread_mutex_unlock(self.mutexProducer);
	
	OSSpinLockLock(&spinlockGpuState);
	gpuStateFlags = (layerState) ? (gpuStateFlags | GPUSTATE_MAIN_BG3_MASK) : (gpuStateFlags & ~GPUSTATE_MAIN_BG3_MASK);
	OSSpinLockUnlock(&spinlockGpuState);
}

- (BOOL) layerMainBG3
{
	pthread_mutex_lock(self.mutexProducer);
	const BOOL layerState = GetGPULayerState(DS_GPU_TYPE_MAIN, 3) ? YES : NO;
	pthread_mutex_unlock(self.mutexProducer);
	
	return layerState;
}

- (void) setLayerMainOBJ:(BOOL)layerState
{
	pthread_mutex_lock(self.mutexProducer);
	SetGPULayerState(DS_GPU_TYPE_MAIN, 4, (layerState) ? true : false);
	pthread_mutex_unlock(self.mutexProducer);
	
	OSSpinLockLock(&spinlockGpuState);
	gpuStateFlags = (layerState) ? (gpuStateFlags | GPUSTATE_MAIN_OBJ_MASK) : (gpuStateFlags & ~GPUSTATE_MAIN_OBJ_MASK);
	OSSpinLockUnlock(&spinlockGpuState);
}

- (BOOL) layerMainOBJ
{
	pthread_mutex_lock(self.mutexProducer);
	const BOOL layerState = GetGPULayerState(DS_GPU_TYPE_MAIN, 4) ? YES : NO;
	pthread_mutex_unlock(self.mutexProducer);
	
	return layerState;
}

- (void) setLayerSubGPU:(BOOL)gpuState
{
	pthread_mutex_lock(self.mutexProducer);
	SetGPUDisplayState(DS_GPU_TYPE_SUB, (gpuState) ? true : false);
	pthread_mutex_unlock(self.mutexProducer);
	
	OSSpinLockLock(&spinlockGpuState);
	gpuStateFlags = (gpuState) ? (gpuStateFlags | GPUSTATE_SUB_GPU_MASK) : (gpuStateFlags & ~GPUSTATE_SUB_GPU_MASK);
	OSSpinLockUnlock(&spinlockGpuState);
}

- (BOOL) layerSubGPU
{
	pthread_mutex_lock(self.mutexProducer);
	const BOOL gpuState = GetGPUDisplayState(DS_GPU_TYPE_SUB) ? YES : NO;
	pthread_mutex_unlock(self.mutexProducer);
	
	return gpuState;
}

- (void) setLayerSubBG0:(BOOL)layerState
{
	pthread_mutex_lock(self.mutexProducer);
	SetGPULayerState(DS_GPU_TYPE_SUB, 0, (layerState) ? true : false);
	pthread_mutex_unlock(self.mutexProducer);
	
	OSSpinLockLock(&spinlockGpuState);
	gpuStateFlags = (layerState) ? (gpuStateFlags | GPUSTATE_SUB_BG0_MASK) : (gpuStateFlags & ~GPUSTATE_SUB_BG0_MASK);
	OSSpinLockUnlock(&spinlockGpuState);
}

- (BOOL) layerSubBG0
{
	pthread_mutex_lock(self.mutexProducer);
	const BOOL layerState = GetGPULayerState(DS_GPU_TYPE_SUB, 0) ? YES : NO;
	pthread_mutex_unlock(self.mutexProducer);
	
	return layerState;
}

- (void) setLayerSubBG1:(BOOL)layerState
{
	pthread_mutex_lock(self.mutexProducer);
	SetGPULayerState(DS_GPU_TYPE_SUB, 1, (layerState) ? true : false);
	pthread_mutex_unlock(self.mutexProducer);
	
	OSSpinLockLock(&spinlockGpuState);
	gpuStateFlags = (layerState) ? (gpuStateFlags | GPUSTATE_SUB_BG1_MASK) : (gpuStateFlags & ~GPUSTATE_SUB_BG1_MASK);
	OSSpinLockUnlock(&spinlockGpuState);
}

- (BOOL) layerSubBG1
{
	pthread_mutex_lock(self.mutexProducer);
	const BOOL layerState = GetGPULayerState(DS_GPU_TYPE_SUB, 1) ? YES : NO;
	pthread_mutex_unlock(self.mutexProducer);
	
	return layerState;
}

- (void) setLayerSubBG2:(BOOL)layerState
{
	pthread_mutex_lock(self.mutexProducer);
	SetGPULayerState(DS_GPU_TYPE_SUB, 2, (layerState) ? true : false);
	pthread_mutex_unlock(self.mutexProducer);
	
	OSSpinLockLock(&spinlockGpuState);
	gpuStateFlags = (layerState) ? (gpuStateFlags | GPUSTATE_SUB_BG2_MASK) : (gpuStateFlags & ~GPUSTATE_SUB_BG2_MASK);
	OSSpinLockUnlock(&spinlockGpuState);
}

- (BOOL) layerSubBG2
{
	pthread_mutex_lock(self.mutexProducer);
	const BOOL layerState = GetGPULayerState(DS_GPU_TYPE_SUB, 2) ? YES : NO;
	pthread_mutex_unlock(self.mutexProducer);
	
	return layerState;
}

- (void) setLayerSubBG3:(BOOL)layerState
{
	pthread_mutex_lock(self.mutexProducer);
	SetGPULayerState(DS_GPU_TYPE_SUB, 3, (layerState) ? true : false);
	pthread_mutex_unlock(self.mutexProducer);
	
	OSSpinLockLock(&spinlockGpuState);
	gpuStateFlags = (layerState) ? (gpuStateFlags | GPUSTATE_SUB_BG3_MASK) : (gpuStateFlags & ~GPUSTATE_SUB_BG3_MASK);
	OSSpinLockUnlock(&spinlockGpuState);
}

- (BOOL) layerSubBG3
{
	pthread_mutex_lock(self.mutexProducer);
	const BOOL layerState = GetGPULayerState(DS_GPU_TYPE_SUB, 3) ? YES : NO;
	pthread_mutex_unlock(self.mutexProducer);
	
	return layerState;
}

- (void) setLayerSubOBJ:(BOOL)layerState
{
	pthread_mutex_lock(self.mutexProducer);
	SetGPULayerState(DS_GPU_TYPE_SUB, 4, (layerState) ? true : false);
	pthread_mutex_unlock(self.mutexProducer);
	
	OSSpinLockLock(&spinlockGpuState);
	gpuStateFlags = (layerState) ? (gpuStateFlags | GPUSTATE_SUB_OBJ_MASK) : (gpuStateFlags & ~GPUSTATE_SUB_OBJ_MASK);
	OSSpinLockUnlock(&spinlockGpuState);
}

- (BOOL) layerSubOBJ
{
	pthread_mutex_lock(self.mutexProducer);
	const BOOL layerState = GetGPULayerState(DS_GPU_TYPE_SUB, 4) ? YES : NO;
	pthread_mutex_unlock(self.mutexProducer);
	
	return layerState;
}

- (BOOL) gpuStateByBit:(const UInt32)stateBit
{
	return ([self gpuStateFlags] & (1 << stateBit)) ? YES : NO;
}

- (BOOL) isGPUTypeDisplayed:(const NSInteger)theGpuType
{
	BOOL result = NO;
	const UInt32 flags = [self gpuStateFlags];
	
	switch (theGpuType)
	{
		case DS_GPU_TYPE_MAIN:
			if (flags & GPUSTATE_MAIN_GPU_MASK)
			{
				result = YES;
			}
			break;
			
		case DS_GPU_TYPE_SUB:
			if (flags & GPUSTATE_SUB_GPU_MASK)
			{
				result = YES;
			}
			break;
			
		case DS_GPU_TYPE_MAIN_AND_SUB:
			if (flags & (GPUSTATE_MAIN_GPU_MASK | GPUSTATE_SUB_GPU_MASK))
			{
				result = YES;
			}
			break;
			
		default:
			break;
	}
	
	return result;
}

- (void) hideGPUType:(const NSInteger)theGpuType
{
	UInt32 flags = [self gpuStateFlags];
	
	switch (theGpuType)
	{
		case DS_GPU_TYPE_MAIN:
			flags &= ~GPUSTATE_MAIN_GPU_MASK;
			break;
			
		case DS_GPU_TYPE_SUB:
			flags &= ~GPUSTATE_SUB_GPU_MASK;
			break;
			
		case DS_GPU_TYPE_MAIN_AND_SUB:
			flags &= ~GPUSTATE_MAIN_GPU_MASK;
			flags &= ~GPUSTATE_SUB_GPU_MASK;
			break;
			
		default:
			break;
	}
	
	[self setGpuStateFlags:flags];
}

- (void) showGPUType:(const NSInteger)theGpuType
{
	UInt32 flags = [self gpuStateFlags];
	
	switch (theGpuType)
	{
		case DS_GPU_TYPE_MAIN:
			flags |= GPUSTATE_MAIN_GPU_MASK;
			break;
			
		case DS_GPU_TYPE_SUB:
			flags |= GPUSTATE_SUB_GPU_MASK;
			break;
			
		case DS_GPU_TYPE_MAIN_AND_SUB:
			flags |= GPUSTATE_MAIN_GPU_MASK;
			flags |= GPUSTATE_SUB_GPU_MASK;
			break;
			
		default:
			break;
	}
	
	[self setGpuStateFlags:flags];
}

- (NSString *) render3DRenderingEngineString
{
	NSString *theString = @"Uninitialized";
	
	pthread_mutex_lock(self.mutexProducer);
	
	if(gpu3D == NULL)
	{
		pthread_mutex_unlock(self.mutexProducer);
		return theString;
	}
	
	const char *theName = gpu3D->name;
	theString = [NSString stringWithCString:theName encoding:NSUTF8StringEncoding];
	
	pthread_mutex_unlock(self.mutexProducer);
	
	return theString;
}

@end

void SetGPULayerState(const int gpuType, const unsigned int i, const bool state)
{
	GPU *theGpu = NULL;
	
	// Check bounds on the layer index.
	if(i > 4)
	{
		return;
	}
	
	switch (gpuType)
	{
		case DS_GPU_TYPE_SUB:
			theGpu = SubScreen.gpu;
			break;
			
		case DS_GPU_TYPE_MAIN:
			theGpu = MainScreen.gpu;
			break;
			
		case DS_GPU_TYPE_MAIN_AND_SUB:
			SetGPULayerState(DS_GPU_TYPE_SUB, i, state); // Recursive call
			theGpu = MainScreen.gpu;
			break;
			
		default:
			break;
	}
	
	if (theGpu != NULL)
	{
		if (state)
		{
			GPU_addBack(theGpu, i);
		}
		else
		{
			GPU_remove(theGpu, i);
		}
	}
}

bool GetGPULayerState(const int gpuType, const unsigned int i)
{
	bool theState = false;
	
	// Check bounds on the layer index.
	if(i > 4)
	{
		return theState;
	}
	
	switch (gpuType)
	{
		case DS_GPU_TYPE_SUB:
			if (SubScreen.gpu != NULL)
			{
				theState = CommonSettings.dispLayers[SubScreen.gpu->core][i];
			}
			break;
			
		case DS_GPU_TYPE_MAIN:
			if (MainScreen.gpu != NULL)
			{
				theState = CommonSettings.dispLayers[MainScreen.gpu->core][i];
			}
			break;
			
		case DS_GPU_TYPE_MAIN_AND_SUB:
			if (SubScreen.gpu != NULL && MainScreen.gpu != NULL)
			{
				theState = (CommonSettings.dispLayers[SubScreen.gpu->core][i] && CommonSettings.dispLayers[MainScreen.gpu->core][i]);
			}
			break;
			
		default:
			break;
	}
	
	return theState;
}

void SetGPUDisplayState(const int gpuType, const bool state)
{
	switch (gpuType)
	{
		case DS_GPU_TYPE_SUB:
			CommonSettings.showGpu.sub = state;
			break;
			
		case DS_GPU_TYPE_MAIN:
			CommonSettings.showGpu.main = state;
			break;
			
		case DS_GPU_TYPE_MAIN_AND_SUB:
			CommonSettings.showGpu.sub = state;
			CommonSettings.showGpu.main = state;
			break;
			
		default:
			break;
	}
}

bool GetGPUDisplayState(const int gpuType)
{
	bool theState = false;
	
	switch (gpuType)
	{
		case DS_GPU_TYPE_SUB:
			theState = CommonSettings.showGpu.sub;
			break;
			
		case DS_GPU_TYPE_MAIN:
			theState = CommonSettings.showGpu.main;
			break;
			
		case DS_GPU_TYPE_MAIN_AND_SUB:
			theState = (CommonSettings.showGpu.sub && CommonSettings.showGpu.main);
			break;
			
		default:
			break;
	}
	
	return theState;
}

CGLContextObj OSXOpenGLRendererContext = NULL;
CGLPBufferObj OSXOpenGLRendererPBuffer = NULL;

bool OSXOpenGLRendererInit()
{
	static bool isContextAlreadyCreated = false;
	
	if (!isContextAlreadyCreated)
	{
		isContextAlreadyCreated = CreateOpenGLRenderer();
	}
	
	return true;
}

bool OSXOpenGLRendererBegin()
{
	CGLSetCurrentContext(OSXOpenGLRendererContext);
	
	return true;
}

void OSXOpenGLRendererEnd()
{
	
}

bool CreateOpenGLRenderer()
{
	bool result = false;
	bool useContext_3_2 = false;
	CGLPixelFormatObj cglPixFormat = NULL;
	CGLContextObj newContext = NULL;
	CGLPBufferObj newPBuffer = NULL;
	GLint virtualScreenCount = 0;
	
	CGLPixelFormatAttribute attrs[] = {
		kCGLPFAColorSize, (CGLPixelFormatAttribute)24,
		kCGLPFAAlphaSize, (CGLPixelFormatAttribute)8,
		kCGLPFADepthSize, (CGLPixelFormatAttribute)24,
		kCGLPFAStencilSize, (CGLPixelFormatAttribute)8,
		kCGLPFAAccelerated,
		(CGLPixelFormatAttribute)0, (CGLPixelFormatAttribute)0,
		(CGLPixelFormatAttribute)0
	};
	
#ifdef MAC_OS_X_VERSION_10_7
	// If we can support a 3.2 Core Profile context, then request that in our
	// pixel format attributes.
	useContext_3_2 = IsOSXVersionSupported(10, 7, 0);
	if (useContext_3_2)
	{
		attrs[9] = kCGLPFAOpenGLProfile;
		attrs[10] = (CGLPixelFormatAttribute)kCGLOGLPVersion_3_2_Core;
	}
#endif
	
	CGLChoosePixelFormat(attrs, &cglPixFormat, &virtualScreenCount);
	if (cglPixFormat == NULL)
	{
		// Remove the HW rendering requirement and try again. Note that this will
		// result in SW rendering, which will cause a substantial speed hit.
		attrs[8] = (CGLPixelFormatAttribute)0;
		CGLChoosePixelFormat(attrs, &cglPixFormat, &virtualScreenCount);
		if (cglPixFormat == NULL)
		{
			return result;
		}
	}
	
	CGLCreateContext(cglPixFormat, NULL, &newContext);
	CGLReleasePixelFormat(cglPixFormat);
	
	// Create a PBuffer for legacy contexts since the availability of FBOs
	// is not guaranteed.
	if (!useContext_3_2)
	{
		CGLCreatePBuffer(GPU_DISPLAY_WIDTH, GPU_DISPLAY_HEIGHT, GL_TEXTURE_2D, GL_RGBA, 0, &newPBuffer);
		
		if (newPBuffer == NULL)
		{
			CGLReleaseContext(newContext);
			return result;
		}
		else
		{
			GLint virtualScreenID = 0;
			
			CGLGetVirtualScreen(newContext, &virtualScreenID);
			CGLSetPBuffer(newContext, newPBuffer, 0, 0, virtualScreenID);
		}
	}
	
	RequestOpenGLRenderer_3_2(useContext_3_2);
	OSXOpenGLRendererContext = newContext;
	OSXOpenGLRendererPBuffer = newPBuffer;
	
	result = true;
	return result;
}

void DestroyOpenGLRenderer()
{
	if (OSXOpenGLRendererContext == NULL)
	{
		return;
	}
	
	CGLReleasePBuffer(OSXOpenGLRendererPBuffer);
	CGLReleaseContext(OSXOpenGLRendererContext);
	OSXOpenGLRendererContext = NULL;
	OSXOpenGLRendererPBuffer = NULL;
}

void RequestOpenGLRenderer_3_2(bool request_3_2)
{
#ifdef OGLRENDER_3_2_H
	if (request_3_2)
	{
		OGLLoadEntryPoints_3_2_Func = &OGLLoadEntryPoints_3_2;
		OGLCreateRenderer_3_2_Func = &OGLCreateRenderer_3_2;
		return;
	}
#endif
	OGLLoadEntryPoints_3_2_Func = NULL;
	OGLCreateRenderer_3_2_Func = NULL;
}

void SetOpenGLRendererFunctions(bool (*initFunction)(),
								bool (*beginOGLFunction)(),
								void (*endOGLFunction)())
{
	oglrender_init = initFunction;
	oglrender_beginOpenGL = beginOGLFunction;
	oglrender_endOpenGL = endOGLFunction;
}
