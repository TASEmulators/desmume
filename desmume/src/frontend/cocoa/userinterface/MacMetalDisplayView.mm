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
@synthesize samplerHUDBox;
@synthesize samplerHUDText;

@synthesize hudIndexBuffer;

@synthesize texDisplayFetch16NativeMain;
@synthesize texDisplayFetch16NativeTouch;
@synthesize texDisplayFetch32NativeMain;
@synthesize texDisplayFetch32NativeTouch;
@synthesize texDisplayFetch16CustomMain;
@synthesize texDisplayFetch16CustomTouch;
@synthesize texDisplayFetch32CustomMain;
@synthesize texDisplayFetch32CustomTouch;

@synthesize texDisplayPostprocessNativeMain;
@synthesize texDisplayPostprocessCustomMain;
@synthesize texDisplayPostprocessNativeTouch;
@synthesize texDisplayPostprocessCustomTouch;

@synthesize texDisplaySrcTargetMain;
@synthesize texDisplaySrcTargetTouch;

@synthesize texLQ2xLUT;
@synthesize texHQ2xLUT;
@synthesize texHQ3xLUT;
@synthesize texHQ4xLUT;
@synthesize texCurrentHQnxLUT;

@synthesize displayFetchNativeBufferSize;
@synthesize displayFetchCustomBufferSize;

@synthesize fetchThreadsPerGroup;
@synthesize fetchThreadGroupsPerGridNative;
@synthesize fetchThreadGroupsPerGridCustom;
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
	
	fetchThreadsPerGroup = MTLSizeMake(tw, th, 1);
	fetchThreadGroupsPerGridNative = MTLSizeMake(GPU_FRAMEBUFFER_NATIVE_WIDTH  / tw,
												 GPU_FRAMEBUFFER_NATIVE_HEIGHT / th,
												 1);
	
	fetchThreadGroupsPerGridCustom = fetchThreadGroupsPerGridNative;
	
	deposterizeThreadsPerGroup = fetchThreadsPerGroup;
	deposterizeThreadGroupsPerGrid = fetchThreadGroupsPerGridNative;
		
	MTLRenderPipelineDescriptor *hudPipelineDesc = [[MTLRenderPipelineDescriptor alloc] init];
	[[[hudPipelineDesc colorAttachments] objectAtIndexedSubscript:0] setPixelFormat:MTLPixelFormatBGRA8Unorm];
	[[[hudPipelineDesc colorAttachments] objectAtIndexedSubscript:0] setBlendingEnabled:YES];
	[[[hudPipelineDesc colorAttachments] objectAtIndexedSubscript:0] setRgbBlendOperation:MTLBlendOperationAdd];
	[[[hudPipelineDesc colorAttachments] objectAtIndexedSubscript:0] setAlphaBlendOperation:MTLBlendOperationAdd];
	[[[hudPipelineDesc colorAttachments] objectAtIndexedSubscript:0] setSourceRGBBlendFactor:MTLBlendFactorSourceAlpha];
	[[[hudPipelineDesc colorAttachments] objectAtIndexedSubscript:0] setSourceAlphaBlendFactor:MTLBlendFactorSourceAlpha];
	[[[hudPipelineDesc colorAttachments] objectAtIndexedSubscript:0] setDestinationRGBBlendFactor:MTLBlendFactorOneMinusSourceAlpha];
	[[[hudPipelineDesc colorAttachments] objectAtIndexedSubscript:0] setDestinationAlphaBlendFactor:MTLBlendFactorOneMinusSourceAlpha];
	[hudPipelineDesc setVertexFunction:[defaultLibrary newFunctionWithName:@"hud_vertex"]];
	[hudPipelineDesc setFragmentFunction:[defaultLibrary newFunctionWithName:@"hud_fragment"]];
	
	hudPipeline = [[device newRenderPipelineStateWithDescriptor:hudPipelineDesc error:nil] retain];
	[hudPipelineDesc release];
	
	hudIndexBuffer = [[device newBufferWithLength:(sizeof(uint16_t) * HUD_MAX_CHARACTERS * 6) options:MTLResourceStorageModeManaged] retain];
	
	uint16_t *idxBufferPtr = (uint16_t *)[hudIndexBuffer contents];
	for (size_t i = 0, j = 0, k = 0; i < HUD_MAX_CHARACTERS; i++, j+=6, k+=4)
	{
		idxBufferPtr[j+0] = k+0;
		idxBufferPtr[j+1] = k+1;
		idxBufferPtr[j+2] = k+2;
		idxBufferPtr[j+3] = k+2;
		idxBufferPtr[j+4] = k+3;
		idxBufferPtr[j+5] = k+0;
	}
	
	[hudIndexBuffer didModifyRange:NSMakeRange(0, sizeof(uint16_t) * HUD_MAX_CHARACTERS * 6)];
	
	_bufDisplayFetchNative[NDSDisplayID_Main][0]  = nil;
	_bufDisplayFetchNative[NDSDisplayID_Main][1]  = nil;
	_bufDisplayFetchNative[NDSDisplayID_Touch][0] = nil;
	_bufDisplayFetchNative[NDSDisplayID_Touch][1] = nil;
	_bufDisplayFetchCustom[NDSDisplayID_Main][0]  = nil;
	_bufDisplayFetchCustom[NDSDisplayID_Main][1]  = nil;
	_bufDisplayFetchCustom[NDSDisplayID_Touch][0] = nil;
	_bufDisplayFetchCustom[NDSDisplayID_Touch][1] = nil;
	displayFetchNativeBufferSize = 0;
	displayFetchCustomBufferSize = 0;
	
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
	
	texDisplayFetch16NativeMain  = [[device newTextureWithDescriptor:texDisplayLoad16Desc] retain];
	texDisplayFetch16NativeTouch = [[device newTextureWithDescriptor:texDisplayLoad16Desc] retain];
	texDisplayFetch16CustomMain  = [[device newTextureWithDescriptor:texDisplayLoad16Desc] retain];
	texDisplayFetch16CustomTouch = [[device newTextureWithDescriptor:texDisplayLoad16Desc] retain];
	
	MTLTextureDescriptor *texDisplayLoad32Desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
																									width:GPU_FRAMEBUFFER_NATIVE_WIDTH
																								   height:GPU_FRAMEBUFFER_NATIVE_HEIGHT
																								mipmapped:NO];
	[texDisplayLoad32Desc setResourceOptions:MTLResourceStorageModeManaged];
	[texDisplayLoad32Desc setStorageMode:MTLStorageModeManaged];
	[texDisplayLoad32Desc setCpuCacheMode:MTLCPUCacheModeWriteCombined];
	[texDisplayLoad32Desc setUsage:MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite];
	
	texDisplayFetch32NativeMain  = [[device newTextureWithDescriptor:texDisplayLoad32Desc] retain];
	texDisplayFetch32NativeTouch = [[device newTextureWithDescriptor:texDisplayLoad32Desc] retain];
	texDisplayFetch32CustomMain  = [[device newTextureWithDescriptor:texDisplayLoad32Desc] retain];
	texDisplayFetch32CustomTouch = [[device newTextureWithDescriptor:texDisplayLoad32Desc] retain];
	
	texDisplayPostprocessNativeMain  = [[device newTextureWithDescriptor:texDisplayLoad32Desc] retain];
	texDisplayPostprocessCustomMain  = [[device newTextureWithDescriptor:texDisplayLoad32Desc] retain];
	texDisplayPostprocessNativeTouch = [[device newTextureWithDescriptor:texDisplayLoad32Desc] retain];
	texDisplayPostprocessCustomTouch = [[device newTextureWithDescriptor:texDisplayLoad32Desc] retain];
	
	uint32_t *blankBuffer = (uint32_t *)calloc(GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT, sizeof(uint32_t));
	const MTLRegion texRegionNative = MTLRegionMake2D(0, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT);
	[texDisplayFetch32NativeMain  replaceRegion:texRegionNative
									mipmapLevel:0
									  withBytes:blankBuffer
									bytesPerRow:GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(uint32_t)];
	[texDisplayFetch32NativeTouch replaceRegion:texRegionNative
									mipmapLevel:0
									  withBytes:blankBuffer
									bytesPerRow:GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(uint32_t)];
	[texDisplayFetch32CustomMain  replaceRegion:texRegionNative
									mipmapLevel:0
									  withBytes:blankBuffer
									bytesPerRow:GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(uint32_t)];
	[texDisplayFetch32CustomTouch replaceRegion:texRegionNative
									mipmapLevel:0
									  withBytes:blankBuffer
									bytesPerRow:GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(uint32_t)];
	
	[texDisplayPostprocessNativeMain  replaceRegion:texRegionNative
										mipmapLevel:0
										  withBytes:blankBuffer
										bytesPerRow:GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(uint32_t)];
	[texDisplayPostprocessCustomMain  replaceRegion:texRegionNative
										mipmapLevel:0
										  withBytes:blankBuffer
										bytesPerRow:GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(uint32_t)];
	[texDisplayPostprocessNativeTouch replaceRegion:texRegionNative
										mipmapLevel:0
										  withBytes:blankBuffer
										bytesPerRow:GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(uint32_t)];
	[texDisplayPostprocessCustomTouch replaceRegion:texRegionNative
										mipmapLevel:0
										  withBytes:blankBuffer
										bytesPerRow:GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(uint32_t)];
	free(blankBuffer);
	
	texDisplaySrcTargetMain  = [texDisplayFetch32NativeMain retain];
	texDisplaySrcTargetTouch = [texDisplayFetch32NativeTouch retain];
	
	// Set up the HQnx LUT textures.
	SetupHQnxLUTs_Metal(device, texLQ2xLUT, texHQ2xLUT, texHQ3xLUT, texHQ4xLUT);
	texCurrentHQnxLUT = nil;
	
	_fetchEncoder = nil;
	pthread_mutex_init(&_mutexFetch, NULL);
	
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
	[hudIndexBuffer release];
	
	[_bufMasterBrightMode[NDSDisplayID_Main] release];
	[_bufMasterBrightMode[NDSDisplayID_Touch] release];
	[_bufMasterBrightIntensity[NDSDisplayID_Main] release];
	[_bufMasterBrightIntensity[NDSDisplayID_Touch] release];
	
	[texDisplayFetch16NativeMain release];
	[texDisplayFetch16NativeTouch release];
	[texDisplayFetch32NativeMain release];
	[texDisplayFetch32NativeTouch release];
	[self setTexDisplayFetch16CustomMain:nil];
	[self setTexDisplayFetch16CustomTouch:nil];
	[self setTexDisplayFetch32CustomMain:nil];
	[self setTexDisplayFetch32CustomTouch:nil];
	
	[self setTexDisplayPostprocessNativeMain:nil];
	[self setTexDisplayPostprocessCustomMain:nil];
	[self setTexDisplayPostprocessNativeTouch:nil];
	[self setTexDisplayPostprocessCustomTouch:nil];
	
	[self setTexDisplaySrcTargetMain:nil];
	[self setTexDisplaySrcTargetTouch:nil];
	
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
	
	pthread_mutex_destroy(&_mutexFetch);
	
	[super dealloc];
}

