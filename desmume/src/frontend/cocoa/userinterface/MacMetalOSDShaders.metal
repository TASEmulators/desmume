/*
	Copyright (C) 2026 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	Metal (MSL) translation of frontend/modules/osd/wgpu/hud_shapes.wgsl,
	hud_text.wgsl and the embedded kTextShader in wgpu_draw.cpp.

	All three offscreen pipelines write premultiplied BGRA8 and use
	One / OneMinusSrcAlpha blending.  Coordinates are pixels with the origin at
	the top left of the 256x384 HUD target, exactly like the WGPU version; do
	not round or offset them.

	Keep OSDShapeInstance in sync with MetalOSDInstance in
	MacMetalOSDRenderer.mm (672 bytes, 16-byte aligned).
*/

#include <metal_stdlib>
using namespace metal;

// ---------------------------------------------------------------------------
// Shared types
// ---------------------------------------------------------------------------

struct OSDShapeInstance
{
	float4 bbox;        // minX, minY, maxX, maxY after stroke expansion
	float4 shape;       // rect: center + half size; ellipse: center + radii;
	                    // line: (x0,y0), (x1,y1)
	float4 params;      // radius, strokeWidth, shapeType, point count
	float4 fill0;
	float4 fill1;
	float4 line0;
	float4 line1;
	float4 fillGrad;    // x0, y0, x1, y1
	float4 lineGrad;
	float4 flags;       // fillGradient, lineGradient, hasFill, hasStroke
	float2 pts[64];
};

struct OSDShapeVtx
{
	float4 position [[position]];
	float2 pixel;
	uint   index;       // integer stage-in values are flat in MSL
};

struct OSDQuadVtx
{
	float4 position [[position]];
	float2 texCoord;
};

// Mirror of DisplayViewShaderProperties in MacMetalDisplayView.h.
struct OSDViewProps
{
	float width;
	float height;
	float rotation;
	float viewScale;
	uint  lowerHUDMipMapLevel;
};

static inline float2 QuadCorner(uint vid)
{
	// Two triangles: 0,1,2 / 0,2,3 with corners TL, TR, BR, BL.
	if (vid == 0u) return float2(0.0f, 0.0f);
	if (vid == 1u) return float2(1.0f, 0.0f);
	if (vid == 2u) return float2(1.0f, 1.0f);
	if (vid == 3u) return float2(0.0f, 0.0f);
	if (vid == 4u) return float2(1.0f, 1.0f);
	return float2(0.0f, 1.0f);
}

static inline float4 ProjectPixel(float2 p, float2 size)
{
	return float4(p.x / size.x * 2.0f - 1.0f,
	              1.0f - p.y / size.y * 2.0f,
	              0.0f, 1.0f);
}

// ---------------------------------------------------------------------------
// Shape pipeline: one instanced quad per primitive, SDF in the fragment stage
// ---------------------------------------------------------------------------

vertex OSDShapeVtx osd_shape_vertex(const device OSDShapeInstance *instances [[buffer(0)]],
                                    constant float4 &uniforms [[buffer(1)]],
                                    uint vid [[vertex_id]],
                                    uint iid [[instance_id]])
{
	const float4 b = instances[iid].bbox;
	float2 p;

	if ((vid == 0u) || (vid == 3u))      p = float2(b.x, b.y);
	else if (vid == 1u)                  p = float2(b.z, b.y);
	else if ((vid == 2u) || (vid == 4u)) p = float2(b.z, b.w);
	else                                  p = float2(b.x, b.w);

	OSDShapeVtx out;
	out.position = ProjectPixel(p, uniforms.xy);
	out.pixel    = p;
	out.index    = iid;
	return out;
}

static float sdBox(float2 p, float2 c, float2 h)
{
	const float2 q = abs(p - c) - h;
	return length(max(q, float2(0.0f))) + min(max(q.x, q.y), 0.0f);
}

static float sdRoundBox(float2 p, float2 c, float2 h, float r)
{
	const float2 q = abs(p - c) - h + float2(r);
	return length(max(q, float2(0.0f))) + min(max(q.x, q.y), 0.0f) - r;
}

// Radial distance divided by the gradient magnitude of the implicit function,
// so flat sides of a non-circular ellipse get the right coverage.
static float sdEllipse(float2 p, float2 c, float2 r)
{
	const float2 rr = max(r, float2(1e-4f));
	const float2 q  = (p - c) / rr;
	const float  l  = length(q);
	const float  g  = max(length(q / rr), 1e-6f);
	return (l - 1.0f) * l / g;
}

