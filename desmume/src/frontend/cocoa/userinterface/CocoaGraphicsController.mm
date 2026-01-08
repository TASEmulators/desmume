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

#import "CocoaGraphicsController.h"

#import "cocoa_globals.h"

#include "cgl_3Demu.h"
#include "../../rasterize.h"

#ifdef MAC_OS_X_VERSION_10_7
	#include "../../OGLRender_3_2.h"
#else
	#include "../../OGLRender.h"
#endif

#include "utilities.h"

#import "CocoaDisplayView.h"
#import "MacOGLDisplayView.h"

#ifdef ENABLE_APPLE_METAL
	#import "MacMetalDisplayView.h"
#endif

#ifdef BOOL
#undef BOOL
#endif


MacGraphicsControl::MacGraphicsControl(bool useMetalGraphics)
{
	oglrender_init        = &cgl_initOpenGL_StandardAuto;
	oglrender_deinit      = &cgl_deinitOpenGL;
	oglrender_beginOpenGL = &cgl_beginOpenGL;
	oglrender_endOpenGL   = &cgl_endOpenGL;
	oglrender_framebufferDidResizeCallback = &cgl_framebufferDidResizeCallback;
	
#ifdef OGLRENDER_3_2_H
	OGLLoadEntryPoints_3_2_Func = &OGLLoadEntryPoints_3_2;
	OGLCreateRenderer_3_2_Func = &OGLCreateRenderer_3_2;
#endif
	
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
		_multisampleMaxClientSize = (uint8_t)maxSamplesOGL;
		
		cgl_endOpenGL();
		cgl_deinitOpenGL();
	}
	
	_fetchObject = NULL;
	
#ifdef ENABLE_APPLE_METAL
	if (useMetalGraphics)
	{
		_fetchObject = new MacMetalFetchObject;
		
		if (_fetchObject->GetClientData() == nil)
		{
			delete _fetchObject;
			_fetchObject = NULL;
		}
		else
		{
			_pageCountPending = METAL_FETCH_BUFFER_COUNT;
			GPU->SetWillPostprocessDisplays(false);
		}
	}
#endif
	
	if (_fetchObject == NULL)
	{
		_fetchObject = new MacOGLClientFetchObject;
		_pageCountPending = OPENGL_FETCH_BUFFER_COUNT;
		GPU->SetWillPostprocessDisplays(false);
	}
	
	_fetchObject->Init();
	
	GPU->SetWillAutoResolveToCustomBuffer(false);
	GPU->SetEventHandler(this);
}

MacGraphicsControl::~MacGraphicsControl()
{
	GPU->SetEventHandler(NULL);
	
	delete this->_fetchObject;
	this->_fetchObject = NULL;
}

void MacGraphicsControl::_UpdateEngineClientProperties()
{
	switch (this->_engineIDApplied)
	{
		case RENDERID_NULL:
		case RENDERID_SOFTRASTERIZER:
			this->_engineClientIDApplied   = this->_engineIDApplied;
			this->_engineClientNameApplied = CurrentRenderer->GetName();
			break;
			
		case RENDERID_OPENGL_AUTO:
		case RENDERID_OPENGL_LEGACY:
		case RENDERID_OPENGL_3_2:
			this->_engineClientIDApplied   = cgl_getHostRendererID();
			this->_engineClientNameApplied = cgl_getHostRendererString();
			break;
	}
}

