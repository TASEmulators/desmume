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

#include "WGPURender.h"

#if !defined(ENABLE_WGPU)

GPU3DInterface gpu3DWGPU = {
	"WebGPU (not built)",
	NULL,
	NULL
};

#else

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <map>
#include <string>
#include <chrono>
#include <thread>

// wgpu-native extensions (instance backends, adapter info, ...) live in wgpu.h,
// on top of the standard webgpu.h that WGPURender.h already pulls in.
#include "wgpu.h"

#include "common.h"
#include "GPU.h"
#include "gfx3d.h"
#include "NDSSystem.h"
#include "filter/filter.h"
#include "utils/colorspacehandler/colorspacehandler.h"

// ---------------------------------------------------------------------------
// Shader state bit layouts
// ---------------------------------------------------------------------------

#define WGPU_FRAME_FOG_ALPHA_ONLY	0x01u
#define WGPU_FRAME_ALPHA_TEST		0x04u
#define WGPU_FRAME_TEX_SMOOTHING	0x08u
#define WGPU_FRAME_TOON_SHADING		0x10u

#define WGPU_POLY_WIREFRAME			0x01u
#define WGPU_POLY_ENABLE_FOG		0x02u
#define WGPU_POLY_NEW_DEPTH			0x04u
#define WGPU_POLY_ENABLE_TEX		0x08u
#define WGPU_POLY_SINGLE_BIT_ALPHA	0x10u
#define WGPU_POLY_BACK_FACING		0x20u
#define WGPU_POLY_DRAWABLE			0x40u
#define WGPU_POLY_TRANSLUCENT		0x80u

// ---------------------------------------------------------------------------
// Shaders
// ---------------------------------------------------------------------------

static const char *kWGSL_Common = R"WGSH(
struct Frame
{
	fogColor: vec4f,
	clearColor: vec4f,
	fogOffset: f32,
	fogStep: f32,
	alphaTestRef: f32,
	flags: u32,
	clearPolyID: f32,
	clearDepth: f32,
	pad0: f32,
	pad1: f32,
};

struct PolyState
{
	id: u32,
	mode: u32,
	alpha: u32,
	flags: u32,
	texSizeS: u32,
	texSizeT: u32,
	wrap: u32,
	pad0: u32,
	vpX: u32,
	vpY: u32,
	vpW: u32,
	vpH: u32,
};

struct Tables
{
	edgeColor: array<vec4f, 8>,
	toonColor: array<vec4f, 32>,
	fogDensity: array<f32, 32>,
};

fn mirrorRepeat(t: f32) -> f32
{
	let f = fract(t * 0.5);
	return 1.0 - abs(f * 2.0 - 1.0);
}

fn wrapCoord(uv: vec2f, wrap: u32) -> vec2f
{
	var r = uv;

	if ((wrap & 1u) != 0u)
	{
		if ((wrap & 2u) != 0u) { r.x = mirrorRepeat(r.x); } else { r.x = fract(r.x); }
	}

	if ((wrap & 4u) != 0u)
	{
		if ((wrap & 8u) != 0u) { r.y = mirrorRepeat(r.y); } else { r.y = fract(r.y); }
	}

	return r;
}
)WGSH";

// Geometry pass. gfx3d hands us vertices that are already in NDS clip space
// (scaled by 4096), with y pointing up, exactly like OpenGL's, and z in [-w, w].
// WebGPU's framebuffer row 0 is the NDC y = +1 edge, and so is the NDS layer's
// first scanline, so y can be used as-is; only z needs the remap to [0, w].
static const char *kWGSL_Geometry = R"WGSH(
@group(0) @binding(0) var<uniform> frame: Frame;
@group(0) @binding(1) var<storage, read> polys: array<PolyState>;
@group(0) @binding(2) var texObject: texture_2d<f32>;
@group(0) @binding(3) var sampNear: sampler;
@group(0) @binding(4) var sampLinear: sampler;
@group(0) @binding(5) var<storage, read> tables: Tables;

struct VSIn
{
	@location(0) pos: vec4i,
	@location(1) uv: vec2i,
	@location(2) color: vec4u,
	@location(3) polyIndex: u32,
};

struct VSOut
{
	@builtin(position) clip: vec4f,
	@location(0) uv: vec2f,
	@location(1) color: vec4f,
	@location(2) @interpolate(flat) polyIndex: u32,
};

struct FSOut
{
	@location(0) color: vec4f,
	@location(1) polyID: vec4f,
	@location(2) fogAttr: vec4f,
	@builtin(frag_depth) depth: f32,
};

@vertex
fn vsGeom(input: VSIn) -> VSOut
{
	let ps = polys[input.polyIndex];
	var out: VSOut;

	let p = vec4f(f32(input.pos.x), f32(input.pos.y), f32(input.pos.z), f32(input.pos.w)) / 4096.0;

	// The NDS viewport is per polygon, but a render pass only has one viewport,
	// so the mapping is folded into the clip-space position here instead.
	let sx = f32(ps.vpW) / 256.0;
	let sy = f32(ps.vpH) / 192.0;
	let ox = (2.0 * f32(ps.vpX) + f32(ps.vpW)) / 256.0 - 1.0;
	let oy = (2.0 * f32(ps.vpY) + f32(ps.vpH)) / 192.0 - 1.0;

	out.clip = vec4f(p.x * sx + ox * p.w,
	                 p.y * sy - oy * p.w,
	                 p.z * 0.5 + p.w * 0.5,
	                 p.w);

	let sizeS = f32(8u << ps.texSizeS);
	let sizeT = f32(8u << ps.texSizeT);
	out.uv = vec2f(f32(input.uv.x) / 16.0 / sizeS, f32(input.uv.y) / 16.0 / sizeT);

	out.color = vec4f(vec3f(input.color.rgb) / 63.0, f32(ps.alpha) / 31.0);
	out.polyIndex = input.polyIndex;

	return out;
}

@fragment
fn fsGeom(input: VSOut) -> FSOut
{
	let ps = polys[input.polyIndex];
	let opaquePass = (ps.flags & 128u) == 0u;
	let enableTexture = (ps.flags & 8u) != 0u;

	var out: FSOut;
	var texColor = vec4f(1.0, 1.0, 1.0, 1.0);

	if (enableTexture)
	{
		let uv = wrapCoord(input.uv, ps.wrap);
		if ((frame.flags & 8u) != 0u)
		{
			texColor = textureSampleLevel(texObject, sampLinear, uv, 0.0);
		}
		else
		{
			texColor = textureSampleLevel(texObject, sampNear, uv, 0.0);
		}
	}

	// Fully transparent texels are discarded in the opaque pass and kept in the
	// translucent pass, matching the reference OpenGL shader.
	if ((ps.flags & 16u) == 0u)
	{
		if (opaquePass)
		{
			if ((ps.mode != 1u) && (texColor.a <= 0.999)) { discard; }
		}
		else
		{
			if (((ps.mode != 1u) && (texColor.a * input.color.a > 0.999)) || ((ps.mode == 1u) && (input.color.a > 0.999))) { discard; }
		}
	}
	else if ((frame.flags & 8u) != 0u)
	{
		// Single-bit alpha textures are un-premultiplied when smoothing is on.
		if (texColor.a < 0.5)
		{
			texColor.a = 0.0;
		}
		else
		{
			texColor = vec4f(texColor.rgb / texColor.a, 1.0);
		}
	}

	var color = texColor * input.color;

	if (ps.mode == 1u)
	{
		let rgb = select(input.color.rgb, mix(input.color.rgb, texColor.rgb, texColor.a), enableTexture);
		color = vec4f(rgb, input.color.a);
	}
	else if (ps.mode == 2u)
	{
		let toon = tables.toonColor[i32(floor(input.color.r * 31.0 + 0.5))].rgb;
		if ((frame.flags & 16u) != 0u)
		{
			color = vec4f(min(texColor.rgb * input.color.r + toon, vec3f(1.0)), color.a);
		}
		else
		{
			color = vec4f(texColor.rgb * toon, color.a);
		}
	}
	else if (ps.mode == 3u)
	{
		color = input.color;
	}

	let isDrawable = (ps.flags & 64u) != 0u;
	if (isDrawable && ((color.a < 0.001) || (((frame.flags & 4u) != 0u) && (color.a < frame.alphaTestRef))))
	{
		discard;
	}

	out.color = color;
	out.polyID = select(vec4f(0.0, 0.0, 0.0, 0.0),
	                    vec4f(f32(ps.id) / 63.0,
	                          select(0.0, 1.0, (ps.flags & 1u) != 0u),
	                          0.0,
	                          select(0.0, 1.0, color.a > 0.999)),
	                    isDrawable);
	out.fogAttr = select(vec4f(0.0, 0.0, 0.0, 0.0),
	                     vec4f(f32(ps.flags & 2u),
	                           0.0,
	                           0.0,
	                           select(0.5, 1.0, color.a > 0.999)),
	                     isDrawable);

	// Quantize to the NDS 24-bit depth buffer. Doing this per fragment (instead
	// of per vertex) avoids cross-GPU interpolation differences, which is the
	// same approach the OpenGL renderer takes.
	out.depth = clamp(floor(input.clip.z * 4194303.0) * (4.0 / 16777215.0), 0.0, 1.0);

	return out;
}

@vertex
fn vsFullscreen(@builtin(vertex_index) vertexIndex: u32) -> @builtin(position) vec4f
{
	var pos = array<vec2f, 3>(vec2f(-1.0, -1.0), vec2f(3.0, -1.0), vec2f(-1.0, 3.0));
	return vec4f(pos[vertexIndex], 0.0, 1.0);
}
)WGSH";

// Postprocess passes (edge marking and fog). Both are fullscreen passes that
// read the polygon ID, fog attribute and depth buffers written by the geometry
// pass.
static const char *kWGSL_Post = R"WGSH(
@group(0) @binding(0) var<uniform> frame: Frame;
@group(0) @binding(1) var texPolyID: texture_2d<f32>;
@group(0) @binding(2) var texFogAttr: texture_2d<f32>;
@group(0) @binding(3) var texDepth: texture_depth_2d;
@group(0) @binding(4) var sampNear: sampler;
@group(0) @binding(5) var<storage, read> tables: Tables;

@vertex
fn vsFullscreen(@builtin(vertex_index) vertexIndex: u32) -> @builtin(position) vec4f
{
	var pos = array<vec2f, 3>(vec2f(-1.0, -1.0), vec2f(3.0, -1.0), vec2f(-1.0, 3.0));
	return vec4f(pos[vertexIndex], 0.0, 1.0);
}

@fragment
fn fsEdgeMark(@builtin(position) fragCoord: vec4f) -> @location(0) vec4f
{
	let size = vec2i(textureDimensions(texPolyID));
	let maxCoord = size - vec2i(1, 1);
	let c = clamp(vec2i(floor(fragCoord.xy)), vec2i(0, 0), maxCoord);

	let p0 = textureLoad(texPolyID, c, 0);
	let p1 = textureLoad(texPolyID, clamp(c + vec2i(1, 0), vec2i(0, 0), maxCoord), 0);
	let p2 = textureLoad(texPolyID, clamp(c + vec2i(0, 1), vec2i(0, 0), maxCoord), 0);
	let p3 = textureLoad(texPolyID, clamp(c + vec2i(-1, 0), vec2i(0, 0), maxCoord), 0);
	let p4 = textureLoad(texPolyID, clamp(c + vec2i(0, -1), vec2i(0, 0), maxCoord), 0);

	let d0 = textureLoad(texDepth, c, 0);
	let d1 = textureLoad(texDepth, clamp(c + vec2i(1, 0), vec2i(0, 0), maxCoord), 0);
	let d2 = textureLoad(texDepth, clamp(c + vec2i(0, 1), vec2i(0, 0), maxCoord), 0);
	let d3 = textureLoad(texDepth, clamp(c + vec2i(-1, 0), vec2i(0, 0), maxCoord), 0);
	let d4 = textureLoad(texDepth, clamp(c + vec2i(0, -1), vec2i(0, 0), maxCoord), 0);

	var outColor = vec4f(0.0, 0.0, 0.0, 0.0);

	if (p0.g < 0.5)
	{
		let id0 = i32(p0.r * 63.0 + 0.5);
		let atRight = fragCoord.x >= f32(size.x) - 1.0;
		let atBottom = fragCoord.y >= f32(size.y) - 1.0;
		let atLeft = fragCoord.x < 1.0;
		let atTop = fragCoord.y < 1.0;

		let isClearValues = (id0 != i32(frame.clearPolyID)) && (d0 < frame.clearDepth);

		let id1 = i32(p1.r * 63.0 + 0.5);
		let id2 = i32(p2.r * 63.0 + 0.5);
		let id3 = i32(p3.r * 63.0 + 0.5);
		let id4 = i32(p4.r * 63.0 + 0.5);

		if (select((id0 != id1) && (d0 >= d1) && (p1.g < 0.5), isClearValues, atRight))
		{
			outColor = select(tables.edgeColor[id1 / 8], tables.edgeColor[id0 / 8], atRight);
		}
		else if (select((id0 != id2) && (d0 >= d2) && (p2.g < 0.5), isClearValues, atBottom))
		{
			outColor = select(tables.edgeColor[id2 / 8], tables.edgeColor[id0 / 8], atBottom);
		}
		else if (select((id0 != id3) && (d0 >= d3) && (p3.g < 0.5), isClearValues, atLeft))
		{
			outColor = select(tables.edgeColor[id3 / 8], tables.edgeColor[id0 / 8], atLeft);
		}
		else if (select((id0 != id4) && (d0 >= d4) && (p4.g < 0.5), isClearValues, atTop))
		{
			outColor = select(tables.edgeColor[id4 / 8], tables.edgeColor[id0 / 8], atTop);
		}
	}

	return outColor;
}

fn fogDensityLookup(t: f32) -> f32
{
	let x = clamp(t, 0.0, 31.0);
	let i = i32(floor(x));
	let f = x - f32(i);
	return mix(tables.fogDensity[i], tables.fogDensity[min(i + 1, 31)], f);
}

@fragment
fn fsFog(@builtin(position) fragCoord: vec4f) -> @location(0) vec4f
{
	let size = vec2i(textureDimensions(texFogAttr));
	let c = clamp(vec2i(floor(fragCoord.xy)), vec2i(0, 0), size - vec2i(1, 1));
	let fogAttr = textureLoad(texFogAttr, c, 0);

	if (fogAttr.r <= 0.999)
	{
		return vec4f(0.0, 0.0, 0.0, 0.0);
	}

	let depth = textureLoad(texDepth, c, 0);
	var densitySelect: f32;

	if (frame.fogStep == 0.0)
	{
		densitySelect = select(0.0, 1.0, depth > (frame.fogOffset / frame.fogStep));
	}
	else
	{
		densitySelect = depth * (1024.0 / frame.fogStep) + (((-frame.fogOffset / frame.fogStep) - 0.5) / 32.0);
	}

	let weight = fogDensityLookup(densitySelect);

	if ((frame.flags & 1u) != 0u)
	{
		return vec4f(0.0, 0.0, 0.0, weight);
	}

	return vec4f(weight, weight, weight, weight);
}
)WGSH";

// Clear-image pass: the NDS "rear plane" capture is copied from native
// resolution source textures into the (possibly upscaled) render targets.
static const char *kWGSL_Clear = R"WGSH(
@group(0) @binding(0) var texCIColor: texture_2d<f32>;
@group(0) @binding(1) var texCIDepth: texture_2d<f32>;
@group(0) @binding(2) var texCIFog: texture_2d<f32>;
@group(0) @binding(3) var sampNear: sampler;

struct ClearVSOut
{
	@builtin(position) clip: vec4f,
	@location(0) uv: vec2f,
};

struct ClearOut
{
	@location(0) color: vec4f,
	@location(1) polyID: vec4f,
	@location(2) fogAttr: vec4f,
	@builtin(frag_depth) depth: f32,
};

@vertex
fn vsClear(@builtin(vertex_index) vertexIndex: u32) -> ClearVSOut
{
	var pos = array<vec2f, 3>(vec2f(-1.0, -1.0), vec2f(3.0, -1.0), vec2f(-1.0, 3.0));
	var out: ClearVSOut;
	out.clip = vec4f(pos[vertexIndex], 0.0, 1.0);
	out.uv = vec2f(pos[vertexIndex].x * 0.5 + 0.5, 0.5 - pos[vertexIndex].y * 0.5);
	return out;
}

