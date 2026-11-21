// xBR-mlv4, pass 3 of 4 of the pixel filter of the WebGPU renderer.
//
// The same shader as XBR_Mlv4_Pass3.hlsl and xbr-mlv4-pass3.glsl, all three
// ported from the libretro GLSL shader "xbr-mlv4-pass3.glsl" (libretro/glsl-
// shaders, xbr/shaders).  It is the pass that reads two textures: the edge
// data pass 2 corrected, at binding 0, and the frame at binding 3.
//
// This one runs at twice the frame size, which is what its preset asks for
// and what makes its subpixel arithmetic mean anything: the quarter of a frame
// texel the sample falls in is frac(vTexCoord * frame size) - 0.5, and at 1:1
// that is zero for every pixel.  What it writes is one straight line per
// eight by eight block of its target - the direction of the edge, the order of
// it in Bx, Ay and C, and the address of the quarter in w.
//
// The licence of the ported shader:
//
//   Hyllian's xBR MultiLevel4 Shader - Pass3
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

const MAXIMO = 255.0;
const LOW    = -64.0;
const HIGH     =  64.0;

// mul(color, yuv_weighted[0]): 48 times the Y axis, as in pass 2.
const LUMA = vec3f(48.0 * 0.299, 48.0 * 0.587, 48.0 * 0.114);

// The line each level of edge follows: the weight of the x axis, the weight of
// the y axis, and where on the line the interpolation starts.  The last four
// entries are the four that continue an edge of level 3 and 4.
const lines = array<vec3f, 13>(
    vec3f( 4.0,  4.0, 4.0),  //  0  NL
    vec3f( 4.0,  4.0, 3.0),  //  1  LV0
    vec3f( 4.0,  4.0, 2.0),  //  2  LV1
    vec3f( 8.0,  4.0, 2.0),  //  3  LV2u
    vec3f( 4.0,  8.0, 2.0),  //  4  LV2l
    vec3f(12.0,  4.0, 2.0),  //  5  LV3u
    vec3f( 4.0, 12.0, 2.0),  //  6  LV3l
    vec3f(16.0,  4.0, 2.0),  //  7  LV4u
    vec3f( 4.0, 16.0, 2.0),  //  8  LV4l
    vec3f(12.0,  4.0, 6.0),  //  9  LV3u
    vec3f( 4.0, 12.0, 6.0),  // 10  LV3l
    vec3f(16.0,  4.0, 6.0),  // 11  LV4u
    vec3f( 4.0, 16.0, 6.0)); // 12  LV4l

fn saturate(x : f32) -> f32 { return clamp(x, 0.0, 1.0); }

fn df(A : f32, B : f32) -> f32 { return abs(A - B); }

// float(bool), the way upstream makes a 0 or a 1 out of a comparison.
fn flag(cond : bool) -> f32 { return select(0.0, 1.0, cond); }

fn roundHalf(x : f32) -> f32 { return floor(x + 0.5); }

fn remapFrom01(v : f32, high : f32) -> f32 { return roundHalf(high * v); }

fn remapTo01(v : vec4f, low : f32, high : f32) -> vec4f
{
    return clamp((v - vec4f(low)) / vec4f(high - low), vec4f(0.0), vec4f(1.0));
}

fn luma(c : vec3f) -> f32
{
    return dot(c, LUMA);
}

// The frame, at a coordinate that is the sample plus a whole number of texels
// or none: what the point sampler of the renderer would read.
fn frameTexel(coord : vec2f) -> vec3f
{
    let size  = vec2i(constants.secondSize.xy);
    let index = clamp(vec2i(floor(coord * constants.secondSize.xy)), vec2i(0), size - vec2i(1));

    return textureLoad(secondTexture, index, 0).rgb;
}

// The edge data pass 2 wrote, at the same coordinate.
fn flagTexel(coord : vec2f) -> vec4f
{
    let size  = vec2i(constants.sourceSize.xy);
    let index = clamp(vec2i(floor(coord * constants.sourceSize.xy)), vec2i(0), size - vec2i(1));

    return textureLoad(sourceTexture, index, 0);
}

