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

#include "wgpu_draw.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <chrono>
#include <thread>
#include <vector>

#if defined(ENABLE_WGPU)
#include "webgpu.h"
#endif

// ---------------------------------------------------------------------------
// WGSL
// ---------------------------------------------------------------------------

// hud_shapes.wgsl is the canonical copy of this shader. It is duplicated here
// because a Windows build embeds resources at link time while the POSIX build
// has no equivalent step; wgpu_osd_selftest compares the two so they cannot
// drift silently.
static const char *kShapeShader = R"WGSL(
// OSD shape rasterizer for the WebGPU OSD backend (stage 2).
//
// One instance per primitive. The vertex stage expands the instance's bounding
// box into two triangles; the fragment stage evaluates a signed distance field
// and returns a premultiplied BGRA color for the pipeline's
// One / OneMinusSrcAlpha blend state.
//
// Coordinates are pixels, origin at the top-left of the 256x384 HUD target,
// exactly like the AggDrawTarget they replace. Do not round or offset them:
// agg_osd.cpp already aligns its coordinates to half pixels itself.
//
// Keep this file in sync with kShapeShader in wgpu_draw.cpp. The self-test
// compares the two.

struct Instance
{
	bbox: vec4f,       // minX, minY, maxX, maxY after stroke expansion
	shape: vec4f,      // rect: center + half size; ellipse: center + radii;
	                   // line: x0, y0, x1, y1
	params: vec4f,     // radius, strokeWidth, shapeType, polygon point count
	fill0: vec4f,      // straight RGBA
	fill1: vec4f,
	line0: vec4f,
	line1: vec4f,
	fillGrad: vec4f,   // x0, y0, x1, y1
	lineGrad: vec4f,
	flags: vec4f,      // fillGradient, lineGradient, hasFill, hasStroke
	pts: array<vec2f, 64>,
};

struct Uniforms
{
	size: vec4f,       // target width, height, 0, 0
};

@group(0) @binding(0) var<storage, read> instances: array<Instance>;
@group(0) @binding(1) var<uniform> uniforms: Uniforms;

struct VSOut
{
	@builtin(position) position: vec4f,
	@location(0) pixel: vec2f,
	@location(1) @interpolate(flat) index: u32,
};

@vertex
fn vsShape(@builtin(vertex_index) vertexIndex: u32,
           @builtin(instance_index) instanceIndex: u32) -> VSOut
{
	let b = instances[instanceIndex].bbox;

	var corners = array<vec2f, 4>(
		vec2f(b.x, b.y),
		vec2f(b.z, b.y),
		vec2f(b.z, b.w),
		vec2f(b.x, b.w));
	var order = array<u32, 6>(0u, 1u, 2u, 0u, 2u, 3u);

	var result: VSOut;
	let p = corners[order[vertexIndex]];
	result.position = vec4f(
		p.x / uniforms.size.x * 2.0 - 1.0,
		1.0 - p.y / uniforms.size.y * 2.0,
		0.0,
		1.0);
	result.pixel = p;
	result.index = instanceIndex;
	return result;
}

fn sdBox(p: vec2f, c: vec2f, h: vec2f) -> f32
{
	let q = abs(p - c) - h;
	return length(max(q, vec2f(0.0))) + min(max(q.x, q.y), 0.0);
}

fn sdRoundBox(p: vec2f, c: vec2f, h: vec2f, r: f32) -> f32
{
	let q = abs(p - c) - h + vec2f(r);
	return length(max(q, vec2f(0.0))) + min(max(q.x, q.y), 0.0) - r;
}

// Radial distance divided by the gradient magnitude of the implicit function,
// so the flat sides of a non-circular ellipse get the right coverage. For a
// circle this reduces to (length - 1) * r, which is exact.
fn sdEllipse(p: vec2f, c: vec2f, r: vec2f) -> f32
{
	let rr = max(r, vec2f(1e-4));
	let q = (p - c) / rr;
	let l = length(q);
	let g = max(length(q / rr), 1e-6);
	return (l - 1.0) * l / g;
}

fn sdSegment(p: vec2f, a: vec2f, b: vec2f) -> f32
{
	let pa = p - a;
	let ba = b - a;
	let h = clamp(dot(pa, ba) / max(dot(ba, ba), 1e-6), 0.0, 1.0);
	return length(pa - ba * h);
}

// The usual even-odd polygon distance. agg_osd.cpp's D-pad outline is concave,
// so a convex test would be wrong.
fn sdPolygon(p: vec2f, n: u32, inst: Instance) -> f32
{
	var d = dot(p - inst.pts[0], p - inst.pts[0]);
	var s = 1.0;

	for (var i = 0u; i < n; i = i + 1u)
	{
		let j = (i + n - 1u) % n;
		let vi = inst.pts[i];
		let vj = inst.pts[j];
		let e = vj - vi;
		let w = p - vi;
		let b = w - e * clamp(dot(w, e) / max(dot(e, e), 1e-6), 0.0, 1.0);
		d = min(d, dot(b, b));

		let c1 = p.y >= vi.y;
		let c2 = p.y < vj.y;
		let c3 = e.x * w.y > e.y * w.x;

		if ((c1 && c2 && c3) || (!c1 && !c2 && !c3))
		{
			s = -s;
		}
	}

	return s * sqrt(d);
}

// Open path: distance to the segments only, no closing edge. Used for Lua's
// stroked arcs and quadratic curves, which AGG draws without a chord.
fn sdPolyline(p: vec2f, n: u32, inst: Instance) -> f32
{
	var d = 1e20;

	for (var i = 1u; i < n; i = i + 1u)
	{
		d = min(d, sdSegment(p, inst.pts[i - 1u], inst.pts[i]));
	}

	return d;
}

fn shapeDistance(p: vec2f, inst: Instance) -> f32
{
	let shapeType = i32(inst.params.z + 0.5);
	let c = inst.shape.xy;

	if (shapeType == 1)
	{
		return sdRoundBox(p, c, inst.shape.zw, inst.params.x);
	}
	if (shapeType == 2)
	{
		return sdEllipse(p, c, inst.shape.zw);
	}
	if (shapeType == 3)
	{
		return sdPolygon(p, u32(inst.params.w + 0.5), inst);
	}
	if (shapeType == 4)
	{
		return sdSegment(p, inst.shape.xy, inst.shape.zw);
	}
	if (shapeType == 5)
	{
		return sdPolyline(p, u32(inst.params.w + 0.5), inst);
	}

	return sdBox(p, c, inst.shape.zw);
}

fn gradientColor(c0: vec4f, c1: vec4f, endpoints: vec4f, p: vec2f, enabled: bool) -> vec4f
{
	if (!enabled)
	{
		return c0;
	}

	let d = endpoints.zw - endpoints.xy;
	let dd = dot(d, d);
	var t = 0.0;

	if (dd > 1e-6)
	{
		t = clamp(dot(p - endpoints.xy, d) / dd, 0.0, 1.0);
	}

	return mix(c0, c1, t);
}

// AGG's rasterizer antialiases over roughly one pixel; match that width.
fn coverage(d: f32) -> f32
{
	return clamp(0.5 - d, 0.0, 1.0);
}

@fragment
fn fsShape(input: VSOut) -> @location(0) vec4f
{
	let inst = instances[input.index];
	let d = shapeDistance(input.pixel, inst);
	let halfStroke = inst.params.y * 0.5;

	var fillAlpha = 0.0;
	var fillColor = vec4f(0.0);

	if (inst.flags.z > 0.5)
	{
		fillColor = gradientColor(inst.fill0, inst.fill1, inst.fillGrad, input.pixel,
		                          inst.flags.x > 0.5);
		fillAlpha = fillColor.a * coverage(d);
	}

	var strokeAlpha = 0.0;
	var strokeColor = vec4f(0.0);

	if (inst.flags.w > 0.5)
	{
		strokeColor = gradientColor(inst.line0, inst.line1, inst.lineGrad, input.pixel,
		                            inst.flags.y > 0.5);
		strokeAlpha = strokeColor.a * coverage(abs(d) - halfStroke);
	}

	// Fill and stroke both feed a single premultiplied source-over result, so a
	// FillAndStroke primitive costs one fragment shader evaluation.
	let fill = vec4f(fillColor.rgb * fillAlpha, fillAlpha);
	let stroke = vec4f(strokeColor.rgb * strokeAlpha, strokeAlpha);

	return vec4f(stroke.rgb + fill.rgb * (1.0 - strokeAlpha),
	             strokeAlpha + fillAlpha * (1.0 - strokeAlpha));
}
)WGSL";