@fragment
fn fsClear(input: ClearVSOut) -> ClearOut
{
	var out: ClearOut;
	out.color = textureSampleLevel(texCIColor, sampNear, input.uv, 0.0);
	out.depth = textureSampleLevel(texCIDepth, sampNear, input.uv, 0.0).r;
	out.fogAttr = vec4f(textureSampleLevel(texCIFog, sampNear, input.uv, 0.0).r, 0.0, 0.0, 1.0);
	out.polyID = vec4f(0.0, 0.0, 0.0, 1.0);
	return out;
}
)WGSH";

// Depth resolve pass: WebGPU cannot resolve a multisampled depth attachment, so
// the postprocess passes (edge marking, fog) read a single-sampled copy that
// this pass writes. Sample 0 is enough because every sample comes from the same
// triangle coverage at polygon boundaries only.
static const char *kWGSL_DepthResolve = R"WGSH(
@group(0) @binding(0) var texMSDepth: texture_depth_multisampled_2d;

struct DepthResolveVSOut
{
	@builtin(position) clip: vec4f,
};

@vertex
fn vsDepthResolve(@builtin(vertex_index) vertexIndex: u32) -> DepthResolveVSOut
{
	var pos = array<vec2f, 3>(vec2f(-1.0, -1.0), vec2f(3.0, -1.0), vec2f(-1.0, 3.0));
	var out: DepthResolveVSOut;
	out.clip = vec4f(pos[vertexIndex], 0.0, 1.0);
	return out;
}

@fragment
fn fsDepthResolve(@builtin(position) fragCoord: vec4f) -> @builtin(frag_depth) f32
{
	let c = vec2i(floor(fragCoord.xy));
	return textureLoad(texMSDepth, c, 0);
}
)WGSH";

// ---------------------------------------------------------------------------
// CPU-side structures, byte-identical to the WGSL declarations
// ---------------------------------------------------------------------------

struct FrameUniform
{
	float fogColor[4];
	float clearColor[4];
	float fogOffset;
	float fogStep;
	float alphaTestRef;
	uint32_t flags;
	float clearPolyID;
	float clearDepth;
	float pad0;
	float pad1;
};

struct TablesGPU
{
	float edgeColor[8][4];
	float toonColor[32][4];
	float fogDensity[32];
};

struct PolyStateGPU
{
	uint32_t id;
	uint32_t mode;
	uint32_t alpha;
	uint32_t flags;
	uint32_t texSizeS;
	uint32_t texSizeT;
	uint32_t wrap;
	uint32_t pad0;
	uint32_t vpX;
	uint32_t vpY;
	uint32_t vpW;
	uint32_t vpH;
};

struct VertexGPU
{
	int32_t pos[4];
	int32_t uv[2];
	uint32_t color;
	uint32_t polyIndex;
};

static const size_t kMaxClippedPolys = CLIPPED_POLYLIST_SIZE;
static const size_t kMaxVertices = CLIPPED_POLYLIST_SIZE * 8;

// Frame upload layout, in bytes inside _frameUploadBuffer.
//
// The uniform and the tables live at fixed 256-byte aligned offsets because
// that is what WebGPU requires of uniform/storage bind group offsets. The
// polygon states are indexed by polygon id, so their base must also stay fixed;
// the vertices are appended right after the polygon states that this frame
// actually uses. That way the whole frame is one contiguous range and costs a
// single wgpuQueueWriteBuffer() call.
static const size_t kUploadUniformOffset    = 0;
static const size_t kUploadTablesOffset     = 256;
static const size_t kUploadPolyStateOffset  = (kUploadTablesOffset + sizeof(TablesGPU) + 255) & ~(size_t)255;
static const size_t kUploadPolyStateBytes   = kMaxClippedPolys * sizeof(PolyStateGPU);
static const size_t kUploadVertexCapBytes   = kMaxVertices * sizeof(VertexGPU);
static const size_t kUploadBufferSize       = kUploadPolyStateOffset + kUploadPolyStateBytes + kUploadVertexCapBytes;

// ---------------------------------------------------------------------------
// wgpu-native helpers
// ---------------------------------------------------------------------------

static WGPUStringView WGSLString(const char *str)
{
	WGPUStringView sv;
	sv.data = str;
	sv.length = WGPU_STRLEN;
	return sv;
}

// wgpu-native (as of v29) does not implement wgpuInstanceWaitAny(), so every
// asynchronous request is driven by pumping instance events until its callback
// has run. The callbacks are registered with WGPUCallbackMode_AllowProcessEvents
// for that reason.
static bool WGSLWaitForFlag(WGPUInstance instance, const bool *done)
{
	const std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);

	while (!*done)
	{
		wgpuInstanceProcessEvents(instance);

		if (std::chrono::steady_clock::now() > deadline)
		{
			return false;
		}

		std::this_thread::yield();
	}

	return true;
}

struct AdapterRequestResult
{
	WGPUAdapter adapter;
	WGPURequestAdapterStatus status;
	bool done;
};

static void WGSLOnAdapterRequested(WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message, void *userdata1, void *userdata2)
{
	AdapterRequestResult *result = (AdapterRequestResult *)userdata1;
	result->adapter = adapter;
	result->status = status;
	result->done = true;
}

struct DeviceRequestResult
{
	WGPUDevice device;
	WGPURequestDeviceStatus status;
	bool done;
};

static void WGSLOnDeviceRequested(WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message, void *userdata1, void *userdata2)
{
	DeviceRequestResult *result = (DeviceRequestResult *)userdata1;
	result->device = device;
	result->status = status;
	result->done = true;
}

static void WGSLOnUncapturedError(WGPUDevice const *device, WGPUErrorType type, WGPUStringView message, void *userdata1, void *userdata2)
{
	if (message.data != NULL)
	{
		const size_t len = (message.length == WGPU_STRLEN) ? strlen(message.data) : message.length;
		printf("WGPU: uncaptured error (type %d): %.*s\n", (int)type, (int)len, message.data);
	}
	else
	{
		printf("WGPU: uncaptured error (type %d)\n", (int)type);
	}
}

// ---------------------------------------------------------------------------
// Backend reporting
// ---------------------------------------------------------------------------

// Per-backend renderer entries. The frontend turns these into separate
// "WebGPU - <backend>" choices, so a user can pick Vulkan instead of D3D12 even
// when both are present.
static Render3D *WGPURender_CreateDX12()   { return WGPURender::CreateWithBackend((uint32_t)WGPUInstanceBackend_DX12); }
static Render3D *WGPURender_CreateVulkan() { return WGPURender::CreateWithBackend((uint32_t)WGPUInstanceBackend_Vulkan); }
static Render3D *WGPURender_CreateMetal()  { return WGPURender::CreateWithBackend((uint32_t)WGPUInstanceBackend_Metal); }

struct WGPUBackendOption
{
	uint32_t instanceBackends;
	const char *label;
	Render3D *(*create)();
};

// Backends that can actually run this renderer. wgpu's OpenGL backend is left
// out on purpose: it cannot create the pipelines used here (no storage buffers
// and no depth texture reads in the fragment stage), so offering it would only
// produce a blank screen.
static const WGPUBackendOption kKnownWGPUBackends[] = {
	{ (uint32_t)WGPUInstanceBackend_DX12,   "D3D12",  WGPURender_CreateDX12   },
	{ (uint32_t)WGPUInstanceBackend_Vulkan, "Vulkan", WGPURender_CreateVulkan },
	{ (uint32_t)WGPUInstanceBackend_Metal,  "Metal",  WGPURender_CreateMetal  }
};

static const size_t kKnownWGPUBackendCount = sizeof(kKnownWGPUBackends) / sizeof(kKnownWGPUBackends[0]);

static char sWGPUBackendNames[kKnownWGPUBackendCount][32];
static GPU3DInterface sWGPUBackendInterfaces[kKnownWGPUBackendCount];
static size_t sWGPUBackendInterfaceCount = 0;
static bool sWGPUBackendListReady = false;

static void WGSLCopyStringView(const WGPUStringView &src, char *dst, const size_t dstSize)
{
	if ( (dst == NULL) || (dstSize == 0) )
	{
		return;
	}

	dst[0] = '\0';

	if (src.data == NULL)
	{
		return;
	}

	const size_t len = (src.length == WGPU_STRLEN) ? strlen(src.data) : (size_t)src.length;
	const size_t copyLen = (len < (dstSize - 1)) ? len : (dstSize - 1);

	memcpy(dst, src.data, copyLen);
	dst[copyLen] = '\0';
}

static const char *WGSLBackendTypeName(const WGPUBackendType backendType)
{
	switch (backendType)
	{
		case WGPUBackendType_D3D12:    return "D3D12";
		case WGPUBackendType_D3D11:    return "D3D11";
		case WGPUBackendType_Vulkan:   return "Vulkan";
		case WGPUBackendType_Metal:    return "Metal";
		case WGPUBackendType_OpenGL:   return "OpenGL";
		case WGPUBackendType_OpenGLES: return "OpenGL ES";
		case WGPUBackendType_WebGPU:   return "WebGPU";
		default:                       return NULL;
	}
}

static uint32_t WGSLBackendTypeToInstanceBackend(const WGPUBackendType backendType)
{
	switch (backendType)
	{
		case WGPUBackendType_D3D12:    return (uint32_t)WGPUInstanceBackend_DX12;
		case WGPUBackendType_Vulkan:   return (uint32_t)WGPUInstanceBackend_Vulkan;
		case WGPUBackendType_Metal:    return (uint32_t)WGPUInstanceBackend_Metal;
		case WGPUBackendType_OpenGL:
		case WGPUBackendType_OpenGLES: return (uint32_t)WGPUInstanceBackend_GL;
		default:                       return 0;
	}
}

// Instance creation always goes through here so that a renderer entry can pin
// the backend it advertises. A mask of zero enables everything wgpu-native was
// built with.
static WGPUInstance WGSLCreateInstance(const uint32_t instanceBackends)
{
	WGPUInstanceExtras extras;
	memset(&extras, 0, sizeof(extras));
	extras.chain.sType = (WGPUSType)WGPUSType_InstanceExtras;
	extras.backends = (WGPUInstanceBackend)instanceBackends;

	WGPUInstanceDescriptor instanceDesc = WGPU_INSTANCE_DESCRIPTOR_INIT;
	instanceDesc.nextInChain = &extras.chain;

	return wgpuCreateInstance(&instanceDesc);
}

// Queries an adapter and returns the instance-backend mask it belongs to, or 0
// when it is a software adapter. Software adapters (WARP, lavapipe, ...) are
// deliberately kept out of the renderer list: the emulator ships its own
// SoftRasterizer for that job and they are far too slow for 3D.
static uint32_t WGSLAdapterInstanceBackend(WGPUAdapter adapter)
{
	WGPUAdapterInfo info = WGPU_ADAPTER_INFO_INIT;

	if (wgpuAdapterGetInfo(adapter, &info) != WGPUStatus_Success)
	{
		return 0;
	}

	const char *backendName = WGSLBackendTypeName(info.backendType);

	char deviceName[128];
	char vendorName[64];
	WGSLCopyStringView(info.device, deviceName, sizeof(deviceName));
	WGSLCopyStringView(info.vendor, vendorName, sizeof(vendorName));

	uint32_t instanceBackends = WGSLBackendTypeToInstanceBackend(info.backendType);

	if (info.adapterType == WGPUAdapterType_CPU)
	{
		instanceBackends = 0;
	}

	printf("WGPU: adapter backend = %s, device = %s, vendor = %s%s\n",
	       (backendName != NULL) ? backendName : "unknown", deviceName, vendorName,
	       (instanceBackends == 0) ? " (skipped: software adapter)" : "");
	fflush(stdout);

	wgpuAdapterInfoFreeMembers(info);

	return instanceBackends;
}

// Asks a single backend for an adapter. Only used when adapter enumeration is
// unavailable in the linked wgpu-native build.
static uint32_t WGSLProbeBackend(const uint32_t instanceBackends)
{
	WGPUInstance instance = WGSLCreateInstance(instanceBackends);
	if (instance == NULL)
	{
		return 0;
	}

	WGPURequestAdapterOptions adapterOptions = WGPU_REQUEST_ADAPTER_OPTIONS_INIT;
	adapterOptions.powerPreference = WGPUPowerPreference_HighPerformance;

	AdapterRequestResult adapterResult;
	adapterResult.adapter = NULL;
	adapterResult.status = WGPURequestAdapterStatus_Error;
	adapterResult.done = false;

	WGPURequestAdapterCallbackInfo adapterCallback = WGPU_REQUEST_ADAPTER_CALLBACK_INFO_INIT;
	adapterCallback.mode = WGPUCallbackMode_AllowProcessEvents;
	adapterCallback.callback = WGSLOnAdapterRequested;
	adapterCallback.userdata1 = &adapterResult;

	wgpuInstanceRequestAdapter(instance, &adapterOptions, adapterCallback);
	WGSLWaitForFlag(instance, &adapterResult.done);

	uint32_t probedBackends = 0;

	if ( (adapterResult.status == WGPURequestAdapterStatus_Success) && (adapterResult.adapter != NULL) )
	{
		probedBackends = WGSLAdapterInstanceBackend(adapterResult.adapter);
		wgpuAdapterRelease(adapterResult.adapter);
	}

	wgpuInstanceRelease(instance);

	return probedBackends;
}

void WGPURender_InitBackendList()
{
	if (sWGPUBackendListReady)
	{
		return;
	}

	sWGPUBackendListReady = true;

	uint32_t availableBackends = 0;

	WGPUInstance instance = WGSLCreateInstance(0);
	if (instance != NULL)
	{
		size_t adapterCount = wgpuInstanceEnumerateAdapters(instance, NULL, NULL);

		if (adapterCount > 0)
		{
			std::vector<WGPUAdapter> adapters(adapterCount);
			adapterCount = wgpuInstanceEnumerateAdapters(instance, NULL, &adapters[0]);

			for (size_t i = 0; i < adapterCount; i++)
			{
				if (adapters[i] == NULL)
				{
					continue;
				}

				availableBackends |= WGSLAdapterInstanceBackend(adapters[i]);
				wgpuAdapterRelease(adapters[i]);
			}
		}
		else
		{
			for (size_t i = 0; i < kKnownWGPUBackendCount; i++)
			{
				availableBackends |= WGSLProbeBackend(kKnownWGPUBackends[i].instanceBackends);
			}
		}

		wgpuInstanceRelease(instance);
	}

	// Ask wgpu-native which backend it would pick on its own. That entry goes
	// first, so a plain "use WebGPU" request (--3d-render wgpu, or an older
	// settings file) keeps landing on the same backend as before.
	const uint32_t preferredBackends = WGSLProbeBackend(0);
	availableBackends |= preferredBackends;

	for (size_t pass = 0; pass < 2; pass++)
	{
		for (size_t i = 0; i < kKnownWGPUBackendCount; i++)
		{
			const uint32_t backendMask = kKnownWGPUBackends[i].instanceBackends;

			if ( (availableBackends & backendMask) == 0 )
			{
				continue;
			}

			if ( (pass == 0) != (backendMask == preferredBackends) )
			{
				continue;
			}

			char *name = sWGPUBackendNames[sWGPUBackendInterfaceCount];
			snprintf(name, 32, "WebGPU - %s", kKnownWGPUBackends[i].label);

			GPU3DInterface *rendererInterface = &sWGPUBackendInterfaces[sWGPUBackendInterfaceCount];
			rendererInterface->name = name;
			rendererInterface->NDS_3D_Init = kKnownWGPUBackends[i].create;
			rendererInterface->NDS_3D_Close = WGPURender::Destroy;

			sWGPUBackendInterfaceCount++;
		}
	}
}

size_t WGPURender_GetBackendCount()
{
	return sWGPUBackendInterfaceCount;
}

GPU3DInterface *WGPURender_GetBackendInterface(const size_t index)
{
	if (index >= sWGPUBackendInterfaceCount)
	{
		return NULL;
	}

	return &sWGPUBackendInterfaces[index];
}

struct BufferMapResult
{
	bool success;
	bool done;
};

static void WGSLOnBufferMapped(WGPUMapAsyncStatus status, WGPUStringView message, void *userdata1, void *userdata2)
{
	BufferMapResult *result = (BufferMapResult *)userdata1;
	result->success = (status == WGPUMapAsyncStatus_Success);
	result->done = true;
}

