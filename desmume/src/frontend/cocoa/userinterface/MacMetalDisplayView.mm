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
	
	commandQueue = [[device newCommandQueue] retain];
	defaultLibrary = [[device newDefaultLibrary] retain];
	_fetch555Pipeline = [[device newComputePipelineStateWithFunction:[defaultLibrary newFunctionWithName:@"nds_fetch555"] error:nil] retain];
	_fetch555ConvertOnlyPipeline = [[device newComputePipelineStateWithFunction:[defaultLibrary newFunctionWithName:@"nds_fetch555ConvertOnly"] error:nil] retain];
	_fetch666Pipeline = [[device newComputePipelineStateWithFunction:[defaultLibrary newFunctionWithName:@"nds_fetch666"] error:nil] retain];
	_fetch888Pipeline = [[device newComputePipelineStateWithFunction:[defaultLibrary newFunctionWithName:@"nds_fetch888"] error:nil] retain];
	deposterizePipeline = [[device newComputePipelineStateWithFunction:[defaultLibrary newFunctionWithName:@"src_filter_deposterize"] error:nil] retain];
	
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
	
	hudIndexBuffer = [[device newBufferWithLength:(sizeof(uint16_t) * HUD_TOTAL_ELEMENTS * 6) options:MTLResourceStorageModeManaged] retain];
	
	uint16_t *idxBufferPtr = (uint16_t *)[hudIndexBuffer contents];
	for (size_t i = 0, j = 0, k = 0; i < HUD_TOTAL_ELEMENTS; i++, j+=6, k+=4)
	{
		idxBufferPtr[j+0] = k+0;
		idxBufferPtr[j+1] = k+1;
		idxBufferPtr[j+2] = k+2;
		idxBufferPtr[j+3] = k+2;
		idxBufferPtr[j+4] = k+3;
		idxBufferPtr[j+5] = k+0;
	}
	
	[hudIndexBuffer didModifyRange:NSMakeRange(0, sizeof(uint16_t) * HUD_TOTAL_ELEMENTS * 6)];
	
	_bufDisplayFetchNative[NDSDisplayID_Main][0]  = nil;
	_bufDisplayFetchNative[NDSDisplayID_Main][1]  = nil;
	_bufDisplayFetchNative[NDSDisplayID_Touch][0] = nil;
	_bufDisplayFetchNative[NDSDisplayID_Touch][1] = nil;
	_bufDisplayFetchCustom[NDSDisplayID_Main][0]  = nil;
	_bufDisplayFetchCustom[NDSDisplayID_Main][1]  = nil;
	_bufDisplayFetchCustom[NDSDisplayID_Touch][0] = nil;
	_bufDisplayFetchCustom[NDSDisplayID_Touch][1] = nil;
	
	_bufMasterBrightMode[NDSDisplayID_Main]  = [[device newBufferWithLength:sizeof(uint8_t) * GPU_FRAMEBUFFER_NATIVE_HEIGHT options:MTLResourceStorageModeManaged] retain];
	_bufMasterBrightMode[NDSDisplayID_Touch] = [[device newBufferWithLength:sizeof(uint8_t) * GPU_FRAMEBUFFER_NATIVE_HEIGHT options:MTLResourceStorageModeManaged] retain];
	_bufMasterBrightIntensity[NDSDisplayID_Main]  = [[device newBufferWithLength:sizeof(uint8_t) * GPU_FRAMEBUFFER_NATIVE_HEIGHT options:MTLResourceStorageModeManaged] retain];
	_bufMasterBrightIntensity[NDSDisplayID_Touch] = [[device newBufferWithLength:sizeof(uint8_t) * GPU_FRAMEBUFFER_NATIVE_HEIGHT options:MTLResourceStorageModeManaged] retain];
	
	// Set up HUD texture samplers.
	MTLSamplerDescriptor *samplerDesc = [[MTLSamplerDescriptor alloc] init];
	[samplerDesc setNormalizedCoordinates:YES];
	[samplerDesc setRAddressMode:MTLSamplerAddressModeClampToEdge];
	[samplerDesc setSAddressMode:MTLSamplerAddressModeClampToEdge];
	[samplerDesc setTAddressMode:MTLSamplerAddressModeClampToEdge];
	[samplerDesc setMinFilter:MTLSamplerMinMagFilterNearest];
	[samplerDesc setMagFilter:MTLSamplerMinMagFilterNearest];
	[samplerDesc setMipFilter:MTLSamplerMipFilterNearest];
	samplerHUDBox = [[device newSamplerStateWithDescriptor:samplerDesc] retain];
	
	[samplerDesc setRAddressMode:MTLSamplerAddressModeClampToZero];
	[samplerDesc setSAddressMode:MTLSamplerAddressModeClampToZero];
	[samplerDesc setTAddressMode:MTLSamplerAddressModeClampToZero];
	[samplerDesc setMinFilter:MTLSamplerMinMagFilterLinear];
	[samplerDesc setMagFilter:MTLSamplerMinMagFilterLinear];
	[samplerDesc setMipFilter:MTLSamplerMipFilterLinear];
	samplerHUDText = [[device newSamplerStateWithDescriptor:samplerDesc] retain];
	
	[samplerDesc release];
	
	// Set up the loading textures. These are special because they copy the raw image data from the emulator to the GPU.
	MTLTextureDescriptor *texDisplayLoad16Desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR16Uint
																									width:GPU_FRAMEBUFFER_NATIVE_WIDTH
																								   height:GPU_FRAMEBUFFER_NATIVE_HEIGHT
																								mipmapped:NO];
	[texDisplayLoad16Desc setResourceOptions:MTLResourceStorageModeManaged];
	[texDisplayLoad16Desc setStorageMode:MTLStorageModeManaged];
	[texDisplayLoad16Desc setCpuCacheMode:MTLCPUCacheModeWriteCombined];
	[texDisplayLoad16Desc setUsage:MTLTextureUsageShaderRead];
	
	_texDisplayFetch16NativeMain  = [[device newTextureWithDescriptor:texDisplayLoad16Desc] retain];
	_texDisplayFetch16NativeTouch = [[device newTextureWithDescriptor:texDisplayLoad16Desc] retain];
	_texDisplayFetch16CustomMain  = [[device newTextureWithDescriptor:texDisplayLoad16Desc] retain];
	_texDisplayFetch16CustomTouch = [[device newTextureWithDescriptor:texDisplayLoad16Desc] retain];
	
	MTLTextureDescriptor *texDisplayLoad32Desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
																									width:GPU_FRAMEBUFFER_NATIVE_WIDTH
																								   height:GPU_FRAMEBUFFER_NATIVE_HEIGHT
																								mipmapped:NO];
	[texDisplayLoad32Desc setResourceOptions:MTLResourceStorageModeManaged];
	[texDisplayLoad32Desc setStorageMode:MTLStorageModeManaged];
	[texDisplayLoad32Desc setCpuCacheMode:MTLCPUCacheModeWriteCombined];
	
	[texDisplayLoad32Desc setUsage:MTLTextureUsageShaderRead];
	_texDisplayFetch32NativeMain  = [[device newTextureWithDescriptor:texDisplayLoad32Desc] retain];
	_texDisplayFetch32NativeTouch = [[device newTextureWithDescriptor:texDisplayLoad32Desc] retain];
	_texDisplayFetch32CustomMain  = [[device newTextureWithDescriptor:texDisplayLoad32Desc] retain];
	_texDisplayFetch32CustomTouch = [[device newTextureWithDescriptor:texDisplayLoad32Desc] retain];
	
	[texDisplayLoad32Desc setUsage:MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite];
	_texDisplayPostprocessNativeMain  = [[device newTextureWithDescriptor:texDisplayLoad32Desc] retain];
	_texDisplayPostprocessNativeTouch = [[device newTextureWithDescriptor:texDisplayLoad32Desc] retain];
	_texDisplayPostprocessCustomMain  = [[device newTextureWithDescriptor:texDisplayLoad32Desc] retain];
	_texDisplayPostprocessCustomTouch = [[device newTextureWithDescriptor:texDisplayLoad32Desc] retain];
	
	uint32_t *blankBuffer = (uint32_t *)calloc(GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT, sizeof(uint32_t));
	const MTLRegion texRegionNative = MTLRegionMake2D(0, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT);
	[_texDisplayFetch32NativeMain  replaceRegion:texRegionNative
									 mipmapLevel:0
									   withBytes:blankBuffer
									 bytesPerRow:GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(uint32_t)];
	[_texDisplayFetch32NativeTouch replaceRegion:texRegionNative
									 mipmapLevel:0
									   withBytes:blankBuffer
									 bytesPerRow:GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(uint32_t)];
	[_texDisplayFetch32CustomMain  replaceRegion:texRegionNative
									 mipmapLevel:0
									   withBytes:blankBuffer
									 bytesPerRow:GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(uint32_t)];
	[_texDisplayFetch32CustomTouch replaceRegion:texRegionNative
									 mipmapLevel:0
									   withBytes:blankBuffer
									 bytesPerRow:GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(uint32_t)];
	
	[_texDisplayPostprocessNativeMain  replaceRegion:texRegionNative
										 mipmapLevel:0
										   withBytes:blankBuffer
										 bytesPerRow:GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(uint32_t)];
	[_texDisplayPostprocessNativeTouch replaceRegion:texRegionNative
										 mipmapLevel:0
										   withBytes:blankBuffer
										 bytesPerRow:GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(uint32_t)];
	[_texDisplayPostprocessCustomMain  replaceRegion:texRegionNative
										 mipmapLevel:0
										   withBytes:blankBuffer
										 bytesPerRow:GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(uint32_t)];
	[_texDisplayPostprocessCustomTouch replaceRegion:texRegionNative
										 mipmapLevel:0
										   withBytes:blankBuffer
										 bytesPerRow:GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(uint32_t)];
	free(blankBuffer);
	
	texFetchMain  = [_texDisplayFetch32NativeMain retain];
	texFetchTouch = [_texDisplayFetch32NativeTouch retain];
	
	_isUsingFramebufferDirectly[NDSDisplayID_Main][0] = 1;
	_isUsingFramebufferDirectly[NDSDisplayID_Main][1] = 1;
	_isUsingFramebufferDirectly[NDSDisplayID_Touch][0] = 1;
	_isUsingFramebufferDirectly[NDSDisplayID_Touch][1] = 1;
	
	// Set up the HQnx LUT textures.
	SetupHQnxLUTs_Metal(device, texLQ2xLUT, texHQ2xLUT, texHQ3xLUT, texHQ4xLUT);
	texCurrentHQnxLUT = nil;
	
	_fetchEncoder = nil;
	
	return self;
}

