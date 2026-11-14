// xBR lv3 multipass, pass 1 of 2, the shader xBR pixel filter of the WebGPU
// renderer.
//
// The same filter as XBR_Lv3_Pass0.hlsl and xbr-lv3-pass0.glsl: Hyllian's
// xBR-lv3 split into the two passes of its preset - this one works out the
// angle of the edge of every pixel, and packs the flags of it into the four
// channels of a texel at the frame size, for the second pass to interpolate
// with.  The OpenGL 3.3 renderer runs the upstream file as it is, from the
// resource XBR_LV3_PASS0_GLSL; Direct3D 11 runs the HLSL port, and this is the
// WGSL one.
//
//   pass 0  the edge data of every pixel, at the frame size
//   pass 1  the interpolation, at whatever size the target has
//
// The licence of the ported shader:
//
//   Hyllian's xBR level 3 pass0 Shader
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
//   Incorporates some of the ideas from SABR shader. Thanks to Joshua Street.
//
// Upstream computes the seven tap rectangles in its vertex shader and hands
// them on as varyings; here they are computed in the pixel shader, as in the
// other ports, and the taps are read with textureLoad and integer coordinates.
// Upstream works in bvec4, and WGSL has no boolean vector: every one of them is
// a vec4 of ones and zeros here, with and, or and not as the arithmetic that
// gives the same answers.
//
// The FILTRO macro of the upstream file writes into six of the values it is
// handed, which a WGSL function cannot do, so it returns them in a struct of
// their own.

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

const coef         = 2.0;
const cf           = 4.0;
const eq_threshold = vec4f(15.0);
const bin1         = vec4f( 1.0,  2.0,  4.0,   8.0);
const bin2         = vec4f(16.0, 32.0, 64.0, 128.0);
const maximo       = vec4f(255.0);

// The luma row of the weighted colour space the shader compares pixels in,
// which is yuv_weighted[0].
const rgbw = vec3f(14.352, 28.176, 5.472);

// The texel nearest to a corner of the output pixel, by index: the taps are
// offsets of whole texels from the sample, so the index is the floor of the
// coordinate in texels, clamped to the edge the way a clamped sampler is.
fn tap(coord : vec2f, x : f32, y : f32) -> vec3f
{
    let size  = vec2f(constants.sourceSize.xy);
    let uv    = coord + constants.sourceSize.zw * vec2f(x, y);
    let index = clamp(vec2i(floor(uv * size)), vec2i(0), vec2i(size) - vec2i(1));

    return textureLoad(sourceTexture, index, 0).rgb;
}

fn df(A : vec4f, B : vec4f) -> vec4f
{
    return abs(A - B);
}

fn remapTo01(v : vec4f, high : vec4f) -> vec4f
{
    return v / high;
}

// Whether two vector components differ: a 1 or a 0 a component, which is what
// GLSL builds out of a bvec4 with notEqual().
fn diff(A : vec4f, B : vec4f) -> vec4f
{
    return abs(sign(A - B));
}

// Whether two vector components are closer to each other than the threshold.
fn eq(A : vec4f, B : vec4f) -> vec4f
{
    return 1.0 - step(eq_threshold, df(A, B));
}

fn weighted_distance(a : vec4f, b : vec4f, c : vec4f, d : vec4f, e : vec4f,
                     f : vec4f, g : vec4f, h : vec4f) -> vec4f
{
    return df(a, b) + df(a, c) + df(d, e) + df(d, f) + 4.0 * df(g, h);
}

fn bAnd(A : vec4f, B : vec4f) -> vec4f
{
    return A * B;
}

fn bOr(A : vec4f, B : vec4f) -> vec4f
{
    return min(A + B, vec4f(1.0));
}

fn bNot(A : vec4f) -> vec4f
{
    return vec4f(1.0) - A;
}

fn lt(A : vec4f, B : vec4f) -> vec4f { return 1.0 - step(B, A); } // A <  B
fn le(A : vec4f, B : vec4f) -> vec4f { return step(A, B); }       // A <= B
fn ge(A : vec4f, B : vec4f) -> vec4f { return step(B, A); }       // A >= B

// What the FILTRO macro hands back: the two pixels and the three lines it
// writes, whichever branch it took.
struct Filtered
{
    px0  : vec2f,
    px1  : vec2f,
    px3  : vec2f,
    lin0 : vec4f,
    lin1 : vec4f,
    lin3 : vec4f,
};

