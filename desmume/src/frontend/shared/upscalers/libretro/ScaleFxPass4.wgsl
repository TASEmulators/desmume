// ScaleFX, pass 5 of 5 of the pixel filter of the WebGPU renderer.
//
// The same shader as ScaleFX_Pass4.hlsl and scalefx-pass4.glsl, all three
// ported from the libretro GLSL shader "scalefx-pass4.glsl" (libretro/glsl-
// shaders, scalefx/shaders).
//
// The last pass draws no interpolation of its own: it reads the subpixel the
// pass in front of it resolved for each ninth of a texel, and takes the colour
// from the frame at the texel that subpixel names.  Upstream reaches the frame
// through PassPrev5Texture, which is the second texture of the chain here
// (binding 3) - the frame itself.
//
// The licence of the ported shader:
//
//   ScaleFX - Pass 4
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
//   secondSize.xy: the size of the second texture, zw: one texel of it
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
@group(0) @binding(4) var secondSampler : sampler;

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

// GLSL's mod(), which WGSL does not have.
fn modv(x : vec4f, y : f32) -> vec4f
{
    return x - y * floor(x / y);
}

// extract corners
fn loadCrn(x : vec4f) -> vec4f
{
    return floor(modv(x * 80.0 + 0.5, 9.0));
}

// extract mids
fn loadMid(x : vec4f) -> vec4f
{
    return floor(modv(x * 8.888888 + 0.055555, 9.0));
}

// The texel at an offset of whole texels, from either texture.
fn TEX(tex : texture_2d<f32>, uv : vec2f, x : f32, y : f32) -> vec4f
{
    let size  = vec2i(constants.sourceSize.xy);
    let index = clamp(vec2i(floor((uv + vec2f(x, y) / constants.sourceSize.xy) * constants.sourceSize.xy)),
                      vec2i(0), size - vec2i(1));

    return textureLoad(tex, index, 0);
}

@fragment
fn fsMain(input : VertexOutput) -> @location(0) vec4f
{
    /*  grid        corners     mids

          B         x   y         x
        D E F             w   y
          H         w   z         z
    */

    let uv = input.uv;

    // read data
    let E = TEX(sourceTexture, uv, 0.0, 0.0);

    // extract data
    let crn = loadCrn(E);
    let mid = loadMid(E);

    // determine subpixel
    let fp = floor(3.0 * fract(uv * constants.sourceSize.xy));

    var sp : f32;
    if (fp.y == 0.0)
    {
        sp = select(crn.y, mid.x, fp.x == 1.0);
        sp = select(sp,     crn.x, fp.x == 0.0);
    }
    else if (fp.y == 1.0)
    {
        sp = select(mid.y, 0.0, fp.x == 1.0);
        sp = select(sp,    mid.w, fp.x == 0.0);
    }
    else
    {
        sp = select(crn.z, mid.z, fp.x == 1.0);
        sp = select(sp,    crn.w, fp.x == 0.0);
    }

    // output coordinate - 0 = E, 1 = D, 2 = D0, 3 = F, 4 = F0, 5 = B, 6 = B0, 7 = H, 8 = H0
    var res : vec2f;
    if (sp == 0.0)      { res = vec2f( 0.0,  0.0); }
    else if (sp == 1.0) { res = vec2f(-1.0,  0.0); }
    else if (sp == 2.0) { res = vec2f(-2.0,  0.0); }
    else if (sp == 3.0) { res = vec2f( 1.0,  0.0); }
    else if (sp == 4.0) { res = vec2f( 2.0,  0.0); }
    else if (sp == 5.0) { res = vec2f( 0.0, -1.0); }
    else if (sp == 6.0) { res = vec2f( 0.0, -2.0); }
    else if (sp == 7.0) { res = vec2f( 0.0,  1.0); }
    else                { res = vec2f( 0.0,  2.0); }

    // ouput
    return TEX(secondTexture, uv, res.x, res.y);
}