- (void)dealloc
{
	[device release];
	
	[commandQueue release];
	[defaultLibrary release];
	[_fetch555Pipeline release];
	[_fetch555ConvertOnlyPipeline release];
	[_fetch666Pipeline release];
	[_fetch888Pipeline release];
	[deposterizePipeline release];
	[hudPipeline release];
	[hudRGBAPipeline release];
	[hudIndexBuffer release];
	
	[_bufMasterBrightMode[NDSDisplayID_Main] release];
	[_bufMasterBrightMode[NDSDisplayID_Touch] release];
	[_bufMasterBrightIntensity[NDSDisplayID_Main] release];
	[_bufMasterBrightIntensity[NDSDisplayID_Touch] release];
	
	[_texDisplayFetch16NativeMain release];
	[_texDisplayFetch16NativeTouch release];
	[_texDisplayFetch32NativeMain release];
	[_texDisplayFetch32NativeTouch release];
	[_texDisplayPostprocessNativeMain release];
	[_texDisplayPostprocessNativeTouch release];
	
	[_texDisplayFetch16CustomMain release];
	[_texDisplayFetch16CustomTouch release];
	[_texDisplayFetch32CustomMain release];
	[_texDisplayFetch32CustomTouch release];
	[_texDisplayPostprocessCustomMain release];
	[_texDisplayPostprocessCustomTouch release];
	
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

- (void) setUsingFramebufferDirectlyAtIndex:(const u8)index displayID:(NDSDisplayID)displayID state:(bool)theState
{
	if (theState)
	{
		OSAtomicOr32(1, &_isUsingFramebufferDirectly[displayID][index]);
	}
	else
	{
		OSAtomicAnd32(0, &_isUsingFramebufferDirectly[displayID][index]);
	}
}

- (bool) isUsingFramebufferDirectlyAtIndex:(const u8)index displayID:(NDSDisplayID)displayID
{
	return (OSAtomicAnd32(1, &_isUsingFramebufferDirectly[displayID][index]) == 1);
}

- (void) setFetchBuffersWithDisplayInfo:(const NDSDisplayInfo &)dispInfo
{
	const size_t w = dispInfo.customWidth;
	const size_t h = dispInfo.customHeight;
	const size_t nativeSize = GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * dispInfo.pixelBytes;
	const size_t customSize = w * h * dispInfo.pixelBytes;
	
	_bufDisplayFetchNative[NDSDisplayID_Main][0]  = [[device newBufferWithBytesNoCopy:(u8 *)dispInfo.masterFramebufferHead
																			   length:nativeSize
																			  options:MTLResourceStorageModeManaged
																		  deallocator:nil] retain];
	
	_bufDisplayFetchNative[NDSDisplayID_Main][1]  = [[device newBufferWithBytesNoCopy:(u8 *)dispInfo.masterFramebufferHead + dispInfo.framebufferSize
																			   length:nativeSize
																			  options:MTLResourceStorageModeManaged
																		  deallocator:nil] retain];
	
	_bufDisplayFetchNative[NDSDisplayID_Touch][0] = [[device newBufferWithBytesNoCopy:(u8 *)dispInfo.masterFramebufferHead + (nativeSize * 1)
																			   length:nativeSize
																			  options:MTLResourceStorageModeManaged
																		  deallocator:nil] retain];
	
	_bufDisplayFetchNative[NDSDisplayID_Touch][1] = [[device newBufferWithBytesNoCopy:(u8 *)dispInfo.masterFramebufferHead + (nativeSize * 1) + dispInfo.framebufferSize
																			   length:nativeSize
																			  options:MTLResourceStorageModeManaged
																		  deallocator:nil] retain];
	
	_bufDisplayFetchCustom[NDSDisplayID_Main][0]  = [[device newBufferWithBytesNoCopy:(u8 *)dispInfo.masterFramebufferHead + (nativeSize * 2)
																			   length:customSize
																			  options:MTLResourceStorageModeManaged
																		  deallocator:nil] retain];
	
	_bufDisplayFetchCustom[NDSDisplayID_Main][1]  = [[device newBufferWithBytesNoCopy:(u8 *)dispInfo.masterFramebufferHead + (nativeSize * 2) + dispInfo.framebufferSize
																			   length:customSize
																			  options:MTLResourceStorageModeManaged
																		  deallocator:nil] retain];
	
	_bufDisplayFetchCustom[NDSDisplayID_Touch][0] = [[device newBufferWithBytesNoCopy:(u8 *)dispInfo.masterFramebufferHead + (nativeSize * 2) + customSize + dispInfo.framebufferSize
																			   length:customSize
																			  options:MTLResourceStorageModeManaged
																		  deallocator:nil] retain];
	
	_bufDisplayFetchCustom[NDSDisplayID_Touch][1] = [[device newBufferWithBytesNoCopy:(u8 *)dispInfo.masterFramebufferHead + (nativeSize * 2) + customSize + dispInfo.framebufferSize
																			   length:customSize
																			  options:MTLResourceStorageModeManaged
																		  deallocator:nil] retain];
	
	// If the existing texture size is different than the incoming size, then remake the textures to match the incoming size.
	if ( ([_texDisplayFetch32CustomMain width] != w) || ([_texDisplayFetch32CustomMain height] != h) )
	{
		MTLTextureDescriptor *texDisplayLoad16Desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR16Uint
																										width:w
																									   height:h
																									mipmapped:NO];
		[texDisplayLoad16Desc setResourceOptions:MTLResourceStorageModeManaged];
		[texDisplayLoad16Desc setStorageMode:MTLStorageModeManaged];
		[texDisplayLoad16Desc setCpuCacheMode:MTLCPUCacheModeWriteCombined];
		[texDisplayLoad16Desc setUsage:MTLTextureUsageShaderRead];
		
		[_texDisplayFetch16CustomMain  release];
		_texDisplayFetch16CustomMain  = [device newTextureWithDescriptor:texDisplayLoad16Desc];
		[_texDisplayFetch16CustomTouch release];
		_texDisplayFetch16CustomTouch = [device newTextureWithDescriptor:texDisplayLoad16Desc];
		
		MTLTextureDescriptor *texDisplayLoad32Desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
																										width:w
																									   height:h
																									mipmapped:NO];
		[texDisplayLoad32Desc setResourceOptions:MTLResourceStorageModePrivate];
		[texDisplayLoad32Desc setStorageMode:MTLStorageModePrivate];
		[texDisplayLoad32Desc setUsage:MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite];
		
		[_texDisplayFetch32CustomMain  release];
		_texDisplayFetch32CustomMain  = [device newTextureWithDescriptor:texDisplayLoad32Desc];
		[_texDisplayFetch32CustomTouch release];
		_texDisplayFetch32CustomTouch = [device newTextureWithDescriptor:texDisplayLoad32Desc];
		
		[_texDisplayPostprocessCustomMain  release];
		_texDisplayPostprocessCustomMain = [device newTextureWithDescriptor:texDisplayLoad32Desc];
		[_texDisplayPostprocessCustomTouch release];
		_texDisplayPostprocessCustomTouch = [device newTextureWithDescriptor:texDisplayLoad32Desc];
		
		const size_t tw = _fetchThreadsPerGroup.width;
		const size_t th = _fetchThreadsPerGroup.height;
		_fetchThreadGroupsPerGridCustom = MTLSizeMake((w + tw - 1) / tw, (h + th - 1) / th, 1);
	}
	
	id<MTLCommandBuffer> cb = [commandQueue commandBufferWithUnretainedReferences];
	[self setFetchTextureBindingsAtIndex:dispInfo.bufferIndex commandBuffer:cb];
	[cb commit];
	[cb waitUntilCompleted];
}

