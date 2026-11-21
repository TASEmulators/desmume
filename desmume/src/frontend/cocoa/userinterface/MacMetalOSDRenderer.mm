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

#import "MacMetalOSDRenderer.h"
#import "MacMetalOSDDraw.h"

#include <math.h>
#include <string.h>
#include <algorithm>
#include <vector>

// ---------------------------------------------------------------------------
// GPU-side PODs.  These mirror OSDShapeInstance / OSDGlyphIn in
// MacMetalOSDShaders.metal; the asserts are the ABI contract.
// ---------------------------------------------------------------------------

struct MetalOSDInstance
{
	float bbox[4];
	float shape[4];
	float params[4];
	float fill0[4];
	float fill1[4];
	float line0[4];
	float line1[4];
	float fillGrad[4];
	float lineGrad[4];
	float flags[4];
	float pts[128];      // 64 * float2
};
static_assert(sizeof(MetalOSDInstance) == 672, "MetalOSDInstance must match the MSL struct");

struct MetalOSDGlyph
{
	float posSize[4];
	float uv[4];
	float color[4];
};
static_assert(sizeof(MetalOSDGlyph) == 48, "MetalOSDGlyph must match the MSL vertex layout");

struct MetalOSDUniforms
{
	float size[4];       // target width, height, 0, 0
};
static_assert(sizeof(MetalOSDUniforms) == 16, "MetalOSDUniforms must be 16 bytes");

// ---------------------------------------------------------------------------
// AGG raster-font parsing and atlas metadata
// ---------------------------------------------------------------------------

struct MetalOSDAtlasGlyph
{
	float u0, v0, u1, v1;
	int   w, h;
};

struct MetalOSDAtlasFont
{
	const uint8_t *data;
	uint32_t size;
	int height;
	int base;
	int first;
	int count;
	std::vector<MetalOSDAtlasGlyph> glyphs;
};

struct MetalOSDAtlasState
{
	std::vector<MetalOSDAtlasFont> fonts;
};

static uint32_t NextPowerOfTwo(uint32_t v)
{
	uint32_t p = 1;
	while ((p < v) && (p < (1u << 31)))
	{
		p <<= 1;
	}
	return p;
}

// Same layout as agg::glyph_raster_bin: height, base, first char, count, a
// little-endian uint16 offset table, then width + MSB-first 1bpp rows.
static int FontCharCount(const uint8_t *f, uint32_t size)
{
	if ((f == NULL) || (size < 4)) return 0;
	const int count = f[3] ? (int)f[3] : 256;
	if ((count <= 0) || (count > 256)) return 0;
	if ((size_t)(4 + count * 2) > (size_t)size) return 0;
	return count;
}

static const uint8_t *FontGlyphData(const uint8_t *f, uint32_t size, int index, int height)
{
	const int count = FontCharCount(f, size);
	if ((count <= 0) || (index < 0) || (index >= count) || (height <= 0) || (height > 64)) return NULL;

	const unsigned off = (unsigned)f[4 + index * 2] | ((unsigned)f[5 + index * 2] << 8);
	const size_t glyphOffset = (size_t)4 + (size_t)count * 2 + (size_t)off;
	if ((glyphOffset >= (size_t)size) || (glyphOffset + 1 > (size_t)size)) return NULL;

	const int width = (int)f[glyphOffset];
	if ((width <= 0) || (width > 1024)) return NULL;
	const size_t needed = 1 + (size_t)((width + 7) >> 3) * (size_t)height;
	if (glyphOffset + needed > (size_t)size) return NULL;

	return f + glyphOffset;
}

static void UnpackColor(uint32_t c, float out[4])
{
	out[0] = (float)((c >> 24) & 0xFF) / 255.0f;
	out[1] = (float)((c >> 16) & 0xFF) / 255.0f;
	out[2] = (float)((c >>  8) & 0xFF) / 255.0f;
	out[3] = (float)( c        & 0xFF) / 255.0f;
}

