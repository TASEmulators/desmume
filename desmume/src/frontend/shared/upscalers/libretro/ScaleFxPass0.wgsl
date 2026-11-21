// ScaleFX, pass 1 of 5 of the pixel filter of the WebGPU renderer.
//
// The same shader as ScaleFX_Pass0.hlsl and scalefx-pass0.glsl, all three
// ported from the libretro GLSL shader "scalefx-pass0.glsl" (libretro/glsl-
// shaders, scalefx/shaders).  The OpenGL 3.3 renderer runs the upstream file
// as it is, Direct3D 11 runs the HLSL port, and this is the WGSL one.
//
// ScaleFX is Sp00kyFox's edge interpolation for pixel art: it interpolates
// edges up to level six, mixes slopes smoothly, and only ever uses colours the
// frame already had.  The five passes are
//
//   pass 0  the colour distance between a texel and its neighbours
//   pass 1  the strength of the four corners of every texel
//   pass 2  the junctions, and the dominance between them
//   pass 3  the subpixel every corner and mid of a texel resolves to
//   pass 4  the colour, taken from the frame at that subpixel
//
// The taps are whole texels from the sample, which is what upstream reaches
// with textureOffset; they are read here with textureLoad and integer indices,
// clamped to the edge the way a clamped sampler is.
//
// The licence of the ported shader:
//
//   ScaleFX - Pass 0
//   by Sp00kyFox, 2017-03-01
//
//   Filter: Nearest
//   Scale:  1x
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

// Reference: http://www.compuphase.com/cmetric.htm
fn dist(A : vec3f, B : vec3f) -> f32
{
    let r = 0.5 * (A.r + B.r);
    let d = A - B;
    let c = vec3f(2.0 + r, 4.0, 3.0 - r);

    return sqrt(dot(c * d, d)) / 3.0;
}

// The texel at an offset of whole texels, which is what upstream's textureOffset
// reaches.
fn TEX(uv : vec2f, x : i32, y : i32) -> vec3f
{
    let size  = vec2i(constants.sourceSize.xy);
    let index = clamp(vec2i(floor(uv * constants.sourceSize.xy)) + vec2i(x, y), vec2i(0), size - vec2i(1));

    return textureLoad(sourceTexture, index, 0).rgb;
}

@fragment
fn fsMain(input : VertexOutput) -> @location(0) vec4f
{
    /*  grid        metric

        A B C       x y z
          E F         o w
    */

    // read texels
    let A = TEX(input.uv, -1, -1);
    let B = TEX(input.uv,  0, -1);
    let C = TEX(input.uv,  1, -1);
    let E = TEX(input.uv,  0,  0);
    let F = TEX(input.uv,  1,  0);

    // output
    return vec4f(dist(E, A), dist(E, B), dist(E, C), dist(E, F));
}
