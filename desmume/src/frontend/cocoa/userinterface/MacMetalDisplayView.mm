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

#include "MacMetalDisplayView.h"
#include "../cocoa_globals.h"

#include "../../../common.h"

@implementation MetalDisplayViewSharedData

@synthesize device;
@synthesize preferredResourceStorageMode;
@synthesize commandQueue;
@synthesize defaultLibrary;

@synthesize deposterizePipeline;
@synthesize hudPipeline;
@synthesize hudRGBAPipeline;
@synthesize samplerHUDBox;
@synthesize samplerHUDText;

@synthesize hudIndexBuffer;

@synthesize texFetchMain;
@synthesize texFetchTouch;

@synthesize texLQ2xLUT;
@synthesize texHQ2xLUT;
@synthesize texHQ3xLUT;
@synthesize texHQ4xLUT;
@synthesize texCurrentHQnxLUT;

@synthesize deposterizeThreadsPerGroup;
@synthesize deposterizeThreadGroupsPerGrid;

- (id)init
{
	self = [super init];
	if (self == nil)
	{
		return nil;
	}
	
	device = MTLCreateSystemDefaultDevice();
	
	if (device == nil)
	{
		[self release];
		return nil;
	}
	
	[device retain];
	
	commandQueue = [device newCommandQueue];
	defaultLibrary = [device newDefaultLibrary];
	_fetch555Pipeline = [[device newComputePipelineStateWithFunction:[defaultLibrary newFunctionWithName:@"nds_fetch555"] error:nil] retain];
	_fetch666Pipeline = [[device newComputePipelineStateWithFunction:[defaultLibrary newFunctionWithName:@"nds_fetch666"] error:nil] retain];
	_fetch888Pipeline = [[device newComputePipelineStateWithFunction:[defaultLibrary newFunctionWithName:@"nds_fetch888"] error:nil] retain];
	_fetch555ConvertOnlyPipeline = [[device newComputePipelineStateWithFunction:[defaultLibrary newFunctionWithName:@"nds_fetch555ConvertOnly"] error:nil] retain];
	_fetch666ConvertOnlyPipeline = [[device newComputePipelineStateWithFunction:[defaultLibrary newFunctionWithName:@"nds_fetch666ConvertOnly"] error:nil] retain];
	deposterizePipeline = [[device newComputePipelineStateWithFunction:[defaultLibrary newFunctionWithName:@"src_filter_deposterize"] error:nil] retain];
	
	if ( IsOSXVersion(10, 13, 0) || IsOSXVersion(10, 13, 1) || IsOSXVersion(10, 13, 2) || IsOSXVersion(10, 13, 3) || IsOSXVersion(10, 13, 4) )
	{
		// On macOS High Sierra, there is currently a bug with newBufferWithBytesNoCopy:length:options:deallocator
		// that causes it to crash with MTLResourceStorageModeManaged. So for these macOS versions, replace
		// MTLResourceStorageModeManaged with MTLResourceStorageModeShared. While this solution causes a very small
		// drop in performance, it is still far superior to use Metal rather than OpenGL.
		//
		// As of this writing, the current version of macOS is v10.13.1. Disabling MTLResourceStorageModeManaged on
		// every point release up to v10.13.4 should, I hope, give Apple enough time to fix their bugs with this!
		preferredResourceStorageMode = MTLResourceStorageModeShared;
	}
	else
	{
		preferredResourceStorageMode = MTLResourceStorageModeManaged;
	}
	
	// TODO: In practice, linear textures with buffer-backed storage won't actually work since synchronization has
	// been removed, so keep this feature disabled until synchronization is reworked.
	//_isSharedBufferTextureSupported = IsOSXVersionSupported(10, 13, 0) && (preferredResourceStorageMode == MTLResourceStorageModeManaged);
	_isSharedBufferTextureSupported = NO;
	
	size_t tw = GetNearestPositivePOT((uint32_t)[_fetch555Pipeline threadExecutionWidth]);
	while ( (tw > [_fetch555Pipeline threadExecutionWidth]) || (tw > GPU_FRAMEBUFFER_NATIVE_WIDTH) )
	{
		tw >>= 1;
	}
	
	size_t th = [_fetch555Pipeline maxTotalThreadsPerThreadgroup] / tw;
	
	_fetchThreadsPerGroup = MTLSizeMake(tw, th, 1);
	_fetchThreadGroupsPerGridNative = MTLSizeMake(GPU_FRAMEBUFFER_NATIVE_WIDTH  / tw,
												  GPU_FRAMEBUFFER_NATIVE_HEIGHT / th,
												  1);
	
	_fetchThreadGroupsPerGridCustom = _fetchThreadGroupsPerGridNative;
	
	deposterizeThreadsPerGroup = _fetchThreadsPerGroup;
	deposterizeThreadGroupsPerGrid = _fetchThreadGroupsPerGridNative;
		
	MTLRenderPipelineDescriptor *hudPipelineDesc = [[MTLRenderPipelineDescriptor alloc] init];
	[[[hudPipelineDesc colorAttachments] objectAtIndexedSubscript:0] setBlendingEnabled:YES];
	[[[hudPipelineDesc colorAttachments] objectAtIndexedSubscript:0] setRgbBlendOperation:MTLBlendOperationAdd];
	[[[hudPipelineDesc colorAttachments] objectAtIndexedSubscript:0] setAlphaBlendOperation:MTLBlendOperationAdd];
	[[[hudPipelineDesc colorAttachments] objectAtIndexedSubscript:0] setSourceRGBBlendFactor:MTLBlendFactorSourceAlpha];
	[[[hudPipelineDesc colorAttachments] objectAtIndexedSubscript:0] setSourceAlphaBlendFactor:MTLBlendFactorZero];
	[[[hudPipelineDesc colorAttachments] objectAtIndexedSubscript:0] setDestinationRGBBlendFactor:MTLBlendFactorOneMinusSourceAlpha];
	[[[hudPipelineDesc colorAttachments] objectAtIndexedSubscript:0] setDestinationAlphaBlendFactor:MTLBlendFactorOne];
	
	id<MTLFunction> hudFragmentFunction = [defaultLibrary newFunctionWithName:@"hud_fragment"];
	[hudPipelineDesc setVertexFunction:[defaultLibrary newFunctionWithName:@"hud_vertex"]];
	[hudPipelineDesc setFragmentFunction:hudFragmentFunction];
	
	[[[hudPipelineDesc colorAttachments] objectAtIndexedSubscript:0] setPixelFormat:MTLPixelFormatBGRA8Unorm];
	hudPipeline = [[device newRenderPipelineStateWithDescriptor:hudPipelineDesc error:nil] retain];
	
	[[[hudPipelineDesc colorAttachments] objectAtIndexedSubscript:0] setPixelFormat:MTLPixelFormatRGBA8Unorm];
	hudRGBAPipeline = [[device newRenderPipelineStateWithDescriptor:hudPipelineDesc error:nil] retain];
	
	[hudPipelineDesc release];
	
	hudIndexBuffer = [device newBufferWithLength:(sizeof(uint16_t) * HUD_TOTAL_ELEMENTS * 6) options:MTLResourceStorageModePrivate];
	
	id<MTLBuffer> tempHUDIndexBuffer = [device newBufferWithLength:(sizeof(uint16_t) * HUD_TOTAL_ELEMENTS * 6) options:MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined];
	uint16_t *idxBufferPtr = (uint16_t *)[tempHUDIndexBuffer contents];
	for (size_t i = 0, j = 0, k = 0; i < HUD_TOTAL_ELEMENTS; i++, j+=6, k+=4)
	{
		idxBufferPtr[j+0] = k+0;
		idxBufferPtr[j+1] = k+1;
		idxBufferPtr[j+2] = k+2;
		idxBufferPtr[j+3] = k+2;
		idxBufferPtr[j+4] = k+3;
		idxBufferPtr[j+5] = k+0;
	}
	
	id<MTLCommandBuffer> cb = [commandQueue commandBufferWithUnretainedReferences];;
	id<MTLBlitCommandEncoder> bce = [cb blitCommandEncoder];
	
	[bce copyFromBuffer:tempHUDIndexBuffer
		   sourceOffset:0
			   toBuffer:hudIndexBuffer
	  destinationOffset:0
				   size:sizeof(uint16_t) * HUD_TOTAL_ELEMENTS * 6];
	
	[bce endEncoding];
	
	[cb commit];
	[cb waitUntilCompleted];
	[tempHUDIndexBuffer release];
	
	_bufDisplayFetchNative[NDSDisplayID_Main][0]  = nil;
	_bufDisplayFetchNative[NDSDisplayID_Main][1]  = nil;
	_bufDisplayFetchNative[NDSDisplayID_Touch][0] = nil;
	_bufDisplayFetchNative[NDSDisplayID_Touch][1] = nil;
	_bufDisplayFetchCustom[NDSDisplayID_Main][0]  = nil;
	_bufDisplayFetchCustom[NDSDisplayID_Main][1]  = nil;
	_bufDisplayFetchCustom[NDSDisplayID_Touch][0] = nil;
	_bufDisplayFetchCustom[NDSDisplayID_Touch][1] = nil;
	
	_bufMasterBrightMode[NDSDisplayID_Main][0]  = [device newBufferWithLength:sizeof(uint8_t) * GPU_FRAMEBUFFER_NATIVE_HEIGHT options:MTLResourceStorageModeManaged | MTLResourceCPUCacheModeWriteCombined];
	_bufMasterBrightMode[NDSDisplayID_Main][1]  = [device newBufferWithLength:sizeof(uint8_t) * GPU_FRAMEBUFFER_NATIVE_HEIGHT options:MTLResourceStorageModeManaged | MTLResourceCPUCacheModeWriteCombined];
	_bufMasterBrightMode[NDSDisplayID_Touch][0] = [device newBufferWithLength:sizeof(uint8_t) * GPU_FRAMEBUFFER_NATIVE_HEIGHT options:MTLResourceStorageModeManaged | MTLResourceCPUCacheModeWriteCombined];
	_bufMasterBrightMode[NDSDisplayID_Touch][1] = [device newBufferWithLength:sizeof(uint8_t) * GPU_FRAMEBUFFER_NATIVE_HEIGHT options:MTLResourceStorageModeManaged | MTLResourceCPUCacheModeWriteCombined];
	_bufMasterBrightIntensity[NDSDisplayID_Main][0]  = [device newBufferWithLength:sizeof(uint8_t) * GPU_FRAMEBUFFER_NATIVE_HEIGHT options:MTLResourceStorageModeManaged | MTLResourceCPUCacheModeWriteCombined];
	_bufMasterBrightIntensity[NDSDisplayID_Main][1]  = [device newBufferWithLength:sizeof(uint8_t) * GPU_FRAMEBUFFER_NATIVE_HEIGHT options:MTLResourceStorageModeManaged | MTLResourceCPUCacheModeWriteCombined];
	_bufMasterBrightIntensity[NDSDisplayID_Touch][0] = [device newBufferWithLength:sizeof(uint8_t) * GPU_FRAMEBUFFER_NATIVE_HEIGHT options:MTLResourceStorageModeManaged | MTLResourceCPUCacheModeWriteCombined];
	_bufMasterBrightIntensity[NDSDisplayID_Touch][1] = [device newBufferWithLength:sizeof(uint8_t) * GPU_FRAMEBUFFER_NATIVE_HEIGHT options:MTLResourceStorageModeManaged | MTLResourceCPUCacheModeWriteCombined];
	
	// Set up HUD texture samplers.
	MTLSamplerDescriptor *samplerDesc = [[MTLSamplerDescriptor alloc] init];
	[samplerDesc setNormalizedCoordinates:YES];
	[samplerDesc setRAddressMode:MTLSamplerAddressModeClampToEdge];
	[samplerDesc setSAddressMode:MTLSamplerAddressModeClampToEdge];
	[samplerDesc setTAddressMode:MTLSamplerAddressModeClampToEdge];
	[samplerDesc setMinFilter:MTLSamplerMinMagFilterNearest];
	[samplerDesc setMagFilter:MTLSamplerMinMagFilterNearest];
	[samplerDesc setMipFilter:MTLSamplerMipFilterNearest];
	samplerHUDBox = [device newSamplerStateWithDescriptor:samplerDesc];
	
	[samplerDesc setRAddressMode:MTLSamplerAddressModeClampToZero];
	[samplerDesc setSAddressMode:MTLSamplerAddressModeClampToZero];
	[samplerDesc setTAddressMode:MTLSamplerAddressModeClampToZero];
	[samplerDesc setMinFilter:MTLSamplerMinMagFilterLinear];
	[samplerDesc setMagFilter:MTLSamplerMinMagFilterLinear];
	[samplerDesc setMipFilter:MTLSamplerMipFilterLinear];
	samplerHUDText = [device newSamplerStateWithDescriptor:samplerDesc];
	
	[samplerDesc release];
	
	// Set up the loading textures. These are special because they copy the raw image data from the emulator to the GPU.
	_fetchPixelBytes = sizeof(uint16_t);
	_nativeLineSize = GPU_FRAMEBUFFER_NATIVE_WIDTH * _fetchPixelBytes;
	_nativeBufferSize = GPU_FRAMEBUFFER_NATIVE_HEIGHT * _nativeLineSize;
	_customLineSize = _nativeLineSize;
	_customBufferSize = _nativeBufferSize;
	
	MTLTextureDescriptor *newTexDisplayDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR16Uint
																								 width:GPU_FRAMEBUFFER_NATIVE_WIDTH
																								height:GPU_FRAMEBUFFER_NATIVE_HEIGHT
																							 mipmapped:NO];
	
	[newTexDisplayDesc setResourceOptions:MTLResourceStorageModeManaged | MTLResourceCPUCacheModeWriteCombined];
	[newTexDisplayDesc setStorageMode:MTLStorageModeManaged];
	[newTexDisplayDesc setUsage:MTLTextureUsageShaderRead];
	
	_texDisplayFetchNative[NDSDisplayID_Main][0]  = [device newTextureWithDescriptor:newTexDisplayDesc];
	_texDisplayFetchNative[NDSDisplayID_Main][1]  = [device newTextureWithDescriptor:newTexDisplayDesc];
	_texDisplayFetchNative[NDSDisplayID_Touch][0] = [device newTextureWithDescriptor:newTexDisplayDesc];
	_texDisplayFetchNative[NDSDisplayID_Touch][1] = [device newTextureWithDescriptor:newTexDisplayDesc];
	
	_texDisplayFetchCustom[NDSDisplayID_Main][0]  = [device newTextureWithDescriptor:newTexDisplayDesc];
	_texDisplayFetchCustom[NDSDisplayID_Main][1]  = [device newTextureWithDescriptor:newTexDisplayDesc];
	_texDisplayFetchCustom[NDSDisplayID_Touch][0] = [device newTextureWithDescriptor:newTexDisplayDesc];
	_texDisplayFetchCustom[NDSDisplayID_Touch][1] = [device newTextureWithDescriptor:newTexDisplayDesc];
	
	MTLTextureDescriptor *newTexPostprocessDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
																									 width:GPU_FRAMEBUFFER_NATIVE_WIDTH
																									height:GPU_FRAMEBUFFER_NATIVE_HEIGHT
																								 mipmapped:NO];
	[newTexPostprocessDesc setResourceOptions:MTLResourceStorageModePrivate];
	[newTexPostprocessDesc setStorageMode:MTLStorageModePrivate];
	[newTexPostprocessDesc setUsage:MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite];
	
	_texDisplayPostprocessNative[NDSDisplayID_Main][0]  = [device newTextureWithDescriptor:newTexPostprocessDesc];
	_texDisplayPostprocessNative[NDSDisplayID_Main][1]  = [device newTextureWithDescriptor:newTexPostprocessDesc];
	_texDisplayPostprocessNative[NDSDisplayID_Touch][0] = [device newTextureWithDescriptor:newTexPostprocessDesc];
	_texDisplayPostprocessNative[NDSDisplayID_Touch][1] = [device newTextureWithDescriptor:newTexPostprocessDesc];
	
	_texDisplayPostprocessCustom[NDSDisplayID_Main][0]  = [device newTextureWithDescriptor:newTexPostprocessDesc];
	_texDisplayPostprocessCustom[NDSDisplayID_Main][1]  = [device newTextureWithDescriptor:newTexPostprocessDesc];
	_texDisplayPostprocessCustom[NDSDisplayID_Touch][0] = [device newTextureWithDescriptor:newTexPostprocessDesc];
	_texDisplayPostprocessCustom[NDSDisplayID_Touch][1] = [device newTextureWithDescriptor:newTexPostprocessDesc];
	
	texFetchMain  = [_texDisplayPostprocessNative[NDSDisplayID_Main][0]  retain];
	texFetchTouch = [_texDisplayPostprocessNative[NDSDisplayID_Touch][0] retain];
	
	// Set up the HQnx LUT textures.
	SetupHQnxLUTs_Metal(device, commandQueue, texLQ2xLUT, texHQ2xLUT, texHQ3xLUT, texHQ4xLUT);
	texCurrentHQnxLUT = nil;
	
	return self;
}

