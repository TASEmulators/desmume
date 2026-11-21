// super-xBR, pass 1 of 3 of the pixel filter of the WebGPU renderer.
//
// The same shader as SuperXBR_Pass0.hlsl and super-xbr-pass0.glsl, all three
// ported from the libretro GLSL shader "super-xbr-pass0.glsl" (libretro/glsl-
// shaders, super-xbr/shaders).  The OpenGL 3.3 renderer runs the upstream file
// as it is, Direct3D 11 runs the HLSL port, and this is the WGSL one.
//
// super-xBR: the edge data, the picture at twice the size blended with the
// frame it came from, and the windowed jinc filter that finishes it.
//
//   pass 0  the edge data, at the frame size
//   pass 1  the doubled picture blended with the frame, at twice its size
//   pass 2  the jinc filter, at whatever size the target has
//
// Upstream computes the four tap rectangles in its vertex shader and hands
// them on as varyings; here they are computed in the pixel shader, which is
// what the other ports do as well - the renderer has one quad vertex shader
// for every upscaler.
//
// The taps are read with textureLoad and integer coordinates: every one of
// them is a whole number of texels from the sample, which is the texel a point
// sampler gives, read exactly and without the sampler state.
//
// The licence of the ported shader:
//
//   *******  Super XBR Shader - pass0  *******
//
//   Copyright (c) 2015 Hyllian - sergiogdb@gmail.com
//
//   Permission is hereby granted, free of charge, to any person obtaining a copy
//   of this software and associated documentation files (the "Software"), to deal
//   in the Software without restriction, including without limitation the rights
//   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//   copies of the Software, and to permit persons to whom the Software is
//   furnished to do so, subject to the following conditions:
//
//   The above copyright notice and this permission notice shall be included in
//   all copies or substantial portions of the Software.
//
//   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//   THE SOFTWARE.

// The block the renderer hands every pass, the one WgpuDisplay::prepareUpscaler()
// fills:
//
//   sourceSize.xy: the size the pass reads, zw: one texel (1 / xy)
//   outputSize.xy: the size of its target, zw: one pixel of it
//   scale.xy:      outputSize.xy / sourceSize.xy
//   secondSize.xy: the size of a second texture, zw: one texel of it
struct UpscalerConstants
{
    sourceSize : vec4f,
    outputSize : vec4f,
    scale      : vec4f,
    secondSize : vec4f,
};

@group(0) @binding(0) var sourceTexture : texture_2d<f32>;
@group(0) @binding(1) var sourceSampler : sampler;
@group(0) @binding(2) var<uniform> constants : UpscalerConstants;

struct VertexOutput
{
    @builtin(position) position : vec4f,
    @location(0) uv : vec2f,
};

@vertex
fn vsMain(@builtin(vertex_index) index : u32) -> VertexOutput
{
    // The triangle strip of the renderer: -1,-1 then -1,1 then 1,-1 then 1,1.
    let x = f32(index >> 1u) * 2.0 - 1.0;
    let y = f32(index & 1u) * 2.0 - 1.0;

    var out : VertexOutput;
    out.position = vec4f(x, y, 0.0, 1.0);
    // The texture is uploaded top row first, so v runs the other way.
    out.uv = vec2f((x + 1.0) * 0.5, (1.0 - y) * 0.5);
    return out;
}

// The settings the shader ships with, from the #pragma parameter lines of the
// upstream file.  The renderer has no parameter menu, so they are its defaults.
//
// MODE ships as 0, "normal"; upstream's other two - 1 "details" and 2
// "adaptive" - pick different weights and a contrast term, and are left out
// here the way the xbr-lv2 port leaves its unused branches out.
const XBR_EDGE_STR       = 0.6;
const XBR_WEIGHT         = 1.0;
const XBR_ANTI_RINGING   = 1.0;
const XBR_EDGE_SHP       = 0.4;
const XBR_TEXTURE_SHP    = 1.0;

const Y = vec3f(0.2126, 0.7152, 0.0722);