- (void) setFetchBuffersWithDisplayInfo:(const NDSDisplayInfo &)dispInfo
{
	const size_t nativeSize = GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * dispInfo.pixelBytes;
	const size_t customSize = dispInfo.customWidth * dispInfo.customHeight * dispInfo.pixelBytes;
	
	id<MTLCommandBuffer> cb = [[self commandQueue] commandBufferWithUnretainedReferences];
	id<MTLBlitCommandEncoder> dummyEncoder = [cb blitCommandEncoder];
	[dummyEncoder endEncoding];
	[cb commit];
	[cb waitUntilCompleted];
	
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
	
	[self setDisplayFetchNativeBufferSize:nativeSize];
	[self setDisplayFetchCustomBufferSize:customSize];
	
	cb = [[self commandQueue] commandBufferWithUnretainedReferences];
	dummyEncoder = [cb blitCommandEncoder];
	[dummyEncoder endEncoding];
	[cb commit];
	[cb waitUntilCompleted];
}

- (void) fetchFromBufferIndex:(const u8)index
{
	pthread_mutex_lock(&_mutexFetch);
	pthread_rwlock_rdlock([self rwlockFramebufferAtIndex:index]);
	
	id<MTLCommandBuffer> cb = [commandQueue commandBufferWithUnretainedReferences];
	_fetchEncoder = [cb blitCommandEncoder];
	
	GPUFetchObject->GPUClientFetchObject::FetchFromBufferIndex(index);
	
	[_fetchEncoder endEncoding];
	
	pthread_rwlock_unlock([self rwlockFramebufferAtIndex:index]);
	pthread_mutex_unlock(&_mutexFetch);
	
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
				texDisplaySrcTarget[NDSDisplayID_Main] = texDisplayFetch32NativeMain;
			}
			else
			{
				texDisplaySrcTarget[NDSDisplayID_Main] = texDisplayFetch32CustomMain;
			}
		}
		
		if (isTouchEnabled)
		{
			if (!currentDisplayInfo.didPerformCustomRender[NDSDisplayID_Touch])
			{
				texDisplaySrcTarget[NDSDisplayID_Touch] = texDisplayFetch32NativeTouch;
			}
			else
			{
				texDisplaySrcTarget[NDSDisplayID_Touch] = texDisplayFetch32CustomTouch;
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
						[cce setTexture:texDisplayFetch16NativeMain atIndex:0];
						[cce setTexture:[self texDisplayPostprocessNativeMain] atIndex:1];
						[cce dispatchThreadgroups:fetchThreadGroupsPerGridNative
							threadsPerThreadgroup:fetchThreadsPerGroup];
						
						texDisplaySrcTarget[NDSDisplayID_Main] = [self texDisplayPostprocessNativeMain];
					}
					else
					{
						[cce setTexture:[self texDisplayFetch16CustomMain] atIndex:0];
						[cce setTexture:[self texDisplayPostprocessCustomMain] atIndex:1];
						[cce dispatchThreadgroups:[self fetchThreadGroupsPerGridCustom]
							threadsPerThreadgroup:[self fetchThreadsPerGroup]];
						
						texDisplaySrcTarget[NDSDisplayID_Main] = [self texDisplayPostprocessCustomMain];
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
						[cce setTexture:texDisplayFetch16NativeTouch atIndex:0];
						[cce setTexture:[self texDisplayPostprocessNativeTouch] atIndex:1];
						[cce dispatchThreadgroups:fetchThreadGroupsPerGridNative
							threadsPerThreadgroup:fetchThreadsPerGroup];
						
						texDisplaySrcTarget[NDSDisplayID_Touch] = [self texDisplayPostprocessNativeTouch];
					}
					else
					{
						[cce setTexture:[self texDisplayFetch16CustomTouch] atIndex:0];
						[cce setTexture:[self texDisplayPostprocessCustomTouch] atIndex:1];
						[cce dispatchThreadgroups:[self fetchThreadGroupsPerGridCustom]
							threadsPerThreadgroup:[self fetchThreadsPerGroup]];
						
						texDisplaySrcTarget[NDSDisplayID_Touch] = [self texDisplayPostprocessCustomTouch];
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
						[cce setTexture:texDisplayFetch16NativeMain atIndex:0];
						[cce setTexture:[self texDisplayPostprocessNativeMain] atIndex:1];
						[cce dispatchThreadgroups:fetchThreadGroupsPerGridNative
							threadsPerThreadgroup:fetchThreadsPerGroup];
						
						texDisplaySrcTarget[NDSDisplayID_Main] = [self texDisplayPostprocessNativeMain];
					}
					else
					{
						[cce setTexture:[self texDisplayFetch16CustomMain] atIndex:0];
						[cce setTexture:[self texDisplayPostprocessCustomMain] atIndex:1];
						[cce dispatchThreadgroups:[self fetchThreadGroupsPerGridCustom]
							threadsPerThreadgroup:[self fetchThreadsPerGroup]];
						
						texDisplaySrcTarget[NDSDisplayID_Main] = [self texDisplayPostprocessCustomMain];
					}
				}
				
				if (isTouchEnabled)
				{
					if (!currentDisplayInfo.didPerformCustomRender[NDSDisplayID_Touch])
					{
						[cce setTexture:texDisplayFetch16NativeTouch atIndex:0];
						[cce setTexture:[self texDisplayPostprocessNativeTouch] atIndex:1];
						[cce dispatchThreadgroups:fetchThreadGroupsPerGridNative
							threadsPerThreadgroup:fetchThreadsPerGroup];
						
						texDisplaySrcTarget[NDSDisplayID_Touch] = [self texDisplayPostprocessNativeTouch];
					}
					else
					{
						[cce setTexture:[self texDisplayFetch16CustomTouch] atIndex:0];
						[cce setTexture:[self texDisplayPostprocessCustomTouch] atIndex:1];
						[cce dispatchThreadgroups:[self fetchThreadGroupsPerGridCustom]
							threadsPerThreadgroup:[self fetchThreadsPerGroup]];
						
						texDisplaySrcTarget[NDSDisplayID_Touch] = [self texDisplayPostprocessCustomTouch];
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
					
					if (texDisplaySrcTarget[NDSDisplayID_Main] == texDisplayFetch32NativeMain)
					{
						[cce setTexture:texDisplayFetch32NativeMain atIndex:0];
						[cce setTexture:[self texDisplayPostprocessNativeMain] atIndex:1];
						[cce dispatchThreadgroups:fetchThreadGroupsPerGridNative
							threadsPerThreadgroup:fetchThreadsPerGroup];
						
						texDisplaySrcTarget[NDSDisplayID_Main] = [self texDisplayPostprocessNativeMain];
					}
					else
					{
						[cce setTexture:[self texDisplayFetch32CustomMain] atIndex:0];
						[cce setTexture:[self texDisplayPostprocessCustomMain] atIndex:1];
						[cce dispatchThreadgroups:[self fetchThreadGroupsPerGridCustom]
							threadsPerThreadgroup:[self fetchThreadsPerGroup]];
						
						texDisplaySrcTarget[NDSDisplayID_Main] = [self texDisplayPostprocessCustomMain];
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
					
					if (texDisplaySrcTarget[NDSDisplayID_Touch] == texDisplayFetch32NativeTouch)
					{
						[cce setTexture:texDisplayFetch32NativeTouch atIndex:0];
						[cce setTexture:[self texDisplayPostprocessNativeTouch] atIndex:1];
						[cce dispatchThreadgroups:fetchThreadGroupsPerGridNative
							threadsPerThreadgroup:fetchThreadsPerGroup];
						
						texDisplaySrcTarget[NDSDisplayID_Touch] = [self texDisplayPostprocessNativeTouch];
					}
					else
					{
						[cce setTexture:[self texDisplayFetch32CustomTouch] atIndex:0];
						[cce setTexture:[self texDisplayPostprocessCustomTouch] atIndex:1];
						[cce dispatchThreadgroups:[self fetchThreadGroupsPerGridCustom]
							threadsPerThreadgroup:[self fetchThreadsPerGroup]];
						
						texDisplaySrcTarget[NDSDisplayID_Touch] = [self texDisplayPostprocessCustomTouch];
					}
				}
			}
		}
		
		[cce endEncoding];
	}
	
	[cb commit];
	
	[self setTexDisplaySrcTargetMain:texDisplaySrcTarget[NDSDisplayID_Main]];
	[self setTexDisplaySrcTargetTouch:texDisplaySrcTarget[NDSDisplayID_Touch]];
}