// ---------------------------------------------------------------------------
// Pipeline helpers
// ---------------------------------------------------------------------------

static void ConfigureOSDBlend(MTLRenderPipelineDescriptor *desc)
{
	MTLRenderPipelineColorAttachmentDescriptor *ca = [[desc colorAttachments] objectAtIndexedSubscript:0];
	[ca setBlendingEnabled:YES];
	[ca setRgbBlendOperation:MTLBlendOperationAdd];
	[ca setAlphaBlendOperation:MTLBlendOperationAdd];
	[ca setSourceRGBBlendFactor:MTLBlendFactorOne];                      // premultiplied source-over
	[ca setDestinationRGBBlendFactor:MTLBlendFactorOneMinusSourceAlpha];
	[ca setSourceAlphaBlendFactor:MTLBlendFactorOne];
	[ca setDestinationAlphaBlendFactor:MTLBlendFactorOneMinusSourceAlpha];
	[ca setPixelFormat:MTLPixelFormatBGRA8Unorm];
}

static void ConfigureOverlayBlend(MTLRenderPipelineDescriptor *desc, MTLPixelFormat format)
{
	MTLRenderPipelineColorAttachmentDescriptor *ca = [[desc colorAttachments] objectAtIndexedSubscript:0];
	[ca setBlendingEnabled:YES];
	[ca setRgbBlendOperation:MTLBlendOperationAdd];
	[ca setAlphaBlendOperation:MTLBlendOperationAdd];
	[ca setSourceRGBBlendFactor:MTLBlendFactorOne];
	[ca setDestinationRGBBlendFactor:MTLBlendFactorOneMinusSourceAlpha];
	[ca setSourceAlphaBlendFactor:MTLBlendFactorZero];                   // keep the opaque destination alpha
	[ca setDestinationAlphaBlendFactor:MTLBlendFactorOne];
	[ca setPixelFormat:format];
}

static id<MTLRenderPipelineState> MakePipeline(id<MTLDevice> device,
                                               id<MTLLibrary> library,
                                               NSString *vertexName,
                                               NSString *fragmentName,
                                               MTLVertexDescriptor *vertexDesc,
                                               void (*configure)(MTLRenderPipelineDescriptor *),
                                               MTLPixelFormat format)
{
	MTLRenderPipelineDescriptor *desc = [[MTLRenderPipelineDescriptor alloc] init];
	id<MTLFunction> vs = [library newFunctionWithName:vertexName];
	id<MTLFunction> fs = [library newFunctionWithName:fragmentName];

	if ((vs == nil) || (fs == nil))
	{
		NSLog(@"DeSmuME Metal OSD: missing shader function %@ / %@", vertexName, fragmentName);
		[vs release];
		[fs release];
		[desc release];
		return nil;
	}

	[desc setVertexFunction:vs];
	[desc setFragmentFunction:fs];
	if (vertexDesc != nil)
	{
		[desc setVertexDescriptor:vertexDesc];
	}

	// Two configure signatures; the composite pass needs the drawable format.
	if (configure != NULL)
	{
		configure(desc);
	}
	else
	{
		ConfigureOverlayBlend(desc, format);
	}

	id<MTLRenderPipelineState> pso = [device newRenderPipelineStateWithDescriptor:desc error:nil];

	[vs release];
	[fs release];
	[desc release];
	return pso;
}

// ---------------------------------------------------------------------------
// MacMetalOSDRenderer
// ---------------------------------------------------------------------------

@implementation MacMetalOSDRenderer

