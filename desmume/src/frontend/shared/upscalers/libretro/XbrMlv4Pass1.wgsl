// xBR-mlv4, pass 1 of 4 of the pixel filter of the WebGPU renderer.
//
// The same shader as XBR_Mlv4_Pass1.hlsl and xbr-mlv4-pass1.glsl, all three
// ported from the libretro GLSL shader "xbr-mlv4-pass1.glsl" (libretro/glsl-
// shaders, xbr/shaders).  The OpenGL 3.3 renderer runs the upstream file as it
// is, from the resource XBR_MLV4_PASS1_GLSL; Direct3D 11 runs the HLSL port,
// and this is the WGSL one.
//
//   pass 1  the edge data of every pixel, at the frame size
//   pass 2  the data again, corrected against the neighbours' data
//   pass 3  the line each of the four quarters of the pixel follows, at twice
//           the frame size
//   pass 4  the interpolation along those lines, at whatever size the target
//           has
//
// The licence of the ported shader:
//
//   Hyllian's xBR MultiLevel4 Shader - Pass1
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
// The four taps of a weighted_distance() call are not four texels: upstream
// measures every tap in YUV, weights the three axes by 4, 1 and 2, and adds
// the absolute values up, which is a number per texel.  b, c, e and the six
// taps further out are each four of those, so the vector b holds the numbers
// of B, D, H and F - in that order, because that is the order upstream builds
// its mat4x3 in - and d, f, g, h and i are rotations of b and c.

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

const EQ_THRESHOLD  = vec4f(  2.0,   2.0,   2.0,   2.0);
const EQ_THRESHOLD3 = vec4f( 25.0,  25.0,  25.0,  25.0);
const MAXIMO        = vec4f(255.0, 255.0, 255.0, 255.0);

// A bvec4 is a mask of 1.0s and 0.0s here rather than a vec4<bool>: the
// restrictions below are ands and ors of six of them at a time, and a GLSL
// bvec4 in the other spelling - vec4<bool> - cannot be multiplied.  The two
// produce the same pixels; the masks are what the arithmetic needs.
fn lt4(A : vec4f, B : vec4f) -> vec4f { return 1.0 - step(B, A); } // A <  B
fn le4(A : vec4f, B : vec4f) -> vec4f { return step(A, B); }       // A <= B
fn ne4(A : vec4f, B : vec4f) -> vec4f { return select(vec4f(0.0), vec4f(1.0), A != B); }
fn not4(A : vec4f) -> vec4f { return 1.0 - A; }
fn or4(A : vec4f, B : vec4f) -> vec4f { return max(A, B); }
fn and4(A : vec4f, B : vec4f) -> vec4f { return A * B; }

// The ternary of the last block, on masks.
fn choose(cond : vec4f, onTrue : vec4f, onFalse : vec4f) -> vec4f
{
    return onFalse + cond * (onTrue - onFalse);
}

fn df(A : vec4f, B : vec4f) -> vec4f { return abs(A - B); }

fn eq(A : vec4f, B : vec4f) -> vec4f { return lt4(df(A, B), EQ_THRESHOLD); }
fn eq3(A : vec4f, B : vec4f) -> vec4f { return lt4(df(A, B), EQ_THRESHOLD3); }

fn id(A : vec4f, B : vec4f, C : vec4f, D : vec4f) -> vec4f
{
    return 1.0 - le4(df(C, D), df(A, B));
}

fn weighted_distance(a : vec4f, b : vec4f, c : vec4f, d : vec4f,
                     e : vec4f, f : vec4f, g : vec4f, h : vec4f) -> vec4f
{
    return df(a, b) + df(a, c) + df(d, e) + df(d, f) + 4.0 * df(g, h);
}

// One texel of the frame, at an offset of whole texels from the one the sample
// was snapped to.
fn tap(base : vec2i, x : i32, y : i32) -> vec3f
{
    let size = vec2i(constants.sourceSize.xy);
    let at   = clamp(base + vec2i(x, y), vec2i(0), size - vec2i(1));

    return textureLoad(sourceTexture, at, 0).rgb;
}

// Upstream's mul(float4x3(...), yuvT), the abs() of every one of its twelve
// components, and mul(..., Yuv_weight): pitch, chroma and the third axis of
// the three colours, weighted 4, 1 and 2, as one number per colour.
fn yuvDistance(c : vec3f) -> f32
{
    let y = dot(c, vec3f( 0.299,  0.587,  0.114));
    let u = dot(c, vec3f(-0.169, -0.331,  0.499));
    let v = dot(c, vec3f( 0.499, -0.418, -0.0813));

    return 4.0 * abs(y) + abs(u) + 2.0 * abs(v);
}

fn yuv4(a : vec3f, b : vec3f, c : vec3f, d : vec3f) -> vec4f
{
    return vec4f(yuvDistance(a), yuvDistance(b), yuvDistance(c), yuvDistance(d));
}

fn remapTo01(v : vec4f, high : vec4f) -> vec4f
{
    return v / high;
}