void MacGraphicsControl::Set3DEngineByID(int engineID)
{
	slock_lock(this->_mutexApplyRender3DSettings);
	
	switch (engineID)
	{
		case RENDERID_NULL:
			this->_3DEngineCallbackStruct = gpu3DNull;
			break;
			
		case RENDERID_SOFTRASTERIZER:
			this->_3DEngineCallbackStruct = gpu3DRasterize;
			break;
			
		case RENDERID_OPENGL_AUTO:
			oglrender_init = &cgl_initOpenGL_StandardAuto;
			this->_3DEngineCallbackStruct = gpu3Dgl;
			break;
			
		case RENDERID_OPENGL_LEGACY:
			oglrender_init = &cgl_initOpenGL_LegacyAuto;
			this->_3DEngineCallbackStruct = gpu3DglOld;
			break;
			
		case RENDERID_OPENGL_3_2:
			oglrender_init = &cgl_initOpenGL_3_2_CoreProfile;
			this->_3DEngineCallbackStruct = gpu3Dgl_3_2;
			break;
			
		default:
			puts("DeSmuME: Invalid 3D renderer chosen; falling back to SoftRasterizer.");
			engineID = RENDERID_SOFTRASTERIZER;
			this->_3DEngineCallbackStruct = gpu3DRasterize;
			break;
	}
	
	this->_engineIDPending = engineID;
	slock_unlock(this->_mutexApplyRender3DSettings);
}

void MacGraphicsControl::SetEngine3DClientIndex(int clientListIndex)
{
	if (clientListIndex < CORE3DLIST_NULL)
	{
		clientListIndex = CORE3DLIST_NULL;
	}
	
	int engineID = RENDERID_NULL;
	
	switch (clientListIndex)
	{
		case CORE3DLIST_NULL:
			engineID = RENDERID_NULL;
			break;
			
		case CORE3DLIST_SWRASTERIZE:
			engineID = RENDERID_SOFTRASTERIZER;
			break;
			
		case CORE3DLIST_OPENGL:
			engineID = RENDERID_OPENGL_AUTO;
			break;
			
		case RENDERID_OPENGL_LEGACY:
			engineID = RENDERID_OPENGL_LEGACY;
			break;
			
		case RENDERID_OPENGL_3_2:
			engineID = RENDERID_OPENGL_3_2;
			break;
			
		default:
			puts("DeSmuME: Invalid 3D renderer chosen; falling back to SoftRasterizer.");
			engineID = RENDERID_NULL;
			break;
	}
	
	this->Set3DEngineByID(engineID);
}

int MacGraphicsControl::GetEngine3DClientIndex()
{
	int clientListIndex = CORE3DLIST_NULL;
	int engineID = this->Get3DEngineByID();
	
	switch (engineID)
	{
		case RENDERID_NULL:
			clientListIndex = CORE3DLIST_NULL;
			break;
			
		case RENDERID_SOFTRASTERIZER:
			clientListIndex = CORE3DLIST_SWRASTERIZE;
			break;
			
		case RENDERID_OPENGL_AUTO:
			clientListIndex = CORE3DLIST_OPENGL;
			break;
			
		case RENDERID_OPENGL_LEGACY:
			clientListIndex = RENDERID_OPENGL_LEGACY;
			break;
			
		case RENDERID_OPENGL_3_2:
			clientListIndex = RENDERID_OPENGL_3_2;
			break;
			
		default:
			break;
	}
	
	return clientListIndex;
}

void MacGraphicsControl::DidFrameBegin(const size_t line, const bool isFrameSkipRequested, const size_t pageCount, u8 &selectedBufferIndexInOut)
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

void MacGraphicsControl::DidFrameEnd(bool isFrameSkipped, const NDSDisplayInfo &latestDisplayInfo)
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

void MacGraphicsControl::ClearWithColor(const uint16_t colorBGRA5551)
{
	MacGPUFetchObjectAsync *asyncFetchObj = (MacGPUFetchObjectAsync *)this->_fetchObject;
	
	const size_t maxPages = GPU->GetDisplayInfo().framebufferPageCount;
	for (size_t i = 0; i < maxPages; i++)
	{
		semaphore_wait( asyncFetchObj->SemaphoreFramebufferPageAtIndex(i) );
	}
	
	GPU->ClearWithColor(colorBGRA5551);
	
	for (size_t i = maxPages - 1; i < maxPages; i--)
	{
		semaphore_signal( asyncFetchObj->SemaphoreFramebufferPageAtIndex(i) );
	}
	
	const u8 bufferIndex = GPU->GetDisplayInfo().bufferIndex;
	asyncFetchObj->SignalFetchAtIndex(bufferIndex, MESSAGE_FETCH_AND_PERFORM_ACTIONS);
}