static float sdSegment(float2 p, float2 a, float2 b)
{
	const float2 pa = p - a;
	const float2 ba = b - a;
	const float  h  = clamp(dot(pa, ba) / max(dot(ba, ba), 1e-6f), 0.0f, 1.0f);
	return length(pa - ba * h);
}

// Even-odd polygon distance. agg_osd.cpp's D-pad outline is concave, so a
// convex test would be wrong.
static float sdPolygon(float2 p, uint n, const device OSDShapeInstance &inst)
{
	float d = dot(p - inst.pts[0], p - inst.pts[0]);
	float s = 1.0f;

	for (uint i = 0u; i < n; i++)
	{
		const uint  j  = (i + n - 1u) % n;
		const float2 vi = inst.pts[i];
		const float2 vj = inst.pts[j];
		const float2 e  = vj - vi;
		const float2 w  = p - vi;
		const float2 bb = w - e * clamp(dot(w, e) / max(dot(e, e), 1e-6f), 0.0f, 1.0f);
		d = min(d, dot(bb, bb));

		const bool c1 = p.y >= vi.y;
		const bool c2 = p.y <  vj.y;
		const bool c3 = e.x * w.y > e.y * w.x;

		if ((c1 && c2 && c3) || (!c1 && !c2 && !c3))
		{
			s = -s;
		}
	}

	return s * sqrt(d);
}

// Open path: distance to the segments only, no closing edge and no fill.
static float sdPolyline(float2 p, uint n, const device OSDShapeInstance &inst)
{
	float d = 1e20f;
	for (uint i = 1u; i < n; i++)
	{
		d = min(d, sdSegment(p, inst.pts[i - 1u], inst.pts[i]));
	}
	return d;
}

static float shapeDistance(float2 p, const device OSDShapeInstance &inst)
{
	const int shapeType = int(inst.params.z + 0.5f);
	const float2 c = inst.shape.xy;

	if (shapeType == 1) return sdRoundBox(p, c, inst.shape.zw, inst.params.x);
	if (shapeType == 2) return sdEllipse(p, c, inst.shape.zw);
	if (shapeType == 3) return sdPolygon(p, uint(inst.params.w + 0.5f), inst);
	if (shapeType == 4) return sdSegment(p, inst.shape.xy, inst.shape.zw);
	if (shapeType == 5) return sdPolyline(p, uint(inst.params.w + 0.5f), inst);

	return sdBox(p, c, inst.shape.zw);
}

static float4 gradientColor(float4 c0, float4 c1, float4 endpoints, float2 p, bool enabled)
{
	if (!enabled) return c0;

	const float2 d  = endpoints.zw - endpoints.xy;
	const float  dd = dot(d, d);
	float t = 0.0f;
	if (dd > 1e-6f)
	{
		t = clamp(dot(p - endpoints.xy, d) / dd, 0.0f, 1.0f);
	}
	return mix(c0, c1, t);
}

// AGG's rasterizer antialiases over roughly one pixel; match that width.
static float coverage(float d)
{
	return clamp(0.5f - d, 0.0f, 1.0f);
}

fragment float4 osd_shape_fragment(OSDShapeVtx input [[stage_in]],
                                   const device OSDShapeInstance *instances [[buffer(0)]])
{
	const device OSDShapeInstance &inst = instances[input.index];
	const float d = shapeDistance(input.pixel, inst);
	const float halfStroke = inst.params.y * 0.5f;

	float fillAlpha = 0.0f;
	float4 fillColor = float4(0.0f);
	if (inst.flags.z > 0.5f)
	{
		fillColor = gradientColor(inst.fill0, inst.fill1, inst.fillGrad,
		                          input.pixel, inst.flags.x > 0.5f);
		fillAlpha = fillColor.a * coverage(d);
	}

	float strokeAlpha = 0.0f;
	float4 strokeColor = float4(0.0f);
	if (inst.flags.w > 0.5f)
	{
		strokeColor = gradientColor(inst.line0, inst.line1, inst.lineGrad,
		                            input.pixel, inst.flags.y > 0.5f);
		strokeAlpha = strokeColor.a * coverage(abs(d) - halfStroke);
	}

	// Fill and stroke both feed one premultiplied source-over result.
	const float4 fill   = float4(fillColor.rgb * fillAlpha, fillAlpha);
	const float4 stroke = float4(strokeColor.rgb * strokeAlpha, strokeAlpha);

	return float4(stroke.rgb + fill.rgb * (1.0f - strokeAlpha),
	              strokeAlpha + fillAlpha * (1.0f - strokeAlpha));
}

