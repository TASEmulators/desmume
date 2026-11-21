// xBR lv3, the shader xBR pixel filter of the WebGPU renderer.
//
// The same filter as XBR_Lv3.hlsl and xbr-lv3.glsl: Hyllian's xBR-lv3, which
// interpolates the edges of three slopes rather than two and so also looks at
// the pixels two apart.  The OpenGL 3.3 renderer runs the upstream file as it
// is, from the resource XBR_LV3_GLSL; Direct3D 11 runs the HLSL port, and this
// is the WGSL one.
//
// Upstream computes the seven tap rectangles in its vertex shader and hands
// them on as varyings.  Here they are computed in the pixel shader, which is
// what the other ports do, and the taps are read with textureLoad and integer
// coordinates, which is the texel a point sampler would have given.
//
// Upstream works in bvec4, and WGSL has no boolean vector: every one of them
// is a vec4 of ones and zeros here, as it is in the HLSL port, with and, or
// and not as the arithmetic that gives the same answers.
//
// The licence of the ported shader:
//
//   Hyllian's xBR-lv3 Shader
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
const XBR_EQ_THRESHOLD    = 10.0;
const XBR_EQ_THRESHOLD2   = 2.0;
const XBR_LV2_COEFFICIENT = 2.0;
const CORNER_TYPE         = 3.0;

// The luma row of the colour space the shader compares pixels in, which is
// XBR_Y_WEIGHT * yuv[0] with the weight upstream ships.
const rgbw  = vec3f(14.352, 28.176, 5.472);
const delta = vec4f(0.4, 0.4, 0.4, 0.4);

const Ao = vec4f( 1.0, -1.0, -1.0, 1.0 );
const Bo = vec4f( 1.0,  1.0, -1.0,-1.0 );
const Co = vec4f( 1.5,  0.5, -0.5, 0.5 );
const Ax = vec4f( 1.0, -1.0, -1.0, 1.0 );
const Bx = vec4f( 0.5,  2.0, -0.5,-2.0 );
const Cx = vec4f( 1.0,  1.0, -0.5, 0.0 );
const Ay = vec4f( 1.0, -1.0, -1.0, 1.0 );
const By = vec4f( 2.0,  0.5, -2.0,-0.5 );
const Cy = vec4f( 2.0,  0.0, -1.0, 0.5 );

const Az = vec4f( 6.0, -2.0, -6.0, 2.0 );
const Bz = vec4f( 2.0,  6.0, -2.0,-6.0 );
const Cz = vec4f( 5.0,  3.0, -3.0,-1.0 );
const Aw = vec4f( 2.0, -6.0, -2.0, 6.0 );
const Bw = vec4f( 6.0,  2.0, -6.0,-2.0 );
const Cw = vec4f( 5.0, -1.0, -3.0, 3.0 );

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

// Whether two vector components differ: a 1 or a 0 a component, which is what
// GLSL builds out of a bvec4.
fn diff(A : vec4f, B : vec4f) -> vec4f
{
    return abs(sign(A - B));
}

// Whether two vector components are equal, within a threshold.  Upstream
// compares with lessThan(), so a difference of exactly the threshold is not
// equal; step() would say it is.
fn eq(A : vec4f, B : vec4f) -> vec4f
{
    return 1.0 - step(vec4f(XBR_EQ_THRESHOLD), df(A, B));
}

// The same, against the second, much tighter threshold.
fn eq2(A : vec4f, B : vec4f) -> vec4f
{
    return 1.0 - step(vec4f(XBR_EQ_THRESHOLD2), df(A, B));
}

// The boolean algebra the shader does with bvec4: with 0 and 1 as the two
// values, AND is a product, OR is the same clamped, and NOT is what is left.
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

// Comparisons, as GLSL writes them: 1 where the comparison holds.
fn lt(A : vec4f, B : vec4f) -> vec4f { return 1.0 - step(B, A); } // A <  B
fn le(A : vec4f, B : vec4f) -> vec4f { return step(A, B); }       // A <= B
fn ge(A : vec4f, B : vec4f) -> vec4f { return step(B, A); }       // A >= B

// A float the shader reads as a boolean: GLSL converts a float to a bool by
// asking whether it is zero, this asks the same of anything from 0 to 1.
fn asBool(value : vec4f) -> vec4f
{
    return ceil(saturate(value));
}

fn weighted_distance(a : vec4f, b : vec4f, c : vec4f, d : vec4f, e : vec4f,
                     f : vec4f, g : vec4f, h : vec4f) -> vec4f
{
    return df(a, b) + df(a, c) + df(d, e) + df(d, f) + 4.0 * df(g, h);
}

fn c_df(c1 : vec3f, c2 : vec3f) -> f32
{
    let diff = abs(c1 - c2);
    return diff.r + diff.g + diff.b;
}

