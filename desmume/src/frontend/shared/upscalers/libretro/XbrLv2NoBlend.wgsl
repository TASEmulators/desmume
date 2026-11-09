// xBR lv2 no-blend, the shader xBR pixel filter of the WebGPU renderer.
//
// The same filter as XBR_Lv2NoBlend.hlsl and xbr-lv2-noblend.glsl: Hyllian's
// xBR-lv2 with the first corner rule, no smooth tips, and the tips chosen by
// comparison rather than blended in.  The OpenGL 3.3 renderer runs the
// upstream file as it is, from the resource XBR_LV2_NOBLEND_GLSL; Direct3D 11
// runs the HLSL port, and this is the WGSL one.
//
// It is a shader of its own rather than a switch on the one above: the luma
// weights are the plain Y row here, the thresholds are tighter, the
// inequations are compared rather than clamped, and the last step picks
// between two pixels instead of mixing them.
//
// Upstream computes the seven tap rectangles in its vertex shader and hands
// them on as varyings.  Here they are computed in the pixel shader, which is
// what the other ports do: the renderer has one quad vertex shader for every
// upscaler, and eight varyings are eight interpolators the pixel shader does
// not need to spend.  The taps go through textureLoad with integer
// coordinates, which is the texel a point sampler would have given.
//
// The licence of the ported shader:
//
//   Hyllian's xBR-lv2-noblend Shader
//
//   Copyright (C) 2011-2016 Hyllian - sergiogdb@gmail.com
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
const XBR_EQ_THRESHOLD     = 0.6;
const XBR_LV2_COEFFICIENT  = 2.0;

// The luma row of the colour space the shader compares pixels in.
const Y = vec3f(0.2126, 0.7152, 0.0722);

const Ao = vec4f( 1.0, -1.0, -1.0, 1.0 );
const Bo = vec4f( 1.0,  1.0, -1.0,-1.0 );
const Co = vec4f( 1.5,  0.5, -0.5, 0.5 );
const Ax = vec4f( 1.0, -1.0, -1.0, 1.0 );
const Bx = vec4f( 0.5,  2.0, -0.5,-2.0 );
const Cx = vec4f( 1.0,  1.0, -0.5, 0.0 );
const Ay = vec4f( 1.0, -1.0, -1.0, 1.0 );
const By = vec4f( 2.0,  0.5, -2.0,-0.5 );
const Cy = vec4f( 2.0,  0.0, -1.0, 0.5 );

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

// Difference between vector components.
fn df(A : vec4f, B : vec4f) -> vec4f
{
    return abs(A - B);
}

// Compare two vectors and return their components are different: a 1 or a 0 a
// component, which is what GLSL builds out of a bvec4.
fn diff(A : vec4f, B : vec4f) -> vec4f
{
    return select(vec4f(0.0), vec4f(1.0), A != B);
}

// Determine if two vector components are equal based on a threshold.
fn eq(A : vec4f, B : vec4f) -> vec4f
{
    return step(df(A, B), vec4f(XBR_EQ_THRESHOLD));
}

// Determine if two vector components are NOT equal based on a threshold.
fn neq(A : vec4f, B : vec4f) -> vec4f
{
    return vec4f(1.0) - eq(A, B);
}

// Weighted distance.
fn wd(a : vec4f, b : vec4f, c : vec4f, d : vec4f, e : vec4f, f : vec4f,
      g : vec4f, h : vec4f) -> vec4f
{
    return df(a, b) + df(a, c) + df(d, e) + df(d, f) + 4.0 * df(g, h);
}