// ---------------------------------------------------------------------------
// Glyph pipeline: one instanced quad per character, R8 coverage atlas
// ---------------------------------------------------------------------------

struct OSDGlyphIn
{
	float4 posSize [[attribute(0)]];  // x, y, w, h in target pixels
	float4 uvRect  [[attribute(1)]];  // u0, v0, u1, v1
	float4 color   [[attribute(2)]];  // straight RGBA
};

struct OSDGlyphVtx
{
	float4 position [[position]];
	float2 uv;
	float4 color;
};

vertex OSDGlyphVtx osd_glyph_vertex(OSDGlyphIn in [[stage_in]],
                                    constant float4 &uniforms [[buffer(1)]],
                                    uint vid [[vertex_id]])
{
	const float2 c = QuadCorner(vid);
	const float2 p = in.posSize.xy + c * in.posSize.zw;

	OSDGlyphVtx out;
	out.position = ProjectPixel(p, uniforms.xy);
	out.uv       = float2(mix(in.uvRect.x, in.uvRect.z, c.x),
	                      mix(in.uvRect.y, in.uvRect.w, c.y));
	out.color    = in.color;
	return out;
}

constexpr sampler osdAtlasSampler(coord::normalized,
                                  address::clamp_to_edge,
                                  filter::nearest);

fragment float4 osd_glyph_fragment(OSDGlyphVtx input [[stage_in]],
                                   texture2d<float> atlas [[texture(0)]])
{
	const float cover = atlas.sample(osdAtlasSampler, input.uv).r;
	const float alpha = input.color.a * cover;
	return float4(input.color.rgb * alpha, alpha);
}

// ---------------------------------------------------------------------------
// CPU fallback layer: full-screen blit of a premultiplied BGRA8 texture
// ---------------------------------------------------------------------------

vertex OSDQuadVtx osd_blit_vertex(uint vid [[vertex_id]])
{
	const float2 c = QuadCorner(vid);

	OSDQuadVtx out;
	out.position = float4(c.x * 2.0f - 1.0f, 1.0f - c.y * 2.0f, 0.0f, 1.0f);
	out.texCoord = c;
	return out;
}

fragment float4 osd_blit_fragment(OSDQuadVtx input [[stage_in]],
                                  texture2d<float> tex [[texture(0)]])
{
	return tex.sample(osdAtlasSampler, input.texCoord);
}

// ---------------------------------------------------------------------------
// Composite: the 256x384 HUD texture onto the two NDS screen quads.
// The upper half maps to the main screen, the lower half to the touch screen.
// ---------------------------------------------------------------------------

vertex OSDQuadVtx osd_overlay_vertex(const device float2 *inPosition [[buffer(0)]],
                                     const device float2 *inTexCoord [[buffer(1)]],
                                     constant OSDViewProps &viewProps [[buffer(2)]],
                                     constant uint8_t &doYFlip [[buffer(3)]],
                                     uint vid [[vertex_id]])
{
	// Same projection as display_output_vertex in MacMetalDisplayViewShaders.metal.
	const float angleRadians = viewProps.rotation * (M_PI_F / 180.0f);

	const float2x2 projection = float2x2(float2(2.0f / viewProps.width, 0.0f),
	                                     float2(0.0f, 2.0f / viewProps.height));
	const float2x2 rotation   = float2x2(float2( cos(angleRadians), sin(angleRadians)),
	                                     float2(-sin(angleRadians), cos(angleRadians)));
	const float2x2 scale      = float2x2(float2(viewProps.viewScale, 0.0f),
	                                     float2(0.0f, viewProps.viewScale));
	const float2 yFlip = (doYFlip != 0) ? float2(1.0f, -1.0f) : float2(1.0f, 1.0f);

	OSDQuadVtx out;
	out.position = float4(projection * rotation * scale * inPosition[vid] * yFlip, 0.0f, 1.0f);
	out.texCoord = inTexCoord[vid];  // already normalized into the 256x384 atlas
	return out;
}

fragment float4 osd_overlay_fragment(OSDQuadVtx input [[stage_in]],
                                     texture2d<float> tex [[texture(0)]])
{
	// The HUD texture is premultiplied; the pipeline uses
	// One / OneMinusSourceAlpha so no extra multiply happens here.
	return tex.sample(osdAtlasSampler, input.texCoord);
}