@fragment
fn fsMain(input : VertexOutput) -> @location(0) vec4f
{
    let coord = input.uv;
    let fp    = fract(coord * constants.sourceSize.xy);

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

    let i4 = vec4f(dot(I4, rgbw), dot(C1, rgbw), dot(A0, rgbw), dot(G5, rgbw));
    let i5 = vec4f(dot(I5, rgbw), dot(C4, rgbw), dot(A1, rgbw), dot(G0, rgbw));
    let h5 = vec4f(dot(H5, rgbw), dot(F4, rgbw), dot(B1, rgbw), dot(D0, rgbw));
    let f4 = h5.yzwx;

    let c1 = i4.yzwx;
    let g0 = i5.wxyz;
    let b1 = h5.zwxy;
    let d0 = h5.wxyz;

    // These inequations define the line below which interpolation occurs.
    let fx       = Ao * fp.y + Bo * fp.x;
    let fx_left  = Ax * fp.y + Bx * fp.x;
    let fx_up    = Ay * fp.y + By * fp.x;
    let fx3_left = Az * fp.y + Bz * fp.x;
    let fx3_up   = Aw * fp.y + Bw * fp.x;

    // Which corners may be interpolated at all.  Upstream picks one of four
    // rules; "xbr-lv3.glsl" ships with the third, which is the branch below.
    var interp_restriction_lv1 : vec4f;

    if (CORNER_TYPE == 1.0)
    {
        interp_restriction_lv1 = bAnd(diff(e, f), diff(e, h));
    }
    else if (CORNER_TYPE == 2.0)
    {
        interp_restriction_lv1 = bAnd(bAnd(diff(e, f), diff(e, h)),
                                      bOr(bOr(bAnd(bAnd(bOr(bAnd(bNot(eq(f, b)), bNot(eq(h, d))),
                                                               eq(e, i)),
                                                         bNot(eq(f, i4))),
                                                    bNot(eq(h, i5))),
                                               eq(e, g)),
                                          eq(e, c)));
    }
    else
    {
        interp_restriction_lv1 = bAnd(bAnd(diff(e, f), diff(e, h)),
                                      bOr(bOr(bAnd(bNot(eq(f, b)), bNot(eq(f, c))),
                                              bAnd(bNot(eq(h, d)), bNot(eq(h, g)))),
                                          bOr(bAnd(eq(e, i),
                                                   bOr(bAnd(bNot(eq(f, f4)), bNot(eq(f, i4))),
                                                       bAnd(bNot(eq(h, h5)), bNot(eq(h, i5))))),
                                              bOr(eq(e, g), eq(e, c)))));
    }

    let interp_restriction_lv2_left = bAnd(diff(e, g), diff(d, g));
    let interp_restriction_lv2_up   = bAnd(diff(e, c), diff(b, c));
    let interp_restriction_lv3_left = bAnd(eq2(g, g0), bNot(eq2(d0, g0)));
    let interp_restriction_lv3_up   = bAnd(eq2(c, c1), bNot(eq2(b1, c1)));

    let fx45 = smoothstep(Co - delta, Co + delta, fx);
    let fx30 = smoothstep(Cx - delta, Cx + delta, fx_left);
    let fx60 = smoothstep(Cy - delta, Cy + delta, fx_up);
    let fx15 = smoothstep(Cz - delta, Cz + delta, fx3_left);
    let fx75 = smoothstep(Cw - delta, Cw + delta, fx3_up);

    let edr       = bAnd(lt(weighted_distance(e, c, g, i, h5, f4, h, f),
                            weighted_distance(h, d, i5, f, i4, b, e, i)), interp_restriction_lv1);
    let edr_left  = bAnd(le(XBR_LV2_COEFFICIENT * df(f, g), df(h, c)), interp_restriction_lv2_left);
    let edr_up    = bAnd(ge(df(f, g), XBR_LV2_COEFFICIENT * df(h, c)), interp_restriction_lv2_up);
    let edr3_left = interp_restriction_lv3_left;
    let edr3_up   = interp_restriction_lv3_up;

    let nc45 = bAnd(edr, asBool(fx45));
    let nc30 = bAnd(edr, bAnd(edr_left, asBool(fx30)));
    let nc60 = bAnd(edr, bAnd(edr_up, asBool(fx60)));
    let nc15 = bAnd(bAnd(edr, edr_left), bAnd(edr3_left, asBool(fx15)));
    let nc75 = bAnd(bAnd(edr, edr_up), bAnd(edr3_up, asBool(fx75)));

    let px = le(df(e, f), df(e, h));

    let nc = bOr(bOr(bOr(bOr(nc75, nc15), nc30), nc60), nc45);

    let final45 = nc45 * fx45;
    let final30 = nc30 * fx30;
    let final60 = nc60 * fx60;
    let final15 = nc15 * fx15;
    let final75 = nc75 * fx75;

    let maximo = max(max(max(final15, final75), max(final30, final60)), final45);

    var pix1 = E;
    var pix2 = E;
    var blend1 = 0.0;
    var blend2 = 0.0;

    if (nc.x > 0.5)      { pix1 = select(H, F, px.x > 0.5); blend1 = maximo.x; }
    else if (nc.y > 0.5) { pix1 = select(F, B, px.y > 0.5); blend1 = maximo.y; }
    else if (nc.z > 0.5) { pix1 = select(B, D, px.z > 0.5); blend1 = maximo.z; }
    else if (nc.w > 0.5) { pix1 = select(D, H, px.w > 0.5); blend1 = maximo.w; }

    if (nc.w > 0.5)      { pix2 = select(D, H, px.w > 0.5); blend2 = maximo.w; }
    else if (nc.z > 0.5) { pix2 = select(B, D, px.z > 0.5); blend2 = maximo.z; }
    else if (nc.y > 0.5) { pix2 = select(F, B, px.y > 0.5); blend2 = maximo.y; }
    else if (nc.x > 0.5) { pix2 = select(H, F, px.x > 0.5); blend2 = maximo.x; }

    let res1 = mix(E, pix1, blend1);
    let res2 = mix(E, pix2, blend2);
    let res  = mix(res1, res2, step(c_df(E, res1), c_df(E, res2)));

    return vec4f(res, 1.0);
}
