/*
	Copyright (C) 2013-2025 DeSmuME team

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

#include "../../NDSSystem.h"
#include "../../rasterize.h"

#ifdef MAC_OS_X_VERSION_10_7
	#include "../../OGLRender_3_2.h"
#else
	#include "../../OGLRender.h"
#endif

#include "cgl_3Demu.h"

#ifdef PORT_VERSION_OS_X_APP
	#import "userinterface/CocoaDisplayView.h"
	#import "userinterface/MacOGLDisplayView.h"

	#ifdef ENABLE_APPLE_METAL
		#import "userinterface/MacMetalDisplayView.h"
	#endif
#else
	#import "openemu/OEDisplayView.h"
#endif

#ifdef BOOL
#undef BOOL
#endif

#define GPU_3D_RENDERER_COUNT 3
GPU3DInterface *core3DList[GPU_3D_RENDERER_COUNT+1] = {
	&gpu3DNull,
	&gpu3DRasterize,
	&gpu3Dgl,
	NULL
};

@implementation CocoaDSGPU

@dynamic gpuStateFlags;
@dynamic gpuDimensions;
@dynamic gpuScale;
@dynamic gpuColorFormat;

@synthesize openglDeviceMaxMultisamples;

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
@dynamic render3DRenderingEngineApplied;
@dynamic render3DRenderingEngineAppliedHostRendererID;
@dynamic render3DRenderingEngineAppliedHostRendererName;
@dynamic render3DHighPrecisionColorInterpolation;
@dynamic render3DEdgeMarking;
@dynamic render3DFog;
@dynamic render3DTextures;
@dynamic render3DThreads;
@dynamic render3DLineHack;
@dynamic render3DMultisampleSize;
@synthesize render3DMultisampleSizeString;
@dynamic render3DTextureDeposterize;
@dynamic render3DTextureSmoothing;
@dynamic render3DTextureScalingFactor;
@dynamic render3DFragmentSamplingHack;
@dynamic openGLEmulateShadowPolygon;
@dynamic openGLEmulateSpecialZeroAlphaBlending;
@dynamic openGLEmulateNDSDepthCalculation;
@dynamic openGLEmulateDepthLEqualPolygonFacing;
@synthesize fetchObject;

- (id)init
{
	self = [super init];
	if (self == nil)
	{
		return self;
	}
	
	_unfairlockGpuState = apple_unfairlock_create();
	
	_gpuScale = 1;
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
	_render3DThreadsRequested = 0;
	_render3DThreadCount = 0;
	
	oglrender_init        = &cgl_initOpenGL_StandardAuto;
	oglrender_deinit      = &cgl_deinitOpenGL;
	oglrender_beginOpenGL = &cgl_beginOpenGL;
	oglrender_endOpenGL   = &cgl_endOpenGL;
	oglrender_framebufferDidResizeCallback = &cgl_framebufferDidResizeCallback;
	
#ifdef OGLRENDER_3_2_H
	OGLLoadEntryPoints_3_2_Func = &OGLLoadEntryPoints_3_2;
	OGLCreateRenderer_3_2_Func = &OGLCreateRenderer_3_2;
#endif
	
	gpuEvent = new MacGPUEventHandlerAsync;
	GPU->SetEventHandler(gpuEvent);
	
	fetchObject = NULL;
	
#ifdef ENABLE_APPLE_METAL
	if (IsOSXVersionSupported(10, 11, 0) && ![[NSUserDefaults standardUserDefaults] boolForKey:@"Debug_DisableMetal"])
	{
		fetchObject = new MacMetalFetchObject;
		
		if (fetchObject->GetClientData() == nil)
		{
			delete fetchObject;
			fetchObject = NULL;
		}
		else
		{
			gpuEvent->SetFramebufferPageCount(METAL_FETCH_BUFFER_COUNT);
			GPU->SetWillPostprocessDisplays(false);
		}
	}
#endif
	
	if (fetchObject == NULL)
	{
#ifdef PORT_VERSION_OS_X_APP
		fetchObject = new MacOGLClientFetchObject;
		GPU->SetWillPostprocessDisplays(false);
#else
		fetchObject = new OE_OGLClientFetchObject;
#endif
		gpuEvent->SetFramebufferPageCount(OPENGL_FETCH_BUFFER_COUNT);
	}
	
	fetchObject->Init();
	gpuEvent->SetFetchObject(fetchObject);
	
	GPU->SetWillAutoResolveToCustomBuffer(false);
	
	openglDeviceMaxMultisamples = 0;
	render3DMultisampleSizeString = @"Off";
	
	bool isTempContextCreated = cgl_initOpenGL_StandardAuto();
	if (isTempContextCreated)
	{
		cgl_beginOpenGL();
		
		GLint maxSamplesOGL = 0;
		
#if defined(GL_MAX_SAMPLES)
		glGetIntegerv(GL_MAX_SAMPLES, &maxSamplesOGL);
#elif defined(GL_MAX_SAMPLES_EXT)
		glGetIntegerv(GL_MAX_SAMPLES_EXT, &maxSamplesOGL);
#endif
		
		openglDeviceMaxMultisamples = maxSamplesOGL;
		
		cgl_endOpenGL();
		cgl_deinitOpenGL();
	}
	
	return self;
}

- (void)dealloc
{
	GPU->SetEventHandler(NULL); // Unassigned our event handler before we delete it.
	
	delete fetchObject;
	delete gpuEvent;
	
	[self setRender3DMultisampleSizeString:nil];
	
	apple_unfairlock_destroy(_unfairlockGpuState);
	
	[super dealloc];
}

- (void) setGpuStateFlags:(UInt32)flags
{
	apple_unfairlock_lock(_unfairlockGpuState);
	gpuStateFlags = flags;
	apple_unfairlock_unlock(_unfairlockGpuState);
	
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
	apple_unfairlock_lock(_unfairlockGpuState);
	const UInt32 flags = gpuStateFlags;
	apple_unfairlock_unlock(_unfairlockGpuState);
	
	return flags;
}

- (void) setGpuDimensions:(NSSize)theDimensions
{
	const size_t w = (size_t)(theDimensions.width + 0.01);
	const size_t h = (size_t)(theDimensions.height + 0.01);
	
	gpuEvent->SetFramebufferDimensions(w, h);
}

- (NSSize) gpuDimensions
{
	size_t w;
	size_t h;
	gpuEvent->GetFramebufferDimensions(w, h);
	
	return NSMakeSize((CGFloat)w, (CGFloat)h);
}

- (void) setGpuScale:(NSUInteger)theScale
{
	_gpuScale = (uint8_t)theScale;
	[self setGpuDimensions:NSMakeSize(GPU_FRAMEBUFFER_NATIVE_WIDTH * theScale, GPU_FRAMEBUFFER_NATIVE_HEIGHT * theScale)];
}

- (NSUInteger) gpuScale
{
	return (NSUInteger)_gpuScale;
}

- (void) setGpuColorFormat:(NSUInteger)colorFormat
{
	// First check for a valid color format. Abort if the color format is invalid.
	switch ((NDSColorFormat)colorFormat)
	{
		case NDSColorFormat_BGR555_Rev:
		case NDSColorFormat_BGR666_Rev:
		case NDSColorFormat_BGR888_Rev:
			break;
			
		default:
			return;
	}
	
	gpuEvent->SetColorFormat((NDSColorFormat)colorFormat);
}

- (NSUInteger) gpuColorFormat
{
	return (NSUInteger)gpuEvent->GetColorFormat();
}

- (void) setRender3DRenderingEngine:(NSInteger)rendererID
{
	if (rendererID < CORE3DLIST_NULL)
	{
		rendererID = CORE3DLIST_NULL;
	}
	else if (rendererID >= GPU_3D_RENDERER_COUNT)
	{
		puts("DeSmuME: Invalid 3D renderer chosen; falling back to SoftRasterizer.");
		rendererID = CORE3DLIST_SWRASTERIZE;
	}
	else if (rendererID == CORE3DLIST_OPENGL)
	{
		oglrender_init = &cgl_initOpenGL_StandardAuto;
	}
	
	gpuEvent->ApplyRender3DSettingsLock();
	GPU->Set3DRendererByID((int)rendererID);
	
	if (rendererID == CORE3DLIST_SWRASTERIZE)
	{
		gpuEvent->SetTempThreadCount(_render3DThreadCount);
		GPU->Set3DRendererByID(CORE3DLIST_SWRASTERIZE);
	}
	
	gpuEvent->ApplyRender3DSettingsUnlock();
}

- (NSInteger) render3DRenderingEngine
{
	gpuEvent->ApplyRender3DSettingsLock();
	const NSInteger rendererID = (NSInteger)GPU->Get3DRendererID();
	gpuEvent->ApplyRender3DSettingsUnlock();
	
	return rendererID;
}

- (NSInteger) render3DRenderingEngineApplied
{
	gpuEvent->ApplyRender3DSettingsLock();
	if ( (gpu3D == NULL) || (CurrentRenderer == NULL) )
	{
		gpuEvent->ApplyRender3DSettingsUnlock();
		return 0;
	}
	
	const NSInteger rendererID = (NSInteger)CurrentRenderer->GetRenderID();
	gpuEvent->ApplyRender3DSettingsUnlock();
	
	return rendererID;
}

- (NSInteger) render3DRenderingEngineAppliedHostRendererID
{
	NSInteger hostID = 0;
	
	gpuEvent->ApplyRender3DSettingsLock();
	
	if ( (gpu3D == NULL) || (CurrentRenderer == NULL) )
	{
		gpuEvent->ApplyRender3DSettingsUnlock();
		return hostID;
	}
	
	switch (CurrentRenderer->GetRenderID())
	{
		case RENDERID_OPENGL_AUTO:
		case RENDERID_OPENGL_LEGACY:
		case RENDERID_OPENGL_3_2:
			hostID = (NSInteger)cgl_getHostRendererID();
			break;
			
		case RENDERID_NULL:
		case RENDERID_SOFTRASTERIZER:
		default:
			break;
	}
	
	gpuEvent->ApplyRender3DSettingsUnlock();
	
	return hostID;
}

- (NSString *) render3DRenderingEngineAppliedHostRendererName
{
	NSString *theString = @"Uninitialized";
	
	gpuEvent->ApplyRender3DSettingsLock();
	
	if ( (gpu3D == NULL) || (CurrentRenderer == NULL) )
	{
		gpuEvent->ApplyRender3DSettingsUnlock();
		return theString;
	}
	
	std::string theName;
	
	switch (CurrentRenderer->GetRenderID())
	{
		case RENDERID_OPENGL_AUTO:
		case RENDERID_OPENGL_LEGACY:
		case RENDERID_OPENGL_3_2:
			theName = cgl_getHostRendererString();
			break;
			
		case RENDERID_NULL:
		case RENDERID_SOFTRASTERIZER:
		default:
			theName = CurrentRenderer->GetName();
			break;
	}
	
	theString = [NSString stringWithCString:theName.c_str() encoding:NSUTF8StringEncoding];
	
	gpuEvent->ApplyRender3DSettingsUnlock();
	
	return theString;
}

- (void) setRender3DHighPrecisionColorInterpolation:(BOOL)state
{
	gpuEvent->ApplyRender3DSettingsLock();
	CommonSettings.GFX3D_HighResolutionInterpolateColor = state ? true : false;
	gpuEvent->ApplyRender3DSettingsUnlock();
}

- (BOOL) render3DHighPrecisionColorInterpolation
{
	gpuEvent->ApplyRender3DSettingsLock();
	const BOOL state = CommonSettings.GFX3D_HighResolutionInterpolateColor ? YES : NO;
	gpuEvent->ApplyRender3DSettingsUnlock();
	
	return state;
}

- (void) setRender3DEdgeMarking:(BOOL)state
{
	gpuEvent->ApplyRender3DSettingsLock();
	CommonSettings.GFX3D_EdgeMark = state ? true : false;
	gpuEvent->ApplyRender3DSettingsUnlock();
}

- (BOOL) render3DEdgeMarking
{
	gpuEvent->ApplyRender3DSettingsLock();
	const BOOL state = CommonSettings.GFX3D_EdgeMark ? YES : NO;
	gpuEvent->ApplyRender3DSettingsUnlock();
	
	return state;
}

- (void) setRender3DFog:(BOOL)state
{
	gpuEvent->ApplyRender3DSettingsLock();
	CommonSettings.GFX3D_Fog = state ? true : false;
	gpuEvent->ApplyRender3DSettingsUnlock();
}

- (BOOL) render3DFog
{
	gpuEvent->ApplyRender3DSettingsLock();
	const BOOL state = CommonSettings.GFX3D_Fog ? YES : NO;
	gpuEvent->ApplyRender3DSettingsUnlock();
	
	return state;
}

- (void) setRender3DTextures:(BOOL)state
{
	gpuEvent->ApplyRender3DSettingsLock();
	CommonSettings.GFX3D_Texture = state ? true : false;
	gpuEvent->ApplyRender3DSettingsUnlock();
}

- (BOOL) render3DTextures
{
	gpuEvent->ApplyRender3DSettingsLock();
	const BOOL state = CommonSettings.GFX3D_Texture ? YES : NO;
	gpuEvent->ApplyRender3DSettingsUnlock();
	
	return state;
}

- (void) setRender3DThreads:(NSUInteger)numberThreads
{
	_render3DThreadsRequested = (int)numberThreads;
	
	const int numberCores = CommonSettings.num_cores;
	int newThreadCount = numberCores;
	
	if (numberThreads == 0)
	{
		isCPUCoreCountAuto = YES;
		if (numberCores < 2)
		{
			newThreadCount = 1;
		}
		else
		{
			const int reserveCoreCount = numberCores / 12; // For every 12 cores, reserve 1 core for the rest of the system.
			newThreadCount -= reserveCoreCount;
		}
	}
	else
	{
		isCPUCoreCountAuto = NO;
		newThreadCount = (int)numberThreads;
	}
	
	const RendererID renderingEngineID = (RendererID)[self render3DRenderingEngine];
	_render3DThreadCount = newThreadCount;
	
	gpuEvent->ApplyRender3DSettingsLock();
	
	if (renderingEngineID == RENDERID_SOFTRASTERIZER)
	{
		gpuEvent->SetTempThreadCount(newThreadCount);
		GPU->Set3DRendererByID(renderingEngineID);
	}
	
	gpuEvent->ApplyRender3DSettingsUnlock();
}

- (NSUInteger) render3DThreads
{
	return (isCPUCoreCountAuto) ? 0 : (NSUInteger)_render3DThreadsRequested;
}

- (void) setRender3DLineHack:(BOOL)state
{
	gpuEvent->ApplyRender3DSettingsLock();
	CommonSettings.GFX3D_LineHack = state ? true : false;
	gpuEvent->ApplyRender3DSettingsUnlock();
}

- (BOOL) render3DLineHack
{
	gpuEvent->ApplyRender3DSettingsLock();
	const BOOL state = CommonSettings.GFX3D_LineHack ? YES : NO;
	gpuEvent->ApplyRender3DSettingsUnlock();
	
	return state;
}

- (void) setRender3DMultisampleSize:(NSUInteger)msaaSize
{
	gpuEvent->ApplyRender3DSettingsLock();
	
	const NSUInteger currentMSAASize = (NSUInteger)CommonSettings.GFX3D_Renderer_MultisampleSize;
	
	if (currentMSAASize != msaaSize)
	{
		switch (currentMSAASize)
		{
			case 0:
			{
				if (msaaSize == (currentMSAASize+1))
				{
					msaaSize = 2;
				}
				break;
			}
				
			case 2:
			{
				if (msaaSize == (currentMSAASize-1))
				{
					msaaSize = 0;
				}
				else if (msaaSize == (currentMSAASize+1))
				{
					msaaSize = 4;
				}
				break;
			}
				
			case 4:
			{
				if (msaaSize == (currentMSAASize-1))
				{
					msaaSize = 2;
				}
				else if (msaaSize == (currentMSAASize+1))
				{
					msaaSize = 8;
				}
				break;
			}
				
			case 8:
			{
				if (msaaSize == (currentMSAASize-1))
				{
					msaaSize = 4;
				}
				else if (msaaSize == (currentMSAASize+1))
				{
					msaaSize = 16;
				}
				break;
			}
				
			case 16:
			{
				if (msaaSize == (currentMSAASize-1))
				{
					msaaSize = 8;
				}
				else if (msaaSize == (currentMSAASize+1))
				{
					msaaSize = 32;
				}
				break;
			}
				
			case 32:
			{
				if (msaaSize == (currentMSAASize-1))
				{
					msaaSize = 16;
				}
				else if (msaaSize == (currentMSAASize+1))
				{
					msaaSize = 32;
				}
				break;
			}
		}
		
		if (msaaSize > openglDeviceMaxMultisamples)
		{
			msaaSize = openglDeviceMaxMultisamples;
		}
		
		msaaSize = GetNearestPositivePOT((uint32_t)msaaSize);
		CommonSettings.GFX3D_Renderer_MultisampleSize = (int)msaaSize;
	}
	
	gpuEvent->ApplyRender3DSettingsUnlock();
	
	NSString *newMsaaSizeString = (msaaSize == 0) ? @"Off" : [NSString stringWithFormat:@"%d", (int)msaaSize];
	[self setRender3DMultisampleSizeString:newMsaaSizeString];
}

- (NSUInteger) render3DMultisampleSize
{
	gpuEvent->ApplyRender3DSettingsLock();
	const NSInteger msaaSize = (NSUInteger)CommonSettings.GFX3D_Renderer_MultisampleSize;
	gpuEvent->ApplyRender3DSettingsUnlock();
	
	return msaaSize;
}

- (void) setRender3DTextureDeposterize:(BOOL)state
{
	gpuEvent->ApplyRender3DSettingsLock();
	CommonSettings.GFX3D_Renderer_TextureDeposterize = state ? true : false;
	gpuEvent->ApplyRender3DSettingsUnlock();
}

- (BOOL) render3DTextureDeposterize
{
	gpuEvent->ApplyRender3DSettingsLock();
	const BOOL state = CommonSettings.GFX3D_Renderer_TextureDeposterize ? YES : NO;
	gpuEvent->ApplyRender3DSettingsUnlock();
	
	return state;
}

- (void) setRender3DTextureSmoothing:(BOOL)state
{
	gpuEvent->ApplyRender3DSettingsLock();
	CommonSettings.GFX3D_Renderer_TextureSmoothing = state ? true : false;
	gpuEvent->ApplyRender3DSettingsUnlock();
}

- (BOOL) render3DTextureSmoothing
{
	gpuEvent->ApplyRender3DSettingsLock();
	const BOOL state = CommonSettings.GFX3D_Renderer_TextureSmoothing ? YES : NO;
	gpuEvent->ApplyRender3DSettingsUnlock();
	
	return state;
}

- (void) setRender3DTextureScalingFactor:(NSUInteger)scalingFactor
{
	int newScalingFactor = (int)scalingFactor;
	
	if (scalingFactor < 1)
	{
		newScalingFactor = 1;
	}
	else if (scalingFactor > 4)
	{
		newScalingFactor = 4;
	}
	
	gpuEvent->ApplyRender3DSettingsLock();
	
	if (newScalingFactor == 3)
	{
		newScalingFactor = (newScalingFactor < CommonSettings.GFX3D_Renderer_TextureScalingFactor) ? 2 : 4;
	}
	
	CommonSettings.GFX3D_Renderer_TextureScalingFactor = newScalingFactor;
	gpuEvent->ApplyRender3DSettingsUnlock();
}

- (NSUInteger) render3DTextureScalingFactor
{
	gpuEvent->ApplyRender3DSettingsLock();
	const NSUInteger scalingFactor = (NSUInteger)CommonSettings.GFX3D_Renderer_TextureScalingFactor;
	gpuEvent->ApplyRender3DSettingsUnlock();
	
	return scalingFactor;
}

- (void) setRender3DFragmentSamplingHack:(BOOL)state
{
	gpuEvent->ApplyRender3DSettingsLock();
	CommonSettings.GFX3D_TXTHack = state ? true : false;
	gpuEvent->ApplyRender3DSettingsUnlock();
}

- (BOOL) render3DFragmentSamplingHack
{
	gpuEvent->ApplyRender3DSettingsLock();
	const BOOL state = CommonSettings.GFX3D_TXTHack ? YES : NO;
	gpuEvent->ApplyRender3DSettingsUnlock();
	
	return state;
}

- (void) setOpenGLEmulateShadowPolygon:(BOOL)state
{
	gpuEvent->ApplyRender3DSettingsLock();
	CommonSettings.OpenGL_Emulation_ShadowPolygon = (state) ? true : false;
	gpuEvent->ApplyRender3DSettingsUnlock();
}

- (BOOL) openGLEmulateShadowPolygon
{
	gpuEvent->ApplyRender3DSettingsLock();
	const BOOL state = (CommonSettings.OpenGL_Emulation_ShadowPolygon) ? YES : NO;
	gpuEvent->ApplyRender3DSettingsUnlock();
	
	return state;
}

- (void) setOpenGLEmulateSpecialZeroAlphaBlending:(BOOL)state
{
	gpuEvent->ApplyRender3DSettingsLock();
	CommonSettings.OpenGL_Emulation_SpecialZeroAlphaBlending = (state) ? true : false;
	gpuEvent->ApplyRender3DSettingsUnlock();
}

- (BOOL) openGLEmulateSpecialZeroAlphaBlending
{
	gpuEvent->ApplyRender3DSettingsLock();
	const BOOL state = (CommonSettings.OpenGL_Emulation_SpecialZeroAlphaBlending) ? YES : NO;
	gpuEvent->ApplyRender3DSettingsUnlock();
	
	return state;
}

- (void) setOpenGLEmulateNDSDepthCalculation:(BOOL)state
{
	gpuEvent->ApplyRender3DSettingsLock();
	CommonSettings.OpenGL_Emulation_NDSDepthCalculation = (state) ? true : false;
	gpuEvent->ApplyRender3DSettingsUnlock();
}

- (BOOL) openGLEmulateNDSDepthCalculation
{
	gpuEvent->ApplyRender3DSettingsLock();
	const BOOL state = (CommonSettings.OpenGL_Emulation_NDSDepthCalculation) ? YES : NO;
	gpuEvent->ApplyRender3DSettingsUnlock();
	
	return state;
}

- (void) setOpenGLEmulateDepthLEqualPolygonFacing:(BOOL)state
{
	gpuEvent->ApplyRender3DSettingsLock();
	CommonSettings.OpenGL_Emulation_DepthLEqualPolygonFacing = (state) ? true : false;
	gpuEvent->ApplyRender3DSettingsUnlock();
}

- (BOOL) openGLEmulateDepthLEqualPolygonFacing
{
	gpuEvent->ApplyRender3DSettingsLock();
	const BOOL state = (CommonSettings.OpenGL_Emulation_DepthLEqualPolygonFacing) ? YES : NO;
	gpuEvent->ApplyRender3DSettingsUnlock();
	
	return state;
}

- (void) setLayerMainGPU:(BOOL)gpuState
{
	gpuEvent->ApplyGPUSettingsLock();
	GPU->GetEngineMain()->SetEnableState((gpuState) ? true : false);
	gpuEvent->ApplyGPUSettingsUnlock();
	
	apple_unfairlock_lock(_unfairlockGpuState);
	gpuStateFlags = (gpuState) ? (gpuStateFlags | GPUSTATE_MAIN_GPU_MASK) : (gpuStateFlags & ~GPUSTATE_MAIN_GPU_MASK);
	apple_unfairlock_unlock(_unfairlockGpuState);
}

- (BOOL) layerMainGPU
{
	gpuEvent->ApplyGPUSettingsLock();
	const BOOL gpuState = GPU->GetEngineMain()->GetEnableState() ? YES : NO;
	gpuEvent->ApplyGPUSettingsUnlock();
	
	return gpuState;
}

- (void) setLayerMainBG0:(BOOL)layerState
{
	gpuEvent->ApplyGPUSettingsLock();
	GPU->GetEngineMain()->SetLayerEnableState(GPULayerID_BG0, (layerState) ? true : false);
	gpuEvent->ApplyGPUSettingsUnlock();
	
	apple_unfairlock_lock(_unfairlockGpuState);
	gpuStateFlags = (layerState) ? (gpuStateFlags | GPUSTATE_MAIN_BG0_MASK) : (gpuStateFlags & ~GPUSTATE_MAIN_BG0_MASK);
	apple_unfairlock_unlock(_unfairlockGpuState);
}

- (BOOL) layerMainBG0
{
	gpuEvent->ApplyGPUSettingsLock();
	const BOOL layerState = GPU->GetEngineMain()->GetLayerEnableState(GPULayerID_BG0);
	gpuEvent->ApplyGPUSettingsUnlock();
	
	return layerState;
}

- (void) setLayerMainBG1:(BOOL)layerState
{
	gpuEvent->ApplyGPUSettingsLock();
	GPU->GetEngineMain()->SetLayerEnableState(GPULayerID_BG1, (layerState) ? true : false);
	gpuEvent->ApplyGPUSettingsUnlock();
	
	apple_unfairlock_lock(_unfairlockGpuState);
	gpuStateFlags = (layerState) ? (gpuStateFlags | GPUSTATE_MAIN_BG1_MASK) : (gpuStateFlags & ~GPUSTATE_MAIN_BG1_MASK);
	apple_unfairlock_unlock(_unfairlockGpuState);
}

- (BOOL) layerMainBG1
{
	gpuEvent->ApplyGPUSettingsLock();
	const BOOL layerState = GPU->GetEngineMain()->GetLayerEnableState(GPULayerID_BG1);
	gpuEvent->ApplyGPUSettingsUnlock();
	
	return layerState;
}

- (void) setLayerMainBG2:(BOOL)layerState
{
	gpuEvent->ApplyGPUSettingsLock();
	GPU->GetEngineMain()->SetLayerEnableState(GPULayerID_BG2, (layerState) ? true : false);
	gpuEvent->ApplyGPUSettingsUnlock();
	
	apple_unfairlock_lock(_unfairlockGpuState);
	gpuStateFlags = (layerState) ? (gpuStateFlags | GPUSTATE_MAIN_BG2_MASK) : (gpuStateFlags & ~GPUSTATE_MAIN_BG2_MASK);
	apple_unfairlock_unlock(_unfairlockGpuState);
}

- (BOOL) layerMainBG2
{
	gpuEvent->ApplyGPUSettingsLock();
	const BOOL layerState = GPU->GetEngineMain()->GetLayerEnableState(GPULayerID_BG2);
	gpuEvent->ApplyGPUSettingsUnlock();
	
	return layerState;
}

- (void) setLayerMainBG3:(BOOL)layerState
{
	gpuEvent->ApplyGPUSettingsLock();
	GPU->GetEngineMain()->SetLayerEnableState(GPULayerID_BG3, (layerState) ? true : false);
	gpuEvent->ApplyGPUSettingsUnlock();
	
	apple_unfairlock_lock(_unfairlockGpuState);
	gpuStateFlags = (layerState) ? (gpuStateFlags | GPUSTATE_MAIN_BG3_MASK) : (gpuStateFlags & ~GPUSTATE_MAIN_BG3_MASK);
	apple_unfairlock_unlock(_unfairlockGpuState);
}

- (BOOL) layerMainBG3
{
	gpuEvent->ApplyGPUSettingsLock();
	const BOOL layerState = GPU->GetEngineMain()->GetLayerEnableState(GPULayerID_BG3);
	gpuEvent->ApplyGPUSettingsUnlock();
	
	return layerState;
}

- (void) setLayerMainOBJ:(BOOL)layerState
{
	gpuEvent->ApplyGPUSettingsLock();
	GPU->GetEngineMain()->SetLayerEnableState(GPULayerID_OBJ, (layerState) ? true : false);
	gpuEvent->ApplyGPUSettingsUnlock();
	
	apple_unfairlock_lock(_unfairlockGpuState);
	gpuStateFlags = (layerState) ? (gpuStateFlags | GPUSTATE_MAIN_OBJ_MASK) : (gpuStateFlags & ~GPUSTATE_MAIN_OBJ_MASK);
	apple_unfairlock_unlock(_unfairlockGpuState);
}

- (BOOL) layerMainOBJ
{
	gpuEvent->ApplyGPUSettingsLock();
	const BOOL layerState = GPU->GetEngineMain()->GetLayerEnableState(GPULayerID_OBJ);
	gpuEvent->ApplyGPUSettingsUnlock();
	
	return layerState;
}

- (void) setLayerSubGPU:(BOOL)gpuState
{
	gpuEvent->ApplyGPUSettingsLock();
	GPU->GetEngineSub()->SetEnableState((gpuState) ? true : false);
	gpuEvent->ApplyGPUSettingsUnlock();
	
	apple_unfairlock_lock(_unfairlockGpuState);
	gpuStateFlags = (gpuState) ? (gpuStateFlags | GPUSTATE_SUB_GPU_MASK) : (gpuStateFlags & ~GPUSTATE_SUB_GPU_MASK);
	apple_unfairlock_unlock(_unfairlockGpuState);
}

- (BOOL) layerSubGPU
{
	gpuEvent->ApplyGPUSettingsLock();
	const BOOL gpuState = GPU->GetEngineSub()->GetEnableState() ? YES : NO;
	gpuEvent->ApplyGPUSettingsUnlock();
	
	return gpuState;
}

- (void) setLayerSubBG0:(BOOL)layerState
{
	gpuEvent->ApplyGPUSettingsLock();
	GPU->GetEngineSub()->SetLayerEnableState(GPULayerID_BG0, (layerState) ? true : false);
	gpuEvent->ApplyGPUSettingsUnlock();
	
	apple_unfairlock_lock(_unfairlockGpuState);
	gpuStateFlags = (layerState) ? (gpuStateFlags | GPUSTATE_SUB_BG0_MASK) : (gpuStateFlags & ~GPUSTATE_SUB_BG0_MASK);
	apple_unfairlock_unlock(_unfairlockGpuState);
}

- (BOOL) layerSubBG0
{
	gpuEvent->ApplyGPUSettingsLock();
	const BOOL layerState = GPU->GetEngineSub()->GetLayerEnableState(GPULayerID_BG0);
	gpuEvent->ApplyGPUSettingsUnlock();
	
	return layerState;
}

- (void) setLayerSubBG1:(BOOL)layerState
{
	gpuEvent->ApplyGPUSettingsLock();
	GPU->GetEngineSub()->SetLayerEnableState(GPULayerID_BG1, (layerState) ? true : false);
	gpuEvent->ApplyGPUSettingsUnlock();
	
	apple_unfairlock_lock(_unfairlockGpuState);
	gpuStateFlags = (layerState) ? (gpuStateFlags | GPUSTATE_SUB_BG1_MASK) : (gpuStateFlags & ~GPUSTATE_SUB_BG1_MASK);
	apple_unfairlock_unlock(_unfairlockGpuState);
}

- (BOOL) layerSubBG1
{
	gpuEvent->ApplyGPUSettingsLock();
	const BOOL layerState = GPU->GetEngineSub()->GetLayerEnableState(GPULayerID_BG1);
	gpuEvent->ApplyGPUSettingsUnlock();
	
	return layerState;
}

- (void) setLayerSubBG2:(BOOL)layerState
{
	gpuEvent->ApplyGPUSettingsLock();
	GPU->GetEngineSub()->SetLayerEnableState(GPULayerID_BG2, (layerState) ? true : false);
	gpuEvent->ApplyGPUSettingsUnlock();
	
	apple_unfairlock_lock(_unfairlockGpuState);
	gpuStateFlags = (layerState) ? (gpuStateFlags | GPUSTATE_SUB_BG2_MASK) : (gpuStateFlags & ~GPUSTATE_SUB_BG2_MASK);
	apple_unfairlock_unlock(_unfairlockGpuState);
}

- (BOOL) layerSubBG2
{
	gpuEvent->ApplyGPUSettingsLock();
	const BOOL layerState = GPU->GetEngineSub()->GetLayerEnableState(GPULayerID_BG2);
	gpuEvent->ApplyGPUSettingsUnlock();
	
	return layerState;
}

- (void) setLayerSubBG3:(BOOL)layerState
{
	gpuEvent->ApplyGPUSettingsLock();
	GPU->GetEngineSub()->SetLayerEnableState(GPULayerID_BG3, (layerState) ? true : false);
	gpuEvent->ApplyGPUSettingsUnlock();
	
	apple_unfairlock_lock(_unfairlockGpuState);
	gpuStateFlags = (layerState) ? (gpuStateFlags | GPUSTATE_SUB_BG3_MASK) : (gpuStateFlags & ~GPUSTATE_SUB_BG3_MASK);
	apple_unfairlock_unlock(_unfairlockGpuState);
}

- (BOOL) layerSubBG3
{
	gpuEvent->ApplyGPUSettingsLock();
	const BOOL layerState = GPU->GetEngineSub()->GetLayerEnableState(GPULayerID_BG3);
	gpuEvent->ApplyGPUSettingsUnlock();
	
	return layerState;
}

- (void) setLayerSubOBJ:(BOOL)layerState
{
	gpuEvent->ApplyGPUSettingsLock();
	GPU->GetEngineSub()->SetLayerEnableState(GPULayerID_OBJ, (layerState) ? true : false);
	gpuEvent->ApplyGPUSettingsUnlock();
	
	apple_unfairlock_lock(_unfairlockGpuState);
	gpuStateFlags = (layerState) ? (gpuStateFlags | GPUSTATE_SUB_OBJ_MASK) : (gpuStateFlags & ~GPUSTATE_SUB_OBJ_MASK);
	apple_unfairlock_unlock(_unfairlockGpuState);
}

- (BOOL) layerSubOBJ
{
	gpuEvent->ApplyGPUSettingsLock();
	const BOOL layerState = GPU->GetEngineSub()->GetLayerEnableState(GPULayerID_OBJ);
	gpuEvent->ApplyGPUSettingsUnlock();
	
	return layerState;
}

- (BOOL) gpuStateByBit:(const UInt32)stateBit
{
	return ([self gpuStateFlags] & (1 << stateBit)) ? YES : NO;
}

- (void) clearWithColor:(const uint16_t)colorBGRA5551
{
#ifdef ENABLE_ASYNC_FETCH
	const size_t maxPages = GPU->GetDisplayInfo().framebufferPageCount;
	for (size_t i = 0; i < maxPages; i++)
	{
		semaphore_wait( ((MacGPUFetchObjectAsync *)fetchObject)->SemaphoreFramebufferPageAtIndex(i) );
	}
#endif
	
	GPU->ClearWithColor(colorBGRA5551);
	
#ifdef ENABLE_ASYNC_FETCH
	for (size_t i = maxPages - 1; i < maxPages; i--)
	{
		semaphore_signal( ((MacGPUFetchObjectAsync *)fetchObject)->SemaphoreFramebufferPageAtIndex(i) );
	}
	
	const u8 bufferIndex = GPU->GetDisplayInfo().bufferIndex;
	((MacGPUFetchObjectAsync *)fetchObject)->SignalFetchAtIndex(bufferIndex, MESSAGE_FETCH_AND_PERFORM_ACTIONS);
#endif
}

@end

#ifdef ENABLE_SHARED_FETCH_OBJECT

@implementation MacClientSharedObject

@synthesize GPUFetchObject;

- (id)init
{
	self = [super init];
	if (self == nil)
	{
		return self;
	}
	
	GPUFetchObject = nil;
	
	return self;
}

@end

#pragma mark -

static void* RunFetchThread(void *arg)
{
#if defined(MAC_OS_X_VERSION_10_6) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6)
	if (kCFCoreFoundationVersionNumber >= kCFCoreFoundationVersionNumber10_6)
	{
		pthread_setname_np("Video Fetch");
	}
#endif
	
	MacGPUFetchObjectAsync *asyncFetchObj = (MacGPUFetchObjectAsync *)arg;
	asyncFetchObj->RunFetchLoop();
	
	return NULL;
}

MacGPUFetchObjectAsync::MacGPUFetchObjectAsync()
{
	_threadFetch = NULL;
	_pauseState = true;
	_threadMessageID = MESSAGE_NONE;
	_fetchIndex = 0;
	pthread_cond_init(&_condSignalFetch, NULL);
	pthread_mutex_init(&_mutexFetchExecute, NULL);
	
	_id = GPUClientFetchObjectID_GenericAsync;
	
	memset(_name, 0, sizeof(_name));
	strncpy(_name, "Generic Asynchronous Video", sizeof(_name) - 1);
	
	memset(_description, 0, sizeof(_description));
	strncpy(_description, "No description.", sizeof(_description) - 1);
	
	_taskEmulationLoop = 0;
	
	for (size_t i = 0; i < MAX_FRAMEBUFFER_PAGES; i++)
	{
		_semFramebuffer[i] = 0;
		_framebufferState[i] = ClientDisplayBufferState_Idle;
		_unfairlockFramebufferStates[i] = apple_unfairlock_create();
	}
}

MacGPUFetchObjectAsync::~MacGPUFetchObjectAsync()
{
	pthread_cancel(this->_threadFetch);
	pthread_join(this->_threadFetch, NULL);
	this->_threadFetch = NULL;
	
	pthread_cond_destroy(&this->_condSignalFetch);
	pthread_mutex_destroy(&this->_mutexFetchExecute);
}

void MacGPUFetchObjectAsync::Init()
{
	if (CommonSettings.num_cores > 1)
	{
		pthread_attr_t threadAttr;
		pthread_attr_init(&threadAttr);
		pthread_attr_setschedpolicy(&threadAttr, SCHED_RR);
		
		struct sched_param sp;
		memset(&sp, 0, sizeof(struct sched_param));
		sp.sched_priority = 44;
		pthread_attr_setschedparam(&threadAttr, &sp);
		
		pthread_create(&_threadFetch, &threadAttr, &RunFetchThread, this);
		pthread_attr_destroy(&threadAttr);
	}
	else
	{
		pthread_create(&_threadFetch, NULL, &RunFetchThread, this);
	}
}

void MacGPUFetchObjectAsync::SetPauseState(bool theState)
{
	this->_pauseState = theState;
}

bool MacGPUFetchObjectAsync::GetPauseState()
{
	return this->_pauseState;
}

void MacGPUFetchObjectAsync::SemaphoreFramebufferCreate()
{
	this->_taskEmulationLoop = mach_task_self();
	
	for (size_t i = 0; i < MAX_FRAMEBUFFER_PAGES; i++)
	{
		semaphore_create(this->_taskEmulationLoop, &this->_semFramebuffer[i], SYNC_POLICY_FIFO, 1);
	}
}

void MacGPUFetchObjectAsync::SemaphoreFramebufferDestroy()
{
	for (size_t i = MAX_FRAMEBUFFER_PAGES - 1; i < MAX_FRAMEBUFFER_PAGES; i--)
	{
		if (this->_semFramebuffer[i] != 0)
		{
			semaphore_destroy(this->_taskEmulationLoop, this->_semFramebuffer[i]);
			this->_semFramebuffer[i] = 0;
		}
	}
}

uint8_t MacGPUFetchObjectAsync::SelectBufferIndex(const uint8_t currentIndex, size_t pageCount)
{
	uint8_t selectedIndex = currentIndex;
	bool stillSearching = true;
	
	// First, search for an idle buffer along with its corresponding semaphore.
	if (stillSearching)
	{
		selectedIndex = (selectedIndex + 1) % pageCount;
		for (; selectedIndex != currentIndex; selectedIndex = (selectedIndex + 1) % pageCount)
		{
			if (this->FramebufferStateAtIndex(selectedIndex) == ClientDisplayBufferState_Idle)
			{
				stillSearching = false;
				break;
			}
		}
	}
	
	// Next, search for either an idle or a ready buffer along with its corresponding semaphore.
	if (stillSearching)
	{
		selectedIndex = (selectedIndex + 1) % pageCount;
		for (size_t spin = 0; spin < 100ULL * pageCount; selectedIndex = (selectedIndex + 1) % pageCount, spin++)
		{
			if ( (this->FramebufferStateAtIndex(selectedIndex) == ClientDisplayBufferState_Idle) ||
				((this->FramebufferStateAtIndex(selectedIndex) == ClientDisplayBufferState_Ready) && (selectedIndex != currentIndex)) )
			{
				stillSearching = false;
				break;
			}
		}
	}
	
	// Since the most available buffers couldn't be taken, we're going to spin for some finite
	// period of time until an idle buffer emerges. If that happens, then force wait on the
	// buffer's corresponding semaphore.
	if (stillSearching)
	{
		selectedIndex = (selectedIndex + 1) % pageCount;
		for (size_t spin = 0; spin < 10000ULL * pageCount; selectedIndex = (selectedIndex + 1) % pageCount, spin++)
		{
			if (this->FramebufferStateAtIndex(selectedIndex) == ClientDisplayBufferState_Idle)
			{
				stillSearching = false;
				break;
			}
		}
	}
	
	// In an effort to find something that is likely to be available shortly in the future,
	// search for any idle, ready or reading buffer, and then force wait on its corresponding
	// semaphore.
	if (stillSearching)
	{
		selectedIndex = (selectedIndex + 1) % pageCount;
		for (; selectedIndex != currentIndex; selectedIndex = (selectedIndex + 1) % pageCount)
		{
			if ( (this->FramebufferStateAtIndex(selectedIndex) == ClientDisplayBufferState_Idle) ||
				 (this->FramebufferStateAtIndex(selectedIndex) == ClientDisplayBufferState_Ready) ||
				 (this->FramebufferStateAtIndex(selectedIndex) == ClientDisplayBufferState_Reading) )
			{
				stillSearching = false;
				break;
			}
		}
	}
	
	return selectedIndex;
}

semaphore_t MacGPUFetchObjectAsync::SemaphoreFramebufferPageAtIndex(const u8 bufferIndex)
{
	assert(bufferIndex < MAX_FRAMEBUFFER_PAGES);
	return this->_semFramebuffer[bufferIndex];
}

ClientDisplayBufferState MacGPUFetchObjectAsync::FramebufferStateAtIndex(uint8_t index)
{
	apple_unfairlock_lock(this->_unfairlockFramebufferStates[index]);
	const ClientDisplayBufferState bufferState = this->_framebufferState[index];
	apple_unfairlock_unlock(this->_unfairlockFramebufferStates[index]);
	
	return bufferState;
}

void MacGPUFetchObjectAsync::SetFramebufferState(ClientDisplayBufferState bufferState, uint8_t index)
{
	apple_unfairlock_lock(this->_unfairlockFramebufferStates[index]);
	this->_framebufferState[index] = bufferState;
	apple_unfairlock_unlock(this->_unfairlockFramebufferStates[index]);
}

void MacGPUFetchObjectAsync::FetchSynchronousAtIndex(uint8_t index)
{
	this->FetchFromBufferIndex(index);
}

void MacGPUFetchObjectAsync::SignalFetchAtIndex(uint8_t index, int32_t messageID)
{
	pthread_mutex_lock(&this->_mutexFetchExecute);
	
	this->_fetchIndex = index;
	this->_threadMessageID = messageID;
	pthread_cond_signal(&this->_condSignalFetch);
	
	pthread_mutex_unlock(&this->_mutexFetchExecute);
}

void MacGPUFetchObjectAsync::RunFetchLoop()
{
	NSAutoreleasePool *tempPool = nil;
	pthread_mutex_lock(&this->_mutexFetchExecute);
	
	do
	{
		tempPool = [[NSAutoreleasePool alloc] init];
		
		while (this->_threadMessageID == MESSAGE_NONE)
		{
			pthread_cond_wait(&this->_condSignalFetch, &this->_mutexFetchExecute);
		}
		
		const uint32_t lastMessageID = this->_threadMessageID;
		this->FetchFromBufferIndex(this->_fetchIndex);
		
		if (lastMessageID == MESSAGE_FETCH_AND_PERFORM_ACTIONS)
		{
			this->DoPostFetchActions();
		}
		
		this->_threadMessageID = MESSAGE_NONE;
		
		[tempPool release];
	} while(true);
}

void MacGPUFetchObjectAsync::DoPostFetchActions()
{
	// Do nothing.
}

#pragma mark -

static void ScreenChangeCallback(CFNotificationCenterRef center,
                                 void *observer,
                                 CFStringRef name,
                                 const void *object,
                                 CFDictionaryRef userInfo)
{
	((MacGPUFetchObjectDisplayLink *)observer)->DisplayLinkListUpdate();
}

static CVReturn MacDisplayLinkCallback(CVDisplayLinkRef displayLinkRef,
                                       const CVTimeStamp *inNow,
                                       const CVTimeStamp *inOutputTime,
                                       CVOptionFlags flagsIn,
                                       CVOptionFlags *flagsOut,
                                       void *displayLinkContext)
{
	MacGPUFetchObjectDisplayLink *fetchObj = (MacGPUFetchObjectDisplayLink *)displayLinkContext;
	
	NSAutoreleasePool *tempPool = [[NSAutoreleasePool alloc] init];
	fetchObj->FlushAllViewsOnDisplayLink(displayLinkRef, inNow, inOutputTime);
	[tempPool release];
	
	return kCVReturnSuccess;
}

MacGPUFetchObjectDisplayLink::MacGPUFetchObjectDisplayLink()
{
	_id = GPUClientFetchObjectID_MacDisplayLink;
	
	memset(_name, 0, sizeof(_name));
	strncpy(_name, "macOS Display Link GPU Fetch", sizeof(_name) - 1);
	
	memset(_description, 0, sizeof(_description));
	strncpy(_description, "No description.", sizeof(_description) - 1);
	
	pthread_mutex_init(&_mutexDisplayLinkLists, NULL);
	
	_displayLinksActiveList.clear();
	_displayLinksStartedList.clear();
	_displayLinkFlushTimeList.clear();
	DisplayLinkListUpdate();
	
    CFNotificationCenterAddObserver(CFNotificationCenterGetLocalCenter(),
	                                this,
	                                ScreenChangeCallback,
	                                CFSTR("NSApplicationDidChangeScreenParametersNotification"),
	                                NULL,
	                                CFNotificationSuspensionBehaviorDeliverImmediately);
}

MacGPUFetchObjectDisplayLink::~MacGPUFetchObjectDisplayLink()
{
	CFNotificationCenterRemoveObserver(CFNotificationCenterGetLocalCenter(),
	                                   this,
	                                   CFSTR("NSApplicationDidChangeScreenParametersNotification"),
	                                   NULL);
	
	pthread_mutex_lock(&this->_mutexDisplayLinkLists);
	
	while (this->_displayLinksActiveList.size() > 0)
	{
		DisplayLinksActiveMap::iterator it = this->_displayLinksActiveList.begin();
		CGDirectDisplayID displayID = it->first;
		CVDisplayLinkRef displayLinkRef = it->second;
		
		if (CVDisplayLinkIsRunning(displayLinkRef))
		{
			CVDisplayLinkStop(displayLinkRef);
			this->_displayLinksStartedList.erase(displayID);
		}
		
		CVDisplayLinkRelease(displayLinkRef);
		
		this->_displayLinksActiveList.erase(displayID);
		this->_displayLinkFlushTimeList.erase(displayID);
	}
	
	pthread_mutex_unlock(&this->_mutexDisplayLinkLists);
	pthread_mutex_destroy(&this->_mutexDisplayLinkLists);
}

void MacGPUFetchObjectDisplayLink::DisplayLinkStartUsingID(CGDirectDisplayID displayID)
{
	CVDisplayLinkRef displayLinkRef = NULL;
	
	pthread_mutex_lock(&this->_mutexDisplayLinkLists);
	
	if (this->_displayLinksActiveList.find(displayID) != this->_displayLinksActiveList.end())
	{
		displayLinkRef = this->_displayLinksActiveList[displayID];
	}
	
	if ( (displayLinkRef != NULL) && !CVDisplayLinkIsRunning(displayLinkRef) )
	{
		CVDisplayLinkStart(displayLinkRef);
		this->_displayLinksStartedList[displayID] = displayLinkRef;
	}
	
	pthread_mutex_unlock(&this->_mutexDisplayLinkLists);
}

void MacGPUFetchObjectDisplayLink::DisplayLinkListUpdate()
{
	// Set up the display links
	NSArray *screenList = [NSScreen screens];
	std::set<CGDirectDisplayID> screenActiveDisplayIDsList;
	
	pthread_mutex_lock(&this->_mutexDisplayLinkLists);
	
	// Add new CGDirectDisplayIDs for new screens
	for (size_t i = 0; i < [screenList count]; i++)
	{
		NSScreen *screen = [screenList objectAtIndex:i];
		NSDictionary *deviceDescription = [screen deviceDescription];
		NSNumber *idNumber = (NSNumber *)[deviceDescription valueForKey:@"NSScreenNumber"];
		
		CGDirectDisplayID displayID = [idNumber unsignedIntValue];
		bool isDisplayLinkStillActive = (this->_displayLinksActiveList.find(displayID) != this->_displayLinksActiveList.end());
		
		if (!isDisplayLinkStillActive)
		{
			CVDisplayLinkRef newDisplayLink;
			CVDisplayLinkCreateWithCGDisplay(displayID, &newDisplayLink);
			CVDisplayLinkSetOutputCallback(newDisplayLink, &MacDisplayLinkCallback, this);
			
			this->_displayLinksActiveList[displayID] = newDisplayLink;
			this->_displayLinkFlushTimeList[displayID] = 0;
		}
		
		// While we're iterating through NSScreens, save the CGDirectDisplayID to a temporary list for later use.
		screenActiveDisplayIDsList.insert(displayID);
	}
	
	// Remove old CGDirectDisplayIDs for screens that no longer exist
	for (DisplayLinksActiveMap::iterator it = this->_displayLinksActiveList.begin(); it != this->_displayLinksActiveList.end(); )
	{
		CGDirectDisplayID displayID = it->first;
		CVDisplayLinkRef displayLinkRef = it->second;
		
		if (screenActiveDisplayIDsList.find(displayID) == screenActiveDisplayIDsList.end())
		{
			if (CVDisplayLinkIsRunning(displayLinkRef))
			{
				CVDisplayLinkStop(displayLinkRef);
				this->_displayLinksStartedList.erase(displayID);
			}
			
			CVDisplayLinkRelease(displayLinkRef);
			
			this->_displayLinksActiveList.erase(displayID);
			this->_displayLinkFlushTimeList.erase(displayID);
			
			if (this->_displayLinksActiveList.empty())
			{
				break;
			}
			else
			{
				it = this->_displayLinksActiveList.begin();
				continue;
			}
		}
		
		++it;
	}
	
	pthread_mutex_unlock(&this->_mutexDisplayLinkLists);
}

void MacGPUFetchObjectDisplayLink::FlushAllViewsOnDisplayLink(CVDisplayLinkRef displayLinkRef, const CVTimeStamp *timeStampNow, const CVTimeStamp *timeStampOutput)
{
	CGDirectDisplayID displayID = CVDisplayLinkGetCurrentCGDisplay(displayLinkRef);
	bool didFlushOccur = false;
	
	std::vector<ClientDisplayViewInterface *> cdvFlushList;
	this->_outputManager->GenerateFlushListForDisplay((int32_t)displayID, cdvFlushList);
	
	const size_t listSize = cdvFlushList.size();
	if (listSize > 0)
	{
		this->FlushMultipleViews(cdvFlushList, (uint64_t)timeStampNow->videoTime, timeStampOutput->hostTime);
		didFlushOccur = true;
	}
	
	if (didFlushOccur)
	{
		// Set the new time limit to 8 seconds after the current time.
		this->_displayLinkFlushTimeList[displayID] = timeStampNow->videoTime + (timeStampNow->videoTimeScale * VIDEO_FLUSH_TIME_LIMIT_OFFSET);
	}
	else if (timeStampNow->videoTime > this->_displayLinkFlushTimeList[displayID])
	{
		CVDisplayLinkStop(displayLinkRef);
		this->_displayLinksStartedList.erase(displayID);
	}
}

void MacGPUFetchObjectDisplayLink::SetPauseState(bool theState)
{
	if (theState == this->_pauseState)
	{
		return;
	}
	
	this->_pauseState = theState;
	
	if (theState)
	{
		for (DisplayLinksActiveMap::iterator it = this->_displayLinksStartedList.begin(); it != this->_displayLinksStartedList.end(); it++)
		{
			CVDisplayLinkRef displayLinkRef = it->second;
			if ( (displayLinkRef != NULL) && CVDisplayLinkIsRunning(displayLinkRef) )
			{
				CVDisplayLinkStop(displayLinkRef);
			}
		}
		
		this->FinalizeAllViews();
	}
	else
	{
		for (DisplayLinksActiveMap::iterator it = this->_displayLinksStartedList.begin(); it != this->_displayLinksStartedList.end(); it++)
		{
			CVDisplayLinkRef displayLinkRef = it->second;
			if ( (displayLinkRef != NULL) && !CVDisplayLinkIsRunning(displayLinkRef) )
			{
				CVDisplayLinkStart(displayLinkRef);
			}
		}
	}
}

void MacGPUFetchObjectDisplayLink::DoPostFetchActions()
{
	this->PushVideoDataToAllDisplayViews();
}

#endif // ENABLE_SHARED_FETCH_OBJECT

#pragma mark -

MacGPUEventHandlerAsync::MacGPUEventHandlerAsync()
{
	_fetchObject = nil;
	
	_widthPending       = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	_heightPending      = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	_colorFormatPending = NDSColorFormat_BGR555_Rev;
	_pageCountPending   = 1;
	
	_didColorFormatChange = true;
	_didWidthChange       = true;
	_didHeightChange      = true;
	_didPageCountChange   = true;
	
	_cpuCoreCountRestoreValue = 0;
	
	pthread_mutex_init(&_mutexApplyGPUSettings, NULL);
	pthread_mutex_init(&_mutexApplyRender3DSettings, NULL);
}

MacGPUEventHandlerAsync::~MacGPUEventHandlerAsync()
{
	pthread_mutex_destroy(&this->_mutexApplyGPUSettings);
	pthread_mutex_destroy(&this->_mutexApplyRender3DSettings);
}

GPUClientFetchObject* MacGPUEventHandlerAsync::GetFetchObject() const
{
	return this->_fetchObject;
}

void MacGPUEventHandlerAsync::SetFetchObject(GPUClientFetchObject *fetchObject)
{
	this->_fetchObject = fetchObject;
}

void MacGPUEventHandlerAsync::SetFramebufferPageCount(size_t pageCount)
{
	this->ApplyGPUSettingsLock();
	const NDSDisplayInfo &displayInfo = GPU->GetDisplayInfo();
	this->_didPageCountChange = (pageCount != displayInfo.framebufferPageCount);
	this->_pageCountPending = pageCount;
	this->ApplyGPUSettingsUnlock();
}

size_t MacGPUEventHandlerAsync::GetFramebufferPageCount()
{
	return this->_pageCountPending;
}

void MacGPUEventHandlerAsync::SetFramebufferDimensions(size_t w, size_t h)
{
	if (w < GPU_FRAMEBUFFER_NATIVE_WIDTH)
	{
		w = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	}
	
	if (h < GPU_FRAMEBUFFER_NATIVE_HEIGHT)
	{
		h = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	}
	
	this->ApplyGPUSettingsLock();
	const NDSDisplayInfo &displayInfo = GPU->GetDisplayInfo();
	this->_didWidthChange  = (w != displayInfo.customWidth);
	this->_didHeightChange = (h != displayInfo.customHeight);
	this->_widthPending  = w;
	this->_heightPending = h;
	this->ApplyGPUSettingsUnlock();
}

void MacGPUEventHandlerAsync::GetFramebufferDimensions(size_t &outWidth, size_t &outHeight)
{
	outWidth  = this->_widthPending;
	outHeight = this->_heightPending;
}

void MacGPUEventHandlerAsync::SetColorFormat(NDSColorFormat colorFormat)
{
	this->ApplyGPUSettingsLock();
	const NDSDisplayInfo &displayInfo = GPU->GetDisplayInfo();
	this->_didColorFormatChange = (colorFormat != displayInfo.colorFormat);
	this->_colorFormatPending = colorFormat;
	this->ApplyGPUSettingsUnlock();
}

NDSColorFormat MacGPUEventHandlerAsync::GetColorFormat()
{
	return this->_colorFormatPending;
}

#ifdef ENABLE_ASYNC_FETCH

void MacGPUEventHandlerAsync::DidFrameBegin(const size_t line, const bool isFrameSkipRequested, const size_t pageCount, u8 &selectedBufferIndexInOut)
{
	MacGPUFetchObjectAsync *asyncFetchObj = (MacGPUFetchObjectAsync *)this->_fetchObject;
	
	if (!isFrameSkipRequested)
	{
		if ( (pageCount > 1) && (line == 0) )
		{
			selectedBufferIndexInOut = asyncFetchObj->SelectBufferIndex(selectedBufferIndexInOut, pageCount);
		}
		
		semaphore_wait( asyncFetchObj->SemaphoreFramebufferPageAtIndex(selectedBufferIndexInOut) );
		asyncFetchObj->SetFramebufferState(ClientDisplayBufferState_Writing, selectedBufferIndexInOut);
	}
}

void MacGPUEventHandlerAsync::DidFrameEnd(bool isFrameSkipped, const NDSDisplayInfo &latestDisplayInfo)
{
	MacGPUFetchObjectAsync *asyncFetchObj = (MacGPUFetchObjectAsync *)this->_fetchObject;
	
	if (!isFrameSkipped)
	{
		asyncFetchObj->SetFetchDisplayInfo(latestDisplayInfo);
		asyncFetchObj->SetFramebufferState(ClientDisplayBufferState_Ready, latestDisplayInfo.bufferIndex);
		semaphore_signal( asyncFetchObj->SemaphoreFramebufferPageAtIndex(latestDisplayInfo.bufferIndex) );
		asyncFetchObj->SignalFetchAtIndex(latestDisplayInfo.bufferIndex, MESSAGE_FETCH_AND_PERFORM_ACTIONS);
	}
}

#endif // ENABLE_ASYNC_FETCH

void MacGPUEventHandlerAsync::DidApplyGPUSettingsBegin()
{
	this->ApplyGPUSettingsLock();
	
	if (this->_didWidthChange || this->_didHeightChange || this->_didColorFormatChange || this->_didPageCountChange)
	{
		MacGPUFetchObjectAsync *asyncFetchObj = (MacGPUFetchObjectAsync *)this->_fetchObject;
		asyncFetchObj->SetPauseState(true);
		
		GPU->SetCustomFramebufferSize(this->_widthPending, this->_heightPending);
		GPU->SetColorFormat(this->_colorFormatPending);
		GPU->SetFramebufferPageCount(this->_pageCountPending);
	}
}

void MacGPUEventHandlerAsync::DidApplyGPUSettingsEnd()
{
	if (this->_didWidthChange || this->_didHeightChange || this->_didColorFormatChange || this->_didPageCountChange)
	{
		const NDSDisplayInfo &displayInfo = GPU->GetDisplayInfo();
		MacGPUFetchObjectAsync *asyncFetchObj = (MacGPUFetchObjectAsync *)this->_fetchObject;
		
#ifdef ENABLE_ASYNC_FETCH
		const size_t maxPages = displayInfo.framebufferPageCount;
		for (size_t i = 0; i < maxPages; i++)
		{
			semaphore_wait( asyncFetchObj->SemaphoreFramebufferPageAtIndex(i) );
		}
#endif
		this->_fetchObject->SetFetchBuffers(displayInfo);

#ifdef ENABLE_ASYNC_FETCH
		for (size_t i = maxPages - 1; i < maxPages; i--)
		{
			semaphore_signal( asyncFetchObj->SemaphoreFramebufferPageAtIndex(i) );
		}
#endif
		asyncFetchObj->SetPauseState(false);
		
		// Update all the change flags now that settings are applied.
		// In theory, all these flags should get cleared.
		this->_didWidthChange       = (this->_widthPending       != displayInfo.customWidth);
		this->_didHeightChange      = (this->_heightPending      != displayInfo.customHeight);
		this->_didColorFormatChange = (this->_colorFormatPending != displayInfo.colorFormat);
		this->_didPageCountChange   = (this->_pageCountPending   != displayInfo.framebufferPageCount);
	}
	
	this->ApplyGPUSettingsUnlock();
}

void MacGPUEventHandlerAsync::DidApplyRender3DSettingsBegin()
{
	this->ApplyRender3DSettingsLock();
}

void MacGPUEventHandlerAsync::DidApplyRender3DSettingsEnd()
{
	if (this->_cpuCoreCountRestoreValue > 0)
	{
		CommonSettings.num_cores = this->_cpuCoreCountRestoreValue;
	}
	
	this->_cpuCoreCountRestoreValue = 0;
	this->ApplyRender3DSettingsUnlock();
}

void MacGPUEventHandlerAsync::ApplyGPUSettingsLock()
{
	pthread_mutex_lock(&this->_mutexApplyGPUSettings);
}

void MacGPUEventHandlerAsync::ApplyGPUSettingsUnlock()
{
	pthread_mutex_unlock(&this->_mutexApplyGPUSettings);
}

void MacGPUEventHandlerAsync::ApplyRender3DSettingsLock()
{
	pthread_mutex_lock(&this->_mutexApplyRender3DSettings);
}

void MacGPUEventHandlerAsync::ApplyRender3DSettingsUnlock()
{
	pthread_mutex_unlock(&this->_mutexApplyRender3DSettings);
}

void MacGPUEventHandlerAsync::SetTempThreadCount(int threadCount)
{
	if (threadCount < 1)
	{
		this->_cpuCoreCountRestoreValue = 0;
	}
	else
	{
		this->_cpuCoreCountRestoreValue = CommonSettings.num_cores;
		CommonSettings.num_cores = threadCount;
	}
}