// The CPU layer holds whatever a caller wrote straight into buf() (Lua's
// gui.* pixels); it is no longer rasterized by AGG. This quad puts it over the
// shapes. Nearest sampling keeps those pixels crisp.
static const char *kTextShader = R"WGSL(
struct VSOut
{
	@builtin(position) position: vec4f,
	@location(0) uv: vec2f,
};

@group(0) @binding(0) var textTexture: texture_2d<f32>;
@group(0) @binding(1) var textSampler: sampler;

@vertex
fn vsText(@builtin(vertex_index) vertexIndex: u32) -> VSOut
{
	var positions = array<vec2f, 6>(
		vec2f(-1.0,  1.0), vec2f(1.0,  1.0), vec2f(1.0, -1.0),
		vec2f(-1.0,  1.0), vec2f(1.0, -1.0), vec2f(-1.0, -1.0));
	var uvs = array<vec2f, 6>(
		vec2f(0.0, 0.0), vec2f(1.0, 0.0), vec2f(1.0, 1.0),
		vec2f(0.0, 0.0), vec2f(1.0, 1.0), vec2f(0.0, 1.0));

	var result: VSOut;
	result.position = vec4f(positions[vertexIndex], 0.0, 1.0);
	result.uv = uvs[vertexIndex];
	return result;
}

@fragment
fn fsText(input: VSOut) -> @location(0) vec4f
{
	return textureSample(textTexture, textSampler, input.uv);
}
)WGSL";

// Bitmap-text pipeline. Keep in sync with hud_text.wgsl.
static const char *kGlyphShader = R"WGSL(
// OSD bitmap-text rasterizer for the WebGPU OSD backend (stage 3).
//
// One instance per glyph. The vertex stage expands a unit quad to the glyph's
// pixel rectangle and picks the glyph's rectangle in the R8 coverage atlas; the
// fragment stage multiplies that coverage by the run's straight RGBA color and
// returns a premultiplied color for the pipeline's One / OneMinusSrcAlpha
// blend state.
//
// The atlas stores the same 1bpp bitmap fonts AGG's glyph_raster_bin used, so
// Nearest sampling reproduces the original pixel look. Coordinates are pixels,
// origin at the top-left of the 256x384 HUD target.
//
// Keep this file in sync with kGlyphShader in wgpu_draw.cpp. The self-test
// compares the two.

struct Uniforms
{
	size: vec4f,       // target width, height, 0, 0
};

@group(0) @binding(0) var<uniform> uniforms: Uniforms;
@group(0) @binding(1) var atlas: texture_2d<f32>;
@group(0) @binding(2) var atlasSampler: sampler;

struct VSOut
{
	@builtin(position) position: vec4f,
	@location(0) uv: vec2f,
	@location(1) color: vec4f,
};

@vertex
fn vsGlyph(@builtin(vertex_index) vertexIndex: u32,
           @location(0) posSize: vec4f,
           @location(1) uvRect: vec4f,
           @location(2) color: vec4f) -> VSOut
{
	var corners = array<vec2f, 6>(
		vec2f(0.0, 0.0), vec2f(1.0, 0.0), vec2f(1.0, 1.0),
		vec2f(0.0, 0.0), vec2f(1.0, 1.0), vec2f(0.0, 1.0));
	let c = corners[vertexIndex];
	let p = posSize.xy + c * posSize.zw;

	var result: VSOut;
	result.position = vec4f(
		p.x / uniforms.size.x * 2.0 - 1.0,
		1.0 - p.y / uniforms.size.y * 2.0,
		0.0,
		1.0);
	result.uv = vec2f(mix(uvRect.x, uvRect.z, c.x),
	                  mix(uvRect.y, uvRect.w, c.y));
	result.color = color;
	return result;
}

@fragment
fn fsGlyph(input: VSOut) -> @location(0) vec4f
{
	let cover = textureSampleLevel(atlas, atlasSampler, input.uv, 0.0).r;
	let alpha = input.color.a * cover;
	return vec4f(input.color.rgb * alpha, alpha);
}
)WGSL";

const char *WGPUOSDShapeShaderSource()
{
	return kShapeShader;
}

const char *WGPUOSDTextShaderSource()
{
	return kGlyphShader;
}

#if defined(ENABLE_WGPU)

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace
{

struct GpuInstance
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
	float pts[128];
};

// Must match struct Instance in hud_shapes.wgsl.
static_assert(sizeof(GpuInstance) == 672, "GpuInstance layout does not match hud_shapes.wgsl");

struct GpuUniforms
{
	float size[4];
};

// One glyph quad. Must match the vertex inputs of vsGlyph in hud_text.wgsl.
struct GpuGlyph
{
	float posSize[4]; // x, y, width, height in target pixels
	float uv[4];      // atlas u0, v0, u1, v1 (texel edges)
	float color[4];   // straight RGBA
};

static_assert(sizeof(GpuGlyph) == 48, "GpuGlyph layout does not match hud_text.wgsl");

// Where one character lives in the coverage atlas.
struct AtlasGlyph
{
	float u0, v0, u1, v1;
	int w, h;
};

// Metadata for one registered bitmap font.
struct AtlasFont
{
	const uint8_t *data;
	int height;
	int base;
	int first;
	int count;
	std::vector<AtlasGlyph> glyphs;
};

static WGPUStringView Lit(const char *s)
{
	WGPUStringView view;
	view.data = s;
	view.length = WGPU_STRLEN;
	return view;
}

static void UnpackColor(uint32_t c, float out[4])
{
	out[0] = (float)((c >> 24) & 0xFF) / 255.0f;
	out[1] = (float)((c >> 16) & 0xFF) / 255.0f;
	out[2] = (float)((c >> 8) & 0xFF) / 255.0f;
	out[3] = (float)(c & 0xFF) / 255.0f;
}

// Same layout as agg::glyph_raster_bin: height, base, first char, count, a
// little-endian offset table, then per-glyph width + 1bpp rows.
static int FontCharCount(const uint8_t *f)
{
	const int count = f[3] ? f[3] : 256;
	return (count > 0 && count <= 256) ? count : 0;
}

static const uint8_t *FontGlyphData(const uint8_t *f, int index)
{
	const int count = FontCharCount(f);
	if ((count <= 0) || (index < 0) || (index >= count))
	{
		return NULL;
	}

	const unsigned off = (unsigned)f[4 + index * 2] | ((unsigned)f[5 + index * 2] << 8);
	return f + 4 + count * 2 + off;
}

static uint32_t NextPowerOfTwo(uint32_t v)
{
	uint32_t p = 1;
	while ((p < v) && (p < (1u << 31)))
	{
		p <<= 1;
	}
	return p;
}

} // namespace

// ---------------------------------------------------------------------------
// WGPUOSDRenderer
// ---------------------------------------------------------------------------

struct WGPUOSDRenderer::Impl
{
	WGPUInstance instance;
	WGPUDevice device;
	WGPUQueue queue;

	int width;
	int height;

	WGPUShaderModule shapeModule;
	WGPUShaderModule textModule;

	WGPUBindGroupLayout shapeBindGroupLayout;
	WGPUBindGroupLayout textBindGroupLayout;

	WGPURenderPipeline shapePipeline;
	WGPURenderPipeline textPipeline;

