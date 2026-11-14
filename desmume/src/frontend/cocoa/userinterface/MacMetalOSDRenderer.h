/*
	Copyright (C) 2026 DeSmuME team

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

#ifndef _MAC_METAL_OSD_RENDERER_H_
#define _MAC_METAL_OSD_RENDERER_H_

#import <Metal/Metal.h>
#include <stdint.h>

#define METAL_OSD_BUFFER_SLOTS 8

// Metal equivalent of WGPUOSDRenderer.  It never creates a device: the view
// hands it the device/library it already owns.  Each renderIntoCommandBuffer:
// encodes an offscreen pass into a 256x384 premultiplied BGRA8 texture; the
// view then composites that texture over the two NDS screens.
@interface MacMetalOSDRenderer : NSObject
{
@private
	id<MTLDevice> _device;

	id<MTLRenderPipelineState> _shapePipeline;
	id<MTLRenderPipelineState> _glyphPipeline;
	id<MTLRenderPipelineState> _blitPipeline;
	id<MTLRenderPipelineState> _overlayBGRAPipeline;
	id<MTLRenderPipelineState> _overlayRGBAPipeline;

	id<MTLTexture> _hudTexture;    // 256x384 BGRA8Unorm, premultiplied
	id<MTLTexture> _atlasTexture;  // R8Unorm glyph coverage atlas
	id<MTLTexture> _cpuTextures[METAL_OSD_BUFFER_SLOTS]; // CPU fallback layer, one per ring slot

	MTLRenderPassDescriptor *_renderPassDesc;

	// Ring buffers: an in-flight command buffer may still be reading a slot.
	id<MTLBuffer> _instanceBuffers[METAL_OSD_BUFFER_SLOTS];
	id<MTLBuffer> _glyphBuffers[METAL_OSD_BUFFER_SLOTS];
	NSUInteger _instanceCapacities[METAL_OSD_BUFFER_SLOTS];
	NSUInteger _glyphCapacities[METAL_OSD_BUFFER_SLOTS];
	NSUInteger _bufferSlot;

	// Normalized UVs for the four screen quads in the view's vertex buffer.
	float _overlayTexCoords[32];

	void *_atlasState;             // AtlasState* (C++ only, see .mm)

	uint64_t _lastGeneration;
	BOOL _hudValid;
	BOOL _hasContent;
	BOOL _ready;
}

- (id)initWithDevice:(id<MTLDevice>)device library:(id<MTLLibrary>)library;
- (BOOL)isReady;

// Encodes the offscreen OSD pass.  Returns YES when hudTexture holds content
// that should be composited by the caller.
- (BOOL)renderIntoCommandBuffer:(id<MTLCommandBuffer>)cb;

- (id<MTLTexture>)hudTexture;
- (const float *)overlayTexCoords;
- (id<MTLRenderPipelineState>)overlayPipelineForFormat:(MTLPixelFormat)format;

@end

#endif // _MAC_METAL_OSD_RENDERER_H_
