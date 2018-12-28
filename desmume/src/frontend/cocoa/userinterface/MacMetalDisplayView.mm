/*
	Copyright (C) 2017-2018 DeSmuME team

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
@synthesize commandQueue;
@synthesize defaultLibrary;

@synthesize deposterizePipeline;
@synthesize hudPipeline;
@synthesize hudRGBAPipeline;
@synthesize samplerHUDBox;
@synthesize samplerHUDText;

@synthesize hudIndexBuffer;

@synthesize texPairFetch;
@synthesize bceFetch;

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
	_fetchCommandQueue = [device newCommandQueue];
	defaultLibrary = [device newDefaultLibrary];
	
	MTLComputePipelineDescriptor *computePipelineDesc = [[MTLComputePipelineDescriptor alloc] init];
	[computePipelineDesc setThreadGroupSizeIsMultipleOfThreadExecutionWidth:YES];
	
	[computePipelineDesc setComputeFunction:[defaultLibrary newFunctionWithName:@"convert_texture_rgb555_to_unorm8888"]];
	_fetch555ConvertOnlyPipeline = [[device newComputePipelineStateWithDescriptor:computePipelineDesc options:MTLPipelineOptionNone reflection:nil error:nil] retain];
	
	[computePipelineDesc setComputeFunction:[defaultLibrary newFunctionWithName:@"convert_texture_unorm666X_to_unorm8888"]];
	_fetch666ConvertOnlyPipeline = [[device newComputePipelineStateWithDescriptor:computePipelineDesc options:MTLPipelineOptionNone reflection:nil error:nil] retain];
	
	[computePipelineDesc setComputeFunction:[defaultLibrary newFunctionWithName:@"src_filter_deposterize"]];
	deposterizePipeline = [[device newComputePipelineStateWithDescriptor:computePipelineDesc options:MTLPipelineOptionNone reflection:nil error:nil] retain];
	
#if defined(MAC_OS_X_VERSION_10_13) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_13)
	if (@available(macOS 10.13, *))
	{
		[[[computePipelineDesc buffers] objectAtIndexedSubscript:0] setMutability:MTLMutabilityImmutable];
		[[[computePipelineDesc buffers] objectAtIndexedSubscript:1] setMutability:MTLMutabilityImmutable];
	}
#endif
	
	[computePipelineDesc setComputeFunction:[defaultLibrary newFunctionWithName:@"nds_fetch555"]];
	_fetch555Pipeline = [[device newComputePipelineStateWithDescriptor:computePipelineDesc options:MTLPipelineOptionNone reflection:nil error:nil] retain];
	
	[computePipelineDesc setComputeFunction:[defaultLibrary newFunctionWithName:@"nds_fetch666"]];
	_fetch666Pipeline = [[device newComputePipelineStateWithDescriptor:computePipelineDesc options:MTLPipelineOptionNone reflection:nil error:nil] retain];
	
	[computePipelineDesc setComputeFunction:[defaultLibrary newFunctionWithName:@"nds_fetch888"]];
	_fetch888Pipeline = [[device newComputePipelineStateWithDescriptor:computePipelineDesc options:MTLPipelineOptionNone reflection:nil error:nil] retain];
	
	[computePipelineDesc release];
	
	NSUInteger tw = [_fetch555Pipeline threadExecutionWidth];
	while ( ((GPU_FRAMEBUFFER_NATIVE_WIDTH  % tw) != 0) || (tw > GPU_FRAMEBUFFER_NATIVE_WIDTH) )
	{
		tw >>= 1;
	}
	
	NSUInteger th = [_fetch555Pipeline maxTotalThreadsPerThreadgroup] / tw;
	while ( ((GPU_FRAMEBUFFER_NATIVE_HEIGHT % th) != 0) || (th > GPU_FRAMEBUFFER_NATIVE_HEIGHT) )
	{
		th >>= 1;
	}
	
	_fetchThreadsPerGroupNative = MTLSizeMake(tw, th, 1);
	_fetchThreadGroupsPerGridNative = MTLSizeMake(GPU_FRAMEBUFFER_NATIVE_WIDTH  / tw,
												  GPU_FRAMEBUFFER_NATIVE_HEIGHT / th,
												  1);
	
	_fetchThreadsPerGroupCustom = _fetchThreadsPerGroupNative;
	_fetchThreadGroupsPerGridCustom = _fetchThreadGroupsPerGridNative;
	
	deposterizeThreadsPerGroup = _fetchThreadsPerGroupNative;
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
	
#if defined(MAC_OS_X_VERSION_10_13) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_13)
	if (@available(macOS 10.13, *))
	{
		[[[hudPipelineDesc vertexBuffers] objectAtIndexedSubscript:0] setMutability:MTLMutabilityImmutable];
		[[[hudPipelineDesc vertexBuffers] objectAtIndexedSubscript:1] setMutability:MTLMutabilityImmutable];
		[[[hudPipelineDesc vertexBuffers] objectAtIndexedSubscript:2] setMutability:MTLMutabilityImmutable];
		[[[hudPipelineDesc vertexBuffers] objectAtIndexedSubscript:3] setMutability:MTLMutabilityImmutable];
		[[[hudPipelineDesc vertexBuffers] objectAtIndexedSubscript:4] setMutability:MTLMutabilityImmutable];
		[[[hudPipelineDesc vertexBuffers] objectAtIndexedSubscript:5] setMutability:MTLMutabilityImmutable];
	}
#endif
	
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
	
	id<MTLCommandBuffer> cb = [_fetchCommandQueue commandBufferWithUnretainedReferences];
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
	[newTexDisplayDesc setResourceOptions:MTLResourceStorageModePrivate];
	[newTexDisplayDesc setStorageMode:MTLStorageModePrivate];
	[newTexDisplayDesc setUsage:MTLTextureUsageShaderRead];
	
	MTLTextureDescriptor *newTexPostprocessDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
																									 width:GPU_FRAMEBUFFER_NATIVE_WIDTH
																									height:GPU_FRAMEBUFFER_NATIVE_HEIGHT
																								 mipmapped:NO];
	[newTexPostprocessDesc setResourceOptions:MTLResourceStorageModePrivate];
	[newTexPostprocessDesc setStorageMode:MTLStorageModePrivate];
	[newTexPostprocessDesc setUsage:MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite];
	
	for (size_t i = 0; i < METAL_FETCH_BUFFER_COUNT; i++)
	{
		_bufDisplayFetchNative[NDSDisplayID_Main][i]  = nil;
		_bufDisplayFetchNative[NDSDisplayID_Touch][i] = nil;
		_bufDisplayFetchCustom[NDSDisplayID_Main][i]  = nil;
		_bufDisplayFetchCustom[NDSDisplayID_Touch][i] = nil;
		
		_bufMasterBrightMode[NDSDisplayID_Main][i]       = [device newBufferWithLength:sizeof(uint8_t) * GPU_FRAMEBUFFER_NATIVE_HEIGHT options:MTLResourceStorageModeManaged | MTLResourceCPUCacheModeWriteCombined];
		_bufMasterBrightMode[NDSDisplayID_Touch][i]      = [device newBufferWithLength:sizeof(uint8_t) * GPU_FRAMEBUFFER_NATIVE_HEIGHT options:MTLResourceStorageModeManaged | MTLResourceCPUCacheModeWriteCombined];
		_bufMasterBrightIntensity[NDSDisplayID_Main][i]  = [device newBufferWithLength:sizeof(uint8_t) * GPU_FRAMEBUFFER_NATIVE_HEIGHT options:MTLResourceStorageModeManaged | MTLResourceCPUCacheModeWriteCombined];
		_bufMasterBrightIntensity[NDSDisplayID_Touch][i] = [device newBufferWithLength:sizeof(uint8_t) * GPU_FRAMEBUFFER_NATIVE_HEIGHT options:MTLResourceStorageModeManaged | MTLResourceCPUCacheModeWriteCombined];
		
		_texDisplayFetchNative[NDSDisplayID_Main][i]  = [device newTextureWithDescriptor:newTexDisplayDesc];
		_texDisplayFetchNative[NDSDisplayID_Touch][i] = [device newTextureWithDescriptor:newTexDisplayDesc];
		_texDisplayFetchCustom[NDSDisplayID_Main][i]  = [device newTextureWithDescriptor:newTexDisplayDesc];
		_texDisplayFetchCustom[NDSDisplayID_Touch][i] = [device newTextureWithDescriptor:newTexDisplayDesc];
		
		_texDisplayPostprocessNative[NDSDisplayID_Main][i]  = [device newTextureWithDescriptor:newTexPostprocessDesc];
		_texDisplayPostprocessNative[NDSDisplayID_Touch][i] = [device newTextureWithDescriptor:newTexPostprocessDesc];
		_texDisplayPostprocessCustom[NDSDisplayID_Main][i]  = [device newTextureWithDescriptor:newTexPostprocessDesc];
		_texDisplayPostprocessCustom[NDSDisplayID_Touch][i] = [device newTextureWithDescriptor:newTexPostprocessDesc];
	}
	
	texPairFetch.bufferIndex = 0;
	texPairFetch.fetchSequenceNumber = 0;
	texPairFetch.main  = [_texDisplayPostprocessNative[NDSDisplayID_Main][0]  retain];
	texPairFetch.touch = [_texDisplayPostprocessNative[NDSDisplayID_Touch][0] retain];
	bceFetch = nil;
	
	// Set up the HQnx LUT textures.
	SetupHQnxLUTs_Metal(device, _fetchCommandQueue, texLQ2xLUT, texHQ2xLUT, texHQ3xLUT, texHQ4xLUT);
	texCurrentHQnxLUT = nil;
	
	return self;
}

- (void)dealloc
{
	[device release];
	
	[commandQueue release];
	[_fetchCommandQueue release];
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
	
	for (size_t i = 0; i < METAL_FETCH_BUFFER_COUNT; i++)
	{
		[_bufDisplayFetchNative[NDSDisplayID_Main][i]  release];
		[_bufDisplayFetchNative[NDSDisplayID_Touch][i] release];
		[_bufDisplayFetchCustom[NDSDisplayID_Main][i]  release];
		[_bufDisplayFetchCustom[NDSDisplayID_Touch][i] release];
		
		[_bufMasterBrightMode[NDSDisplayID_Main][i]  release];
		[_bufMasterBrightMode[NDSDisplayID_Touch][i] release];
		[_bufMasterBrightIntensity[NDSDisplayID_Main][i]  release];
		[_bufMasterBrightIntensity[NDSDisplayID_Touch][i] release];
		
		[_texDisplayFetchNative[NDSDisplayID_Main][i]  release];
		[_texDisplayFetchNative[NDSDisplayID_Touch][i] release];
		[_texDisplayFetchCustom[NDSDisplayID_Main][i]  release];
		[_texDisplayFetchCustom[NDSDisplayID_Touch][i] release];
		
		[_texDisplayPostprocessNative[NDSDisplayID_Main][i]  release];
		[_texDisplayPostprocessNative[NDSDisplayID_Touch][i] release];
		[_texDisplayPostprocessCustom[NDSDisplayID_Main][i]  release];
		[_texDisplayPostprocessCustom[NDSDisplayID_Touch][i] release];
	}
	
	[texPairFetch.main  release];
	[texPairFetch.touch release];
	
	DeleteHQnxLUTs_Metal(texLQ2xLUT, texHQ2xLUT, texHQ3xLUT, texHQ4xLUT);
	[self setTexCurrentHQnxLUT:nil];
	
	[samplerHUDBox release];
	[samplerHUDText release];
	
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
	
	MTLTextureDescriptor *newTexDisplayNativeDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:(dispInfo.colorFormat == NDSColorFormat_BGR555_Rev) ? MTLPixelFormatR16Uint : MTLPixelFormatRGBA8Unorm
																									   width:GPU_FRAMEBUFFER_NATIVE_WIDTH
																									  height:GPU_FRAMEBUFFER_NATIVE_HEIGHT
																								   mipmapped:NO];
	[newTexDisplayNativeDesc setResourceOptions:MTLResourceStorageModePrivate];
	[newTexDisplayNativeDesc setStorageMode:MTLStorageModePrivate];
	[newTexDisplayNativeDesc setUsage:MTLTextureUsageShaderRead];
	
	MTLTextureDescriptor *newTexPostprocessNativeDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
																										   width:GPU_FRAMEBUFFER_NATIVE_WIDTH
																										  height:GPU_FRAMEBUFFER_NATIVE_HEIGHT
																									   mipmapped:NO];
	[newTexPostprocessNativeDesc setResourceOptions:MTLResourceStorageModePrivate];
	[newTexPostprocessNativeDesc setStorageMode:MTLStorageModePrivate];
	[newTexPostprocessNativeDesc setUsage:MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite];
	
	MTLTextureDescriptor *newTexDisplayCustomDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:(dispInfo.colorFormat == NDSColorFormat_BGR555_Rev) ? MTLPixelFormatR16Uint : MTLPixelFormatRGBA8Unorm
																									   width:w
																									  height:h
																								   mipmapped:NO];
	[newTexDisplayCustomDesc setResourceOptions:MTLResourceStorageModePrivate];
	[newTexDisplayCustomDesc setStorageMode:MTLStorageModePrivate];
	[newTexDisplayCustomDesc setUsage:MTLTextureUsageShaderRead];
	
	MTLTextureDescriptor *newTexPostprocessCustomDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
																										   width:w
																										  height:h
																									   mipmapped:NO];
	[newTexPostprocessCustomDesc setResourceOptions:MTLResourceStorageModePrivate];
	[newTexPostprocessCustomDesc setStorageMode:MTLStorageModePrivate];
	[newTexPostprocessCustomDesc setUsage:MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite];
	
	for (size_t i = 0; i < dispInfo.framebufferPageCount; i++)
	{
		const NDSDisplayInfo &dispInfoAtIndex = GPUFetchObject->GetFetchDisplayInfoForBufferIndex(i);
		
		[_bufDisplayFetchNative[NDSDisplayID_Main][i]  release];
		[_bufDisplayFetchNative[NDSDisplayID_Touch][i] release];
		[_bufDisplayFetchCustom[NDSDisplayID_Main][i]  release];
		[_bufDisplayFetchCustom[NDSDisplayID_Touch][i] release];
		
		_bufDisplayFetchNative[NDSDisplayID_Main][i]  = [device newBufferWithBytesNoCopy:dispInfoAtIndex.nativeBuffer[NDSDisplayID_Main]
																				  length:_nativeBufferSize
																				 options:MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined
																			 deallocator:nil];
		
		_bufDisplayFetchNative[NDSDisplayID_Touch][i] = [device newBufferWithBytesNoCopy:dispInfoAtIndex.nativeBuffer[NDSDisplayID_Touch]
																				  length:_nativeBufferSize
																				 options:MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined
																			 deallocator:nil];
		
		_bufDisplayFetchCustom[NDSDisplayID_Main][i]  = [device newBufferWithBytesNoCopy:dispInfoAtIndex.customBuffer[NDSDisplayID_Main]
																				  length:_customBufferSize
																				 options:MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined
																			 deallocator:nil];
		
		_bufDisplayFetchCustom[NDSDisplayID_Touch][i] = [device newBufferWithBytesNoCopy:dispInfoAtIndex.customBuffer[NDSDisplayID_Touch]
																				  length:_customBufferSize
																				 options:MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined
																			 deallocator:nil];
		
		if (_fetchPixelBytes != dispInfo.pixelBytes)
		{
			[_texDisplayFetchNative[NDSDisplayID_Main][i]  release];
			[_texDisplayFetchNative[NDSDisplayID_Touch][i] release];
			[_texDisplayPostprocessNative[NDSDisplayID_Main][i]  release];
			[_texDisplayPostprocessNative[NDSDisplayID_Touch][i] release];
			
			_texDisplayFetchNative[NDSDisplayID_Main][i]  = [device newTextureWithDescriptor:newTexDisplayNativeDesc];
			_texDisplayFetchNative[NDSDisplayID_Touch][i] = [device newTextureWithDescriptor:newTexDisplayNativeDesc];
			_texDisplayPostprocessNative[NDSDisplayID_Main][i]  = [device newTextureWithDescriptor:newTexPostprocessNativeDesc];
			_texDisplayPostprocessNative[NDSDisplayID_Touch][i] = [device newTextureWithDescriptor:newTexPostprocessNativeDesc];
		}
		
		if ( (_fetchPixelBytes != dispInfo.pixelBytes) ||
			 ([_texDisplayFetchCustom[NDSDisplayID_Main][i] width] != w) ||
			 ([_texDisplayFetchCustom[NDSDisplayID_Main][i] height] != h) )
		{
			[_texDisplayFetchCustom[NDSDisplayID_Main][i]  release];
			[_texDisplayFetchCustom[NDSDisplayID_Touch][i] release];
			[_texDisplayPostprocessCustom[NDSDisplayID_Main][i]  release];
			[_texDisplayPostprocessCustom[NDSDisplayID_Touch][i] release];
			
			_texDisplayFetchCustom[NDSDisplayID_Main][i]  = [device newTextureWithDescriptor:newTexDisplayCustomDesc];
			_texDisplayFetchCustom[NDSDisplayID_Touch][i] = [device newTextureWithDescriptor:newTexDisplayCustomDesc];
			_texDisplayPostprocessCustom[NDSDisplayID_Main][i]  = [device newTextureWithDescriptor:newTexPostprocessCustomDesc];
			_texDisplayPostprocessCustom[NDSDisplayID_Touch][i] = [device newTextureWithDescriptor:newTexPostprocessCustomDesc];
		}
	}
	
	_fetchPixelBytes = dispInfo.pixelBytes;
	
	NSUInteger tw = [_fetch555Pipeline threadExecutionWidth];
	while ( ((w % tw) != 0) || (tw > w) )
	{
		tw >>= 1;
	}
	
	NSUInteger th = [_fetch555Pipeline maxTotalThreadsPerThreadgroup] / tw;
	while ( ((h % th) != 0) || (th > h) )
	{
		th >>= 1;
	}
	
	_fetchThreadsPerGroupCustom = MTLSizeMake(tw, th, 1);
	_fetchThreadGroupsPerGridCustom = MTLSizeMake(w / tw,
												  h / th,
												  1);
	
	id<MTLCommandBuffer> cb = [_fetchCommandQueue commandBufferWithUnretainedReferences];
	MetalTexturePair newTexPair = [self setFetchTextureBindingsAtIndex:dispInfo.bufferIndex commandBuffer:cb];
	[cb commit];
	
	const MetalTexturePair oldTexPair = [self texPairFetch];
	
	[newTexPair.main  retain];
	[newTexPair.touch retain];
	[self setTexPairFetch:newTexPair];
	
	[oldTexPair.main  release];
	[oldTexPair.touch release];
}

- (MetalTexturePair) setFetchTextureBindingsAtIndex:(const uint8_t)index commandBuffer:(id<MTLCommandBuffer>)cb
{
	const NDSDisplayInfo &currentDisplayInfo = GPUFetchObject->GetFetchDisplayInfoForBufferIndex(index);
	const bool isMainEnabled  = currentDisplayInfo.isDisplayEnabled[NDSDisplayID_Main];
	const bool isTouchEnabled = currentDisplayInfo.isDisplayEnabled[NDSDisplayID_Touch];
	
	MetalTexturePair targetTexPair = {index, currentDisplayInfo.sequenceNumber, nil, nil};
	
	if (isMainEnabled || isTouchEnabled)
	{
		if (isMainEnabled)
		{
			if (!currentDisplayInfo.didPerformCustomRender[NDSDisplayID_Main])
			{
				targetTexPair.main  = _texDisplayFetchNative[NDSDisplayID_Main][index];
			}
			else
			{
				targetTexPair.main  = _texDisplayFetchCustom[NDSDisplayID_Main][index];
			}
		}
		
		if (isTouchEnabled)
		{
			if (!currentDisplayInfo.didPerformCustomRender[NDSDisplayID_Touch])
			{
				targetTexPair.touch = _texDisplayFetchNative[NDSDisplayID_Touch][index];
			}
			else
			{
				targetTexPair.touch = _texDisplayFetchCustom[NDSDisplayID_Touch][index];
			}
		}
		
		if (currentDisplayInfo.needApplyMasterBrightness[NDSDisplayID_Main] || currentDisplayInfo.needApplyMasterBrightness[NDSDisplayID_Touch])
		{
			id<MTLComputeCommandEncoder> cce = [cb computeCommandEncoder];
			
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
						threadsPerThreadgroup:_fetchThreadsPerGroupNative];
					
					targetTexPair.main  = _texDisplayPostprocessNative[NDSDisplayID_Main][index];
				}
				else
				{
					[cce setTexture:_texDisplayFetchCustom[NDSDisplayID_Main][index] atIndex:0];
					[cce setTexture:_texDisplayPostprocessCustom[NDSDisplayID_Main][index] atIndex:1];
					[cce dispatchThreadgroups:_fetchThreadGroupsPerGridCustom
						threadsPerThreadgroup:_fetchThreadsPerGroupCustom];
					
					targetTexPair.main  = _texDisplayPostprocessCustom[NDSDisplayID_Main][index];
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
						threadsPerThreadgroup:_fetchThreadsPerGroupNative];
					
					targetTexPair.touch = _texDisplayPostprocessNative[NDSDisplayID_Touch][index];
				}
				else
				{
					[cce setTexture:_texDisplayFetchCustom[NDSDisplayID_Touch][index] atIndex:0];
					[cce setTexture:_texDisplayPostprocessCustom[NDSDisplayID_Touch][index] atIndex:1];
					[cce dispatchThreadgroups:_fetchThreadGroupsPerGridCustom
						threadsPerThreadgroup:_fetchThreadsPerGroupCustom];
					
					targetTexPair.touch = _texDisplayPostprocessCustom[NDSDisplayID_Touch][index];
				}
			}
			
			[cce endEncoding];
		}
		else if (currentDisplayInfo.colorFormat != NDSColorFormat_BGR888_Rev)
		{
			id<MTLComputeCommandEncoder> cce = [cb computeCommandEncoder];
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
							threadsPerThreadgroup:_fetchThreadsPerGroupNative];
						
						targetTexPair.main  = _texDisplayPostprocessNative[NDSDisplayID_Main][index];
					}
					else
					{
						[cce setTexture:_texDisplayFetchCustom[NDSDisplayID_Main][index] atIndex:0];
						[cce setTexture:_texDisplayPostprocessCustom[NDSDisplayID_Main][index] atIndex:1];
						[cce dispatchThreadgroups:_fetchThreadGroupsPerGridCustom
							threadsPerThreadgroup:_fetchThreadsPerGroupCustom];
						
						targetTexPair.main  = _texDisplayPostprocessCustom[NDSDisplayID_Main][index];
					}
				}
				
				if (isTouchEnabled)
				{
					if (!currentDisplayInfo.didPerformCustomRender[NDSDisplayID_Touch])
					{
						[cce setTexture:_texDisplayFetchNative[NDSDisplayID_Touch][index] atIndex:0];
						[cce setTexture:_texDisplayPostprocessNative[NDSDisplayID_Touch][index] atIndex:1];
						[cce dispatchThreadgroups:_fetchThreadGroupsPerGridNative
							threadsPerThreadgroup:_fetchThreadsPerGroupNative];
						
						targetTexPair.touch = _texDisplayPostprocessNative[NDSDisplayID_Touch][index];
					}
					else
					{
						[cce setTexture:_texDisplayFetchCustom[NDSDisplayID_Touch][index] atIndex:0];
						[cce setTexture:_texDisplayPostprocessCustom[NDSDisplayID_Touch][index] atIndex:1];
						[cce dispatchThreadgroups:_fetchThreadGroupsPerGridCustom
							threadsPerThreadgroup:_fetchThreadsPerGroupCustom];
						
						targetTexPair.touch = _texDisplayPostprocessCustom[NDSDisplayID_Touch][index];
					}
				}
			}
			
			[cce endEncoding];
		}
	}
	
	return targetTexPair;
}

- (void) fetchFromBufferIndex:(const u8)index
{
	id<MTLCommandBuffer> cb = [_fetchCommandQueue commandBufferWithUnretainedReferences];
	
	semaphore_wait([self semaphoreFramebufferPageAtIndex:index]);
	[self setFramebufferState:ClientDisplayBufferState_Reading index:index];
	
	id<MTLBlitCommandEncoder> bce = [cb blitCommandEncoder];
	[self setBceFetch:bce];
	GPUFetchObject->GPUClientFetchObject::FetchFromBufferIndex(index);
	[self setBceFetch:nil];
	[bce endEncoding];
	
	const MetalTexturePair newTexPair = [self setFetchTextureBindingsAtIndex:index commandBuffer:cb];
	[newTexPair.main  retain];
	[newTexPair.touch retain];
	
	[cb addCompletedHandler:^(id<MTLCommandBuffer> block) {
		const MetalTexturePair oldTexPair = [self texPairFetch];
		[self setTexPairFetch:newTexPair];
		[oldTexPair.main  release];
		[oldTexPair.touch release];
		
		[self setFramebufferState:ClientDisplayBufferState_Idle index:index];
		semaphore_signal([self semaphoreFramebufferPageAtIndex:index]);
	}];
	
	[cb commit];
}

- (void) fetchNativeDisplayByID:(const NDSDisplayID)displayID bufferIndex:(const u8)bufferIndex blitCommandEncoder:(id<MTLBlitCommandEncoder>)bce
{
	if (bce == nil)
	{
		return;
	}
	
	id<MTLTexture> targetDestination = _texDisplayFetchNative[displayID][bufferIndex];
	
	[bce copyFromBuffer:_bufDisplayFetchNative[displayID][bufferIndex]
		   sourceOffset:0
	  sourceBytesPerRow:_nativeLineSize
	sourceBytesPerImage:_nativeBufferSize
			 sourceSize:MTLSizeMake(GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT, 1)
			  toTexture:targetDestination
	   destinationSlice:0
	   destinationLevel:0
	  destinationOrigin:MTLOriginMake(0, 0, 0)];
}

- (void) fetchCustomDisplayByID:(const NDSDisplayID)displayID bufferIndex:(const u8)bufferIndex blitCommandEncoder:(id<MTLBlitCommandEncoder>)bce
{
	if (bce == nil)
	{
		return;
	}
	
	const NDSDisplayInfo &currentDisplayInfo = GPUFetchObject->GetFetchDisplayInfoForBufferIndex(bufferIndex);
	id<MTLTexture> targetDestination = _texDisplayFetchCustom[displayID][bufferIndex];
	
	[bce copyFromBuffer:_bufDisplayFetchCustom[displayID][bufferIndex]
		   sourceOffset:0
	  sourceBytesPerRow:_customLineSize
	sourceBytesPerImage:_customBufferSize
			 sourceSize:MTLSizeMake(currentDisplayInfo.customWidth, currentDisplayInfo.customHeight, 1)
			  toTexture:targetDestination
	   destinationSlice:0
	   destinationLevel:0
	  destinationOrigin:MTLOriginMake(0, 0, 0)];
}

- (void) flushMultipleViews:(const std::vector<ClientDisplay3DView *> &)cdvFlushList timeStampNow:(const CVTimeStamp *)timeStampNow timeStampOutput:(const CVTimeStamp *)timeStampOutput
{
	const size_t listSize = cdvFlushList.size();
	
	@autoreleasepool
	{
		id<MTLCommandBuffer> cbFlush = [commandQueue commandBufferWithUnretainedReferences];
		id<MTLCommandBuffer> cbFinalize = [commandQueue commandBufferWithUnretainedReferences];
		
		for (size_t i = 0; i < listSize; i++)
		{
			ClientDisplay3DView *cdv = (ClientDisplay3DView *)cdvFlushList[i];
			cdv->FlushView(cbFlush);
		}
		
		for (size_t i = 0; i < listSize; i++)
		{
			ClientDisplay3DView *cdv = (ClientDisplay3DView *)cdvFlushList[i];
			cdv->FinalizeFlush(cbFinalize, timeStampOutput->hostTime);
		}
		
		[cbFlush enqueue];
		[cbFinalize enqueue];
		
		[cbFlush commit];
		[cbFinalize commit];
		
#ifdef DEBUG
		[commandQueue insertDebugCaptureBoundary];
#endif
	}
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
@synthesize bufCPUFilterDstMain;
@synthesize bufCPUFilterDstTouch;
@synthesize texHUDCharMap;
@synthesize needsProcessFrameWait;
@synthesize needsViewportUpdate;
@synthesize needsRotationScaleUpdate;
@synthesize needsScreenVerticesUpdate;
@synthesize needsHUDVerticesUpdate;
@synthesize texPairProcess;
@synthesize renderFrameInfo;
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
	
	_texDisplaySrcDeposterize[NDSDisplayID_Main][0]  = nil;
	_texDisplaySrcDeposterize[NDSDisplayID_Touch][0] = nil;
	_texDisplaySrcDeposterize[NDSDisplayID_Main][1]  = nil;
	_texDisplaySrcDeposterize[NDSDisplayID_Touch][1] = nil;
	
	_texDisplayPixelScaler[NDSDisplayID_Main]  = nil;
	_texDisplayPixelScaler[NDSDisplayID_Touch] = nil;
	
	for (size_t i = 0; i < RENDER_BUFFER_COUNT; i++)
	{
		_hudVtxPositionBuffer[i] = nil;
		_hudVtxColorBuffer[i]    = nil;
		_hudTexCoordBuffer[i]    = nil;
	}
	
	_bufCPUFilterSrcMain  = nil;
	_bufCPUFilterSrcTouch = nil;
	bufCPUFilterDstMain  = nil;
	bufCPUFilterDstTouch = nil;
	
	texHUDCharMap = nil;
	
	_pixelScalerThreadsPerGroup = MTLSizeMake(1, 1, 1);
	_pixelScalerThreadGroupsPerGrid = MTLSizeMake(1, 1, 1);
	
	needsProcessFrameWait = YES;
	needsViewportUpdate = YES;
	needsRotationScaleUpdate = YES;
	needsScreenVerticesUpdate = YES;
	needsHUDVerticesUpdate = YES;
	
	texPairProcess.bufferIndex = 0;
	texPairProcess.fetchSequenceNumber = 0;
    texPairProcess.main  = nil;
    texPairProcess.touch = nil;
	
	for (size_t i = 0; i < RENDER_BUFFER_COUNT; i++)
	{
		_semRenderBuffers[i] = dispatch_semaphore_create(1);
		_renderBufferState[i] = ClientDisplayBufferState_Idle;
		_spinlockRenderBufferStates[i] = OS_SPINLOCK_INIT;
	}
	
	MTLViewport newViewport;
	newViewport.originX = 0.0;
	newViewport.originY = 0.0;
	newViewport.width  = cdp->GetPresenterProperties().clientWidth;
	newViewport.height = cdp->GetPresenterProperties().clientHeight;
	newViewport.znear = 0.0;
	newViewport.zfar = 1.0;
	
	renderFrameInfo.renderIndex = 0;
	renderFrameInfo.needEncodeViewport = true;
	renderFrameInfo.newViewport = newViewport;
	renderFrameInfo.willDrawHUD = false;
	renderFrameInfo.willDrawHUDInput = false;
	renderFrameInfo.hudStringLength = 0;
	renderFrameInfo.hudTouchLineLength = 0;
	renderFrameInfo.hudTotalLength = 0;
	
	return self;
}

- (void)dealloc
{
	[[self colorAttachment0Desc] setTexture:nil];
	[_outputRenderPassDesc release];
	
	[_texDisplaySrcDeposterize[NDSDisplayID_Main][0]  release];
	[_texDisplaySrcDeposterize[NDSDisplayID_Touch][0] release];
	[_texDisplaySrcDeposterize[NDSDisplayID_Main][1]  release];
	[_texDisplaySrcDeposterize[NDSDisplayID_Touch][1] release];
	
	[_texDisplayPixelScaler[NDSDisplayID_Main]  release];
	[_texDisplayPixelScaler[NDSDisplayID_Touch] release];
	
	for (size_t i = 0; i < RENDER_BUFFER_COUNT; i++)
	{
		[_hudVtxPositionBuffer[i] release];
		[_hudVtxColorBuffer[i]    release];
		[_hudTexCoordBuffer[i]    release];
	}
	
	[_bufCPUFilterSrcMain  release];
	[_bufCPUFilterSrcTouch release];
	[self setBufCPUFilterDstMain:nil];
	[self setBufCPUFilterDstTouch:nil];
	
	[texPairProcess.main  release];
	[texPairProcess.touch release];
	
	[self setPixelScalePipeline:nil];
	[self setOutputRGBAPipeline:nil];
	[self setOutputDrawablePipeline:nil];
	[self setTexHUDCharMap:nil];
	
	[self setSharedData:nil];
	
	for (size_t i = 0; i < RENDER_BUFFER_COUNT; i++)
	{
		dispatch_release(_semRenderBuffers[i]);
	}
	
	[super dealloc];
}

- (VideoFilterTypeID) pixelScaler
{
	return cdp->GetPixelScaler();
}

- (void) setPixelScaler:(VideoFilterTypeID)filterID
{
	id<MTLTexture> currentHQnxLUT = nil;
	
	MTLComputePipelineDescriptor *computePipelineDesc = [[MTLComputePipelineDescriptor alloc] init];
	[computePipelineDesc setThreadGroupSizeIsMultipleOfThreadExecutionWidth:YES];
	
	switch (filterID)
	{
		case VideoFilterTypeID_Nearest2X:
			[computePipelineDesc setComputeFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_nearest2x"]];
			break;
			
		case VideoFilterTypeID_Scanline:
			[computePipelineDesc setComputeFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_scanline"]];
			break;
			
		case VideoFilterTypeID_EPX:
			[computePipelineDesc setComputeFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_2xEPX"]];
			break;
			
		case VideoFilterTypeID_EPXPlus:
			[computePipelineDesc setComputeFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_2xEPXPlus"]];
			break;
			
		case VideoFilterTypeID_2xSaI:
			[computePipelineDesc setComputeFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_2xSaI"]];
			break;
			
		case VideoFilterTypeID_Super2xSaI:
			[computePipelineDesc setComputeFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_Super2xSaI"]];
			break;
			
		case VideoFilterTypeID_SuperEagle:
			[computePipelineDesc setComputeFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_2xSuperEagle"]];
			break;
			
		case VideoFilterTypeID_LQ2X:
			[computePipelineDesc setComputeFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_LQ2x"]];
			currentHQnxLUT = [sharedData texLQ2xLUT];
			break;
			
		case VideoFilterTypeID_LQ2XS:
			[computePipelineDesc setComputeFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_LQ2xS"]];
			currentHQnxLUT = [sharedData texLQ2xLUT];
			break;
			
		case VideoFilterTypeID_HQ2X:
			[computePipelineDesc setComputeFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_HQ2x"]];
			currentHQnxLUT = [sharedData texHQ2xLUT];
			break;
			
		case VideoFilterTypeID_HQ2XS:
			[computePipelineDesc setComputeFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_HQ2xS"]];
			currentHQnxLUT = [sharedData texHQ2xLUT];
			break;
			
		case VideoFilterTypeID_HQ3X:
			[computePipelineDesc setComputeFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_HQ3x"]];
			currentHQnxLUT = [sharedData texHQ3xLUT];
			break;
			
		case VideoFilterTypeID_HQ3XS:
			[computePipelineDesc setComputeFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_HQ3xS"]];
			currentHQnxLUT = [sharedData texHQ3xLUT];
			break;
			
		case VideoFilterTypeID_HQ4X:
			[computePipelineDesc setComputeFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_HQ4x"]];
			currentHQnxLUT = [sharedData texHQ4xLUT];
			break;
			
		case VideoFilterTypeID_HQ4XS:
			[computePipelineDesc setComputeFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_HQ4xS"]];
			currentHQnxLUT = [sharedData texHQ4xLUT];
			break;
			
		case VideoFilterTypeID_2xBRZ:
			[computePipelineDesc setComputeFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_2xBRZ"]];
			break;
			
		case VideoFilterTypeID_3xBRZ:
			[computePipelineDesc setComputeFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_3xBRZ"]];
			break;
			
		case VideoFilterTypeID_4xBRZ:
			[computePipelineDesc setComputeFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_4xBRZ"]];
			break;
			
		case VideoFilterTypeID_5xBRZ:
			[computePipelineDesc setComputeFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_5xBRZ"]];
			break;
			
		case VideoFilterTypeID_6xBRZ:
			[computePipelineDesc setComputeFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_6xBRZ"]];
			break;
			
		case VideoFilterTypeID_None:
		default:
			[computePipelineDesc release];
			computePipelineDesc = nil;
			break;
	}
	
	[sharedData setTexCurrentHQnxLUT:currentHQnxLUT];
	
	if (computePipelineDesc != nil)
	{
		[self setPixelScalePipeline:[[sharedData device] newComputePipelineStateWithDescriptor:computePipelineDesc options:MTLPipelineOptionNone reflection:nil error:nil]];
		[computePipelineDesc release];
		computePipelineDesc = nil;
	}
	else
	{
		[self setPixelScalePipeline:nil];
	}
	
	if ([self pixelScalePipeline] != nil)
	{
        const VideoFilterAttributes vfAttr = VideoFilter::GetAttributesByID(filterID);
        const size_t newScalerWidth  = GPU_FRAMEBUFFER_NATIVE_WIDTH  * vfAttr.scaleMultiply / vfAttr.scaleDivide;
        const size_t newScalerHeight = GPU_FRAMEBUFFER_NATIVE_HEIGHT * vfAttr.scaleMultiply / vfAttr.scaleDivide;
        
        MTLTextureDescriptor *texDisplayPixelScaleDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                                            width:newScalerWidth
                                                                                                           height:newScalerHeight
                                                                                                        mipmapped:NO];
        [texDisplayPixelScaleDesc setResourceOptions:MTLResourceStorageModePrivate];
        [texDisplayPixelScaleDesc setStorageMode:MTLStorageModePrivate];
        [texDisplayPixelScaleDesc setUsage:MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite];
		
		[_texDisplayPixelScaler[NDSDisplayID_Main]  release];
		[_texDisplayPixelScaler[NDSDisplayID_Touch] release];
		_texDisplayPixelScaler[NDSDisplayID_Main]  = [[sharedData device] newTextureWithDescriptor:texDisplayPixelScaleDesc];
		_texDisplayPixelScaler[NDSDisplayID_Touch] = [[sharedData device] newTextureWithDescriptor:texDisplayPixelScaleDesc];
		
		NSUInteger tw = [[self pixelScalePipeline] threadExecutionWidth];
		while ( ((newScalerWidth  % tw) != 0) || (tw > newScalerWidth) )
		{
			tw >>= 1;
		}
		
		NSUInteger th = [[self pixelScalePipeline] maxTotalThreadsPerThreadgroup] / tw;
		while ( ((newScalerHeight % th) != 0) || (th > newScalerHeight) )
		{
			th >>= 1;
		}
		
		_pixelScalerThreadsPerGroup = MTLSizeMake(tw, th, 1);
		_pixelScalerThreadGroupsPerGrid = MTLSizeMake(newScalerWidth  / tw,
													  newScalerHeight / th,
													  1);
	}
	else
	{
		[_texDisplayPixelScaler[NDSDisplayID_Main]  release];
		[_texDisplayPixelScaler[NDSDisplayID_Touch] release];
		_texDisplayPixelScaler[NDSDisplayID_Main]  = nil;
		_texDisplayPixelScaler[NDSDisplayID_Touch] = nil;
		
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
	
#if defined(MAC_OS_X_VERSION_10_13) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_13)
	if (@available(macOS 10.13, *))
	{
		[[[outputPipelineDesc vertexBuffers] objectAtIndexedSubscript:0] setMutability:MTLMutabilityImmutable];
		[[[outputPipelineDesc vertexBuffers] objectAtIndexedSubscript:1] setMutability:MTLMutabilityImmutable];
		[[[outputPipelineDesc vertexBuffers] objectAtIndexedSubscript:2] setMutability:MTLMutabilityImmutable];
		[[[outputPipelineDesc vertexBuffers] objectAtIndexedSubscript:3] setMutability:MTLMutabilityImmutable];
		[[[outputPipelineDesc fragmentBuffers] objectAtIndexedSubscript:0] setMutability:MTLMutabilityImmutable];
	}
#endif
	
	[outputPipelineDesc release];
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
	
#if defined(MAC_OS_X_VERSION_10_13) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_13)
	if (@available(macOS 10.13, *))
	{
		[[[outputPipelineDesc vertexBuffers] objectAtIndexedSubscript:0] setMutability:MTLMutabilityImmutable];
		[[[outputPipelineDesc vertexBuffers] objectAtIndexedSubscript:1] setMutability:MTLMutabilityImmutable];
		[[[outputPipelineDesc vertexBuffers] objectAtIndexedSubscript:2] setMutability:MTLMutabilityImmutable];
		[[[outputPipelineDesc vertexBuffers] objectAtIndexedSubscript:3] setMutability:MTLMutabilityImmutable];
		[[[outputPipelineDesc fragmentBuffers] objectAtIndexedSubscript:0] setMutability:MTLMutabilityImmutable];
	}
#endif
	
	[[[outputPipelineDesc colorAttachments] objectAtIndexedSubscript:0] setPixelFormat:MTLPixelFormatRGBA8Unorm];
	outputRGBAPipeline = [[[sharedData device] newRenderPipelineStateWithDescriptor:outputPipelineDesc error:nil] retain];
	
	if ([self drawableFormat] != MTLPixelFormatInvalid)
	{
		[[[outputPipelineDesc colorAttachments] objectAtIndexedSubscript:0] setPixelFormat:[self drawableFormat]];
		outputDrawablePipeline = [[[sharedData device] newRenderPipelineStateWithDescriptor:outputPipelineDesc error:nil] retain];
	}
	
	[outputPipelineDesc release];
	
	// Set up processing textures.
	MTLTextureDescriptor *texDisplaySrcDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
																								 width:GPU_FRAMEBUFFER_NATIVE_WIDTH
																								height:GPU_FRAMEBUFFER_NATIVE_HEIGHT
																							 mipmapped:NO];
	[texDisplaySrcDesc setResourceOptions:MTLResourceStorageModePrivate];
	[texDisplaySrcDesc setStorageMode:MTLStorageModePrivate];
	[texDisplaySrcDesc setUsage:MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite];
	
	_texDisplaySrcDeposterize[NDSDisplayID_Main][0]  = [[sharedData device] newTextureWithDescriptor:texDisplaySrcDesc];
	_texDisplaySrcDeposterize[NDSDisplayID_Touch][0] = [[sharedData device] newTextureWithDescriptor:texDisplaySrcDesc];
	_texDisplaySrcDeposterize[NDSDisplayID_Main][1]  = [[sharedData device] newTextureWithDescriptor:texDisplaySrcDesc];
	_texDisplaySrcDeposterize[NDSDisplayID_Touch][1] = [[sharedData device] newTextureWithDescriptor:texDisplaySrcDesc];
	
	_texDisplayPixelScaler[NDSDisplayID_Main]  = nil;
	_texDisplayPixelScaler[NDSDisplayID_Touch] = nil;
	
	for (size_t i = 0; i < RENDER_BUFFER_COUNT; i++)
	{
		_hudVtxPositionBuffer[i] = [[sharedData device] newBufferWithLength:HUD_VERTEX_ATTRIBUTE_BUFFER_SIZE       options:MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined];
		_hudVtxColorBuffer[i]    = [[sharedData device] newBufferWithLength:HUD_VERTEX_COLOR_ATTRIBUTE_BUFFER_SIZE options:MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined];
		_hudTexCoordBuffer[i]    = [[sharedData device] newBufferWithLength:HUD_VERTEX_ATTRIBUTE_BUFFER_SIZE       options:MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined];
	}
	
	MetalTexturePair texPairFetch = [sharedData texPairFetch];
	texPairProcess.bufferIndex = texPairFetch.bufferIndex;
	texPairProcess.fetchSequenceNumber = texPairFetch.fetchSequenceNumber;
	texPairProcess.main  = [texPairFetch.main  retain];
	texPairProcess.touch = [texPairFetch.touch retain];
	
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
	
	[self setBufCPUFilterDstMain:[[[sharedData device] newBufferWithBytesNoCopy:vfMain->GetDstBufferPtr()
																		 length:vfMain->GetDstWidth() * vfMain->GetDstHeight() * sizeof(uint32_t)
																		options:MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined
																	deallocator:nil] autorelease]];
	
	[self setBufCPUFilterDstTouch:[[[sharedData device] newBufferWithBytesNoCopy:vfTouch->GetDstBufferPtr()
																		  length:vfTouch->GetDstWidth() * vfTouch->GetDstHeight() * sizeof(uint32_t)
																		 options:MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined
																	 deallocator:nil] autorelease]];
	
	texHUDCharMap = nil;
}

- (void) resizeCPUPixelScalerUsingFilterID:(const VideoFilterTypeID)filterID
{
	const VideoFilterAttributes vfAttr = VideoFilter::GetAttributesByID(filterID);
	
	VideoFilter *vfMain  = cdp->GetPixelScalerObject(NDSDisplayID_Main);
	[self setBufCPUFilterDstMain:[[[sharedData device] newBufferWithBytesNoCopy:vfMain->GetDstBufferPtr()
																		 length:(vfMain->GetSrcWidth()  * vfAttr.scaleMultiply / vfAttr.scaleDivide) * (vfMain->GetSrcHeight()  * vfAttr.scaleMultiply / vfAttr.scaleDivide) * sizeof(uint32_t)
																		options:MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined
																	deallocator:nil] autorelease]];
	
	VideoFilter *vfTouch = cdp->GetPixelScalerObject(NDSDisplayID_Touch);
	[self setBufCPUFilterDstTouch:[[[sharedData device] newBufferWithBytesNoCopy:vfTouch->GetDstBufferPtr()
																		  length:(vfTouch->GetSrcWidth() * vfAttr.scaleMultiply / vfAttr.scaleDivide) * (vfTouch->GetSrcHeight() * vfAttr.scaleMultiply / vfAttr.scaleDivide) * sizeof(uint32_t)
																		 options:MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined
																	 deallocator:nil] autorelease]];
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
	const MetalTexturePair texFetch = [sharedData texPairFetch];
	const NDSDisplayInfo &fetchDisplayInfo = [sharedData GPUFetchObject]->GetFetchDisplayInfoForBufferIndex(texFetch.bufferIndex);
	const ClientDisplayMode mode = cdp->GetPresenterProperties().mode;
	const bool useDeposterize = cdp->GetSourceDeposterize();
	const NDSDisplayID selectedDisplaySourceMain  = cdp->GetSelectedDisplaySourceForDisplay(NDSDisplayID_Main);
	const NDSDisplayID selectedDisplaySourceTouch = cdp->GetSelectedDisplaySourceForDisplay(NDSDisplayID_Touch);
	
	MetalTexturePair newTexProcess;
	newTexProcess.bufferIndex = texFetch.bufferIndex;
	newTexProcess.fetchSequenceNumber = texFetch.fetchSequenceNumber;
	newTexProcess.main  = (selectedDisplaySourceMain  == NDSDisplayID_Main)  ? texFetch.main  : texFetch.touch;
	newTexProcess.touch = (selectedDisplaySourceTouch == NDSDisplayID_Touch) ? texFetch.touch : texFetch.main;
	
	if ( (fetchDisplayInfo.pixelBytes != 0) && (useDeposterize || (cdp->GetPixelScaler() != VideoFilterTypeID_None)) )
	{
		pthread_mutex_lock(((MacMetalDisplayPresenter *)cdp)->GetMutexProcessPtr());
		
		const bool willFilterOnGPU = cdp->WillFilterOnGPU();
		const bool shouldProcessDisplayMain  = (!fetchDisplayInfo.didPerformCustomRender[selectedDisplaySourceMain]  || !fetchDisplayInfo.isCustomSizeRequested) && cdp->IsSelectedDisplayEnabled(NDSDisplayID_Main)  && (mode == ClientDisplayMode_Main  || mode == ClientDisplayMode_Dual);
		const bool shouldProcessDisplayTouch = (!fetchDisplayInfo.didPerformCustomRender[selectedDisplaySourceTouch] || !fetchDisplayInfo.isCustomSizeRequested) && cdp->IsSelectedDisplayEnabled(NDSDisplayID_Touch) && (mode == ClientDisplayMode_Touch || mode == ClientDisplayMode_Dual) && (selectedDisplaySourceMain != selectedDisplaySourceTouch);
		
		// First try a GPU-only pipeline.
		if ( willFilterOnGPU || (useDeposterize && (cdp->GetPixelScaler() == VideoFilterTypeID_None)) )
		{
			id<MTLCommandBuffer> cb = [self newCommandBuffer];
			id<MTLComputeCommandEncoder> cce = [cb computeCommandEncoder];
			
			if (useDeposterize)
			{
				[cce setComputePipelineState:[sharedData deposterizePipeline]];
				
				if (shouldProcessDisplayMain && (newTexProcess.main != nil))
				{
					[cce setTexture:newTexProcess.main atIndex:0];
					[cce setTexture:_texDisplaySrcDeposterize[NDSDisplayID_Main][0] atIndex:1];
					[cce dispatchThreadgroups:[sharedData deposterizeThreadGroupsPerGrid]
						threadsPerThreadgroup:[sharedData deposterizeThreadsPerGroup]];
					
					[cce setTexture:_texDisplaySrcDeposterize[NDSDisplayID_Main][0] atIndex:0];
					[cce setTexture:_texDisplaySrcDeposterize[NDSDisplayID_Main][1] atIndex:1];
					[cce dispatchThreadgroups:[sharedData deposterizeThreadGroupsPerGrid]
						threadsPerThreadgroup:[sharedData deposterizeThreadsPerGroup]];
					
					newTexProcess.main = _texDisplaySrcDeposterize[NDSDisplayID_Main][1];
					
					if (selectedDisplaySourceMain == selectedDisplaySourceTouch)
					{
						newTexProcess.touch = newTexProcess.main;
					}
				}
				
				if (shouldProcessDisplayTouch && (newTexProcess.touch != nil))
				{
					[cce setTexture:newTexProcess.touch atIndex:0];
					[cce setTexture:_texDisplaySrcDeposterize[NDSDisplayID_Touch][0] atIndex:1];
					[cce dispatchThreadgroups:[sharedData deposterizeThreadGroupsPerGrid]
						threadsPerThreadgroup:[sharedData deposterizeThreadsPerGroup]];
					
					[cce setTexture:_texDisplaySrcDeposterize[NDSDisplayID_Touch][0] atIndex:0];
					[cce setTexture:_texDisplaySrcDeposterize[NDSDisplayID_Touch][1] atIndex:1];
					[cce dispatchThreadgroups:[sharedData deposterizeThreadGroupsPerGrid]
						threadsPerThreadgroup:[sharedData deposterizeThreadsPerGroup]];
					
					newTexProcess.touch = _texDisplaySrcDeposterize[NDSDisplayID_Touch][1];
				}
			}
			
			if (cdp->GetPixelScaler() != VideoFilterTypeID_None)
			{
				[cce setComputePipelineState:[self pixelScalePipeline]];
				
				if (shouldProcessDisplayMain && (newTexProcess.main != nil) && (_texDisplayPixelScaler[NDSDisplayID_Main] != nil))
				{
					[cce setTexture:newTexProcess.main atIndex:0];
					[cce setTexture:_texDisplayPixelScaler[NDSDisplayID_Main] atIndex:1];
					[cce setTexture:[sharedData texCurrentHQnxLUT] atIndex:2];
					[cce dispatchThreadgroups:_pixelScalerThreadGroupsPerGrid threadsPerThreadgroup:_pixelScalerThreadsPerGroup];
					
					newTexProcess.main = _texDisplayPixelScaler[NDSDisplayID_Main];
					
					if (selectedDisplaySourceMain == selectedDisplaySourceTouch)
					{
						newTexProcess.touch = newTexProcess.main;
					}
				}
				
				if (shouldProcessDisplayTouch && (newTexProcess.touch != nil) && (_texDisplayPixelScaler[NDSDisplayID_Touch] != nil))
				{
					[cce setTexture:newTexProcess.touch atIndex:0];
					[cce setTexture:_texDisplayPixelScaler[NDSDisplayID_Touch] atIndex:1];
					[cce setTexture:[sharedData texCurrentHQnxLUT] atIndex:2];
					[cce dispatchThreadgroups:_pixelScalerThreadGroupsPerGrid threadsPerThreadgroup:_pixelScalerThreadsPerGroup];
					
					newTexProcess.touch = _texDisplayPixelScaler[NDSDisplayID_Touch];
				}
			}
			
			[cce endEncoding];
			
			[newTexProcess.main  retain];
			[newTexProcess.touch retain];
			
			[cb addCompletedHandler:^(id<MTLCommandBuffer> block) {
				const MetalTexturePair oldTexPair = [self texPairProcess];
				[self setTexPairProcess:newTexProcess];
				[oldTexPair.main  release];
				[oldTexPair.touch release];
			}];
			
			[cb commit];
			
			if ([self needsProcessFrameWait])
			{
				[self setNeedsProcessFrameWait:NO];
				[cb waitUntilCompleted];
			}
		}
		else // Otherwise, try a mixed CPU/GPU-based path (may cause a performance hit on pixel download)
		{
			VideoFilter *vfMain  = cdp->GetPixelScalerObject(NDSDisplayID_Main);
			VideoFilter *vfTouch = cdp->GetPixelScalerObject(NDSDisplayID_Touch);
			
			// If source filters are used, we need to use a special pipeline for this particular use case.
			if (useDeposterize)
			{
				id<MTLCommandBuffer> cb = [self newCommandBuffer];
				
				if (shouldProcessDisplayMain && (newTexProcess.main != nil))
				{
					id<MTLComputeCommandEncoder> cce = [cb computeCommandEncoder];
					[cce setComputePipelineState:[sharedData deposterizePipeline]];
					
					[cce setTexture:newTexProcess.main atIndex:0];
					[cce setTexture:_texDisplaySrcDeposterize[NDSDisplayID_Main][0] atIndex:1];
					[cce dispatchThreadgroups:[sharedData deposterizeThreadGroupsPerGrid]
						threadsPerThreadgroup:[sharedData deposterizeThreadsPerGroup]];
					
					[cce setTexture:_texDisplaySrcDeposterize[NDSDisplayID_Main][0] atIndex:0];
					[cce setTexture:_texDisplaySrcDeposterize[NDSDisplayID_Main][1] atIndex:1];
					[cce dispatchThreadgroups:[sharedData deposterizeThreadGroupsPerGrid]
						threadsPerThreadgroup:[sharedData deposterizeThreadsPerGroup]];
					
					newTexProcess.main = _texDisplaySrcDeposterize[NDSDisplayID_Main][1];
					
					if (selectedDisplaySourceMain == selectedDisplaySourceTouch)
					{
						newTexProcess.touch = newTexProcess.main;
					}
					
					[cce endEncoding];
					
					if (_texDisplayPixelScaler[NDSDisplayID_Main] != nil)
					{
						id<MTLBlitCommandEncoder> bce = [cb blitCommandEncoder];
						
						[bce copyFromTexture:_texDisplaySrcDeposterize[NDSDisplayID_Main][1]
								 sourceSlice:0
								 sourceLevel:0
								sourceOrigin:MTLOriginMake(0, 0, 0)
								  sourceSize:MTLSizeMake(vfMain->GetSrcWidth(), vfMain->GetSrcHeight(), 1)
									toBuffer:_bufCPUFilterSrcMain
						   destinationOffset:0
					  destinationBytesPerRow:vfMain->GetSrcWidth() * sizeof(uint32_t)
					destinationBytesPerImage:vfMain->GetSrcWidth() * vfMain->GetSrcHeight() * sizeof(uint32_t)];
						
						[bce endEncoding];
						[cb commit];
						
						cb = [self newCommandBuffer];
						
						[cb addScheduledHandler:^(id<MTLCommandBuffer> block) {
							dispatch_semaphore_wait(((MacMetalDisplayPresenter *)cdp)->GetCPUFilterSemaphore(NDSDisplayID_Main), DISPATCH_TIME_FOREVER);
							vfMain->RunFilter();
							dispatch_semaphore_signal(((MacMetalDisplayPresenter *)cdp)->GetCPUFilterSemaphore(NDSDisplayID_Main));
						}];
						
						bce = [cb blitCommandEncoder];
						
						[bce copyFromBuffer:[self bufCPUFilterDstMain]
							   sourceOffset:0
						  sourceBytesPerRow:vfMain->GetDstWidth() * sizeof(uint32_t)
						sourceBytesPerImage:vfMain->GetDstWidth() * vfMain->GetDstHeight() * sizeof(uint32_t)
								 sourceSize:MTLSizeMake(vfMain->GetDstWidth(), vfMain->GetDstHeight(), 1)
								  toTexture:_texDisplayPixelScaler[NDSDisplayID_Main]
						   destinationSlice:0
						   destinationLevel:0
						  destinationOrigin:MTLOriginMake(0, 0, 0)];
						
						newTexProcess.main = _texDisplayPixelScaler[NDSDisplayID_Main];
						
						if (selectedDisplaySourceMain == selectedDisplaySourceTouch)
						{
							newTexProcess.touch = newTexProcess.main;
						}
						
						[bce endEncoding];
					}
				}
				
				if (shouldProcessDisplayTouch && (newTexProcess.touch != nil))
				{
					id<MTLComputeCommandEncoder> cce = [cb computeCommandEncoder];
					[cce setComputePipelineState:[sharedData deposterizePipeline]];
					
					[cce setTexture:newTexProcess.touch atIndex:0];
					[cce setTexture:_texDisplaySrcDeposterize[NDSDisplayID_Touch][0] atIndex:1];
					[cce dispatchThreadgroups:[sharedData deposterizeThreadGroupsPerGrid]
						threadsPerThreadgroup:[sharedData deposterizeThreadsPerGroup]];
					
					[cce setTexture:_texDisplaySrcDeposterize[NDSDisplayID_Touch][0] atIndex:0];
					[cce setTexture:_texDisplaySrcDeposterize[NDSDisplayID_Touch][1] atIndex:1];
					[cce dispatchThreadgroups:[sharedData deposterizeThreadGroupsPerGrid]
						threadsPerThreadgroup:[sharedData deposterizeThreadsPerGroup]];
					
					newTexProcess.touch = _texDisplaySrcDeposterize[NDSDisplayID_Touch][1];
					
					[cce endEncoding];
					
					if (_texDisplayPixelScaler[NDSDisplayID_Touch] != nil)
					{
						id<MTLBlitCommandEncoder> bce = [cb blitCommandEncoder];
						
						[bce copyFromTexture:_texDisplaySrcDeposterize[NDSDisplayID_Touch][1]
								 sourceSlice:0
								 sourceLevel:0
								sourceOrigin:MTLOriginMake(0, 0, 0)
								  sourceSize:MTLSizeMake(vfTouch->GetSrcWidth(), vfTouch->GetSrcHeight(), 1)
									toBuffer:_bufCPUFilterSrcTouch
						   destinationOffset:0
					  destinationBytesPerRow:vfTouch->GetSrcWidth() * sizeof(uint32_t)
					destinationBytesPerImage:vfTouch->GetSrcWidth() * vfTouch->GetSrcHeight() * sizeof(uint32_t)];
						
						[bce endEncoding];
						[cb commit];
						
						cb = [self newCommandBuffer];
						
						[cb addScheduledHandler:^(id<MTLCommandBuffer> block) {
							dispatch_semaphore_wait(((MacMetalDisplayPresenter *)cdp)->GetCPUFilterSemaphore(NDSDisplayID_Touch), DISPATCH_TIME_FOREVER);
							vfTouch->RunFilter();
							dispatch_semaphore_signal(((MacMetalDisplayPresenter *)cdp)->GetCPUFilterSemaphore(NDSDisplayID_Touch));
						}];
						
						bce = [cb blitCommandEncoder];
						
						[bce copyFromBuffer:[self bufCPUFilterDstTouch]
							   sourceOffset:0
						  sourceBytesPerRow:vfTouch->GetDstWidth() * sizeof(uint32_t)
						sourceBytesPerImage:vfTouch->GetDstWidth() * vfTouch->GetDstHeight() * sizeof(uint32_t)
								 sourceSize:MTLSizeMake(vfTouch->GetDstWidth(), vfTouch->GetDstHeight(), 1)
								  toTexture:_texDisplayPixelScaler[NDSDisplayID_Touch]
						   destinationSlice:0
						   destinationLevel:0
						  destinationOrigin:MTLOriginMake(0, 0, 0)];
						
						newTexProcess.touch = _texDisplayPixelScaler[NDSDisplayID_Touch];
						
						[bce endEncoding];
					}
				}
				
				[newTexProcess.main  retain];
				[newTexProcess.touch retain];
				
				[cb addCompletedHandler:^(id<MTLCommandBuffer> block) {
					const MetalTexturePair oldTexPair = [self texPairProcess];
					[self setTexPairProcess:newTexProcess];
					[oldTexPair.main  release];
					[oldTexPair.touch release];
				}];
				
				[cb commit];
				
				if ([self needsProcessFrameWait])
				{
					[self setNeedsProcessFrameWait:NO];
					[cb waitUntilCompleted];
				}
			}
			else // Otherwise, we can assume to run the CPU-based filters at the start, and then upload as soon as possible.
			{
				id<MTLCommandBuffer> cb = [self newCommandBuffer];
				
				if (shouldProcessDisplayMain && (_texDisplayPixelScaler[NDSDisplayID_Main] != nil))
				{
					[cb addScheduledHandler:^(id<MTLCommandBuffer> block) {
						dispatch_semaphore_wait(((MacMetalDisplayPresenter *)cdp)->GetCPUFilterSemaphore(NDSDisplayID_Main), DISPATCH_TIME_FOREVER);
						vfMain->RunFilter();
						dispatch_semaphore_signal(((MacMetalDisplayPresenter *)cdp)->GetCPUFilterSemaphore(NDSDisplayID_Main));
					}];
					
					id<MTLBlitCommandEncoder> bce = [cb blitCommandEncoder];
					
					[bce copyFromBuffer:[self bufCPUFilterDstMain]
						   sourceOffset:0
					  sourceBytesPerRow:vfMain->GetDstWidth() * sizeof(uint32_t)
					sourceBytesPerImage:vfMain->GetDstWidth() * vfMain->GetDstHeight() * sizeof(uint32_t)
							 sourceSize:MTLSizeMake(vfMain->GetDstWidth(), vfMain->GetDstHeight(), 1)
							  toTexture:_texDisplayPixelScaler[NDSDisplayID_Main]
					   destinationSlice:0
					   destinationLevel:0
					  destinationOrigin:MTLOriginMake(0, 0, 0)];
					
					[bce endEncoding];
					
					[cb commit];
					cb = [self newCommandBuffer];
					
					newTexProcess.main = _texDisplayPixelScaler[NDSDisplayID_Main];
					
					if (selectedDisplaySourceMain == selectedDisplaySourceTouch)
					{
						newTexProcess.touch = newTexProcess.main;
					}
				}
				
				if (shouldProcessDisplayTouch && (_texDisplayPixelScaler[NDSDisplayID_Touch] != nil))
				{
					[cb addScheduledHandler:^(id<MTLCommandBuffer> block) {
						dispatch_semaphore_wait(((MacMetalDisplayPresenter *)cdp)->GetCPUFilterSemaphore(NDSDisplayID_Touch), DISPATCH_TIME_FOREVER);
						vfTouch->RunFilter();
						dispatch_semaphore_signal(((MacMetalDisplayPresenter *)cdp)->GetCPUFilterSemaphore(NDSDisplayID_Touch));
					}];
					
					id<MTLBlitCommandEncoder> bce = [cb blitCommandEncoder];
					
					[bce copyFromBuffer:[self bufCPUFilterDstTouch]
						   sourceOffset:0
					  sourceBytesPerRow:vfTouch->GetDstWidth() * sizeof(uint32_t)
					sourceBytesPerImage:vfTouch->GetDstWidth() * vfTouch->GetDstHeight() * sizeof(uint32_t)
							 sourceSize:MTLSizeMake(vfTouch->GetDstWidth(), vfTouch->GetDstHeight(), 1)
							  toTexture:_texDisplayPixelScaler[NDSDisplayID_Touch]
					   destinationSlice:0
					   destinationLevel:0
					  destinationOrigin:MTLOriginMake(0, 0, 0)];
					
					[bce endEncoding];
					
					newTexProcess.touch = _texDisplayPixelScaler[NDSDisplayID_Touch];
				}
				
				[newTexProcess.main  retain];
				[newTexProcess.touch retain];
				
				[cb addCompletedHandler:^(id<MTLCommandBuffer> block) {
					const MetalTexturePair oldTexPair = [self texPairProcess];
					[self setTexPairProcess:newTexProcess];
					[oldTexPair.main  release];
					[oldTexPair.touch release];
				}];
				
				[cb commit];
				
				if ([self needsProcessFrameWait])
				{
					[self setNeedsProcessFrameWait:NO];
					[cb waitUntilCompleted];
				}
			}
		}
		
		pthread_mutex_unlock(((MacMetalDisplayPresenter *)cdp)->GetMutexProcessPtr());
	}
	else
	{
		[newTexProcess.main  retain];
		[newTexProcess.touch retain];
		
		id<MTLCommandBuffer> cb = [self newCommandBuffer];
		
		[cb addCompletedHandler:^(id<MTLCommandBuffer> block) {
			const MetalTexturePair oldTexPair = [self texPairProcess];
			[self setTexPairProcess:newTexProcess];
			[oldTexPair.main  release];
			[oldTexPair.touch release];
		}];
		
		[cb commit];
		
		if ([self needsProcessFrameWait])
		{
			[self setNeedsProcessFrameWait:NO];
			[cb waitUntilCompleted];
		}
	}
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
	
	MetalRenderFrameInfo mrfi = [self renderFrameInfo];
	uint8_t selectedIndex = mrfi.renderIndex;
	
	willDrawHUD = willDrawHUD && (hudTotalLength > 1);
	if (willDrawHUD && cdp->HUDNeedsUpdate())
	{
		bool stillSearching = true;
		bool forceWait = true;
		
		// First, search for an idle buffer along with its corresponding semaphore.
		if (stillSearching)
		{
			selectedIndex = (selectedIndex + 1) % RENDER_BUFFER_COUNT;
			for (; selectedIndex != mrfi.renderIndex; selectedIndex = (selectedIndex + 1) % RENDER_BUFFER_COUNT)
			{
				if ([self renderBufferStateAtIndex:selectedIndex] == ClientDisplayBufferState_Idle)
				{
					if (dispatch_semaphore_wait(_semRenderBuffers[selectedIndex], DISPATCH_TIME_NOW) == 0)
					{
						stillSearching = false;
						forceWait = false;
						break;
					}
				}
			}
		}
		
		// Next, search for either an idle or a ready buffer along with its corresponding semaphore.
		if (stillSearching)
		{
			selectedIndex = (selectedIndex + 1) % RENDER_BUFFER_COUNT;
			for (size_t spin = 0; spin < 100ULL * RENDER_BUFFER_COUNT; selectedIndex = (selectedIndex + 1) % RENDER_BUFFER_COUNT, spin++)
			{
				if ( ([self renderBufferStateAtIndex:selectedIndex] == ClientDisplayBufferState_Idle) ||
					(([self renderBufferStateAtIndex:selectedIndex] == ClientDisplayBufferState_Ready) && (selectedIndex != mrfi.renderIndex)) )
				{
					if (dispatch_semaphore_wait(_semRenderBuffers[selectedIndex], DISPATCH_TIME_NOW) == 0)
					{
						stillSearching = false;
						forceWait = false;
						break;
					}
				}
			}
		}
		
		// Since the most available buffers couldn't be taken, we're going to spin for some finite
		// period of time until an idle buffer emerges. If that happens, then force wait on the
		// buffer's corresponding semaphore.
		if (stillSearching)
		{
			selectedIndex = (selectedIndex + 1) % RENDER_BUFFER_COUNT;
			for (size_t spin = 0; spin < 10000ULL * RENDER_BUFFER_COUNT; selectedIndex = (selectedIndex + 1) % RENDER_BUFFER_COUNT, spin++)
			{
				if ([self renderBufferStateAtIndex:selectedIndex] == ClientDisplayBufferState_Idle)
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
			selectedIndex = (selectedIndex + 1) % RENDER_BUFFER_COUNT;
			for (; selectedIndex != mrfi.renderIndex; selectedIndex = (selectedIndex + 1) % RENDER_BUFFER_COUNT)
			{
				if ( ([self renderBufferStateAtIndex:selectedIndex] == ClientDisplayBufferState_Idle) ||
					 ([self renderBufferStateAtIndex:selectedIndex] == ClientDisplayBufferState_Ready) ||
					 ([self renderBufferStateAtIndex:selectedIndex] == ClientDisplayBufferState_Reading) )
				{
					stillSearching = false;
					break;
				}
			}
		}
		
		if (forceWait)
		{
			dispatch_semaphore_wait(_semRenderBuffers[selectedIndex], DISPATCH_TIME_FOREVER);
		}
		
		[self setRenderBufferState:ClientDisplayBufferState_Writing index:selectedIndex];
		
		cdp->SetHUDPositionVertices((float)cdp->GetPresenterProperties().clientWidth, (float)cdp->GetPresenterProperties().clientHeight, (float *)[_hudVtxPositionBuffer[selectedIndex] contents]);
		cdp->SetHUDColorVertices((uint32_t *)[_hudVtxColorBuffer[selectedIndex] contents]);
		cdp->SetHUDTextureCoordinates((float *)[_hudTexCoordBuffer[selectedIndex] contents]);
		
		[self setRenderBufferState:ClientDisplayBufferState_Ready index:selectedIndex];
		dispatch_semaphore_signal(_semRenderBuffers[selectedIndex]);
		
		cdp->ClearHUDNeedsUpdate();
	}
	
	mrfi.renderIndex = selectedIndex;
	mrfi.needEncodeViewport = needEncodeViewport;
	mrfi.newViewport = newViewport;
	mrfi.willDrawHUD = willDrawHUD;
	mrfi.willDrawHUDInput = cdp->GetHUDShowInput();
	mrfi.hudStringLength = cdp->GetHUDString().length();
	mrfi.hudTouchLineLength = hudTouchLineLength;
	mrfi.hudTotalLength = hudTotalLength;
	
	[self setRenderFrameInfo:mrfi];
}

- (void) renderForCommandBuffer:(id<MTLCommandBuffer>)cb
			outputPipelineState:(id<MTLRenderPipelineState>)outputPipelineState
			   hudPipelineState:(id<MTLRenderPipelineState>)hudPipelineState
					texDisplays:(MetalTexturePair)texDisplay
						   mrfi:(MetalRenderFrameInfo)mrfi
						doYFlip:(BOOL)willFlip
{
	// Generate the command encoder.
	id<MTLRenderCommandEncoder> rce = [cb renderCommandEncoderWithDescriptor:_outputRenderPassDesc];
	
	cdp->SetScreenTextureCoordinates((float)[texDisplay.main  width], (float)[texDisplay.main  height],
									 (float)[texDisplay.touch width], (float)[texDisplay.touch height],
									 _texCoordBuffer);
	
	if (mrfi.needEncodeViewport)
	{
		[rce setViewport:mrfi.newViewport];
	}
	
	// Draw the NDS displays.
	const uint8_t doYFlip = (willFlip) ? 1 : 0;
	const NDSDisplayInfo &displayInfo = cdp->GetEmuDisplayInfo();
	const float backlightIntensity[2] = { displayInfo.backlightIntensity[NDSDisplayID_Main], displayInfo.backlightIntensity[NDSDisplayID_Touch] };
	
	[rce setRenderPipelineState:outputPipelineState];
	[rce setVertexBytes:_vtxPositionBuffer length:sizeof(_vtxPositionBuffer) atIndex:0];
	[rce setVertexBytes:_texCoordBuffer length:sizeof(_texCoordBuffer) atIndex:1];
	[rce setVertexBytes:&_cdvPropertiesBuffer length:sizeof(_cdvPropertiesBuffer) atIndex:2];
	[rce setVertexBytes:&doYFlip length:sizeof(uint8_t) atIndex:3];
	
	switch (cdp->GetPresenterProperties().mode)
	{
		case ClientDisplayMode_Main:
		{
			if ( (texDisplay.main != nil) && cdp->IsSelectedDisplayEnabled(NDSDisplayID_Main) )
			{
				[rce setFragmentBytes:&backlightIntensity[NDSDisplayID_Main] length:sizeof(backlightIntensity[NDSDisplayID_Main]) atIndex:0];
				[rce setFragmentTexture:texDisplay.main atIndex:0];
				[rce drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
			}
			break;
		}
			
		case ClientDisplayMode_Touch:
		{
			if ( (texDisplay.touch != nil) && cdp->IsSelectedDisplayEnabled(NDSDisplayID_Touch) )
			{
				[rce setFragmentBytes:&backlightIntensity[NDSDisplayID_Touch] length:sizeof(backlightIntensity[NDSDisplayID_Touch]) atIndex:0];
				[rce setFragmentTexture:texDisplay.touch atIndex:0];
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
					if ( (texDisplay.tex[majorDisplayID] != nil) && cdp->IsSelectedDisplayEnabled(majorDisplayID) )
					{
						[rce setFragmentBytes:&backlightIntensity[majorDisplayID] length:sizeof(backlightIntensity[majorDisplayID]) atIndex:0];
						[rce setFragmentTexture:texDisplay.tex[majorDisplayID] atIndex:0];
						[rce drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:majorDisplayVtx vertexCount:4];
					}
					break;
				}
					
				default:
					break;
			}
			
			if ( (texDisplay.main != nil) && cdp->IsSelectedDisplayEnabled(NDSDisplayID_Main) )
			{
				[rce setFragmentBytes:&backlightIntensity[NDSDisplayID_Main] length:sizeof(backlightIntensity[NDSDisplayID_Main]) atIndex:0];
				[rce setFragmentTexture:texDisplay.main atIndex:0];
				[rce drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
			}
			
			if ( (texDisplay.touch != nil) && cdp->IsSelectedDisplayEnabled(NDSDisplayID_Touch) )
			{
				[rce setFragmentBytes:&backlightIntensity[NDSDisplayID_Touch] length:sizeof(backlightIntensity[NDSDisplayID_Touch]) atIndex:0];
				[rce setFragmentTexture:texDisplay.touch atIndex:0];
				[rce drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:4 vertexCount:4];
			}
		}
			
		default:
			break;
	}
	
	// Draw the HUD.
	[self renderStartAtIndex:mrfi.renderIndex];
	
	if (mrfi.willDrawHUD)
	{
		uint8_t isScreenOverlay = 0;
		
		[rce setRenderPipelineState:hudPipelineState];
		[rce setVertexBuffer:_hudVtxPositionBuffer[mrfi.renderIndex] offset:0 atIndex:0];
		[rce setVertexBuffer:_hudVtxColorBuffer[mrfi.renderIndex]    offset:0 atIndex:1];
		[rce setVertexBuffer:_hudTexCoordBuffer[mrfi.renderIndex]    offset:0 atIndex:2];
		[rce setVertexBytes:&_cdvPropertiesBuffer length:sizeof(_cdvPropertiesBuffer) atIndex:3];
		[rce setVertexBytes:&doYFlip length:sizeof(uint8_t) atIndex:4];
		[rce setFragmentTexture:[self texHUDCharMap] atIndex:0];
		
		// First, draw the inputs.
		if (mrfi.willDrawHUDInput)
		{
			isScreenOverlay = 1;
			[rce setVertexBytes:&isScreenOverlay length:sizeof(uint8_t) atIndex:5];
			[rce setFragmentSamplerState:[sharedData samplerHUDBox] atIndex:0];
			[rce drawIndexedPrimitives:MTLPrimitiveTypeTriangle
							indexCount:mrfi.hudTouchLineLength * 6
							 indexType:MTLIndexTypeUInt16
						   indexBuffer:[sharedData hudIndexBuffer]
					 indexBufferOffset:(mrfi.hudStringLength + HUD_INPUT_ELEMENT_LENGTH) * 6 * sizeof(uint16_t)];
			
			isScreenOverlay = 0;
			[rce setVertexBytes:&isScreenOverlay length:sizeof(uint8_t) atIndex:5];
			[rce setFragmentSamplerState:[sharedData samplerHUDText] atIndex:0];
			[rce drawIndexedPrimitives:MTLPrimitiveTypeTriangle
							indexCount:HUD_INPUT_ELEMENT_LENGTH * 6
							 indexType:MTLIndexTypeUInt16
						   indexBuffer:[sharedData hudIndexBuffer]
					 indexBufferOffset:mrfi.hudStringLength * 6 * sizeof(uint16_t)];
		}
		
		// Next, draw the backing text box.
		[rce setVertexBytes:&isScreenOverlay length:sizeof(uint8_t) atIndex:5];
		[rce setFragmentSamplerState:[sharedData samplerHUDBox] atIndex:0];
		[rce drawIndexedPrimitives:MTLPrimitiveTypeTriangle
						indexCount:6
						 indexType:MTLIndexTypeUInt16
					   indexBuffer:[sharedData hudIndexBuffer]
				 indexBufferOffset:0];
		
		// Finally, draw each character inside the box.
		[rce setFragmentSamplerState:[sharedData samplerHUDText] atIndex:0];
		[rce drawIndexedPrimitives:MTLPrimitiveTypeTriangle
						indexCount:(mrfi.hudStringLength - 1) * 6
						 indexType:MTLIndexTypeUInt16
					   indexBuffer:[sharedData hudIndexBuffer]
				 indexBufferOffset:6 * sizeof(uint16_t)];
	}
	
	[rce endEncoding];
}

- (void) renderStartAtIndex:(uint8_t)index
{
	dispatch_semaphore_wait(_semRenderBuffers[index], DISPATCH_TIME_FOREVER);
	[self setRenderBufferState:ClientDisplayBufferState_PendingRead index:index];
}

- (void) renderFinishAtIndex:(uint8_t)index
{
	[self setRenderBufferState:ClientDisplayBufferState_Idle index:index];
	dispatch_semaphore_signal(_semRenderBuffers[index]);
}

- (ClientDisplayBufferState) renderBufferStateAtIndex:(uint8_t)index
{
	OSSpinLockLock(&_spinlockRenderBufferStates[index]);
	const ClientDisplayBufferState bufferState = _renderBufferState[index];
	OSSpinLockUnlock(&_spinlockRenderBufferStates[index]);
	
	return bufferState;
}

- (void) setRenderBufferState:(ClientDisplayBufferState)bufferState index:(uint8_t)index
{
	OSSpinLockLock(&_spinlockRenderBufferStates[index]);
	_renderBufferState[index] = bufferState;
	OSSpinLockUnlock(&_spinlockRenderBufferStates[index]);
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
		
		const MetalTexturePair texProcess = [self texPairProcess];
		const MetalRenderFrameInfo mrfi = [self renderFrameInfo];
		
		[self renderForCommandBuffer:cb
				 outputPipelineState:[self outputRGBAPipeline]
					hudPipelineState:[sharedData hudRGBAPipeline]
						 texDisplays:texProcess
								mrfi:mrfi
							 doYFlip:NO];
		
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
		
		[cb addScheduledHandler:^(id<MTLCommandBuffer> block) {
			[self setRenderBufferState:ClientDisplayBufferState_Reading index:mrfi.renderIndex];
		}];
		
		[cb addCompletedHandler:^(id<MTLCommandBuffer> block) {
			[self renderFinishAtIndex:mrfi.renderIndex];
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
@synthesize layerDrawable0;
@synthesize layerDrawable1;
@synthesize layerDrawable2;

- (id) initWithDisplayPresenterObject:(MacMetalDisplayPresenterObject *)thePresenterObject
{
	self = [super init];
	if(self == nil)
	{
		return nil;
	}
	
	_cdv = NULL;
	_semDrawable = dispatch_semaphore_create(3);
	_currentDrawable = nil;
	layerDrawable0 = nil;
	layerDrawable1 = nil;
	layerDrawable2 = nil;
	
	_displayTexturePair.bufferIndex = 0;
	_displayTexturePair.fetchSequenceNumber = 0;
	_displayTexturePair.main  = nil;
	_displayTexturePair.touch = nil;
	
	presenterObject = thePresenterObject;
	if (thePresenterObject != nil)
	{
		[self setDevice:[[thePresenterObject sharedData] device]];
		[thePresenterObject setDrawableFormat:[self pixelFormat]];
	}
	
	[self setOpaque:YES];
	
	return self;
}

- (void)dealloc
{
	[self setLayerDrawable0:nil];
	[self setLayerDrawable1:nil];
	[self setLayerDrawable2:nil];
	dispatch_release(_semDrawable);
	
	[_displayTexturePair.main  release];
	[_displayTexturePair.touch release];
	
	[super dealloc];
}

- (void) setupLayer
{
	if ([self device] == nil)
	{
		[self setDevice:[[presenterObject sharedData] device]];
		[presenterObject setDrawableFormat:[self pixelFormat]];
	}
}

- (void) renderToDrawableUsingCommandBuffer:(id<MTLCommandBuffer>)cb
{
	dispatch_semaphore_wait(_semDrawable, DISPATCH_TIME_FOREVER);
	
	id<CAMetalDrawable> drawable = [self nextDrawable];
	if (drawable == nil)
	{
		_currentDrawable = nil;
		dispatch_semaphore_signal(_semDrawable);
		return;
	}
	else
	{
		if ([self layerDrawable0] == nil)
		{
			[self setLayerDrawable0:drawable];
		}
		else if ([self layerDrawable1] == nil)
		{
			[self setLayerDrawable1:drawable];
		}
		else if ([self layerDrawable2] == nil)
		{
			[self setLayerDrawable2:drawable];
		}
	}
	
	id<MTLTexture> texDrawable = [drawable texture];
	[[presenterObject colorAttachment0Desc] setTexture:texDrawable];
	
	const MetalTexturePair texProcess = [presenterObject texPairProcess];
	id<MTLTexture> oldTexMain  = _displayTexturePair.main;
	id<MTLTexture> oldTexTouch = _displayTexturePair.touch;
	
	_displayTexturePair.bufferIndex = texProcess.bufferIndex;
	_displayTexturePair.fetchSequenceNumber = texProcess.fetchSequenceNumber;
	_displayTexturePair.main  = [texProcess.main  retain];
	_displayTexturePair.touch = [texProcess.touch retain];
	
	[oldTexMain  release];
	[oldTexTouch release];
	
	const MetalRenderFrameInfo mrfi = [presenterObject renderFrameInfo];
	
	[presenterObject renderForCommandBuffer:cb
						outputPipelineState:[presenterObject outputDrawablePipeline]
						   hudPipelineState:[[presenterObject sharedData] hudPipeline]
								texDisplays:_displayTexturePair
									   mrfi:mrfi
									doYFlip:NO];
	
	[cb addScheduledHandler:^(id<MTLCommandBuffer> block) {
		[presenterObject setRenderBufferState:ClientDisplayBufferState_Reading index:mrfi.renderIndex];
	}];
	
	[cb addCompletedHandler:^(id<MTLCommandBuffer> block) {
		[presenterObject renderFinishAtIndex:mrfi.renderIndex];
	}];
	
	_currentDrawable = drawable;
}

- (void) presentDrawableWithCommandBuffer:(id<MTLCommandBuffer>)cb outputTime:(uint64_t)outputTime
{
	id<CAMetalDrawable> drawable = _currentDrawable;
	if (drawable == nil)
	{
		printf("Metal: No drawable was assigned!\n");
		return;
	}
	
	// Apple's documentation might seem to suggest that [MTLCommandBuffer presentDrawable:atTime:]
	// and [MTLDrawable presentAtTime:] inside of a [MTLCommandBuffer addScheduledHandler:] block
	// are equivalent. However, much testing has shown that this is NOT the case.
	//
	// So rather than using [MTLCommandBuffer presentDrawable:atTime:], which causes Metal to
	// present the drawable whenever it pleases, we manually call [MTLDrawable presentAtTime] so
	// that we can synchronously force the presentation order of the drawables. If we don't do
	// this, then Metal may start presenting the drawables in some random order, causing some
	// really nasty microstuttering.
	
	[cb addScheduledHandler:^(id<MTLCommandBuffer> block) {
		@autoreleasepool
		{
			[drawable presentAtTime:(CFTimeInterval)outputTime / 1000000000.0];
			
			if (drawable == [self layerDrawable0])
			{
				[self setLayerDrawable0:nil];
			}
			else if (drawable == [self layerDrawable1])
			{
				[self setLayerDrawable1:nil];
			}
			else if (drawable == [self layerDrawable2])
			{
				[self setLayerDrawable2:nil];
			}
		}
		
		dispatch_semaphore_signal(_semDrawable);
	}];
}

- (void) renderAndPresentDrawableImmediate
{
	@autoreleasepool
	{
		id<MTLCommandBuffer> cbFlush = [presenterObject newCommandBuffer];
		id<MTLCommandBuffer> cbFinalize = [presenterObject newCommandBuffer];
		
		_cdv->FlushView(cbFlush);
		_cdv->FinalizeFlush(cbFinalize, 0);
		
		[cbFlush enqueue];
		[cbFinalize enqueue];
		
		[cbFlush commit];
		[cbFinalize commit];
		
#ifdef DEBUG
		[[[presenterObject sharedData] commandQueue] insertDebugCaptureBoundary];
#endif
	}
}

- (void) display
{
	[self renderAndPresentDrawableImmediate];
}

@end

#pragma mark -

MacMetalFetchObject::MacMetalFetchObject()
{
	_useDirectToCPUFilterPipeline = true;
	
	_srcNativeCloneMaster = (uint32_t *)malloc_alignedPage(GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * 2 * METAL_FETCH_BUFFER_COUNT * sizeof(uint32_t));
	memset(_srcNativeCloneMaster, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * 2 * METAL_FETCH_BUFFER_COUNT * sizeof(uint32_t));
	
	for (size_t i = 0; i < METAL_FETCH_BUFFER_COUNT; i++)
	{
		pthread_rwlock_init(&_srcCloneRWLock[NDSDisplayID_Main][i],  NULL);
		pthread_rwlock_init(&_srcCloneRWLock[NDSDisplayID_Touch][i], NULL);
		_srcNativeClone[NDSDisplayID_Main][i]  = _srcNativeCloneMaster + (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * ((i * 2) + 0));
		_srcNativeClone[NDSDisplayID_Touch][i] = _srcNativeCloneMaster + (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * ((i * 2) + 1));
	}
	
	_clientData = [[MetalDisplayViewSharedData alloc] init];
}

MacMetalFetchObject::~MacMetalFetchObject()
{
	[(MetalDisplayViewSharedData *)this->_clientData release];
	
	for (size_t i = 0; i < METAL_FETCH_BUFFER_COUNT; i++)
	{
		pthread_rwlock_wrlock(&this->_srcCloneRWLock[NDSDisplayID_Main][i]);
		pthread_rwlock_wrlock(&this->_srcCloneRWLock[NDSDisplayID_Touch][i]);
		this->_srcNativeClone[NDSDisplayID_Main][i]  = NULL;
		this->_srcNativeClone[NDSDisplayID_Touch][i] = NULL;
	}
	
	free_aligned(this->_srcNativeCloneMaster);
	this->_srcNativeCloneMaster = NULL;
	
	for (size_t i = METAL_FETCH_BUFFER_COUNT - 1; i < METAL_FETCH_BUFFER_COUNT; i--)
	{
		pthread_rwlock_unlock(&this->_srcCloneRWLock[NDSDisplayID_Touch][i]);
		pthread_rwlock_unlock(&this->_srcCloneRWLock[NDSDisplayID_Main][i]);
		pthread_rwlock_destroy(&this->_srcCloneRWLock[NDSDisplayID_Touch][i]);
		pthread_rwlock_destroy(&this->_srcCloneRWLock[NDSDisplayID_Main][i]);
	}
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
	this->GPUClientFetchObject::SetFetchBuffers(currentDisplayInfo);
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
	
	[(MetalDisplayViewSharedData *)this->_clientData fetchNativeDisplayByID:displayID bufferIndex:bufferIndex blitCommandEncoder:[(MetalDisplayViewSharedData *)this->_clientData bceFetch]];
}

void MacMetalFetchObject::_FetchCustomDisplayByID(const NDSDisplayID displayID, const u8 bufferIndex)
{
	[(MetalDisplayViewSharedData *)this->_clientData fetchCustomDisplayByID:displayID bufferIndex:bufferIndex blitCommandEncoder:[(MetalDisplayViewSharedData *)this->_clientData bceFetch]];
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
	
	dispatch_release(this->_semCPUFilter[NDSDisplayID_Main]);
	dispatch_release(this->_semCPUFilter[NDSDisplayID_Touch]);
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
	
	_semCPUFilter[NDSDisplayID_Main]  = dispatch_semaphore_create(1);
	_semCPUFilter[NDSDisplayID_Touch] = dispatch_semaphore_create(1);
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
		
		dispatch_semaphore_wait(this->_semCPUFilter[displayID], DISPATCH_TIME_FOREVER);
		fetchObjMutable.CopyFromSrcClone(vf->GetSrcBufferPtr(), displayID, bufferIndex);
		dispatch_semaphore_signal(this->_semCPUFilter[displayID]);
	}
}

void MacMetalDisplayPresenter::_ResizeCPUPixelScaler(const VideoFilterTypeID filterID)
{
	dispatch_semaphore_wait(this->_semCPUFilter[NDSDisplayID_Main],  DISPATCH_TIME_FOREVER);
	dispatch_semaphore_wait(this->_semCPUFilter[NDSDisplayID_Touch], DISPATCH_TIME_FOREVER);
	
	this->ClientDisplay3DPresenter::_ResizeCPUPixelScaler(filterID);
	[this->_presenterObject resizeCPUPixelScalerUsingFilterID:filterID];
	
	dispatch_semaphore_signal(this->_semCPUFilter[NDSDisplayID_Touch]);
	dispatch_semaphore_signal(this->_semCPUFilter[NDSDisplayID_Main]);
}

MacMetalDisplayPresenterObject* MacMetalDisplayPresenter::GetPresenterObject() const
{
	return this->_presenterObject;
}

pthread_mutex_t* MacMetalDisplayPresenter::GetMutexProcessPtr()
{
	return &this->_mutexProcessPtr;
}

dispatch_semaphore_t MacMetalDisplayPresenter::GetCPUFilterSemaphore(const NDSDisplayID displayID)
{
	return this->_semCPUFilter[displayID];
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
	[this->_presenterObject setNeedsProcessFrameWait:YES];
	
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
	[this->_presenterObject setNeedsProcessFrameWait:YES];
	
	pthread_mutex_unlock(&this->_mutexProcessPtr);
}

// NDS GPU interface
void MacMetalDisplayPresenter::ProcessDisplays()
{
	[this->_presenterObject processDisplays];
}

void MacMetalDisplayPresenter::UpdateLayout()
{
	[this->_presenterObject updateRenderBuffers];
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
	
	if (this->GetRenderToCALayer())
	{
		this->_presenter->UpdateLayout();
		[this->_caLayer setNeedsDisplay];
		[CATransaction flush];
	}
	else
	{
		// For every update, ensure that the CVDisplayLink is started so that the update
		// will eventually get flushed.
		this->SetAllowViewFlushes(true);
		this->_presenter->UpdateLayout();
		
		OSSpinLockLock(&this->_spinlockViewNeedsFlush);
		this->_viewNeedsFlush = true;
		OSSpinLockUnlock(&this->_spinlockViewNeedsFlush);
	}
}

void MacMetalDisplayView::SetAllowViewFlushes(bool allowFlushes)
{
	CGDirectDisplayID displayID = (CGDirectDisplayID)this->GetDisplayViewID();
	MacClientSharedObject *sharedData = ((MacMetalDisplayPresenter *)this->_presenter)->GetSharedData();
	[sharedData displayLinkStartUsingID:displayID];
}

void MacMetalDisplayView::FlushView(void *userData)
{
	OSSpinLockLock(&this->_spinlockViewNeedsFlush);
	this->_viewNeedsFlush = false;
	OSSpinLockUnlock(&this->_spinlockViewNeedsFlush);
	
	[(DisplayViewMetalLayer *)this->_caLayer renderToDrawableUsingCommandBuffer:(id<MTLCommandBuffer>)userData];
}

void MacMetalDisplayView::FinalizeFlush(void *userData, uint64_t outputTime)
{
	[(DisplayViewMetalLayer *)this->_caLayer presentDrawableWithCommandBuffer:(id<MTLCommandBuffer>)userData outputTime:outputTime];
}

void MacMetalDisplayView::FlushAndFinalizeImmediate()
{
	[(DisplayViewMetalLayer *)this->_caLayer renderAndPresentDrawableImmediate];
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
