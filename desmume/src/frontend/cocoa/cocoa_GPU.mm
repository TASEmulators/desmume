/*
	Copyright (C) 2013-2018 DeSmuME team

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
#import "cocoa_output.h"
#import "cocoa_globals.h"
#include "utilities.h"

#include "../../NDSSystem.h"
#include "../../rasterize.h"

#ifdef MAC_OS_X_VERSION_10_7
	#include "../../OGLRender_3_2.h"
#else
	#include "../../OGLRender.h"
#endif

#include <OpenGL/OpenGL.h>
#import "userinterface/MacOGLDisplayView.h"

#ifdef ENABLE_APPLE_METAL
	#import "userinterface/MacMetalDisplayView.h"
#endif

#ifdef BOOL
#undef BOOL
#endif

GPU3DInterface *core3DList[] = {
	&gpu3DNull,
	&gpu3DRasterize,
	&gpu3Dgl,
	NULL
};

class GPUEventHandlerOSX : public GPUEventHandlerDefault
{
private:
	GPUClientFetchObject *_fetchObject;
	
	pthread_mutex_t _mutexFrame;
	pthread_mutex_t _mutex3DRender;
	pthread_mutex_t _mutexApplyGPUSettings;
	pthread_mutex_t _mutexApplyRender3DSettings;
	bool _render3DNeedsFinish;
	
public:
	GPUEventHandlerOSX();
	~GPUEventHandlerOSX();
	
	GPUClientFetchObject* GetFetchObject() const;
	void SetFetchObject(GPUClientFetchObject *fetchObject);
	
	void FramebufferLock();
	void FramebufferUnlock();
	void Render3DLock();
	void Render3DUnlock();
	void ApplyGPUSettingsLock();
	void ApplyGPUSettingsUnlock();
	void ApplyRender3DSettingsLock();
	void ApplyRender3DSettingsUnlock();
	
	bool GetRender3DNeedsFinish();
	
	virtual void DidFrameBegin(const size_t line, const bool isFrameSkipRequested, const size_t pageCount, u8 &selectedBufferIndexInOut);
	virtual void DidFrameEnd(bool isFrameSkipped, const NDSDisplayInfo &latestDisplayInfo);
	virtual void DidRender3DBegin();
	virtual void DidRender3DEnd();
	virtual void DidApplyGPUSettingsBegin();
	virtual void DidApplyGPUSettingsEnd();
	virtual void DidApplyRender3DSettingsBegin();
	virtual void DidApplyRender3DSettingsEnd();
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

#ifdef ENABLE_SHARED_FETCH_OBJECT
@synthesize fetchObject;
@dynamic sharedData;
#endif

- (id)init
{
	self = [super init];
	if (self == nil)
	{
		return self;
	}
	
	spinlockGpuState = OS_SPINLOCK_INIT;
	
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
	_needRestoreRender3DLock = NO;
		
	SetOpenGLRendererFunctions(&OSXOpenGLRendererInit,
							   &OSXOpenGLRendererBegin,
							   &OSXOpenGLRendererEnd,
							   &OSXOpenGLRendererFramebufferDidResize);
	
	gpuEvent = new GPUEventHandlerOSX;
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
			GPU->SetFramebufferPageCount(METAL_FETCH_BUFFER_COUNT);
			GPU->SetWillPostprocessDisplays(false);
		}
	}
#endif
	
#ifdef ENABLE_SHARED_FETCH_OBJECT
	if (fetchObject == NULL)
	{
		fetchObject = new MacOGLClientFetchObject;
		GPU->SetFramebufferPageCount(OPENGL_FETCH_BUFFER_COUNT);
	}
	
	fetchObject->Init();
	gpuEvent->SetFetchObject(fetchObject);
	
	GPU->SetWillAutoResolveToCustomBuffer(false);
#endif
	
	openglDeviceMaxMultisamples = 0;
	render3DMultisampleSizeString = @"Off";
	
	bool isTempContextCreated = OSXOpenGLRendererInit();
	if (isTempContextCreated)
	{
		OSXOpenGLRendererBegin();
		GLint maxSamplesOGL = 0;
		
#if defined(GL_MAX_SAMPLES)
		glGetIntegerv(GL_MAX_SAMPLES, &maxSamplesOGL);
#elif defined(GL_MAX_SAMPLES_EXT)
		glGetIntegerv(GL_MAX_SAMPLES_EXT, &maxSamplesOGL);
#endif
		
		openglDeviceMaxMultisamples = maxSamplesOGL;
		
		OSXOpenGLRendererEnd();
		DestroyOpenGLRenderer();
	}
	
	return self;
}

- (void)dealloc
{
	DestroyOpenGLRenderer();
	
	delete fetchObject;
	delete gpuEvent;
	
	[self setRender3DMultisampleSizeString:nil];
	
	[super dealloc];
}

- (GPUClientFetchObject *) fetchObject
{
	return fetchObject;
}

- (MacClientSharedObject *) sharedData
{
	return (MacClientSharedObject *)fetchObject->GetClientData();
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

- (void) setGpuDimensions:(NSSize)theDimensions
{
	const size_t w = (size_t)(theDimensions.width + 0.01);
	const size_t h = (size_t)(theDimensions.height + 0.01);
	
	gpuEvent->Render3DLock();
	gpuEvent->FramebufferLock();
	
#ifdef ENABLE_SHARED_FETCH_OBJECT
	const size_t maxPages = GPU->GetDisplayInfo().framebufferPageCount;
	for (size_t i = 0; i < maxPages; i++)
	{
		semaphore_wait([[self sharedData] semaphoreFramebufferPageAtIndex:i]);
	}
#endif
	
	GPU->SetCustomFramebufferSize(w, h);
	
#ifdef ENABLE_SHARED_FETCH_OBJECT
	fetchObject->SetFetchBuffers(GPU->GetDisplayInfo());
	
	for (size_t i = maxPages - 1; i < maxPages; i--)
	{
		semaphore_signal([[self sharedData] semaphoreFramebufferPageAtIndex:i]);
	}
#endif
	
	gpuEvent->FramebufferUnlock();
	gpuEvent->Render3DUnlock();
}

- (NSSize) gpuDimensions
{
	gpuEvent->Render3DLock();
	gpuEvent->FramebufferLock();
	const NSSize dimensions = NSMakeSize(GPU->GetCustomFramebufferWidth(), GPU->GetCustomFramebufferHeight());
	gpuEvent->FramebufferUnlock();
	gpuEvent->Render3DUnlock();
	
	return dimensions;
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
	
	// Change the color format.
	gpuEvent->Render3DLock();
	gpuEvent->FramebufferLock();
	
	const NDSDisplayInfo &dispInfo = GPU->GetDisplayInfo();
	
	if (colorFormat != dispInfo.colorFormat)
	{
#ifdef ENABLE_SHARED_FETCH_OBJECT
		const size_t maxPages = GPU->GetDisplayInfo().framebufferPageCount;
		for (size_t i = 0; i < maxPages; i++)
		{
			semaphore_wait([[self sharedData] semaphoreFramebufferPageAtIndex:i]);
		}
#endif
		
		GPU->SetColorFormat((NDSColorFormat)colorFormat);
		
#ifdef ENABLE_SHARED_FETCH_OBJECT
		fetchObject->SetFetchBuffers(GPU->GetDisplayInfo());
		
		for (size_t i = maxPages - 1; i < maxPages; i--)
		{
			semaphore_signal([[self sharedData] semaphoreFramebufferPageAtIndex:i]);
		}
#endif
	}
	
	gpuEvent->FramebufferUnlock();
	gpuEvent->Render3DUnlock();
}

- (NSUInteger) gpuColorFormat
{
	gpuEvent->Render3DLock();
	gpuEvent->FramebufferLock();
	const NSUInteger colorFormat = (NSUInteger)GPU->GetDisplayInfo().colorFormat;
	gpuEvent->FramebufferUnlock();
	gpuEvent->Render3DUnlock();
	
	return colorFormat;
}

#ifdef ENABLE_SHARED_FETCH_OBJECT
- (void) setOutputList:(NSMutableArray *)theOutputList rwlock:(pthread_rwlock_t *)theRWLock
{
	[(MacClientSharedObject *)fetchObject->GetClientData() setOutputList:theOutputList rwlock:theRWLock];
}
#endif

- (void) setRender3DRenderingEngine:(NSInteger)rendererID
{
	gpuEvent->ApplyRender3DSettingsLock();
	GPU->Set3DRendererByID(rendererID);
	gpuEvent->ApplyRender3DSettingsUnlock();
}

- (NSInteger) render3DRenderingEngine
{
	gpuEvent->ApplyRender3DSettingsLock();
	const NSInteger rendererID = (NSInteger)GPU->Get3DRendererID();
	gpuEvent->ApplyRender3DSettingsUnlock();
	
	return rendererID;
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
	NSUInteger numberCores = [[NSProcessInfo processInfo] activeProcessorCount];
	if (numberThreads == 0)
	{
		isCPUCoreCountAuto = YES;
		if (numberCores < 2)
		{
			numberCores = 1;
		}
		else
		{
			const NSUInteger reserveCoreCount = numberCores / 12; // For every 12 cores, reserve 1 core for the rest of the system.
			numberCores -= reserveCoreCount;
		}
	}
	else
	{
		isCPUCoreCountAuto = NO;
		numberCores = numberThreads;
	}
	
	const RendererID renderingEngineID = (RendererID)[self render3DRenderingEngine];
	
	gpuEvent->ApplyRender3DSettingsLock();
	
	CommonSettings.num_cores = numberCores;
	
	if (renderingEngineID == CORE3DLIST_SWRASTERIZE)
	{
		GPU->Set3DRendererByID(renderingEngineID);
	}
	
	gpuEvent->ApplyRender3DSettingsUnlock();
}

- (NSUInteger) render3DThreads
{
	gpuEvent->ApplyRender3DSettingsLock();
	const NSUInteger numberThreads = isCPUCoreCountAuto ? 0 : (NSUInteger)CommonSettings.num_cores;
	gpuEvent->ApplyRender3DSettingsUnlock();
	
	return numberThreads;
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
	
	const int currentMSAASize = CommonSettings.GFX3D_Renderer_MultisampleSize;
	
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
		
		msaaSize = GetNearestPositivePOT(msaaSize);
		CommonSettings.GFX3D_Renderer_MultisampleSize = msaaSize;
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
	
	OSSpinLockLock(&spinlockGpuState);
	gpuStateFlags = (gpuState) ? (gpuStateFlags | GPUSTATE_MAIN_GPU_MASK) : (gpuStateFlags & ~GPUSTATE_MAIN_GPU_MASK);
	OSSpinLockUnlock(&spinlockGpuState);
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
	
	OSSpinLockLock(&spinlockGpuState);
	gpuStateFlags = (layerState) ? (gpuStateFlags | GPUSTATE_MAIN_BG0_MASK) : (gpuStateFlags & ~GPUSTATE_MAIN_BG0_MASK);
	OSSpinLockUnlock(&spinlockGpuState);
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
	
	OSSpinLockLock(&spinlockGpuState);
	gpuStateFlags = (layerState) ? (gpuStateFlags | GPUSTATE_MAIN_BG1_MASK) : (gpuStateFlags & ~GPUSTATE_MAIN_BG1_MASK);
	OSSpinLockUnlock(&spinlockGpuState);
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
	
	OSSpinLockLock(&spinlockGpuState);
	gpuStateFlags = (layerState) ? (gpuStateFlags | GPUSTATE_MAIN_BG2_MASK) : (gpuStateFlags & ~GPUSTATE_MAIN_BG2_MASK);
	OSSpinLockUnlock(&spinlockGpuState);
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
	
	OSSpinLockLock(&spinlockGpuState);
	gpuStateFlags = (layerState) ? (gpuStateFlags | GPUSTATE_MAIN_BG3_MASK) : (gpuStateFlags & ~GPUSTATE_MAIN_BG3_MASK);
	OSSpinLockUnlock(&spinlockGpuState);
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
	
	OSSpinLockLock(&spinlockGpuState);
	gpuStateFlags = (layerState) ? (gpuStateFlags | GPUSTATE_MAIN_OBJ_MASK) : (gpuStateFlags & ~GPUSTATE_MAIN_OBJ_MASK);
	OSSpinLockUnlock(&spinlockGpuState);
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
	
	OSSpinLockLock(&spinlockGpuState);
	gpuStateFlags = (gpuState) ? (gpuStateFlags | GPUSTATE_SUB_GPU_MASK) : (gpuStateFlags & ~GPUSTATE_SUB_GPU_MASK);
	OSSpinLockUnlock(&spinlockGpuState);
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
	
	OSSpinLockLock(&spinlockGpuState);
	gpuStateFlags = (layerState) ? (gpuStateFlags | GPUSTATE_SUB_BG0_MASK) : (gpuStateFlags & ~GPUSTATE_SUB_BG0_MASK);
	OSSpinLockUnlock(&spinlockGpuState);
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
	
	OSSpinLockLock(&spinlockGpuState);
	gpuStateFlags = (layerState) ? (gpuStateFlags | GPUSTATE_SUB_BG1_MASK) : (gpuStateFlags & ~GPUSTATE_SUB_BG1_MASK);
	OSSpinLockUnlock(&spinlockGpuState);
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
	
	OSSpinLockLock(&spinlockGpuState);
	gpuStateFlags = (layerState) ? (gpuStateFlags | GPUSTATE_SUB_BG2_MASK) : (gpuStateFlags & ~GPUSTATE_SUB_BG2_MASK);
	OSSpinLockUnlock(&spinlockGpuState);
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
	
	OSSpinLockLock(&spinlockGpuState);
	gpuStateFlags = (layerState) ? (gpuStateFlags | GPUSTATE_SUB_BG3_MASK) : (gpuStateFlags & ~GPUSTATE_SUB_BG3_MASK);
	OSSpinLockUnlock(&spinlockGpuState);
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
	
	OSSpinLockLock(&spinlockGpuState);
	gpuStateFlags = (layerState) ? (gpuStateFlags | GPUSTATE_SUB_OBJ_MASK) : (gpuStateFlags & ~GPUSTATE_SUB_OBJ_MASK);
	OSSpinLockUnlock(&spinlockGpuState);
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

- (NSString *) render3DRenderingEngineString
{
	NSString *theString = @"Uninitialized";
	
	gpuEvent->ApplyRender3DSettingsLock();
	
	if (gpu3D == NULL)
	{
		gpuEvent->ApplyRender3DSettingsUnlock();
		return theString;
	}
	
	const char *theName = gpu3D->name;
	theString = [NSString stringWithCString:theName encoding:NSUTF8StringEncoding];
	
	gpuEvent->ApplyRender3DSettingsUnlock();
	
	return theString;
}

- (void) clearWithColor:(const uint16_t)colorBGRA5551
{
	gpuEvent->FramebufferLock();
	
#ifdef ENABLE_SHARED_FETCH_OBJECT
	const size_t maxPages = GPU->GetDisplayInfo().framebufferPageCount;
	for (size_t i = 0; i < maxPages; i++)
	{
		semaphore_wait([[self sharedData] semaphoreFramebufferPageAtIndex:i]);
	}
#endif
	
	GPU->ClearWithColor(colorBGRA5551);
	
#ifdef ENABLE_SHARED_FETCH_OBJECT
	for (size_t i = maxPages - 1; i < maxPages; i--)
	{
		semaphore_signal([[self sharedData] semaphoreFramebufferPageAtIndex:i]);
	}
#endif
	
	gpuEvent->FramebufferUnlock();
	
#ifdef ENABLE_SHARED_FETCH_OBJECT
	const u8 bufferIndex = GPU->GetDisplayInfo().bufferIndex;
	[[self sharedData] signalFetchAtIndex:bufferIndex message:MESSAGE_FETCH_AND_PUSH_VIDEO];
#endif
}

- (void) respondToPauseState:(BOOL)isPaused
{
	if (isPaused)
	{
		if (!_needRestoreRender3DLock && gpuEvent->GetRender3DNeedsFinish())
		{
			gpuEvent->Render3DUnlock();
			_needRestoreRender3DLock = YES;
		}
	}
	else
	{
		if (_needRestoreRender3DLock && gpuEvent->GetRender3DNeedsFinish())
		{
			gpuEvent->Render3DLock();
		}
		
		_needRestoreRender3DLock = NO;
	}
}

@end

#ifdef ENABLE_SHARED_FETCH_OBJECT

@implementation MacClientSharedObject

@synthesize GPUFetchObject;
@synthesize numberViewsUsingDirectToCPUFiltering;

- (id)init
{
	self = [super init];
	if (self == nil)
	{
		return self;
	}
	
	pthread_mutex_init(&_mutexDisplayLinkLists, NULL);
	
	GPUFetchObject = nil;
	_rwlockOutputList = NULL;
	_cdsOutputList = nil;
	numberViewsUsingDirectToCPUFiltering = 0;
	
	_displayLinksActiveList.clear();
	_displayLinkFlushTimeList.clear();
	[self displayLinkListUpdate];
	
	spinlockFetchSignal = OS_SPINLOCK_INIT;
	_threadMessageID = MESSAGE_NONE;
	_fetchIndex = 0;
	pthread_cond_init(&_condSignalFetch, NULL);
	pthread_mutex_init(&_mutexFetchExecute, NULL);
	
	pthread_attr_t threadAttr;
	pthread_attr_init(&threadAttr);
	pthread_attr_setschedpolicy(&threadAttr, SCHED_RR);
	
	struct sched_param sp;
	memset(&sp, 0, sizeof(struct sched_param));
	sp.sched_priority = 44;
	pthread_attr_setschedparam(&threadAttr, &sp);
	
	pthread_create(&_threadFetch, &threadAttr, &RunFetchThread, self);
	pthread_attr_destroy(&threadAttr);
	
	_taskEmulationLoop = 0;
	
	for (size_t i = 0; i < MAX_FRAMEBUFFER_PAGES; i++)
	{
		_semFramebuffer[i] = 0;
		_framebufferState[i] = ClientDisplayBufferState_Idle;
		_spinlockFramebufferStates[i] = OS_SPINLOCK_INIT;
	}
	
	[[NSNotificationCenter defaultCenter] addObserver:self
											 selector:@selector(respondToScreenChange:)
												 name:@"NSApplicationDidChangeScreenParametersNotification"
											   object:NSApp];
	
	return self;
}

- (void)dealloc
{
	pthread_cancel(_threadFetch);
	pthread_join(_threadFetch, NULL);
	_threadFetch = NULL;
	
	pthread_cond_destroy(&_condSignalFetch);
	pthread_mutex_destroy(&_mutexFetchExecute);
	
	pthread_mutex_lock(&_mutexDisplayLinkLists);
	
	while (_displayLinksActiveList.size() > 0)
	{
		DisplayLinksActiveMap::iterator it = _displayLinksActiveList.begin();
		CGDirectDisplayID displayID = it->first;
		CVDisplayLinkRef displayLinkRef = it->second;
		
		if (CVDisplayLinkIsRunning(displayLinkRef))
		{
			CVDisplayLinkStop(displayLinkRef);
		}
		
		CVDisplayLinkRelease(displayLinkRef);
		
		_displayLinksActiveList.erase(displayID);
		_displayLinkFlushTimeList.erase(displayID);
	}
	
	pthread_mutex_unlock(&_mutexDisplayLinkLists);
	pthread_mutex_destroy(&_mutexDisplayLinkLists);
	
	pthread_rwlock_t *currentRWLock = _rwlockOutputList;
	
	if (currentRWLock != NULL)
	{
		pthread_rwlock_wrlock(currentRWLock);
	}
	
	[_cdsOutputList release];
	
	if (currentRWLock != NULL)
	{
		pthread_rwlock_unlock(currentRWLock);
	}
	
	[super dealloc];
}

- (void) semaphoreFramebufferCreate
{
	_taskEmulationLoop = mach_task_self();
	
	for (size_t i = 0; i < MAX_FRAMEBUFFER_PAGES; i++)
	{
		semaphore_create(_taskEmulationLoop, &_semFramebuffer[i], SYNC_POLICY_FIFO, 1);
	}
}

- (void) semaphoreFramebufferDestroy
{
	for (size_t i = MAX_FRAMEBUFFER_PAGES - 1; i < MAX_FRAMEBUFFER_PAGES; i--)
	{
		if (_semFramebuffer[i] != 0)
		{
			semaphore_destroy(_taskEmulationLoop, _semFramebuffer[i]);
			_semFramebuffer[i] = 0;
		}
	}
}

- (u8) selectBufferIndex:(const u8)currentIndex pageCount:(size_t)pageCount
{
	u8 selectedIndex = currentIndex;
	bool stillSearching = true;
	
	// First, search for an idle buffer along with its corresponding semaphore.
	if (stillSearching)
	{
		selectedIndex = (selectedIndex + 1) % pageCount;
		for (; selectedIndex != currentIndex; selectedIndex = (selectedIndex + 1) % pageCount)
		{
			if ([self framebufferStateAtIndex:selectedIndex] == ClientDisplayBufferState_Idle)
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
			if ( ([self framebufferStateAtIndex:selectedIndex] == ClientDisplayBufferState_Idle) ||
				(([self framebufferStateAtIndex:selectedIndex] == ClientDisplayBufferState_Ready) && (selectedIndex != currentIndex)) )
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
			if ([self framebufferStateAtIndex:selectedIndex] == ClientDisplayBufferState_Idle)
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
			if ( ([self framebufferStateAtIndex:selectedIndex] == ClientDisplayBufferState_Idle) ||
				 ([self framebufferStateAtIndex:selectedIndex] == ClientDisplayBufferState_Ready) ||
				 ([self framebufferStateAtIndex:selectedIndex] == ClientDisplayBufferState_Reading) )
			{
				stillSearching = false;
				break;
			}
		}
	}
	
	return selectedIndex;
}

- (semaphore_t) semaphoreFramebufferPageAtIndex:(const u8)bufferIndex
{
	assert(bufferIndex < MAX_FRAMEBUFFER_PAGES);
	return _semFramebuffer[bufferIndex];
}

- (ClientDisplayBufferState) framebufferStateAtIndex:(uint8_t)index
{
	OSSpinLockLock(&_spinlockFramebufferStates[index]);
	const ClientDisplayBufferState bufferState = _framebufferState[index];
	OSSpinLockUnlock(&_spinlockFramebufferStates[index]);
	
	return bufferState;
}

- (void) setFramebufferState:(ClientDisplayBufferState)bufferState index:(uint8_t)index
{
	OSSpinLockLock(&_spinlockFramebufferStates[index]);
	_framebufferState[index] = bufferState;
	OSSpinLockUnlock(&_spinlockFramebufferStates[index]);
}

- (void) setOutputList:(NSMutableArray *)theOutputList rwlock:(pthread_rwlock_t *)theRWLock
{
	pthread_rwlock_t *currentRWLock = _rwlockOutputList;
	
	if (currentRWLock != NULL)
	{
		pthread_rwlock_wrlock(currentRWLock);
	}
	
	[_cdsOutputList release];
	_cdsOutputList = theOutputList;
	[_cdsOutputList retain];
	
	if (currentRWLock != NULL)
	{
		pthread_rwlock_unlock(currentRWLock);
	}
	
	_rwlockOutputList = theRWLock;
}

- (void) incrementViewsUsingDirectToCPUFiltering
{
	OSAtomicIncrement32(&numberViewsUsingDirectToCPUFiltering);
}

- (void) decrementViewsUsingDirectToCPUFiltering
{
	OSAtomicDecrement32(&numberViewsUsingDirectToCPUFiltering);
}

- (void) pushVideoDataToAllDisplayViews
{
	pthread_rwlock_t *currentRWLock = _rwlockOutputList;
	
	if (currentRWLock != NULL)
	{
		pthread_rwlock_rdlock(currentRWLock);
	}
	
	for (CocoaDSOutput *cdsOutput in _cdsOutputList)
	{
		if ([cdsOutput isKindOfClass:[CocoaDSDisplay class]])
		{
			[(CocoaDSDisplay *)cdsOutput signalMessage:MESSAGE_RECEIVE_GPU_FRAME];
		}
	}
	
	if (currentRWLock != NULL)
	{
		pthread_rwlock_unlock(currentRWLock);
	}
}

- (void) flushAllDisplaysOnDisplayLink:(CVDisplayLinkRef)displayLink timeStampNow:(const CVTimeStamp *)timeStampNow timeStampOutput:(const CVTimeStamp *)timeStampOutput
{
	pthread_rwlock_t *currentRWLock = _rwlockOutputList;
	CGDirectDisplayID displayID = CVDisplayLinkGetCurrentCGDisplay(displayLink);
	bool didFlushOccur = false;
	
	std::vector<ClientDisplay3DView *> cdvFlushList;
	
	if (currentRWLock != NULL)
	{
		pthread_rwlock_rdlock(currentRWLock);
	}
	
	for (CocoaDSOutput *cdsOutput in _cdsOutputList)
	{
		if ([cdsOutput isKindOfClass:[CocoaDSDisplayVideo class]])
		{
			if ([(CocoaDSDisplayVideo *)cdsOutput currentDisplayID] == displayID)
			{
				ClientDisplay3DView *cdv = [(CocoaDSDisplayVideo *)cdsOutput clientDisplay3DView];
				
				if (cdv->GetViewNeedsFlush())
				{
					cdvFlushList.push_back(cdv);
				}
			}
		}
	}
	
	const size_t listSize = cdvFlushList.size();
	
	if (listSize > 0)
	{
		[self flushMultipleViews:cdvFlushList timeStampNow:timeStampNow timeStampOutput:timeStampOutput];
		didFlushOccur = true;
	}
	
	if (currentRWLock != NULL)
	{
		pthread_rwlock_unlock(currentRWLock);
	}
	
	if (didFlushOccur)
	{
		// Set the new time limit to 8 seconds after the current time.
		_displayLinkFlushTimeList[displayID] = timeStampNow->videoTime + (timeStampNow->videoTimeScale * VIDEO_FLUSH_TIME_LIMIT_OFFSET);
	}
	else if (timeStampNow->videoTime > _displayLinkFlushTimeList[displayID])
	{
		CVDisplayLinkStop(displayLink);
	}
}

- (void) flushMultipleViews:(const std::vector<ClientDisplay3DView *> &)cdvFlushList timeStampNow:(const CVTimeStamp *)timeStampNow timeStampOutput:(const CVTimeStamp *)timeStampOutput
{
	const size_t listSize = cdvFlushList.size();
	
	for (size_t i = 0; i < listSize; i++)
	{
		ClientDisplay3DView *cdv = (ClientDisplay3DView *)cdvFlushList[i];
		cdv->FlushView(NULL);
	}
	
	for (size_t i = 0; i < listSize; i++)
	{
		ClientDisplay3DView *cdv = (ClientDisplay3DView *)cdvFlushList[i];
		cdv->FinalizeFlush(NULL, timeStampOutput->hostTime);
	}
}

- (void) displayLinkStartUsingID:(CGDirectDisplayID)displayID
{
	CVDisplayLinkRef displayLink = NULL;
	
	pthread_mutex_lock(&_mutexDisplayLinkLists);
	
	if (_displayLinksActiveList.find(displayID) != _displayLinksActiveList.end())
	{
		displayLink = _displayLinksActiveList[displayID];
	}
	
	if ( (displayLink != NULL) && !CVDisplayLinkIsRunning(displayLink) )
	{
		CVDisplayLinkStart(displayLink);
	}
	
	pthread_mutex_unlock(&_mutexDisplayLinkLists);
}

- (void) displayLinkListUpdate
{
	// Set up the display links
	NSArray *screenList = [NSScreen screens];
	std::set<CGDirectDisplayID> screenActiveDisplayIDsList;
	
	pthread_mutex_lock(&_mutexDisplayLinkLists);
	
	// Add new CGDirectDisplayIDs for new screens
	for (size_t i = 0; i < [screenList count]; i++)
	{
		NSScreen *screen = [screenList objectAtIndex:i];
		NSDictionary *deviceDescription = [screen deviceDescription];
		NSNumber *idNumber = (NSNumber *)[deviceDescription valueForKey:@"NSScreenNumber"];
		
		CGDirectDisplayID displayID = [idNumber unsignedIntValue];
		bool isDisplayLinkStillActive = (_displayLinksActiveList.find(displayID) != _displayLinksActiveList.end());
		
		if (!isDisplayLinkStillActive)
		{
			CVDisplayLinkRef newDisplayLink;
			CVDisplayLinkCreateWithCGDisplay(displayID, &newDisplayLink);
			CVDisplayLinkSetOutputCallback(newDisplayLink, &MacDisplayLinkCallback, self);
			
			_displayLinksActiveList[displayID] = newDisplayLink;
			_displayLinkFlushTimeList[displayID] = 0;
		}
		
		// While we're iterating through NSScreens, save the CGDirectDisplayID to a temporary list for later use.
		screenActiveDisplayIDsList.insert(displayID);
	}
	
	// Remove old CGDirectDisplayIDs for screens that no longer exist
	for (DisplayLinksActiveMap::iterator it = _displayLinksActiveList.begin(); it != _displayLinksActiveList.end(); )
	{
		CGDirectDisplayID displayID = it->first;
		CVDisplayLinkRef displayLinkRef = it->second;
		
		if (screenActiveDisplayIDsList.find(displayID) == screenActiveDisplayIDsList.end())
		{
			if (CVDisplayLinkIsRunning(displayLinkRef))
			{
				CVDisplayLinkStop(displayLinkRef);
			}
			
			CVDisplayLinkRelease(displayLinkRef);
			
			_displayLinksActiveList.erase(displayID);
			_displayLinkFlushTimeList.erase(displayID);
			
			if (_displayLinksActiveList.empty())
			{
				break;
			}
			else
			{
				it = _displayLinksActiveList.begin();
				continue;
			}
		}
		
		++it;
	}
	
	pthread_mutex_unlock(&_mutexDisplayLinkLists);
}

- (void) fetchSynchronousAtIndex:(uint8_t)index
{
	GPUFetchObject->FetchFromBufferIndex(index);
}

- (void) signalFetchAtIndex:(uint8_t)index message:(int32_t)messageID
{
	pthread_mutex_lock(&_mutexFetchExecute);
	
	_fetchIndex = index;
	_threadMessageID = messageID;
	pthread_cond_signal(&_condSignalFetch);
	
	pthread_mutex_unlock(&_mutexFetchExecute);
}

- (void) runFetchLoop
{
	pthread_mutex_lock(&_mutexFetchExecute);
	
	do
	{
		while (_threadMessageID == MESSAGE_NONE)
		{
			pthread_cond_wait(&_condSignalFetch, &_mutexFetchExecute);
		}
		
		GPUFetchObject->FetchFromBufferIndex(_fetchIndex);
		[self pushVideoDataToAllDisplayViews];
		_threadMessageID = MESSAGE_NONE;
		
	} while(true);
}

- (void) respondToScreenChange:(NSNotification *)aNotification
{
	[self displayLinkListUpdate];
}

@end

#endif

#pragma mark -

GPUEventHandlerOSX::GPUEventHandlerOSX()
{
	_fetchObject = nil;
	_render3DNeedsFinish = false;
	pthread_mutex_init(&_mutexFrame, NULL);
	pthread_mutex_init(&_mutex3DRender, NULL);
	pthread_mutex_init(&_mutexApplyGPUSettings, NULL);
	pthread_mutex_init(&_mutexApplyRender3DSettings, NULL);
}

GPUEventHandlerOSX::~GPUEventHandlerOSX()
{
	if (this->_render3DNeedsFinish)
	{
		pthread_mutex_unlock(&this->_mutex3DRender);
	}
	
	pthread_mutex_destroy(&this->_mutexFrame);
	pthread_mutex_destroy(&this->_mutex3DRender);
	pthread_mutex_destroy(&this->_mutexApplyGPUSettings);
	pthread_mutex_destroy(&this->_mutexApplyRender3DSettings);
}

GPUClientFetchObject* GPUEventHandlerOSX::GetFetchObject() const
{
	return this->_fetchObject;
}

void GPUEventHandlerOSX::SetFetchObject(GPUClientFetchObject *fetchObject)
{
	this->_fetchObject = fetchObject;
}

void GPUEventHandlerOSX::DidFrameBegin(const size_t line, const bool isFrameSkipRequested, const size_t pageCount, u8 &selectedBufferIndexInOut)
{
	this->FramebufferLock();
	
#ifdef ENABLE_SHARED_FETCH_OBJECT
	if (!isFrameSkipRequested)
	{
		MacClientSharedObject *sharedViewObject = (MacClientSharedObject *)this->_fetchObject->GetClientData();
		
		if ( (pageCount > 1) && (line == 0) )
		{
			selectedBufferIndexInOut = [sharedViewObject selectBufferIndex:selectedBufferIndexInOut pageCount:pageCount];
		}
		
		semaphore_wait([sharedViewObject semaphoreFramebufferPageAtIndex:selectedBufferIndexInOut]);
		[sharedViewObject setFramebufferState:ClientDisplayBufferState_Writing index:selectedBufferIndexInOut];
	}
#endif
}

void GPUEventHandlerOSX::DidFrameEnd(bool isFrameSkipped, const NDSDisplayInfo &latestDisplayInfo)
{
#ifdef ENABLE_SHARED_FETCH_OBJECT
	MacClientSharedObject *sharedViewObject = (MacClientSharedObject *)this->_fetchObject->GetClientData();
	if (!isFrameSkipped)
	{
		this->_fetchObject->SetFetchDisplayInfo(latestDisplayInfo);
		[sharedViewObject setFramebufferState:ClientDisplayBufferState_Ready index:latestDisplayInfo.bufferIndex];
		semaphore_signal([sharedViewObject semaphoreFramebufferPageAtIndex:latestDisplayInfo.bufferIndex]);
	}
#endif
	
	this->FramebufferUnlock();
	
#ifdef ENABLE_SHARED_FETCH_OBJECT
	if (!isFrameSkipped)
	{
		[sharedViewObject signalFetchAtIndex:latestDisplayInfo.bufferIndex message:MESSAGE_FETCH_AND_PUSH_VIDEO];
	}
#endif
}

void GPUEventHandlerOSX::DidRender3DBegin()
{
	this->Render3DLock();
	this->_render3DNeedsFinish = true;
}

void GPUEventHandlerOSX::DidRender3DEnd()
{
	this->_render3DNeedsFinish = false;
	this->Render3DUnlock();
}

void GPUEventHandlerOSX::DidApplyGPUSettingsBegin()
{
	this->ApplyGPUSettingsLock();
}

void GPUEventHandlerOSX::DidApplyGPUSettingsEnd()
{
	this->ApplyGPUSettingsUnlock();
}

void GPUEventHandlerOSX::DidApplyRender3DSettingsBegin()
{
	this->ApplyRender3DSettingsLock();
}

void GPUEventHandlerOSX::DidApplyRender3DSettingsEnd()
{
	this->ApplyRender3DSettingsUnlock();
}

void GPUEventHandlerOSX::FramebufferLock()
{
	pthread_mutex_lock(&this->_mutexFrame);
}

void GPUEventHandlerOSX::FramebufferUnlock()
{
	pthread_mutex_unlock(&this->_mutexFrame);
}

void GPUEventHandlerOSX::Render3DLock()
{
	pthread_mutex_lock(&this->_mutex3DRender);
}

void GPUEventHandlerOSX::Render3DUnlock()
{
	pthread_mutex_unlock(&this->_mutex3DRender);
}

void GPUEventHandlerOSX::ApplyGPUSettingsLock()
{
	pthread_mutex_lock(&this->_mutexApplyGPUSettings);
}

void GPUEventHandlerOSX::ApplyGPUSettingsUnlock()
{
	pthread_mutex_unlock(&this->_mutexApplyGPUSettings);
}

void GPUEventHandlerOSX::ApplyRender3DSettingsLock()
{
	pthread_mutex_lock(&this->_mutexApplyRender3DSettings);
}

void GPUEventHandlerOSX::ApplyRender3DSettingsUnlock()
{
	pthread_mutex_unlock(&this->_mutexApplyRender3DSettings);
}

bool GPUEventHandlerOSX::GetRender3DNeedsFinish()
{
	return this->_render3DNeedsFinish;
}

#pragma mark -

CGLContextObj OSXOpenGLRendererContext = NULL;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
CGLPBufferObj OSXOpenGLRendererPBuffer = NULL;
#pragma GCC diagnostic pop

#ifdef ENABLE_SHARED_FETCH_OBJECT

static void* RunFetchThread(void *arg)
{
	MacClientSharedObject *sharedData = (MacClientSharedObject *)arg;
	[sharedData runFetchLoop];
	
	return NULL;
}

CVReturn MacDisplayLinkCallback(CVDisplayLinkRef displayLink,
								const CVTimeStamp *inNow,
								const CVTimeStamp *inOutputTime,
								CVOptionFlags flagsIn,
								CVOptionFlags *flagsOut,
								void *displayLinkContext)
{
	MacClientSharedObject *sharedData = (MacClientSharedObject *)displayLinkContext;
	[sharedData flushAllDisplaysOnDisplayLink:displayLink timeStampNow:inNow timeStampOutput:inOutputTime];
	
	return kCVReturnSuccess;
}

#endif

bool OSXOpenGLRendererInit()
{
	bool isContextCreated = (OSXOpenGLRendererContext != NULL);
	if (!isContextCreated)
	{
		isContextCreated = CreateOpenGLRenderer();
	}
	
	return isContextCreated;
}

bool OSXOpenGLRendererBegin()
{
	CGLSetCurrentContext(OSXOpenGLRendererContext);
	
	return true;
}

void OSXOpenGLRendererEnd()
{
	
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

bool OSXOpenGLRendererFramebufferDidResize(const bool isFBOSupported, size_t w, size_t h)
{
	bool result = false;
	
	if (isFBOSupported)
	{
		result = true;
		return result;
	}
	
	if (IsOSXVersionSupported(10, 13, 0))
	{
		printf("Mac OpenGL: P-Buffers cannot be created on macOS v10.13 High Sierra and later.\n");
		return result;
	}
	
	// Create a PBuffer if FBOs are not supported.
	CGLPBufferObj newPBuffer = NULL;
	CGLError error = CGLCreatePBuffer(w, h, GL_TEXTURE_2D, GL_RGBA, 0, &newPBuffer);
	
	if ( (newPBuffer == NULL) || (error != kCGLNoError) )
	{
		printf("Mac OpenGL: ERROR - Could not create the P-Buffer: %s\n", CGLErrorString(error));
		return result;
	}
	else
	{
		GLint virtualScreenID = 0;
		CGLGetVirtualScreen(OSXOpenGLRendererContext, &virtualScreenID);
		CGLSetPBuffer(OSXOpenGLRendererContext, newPBuffer, 0, 0, virtualScreenID);
	}
	
	CGLPBufferObj oldPBuffer = OSXOpenGLRendererPBuffer;
	OSXOpenGLRendererPBuffer = newPBuffer;
	CGLReleasePBuffer(oldPBuffer);
	
	result = true;
	return result;
}

#pragma GCC diagnostic pop

bool CreateOpenGLRenderer()
{
	bool result = false;
	bool useContext_3_2 = false;
	CGLPixelFormatObj cglPixFormat = NULL;
	CGLContextObj newContext = NULL;
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
	
	RequestOpenGLRenderer_3_2(useContext_3_2);
	OSXOpenGLRendererContext = newContext;
	
	result = true;
	return result;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

void DestroyOpenGLRenderer()
{
	if (OSXOpenGLRendererContext == NULL)
	{
		return;
	}
	
	CGLReleasePBuffer(OSXOpenGLRendererPBuffer);
	OSXOpenGLRendererPBuffer = NULL;
	
	CGLReleaseContext(OSXOpenGLRendererContext);
	OSXOpenGLRendererContext = NULL;
}

#pragma GCC diagnostic pop

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
								void (*endOGLFunction)(),
								bool (*resizeOGLFunction)(const bool isFBOSupported, size_t w, size_t h))
{
	oglrender_init = initFunction;
	oglrender_beginOpenGL = beginOGLFunction;
	oglrender_endOpenGL = endOGLFunction;
	oglrender_framebufferDidResizeCallback = resizeOGLFunction;
}