	WGPUBuffer uniformBuffer;
	WGPUSampler sampler;
	WGPUTexture textTexture;
	WGPUTextureView textView;
	WGPUBindGroup textBindGroup;

	WGPUTexture targetTexture;
	WGPUTextureView targetView;

	WGPUBuffer instanceBuffer;
	WGPUBindGroup instanceBindGroup;
	uint32_t instanceCapacity;

	WGPUShaderModule glyphModule;
	WGPUBindGroupLayout glyphBindGroupLayout;
	WGPURenderPipeline glyphPipeline;

	WGPUBuffer glyphBuffer;
	uint32_t glyphCapacity;

	WGPUTexture atlasTexture;
	WGPUTextureView atlasView;
	WGPUBindGroup glyphBindGroup;

	std::vector<GpuInstance> instances;
	std::vector<unsigned char> textStaging;

	std::vector<WGPUOSDFont> fonts;
	std::vector<AtlasFont> atlasFonts;
	std::vector<GpuGlyph> glyphs;
	std::vector<unsigned char> atlasPixels;

	bool ready;

	Impl()
		: instance(NULL)
		, device(NULL)
		, queue(NULL)
		, width(0)
		, height(0)
		, shapeModule(NULL)
		, textModule(NULL)
		, shapeBindGroupLayout(NULL)
		, textBindGroupLayout(NULL)
		, shapePipeline(NULL)
		, textPipeline(NULL)
		, uniformBuffer(NULL)
		, sampler(NULL)
		, textTexture(NULL)
		, textView(NULL)
		, textBindGroup(NULL)
		, targetTexture(NULL)
		, targetView(NULL)
		, instanceBuffer(NULL)
		, instanceBindGroup(NULL)
		, instanceCapacity(0)
		, glyphModule(NULL)
		, glyphBindGroupLayout(NULL)
		, glyphPipeline(NULL)
		, glyphBuffer(NULL)
		, glyphCapacity(0)
		, atlasTexture(NULL)
		, atlasView(NULL)
		, glyphBindGroup(NULL)
		, ready(false)
	{}

	bool CreateResources();
	bool EnsureInstanceBuffer(uint32_t count);
	bool EnsureGlyphBuffer(uint32_t count);
	bool BuildAtlas();
	void ReleaseAtlas();
	void PackInstance(const WGPUOSDShape &s, const WGPUOSDPoint *points, int pointCount, GpuInstance &out) const;
	void UploadText(const uint32_t *pixels, int strideBytes);
	void PackGlyphs(const WGPUOSDTextRun *runs, int runCount, const char *text, int textLength);
};

static bool CreateBlendPipeline(WGPUDevice device,
                                WGPUBindGroupLayout bindGroupLayout,
                                WGPUShaderModule module,
                                const char *vertexEntry,
                                const char *fragmentEntry,
                                const WGPUVertexBufferLayout *vertexBuffer,
                                WGPURenderPipeline *out)
{
	WGPUPipelineLayoutDescriptor layoutDesc = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
	layoutDesc.bindGroupLayoutCount = 1;
	layoutDesc.bindGroupLayouts = &bindGroupLayout;
	WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &layoutDesc);

	if (pipelineLayout == NULL)
	{
		return false;
	}

	WGPUBlendComponent blend = WGPU_BLEND_COMPONENT_INIT;
	blend.operation = WGPUBlendOperation_Add;
	blend.srcFactor = WGPUBlendFactor_One;
	blend.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;

	WGPUBlendState blendState = WGPU_BLEND_STATE_INIT;
	blendState.color = blend;
	blendState.alpha = blend;

	WGPUColorTargetState colorTarget = WGPU_COLOR_TARGET_STATE_INIT;
	colorTarget.format = WGPUTextureFormat_BGRA8Unorm;
	colorTarget.blend = &blendState;
	colorTarget.writeMask = WGPUColorWriteMask_All;

	WGPUFragmentState fragmentState = WGPU_FRAGMENT_STATE_INIT;
	fragmentState.module = module;
	fragmentState.entryPoint = Lit(fragmentEntry);
	fragmentState.targetCount = 1;
	fragmentState.targets = &colorTarget;

	WGPUVertexState vertexState = WGPU_VERTEX_STATE_INIT;
	vertexState.module = module;
	vertexState.entryPoint = Lit(vertexEntry);
	vertexState.bufferCount = (vertexBuffer != NULL) ? 1 : 0;
	vertexState.buffers = vertexBuffer;

	WGPUPrimitiveState primitiveState = WGPU_PRIMITIVE_STATE_INIT;
	primitiveState.topology = WGPUPrimitiveTopology_TriangleList;
	primitiveState.cullMode = WGPUCullMode_None;

	WGPUMultisampleState multisampleState = WGPU_MULTISAMPLE_STATE_INIT;
	multisampleState.count = 1;

	WGPURenderPipelineDescriptor pipelineDesc = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
	pipelineDesc.layout = pipelineLayout;
	pipelineDesc.vertex = vertexState;
	pipelineDesc.primitive = primitiveState;
	pipelineDesc.multisample = multisampleState;
	pipelineDesc.fragment = &fragmentState;

	*out = wgpuDeviceCreateRenderPipeline(device, &pipelineDesc);
	wgpuPipelineLayoutRelease(pipelineLayout);

	return *out != NULL;
}

static WGPUTexture CreateColorTexture(WGPUDevice device, int width, int height, WGPUTextureUsage usage)
{
	WGPUTextureDescriptor desc = WGPU_TEXTURE_DESCRIPTOR_INIT;
	desc.usage = usage;
	desc.dimension = WGPUTextureDimension_2D;
	desc.size.width = (uint32_t)width;
	desc.size.height = (uint32_t)height;
	desc.size.depthOrArrayLayers = 1;
	desc.format = WGPUTextureFormat_BGRA8Unorm;
	desc.mipLevelCount = 1;
	desc.sampleCount = 1;

	return wgpuDeviceCreateTexture(device, &desc);
}