// ---------------------------------------------------------------------------
// WGPUColorOut
// ---------------------------------------------------------------------------

WGPUColorOut::WGPUColorOut(size_t w, size_t h)
{
	_bufferCPU32[0] = NULL;
	_bufferCPU32[1] = NULL;

	this->SetSize(w, h);

	_state[0] = AsyncReadState_Free;
	_state[1] = AsyncReadState_Free;
}

WGPUColorOut::~WGPUColorOut()
{
	free_aligned(this->_buffer32[0]);
	free_aligned(this->_buffer32[1]);
	free_aligned(_bufferCPU32[0]);
	free_aligned(_bufferCPU32[1]);
}

Render3DError WGPUColorOut::SetSize(size_t w, size_t h)
{
	if ( (w == 0) || (h == 0) )
	{
		return RENDER3DERROR_INVALID_VALUE;
	}

	this->Render3DColorOut::SetSize(w, h);

	free_aligned(this->_buffer32[0]);
	free_aligned(this->_buffer32[1]);
	free_aligned(_bufferCPU32[0]);
	free_aligned(_bufferCPU32[1]);

	this->_buffer32[0] = (Color4u8 *)malloc_alignedCacheLine(w * h * sizeof(Color4u8));
	this->_buffer32[1] = (Color4u8 *)malloc_alignedCacheLine(w * h * sizeof(Color4u8));
	_bufferCPU32[0] = (Color4u8 *)malloc_alignedCacheLine(w * h * sizeof(Color4u8));
	_bufferCPU32[1] = (Color4u8 *)malloc_alignedCacheLine(w * h * sizeof(Color4u8));

	memset(this->_buffer32[0], 0, w * h * sizeof(Color4u8));
	memset(this->_buffer32[1], 0, w * h * sizeof(Color4u8));
	memset(_bufferCPU32[0], 0, w * h * sizeof(Color4u8));
	memset(_bufferCPU32[1], 0, w * h * sizeof(Color4u8));

	return RENDER3DERROR_NOERR;
}

Render3DError WGPUColorOut::FillZero()
{
	memset(this->_buffer32[0], 0, this->_framebufferSize32);
	memset(this->_buffer32[1], 0, this->_framebufferSize32);
	memset(_bufferCPU32[0], 0, this->_framebufferSize32);
	memset(_bufferCPU32[1], 0, this->_framebufferSize32);

	return RENDER3DERROR_NOERR;
}

Render3DError WGPUColorOut::FillColor32(const Color4u8 *src, const bool isSrcNativeSize)
{
	return RENDER3DERROR_NOERR;
}

size_t WGPUColorOut::BindRead32()
{
	WGPURender *renderer = (WGPURender *)this->_renderer;
	const size_t oldReadingIdx = this->_currentReadingIdx32;

	if (this->_currentUsageIdx != RENDER3D_RESOURCE_INDEX_NONE)
	{
		this->_state[this->_currentUsageIdx] = AsyncReadState_Reading;
		this->_currentReadingIdx32 = this->_currentUsageIdx;
		this->_currentUsageIdx = RENDER3D_RESOURCE_INDEX_NONE;
	}
	else if (this->_currentReadyIdx != RENDER3D_RESOURCE_INDEX_NONE)
	{
		this->_state[this->_currentReadyIdx] = AsyncReadState_Reading;
		this->_currentReadingIdx32 = this->_currentReadyIdx;
		this->_currentReadyIdx = RENDER3D_RESOURCE_INDEX_NONE;
	}

	if ( (oldReadingIdx != RENDER3D_RESOURCE_INDEX_NONE) && (this->_currentReadingIdx32 != oldReadingIdx) )
	{
		this->_state[oldReadingIdx] = AsyncReadState_Free;
	}

	if (this->_currentReadingIdx32 == RENDER3D_RESOURCE_INDEX_NONE)
	{
		return this->_currentReadingIdx32;
	}

	if (renderer != NULL)
	{
		renderer->ReadbackFramebuffer(this->_currentReadingIdx32,
		                              (u8 *)this->_buffer32[this->_currentReadingIdx32],
		                              (u8 *)_bufferCPU32[this->_currentReadingIdx32]);
	}

	return this->_currentReadingIdx32;
}

const Color4u8* WGPUColorOut::GetFramebuffer32() const
{
	if (this->_currentReadingIdx32 != RENDER3D_RESOURCE_INDEX_NONE)
	{
		if (this->_state[this->_currentReadingIdx32] == AsyncReadState_Reading)
		{
			return _bufferCPU32[this->_currentReadingIdx32];
		}
	}

	return NULL;
}

// ---------------------------------------------------------------------------
// WGPURender
// ---------------------------------------------------------------------------

WGPURender::WGPURender()
	: WGPURender(0u)
{
}

// WebGPU only allows 1 or 4 samples per pixel, so a request for 2x is rounded
// up rather than silently dropped.
static uint32_t WGSLRequestedSampleCount()
{
	const int requested = CommonSettings.GFX3D_Renderer_MultisampleSize;
	return (requested >= 2) ? 4u : 1u;
}

WGPURenderTexture::WGPURenderTexture(TEXIMAGE_PARAM texAttributes, u32 palAttributes) : Render3DTexture(texAttributes, palAttributes)
{
	_unpackBuffer = NULL;
	_upscaleBuffer = NULL;
	_baseData = NULL;
}

void WGPURenderTexture::SetUnpackBuffer(u32 *unpackBuffer)
{
	_unpackBuffer = unpackBuffer;
}

void WGPURenderTexture::SetUpscaleBuffer(u32 *upscaleBuffer)
{
	_upscaleBuffer = upscaleBuffer;
}

void WGPURenderTexture::SetDeposterizeBuffer(void *dstBuffer, void *workingBuffer)
{
	this->_deposterizeDstSurface.Surface = (unsigned char *)dstBuffer;
	this->_deposterizeDstSurface.workingSurface[0] = (unsigned char *)workingBuffer;
}

void WGPURenderTexture::Load()
{
	_baseData = _unpackBuffer;

	if (_unpackBuffer == NULL)
	{
		return;
	}

	TextureStore::Load(_unpackBuffer);

	// The unpack surface is what deposterize reads from; the destination and its
	// working surface are owned by the renderer (Render3D allocates them in
	// SetTextureProcessingProperties()).
	this->_deposterizeSrcSurface.Surface = (unsigned char *)_unpackBuffer;

	if ( this->_useDeposterize &&
	     (this->_deposterizeDstSurface.Surface != NULL) &&
	     (this->_deposterizeDstSurface.workingSurface[0] != NULL) )
	{
		RenderDeposterize(this->_deposterizeSrcSurface, this->_deposterizeDstSurface);
		_baseData = (const u32 *)this->_deposterizeDstSurface.Surface;
	}

	this->_isLoadNeeded = false;
}

const u32 *WGPURenderTexture::GetMipLevelData(const size_t level, const size_t mipLevelCount,
										size_t &outWidth, size_t &outHeight)
{
	const size_t scale = (level < mipLevelCount) ? ((size_t)1 << (mipLevelCount - 1 - level)) : 1;

	// Without an upscale buffer there is nothing to generate, so every level
	// falls back to the native image (the caller never asks for those levels in
	// that case).
	if ( (scale <= 1) || (_upscaleBuffer == NULL) || (_baseData == NULL) )
	{
		outWidth = this->_sizeS;
		outHeight = this->_sizeT;
		return _baseData;
	}

	outWidth = this->_sizeS * scale;
	outHeight = this->_sizeT * scale;

	if (scale == 2)
	{
		this->_Upscale<2>(_baseData, _upscaleBuffer);
	}
	else
	{
		this->_Upscale<4>(_baseData, _upscaleBuffer);
	}

	return _upscaleBuffer;
}

WGPURender::WGPURender(const uint32_t instanceBackends)
{
	_instanceBackends = instanceBackends;

	_deviceInfo.renderID = RENDERID_WGPU;
	_deviceInfo.renderName = "WebGPU";
	_deviceInfo.isTexturingSupported = true;
	_deviceInfo.isEdgeMarkSupported = true;
	_deviceInfo.isFogSupported = true;
	_deviceInfo.isTextureSmoothingSupported = true;
	_deviceInfo.maxAnisotropy = 1.0f;
	// WebGPU guarantees 4x MSAA for the formats used here; anything else must
	// fall back to 1 sample.
	_deviceInfo.maxSamples = 4;

	_instance = NULL;
	_adapter = NULL;
	_device = NULL;
	_queue = NULL;
	_encoder = NULL;

	_isDeviceReady = false;
	_hasFailed = false;

	_sampleCount = 1;
	_colorTex = NULL;
	_colorView = NULL;
	_polyIDTex = NULL;
	_polyIDView = NULL;
	_fogTex = NULL;
	_fogView = NULL;
	_depthTex = NULL;
	_depthView = NULL;

	_colorTexMS = NULL;
	_colorViewMS = NULL;
	_polyIDTexMS = NULL;
	_polyIDViewMS = NULL;
	_fogTexMS = NULL;
	_fogViewMS = NULL;
	_depthTexMS = NULL;
	_depthViewMS = NULL;

	_clearColorTex = NULL;
	_clearColorView = NULL;
	_clearDepthTex = NULL;
	_clearDepthView = NULL;
	_clearFogTex = NULL;
	_clearFogView = NULL;

	_frameUploadBuffer = NULL;
	_frameUploadVertexOffset = kUploadPolyStateOffset;
	_frameUploadVertexBytes = 0;
	_readbackBuffer[0] = NULL;
	_readbackBuffer[1] = NULL;
	_readbackBufferSize = 0;

	_shaderModuleGeometry = NULL;
	_shaderModulePost = NULL;
	_shaderModuleClear = NULL;
	_shaderModuleDepthResolve = NULL;

	_geomBGL = NULL;
	_postBGL = NULL;
	_clearBGL = NULL;
	_depthResolveBGL = NULL;
	_geomLayout = NULL;
	_postLayout = NULL;
	_clearLayout = NULL;
	_depthResolveLayout = NULL;

	_samplerNearest = NULL;
	_samplerLinear = NULL;

	_postBindGroup = NULL;
	_clearBindGroup = NULL;
	_depthResolveBindGroup = NULL;

	_whiteTex = NULL;
	_whiteView = NULL;
	_whiteBindGroup = NULL;

	memset(_geomPipelines, 0, sizeof(_geomPipelines));
	_edgePipeline = NULL;
	_fogPipeline = NULL;
	_clearPipeline = NULL;
	_depthResolvePipeline = NULL;

	_currentState = NULL;
	_useClearImage = false;

	_texUnpackBuffer = (u8 *)malloc_alignedCacheLine(1024 * 1024 * sizeof(u32));
	memset(_frameUniformData, 0, sizeof(_frameUniformData));

	_wgpuColorOut = new WGPUColorOut(GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT);
	_colorOut = _wgpuColorOut;
	_colorOut->SetRenderer(this);
}

WGPURender::~WGPURender()
{
	this->_TeardownDevice();

	free_aligned(_texUnpackBuffer);
	_texUnpackBuffer = NULL;

	_colorOut = NULL;

	if (_wgpuColorOut != NULL)
	{
		delete _wgpuColorOut;
		_wgpuColorOut = NULL;
	}
}

Render3D* WGPURender::Create()
{
	return new WGPURender();
}

Render3D* WGPURender::CreateWithBackend(const uint32_t instanceBackends)
{
	return new WGPURender(instanceBackends);
}

void WGPURender::Destroy()
{
	if (CurrentRenderer != BaseRenderer)
	{
		WGPURender *oldRenderer = (WGPURender *)CurrentRenderer;
		CurrentRenderer = BaseRenderer;
		delete oldRenderer;
	}
}

Render3DError WGPURender::ApplyRenderingSettings(const GFX3D_State &renderState)
{
	Render3DError error = this->Render3D::ApplyRenderingSettings(renderState);
	if (error != RENDER3DERROR_NOERR)
	{
		return error;
	}

	const uint32_t requestedSampleCount = WGSLRequestedSampleCount();
	if (requestedSampleCount == _sampleCount)
	{
		return RENDER3DERROR_NOERR;
	}

	_sampleCount = requestedSampleCount;

	if (!this->_isDeviceReady)
	{
		// The device is created lazily on the first frame; it will pick up the
		// new sample count when it builds the render targets and pipelines.
		return RENDER3DERROR_NOERR;
	}

	// The sample count is baked into both the pipelines and the render targets,
	// so changing it means rebuilding them. Texture views are unaffected.
	this->_FlushTextureMap();
	this->_DestroyRenderTargets();
	this->_DestroyPipelines();

	// Targets first: they fall back to one sample if the multisampled ones
	// cannot be created.
	this->_CreateRenderTargets(this->_framebufferWidth, this->_framebufferHeight);

	if (this->_CreatePipelines() != RENDER3DERROR_NOERR)
	{
		return RENDER3DERROR_INVALID_VALUE;
	}

	this->_CreateBindGroups();

	return RENDER3DERROR_NOERR;
}

Render3DError WGPURender::Reset()
{
	this->_FlushTextureMap();
	this->Render3D::Reset();
	_useClearImage = false;

	return RENDER3DERROR_NOERR;
}

Render3DError WGPURender::RenderPowerOff()
{
	if (!this->_isPoweredOn)
	{
		return RENDER3DERROR_NOERR;
	}

	this->_isPoweredOn = false;
	this->_colorOut->FillZero();

	return RENDER3DERROR_NOERR;
}

Render3DError WGPURender::VramReconfigureSignal()
{
	this->_FlushTextureMap();
	texCache.Invalidate();

	return RENDER3DERROR_NOERR;
}

Render3DError WGPURender::SetFramebufferSize(size_t w, size_t h)
{
	if (w < GPU_FRAMEBUFFER_NATIVE_WIDTH || h < GPU_FRAMEBUFFER_NATIVE_HEIGHT)
	{
		return RENDER3DERROR_NOERR;
	}

	if ( (w == this->_framebufferWidth) && (h == this->_framebufferHeight) && this->_isDeviceReady )
	{
		return RENDER3DERROR_NOERR;
	}

	this->_framebufferWidth = w;
	this->_framebufferHeight = h;
	this->_framebufferPixCount = w * h;
	this->_framebufferColorSizeBytes = w * h * sizeof(Color4u8);
	this->_colorOut->SetSize(w, h);

	if (!this->_isDeviceReady)
	{
		return RENDER3DERROR_NOERR;
	}

	this->_FlushTextureMap();

	Render3DError error = this->_CreateRenderTargets(w, h);
	if (error != RENDER3DERROR_NOERR)
	{
		return error;
	}

	this->_CreateBindGroups();

	return RENDER3DERROR_NOERR;
}

// --- device lifetime -------------------------------------------------------