- (void)dealloc
{
	[device release];
	
	[commandQueue release];
	[defaultLibrary release];
	[_fetch555Pipeline release];
	[_fetch666Pipeline release];
	[_fetch888Pipeline release];
	[_fetch555ConvertOnlyPipeline release];
	[_fetch666ConvertOnlyPipeline release];
	[deposterizePipeline release];
	[hudPipeline release];
	[hudRGBAPipeline release];
	[hudIndexBuffer release];
	
	[_bufMasterBrightMode[NDSDisplayID_Main][0] release];
	[_bufMasterBrightMode[NDSDisplayID_Main][1] release];
	[_bufMasterBrightMode[NDSDisplayID_Touch][0] release];
	[_bufMasterBrightMode[NDSDisplayID_Touch][1] release];
	[_bufMasterBrightIntensity[NDSDisplayID_Main][0] release];
	[_bufMasterBrightIntensity[NDSDisplayID_Main][1] release];
	[_bufMasterBrightIntensity[NDSDisplayID_Touch][0] release];
	[_bufMasterBrightIntensity[NDSDisplayID_Touch][1] release];
	
	[_texDisplayFetchNative[NDSDisplayID_Main][0]  release];
	[_texDisplayFetchNative[NDSDisplayID_Main][1]  release];
	[_texDisplayFetchNative[NDSDisplayID_Touch][0] release];
	[_texDisplayFetchNative[NDSDisplayID_Touch][1] release];
	
	[_texDisplayFetchCustom[NDSDisplayID_Main][0]  release];
	[_texDisplayFetchCustom[NDSDisplayID_Main][1]  release];
	[_texDisplayFetchCustom[NDSDisplayID_Touch][0] release];
	[_texDisplayFetchCustom[NDSDisplayID_Touch][1] release];
	
	[_texDisplayPostprocessNative[NDSDisplayID_Main][0]  release];
	[_texDisplayPostprocessNative[NDSDisplayID_Main][1]  release];
	[_texDisplayPostprocessNative[NDSDisplayID_Touch][0] release];
	[_texDisplayPostprocessNative[NDSDisplayID_Touch][1] release];
	
	[_texDisplayPostprocessCustom[NDSDisplayID_Main][0]  release];
	[_texDisplayPostprocessCustom[NDSDisplayID_Main][1]  release];
	[_texDisplayPostprocessCustom[NDSDisplayID_Touch][0] release];
	[_texDisplayPostprocessCustom[NDSDisplayID_Touch][1] release];
	
	[self setTexFetchMain:nil];
	[self setTexFetchTouch:nil];
	
	DeleteHQnxLUTs_Metal(texLQ2xLUT, texHQ2xLUT, texHQ3xLUT, texHQ4xLUT);
	[self setTexCurrentHQnxLUT:nil];
	
	[samplerHUDBox release];
	[samplerHUDText release];
		
	[_bufDisplayFetchNative[NDSDisplayID_Main][0]  release];
	[_bufDisplayFetchNative[NDSDisplayID_Main][1]  release];
	[_bufDisplayFetchNative[NDSDisplayID_Touch][0] release];
	[_bufDisplayFetchNative[NDSDisplayID_Touch][1] release];
	[_bufDisplayFetchCustom[NDSDisplayID_Main][0]  release];
	[_bufDisplayFetchCustom[NDSDisplayID_Main][1]  release];
	[_bufDisplayFetchCustom[NDSDisplayID_Touch][0] release];
	[_bufDisplayFetchCustom[NDSDisplayID_Touch][1] release];
	
	[super dealloc];
}

- (void) setFetchBuffersWithDisplayInfo:(const NDSDisplayInfo &)dispInfo
{
	const size_t w = dispInfo.customWidth;
	const size_t h = dispInfo.customHeight;
	
	_nativeLineSize = GPU_FRAMEBUFFER_NATIVE_WIDTH * dispInfo.pixelBytes;
	_nativeBufferSize = GPU_FRAMEBUFFER_NATIVE_HEIGHT * _nativeLineSize;
	_customLineSize = w * dispInfo.pixelBytes;
	_customBufferSize = h * _customLineSize;
	
	if (_fetchPixelBytes != dispInfo.pixelBytes)
	{
		MTLTextureDescriptor *newTexDisplayNativeDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:(dispInfo.colorFormat == NDSColorFormat_BGR555_Rev) ? MTLPixelFormatR16Uint : MTLPixelFormatRGBA8Unorm
																										   width:GPU_FRAMEBUFFER_NATIVE_WIDTH
																										  height:GPU_FRAMEBUFFER_NATIVE_HEIGHT
																									   mipmapped:NO];
		
		[newTexDisplayNativeDesc setResourceOptions:MTLResourceStorageModeManaged | MTLResourceCPUCacheModeWriteCombined];
		[newTexDisplayNativeDesc setStorageMode:MTLStorageModeManaged];
		[newTexDisplayNativeDesc setUsage:MTLTextureUsageShaderRead];
		
		[_texDisplayFetchNative[NDSDisplayID_Main][0]  release];
		[_texDisplayFetchNative[NDSDisplayID_Main][1]  release];
		[_texDisplayFetchNative[NDSDisplayID_Touch][0] release];
		[_texDisplayFetchNative[NDSDisplayID_Touch][1] release];
		
#ifdef MAC_OS_X_VERSION_10_13
		if (_isSharedBufferTextureSupported)
		{
			const NDSDisplayInfo &dispInfo0 = GPUFetchObject->GetFetchDisplayInfoForBufferIndex(0);
			const NDSDisplayInfo &dispInfo1 = GPUFetchObject->GetFetchDisplayInfoForBufferIndex(1);
			
			[_bufDisplayFetchNative[NDSDisplayID_Main][0]  release];
			[_bufDisplayFetchNative[NDSDisplayID_Main][1]  release];
			[_bufDisplayFetchNative[NDSDisplayID_Touch][0] release];
			[_bufDisplayFetchNative[NDSDisplayID_Touch][1] release];
			
			_bufDisplayFetchNative[NDSDisplayID_Main][0]  = [device newBufferWithBytesNoCopy:dispInfo0.nativeBuffer[NDSDisplayID_Main]
																					  length:_nativeBufferSize
																					 options:MTLResourceStorageModeManaged | MTLResourceCPUCacheModeWriteCombined
																				 deallocator:nil];
			
			_bufDisplayFetchNative[NDSDisplayID_Main][1]  = [device newBufferWithBytesNoCopy:dispInfo1.nativeBuffer[NDSDisplayID_Main]
																					  length:_nativeBufferSize
																					 options:MTLResourceStorageModeManaged | MTLResourceCPUCacheModeWriteCombined
																				 deallocator:nil];
			
			_bufDisplayFetchNative[NDSDisplayID_Touch][0] = [device newBufferWithBytesNoCopy:dispInfo0.nativeBuffer[NDSDisplayID_Touch]
																					  length:_nativeBufferSize
																					 options:MTLResourceStorageModeManaged | MTLResourceCPUCacheModeWriteCombined
																				 deallocator:nil];
			
			_bufDisplayFetchNative[NDSDisplayID_Touch][1] = [device newBufferWithBytesNoCopy:dispInfo1.nativeBuffer[NDSDisplayID_Touch]
																					  length:_nativeBufferSize
																					 options:MTLResourceStorageModeManaged | MTLResourceCPUCacheModeWriteCombined
																				 deallocator:nil];
			
			_texDisplayFetchNative[NDSDisplayID_Main][0]  = [_bufDisplayFetchNative[NDSDisplayID_Main][0]  newTextureWithDescriptor:newTexDisplayNativeDesc offset:0 bytesPerRow:_nativeLineSize];
			_texDisplayFetchNative[NDSDisplayID_Main][1]  = [_bufDisplayFetchNative[NDSDisplayID_Main][1]  newTextureWithDescriptor:newTexDisplayNativeDesc offset:0 bytesPerRow:_nativeLineSize];
			_texDisplayFetchNative[NDSDisplayID_Touch][0] = [_bufDisplayFetchNative[NDSDisplayID_Touch][0] newTextureWithDescriptor:newTexDisplayNativeDesc offset:0 bytesPerRow:_nativeLineSize];
			_texDisplayFetchNative[NDSDisplayID_Touch][1] = [_bufDisplayFetchNative[NDSDisplayID_Touch][1] newTextureWithDescriptor:newTexDisplayNativeDesc offset:0 bytesPerRow:_nativeLineSize];
		}
		else
#endif
		{
			_texDisplayFetchNative[NDSDisplayID_Main][0]  = [device newTextureWithDescriptor:newTexDisplayNativeDesc];
			_texDisplayFetchNative[NDSDisplayID_Main][1]  = [device newTextureWithDescriptor:newTexDisplayNativeDesc];
			_texDisplayFetchNative[NDSDisplayID_Touch][0] = [device newTextureWithDescriptor:newTexDisplayNativeDesc];
			_texDisplayFetchNative[NDSDisplayID_Touch][1] = [device newTextureWithDescriptor:newTexDisplayNativeDesc];
		}
		
		MTLTextureDescriptor *newTexPostprocessNativeDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
																											   width:GPU_FRAMEBUFFER_NATIVE_WIDTH
																											  height:GPU_FRAMEBUFFER_NATIVE_HEIGHT
																										   mipmapped:NO];
		[newTexPostprocessNativeDesc setResourceOptions:MTLResourceStorageModePrivate];
		[newTexPostprocessNativeDesc setStorageMode:MTLStorageModePrivate];
		[newTexPostprocessNativeDesc setUsage:MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite];
		
		[_texDisplayPostprocessNative[NDSDisplayID_Main][0]  release];
		[_texDisplayPostprocessNative[NDSDisplayID_Main][1]  release];
		[_texDisplayPostprocessNative[NDSDisplayID_Touch][0] release];
		[_texDisplayPostprocessNative[NDSDisplayID_Touch][1] release];
		_texDisplayPostprocessNative[NDSDisplayID_Main][0]  = [device newTextureWithDescriptor:newTexPostprocessNativeDesc];
		_texDisplayPostprocessNative[NDSDisplayID_Main][1]  = [device newTextureWithDescriptor:newTexPostprocessNativeDesc];
		_texDisplayPostprocessNative[NDSDisplayID_Touch][0] = [device newTextureWithDescriptor:newTexPostprocessNativeDesc];
		_texDisplayPostprocessNative[NDSDisplayID_Touch][1] = [device newTextureWithDescriptor:newTexPostprocessNativeDesc];
	}
	
	if ( (_fetchPixelBytes != dispInfo.pixelBytes) ||
	     ([_texDisplayFetchCustom[NDSDisplayID_Main][0] width] != w) ||
	     ([_texDisplayFetchCustom[NDSDisplayID_Main][0] height] != h) )
	{
		MTLTextureDescriptor *newTexDisplayCustomDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:(dispInfo.colorFormat == NDSColorFormat_BGR555_Rev) ? MTLPixelFormatR16Uint : MTLPixelFormatRGBA8Unorm
																										   width:w
																										  height:h
																									   mipmapped:NO];
		
		[newTexDisplayCustomDesc setResourceOptions:MTLResourceStorageModeManaged | MTLResourceCPUCacheModeWriteCombined];
		[newTexDisplayCustomDesc setStorageMode:MTLStorageModeManaged];
		[newTexDisplayCustomDesc setUsage:MTLTextureUsageShaderRead];
		
		[_texDisplayFetchCustom[NDSDisplayID_Main][0]  release];
		[_texDisplayFetchCustom[NDSDisplayID_Main][1]  release];
		[_texDisplayFetchCustom[NDSDisplayID_Touch][0] release];
		[_texDisplayFetchCustom[NDSDisplayID_Touch][1] release];
		
#ifdef MAC_OS_X_VERSION_10_13
		if (_isSharedBufferTextureSupported)
		{
			const NDSDisplayInfo &dispInfo0 = GPUFetchObject->GetFetchDisplayInfoForBufferIndex(0);
			const NDSDisplayInfo &dispInfo1 = GPUFetchObject->GetFetchDisplayInfoForBufferIndex(1);
			
			[_bufDisplayFetchCustom[NDSDisplayID_Main][0]  release];
			[_bufDisplayFetchCustom[NDSDisplayID_Main][1]  release];
			[_bufDisplayFetchCustom[NDSDisplayID_Touch][0] release];
			[_bufDisplayFetchCustom[NDSDisplayID_Touch][1] release];
			
			_bufDisplayFetchCustom[NDSDisplayID_Main][0]  = [device newBufferWithBytesNoCopy:dispInfo0.customBuffer[NDSDisplayID_Main]
																					  length:_customBufferSize
																					 options:MTLResourceStorageModeManaged | MTLResourceCPUCacheModeWriteCombined
																				 deallocator:nil];
			
			_bufDisplayFetchCustom[NDSDisplayID_Main][1]  = [device newBufferWithBytesNoCopy:dispInfo1.customBuffer[NDSDisplayID_Main]
																					  length:_customBufferSize
																					 options:MTLResourceStorageModeManaged | MTLResourceCPUCacheModeWriteCombined
																				 deallocator:nil];
			
			_bufDisplayFetchCustom[NDSDisplayID_Touch][0] = [device newBufferWithBytesNoCopy:dispInfo0.customBuffer[NDSDisplayID_Touch]
																					  length:_customBufferSize
																					 options:MTLResourceStorageModeManaged | MTLResourceCPUCacheModeWriteCombined
																				 deallocator:nil];
			
			_bufDisplayFetchCustom[NDSDisplayID_Touch][1] = [device newBufferWithBytesNoCopy:dispInfo1.customBuffer[NDSDisplayID_Touch]
																					  length:_customBufferSize
																					 options:MTLResourceStorageModeManaged | MTLResourceCPUCacheModeWriteCombined
																				 deallocator:nil];
			
			_texDisplayFetchCustom[NDSDisplayID_Main][0]  = [_bufDisplayFetchCustom[NDSDisplayID_Main][0]  newTextureWithDescriptor:newTexDisplayCustomDesc offset:0 bytesPerRow:_customLineSize];
			_texDisplayFetchCustom[NDSDisplayID_Main][1]  = [_bufDisplayFetchCustom[NDSDisplayID_Main][1]  newTextureWithDescriptor:newTexDisplayCustomDesc offset:0 bytesPerRow:_customLineSize];
			_texDisplayFetchCustom[NDSDisplayID_Touch][0] = [_bufDisplayFetchCustom[NDSDisplayID_Touch][0] newTextureWithDescriptor:newTexDisplayCustomDesc offset:0 bytesPerRow:_customLineSize];
			_texDisplayFetchCustom[NDSDisplayID_Touch][1] = [_bufDisplayFetchCustom[NDSDisplayID_Touch][1] newTextureWithDescriptor:newTexDisplayCustomDesc offset:0 bytesPerRow:_customLineSize];
		}
		else