- (void) fetchNativeDisplayByID:(const NDSDisplayID)displayID bufferIndex:(const u8)bufferIndex
{
	const NDSDisplayInfo &currentDisplayInfo = GPUFetchObject->GetFetchDisplayInfoForBufferIndex(bufferIndex);
	
	id<MTLTexture> texFetch16 = (displayID == NDSDisplayID_Main) ? texDisplayFetch16NativeMain : texDisplayFetch16NativeTouch;
	id<MTLTexture> texFetch32 = (displayID == NDSDisplayID_Main) ? texDisplayFetch32NativeMain : texDisplayFetch32NativeTouch;
	const size_t bufferSize = [self displayFetchNativeBufferSize];
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
	const size_t w = currentDisplayInfo.customWidth;
	const size_t h = currentDisplayInfo.customHeight;
	
	id<MTLTexture> texFetch16 = (displayID == NDSDisplayID_Main) ? [self texDisplayFetch16CustomMain] : [self texDisplayFetch16CustomTouch];
	id<MTLTexture> texFetch32 = (displayID == NDSDisplayID_Main) ? [self texDisplayFetch32CustomMain] : [self texDisplayFetch32CustomTouch];
	
	// If the existing texture size is different than the incoming size, then remake the textures to match the incoming size.
	if ( ([texFetch32 width] != w) || ([texFetch32 height] != h) )
	{
		MTLTextureDescriptor *texDisplayLoad16Desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR16Uint
																										width:w
																									   height:h
																									mipmapped:NO];
		[texDisplayLoad16Desc setResourceOptions:MTLResourceStorageModeManaged];
		[texDisplayLoad16Desc setStorageMode:MTLStorageModeManaged];
		[texDisplayLoad16Desc setCpuCacheMode:MTLCPUCacheModeWriteCombined];
		[texDisplayLoad16Desc setUsage:MTLTextureUsageShaderRead];
		
		MTLTextureDescriptor *texDisplayLoad32Desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
																										width:w
																									   height:h
																									mipmapped:NO];
		[texDisplayLoad32Desc setResourceOptions:MTLResourceStorageModePrivate];
		[texDisplayLoad32Desc setStorageMode:MTLStorageModePrivate];
		[texDisplayLoad32Desc setUsage:MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite];
		
		if (displayID == NDSDisplayID_Main)
		{
			[self setTexDisplayFetch16CustomMain:[device newTextureWithDescriptor:texDisplayLoad16Desc]];
			[self setTexDisplayFetch32CustomMain:[device newTextureWithDescriptor:texDisplayLoad32Desc]];
			[self setTexDisplayPostprocessCustomMain:[device newTextureWithDescriptor:texDisplayLoad32Desc]];
			texFetch16 = [self texDisplayFetch16CustomMain];
			texFetch32 = [self texDisplayFetch32CustomMain];
		}
		else
		{
			[self setTexDisplayFetch16CustomTouch:[device newTextureWithDescriptor:texDisplayLoad16Desc]];
			[self setTexDisplayFetch32CustomTouch:[device newTextureWithDescriptor:texDisplayLoad32Desc]];
			[self setTexDisplayPostprocessCustomTouch:[device newTextureWithDescriptor:texDisplayLoad32Desc]];
			texFetch16 = [self texDisplayFetch16CustomTouch];
			texFetch32 = [self texDisplayFetch32CustomTouch];
		}
		
		const size_t tw = fetchThreadsPerGroup.width;
		const size_t th = fetchThreadsPerGroup.height;
		
		[self setFetchThreadGroupsPerGridCustom:MTLSizeMake((currentDisplayInfo.customWidth  + tw - 1) / tw,
															(currentDisplayInfo.customHeight + th - 1) / th,
															1)];
	}
	
	const size_t bufferSize = [self displayFetchCustomBufferSize];
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

@implementation DisplayViewMetalLayer

@synthesize sharedData;
@synthesize colorAttachment0Desc;
@synthesize pixelScalePipeline;
@synthesize displayOutputPipeline;
@synthesize bufCPUFilterSrcMain;
@synthesize bufCPUFilterSrcTouch;
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

