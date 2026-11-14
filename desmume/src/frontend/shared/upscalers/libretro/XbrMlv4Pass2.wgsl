// xBR-mlv4, pass 2 of 4 of the pixel filter of the WebGPU renderer.
//
// The same shader as XBR_Mlv4_Pass2.hlsl and xbr-mlv4-pass2.glsl, all three
// ported from the libretro GLSL shader "xbr-mlv4-pass2.glsl" (libretro/glsl-
// shaders, xbr/shaders).  It is the pass that reads two textures: the edge
// data pass 1 wrote, at binding 0, and the frame at binding 3.
//
// The licence of the ported shader:
//
//   Hyllian's xBR MultiLevel4 Shader - Pass2
//
//   Copyright (C) 2011-2015 Hyllian - sergiogdb@gmail.com
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
//
// What the pass does is take the eight levels of pass 1 back to a number and
// lower the ones whose neighbours do not agree with them: an edge of level 7
// or 8 that is not jagged stays, and the same edge of level 5 or 6 needs less
// to survive.  The four channels are the four quarters of the pixel, as they
// were in pass 1.
//
// One thing here is upstream's and is kept as it is: the distances are the Y
// of YUV times 48, with no abs() around it, where pass 1 weights three axes
// and takes the absolute value of each.
//
// jag2 is fixed, and fixed in all three copies at once.  Upstream assigns its
// .x four times and never its .y, .z or .w, which glslang zeroes - three
// quarters of the channels read false, which looks like a copy-paste slip
// rather than intent.  The GLSL and the HLSL beside this file now carry the
// four assignments, and the three renderers still agree.

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
@group(0) @binding(3) var secondTexture : texture_2d<f32>;

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

const MAXIMO = vec4f(255.0, 255.0, 255.0, 255.0);

// mul(float4x3(B, D, H, F), yuv_weighted[0]): the first column of upstream's
// weighted matrix, times 48 - the Y axis on its own.
const LUMA = vec3f(48.0 * 0.299, 48.0 * 0.587, 48.0 * 0.114);

fn lt4(A : vec4f, B : vec4f) -> vec4f { return 1.0 - step(B, A); } // A <  B
fn le4(A : vec4f, B : vec4f) -> vec4f { return step(A, B); }       // A <= B
fn not4(A : vec4f) -> vec4f { return 1.0 - A; }
fn or4(A : vec4f, B : vec4f) -> vec4f { return max(A, B); }
fn and4(A : vec4f, B : vec4f) -> vec4f { return A * B; }

fn choose(cond : vec4f, onTrue : vec4f, onFalse : vec4f) -> vec4f
{
    return onFalse + cond * (onTrue - onFalse);
}

// The masks of the comparisons against a constant, and against zero.
fn gt4(A : vec4f, B : f32) -> vec4f { return 1.0 - step(A, vec4f(B)); }
fn eq4(A : vec4f, B : f32) -> vec4f { return select(vec4f(0.0), vec4f(1.0), A == vec4f(B)); }

fn df(A : vec4f, B : vec4f) -> vec4f { return abs(A - B); }

fn id(A : vec4f, B : vec4f, C : vec4f, D : vec4f) -> vec4f
{
    return 1.0 - le4(df(C, D), df(A, B));
}

fn rd(A : vec4f, B : vec4f, C : vec4f, D : vec4f) -> vec4f
{
    return gt4(df(C, D) / (df(A, B) + 0.000000001), 2.0);
}

// Upstream rounds with floor(x + 0.5), and the values here are exactly halfway
// often enough that the two roundings differ.
fn roundHalf(x : f32) -> f32
{
    return floor(x + 0.5);
}

fn remapFrom01(v : vec4f, high : vec4f) -> vec4f
{
    return vec4f(roundHalf(high.x * v.x), roundHalf(high.y * v.y),
                 roundHalf(high.z * v.z), roundHalf(high.w * v.w));
}

fn remapTo01(v : vec4f, high : vec4f) -> vec4f
{
    return v / high;
}

// The edge data pass 1 wrote, at an offset of whole texels.
fn flagTexel(base : vec2i, x : i32, y : i32) -> vec4f
{
    let size = vec2i(constants.sourceSize.xy);
    let at   = clamp(base + vec2i(x, y), vec2i(0), size - vec2i(1));

    return textureLoad(sourceTexture, at, 0);
}

// The frame, at the same offset.
fn frameTexel(base : vec2i, x : i32, y : i32) -> vec3f
{
    let size = vec2i(constants.secondSize.xy);
    let at   = clamp(base + vec2i(x, y), vec2i(0), size - vec2i(1));

    return textureLoad(secondTexture, at, 0).rgb;
}