bool WGPUOSDRenderer::Impl::CreateResources()
{
	WGPUShaderSourceWGSL shapeSource = WGPU_SHADER_SOURCE_WGSL_INIT;
	shapeSource.code = Lit(kShapeShader);

	WGPUShaderModuleDescriptor shapeDesc = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
	shapeDesc.nextInChain = &shapeSource.chain;
	shapeModule = wgpuDeviceCreateShaderModule(device, &shapeDesc);

	WGPUShaderSourceWGSL textSource = WGPU_SHADER_SOURCE_WGSL_INIT;
	textSource.code = Lit(kTextShader);

	WGPUShaderModuleDescriptor textDesc = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
	textDesc.nextInChain = &textSource.chain;
	textModule = wgpuDeviceCreateShaderModule(device, &textDesc);

	WGPUShaderSourceWGSL glyphSource = WGPU_SHADER_SOURCE_WGSL_INIT;
	glyphSource.code = Lit(kGlyphShader);

	WGPUShaderModuleDescriptor glyphDesc = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
	glyphDesc.nextInChain = &glyphSource.chain;
	glyphModule = wgpuDeviceCreateShaderModule(device, &glyphDesc);

	if ((shapeModule == NULL) || (textModule == NULL) || (glyphModule == NULL))
	{
		return false;
	}

	WGPUBindGroupLayoutEntry shapeEntries[2] = {
		WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT,
		WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT
	};

	shapeEntries[0].binding = 0;
	shapeEntries[0].visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
	shapeEntries[0].buffer.type = WGPUBufferBindingType_ReadOnlyStorage;

	shapeEntries[1].binding = 1;
	shapeEntries[1].visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
	shapeEntries[1].buffer.type = WGPUBufferBindingType_Uniform;

	WGPUBindGroupLayoutDescriptor shapeLayoutDesc = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
	shapeLayoutDesc.entryCount = 2;
	shapeLayoutDesc.entries = shapeEntries;
	shapeBindGroupLayout = wgpuDeviceCreateBindGroupLayout(device, &shapeLayoutDesc);

	WGPUBindGroupLayoutEntry textEntries[2] = {
		WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT,
		WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT
	};

	textEntries[0].binding = 0;
	textEntries[0].visibility = WGPUShaderStage_Fragment;
	textEntries[0].texture.sampleType = WGPUTextureSampleType_Float;
	textEntries[0].texture.viewDimension = WGPUTextureViewDimension_2D;

	textEntries[1].binding = 1;
	textEntries[1].visibility = WGPUShaderStage_Fragment;
	textEntries[1].sampler.type = WGPUSamplerBindingType_Filtering;

	WGPUBindGroupLayoutDescriptor textLayoutDesc = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
	textLayoutDesc.entryCount = 2;
	textLayoutDesc.entries = textEntries;
	textBindGroupLayout = wgpuDeviceCreateBindGroupLayout(device, &textLayoutDesc);

	WGPUBindGroupLayoutEntry glyphEntries[3] = {
		WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT,
		WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT,
		WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT
	};

	glyphEntries[0].binding = 0;
	glyphEntries[0].visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
	glyphEntries[0].buffer.type = WGPUBufferBindingType_Uniform;

	glyphEntries[1].binding = 1;
	glyphEntries[1].visibility = WGPUShaderStage_Fragment;
	glyphEntries[1].texture.sampleType = WGPUTextureSampleType_Float;
	glyphEntries[1].texture.viewDimension = WGPUTextureViewDimension_2D;

	glyphEntries[2].binding = 2;
	glyphEntries[2].visibility = WGPUShaderStage_Fragment;
	glyphEntries[2].sampler.type = WGPUSamplerBindingType_Filtering;

	WGPUBindGroupLayoutDescriptor glyphLayoutDesc = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
	glyphLayoutDesc.entryCount = 3;
	glyphLayoutDesc.entries = glyphEntries;
	glyphBindGroupLayout = wgpuDeviceCreateBindGroupLayout(device, &glyphLayoutDesc);

	if ((shapeBindGroupLayout == NULL) || (textBindGroupLayout == NULL) || (glyphBindGroupLayout == NULL))
	{
		return false;
	}

	if (!CreateBlendPipeline(device, shapeBindGroupLayout, shapeModule, "vsShape", "fsShape", NULL, &shapePipeline) ||
	    !CreateBlendPipeline(device, textBindGroupLayout, textModule, "vsText", "fsText", NULL, &textPipeline))
	{
		return false;
	}

	WGPUVertexAttribute glyphAttributes[3] = {
		WGPU_VERTEX_ATTRIBUTE_INIT,
		WGPU_VERTEX_ATTRIBUTE_INIT,
		WGPU_VERTEX_ATTRIBUTE_INIT
	};

	for (uint32_t i = 0; i < 3; i++)
	{
		glyphAttributes[i].shaderLocation = i;
		glyphAttributes[i].format = WGPUVertexFormat_Float32x4;
		glyphAttributes[i].offset = (uint64_t)(i * 16);
	}

	WGPUVertexBufferLayout glyphBufferLayout = WGPU_VERTEX_BUFFER_LAYOUT_INIT;
	glyphBufferLayout.stepMode = WGPUVertexStepMode_Instance;
	glyphBufferLayout.arrayStride = sizeof(GpuGlyph);
	glyphBufferLayout.attributeCount = 3;
	glyphBufferLayout.attributes = glyphAttributes;

	if (!CreateBlendPipeline(device, glyphBindGroupLayout, glyphModule, "vsGlyph", "fsGlyph",
	                         &glyphBufferLayout, &glyphPipeline))
	{
		return false;
	}

	WGPUBufferDescriptor uniformDesc = WGPU_BUFFER_DESCRIPTOR_INIT;
	uniformDesc.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
	uniformDesc.size = sizeof(GpuUniforms);
	uniformBuffer = wgpuDeviceCreateBuffer(device, &uniformDesc);

	if (uniformBuffer == NULL)
	{
		return false;
	}

	GpuUniforms uniforms;
	uniforms.size[0] = (float)width;
	uniforms.size[1] = (float)height;
	uniforms.size[2] = 0.0f;
	uniforms.size[3] = 0.0f;
	wgpuQueueWriteBuffer(queue, uniformBuffer, 0, &uniforms, sizeof(uniforms));

	targetTexture = CreateColorTexture(device, width, height,
	                                   WGPUTextureUsage_RenderAttachment |
	                                   WGPUTextureUsage_TextureBinding |
	                                   WGPUTextureUsage_CopySrc);
	textTexture = CreateColorTexture(device, width, height,
	                                 WGPUTextureUsage_TextureBinding |
	                                 WGPUTextureUsage_CopyDst);

	if ((targetTexture == NULL) || (textTexture == NULL))
	{
		return false;
	}

	targetView = wgpuTextureCreateView(targetTexture, NULL);
	textView = wgpuTextureCreateView(textTexture, NULL);

	if ((targetView == NULL) || (textView == NULL))
	{
		return false;
	}

	WGPUSamplerDescriptor samplerDesc = WGPU_SAMPLER_DESCRIPTOR_INIT;
	samplerDesc.addressModeU = WGPUAddressMode_ClampToEdge;
	samplerDesc.addressModeV = WGPUAddressMode_ClampToEdge;
	samplerDesc.addressModeW = WGPUAddressMode_ClampToEdge;
	samplerDesc.magFilter = WGPUFilterMode_Nearest;
	samplerDesc.minFilter = WGPUFilterMode_Nearest;
	samplerDesc.mipmapFilter = WGPUMipmapFilterMode_Nearest;
	sampler = wgpuDeviceCreateSampler(device, &samplerDesc);

	if (sampler == NULL)
	{
		return false;
	}

	WGPUBindGroupEntry textGroupEntries[2] = {
		WGPU_BIND_GROUP_ENTRY_INIT,
		WGPU_BIND_GROUP_ENTRY_INIT
	};

	textGroupEntries[0].binding = 0;
	textGroupEntries[0].textureView = textView;

	textGroupEntries[1].binding = 1;
	textGroupEntries[1].sampler = sampler;

	WGPUBindGroupDescriptor textGroupDesc = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
	textGroupDesc.layout = textBindGroupLayout;
	textGroupDesc.entryCount = 2;
	textGroupDesc.entries = textGroupEntries;
	textBindGroup = wgpuDeviceCreateBindGroup(device, &textGroupDesc);

	if (textBindGroup == NULL)
	{
		return false;
	}

	return BuildAtlas();
}

bool WGPUOSDRenderer::Impl::EnsureInstanceBuffer(uint32_t count)
{
	if ((instanceBuffer != NULL) && (count <= instanceCapacity))
	{
		return true;
	}

	if (instanceBindGroup != NULL)
	{
		wgpuBindGroupRelease(instanceBindGroup);
		instanceBindGroup = NULL;
	}

	if (instanceBuffer != NULL)
	{
		wgpuBufferRelease(instanceBuffer);
		instanceBuffer = NULL;
	}

	uint32_t capacity = (count < 32) ? 32 : count;

	WGPUBufferDescriptor bufferDesc = WGPU_BUFFER_DESCRIPTOR_INIT;
	bufferDesc.usage = WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst;
	bufferDesc.size = (uint64_t)capacity * sizeof(GpuInstance);
	instanceBuffer = wgpuDeviceCreateBuffer(device, &bufferDesc);

	if (instanceBuffer == NULL)
	{
		return false;
	}

	WGPUBindGroupEntry entries[2] = {
		WGPU_BIND_GROUP_ENTRY_INIT,
		WGPU_BIND_GROUP_ENTRY_INIT
	};

	entries[0].binding = 0;
	entries[0].buffer = instanceBuffer;
	entries[0].offset = 0;
	entries[0].size = bufferDesc.size;

	entries[1].binding = 1;
	entries[1].buffer = uniformBuffer;
	entries[1].offset = 0;
	entries[1].size = sizeof(GpuUniforms);

	WGPUBindGroupDescriptor groupDesc = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
	groupDesc.layout = shapeBindGroupLayout;
	groupDesc.entryCount = 2;
	groupDesc.entries = entries;
	instanceBindGroup = wgpuDeviceCreateBindGroup(device, &groupDesc);

	instanceCapacity = capacity;
	return instanceBindGroup != NULL;
}