Render3DError WGPURender::_SetupDevice()
{
	if (_isDeviceReady)
	{
		return RENDER3DERROR_NOERR;
	}

	if (_hasFailed)
	{
		return RENDER3DERROR_INVALID_VALUE;
	}

	WGPUInstance instance = WGSLCreateInstance(_instanceBackends);
	if (instance == NULL)
	{
		printf("WGPU: wgpuCreateInstance() failed\n");
		_hasFailed = true;
		return RENDER3DERROR_INVALID_VALUE;
	}

	_instance = instance;

	WGPURequestAdapterOptions adapterOptions = WGPU_REQUEST_ADAPTER_OPTIONS_INIT;
	adapterOptions.powerPreference = WGPUPowerPreference_HighPerformance;

	AdapterRequestResult adapterResult;
	adapterResult.adapter = NULL;
	adapterResult.status = WGPURequestAdapterStatus_Error;
	adapterResult.done = false;

	WGPURequestAdapterCallbackInfo adapterCallback = WGPU_REQUEST_ADAPTER_CALLBACK_INFO_INIT;
	adapterCallback.mode = WGPUCallbackMode_AllowProcessEvents;
	adapterCallback.callback = WGSLOnAdapterRequested;
	adapterCallback.userdata1 = &adapterResult;

	wgpuInstanceRequestAdapter(_instance, &adapterOptions, adapterCallback);
	WGSLWaitForFlag(_instance, &adapterResult.done);

	if ( (adapterResult.status != WGPURequestAdapterStatus_Success) || (adapterResult.adapter == NULL) )
	{
		printf("WGPU: no suitable adapter found\n");
		_hasFailed = true;
		return RENDER3DERROR_INVALID_VALUE;
	}

	_adapter = adapterResult.adapter;
	WGSLAdapterInstanceBackend(_adapter);

	WGPUDeviceDescriptor deviceDesc = WGPU_DEVICE_DESCRIPTOR_INIT;
	deviceDesc.label = WGSLString("DeSmuME WebGPU device");
	deviceDesc.uncapturedErrorCallbackInfo.callback = WGSLOnUncapturedError;

	DeviceRequestResult deviceResult;
	deviceResult.device = NULL;
	deviceResult.status = WGPURequestDeviceStatus_Error;
	deviceResult.done = false;

	WGPURequestDeviceCallbackInfo deviceCallback = WGPU_REQUEST_DEVICE_CALLBACK_INFO_INIT;
	deviceCallback.mode = WGPUCallbackMode_AllowProcessEvents;
	deviceCallback.callback = WGSLOnDeviceRequested;
	deviceCallback.userdata1 = &deviceResult;

	wgpuAdapterRequestDevice(_adapter, &deviceDesc, deviceCallback);
	WGSLWaitForFlag(_instance, &deviceResult.done);

	if ( (deviceResult.status != WGPURequestDeviceStatus_Success) || (deviceResult.device == NULL) )
	{
		printf("WGPU: wgpuAdapterRequestDevice() failed\n");
		_hasFailed = true;
		return RENDER3DERROR_INVALID_VALUE;
	}

	_device = deviceResult.device;
	_queue = wgpuDeviceGetQueue(_device);

	_isDeviceReady = true;
	return RENDER3DERROR_NOERR;
}

void WGPURender::_TeardownDevice()
{
	this->_FlushTextureMap();
	this->_DestroyRenderTargets();
	this->_DestroyPipelines();

	if (_frameUploadBuffer) { wgpuBufferRelease(_frameUploadBuffer); _frameUploadBuffer = NULL; }
	_frameUploadVertexOffset = kUploadPolyStateOffset;
	_frameUploadVertexBytes = 0;
	_frameUploadStaging.clear();
}

void WGPURender::_DestroyPipelines()
{
	if (_postBindGroup) { wgpuBindGroupRelease(_postBindGroup); _postBindGroup = NULL; }
	if (_clearBindGroup) { wgpuBindGroupRelease(_clearBindGroup); _clearBindGroup = NULL; }
	if (_depthResolveBindGroup) { wgpuBindGroupRelease(_depthResolveBindGroup); _depthResolveBindGroup = NULL; }

	if (_edgePipeline) { wgpuRenderPipelineRelease(_edgePipeline); _edgePipeline = NULL; }
	if (_fogPipeline) { wgpuRenderPipelineRelease(_fogPipeline); _fogPipeline = NULL; }
	if (_clearPipeline) { wgpuRenderPipelineRelease(_clearPipeline); _clearPipeline = NULL; }
	if (_depthResolvePipeline) { wgpuRenderPipelineRelease(_depthResolvePipeline); _depthResolvePipeline = NULL; }

	for (size_t a = 0; a < 2; a++)
	{
		for (size_t b = 0; b < 2; b++)
		{
			for (size_t c = 0; c < 2; c++)
			{
				for (size_t d = 0; d < 2; d++)
				{
					if (_geomPipelines[a][b][c][d] != NULL)
					{
						wgpuRenderPipelineRelease(_geomPipelines[a][b][c][d]);
						_geomPipelines[a][b][c][d] = NULL;
					}
				}
			}
		}
	}

	if (_samplerNearest) { wgpuSamplerRelease(_samplerNearest); _samplerNearest = NULL; }
	if (_samplerLinear) { wgpuSamplerRelease(_samplerLinear); _samplerLinear = NULL; }

	if (_shaderModuleGeometry) { wgpuShaderModuleRelease(_shaderModuleGeometry); _shaderModuleGeometry = NULL; }
	if (_shaderModulePost) { wgpuShaderModuleRelease(_shaderModulePost); _shaderModulePost = NULL; }
	if (_shaderModuleClear) { wgpuShaderModuleRelease(_shaderModuleClear); _shaderModuleClear = NULL; }
	if (_shaderModuleDepthResolve) { wgpuShaderModuleRelease(_shaderModuleDepthResolve); _shaderModuleDepthResolve = NULL; }

	if (_geomLayout) { wgpuPipelineLayoutRelease(_geomLayout); _geomLayout = NULL; }
	if (_postLayout) { wgpuPipelineLayoutRelease(_postLayout); _postLayout = NULL; }
	if (_clearLayout) { wgpuPipelineLayoutRelease(_clearLayout); _clearLayout = NULL; }
	if (_depthResolveLayout) { wgpuPipelineLayoutRelease(_depthResolveLayout); _depthResolveLayout = NULL; }

	if (_geomBGL) { wgpuBindGroupLayoutRelease(_geomBGL); _geomBGL = NULL; }
	if (_postBGL) { wgpuBindGroupLayoutRelease(_postBGL); _postBGL = NULL; }
	if (_clearBGL) { wgpuBindGroupLayoutRelease(_clearBGL); _clearBGL = NULL; }
	if (_depthResolveBGL) { wgpuBindGroupLayoutRelease(_depthResolveBGL); _depthResolveBGL = NULL; }

	if (_whiteBindGroup) { wgpuBindGroupRelease(_whiteBindGroup); _whiteBindGroup = NULL; }
	if (_whiteView) { wgpuTextureViewRelease(_whiteView); _whiteView = NULL; }
	if (_whiteTex) { wgpuTextureRelease(_whiteTex); _whiteTex = NULL; }
}

void WGPURender::_FlushTextureMap()
{
	for (std::map<u64, WGPUTextureEntry>::iterator it = _textureMap.begin(); it != _textureMap.end(); ++it)
	{
		if (it->second.bindGroup != NULL) { wgpuBindGroupRelease(it->second.bindGroup); }
		if (it->second.view != NULL) { wgpuTextureViewRelease(it->second.view); }
		if (it->second.texture != NULL) { wgpuTextureRelease(it->second.texture); }
	}

	_textureMap.clear();
}

Render3DError WGPURender::_CreateRenderTargets(size_t w, size_t h)
{
	this->_DestroyRenderTargets();

	if (!this->_isDeviceReady)
	{
		return RENDER3DERROR_NOERR;
	}

	const WGPUTextureUsage colorUsage = (WGPUTextureUsage)(WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopySrc);
	const WGPUTextureUsage depthUsage = (WGPUTextureUsage)(WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding);
	const WGPUTextureUsage srcUsage = (WGPUTextureUsage)(WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst);

	WGPUTextureDescriptor texDesc = WGPU_TEXTURE_DESCRIPTOR_INIT;
	texDesc.dimension = WGPUTextureDimension_2D;
	texDesc.format = WGPUTextureFormat_RGBA8Unorm;
	texDesc.usage = colorUsage;
	texDesc.size.width = (uint32_t)w;
	texDesc.size.height = (uint32_t)h;
	texDesc.size.depthOrArrayLayers = 1;

	_colorTex = wgpuDeviceCreateTexture(_device, &texDesc);
	_polyIDTex = wgpuDeviceCreateTexture(_device, &texDesc);
	_fogTex = wgpuDeviceCreateTexture(_device, &texDesc);

	WGPUTextureDescriptor depthDesc = texDesc;
	depthDesc.format = WGPUTextureFormat_Depth32Float;
	depthDesc.usage = depthUsage;
	_depthTex = wgpuDeviceCreateTexture(_device, &depthDesc);

	WGPUTextureViewDescriptor viewDesc = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
	viewDesc.dimension = WGPUTextureViewDimension_2D;
	viewDesc.format = WGPUTextureFormat_RGBA8Unorm;
	viewDesc.mipLevelCount = 1;
	viewDesc.arrayLayerCount = 1;

	_colorView = wgpuTextureCreateView(_colorTex, &viewDesc);
	_polyIDView = wgpuTextureCreateView(_polyIDTex, &viewDesc);
	_fogView = wgpuTextureCreateView(_fogTex, &viewDesc);

	viewDesc.format = WGPUTextureFormat_Depth32Float;
	_depthView = wgpuTextureCreateView(_depthTex, &viewDesc);

	// Multisampled variants. When MSAA is on, the single-sampled color targets
	// above become resolve targets, and a fullscreen pass copies the
	// multisampled depth into _depthTex for the postprocess passes.
	if (_sampleCount > 1)
	{
		WGPUTextureDescriptor msDesc = texDesc;
		msDesc.sampleCount = _sampleCount;

		_colorTexMS = wgpuDeviceCreateTexture(_device, &msDesc);
		_polyIDTexMS = wgpuDeviceCreateTexture(_device, &msDesc);
		_fogTexMS = wgpuDeviceCreateTexture(_device, &msDesc);

		msDesc.format = WGPUTextureFormat_Depth32Float;
		msDesc.usage = depthUsage;
		_depthTexMS = wgpuDeviceCreateTexture(_device, &msDesc);

		WGPUTextureViewDescriptor msViewDesc = viewDesc;
		msViewDesc.format = WGPUTextureFormat_RGBA8Unorm;
		_colorViewMS = wgpuTextureCreateView(_colorTexMS, &msViewDesc);
		_polyIDViewMS = wgpuTextureCreateView(_polyIDTexMS, &msViewDesc);
		_fogViewMS = wgpuTextureCreateView(_fogTexMS, &msViewDesc);

		msViewDesc.format = WGPUTextureFormat_Depth32Float;
		_depthViewMS = wgpuTextureCreateView(_depthTexMS, &msViewDesc);

		if ( (_colorTexMS == NULL) || (_polyIDTexMS == NULL) || (_fogTexMS == NULL) || (_depthTexMS == NULL) )
		{
			printf("WGPU: multisampled render target creation failed; falling back to 1 sample\n");
			this->_DestroyRenderTargets();
			_sampleCount = 1;
			return this->_CreateRenderTargets(w, h);
		}
	}

	// Native-resolution sources for ClearUsingImage().
	WGPUTextureDescriptor ciDesc = texDesc;
	ciDesc.usage = srcUsage;
	ciDesc.size.width = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	ciDesc.size.height = GPU_FRAMEBUFFER_NATIVE_HEIGHT;

	_clearColorTex = wgpuDeviceCreateTexture(_device, &ciDesc);

	ciDesc.format = WGPUTextureFormat_R32Float;
	_clearDepthTex = wgpuDeviceCreateTexture(_device, &ciDesc);

	ciDesc.format = WGPUTextureFormat_R8Unorm;
	_clearFogTex = wgpuDeviceCreateTexture(_device, &ciDesc);

	WGPUTextureViewDescriptor ciViewDesc = viewDesc;
	ciViewDesc.format = WGPUTextureFormat_RGBA8Unorm;
	_clearColorView = wgpuTextureCreateView(_clearColorTex, &ciViewDesc);

	ciViewDesc.format = WGPUTextureFormat_R32Float;
	_clearDepthView = wgpuTextureCreateView(_clearDepthTex, &ciViewDesc);

	ciViewDesc.format = WGPUTextureFormat_R8Unorm;
	_clearFogView = wgpuTextureCreateView(_clearFogTex, &ciViewDesc);

	// One readback buffer per color-out slot so that a queued copy is never
	// overwritten by the next frame before the CPU has mapped it.
	const size_t bytesPerRow = (w * sizeof(Color4u8) + 255) & ~(size_t)255;
	_readbackBufferSize = bytesPerRow * h;

	for (size_t i = 0; i < 2; i++)
	{
		WGPUBufferDescriptor bufDesc = WGPU_BUFFER_DESCRIPTOR_INIT;
		bufDesc.usage = (WGPUBufferUsage)(WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead);
		bufDesc.size = _readbackBufferSize;
		_readbackBuffer[i] = wgpuDeviceCreateBuffer(_device, &bufDesc);
	}

	return RENDER3DERROR_NOERR;
}

void WGPURender::_DestroyRenderTargets()
{
	if (_colorView) { wgpuTextureViewRelease(_colorView); _colorView = NULL; }
	if (_colorTex) { wgpuTextureRelease(_colorTex); _colorTex = NULL; }
	if (_polyIDView) { wgpuTextureViewRelease(_polyIDView); _polyIDView = NULL; }
	if (_polyIDTex) { wgpuTextureRelease(_polyIDTex); _polyIDTex = NULL; }
	if (_fogView) { wgpuTextureViewRelease(_fogView); _fogView = NULL; }
	if (_fogTex) { wgpuTextureRelease(_fogTex); _fogTex = NULL; }
	if (_depthView) { wgpuTextureViewRelease(_depthView); _depthView = NULL; }
	if (_depthTex) { wgpuTextureRelease(_depthTex); _depthTex = NULL; }

	if (_colorViewMS) { wgpuTextureViewRelease(_colorViewMS); _colorViewMS = NULL; }
	if (_colorTexMS) { wgpuTextureRelease(_colorTexMS); _colorTexMS = NULL; }
	if (_polyIDViewMS) { wgpuTextureViewRelease(_polyIDViewMS); _polyIDViewMS = NULL; }
	if (_polyIDTexMS) { wgpuTextureRelease(_polyIDTexMS); _polyIDTexMS = NULL; }
	if (_fogViewMS) { wgpuTextureViewRelease(_fogViewMS); _fogViewMS = NULL; }
	if (_fogTexMS) { wgpuTextureRelease(_fogTexMS); _fogTexMS = NULL; }
	if (_depthViewMS) { wgpuTextureViewRelease(_depthViewMS); _depthViewMS = NULL; }
	if (_depthTexMS) { wgpuTextureRelease(_depthTexMS); _depthTexMS = NULL; }

	if (_clearColorView) { wgpuTextureViewRelease(_clearColorView); _clearColorView = NULL; }
	if (_clearColorTex) { wgpuTextureRelease(_clearColorTex); _clearColorTex = NULL; }
	if (_clearDepthView) { wgpuTextureViewRelease(_clearDepthView); _clearDepthView = NULL; }
	if (_clearDepthTex) { wgpuTextureRelease(_clearDepthTex); _clearDepthTex = NULL; }
	if (_clearFogView) { wgpuTextureViewRelease(_clearFogView); _clearFogView = NULL; }
	if (_clearFogTex) { wgpuTextureRelease(_clearFogTex); _clearFogTex = NULL; }

	if (_postBindGroup) { wgpuBindGroupRelease(_postBindGroup); _postBindGroup = NULL; }
	if (_clearBindGroup) { wgpuBindGroupRelease(_clearBindGroup); _clearBindGroup = NULL; }
	if (_depthResolveBindGroup) { wgpuBindGroupRelease(_depthResolveBindGroup); _depthResolveBindGroup = NULL; }

	for (size_t i = 0; i < 2; i++)
	{
		if (_readbackBuffer[i]) { wgpuBufferRelease(_readbackBuffer[i]); _readbackBuffer[i] = NULL; }
	}

	_readbackBufferSize = 0;
}

// --- pipelines -------------------------------------------------------------