fn RGBtoYUV(color : vec3f) -> f32
{
    return dot(color, Y);
}

fn df(A : f32, B : f32) -> f32
{
    return abs(A - B);
}

/*
                              P1
     |P0|B |C |P1|         C     F4          |a0|b1|c2|d3|
     |D |E |F |F4|      B     F     I4       |b0|c1|d2|e3|   |e1|i1|i2|e2|
     |G |H |I |I4|   P0    E  A  I     P3    |c0|d1|e2|f3|   |e3|i3|i4|e4|
     |P2|H5|I5|P3|      D     H     I5       |d0|e1|f2|g3|
                           G     H5
                              P2
*/

fn d_wd(wp1 : f32, wp2 : f32, wp3 : f32, wp4 : f32, wp5 : f32, wp6 : f32,
        b0 : f32, b1 : f32, c0 : f32, c1 : f32, c2 : f32, d0 : f32, d1 : f32,
        d2 : f32, d3 : f32, e1 : f32, e2 : f32, e3 : f32, f2 : f32, f3 : f32) -> f32
{
    return (wp1 * (df(c1, c2) + df(c1, c0) + df(e2, e1) + df(e2, e3)) + wp2 * (df(d2, d3) + df(d0, d1)) + wp3 * (df(d1, d3) + df(d0, d2)) + wp4 * df(d1, d2) + wp5 * (df(c0, c2) + df(e1, e3)) + wp6 * (df(b0, b1) + df(f2, f3)));
}

fn hv_wd(wp1 : f32, wp2 : f32, wp3 : f32, wp4 : f32, wp5 : f32, wp6 : f32,
         i1 : f32, i2 : f32, i3 : f32, i4 : f32, e1 : f32, e2 : f32, e3 : f32, e4 : f32) -> f32
{
    return (wp4 * (df(i1, i2) + df(i3, i4)) + wp1 * (df(i1, e1) + df(i2, e2) + df(i3, e3) + df(i4, e4)) + wp3 * (df(i1, e2) + df(i3, e4) + df(e1, i2) + df(e3, i4)));
}

fn min4(a : vec3f, b : vec3f, c : vec3f, d : vec3f) -> vec3f
{
    return min(a, min(b, min(c, d)));
}

fn max4(a : vec3f, b : vec3f, c : vec3f, d : vec3f) -> vec3f
{
    return max(a, max(b, max(c, d)));
}

// The vec4 * mat4x3 the upstream writes as mul(w, mat4x3(c0, c1, c2, c3)): a
// row vector times a matrix of four columns is the four columns weighted and
// summed.
fn mul4x3(w : vec4f, c0 : vec3f, c1 : vec3f, c2 : vec3f, c3 : vec3f) -> vec3f
{
    return w.x * c0 + w.y * c1 + w.z * c2 + w.w * c3;
}

// One texel of the frame, by offset in texels from the sample.  The index is
// the floor of the coordinate in texels, clamped to the edge, which is the
// texel a clamped point sampler reads.
fn tap(coord : vec2f, x : f32, y : f32) -> vec3f
{
    let size  = vec2f(constants.sourceSize.xy);
    let uv    = coord + constants.sourceSize.zw * vec2f(x, y);
    let index = clamp(vec2i(floor(uv * size)), vec2i(0), vec2i(size) - vec2i(1));

    return textureLoad(sourceTexture, index, 0).rgb;
}