bool WGPUOSDRenderer::Impl::EnsureGlyphBuffer(uint32_t count)
{
	if ((glyphBuffer != NULL) && (count <= glyphCapacity))
	{
		return true;
	}

	if (glyphBuffer != NULL)
	{
		wgpuBufferRelease(glyphBuffer);
		glyphBuffer = NULL;
	}

	uint32_t capacity = (count < 64) ? 64 : count;

	WGPUBufferDescriptor bufferDesc = WGPU_BUFFER_DESCRIPTOR_INIT;
	bufferDesc.usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;
	bufferDesc.size = (uint64_t)capacity * sizeof(GpuGlyph);
	glyphBuffer = wgpuDeviceCreateBuffer(device, &bufferDesc);

	if (glyphBuffer == NULL)
	{
		return false;
	}

	glyphCapacity = capacity;
	return true;
}

void WGPUOSDRenderer::Impl::ReleaseAtlas()
{
	if (glyphBindGroup != NULL) { wgpuBindGroupRelease(glyphBindGroup); glyphBindGroup = NULL; }
	if (atlasView != NULL) { wgpuTextureViewRelease(atlasView); atlasView = NULL; }
	if (atlasTexture != NULL) { wgpuTextureRelease(atlasTexture); atlasTexture = NULL; }

	atlasFonts.clear();
	atlasPixels.clear();
}

bool WGPUOSDRenderer::Impl::BuildAtlas()
{
	ReleaseAtlas();

	if (fonts.empty())
	{
		return true;
	}

	struct Placement
	{
		int font;
		int glyph;
		int x;
		int y;
	};

	const int atlasWidth = 1024;
	std::vector<Placement> placements;

	int penX = 0;
	int penY = 0;
	int rowHeight = 0;

	atlasFonts.resize(fonts.size());

	for (size_t fi = 0; fi < fonts.size(); fi++)
	{
		AtlasFont &af = atlasFonts[fi];
		const uint8_t *data = fonts[fi].data;

		af.data = data;
		af.height = 0;
		af.base = 0;
		af.first = 0;
		af.count = 0;

		if (data == NULL)
		{
			continue;
		}

		const int height = data[0];
		const int count = FontCharCount(data);

		if ((height <= 0) || (height > 64) || (count <= 0))
		{
			continue;
		}

		af.height = height;
		af.base = data[1];
		af.first = data[2];
		af.count = count;
		af.glyphs.resize((size_t)count);

		for (int i = 0; i < count; i++)
		{
			AtlasGlyph &g = af.glyphs[(size_t)i];
			memset(&g, 0, sizeof(g));
			g.h = height;

			const uint8_t *glyph = FontGlyphData(data, i);

			if (glyph == NULL)
			{
				continue;
			}

			const int w = glyph[0];

			if ((w <= 0) || (w > atlasWidth))
			{
				continue;
			}

			g.w = w;

			if (penX + w + 1 > atlasWidth)
			{
				penX = 0;
				penY += rowHeight + 1;
				rowHeight = 0;
			}

			Placement p;
			p.font = (int)fi;
			p.glyph = i;
			p.x = penX;
			p.y = penY;
			placements.push_back(p);

			penX += w + 1;
			rowHeight = std::max(rowHeight, height);
		}
	}

	const int usedHeight = std::max(penY + rowHeight, 1);
	const int atlasHeight = (int)NextPowerOfTwo((uint32_t)usedHeight);

	atlasPixels.assign((size_t)atlasWidth * (size_t)atlasHeight, 0);

	for (size_t i = 0; i < placements.size(); i++)
	{
		const Placement &p = placements[i];
		const AtlasFont &af = atlasFonts[(size_t)p.font];
		const AtlasGlyph &ag = af.glyphs[(size_t)p.glyph];
		const uint8_t *glyph = FontGlyphData(af.data, p.glyph);

		if ((glyph == NULL) || (ag.w <= 0))
		{
			continue;
		}

		const int byteWidth = (ag.w + 7) >> 3;
		const uint8_t *bits = glyph + 1;

		for (int row = 0; row < ag.h; row++)
		{
			unsigned char *dst = atlasPixels.data() +
			                     (size_t)(p.y + row) * (size_t)atlasWidth +
			                     (size_t)p.x;
			const uint8_t *src = bits + (size_t)row * (size_t)byteWidth;

			for (int col = 0; col < ag.w; col++)
			{
				dst[col] = (src[col >> 3] & (0x80u >> (col & 7))) ? 255 : 0;
			}
		}
	}

	for (size_t i = 0; i < placements.size(); i++)
	{
		const Placement &p = placements[i];
		AtlasGlyph &ag = atlasFonts[(size_t)p.font].glyphs[(size_t)p.glyph];

		ag.u0 = (float)p.x / (float)atlasWidth;
		ag.v0 = (float)p.y / (float)atlasHeight;
		ag.u1 = (float)(p.x + ag.w) / (float)atlasWidth;
		ag.v1 = (float)(p.y + ag.h) / (float)atlasHeight;
	}

	WGPUTextureDescriptor textureDesc = WGPU_TEXTURE_DESCRIPTOR_INIT;
	textureDesc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
	textureDesc.dimension = WGPUTextureDimension_2D;
	textureDesc.size.width = (uint32_t)atlasWidth;
	textureDesc.size.height = (uint32_t)atlasHeight;
	textureDesc.size.depthOrArrayLayers = 1;
	textureDesc.format = WGPUTextureFormat_R8Unorm;
	textureDesc.mipLevelCount = 1;
	textureDesc.sampleCount = 1;
	atlasTexture = wgpuDeviceCreateTexture(device, &textureDesc);

	if (atlasTexture == NULL)
	{
		ReleaseAtlas();
		return false;
	}

	atlasView = wgpuTextureCreateView(atlasTexture, NULL);

	if (atlasView == NULL)
	{
		ReleaseAtlas();
		return false;
	}

	const uint32_t pitch = ((uint32_t)atlasWidth + 255u) & ~255u;
	const void *upload = atlasPixels.data();
	std::vector<unsigned char> staging;

	if (pitch != (uint32_t)atlasWidth)
	{
		staging.resize((size_t)pitch * (size_t)atlasHeight);

		for (int y = 0; y < atlasHeight; y++)
		{
			memcpy(staging.data() + (size_t)y * (size_t)pitch,
			       atlasPixels.data() + (size_t)y * (size_t)atlasWidth,
			       (size_t)atlasWidth);
		}

		upload = staging.data();
	}

	WGPUTexelCopyTextureInfo copyDest = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
	copyDest.texture = atlasTexture;

	WGPUTexelCopyBufferLayout copyLayout = WGPU_TEXEL_COPY_BUFFER_LAYOUT_INIT;
	copyLayout.bytesPerRow = pitch;
	copyLayout.rowsPerImage = (uint32_t)atlasHeight;

	WGPUExtent3D copySize = WGPU_EXTENT_3D_INIT;
	copySize.width = (uint32_t)atlasWidth;
	copySize.height = (uint32_t)atlasHeight;
	copySize.depthOrArrayLayers = 1;

	wgpuQueueWriteTexture(queue, &copyDest, upload,
	                      (size_t)pitch * (size_t)atlasHeight, &copyLayout, &copySize);

	atlasPixels.clear();
	atlasPixels.shrink_to_fit();

	WGPUBindGroupEntry entries[3] = {
		WGPU_BIND_GROUP_ENTRY_INIT,
		WGPU_BIND_GROUP_ENTRY_INIT,
		WGPU_BIND_GROUP_ENTRY_INIT
	};

	entries[0].binding = 0;
	entries[0].buffer = uniformBuffer;
	entries[0].offset = 0;
	entries[0].size = sizeof(GpuUniforms);

	entries[1].binding = 1;
	entries[1].textureView = atlasView;

	entries[2].binding = 2;
	entries[2].sampler = sampler;

	WGPUBindGroupDescriptor groupDesc = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
	groupDesc.layout = glyphBindGroupLayout;
	groupDesc.entryCount = 3;
	groupDesc.entries = entries;
	glyphBindGroup = wgpuDeviceCreateBindGroup(device, &groupDesc);

	return glyphBindGroup != NULL;
}