Render3DError WGPURender::_CreatePipelines()
{
	WGPUShaderSourceWGSL wgsl = WGPU_SHADER_SOURCE_WGSL_INIT;
	wgsl.code = WGSLString(kWGSL_Common);

	WGPUShaderModuleDescriptor moduleDesc = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
	moduleDesc.nextInChain = (WGPUChainedStruct *)&wgsl;

	const std::string geomSrc = std::string(kWGSL_Common) + kWGSL_Geometry;
	const std::string postSrc = std::string(kWGSL_Common) + kWGSL_Post;

	wgsl.code = WGSLString(geomSrc.c_str());
	_shaderModuleGeometry = wgpuDeviceCreateShaderModule(_device, &moduleDesc);

	wgsl.code = WGSLString(postSrc.c_str());
	_shaderModulePost = wgpuDeviceCreateShaderModule(_device, &moduleDesc);

	wgsl.code = WGSLString(kWGSL_Clear);
	_shaderModuleClear = wgpuDeviceCreateShaderModule(_device, &moduleDesc);

	wgsl.code = WGSLString(kWGSL_DepthResolve);
	_shaderModuleDepthResolve = wgpuDeviceCreateShaderModule(_device, &moduleDesc);

	if ( (_shaderModuleGeometry == NULL) || (_shaderModulePost == NULL) ||
	     (_shaderModuleClear == NULL) || (_shaderModuleDepthResolve == NULL) )
	{
		printf("WGPU: shader module creation failed\n");
		return RENDER3DERROR_INVALID_VALUE;
	}

	// Samplers. NDS wrap modes are handled in the shader so that a single
	// sampler per filtering mode is enough.
	{
		WGPUSamplerDescriptor desc = WGPU_SAMPLER_DESCRIPTOR_INIT;
		desc.addressModeU = WGPUAddressMode_ClampToEdge;
		desc.addressModeV = WGPUAddressMode_ClampToEdge;
		desc.addressModeW = WGPUAddressMode_ClampToEdge;
		desc.magFilter = WGPUFilterMode_Nearest;
		desc.minFilter = WGPUFilterMode_Nearest;
		desc.mipmapFilter = WGPUMipmapFilterMode_Nearest;
		desc.lodMinClamp = 0.0f;
		desc.lodMaxClamp = 32.0f;
		desc.maxAnisotropy = 1;
		_samplerNearest = wgpuDeviceCreateSampler(_device, &desc);

		desc.magFilter = WGPUFilterMode_Linear;
		desc.minFilter = WGPUFilterMode_Linear;
		// Only matters when upscaled textures carry mip levels; with a single
		// level nothing is filtered.
		desc.mipmapFilter = WGPUMipmapFilterMode_Linear;
		_samplerLinear = wgpuDeviceCreateSampler(_device, &desc);
	}

	// Geometry bind group layout.
	{
		WGPUBindGroupLayoutEntry entries[6];
		memset(entries, 0, sizeof(entries));

		entries[0].binding = 0;
		entries[0].visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
		entries[0].buffer.type = WGPUBufferBindingType_Uniform;

		entries[1].binding = 1;
		entries[1].visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
		entries[1].buffer.type = WGPUBufferBindingType_ReadOnlyStorage;

		entries[2].binding = 2;
		entries[2].visibility = WGPUShaderStage_Fragment;
		entries[2].texture.sampleType = WGPUTextureSampleType_Float;
		entries[2].texture.viewDimension = WGPUTextureViewDimension_2D;

		entries[3].binding = 3;
		entries[3].visibility = WGPUShaderStage_Fragment;
		entries[3].sampler.type = WGPUSamplerBindingType_Filtering;

		entries[4].binding = 4;
		entries[4].visibility = WGPUShaderStage_Fragment;
		entries[4].sampler.type = WGPUSamplerBindingType_Filtering;

		entries[5].binding = 5;
		entries[5].visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
		entries[5].buffer.type = WGPUBufferBindingType_ReadOnlyStorage;

		WGPUBindGroupLayoutDescriptor desc = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
		desc.entryCount = 6;
		desc.entries = entries;
		_geomBGL = wgpuDeviceCreateBindGroupLayout(_device, &desc);
	}

	// Postprocess bind group layout.
	{
		WGPUBindGroupLayoutEntry entries[6];
		memset(entries, 0, sizeof(entries));

		entries[0].binding = 0;
		entries[0].visibility = WGPUShaderStage_Fragment;
		entries[0].buffer.type = WGPUBufferBindingType_Uniform;

		entries[1].binding = 1;
		entries[1].visibility = WGPUShaderStage_Fragment;
		entries[1].texture.sampleType = WGPUTextureSampleType_Float;
		entries[1].texture.viewDimension = WGPUTextureViewDimension_2D;

		entries[2].binding = 2;
		entries[2].visibility = WGPUShaderStage_Fragment;
		entries[2].texture.sampleType = WGPUTextureSampleType_Float;
		entries[2].texture.viewDimension = WGPUTextureViewDimension_2D;

		entries[3].binding = 3;
		entries[3].visibility = WGPUShaderStage_Fragment;
		entries[3].texture.sampleType = WGPUTextureSampleType_Depth;
		entries[3].texture.viewDimension = WGPUTextureViewDimension_2D;

		entries[4].binding = 4;
		entries[4].visibility = WGPUShaderStage_Fragment;
		entries[4].sampler.type = WGPUSamplerBindingType_NonFiltering;

		entries[5].binding = 5;
		entries[5].visibility = WGPUShaderStage_Fragment;
		entries[5].buffer.type = WGPUBufferBindingType_ReadOnlyStorage;

		WGPUBindGroupLayoutDescriptor desc = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
		desc.entryCount = 6;
		desc.entries = entries;
		_postBGL = wgpuDeviceCreateBindGroupLayout(_device, &desc);
	}

	// Clear-image bind group layout.
	{
		WGPUBindGroupLayoutEntry entries[4];
		memset(entries, 0, sizeof(entries));

		for (uint32_t i = 0; i < 3; i++)
		{
			entries[i].binding = i;
			entries[i].visibility = WGPUShaderStage_Fragment;
			// R32Float (the clear-image depth source) is not filterable.
			entries[i].texture.sampleType = (i == 1) ? WGPUTextureSampleType_UnfilterableFloat : WGPUTextureSampleType_Float;
			entries[i].texture.viewDimension = WGPUTextureViewDimension_2D;
		}

		entries[3].binding = 3;
		entries[3].visibility = WGPUShaderStage_Fragment;
		entries[3].sampler.type = WGPUSamplerBindingType_NonFiltering;

		WGPUBindGroupLayoutDescriptor desc = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
		desc.entryCount = 4;
		desc.entries = entries;
		_clearBGL = wgpuDeviceCreateBindGroupLayout(_device, &desc);
	}

	// Depth resolve bind group layout (one multisampled depth texture).
	{
		WGPUBindGroupLayoutEntry entry;
		memset(&entry, 0, sizeof(entry));
		entry.binding = 0;
		entry.visibility = WGPUShaderStage_Fragment;
		entry.texture.sampleType = WGPUTextureSampleType_Depth;
		entry.texture.viewDimension = WGPUTextureViewDimension_2D;
		entry.texture.multisampled = WGPU_TRUE;

		WGPUBindGroupLayoutDescriptor desc = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
		desc.entryCount = 1;
		desc.entries = &entry;
		_depthResolveBGL = wgpuDeviceCreateBindGroupLayout(_device, &desc);
	}

	{
		WGPUBindGroupLayout layouts[1] = {_geomBGL};
		WGPUPipelineLayoutDescriptor desc = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
		desc.bindGroupLayoutCount = 1;
		desc.bindGroupLayouts = layouts;
		_geomLayout = wgpuDeviceCreatePipelineLayout(_device, &desc);

		layouts[0] = _postBGL;
		_postLayout = wgpuDeviceCreatePipelineLayout(_device, &desc);

		layouts[0] = _clearBGL;
		_clearLayout = wgpuDeviceCreatePipelineLayout(_device, &desc);

		layouts[0] = _depthResolveBGL;
		_depthResolveLayout = wgpuDeviceCreatePipelineLayout(_device, &desc);
	}

	if ( (_geomBGL == NULL) || (_postBGL == NULL) || (_clearBGL == NULL) || (_depthResolveBGL == NULL) ||
	     (_geomLayout == NULL) || (_postLayout == NULL) || (_clearLayout == NULL) || (_depthResolveLayout == NULL) )
	{
		printf("WGPU: bind group layout creation failed\n");
		return RENDER3DERROR_INVALID_VALUE;
	}

	// Fullscreen postprocess pipelines.
	{
		WGPURenderPipelineDescriptor desc = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
		desc.layout = _postLayout;
		desc.vertex.module = _shaderModulePost;
		desc.vertex.entryPoint = WGSLString("vsFullscreen");
		desc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
		desc.primitive.cullMode = WGPUCullMode_None;
		desc.multisample.count = 1;

		WGPUBlendState edgeBlend = WGPU_BLEND_STATE_INIT;
		edgeBlend.color.operation = WGPUBlendOperation_Add;
		edgeBlend.color.srcFactor = WGPUBlendFactor_SrcAlpha;
		edgeBlend.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
		// WebGPU requires factor "one" for min/max blend operations. OpenGL
		// ignores the factors for GL_MAX anyway, so this is equivalent.
		edgeBlend.alpha.operation = WGPUBlendOperation_Max;
		edgeBlend.alpha.srcFactor = WGPUBlendFactor_One;
		edgeBlend.alpha.dstFactor = WGPUBlendFactor_One;

		WGPUBlendState fogBlend = WGPU_BLEND_STATE_INIT;
		fogBlend.color.operation = WGPUBlendOperation_Add;
		fogBlend.color.srcFactor = WGPUBlendFactor_Constant;
		fogBlend.color.dstFactor = WGPUBlendFactor_OneMinusSrc;
		fogBlend.alpha.operation = WGPUBlendOperation_Add;
		fogBlend.alpha.srcFactor = WGPUBlendFactor_Constant;
		fogBlend.alpha.dstFactor = WGPUBlendFactor_OneMinusSrc;

		WGPUColorTargetState target = WGPU_COLOR_TARGET_STATE_INIT;
		target.format = WGPUTextureFormat_RGBA8Unorm;
		target.writeMask = WGPUColorWriteMask_All;

		WGPUFragmentState fragment = WGPU_FRAGMENT_STATE_INIT;
		fragment.module = _shaderModulePost;
		fragment.targetCount = 1;
		fragment.targets = &target;

		target.blend = &edgeBlend;
		fragment.entryPoint = WGSLString("fsEdgeMark");
		desc.fragment = &fragment;
		_edgePipeline = wgpuDeviceCreateRenderPipeline(_device, &desc);

		target.blend = &fogBlend;
		fragment.entryPoint = WGSLString("fsFog");
		desc.fragment = &fragment;
		_fogPipeline = wgpuDeviceCreateRenderPipeline(_device, &desc);
	}

	// Clear-image pipeline.
	{
		WGPURenderPipelineDescriptor desc = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
		desc.layout = _clearLayout;
		desc.vertex.module = _shaderModuleClear;
		desc.vertex.entryPoint = WGSLString("vsClear");
		desc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
		desc.primitive.cullMode = WGPUCullMode_None;
		desc.multisample.count = _sampleCount;

		WGPUDepthStencilState depth = WGPU_DEPTH_STENCIL_STATE_INIT;
		depth.format = WGPUTextureFormat_Depth32Float;
		depth.depthWriteEnabled = WGPUOptionalBool_True;
		depth.depthCompare = WGPUCompareFunction_Always;

		WGPUColorTargetState targets[3];
		for (size_t i = 0; i < 3; i++)
		{
			targets[i] = WGPU_COLOR_TARGET_STATE_INIT;
			targets[i].format = WGPUTextureFormat_RGBA8Unorm;
			targets[i].writeMask = WGPUColorWriteMask_All;
		}

		WGPUFragmentState fragment = WGPU_FRAGMENT_STATE_INIT;
		fragment.module = _shaderModuleClear;
		fragment.entryPoint = WGSLString("fsClear");
		fragment.targetCount = 3;
		fragment.targets = targets;

		desc.fragment = &fragment;
		desc.depthStencil = &depth;
		_clearPipeline = wgpuDeviceCreateRenderPipeline(_device, &desc);
	}

	// Depth resolve pipeline: fullscreen triangle, depth-only target, no color.
	{
		WGPURenderPipelineDescriptor desc = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
		desc.layout = _depthResolveLayout;
		desc.vertex.module = _shaderModuleDepthResolve;
		desc.vertex.entryPoint = WGSLString("vsDepthResolve");
		desc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
		desc.primitive.cullMode = WGPUCullMode_None;
		desc.multisample.count = 1;

		WGPUDepthStencilState depth = WGPU_DEPTH_STENCIL_STATE_INIT;
		depth.format = WGPUTextureFormat_Depth32Float;
		depth.depthWriteEnabled = WGPUOptionalBool_True;
		depth.depthCompare = WGPUCompareFunction_Always;

		WGPUFragmentState fragment = WGPU_FRAGMENT_STATE_INIT;
		fragment.module = _shaderModuleDepthResolve;
		fragment.entryPoint = WGSLString("fsDepthResolve");
		fragment.targetCount = 0;
		fragment.targets = NULL;

		desc.fragment = &fragment;
		desc.depthStencil = &depth;
		_depthResolvePipeline = wgpuDeviceCreateRenderPipeline(_device, &desc);
	}

	if ( (_edgePipeline == NULL) || (_fogPipeline == NULL) || (_clearPipeline == NULL) || (_depthResolvePipeline == NULL) )
	{
		printf("WGPU: postprocess pipeline creation failed\n");
		return RENDER3DERROR_INVALID_VALUE;
	}

	return RENDER3DERROR_NOERR;
}

void WGPURender::_CreateBindGroups()
{
	if ( (_frameUploadBuffer == NULL) || (_polyIDView == NULL) )
	{
		return;
	}

	if (_postBindGroup != NULL) { wgpuBindGroupRelease(_postBindGroup); _postBindGroup = NULL; }
	if (_clearBindGroup != NULL) { wgpuBindGroupRelease(_clearBindGroup); _clearBindGroup = NULL; }
	if (_depthResolveBindGroup != NULL) { wgpuBindGroupRelease(_depthResolveBindGroup); _depthResolveBindGroup = NULL; }
	if (_whiteBindGroup != NULL) { wgpuBindGroupRelease(_whiteBindGroup); _whiteBindGroup = NULL; }

	// Untextured polygons still draw through the geometry pipeline, whose layout
	// requires a bind group at index 0. A 1x1 white texel is the identity for
	// every NDS texture mode, so it stands in for the missing texture instead of
	// leaving the slot unbound (which wgpu rejects as a validation error).
	if (_whiteTex == NULL)
	{
		WGPUTextureDescriptor texDesc = WGPU_TEXTURE_DESCRIPTOR_INIT;
		texDesc.usage = (WGPUTextureUsage)(WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst);
		texDesc.dimension = WGPUTextureDimension_2D;
		texDesc.size.width = 1;
		texDesc.size.height = 1;
		texDesc.size.depthOrArrayLayers = 1;
		texDesc.format = WGPUTextureFormat_RGBA8Unorm;
		texDesc.mipLevelCount = 1;
		texDesc.sampleCount = 1;
		_whiteTex = wgpuDeviceCreateTexture(_device, &texDesc);

		if (_whiteTex != NULL)
		{
			const u32 whitePixel = 0xFFFFFFFF;
			WGPUTexelCopyTextureInfo dst = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
			dst.texture = _whiteTex;

			WGPUTexelCopyBufferLayout layout = WGPU_TEXEL_COPY_BUFFER_LAYOUT_INIT;
			layout.bytesPerRow = (uint32_t)sizeof(whitePixel);
			layout.rowsPerImage = 1;

			WGPUExtent3D extent = WGPU_EXTENT_3D_INIT;
			extent.width = 1;
			extent.height = 1;
			extent.depthOrArrayLayers = 1;

			wgpuQueueWriteTexture(_queue, &dst, &whitePixel, sizeof(whitePixel), &layout, &extent);

			WGPUTextureViewDescriptor viewDesc = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
			viewDesc.format = WGPUTextureFormat_RGBA8Unorm;
			viewDesc.dimension = WGPUTextureViewDimension_2D;
			viewDesc.mipLevelCount = 1;
			viewDesc.arrayLayerCount = 1;
			_whiteView = wgpuTextureCreateView(_whiteTex, &viewDesc);
		}
	}

	if (_whiteView != NULL)
	{
		WGPUBindGroupEntry entries[6];
		memset(entries, 0, sizeof(entries));

		entries[0].binding = 0;
		entries[0].buffer = _frameUploadBuffer;
		entries[0].offset = kUploadUniformOffset;
		entries[0].size = sizeof(FrameUniform);

		entries[1].binding = 1;
		entries[1].buffer = _frameUploadBuffer;
		entries[1].offset = kUploadPolyStateOffset;
		entries[1].size = kUploadPolyStateBytes;

		entries[2].binding = 2;
		entries[2].textureView = _whiteView;

		entries[3].binding = 3;
		entries[3].sampler = _samplerNearest;

		entries[4].binding = 4;
		entries[4].sampler = _samplerLinear;

		entries[5].binding = 5;
		entries[5].buffer = _frameUploadBuffer;
		entries[5].offset = kUploadTablesOffset;
		entries[5].size = sizeof(TablesGPU);

		WGPUBindGroupDescriptor desc = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
		desc.layout = _geomBGL;
		desc.entryCount = 6;
		desc.entries = entries;
		_whiteBindGroup = wgpuDeviceCreateBindGroup(_device, &desc);
	}

	{
		WGPUBindGroupEntry entries[6];
		memset(entries, 0, sizeof(entries));

		entries[0].binding = 0;
		entries[0].buffer = _frameUploadBuffer;
		entries[0].offset = kUploadUniformOffset;
		entries[0].size = sizeof(FrameUniform);

		entries[1].binding = 1;
		entries[1].textureView = _polyIDView;

		entries[2].binding = 2;
		entries[2].textureView = _fogView;

		entries[3].binding = 3;
		entries[3].textureView = _depthView;

		entries[4].binding = 4;
		entries[4].sampler = _samplerNearest;

		entries[5].binding = 5;
		entries[5].buffer = _frameUploadBuffer;
		entries[5].offset = kUploadTablesOffset;
		entries[5].size = sizeof(TablesGPU);

		WGPUBindGroupDescriptor desc = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
		desc.layout = _postBGL;
		desc.entryCount = 6;
		desc.entries = entries;
		_postBindGroup = wgpuDeviceCreateBindGroup(_device, &desc);
	}

	{
		WGPUBindGroupEntry entries[4];
		memset(entries, 0, sizeof(entries));

		entries[0].binding = 0;
		entries[0].textureView = _clearColorView;

		entries[1].binding = 1;
		entries[1].textureView = _clearDepthView;

		entries[2].binding = 2;
		entries[2].textureView = _clearFogView;

		entries[3].binding = 3;
		entries[3].sampler = _samplerNearest;

		WGPUBindGroupDescriptor desc = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
		desc.layout = _clearBGL;
		desc.entryCount = 4;
		desc.entries = entries;
		_clearBindGroup = wgpuDeviceCreateBindGroup(_device, &desc);
	}

	if (_depthViewMS != NULL)
	{
		WGPUBindGroupEntry entry;
		memset(&entry, 0, sizeof(entry));
		entry.binding = 0;
		entry.textureView = _depthViewMS;

		WGPUBindGroupDescriptor desc = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
		desc.layout = _depthResolveBGL;
		desc.entryCount = 1;
		desc.entries = &entry;
		_depthResolveBindGroup = wgpuDeviceCreateBindGroup(_device, &desc);
	}
}