void MacGraphicsControl::DidApplyGPUSettingsBegin()
{
	if (this->_didWidthChange || this->_didHeightChange || this->_didColorFormatChange || this->_didPageCountChange)
	{
		MacGPUFetchObjectAsync *asyncFetchObj = (MacGPUFetchObjectAsync *)this->_fetchObject;
		asyncFetchObj->SetPauseState(true);
	}
	
	this->ClientGraphicsControl::DidApplyGPUSettingsBegin();
}

void MacGraphicsControl::DidApplyGPUSettingsEnd()
{
	if (this->_didWidthChange || this->_didHeightChange || this->_didColorFormatChange || this->_didPageCountChange)
	{
		const NDSDisplayInfo &displayInfo = GPU->GetDisplayInfo();
		MacGPUFetchObjectAsync *asyncFetchObj = (MacGPUFetchObjectAsync *)this->_fetchObject;
		
		const size_t maxPages = displayInfo.framebufferPageCount;
		for (size_t i = 0; i < maxPages; i++)
		{
			semaphore_wait( asyncFetchObj->SemaphoreFramebufferPageAtIndex(i) );
		}
		
		this->_fetchObject->SetFetchBuffers(displayInfo);

		for (size_t i = maxPages - 1; i < maxPages; i--)
		{
			semaphore_signal( asyncFetchObj->SemaphoreFramebufferPageAtIndex(i) );
		}
		
		asyncFetchObj->SetPauseState(false);
	}
	
	this->ClientGraphicsControl::DidApplyGPUSettingsEnd();
}

@implementation CocoaGraphicsController

@dynamic gpuStateFlags;
@dynamic gpu2DSettings;
@dynamic gpuScale;
@dynamic gpuColorFormat;

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

@dynamic engine3DClientIndex;
@dynamic engine3DInternalIDApplied;
@dynamic engine3DClientIDApplied;
@dynamic _engine3DClientName;
@dynamic highPrecisionColorInterpolationEnable;
@dynamic edgeMarkEnable;
@dynamic fogEnable;
@dynamic textureEnable;
@dynamic render3DThreads;
@dynamic lineHackEnable;
@dynamic maxMultisampleSize;
@dynamic multisampleSize;
@dynamic _msaaSizeString;
@dynamic textureDeposterizeEnable;
@dynamic textureSmoothingEnable;
@dynamic textureScalingFactor;
@dynamic fragmentSamplingHackEnable;
@dynamic shadowPolygonEnable;
@dynamic specialZeroAlphaBlendingEnable;
@dynamic ndsDepthCalculationEnable;
@dynamic depthLEqualPolygonFacingEnable;
@dynamic fetchObject;

- (id) initWithGraphicsControl:(MacGraphicsControl *)graphicsControl
{
	if (graphicsControl == NULL)
	{
		return nil;
	}
	
	self = [super init];
	if (self == nil)
	{
		return self;
	}
	
	_graphicsControl = graphicsControl;
	_engine3DClientName = [[NSString stringWithCString:_graphicsControl->Get3DEngineClientName() encoding:NSUTF8StringEncoding] retain];
	_msaaSizeString = [[NSString stringWithCString:_graphicsControl->Get3DMultisampleSizeString() encoding:NSUTF8StringEncoding] retain];
	
	gpuStateFlags = GPUSTATE_MAIN_GPU_MASK | GPUSTATE_MAIN_BG0_MASK | GPUSTATE_MAIN_BG1_MASK | GPUSTATE_MAIN_BG2_MASK | GPUSTATE_MAIN_BG3_MASK | GPUSTATE_MAIN_OBJ_MASK | GPUSTATE_SUB_GPU_MASK | GPUSTATE_SUB_BG0_MASK | GPUSTATE_SUB_BG1_MASK | GPUSTATE_SUB_BG2_MASK | GPUSTATE_SUB_BG3_MASK | GPUSTATE_SUB_OBJ_MASK;
	
	_graphicsControl->SetClientObject(self);
	
	return self;
}