fn filtro(EDR0 : f32, EDR : f32, LEFT : f32, UP : f32, LEFT3 : f32, UP3 : f32,
          px0 : vec2f, px1 : vec2f, px3 : vec2f,
          lin0 : vec4f, lin1 : vec4f, lin3 : vec4f, PX : f32) -> Filtered
{
    var out : Filtered;
    out.px0  = px0;
    out.px1  = px1;
    out.px3  = px3;
    out.lin0 = lin0;
    out.lin1 = lin1;
    out.lin3 = lin3;

    if (LEFT > 0.5 && UP <= 0.5)
    {
        out.px0 = vec2f(0.0, PX);
        out.px3 = vec2f(PX,  1.0);

        if (LEFT3 > 0.5)
        {
            out.lin0 = vec4f(0.0, 1.0, 0.0, 0.0);
            out.lin3 = vec4f(1.0, 0.0, 0.0, 0.0);
        }
        else
        {
            out.lin0 = vec4f(0.0, 0.0, 1.0, 0.0);
            out.lin3 = vec4f(0.0, 1.0, 1.0, 0.0);
        }
    }
    else if (UP > 0.5 && LEFT <= 0.5)
    {
        out.px0 = vec2f(0.0,     PX);
        out.px1 = vec2f(1.0 - PX, 0.0);

        if (UP3 > 0.5)
        {
            out.lin0 = vec4f(0.0, 1.0, 0.0, 1.0);
            out.lin1 = vec4f(1.0, 0.0, 0.0, 1.0);
        }
        else
        {
            out.lin0 = vec4f(0.0, 0.0, 1.0, 1.0);
            out.lin1 = vec4f(0.0, 1.0, 1.0, 1.0);
        }
    }
    else if (EDR > 0.5)
    {
        out.px0  = vec2f(0.0, PX);
        out.lin0 = vec4f(0.0, 0.0, 0.0, 1.0);
    }
    else if (EDR0 > 0.5)
    {
        out.px0  = vec2f(0.0, PX);
        out.lin0 = vec4f(0.0);
    }

    return out;
}