WGPURenderPipeline WGPURender::_GetGeomPipeline(const WGPUDrawRun &run)
{
	WGPURenderPipeline &pipeline = _geomPipelines[run.topology][run.depthEqual][run.depthWrite][run.blend];
	if (pipeline != NULL)
	{
		return pipeline;
	}

	WGPURenderPipelineDescriptor desc = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
	desc.layout = _geomLayout;
	desc.vertex.module = _shaderModuleGeometry;
	desc.vertex.entryPoint = WGSLString("vsGeom");

	WGPUVertexAttribute attributes[4];
	attributes[0] = WGPU_VERTEX_ATTRIBUTE_INIT;
	attributes[0].format = WGPUVertexFormat_Sint32x4;
	attributes[0].offset = 0;
	attributes[0].shaderLocation = 0;

	attributes[1] = WGPU_VERTEX_ATTRIBUTE_INIT;
	attributes[1].format = WGPUVertexFormat_Sint32x2;
	attributes[1].offset = 16;
	attributes[1].shaderLocation = 1;

	attributes[2] = WGPU_VERTEX_ATTRIBUTE_INIT;
	attributes[2].format = WGPUVertexFormat_Uint8x4;
	attributes[2].offset = 24;
	attributes[2].shaderLocation = 2;

	attributes[3] = WGPU_VERTEX_ATTRIBUTE_INIT;
	attributes[3].format = WGPUVertexFormat_Uint32;
	attributes[3].offset = 28;
	attributes[3].shaderLocation = 3;

	WGPUVertexBufferLayout bufferLayout = WGPU_VERTEX_BUFFER_LAYOUT_INIT;
	bufferLayout.stepMode = WGPUVertexStepMode_Vertex;
	bufferLayout.arrayStride = sizeof(VertexGPU);
	bufferLayout.attributeCount = 4;
	bufferLayout.attributes = attributes;

	desc.vertex.bufferCount = 1;
	desc.vertex.buffers = &bufferLayout;

	desc.primitive.topology = (run.topology == 0) ? WGPUPrimitiveTopology_TriangleList : WGPUPrimitiveTopology_LineList;
	desc.primitive.cullMode = WGPUCullMode_None;
	desc.primitive.frontFace = WGPUFrontFace_CCW;
	desc.multisample.count = _sampleCount;

	WGPUDepthStencilState depth = WGPU_DEPTH_STENCIL_STATE_INIT;
	depth.format = WGPUTextureFormat_Depth32Float;
	depth.depthWriteEnabled = (run.depthWrite != 0) ? WGPUOptionalBool_True : WGPUOptionalBool_False;
	depth.depthCompare = (run.depthEqual != 0) ? WGPUCompareFunction_LessEqual : WGPUCompareFunction_Less;
	desc.depthStencil = &depth;

	WGPUBlendState blend = WGPU_BLEND_STATE_INIT;
	blend.color.operation = WGPUBlendOperation_Add;
	blend.color.srcFactor = WGPUBlendFactor_SrcAlpha;
	blend.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
	// See the edge-mark blend state above for why the alpha factors are "one".
	blend.alpha.operation = WGPUBlendOperation_Max;
	blend.alpha.srcFactor = WGPUBlendFactor_One;
	blend.alpha.dstFactor = WGPUBlendFactor_One;

	WGPUColorTargetState targets[3];
	for (size_t i = 0; i < 3; i++)
	{
		targets[i] = WGPU_COLOR_TARGET_STATE_INIT;
		targets[i].format = WGPUTextureFormat_RGBA8Unorm;
		targets[i].writeMask = WGPUColorWriteMask_All;
	}
	targets[0].blend = (run.blend != 0) ? &blend : NULL;

	WGPUFragmentState fragment = WGPU_FRAGMENT_STATE_INIT;
	fragment.module = _shaderModuleGeometry;
	fragment.entryPoint = WGSLString("fsGeom");
	fragment.targetCount = 3;
	fragment.targets = targets;

	desc.fragment = &fragment;

	pipeline = wgpuDeviceCreateRenderPipeline(_device, &desc);
	return pipeline;
}

// --- per-frame work --------------------------------------------------------

Render3DTexture* WGPURender::_GetLoadedTexture(const POLY &thePoly)
{
	WGPURenderTexture *theTexture = (WGPURenderTexture *)texCache.GetTexture(thePoly.texParam, thePoly.texPalette);

	if (theTexture == NULL)
	{
		theTexture = new WGPURenderTexture(thePoly.texParam, thePoly.texPalette);
		texCache.Add(theTexture);
	}

	const bool isTextureEnabled = ( (theTexture->GetPackFormat() != TEXMODE_NONE) && this->_enableTextureSampling );
	theTexture->SetSamplingEnabled(isTextureEnabled);

	if (theTexture->IsLoadNeeded() && isTextureEnabled)
	{
		// The upscale buffer is allocated lazily by the base class, so fall back
		// to 1x if it is not there yet.
		const size_t scalingFactor = (this->_textureUpscaleBuffer != NULL) ? this->_textureScalingFactor : 1;

		theTexture->SetUnpackBuffer((u32 *)this->_texUnpackBuffer);
		theTexture->SetUpscaleBuffer(this->_textureUpscaleBuffer);
		// Same arrangement as the OpenGL renderer: the deposterized result goes
		// back into the unpack buffer, using the renderer's scratch surface as
		// the intermediate.
		theTexture->SetDeposterizeBuffer(this->_texUnpackBuffer,
										 this->_textureDeposterizeDstSurface.workingSurface[0]);
		theTexture->SetUseDeposterize(this->_enableTextureDeposterize);
		theTexture->SetScalingFactor(scalingFactor);
	}

	return theTexture;
}

void WGPURender::_WriteFrameUniforms(const GFX3D_State &renderState)
{
	FrameUniform *uniform = (FrameUniform *)_frameUniformData;
	TablesGPU tables;
	memset(&tables, 0, sizeof(tables));

	uniform->flags = 0;
	if (renderState.DISP3DCNT.FogOnlyAlpha) { uniform->flags |= WGPU_FRAME_FOG_ALPHA_ONLY; }
	if (renderState.DISP3DCNT.EnableAlphaTest) { uniform->flags |= WGPU_FRAME_ALPHA_TEST; }
	if (this->_enableTextureSmoothing) { uniform->flags |= WGPU_FRAME_TEX_SMOOTHING; }
	if (renderState.DISP3DCNT.PolygonShading) { uniform->flags |= WGPU_FRAME_TOON_SHADING; }

	uniform->clearColor[0] = (float)this->_clearColor6665.r / 63.0f;
	uniform->clearColor[1] = (float)this->_clearColor6665.g / 63.0f;
	uniform->clearColor[2] = (float)this->_clearColor6665.b / 63.0f;
	uniform->clearColor[3] = (float)this->_clearColor6665.a / 31.0f;

	uniform->clearPolyID = (float)this->_clearAttributes.opaquePolyID;
	uniform->clearDepth = (float)this->_clearAttributes.depth / (float)0x00FFFFFF;
	uniform->alphaTestRef = (float)renderState.alphaTestRef / 31.0f;

	uniform->fogOffset = (float)(renderState.fogOffset & 0x7FFF);
	uniform->fogStep = (float)(0x0400 >> (renderState.fogShift & 0x0F));

	uniform->fogColor[0] = (float)((renderState.fogColor) & 0x1F) / 31.0f;
	uniform->fogColor[1] = (float)((renderState.fogColor >> 5) & 0x1F) / 31.0f;
	uniform->fogColor[2] = (float)((renderState.fogColor >> 10) & 0x1F) / 31.0f;
	uniform->fogColor[3] = (float)((renderState.fogColor >> 16) & 0x1F) / 31.0f;

	const float edgeColorAlpha = (renderState.DISP3DCNT.EnableAntialiasing) ? (16.0f / 31.0f) : 1.0f;
	for (size_t i = 0; i < 8; i++)
	{
		tables.edgeColor[i][0] = (float)((renderState.edgeMarkColorTable[i]) & 0x1F) / 31.0f;
		tables.edgeColor[i][1] = (float)((renderState.edgeMarkColorTable[i] >> 5) & 0x1F) / 31.0f;
		tables.edgeColor[i][2] = (float)((renderState.edgeMarkColorTable[i] >> 10) & 0x1F) / 31.0f;
		tables.edgeColor[i][3] = edgeColorAlpha;
	}

	for (size_t i = 0; i < 32; i++)
	{
		tables.toonColor[i][0] = (float)((renderState.toonTable16[i]) & 0x1F) / 31.0f;
		tables.toonColor[i][1] = (float)((renderState.toonTable16[i] >> 5) & 0x1F) / 31.0f;
		tables.toonColor[i][2] = (float)((renderState.toonTable16[i] >> 10) & 0x1F) / 31.0f;
		tables.toonColor[i][3] = 1.0f;

		const u8 density = renderState.fogDensityTable[i];
		tables.fogDensity[i] = (float)((density == 127) ? 255 : (density << 1)) / 255.0f;
	}

	// Everything this frame needs is already laid out in the upload staging
	// buffer, so the frame costs one upload call. wgpu-native allocates a
	// staging buffer per call, and this used to be four of them per frame.
	if (_frameUploadBuffer == NULL)
	{
		return;
	}

	memcpy(_frameUploadStaging.data() + kUploadUniformOffset, _frameUniformData, sizeof(FrameUniform));
	memcpy(_frameUploadStaging.data() + kUploadTablesOffset, &tables, sizeof(TablesGPU));

	wgpuQueueWriteBuffer(_queue, _frameUploadBuffer, 0, _frameUploadStaging.data(), _frameUploadVertexOffset + _frameUploadVertexBytes);
}

void WGPURender::_BuildGeometry(const GFX3D_State &renderState, const GFX3D_GeometryList &renderGList)
{
	_clippedPolyCount = renderGList.clippedPolyCount;
	_clippedPolyOpaqueCount = renderGList.clippedPolyOpaqueCount;
	_clippedPolyList = (CPoly *)renderGList.clippedPolyList;
	_rawPolyList = (POLY *)renderGList.rawPolyList;

	_drawRuns.clear();
	_frameUploadVertexOffset = kUploadPolyStateOffset;
	_frameUploadVertexBytes = 0;

	if (_clippedPolyCount == 0)
	{
		return;
	}

	if (_frameUploadStaging.size() < kUploadBufferSize)
	{
		return;
	}

	// Polygon states are indexed by polygon id, so they stay at a fixed offset;
	// the vertices are appended right after the states this frame uses. Both go
	// straight into the upload staging buffer, so no extra copy is needed
	// before the single upload in _WriteFrameUniforms().
	const size_t polyStateBytes = _clippedPolyCount * sizeof(PolyStateGPU);
	u8 * const staging = _frameUploadStaging.data();

	memset(staging + kUploadPolyStateOffset, 0, polyStateBytes);

	PolyStateGPU * const polyStatePtr = (PolyStateGPU *)(staging + kUploadPolyStateOffset);
	_frameUploadVertexOffset = kUploadPolyStateOffset + polyStateBytes;
	VertexGPU * const vertexBase = (VertexGPU *)(staging + _frameUploadVertexOffset);

	for (size_t i = 0; i < _clippedPolyCount; i++)
	{
		const CPoly &cPoly = _clippedPolyList[i];
		const POLY &rawPoly = _rawPolyList[cPoly.index];
		const bool isTranslucent = (i >= _clippedPolyOpaqueCount);
		const bool isWireframe = GFX3D_IsPolyWireframe(rawPoly);
		const bool isLinePrimitive = isWireframe || (rawPoly.vtxFormat >= GFX3D_TRIANGLES_LINE);
		const NDSTextureFormat packFormat = (NDSTextureFormat)rawPoly.texParam.PackedFormat;

		// Shadow polygons are meant to be drawn through a stencil volume, which
		// this renderer does not emulate yet. With the option off they are
		// skipped entirely, matching the OpenGL renderer; with it on they are
		// drawn as flat polygons, which is the closest approximation available
		// without the stencil pass.
		if ( (rawPoly.attribute.Mode == 3) && !CommonSettings.OpenGL_Emulation_ShadowPolygon )
		{
			_textureList[i] = NULL;
			continue;
		}

		_textureList[i] = this->_GetLoadedTexture(rawPoly);

		// Per-polygon shader state.
		PolyStateGPU &ps = polyStatePtr[i];
		ps.id = rawPoly.attribute.PolygonID;
		ps.mode = rawPoly.attribute.Mode;
		ps.alpha = isWireframe ? 0x1F : rawPoly.attribute.Alpha;
		ps.flags = WGPU_POLY_DRAWABLE;
		if (isWireframe) { ps.flags |= WGPU_POLY_WIREFRAME; }
		if (rawPoly.attribute.Fog_Enable) { ps.flags |= WGPU_POLY_ENABLE_FOG; }
		if (rawPoly.attribute.TranslucentDepthWrite_Enable) { ps.flags |= WGPU_POLY_NEW_DEPTH; }
		if ( (_textureList[i] != NULL) && _textureList[i]->IsSamplingEnabled() ) { ps.flags |= WGPU_POLY_ENABLE_TEX; }
		if ( (packFormat != TEXMODE_A3I5) && (packFormat != TEXMODE_A5I3) ) { ps.flags |= WGPU_POLY_SINGLE_BIT_ALPHA; }
		if (cPoly.isPolyBackFacing) { ps.flags |= WGPU_POLY_BACK_FACING; }
		if (isTranslucent) { ps.flags |= WGPU_POLY_TRANSLUCENT; }

		ps.texSizeS = rawPoly.texParam.SizeShiftS;
		ps.texSizeT = rawPoly.texParam.SizeShiftT;

		ps.wrap = 0;
		if (rawPoly.texParam.RepeatS_Enable)
		{
			ps.wrap |= 0x01;
			if (rawPoly.texParam.MirroredRepeatS_Enable) { ps.wrap |= 0x02; }
		}
		if (rawPoly.texParam.RepeatT_Enable)
		{
			ps.wrap |= 0x04;
			if (rawPoly.texParam.MirroredRepeatT_Enable) { ps.wrap |= 0x08; }
		}
		ps.pad0 = 0;

		// Viewport, in native screen pixels. A zero-sized viewport would make
		// the polygon degenerate, so fall back to the full screen.
		const GFX3D_Viewport &vp = rawPoly.viewport;
		const bool vpValid = (vp.width != 0) && (vp.height != 0);
		ps.vpX = vpValid ? vp.x : 0;
		ps.vpY = vpValid ? vp.y : 0;
		ps.vpW = vpValid ? vp.width : GPU_FRAMEBUFFER_NATIVE_WIDTH;
		ps.vpH = vpValid ? vp.height : GPU_FRAMEBUFFER_NATIVE_HEIGHT;

		// Vertices are duplicated instead of indexed so that each one can carry
		// the index of the polygon it belongs to.
		const size_t vertCount = (size_t)rawPoly.type;
		const bool isLoop = isWireframe || (rawPoly.vtxFormat == GFX3D_TRIANGLES_LINE) || (rawPoly.vtxFormat == GFX3D_QUADS_LINE);

		size_t vertexCount = 0;
		if (isLinePrimitive)
		{
			vertexCount = isLoop ? (vertCount * 2) : ((vertCount - 1) * 2);
		}
		else
		{
			vertexCount = (vertCount == 4) ? 6 : 3;
		}

		const size_t firstVertex = _frameUploadVertexBytes / sizeof(VertexGPU);
		if ( (_frameUploadVertexBytes + (vertexCount * sizeof(VertexGPU))) > kUploadVertexCapBytes )
		{
			_textureList[i] = NULL;
			continue;
		}

		VertexGPU * const vtxPtr = vertexBase + firstVertex;
		_frameUploadVertexBytes += vertexCount * sizeof(VertexGPU);

		for (size_t v = 0; v < vertCount; v++)
		{
			const NDSVertex &srcVtx = renderGList.rawVtxList[rawPoly.vertIndexes[v]];
			VertexGPU *dst = &vtxPtr[v];

			dst->pos[0] = srcVtx.position.x;
			dst->pos[1] = srcVtx.position.y;
			dst->pos[2] = srcVtx.position.z;
			dst->pos[3] = srcVtx.position.w;
			dst->uv[0] = srcVtx.texCoord.x;
			dst->uv[1] = srcVtx.texCoord.y;
			dst->color = (uint32_t)srcVtx.color.r | ((uint32_t)srcVtx.color.g << 8) | ((uint32_t)srcVtx.color.b << 16) | 0xFF000000u;
			dst->polyIndex = (uint32_t)i;
		}

		if (isLinePrimitive)
		{
			const size_t segCount = isLoop ? vertCount : (vertCount - 1);

			for (size_t s = 0; s < segCount; s++)
			{
				vtxPtr[s * 2 + 0] = vtxPtr[s];
				vtxPtr[s * 2 + 1] = vtxPtr[(s + 1) % vertCount];
			}
		}
		else if (vertCount == 4)
		{
			const VertexGPU v0 = vtxPtr[0];
			const VertexGPU v1 = vtxPtr[1];
			const VertexGPU v2 = vtxPtr[2];
			const VertexGPU v3 = vtxPtr[3];

			vtxPtr[0] = v0; vtxPtr[1] = v1; vtxPtr[2] = v2;
			vtxPtr[3] = v2; vtxPtr[4] = v3; vtxPtr[5] = v0;
		}

		WGPUDrawRun run;
		run.firstVertex = (uint32_t)firstVertex;
		run.vertexCount = (uint32_t)vertexCount;
		run.tex = _textureList[i];
		run.topology = isLinePrimitive ? 1 : 0;
		run.depthEqual = (rawPoly.attribute.DepthEqualTest_Enable) ? 1 : 0;
		run.depthWrite = (isTranslucent) ? ((rawPoly.attribute.TranslucentDepthWrite_Enable) ? 1 : 0) : 1;
		run.blend = (isTranslucent) ? 1 : 0;

		if ( !_drawRuns.empty() )
		{
			WGPUDrawRun &prev = _drawRuns.back();
			if ( (prev.tex == run.tex) && (prev.topology == run.topology) &&
			     (prev.depthEqual == run.depthEqual) && (prev.depthWrite == run.depthWrite) &&
			     (prev.blend == run.blend) &&
			     ((prev.firstVertex + prev.vertexCount) == run.firstVertex) )
			{
				prev.vertexCount += run.vertexCount;
			}
			else
			{
				_drawRuns.push_back(run);
			}
		}
		else
		{
			_drawRuns.push_back(run);
		}
	}
}