- (void) setFetchTextureBindingsAtIndex:(const u8)index commandBuffer:(id<MTLCommandBuffer>)cb
{
	id<MTLTexture> texDisplaySrcTarget[2] = {nil, nil};
	const NDSDisplayInfo &currentDisplayInfo = GPUFetchObject->GetFetchDisplayInfoForBufferIndex(index);
	const bool isMainEnabled  = currentDisplayInfo.isDisplayEnabled[NDSDisplayID_Main];
	const bool isTouchEnabled = currentDisplayInfo.isDisplayEnabled[NDSDisplayID_Touch];
	
	if (isMainEnabled || isTouchEnabled)
	{
		if (isMainEnabled)
		{
			if (!currentDisplayInfo.didPerformCustomRender[NDSDisplayID_Main])
			{
				texDisplaySrcTarget[NDSDisplayID_Main] = _texDisplayFetch32NativeMain;
			}
			else
			{
				texDisplaySrcTarget[NDSDisplayID_Main] = _texDisplayFetch32CustomMain;
			}
		}
		
		if (isTouchEnabled)
		{
			if (!currentDisplayInfo.didPerformCustomRender[NDSDisplayID_Touch])
			{
				texDisplaySrcTarget[NDSDisplayID_Touch] = _texDisplayFetch32NativeTouch;
			}
			else
			{
				texDisplaySrcTarget[NDSDisplayID_Touch] = _texDisplayFetch32CustomTouch;
			}
		}
		
		id<MTLComputeCommandEncoder> cce = [cb computeCommandEncoder];
		
		if (currentDisplayInfo.colorFormat == NDSColorFormat_BGR555_Rev)
		{
			// 16-bit textures aren't handled natively in Metal for macOS, so we need to explicitly convert to 32-bit here.
			
			if (currentDisplayInfo.needConvertColorFormat[NDSDisplayID_Main]    || currentDisplayInfo.needConvertColorFormat[NDSDisplayID_Touch] ||
				currentDisplayInfo.needApplyMasterBrightness[NDSDisplayID_Main] || currentDisplayInfo.needApplyMasterBrightness[NDSDisplayID_Touch])
			{
				[cce setComputePipelineState:_fetch555Pipeline];
				
				if (isMainEnabled)
				{
					memcpy([_bufMasterBrightMode[NDSDisplayID_Main] contents], currentDisplayInfo.masterBrightnessMode[NDSDisplayID_Main], sizeof(uint8_t) * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
					memcpy([_bufMasterBrightIntensity[NDSDisplayID_Main] contents], currentDisplayInfo.masterBrightnessIntensity[NDSDisplayID_Main], sizeof(uint8_t) * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
					[_bufMasterBrightMode[NDSDisplayID_Main] didModifyRange:NSMakeRange(0, sizeof(uint8_t) * GPU_FRAMEBUFFER_NATIVE_HEIGHT)];
					[_bufMasterBrightIntensity[NDSDisplayID_Main] didModifyRange:NSMakeRange(0, sizeof(uint8_t) * GPU_FRAMEBUFFER_NATIVE_HEIGHT)];
					
					[cce setBuffer:_bufMasterBrightMode[NDSDisplayID_Main] offset:0 atIndex:0];
					[cce setBuffer:_bufMasterBrightIntensity[NDSDisplayID_Main] offset:0 atIndex:1];
					
					if (!currentDisplayInfo.didPerformCustomRender[NDSDisplayID_Main])
					{
						[cce setTexture:_texDisplayFetch16NativeMain atIndex:0];
						[cce setTexture:_texDisplayPostprocessNativeMain atIndex:1];
						[cce dispatchThreadgroups:_fetchThreadGroupsPerGridNative
							threadsPerThreadgroup:_fetchThreadsPerGroup];
						
						texDisplaySrcTarget[NDSDisplayID_Main] = _texDisplayPostprocessNativeMain;
					}
					else
					{
						[cce setTexture:_texDisplayFetch16CustomMain atIndex:0];
						[cce setTexture:_texDisplayPostprocessCustomMain atIndex:1];
						[cce dispatchThreadgroups:_fetchThreadGroupsPerGridCustom
							threadsPerThreadgroup:_fetchThreadsPerGroup];
						
						texDisplaySrcTarget[NDSDisplayID_Main] = _texDisplayPostprocessCustomMain;
					}
				}
				
				if (isTouchEnabled)
				{
					memcpy([_bufMasterBrightMode[NDSDisplayID_Touch] contents], currentDisplayInfo.masterBrightnessMode[NDSDisplayID_Touch], sizeof(uint8_t) * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
					memcpy([_bufMasterBrightIntensity[NDSDisplayID_Touch] contents], currentDisplayInfo.masterBrightnessIntensity[NDSDisplayID_Touch], sizeof(uint8_t) * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
					[_bufMasterBrightMode[NDSDisplayID_Touch] didModifyRange:NSMakeRange(0, sizeof(uint8_t) * GPU_FRAMEBUFFER_NATIVE_HEIGHT)];
					[_bufMasterBrightIntensity[NDSDisplayID_Touch] didModifyRange:NSMakeRange(0, sizeof(uint8_t) * GPU_FRAMEBUFFER_NATIVE_HEIGHT)];
					
					[cce setBuffer:_bufMasterBrightMode[NDSDisplayID_Touch] offset:0 atIndex:0];
					[cce setBuffer:_bufMasterBrightIntensity[NDSDisplayID_Touch] offset:0 atIndex:1];
					
					if (!currentDisplayInfo.didPerformCustomRender[NDSDisplayID_Touch])
					{
						[cce setTexture:_texDisplayFetch16NativeTouch atIndex:0];
						[cce setTexture:_texDisplayPostprocessNativeTouch atIndex:1];
						[cce dispatchThreadgroups:_fetchThreadGroupsPerGridNative
							threadsPerThreadgroup:_fetchThreadsPerGroup];
						
						texDisplaySrcTarget[NDSDisplayID_Touch] = _texDisplayPostprocessNativeTouch;
					}
					else
					{
						[cce setTexture:_texDisplayFetch16CustomTouch atIndex:0];
						[cce setTexture:_texDisplayPostprocessCustomTouch atIndex:1];
						[cce dispatchThreadgroups:_fetchThreadGroupsPerGridCustom
							threadsPerThreadgroup:_fetchThreadsPerGroup];
						
						texDisplaySrcTarget[NDSDisplayID_Touch] = _texDisplayPostprocessCustomTouch;
					}
				}
			}
			else
			{
				[cce setComputePipelineState:_fetch555ConvertOnlyPipeline];
				
				if (isMainEnabled)
				{
					if (!currentDisplayInfo.didPerformCustomRender[NDSDisplayID_Main])
					{
						[cce setTexture:_texDisplayFetch16NativeMain atIndex:0];
						[cce setTexture:_texDisplayPostprocessNativeMain atIndex:1];
						[cce dispatchThreadgroups:_fetchThreadGroupsPerGridNative
							threadsPerThreadgroup:_fetchThreadsPerGroup];
						
						texDisplaySrcTarget[NDSDisplayID_Main] = _texDisplayPostprocessNativeMain;
					}
					else
					{
						[cce setTexture:_texDisplayFetch16CustomMain atIndex:0];
						[cce setTexture:_texDisplayPostprocessCustomMain atIndex:1];
						[cce dispatchThreadgroups:_fetchThreadGroupsPerGridCustom
							threadsPerThreadgroup:_fetchThreadsPerGroup];
						
						texDisplaySrcTarget[NDSDisplayID_Main] = _texDisplayPostprocessCustomMain;
					}
				}
				
				if (isTouchEnabled)
				{
					if (!currentDisplayInfo.didPerformCustomRender[NDSDisplayID_Touch])
					{
						[cce setTexture:_texDisplayFetch16NativeTouch atIndex:0];
						[cce setTexture:_texDisplayPostprocessNativeTouch atIndex:1];
						[cce dispatchThreadgroups:_fetchThreadGroupsPerGridNative
							threadsPerThreadgroup:_fetchThreadsPerGroup];
						
						texDisplaySrcTarget[NDSDisplayID_Touch] = _texDisplayPostprocessNativeTouch;
					}
					else
					{
						[cce setTexture:_texDisplayFetch16CustomTouch atIndex:0];
						[cce setTexture:_texDisplayPostprocessCustomTouch atIndex:1];
						[cce dispatchThreadgroups:_fetchThreadGroupsPerGridCustom
							threadsPerThreadgroup:_fetchThreadsPerGroup];
						
						texDisplaySrcTarget[NDSDisplayID_Touch] = _texDisplayPostprocessCustomTouch;
					}
				}
			}
		}
		else
		{
			if (currentDisplayInfo.needConvertColorFormat[NDSDisplayID_Main]    || currentDisplayInfo.needConvertColorFormat[NDSDisplayID_Touch] ||
				currentDisplayInfo.needApplyMasterBrightness[NDSDisplayID_Main] || currentDisplayInfo.needApplyMasterBrightness[NDSDisplayID_Touch])
			{
				if (currentDisplayInfo.colorFormat == NDSColorFormat_BGR666_Rev)
				{
					[cce setComputePipelineState:_fetch666Pipeline];
				}
				else
				{
					[cce setComputePipelineState:_fetch888Pipeline];
				}
				
				if (isMainEnabled)
				{
					memcpy([_bufMasterBrightMode[NDSDisplayID_Main] contents], currentDisplayInfo.masterBrightnessMode[NDSDisplayID_Main], sizeof(uint8_t) * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
					memcpy([_bufMasterBrightIntensity[NDSDisplayID_Main] contents], currentDisplayInfo.masterBrightnessIntensity[NDSDisplayID_Main], sizeof(uint8_t) * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
					[_bufMasterBrightMode[NDSDisplayID_Main] didModifyRange:NSMakeRange(0, sizeof(uint8_t) * GPU_FRAMEBUFFER_NATIVE_HEIGHT)];
					[_bufMasterBrightIntensity[NDSDisplayID_Main] didModifyRange:NSMakeRange(0, sizeof(uint8_t) * GPU_FRAMEBUFFER_NATIVE_HEIGHT)];
					
					[cce setBuffer:_bufMasterBrightMode[NDSDisplayID_Main] offset:0 atIndex:0];
					[cce setBuffer:_bufMasterBrightIntensity[NDSDisplayID_Main] offset:0 atIndex:1];
					
					if (texDisplaySrcTarget[NDSDisplayID_Main] == _texDisplayFetch32NativeMain)
					{
						[cce setTexture:_texDisplayFetch32NativeMain atIndex:0];
						[cce setTexture:_texDisplayPostprocessNativeMain atIndex:1];
						[cce dispatchThreadgroups:_fetchThreadGroupsPerGridNative
							threadsPerThreadgroup:_fetchThreadsPerGroup];
						
						texDisplaySrcTarget[NDSDisplayID_Main] = _texDisplayPostprocessNativeMain;
					}
					else
					{
						[cce setTexture:_texDisplayFetch32CustomMain atIndex:0];
						[cce setTexture:_texDisplayPostprocessCustomMain atIndex:1];
						[cce dispatchThreadgroups:_fetchThreadGroupsPerGridCustom
							threadsPerThreadgroup:_fetchThreadsPerGroup];
						
						texDisplaySrcTarget[NDSDisplayID_Main] = _texDisplayPostprocessCustomMain;
					}
				}
				
				if (isTouchEnabled)
				{
					memcpy([_bufMasterBrightMode[NDSDisplayID_Touch] contents], currentDisplayInfo.masterBrightnessMode[NDSDisplayID_Touch], sizeof(uint8_t) * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
					memcpy([_bufMasterBrightIntensity[NDSDisplayID_Touch] contents], currentDisplayInfo.masterBrightnessIntensity[NDSDisplayID_Touch], sizeof(uint8_t) * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
					[_bufMasterBrightMode[NDSDisplayID_Touch] didModifyRange:NSMakeRange(0, sizeof(uint8_t) * GPU_FRAMEBUFFER_NATIVE_HEIGHT)];
					[_bufMasterBrightIntensity[NDSDisplayID_Touch] didModifyRange:NSMakeRange(0, sizeof(uint8_t) * GPU_FRAMEBUFFER_NATIVE_HEIGHT)];
					
					[cce setBuffer:_bufMasterBrightMode[NDSDisplayID_Touch] offset:0 atIndex:0];
					[cce setBuffer:_bufMasterBrightIntensity[NDSDisplayID_Touch] offset:0 atIndex:1];
					
					if (texDisplaySrcTarget[NDSDisplayID_Touch] == _texDisplayFetch32NativeTouch)
					{
						[cce setTexture:_texDisplayFetch32NativeTouch atIndex:0];
						[cce setTexture:_texDisplayPostprocessNativeTouch atIndex:1];
						[cce dispatchThreadgroups:_fetchThreadGroupsPerGridNative
							threadsPerThreadgroup:_fetchThreadsPerGroup];
						
						texDisplaySrcTarget[NDSDisplayID_Touch] = _texDisplayPostprocessNativeTouch;
					}
					else
					{
						[cce setTexture:_texDisplayFetch32CustomTouch atIndex:0];
						[cce setTexture:_texDisplayPostprocessCustomTouch atIndex:1];
						[cce dispatchThreadgroups:_fetchThreadGroupsPerGridCustom
							threadsPerThreadgroup:_fetchThreadsPerGroup];
						
						texDisplaySrcTarget[NDSDisplayID_Touch] = _texDisplayPostprocessCustomTouch;
					}
				}
			}
		}
		
		[cce endEncoding];
	}
	
	[self setUsingFramebufferDirectlyAtIndex:index
								   displayID:NDSDisplayID_Main
									   state:(texDisplaySrcTarget[NDSDisplayID_Main] == _texDisplayFetch32NativeMain) || (texDisplaySrcTarget[NDSDisplayID_Main] == _texDisplayFetch32CustomMain)];
	
	[self setUsingFramebufferDirectlyAtIndex:index
								   displayID:NDSDisplayID_Touch
									   state:(texDisplaySrcTarget[NDSDisplayID_Touch] == _texDisplayFetch32NativeTouch) || (texDisplaySrcTarget[NDSDisplayID_Touch] == _texDisplayFetch32CustomTouch)];
	
	[self setTexFetchMain:texDisplaySrcTarget[NDSDisplayID_Main]];
	[self setTexFetchTouch:texDisplaySrcTarget[NDSDisplayID_Touch]];
}

- (void) fetchFromBufferIndex:(const u8)index
{
	pthread_rwlock_rdlock([self rwlockFramebufferAtIndex:index]);
	
	id<MTLCommandBuffer> cb = [commandQueue commandBufferWithUnretainedReferences];
	_fetchEncoder = [cb blitCommandEncoder];
	
	GPUFetchObject->GPUClientFetchObject::FetchFromBufferIndex(index);
	
	[_fetchEncoder endEncoding];
	
	[self setFetchTextureBindingsAtIndex:index commandBuffer:cb];
	
	[cb addCompletedHandler:^(id<MTLCommandBuffer> block) {
		pthread_rwlock_unlock([self rwlockFramebufferAtIndex:index]);
	}];
	[cb commit];
}

- (void) fetchNativeDisplayByID:(const NDSDisplayID)displayID bufferIndex:(const u8)bufferIndex
{
	const NDSDisplayInfo &currentDisplayInfo = GPUFetchObject->GetFetchDisplayInfoForBufferIndex(bufferIndex);
	
	id<MTLTexture> texFetch16 = (displayID == NDSDisplayID_Main) ? _texDisplayFetch16NativeMain : _texDisplayFetch16NativeTouch;
	id<MTLTexture> texFetch32 = (displayID == NDSDisplayID_Main) ? _texDisplayFetch32NativeMain : _texDisplayFetch32NativeTouch;
	const size_t bufferSize = GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * currentDisplayInfo.pixelBytes;
	const id<MTLBuffer> targetSource = (displayID == NDSDisplayID_Main) ? _bufDisplayFetchNative[NDSDisplayID_Main][currentDisplayInfo.bufferIndex] : _bufDisplayFetchNative[NDSDisplayID_Touch][currentDisplayInfo.bufferIndex];
	[targetSource didModifyRange:NSMakeRange(0, bufferSize)];
	
	[_fetchEncoder copyFromBuffer:targetSource
					 sourceOffset:0
				sourceBytesPerRow:GPU_FRAMEBUFFER_NATIVE_WIDTH * currentDisplayInfo.pixelBytes
			  sourceBytesPerImage:bufferSize
					   sourceSize:MTLSizeMake(GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT, 1)
						toTexture:(currentDisplayInfo.colorFormat == NDSColorFormat_BGR555_Rev) ? texFetch16 : texFetch32
				 destinationSlice:0
				 destinationLevel:0
				destinationOrigin:MTLOriginMake(0, 0, 0)];
}

- (void) fetchCustomDisplayByID:(const NDSDisplayID)displayID bufferIndex:(const u8)bufferIndex
{
	const NDSDisplayInfo &currentDisplayInfo = GPUFetchObject->GetFetchDisplayInfoForBufferIndex(bufferIndex);
	
	id<MTLTexture> texFetch16 = (displayID == NDSDisplayID_Main) ? _texDisplayFetch16CustomMain : _texDisplayFetch16CustomTouch;
	id<MTLTexture> texFetch32 = (displayID == NDSDisplayID_Main) ? _texDisplayFetch32CustomMain : _texDisplayFetch32CustomTouch;
	const size_t bufferSize = currentDisplayInfo.customWidth * currentDisplayInfo.customHeight * currentDisplayInfo.pixelBytes;
	const id<MTLBuffer> targetSource = (displayID == NDSDisplayID_Main) ? _bufDisplayFetchCustom[NDSDisplayID_Main][currentDisplayInfo.bufferIndex] : _bufDisplayFetchCustom[NDSDisplayID_Touch][currentDisplayInfo.bufferIndex];
	[targetSource didModifyRange:NSMakeRange(0, bufferSize)];
	
	[_fetchEncoder copyFromBuffer:targetSource
					 sourceOffset:0
				sourceBytesPerRow:currentDisplayInfo.customWidth * currentDisplayInfo.pixelBytes
			  sourceBytesPerImage:bufferSize
					   sourceSize:MTLSizeMake(currentDisplayInfo.customWidth, currentDisplayInfo.customHeight, 1)
						toTexture:(currentDisplayInfo.colorFormat == NDSColorFormat_BGR555_Rev) ? texFetch16 : texFetch32
				 destinationSlice:0
				 destinationLevel:0
				destinationOrigin:MTLOriginMake(0, 0, 0)];
}

@end

@implementation MacMetalDisplayPresenterObject

@synthesize cdp;
@synthesize sharedData;
@synthesize colorAttachment0Desc;
@dynamic mutexBufferUpdate;
@synthesize pixelScalePipeline;
@synthesize outputRGBAPipeline;
@synthesize outputDrawablePipeline;
@synthesize drawableFormat;
@synthesize bufCPUFilterDstMain;
@synthesize bufCPUFilterDstTouch;
@synthesize texDisplayPixelScaleMain;
@synthesize texDisplayPixelScaleTouch;
@synthesize texHUDCharMap;
@synthesize needsViewportUpdate;
@synthesize needsRotationScaleUpdate;
@synthesize needsScreenVerticesUpdate;
@synthesize needsHUDVerticesUpdate;
@dynamic pixelScaler;
@dynamic outputFilter;
@synthesize processedFrameInfo;

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
	
	_cdvPropertiesBuffer = nil;
	_displayVtxPositionBuffer = nil;
	_displayTexCoordBuffer = nil;
	_hudVtxPositionBuffer = nil;
	_hudVtxColorBuffer = nil;
	_hudTexCoordBuffer = nil;
	
	_texDisplaySrcDeposterize[NDSDisplayID_Main][0]  = nil;
	_texDisplaySrcDeposterize[NDSDisplayID_Touch][0] = nil;
	_texDisplaySrcDeposterize[NDSDisplayID_Main][1]  = nil;
	_texDisplaySrcDeposterize[NDSDisplayID_Touch][1] = nil;
	
	_bufCPUFilterSrcMain  = nil;
	_bufCPUFilterSrcTouch = nil;
	bufCPUFilterDstMain  = nil;
	bufCPUFilterDstTouch = nil;
	texDisplayPixelScaleMain  = nil;
	texDisplayPixelScaleTouch = nil;
	texHUDCharMap = nil;
	
	_pixelScalerThreadsPerGroup = MTLSizeMake(1, 1, 1);
	_pixelScalerThreadGroupsPerGrid = MTLSizeMake(1, 1, 1);
	
	needsViewportUpdate = YES;
	needsRotationScaleUpdate = YES;
	needsScreenVerticesUpdate = YES;
	needsHUDVerticesUpdate = YES;
	
	processedFrameInfo.bufferIndex = 0;
	processedFrameInfo.tex[NDSDisplayID_Main]  = nil;
	processedFrameInfo.tex[NDSDisplayID_Touch] = nil;
	processedFrameInfo.isMainDisplayProcessed = false;
	processedFrameInfo.isTouchDisplayProcessed = false;
	
	pthread_mutex_init(&_mutexBufferUpdate, NULL);
	
	return self;
}

- (void)dealloc
{
	[[self colorAttachment0Desc] setTexture:nil];
	[_outputRenderPassDesc release];
	
	[_cdvPropertiesBuffer release];
	[_displayVtxPositionBuffer release];
	[_displayTexCoordBuffer release];
	[_hudVtxPositionBuffer release];
	[_hudVtxColorBuffer release];
	[_hudTexCoordBuffer release];
	
	[_texDisplaySrcDeposterize[NDSDisplayID_Main][0]  release];
	[_texDisplaySrcDeposterize[NDSDisplayID_Touch][0] release];
	[_texDisplaySrcDeposterize[NDSDisplayID_Main][1]  release];
	[_texDisplaySrcDeposterize[NDSDisplayID_Touch][1] release];
	
	[_bufCPUFilterSrcMain  release];
	[_bufCPUFilterSrcTouch release];
	[self setBufCPUFilterDstMain:nil];
	[self setBufCPUFilterDstTouch:nil];
	[self setTexDisplayPixelScaleMain:nil];
	[self setTexDisplayPixelScaleTouch:nil];
	
	[self setPixelScalePipeline:nil];
	[self setOutputRGBAPipeline:nil];
	[self setOutputDrawablePipeline:nil];
	[self setTexHUDCharMap:nil];
	
	[self setSharedData:nil];
	
	pthread_mutex_destroy(&_mutexBufferUpdate);
	
	[super dealloc];
}

- (pthread_mutex_t *) mutexBufferUpdate
{
	return &_mutexBufferUpdate;
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
	
	[self setTexDisplayPixelScaleMain:[[sharedData device] newTextureWithDescriptor:texDisplayPixelScaleDesc]];
	[self setTexDisplayPixelScaleTouch:[[sharedData device] newTextureWithDescriptor:texDisplayPixelScaleDesc]];
	
	if ([self pixelScalePipeline] != nil)
	{
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
	
	_cdvPropertiesBuffer = [[[sharedData device] newBufferWithLength:sizeof(DisplayViewShaderProperties) options:MTLResourceStorageModeManaged] retain];
	_displayVtxPositionBuffer = [[[sharedData device] newBufferWithLength:(sizeof(float) * (4 * 8)) options:MTLResourceStorageModeManaged] retain];
	_displayTexCoordBuffer = [[[sharedData device] newBufferWithLength:(sizeof(float) * (4 * 8)) options:MTLResourceStorageModeManaged] retain];
	_hudVtxPositionBuffer = [[[sharedData device] newBufferWithLength:HUD_VERTEX_ATTRIBUTE_BUFFER_SIZE options:MTLResourceStorageModeManaged] retain];
	_hudVtxColorBuffer = [[[sharedData device] newBufferWithLength:HUD_VERTEX_COLOR_ATTRIBUTE_BUFFER_SIZE options:MTLResourceStorageModeManaged] retain];
	_hudTexCoordBuffer = [[[sharedData device] newBufferWithLength:HUD_VERTEX_ATTRIBUTE_BUFFER_SIZE options:MTLResourceStorageModeManaged] retain];
	
	DisplayViewShaderProperties *viewProps = (DisplayViewShaderProperties *)[_cdvPropertiesBuffer contents];
	viewProps->width               = cdp->GetPresenterProperties().clientWidth;
	viewProps->height              = cdp->GetPresenterProperties().clientHeight;
	viewProps->rotation            = cdp->GetPresenterProperties().rotation;
	viewProps->viewScale           = cdp->GetPresenterProperties().viewScale;
	viewProps->lowerHUDMipMapLevel = 0;
	[_cdvPropertiesBuffer didModifyRange:NSMakeRange(0, sizeof(DisplayViewShaderProperties))];
	
	// Set up processing textures.
	MTLTextureDescriptor *texDisplaySrcDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
																								 width:GPU_FRAMEBUFFER_NATIVE_WIDTH
																								height:GPU_FRAMEBUFFER_NATIVE_HEIGHT
																							 mipmapped:NO];
	[texDisplaySrcDesc setResourceOptions:MTLResourceStorageModePrivate];
	[texDisplaySrcDesc setStorageMode:MTLStorageModePrivate];
	[texDisplaySrcDesc setUsage:MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite];
	
	_texDisplaySrcDeposterize[NDSDisplayID_Main][0]  = [[[sharedData device] newTextureWithDescriptor:texDisplaySrcDesc] retain];
	_texDisplaySrcDeposterize[NDSDisplayID_Touch][0] = [[[sharedData device] newTextureWithDescriptor:texDisplaySrcDesc] retain];
	_texDisplaySrcDeposterize[NDSDisplayID_Main][1]  = [[[sharedData device] newTextureWithDescriptor:texDisplaySrcDesc] retain];
	_texDisplaySrcDeposterize[NDSDisplayID_Touch][1] = [[[sharedData device] newTextureWithDescriptor:texDisplaySrcDesc] retain];
	
	MTLTextureDescriptor *texDisplayPixelScaleDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
																										width:GPU_FRAMEBUFFER_NATIVE_WIDTH*2
																									   height:GPU_FRAMEBUFFER_NATIVE_HEIGHT*2
																									mipmapped:NO];
	[texDisplayPixelScaleDesc setResourceOptions:MTLResourceStorageModePrivate];
	[texDisplayPixelScaleDesc setStorageMode:MTLStorageModePrivate];
	[texDisplayPixelScaleDesc setUsage:MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite];
	
	[self setTexDisplayPixelScaleMain:[[sharedData device] newTextureWithDescriptor:texDisplayPixelScaleDesc]];
	[self setTexDisplayPixelScaleTouch:[[sharedData device] newTextureWithDescriptor:texDisplayPixelScaleDesc]];
	
	processedFrameInfo.tex[NDSDisplayID_Main]  = [sharedData texFetchMain];
	processedFrameInfo.tex[NDSDisplayID_Touch] = [sharedData texFetchTouch];
	
	VideoFilter *vfMain  = cdp->GetPixelScalerObject(NDSDisplayID_Main);
	_bufCPUFilterSrcMain = [[[sharedData device] newBufferWithBytesNoCopy:vfMain->GetSrcBufferPtr()
																		length:vfMain->GetSrcWidth() * vfMain->GetSrcHeight() * sizeof(uint32_t)
																 options:MTLResourceStorageModeManaged
															 deallocator:nil] retain];
	
	[self setBufCPUFilterDstMain:[[sharedData device] newBufferWithBytesNoCopy:vfMain->GetDstBufferPtr()
																		length:vfMain->GetDstWidth() * vfMain->GetDstHeight() * sizeof(uint32_t)
																	   options:MTLResourceStorageModeManaged
																   deallocator:nil]];
	
	VideoFilter *vfTouch = cdp->GetPixelScalerObject(NDSDisplayID_Touch);
	_bufCPUFilterSrcTouch = [[[sharedData device] newBufferWithBytesNoCopy:vfTouch->GetSrcBufferPtr()
																		 length:vfTouch->GetSrcWidth() * vfTouch->GetSrcHeight() * sizeof(uint32_t)
																		options:MTLResourceStorageModeManaged
																	deallocator:nil] retain];
	
	[self setBufCPUFilterDstTouch:[[sharedData device] newBufferWithBytesNoCopy:vfTouch->GetDstBufferPtr()
																		 length:vfTouch->GetDstWidth() * vfTouch->GetDstHeight() * sizeof(uint32_t)
																		options:MTLResourceStorageModeManaged
																	deallocator:nil]];
	
	texHUDCharMap = nil;
}

- (void) resizeCPUPixelScalerUsingFilterID:(const VideoFilterTypeID)filterID
{
	const VideoFilterAttributes vfAttr = VideoFilter::GetAttributesByID(filterID);
	
	id<MTLCommandBuffer> cb = [self newCommandBuffer];
	id<MTLBlitCommandEncoder> dummyEncoder = [cb blitCommandEncoder];
	[dummyEncoder endEncoding];
	[cb commit];
	[cb waitUntilCompleted];
	
	VideoFilter *vfMain  = cdp->GetPixelScalerObject(NDSDisplayID_Main);
	[self setBufCPUFilterDstMain:[[sharedData device] newBufferWithBytesNoCopy:vfMain->GetDstBufferPtr()
																		length:(vfMain->GetSrcWidth()  * vfAttr.scaleMultiply / vfAttr.scaleDivide) * (vfMain->GetSrcHeight()  * vfAttr.scaleMultiply / vfAttr.scaleDivide) * sizeof(uint32_t)
																	   options:MTLResourceStorageModeManaged
																   deallocator:nil]];
	
	VideoFilter *vfTouch = cdp->GetPixelScalerObject(NDSDisplayID_Touch);
	[self setBufCPUFilterDstTouch:[[sharedData device] newBufferWithBytesNoCopy:vfTouch->GetDstBufferPtr()
																		 length:(vfTouch->GetSrcWidth() * vfAttr.scaleMultiply / vfAttr.scaleDivide) * (vfTouch->GetSrcHeight() * vfAttr.scaleMultiply / vfAttr.scaleDivide) * sizeof(uint32_t)
																		options:MTLResourceStorageModeManaged
																	deallocator:nil]];
	
	cb = [self newCommandBuffer];
	dummyEncoder = [cb blitCommandEncoder];
	[dummyEncoder endEncoding];
	[cb commit];
	[cb waitUntilCompleted];
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
	
	[self setTexHUDCharMap:[[sharedData device] newTextureWithDescriptor:texHUDCharMapDesc]];
	
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
	const u8 bufferIndex = [sharedData GPUFetchObject]->GetLastFetchIndex();
	const NDSDisplayInfo &fetchDisplayInfo = [sharedData GPUFetchObject]->GetFetchDisplayInfoForBufferIndex(bufferIndex);
	const ClientDisplayMode mode = cdp->GetPresenterProperties().mode;
	const bool useDeposterize = cdp->GetSourceDeposterize();
	
	const NDSDisplayID selectedDisplaySource[2] = { cdp->GetSelectedDisplaySourceForDisplay(NDSDisplayID_Main), cdp->GetSelectedDisplaySourceForDisplay(NDSDisplayID_Touch) };
	id<MTLTexture> texMain  = (selectedDisplaySource[NDSDisplayID_Main]  == NDSDisplayID_Main)  ? [sharedData texFetchMain]  : [sharedData texFetchTouch];
	id<MTLTexture> texTouch = (selectedDisplaySource[NDSDisplayID_Touch] == NDSDisplayID_Touch) ? [sharedData texFetchTouch] : [sharedData texFetchMain];
	
	bool isDisplayProcessedMain  = ![sharedData isUsingFramebufferDirectlyAtIndex:bufferIndex displayID:selectedDisplaySource[NDSDisplayID_Main]];
	bool isDisplayProcessedTouch = ![sharedData isUsingFramebufferDirectlyAtIndex:bufferIndex displayID:selectedDisplaySource[NDSDisplayID_Touch]];
	
	if ( (fetchDisplayInfo.pixelBytes != 0) && (useDeposterize || (cdp->GetPixelScaler() != VideoFilterTypeID_None)) )
	{
		const bool willFilterOnGPU = cdp->WillFilterOnGPU();
		const bool shouldProcessDisplay[2] = { (!fetchDisplayInfo.didPerformCustomRender[selectedDisplaySource[NDSDisplayID_Main]]  || !fetchDisplayInfo.isCustomSizeRequested) && cdp->IsSelectedDisplayEnabled(NDSDisplayID_Main)  && (mode == ClientDisplayMode_Main  || mode == ClientDisplayMode_Dual),
		                                       (!fetchDisplayInfo.didPerformCustomRender[selectedDisplaySource[NDSDisplayID_Touch]] || !fetchDisplayInfo.isCustomSizeRequested) && cdp->IsSelectedDisplayEnabled(NDSDisplayID_Touch) && (mode == ClientDisplayMode_Touch || mode == ClientDisplayMode_Dual) && (selectedDisplaySource[NDSDisplayID_Main] != selectedDisplaySource[NDSDisplayID_Touch]) };
		
		bool texFetchMainNeedsLock  = (useDeposterize || ((cdp->GetPixelScaler() != VideoFilterTypeID_None) && willFilterOnGPU)) && shouldProcessDisplay[NDSDisplayID_Main];
		bool texFetchTouchNeedsLock = (useDeposterize || ((cdp->GetPixelScaler() != VideoFilterTypeID_None) && willFilterOnGPU)) && shouldProcessDisplay[NDSDisplayID_Touch];
		bool needsFetchBuffersLock = texFetchMainNeedsLock || texFetchTouchNeedsLock;
		
		id<MTLCommandBuffer> cb = [self newCommandBuffer];
		id<MTLComputeCommandEncoder> cce = [cb computeCommandEncoder];
		
		// Run the video source filters and the pixel scalers
		if (useDeposterize)
		{
			[cce setComputePipelineState:[sharedData deposterizePipeline]];
			
			if (needsFetchBuffersLock)
			{
				pthread_rwlock_rdlock([sharedData rwlockFramebufferAtIndex:bufferIndex]);
			}
			
			if (shouldProcessDisplay[NDSDisplayID_Main])
			{
				[cce setTexture:texMain atIndex:0];
				[cce setTexture:_texDisplaySrcDeposterize[NDSDisplayID_Main][0] atIndex:1];
				[cce dispatchThreadgroups:[sharedData deposterizeThreadGroupsPerGrid]
					threadsPerThreadgroup:[sharedData deposterizeThreadsPerGroup]];
				
				[cce setTexture:_texDisplaySrcDeposterize[NDSDisplayID_Main][0] atIndex:0];
				[cce setTexture:_texDisplaySrcDeposterize[NDSDisplayID_Main][1] atIndex:1];
				[cce dispatchThreadgroups:[sharedData deposterizeThreadGroupsPerGrid]
					threadsPerThreadgroup:[sharedData deposterizeThreadsPerGroup]];
				
				texMain = _texDisplaySrcDeposterize[NDSDisplayID_Main][1];
				isDisplayProcessedMain = true;
				
				if (selectedDisplaySource[NDSDisplayID_Main] == selectedDisplaySource[NDSDisplayID_Touch])
				{
					isDisplayProcessedTouch = true;
				}
			}
			
			if (shouldProcessDisplay[NDSDisplayID_Touch])
			{
				[cce setTexture:texTouch atIndex:0];
				[cce setTexture:_texDisplaySrcDeposterize[NDSDisplayID_Touch][0] atIndex:1];
				[cce dispatchThreadgroups:[sharedData deposterizeThreadGroupsPerGrid]
					threadsPerThreadgroup:[sharedData deposterizeThreadsPerGroup]];
				
				[cce setTexture:_texDisplaySrcDeposterize[NDSDisplayID_Touch][0] atIndex:0];
				[cce setTexture:_texDisplaySrcDeposterize[NDSDisplayID_Touch][1] atIndex:1];
				[cce dispatchThreadgroups:[sharedData deposterizeThreadGroupsPerGrid]
					threadsPerThreadgroup:[sharedData deposterizeThreadsPerGroup]];
				
				texTouch = _texDisplaySrcDeposterize[NDSDisplayID_Touch][1];
				isDisplayProcessedTouch = true;
			}
			
			if (needsFetchBuffersLock)
			{
				[cce endEncoding];
				[cb addCompletedHandler:^(id<MTLCommandBuffer> block) {
					pthread_rwlock_unlock([sharedData rwlockFramebufferAtIndex:bufferIndex]);
				}];
				[cb commit];
				
				cb = [self newCommandBuffer];
				cce = [cb computeCommandEncoder];
				
				needsFetchBuffersLock = !isDisplayProcessedMain || !isDisplayProcessedTouch;
			}
		}
		
		// Run the pixel scalers. First attempt on the GPU.
		if ( (cdp->GetPixelScaler() != VideoFilterTypeID_None) && willFilterOnGPU )
		{
			[cce setComputePipelineState:[self pixelScalePipeline]];
			
			if (needsFetchBuffersLock)
			{
				pthread_rwlock_rdlock([sharedData rwlockFramebufferAtIndex:bufferIndex]);
			}
			
			if (shouldProcessDisplay[NDSDisplayID_Main])
			{
				[cce setTexture:texMain atIndex:0];
				[cce setTexture:[self texDisplayPixelScaleMain] atIndex:1];
				[cce setTexture:[sharedData texCurrentHQnxLUT] atIndex:2];
				[cce dispatchThreadgroups:_pixelScalerThreadGroupsPerGrid threadsPerThreadgroup:_pixelScalerThreadsPerGroup];
				
				texMain  = [self texDisplayPixelScaleMain];
				isDisplayProcessedMain = true;
				
				if (selectedDisplaySource[NDSDisplayID_Main] == selectedDisplaySource[NDSDisplayID_Touch])
				{
					isDisplayProcessedTouch = true;
				}
			}
			
			if (shouldProcessDisplay[NDSDisplayID_Touch])
			{
				[cce setTexture:texTouch atIndex:0];
				[cce setTexture:[self texDisplayPixelScaleTouch] atIndex:1];
				[cce setTexture:[sharedData texCurrentHQnxLUT] atIndex:2];
				[cce dispatchThreadgroups:_pixelScalerThreadGroupsPerGrid threadsPerThreadgroup:_pixelScalerThreadsPerGroup];
				
				texTouch = [self texDisplayPixelScaleTouch];
				isDisplayProcessedTouch = true;
			}
			
			if (needsFetchBuffersLock)
			{
				[cce endEncoding];
				[cb addCompletedHandler:^(id<MTLCommandBuffer> block) {
					pthread_rwlock_unlock([sharedData rwlockFramebufferAtIndex:bufferIndex]);
				}];
				[cb commit];
				
				cb = [self newCommandBuffer];
				cce = [cb computeCommandEncoder];
			}
		}
		
		[cce endEncoding];
		
		// If the pixel scaler didn't already run on the GPU, then run the pixel scaler on the CPU.
		if ( (cdp->GetPixelScaler() != VideoFilterTypeID_None) && !willFilterOnGPU )
		{
			VideoFilter *vfMain  = cdp->GetPixelScalerObject(NDSDisplayID_Main);
			VideoFilter *vfTouch = cdp->GetPixelScalerObject(NDSDisplayID_Touch);
			
			id<MTLBlitCommandEncoder> bce = [cb blitCommandEncoder];
			
			if (useDeposterize)
			{
				// Hybrid CPU/GPU-based path (may cause a performance hit on pixel download)
				if (shouldProcessDisplay[NDSDisplayID_Main])
				{
					[bce copyFromTexture:_texDisplaySrcDeposterize[NDSDisplayID_Main][1]
							 sourceSlice:0
							 sourceLevel:0
							sourceOrigin:MTLOriginMake(0, 0, 0)
							  sourceSize:MTLSizeMake(vfMain->GetSrcWidth(), vfMain->GetSrcHeight(), 1)
								toBuffer:_bufCPUFilterSrcMain
					   destinationOffset:0
				  destinationBytesPerRow:vfMain->GetSrcWidth() * sizeof(uint32_t)
				destinationBytesPerImage:vfMain->GetSrcWidth() * vfMain->GetSrcHeight() * sizeof(uint32_t)];
					
					[bce synchronizeResource:_bufCPUFilterSrcMain];
				}
				
				if (shouldProcessDisplay[NDSDisplayID_Touch])
				{
					[bce copyFromTexture:_texDisplaySrcDeposterize[NDSDisplayID_Touch][1]
							 sourceSlice:0
							 sourceLevel:0
							sourceOrigin:MTLOriginMake(0, 0, 0)
							  sourceSize:MTLSizeMake(vfTouch->GetSrcWidth(), vfTouch->GetSrcHeight(), 1)
								toBuffer:_bufCPUFilterSrcTouch
					   destinationOffset:0
				  destinationBytesPerRow:vfTouch->GetSrcWidth() * sizeof(uint32_t)
				destinationBytesPerImage:vfTouch->GetSrcWidth() * vfTouch->GetSrcHeight() * sizeof(uint32_t)];
					
					[bce synchronizeResource:_bufCPUFilterSrcTouch];
				}
			}
			
			pthread_mutex_lock(((MacMetalDisplayPresenter *)cdp)->GetMutexProcessPtr());
			
			if (shouldProcessDisplay[NDSDisplayID_Main])
			{
				pthread_rwlock_rdlock(((MacMetalDisplayPresenter *)cdp)->GetCPUFilterRWLock(NDSDisplayID_Main, bufferIndex));
				vfMain->RunFilter();
				pthread_rwlock_unlock(((MacMetalDisplayPresenter *)cdp)->GetCPUFilterRWLock(NDSDisplayID_Main, bufferIndex));
			}
			
			if (shouldProcessDisplay[NDSDisplayID_Touch])
			{
				pthread_rwlock_rdlock(((MacMetalDisplayPresenter *)cdp)->GetCPUFilterRWLock(NDSDisplayID_Touch, bufferIndex));
				vfTouch->RunFilter();
				pthread_rwlock_unlock(((MacMetalDisplayPresenter *)cdp)->GetCPUFilterRWLock(NDSDisplayID_Touch, bufferIndex));
			}
			
			if (shouldProcessDisplay[NDSDisplayID_Main])
			{
				[[self bufCPUFilterDstMain] didModifyRange:NSMakeRange(0, vfMain->GetDstWidth() * vfMain->GetDstHeight() * sizeof(uint32_t))];
				
				[bce copyFromBuffer:[self bufCPUFilterDstMain]
					   sourceOffset:0
				  sourceBytesPerRow:vfMain->GetDstWidth() * sizeof(uint32_t)
				sourceBytesPerImage:vfMain->GetDstWidth() * vfMain->GetDstHeight() * sizeof(uint32_t)
						 sourceSize:MTLSizeMake(vfMain->GetDstWidth(), vfMain->GetDstHeight(), 1)
						  toTexture:[self texDisplayPixelScaleMain]
				   destinationSlice:0
				   destinationLevel:0
				  destinationOrigin:MTLOriginMake(0, 0, 0)];
				
				texMain = [self texDisplayPixelScaleMain];
				isDisplayProcessedMain = true;
				
				if (selectedDisplaySource[NDSDisplayID_Main] == selectedDisplaySource[NDSDisplayID_Touch])
				{
					isDisplayProcessedTouch = true;
				}
			}
			
			if (shouldProcessDisplay[NDSDisplayID_Touch])
			{
				[[self bufCPUFilterDstTouch] didModifyRange:NSMakeRange(0, vfTouch->GetDstWidth() * vfTouch->GetDstHeight() * sizeof(uint32_t))];
				
				[bce copyFromBuffer:[self bufCPUFilterDstTouch]
					   sourceOffset:0
				  sourceBytesPerRow:vfTouch->GetDstWidth() * sizeof(uint32_t)
				sourceBytesPerImage:vfTouch->GetDstWidth() * vfTouch->GetDstHeight() * sizeof(uint32_t)
						 sourceSize:MTLSizeMake(vfTouch->GetDstWidth(), vfTouch->GetDstHeight(), 1)
						  toTexture:[self texDisplayPixelScaleTouch]
				   destinationSlice:0
				   destinationLevel:0
				  destinationOrigin:MTLOriginMake(0, 0, 0)];
				
				texTouch = [self texDisplayPixelScaleTouch];
				isDisplayProcessedTouch = true;
			}
			
			pthread_mutex_unlock(((MacMetalDisplayPresenter *)cdp)->GetMutexProcessPtr());
			
			[bce endEncoding];
		}
		
		[cb commit];
	}
	
	if (selectedDisplaySource[NDSDisplayID_Touch] == selectedDisplaySource[NDSDisplayID_Main])
	{
		texTouch = texMain;
	}
	
	// Update the texture coordinates
	cdp->SetScreenTextureCoordinates((float)[texMain  width], (float)[texMain  height],
									 (float)[texTouch width], (float)[texTouch height],
									 (float *)[_displayTexCoordBuffer contents]);
	[_displayTexCoordBuffer didModifyRange:NSMakeRange(0, sizeof(float) * (4 * 8))];
	
	// Update the frame info
	MetalProcessedFrameInfo newFrameInfo;
	newFrameInfo.bufferIndex = bufferIndex;
	newFrameInfo.isMainDisplayProcessed  = isDisplayProcessedMain;
	newFrameInfo.isTouchDisplayProcessed = isDisplayProcessedTouch;
	newFrameInfo.tex[NDSDisplayID_Main]  = texMain;
	newFrameInfo.tex[NDSDisplayID_Touch] = texTouch;
	
	[self setProcessedFrameInfo:newFrameInfo];
}

- (void) updateRenderBuffers
{
	// Set up the view properties.
	bool didChangeViewProperties = false;
	bool needEncodeViewport = false;
	
	MTLViewport newViewport;
	newViewport.originX = 0.0;
	newViewport.originY = 0.0;
	newViewport.width  = cdp->GetPresenterProperties().clientWidth;
	newViewport.height = cdp->GetPresenterProperties().clientHeight;
	newViewport.znear = 0.0;
	newViewport.zfar = 1.0;
	
	pthread_mutex_lock(&_mutexBufferUpdate);
	
	if ([self needsViewportUpdate])
	{
		needEncodeViewport = true;
		
		DisplayViewShaderProperties *viewProps = (DisplayViewShaderProperties *)[_cdvPropertiesBuffer contents];
		viewProps->width    = cdp->GetPresenterProperties().clientWidth;
		viewProps->height   = cdp->GetPresenterProperties().clientHeight;
		didChangeViewProperties = true;
		
		[self setNeedsViewportUpdate:NO];
	}
	
	if ([self needsRotationScaleUpdate])
	{
		DisplayViewShaderProperties *viewProps = (DisplayViewShaderProperties *)[_cdvPropertiesBuffer contents];
		viewProps->rotation            = cdp->GetPresenterProperties().rotation;
		viewProps->viewScale           = cdp->GetPresenterProperties().viewScale;
		viewProps->lowerHUDMipMapLevel = ( ((float)HUD_TEXTBOX_BASE_SCALE * cdp->GetHUDObjectScale() / cdp->GetScaleFactor()) >= (2.0/3.0) ) ? 0 : 1;
		didChangeViewProperties = true;
		
		[self setNeedsRotationScaleUpdate:NO];
	}
	
	if (didChangeViewProperties)
	{
		[_cdvPropertiesBuffer didModifyRange:NSMakeRange(0, sizeof(DisplayViewShaderProperties))];
	}
	
	// Set up the display properties.
	if ([self needsScreenVerticesUpdate])
	{
		cdp->SetScreenVertices((float *)[_displayVtxPositionBuffer contents]);
		[_displayVtxPositionBuffer didModifyRange:NSMakeRange(0, sizeof(float) * (4 * 8))];
		
		[self setNeedsScreenVerticesUpdate:NO];
	}
	
	// Set up the HUD properties.
	size_t hudLength = cdp->GetHUDString().length();
	size_t hudTouchLineLength = 0;
	bool willDrawHUD = cdp->GetHUDVisibility() && ([self texHUDCharMap] != nil);
	
	if (cdp->GetHUDShowInput())
	{
		hudLength += HUD_INPUT_ELEMENT_LENGTH;
		
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
		
		hudLength += hudTouchLineLength;
	}
	
	willDrawHUD = willDrawHUD && (hudLength > 1);
	
	if (willDrawHUD && cdp->HUDNeedsUpdate())
	{
		cdp->SetHUDPositionVertices((float)cdp->GetPresenterProperties().clientWidth, (float)cdp->GetPresenterProperties().clientHeight, (float *)[_hudVtxPositionBuffer contents]);
		[_hudVtxPositionBuffer didModifyRange:NSMakeRange(0, sizeof(float) * hudLength * 8)];
		
		cdp->SetHUDColorVertices((uint32_t *)[_hudVtxColorBuffer contents]);
		[_hudVtxColorBuffer didModifyRange:NSMakeRange(0, sizeof(uint32_t) * hudLength * 4)];
		
		cdp->SetHUDTextureCoordinates((float *)[_hudTexCoordBuffer contents]);
		[_hudTexCoordBuffer didModifyRange:NSMakeRange(0, sizeof(float) * hudLength * 8)];
		
		cdp->ClearHUDNeedsUpdate();
	}
	
	_needEncodeViewport = needEncodeViewport;
	_newViewport = newViewport;
	_willDrawHUD = willDrawHUD;
	_willDrawHUDInput = cdp->GetHUDShowInput();
	_hudStringLength = cdp->GetHUDString().length();
	_hudTouchLineLength = hudTouchLineLength;
	
	pthread_mutex_unlock(&_mutexBufferUpdate);
}

- (void) renderForCommandBuffer:(id<MTLCommandBuffer>)cb
			outputPipelineState:(id<MTLRenderPipelineState>)outputPipelineState
			   hudPipelineState:(id<MTLRenderPipelineState>)hudPipelineState
				 texDisplayMain:(id<MTLTexture>)texDisplayMain
				texDisplayTouch:(id<MTLTexture>)texDisplayTouch
{
	id<MTLRenderCommandEncoder> rce = [cb renderCommandEncoderWithDescriptor:_outputRenderPassDesc];
	
	if (_needEncodeViewport)
	{
		[rce setViewport:_newViewport];
	}
	
	// Draw the NDS displays.
	const NDSDisplayInfo &displayInfo = cdp->GetEmuDisplayInfo();
	const float backlightIntensity[2] = { displayInfo.backlightIntensity[NDSDisplayID_Main], displayInfo.backlightIntensity[NDSDisplayID_Touch] };
	
	[rce setRenderPipelineState:outputPipelineState];
	[rce setVertexBuffer:_displayVtxPositionBuffer offset:0 atIndex:0];
	[rce setVertexBuffer:_displayTexCoordBuffer offset:0 atIndex:1];
	[rce setVertexBuffer:_cdvPropertiesBuffer offset:0 atIndex:2];
	
	switch (cdp->GetPresenterProperties().mode)
	{
		case ClientDisplayMode_Main:
		{
			if (cdp->IsSelectedDisplayEnabled(NDSDisplayID_Main))
			{
				[rce setFragmentBytes:&backlightIntensity[NDSDisplayID_Main] length:sizeof(backlightIntensity[NDSDisplayID_Main]) atIndex:0];
				[rce setFragmentTexture:texDisplayMain atIndex:0];
				[rce drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
			}
			break;
		}
			
		case ClientDisplayMode_Touch:
		{
			if (cdp->IsSelectedDisplayEnabled(NDSDisplayID_Touch))
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
					if (cdp->IsSelectedDisplayEnabled(majorDisplayID))
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
			
			if (cdp->IsSelectedDisplayEnabled(NDSDisplayID_Main))
			{
				[rce setFragmentBytes:&backlightIntensity[NDSDisplayID_Main] length:sizeof(backlightIntensity[NDSDisplayID_Main]) atIndex:0];
				[rce setFragmentTexture:texDisplayMain atIndex:0];
				[rce drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
			}
			
			if (cdp->IsSelectedDisplayEnabled(NDSDisplayID_Touch))
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
	if (_willDrawHUD)
	{
		uint8_t isScreenOverlay = 0;
		
		[rce setRenderPipelineState:hudPipelineState];
		[rce setVertexBuffer:_hudVtxPositionBuffer offset:0 atIndex:0];
		[rce setVertexBuffer:_hudVtxColorBuffer offset:0 atIndex:1];
		[rce setVertexBuffer:_hudTexCoordBuffer offset:0 atIndex:2];
		[rce setVertexBuffer:_cdvPropertiesBuffer offset:0 atIndex:3];
		[rce setFragmentTexture:[self texHUDCharMap] atIndex:0];
		
		// First, draw the inputs.
		if (_willDrawHUDInput)
		{
			isScreenOverlay = 1;
			[rce setVertexBytes:&isScreenOverlay length:sizeof(uint8_t) atIndex:4];
			[rce setFragmentSamplerState:[sharedData samplerHUDBox] atIndex:0];
			[rce drawIndexedPrimitives:MTLPrimitiveTypeTriangle
							indexCount:_hudTouchLineLength * 6
							 indexType:MTLIndexTypeUInt16
						   indexBuffer:[sharedData hudIndexBuffer]
					 indexBufferOffset:(_hudStringLength + HUD_INPUT_ELEMENT_LENGTH) * 6 * sizeof(uint16_t)];
			
			isScreenOverlay = 0;
			[rce setVertexBytes:&isScreenOverlay length:sizeof(uint8_t) atIndex:4];
			[rce setFragmentSamplerState:[sharedData samplerHUDText] atIndex:0];
			[rce drawIndexedPrimitives:MTLPrimitiveTypeTriangle
							indexCount:HUD_INPUT_ELEMENT_LENGTH * 6
							 indexType:MTLIndexTypeUInt16
						   indexBuffer:[sharedData hudIndexBuffer]
					 indexBufferOffset:_hudStringLength * 6 * sizeof(uint16_t)];
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
						indexCount:(_hudStringLength - 1) * 6
						 indexType:MTLIndexTypeUInt16
					   indexBuffer:[sharedData hudIndexBuffer]
				 indexBufferOffset:6 * sizeof(uint16_t)];
	}
	
	[rce endEncoding];
}

- (void) renderToBuffer:(uint32_t *)dstBuffer
{
	const size_t clientWidth  = cdp->GetPresenterProperties().clientWidth;
	const size_t clientHeight = cdp->GetPresenterProperties().clientHeight;
	
	@autoreleasepool
	{
		MTLTextureDescriptor *texRenderDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
																								 width:clientWidth
																								height:clientHeight
																							 mipmapped:NO];
		[texRenderDesc setResourceOptions:MTLResourceStorageModePrivate];
		[texRenderDesc setStorageMode:MTLStorageModePrivate];
		[texRenderDesc setUsage:MTLTextureUsageRenderTarget];
		
		id<MTLTexture> texRender = [[[sharedData device] newTextureWithDescriptor:texRenderDesc] retain];
		id<MTLBuffer> dstMTLBuffer = [[[sharedData device] newBufferWithLength:clientWidth * clientHeight * sizeof(uint32_t) options:MTLResourceStorageModeManaged] retain];
		
		const MetalProcessedFrameInfo processedInfo = [self processedFrameInfo];
		const bool needsFetchBuffersLock = !processedInfo.isMainDisplayProcessed || !processedInfo.isTouchDisplayProcessed;
		
		pthread_mutex_lock(&_mutexBufferUpdate);
		
		if (needsFetchBuffersLock)
		{
			pthread_rwlock_rdlock([sharedData rwlockFramebufferAtIndex:processedInfo.bufferIndex]);
		}
		
		// Now that everything is set up, go ahead and draw everything.
		[colorAttachment0Desc setTexture:texRender];
		id<MTLCommandBuffer> cb = [self newCommandBuffer];
		
		[self renderForCommandBuffer:cb
				 outputPipelineState:[self outputRGBAPipeline]
					hudPipelineState:[sharedData hudRGBAPipeline]
					  texDisplayMain:(processedInfo.isMainDisplayProcessed)  ? processedInfo.tex[NDSDisplayID_Main]  : ((cdp->GetSelectedDisplaySourceForDisplay(NDSDisplayID_Main)  == NDSDisplayID_Main)  ? [sharedData texFetchMain]  : [sharedData texFetchTouch] )
					 texDisplayTouch:(processedInfo.isTouchDisplayProcessed) ? processedInfo.tex[NDSDisplayID_Touch] : ((cdp->GetSelectedDisplaySourceForDisplay(NDSDisplayID_Touch) == NDSDisplayID_Touch) ? [sharedData texFetchTouch] : [sharedData texFetchMain]  )];
		
		[cb addCompletedHandler:^(id<MTLCommandBuffer> block) {
			if (needsFetchBuffersLock)
			{
				pthread_rwlock_unlock([sharedData rwlockFramebufferAtIndex:processedInfo.bufferIndex]);
			}
			
			pthread_mutex_unlock(&_mutexBufferUpdate);
		}];
		
		[cb commit];
		
		cb = [self newCommandBuffer];
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
		
		[cb commit];
		[cb waitUntilCompleted];
		
		memcpy(dstBuffer, [dstMTLBuffer contents], clientWidth * clientHeight * sizeof(uint32_t));
		
		[texRender release];
		[dstMTLBuffer release];
	}
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
		const MetalProcessedFrameInfo processedInfo = [presenterObject processedFrameInfo];
		const bool needsFetchBuffersLock = !processedInfo.isMainDisplayProcessed || !processedInfo.isTouchDisplayProcessed;
		
		pthread_mutex_lock([presenterObject mutexBufferUpdate]);
		
		if (needsFetchBuffersLock)
		{
			pthread_rwlock_rdlock([[presenterObject sharedData] rwlockFramebufferAtIndex:processedInfo.bufferIndex]);
		}
				
		// Now that everything is set up, go ahead and draw everything.
		id<CAMetalDrawable> layerDrawable = [self nextDrawable];
		[[presenterObject colorAttachment0Desc] setTexture:[layerDrawable texture]];
		id<MTLCommandBuffer> cb = [presenterObject newCommandBuffer];
		
		[presenterObject renderForCommandBuffer:cb
							outputPipelineState:[presenterObject outputDrawablePipeline]
							   hudPipelineState:[[presenterObject sharedData] hudPipeline]
								 texDisplayMain:(processedInfo.isMainDisplayProcessed)  ? processedInfo.tex[NDSDisplayID_Main]  : (([presenterObject cdp]->GetSelectedDisplaySourceForDisplay(NDSDisplayID_Main)  == NDSDisplayID_Main)  ? [[presenterObject sharedData] texFetchMain]  : [[presenterObject sharedData] texFetchTouch] )
								texDisplayTouch:(processedInfo.isTouchDisplayProcessed) ? processedInfo.tex[NDSDisplayID_Touch] : (([presenterObject cdp]->GetSelectedDisplaySourceForDisplay(NDSDisplayID_Touch) == NDSDisplayID_Touch) ? [[presenterObject sharedData] texFetchTouch] : [[presenterObject sharedData] texFetchMain]  )];
		
		[cb presentDrawable:layerDrawable];
		[cb addCompletedHandler:^(id<MTLCommandBuffer> block) {
			if (needsFetchBuffersLock)
			{
				pthread_rwlock_unlock([[presenterObject sharedData] rwlockFramebufferAtIndex:processedInfo.bufferIndex]);
			}
			
			pthread_mutex_unlock([presenterObject mutexBufferUpdate]);
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
	
	pthread_rwlock_destroy(&this->_cpuFilterRWLock[NDSDisplayID_Main][0]);
	pthread_rwlock_destroy(&this->_cpuFilterRWLock[NDSDisplayID_Touch][0]);
	pthread_rwlock_destroy(&this->_cpuFilterRWLock[NDSDisplayID_Main][1]);
	pthread_rwlock_destroy(&this->_cpuFilterRWLock[NDSDisplayID_Touch][1]);
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
	
	pthread_rwlock_init(&_cpuFilterRWLock[NDSDisplayID_Main][0],  NULL);
	pthread_rwlock_init(&_cpuFilterRWLock[NDSDisplayID_Touch][0], NULL);
	pthread_rwlock_init(&_cpuFilterRWLock[NDSDisplayID_Main][1],  NULL);
	pthread_rwlock_init(&_cpuFilterRWLock[NDSDisplayID_Touch][1], NULL);
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
		
		pthread_rwlock_wrlock(&this->_cpuFilterRWLock[displayID][bufferIndex]);
		fetchObjMutable.CopyFromSrcClone(vf->GetSrcBufferPtr(), displayID, bufferIndex);
		pthread_rwlock_unlock(&this->_cpuFilterRWLock[displayID][bufferIndex]);
	}
}

void MacMetalDisplayPresenter::_ResizeCPUPixelScaler(const VideoFilterTypeID filterID)
{
	this->ClientDisplay3DPresenter::_ResizeCPUPixelScaler(filterID);
	[this->_presenterObject resizeCPUPixelScalerUsingFilterID:filterID];
}

MacMetalDisplayPresenterObject* MacMetalDisplayPresenter::GetPresenterObject() const
{
	return this->_presenterObject;
}

pthread_mutex_t* MacMetalDisplayPresenter::GetMutexProcessPtr()
{
	return &this->_mutexProcessPtr;
}

pthread_rwlock_t* MacMetalDisplayPresenter::GetCPUFilterRWLock(const NDSDisplayID displayID, const uint8_t bufferIndex)
{
	return &this->_cpuFilterRWLock[displayID][bufferIndex];
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
void SetupHQnxLUTs_Metal(id<MTLDevice> &device, id<MTLTexture> &texLQ2xLUT, id<MTLTexture> &texHQ2xLUT, id<MTLTexture> &texHQ3xLUT, id<MTLTexture> &texHQ4xLUT)
{
	MTLTextureDescriptor *texHQ2xLUTDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
																							  width:256 * 2
																							 height:4
																						  mipmapped:NO];
	
	[texHQ2xLUTDesc setTextureType:MTLTextureType3D];
	[texHQ2xLUTDesc setDepth:16];
	[texHQ2xLUTDesc setResourceOptions:MTLResourceStorageModeManaged];
	[texHQ2xLUTDesc setStorageMode:MTLStorageModeManaged];
	[texHQ2xLUTDesc setCpuCacheMode:MTLCPUCacheModeWriteCombined];
	[texHQ2xLUTDesc setUsage:MTLTextureUsageShaderRead];
	
	MTLTextureDescriptor *texHQ3xLUTDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
																							  width:256 * 2
																							 height:9
																						  mipmapped:NO];
	[texHQ3xLUTDesc setTextureType:MTLTextureType3D];
	[texHQ3xLUTDesc setDepth:16];
	[texHQ3xLUTDesc setResourceOptions:MTLResourceStorageModeManaged];
	[texHQ3xLUTDesc setStorageMode:MTLStorageModeManaged];
	[texHQ3xLUTDesc setCpuCacheMode:MTLCPUCacheModeWriteCombined];
	[texHQ3xLUTDesc setUsage:MTLTextureUsageShaderRead];
	
	MTLTextureDescriptor *texHQ4xLUTDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
																							  width:256 * 2
																							 height:16
																						  mipmapped:NO];
	[texHQ4xLUTDesc setTextureType:MTLTextureType3D];
	[texHQ4xLUTDesc setDepth:16];
	[texHQ4xLUTDesc setResourceOptions:MTLResourceStorageModeManaged];
	[texHQ4xLUTDesc setStorageMode:MTLStorageModeManaged];
	[texHQ4xLUTDesc setCpuCacheMode:MTLCPUCacheModeWriteCombined];
	[texHQ4xLUTDesc setUsage:MTLTextureUsageShaderRead];
	
	texLQ2xLUT = [[device newTextureWithDescriptor:texHQ2xLUTDesc] retain];
	texHQ2xLUT = [[device newTextureWithDescriptor:texHQ2xLUTDesc] retain];
	texHQ3xLUT = [[device newTextureWithDescriptor:texHQ3xLUTDesc] retain];
	texHQ4xLUT = [[device newTextureWithDescriptor:texHQ4xLUTDesc] retain];
	
	InitHQnxLUTs();
	[texLQ2xLUT replaceRegion:MTLRegionMake3D(0, 0, 0, 256 * 2, 4, 16)
				  mipmapLevel:0
						slice:0
					withBytes:_LQ2xLUT
				  bytesPerRow:256 * 2 * sizeof(uint32_t)
				bytesPerImage:256 * 2 * 4 * sizeof(uint32_t)];
	
	[texHQ2xLUT replaceRegion:MTLRegionMake3D(0, 0, 0, 256 * 2, 4, 16)
				  mipmapLevel:0
						slice:0
					withBytes:_HQ2xLUT
				  bytesPerRow:256 * 2 * sizeof(uint32_t)
				bytesPerImage:256 * 2 * 4 * sizeof(uint32_t)];
	
	[texHQ3xLUT replaceRegion:MTLRegionMake3D(0, 0, 0, 256 * 2, 9, 16)
				  mipmapLevel:0
						slice:0
					withBytes:_HQ3xLUT
				  bytesPerRow:256 * 2 * sizeof(uint32_t)
				bytesPerImage:256 * 2 * 9 * sizeof(uint32_t)];
	
	[texHQ4xLUT replaceRegion:MTLRegionMake3D(0, 0, 0, 256 * 2, 16, 16)
				  mipmapLevel:0
						slice:0
					withBytes:_HQ4xLUT
				  bytesPerRow:256 * 2 * sizeof(uint32_t)
				bytesPerImage:256 * 2 * 16 * sizeof(uint32_t)];
}

void DeleteHQnxLUTs_Metal(id<MTLTexture> &texLQ2xLUT, id<MTLTexture> &texHQ2xLUT, id<MTLTexture> &texHQ3xLUT, id<MTLTexture> &texHQ4xLUT)
{
	[texLQ2xLUT release];
	[texHQ2xLUT release];
	[texHQ3xLUT release];
	[texHQ4xLUT release];
}