- (id)initWithDevice:(id<MTLDevice>)device library:(id<MTLLibrary>)library
{
	self = [super init];
	if (self == nil) return nil;

	memset(_instanceCapacities, 0, sizeof(_instanceCapacities));
	memset(_glyphCapacities,   0, sizeof(_glyphCapacities));
	for (int i = 0; i < METAL_OSD_BUFFER_SLOTS; i++)
	{
		_instanceBuffers[i] = nil;
		_glyphBuffers[i]    = nil;
		_cpuTextures[i]     = nil;
	}

	_device = [device retain];
	_bufferSlot = 0;
	_lastGeneration = 0;
	_hudValid = NO;
	_hasContent = NO;
	_ready = NO;
	_atlasState = NULL;
	_renderPassDesc = nil;

	// The four screen quads in the view's vertex buffer are:
	//   0: main, 1: touch, 2: main (hybrid major), 3: touch (hybrid major).
	// Map the upper half of the 256x384 HUD to main and the lower half to touch.
	static const float kQuadUV[4][8] =
	{
		{ 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.5f, 1.0f, 0.5f },
		{ 0.0f, 0.5f, 1.0f, 0.5f, 0.0f, 1.0f, 1.0f, 1.0f },
		{ 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.5f, 1.0f, 0.5f },
		{ 0.0f, 0.5f, 1.0f, 0.5f, 0.0f, 1.0f, 1.0f, 1.0f }
	};
	memcpy(_overlayTexCoords, kQuadUV, sizeof(kQuadUV));

	if ((device == nil) || (library == nil))
	{
		return self;
	}

	// Offscreen pipelines (fixed BGRA8Unorm target).
	_shapePipeline = MakePipeline(device, library, @"osd_shape_vertex", @"osd_shape_fragment",
	                              nil, &ConfigureOSDBlend, MTLPixelFormatBGRA8Unorm);
	_blitPipeline  = MakePipeline(device, library, @"osd_blit_vertex",  @"osd_blit_fragment",
	                              nil, &ConfigureOSDBlend, MTLPixelFormatBGRA8Unorm);

	MTLVertexDescriptor *glyphVD = [MTLVertexDescriptor vertexDescriptor];
	for (NSUInteger i = 0; i < 3; i++)
	{
		MTLVertexAttributeDescriptor *attr = [[glyphVD attributes] objectAtIndexedSubscript:i];
		[attr setFormat:MTLVertexFormatFloat4];
		[attr setOffset:i * 16];
		[attr setBufferIndex:0];
	}
	MTLVertexBufferLayoutDescriptor *layout = [[glyphVD layouts] objectAtIndexedSubscript:0];
	[layout setStride:sizeof(MetalOSDGlyph)];
	[layout setStepFunction:MTLVertexStepFunctionPerInstance];
	[layout setStepRate:1];

	_glyphPipeline = MakePipeline(device, library, @"osd_glyph_vertex", @"osd_glyph_fragment",
	                              glyphVD, &ConfigureOSDBlend, MTLPixelFormatBGRA8Unorm);

	// Composite pipelines, one per possible drawable format.
	_overlayBGRAPipeline = MakePipeline(device, library, @"osd_overlay_vertex", @"osd_overlay_fragment",
	                                    nil, NULL, MTLPixelFormatBGRA8Unorm);
	_overlayRGBAPipeline = MakePipeline(device, library, @"osd_overlay_vertex", @"osd_overlay_fragment",
	                                    nil, NULL, MTLPixelFormatRGBA8Unorm);

	if ((_shapePipeline == nil) || (_glyphPipeline == nil) || (_blitPipeline == nil) ||
	    (_overlayBGRAPipeline == nil) || (_overlayRGBAPipeline == nil))
	{
		NSLog(@"DeSmuME Metal OSD: pipeline creation failed; the OSD will be skipped.");
		return self;
	}

	// Offscreen HUD target.
	MTLTextureDescriptor *hudDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
	                                                                                width:METAL_OSD_WIDTH
	                                                                               height:METAL_OSD_HEIGHT
	                                                                            mipmapped:NO];
	[hudDesc setStorageMode:MTLStorageModePrivate];
	[hudDesc setUsage:MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead];
	_hudTexture = [device newTextureWithDescriptor:hudDesc];

	if (_hudTexture == nil)
	{
		NSLog(@"DeSmuME Metal OSD: texture creation failed; the OSD will be skipped.");
		return self;
	}

	_renderPassDesc = [[MTLRenderPassDescriptor renderPassDescriptor] retain];
	MTLRenderPassColorAttachmentDescriptor *ca = [[_renderPassDesc colorAttachments] objectAtIndexedSubscript:0];
	[ca setTexture:_hudTexture];
	[ca setLoadAction:MTLLoadActionClear];
	[ca setStoreAction:MTLStoreActionStore];
	[ca setClearColor:MTLClearColorMake(0.0, 0.0, 0.0, 0.0)];

	_atlasState = new MetalOSDAtlasState();
	if (![self buildAtlas])
	{
		NSLog(@"DeSmuME Metal OSD: glyph atlas creation failed; text will be skipped.");
		return self;
	}

	_ready = YES;
	return self;
}