- (id)init
{
	self = [super init];
	if(self == nil)
	{
		return nil;
	}
	
	sharedData = nil;
	availableResources = dispatch_semaphore_create(3);
	
	_cdv = new MacMetalDisplayView();
	_cdv->SetFrontendLayer(self);
	
	_outputRenderPassDesc = [[MTLRenderPassDescriptor renderPassDescriptor] retain];
	colorAttachment0Desc = [[_outputRenderPassDesc colorAttachments] objectAtIndexedSubscript:0];
	[colorAttachment0Desc setLoadAction:MTLLoadActionClear];
	[colorAttachment0Desc setStoreAction:MTLStoreActionStore];
	[colorAttachment0Desc setClearColor:MTLClearColorMake(0.0, 0.0, 0.0, 1.0)];
	
	pixelScalePipeline = nil;
	displayOutputPipeline = nil;
	
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
	bufCPUFilterSrcMain  = nil;
	bufCPUFilterSrcTouch = nil;
	bufCPUFilterDstMain  = nil;
	bufCPUFilterDstTouch = nil;
	texDisplayPixelScaleMain  = nil;
	texDisplayPixelScaleTouch = nil;
	_texDisplayOutput[0] = nil;
	_texDisplayOutput[1] = nil;
	texHUDCharMap = nil;
	
	[self setOpaque:YES];
	
	_pixelScalerThreadsPerGroup = MTLSizeMake(1, 1, 1);
	_pixelScalerThreadGroupsPerGrid = MTLSizeMake(1, 1, 1);
	
	needsViewportUpdate = YES;
	needsRotationScaleUpdate = YES;
	needsScreenVerticesUpdate = YES;
	needsHUDVerticesUpdate = YES;
	
	return self;
}

- (void)dealloc
{
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
	
	[self setBufCPUFilterSrcMain:nil];
	[self setBufCPUFilterSrcTouch:nil];
	[self setBufCPUFilterDstMain:nil];
	[self setBufCPUFilterDstTouch:nil];
	[self setTexDisplayPixelScaleMain:nil];
	[self setTexDisplayPixelScaleTouch:nil];
	
	[self setPixelScalePipeline:nil];
	[self setDisplayOutputPipeline:nil];
	[self setTexHUDCharMap:nil];
	
	[self setSharedData:nil];
	delete _cdv;
	
	dispatch_release(availableResources);
	
	[super dealloc];
}

- (ClientDisplay3DView *)clientDisplay3DView
{
	return _cdv;
}

- (VideoFilterTypeID) pixelScaler
{
	return _cdv->GetPixelScaler();
}