fn luma(c : vec3f) -> f32
{
    return dot(c, LUMA);
}

fn luma4(a : vec3f, b : vec3f, c : vec3f, d : vec3f) -> vec4f
{
    return vec4f(luma(a), luma(b), luma(c), luma(d));
}

@fragment
fn fsMain(input : VertexOutput) -> @location(0) vec4f
{
    //    A1 B1 C1
    // A0  A  B  C C4
    // D0  D  E  F F4
    // G0  G  H  I I4
    //    G5 H5 I5
    let flags = vec2i(floor(input.uv * constants.sourceSize.xy));
    let frame = vec2i(floor(input.uv * constants.secondSize.xy));

    let A1 = frameTexel(frame, -1, -2);
    let B1 = frameTexel(frame,  0, -2);
    let C1 = frameTexel(frame,  1, -2);

    let A  = frameTexel(frame, -1, -1);
    let B  = frameTexel(frame,  0, -1);
    let C  = frameTexel(frame,  1, -1);

    let D  = frameTexel(frame, -1,  0);
    let E  = frameTexel(frame,  0,  0);
    let F  = frameTexel(frame,  1,  0);

    let G  = frameTexel(frame, -1,  1);
    let H  = frameTexel(frame,  0,  1);
    let I  = frameTexel(frame,  1,  1);

    let G5 = frameTexel(frame, -1,  2);
    let H5 = frameTexel(frame,  0,  2);
    let I5 = frameTexel(frame,  1,  2);

    let A0 = frameTexel(frame, -2, -1);
    let D0 = frameTexel(frame, -2,  0);
    let G0 = frameTexel(frame, -2,  1);

    let C4 = frameTexel(frame,  2, -1);
    let F4 = frameTexel(frame,  2,  0);
    let I4 = frameTexel(frame,  2,  1);

    let b = luma4(B, D, H, F);
    let c = luma4(C, A, G, I);
    let e = luma4(E, E, E, E);

    let d = b.yzwx;
    let f = b.wxyz;
    let g = c.zwxy;
    let h = b.zwxy;
    let i = c.wxyz;

    let i4 = luma4(I4, C1, A0, G5);
    let i5 = luma4(I5, C4, A1, G0);
    let h5 = luma4(H5, F4, B1, D0);

    let f4 = h5.yzwx;

    // The eight levels of this pixel and of the two neighbours the edge would
    // run into, back out of the channels of the edge data: 255 * it + 0.5.
    var pe = remapFrom01(flagTexel(flags, 0, 0), MAXIMO);
    let pb = remapFrom01(flagTexel(flags, 0, -1), MAXIMO);
    let pd = remapFrom01(flagTexel(flags, -1, 0), MAXIMO);
    let pf = remapFrom01(flagTexel(flags, 1, 0), MAXIMO);
    let ph = remapFrom01(flagTexel(flags, 0, 1), MAXIMO);

    let f2 = vec4f(pf.z, pb.w, pd.x, ph.y);
    let h2 = vec4f(ph.z, pf.w, pb.x, pd.y);
    let f1 = vec4f(pf.y, pb.z, pd.w, ph.x);
    let h3 = vec4f(ph.w, pf.x, pb.y, pd.z);

    let one = vec4f(1.0);

    // A neighbour above one has an edge of its own.
    let nbrs = or4(gt4(pe.yzwx, 1.0), gt4(pe.wxyz, 1.0));
    let jag1 = or4(gt4(f2, 1.0), gt4(h2, 1.0));
    let jag2 = or4(gt4(f2, 2.0), gt4(h2, 2.0));
    let jag3 = or4(gt4(f2, 4.0), gt4(h2, 4.0));

    pe = choose(eq4(pe, 7.0) + eq4(pe, 8.0), choose(jag3, pe, pe - 2.0), pe);
    pe = choose(eq4(pe, 5.0) + eq4(pe, 6.0), choose(jag2, pe, pe - 2.0), pe);

    let jag91 = and4(and4(or4(id(h, i, e, h), id(i4, i, f4, i4)), gt4(f2, 1.0)),
                     gt4(f1, 1.0));
    let jag92 = and4(and4(or4(id(f, i, e, f), id(i5, i, h5, i5)), gt4(h2, 1.0)),
                     gt4(h3, 1.0));

    let jag93 = rd(h, g, e, g);
    let jag94 = rd(f, c, e, c);

    let jag9 = not4(or4(and4(jag91, jag93), and4(jag92, jag94)));

    pe = choose(or4(eq4(pe, 0.0), and4(or4(not4(nbrs), jag1), jag9)), pe, one);

    return remapTo01(pe, MAXIMO);
}