void WGPUOSDRenderer::Impl::PackGlyphs(const WGPUOSDTextRun *runs,
                                       int runCount,
                                       const char *text,
                                       int textLength)
{
	glyphs.clear();

	if ((runs == NULL) || (runCount <= 0) || (text == NULL) || (textLength <= 0))
	{
		return;
	}

	for (int r = 0; r < runCount; r++)
	{
		const WGPUOSDTextRun &run = runs[r];
		const AtlasFont *font = NULL;

		for (size_t i = 0; i < atlasFonts.size(); i++)
		{
			if (atlasFonts[i].data == run.font)
			{
				font = &atlasFonts[i];
				break;
			}
		}

		if ((font == NULL) || (font->count <= 0))
		{
			continue;
		}

		float color[4];
		UnpackColor(run.color, color);

		double x = run.x;
		const int yTop = (int)run.y - font->base;

		const uint32_t start = run.charOffset;
		uint32_t end = start + run.charCount;

		if ((start >= (uint32_t)textLength) || (end > (uint32_t)textLength))
		{
			continue;
		}

		for (uint32_t i = start; i < end; i++)
		{
			const unsigned char ch = (unsigned char)text[i];

			if (ch == 0)
			{
				break;
			}

			const int index = (int)ch - font->first;

			if ((index < 0) || (index >= font->count))
			{
				continue;
			}

			const AtlasGlyph &ag = font->glyphs[(size_t)index];

			if ((ag.w <= 0) || (ag.h <= 0))
			{
				x += ag.w;
				continue;
			}

			GpuGlyph g;
			g.posSize[0] = (float)(int)x;
			g.posSize[1] = (float)yTop;
			g.posSize[2] = (float)ag.w;
			g.posSize[3] = (float)ag.h;
			g.uv[0] = ag.u0;
			g.uv[1] = ag.v0;
			g.uv[2] = ag.u1;
			g.uv[3] = ag.v1;
			g.color[0] = color[0];
			g.color[1] = color[1];
			g.color[2] = color[2];
			g.color[3] = color[3];

			glyphs.push_back(g);

			x += ag.w;
		}
	}
}

void WGPUOSDRenderer::Impl::PackInstance(const WGPUOSDShape &s,
                                         const WGPUOSDPoint *points,
                                         int pointCount,
                                         GpuInstance &out) const
{
	memset(&out, 0, sizeof(out));

	const double strokeWidth = (s.lineWidth > 0.0) ? s.lineWidth : 0.0;
	const float strokeExpand = (s.hasStroke != 0) ? (float)(strokeWidth * 0.5) : 0.0f;

	float minX, minY, maxX, maxY;

	switch (s.type)
	{
		case WGPUOSDShape_Ellipse:
		{
			const float cx = (float)s.x0;
			const float cy = (float)s.y0;
			const float rx = (float)fabs(s.x1);
			const float ry = (float)fabs(s.y1);

			out.shape[0] = cx;
			out.shape[1] = cy;
			out.shape[2] = rx;
			out.shape[3] = ry;

			minX = cx - rx;
			minY = cy - ry;
			maxX = cx + rx;
			maxY = cy + ry;
			break;
		}

		case WGPUOSDShape_Line:
		{
			const float x0 = (float)s.x0;
			const float y0 = (float)s.y0;
			const float x1 = (float)s.x1;
			const float y1 = (float)s.y1;

			out.shape[0] = x0;
			out.shape[1] = y0;
			out.shape[2] = x1;
			out.shape[3] = y1;

			minX = std::min(x0, x1);
			minY = std::min(y0, y1);
			maxX = std::max(x0, x1);
			maxY = std::max(y0, y1);
			break;
		}

		case WGPUOSDShape_Polygon:
		case WGPUOSDShape_Polyline:
		{
			minX = minY = 3.4e38f;
			maxX = maxY = -3.4e38f;

			uint32_t count = std::min<uint32_t>(s.pointCount, 64);

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
				const WGPUOSDPoint &p = points[s.pointOffset + i];
				out.pts[i * 2 + 0] = p.x;
				out.pts[i * 2 + 1] = p.y;
				minX = std::min(minX, p.x);
				minY = std::min(minY, p.y);
				maxX = std::max(maxX, p.x);
				maxY = std::max(maxY, p.y);
			}

			out.params[3] = (float)count;
			break;
		}

		default:
		{
			const float x0 = (float)std::min(s.x0, s.x1);
			const float y0 = (float)std::min(s.y0, s.y1);
			const float x1 = (float)std::max(s.x0, s.x1);
			const float y1 = (float)std::max(s.y0, s.y1);

			const float hx = (x1 - x0) * 0.5f;
			const float hy = (y1 - y0) * 0.5f;

			out.shape[0] = x0 + hx;
			out.shape[1] = y0 + hy;
			out.shape[2] = hx;
			out.shape[3] = hy;

			out.params[0] = (float)std::min((double)std::min(hx, hy), s.radius);

			minX = x0;
			minY = y0;
			maxX = x1;
			maxY = y1;
			break;
		}
	}

	// One pixel of margin covers antialiasing; strokes also need half their
	// width outside the geometric outline.
	minX -= strokeExpand + 1.0f;
	minY -= strokeExpand + 1.0f;
	maxX += strokeExpand + 1.0f;
	maxY += strokeExpand + 1.0f;

	out.bbox[0] = minX;
	out.bbox[1] = minY;
	out.bbox[2] = maxX;
	out.bbox[3] = maxY;

	out.params[1] = (float)strokeWidth;
	out.params[2] = (float)s.type;

	UnpackColor(s.fill0, out.fill0);
	UnpackColor(s.fill1, out.fill1);
	UnpackColor(s.line0, out.line0);
	UnpackColor(s.line1, out.line1);

	out.fillGrad[0] = (float)s.fillX0;
	out.fillGrad[1] = (float)s.fillY0;
	out.fillGrad[2] = (float)s.fillX1;
	out.fillGrad[3] = (float)s.fillY1;

	out.lineGrad[0] = (float)s.lineX0;
	out.lineGrad[1] = (float)s.lineY0;
	out.lineGrad[2] = (float)s.lineX1;
	out.lineGrad[3] = (float)s.lineY1;

	out.flags[0] = (float)(s.fillGradient != 0);
	out.flags[1] = (float)(s.lineGradient != 0);
	out.flags[2] = (float)(s.hasFill != 0);
	out.flags[3] = (float)(s.hasStroke != 0);
}

void WGPUOSDRenderer::Impl::UploadText(const uint32_t *pixels, int strideBytes)
{
	const size_t rowBytes = (size_t)width * 4;
	const uint32_t pitch = (uint32_t)((rowBytes + 255) & ~(size_t)255);

	const unsigned char *src = (const unsigned char *)pixels;
	const void *data = src;

	if ((uint32_t)strideBytes != pitch)
	{
		textStaging.resize((size_t)pitch * (size_t)height);

		for (int y = 0; y < height; y++)
		{
			memcpy(textStaging.data() + (size_t)y * pitch,
			       src + (size_t)y * (size_t)strideBytes, rowBytes);
		}

		data = textStaging.data();
	}

	WGPUTexelCopyTextureInfo copyDest = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
	copyDest.texture = textTexture;

	WGPUTexelCopyBufferLayout copyLayout = WGPU_TEXEL_COPY_BUFFER_LAYOUT_INIT;
	copyLayout.bytesPerRow = pitch;
	copyLayout.rowsPerImage = (uint32_t)height;

	WGPUExtent3D copySize = WGPU_EXTENT_3D_INIT;
	copySize.width = (uint32_t)width;
	copySize.height = (uint32_t)height;
	copySize.depthOrArrayLayers = 1;

	wgpuQueueWriteTexture(queue, &copyDest, data, (size_t)pitch * (size_t)height, &copyLayout, &copySize);
}