- (void)dealloc
{
	[_device release];

	[_shapePipeline release];
	[_glyphPipeline release];
	[_blitPipeline release];
	[_overlayBGRAPipeline release];
	[_overlayRGBAPipeline release];

	[_hudTexture release];
	[_atlasTexture release];
	[_renderPassDesc release];

	for (int i = 0; i < METAL_OSD_BUFFER_SLOTS; i++)
	{
		[_instanceBuffers[i] release];
		[_glyphBuffers[i] release];
		[_cpuTextures[i] release];
	}

	if (_atlasState != NULL)
	{
		delete (MetalOSDAtlasState *)_atlasState;
	}

	[super dealloc];
}

- (BOOL)isReady { return _ready; }
- (id<MTLTexture>)hudTexture { return _hudTexture; }
- (const float *)overlayTexCoords { return _overlayTexCoords; }

- (id<MTLRenderPipelineState>)overlayPipelineForFormat:(MTLPixelFormat)format
{
	return (format == MTLPixelFormatRGBA8Unorm) ? _overlayRGBAPipeline : _overlayBGRAPipeline;
}

// ---------------------------------------------------------------------------
// Glyph atlas
// ---------------------------------------------------------------------------

- (BOOL)buildAtlas
{
	if (_atlasState == NULL) return NO;

	MetalOSDAtlasState &state = *(MetalOSDAtlasState *)_atlasState;
	state.fonts.clear();

	const int atlasWidth = 1024;
	struct Placement { int font; int glyph; int x; int y; };
	std::vector<Placement> placements;

	state.fonts.resize((size_t)MacMetalOSDEmbeddedFontCount);
	int penX = 0;
	int penY = 0;
	int rowHeight = 0;

	for (int fi = 0; fi < MacMetalOSDEmbeddedFontCount; fi++)
	{
		MetalOSDAtlasFont &af = state.fonts[(size_t)fi];
		const uint8_t *data = MacMetalOSDEmbeddedFonts[fi].data;
		const uint32_t size = MacMetalOSDEmbeddedFonts[fi].size;

		af.data   = data;
		af.size   = size;
		af.height = 0;
		af.base   = 0;
		af.first  = 0;
		af.count  = 0;
		af.glyphs.clear();

		const int height = (size >= 1) ? (int)data[0] : 0;
		const int count  = FontCharCount(data, size);
		if ((height <= 0) || (height > 64) || (count <= 0) || (size < 4)) continue;

		af.height = height;
		af.base   = (int)data[1];
		af.first  = (int)data[2];
		af.count  = count;
		af.glyphs.resize((size_t)count);

		for (int i = 0; i < count; i++)
		{
			MetalOSDAtlasGlyph &g = af.glyphs[(size_t)i];
			memset(&g, 0, sizeof(g));
			g.h = height;   // all glyphs have the font's height

			const uint8_t *glyph = FontGlyphData(data, size, i, height);
			if (glyph == NULL) continue;

			const int w = (int)glyph[0];
			if ((w <= 0) || (w > atlasWidth)) continue;
			g.w = w;

			if (penX + w + 1 > atlasWidth)
			{
				penX = 0;
				penY += rowHeight + 1;
				rowHeight = 0;
			}

			Placement p = { fi, i, penX, penY };
			placements.push_back(p);
			penX += w + 1;                       // 1px gutter on the right
			rowHeight = std::max(rowHeight, height);
		}
	}

	const int usedHeight  = std::max(penY + rowHeight, 1);
	const int atlasHeight = (int)NextPowerOfTwo((uint32_t)usedHeight);
	std::vector<uint8_t> pixels((size_t)atlasWidth * (size_t)atlasHeight, 0);

	for (size_t pi = 0; pi < placements.size(); pi++)
	{
		const Placement &p = placements[pi];
		const MetalOSDAtlasFont &af = state.fonts[(size_t)p.font];
		const MetalOSDAtlasGlyph &g = af.glyphs[(size_t)p.glyph];
		const uint8_t *glyph = FontGlyphData(af.data, af.size, p.glyph, af.height);
		if (glyph == NULL) continue;

		const int byteWidth = (g.w + 7) >> 3;
		const uint8_t *bits = glyph + 1;
		for (int row = 0; row < g.h; row++)
		{
			uint8_t *dst = pixels.data() + (size_t)(p.y + row) * atlasWidth + p.x;
			const uint8_t *src = bits + (size_t)row * byteWidth;
			for (int col = 0; col < g.w; col++)
			{
				dst[col] = (src[col >> 3] & (0x80u >> (col & 7))) ? 255 : 0;
			}
		}
	}

	for (size_t pi = 0; pi < placements.size(); pi++)
	{
		const Placement &p = placements[pi];
		MetalOSDAtlasGlyph &g = state.fonts[(size_t)p.font].glyphs[(size_t)p.glyph];
		g.u0 = (float)p.x            / (float)atlasWidth;
		g.v0 = (float)p.y            / (float)atlasHeight;
		g.u1 = (float)(p.x + g.w)    / (float)atlasWidth;
		g.v1 = (float)(p.y + g.h)    / (float)atlasHeight;
	}

	MTLTextureDescriptor *atlasDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Unorm
	                                                                                   width:atlasWidth
	                                                                                  height:atlasHeight
	                                                                               mipmapped:NO];
	[atlasDesc setStorageMode:MTLStorageModeShared];
	[atlasDesc setUsage:MTLTextureUsageShaderRead];

	id<MTLTexture> newAtlas = [_device newTextureWithDescriptor:atlasDesc];
	if (newAtlas == nil) return NO;

	[newAtlas replaceRegion:MTLRegionMake2D(0, 0, atlasWidth, atlasHeight)
	            mipmapLevel:0
	              withBytes:pixels.data()
	            bytesPerRow:atlasWidth];

	[_atlasTexture release];
	_atlasTexture = newAtlas;
	return YES;
}