@fragment
fn fsMain(input : VertexOutput) -> @location(0) vec4f
{
    let coord = input.uv;

    // Upstream multiplies by the size rather than dividing by the texel it
    // derived, which is the same number and one rounding less.
    let fp = fract(coord * constants.sourceSize.xy);

    let A1 = tap(coord, -1.0, -2.0);
    let B1 = tap(coord,  0.0, -2.0);
    let C1 = tap(coord,  1.0, -2.0);
    let A  = tap(coord, -1.0, -1.0);
    let B  = tap(coord,  0.0, -1.0);
    let C  = tap(coord,  1.0, -1.0);
    let D  = tap(coord, -1.0,  0.0);
    let E  = tap(coord,  0.0,  0.0);
    let F  = tap(coord,  1.0,  0.0);
    let G  = tap(coord, -1.0,  1.0);
    let H  = tap(coord,  0.0,  1.0);
    let I  = tap(coord,  1.0,  1.0);
    let G5 = tap(coord, -1.0,  2.0);
    let H5 = tap(coord,  0.0,  2.0);
    let I5 = tap(coord,  1.0,  2.0);
    let A0 = tap(coord, -2.0, -1.0);
    let D0 = tap(coord, -2.0,  0.0);
    let G0 = tap(coord, -2.0,  1.0);
    let C4 = tap(coord,  2.0, -1.0);
    let F4 = tap(coord,  2.0,  0.0);
    let I4 = tap(coord,  2.0,  1.0);

    // Upstream builds these out of mat4x3 products with the luma row, which is
    // the four dot products spelled out.
    let b = vec4f(dot(B, Y), dot(D, Y), dot(H, Y), dot(F, Y));
    let c = vec4f(dot(C, Y), dot(A, Y), dot(G, Y), dot(I, Y));
    let e = vec4f(dot(E, Y));
    let d = b.yzwx;
    let f = b.wxyz;
    let g = c.zwxy;
    let h = b.zwxy;
    let i = c.wxyz;

    let i4 = vec4f(dot(I4, Y), dot(C1, Y), dot(A0, Y), dot(G5, Y));
    let i5 = vec4f(dot(I5, Y), dot(C4, Y), dot(A1, Y), dot(G0, Y));
    let h5 = vec4f(dot(H5, Y), dot(F4, Y), dot(B1, Y), dot(D0, Y));
    let f4 = h5.yzwx;

    // These inequations define the line below which interpolation occurs.
    // GLSL turns the comparisons into a vec4 of ones and zeros, which is what
    // the arithmetic below reads them as.
    let fx   = select(vec4f(0.0), vec4f(1.0), Ao * fp.y + Bo * fp.x > Co);
    let fx_l = select(vec4f(0.0), vec4f(1.0), Ax * fp.y + Bx * fp.x > Cx);
    let fx_u = select(vec4f(0.0), vec4f(1.0), Ay * fp.y + By * fp.x > Cy);

    // CORNER_A, the corner detection the upstream file ships with.
    let irlv1 = diff(e, f) * diff(e, h);

    let irlv2l = diff(e, g) * diff(d, g);
    let irlv2u = diff(e, c) * diff(b, c);

    let wd1 = wd(e, c, g, i, h5, f4, h, f);
    let wd2 = wd(h, d, i5, f, i4, b, e, i);

    let edri  = step(wd1, wd2) * irlv1;
    let edr   = step(wd1 + vec4f(0.1), wd2) * step(vec4f(0.5), irlv1);
    let edr_l = step(XBR_LV2_COEFFICIENT * df(f, g), df(h, c)) * irlv2l * edr;
    let edr_u = step(XBR_LV2_COEFFICIENT * df(h, c), df(f, g)) * irlv2u * edr;

    // A bvec4 in GLSL: whether the weight came out non-zero.
    let nc = (edr * (fx + edr_l * fx_l) + edr_u * fx_u) != vec4f(0.0);

    let px = df(e, f) <= df(e, h);

    // The two tips, picked rather than blended: the chain of ternaries upstream
    // writes, read from the inside out.
    let e3 = select(B, D, px.z);
    let e2 = select(E, e3, nc.z);
    let e1 = select(F, B, px.y);
    let res1 = select(select(e2, e1, nc.y), select(H, F, px.x), nc.x);

    let d3 = select(B, D, px.z);
    let d2 = select(E, d3, nc.z);
    let d1 = select(F, B, px.y);
    let res2 = select(select(d2, d1, nc.y), select(D, H, px.w), nc.w);

    // mat2x3(res1, res2) * Y, which is the two luma values of the tips.
    let df12 = abs(vec2f(dot(res1, Y), dot(res2, Y)) - e.xy);

    let res = mix(res1, res2, step(df12.x, df12.y));

    return vec4f(res, 1.0);
}