WGPUTextureEntry* WGPURender::_GetTextureEntry(Render3DTexture *tex)
{
	if (tex == NULL)
	{
		return NULL;
	}

	const u64 key = (u64)tex->GetCacheKey();
	std::map<u64, WGPUTextureEntry>::iterator it = _textureMap.find(key);

	if ( (it != _textureMap.end()) && !tex->IsLoadNeeded() )
	{
		tex->ResetCacheAge();
		tex->IncreaseCacheUsageCount(1);
		return &it->second;
	}

	const size_t texW = tex->GetWidth();
	const size_t texH = tex->GetHeight();
	if ( (texW == 0) || (texH == 0) )
	{
		return NULL;
	}

	WGPURenderTexture *wgpuTexture = (WGPURenderTexture *)tex;
	wgpuTexture->Load();

	if (wgpuTexture->IsLoadNeeded())
	{
		// No unpack buffer was assigned (the texture was never enabled for
		// sampling), so there is nothing to upload.
		return NULL;
	}

	// xBRZ upscaling gives the texture a small mip chain: level 0 is the
	// largest image and the last level is always the native one.
	size_t mipLevelCount = 1;
	if ( (this->_textureUpscaleBuffer != NULL) && (tex->GetScalingFactor() >= 2) )
	{
		mipLevelCount = (tex->GetScalingFactor() >= 4) ? 3 : 2;
	}

	WGPUTextureEntry entry;
	entry.texture = NULL;
	entry.view = NULL;
	entry.bindGroup = NULL;

	// The cache key does not include the texture contents, so an entry that is
	// being reloaded has to be recreated from scratch.
	if (it != _textureMap.end())
	{
		if (it->second.bindGroup) { wgpuBindGroupRelease(it->second.bindGroup); }
		if (it->second.view) { wgpuTextureViewRelease(it->second.view); }
		if (it->second.texture) { wgpuTextureRelease(it->second.texture); }
		_textureMap.erase(it);
	}

	// A mip chain of N levels starts at the largest image, so the texture is
	// created at the upscaled size and the smaller levels are the native ones.
	const size_t baseScale = (size_t)1 << (mipLevelCount - 1);

	WGPUTextureDescriptor texDesc = WGPU_TEXTURE_DESCRIPTOR_INIT;
	texDesc.dimension = WGPUTextureDimension_2D;
	texDesc.format = WGPUTextureFormat_RGBA8Unorm;
	texDesc.usage = (WGPUTextureUsage)(WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst);
	texDesc.size.width = (uint32_t)(texW * baseScale);
	texDesc.size.height = (uint32_t)(texH * baseScale);
	texDesc.size.depthOrArrayLayers = 1;
	texDesc.mipLevelCount = (uint32_t)mipLevelCount;
	entry.texture = wgpuDeviceCreateTexture(_device, &texDesc);

	if (entry.texture == NULL)
	{
		return NULL;
	}

	WGPUTexelCopyTextureInfo dst = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
	dst.texture = entry.texture;

	for (size_t level = 0; level < mipLevelCount; level++)
	{
		size_t levelW = 0;
		size_t levelH = 0;
		const u32 *levelData = wgpuTexture->GetMipLevelData(level, mipLevelCount, levelW, levelH);

		if ( (levelData == NULL) || (levelW == 0) || (levelH == 0) )
		{
			break;
		}

		dst.mipLevel = (uint32_t)level;

		WGPUTexelCopyBufferLayout layout = WGPU_TEXEL_COPY_BUFFER_LAYOUT_INIT;
		layout.offset = 0;
		layout.bytesPerRow = (uint32_t)(levelW * sizeof(u32));
		layout.rowsPerImage = (uint32_t)levelH;

		WGPUExtent3D extent = WGPU_EXTENT_3D_INIT;
		extent.width = (uint32_t)levelW;
		extent.height = (uint32_t)levelH;
		extent.depthOrArrayLayers = 1;

		wgpuQueueWriteTexture(_queue, &dst, levelData, levelW * levelH * sizeof(u32), &layout, &extent);
	}

	WGPUTextureViewDescriptor viewDesc = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
	viewDesc.format = WGPUTextureFormat_RGBA8Unorm;
	viewDesc.dimension = WGPUTextureViewDimension_2D;
	viewDesc.mipLevelCount = (uint32_t)mipLevelCount;
	viewDesc.arrayLayerCount = 1;
	entry.view = wgpuTextureCreateView(entry.texture, &viewDesc);

	WGPUBindGroupEntry entries[6];
	memset(entries, 0, sizeof(entries));

	entries[0].binding = 0;
	entries[0].buffer = _frameUploadBuffer;
	entries[0].offset = kUploadUniformOffset;
	entries[0].size = sizeof(FrameUniform);

	entries[1].binding = 1;
	entries[1].buffer = _frameUploadBuffer;
	entries[1].offset = kUploadPolyStateOffset;
	entries[1].size = kUploadPolyStateBytes;

	entries[2].binding = 2;
	entries[2].textureView = entry.view;

	entries[3].binding = 3;
	entries[3].sampler = _samplerNearest;

	entries[4].binding = 4;
	entries[4].sampler = _samplerLinear;

	entries[5].binding = 5;
	entries[5].buffer = _frameUploadBuffer;
	entries[5].offset = kUploadTablesOffset;
	entries[5].size = sizeof(TablesGPU);

	WGPUBindGroupDescriptor bgDesc = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
	bgDesc.layout = _geomBGL;
	bgDesc.entryCount = 6;
	bgDesc.entries = entries;
	entry.bindGroup = wgpuDeviceCreateBindGroup(_device, &bgDesc);

	_textureMap[key] = entry;

	tex->ResetCacheAge();
	tex->IncreaseCacheUsageCount(1);

	return &_textureMap[key];
}

// --- render pass encoding --------------------------------------------------

void WGPURender::_EncodeRenderPasses(const GFX3D_State &renderState)
{
	WGPUCommandEncoderDescriptor encDesc = WGPU_COMMAND_ENCODER_DESCRIPTOR_INIT;
	WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(_device, &encDesc);

	FrameUniform *uniform = (FrameUniform *)_frameUniformData;
	const bool isMSAA = (_sampleCount > 1) && (_colorViewMS != NULL) && (_depthViewMS != NULL);

	// --- pass 1: clear + geometry ---
	{
		WGPURenderPassColorAttachment colorAttachments[3];
		for (size_t i = 0; i < 3; i++)
		{
			colorAttachments[i] = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
			colorAttachments[i].loadOp = WGPULoadOp_Clear;
			colorAttachments[i].storeOp = WGPUStoreOp_Store;
		}

		colorAttachments[0].view = isMSAA ? _colorViewMS : _colorView;
		colorAttachments[0].clearValue.r = uniform->clearColor[0];
		colorAttachments[0].clearValue.g = uniform->clearColor[1];
		colorAttachments[0].clearValue.b = uniform->clearColor[2];
		colorAttachments[0].clearValue.a = uniform->clearColor[3];

		colorAttachments[1].view = isMSAA ? _polyIDViewMS : _polyIDView;
		colorAttachments[1].clearValue.r = uniform->clearPolyID / 63.0;
		colorAttachments[1].clearValue.g = 0.0;
		colorAttachments[1].clearValue.b = 0.0;
		colorAttachments[1].clearValue.a = 1.0;

		colorAttachments[2].view = isMSAA ? _fogViewMS : _fogView;
		colorAttachments[2].clearValue.r = this->_clearAttributes.isFogged ? 1.0 : 0.0;
		colorAttachments[2].clearValue.g = 0.0;
		colorAttachments[2].clearValue.b = 0.0;
		colorAttachments[2].clearValue.a = 1.0;

		if (isMSAA)
		{
			// The single-sampled targets receive the resolved color.
			colorAttachments[0].resolveTarget = _colorView;
			colorAttachments[1].resolveTarget = _polyIDView;
			colorAttachments[2].resolveTarget = _fogView;
		}

		WGPURenderPassDepthStencilAttachment depthAttachment = WGPU_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_INIT;
		depthAttachment.view = isMSAA ? _depthViewMS : _depthView;
		depthAttachment.depthLoadOp = WGPULoadOp_Clear;
		depthAttachment.depthStoreOp = WGPUStoreOp_Store;
		depthAttachment.depthClearValue = uniform->clearDepth;

		WGPURenderPassDescriptor passDesc = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
		passDesc.colorAttachmentCount = 3;
		passDesc.colorAttachments = colorAttachments;
		passDesc.depthStencilAttachment = &depthAttachment;

		WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &passDesc);

		if (_useClearImage && (_clearPipeline != NULL))
		{
			wgpuRenderPassEncoderSetPipeline(pass, _clearPipeline);
			wgpuRenderPassEncoderSetBindGroup(pass, 0, _clearBindGroup, 0, NULL);
			wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
		}

		if (_frameUploadVertexBytes > 0)
		{
			wgpuRenderPassEncoderSetVertexBuffer(pass, 0, _frameUploadBuffer, _frameUploadVertexOffset, _frameUploadVertexBytes);
		}

		for (size_t i = 0; i < _drawRuns.size(); i++)
		{
			const WGPUDrawRun &run = _drawRuns[i];
			if ( (run.vertexCount == 0) || ((run.firstVertex + run.vertexCount) * sizeof(VertexGPU) > _frameUploadVertexBytes) )
			{
				continue;
			}

			WGPURenderPipeline pipeline = this->_GetGeomPipeline(run);
			if (pipeline == NULL)
			{
				continue;
			}

			wgpuRenderPassEncoderSetPipeline(pass, pipeline);

			WGPUTextureEntry *texEntry = this->_GetTextureEntry(run.tex);
			WGPUBindGroup geomBindGroup = _whiteBindGroup;
			if ( (texEntry != NULL) && (texEntry->bindGroup != NULL) )
			{
				geomBindGroup = texEntry->bindGroup;
			}

			if (geomBindGroup != NULL)
			{
				wgpuRenderPassEncoderSetBindGroup(pass, 0, geomBindGroup, 0, NULL);
			}

			wgpuRenderPassEncoderDraw(pass, run.vertexCount, 1, run.firstVertex, 0);
		}

		wgpuRenderPassEncoderEnd(pass);
		wgpuRenderPassEncoderRelease(pass);
	}

	// --- pass 1b: depth resolve (MSAA only) ---
	if (isMSAA && (_depthResolvePipeline != NULL) && (_depthResolveBindGroup != NULL))
	{
		WGPURenderPassDepthStencilAttachment depthAttachment = WGPU_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_INIT;
		depthAttachment.view = _depthView;
		depthAttachment.depthLoadOp = WGPULoadOp_Clear;
		depthAttachment.depthStoreOp = WGPUStoreOp_Store;
		depthAttachment.depthClearValue = 1.0;

		WGPURenderPassDescriptor passDesc = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
		passDesc.colorAttachmentCount = 0;
		passDesc.colorAttachments = NULL;
		passDesc.depthStencilAttachment = &depthAttachment;

		WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &passDesc);
		wgpuRenderPassEncoderSetPipeline(pass, _depthResolvePipeline);
		wgpuRenderPassEncoderSetBindGroup(pass, 0, _depthResolveBindGroup, 0, NULL);
		wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
		wgpuRenderPassEncoderEnd(pass);
		wgpuRenderPassEncoderRelease(pass);
	}

	// --- pass 2: edge marking ---
	if (this->_enableEdgeMark && this->_deviceInfo.isEdgeMarkSupported && (_clippedPolyCount > 0) && (_postBindGroup != NULL))
	{
		WGPURenderPassColorAttachment colorAttachment = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
		colorAttachment.view = _colorView;
		colorAttachment.loadOp = WGPULoadOp_Load;
		colorAttachment.storeOp = WGPUStoreOp_Store;

		WGPURenderPassDescriptor passDesc = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
		passDesc.colorAttachmentCount = 1;
		passDesc.colorAttachments = &colorAttachment;

		WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &passDesc);
		wgpuRenderPassEncoderSetPipeline(pass, _edgePipeline);
		wgpuRenderPassEncoderSetBindGroup(pass, 0, _postBindGroup, 0, NULL);
		wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
		wgpuRenderPassEncoderEnd(pass);
		wgpuRenderPassEncoderRelease(pass);
	}

	// --- pass 3: fog ---
	if (this->_enableFog && this->_deviceInfo.isFogSupported && (_clippedPolyCount > 0) && (_postBindGroup != NULL))
	{
		WGPURenderPassColorAttachment colorAttachment = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
		colorAttachment.view = _colorView;
		colorAttachment.loadOp = WGPULoadOp_Load;
		colorAttachment.storeOp = WGPUStoreOp_Store;

		WGPURenderPassDescriptor passDesc = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
		passDesc.colorAttachmentCount = 1;
		passDesc.colorAttachments = &colorAttachment;

		WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &passDesc);

		WGPUColor blendColor;
		blendColor.r = uniform->fogColor[0];
		blendColor.g = uniform->fogColor[1];
		blendColor.b = uniform->fogColor[2];
		blendColor.a = uniform->fogColor[3];

		wgpuRenderPassEncoderSetBlendConstant(pass, &blendColor);
		wgpuRenderPassEncoderSetPipeline(pass, _fogPipeline);
		wgpuRenderPassEncoderSetBindGroup(pass, 0, _postBindGroup, 0, NULL);
		wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
		wgpuRenderPassEncoderEnd(pass);
		wgpuRenderPassEncoderRelease(pass);
	}

	// --- queue the readback for the color-out slot that owns this frame ---
	const size_t colorOutIdx = (this->_lastBoundColorOut <= 1) ? this->_lastBoundColorOut : RENDER3D_RESOURCE_INDEX_NONE;

	if ( (colorOutIdx != RENDER3D_RESOURCE_INDEX_NONE) && (_readbackBuffer[colorOutIdx] != NULL) )
	{
		WGPUTexelCopyTextureInfo src = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
		src.texture = _colorTex;

		WGPUTexelCopyBufferInfo dst = WGPU_TEXEL_COPY_BUFFER_INFO_INIT;
		dst.buffer = _readbackBuffer[colorOutIdx];
		dst.layout.offset = 0;
		dst.layout.bytesPerRow = (uint32_t)(((_framebufferWidth * sizeof(Color4u8)) + 255) & ~(size_t)255);
		dst.layout.rowsPerImage = (uint32_t)_framebufferHeight;

		WGPUExtent3D extent = WGPU_EXTENT_3D_INIT;
		extent.width = (uint32_t)_framebufferWidth;
		extent.height = (uint32_t)_framebufferHeight;
		extent.depthOrArrayLayers = 1;

		wgpuCommandEncoderCopyTextureToBuffer(encoder, &src, &dst, &extent);
	}

	WGPUCommandBufferDescriptor cmdDesc = WGPU_COMMAND_BUFFER_DESCRIPTOR_INIT;
	WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(encoder, &cmdDesc);
	wgpuQueueSubmit(_queue, 1, &cmd);
	wgpuCommandBufferRelease(cmd);
}

