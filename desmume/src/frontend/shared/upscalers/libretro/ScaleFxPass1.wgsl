// ScaleFX, pass 2 of 5 of the pixel filter of the WebGPU renderer.
//
// The same shader as ScaleFX_Pass1.hlsl and scalefx-pass1.glsl, all three
// ported from the libretro GLSL shader "scalefx-pass1.glsl" (libretro/glsl-
// shaders, scalefx/shaders).
//
// The strength of each of the four corners of a texel, out of the metric data
// pass 0 wrote.  Everything is read back as four channels, alpha included: the
// metrics of a texel are its four colour distances.
//
// The licence of the ported shader:
//
//   ScaleFX - Pass 1
//   by Sp00kyFox, 2017-03-01
//
//   Copyright (c) 2016 Sp00kyFox - ScaleFX@web.de
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
// upstream file.
const SFX_CLR = 0.5;
const SFX_SAA = 1.0;

// The texel at an offset of whole texels, all four channels of it.
fn TEX(uv : vec2f, x : i32, y : i32) -> vec4f
{
    let size  = vec2i(constants.sourceSize.xy);
    let index = clamp(vec2i(floor(uv * constants.sourceSize.xy)) + vec2i(x, y), vec2i(0), size - vec2i(1));

    return textureLoad(sourceTexture, index, 0);
}

// corner strength
fn str(d : f32, a : vec2f, b : vec2f) -> f32
{
    let diff  = a.x - a.y;
    let wght1 = max(SFX_CLR - d, 0.0) / SFX_CLR;
    let wght2 = clamp((1.0 - d) + select(-diff, diff, min(a.x, b.x) + a.x > min(a.y, b.y) + a.y), 0.0, 1.0);

    return select(0.0, (wght1 * wght2) * (a.x * a.y), SFX_SAA == 1.0 || 2.0 * d < a.x + a.y);
}

@fragment
fn fsMain(input : VertexOutput) -> @location(0) vec4f
{
    /*  grid        metric      pattern

        A B         x y z       x y
        D E F         o w       w z
        G H I
    */

    let uv = input.uv;

    // metric data
    let A = TEX(uv, -1, -1); let B = TEX(uv, 0, -1);
    let D = TEX(uv, -1,  0); let E = TEX(uv, 0,  0); let F = TEX(uv, 1, 0);
    let G = TEX(uv, -1,  1); let H = TEX(uv, 0,  1); let I = TEX(uv, 1, 1);

    // corner strength
    var res : vec4f;
    res.x = str(D.z, vec2f(D.w, E.y), vec2f(A.w, D.y));
    res.y = str(F.x, vec2f(E.w, E.y), vec2f(B.w, F.y));
    res.z = str(H.z, vec2f(E.w, H.y), vec2f(H.w, I.y));
    res.w = str(H.x, vec2f(D.w, H.y), vec2f(G.w, G.y));

    return res;
}
