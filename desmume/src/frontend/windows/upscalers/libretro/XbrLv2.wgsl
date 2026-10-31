// xBR lv2, the shader xBR pixel filter of the WebGPU renderer.
//
// The same filter as XBR_Lv2.hlsl and xbr-lv2.glsl: Hyllian's xBR-lv2, corner
// detection of the third kind with the smooth tips, at whatever size the
// window has.  The OpenGL 3.3 renderer runs the upstream file as it is, from
// the resource XBR_LV2_GLSL; Direct3D 11 runs the HLSL port, and this is the
// WGSL one.
//
// Upstream computes the seven tap rectangles in its vertex shader and hands
// them on as varyings.  Here they are computed in the pixel shader instead,
// which is what the other two ports do as well: the renderer has one quad
// vertex shader for every upscaler, and eight varyings are eight interpolators
// the pixel shader does not need to spend.
//
// The taps are read with textureLoad and integer coordinates rather than
// through a sampler.  A tap of this filter is the texel nearest to a corner of
// the output pixel, which is what a point sampler gives; the integer read is
// the same texel, exactly, without the sampler state.
//
// The licence of the ported shader:
//
//   Hyllian's xBR-lv2 Shader
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
const XBR_EQ_THRESHOLD    = 15.0;
const XBR_LV2_COEFFICIENT = 2.0;

// Upstream picks the corner detection with one of four defines; "xbr-lv2.glsl"
// ships with CORNER_C, which also turns the smooth tips on.
const XBR_SCALE = 3.0;

// The luma row of the colour space the shader compares pixels in, which is
// XBR_Y_WEIGHT * (0.2126, 0.7152, 0.0722) with the weight upstream ships.
const rgbw = vec3f(14.352, 28.176, 5.472);

const delta   = vec4f(1.0 / XBR_SCALE, 1.0 / XBR_SCALE, 1.0 / XBR_SCALE, 1.0 / XBR_SCALE);
const delta_l = vec4f(0.5 / XBR_SCALE, 1.0 / XBR_SCALE, 0.5 / XBR_SCALE, 1.0 / XBR_SCALE);
const delta_u = vec4f(1.0 / XBR_SCALE, 0.5 / XBR_SCALE, 1.0 / XBR_SCALE, 0.5 / XBR_SCALE);

const Ao = vec4f( 1.0, -1.0, -1.0, 1.0 );
const Bo = vec4f( 1.0,  1.0, -1.0,-1.0 );
const Co = vec4f( 1.5,  0.5, -0.5, 0.5 );
const Ax = vec4f( 1.0, -1.0, -1.0, 1.0 );
const Bx = vec4f( 0.5,  2.0, -0.5,-2.0 );
const Cx = vec4f( 1.0,  1.0, -0.5, 0.0 );
const Ay = vec4f( 1.0, -1.0, -1.0, 1.0 );
const By = vec4f( 2.0,  0.5, -2.0,-0.5 );
const Cy = vec4f( 2.0,  0.0, -1.0, 0.5 );
const Ci = vec4f( 0.25, 0.25, 0.25, 0.25 );

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

fn c_df(c1 : vec3f, c2 : vec3f) -> f32
{
    let df = abs(c1 - c2);
    return df.r + df.g + df.b;
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

    let b = vec4f(dot(B, rgbw), dot(D, rgbw), dot(H, rgbw), dot(F, rgbw));
    let c = vec4f(dot(C, rgbw), dot(A, rgbw), dot(G, rgbw), dot(I, rgbw));
    let d = b.yzwx;
    let e = vec4f(dot(E, rgbw));
    let f = b.wxyz;
    let g = c.zwxy;
    let h = b.zwxy;
    let i = c.wxyz;

    // small_details is 0 in the file this is ported from, which is the branch
    // below; the other one reads a mat4x3 and is left out.
    let i4 = vec4f(dot(I4, rgbw), dot(C1, rgbw), dot(A0, rgbw), dot(G5, rgbw));
    let i5 = vec4f(dot(I5, rgbw), dot(C4, rgbw), dot(A1, rgbw), dot(G0, rgbw));
    let h5 = vec4f(dot(H5, rgbw), dot(F4, rgbw), dot(B1, rgbw), dot(D0, rgbw));
    let f4 = h5.yzwx;

    // These inequations define the line below which interpolation occurs.
    let fx   = Ao * fp.y + Bo * fp.x;
    let fx_l = Ax * fp.y + Bx * fp.x;
    let fx_u = Ay * fp.y + By * fp.x;

    let irlv0 = diff(e, f) * diff(e, h);

    // CORNER_C, the corner detection the upstream file ships with.
    let irlv1 = irlv0 * (neq(f, b) * neq(f, c) + neq(h, d) * neq(h, g) +
                         eq(e, i) * (neq(f, f4) * neq(f, i4) + neq(h, h5) * neq(h, i5)) +
                         eq(e, g) + eq(e, c));

    let irlv2l = diff(e, g) * diff(d, g);
    let irlv2u = diff(e, c) * diff(b, c);

    var fx45i = saturate((fx   + delta   - Co - Ci) / (2.0 * delta  ));
    var fx45  = saturate((fx   + delta   - Co     ) / (2.0 * delta  ));
    var fx30  = saturate((fx_l + delta_l - Cx     ) / (2.0 * delta_l));
    var fx60  = saturate((fx_u + delta_u - Cy     ) / (2.0 * delta_u));

    let wd1 = wd(e, c, g, i, h5, f4, h, f);
    let wd2 = wd(h, d, i5, f, i4, b, e, i);

    let edri  = step(wd1, wd2) * irlv0;
    let edr   = step(wd1 + vec4f(0.1), wd2) * step(vec4f(0.5), irlv1);
    let edr_l = step(XBR_LV2_COEFFICIENT * df(f, g), df(h, c)) * irlv2l * edr;
    let edr_u = step(XBR_LV2_COEFFICIENT * df(h, c), df(f, g)) * irlv2u * edr;

    fx45  = edr   * fx45;
    fx30  = edr_l * fx30;
    fx60  = edr_u * fx60;
    fx45i = edri  * fx45i;

    let px = step(df(e, f), df(e, h));

    // SMOOTH_TIPS, which CORNER_C turns on.
    let maximos = max(max(fx30, fx60), max(fx45, fx45i));

    var res1 = E;
    res1 = mix(res1, mix(H, F, px.x), maximos.x);
    res1 = mix(res1, mix(B, D, px.z), maximos.z);

    var res2 = E;
    res2 = mix(res2, mix(F, B, px.y), maximos.y);
    res2 = mix(res2, mix(D, H, px.w), maximos.w);

    let res = mix(res1, res2, step(c_df(E, res1), c_df(E, res2)));

    return vec4f(res, 1.0);
}