void WGPURender::ReadbackFramebuffer(const size_t idx, u8 *dstRaw, u8 *dstConverted)
{
	if ( (idx > 1) || (_readbackBuffer[idx] == NULL) || (dstRaw == NULL) || (dstConverted == NULL) )
	{
		return;
	}

	const size_t rowBytes = _framebufferWidth * sizeof(Color4u8);
	const size_t paddedRowBytes = (rowBytes + 255) & ~(size_t)255;

	BufferMapResult mapResult;
	mapResult.success = false;
	mapResult.done = false;

	WGPUBufferMapCallbackInfo callback = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
	callback.mode = WGPUCallbackMode_AllowProcessEvents;
	callback.callback = WGSLOnBufferMapped;
	callback.userdata1 = &mapResult;

	wgpuBufferMapAsync(_readbackBuffer[idx], WGPUMapMode_Read, 0, _readbackBufferSize, callback);
	WGSLWaitForFlag(_instance, &mapResult.done);

	if (!mapResult.success)
	{
		return;
	}

	const u8 *mapped = (const u8 *)wgpuBufferGetConstMappedRange(_readbackBuffer[idx], 0, _readbackBufferSize);
	if (mapped != NULL)
	{
		for (size_t y = 0; y < _framebufferHeight; y++)
		{
			memcpy(dstRaw + y * rowBytes, mapped + y * paddedRowBytes, rowBytes);
		}
	}

	wgpuBufferUnmap(_readbackBuffer[idx]);

	// Colorspace conversion, matching what the OpenGL color output does.
	const size_t pixCount = _framebufferWidth * _framebufferHeight;

	if (this->_outputFormat == NDSColorFormat_BGR666_Rev)
	{
		ColorspaceConvertBuffer8888To6665<false, false>((const u32 *)dstRaw, (u32 *)dstConverted, pixCount);
	}
	else
	{
		ColorspaceCopyBuffer32<false, false>((const u32 *)dstRaw, (u32 *)dstConverted, pixCount);
	}

	memcpy(dstRaw, dstConverted, pixCount * sizeof(u32));
}

// --- Render3D hooks --------------------------------------------------------

Render3DError WGPURender::Render(const GFX3D_State &renderState, const GFX3D_GeometryList &renderGList)
{
	if (!_isDeviceReady)
	{
		if (this->_SetupDevice() != RENDER3DERROR_NOERR)
		{
			return RENDER3DERROR_INVALID_VALUE;
		}

		// Render targets come first: they can downgrade the sample count when
		// multisampled targets cannot be allocated, and the pipelines bake the
		// final sample count into their state.
		this->_CreateRenderTargets(this->_framebufferWidth, this->_framebufferHeight);

		if (this->_CreatePipelines() != RENDER3DERROR_NOERR)
		{
			return RENDER3DERROR_INVALID_VALUE;
		}

		{
			WGPUBufferDescriptor desc = WGPU_BUFFER_DESCRIPTOR_INIT;
			desc.usage = (WGPUBufferUsage)(WGPUBufferUsage_Uniform | WGPUBufferUsage_Storage | WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst);
			desc.size = kUploadBufferSize;
			_frameUploadBuffer = wgpuDeviceCreateBuffer(_device, &desc);
			_frameUploadStaging.resize(kUploadBufferSize);
		}

		this->_CreateBindGroups();
	}

	_currentState = &renderState;
	Render3DError error = this->Render3D::Render(renderState, renderGList);
	_currentState = NULL;

	return error;
}

Render3DError WGPURender::BeginRender(const GFX3D_State &renderState, const GFX3D_GeometryList &renderGList)
{
	this->_BuildGeometry(renderState, renderGList);

	this->_lastBoundColorOut = this->_colorOut->BindRenderer();
	this->_renderNeedsFinish = true;

	return RENDER3DERROR_NOERR;
}

Render3DError WGPURender::RenderGeometry()
{
	if (_lastBoundColorOut == RENDER3D_RESOURCE_INDEX_NONE)
	{
		return RENDER3DERROR_NOERR;
	}

	if (_currentState != NULL)
	{
		// The clear values are only known after ClearFramebuffer() has run, so
		// the frame constants are written here instead of in BeginRender().
		this->_WriteFrameUniforms(*_currentState);
		this->_EncodeRenderPasses(*_currentState);
	}

	return RENDER3DERROR_NOERR;
}

Render3DError WGPURender::PostprocessFramebuffer()
{
	return RENDER3DERROR_NOERR;
}

Render3DError WGPURender::EndRender()
{
	if (_lastBoundColorOut != RENDER3D_RESOURCE_INDEX_NONE)
	{
		this->_colorOut->UnbindRenderer(_lastBoundColorOut);
		_lastBoundColorOut = RENDER3D_RESOURCE_INDEX_NONE;
	}

	texCache.Evict();

	return RENDER3DERROR_NOERR;
}

Render3DError WGPURender::RenderFinish()
{
	if (!this->_renderNeedsFinish)
	{
		return RENDER3DERROR_NOERR;
	}

	this->_renderNeedsFlushMain = true;
	this->_renderNeedsFlush16 = true;

	return RENDER3DERROR_NOERR;
}

Render3DError WGPURender::RenderFlush(bool willFlushBuffer32, bool willFlushBuffer16)
{
	if ( !this->_isPoweredOn || (!this->_renderNeedsFlushMain && !this->_renderNeedsFlush16) )
	{
		return RENDER3DERROR_NOERR;
	}

	if (this->_renderNeedsFlushMain && willFlushBuffer32)
	{
		this->_renderNeedsFlushMain = false;
		this->_colorOut->BindRead32();
	}

	if (this->_renderNeedsFlush16 && willFlushBuffer16)
	{
		if (this->_renderNeedsFlushMain)
		{
			this->_renderNeedsFlushMain = false;
			this->_colorOut->BindRead32();
		}

		this->_renderNeedsFlush16 = false;
		this->_colorOut->BindRead16();
	}

	return RENDER3DERROR_NOERR;
}

Render3DError WGPURender::ClearUsingValues(const Color4u8 &clearColor6665, const FragmentAttributes &clearAttributes)
{
	this->_clearColor6665 = clearColor6665;
	this->_clearAttributes = clearAttributes;
	_useClearImage = false;

	return RENDER3DERROR_NOERR;
}

Render3DError WGPURender::ClearUsingImage(const u16 *__restrict colorBuffer, const u32 *__restrict depthBuffer, const u8 *__restrict fogBuffer, const u8 opaquePolyID)
{
	if (!_isDeviceReady)
	{
		return RENDER3DERROR_INVALID_VALUE;
	}

	static std::vector<u32> colorScratch;
	static std::vector<float> depthScratch;
	static std::vector<u8> fogScratch;

	const size_t pixCount = GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT;

	colorScratch.resize(pixCount);
	depthScratch.resize(pixCount);
	fogScratch.resize(pixCount);

	ColorspaceConvertBuffer5551To8888<false, false, BESwapNone>(colorBuffer, &colorScratch[0], pixCount);

	for (size_t i = 0; i < pixCount; i++)
	{
		depthScratch[i] = (float)(depthBuffer[i] & 0x00FFFFFF) / (float)0x00FFFFFF;
		fogScratch[i] = (fogBuffer[i] != 0) ? 0xFF : 0x00;
	}

	WGPUTexelCopyTextureInfo dst = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;

	WGPUTexelCopyBufferLayout layout = WGPU_TEXEL_COPY_BUFFER_LAYOUT_INIT;
	layout.offset = 0;
	layout.bytesPerRow = (uint32_t)(GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u32));
	layout.rowsPerImage = GPU_FRAMEBUFFER_NATIVE_HEIGHT;

	WGPUExtent3D extent = WGPU_EXTENT_3D_INIT;
	extent.width = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	extent.height = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	extent.depthOrArrayLayers = 1;

	dst.texture = _clearColorTex;
	wgpuQueueWriteTexture(_queue, &dst, &colorScratch[0], pixCount * sizeof(u32), &layout, &extent);

	layout.bytesPerRow = (uint32_t)(GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(float));
	dst.texture = _clearDepthTex;
	wgpuQueueWriteTexture(_queue, &dst, &depthScratch[0], pixCount * sizeof(float), &layout, &extent);

	layout.bytesPerRow = (uint32_t)GPU_FRAMEBUFFER_NATIVE_WIDTH;
	dst.texture = _clearFogTex;
	wgpuQueueWriteTexture(_queue, &dst, &fogScratch[0], pixCount, &layout, &extent);

	_clearAttributes.opaquePolyID = opaquePolyID;
	_useClearImage = true;

	return RENDER3DERROR_NOERR;
}

// ---------------------------------------------------------------------------
// Self-test: drives the whole pipeline with a synthetic scene
// ---------------------------------------------------------------------------

static int WGPURender_SelfTestBody(const char *outPathPPM)
{
	WGPURender renderer;
	renderer.RequestColorFormat(NDSColorFormat_BGR888_Rev);
	renderer.SetFramebufferSize(GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT);

	GFX3D_State state;
	memset(&state, 0, sizeof(state));
	state.clearColor = (15u << 0) | (15u << 5) | (15u << 10) | (31u << 16);
	state.clearDepth = 0x00FFFFFF;
	state.alphaTestRef = 31;
	state.DISP3DCNT.EnableTexMapping = 1;
	state.DISP3DCNT.EnableAlphaTest = 1;
	state.DISP3DCNT.EnableAlphaBlending = 1;

	for (size_t i = 0; i < 32; i++)
	{
		state.fogDensityTable[i] = 0;
		state.toonTable16[i] = 0x7FFF;
	}

	GFX3D_GeometryList *glist = new GFX3D_GeometryList;
	memset(glist, 0, sizeof(GFX3D_GeometryList));

	// One screen-covering triangle with red / green / blue corners.
	NDSVertex *vtx = glist->rawVtxList;
	vtx[0].position.x = -2048; vtx[0].position.y = -2048; vtx[0].position.z = 0; vtx[0].position.w = 4096;
	vtx[0].texCoord.x = 0; vtx[0].texCoord.y = 0;
	vtx[0].color.r = 63; vtx[0].color.g = 0; vtx[0].color.b = 0;

	vtx[1].position.x = 6144; vtx[1].position.y = -2048; vtx[1].position.z = 0; vtx[1].position.w = 4096;
	vtx[1].texCoord.x = 64; vtx[1].texCoord.y = 0;
	vtx[1].color.r = 0; vtx[1].color.g = 63; vtx[1].color.b = 0;

	vtx[2].position.x = -2048; vtx[2].position.y = 6144; vtx[2].position.z = 0; vtx[2].position.w = 4096;
	vtx[2].texCoord.x = 0; vtx[2].texCoord.y = 64;
	vtx[2].color.r = 0; vtx[2].color.g = 0; vtx[2].color.b = 63;

	POLY *poly = glist->rawPolyList;
	poly[0].type = POLYGON_TYPE_TRIANGLE;
	poly[0].vtxFormat = GFX3D_TRIANGLES;
	poly[0].vertIndexes[0] = 0;
	poly[0].vertIndexes[1] = 1;
	poly[0].vertIndexes[2] = 2;
	poly[0].attribute.value = 0;
	poly[0].attribute.Alpha = 31;
	poly[0].attribute.Mode = POLYGON_MODE_MODULATE;
	poly[0].attribute.PolygonID = 7;
	poly[0].texParam.value = 0;

	CPoly *cPoly = glist->clippedPolyList;
	cPoly[0].index = 0;
	cPoly[0].type = POLYGON_TYPE_TRIANGLE;

	glist->rawVertCount = 3;
	glist->rawPolyCount = 1;
	glist->clippedPolyCount = 1;
	glist->clippedPolyOpaqueCount = 1;

	renderer.Render(state, *glist);
	renderer.RenderFinish();
	renderer.RenderFlush(true, true);

	const Color4u8 *fb = renderer.GetFramebuffer32();
	if (fb == NULL)
	{
		printf("WGPU self-test: no framebuffer was produced\n");
		delete glist;
		return 1;
	}

	FILE *fp = fopen(outPathPPM, "wb");
	if (fp == NULL)
	{
		printf("WGPU self-test: cannot open %s\n", outPathPPM);
		delete glist;
		return 2;
	}

	fprintf(fp, "P6\n%u %u\n255\n", (unsigned)renderer.GetFramebufferWidth(), (unsigned)renderer.GetFramebufferHeight());
	for (size_t i = 0; i < renderer.GetFramebufferWidth() * renderer.GetFramebufferHeight(); i++)
	{
		const u8 rgb[3] = {fb[i].r, fb[i].g, fb[i].b};
		fwrite(rgb, 1, 3, fp);
	}
	fclose(fp);

	printf("WGPU self-test: wrote %s\n", outPathPPM);

	delete glist;
	return 0;
}

int WGPURender_SelfTest(const char *outPathPPM)
{
	// The renderer base class expects Render3D_Init() to have run, which the
	// emulator normally does during NDS_Init(). The colorspace lookup tables
	// have to exist as well.
	ColorspaceHandlerInit();
	Render3D_Init();

	const int result = WGPURender_SelfTestBody(outPathPPM);

	Render3D_DeInit();

	return result;
}

// The generic entry, used when the frontend can't offer one entry per backend
// (or when adapter enumeration finds nothing). wgpu-native then picks whatever
// backend it prefers.
GPU3DInterface gpu3DWGPU = {
	"WebGPU",
	WGPURender::Create,
	WGPURender::Destroy
};

#endif // ENABLE_WGPU