- (void)dealloc
{
	[_msaaSizeString release];
	_msaaSizeString = nil;
	
	[super dealloc];
}

#pragma mark Class Methods
- (BOOL) gpuStateByBit:(const UInt32)stateBit
{
	return ([self gpuStateFlags] & (1 << stateBit)) ? YES : NO;
}

- (void) clearWithColor:(const uint16_t)colorBGRA5551
{
	_graphicsControl->ClearWithColor(colorBGRA5551);
}

#pragma mark Properties
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

- (void) setGpu2DSettings:(Client2DGraphicsSettings)settings2D
{
	_graphicsControl->Set2DGraphicsSettings(settings2D);
}

- (Client2DGraphicsSettings) gpu2DSettings
{
	return _graphicsControl->Get2DGraphicsSettings();
}

- (void) setGpuScale:(NSUInteger)theScale
{
	_graphicsControl->SetFramebufferDimensionsByScaleFactorInteger((size_t)theScale);
}

- (NSUInteger) gpuScale
{
	return (NSUInteger)_graphicsControl->GetFramebufferDimensionsByScaleFactorInteger();
}

- (void) setGpuColorFormat:(NSUInteger)theColorFormat
{
	_graphicsControl->SetColorFormat((NDSColorFormat)theColorFormat);
}

- (NSUInteger) gpuColorFormat
{
	return (NSUInteger)_graphicsControl->GetColorFormat();
}

- (void) setLayerMainGPU:(BOOL)gpuState
{
	_graphicsControl->SetMainEngineEnable((gpuState) ? true : false);
}

- (BOOL) layerMainGPU
{
	return _graphicsControl->GetMainEngineEnable() ? YES : NO;
}

- (void) setLayerMainBG0:(BOOL)layerState
{
	_graphicsControl->Set2DLayerEnableMainBG0((layerState) ? true : false);
}

- (BOOL) layerMainBG0
{
	return _graphicsControl->Get2DLayerEnableMainBG0() ? YES : NO;
}

- (void) setLayerMainBG1:(BOOL)layerState
{
	_graphicsControl->Set2DLayerEnableMainBG1((layerState) ? true : false);
}

- (BOOL) layerMainBG1
{
	return _graphicsControl->Get2DLayerEnableMainBG1() ? YES : NO;
}

- (void) setLayerMainBG2:(BOOL)layerState
{
	_graphicsControl->Set2DLayerEnableMainBG2((layerState) ? true : false);
}

- (BOOL) layerMainBG2
{
	return _graphicsControl->Get2DLayerEnableMainBG2() ? YES : NO;
}

- (void) setLayerMainBG3:(BOOL)layerState
{
	_graphicsControl->Set2DLayerEnableMainBG3((layerState) ? true : false);
}

- (BOOL) layerMainBG3
{
	return _graphicsControl->Get2DLayerEnableMainBG3() ? YES : NO;
}

- (void) setLayerMainOBJ:(BOOL)layerState
{
	_graphicsControl->Set2DLayerEnableMainOBJ((layerState) ? true : false);
}

- (BOOL) layerMainOBJ
{
	return _graphicsControl->Get2DLayerEnableMainOBJ() ? YES : NO;
}

- (void) setLayerSubGPU:(BOOL)gpuState
{
	_graphicsControl->SetSubEngineEnable((gpuState) ? true : false);
}

- (BOOL) layerSubGPU
{
	return _graphicsControl->GetSubEngineEnable() ? YES : NO;
}

- (void) setLayerSubBG0:(BOOL)layerState
{
	_graphicsControl->Set2DLayerEnableSubBG0((layerState) ? true : false);
}