- (void) setPixelScaler:(VideoFilterTypeID)filterID
{
	id<MTLTexture> currentHQnxLUT = nil;
	
	switch (filterID)
	{
		case VideoFilterTypeID_Nearest2X:
			[self setPixelScalePipeline:[[self device] newComputePipelineStateWithFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_nearest2x"] error:nil]];
			break;
			
		case VideoFilterTypeID_Scanline:
			[self setPixelScalePipeline:[[self device] newComputePipelineStateWithFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_scanline"] error:nil]];
			break;
			
		case VideoFilterTypeID_EPX:
			[self setPixelScalePipeline:[[self device] newComputePipelineStateWithFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_2xEPX"] error:nil]];
			break;
			
		case VideoFilterTypeID_EPXPlus:
			[self setPixelScalePipeline:[[self device] newComputePipelineStateWithFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_2xEPXPlus"] error:nil]];
			break;
			
		case VideoFilterTypeID_2xSaI:
			[self setPixelScalePipeline:[[self device] newComputePipelineStateWithFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_2xSaI"] error:nil]];
			break;
			
		case VideoFilterTypeID_Super2xSaI:
			[self setPixelScalePipeline:[[self device] newComputePipelineStateWithFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_Super2xSaI"] error:nil]];
			break;
			
		case VideoFilterTypeID_SuperEagle:
			[self setPixelScalePipeline:[[self device] newComputePipelineStateWithFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_2xSuperEagle"] error:nil]];
			break;
			
		case VideoFilterTypeID_LQ2X:
			[self setPixelScalePipeline:[[self device] newComputePipelineStateWithFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_LQ2x"] error:nil]];
			currentHQnxLUT = [sharedData texLQ2xLUT];
			break;
			
		case VideoFilterTypeID_LQ2XS:
			[self setPixelScalePipeline:[[self device] newComputePipelineStateWithFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_LQ2xS"] error:nil]];
			currentHQnxLUT = [sharedData texLQ2xLUT];
			break;
			
		case VideoFilterTypeID_HQ2X:
			[self setPixelScalePipeline:[[self device] newComputePipelineStateWithFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_HQ2x"] error:nil]];
			currentHQnxLUT = [sharedData texHQ2xLUT];
			break;
			
		case VideoFilterTypeID_HQ2XS:
			[self setPixelScalePipeline:[[self device] newComputePipelineStateWithFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_HQ2xS"] error:nil]];
			currentHQnxLUT = [sharedData texHQ2xLUT];
			break;
			
		case VideoFilterTypeID_HQ3X:
			[self setPixelScalePipeline:[[self device] newComputePipelineStateWithFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_HQ3x"] error:nil]];
			currentHQnxLUT = [sharedData texHQ3xLUT];
			break;
			
		case VideoFilterTypeID_HQ3XS:
			[self setPixelScalePipeline:[[self device] newComputePipelineStateWithFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_HQ3xS"] error:nil]];
			currentHQnxLUT = [sharedData texHQ3xLUT];
			break;
			
		case VideoFilterTypeID_HQ4X:
			[self setPixelScalePipeline:[[self device] newComputePipelineStateWithFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_HQ4x"] error:nil]];
			currentHQnxLUT = [sharedData texHQ4xLUT];
			break;
			
		case VideoFilterTypeID_HQ4XS:
			[self setPixelScalePipeline:[[self device] newComputePipelineStateWithFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_HQ4xS"] error:nil]];
			currentHQnxLUT = [sharedData texHQ4xLUT];
			break;
			
		case VideoFilterTypeID_2xBRZ:
			[self setPixelScalePipeline:[[self device] newComputePipelineStateWithFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_2xBRZ"] error:nil]];
			break;
			
		case VideoFilterTypeID_3xBRZ:
			[self setPixelScalePipeline:[[self device] newComputePipelineStateWithFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_3xBRZ"] error:nil]];
			break;
			
		case VideoFilterTypeID_4xBRZ:
			[self setPixelScalePipeline:[[self device] newComputePipelineStateWithFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_4xBRZ"] error:nil]];
			break;
			
		case VideoFilterTypeID_5xBRZ:
			[self setPixelScalePipeline:[[self device] newComputePipelineStateWithFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_5xBRZ"] error:nil]];
			break;
			
		case VideoFilterTypeID_6xBRZ:
			[self setPixelScalePipeline:[[self device] newComputePipelineStateWithFunction:[[sharedData defaultLibrary] newFunctionWithName:@"pixel_scaler_6xBRZ"] error:nil]];
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
	
	[self setTexDisplayPixelScaleMain:[[self device] newTextureWithDescriptor:texDisplayPixelScaleDesc]];
	[self setTexDisplayPixelScaleTouch:[[self device] newTextureWithDescriptor:texDisplayPixelScaleDesc]];
	
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
	return _cdv->GetOutputFilter();
}

- (void) setOutputFilter:(OutputFilterTypeID)filterID
{
	MTLRenderPipelineDescriptor *outputPipelineDesc = [[MTLRenderPipelineDescriptor alloc] init];
	[[[outputPipelineDesc colorAttachments] objectAtIndexedSubscript:0] setPixelFormat:[self pixelFormat]];
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
	
	[self setDisplayOutputPipeline:[[self device] newRenderPipelineStateWithDescriptor:outputPipelineDesc error:nil]];
	[outputPipelineDesc release];
}

- (id<MTLCommandBuffer>) newCommandBuffer
{
	return [[sharedData commandQueue] commandBufferWithUnretainedReferences];
}

- (void) setupLayer
{
	[self setDevice:[sharedData device]];
	
	MTLRenderPipelineDescriptor *outputPipelineDesc = [[MTLRenderPipelineDescriptor alloc] init];
	[[[outputPipelineDesc colorAttachments] objectAtIndexedSubscript:0] setPixelFormat:[self pixelFormat]];
	[outputPipelineDesc setAlphaToOneEnabled:YES];
	[outputPipelineDesc setVertexFunction:[[sharedData defaultLibrary] newFunctionWithName:@"display_output_vertex"]];
	[outputPipelineDesc setFragmentFunction:[[sharedData defaultLibrary] newFunctionWithName:@"output_filter_bilinear"]];
	
	displayOutputPipeline = [[[self device] newRenderPipelineStateWithDescriptor:outputPipelineDesc error:nil] retain];
	[outputPipelineDesc release];
	
	_cdvPropertiesBuffer = [[[self device] newBufferWithLength:sizeof(DisplayViewShaderProperties) options:MTLResourceStorageModeManaged] retain];
	_displayVtxPositionBuffer = [[[self device] newBufferWithLength:(sizeof(float) * (4 * 8)) options:MTLResourceStorageModeManaged] retain];
	_displayTexCoordBuffer = [[[self device] newBufferWithLength:(sizeof(float) * (4 * 8)) options:MTLResourceStorageModeManaged] retain];
	_hudVtxPositionBuffer = [[[self device] newBufferWithLength:HUD_VERTEX_ATTRIBUTE_BUFFER_SIZE options:MTLResourceStorageModeManaged] retain];
	_hudVtxColorBuffer = [[[self device] newBufferWithLength:HUD_VERTEX_COLOR_ATTRIBUTE_BUFFER_SIZE options:MTLResourceStorageModeManaged] retain];
	_hudTexCoordBuffer = [[[self device] newBufferWithLength:HUD_VERTEX_ATTRIBUTE_BUFFER_SIZE options:MTLResourceStorageModeManaged] retain];
	
	DisplayViewShaderProperties *viewProps = (DisplayViewShaderProperties *)[_cdvPropertiesBuffer contents];
	viewProps->width               = _cdv->GetViewProperties().clientWidth;
	viewProps->height              = _cdv->GetViewProperties().clientHeight;
	viewProps->rotation            = _cdv->GetViewProperties().rotation;
	viewProps->viewScale           = _cdv->GetViewProperties().viewScale;
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
	
	_texDisplaySrcDeposterize[NDSDisplayID_Main][0]  = [[[self device] newTextureWithDescriptor:texDisplaySrcDesc] retain];
	_texDisplaySrcDeposterize[NDSDisplayID_Touch][0] = [[[self device] newTextureWithDescriptor:texDisplaySrcDesc] retain];
	_texDisplaySrcDeposterize[NDSDisplayID_Main][1]  = [[[self device] newTextureWithDescriptor:texDisplaySrcDesc] retain];
	_texDisplaySrcDeposterize[NDSDisplayID_Touch][1] = [[[self device] newTextureWithDescriptor:texDisplaySrcDesc] retain];
	
	MTLTextureDescriptor *texDisplayPixelScaleDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
																										width:GPU_FRAMEBUFFER_NATIVE_WIDTH*2
																									   height:GPU_FRAMEBUFFER_NATIVE_HEIGHT*2
																									mipmapped:NO];
	[texDisplayPixelScaleDesc setResourceOptions:MTLResourceStorageModePrivate];
	[texDisplayPixelScaleDesc setStorageMode:MTLStorageModePrivate];
	[texDisplayPixelScaleDesc setUsage:MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite];
	
	[self setTexDisplayPixelScaleMain:[[self device] newTextureWithDescriptor:texDisplayPixelScaleDesc]];
	[self setTexDisplayPixelScaleTouch:[[self device] newTextureWithDescriptor:texDisplayPixelScaleDesc]];
	
	_texDisplayOutput[NDSDisplayID_Main]  = [sharedData texDisplayFetch32NativeMain];
	_texDisplayOutput[NDSDisplayID_Touch] = [sharedData texDisplayFetch32NativeTouch];
	
	VideoFilter *vfMain  = _cdv->GetPixelScalerObject(NDSDisplayID_Main);
	[self setBufCPUFilterSrcMain:[[self device] newBufferWithBytesNoCopy:vfMain->GetSrcBufferPtr()
																  length:vfMain->GetSrcWidth() * vfMain->GetSrcHeight() * sizeof(uint32_t)
																 options:MTLResourceStorageModeManaged
															 deallocator:nil]];
	
	[self setBufCPUFilterDstMain:[[self device] newBufferWithBytesNoCopy:vfMain->GetDstBufferPtr()
																  length:vfMain->GetDstWidth() * vfMain->GetDstHeight() * sizeof(uint32_t)
																 options:MTLResourceStorageModeManaged
															 deallocator:nil]];
	
	VideoFilter *vfTouch = _cdv->GetPixelScalerObject(NDSDisplayID_Touch);
	[self setBufCPUFilterSrcTouch:[[self device] newBufferWithBytesNoCopy:vfTouch->GetSrcBufferPtr()
																   length:vfTouch->GetSrcWidth() * vfTouch->GetSrcHeight() * sizeof(uint32_t)
																  options:MTLResourceStorageModeManaged
															  deallocator:nil]];
	
	[self setBufCPUFilterDstTouch:[[self device] newBufferWithBytesNoCopy:vfTouch->GetDstBufferPtr()
																   length:vfTouch->GetDstWidth() * vfTouch->GetDstHeight() * sizeof(uint32_t)
																  options:MTLResourceStorageModeManaged
															  deallocator:nil]];
	
	texHUDCharMap = nil;
}

- (void) resizeCPUPixelScalerUsingFilterID:(const VideoFilterTypeID)filterID
{
	const VideoFilterAttributes vfAttr = VideoFilter::GetAttributesByID(filterID);
	
	id<MTLCommandBuffer> cb = [[sharedData commandQueue] commandBufferWithUnretainedReferences];
	id<MTLBlitCommandEncoder> dummyEncoder = [cb blitCommandEncoder];
	[dummyEncoder endEncoding];
	[cb commit];
	[cb waitUntilCompleted];
	
	VideoFilter *vfMain  = _cdv->GetPixelScalerObject(NDSDisplayID_Main);
	[self setBufCPUFilterDstMain:[[self device] newBufferWithBytesNoCopy:vfMain->GetDstBufferPtr()
																  length:(vfMain->GetSrcWidth()  * vfAttr.scaleMultiply / vfAttr.scaleDivide) * (vfMain->GetSrcHeight()  * vfAttr.scaleMultiply / vfAttr.scaleDivide) * sizeof(uint32_t)
																 options:MTLResourceStorageModeManaged
															 deallocator:nil]];
	
	VideoFilter *vfTouch = _cdv->GetPixelScalerObject(NDSDisplayID_Touch);
	[self setBufCPUFilterDstTouch:[[self device] newBufferWithBytesNoCopy:vfTouch->GetDstBufferPtr()
																   length:(vfTouch->GetSrcWidth() * vfAttr.scaleMultiply / vfAttr.scaleDivide) * (vfTouch->GetSrcHeight() * vfAttr.scaleMultiply / vfAttr.scaleDivide) * sizeof(uint32_t)
																  options:MTLResourceStorageModeManaged
															  deallocator:nil]];
	
	cb = [[sharedData commandQueue] commandBufferWithUnretainedReferences];
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
	
	[self setTexHUDCharMap:[[self device] newTextureWithDescriptor:texHUDCharMapDesc]];
	
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
	const NDSDisplayInfo &fetchDisplayInfo = _cdv->GetEmuDisplayInfo();
	const ClientDisplayMode mode = _cdv->GetViewProperties().mode;
	const bool useDeposterize = _cdv->GetSourceDeposterize();
	const NDSDisplayID selectedDisplaySource[2] = { _cdv->GetSelectedDisplaySourceForDisplay(NDSDisplayID_Main), _cdv->GetSelectedDisplaySourceForDisplay(NDSDisplayID_Touch) };
	
	if (selectedDisplaySource[NDSDisplayID_Main] == NDSDisplayID_Main)
	{
		_texDisplayOutput[NDSDisplayID_Main]  = [sharedData texDisplaySrcTargetMain];
	}
	else
	{
		_texDisplayOutput[NDSDisplayID_Main]  = [sharedData texDisplaySrcTargetTouch];
	}
	
	if (selectedDisplaySource[NDSDisplayID_Touch] == NDSDisplayID_Touch)
	{
		_texDisplayOutput[NDSDisplayID_Touch] = [sharedData texDisplaySrcTargetTouch];
	}
	else
	{
		_texDisplayOutput[NDSDisplayID_Touch] = [sharedData texDisplaySrcTargetMain];
	}
	
	if ( (fetchDisplayInfo.pixelBytes != 0) && (useDeposterize || (_cdv->GetPixelScaler() != VideoFilterTypeID_None)) )
	{
		const bool willFilterOnGPU = _cdv->WillFilterOnGPU();
		const bool shouldProcessDisplay[2] = { (!fetchDisplayInfo.didPerformCustomRender[selectedDisplaySource[NDSDisplayID_Main]]  || !fetchDisplayInfo.isCustomSizeRequested) && _cdv->IsSelectedDisplayEnabled(NDSDisplayID_Main)  && (mode == ClientDisplayMode_Main  || mode == ClientDisplayMode_Dual),
		                                       (!fetchDisplayInfo.didPerformCustomRender[selectedDisplaySource[NDSDisplayID_Touch]] || !fetchDisplayInfo.isCustomSizeRequested) && _cdv->IsSelectedDisplayEnabled(NDSDisplayID_Touch) && (mode == ClientDisplayMode_Touch || mode == ClientDisplayMode_Dual) && (selectedDisplaySource[NDSDisplayID_Main] != selectedDisplaySource[NDSDisplayID_Touch]) };
		
		VideoFilter *vfMain  = _cdv->GetPixelScalerObject(NDSDisplayID_Main);
		VideoFilter *vfTouch = _cdv->GetPixelScalerObject(NDSDisplayID_Touch);
		
		id<MTLCommandBuffer> cb = [[sharedData commandQueue] commandBufferWithUnretainedReferences];
		id<MTLComputeCommandEncoder> cce = [cb computeCommandEncoder];
		
		// Run the video source filters and the pixel scalers
		if (useDeposterize)
		{
			[cce setComputePipelineState:[sharedData deposterizePipeline]];
			
			if (shouldProcessDisplay[NDSDisplayID_Main])
			{
				[cce setTexture:_texDisplayOutput[NDSDisplayID_Main] atIndex:0];
				[cce setTexture:_texDisplaySrcDeposterize[NDSDisplayID_Main][0] atIndex:1];
				[cce dispatchThreadgroups:[sharedData deposterizeThreadGroupsPerGrid]
					threadsPerThreadgroup:[sharedData deposterizeThreadsPerGroup]];
				
				[cce setTexture:_texDisplaySrcDeposterize[NDSDisplayID_Main][0] atIndex:0];
				[cce setTexture:_texDisplaySrcDeposterize[NDSDisplayID_Main][1] atIndex:1];
				[cce dispatchThreadgroups:[sharedData deposterizeThreadGroupsPerGrid]
					threadsPerThreadgroup:[sharedData deposterizeThreadsPerGroup]];
				
				_texDisplayOutput[NDSDisplayID_Main] = _texDisplaySrcDeposterize[NDSDisplayID_Main][1];
			}
			
			if (shouldProcessDisplay[NDSDisplayID_Touch])
			{
				[cce setTexture:_texDisplayOutput[NDSDisplayID_Touch] atIndex:0];
				[cce setTexture:_texDisplaySrcDeposterize[NDSDisplayID_Touch][0] atIndex:1];
				[cce dispatchThreadgroups:[sharedData deposterizeThreadGroupsPerGrid]
					threadsPerThreadgroup:[sharedData deposterizeThreadsPerGroup]];
				
				[cce setTexture:_texDisplaySrcDeposterize[NDSDisplayID_Touch][0] atIndex:0];
				[cce setTexture:_texDisplaySrcDeposterize[NDSDisplayID_Touch][1] atIndex:1];
				[cce dispatchThreadgroups:[sharedData deposterizeThreadGroupsPerGrid]
					threadsPerThreadgroup:[sharedData deposterizeThreadsPerGroup]];
				
				_texDisplayOutput[NDSDisplayID_Touch] = _texDisplaySrcDeposterize[NDSDisplayID_Touch][1];
			}
		}
		
		// Run the pixel scalers. First attempt on the GPU.
		if ( (_cdv->GetPixelScaler() != VideoFilterTypeID_None) && willFilterOnGPU )
		{
			[cce setComputePipelineState:[self pixelScalePipeline]];
			
			if (shouldProcessDisplay[NDSDisplayID_Main])
			{
				[cce setTexture:_texDisplayOutput[NDSDisplayID_Main] atIndex:0];
				[cce setTexture:[self texDisplayPixelScaleMain] atIndex:1];
				[cce setTexture:[sharedData texCurrentHQnxLUT] atIndex:2];
				[cce dispatchThreadgroups:_pixelScalerThreadGroupsPerGrid threadsPerThreadgroup:_pixelScalerThreadsPerGroup];
				
				_texDisplayOutput[NDSDisplayID_Main]  = [self texDisplayPixelScaleMain];
			}
			
			if (shouldProcessDisplay[NDSDisplayID_Touch])
			{
				[cce setTexture:_texDisplayOutput[NDSDisplayID_Touch] atIndex:0];
				[cce setTexture:[self texDisplayPixelScaleTouch] atIndex:1];
				[cce setTexture:[sharedData texCurrentHQnxLUT] atIndex:2];
				[cce dispatchThreadgroups:_pixelScalerThreadGroupsPerGrid threadsPerThreadgroup:_pixelScalerThreadsPerGroup];
				
				_texDisplayOutput[NDSDisplayID_Touch] = [self texDisplayPixelScaleTouch];
			}
		}
		
		[cce endEncoding];
		
		// If the pixel scaler didn't already run on the GPU, then run the pixel scaler on the CPU.
		if ( (_cdv->GetPixelScaler() != VideoFilterTypeID_None) && !willFilterOnGPU )
		{
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
								toBuffer:[self bufCPUFilterSrcMain]
					   destinationOffset:0
				  destinationBytesPerRow:vfMain->GetSrcWidth() * sizeof(uint32_t)
				destinationBytesPerImage:vfMain->GetSrcWidth() * vfMain->GetSrcHeight() * sizeof(uint32_t)];
					
					[bce synchronizeResource:[self bufCPUFilterSrcMain]];
				}
				
				if (shouldProcessDisplay[NDSDisplayID_Touch])
				{
					[bce copyFromTexture:_texDisplaySrcDeposterize[NDSDisplayID_Touch][1]
							 sourceSlice:0
							 sourceLevel:0
							sourceOrigin:MTLOriginMake(0, 0, 0)
							  sourceSize:MTLSizeMake(vfTouch->GetSrcWidth(), vfTouch->GetSrcHeight(), 1)
								toBuffer:[self bufCPUFilterSrcTouch]
					   destinationOffset:0
				  destinationBytesPerRow:vfTouch->GetSrcWidth() * sizeof(uint32_t)
				destinationBytesPerImage:vfTouch->GetSrcWidth() * vfTouch->GetSrcHeight() * sizeof(uint32_t)];
					
					[bce synchronizeResource:[self bufCPUFilterSrcTouch]];
				}
			}
			
			pthread_mutex_lock(_cdv->GetMutexProcessPtr());
			
			if (shouldProcessDisplay[NDSDisplayID_Main])
			{
				vfMain->RunFilter();
			}
			
			if (shouldProcessDisplay[NDSDisplayID_Touch])
			{
				vfTouch->RunFilter();
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
				
				_texDisplayOutput[NDSDisplayID_Main]  = [self texDisplayPixelScaleMain];
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
				
				_texDisplayOutput[NDSDisplayID_Touch] = [self texDisplayPixelScaleTouch];
			}
			
			pthread_mutex_unlock(_cdv->GetMutexProcessPtr());
			
			[bce endEncoding];
		}
		
		[cb commit];
	}
	
	if (selectedDisplaySource[NDSDisplayID_Touch] == selectedDisplaySource[NDSDisplayID_Main])
	{
		_texDisplayOutput[NDSDisplayID_Touch] = _texDisplayOutput[NDSDisplayID_Main];
	}
	
	// Update the texture coordinates
	_cdv->SetScreenTextureCoordinates((float)[_texDisplayOutput[NDSDisplayID_Main]  width], (float)[_texDisplayOutput[NDSDisplayID_Main]  height],
									  (float)[_texDisplayOutput[NDSDisplayID_Touch] width], (float)[_texDisplayOutput[NDSDisplayID_Touch] height],
									  (float *)[_displayTexCoordBuffer contents]);
	[_displayTexCoordBuffer didModifyRange:NSMakeRange(0, sizeof(float) * (4 * 8))];
}

- (void) renderToDrawable
{
	NSAutoreleasePool *renderAutoreleasePool = [[NSAutoreleasePool alloc] init];
	
	id<CAMetalDrawable> drawable = [self nextDrawable];
	id<MTLTexture> texture = [drawable texture];
	if (texture == nil)
	{
		[renderAutoreleasePool release];
		return;
	}
	
	const NDSDisplayInfo &displayInfo = _cdv->GetEmuDisplayInfo();
	
	dispatch_semaphore_wait(availableResources, DISPATCH_TIME_FOREVER);
	
	[[self colorAttachment0Desc] setTexture:texture];
	
	id<MTLCommandBuffer> cb = [[sharedData commandQueue] commandBufferWithUnretainedReferences];
	id<MTLRenderCommandEncoder> ce = [cb renderCommandEncoderWithDescriptor:_outputRenderPassDesc];
	
	// Set up the view properties.
	BOOL didChangeViewProperties = NO;
	
	if ([self needsViewportUpdate])
	{
		MTLViewport newViewport;
		newViewport.originX = 0.0;
		newViewport.originY = 0.0;
		newViewport.width  = _cdv->GetViewProperties().clientWidth;
		newViewport.height = _cdv->GetViewProperties().clientHeight;
		newViewport.znear = 0.0;
		newViewport.zfar = 1.0;
		[ce setViewport:newViewport];
		
		DisplayViewShaderProperties *viewProps = (DisplayViewShaderProperties *)[_cdvPropertiesBuffer contents];
		viewProps->width    = _cdv->GetViewProperties().clientWidth;
		viewProps->height   = _cdv->GetViewProperties().clientHeight;
		didChangeViewProperties = YES;
		
		[self setNeedsViewportUpdate:NO];
	}
	
	if ([self needsRotationScaleUpdate])
	{
		DisplayViewShaderProperties *viewProps = (DisplayViewShaderProperties *)[_cdvPropertiesBuffer contents];
		viewProps->rotation            = _cdv->GetViewProperties().rotation;
		viewProps->viewScale           = _cdv->GetViewProperties().viewScale;
		viewProps->lowerHUDMipMapLevel = ( ((float)HUD_TEXTBOX_BASE_SCALE * _cdv->GetHUDObjectScale() / _cdv->GetScaleFactor()) >= (2.0/3.0) ) ? 0 : 1;
		didChangeViewProperties = YES;
		
		[self setNeedsRotationScaleUpdate:NO];
	}
	
	if (didChangeViewProperties)
	{
		[_cdvPropertiesBuffer didModifyRange:NSMakeRange(0, sizeof(DisplayViewShaderProperties))];
	}
	
	// Draw the NDS displays.
	if (displayInfo.pixelBytes != 0)
	{
		if ([self needsScreenVerticesUpdate])
		{
			_cdv->SetScreenVertices((float *)[_displayVtxPositionBuffer contents]);
			[_displayVtxPositionBuffer didModifyRange:NSMakeRange(0, sizeof(float) * (4 * 8))];
			
			[self setNeedsScreenVerticesUpdate:NO];
		}
		
		[ce setRenderPipelineState:[self displayOutputPipeline]];
		[ce setVertexBuffer:_displayVtxPositionBuffer offset:0 atIndex:0];
		[ce setVertexBuffer:_displayTexCoordBuffer offset:0 atIndex:1];
		[ce setVertexBuffer:_cdvPropertiesBuffer offset:0 atIndex:2];
		
		switch (_cdv->GetViewProperties().mode)
		{
			case ClientDisplayMode_Main:
			{
				if (_cdv->IsSelectedDisplayEnabled(NDSDisplayID_Main))
				{
					[ce setFragmentTexture:_texDisplayOutput[NDSDisplayID_Main] atIndex:0];
					[ce drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
				}
				break;
			}
				
			case ClientDisplayMode_Touch:
			{
				if (_cdv->IsSelectedDisplayEnabled(NDSDisplayID_Touch))
				{
					[ce setFragmentTexture:_texDisplayOutput[NDSDisplayID_Touch] atIndex:0];
					[ce drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:4 vertexCount:4];
				}
				break;
			}
				
			case ClientDisplayMode_Dual:
			{
				const NDSDisplayID majorDisplayID = (_cdv->GetViewProperties().order == ClientDisplayOrder_MainFirst) ? NDSDisplayID_Main : NDSDisplayID_Touch;
				const size_t majorDisplayVtx = (_cdv->GetViewProperties().order == ClientDisplayOrder_MainFirst) ? 8 : 12;
				
				switch (_cdv->GetViewProperties().layout)
				{
					case ClientDisplayLayout_Hybrid_2_1:
					case ClientDisplayLayout_Hybrid_16_9:
					case ClientDisplayLayout_Hybrid_16_10:
					{
						if (_cdv->IsSelectedDisplayEnabled(majorDisplayID))
						{
							[ce setFragmentTexture:_texDisplayOutput[majorDisplayID] atIndex:0];
							[ce drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:majorDisplayVtx vertexCount:4];
						}
						break;
					}
						
					default:
						break;
				}
				
				if (_cdv->IsSelectedDisplayEnabled(NDSDisplayID_Main))
				{
					[ce setFragmentTexture:_texDisplayOutput[NDSDisplayID_Main] atIndex:0];
					[ce drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
				}
				
				if (_cdv->IsSelectedDisplayEnabled(NDSDisplayID_Touch))
				{
					[ce setFragmentTexture:_texDisplayOutput[NDSDisplayID_Touch] atIndex:0];
					[ce drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:4 vertexCount:4];
				}
			}
				
			default:
				break;
		}
	}
	
	// Draw the HUD.
	const size_t hudLength = _cdv->GetHUDString().length();
	if ( _cdv->GetHUDVisibility() && (hudLength > 1) && ([self texHUDCharMap] != nil) )
	{
		if (_cdv->HUDNeedsUpdate())
		{
			_cdv->SetHUDPositionVertices((float)_cdv->GetViewProperties().clientWidth, (float)_cdv->GetViewProperties().clientHeight, (float *)[_hudVtxPositionBuffer contents]);
			[_hudVtxPositionBuffer didModifyRange:NSMakeRange(0, sizeof(float) * hudLength * 8)];
			
			_cdv->SetHUDColorVertices((uint32_t *)[_hudVtxColorBuffer contents]);
			[_hudVtxColorBuffer didModifyRange:NSMakeRange(0, sizeof(uint32_t) * hudLength * 4)];
			
			_cdv->SetHUDTextureCoordinates((float *)[_hudTexCoordBuffer contents]);
			[_hudTexCoordBuffer didModifyRange:NSMakeRange(0, sizeof(float) * hudLength * 8)];
			
			_cdv->ClearHUDNeedsUpdate();
		}
		
		[ce setRenderPipelineState:[sharedData hudPipeline]];
		[ce setVertexBuffer:_hudVtxPositionBuffer offset:0 atIndex:0];
		[ce setVertexBuffer:_hudVtxColorBuffer offset:0 atIndex:1];
		[ce setVertexBuffer:_hudTexCoordBuffer offset:0 atIndex:2];
		[ce setVertexBuffer:_cdvPropertiesBuffer offset:0 atIndex:3];
		[ce setFragmentTexture:[self texHUDCharMap] atIndex:0];
		
		// First, draw the backing text box.
		[ce setFragmentSamplerState:[sharedData samplerHUDBox] atIndex:0];
		[ce drawIndexedPrimitives:MTLPrimitiveTypeTriangle
					   indexCount:6
						indexType:MTLIndexTypeUInt16
					  indexBuffer:[sharedData hudIndexBuffer]
				indexBufferOffset:0];
		
		// Next, draw each character inside the box.
		[ce setFragmentSamplerState:[sharedData samplerHUDText] atIndex:0];
		[ce drawIndexedPrimitives:MTLPrimitiveTypeTriangle
					   indexCount:(hudLength - 1) * 6
						indexType:MTLIndexTypeUInt16
					  indexBuffer:[sharedData hudIndexBuffer]
				indexBufferOffset:6 * sizeof(uint16_t)];
	}
	
	[ce endEncoding];
	
	[cb presentDrawable:drawable];
	[cb addCompletedHandler:^(id<MTLCommandBuffer> block) {
		dispatch_semaphore_signal(availableResources);
	}];
	
	[cb commit];
	
	[renderAutoreleasePool release];
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
	[(MetalDisplayViewSharedData *)this->_clientData setFetchBuffersWithDisplayInfo:currentDisplayInfo];
}

void MacMetalFetchObject::FetchFromBufferIndex(const u8 index)
{
	MacClientSharedObject *sharedViewObject = (MacClientSharedObject *)this->_clientData;
	this->_useDirectToCPUFilterPipeline = ([sharedViewObject numberViewsUsingDirectToCPUFiltering] > 0);
	
	[(MetalDisplayViewSharedData *)this->_clientData fetchFromBufferIndex:index];
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

MacMetalDisplayView::MacMetalDisplayView()
{
	_allowViewUpdates = false;
	_canFilterOnGPU = true;
	_filtersPreferGPU = true;
	_willFilterOnGPU = true;
	
	_mutexProcessPtr = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(_mutexProcessPtr, NULL);
}

MacMetalDisplayView::~MacMetalDisplayView()
{
	pthread_mutex_destroy(this->_mutexProcessPtr);
	free(this->_mutexProcessPtr);
}

void MacMetalDisplayView::_UpdateNormalSize()
{
	[(DisplayViewMetalLayer *)this->GetFrontendLayer() setNeedsScreenVerticesUpdate:YES];
}

void MacMetalDisplayView::_UpdateOrder()
{
	[(DisplayViewMetalLayer *)this->GetFrontendLayer() setNeedsScreenVerticesUpdate:YES];
}

void MacMetalDisplayView::_UpdateRotation()
{
	[(DisplayViewMetalLayer *)this->GetFrontendLayer() setNeedsRotationScaleUpdate:YES];
}

void MacMetalDisplayView::_UpdateClientSize()
{
	[(DisplayViewMetalLayer *)this->GetFrontendLayer() setNeedsViewportUpdate:YES];
	[(DisplayViewMetalLayer *)this->GetFrontendLayer() setNeedsHUDVerticesUpdate:YES];
	this->ClientDisplay3DView::_UpdateClientSize();
}

void MacMetalDisplayView::_UpdateViewScale()
{
	this->ClientDisplay3DView::_UpdateViewScale();
	[(DisplayViewMetalLayer *)this->GetFrontendLayer() setNeedsRotationScaleUpdate:YES];
}

void MacMetalDisplayView::_LoadNativeDisplayByID(const NDSDisplayID displayID)
{
	if ((this->GetPixelScaler() != VideoFilterTypeID_None) && !this->WillFilterOnGPU() && !this->GetSourceDeposterize())
	{
		MacMetalFetchObject &fetchObjMutable = (MacMetalFetchObject &)this->GetFetchObject();
		VideoFilter *vf = this->GetPixelScalerObject(displayID);
		
		fetchObjMutable.CopyFromSrcClone(vf->GetSrcBufferPtr(), displayID, this->GetEmuDisplayInfo().bufferIndex);
	}
}

void MacMetalDisplayView::_ResizeCPUPixelScaler(const VideoFilterTypeID filterID)
{
	this->ClientDisplay3DView::_ResizeCPUPixelScaler(filterID);
	[(DisplayViewMetalLayer *)this->GetFrontendLayer() resizeCPUPixelScalerUsingFilterID:filterID];
}

pthread_mutex_t* MacMetalDisplayView::GetMutexProcessPtr() const
{
	return this->_mutexProcessPtr;
}

void MacMetalDisplayView::Init()
{
	[(DisplayViewMetalLayer *)this->GetFrontendLayer() setupLayer];
}

void MacMetalDisplayView::CopyHUDFont(const FT_Face &fontFace, const size_t glyphSize, const size_t glyphTileSize, GlyphInfo *glyphInfo)
{
	[(DisplayViewMetalLayer *)this->GetFrontendLayer() copyHUDFontUsingFace:fontFace size:glyphSize tileSize:glyphTileSize info:glyphInfo];
}

// NDS screen filters
void MacMetalDisplayView::SetPixelScaler(const VideoFilterTypeID filterID)
{
	pthread_mutex_lock(this->_mutexProcessPtr);
	
	this->ClientDisplay3DView::SetPixelScaler(filterID);
	[(DisplayViewMetalLayer *)this->GetFrontendLayer() setPixelScaler:this->_pixelScaler];
	this->_willFilterOnGPU = (this->GetFiltersPreferGPU()) ? ([(DisplayViewMetalLayer *)this->GetFrontendLayer() pixelScalePipeline] != nil) : false;
	
	pthread_mutex_unlock(this->_mutexProcessPtr);
}

void MacMetalDisplayView::SetOutputFilter(const OutputFilterTypeID filterID)
{
	this->ClientDisplay3DView::SetOutputFilter(filterID);
	[(DisplayViewMetalLayer *)this->GetFrontendLayer() setOutputFilter:filterID];
}

void MacMetalDisplayView::SetFiltersPreferGPU(const bool preferGPU)
{
	pthread_mutex_lock(this->_mutexProcessPtr);
	
	this->_filtersPreferGPU = preferGPU;
	this->_willFilterOnGPU = (preferGPU) ? ([(DisplayViewMetalLayer *)this->GetFrontendLayer() pixelScalePipeline] != nil) : false;
	
	pthread_mutex_unlock(this->_mutexProcessPtr);
}

// NDS GPU interface
void MacMetalDisplayView::ProcessDisplays()
{
	[(DisplayViewMetalLayer *)this->GetFrontendLayer() processDisplays];
}

void MacMetalDisplayView::UpdateView()
{
	if (this->_allowViewUpdates)
	{
		[(DisplayViewMetalLayer *)this->GetFrontendLayer() renderToDrawable];
	}
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