@fragment
fn fsMain(input : VertexOutput) -> @location(0) vec4f
{
    let uv = input.uv;

    // The quarter of the frame texel the sample sits in, and the direction it
    // leans to.  A sample on a texel centre has no direction, and then both
    // shifts below are zero.
    let pos = fract(uv * constants.secondSize.xy) - vec2f(0.5, 0.5);
    let dir = sign(pos);

    // t1 = (dx, 0, 0, dy) in texels of the frame: the x arm and the y arm of
    // the cross the edge is measured on.
    let prod = dir.x * dir.y;

    var shift1 = vec2f(0.0);
    var shift2 = vec2f(0.0);

    if (prod > 0.0)
    {
        shift1 = vec2f(constants.secondSize.z, 0.0);
        shift2 = vec2f(0.0, constants.secondSize.w);
    }
    else if (prod < 0.0)
    {
        shift1 = vec2f(0.0, constants.secondSize.w);
        shift2 = vec2f(constants.secondSize.z, 0.0);
    }

    let g1 = dir * shift1;
    let g2 = dir * shift2;

    let E  = luma(frameTexel(uv));
    let F  = luma(frameTexel(uv + g1));
    let H  = luma(frameTexel(uv + g2));
    let I  = luma(frameTexel(uv + g1 + g2));
    let F4 = luma(frameTexel(uv + 2.0 * g1));
    let H5 = luma(frameTexel(uv + 2.0 * g2));

    // The four corners, as the vector the direction is tested against:
    // mat2x4(1, 1, -1, -1, 1, -1, -1, 1) * dir, one dot per row.
    var icomp = vec4f(dot(dir, vec2f( 1.0,  1.0)),
                      dot(dir, vec2f( 1.0, -1.0)),
                      dot(dir, vec2f(-1.0, -1.0)),
                      dot(dir, vec2f(-1.0,  1.0)));
    icomp = floor(clamp(icomp, vec4f(0.0), vec4f(1.0)) + 0.5);

    // The level of this pixel and of the two neighbours the edge would run
    // into, out of the channels of the edge data: 255 * the channel + 0.5.
    let infoE = remapFrom01(dot(flagTexel(uv), icomp), MAXIMO);
    let infoF = remapFrom01(dot(flagTexel(uv + g1), icomp), MAXIMO);
    let infoH = remapFrom01(dot(flagTexel(uv + g2), icomp), MAXIMO);

    var line  = vec3f(0.0);
    var addr  = vec2f(0.0);
    var px    = 0.0;

    // A neighbour of level 4 says which of the four lines the edge continues.
    if (infoF == 8.0)
    {
        line = lines[12];
        px   = flag(df(F, F4) <= df(F, I));
        addr = vec2f(2.0 * px + saturate(1.0 - px), saturate(1.0 - px));
    }
    else if (infoH == 7.0)
    {
        line = lines[11];
        px   = flag(df(H, H5) <= df(H, I));
        addr = vec2f(saturate(1.0 - px), 2.0 * px + saturate(1.0 - px));
    }
    else if (infoF == 6.0)
    {
        line = lines[10];
        px   = flag(df(F, F4) <= df(F, I));
        addr = vec2f(2.0 * px + saturate(1.0 - px), saturate(1.0 - px));
    }
    else if (infoH == 5.0)
    {
        line = lines[9];
        px   = flag(df(H, H5) <= df(H, I));
        addr = vec2f(saturate(1.0 - px), 2.0 * px + saturate(1.0 - px));
    }
    else
    {
        px   = flag(df(E, F) <= df(E, H));
        addr = vec2f(px, saturate(1.0 - px));

        // The chain of ternaries upstream writes picks lines[1] to lines[8]
        // for exactly those levels and lines[0] for everything else - the
        // channel is not a level at all when it is not one of them, and a
        // clamp would read it as the highest.
        var level = 0.0;
        level = select(level, 1.0, infoE == 1.0);
        level = select(level, 2.0, infoE == 2.0);
        level = select(level, 3.0, infoE == 3.0);
        level = select(level, 4.0, infoE == 4.0);
        level = select(level, 5.0, infoE == 5.0);
        level = select(level, 6.0, infoE == 6.0);
        level = select(level, 7.0, infoE == 7.0);
        level = select(level, 8.0, infoE == 8.0);

        // A const array can only be indexed by a constant, so the table is
        // copied to a var first.
        var table = lines;
        line = table[u32(level)];
    }

    let inv = (dir.x * dir.y) < 0.0;

    // The address and the line are relative to the quarter; both are turned to
    // absolute by the direction, then by the flip of one of its axes.
    addr = addr * dir.yx;
    addr = select(addr, addr.yx, inv);

    line = vec3f(line.xy * dir.yx, line.z);
    line = vec3f(select(line.xy, line.yx, inv), line.z);

    addr += vec2f(2.0, 2.0);

    // The address of the quarter, 8 * x + y, in the fourth component.
    let param = vec4f(line, addr.x * 8.0 + addr.y);

    return remapTo01(param, LOW, HIGH);
}
