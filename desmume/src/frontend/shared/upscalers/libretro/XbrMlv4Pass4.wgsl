// xBR-mlv4, pass 4 of 4 of the pixel filter of the WebGPU renderer.
//
// The same shader as XBR_Mlv4_Pass4.hlsl and xbr-mlv4-pass4.glsl, all three
// ported from the libretro GLSL shader "xbr-mlv4-pass4.glsl" (libretro/glsl-
// shaders, xbr/shaders).  It is the pass that reads two textures: the lines
// pass 3 wrote at twice the frame size, at binding 0, and the frame at
// binding 3.
//
// Every pixel is one quarter of a frame texel.  The line of its quarter says
// which colour to interpolate towards - the one along the line, one or two
// texels out, encoded as an address in the fourth component of the line pass -
// and how far along the line the quarter is.  The four quarters of a frame
// texel are read at the four addresses and the one that differs most from the
// frame's own colour wins.
//
// The licence of the ported shader:
//
//   Hyllian's xBR MultiLevel4 Shader - Pass4
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
// Upstream nudges the coordinate here, and by a different amount on each axis
// (1.0004 across, 0.9995 down) rather than in the vertex shader of the file.
// It is kept: the quarter a pixel falls in is a frac() of the coordinate, and
// without the nudge every pixel of a 1:1 frame sits on a boundary.

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

const BIN = vec3f(4.0, 2.0, 1.0);

fn saturate(x : f32) -> f32 { return clamp(x, 0.0, 1.0); }

fn roundHalf(x : f32) -> f32 { return floor(x + 0.5); }

// remapFrom01(v, (-64, -64, -64, -64), (64, 64, 64, 64)): a rounding of the
// range back to whole numbers.
fn remapFrom01(v : vec4f) -> vec4f
{
    let scaled = -64.0 + 128.0 * v;

    return vec4f(roundHalf(scaled.x), roundHalf(scaled.y),
                 roundHalf(scaled.z), roundHalf(scaled.w));
}

// c_df(): how far apart two colours are, by the sum of the three channels.
fn c_df(c1 : vec3f, c2 : vec3f) -> f32
{
    let d = abs(c1 - c2);

    return d.r + d.g + d.b;
}

// One texel of what pass 3 wrote, at the coordinate of a pixel of the target.
fn lineTexel(coord : vec2f) -> vec4f
{
    let size  = vec2i(constants.sourceSize.xy);
    let index = clamp(vec2i(floor(coord * constants.sourceSize.xy)), vec2i(0), size - vec2i(1));

    return textureLoad(sourceTexture, index, 0);
}

// One texel of the frame.
fn frameTexel(coord : vec2f) -> vec3f
{
    let size  = vec2i(constants.secondSize.xy);
    let index = clamp(vec2i(floor(coord * constants.secondSize.xy)), vec2i(0), size - vec2i(1));

    return textureLoad(secondTexture, index, 0).rgb;
}

// modf(value / 1.9999, whole): the three digits of the address upstream packs
// into the fourth component, three at a time, least significant first.
fn digit(value : f32) -> vec2f
{
    let scaled = value / 1.9999;
    let whole  = trunc(scaled);

    return vec2f(roundHalf(scaled - whole), whole);
}

// GET_PIXEL: unpack the address apart and read the frame at the texel it names.
// The 1.9999 is upstream's, and it is what keeps the address exactly off a
// texel edge: 1.0001 of a texel reads as the next one, 0.0001 as this one.
fn addressedTexel(uv : vec2f, value : f32) -> vec3f
{
    var info = value;

    var ay = vec3f(0.0);
    var ax = vec3f(0.0);

    var part = digit(info); ay.z = part.x; info = part.y;
    part = digit(info);     ay.y = part.x; info = part.y;
    part = digit(info);     ay.x = part.x; info = part.y;
    part = digit(info);     ax.z = part.x; info = part.y;
    part = digit(info);     ax.y = part.x; info = part.y;
    ax.x = roundHalf(info);

    let iq = vec2f(dot(ax, BIN) - 1.9999, dot(ay, BIN) - 1.9999);

    return frameTexel(uv + vec2f(iq.x * constants.secondSize.z, iq.y * constants.secondSize.w));
}

@fragment
fn fsMain(input : VertexOutput) -> @location(0) vec4f
{
    let uv = input.uv * vec2f(1.0004, 0.9995);

    // Where in its frame texel the pixel sits, as -0.5 to 0.5 on each axis.
    let fp = fract(uv * constants.secondSize.xy) - vec2f(0.5, 0.5);

    // The corner of that texel, which the four quarters are addressed from.
    // Upstream writes it as floor(uv * size) / size; the texels below are read
    // at whole numbers of the twice-sized target, so the floor is enough.
    let quarter = floor(uv * constants.secondSize.xy) / constants.secondSize.xy;
    let stepX   = vec2f(constants.secondSize.z, 0.0);
    let stepY   = vec2f(0.0, constants.secondSize.w);

    let UL = remapFrom01(lineTexel(quarter + 0.25 * stepX + 0.25 * stepY));
    let UR = remapFrom01(lineTexel(quarter + 0.75 * stepX + 0.25 * stepY));
    let DL = remapFrom01(lineTexel(quarter + 0.25 * stepX + 0.75 * stepY));
    let DR = remapFrom01(lineTexel(quarter + 0.75 * stepX + 0.75 * stepY));

    let E = frameTexel(uv);

    let PX = addressedTexel(uv, UL.w);
    let PY = addressedTexel(uv, UR.w);
    let PZ = addressedTexel(uv, DL.w);
    let PW = addressedTexel(uv, DR.w);

    // The line each quarter follows, as the distance along it, and the
    // steepness of it: the ratio of the two weights, whichever way it leans.
    let fp1 = vec3f(fp, -1.0);

    let inc   = vec4f(abs(UL.x / UL.y), abs(UR.x / UR.y), abs(DL.x / DL.y), abs(DR.x / DR.y));
    let level = max(inc, 1.0 / inc);

    // How wide the interpolation band is, in pixels of the target: upstream
    // takes OutputSize over the input size it read the frame at.
    let scaleFactor = constants.outputSize.x / constants.secondSize.x;

    let slope = vec4f(scaleFactor) / (8.0 * level);

    let fx = vec4f(saturate(dot(fp1, UL.xyz) * slope.x + 0.5),
                   saturate(dot(fp1, UR.xyz) * slope.y + 0.5),
                   saturate(dot(fp1, DL.xyz) * slope.z + 0.5),
                   saturate(dot(fp1, DR.xyz) * slope.w + 0.5));

    let c1 = mix(E, PX, fx.x);
    let c2 = mix(E, PY, fx.y);
    let c3 = mix(E, PZ, fx.z);
    let c4 = mix(E, PW, fx.w);

    // The quarter that the line took furthest from the frame's own colour.
    var color = c1;
    color = select(color, c2, c_df(c2, E) > c_df(color, E));
    color = select(color, c3, c_df(c3, E) > c_df(color, E));
    color = select(color, c4, c_df(c4, E) > c_df(color, E));

    return vec4f(color, 1.0);
}