WGPUOSDRenderer::WGPUOSDRenderer()
	: _impl(new Impl())
{
}

WGPUOSDRenderer::~WGPUOSDRenderer()
{
	kill();
	delete _impl;
}

bool WGPUOSDRenderer::begin(void *instance, void *device, void *queue, int width, int height)
{
	if ((instance == NULL) || (device == NULL) || (queue == NULL) || (width <= 0) || (height <= 0))
	{
		return false;
	}

	if (_impl->ready &&
	    (_impl->instance == (WGPUInstance)instance) &&
	    (_impl->device == (WGPUDevice)device) &&
	    (_impl->width == width) &&
	    (_impl->height == height))
	{
		return true;
	}

	kill();

	_impl->instance = (WGPUInstance)instance;
	_impl->device = (WGPUDevice)device;
	_impl->queue = (WGPUQueue)queue;
	_impl->width = width;
	_impl->height = height;

	// The presentation layer can be killed (display method switch, window
	// recreation) while the OSD is still holding these pointers. Own a reference
	// so the renderer outlives any such teardown and can always release what it
	// created on them.
	wgpuInstanceAddRef(_impl->instance);
	wgpuDeviceAddRef(_impl->device);
	wgpuQueueAddRef(_impl->queue);

	if (!_impl->CreateResources())
	{
		kill();
		return false;
	}

	_impl->ready = true;
	return true;
}

bool WGPUOSDRenderer::setFonts(const WGPUOSDFont *fonts, int count)
{
	Impl &d = *_impl;

	d.fonts.clear();

	if ((fonts != NULL) && (count > 0))
	{
		d.fonts.assign(fonts, fonts + count);
	}

	if (d.ready)
	{
		return d.BuildAtlas();
	}

	return true;
}

bool WGPUOSDRenderer::hasFont(const void *data) const
{
	if ((data == NULL) || !_impl->ready)
	{
		return false;
	}

	for (size_t i = 0; i < _impl->atlasFonts.size(); i++)
	{
		if ((_impl->atlasFonts[i].data == data) && (_impl->atlasFonts[i].count > 0))
		{
			return true;
		}
	}

	return false;
}

void WGPUOSDRenderer::kill()
{
	Impl &d = *_impl;

	_impl->ready = false;

	d.ReleaseAtlas();

	if (d.glyphBuffer != NULL) { wgpuBufferRelease(d.glyphBuffer); d.glyphBuffer = NULL; }
	d.glyphCapacity = 0;

	if (d.instanceBindGroup != NULL) { wgpuBindGroupRelease(d.instanceBindGroup); d.instanceBindGroup = NULL; }
	if (d.instanceBuffer != NULL) { wgpuBufferRelease(d.instanceBuffer); d.instanceBuffer = NULL; }
	d.instanceCapacity = 0;

	if (d.targetView != NULL) { wgpuTextureViewRelease(d.targetView); d.targetView = NULL; }
	if (d.targetTexture != NULL) { wgpuTextureRelease(d.targetTexture); d.targetTexture = NULL; }

	if (d.textBindGroup != NULL) { wgpuBindGroupRelease(d.textBindGroup); d.textBindGroup = NULL; }
	if (d.sampler != NULL) { wgpuSamplerRelease(d.sampler); d.sampler = NULL; }
	if (d.textView != NULL) { wgpuTextureViewRelease(d.textView); d.textView = NULL; }
	if (d.textTexture != NULL) { wgpuTextureRelease(d.textTexture); d.textTexture = NULL; }

	if (d.uniformBuffer != NULL) { wgpuBufferRelease(d.uniformBuffer); d.uniformBuffer = NULL; }

	if (d.shapePipeline != NULL) { wgpuRenderPipelineRelease(d.shapePipeline); d.shapePipeline = NULL; }
	if (d.textPipeline != NULL) { wgpuRenderPipelineRelease(d.textPipeline); d.textPipeline = NULL; }
	if (d.glyphPipeline != NULL) { wgpuRenderPipelineRelease(d.glyphPipeline); d.glyphPipeline = NULL; }

	if (d.shapeBindGroupLayout != NULL) { wgpuBindGroupLayoutRelease(d.shapeBindGroupLayout); d.shapeBindGroupLayout = NULL; }
	if (d.textBindGroupLayout != NULL) { wgpuBindGroupLayoutRelease(d.textBindGroupLayout); d.textBindGroupLayout = NULL; }
	if (d.glyphBindGroupLayout != NULL) { wgpuBindGroupLayoutRelease(d.glyphBindGroupLayout); d.glyphBindGroupLayout = NULL; }

	if (d.shapeModule != NULL) { wgpuShaderModuleRelease(d.shapeModule); d.shapeModule = NULL; }
	if (d.textModule != NULL) { wgpuShaderModuleRelease(d.textModule); d.textModule = NULL; }
	if (d.glyphModule != NULL) { wgpuShaderModuleRelease(d.glyphModule); d.glyphModule = NULL; }

	// Everything above is gone, so the references begin() took can be dropped.
	if (d.queue != NULL) { wgpuQueueRelease(d.queue); d.queue = NULL; }
	if (d.device != NULL) { wgpuDeviceRelease(d.device); d.device = NULL; }
	if (d.instance != NULL) { wgpuInstanceRelease(d.instance); d.instance = NULL; }

	d.width = 0;
	d.height = 0;
}

bool WGPUOSDRenderer::isReady() const
{
	return _impl->ready;
}

int WGPUOSDRenderer::width() const
{
	return _impl->width;
}

int WGPUOSDRenderer::height() const
{
	return _impl->height;
}