@fragment
fn fsMain(input : VertexOutput) -> @location(0) vec4f
{
    // Upstream nudges the coordinate by 0.04% so that no tap lands exactly on
    // a texel edge; the taps below inherit it.
    let TEX0 = input.uv * 1.0004;

    //      A1 B1 C1
    //   A0  A  B  C C4
    //   D0  D  E  F F4
    //   G0  G  H  I I4
    //      G5 H5 I5

    let A1 = tap(TEX0, -1.0, -2.0);
    let B1 = tap(TEX0,  0.0, -2.0);
    let C1 = tap(TEX0,  1.0, -2.0);
    let A  = tap(TEX0, -1.0, -1.0);
    let B  = tap(TEX0,  0.0, -1.0);
    let C  = tap(TEX0,  1.0, -1.0);
    let D  = tap(TEX0, -1.0,  0.0);
    let E  = tap(TEX0,  0.0,  0.0);
    let F  = tap(TEX0,  1.0,  0.0);
    let G  = tap(TEX0, -1.0,  1.0);
    let H  = tap(TEX0,  0.0,  1.0);
    let I  = tap(TEX0,  1.0,  1.0);
    let G5 = tap(TEX0, -1.0,  2.0);
    let H5 = tap(TEX0,  0.0,  2.0);
    let I5 = tap(TEX0,  1.0,  2.0);
    let A0 = tap(TEX0, -2.0, -1.0);
    let D0 = tap(TEX0, -2.0,  0.0);
    let G0 = tap(TEX0, -2.0,  1.0);
    let C4 = tap(TEX0,  2.0, -1.0);
    let F4 = tap(TEX0,  2.0,  0.0);
    let I4 = tap(TEX0,  2.0,  1.0);

    let b = vec4f(dot(B, rgbw), dot(D, rgbw), dot(H, rgbw), dot(F, rgbw));
    let c = vec4f(dot(C, rgbw), dot(A, rgbw), dot(G, rgbw), dot(I, rgbw));
    let e = vec4f(dot(E, rgbw));
    let d = b.yzwx;
    let f = b.wxyz;
    let g = c.zwxy;
    let h = b.zwxy;
    let i = c.wxyz;

    let i4 = vec4f(dot(I4, rgbw), dot(C1, rgbw), dot(A0, rgbw), dot(G5, rgbw));
    let i5 = vec4f(dot(I5, rgbw), dot(C4, rgbw), dot(A1, rgbw), dot(G0, rgbw));
    let h5 = vec4f(dot(H5, rgbw), dot(F4, rgbw), dot(B1, rgbw), dot(D0, rgbw));
    let f4 = h5.yzwx;

    let c1 = i4.yzwx;
    let g0 = i5.wxyz;
    let b1 = h5.zwxy;
    let d0 = h5.wxyz;

    let interp_restriction_lv0 = bAnd(diff(e, f), diff(e, h));
    let comp1 = bAnd(bNot(eq(h, h5)), bNot(eq(h, i5)));
    let comp2 = bAnd(bNot(eq(h, d)),  bNot(eq(h, g)));
    let comp3 = bAnd(bNot(eq(f, f4)), bNot(eq(f, i4)));
    let comp4 = bAnd(bNot(eq(f, b)),  bNot(eq(f, c)));
    let comp5 = bAnd(eq(e, i), bOr(comp3, comp1));

    let interp_restriction_lv1 = bOr(comp4, bOr(comp2, bOr(comp5, bOr(eq(e, g), eq(e, c)))));

    let interp_restriction_lv2_left = bAnd(diff(e, g), diff(d, g));
    let interp_restriction_lv2_up   = bAnd(diff(e, c), diff(b, c));
    let interp_restriction_lv3_left = bAnd(diff(e, g0), diff(d0, g0));
    let interp_restriction_lv3_up   = bAnd(diff(e, c1), diff(b1, c1));

    let edr0 = bAnd(lt(weighted_distance(e, c, g, i, h5, f4, h, f),
                       weighted_distance(h, d, i5, f, i4, b, e, i)),
                    interp_restriction_lv0);

    let edr       = bAnd(edr0, interp_restriction_lv1);
    let edr_left  = bAnd(le(coef * df(f, g), df(h, c)), bAnd(interp_restriction_lv2_left, edr));
    let edr_up    = bAnd(ge(df(f, g), coef * df(h, c)), bAnd(interp_restriction_lv2_up,   edr));
    let edr3_left = bAnd(le(cf * df(f, g0), df(h, c1)), bAnd(interp_restriction_lv3_left, edr_left));
    let edr3_up   = bAnd(ge(df(f, g0), cf * df(h, c1)), bAnd(interp_restriction_lv3_up,   edr_up));

    let px = le(df(e, f), df(e, h));

    var px0 = vec2f(0.0);
    var px1 = vec2f(0.0);
    var px2 = vec2f(0.0);
    var px3 = vec2f(0.0);
    var lin0 = vec4f(1.0);
    var lin1 = vec4f(1.0);
    var lin2 = vec4f(1.0);
    var lin3 = vec4f(1.0);

    var r : Filtered;

    r = filtro(edr0.x, edr.x, edr_left.x, edr_up.x, edr3_left.x, edr3_up.x,
               px0, px1, px3, lin0, lin1, lin3, px.x);
    px0 = r.px0; px1 = r.px1; px3 = r.px3;
    lin0 = r.lin0; lin1 = r.lin1; lin3 = r.lin3;

    r = filtro(edr0.y, edr.y, edr_left.y, edr_up.y, edr3_left.y, edr3_up.y,
               px1, px2, px0, lin1, lin2, lin0, px.y);
    px1 = r.px0; px2 = r.px1; px0 = r.px3;
    lin1 = r.lin0; lin2 = r.lin1; lin0 = r.lin3;

    r = filtro(edr0.z, edr.z, edr_left.z, edr_up.z, edr3_left.z, edr3_up.z,
               px2, px3, px1, lin2, lin3, lin1, px.z);
    px2 = r.px0; px3 = r.px1; px1 = r.px3;
    lin2 = r.lin0; lin3 = r.lin1; lin1 = r.lin3;

    r = filtro(edr0.w, edr.w, edr_left.w, edr_up.w, edr3_left.w, edr3_up.w,
               px3, px0, px2, lin3, lin0, lin2, px.w);
    px3 = r.px0; px0 = r.px1; px2 = r.px3;
    lin3 = r.lin0; lin0 = r.lin1; lin2 = r.lin3;

    // mul(bin1, mat4(edr3_left, edr3_up, px0.x..px3.x, px0.y..px3.y)) is
    // mat4 times bin1 - upstream defines mul as (b * a) - so each channel is
    // the dot of a row of that matrix with the bin, one channel per call of
    // the filter above.  The same with bin2 over the lines is the dot of each
    // line with bin2.
    var info = vec4f(dot(vec4f(edr3_left.x, edr3_up.x, px0.x, px0.y), bin1),
                     dot(vec4f(edr3_left.y, edr3_up.y, px1.x, px1.y), bin1),
                     dot(vec4f(edr3_left.z, edr3_up.z, px2.x, px2.y), bin1),
                     dot(vec4f(edr3_left.w, edr3_up.w, px3.x, px3.y), bin1));

    info += vec4f(dot(lin0, bin2), dot(lin1, bin2), dot(lin2, bin2), dot(lin3, bin2));

    return vec4f(remapTo01(info, maximo));
}