// ---------------------------------------------------------------------------
// Instance / glyph packing (direct translation of wgpu_draw.cpp)
// ---------------------------------------------------------------------------

static void PackInstance(const MetalOSDShape &s, const MetalOSDPoint *points, int pointCount,
                         MetalOSDInstance &out)
{
	memset(&out, 0, sizeof(out));

	const double strokeWidth  = (s.lineWidth > 0.0) ? s.lineWidth : 0.0;
	const float  strokeExpand = (s.hasStroke != 0) ? (float)(strokeWidth * 0.5) : 0.0f;
	float minX, minY, maxX, maxY;

	switch (s.type)
	{
		case MetalOSDShape_Ellipse:
		{
			const float cx = (float)s.x0, cy = (float)s.y0;
			const float rx = (float)fabs(s.x1), ry = (float)fabs(s.y1);
			out.shape[0] = cx; out.shape[1] = cy; out.shape[2] = rx; out.shape[3] = ry;
			minX = cx - rx; minY = cy - ry; maxX = cx + rx; maxY = cy + ry;
			break;
		}
		case MetalOSDShape_Line:
		{
			const float x0 = (float)s.x0, y0 = (float)s.y0, x1 = (float)s.x1, y1 = (float)s.y1;
			out.shape[0] = x0; out.shape[1] = y0; out.shape[2] = x1; out.shape[3] = y1;
			minX = std::min(x0, x1); minY = std::min(y0, y1);
			maxX = std::max(x0, x1); maxY = std::max(y0, y1);
			break;
		}
		case MetalOSDShape_Polygon:
		case MetalOSDShape_Polyline:
		{
			minX = minY =  3.4e38f;
			maxX = maxY = -3.4e38f;
			uint32_t count = std::min<uint32_t>(s.pointCount, METAL_OSD_MAX_SHAPE_POINTS);
			if ((points == NULL) || (s.pointOffset >= (uint32_t)pointCount))
			{
				count = 0;
			}
			else if (count > (uint32_t)pointCount - s.pointOffset)
			{
				count = (uint32_t)pointCount - s.pointOffset;
			}
			for (uint32_t i = 0; i < count; i++)
			{
				const MetalOSDPoint &p = points[s.pointOffset + i];
				out.pts[i * 2 + 0] = p.x;
				out.pts[i * 2 + 1] = p.y;
				minX = std::min(minX, p.x); minY = std::min(minY, p.y);
				maxX = std::max(maxX, p.x); maxY = std::max(maxY, p.y);
			}
			if (count == 0) { minX = minY = maxX = maxY = 0.0f; }
			out.params[3] = (float)count;
			break;
		}
		default:  // Rectangle and RoundedRect
		{
			const float x0 = (float)std::min(s.x0, s.x1), y0 = (float)std::min(s.y0, s.y1);
			const float x1 = (float)std::max(s.x0, s.x1), y1 = (float)std::max(s.y0, s.y1);
			const float hx = (x1 - x0) * 0.5f, hy = (y1 - y0) * 0.5f;
			out.shape[0] = x0 + hx; out.shape[1] = y0 + hy;
			out.shape[2] = hx;      out.shape[3] = hy;
			out.params[0] = (float)std::min((double)std::min(hx, hy), s.radius);
			minX = x0; minY = y0; maxX = x1; maxY = y1;
			break;
		}
	}

	// Expand by half the stroke plus one pixel for AA coverage.
	minX -= strokeExpand + 1.0f; minY -= strokeExpand + 1.0f;
	maxX += strokeExpand + 1.0f; maxY += strokeExpand + 1.0f;
	out.bbox[0] = minX; out.bbox[1] = minY; out.bbox[2] = maxX; out.bbox[3] = maxY;

	out.params[1] = (float)strokeWidth;
	out.params[2] = (float)s.type;

	UnpackColor(s.fill0, out.fill0);
	UnpackColor(s.fill1, out.fill1);
	UnpackColor(s.line0, out.line0);
	UnpackColor(s.line1, out.line1);

	out.fillGrad[0] = (float)s.fillX0; out.fillGrad[1] = (float)s.fillY0;
	out.fillGrad[2] = (float)s.fillX1; out.fillGrad[3] = (float)s.fillY1;
	out.lineGrad[0] = (float)s.lineX0; out.lineGrad[1] = (float)s.lineY0;
	out.lineGrad[2] = (float)s.lineX1; out.lineGrad[3] = (float)s.lineY1;

	out.flags[0] = (float)(s.fillGradient != 0);
	out.flags[1] = (float)(s.lineGradient != 0);
	out.flags[2] = (float)(s.hasFill     != 0);
	out.flags[3] = (float)(s.hasStroke   != 0);
}