bool WGPUOSDRenderer::render(const uint32_t *textPixels, int textStrideBytes,
                             const WGPUOSDShape *shapes, int shapeCount,
                             const WGPUOSDPoint *points, int pointCount,
                             const WGPUOSDTextRun *runs, int runCount,
                             const char *text, int textLength)
{
	Impl &d = *_impl;

	if (!d.ready)
	{
		return false;
	}

	const bool drawText = (textPixels != NULL) && (textStrideBytes >= d.width * 4);
	bool drawShapes = (shapeCount > 0) && (shapes != NULL) && (d.shapePipeline != NULL);
	bool drawGlyphs = (runCount > 0) && (runs != NULL) && (text != NULL) && (textLength > 0) &&
	                  (d.glyphPipeline != NULL) && (d.glyphBindGroup != NULL);

	if (drawText)
	{
		d.UploadText(textPixels, textStrideBytes);
	}

	if (drawShapes)
	{
		if (!d.EnsureInstanceBuffer((uint32_t)shapeCount))
		{
			drawShapes = false;
		}
		else
		{
			d.instances.resize((size_t)shapeCount);

			for (int i = 0; i < shapeCount; i++)
			{
				d.PackInstance(shapes[i], points, pointCount, d.instances[(size_t)i]);
			}

			wgpuQueueWriteBuffer(d.queue, d.instanceBuffer, 0, d.instances.data(),
			                     (size_t)shapeCount * sizeof(GpuInstance));
		}
	}

	if (drawGlyphs)
	{
		d.PackGlyphs(runs, runCount, text, textLength);

		const uint32_t glyphCount = (uint32_t)d.glyphs.size();
		drawGlyphs = (glyphCount > 0) && d.EnsureGlyphBuffer(glyphCount);

		if (drawGlyphs)
		{
			wgpuQueueWriteBuffer(d.queue, d.glyphBuffer, 0, d.glyphs.data(),
			                     (size_t)glyphCount * sizeof(GpuGlyph));
		}
	}

	WGPUCommandEncoderDescriptor encoderDesc = WGPU_COMMAND_ENCODER_DESCRIPTOR_INIT;
	WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(d.device, &encoderDesc);

	if (encoder == NULL)
	{
		return false;
	}

	WGPURenderPassColorAttachment colorAttachment = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
	colorAttachment.view = d.targetView;
	colorAttachment.loadOp = WGPULoadOp_Clear;
	colorAttachment.storeOp = WGPUStoreOp_Store;
	colorAttachment.clearValue.r = 0.0;
	colorAttachment.clearValue.g = 0.0;
	colorAttachment.clearValue.b = 0.0;
	colorAttachment.clearValue.a = 0.0;

	WGPURenderPassDescriptor passDesc = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
	passDesc.colorAttachmentCount = 1;
	passDesc.colorAttachments = &colorAttachment;

	WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &passDesc);

	if (pass == NULL)
	{
		wgpuCommandEncoderRelease(encoder);
		return false;
	}

	if (drawShapes && (d.instanceBindGroup != NULL))
	{
		wgpuRenderPassEncoderSetPipeline(pass, d.shapePipeline);
		wgpuRenderPassEncoderSetBindGroup(pass, 0, d.instanceBindGroup, 0, NULL);
		wgpuRenderPassEncoderDraw(pass, 6, (uint32_t)shapeCount, 0, 0);
	}

	if (drawGlyphs)
	{
		wgpuRenderPassEncoderSetPipeline(pass, d.glyphPipeline);
		wgpuRenderPassEncoderSetBindGroup(pass, 0, d.glyphBindGroup, 0, NULL);
		wgpuRenderPassEncoderSetVertexBuffer(pass, 0, d.glyphBuffer, 0,
		                                     (uint64_t)d.glyphs.size() * sizeof(GpuGlyph));
		wgpuRenderPassEncoderDraw(pass, 6, (uint32_t)d.glyphs.size(), 0, 0);
	}

	if (drawText)
	{
		wgpuRenderPassEncoderSetPipeline(pass, d.textPipeline);
		wgpuRenderPassEncoderSetBindGroup(pass, 0, d.textBindGroup, 0, NULL);
		wgpuRenderPassEncoderDraw(pass, 6, 1, 0, 0);
	}

	wgpuRenderPassEncoderEnd(pass);
	wgpuRenderPassEncoderRelease(pass);

	WGPUCommandBufferDescriptor commandDesc = WGPU_COMMAND_BUFFER_DESCRIPTOR_INIT;
	WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, &commandDesc);
	wgpuCommandEncoderRelease(encoder);

	if (command == NULL)
	{
		return false;
	}

	wgpuQueueSubmit(d.queue, 1, &command);
	wgpuCommandBufferRelease(command);

	return true;
}

struct ReadbackResult
{
	bool done;
	WGPUMapAsyncStatus status;
};

static void OnBufferMapped(WGPUMapAsyncStatus status, WGPUStringView message, void *userdata1, void *userdata2)
{
	(void)message;
	(void)userdata2;

	ReadbackResult *result = (ReadbackResult *)userdata1;
	result->status = status;
	result->done = true;
}

bool WGPUOSDRenderer::readPixels(uint8_t *dst, int dstStrideBytes)
{
	Impl &d = *_impl;

	if (!d.ready || (dst == NULL) || (dstStrideBytes < d.width * 4))
	{
		return false;
	}

	const size_t rowBytes = (size_t)d.width * 4;
	const uint32_t pitch = (uint32_t)((rowBytes + 255) & ~(size_t)255);

	WGPUBufferDescriptor bufferDesc = WGPU_BUFFER_DESCRIPTOR_INIT;
	bufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
	bufferDesc.size = (uint64_t)pitch * (uint64_t)d.height;

	WGPUBuffer buffer = wgpuDeviceCreateBuffer(d.device, &bufferDesc);

	if (buffer == NULL)
	{
		return false;
	}

	WGPUCommandEncoderDescriptor encoderDesc = WGPU_COMMAND_ENCODER_DESCRIPTOR_INIT;
	WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(d.device, &encoderDesc);

	WGPUTexelCopyTextureInfo source = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
	source.texture = d.targetTexture;

	WGPUTexelCopyBufferInfo destination = WGPU_TEXEL_COPY_BUFFER_INFO_INIT;
	destination.buffer = buffer;
	destination.layout.bytesPerRow = pitch;
	destination.layout.rowsPerImage = (uint32_t)d.height;

	WGPUExtent3D copySize = WGPU_EXTENT_3D_INIT;
	copySize.width = (uint32_t)d.width;
	copySize.height = (uint32_t)d.height;
	copySize.depthOrArrayLayers = 1;

	wgpuCommandEncoderCopyTextureToBuffer(encoder, &source, &destination, &copySize);

	WGPUCommandBufferDescriptor commandDesc = WGPU_COMMAND_BUFFER_DESCRIPTOR_INIT;
	WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, &commandDesc);
	wgpuCommandEncoderRelease(encoder);

	wgpuQueueSubmit(d.queue, 1, &command);
	wgpuCommandBufferRelease(command);

	ReadbackResult result;
	result.done = false;
	result.status = WGPUMapAsyncStatus_Error;

	WGPUBufferMapCallbackInfo callback = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
	callback.mode = WGPUCallbackMode_AllowProcessEvents;
	callback.callback = OnBufferMapped;
	callback.userdata1 = &result;

	wgpuBufferMapAsync(buffer, WGPUMapMode_Read, 0, bufferDesc.size, callback);

	const std::chrono::steady_clock::time_point deadline =
		std::chrono::steady_clock::now() + std::chrono::seconds(10);

	while (!result.done && (std::chrono::steady_clock::now() < deadline))
	{
		wgpuInstanceProcessEvents(d.instance);
		std::this_thread::yield();
	}

	bool ok = result.done && (result.status == WGPUMapAsyncStatus_Success);

	if (ok)
	{
		const uint8_t *mapped = (const uint8_t *)wgpuBufferGetConstMappedRange(buffer, 0, bufferDesc.size);

		if (mapped != NULL)
		{
			for (int y = 0; y < d.height; y++)
			{
				memcpy(dst + (size_t)y * (size_t)dstStrideBytes,
				       mapped + (size_t)y * pitch, rowBytes);
			}
		}
		else
		{
			ok = false;
		}
	}

	if (result.done)
	{
		wgpuBufferUnmap(buffer);
	}

	wgpuBufferRelease(buffer);

	return ok;
}

void *WGPUOSDRenderer::textureView() const
{
	return _impl->targetView;
}

#else // !ENABLE_WGPU

// The class still has to link when the build has no WebGPU, so the renderer is
// a stub that always reports failure. The AGG-free recorder in
// wgpu_osd_draw.cpp keeps working independently of it.
struct WGPUOSDRenderer::Impl
{
};

WGPUOSDRenderer::WGPUOSDRenderer() : _impl(NULL) {}
WGPUOSDRenderer::~WGPUOSDRenderer() {}

bool WGPUOSDRenderer::setFonts(const WGPUOSDFont *, int) { (void)kGlyphShader; return false; }
bool WGPUOSDRenderer::begin(void *, void *, void *, int, int) { (void)kTextShader; return false; }
void WGPUOSDRenderer::kill() {}
bool WGPUOSDRenderer::hasFont(const void *) const { return false; }
bool WGPUOSDRenderer::isReady() const { return false; }
int WGPUOSDRenderer::width() const { return 0; }
int WGPUOSDRenderer::height() const { return 0; }

bool WGPUOSDRenderer::render(const uint32_t *, int, const WGPUOSDShape *, int, const WGPUOSDPoint *, int,
                             const WGPUOSDTextRun *, int, const char *, int)
{
	return false;
}

bool WGPUOSDRenderer::readPixels(uint8_t *, int) { return false; }
void *WGPUOSDRenderer::textureView() const { return NULL; }

#endif // ENABLE_WGPU