@fragment
fn fsMain(input : VertexOutput) -> @location(0) vec4f
{
    // settings //
    // MODE 0, the "normal" the upstream file ships with.
    let wp1 = 2.0;
    let wp2 = 1.0;
    let wp3 = -1.0;
    let wp4 = 4.0;
    let wp5 = -1.0;
    let wp6 = 1.0;

    let weight1 = (XBR_WEIGHT * 1.29633 / 10.0);
    let weight2 = (XBR_WEIGHT * 1.75068 / 10.0 / 2.0);
    // end settings //

    let coord = input.uv;

    let P0 = tap(coord, -1.0, -1.0);
    let P1 = tap(coord,  2.0, -1.0);
    let P2 = tap(coord, -1.0,  2.0);
    let P3 = tap(coord,  2.0,  2.0);

    let  B = tap(coord,  0.0, -1.0);
    let  C = tap(coord,  1.0, -1.0);
    let H5 = tap(coord,  0.0,  2.0);
    let I5 = tap(coord,  1.0,  2.0);

    let  D = tap(coord, -1.0,  0.0);
    let F4 = tap(coord,  2.0,  0.0);
    let  G = tap(coord, -1.0,  1.0);
    let I4 = tap(coord,  2.0,  1.0);

    let  E = tap(coord,  0.0,  0.0);
    let  F = tap(coord,  1.0,  0.0);
    let  H = tap(coord,  0.0,  1.0);
    let  I = tap(coord,  1.0,  1.0);

    let b = RGBtoYUV(B);
    let c = RGBtoYUV(C);
    let d = RGBtoYUV(D);
    let e = RGBtoYUV(E);
    let f = RGBtoYUV(F);
    let g = RGBtoYUV(G);
    let h = RGBtoYUV(H);
    let i = RGBtoYUV(I);

    let i4 = RGBtoYUV(I4); let p0 = RGBtoYUV(P0);
    let i5 = RGBtoYUV(I5); let p1 = RGBtoYUV(P1);
    let h5 = RGBtoYUV(H5); let p2 = RGBtoYUV(P2);
    let f4 = RGBtoYUV(F4); let p3 = RGBtoYUV(P3);

    // Calc edgeness in diagonal directions.
    let d_edge = (d_wd(wp1, wp2, wp3, wp4, wp5, wp6, d, b, g, e, c, p2, h, f, p1, h5, i, f4, i5, i4) - d_wd(wp1, wp2, wp3, wp4, wp5, wp6, c, f4, b, f, i4, p0, e, i, p3, d, h, i5, g, h5));

    // Calc edgeness in horizontal/vertical directions.
    let hv_edge = (hv_wd(wp1, wp2, wp3, wp4, wp5, wp6, f, i, e, h, c, i5, b, h5) - hv_wd(wp1, wp2, wp3, wp4, wp5, wp6, e, f, h, i, d, f4, g, i4));

    let limits = XBR_EDGE_STR + 0.000001;
    let edge_strength = smoothstep(0.0, limits, abs(d_edge));

    // Filter weights. Two taps only.
    let w1 = vec4f(-weight1, weight1 + 0.5, weight1 + 0.5, -weight1);
    let w2 = vec4f(-weight2, weight2 + 0.25, weight2 + 0.25, -weight2);

    let c3 = mul4x3(w2, D + G, E + H, F + I, F4 + I4);
    let c4 = mul4x3(w2, C + B, F + E, I + H, I5 + H5);

    // Filtering and normalization in four direction generating four colors.
    let c1 = mul4x3(w1, P2, H, F, P1);
    let c2 = mul4x3(w1, P0, E, I, P3);

    // Smoothly blends the two strongest directions (one in diagonal and the
    // other in vert/horiz direction).
    let step_d  = select(0.0, 1.0, d_edge  >= 0.0);
    let step_hv = select(0.0, 1.0, hv_edge >= 0.0);

    var color = mix(mix(c1, c2, step_d), mix(c3, c4, step_hv), 1.0 - edge_strength);

    // Anti-ringing code.
    let min_sample = min4(E, F, H, I) + (1.0 - XBR_ANTI_RINGING) * mix((P2 - H) * (F - P1), (P0 - E) * (I - P3), step_d);
    let max_sample = max4(E, F, H, I) - (1.0 - XBR_ANTI_RINGING) * mix((P2 - H) * (F - P1), (P0 - E) * (I - P3), step_d);
    color = clamp(color, min_sample, max_sample);

    return vec4f(color, 1.0);
}