- (BOOL) layerSubBG0
{
	return _graphicsControl->Get2DLayerEnableSubBG0() ? YES : NO;
}

- (void) setLayerSubBG1:(BOOL)layerState
{
	_graphicsControl->Set2DLayerEnableSubBG1((layerState) ? true : false);
}

- (BOOL) layerSubBG1
{
	return _graphicsControl->Get2DLayerEnableSubBG1() ? YES : NO;
}

- (void) setLayerSubBG2:(BOOL)layerState
{
	_graphicsControl->Set2DLayerEnableSubBG2((layerState) ? true : false);
}

- (BOOL) layerSubBG2
{
	return _graphicsControl->Get2DLayerEnableSubBG2() ? YES : NO;
}

- (void) setLayerSubBG3:(BOOL)layerState
{
	_graphicsControl->Set2DLayerEnableSubBG3((layerState) ? true : false);
}

- (BOOL) layerSubBG3
{
	return _graphicsControl->Get2DLayerEnableSubBG3() ? YES : NO;
}

- (void) setLayerSubOBJ:(BOOL)layerState
{
	_graphicsControl->Set2DLayerEnableSubOBJ((layerState) ? true : false);
}

- (BOOL) layerSubOBJ
{
	return _graphicsControl->Get2DLayerEnableSubOBJ() ? YES : NO;
}

- (void) setEngine3DClientIndex:(NSInteger)clientListIndex
{
	_graphicsControl->SetEngine3DClientIndex((int)clientListIndex);
}

- (NSInteger) engine3DClientIndex
{
	return (NSInteger)_graphicsControl->GetEngine3DClientIndex();
}

- (NSInteger) engine3DInternalIDApplied
{
	return (NSInteger)_graphicsControl->Get3DEngineByIDApplied();
}

- (NSInteger) engine3DClientIDApplied
{
	return (NSInteger)_graphicsControl->Get3DEngineClientID();
}

- (NSString *) engine3DClientNameApplied
{
	NSString *currString = [NSString stringWithCString:_graphicsControl->Get3DEngineClientName() encoding:NSUTF8StringEncoding];
	if (![_engine3DClientName isEqualToString:currString])
	{
		[_engine3DClientName release];
		_engine3DClientName = [currString retain];
	}
	
	return _engine3DClientName;
}

- (void) setHighPrecisionColorInterpolationEnable:(BOOL)state
{
	_graphicsControl->Set3DHighPrecisionColorInterpolation((state) ? true : false);
}

- (BOOL) highPrecisionColorInterpolationEnable
{
	return _graphicsControl->Get3DHighPrecisionColorInterpolation() ? YES : NO;
}

- (void) setEdgeMarkEnable:(BOOL)state
{
	_graphicsControl->Set3DEdgeMarkEnable((state) ? true : false);
}

- (BOOL) edgeMarkEnable
{
	return _graphicsControl->Get3DEdgeMarkEnable() ? YES : NO;
}

- (void) setFogEnable:(BOOL)state
{
	_graphicsControl->Set3DFogEnable((state) ? true : false);
}

- (BOOL) fogEnable
{
	return _graphicsControl->Get3DFogEnable() ? YES : NO;
}

- (void) setTextureEnable:(BOOL)state
{
	_graphicsControl->Set3DTextureEnable((state) ? true : false);
}

- (BOOL) textureEnable
{
	return _graphicsControl->Get3DTextureEnable() ? YES : NO;
}

- (void) setRender3DThreads:(NSUInteger)numberThreads
{
	// MacGraphicsControl caps this to 63.
	if (numberThreads > 63)
	{
		numberThreads = 63;
	}
	
	_graphicsControl->Set3DRenderingThreadCount((uint8_t)numberThreads);
}

- (NSUInteger) render3DThreads
{
	return (NSUInteger)_graphicsControl->Get3DRenderingThreadCount();
}