#endif
		{
			_texDisplayFetchCustom[NDSDisplayID_Main][0]  = [device newTextureWithDescriptor:newTexDisplayCustomDesc];
			_texDisplayFetchCustom[NDSDisplayID_Main][1]  = [device newTextureWithDescriptor:newTexDisplayCustomDesc];
			_texDisplayFetchCustom[NDSDisplayID_Touch][0] = [device newTextureWithDescriptor:newTexDisplayCustomDesc];
			_texDisplayFetchCustom[NDSDisplayID_Touch][1] = [device newTextureWithDescriptor:newTexDisplayCustomDesc];
		}
		
		MTLTextureDescriptor *newTexPostprocessCustomDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
																											   width:w
																											  height:h
																										   mipmapped:NO];
		[newTexPostprocessCustomDesc setResourceOptions:MTLResourceStorageModePrivate];
		[newTexPostprocessCustomDesc setStorageMode:MTLStorageModePrivate];
		[newTexPostprocessCustomDesc setUsage:MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite];
		
		[_texDisplayPostprocessCustom[NDSDisplayID_Main][0]  release];
		[_texDisplayPostprocessCustom[NDSDisplayID_Main][1]  release];
		[_texDisplayPostprocessCustom[NDSDisplayID_Touch][0] release];
		[_texDisplayPostprocessCustom[NDSDisplayID_Touch][1] release];
		_texDisplayPostprocessCustom[NDSDisplayID_Main][0]  = [device newTextureWithDescriptor:newTexPostprocessCustomDesc];
		_texDisplayPostprocessCustom[NDSDisplayID_Main][1]  = [device newTextureWithDescriptor:newTexPostprocessCustomDesc];
		_texDisplayPostprocessCustom[NDSDisplayID_Touch][0] = [device newTextureWithDescriptor:newTexPostprocessCustomDesc];
		_texDisplayPostprocessCustom[NDSDisplayID_Touch][1] = [device newTextureWithDescriptor:newTexPostprocessCustomDesc];
	}
	
	_fetchPixelBytes = dispInfo.pixelBytes;
	
	const size_t tw = _fetchThreadsPerGroup.width;
	const size_t th = _fetchThreadsPerGroup.height;
	_fetchThreadGroupsPerGridCustom = MTLSizeMake((w + tw - 1) / tw, (h + th - 1) / th, 1);
	
	id<MTLCommandBuffer> cb = [commandQueue commandBufferWithUnretainedReferences];
	[self setFetchTextureBindingsAtIndex:dispInfo.bufferIndex commandBuffer:cb];
	[cb commit];
	[cb waitUntilCompleted];
}