static void PackGlyphs(const MetalOSDFrame &frame, const MetalOSDAtlasState &state,
                       std::vector<MetalOSDGlyph> &glyphs)
{
	glyphs.clear();
	if ((frame.text.empty()) || ((int)frame.text.size() <= 0)) return;
	const int textLength = (int)frame.text.size();

	for (size_t r = 0; r < frame.runs.size(); r++)
	{
		const MetalOSDTextRun &run = frame.runs[r];
		const MetalOSDAtlasFont *font = NULL;
		for (size_t i = 0; i < state.fonts.size(); i++)
		{
			if (state.fonts[i].data == run.font) { font = &state.fonts[i]; break; }
		}
		if ((font == NULL) || (font->count <= 0)) continue;

		float color[4];
		UnpackColor(run.color, color);

		double x     = run.x;
		const int yTop = (int)run.y - font->base;

		const uint32_t start = run.charOffset;
		const uint32_t end   = start + run.charCount;
		if ((start >= (uint32_t)textLength) || (end > (uint32_t)textLength)) continue;

		for (uint32_t i = start; i < end; i++)
		{
			const unsigned char ch = (unsigned char)frame.text[i];
			if (ch == 0) break;
			const int index = (int)ch - font->first;
			if ((index < 0) || (index >= font->count)) continue;

			const MetalOSDAtlasGlyph &ag = font->glyphs[(size_t)index];
			if ((ag.w <= 0) || (ag.h <= 0)) { x += ag.w; continue; }

			MetalOSDGlyph g;
			g.posSize[0] = (float)(int)x;
			g.posSize[1] = (float)yTop;
			g.posSize[2] = (float)ag.w;
			g.posSize[3] = (float)ag.h;
			g.uv[0] = ag.u0; g.uv[1] = ag.v0; g.uv[2] = ag.u1; g.uv[3] = ag.v1;
			g.color[0] = color[0]; g.color[1] = color[1];
			g.color[2] = color[2]; g.color[3] = color[3];
			glyphs.push_back(g);
			x += ag.w;
		}
	}
}