@fragment
fn fsMain(input : VertexOutput) -> @location(0) vec4f
{
    // The texel the sample was snapped to, which every tap below is an offset
    // of.  Upstream makes the same snap with the (0.5 / size) the texel centre
    // of the interpolated coordinate is, and then walks in whole texels.
    let base = vec2i(floor(input.uv * constants.sourceSize.xy));

    //      A3 B3 C3
    //      A1 B1 C1
    // A2 A0  A  B  C C4 C6
    // D2 D0  D  E  F F4 F6
    // G2 G0  G  H  I I4 I6
    //      G5 H5 I5
    //      G7 H7 I7
    let A3 = tap(base, -1, -3); let B3 = tap(base, 0, -3); let C3 = tap(base, 1, -3);
    let A1 = tap(base, -1, -2); let B1 = tap(base, 0, -2); let C1 = tap(base, 1, -2);
    let A2 = tap(base, -3, -1); let A0 = tap(base, -2, -1);
    let A  = tap(base, -1, -1); let B  = tap(base,  0, -1); let C  = tap(base,  1, -1);
    let C4 = tap(base,  2, -1); let C6 = tap(base,  3, -1);
    let D2 = tap(base, -3,  0); let D0 = tap(base, -2,  0);
    let D  = tap(base, -1,  0); let E  = tap(base,  0,  0); let F  = tap(base,  1,  0);
    let F4 = tap(base,  2,  0); let F6 = tap(base,  3,  0);
    let G2 = tap(base, -3,  1); let G0 = tap(base, -2,  1);
    let G  = tap(base, -1,  1); let H  = tap(base,  0,  1); let I  = tap(base,  1,  1);
    let I4 = tap(base,  2,  1); let I6 = tap(base,  3,  1);
    let G5 = tap(base, -1,  2); let H5 = tap(base,  0,  2); let I5 = tap(base,  1,  2);
    let G7 = tap(base, -1,  3); let H7 = tap(base,  0,  3); let I7 = tap(base,  1,  3);

    let b = yuv4(B, D, H, F);
    let c = yuv4(C, A, G, I);
    let e = yuv4(E, E, E, E);

    // The four numbers of b and c, rotated so that each names the tap it came
    // from: d is D and its three, f is F and its three, and so on.
    let d = b.yzwx;
    let f = b.wxyz;
    let g = c.zwxy;
    let h = b.zwxy;
    let i = c.wxyz;

    let i4 = yuv4(I4, C1, A0, G5);
    let i5 = yuv4(I5, C4, A1, G0);
    let h5 = yuv4(H5, F4, B1, D0);

    let f4 = h5.yzwx;

    let c1 = i4.yzwx;
    let g0 = i5.wxyz;
    let b1 = h5.zwxy;
    let d0 = h5.wxyz;

    let i6 = yuv4(I6, C3, A2, G7);
    let i7 = yuv4(I7, C6, A3, G2);
    let h7 = yuv4(H7, F6, B3, D2);

    let f6 = h7.yzwx;

    let c3 = i6.yzwx;
    let g2 = i7.wxyz;
    let b3 = h7.zwxy;
    let d2 = h7.wxyz;

    // The restrictions, one per level and direction.
    let lv1      = and4(ne4(e, f), ne4(e, h));

    let lv2_left = and4(and4(ne4(e, g), ne4(d, g)), or4(eq(e, d), eq(h, g)));
    let lv2_up   = and4(and4(ne4(e, c), ne4(b, c)), or4(eq(e, b), eq(f, c)));

    let lv3_left = and4(and4(ne4(e, g0), ne4(d0, g0)), or4(eq(d, d0), eq(g, g0)));
    let lv3_up   = and4(and4(ne4(e, c1), ne4(b1, c1)), or4(eq(b, b1), eq(c, c1)));

    let lv4_left = and4(and4(ne4(e, g2), ne4(d2, g2)), or4(eq(d0, d2), eq(g0, g2)));
    let lv4_up   = and4(and4(ne4(e, c3), ne4(b3, c3)), or4(eq(b1, b3), eq(c1, c3)));

    let wd1 = weighted_distance(e, c, g, i, h5, f4, h, f);
    let wd2 = weighted_distance(h, d, i5, f, i4, b, e, i);

    let edr0 = and4(le4(wd1, wd2), lv1);

    // The edge is one the two diagonals disagree about, or one the pixel sits
    // beside: (!eq(f,b) && !id(f,c,f,b) || !eq(h,d) && !id(h,g,h,d) || eq(e,g)
    // || eq(e,c)).
    let edr = and4(and4(lt4(wd1, wd2), lv1),
                   or4(or4(and4(not4(eq(f, b)), not4(id(f, c, f, b))),
                           and4(not4(eq(h, d)), not4(id(h, g, h, d)))),
                       or4(eq(e, g), eq(e, c))));

    let edr_left    = and4(and4(le4(2.0 * df(f, g),  df(h, c)),  lv2_left), edr);
    let edr_up      = and4(and4(le4(2.0 * df(h, c),  df(f, g)),  lv2_up),   edr);

    let edr3_left   = and4(and4(le4(4.0 * df(f, g0), df(h, c1)), lv3_left), edr_left);
    let edr3_up     = and4(and4(le4(4.0 * df(h, c1), df(f, g0)), lv3_up),   edr_up);

    let edr4_left   = and4(and4(le4(4.0 * df(f, g2), df(h, c3)), lv4_left), edr3_left);
    let edr4_up     = and4(and4(le4(4.0 * df(h, c3), df(f, g2)), lv4_up),   edr3_up);

    // Eight levels of edge, the strongest first, as the ternary chain upstream
    // spells out once per channel.
    let info =
        choose(and4(edr4_left, not4(edr4_up)), vec4f(8.0),
        choose(and4(edr4_up, not4(edr4_left)), vec4f(7.0),
        choose(and4(edr3_left, not4(edr3_up)), vec4f(6.0),
        choose(and4(edr3_up, not4(edr3_left)), vec4f(5.0),
        choose(and4(edr_left, not4(edr_up)), vec4f(4.0),
        choose(and4(edr_up, not4(edr_left)), vec4f(3.0),
        choose(edr, vec4f(2.0),
        choose(edr0, vec4f(1.0), vec4f(0.0)))))))));

    return remapTo01(info, MAXIMO);
}