- (void) setFetchTextureBindingsAtIndex:(const u8)index commandBuffer:(id<MTLCommandBuffer>)cb
{
	id<MTLTexture> texFetchTargetMain = nil;
	id<MTLTexture> texFetchTargetTouch = nil;
	const NDSDisplayInfo &currentDisplayInfo = GPUFetchObject->GetFetchDisplayInfoForBufferIndex(index);
	const bool isMainEnabled  = currentDisplayInfo.isDisplayEnabled[NDSDisplayID_Main];
	const bool isTouchEnabled = currentDisplayInfo.isDisplayEnabled[NDSDisplayID_Touch];
	
	if (isMainEnabled || isTouchEnabled)
	{
		if (isMainEnabled)
		{
			if (!currentDisplayInfo.didPerformCustomRender[NDSDisplayID_Main])
			{
				texFetchTargetMain = _texDisplayFetchNative[NDSDisplayID_Main][index];
			}
			else
			{
				texFetchTargetMain = _texDisplayFetchCustom[NDSDisplayID_Main][index];
			}
		}
		
		if (isTouchEnabled)
		{
			if (!currentDisplayInfo.didPerformCustomRender[NDSDisplayID_Touch])
			{
				texFetchTargetTouch = _texDisplayFetchNative[NDSDisplayID_Touch][index];
			}
			else
			{
				texFetchTargetTouch = _texDisplayFetchCustom[NDSDisplayID_Touch][index];
			}
		}
		
		id<MTLComputeCommandEncoder> cce = [cb computeCommandEncoder];
		
		if (currentDisplayInfo.needApplyMasterBrightness[NDSDisplayID_Main] || currentDisplayInfo.needApplyMasterBrightness[NDSDisplayID_Touch])
		{
			if (currentDisplayInfo.colorFormat == NDSColorFormat_BGR555_Rev)
			{
				[cce setComputePipelineState:_fetch555Pipeline];
			}
			else if ( (currentDisplayInfo.colorFormat == NDSColorFormat_BGR666_Rev) &&
					  (currentDisplayInfo.needConvertColorFormat[NDSDisplayID_Main] || currentDisplayInfo.needConvertColorFormat[NDSDisplayID_Touch]) )
			{
				[cce setComputePipelineState:_fetch666Pipeline];
			}
			else
			{
				[cce setComputePipelineState:_fetch888Pipeline];
			}
			
			if (isMainEnabled)
			{
				memcpy([_bufMasterBrightMode[NDSDisplayID_Main][index] contents], currentDisplayInfo.masterBrightnessMode[NDSDisplayID_Main], sizeof(uint8_t) * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
				memcpy([_bufMasterBrightIntensity[NDSDisplayID_Main][index] contents], currentDisplayInfo.masterBrightnessIntensity[NDSDisplayID_Main], sizeof(uint8_t) * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
				[_bufMasterBrightMode[NDSDisplayID_Main][index] didModifyRange:NSMakeRange(0, sizeof(uint8_t) * GPU_FRAMEBUFFER_NATIVE_HEIGHT)];
				[_bufMasterBrightIntensity[NDSDisplayID_Main][index] didModifyRange:NSMakeRange(0, sizeof(uint8_t) * GPU_FRAMEBUFFER_NATIVE_HEIGHT)];
				
				[cce setBuffer:_bufMasterBrightMode[NDSDisplayID_Main][index] offset:0 atIndex:0];
				[cce setBuffer:_bufMasterBrightIntensity[NDSDisplayID_Main][index] offset:0 atIndex:1];
				
				if (!currentDisplayInfo.didPerformCustomRender[NDSDisplayID_Main])
				{
					[cce setTexture:_texDisplayFetchNative[NDSDisplayID_Main][index] atIndex:0];
					[cce setTexture:_texDisplayPostprocessNative[NDSDisplayID_Main][index] atIndex:1];
					[cce dispatchThreadgroups:_fetchThreadGroupsPerGridNative
						threadsPerThreadgroup:_fetchThreadsPerGroup];
					
					texFetchTargetMain = _texDisplayPostprocessNative[NDSDisplayID_Main][index];
				}
				else
				{
					[cce setTexture:_texDisplayFetchCustom[NDSDisplayID_Main][index] atIndex:0];
					[cce setTexture:_texDisplayPostprocessCustom[NDSDisplayID_Main][index] atIndex:1];
					[cce dispatchThreadgroups:_fetchThreadGroupsPerGridCustom
						threadsPerThreadgroup:_fetchThreadsPerGroup];
					
					texFetchTargetMain = _texDisplayPostprocessCustom[NDSDisplayID_Main][index];
				}
			}
			
			if (isTouchEnabled)
			{
				memcpy([_bufMasterBrightMode[NDSDisplayID_Touch][index] contents], currentDisplayInfo.masterBrightnessMode[NDSDisplayID_Touch], sizeof(uint8_t) * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
				memcpy([_bufMasterBrightIntensity[NDSDisplayID_Touch][index] contents], currentDisplayInfo.masterBrightnessIntensity[NDSDisplayID_Touch], sizeof(uint8_t) * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
				[_bufMasterBrightMode[NDSDisplayID_Touch][index] didModifyRange:NSMakeRange(0, sizeof(uint8_t) * GPU_FRAMEBUFFER_NATIVE_HEIGHT)];
				[_bufMasterBrightIntensity[NDSDisplayID_Touch][index] didModifyRange:NSMakeRange(0, sizeof(uint8_t) * GPU_FRAMEBUFFER_NATIVE_HEIGHT)];
				
				[cce setBuffer:_bufMasterBrightMode[NDSDisplayID_Touch][index] offset:0 atIndex:0];
				[cce setBuffer:_bufMasterBrightIntensity[NDSDisplayID_Touch][index] offset:0 atIndex:1];
				
				if (!currentDisplayInfo.didPerformCustomRender[NDSDisplayID_Touch])
				{
					[cce setTexture:_texDisplayFetchNative[NDSDisplayID_Touch][index] atIndex:0];
					[cce setTexture:_texDisplayPostprocessNative[NDSDisplayID_Touch][index] atIndex:1];
					[cce dispatchThreadgroups:_fetchThreadGroupsPerGridNative
						threadsPerThreadgroup:_fetchThreadsPerGroup];
					
					texFetchTargetTouch = _texDisplayPostprocessNative[NDSDisplayID_Touch][index];
				}
				else
				{
					[cce setTexture:_texDisplayFetchCustom[NDSDisplayID_Touch][index] atIndex:0];
					[cce setTexture:_texDisplayPostprocessCustom[NDSDisplayID_Touch][index] atIndex:1];
					[cce dispatchThreadgroups:_fetchThreadGroupsPerGridCustom
						threadsPerThreadgroup:_fetchThreadsPerGroup];
					
					texFetchTargetTouch = _texDisplayPostprocessCustom[NDSDisplayID_Touch][index];
				}
			}
		}
		else if (currentDisplayInfo.colorFormat != NDSColorFormat_BGR888_Rev)
		{
			bool isPipelineStateSet = false;
			
			if (currentDisplayInfo.colorFormat == NDSColorFormat_BGR555_Rev)
			{
				// 16-bit textures aren't handled natively in Metal for macOS, so we need to explicitly convert to 32-bit here.
				[cce setComputePipelineState:_fetch555ConvertOnlyPipeline];
				isPipelineStateSet = true;
			}
			else if ( (currentDisplayInfo.colorFormat == NDSColorFormat_BGR666_Rev) &&
					  (currentDisplayInfo.needConvertColorFormat[NDSDisplayID_Main] || currentDisplayInfo.needConvertColorFormat[NDSDisplayID_Touch]) )
			{
				[cce setComputePipelineState:_fetch666ConvertOnlyPipeline];
				isPipelineStateSet = true;
			}
			
			if (isPipelineStateSet)
			{
				if (isMainEnabled)
				{
					if (!currentDisplayInfo.didPerformCustomRender[NDSDisplayID_Main])
					{
						[cce setTexture:_texDisplayFetchNative[NDSDisplayID_Main][index] atIndex:0];
						[cce setTexture:_texDisplayPostprocessNative[NDSDisplayID_Main][index] atIndex:1];
						[cce dispatchThreadgroups:_fetchThreadGroupsPerGridNative
							threadsPerThreadgroup:_fetchThreadsPerGroup];
						
						texFetchTargetMain = _texDisplayPostprocessNative[NDSDisplayID_Main][index];
					}
					else
					{
						[cce setTexture:_texDisplayFetchCustom[NDSDisplayID_Main][index] atIndex:0];
						[cce setTexture:_texDisplayPostprocessCustom[NDSDisplayID_Main][index] atIndex:1];
						[cce dispatchThreadgroups:_fetchThreadGroupsPerGridCustom
							threadsPerThreadgroup:_fetchThreadsPerGroup];
						
						texFetchTargetMain = _texDisplayPostprocessCustom[NDSDisplayID_Main][index];
					}
				}
				
				if (isTouchEnabled)
				{
					if (!currentDisplayInfo.didPerformCustomRender[NDSDisplayID_Touch])
					{
						[cce setTexture:_texDisplayFetchNative[NDSDisplayID_Touch][index] atIndex:0];
						[cce setTexture:_texDisplayPostprocessNative[NDSDisplayID_Touch][index] atIndex:1];
						[cce dispatchThreadgroups:_fetchThreadGroupsPerGridNative
							threadsPerThreadgroup:_fetchThreadsPerGroup];
						
						texFetchTargetTouch = _texDisplayPostprocessNative[NDSDisplayID_Touch][index];
					}
					else
					{
						[cce setTexture:_texDisplayFetchCustom[NDSDisplayID_Touch][index] atIndex:0];
						[cce setTexture:_texDisplayPostprocessCustom[NDSDisplayID_Touch][index] atIndex:1];
						[cce dispatchThreadgroups:_fetchThreadGroupsPerGridCustom
							threadsPerThreadgroup:_fetchThreadsPerGroup];
						
						texFetchTargetTouch = _texDisplayPostprocessCustom[NDSDisplayID_Touch][index];
					}
				}
			}
		}
		
		[cce endEncoding];
	}
	
	[self setTexFetchMain:texFetchTargetMain];
	[self setTexFetchTouch:texFetchTargetTouch];
}

- (void) fetchFromBufferIndex:(const u8)index
{
	id<MTLCommandBuffer> cb = [commandQueue commandBufferWithUnretainedReferences];
	
	if (!_isSharedBufferTextureSupported)
	{
		semaphore_wait([self semaphoreFramebufferAtIndex:index]);
		GPUFetchObject->GPUClientFetchObject::FetchFromBufferIndex(index);
		semaphore_signal([self semaphoreFramebufferAtIndex:index]);
	}
	
	[self setFetchTextureBindingsAtIndex:index commandBuffer:cb];
	[cb commit];
}

- (void) fetchNativeDisplayByID:(const NDSDisplayID)displayID bufferIndex:(const u8)bufferIndex
{
	id<MTLTexture> targetDestination = _texDisplayFetchNative[displayID][bufferIndex];
	const NDSDisplayInfo &currentDisplayInfo = GPUFetchObject->GetFetchDisplayInfoForBufferIndex(bufferIndex);
	
	[targetDestination replaceRegion:MTLRegionMake2D(0, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT)
						 mipmapLevel:0
						   withBytes:currentDisplayInfo.nativeBuffer[displayID]
						 bytesPerRow:_nativeLineSize];
}

- (void) fetchCustomDisplayByID:(const NDSDisplayID)displayID bufferIndex:(const u8)bufferIndex
{
	const NDSDisplayInfo &currentDisplayInfo = GPUFetchObject->GetFetchDisplayInfoForBufferIndex(bufferIndex);
	id<MTLTexture> targetDestination = _texDisplayFetchCustom[displayID][bufferIndex];
	
	[targetDestination replaceRegion:MTLRegionMake2D(0, 0, currentDisplayInfo.customWidth, currentDisplayInfo.customHeight)
						 mipmapLevel:0
						   withBytes:currentDisplayInfo.customBuffer[displayID]
						 bytesPerRow:_customLineSize];
}

@end

@implementation MacMetalDisplayPresenterObject

@synthesize cdp;
@synthesize sharedData;
@synthesize colorAttachment0Desc;
@synthesize pixelScalePipeline;
@synthesize outputRGBAPipeline;
@synthesize outputDrawablePipeline;
@synthesize drawableFormat;
@synthesize texDisplayPixelScaleMain;
@synthesize texDisplayPixelScaleTouch;
@synthesize texHUDCharMap;
@synthesize needsViewportUpdate;
@synthesize needsRotationScaleUpdate;
@synthesize needsScreenVerticesUpdate;
@synthesize needsHUDVerticesUpdate;
@dynamic pixelScaler;
@dynamic outputFilter;

- (id) initWithDisplayPresenter:(MacMetalDisplayPresenter *)thePresenter
{
	self = [super init];
	if(self == nil)
	{
		return nil;
	}
	
	cdp = thePresenter;
	
	if (thePresenter != NULL)
	{
		sharedData = (MetalDisplayViewSharedData *)thePresenter->GetSharedData();
	}
	else
	{
		sharedData = nil;
	}
	
	_outputRenderPassDesc = [[MTLRenderPassDescriptor renderPassDescriptor] retain];
	colorAttachment0Desc = [[_outputRenderPassDesc colorAttachments] objectAtIndexedSubscript:0];
	[colorAttachment0Desc setLoadAction:MTLLoadActionClear];
	[colorAttachment0Desc setStoreAction:MTLStoreActionStore];
	[colorAttachment0Desc setClearColor:MTLClearColorMake(0.0, 0.0, 0.0, 1.0)];
	[colorAttachment0Desc setTexture:nil];
	
	pixelScalePipeline = nil;
	outputRGBAPipeline = nil;
	outputDrawablePipeline = nil;
	drawableFormat = MTLPixelFormatInvalid;
	
	for (size_t i = 0; i < METAL_BUFFER_COUNT; i++)
	{
		_texDisplaySrcDeposterize[i][NDSDisplayID_Main][0]  = nil;
		_texDisplaySrcDeposterize[i][NDSDisplayID_Touch][0] = nil;
		_texDisplaySrcDeposterize[i][NDSDisplayID_Main][1]  = nil;
		_texDisplaySrcDeposterize[i][NDSDisplayID_Touch][1] = nil;
		
		_hudVtxPositionBuffer[i] = nil;
		_hudVtxColorBuffer[i]    = nil;
		_hudTexCoordBuffer[i]    = nil;
	}
	
	_bufCPUFilterSrcMain  = nil;
	_bufCPUFilterSrcTouch = nil;
	texDisplayPixelScaleMain  = nil;
	texDisplayPixelScaleTouch = nil;
	texHUDCharMap = nil;
	
	_pixelScalerThreadsPerGroup = MTLSizeMake(1, 1, 1);
	_pixelScalerThreadGroupsPerGrid = MTLSizeMake(1, 1, 1);
	
	needsViewportUpdate = YES;
	needsRotationScaleUpdate = YES;
	needsScreenVerticesUpdate = YES;
	needsHUDVerticesUpdate = YES;
	
	_processIndex = 0;
	_mpfi.processIndex = _processIndex;
    _mpfi.tex[NDSDisplayID_Main]  = nil;
    _mpfi.tex[NDSDisplayID_Touch] = nil;
	
	_spinlockProcessedFrameInfo = OS_SPINLOCK_INIT;
	
	_semProcessBuffers = dispatch_semaphore_create(METAL_BUFFER_COUNT);
	_semRenderBuffers = dispatch_semaphore_create(METAL_BUFFER_COUNT);
	_mrfi.renderIndex = 0;
	
	return self;
}

- (void)dealloc
{
	[[self colorAttachment0Desc] setTexture:nil];
	[_outputRenderPassDesc release];
	
	for (size_t i = 0; i < METAL_BUFFER_COUNT; i++)
	{
		[_texDisplaySrcDeposterize[i][NDSDisplayID_Main][0]  release];
		[_texDisplaySrcDeposterize[i][NDSDisplayID_Touch][0] release];
		[_texDisplaySrcDeposterize[i][NDSDisplayID_Main][1]  release];
		[_texDisplaySrcDeposterize[i][NDSDisplayID_Touch][1] release];
		
		[_hudVtxPositionBuffer[i] release];
		[_hudVtxColorBuffer[i]    release];
		[_hudTexCoordBuffer[i]    release];
	}
	
	[_bufCPUFilterSrcMain  release];
	[_bufCPUFilterSrcTouch release];
	[self setTexDisplayPixelScaleMain:nil];
	[self setTexDisplayPixelScaleTouch:nil];
	
	[_mpfi.tex[NDSDisplayID_Main]  release];
	[_mpfi.tex[NDSDisplayID_Touch] release];
	
	[self setPixelScalePipeline:nil];
	[self setOutputRGBAPipeline:nil];
	[self setOutputDrawablePipeline:nil];
	[self setTexHUDCharMap:nil];
	
	[self setSharedData:nil];
	
	dispatch_release(_semProcessBuffers);
	dispatch_release(_semRenderBuffers);
	
	[super dealloc];
}

- (VideoFilterTypeID) pixelScaler
{
	return cdp->GetPixelScaler();
}

- (void) setPixelScaler:(VideoFilterTypeID)filterID
{
	id<MTLTexture> currentHQnxLUT = nil;
	
	switch (filterID)
	{
		case VideoFilterTypeID_Nearest2X:
			[self setPixelScalePipeline:[[sharedData device] newComputePipelineStateWithFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_nearest2x"] error:nil]];
			break;
			
		case VideoFilterTypeID_Scanline:
			[self setPixelScalePipeline:[[sharedData device] newComputePipelineStateWithFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_scanline"] error:nil]];
			break;
			
		case VideoFilterTypeID_EPX:
			[self setPixelScalePipeline:[[sharedData device] newComputePipelineStateWithFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_2xEPX"] error:nil]];
			break;
			
		case VideoFilterTypeID_EPXPlus:
			[self setPixelScalePipeline:[[sharedData device] newComputePipelineStateWithFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_2xEPXPlus"] error:nil]];
			break;
			
		case VideoFilterTypeID_2xSaI:
			[self setPixelScalePipeline:[[sharedData device] newComputePipelineStateWithFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_2xSaI"] error:nil]];
			break;
			
		case VideoFilterTypeID_Super2xSaI:
			[self setPixelScalePipeline:[[sharedData device] newComputePipelineStateWithFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_Super2xSaI"] error:nil]];
			break;
			
		case VideoFilterTypeID_SuperEagle:
			[self setPixelScalePipeline:[[sharedData device] newComputePipelineStateWithFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_2xSuperEagle"] error:nil]];
			break;
			
		case VideoFilterTypeID_LQ2X:
			[self setPixelScalePipeline:[[sharedData device] newComputePipelineStateWithFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_LQ2x"] error:nil]];
			currentHQnxLUT = [sharedData texLQ2xLUT];
			break;
			
		case VideoFilterTypeID_LQ2XS:
			[self setPixelScalePipeline:[[sharedData device] newComputePipelineStateWithFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_LQ2xS"] error:nil]];
			currentHQnxLUT = [sharedData texLQ2xLUT];
			break;
			
		case VideoFilterTypeID_HQ2X:
			[self setPixelScalePipeline:[[sharedData device] newComputePipelineStateWithFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_HQ2x"] error:nil]];
			currentHQnxLUT = [sharedData texHQ2xLUT];
			break;
			
		case VideoFilterTypeID_HQ2XS:
			[self setPixelScalePipeline:[[sharedData device] newComputePipelineStateWithFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_HQ2xS"] error:nil]];
			currentHQnxLUT = [sharedData texHQ2xLUT];
			break;
			
		case VideoFilterTypeID_HQ3X:
			[self setPixelScalePipeline:[[sharedData device] newComputePipelineStateWithFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_HQ3x"] error:nil]];
			currentHQnxLUT = [sharedData texHQ3xLUT];
			break;
			
		case VideoFilterTypeID_HQ3XS:
			[self setPixelScalePipeline:[[sharedData device] newComputePipelineStateWithFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_HQ3xS"] error:nil]];
			currentHQnxLUT = [sharedData texHQ3xLUT];
			break;
			
		case VideoFilterTypeID_HQ4X:
			[self setPixelScalePipeline:[[sharedData device] newComputePipelineStateWithFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_HQ4x"] error:nil]];
			currentHQnxLUT = [sharedData texHQ4xLUT];
			break;
			
		case VideoFilterTypeID_HQ4XS:
			[self setPixelScalePipeline:[[sharedData device] newComputePipelineStateWithFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_HQ4xS"] error:nil]];
			currentHQnxLUT = [sharedData texHQ4xLUT];
			break;
			
		case VideoFilterTypeID_2xBRZ:
			[self setPixelScalePipeline:[[sharedData device] newComputePipelineStateWithFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_2xBRZ"] error:nil]];
			break;
			
		case VideoFilterTypeID_3xBRZ:
			[self setPixelScalePipeline:[[sharedData device] newComputePipelineStateWithFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_3xBRZ"] error:nil]];
			break;
			
		case VideoFilterTypeID_4xBRZ:
			[self setPixelScalePipeline:[[sharedData device] newComputePipelineStateWithFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_4xBRZ"] error:nil]];
			break;
			
		case VideoFilterTypeID_5xBRZ:
			[self setPixelScalePipeline:[[sharedData device] newComputePipelineStateWithFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_5xBRZ"] error:nil]];
			break;
			
		case VideoFilterTypeID_6xBRZ:
			[self setPixelScalePipeline:[[sharedData device] newComputePipelineStateWithFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_6xBRZ"] error:nil]];
			break;
			
		case VideoFilterTypeID_None:
		default:
			[self setPixelScalePipeline:nil];
			break;
	}
	
	[sharedData setTexCurrentHQnxLUT:currentHQnxLUT];
	
	if ([self pixelScalePipeline] != nil)
	{
        const VideoFilterAttributes vfAttr = VideoFilter::GetAttributesByID(filterID);
        const size_t newScalerWidth  = GPU_FRAMEBUFFER_NATIVE_WIDTH  * vfAttr.scaleMultiply / vfAttr.scaleDivide;
        const size_t newScalerHeight = GPU_FRAMEBUFFER_NATIVE_HEIGHT * vfAttr.scaleMultiply / vfAttr.scaleDivide;
        
        MTLTextureDescriptor *texDisplayPixelScaleDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                                            width:newScalerWidth
                                                                                                           height:newScalerHeight
                                                                                                        mipmapped:NO];
        [texDisplayPixelScaleDesc setResourceOptions:MTLResourceStorageModeManaged | MTLCPUCacheModeWriteCombined];
        [texDisplayPixelScaleDesc setStorageMode:MTLStorageModeManaged];
        [texDisplayPixelScaleDesc setUsage:MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite];
        
        [self setTexDisplayPixelScaleMain:[[[sharedData device] newTextureWithDescriptor:texDisplayPixelScaleDesc] autorelease]];
        [self setTexDisplayPixelScaleTouch:[[[sharedData device] newTextureWithDescriptor:texDisplayPixelScaleDesc] autorelease]];
        
		size_t tw = GetNearestPositivePOT((uint32_t)[[self pixelScalePipeline] threadExecutionWidth]);
		while ( (tw > [[self pixelScalePipeline] threadExecutionWidth]) || (tw > GPU_FRAMEBUFFER_NATIVE_WIDTH) )
		{
			tw >>= 1;
		}
		
		const size_t th = [[self pixelScalePipeline] maxTotalThreadsPerThreadgroup] / tw;
		
		_pixelScalerThreadsPerGroup = MTLSizeMake(tw, th, 1);
		_pixelScalerThreadGroupsPerGrid = MTLSizeMake(GPU_FRAMEBUFFER_NATIVE_WIDTH  / tw,
													  GPU_FRAMEBUFFER_NATIVE_HEIGHT / th,
													  1);
	}
	else
	{
        [self setTexDisplayPixelScaleMain:nil];
        [self setTexDisplayPixelScaleTouch:nil];
		_pixelScalerThreadsPerGroup = MTLSizeMake(1, 1, 1);
		_pixelScalerThreadGroupsPerGrid = MTLSizeMake(1, 1, 1);
	}
}

- (OutputFilterTypeID) outputFilter
{
	return cdp->GetOutputFilter();
}

- (void) setOutputFilter:(OutputFilterTypeID)filterID
{
	MTLRenderPipelineDescriptor *outputPipelineDesc = [[MTLRenderPipelineDescriptor alloc] init];
	[outputPipelineDesc setAlphaToOneEnabled:YES];
	
	switch (filterID)
	{
		case OutputFilterTypeID_NearestNeighbor:
			[outputPipelineDesc setVertexFunction:[[sharedData defaultLibrary] newFunctionWithName:@"display_output_vertex"]];
			[outputPipelineDesc setFragmentFunction:[[sharedData defaultLibrary] newFunctionWithName:@"output_filter_nearest"]];
			break;
			
		case OutputFilterTypeID_BicubicBSpline:
			[outputPipelineDesc setVertexFunction:[[sharedData defaultLibrary] newFunctionWithName:@"display_output_bicubic_vertex"]];
			[outputPipelineDesc setFragmentFunction:[[sharedData defaultLibrary] newFunctionWithName:@"output_filter_bicubic_bspline"]];
			break;
			
		case OutputFilterTypeID_BicubicMitchell:
			[outputPipelineDesc setVertexFunction:[[sharedData defaultLibrary] newFunctionWithName:@"display_output_bicubic_vertex"]];
			[outputPipelineDesc setFragmentFunction:[[sharedData defaultLibrary] newFunctionWithName:@"output_filter_bicubic_mitchell_netravali"]];
			break;
			
		case OutputFilterTypeID_Lanczos2:
			[outputPipelineDesc setVertexFunction:[[sharedData defaultLibrary] newFunctionWithName:@"display_output_bicubic_vertex"]];
			[outputPipelineDesc setFragmentFunction:[[sharedData defaultLibrary] newFunctionWithName:@"output_filter_lanczos2"]];
			break;
			
		case OutputFilterTypeID_Lanczos3:
			[outputPipelineDesc setVertexFunction:[[sharedData defaultLibrary] newFunctionWithName:@"display_output_bicubic_vertex"]];
			[outputPipelineDesc setFragmentFunction:[[sharedData defaultLibrary] newFunctionWithName:@"output_filter_lanczos3"]];
			break;
			
		case OutputFilterTypeID_Bilinear:
		default:
			[outputPipelineDesc setVertexFunction:[[sharedData defaultLibrary] newFunctionWithName:@"display_output_vertex"]];
			[outputPipelineDesc setFragmentFunction:[[sharedData defaultLibrary] newFunctionWithName:@"output_filter_bilinear"]];
			break;
	}
	
	[[[outputPipelineDesc colorAttachments] objectAtIndexedSubscript:0] setPixelFormat:MTLPixelFormatRGBA8Unorm];
	[self setOutputRGBAPipeline:[[sharedData device] newRenderPipelineStateWithDescriptor:outputPipelineDesc error:nil]];
	
	if ([self drawableFormat] != MTLPixelFormatInvalid)
	{
		[[[outputPipelineDesc colorAttachments] objectAtIndexedSubscript:0] setPixelFormat:[self drawableFormat]];
		[self setOutputDrawablePipeline:[[sharedData device] newRenderPipelineStateWithDescriptor:outputPipelineDesc error:nil]];
	}
	
	[outputPipelineDesc release];
}

- (MetalProcessedFrameInfo) processedFrameInfo
{
	OSSpinLockLock(&_spinlockProcessedFrameInfo);
	const MetalProcessedFrameInfo frameInfo = _mpfi;
	OSSpinLockUnlock(&_spinlockProcessedFrameInfo);
	
	return frameInfo;
}

- (id<MTLCommandBuffer>) newCommandBuffer
{
	return [[sharedData commandQueue] commandBufferWithUnretainedReferences];
}

- (void) setup
{
	MTLRenderPipelineDescriptor *outputPipelineDesc = [[MTLRenderPipelineDescriptor alloc] init];
	[outputPipelineDesc setAlphaToOneEnabled:YES];
	[outputPipelineDesc setVertexFunction:[[sharedData defaultLibrary] newFunctionWithName:@"display_output_vertex"]];
	[outputPipelineDesc setFragmentFunction:[[sharedData defaultLibrary] newFunctionWithName:@"output_filter_bilinear"]];
	
	[[[outputPipelineDesc colorAttachments] objectAtIndexedSubscript:0] setPixelFormat:MTLPixelFormatRGBA8Unorm];
	outputRGBAPipeline = [[[sharedData device] newRenderPipelineStateWithDescriptor:outputPipelineDesc error:nil] retain];
	
	if ([self drawableFormat] != MTLPixelFormatInvalid)
	{
		[[[outputPipelineDesc colorAttachments] objectAtIndexedSubscript:0] setPixelFormat:[self drawableFormat]];
		outputDrawablePipeline = [[[sharedData device] newRenderPipelineStateWithDescriptor:outputPipelineDesc error:nil] retain];
	}
	
	[outputPipelineDesc release];
	
	// Set up processing textures.
	[self setTexDisplayPixelScaleMain:nil];
	[self setTexDisplayPixelScaleTouch:nil];
	
	MTLTextureDescriptor *texDisplaySrcDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
																								 width:GPU_FRAMEBUFFER_NATIVE_WIDTH
																								height:GPU_FRAMEBUFFER_NATIVE_HEIGHT
																							 mipmapped:NO];
	[texDisplaySrcDesc setResourceOptions:MTLResourceStorageModePrivate];
	[texDisplaySrcDesc setStorageMode:MTLStorageModePrivate];
	[texDisplaySrcDesc setUsage:MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite];
	
	for (size_t i = 0; i < METAL_BUFFER_COUNT; i++)
	{
		_texDisplaySrcDeposterize[i][NDSDisplayID_Main][0]  = [[sharedData device] newTextureWithDescriptor:texDisplaySrcDesc];
		_texDisplaySrcDeposterize[i][NDSDisplayID_Touch][0] = [[sharedData device] newTextureWithDescriptor:texDisplaySrcDesc];
		_texDisplaySrcDeposterize[i][NDSDisplayID_Main][1]  = [[sharedData device] newTextureWithDescriptor:texDisplaySrcDesc];
		_texDisplaySrcDeposterize[i][NDSDisplayID_Touch][1] = [[sharedData device] newTextureWithDescriptor:texDisplaySrcDesc];
		
		_hudVtxPositionBuffer[i] = [[sharedData device] newBufferWithLength:HUD_VERTEX_ATTRIBUTE_BUFFER_SIZE       options:MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined];
		_hudVtxColorBuffer[i]    = [[sharedData device] newBufferWithLength:HUD_VERTEX_COLOR_ATTRIBUTE_BUFFER_SIZE options:MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined];
		_hudTexCoordBuffer[i]    = [[sharedData device] newBufferWithLength:HUD_VERTEX_ATTRIBUTE_BUFFER_SIZE       options:MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined];
	}
	
	_mpfi.tex[NDSDisplayID_Main]  = [[sharedData texFetchMain]  retain];
	_mpfi.tex[NDSDisplayID_Touch] = [[sharedData texFetchTouch] retain];
	
	VideoFilter *vfMain  = cdp->GetPixelScalerObject(NDSDisplayID_Main);
	_bufCPUFilterSrcMain = [[sharedData device] newBufferWithBytesNoCopy:vfMain->GetSrcBufferPtr()
																  length:vfMain->GetSrcWidth() * vfMain->GetSrcHeight() * sizeof(uint32_t)
																 options:MTLResourceStorageModeShared
															 deallocator:nil];
	
	VideoFilter *vfTouch = cdp->GetPixelScalerObject(NDSDisplayID_Touch);
	_bufCPUFilterSrcTouch = [[sharedData device] newBufferWithBytesNoCopy:vfTouch->GetSrcBufferPtr()
																   length:vfTouch->GetSrcWidth() * vfTouch->GetSrcHeight() * sizeof(uint32_t)
																  options:MTLResourceStorageModeShared
															  deallocator:nil];
	
	texHUDCharMap = nil;
}

- (void) copyHUDFontUsingFace:(const FT_Face &)fontFace
						 size:(const size_t)glyphSize
					 tileSize:(const size_t)glyphTileSize
						 info:(GlyphInfo *)glyphInfo
{
	FT_Error error = FT_Err_Ok;
	size_t texLevel = 0;
	for (size_t tileSize = glyphTileSize; tileSize >= 4; texLevel++, tileSize >>= 1)
	{ }
	
	MTLTextureDescriptor *texHUDCharMapDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
																								 width:16 * glyphTileSize
																								height:16 * glyphTileSize
																							 mipmapped:YES];
	[texHUDCharMapDesc setResourceOptions:MTLResourceStorageModeManaged];
	[texHUDCharMapDesc setStorageMode:MTLStorageModeManaged];
	[texHUDCharMapDesc setCpuCacheMode:MTLCPUCacheModeWriteCombined];
	[texHUDCharMapDesc setUsage:MTLTextureUsageShaderRead];
	[texHUDCharMapDesc setMipmapLevelCount:texLevel];
	
	[self setTexHUDCharMap:[[[sharedData device] newTextureWithDescriptor:texHUDCharMapDesc] autorelease]];
	
	texLevel = 0;
	for (size_t tileSize = glyphTileSize, gSize = glyphSize; tileSize >= 4; texLevel++, tileSize >>= 1, gSize = (GLfloat)tileSize * 0.75f)
	{
		const size_t charMapBufferPixCount = (16 * tileSize) * (16 * tileSize);
		
		const uint32_t fontColor = 0x00FFFFFF;
		uint32_t *charMapBuffer = (uint32_t *)malloc(charMapBufferPixCount * 2 * sizeof(uint32_t));
		for (size_t i = 0; i < charMapBufferPixCount; i++)
		{
			charMapBuffer[i] = fontColor;
		}
		
		error = FT_Set_Char_Size(fontFace, gSize << 6, gSize << 6, 72, 72);
		if (error)
		{
			printf("OGLVideoOutput: FreeType failed to set the font size!\n");
		}
		
		const FT_GlyphSlot glyphSlot = fontFace->glyph;
		
		// Fill the box with a translucent black color.
		for (size_t rowIndex = 0; rowIndex < tileSize; rowIndex++)
		{
			for (size_t pixIndex = 0; pixIndex < tileSize; pixIndex++)
			{
				const uint32_t colorRGBA8888 = 0xFFFFFFFF;
				charMapBuffer[(tileSize + pixIndex) + (rowIndex * (16 * tileSize))] = colorRGBA8888;
			}
		}
		
		// Set up the glyphs.
		for (unsigned char c = 32; c < 255; c++)
		{
			error = FT_Load_Char(fontFace, c, FT_LOAD_RENDER);
			if (error)
			{
				continue;
			}
			
			const uint16_t tileOffsetX = (c & 0x0F) * tileSize;
			const uint16_t tileOffsetY = (c >> 4) * tileSize;
			const uint16_t tileOffsetY_texture = tileOffsetY - (tileSize - gSize + (gSize / 16));
			const uint16_t texSize = tileSize * 16;
			const GLuint glyphWidth = glyphSlot->bitmap.width;
			
			if (tileSize == glyphTileSize)
			{
				GlyphInfo &gi = glyphInfo[c];
				gi.width = (c != ' ') ? glyphWidth : (GLfloat)tileSize / 5.0f;
				gi.texCoord[0] = (GLfloat)tileOffsetX / (GLfloat)texSize;					gi.texCoord[1] = (GLfloat)tileOffsetY / (GLfloat)texSize;
				gi.texCoord[2] = (GLfloat)(tileOffsetX + glyphWidth) / (GLfloat)texSize;	gi.texCoord[3] = (GLfloat)tileOffsetY / (GLfloat)texSize;
				gi.texCoord[4] = (GLfloat)(tileOffsetX + glyphWidth) / (GLfloat)texSize;	gi.texCoord[5] = (GLfloat)(tileOffsetY + tileSize) / (GLfloat)texSize;
				gi.texCoord[6] = (GLfloat)tileOffsetX / (GLfloat)texSize;					gi.texCoord[7] = (GLfloat)(tileOffsetY + tileSize) / (GLfloat)texSize;
			}
			
			// Draw the glyph to the client-side buffer.
			for (size_t rowIndex = 0; rowIndex < glyphSlot->bitmap.rows; rowIndex++)
			{
				for (size_t pixIndex = 0; pixIndex < glyphWidth; pixIndex++)
				{
					const uint32_t colorRGBA8888 = fontColor | ((uint32_t)((uint8_t *)(glyphSlot->bitmap.buffer))[pixIndex + (rowIndex * glyphWidth)] << 24);
					charMapBuffer[(tileOffsetX + pixIndex) + ((tileOffsetY_texture + rowIndex + (tileSize - glyphSlot->bitmap_top)) * (16 * tileSize))] = colorRGBA8888;
				}
			}
		}
		
		[[self texHUDCharMap] replaceRegion:MTLRegionMake2D(0, 0, 16 * tileSize, 16 * tileSize)
								mipmapLevel:texLevel
								  withBytes:charMapBuffer
								bytesPerRow:16 * tileSize * sizeof(uint32_t)];
		
		free(charMapBuffer);
	}
}

- (void) processDisplays
{
	const uint8_t bufferIndex = [sharedData GPUFetchObject]->GetLastFetchIndex();
	const NDSDisplayInfo &fetchDisplayInfo = [sharedData GPUFetchObject]->GetFetchDisplayInfoForBufferIndex(bufferIndex);
	const ClientDisplayMode mode = cdp->GetPresenterProperties().mode;
	const bool useDeposterize = cdp->GetSourceDeposterize();
	const NDSDisplayID selectedDisplaySource[2] = { cdp->GetSelectedDisplaySourceForDisplay(NDSDisplayID_Main), cdp->GetSelectedDisplaySourceForDisplay(NDSDisplayID_Touch) };
	
	id<MTLTexture> texMain  = (selectedDisplaySource[NDSDisplayID_Main]  == NDSDisplayID_Main)  ? [sharedData texFetchMain]  : [sharedData texFetchTouch];
	id<MTLTexture> texTouch = (selectedDisplaySource[NDSDisplayID_Touch] == NDSDisplayID_Touch) ? [sharedData texFetchTouch] : [sharedData texFetchMain];
	
	if ( (fetchDisplayInfo.pixelBytes != 0) && (useDeposterize || (cdp->GetPixelScaler() != VideoFilterTypeID_None)) )
	{
		pthread_mutex_lock(((MacMetalDisplayPresenter *)cdp)->GetMutexProcessPtr());
		
		const bool willFilterOnGPU = cdp->WillFilterOnGPU();
		const bool shouldProcessDisplay[2] = { (!fetchDisplayInfo.didPerformCustomRender[selectedDisplaySource[NDSDisplayID_Main]]  || !fetchDisplayInfo.isCustomSizeRequested) && cdp->IsSelectedDisplayEnabled(NDSDisplayID_Main)  && (mode == ClientDisplayMode_Main  || mode == ClientDisplayMode_Dual),
		                                       (!fetchDisplayInfo.didPerformCustomRender[selectedDisplaySource[NDSDisplayID_Touch]] || !fetchDisplayInfo.isCustomSizeRequested) && cdp->IsSelectedDisplayEnabled(NDSDisplayID_Touch) && (mode == ClientDisplayMode_Touch || mode == ClientDisplayMode_Dual) && (selectedDisplaySource[NDSDisplayID_Main] != selectedDisplaySource[NDSDisplayID_Touch]) };
		
		_processIndex = (_processIndex + 1) % METAL_BUFFER_COUNT;
		id<MTLCommandBuffer> cb = [self newCommandBuffer];
		
		if ( willFilterOnGPU || (useDeposterize && (cdp->GetPixelScaler() == VideoFilterTypeID_None)) )
		{
			id<MTLComputeCommandEncoder> cce = [cb computeCommandEncoder];
			
			// Run the video source filters and the pixel scalers
			if (useDeposterize)
			{
				[cce setComputePipelineState:[sharedData deposterizePipeline]];
				
				if (shouldProcessDisplay[NDSDisplayID_Main] && (texMain != nil))
				{
					[cce setTexture:texMain atIndex:0];
					[cce setTexture:_texDisplaySrcDeposterize[_processIndex][NDSDisplayID_Main][0] atIndex:1];
					[cce dispatchThreadgroups:[sharedData deposterizeThreadGroupsPerGrid]
						threadsPerThreadgroup:[sharedData deposterizeThreadsPerGroup]];
					
					[cce setTexture:_texDisplaySrcDeposterize[_processIndex][NDSDisplayID_Main][0] atIndex:0];
					[cce setTexture:_texDisplaySrcDeposterize[_processIndex][NDSDisplayID_Main][1] atIndex:1];
					[cce dispatchThreadgroups:[sharedData deposterizeThreadGroupsPerGrid]
						threadsPerThreadgroup:[sharedData deposterizeThreadsPerGroup]];
					
					texMain = _texDisplaySrcDeposterize[_processIndex][NDSDisplayID_Main][1];
					
					if (selectedDisplaySource[NDSDisplayID_Main] == selectedDisplaySource[NDSDisplayID_Touch])
					{
						texTouch = texMain;
					}
				}
				
				if (shouldProcessDisplay[NDSDisplayID_Touch] && (texTouch != nil))
				{
					[cce setTexture:texTouch atIndex:0];
					[cce setTexture:_texDisplaySrcDeposterize[_processIndex][NDSDisplayID_Touch][0] atIndex:1];
					[cce dispatchThreadgroups:[sharedData deposterizeThreadGroupsPerGrid]
						threadsPerThreadgroup:[sharedData deposterizeThreadsPerGroup]];
					
					[cce setTexture:_texDisplaySrcDeposterize[_processIndex][NDSDisplayID_Touch][0] atIndex:0];
					[cce setTexture:_texDisplaySrcDeposterize[_processIndex][NDSDisplayID_Touch][1] atIndex:1];
					[cce dispatchThreadgroups:[sharedData deposterizeThreadGroupsPerGrid]
						threadsPerThreadgroup:[sharedData deposterizeThreadsPerGroup]];
					
					texTouch = _texDisplaySrcDeposterize[_processIndex][NDSDisplayID_Touch][1];
				}
			}
			
			// Run the pixel scalers. First attempt on the GPU.
			if (cdp->GetPixelScaler() != VideoFilterTypeID_None)
			{
				[cce setComputePipelineState:[self pixelScalePipeline]];
				
				if (shouldProcessDisplay[NDSDisplayID_Main] && (texMain != nil) && ([self texDisplayPixelScaleMain] != nil))
				{
					[cce setTexture:texMain atIndex:0];
					[cce setTexture:[self texDisplayPixelScaleMain] atIndex:1];
					[cce setTexture:[sharedData texCurrentHQnxLUT] atIndex:2];
					[cce dispatchThreadgroups:_pixelScalerThreadGroupsPerGrid threadsPerThreadgroup:_pixelScalerThreadsPerGroup];
					
					texMain = [self texDisplayPixelScaleMain];
					
					if (selectedDisplaySource[NDSDisplayID_Main] == selectedDisplaySource[NDSDisplayID_Touch])
					{
						texTouch = texMain;
					}
				}
				
				if (shouldProcessDisplay[NDSDisplayID_Touch] && (texTouch != nil) && ([self texDisplayPixelScaleTouch] != nil))
				{
					[cce setTexture:texTouch atIndex:0];
					[cce setTexture:[self texDisplayPixelScaleTouch] atIndex:1];
					[cce setTexture:[sharedData texCurrentHQnxLUT] atIndex:2];
					[cce dispatchThreadgroups:_pixelScalerThreadGroupsPerGrid threadsPerThreadgroup:_pixelScalerThreadsPerGroup];
					
					texTouch = [self texDisplayPixelScaleTouch];
				}
			}
			
			[cce endEncoding];
			[cb commit];
		}
		else
		{
			VideoFilter *vfMain  = cdp->GetPixelScalerObject(NDSDisplayID_Main);
			VideoFilter *vfTouch = cdp->GetPixelScalerObject(NDSDisplayID_Touch);
			
			// Run the video source filters and the pixel scalers
			if (useDeposterize)
			{
				id<MTLComputeCommandEncoder> cce = [cb computeCommandEncoder];
				[cce setComputePipelineState:[sharedData deposterizePipeline]];
				
				dispatch_semaphore_wait(_semProcessBuffers, DISPATCH_TIME_FOREVER);
				
				if (shouldProcessDisplay[NDSDisplayID_Main] && (texMain != nil))
				{
					[cce setTexture:texMain atIndex:0];
					[cce setTexture:_texDisplaySrcDeposterize[_processIndex][NDSDisplayID_Main][0] atIndex:1];
					[cce dispatchThreadgroups:[sharedData deposterizeThreadGroupsPerGrid]
						threadsPerThreadgroup:[sharedData deposterizeThreadsPerGroup]];
					
					[cce setTexture:_texDisplaySrcDeposterize[_processIndex][NDSDisplayID_Main][0] atIndex:0];
					[cce setTexture:_texDisplaySrcDeposterize[_processIndex][NDSDisplayID_Main][1] atIndex:1];
					[cce dispatchThreadgroups:[sharedData deposterizeThreadGroupsPerGrid]
						threadsPerThreadgroup:[sharedData deposterizeThreadsPerGroup]];
					
					texMain = _texDisplaySrcDeposterize[_processIndex][NDSDisplayID_Main][1];
					
					if (selectedDisplaySource[NDSDisplayID_Main] == selectedDisplaySource[NDSDisplayID_Touch])
					{
						texTouch = texMain;
					}
				}
				
				if (shouldProcessDisplay[NDSDisplayID_Touch] && (texTouch != nil))
				{
					[cce setTexture:texTouch atIndex:0];
					[cce setTexture:_texDisplaySrcDeposterize[_processIndex][NDSDisplayID_Touch][0] atIndex:1];
					[cce dispatchThreadgroups:[sharedData deposterizeThreadGroupsPerGrid]
						threadsPerThreadgroup:[sharedData deposterizeThreadsPerGroup]];
					
					[cce setTexture:_texDisplaySrcDeposterize[_processIndex][NDSDisplayID_Touch][0] atIndex:0];
					[cce setTexture:_texDisplaySrcDeposterize[_processIndex][NDSDisplayID_Touch][1] atIndex:1];
					[cce dispatchThreadgroups:[sharedData deposterizeThreadGroupsPerGrid]
						threadsPerThreadgroup:[sharedData deposterizeThreadsPerGroup]];
					
					texTouch = _texDisplaySrcDeposterize[_processIndex][NDSDisplayID_Touch][1];
				}
				
				[cce endEncoding];
				
				id<MTLBlitCommandEncoder> bce = [cb blitCommandEncoder];
				
				// Hybrid CPU/GPU-based path (may cause a performance hit on pixel download)
				if (shouldProcessDisplay[NDSDisplayID_Main])
				{
					[bce copyFromTexture:_texDisplaySrcDeposterize[_processIndex][NDSDisplayID_Main][1]
							 sourceSlice:0
							 sourceLevel:0
							sourceOrigin:MTLOriginMake(0, 0, 0)
							  sourceSize:MTLSizeMake(vfMain->GetSrcWidth(), vfMain->GetSrcHeight(), 1)
								toBuffer:_bufCPUFilterSrcMain
					   destinationOffset:0
				  destinationBytesPerRow:vfMain->GetSrcWidth() * sizeof(uint32_t)
				destinationBytesPerImage:vfMain->GetSrcWidth() * vfMain->GetSrcHeight() * sizeof(uint32_t)];
				}
				
				if (shouldProcessDisplay[NDSDisplayID_Touch])
				{
					[bce copyFromTexture:_texDisplaySrcDeposterize[_processIndex][NDSDisplayID_Touch][1]
							 sourceSlice:0
							 sourceLevel:0
							sourceOrigin:MTLOriginMake(0, 0, 0)
							  sourceSize:MTLSizeMake(vfTouch->GetSrcWidth(), vfTouch->GetSrcHeight(), 1)
								toBuffer:_bufCPUFilterSrcTouch
					   destinationOffset:0
				  destinationBytesPerRow:vfTouch->GetSrcWidth() * sizeof(uint32_t)
				destinationBytesPerImage:vfTouch->GetSrcWidth() * vfTouch->GetSrcHeight() * sizeof(uint32_t)];
				}
				
				[bce endEncoding];
				
				[cb addCompletedHandler:^(id<MTLCommandBuffer> block) {
					dispatch_semaphore_signal(_semProcessBuffers);
				}];
				[cb commit];
			}
			
			// If the pixel scaler didn't already run on the GPU, then run the pixel scaler on the CPU.
			if (cdp->GetPixelScaler() != VideoFilterTypeID_None)
			{
				if (shouldProcessDisplay[NDSDisplayID_Main] && ([self texDisplayPixelScaleMain] != nil))
				{
					vfMain->RunFilter();
					
					[[self texDisplayPixelScaleMain] replaceRegion:MTLRegionMake2D(0, 0, vfMain->GetDstWidth(), vfMain->GetDstHeight())
													   mipmapLevel:0
														 withBytes:vfMain->GetDstBufferPtr()
													   bytesPerRow:vfMain->GetDstWidth() * sizeof(uint32_t)];
					
					texMain = [self texDisplayPixelScaleMain];
					
					if (selectedDisplaySource[NDSDisplayID_Main] == selectedDisplaySource[NDSDisplayID_Touch])
					{
						texTouch = texMain;
					}
				}
				
				if (shouldProcessDisplay[NDSDisplayID_Touch] && ([self texDisplayPixelScaleTouch] != nil))
				{
					vfTouch->RunFilter();
					
					[[self texDisplayPixelScaleTouch] replaceRegion:MTLRegionMake2D(0, 0, vfTouch->GetDstWidth(), vfTouch->GetDstHeight())
														mipmapLevel:0
														  withBytes:vfTouch->GetDstBufferPtr()
														bytesPerRow:vfTouch->GetDstWidth() * sizeof(uint32_t)];
					
					texTouch = [self texDisplayPixelScaleTouch];
				}
			}
		}
		
		pthread_mutex_unlock(((MacMetalDisplayPresenter *)cdp)->GetMutexProcessPtr());
	}
	
	// Update the texture coordinates
	OSSpinLockLock(&_spinlockProcessedFrameInfo);
	
	// Update the frame info
	id<MTLTexture> oldDisplayProcessedMain  = _mpfi.tex[NDSDisplayID_Main];
	id<MTLTexture> oldDisplayProcessedTouch = _mpfi.tex[NDSDisplayID_Touch];
	
	_mpfi.processIndex = _processIndex;
	_mpfi.tex[NDSDisplayID_Main]  = [texMain  retain];
	_mpfi.tex[NDSDisplayID_Touch] = [texTouch retain];
	
	OSSpinLockUnlock(&_spinlockProcessedFrameInfo);
	
	[oldDisplayProcessedMain  release];
	[oldDisplayProcessedTouch release];
}

- (void) updateRenderBuffers
{
	// Set up the view properties.
	bool needEncodeViewport = false;
	
	MTLViewport newViewport;
	newViewport.originX = 0.0;
	newViewport.originY = 0.0;
	newViewport.width  = cdp->GetPresenterProperties().clientWidth;
	newViewport.height = cdp->GetPresenterProperties().clientHeight;
	newViewport.znear = 0.0;
	newViewport.zfar = 1.0;
	
	dispatch_semaphore_wait(_semRenderBuffers, DISPATCH_TIME_FOREVER);
	const uint8_t renderIndex = (_mrfi.renderIndex + 1) % METAL_BUFFER_COUNT;
	
	if ([self needsViewportUpdate])
	{
		needEncodeViewport = true;
		
		_cdvPropertiesBuffer.width    = cdp->GetPresenterProperties().clientWidth;
		_cdvPropertiesBuffer.height   = cdp->GetPresenterProperties().clientHeight;
		
		[self setNeedsViewportUpdate:NO];
	}
	
	if ([self needsRotationScaleUpdate])
	{
		_cdvPropertiesBuffer.rotation            = cdp->GetPresenterProperties().rotation;
		_cdvPropertiesBuffer.viewScale           = cdp->GetPresenterProperties().viewScale;
		_cdvPropertiesBuffer.lowerHUDMipMapLevel = ( ((float)HUD_TEXTBOX_BASE_SCALE * cdp->GetHUDObjectScale() / cdp->GetScaleFactor()) >= (2.0/3.0) ) ? 0 : 1;
		
		[self setNeedsRotationScaleUpdate:NO];
	}
	
	// Set up the display properties.
	if ([self needsScreenVerticesUpdate])
	{
		cdp->SetScreenVertices(_vtxPositionBuffer);
		[self setNeedsScreenVerticesUpdate:NO];
	}
	
	// Set up the HUD properties.
	size_t hudTotalLength = cdp->GetHUDString().length();
	size_t hudTouchLineLength = 0;
	bool willDrawHUD = cdp->GetHUDVisibility() && ([self texHUDCharMap] != nil);
	
	if (cdp->GetHUDShowInput())
	{
		hudTotalLength += HUD_INPUT_ELEMENT_LENGTH;
		
		switch (cdp->GetMode())
		{
			case ClientDisplayMode_Main:
			case ClientDisplayMode_Touch:
				hudTouchLineLength = HUD_INPUT_TOUCH_LINE_ELEMENTS / 2;
				break;
				
			case ClientDisplayMode_Dual:
			{
				switch (cdp->GetLayout())
				{
					case ClientDisplayLayout_Vertical:
					case ClientDisplayLayout_Horizontal:
						hudTouchLineLength = HUD_INPUT_TOUCH_LINE_ELEMENTS / 2;
						break;
						
					case ClientDisplayLayout_Hybrid_2_1:
					case ClientDisplayLayout_Hybrid_16_9:
					case ClientDisplayLayout_Hybrid_16_10:
						hudTouchLineLength = HUD_INPUT_TOUCH_LINE_ELEMENTS;
						break;
				}
				
				break;
			}
		}
		
		hudTotalLength += hudTouchLineLength;
	}
	
	willDrawHUD = willDrawHUD && (hudTotalLength > 1);
	
	if (willDrawHUD && cdp->HUDNeedsUpdate())
	{
		cdp->SetHUDPositionVertices((float)cdp->GetPresenterProperties().clientWidth, (float)cdp->GetPresenterProperties().clientHeight, (float *)[_hudVtxPositionBuffer[renderIndex] contents]);
		cdp->SetHUDColorVertices((uint32_t *)[_hudVtxColorBuffer[renderIndex] contents]);
		cdp->SetHUDTextureCoordinates((float *)[_hudTexCoordBuffer[renderIndex] contents]);
		
		cdp->ClearHUDNeedsUpdate();
	}
	
	_mrfi.renderIndex = renderIndex;
	_mrfi.needEncodeViewport = needEncodeViewport;
	_mrfi.newViewport = newViewport;
	_mrfi.willDrawHUD = willDrawHUD;
	_mrfi.willDrawHUDInput = cdp->GetHUDShowInput();
	_mrfi.hudStringLength = cdp->GetHUDString().length();
	_mrfi.hudTouchLineLength = hudTouchLineLength;
	_mrfi.hudTotalLength = hudTotalLength;
}

- (void) renderForCommandBuffer:(id<MTLCommandBuffer>)cb
			outputPipelineState:(id<MTLRenderPipelineState>)outputPipelineState
			   hudPipelineState:(id<MTLRenderPipelineState>)hudPipelineState
				 texDisplayMain:(id<MTLTexture>)texDisplayMain
				texDisplayTouch:(id<MTLTexture>)texDisplayTouch
{
	// Update the buffers for rendering the frame.
	[self updateRenderBuffers];
	
	// Generate the command encoder.
	id<MTLRenderCommandEncoder> rce = [cb renderCommandEncoderWithDescriptor:_outputRenderPassDesc];
	
	cdp->SetScreenTextureCoordinates((float)[texDisplayMain  width], (float)[texDisplayMain  height],
									 (float)[texDisplayTouch width], (float)[texDisplayTouch height],
									 _texCoordBuffer);
	
	if (_mrfi.needEncodeViewport)
	{
		[rce setViewport:_mrfi.newViewport];
	}
	
	// Draw the NDS displays.
	const NDSDisplayInfo &displayInfo = cdp->GetEmuDisplayInfo();
	const float backlightIntensity[2] = { displayInfo.backlightIntensity[NDSDisplayID_Main], displayInfo.backlightIntensity[NDSDisplayID_Touch] };
	
	[rce setRenderPipelineState:outputPipelineState];
	[rce setVertexBytes:_vtxPositionBuffer length:sizeof(_vtxPositionBuffer) atIndex:0];
	[rce setVertexBytes:_texCoordBuffer length:sizeof(_texCoordBuffer) atIndex:1];
	[rce setVertexBytes:&_cdvPropertiesBuffer length:sizeof(_cdvPropertiesBuffer) atIndex:2];
	
	switch (cdp->GetPresenterProperties().mode)
	{
		case ClientDisplayMode_Main:
		{
			if ( (texDisplayMain != nil) && cdp->IsSelectedDisplayEnabled(NDSDisplayID_Main) )
			{
				[rce setFragmentBytes:&backlightIntensity[NDSDisplayID_Main] length:sizeof(backlightIntensity[NDSDisplayID_Main]) atIndex:0];
				[rce setFragmentTexture:texDisplayMain atIndex:0];
				[rce drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
			}
			break;
		}
			
		case ClientDisplayMode_Touch:
		{
			if ( (texDisplayTouch != nil) && cdp->IsSelectedDisplayEnabled(NDSDisplayID_Touch) )
			{
				[rce setFragmentBytes:&backlightIntensity[NDSDisplayID_Touch] length:sizeof(backlightIntensity[NDSDisplayID_Touch]) atIndex:0];
				[rce setFragmentTexture:texDisplayTouch atIndex:0];
				[rce drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:4 vertexCount:4];
			}
			break;
		}
			
		case ClientDisplayMode_Dual:
		{
			const NDSDisplayID majorDisplayID = (cdp->GetPresenterProperties().order == ClientDisplayOrder_MainFirst) ? NDSDisplayID_Main : NDSDisplayID_Touch;
			const size_t majorDisplayVtx = (cdp->GetPresenterProperties().order == ClientDisplayOrder_MainFirst) ? 8 : 12;
			
			switch (cdp->GetPresenterProperties().layout)
			{
				case ClientDisplayLayout_Hybrid_2_1:
				case ClientDisplayLayout_Hybrid_16_9:
				case ClientDisplayLayout_Hybrid_16_10:
				{
					if ( (((majorDisplayID == NDSDisplayID_Main) && (texDisplayMain != nil)) || ((majorDisplayID == NDSDisplayID_Touch) && (texDisplayTouch != nil))) && cdp->IsSelectedDisplayEnabled(majorDisplayID) )
					{
						[rce setFragmentBytes:&backlightIntensity[majorDisplayID] length:sizeof(backlightIntensity[majorDisplayID]) atIndex:0];
						[rce setFragmentTexture:((majorDisplayID == NDSDisplayID_Main) ? texDisplayMain : texDisplayTouch) atIndex:0];
						[rce drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:majorDisplayVtx vertexCount:4];
					}
					break;
				}
					
				default:
					break;
			}
			
			if ( (texDisplayMain != nil) && cdp->IsSelectedDisplayEnabled(NDSDisplayID_Main) )
			{
				[rce setFragmentBytes:&backlightIntensity[NDSDisplayID_Main] length:sizeof(backlightIntensity[NDSDisplayID_Main]) atIndex:0];
				[rce setFragmentTexture:texDisplayMain atIndex:0];
				[rce drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
			}
			
			if ( (texDisplayTouch != nil) && cdp->IsSelectedDisplayEnabled(NDSDisplayID_Touch) )
			{
				[rce setFragmentBytes:&backlightIntensity[NDSDisplayID_Touch] length:sizeof(backlightIntensity[NDSDisplayID_Touch]) atIndex:0];
				[rce setFragmentTexture:texDisplayTouch atIndex:0];
				[rce drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:4 vertexCount:4];
			}
		}
			
		default:
			break;
	}
	
	// Draw the HUD.
	if (_mrfi.willDrawHUD)
	{
		uint8_t isScreenOverlay = 0;
		
		[rce setRenderPipelineState:hudPipelineState];
		[rce setVertexBuffer:_hudVtxPositionBuffer[_mrfi.renderIndex] offset:0 atIndex:0];
		[rce setVertexBuffer:_hudVtxColorBuffer[_mrfi.renderIndex]    offset:0 atIndex:1];
		[rce setVertexBuffer:_hudTexCoordBuffer[_mrfi.renderIndex]    offset:0 atIndex:2];
		[rce setVertexBytes:&_cdvPropertiesBuffer length:sizeof(_cdvPropertiesBuffer) atIndex:3];
		[rce setFragmentTexture:[self texHUDCharMap] atIndex:0];
		
		// First, draw the inputs.
		if (_mrfi.willDrawHUDInput)
		{
			isScreenOverlay = 1;
			[rce setVertexBytes:&isScreenOverlay length:sizeof(uint8_t) atIndex:4];
			[rce setFragmentSamplerState:[sharedData samplerHUDBox] atIndex:0];
			[rce drawIndexedPrimitives:MTLPrimitiveTypeTriangle
							indexCount:_mrfi.hudTouchLineLength * 6
							 indexType:MTLIndexTypeUInt16
						   indexBuffer:[sharedData hudIndexBuffer]
					 indexBufferOffset:(_mrfi.hudStringLength + HUD_INPUT_ELEMENT_LENGTH) * 6 * sizeof(uint16_t)];
			
			isScreenOverlay = 0;
			[rce setVertexBytes:&isScreenOverlay length:sizeof(uint8_t) atIndex:4];
			[rce setFragmentSamplerState:[sharedData samplerHUDText] atIndex:0];
			[rce drawIndexedPrimitives:MTLPrimitiveTypeTriangle
							indexCount:HUD_INPUT_ELEMENT_LENGTH * 6
							 indexType:MTLIndexTypeUInt16
						   indexBuffer:[sharedData hudIndexBuffer]
					 indexBufferOffset:_mrfi.hudStringLength * 6 * sizeof(uint16_t)];
		}
		
		// Next, draw the backing text box.
		[rce setVertexBytes:&isScreenOverlay length:sizeof(uint8_t) atIndex:4];
		[rce setFragmentSamplerState:[sharedData samplerHUDBox] atIndex:0];
		[rce drawIndexedPrimitives:MTLPrimitiveTypeTriangle
						indexCount:6
						 indexType:MTLIndexTypeUInt16
					   indexBuffer:[sharedData hudIndexBuffer]
				 indexBufferOffset:0];
		
		// Finally, draw each character inside the box.
		[rce setFragmentSamplerState:[sharedData samplerHUDText] atIndex:0];
		[rce drawIndexedPrimitives:MTLPrimitiveTypeTriangle
						indexCount:(_mrfi.hudStringLength - 1) * 6
						 indexType:MTLIndexTypeUInt16
					   indexBuffer:[sharedData hudIndexBuffer]
				 indexBufferOffset:6 * sizeof(uint16_t)];
	}
	
	[rce endEncoding];
}

- (void) renderFinish
{
	dispatch_semaphore_signal(_semRenderBuffers);
}

- (void) renderToBuffer:(uint32_t *)dstBuffer
{
	const size_t clientWidth  = cdp->GetPresenterProperties().clientWidth;
	const size_t clientHeight = cdp->GetPresenterProperties().clientHeight;
	
	task_t renderTask = mach_task_self();
	semaphore_t semRenderToBuffer = 0;
	semaphore_create(renderTask, &semRenderToBuffer, SYNC_POLICY_PREPOST, 0);
	
	@autoreleasepool
	{
		MTLTextureDescriptor *texRenderDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
																								 width:clientWidth
																								height:clientHeight
																							 mipmapped:NO];
		[texRenderDesc setResourceOptions:MTLResourceStorageModePrivate];
		[texRenderDesc setStorageMode:MTLStorageModePrivate];
		[texRenderDesc setUsage:MTLTextureUsageRenderTarget];
		
		id<MTLTexture> texRender = [[sharedData device] newTextureWithDescriptor:texRenderDesc];
		id<MTLBuffer> dstMTLBuffer = [[sharedData device] newBufferWithLength:clientWidth * clientHeight * sizeof(uint32_t) options:MTLResourceStorageModeManaged];
		
		// Now that everything is set up, go ahead and draw everything.
		[colorAttachment0Desc setTexture:texRender];
		id<MTLCommandBuffer> cb = [self newCommandBuffer];
		
		const MetalProcessedFrameInfo processedInfo = [self processedFrameInfo];
		
		[self renderForCommandBuffer:cb
				 outputPipelineState:[self outputRGBAPipeline]
					hudPipelineState:[sharedData hudRGBAPipeline]
					  texDisplayMain:processedInfo.tex[NDSDisplayID_Main]
					 texDisplayTouch:processedInfo.tex[NDSDisplayID_Touch]];
		
		id<MTLBlitCommandEncoder> bce = [cb blitCommandEncoder];
		
		[bce copyFromTexture:texRender
				 sourceSlice:0
				 sourceLevel:0
				sourceOrigin:MTLOriginMake(0, 0, 0)
				  sourceSize:MTLSizeMake(clientWidth, clientHeight, 1)
					toBuffer:dstMTLBuffer
		   destinationOffset:0
	  destinationBytesPerRow:clientWidth * sizeof(uint32_t)
	destinationBytesPerImage:clientWidth * clientHeight * sizeof(uint32_t)];
		
		[bce synchronizeResource:dstMTLBuffer];
		[bce endEncoding];
		
		[cb addCompletedHandler:^(id<MTLCommandBuffer> block) {
			[self renderFinish];
			semaphore_signal(semRenderToBuffer);
		}];
		[cb commit];
		
		// Wait on this thread until the GPU completes its task, then continue execution on this thread.
		semaphore_wait(semRenderToBuffer);
		
		memcpy(dstBuffer, [dstMTLBuffer contents], clientWidth * clientHeight * sizeof(uint32_t));
		
		[texRender release];
		[dstMTLBuffer release];
	}
	
	semaphore_destroy(renderTask, semRenderToBuffer);
}

@end

@implementation DisplayViewMetalLayer

@synthesize _cdv;
@synthesize presenterObject;

- (id) initWithDisplayPresenterObject:(MacMetalDisplayPresenterObject *)thePresenterObject
{
	self = [super init];
	if(self == nil)
	{
		return nil;
	}
	
	_cdv = NULL;
	
	presenterObject = thePresenterObject;
	if (thePresenterObject != nil)
	{
		[self setDevice:[[thePresenterObject sharedData] device]];
		[thePresenterObject setDrawableFormat:[self pixelFormat]];
	}
	
	[self setOpaque:YES];
	
	return self;
}

- (void) setupLayer
{
	if ([self device] == nil)
	{
		[self setDevice:[[presenterObject sharedData] device]];
		[presenterObject setDrawableFormat:[self pixelFormat]];
	}
}

- (void) renderToDrawable
{
	@autoreleasepool
	{
		// Now that everything is set up, go ahead and draw everything.
		id<CAMetalDrawable> layerDrawable = [self nextDrawable];
		[[presenterObject colorAttachment0Desc] setTexture:[layerDrawable texture]];
		id<MTLCommandBuffer> cb = [presenterObject newCommandBuffer];
		
		const MetalProcessedFrameInfo processedInfo = [presenterObject processedFrameInfo];
		
		[presenterObject renderForCommandBuffer:cb
							outputPipelineState:[presenterObject outputDrawablePipeline]
							   hudPipelineState:[[presenterObject sharedData] hudPipeline]
								 texDisplayMain:processedInfo.tex[NDSDisplayID_Main]
								texDisplayTouch:processedInfo.tex[NDSDisplayID_Touch]];
		
		[cb presentDrawable:layerDrawable];
		[cb addCompletedHandler:^(id<MTLCommandBuffer> block) {
			[presenterObject renderFinish];
		}];
		[cb commit];
	}
}

@end

#pragma mark -

MacMetalFetchObject::MacMetalFetchObject()
{
	_useDirectToCPUFilterPipeline = true;
	
	pthread_rwlock_init(&_srcCloneRWLock[NDSDisplayID_Main][0],  NULL);
	pthread_rwlock_init(&_srcCloneRWLock[NDSDisplayID_Touch][0], NULL);
	pthread_rwlock_init(&_srcCloneRWLock[NDSDisplayID_Main][1],  NULL);
	pthread_rwlock_init(&_srcCloneRWLock[NDSDisplayID_Touch][1], NULL);
	
	_srcNativeCloneMaster = (uint32_t *)malloc_alignedPage(GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * 2 * 2 * sizeof(uint32_t));
	_srcNativeClone[NDSDisplayID_Main][0]  = _srcNativeCloneMaster + (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * 0);
	_srcNativeClone[NDSDisplayID_Touch][0] = _srcNativeCloneMaster + (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * 1);
	_srcNativeClone[NDSDisplayID_Main][1]  = _srcNativeCloneMaster + (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * 2);
	_srcNativeClone[NDSDisplayID_Touch][1] = _srcNativeCloneMaster + (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * 3);
	memset(_srcNativeCloneMaster, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * 2 * sizeof(uint32_t));
	
	_clientData = [[MetalDisplayViewSharedData alloc] init];
}

MacMetalFetchObject::~MacMetalFetchObject()
{
	[(MetalDisplayViewSharedData *)this->_clientData release];
	
	pthread_rwlock_wrlock(&this->_srcCloneRWLock[NDSDisplayID_Main][0]);
	pthread_rwlock_wrlock(&this->_srcCloneRWLock[NDSDisplayID_Touch][0]);
	pthread_rwlock_wrlock(&this->_srcCloneRWLock[NDSDisplayID_Main][1]);
	pthread_rwlock_wrlock(&this->_srcCloneRWLock[NDSDisplayID_Touch][1]);
	free_aligned(this->_srcNativeCloneMaster);
	this->_srcNativeCloneMaster = NULL;
	this->_srcNativeClone[NDSDisplayID_Main][0]  = NULL;
	this->_srcNativeClone[NDSDisplayID_Touch][0] = NULL;
	this->_srcNativeClone[NDSDisplayID_Main][1]  = NULL;
	this->_srcNativeClone[NDSDisplayID_Touch][1] = NULL;
	pthread_rwlock_unlock(&this->_srcCloneRWLock[NDSDisplayID_Touch][1]);
	pthread_rwlock_unlock(&this->_srcCloneRWLock[NDSDisplayID_Main][1]);
	pthread_rwlock_unlock(&this->_srcCloneRWLock[NDSDisplayID_Touch][0]);
	pthread_rwlock_unlock(&this->_srcCloneRWLock[NDSDisplayID_Main][0]);
	
	pthread_rwlock_destroy(&this->_srcCloneRWLock[NDSDisplayID_Main][0]);
	pthread_rwlock_destroy(&this->_srcCloneRWLock[NDSDisplayID_Touch][0]);
	pthread_rwlock_destroy(&this->_srcCloneRWLock[NDSDisplayID_Main][1]);
	pthread_rwlock_destroy(&this->_srcCloneRWLock[NDSDisplayID_Touch][1]);
}

void MacMetalFetchObject::Init()
{
	[(MacClientSharedObject *)this->_clientData setGPUFetchObject:this];
}

void MacMetalFetchObject::CopyFromSrcClone(uint32_t *dstBufferPtr, const NDSDisplayID displayID, const u8 bufferIndex)
{
	pthread_rwlock_rdlock(&this->_srcCloneRWLock[displayID][bufferIndex]);
	memcpy(dstBufferPtr, this->_srcNativeClone[displayID][bufferIndex], GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(uint32_t));
	pthread_rwlock_unlock(&this->_srcCloneRWLock[displayID][bufferIndex]);
}

void MacMetalFetchObject::SetFetchBuffers(const NDSDisplayInfo &currentDisplayInfo)
{
	const size_t nativeSize = GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * currentDisplayInfo.pixelBytes;
	const size_t customSize = currentDisplayInfo.customWidth * currentDisplayInfo.customHeight * currentDisplayInfo.pixelBytes;
	
	this->_fetchDisplayInfo[0] = currentDisplayInfo;
	this->_fetchDisplayInfo[1] = currentDisplayInfo;
	
	this->_fetchDisplayInfo[0].nativeBuffer[NDSDisplayID_Main]  = (u8 *)currentDisplayInfo.masterFramebufferHead;
	this->_fetchDisplayInfo[0].nativeBuffer[NDSDisplayID_Touch] = (u8 *)currentDisplayInfo.masterFramebufferHead + (nativeSize * 1);
	this->_fetchDisplayInfo[0].customBuffer[NDSDisplayID_Main]  = (u8 *)currentDisplayInfo.masterFramebufferHead + (nativeSize * 2);
	this->_fetchDisplayInfo[0].customBuffer[NDSDisplayID_Touch] = (u8 *)currentDisplayInfo.masterFramebufferHead + (nativeSize * 2) + customSize;
	
	this->_fetchDisplayInfo[1].nativeBuffer[NDSDisplayID_Main]  = (u8 *)this->_fetchDisplayInfo[0].nativeBuffer[NDSDisplayID_Main]  + currentDisplayInfo.framebufferSize;
	this->_fetchDisplayInfo[1].nativeBuffer[NDSDisplayID_Touch] = (u8 *)this->_fetchDisplayInfo[0].nativeBuffer[NDSDisplayID_Touch] + currentDisplayInfo.framebufferSize;
	this->_fetchDisplayInfo[1].customBuffer[NDSDisplayID_Main]  = (u8 *)this->_fetchDisplayInfo[0].customBuffer[NDSDisplayID_Main]  + currentDisplayInfo.framebufferSize;
	this->_fetchDisplayInfo[1].customBuffer[NDSDisplayID_Touch] = (u8 *)this->_fetchDisplayInfo[0].customBuffer[NDSDisplayID_Touch] + currentDisplayInfo.framebufferSize;
	
	[(MetalDisplayViewSharedData *)this->_clientData setFetchBuffersWithDisplayInfo:currentDisplayInfo];
}

void MacMetalFetchObject::FetchFromBufferIndex(const u8 index)
{
	MacClientSharedObject *sharedViewObject = (MacClientSharedObject *)this->_clientData;
	this->_useDirectToCPUFilterPipeline = ([sharedViewObject numberViewsUsingDirectToCPUFiltering] > 0);
	
	@autoreleasepool
	{
		[(MetalDisplayViewSharedData *)this->_clientData fetchFromBufferIndex:index];
	}
}

void MacMetalFetchObject::_FetchNativeDisplayByID(const NDSDisplayID displayID, const u8 bufferIndex)
{	
	if (this->_useDirectToCPUFilterPipeline)
	{
		GPU->PostprocessDisplay(displayID, this->_fetchDisplayInfo[bufferIndex]);
		
		pthread_rwlock_wrlock(&this->_srcCloneRWLock[displayID][bufferIndex]);
		
		if (this->_fetchDisplayInfo[bufferIndex].pixelBytes == 2)
		{
			ColorspaceConvertBuffer555To8888Opaque<false, false>((const uint16_t *)this->_fetchDisplayInfo[bufferIndex].nativeBuffer[displayID], this->_srcNativeClone[displayID][bufferIndex], GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
		}
		else
		{
			ColorspaceConvertBuffer888XTo8888Opaque<false, false>((const uint32_t *)this->_fetchDisplayInfo[bufferIndex].nativeBuffer[displayID], this->_srcNativeClone[displayID][bufferIndex], GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
		}
		
		pthread_rwlock_unlock(&this->_srcCloneRWLock[displayID][bufferIndex]);
	}
	
	[(MetalDisplayViewSharedData *)this->_clientData fetchNativeDisplayByID:displayID bufferIndex:bufferIndex];
}

void MacMetalFetchObject::_FetchCustomDisplayByID(const NDSDisplayID displayID, const u8 bufferIndex)
{
	[(MetalDisplayViewSharedData *)this->_clientData fetchCustomDisplayByID:displayID bufferIndex:bufferIndex];
}

#pragma mark -

MacMetalDisplayPresenter::MacMetalDisplayPresenter()
{
	__InstanceInit(nil);
}

MacMetalDisplayPresenter::MacMetalDisplayPresenter(MacClientSharedObject *sharedObject) : MacDisplayPresenterInterface(sharedObject)
{
	__InstanceInit(sharedObject);
}

MacMetalDisplayPresenter::~MacMetalDisplayPresenter()
{
	[this->_presenterObject release];
	
	pthread_mutex_destroy(&this->_mutexProcessPtr);
	
	dispatch_release(this->_semCPUFilter[NDSDisplayID_Main][0]);
	dispatch_release(this->_semCPUFilter[NDSDisplayID_Main][1]);
	dispatch_release(this->_semCPUFilter[NDSDisplayID_Touch][0]);
	dispatch_release(this->_semCPUFilter[NDSDisplayID_Touch][1]);
}

void MacMetalDisplayPresenter::__InstanceInit(MacClientSharedObject *sharedObject)
{
	_canFilterOnGPU = true;
	_filtersPreferGPU = true;
	_willFilterOnGPU = true;
	
	if (sharedObject != nil)
	{
		SetFetchObject([sharedObject GPUFetchObject]);
	}
	
	_presenterObject = [[MacMetalDisplayPresenterObject alloc] initWithDisplayPresenter:this];
	
	pthread_mutex_init(&_mutexProcessPtr, NULL);
	
	_semCPUFilter[NDSDisplayID_Main][0]  = dispatch_semaphore_create(1);
	_semCPUFilter[NDSDisplayID_Main][1]  = dispatch_semaphore_create(1);
	_semCPUFilter[NDSDisplayID_Touch][0] = dispatch_semaphore_create(1);
	_semCPUFilter[NDSDisplayID_Touch][1] = dispatch_semaphore_create(1);
}

void MacMetalDisplayPresenter::_UpdateNormalSize()
{
	[this->_presenterObject setNeedsScreenVerticesUpdate:YES];
}

void MacMetalDisplayPresenter::_UpdateOrder()
{
	[this->_presenterObject setNeedsScreenVerticesUpdate:YES];
}

void MacMetalDisplayPresenter::_UpdateRotation()
{
	[this->_presenterObject setNeedsRotationScaleUpdate:YES];
}

void MacMetalDisplayPresenter::_UpdateClientSize()
{
	[this->_presenterObject setNeedsViewportUpdate:YES];
	[this->_presenterObject setNeedsHUDVerticesUpdate:YES];
	this->ClientDisplay3DPresenter::_UpdateClientSize();
}

void MacMetalDisplayPresenter::_UpdateViewScale()
{
	this->ClientDisplay3DPresenter::_UpdateViewScale();
	[this->_presenterObject setNeedsRotationScaleUpdate:YES];
}

void MacMetalDisplayPresenter::_LoadNativeDisplayByID(const NDSDisplayID displayID)
{
	if ((this->GetPixelScaler() != VideoFilterTypeID_None) && !this->WillFilterOnGPU() && !this->GetSourceDeposterize())
	{
		MacMetalFetchObject &fetchObjMutable = (MacMetalFetchObject &)this->GetFetchObject();
		VideoFilter *vf = this->GetPixelScalerObject(displayID);
		
		const uint8_t bufferIndex = fetchObjMutable.GetLastFetchIndex();
		
		dispatch_semaphore_wait(this->_semCPUFilter[displayID][bufferIndex], DISPATCH_TIME_FOREVER);
		fetchObjMutable.CopyFromSrcClone(vf->GetSrcBufferPtr(), displayID, bufferIndex);
		dispatch_semaphore_signal(this->_semCPUFilter[displayID][bufferIndex]);
	}
}

MacMetalDisplayPresenterObject* MacMetalDisplayPresenter::GetPresenterObject() const
{
	return this->_presenterObject;
}

pthread_mutex_t* MacMetalDisplayPresenter::GetMutexProcessPtr()
{
	return &this->_mutexProcessPtr;
}

dispatch_semaphore_t MacMetalDisplayPresenter::GetCPUFilterSemaphore(const NDSDisplayID displayID, const uint8_t bufferIndex)
{
	return this->_semCPUFilter[displayID][bufferIndex];
}

void MacMetalDisplayPresenter::Init()
{
	[this->_presenterObject setup];
}

void MacMetalDisplayPresenter::CopyHUDFont(const FT_Face &fontFace, const size_t glyphSize, const size_t glyphTileSize, GlyphInfo *glyphInfo)
{
	[this->_presenterObject copyHUDFontUsingFace:fontFace size:glyphSize tileSize:glyphTileSize info:glyphInfo];
}

void MacMetalDisplayPresenter::SetSharedData(MacClientSharedObject *sharedObject)
{
	[this->_presenterObject setSharedData:(MetalDisplayViewSharedData *)sharedObject];
	this->SetFetchObject([sharedObject GPUFetchObject]);
}

// NDS screen filters
void MacMetalDisplayPresenter::SetPixelScaler(const VideoFilterTypeID filterID)
{
	pthread_mutex_lock(&this->_mutexProcessPtr);
	
	this->ClientDisplay3DPresenter::SetPixelScaler(filterID);
	[this->_presenterObject setPixelScaler:this->_pixelScaler];
	this->_willFilterOnGPU = (this->GetFiltersPreferGPU()) ? ([this->_presenterObject pixelScalePipeline] != nil) : false;
	
	pthread_mutex_unlock(&this->_mutexProcessPtr);
}

void MacMetalDisplayPresenter::SetOutputFilter(const OutputFilterTypeID filterID)
{
	this->ClientDisplay3DPresenter::SetOutputFilter(filterID);
	[this->_presenterObject setOutputFilter:filterID];
}

void MacMetalDisplayPresenter::SetFiltersPreferGPU(const bool preferGPU)
{
	pthread_mutex_lock(&this->_mutexProcessPtr);
	
	this->_filtersPreferGPU = preferGPU;
	this->_willFilterOnGPU = (preferGPU) ? ([this->_presenterObject pixelScalePipeline] != nil) : false;
	
	pthread_mutex_unlock(&this->_mutexProcessPtr);
}

// NDS GPU interface
void MacMetalDisplayPresenter::ProcessDisplays()
{
	[this->_presenterObject processDisplays];
}

void MacMetalDisplayPresenter::CopyFrameToBuffer(uint32_t *dstBuffer)
{
	[this->_presenterObject renderToBuffer:dstBuffer];
}

#pragma mark -

MacMetalDisplayView::MacMetalDisplayView()
{
	__InstanceInit(nil);
}

MacMetalDisplayView::MacMetalDisplayView(MacClientSharedObject *sharedObject)
{
	__InstanceInit(sharedObject);
}

MacMetalDisplayView::~MacMetalDisplayView()
{
	[this->_caLayer release];
}

void MacMetalDisplayView::__InstanceInit(MacClientSharedObject *sharedObject)
{
	_allowViewUpdates = false;
	_spinlockViewNeedsFlush = OS_SPINLOCK_INIT;
	
	MacMetalDisplayPresenter *newMetalPresenter = new MacMetalDisplayPresenter(sharedObject);
	_presenter = newMetalPresenter;
	
	_caLayer = [[DisplayViewMetalLayer alloc] initWithDisplayPresenterObject:newMetalPresenter->GetPresenterObject()];
	[_caLayer setClientDisplayView:this];
}

void MacMetalDisplayView::Init()
{
	[(DisplayViewMetalLayer *)this->_caLayer setupLayer];
	MacDisplayLayeredView::Init();
}

bool MacMetalDisplayView::GetViewNeedsFlush()
{
	OSSpinLockLock(&this->_spinlockViewNeedsFlush);
	const bool viewNeedsFlush = this->_viewNeedsFlush;
	OSSpinLockUnlock(&this->_spinlockViewNeedsFlush);
	
	return viewNeedsFlush;
}

void MacMetalDisplayView::SetViewNeedsFlush()
{
	if (!this->_allowViewUpdates || (this->_presenter == nil) || (this->_caLayer == nil))
	{
		return;
	}
	
	// For every update, ensure that the CVDisplayLink is started so that the update
	// will eventually get flushed.
	this->SetAllowViewFlushes(true);
	
	this->_presenter->UpdateLayout();
	
	OSSpinLockLock(&this->_spinlockViewNeedsFlush);
	this->_viewNeedsFlush = true;
	OSSpinLockUnlock(&this->_spinlockViewNeedsFlush);
}

void MacMetalDisplayView::SetAllowViewFlushes(bool allowFlushes)
{
	CGDirectDisplayID displayID = (CGDirectDisplayID)this->GetDisplayViewID();
	MacClientSharedObject *sharedData = ((MacMetalDisplayPresenter *)this->_presenter)->GetSharedData();
	[sharedData displayLinkStartUsingID:displayID];
}

void MacMetalDisplayView::FlushView()
{
	OSSpinLockLock(&this->_spinlockViewNeedsFlush);
	this->_viewNeedsFlush = false;
	OSSpinLockUnlock(&this->_spinlockViewNeedsFlush);
	
	[(DisplayViewMetalLayer *)this->_caLayer renderToDrawable];
}

#pragma mark -
void SetupHQnxLUTs_Metal(id<MTLDevice> &device, id<MTLCommandQueue> &commandQueue, id<MTLTexture> &texLQ2xLUT, id<MTLTexture> &texHQ2xLUT, id<MTLTexture> &texHQ3xLUT, id<MTLTexture> &texHQ4xLUT)
{
	InitHQnxLUTs();
	
	// Create the MTLBuffer objects to wrap the the existing LUT buffers that are already in memory.
	id<MTLBuffer> bufLQ2xLUT = [device newBufferWithBytesNoCopy:_LQ2xLUT
														 length:256 * 2 * 4  * 16 * sizeof(uint32_t)
														options:MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined
													deallocator:nil];
	
	id<MTLBuffer> bufHQ2xLUT = [device newBufferWithBytesNoCopy:_HQ2xLUT
														 length:256 * 2 * 4  * 16 * sizeof(uint32_t)
														options:MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined
													deallocator:nil];
	
	id<MTLBuffer> bufHQ3xLUT = [device newBufferWithBytesNoCopy:_HQ3xLUT
														 length:256 * 2 * 9  * 16 * sizeof(uint32_t)
														options:MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined
													deallocator:nil];
	
	id<MTLBuffer> bufHQ4xLUT = [device newBufferWithBytesNoCopy:_HQ4xLUT
														 length:256 * 2 * 16 * 16 * sizeof(uint32_t)
														options:MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined
													deallocator:nil];
	
	// Create the MTLTexture objects that will be used as LUTs in the Metal shaders.
	MTLTextureDescriptor *texHQ2xLUTDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
																							  width:256 * 2
																							 height:4
																						  mipmapped:NO];
	
	[texHQ2xLUTDesc setTextureType:MTLTextureType3D];
	[texHQ2xLUTDesc setDepth:16];
	[texHQ2xLUTDesc setResourceOptions:MTLResourceStorageModePrivate];
	[texHQ2xLUTDesc setStorageMode:MTLStorageModePrivate];
	[texHQ2xLUTDesc setUsage:MTLTextureUsageShaderRead];
	
	MTLTextureDescriptor *texHQ3xLUTDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
																							  width:256 * 2
																							 height:9
																						  mipmapped:NO];
	[texHQ3xLUTDesc setTextureType:MTLTextureType3D];
	[texHQ3xLUTDesc setDepth:16];
	[texHQ3xLUTDesc setResourceOptions:MTLResourceStorageModePrivate];
	[texHQ3xLUTDesc setStorageMode:MTLStorageModePrivate];
	[texHQ3xLUTDesc setUsage:MTLTextureUsageShaderRead];
	
	MTLTextureDescriptor *texHQ4xLUTDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
																							  width:256 * 2
																							 height:16
																						  mipmapped:NO];
	[texHQ4xLUTDesc setTextureType:MTLTextureType3D];
	[texHQ4xLUTDesc setDepth:16];
	[texHQ4xLUTDesc setResourceOptions:MTLResourceStorageModePrivate];
	[texHQ4xLUTDesc setStorageMode:MTLStorageModePrivate];
	[texHQ4xLUTDesc setUsage:MTLTextureUsageShaderRead];
	
	texLQ2xLUT = [device newTextureWithDescriptor:texHQ2xLUTDesc];
	texHQ2xLUT = [device newTextureWithDescriptor:texHQ2xLUTDesc];
	texHQ3xLUT = [device newTextureWithDescriptor:texHQ3xLUTDesc];
	texHQ4xLUT = [device newTextureWithDescriptor:texHQ4xLUTDesc];
	
	// Copy the LUT buffers from main memory to the GPU.
	id<MTLCommandBuffer> cb = [commandQueue commandBufferWithUnretainedReferences];;
	id<MTLBlitCommandEncoder> bce = [cb blitCommandEncoder];
	
	[bce copyFromBuffer:bufLQ2xLUT
		   sourceOffset:0
	  sourceBytesPerRow:256 * 2 * sizeof(uint32_t)
	sourceBytesPerImage:256 * 2 * 4 * sizeof(uint32_t)
			 sourceSize:MTLSizeMake(256 * 2, 4, 16)
			  toTexture:texLQ2xLUT
	   destinationSlice:0
	   destinationLevel:0
	  destinationOrigin:MTLOriginMake(0, 0, 0)];
	
	[bce copyFromBuffer:bufHQ2xLUT
		   sourceOffset:0
	  sourceBytesPerRow:256 * 2 * sizeof(uint32_t)
	sourceBytesPerImage:256 * 2 * 4 * sizeof(uint32_t)
			 sourceSize:MTLSizeMake(256 * 2, 4, 16)
			  toTexture:texHQ2xLUT
	   destinationSlice:0
	   destinationLevel:0
	  destinationOrigin:MTLOriginMake(0, 0, 0)];
	
	[bce copyFromBuffer:bufHQ3xLUT
		   sourceOffset:0
	  sourceBytesPerRow:256 * 2 * sizeof(uint32_t)
	sourceBytesPerImage:256 * 2 * 9 * sizeof(uint32_t)
			 sourceSize:MTLSizeMake(256 * 2, 9, 16)
			  toTexture:texHQ3xLUT
	   destinationSlice:0
	   destinationLevel:0
	  destinationOrigin:MTLOriginMake(0, 0, 0)];
	
	[bce copyFromBuffer:bufHQ4xLUT
		   sourceOffset:0
	  sourceBytesPerRow:256 * 2 * sizeof(uint32_t)
	sourceBytesPerImage:256 * 2 * 16 * sizeof(uint32_t)
			 sourceSize:MTLSizeMake(256 * 2, 16, 16)
			  toTexture:texHQ4xLUT
	   destinationSlice:0
	   destinationLevel:0
	  destinationOrigin:MTLOriginMake(0, 0, 0)];
	
	[bce endEncoding];
	
	[cb commit];
	[cb waitUntilCompleted];
	
	[bufLQ2xLUT release];
	[bufHQ2xLUT release];
	[bufHQ3xLUT release];
	[bufHQ4xLUT release];
}

void DeleteHQnxLUTs_Metal(id<MTLTexture> &texLQ2xLUT, id<MTLTexture> &texHQ2xLUT, id<MTLTexture> &texHQ3xLUT, id<MTLTexture> &texHQ4xLUT)
{
	[texLQ2xLUT release];
	[texHQ2xLUT release];
	[texHQ3xLUT release];
	[texHQ4xLUT release];
}