// ---------------------------------------------------------------------------
// Buffer ring
// ---------------------------------------------------------------------------

- (id<MTLTexture>)cpuTextureForSlot:(int)slot
{
	if (_cpuTextures[slot] != nil)
	{
		return _cpuTextures[slot];
	}

	MTLTextureDescriptor *cpuDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
	                                                                                width:METAL_OSD_WIDTH
	                                                                               height:METAL_OSD_HEIGHT
	                                                                            mipmapped:NO];
	[cpuDesc setStorageMode:MTLStorageModeShared];
	[cpuDesc setUsage:MTLTextureUsageShaderRead];
	_cpuTextures[slot] = [_device newTextureWithDescriptor:cpuDesc];
	return _cpuTextures[slot];
}

- (id<MTLBuffer>)bufferForSlot:(int)slot
                          kind:(int)kind
                    neededBytes:(NSUInteger)needed
{
	id<MTLBuffer> *slots    = (kind == 0) ? _instanceBuffers : _glyphBuffers;
	NSUInteger   *capacity  = (kind == 0) ? _instanceCapacities : _glyphCapacities;

	if ((slots[slot] != nil) && (capacity[slot] >= needed))
	{
		return slots[slot];
	}

	const NSUInteger unit = (kind == 0) ? sizeof(MetalOSDInstance) : sizeof(MetalOSDGlyph);
	const NSUInteger cap  = std::max(needed, unit * (NSUInteger)32);
	id<MTLBuffer> buf = [_device newBufferWithLength:cap options:MTLResourceStorageModeShared];
	if (buf == nil) return nil;

	[slots[slot] release];
	slots[slot] = buf;      // +1 from newBuffer..., released in dealloc
	capacity[slot] = cap;
	return buf;
}

// ---------------------------------------------------------------------------
// Per-frame offscreen render
// ---------------------------------------------------------------------------