- (void) setLineHackEnable:(BOOL)state
{
	_graphicsControl->Set3DLineHackEnable((state) ? true : false);
}

- (BOOL) lineHackEnable
{
	return _graphicsControl->Get3DLineHackEnable() ? YES : NO;
}

- (NSUInteger) maxMultisampleSize
{
	return (NSUInteger)_graphicsControl->Get3DMultisampleMaxClientSize();
}

- (void) setMultisampleSize:(NSUInteger)msaaSize
{
	[self willChangeValueForKey:@"_msaaSizeString"];
	_graphicsControl->Set3DMultisampleSize((uint8_t)msaaSize);
	[self didChangeValueForKey:@"_msaaSizeString"];
}

- (NSUInteger) multisampleSize
{
	return (NSUInteger)_graphicsControl->Get3DMultisampleSize();
}

- (NSString *) multisampleSizeString
{
	NSString *currString = [NSString stringWithCString:_graphicsControl->Get3DMultisampleSizeString() encoding:NSUTF8StringEncoding];
	if (![_msaaSizeString isEqualToString:currString])
	{
		[_msaaSizeString release];
		_msaaSizeString = [currString retain];
	}
	
	return _msaaSizeString;
}

- (void) setTextureDeposterizeEnable:(BOOL)state
{
	_graphicsControl->Set3DTextureDeposterize((state) ? true : false);
}

- (BOOL) textureDeposterizeEnable
{
	return _graphicsControl->Get3DTextureDeposterize() ? YES : NO;
}

- (void) setTextureSmoothingEnable:(BOOL)state
{
	_graphicsControl->Set3DTextureSmoothingEnable((state) ? true : false);
}

- (BOOL) textureSmoothingEnable
{
	return _graphicsControl->Get3DTextureSmoothingEnable() ? YES : NO;
}

- (void) setTextureScalingFactor:(NSUInteger)scalingFactor
{
	_graphicsControl->Set3DTextureScalingFactor((uint8_t)scalingFactor);
}

- (NSUInteger) textureScalingFactor
{
	return (NSUInteger)_graphicsControl->Get3DTextureScalingFactor();
}

- (void) setFragmentSamplingHackEnable:(BOOL)state
{
	_graphicsControl->Set3DFragmentSamplingHackEnable((state) ? true : false);
}

- (BOOL) fragmentSamplingHackEnable
{
	return _graphicsControl->Get3DFragmentSamplingHackEnable() ? YES : NO;
}

- (void) setShadowPolygonEnable:(BOOL)state
{
	_graphicsControl->Set3DShadowPolygonEnable((state) ? true : false);
}

- (BOOL) shadowPolygonEnable
{
	return _graphicsControl->Get3DShadowPolygonEnable() ? YES : NO;
}

- (void) setSpecialZeroAlphaBlendingEnable:(BOOL)state
{
	_graphicsControl->Set3DSpecialZeroAlphaBlendingEnable((state) ? true : false);
}

- (BOOL) specialZeroAlphaBlendingEnable
{
	return _graphicsControl->Get3DSpecialZeroAlphaBlendingEnable() ? YES : NO;
}

- (void) setNdsDepthCalculationEnable:(BOOL)state
{
	_graphicsControl->Set3DNDSDepthCalculationEnable((state) ? true : false);
}

- (BOOL) ndsDepthCalculationEnable
{
	return _graphicsControl->Get3DNDSDepthCalculationEnable() ? YES : NO;
}

- (void) setDepthLEqualPolygonFacingEnable:(BOOL)state
{
	_graphicsControl->Set3DDepthLEqualPolygonFacingEnable((state) ? true : false);
}

- (BOOL) depthLEqualPolygonFacingEnable
{
	return _graphicsControl->Get3DDepthLEqualPolygonFacingEnable() ? YES : NO;
}

- (GPUClientFetchObject *) fetchObject
{
	return _graphicsControl->GetFetchObject();
}

@end