- (BOOL)renderIntoCommandBuffer:(id<MTLCommandBuffer>)cb
{
	if (!_ready || (cb == nil) || (_renderPassDesc == nil)) return NO;

	const MetalOSDFrame *published = MacMetalOSD_LockFront();
	MetalOSDFrame frame = *published;   // deep copy so we can release the lock
	MacMetalOSD_UnlockFront();

	_hasContent = frame.hasContent();
	if (!_hasContent)
	{
		_hudValid = NO;
		return NO;
	}

	if (_hudValid && (frame.generation == _lastGeneration))
	{
		return YES;        // texture already holds this frame
	}

	MetalOSDAtlasState &state = *(MetalOSDAtlasState *)_atlasState;
	std::vector<MetalOSDInstance> instances;
	instances.reserve(frame.shapes.size());
	for (size_t i = 0; i < frame.shapes.size(); i++)
	{
		MetalOSDInstance inst;
		PackInstance(frame.shapes[i],
		             frame.points.empty() ? NULL : frame.points.data(),
		             (int)frame.points.size(), inst);
		instances.push_back(inst);
	}

	std::vector<MetalOSDGlyph> glyphs;
	PackGlyphs(frame, state, glyphs);

	const int slot = (int)(_bufferSlot++ % METAL_OSD_BUFFER_SLOTS);
	const size_t instanceBytes = instances.size() * sizeof(MetalOSDInstance);
	const size_t glyphBytes    = glyphs.size() * sizeof(MetalOSDGlyph);

	id<MTLBuffer> instanceBuffer = nil;
	if (instanceBytes > 0)
	{
		instanceBuffer = [self bufferForSlot:slot kind:0 neededBytes:instanceBytes];
		if (instanceBuffer != nil)
		{
			memcpy([instanceBuffer contents], instances.data(), instanceBytes);
		}
	}

	id<MTLBuffer> glyphBuffer = nil;
	if (glyphBytes > 0)
	{
		glyphBuffer = [self bufferForSlot:slot kind:1 neededBytes:glyphBytes];
		if (glyphBuffer != nil)
		{
			memcpy([glyphBuffer contents], glyphs.data(), glyphBytes);
		}
	}

	// The CPU fallback layer uses the same ring slot as the instance buffers,
	// so replaceRegion never touches a texture an in-flight command buffer reads.
	id<MTLTexture> cpuTexture = nil;
	const BOOL drawCpuLayer = frame.hasCpuLayer && !frame.cpuPixels.empty();
	if (drawCpuLayer)
	{
		cpuTexture = [self cpuTextureForSlot:slot];
		if (cpuTexture != nil)
		{
			[cpuTexture replaceRegion:MTLRegionMake2D(0, 0, METAL_OSD_WIDTH, METAL_OSD_HEIGHT)
			              mipmapLevel:0
			                withBytes:frame.cpuPixels.data()
			              bytesPerRow:METAL_OSD_WIDTH * sizeof(uint32_t)];
		}
	}

	MetalOSDUniforms uniforms = { { (float)METAL_OSD_WIDTH, (float)METAL_OSD_HEIGHT, 0.0f, 0.0f } };
	id<MTLRenderCommandEncoder> rce = [cb renderCommandEncoderWithDescriptor:_renderPassDesc];

	// Fixed order: shapes -> glyphs -> CPU fallback layer (same as WGPU).
	if ((instanceBuffer != nil) && !instances.empty())
	{
		[rce setRenderPipelineState:_shapePipeline];
		[rce setVertexBuffer:instanceBuffer offset:0 atIndex:0];
		[rce setFragmentBuffer:instanceBuffer offset:0 atIndex:0];
		[rce setVertexBytes:&uniforms length:sizeof(uniforms) atIndex:1];
		[rce drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6
		                                instanceCount:instances.size()];
	}

	if ((glyphBuffer != nil) && !glyphs.empty() && (_atlasTexture != nil))
	{
		[rce setRenderPipelineState:_glyphPipeline];
		[rce setVertexBuffer:glyphBuffer offset:0 atIndex:0];
		[rce setVertexBytes:&uniforms length:sizeof(uniforms) atIndex:1];
		[rce setFragmentTexture:_atlasTexture atIndex:0];
		[rce drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6
		                                instanceCount:glyphs.size()];
	}

	if (cpuTexture != nil)
	{
		[rce setRenderPipelineState:_blitPipeline];
		[rce setFragmentTexture:cpuTexture atIndex:0];
		[rce drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];
	}

	[rce endEncoding];

	_lastGeneration = frame.generation;
	_hudValid = YES;
	return YES;
}

@end
