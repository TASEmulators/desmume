// xBRZ-Freescale, the shader xBRZ pixel filter of the WebGPU renderer.
//
// The same shader as XBRZ_Freescale.glsl and XBRZ_Freescale.hlsl, which the
// OpenGL and Direct3D 11 renderers use, all three ported from the libretro
// GLSL shader "xbrz-freescale.glsl" (libretro/glsl-shaders, xbrz/shaders).  It
// applies the xBRZ corner analysis and blending in a single pass and without a
// fixed scaling factor: the frame texture is sampled and blended while the quad
// is stretched over the window, so the emulation thread never runs a filter.
//
// The taps are read with textureLoad and integer coordinates rather than
// through a sampler.  Every tap of this shader is a whole number of texels from
// the texel centre the sample was snapped to, which is the texel a point
// sampler gives; the integer read is that texel exactly, without the sampler
// state.  The index is clamped to the edge, which is what the clamped sampler
// of the other two renderers does with the same taps.
//
// The original licences of the ported shader:
//
//   Hyllian's xBR-vertex code and texel mapping
//
//   Copyright (C) 2011/2016 Hyllian - sergiogdb@gmail.com
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
//   This shader also uses code and/or concepts from xBRZ as it appears in the
//   Desmume source code. The license for which is as follows:
//
//   This file is part of the HqMAME project. It is distributed under GNU
//   General Public License: http://www.gnu.org/licenses/gpl-3.0
//   Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved
//
//   Additionally and as a special exception, the author gives permission to
//   link the code of this program with the MAME library (or with modified
//   versions of MAME that use the same license as MAME), and distribute linked
//   combinations including the two. You must obey the GNU General Public
//   License in all respects for all of the code used other than MAME. If you
//   modify this file, you may extend this exception to your version of the
//   file, but you are not obligated to do so. If you do not wish to do so,
//   delete this exception statement from your version.

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

const BLEND_NONE     : i32 = 0;
const BLEND_NORMAL   : i32 = 1;
const BLEND_DOMINANT : i32 = 2;

const LUMINANCE_WEIGHT             = 1.0;
const EQUAL_COLOR_TOLERANCE        = 30.0 / 255.0;
const STEEP_DIRECTION_THRESHOLD    = 2.2;
const DOMINANT_DIRECTION_THRESHOLD = 3.6;

fn DistYCbCr(pixA : vec3f, pixB : vec3f) -> f32
{
    let w      = vec3f(0.2627, 0.6780, 0.0593);
    let scaleB = 0.5 / (1.0 - w.b);
    let scaleR = 0.5 / (1.0 - w.r);

    let diff = pixA - pixB;
    let Y    = dot(diff, w);
    let Cb   = scaleB * (diff.b - Y);
    let Cr   = scaleR * (diff.r - Y);

    return sqrt(((LUMINANCE_WEIGHT * Y) * (LUMINANCE_WEIGHT * Y)) + (Cb * Cb) + (Cr * Cr));
}

fn IsPixEqual(pixA : vec3f, pixB : vec3f) -> bool
{
    return DistYCbCr(pixA, pixB) < EQUAL_COLOR_TOLERANCE;
}

fn get_left_ratio(center : vec2f, origin : vec2f, direction : vec2f, scale : vec2f) -> f32
{
    let P0    = center - origin;
    let proj  = direction * (dot(P0, direction) / dot(direction, direction));
    let distv = P0 - proj;
    let orth  = vec2f(-direction.y, direction.x);
    let side  = sign(dot(P0, orth));
    let v     = side * length(distv * scale);

    return smoothstep(-sqrt(2.0) / 2.0, sqrt(2.0) / 2.0, v);
}

// GLSL compares whole vectors with == and !=, so every one of these is an
// "all of the components" and not a component-wise result.
fn pixEq(a : vec3f, b : vec3f) -> bool
{
    return all(a == b);
}

fn pixNeq(a : vec3f, b : vec3f) -> bool
{
    return any(a != b);
}

// One texel of the frame, by offset from the texel centre the sample was
// snapped to.  Upstream reaches it through the macro P(x, y), which is why
// this keeps that shape: the body reads the way the upstream one does.
fn P(coord : vec2f, x : f32, y : f32) -> vec3f
{
    let size  = vec2f(constants.sourceSize.xy);
    let uv    = coord + constants.sourceSize.zw * vec2f(x, y);
    let index = clamp(vec2i(floor(uv * size)), vec2i(0), vec2i(size) - vec2i(1));

    return textureLoad(sourceTexture, index, 0).rgb;
}

@fragment
fn fsMain(input : VertexOutput) -> @location(0) vec4f
{
    //---------------------------------------
    // Input Pixel Mapping:  -|x|x|x|-
    //                       x|A|B|C|x
    //                       x|D|E|F|x
    //                       x|G|H|I|x
    //                       -|x|x|x|-

    let texel = constants.sourceSize.zw;
    // Upstream multiplies OutputSize by one texel; that is the same number as
    // the scale the renderer hands over, and one rounding less.
    let scale = constants.scale.xy;
    let pos   = fract(input.uv * constants.sourceSize.xy) - vec2f(0.5, 0.5);
    let coord = input.uv - pos * texel;

    let A = P(coord, -1.0, -1.0);
    let B = P(coord,  0.0, -1.0);
    let C = P(coord,  1.0, -1.0);
    let D = P(coord, -1.0,  0.0);
    let E = P(coord,  0.0,  0.0);
    let F = P(coord,  1.0,  0.0);
    let G = P(coord, -1.0,  1.0);
    let H = P(coord,  0.0,  1.0);
    let I = P(coord,  1.0,  1.0);

    // blendResult Mapping: x|y|
    //                      w|z|
    var blendResult = vec4i(BLEND_NONE, BLEND_NONE, BLEND_NONE, BLEND_NONE);

    // Preprocess corners
    // Pixel Tap Mapping: -|-|-|-|-
    //                    -|-|B|C|-
    //                    -|D|E|F|x
    //                    -|G|H|I|x
    //                    -|-|x|x|-
    if (!((pixEq(E, F) && pixEq(H, I)) || (pixEq(E, H) && pixEq(F, I))))
    {
        let dist_H_F = DistYCbCr(G, E) + DistYCbCr(E, C) + DistYCbCr(P(coord, 0.0, 2.0), I) + DistYCbCr(I, P(coord, 2.0, 0.0)) + (4.0 * DistYCbCr(H, F));
        let dist_E_I = DistYCbCr(D, H) + DistYCbCr(H, P(coord, 1.0, 2.0)) + DistYCbCr(B, F) + DistYCbCr(F, P(coord, 2.0, 1.0)) + (4.0 * DistYCbCr(E, I));
        let dominantGradient = (DOMINANT_DIRECTION_THRESHOLD * dist_H_F) < dist_E_I;
        blendResult.z = select(BLEND_NONE,
                               select(BLEND_NORMAL, BLEND_DOMINANT, dominantGradient),
                               (dist_H_F < dist_E_I) && pixNeq(E, F) && pixNeq(E, H));
    }

    // Pixel Tap Mapping: -|-|-|-|-
    //                    -|A|B|-|-
    //                    x|D|E|F|-
    //                    x|G|H|I|-
    //                    -|x|x|-|-
    if (!((pixEq(D, E) && pixEq(G, H)) || (pixEq(D, G) && pixEq(E, H))))
    {
        let dist_G_E = DistYCbCr(P(coord, -2.0,  1.0), D) + DistYCbCr(D, B) + DistYCbCr(P(coord, -1.0, 2.0), H) + DistYCbCr(H, F) + (4.0 * DistYCbCr(G, E));
        let dist_D_H = DistYCbCr(P(coord, -2.0,  0.0), G) + DistYCbCr(G, P(coord, 0.0, 2.0)) + DistYCbCr(A, E) + DistYCbCr(E, I) + (4.0 * DistYCbCr(D, H));
        let dominantGradient = (DOMINANT_DIRECTION_THRESHOLD * dist_D_H) < dist_G_E;
        blendResult.w = select(BLEND_NONE,
                               select(BLEND_NORMAL, BLEND_DOMINANT, dominantGradient),
                               (dist_G_E > dist_D_H) && pixNeq(E, D) && pixNeq(E, H));
    }

    // Pixel Tap Mapping: -|-|x|x|-
    //                    -|A|B|C|x
    //                    -|D|E|F|x
    //                    -|-|H|I|-
    //                    -|-|-|-|-
    if (!((pixEq(B, C) && pixEq(E, F)) || (pixEq(B, E) && pixEq(C, F))))
    {
        let dist_E_C = DistYCbCr(D, B) + DistYCbCr(B, P(coord, 1.0, -2.0)) + DistYCbCr(H, F) + DistYCbCr(F, P(coord, 2.0, -1.0)) + (4.0 * DistYCbCr(E, C));
        let dist_B_F = DistYCbCr(A, E) + DistYCbCr(E, I) + DistYCbCr(P(coord, 0.0, -2.0), C) + DistYCbCr(C, P(coord, 2.0, 0.0)) + (4.0 * DistYCbCr(B, F));
        let dominantGradient = (DOMINANT_DIRECTION_THRESHOLD * dist_B_F) < dist_E_C;
        blendResult.y = select(BLEND_NONE,
                               select(BLEND_NORMAL, BLEND_DOMINANT, dominantGradient),
                               (dist_E_C > dist_B_F) && pixNeq(E, B) && pixNeq(E, F));
    }

    // Pixel Tap Mapping: -|x|x|-|-
    //                    x|A|B|C|-
    //                    x|D|E|F|-
    //                    -|G|H|-|-
    //                    -|-|-|-|-
    if (!((pixEq(A, B) && pixEq(D, E)) || (pixEq(A, D) && pixEq(B, E))))
    {
        let dist_D_B = DistYCbCr(P(coord, -2.0,  0.0), A) + DistYCbCr(A, P(coord, 0.0, -2.0)) + DistYCbCr(G, E) + DistYCbCr(E, C) + (4.0 * DistYCbCr(D, B));
        let dist_A_E = DistYCbCr(P(coord, -2.0, -1.0), D) + DistYCbCr(D, H) + DistYCbCr(P(coord, -1.0, -2.0), B) + DistYCbCr(B, F) + (4.0 * DistYCbCr(A, E));
        let dominantGradient = (DOMINANT_DIRECTION_THRESHOLD * dist_D_B) < dist_A_E;
        blendResult.x = select(BLEND_NONE,
                               select(BLEND_NORMAL, BLEND_DOMINANT, dominantGradient),
                               (dist_D_B < dist_A_E) && pixNeq(E, D) && pixNeq(E, B));
    }

    var res = E;

    // Pixel Tap Mapping: -|-|-|-|-
    //                    -|-|B|C|-
    //                    -|D|E|F|x
    //                    -|G|H|I|x
    //                    -|-|x|x|-
    if (blendResult.z != BLEND_NONE)
    {
        let dist_F_G = DistYCbCr(F, G);
        let dist_H_C = DistYCbCr(H, C);
        let doLineBlend = (blendResult.z == BLEND_DOMINANT ||
                           !((blendResult.y != BLEND_NONE && !IsPixEqual(E, G)) || (blendResult.w != BLEND_NONE && !IsPixEqual(E, C)) ||
                             (IsPixEqual(G, H) && IsPixEqual(H, I) && IsPixEqual(I, F) && IsPixEqual(F, C) && !IsPixEqual(E, I))));

        var origin    = vec2f(0.0, 1.0 / sqrt(2.0));
        var direction = vec2f(1.0, -1.0);

        if (doLineBlend)
        {
            let haveShallowLine = (STEEP_DIRECTION_THRESHOLD * dist_F_G <= dist_H_C) && pixNeq(E, G) && pixNeq(D, G);
            let haveSteepLine   = (STEEP_DIRECTION_THRESHOLD * dist_H_C <= dist_F_G) && pixNeq(E, C) && pixNeq(B, C);

            origin       = select(vec2f(0.0, 0.5), vec2f(0.0, 0.25), haveShallowLine);
            direction.x += select(0.0, 1.0, haveShallowLine);
            direction.y -= select(0.0, 1.0, haveSteepLine);
        }

        let blendPix = mix(H, F, select(0.0, 1.0, DistYCbCr(E, H) >= DistYCbCr(E, F)));
        res = mix(res, blendPix, get_left_ratio(pos, origin, direction, scale));
    }

    // Pixel Tap Mapping: -|-|-|-|-
    //                    -|A|B|-|-
    //                    x|D|E|F|-
    //                    x|G|H|I|-
    //                    -|x|x|-|-
    if (blendResult.w != BLEND_NONE)
    {
        let dist_H_A = DistYCbCr(H, A);
        let dist_D_I = DistYCbCr(D, I);
        let doLineBlend = (blendResult.w == BLEND_DOMINANT ||
                           !((blendResult.z != BLEND_NONE && !IsPixEqual(E, A)) || (blendResult.x != BLEND_NONE && !IsPixEqual(E, I)) ||
                             (IsPixEqual(A, D) && IsPixEqual(D, G) && IsPixEqual(G, H) && IsPixEqual(H, I) && !IsPixEqual(E, G))));

        var origin    = vec2f(-1.0 / sqrt(2.0), 0.0);
        var direction = vec2f(1.0, 1.0);

        if (doLineBlend)
        {
            let haveShallowLine = (STEEP_DIRECTION_THRESHOLD * dist_H_A <= dist_D_I) && pixNeq(E, A) && pixNeq(B, A);
            let haveSteepLine   = (STEEP_DIRECTION_THRESHOLD * dist_D_I <= dist_H_A) && pixNeq(E, I) && pixNeq(F, I);

            origin       = select(vec2f(-0.5, 0.0), vec2f(-0.25, 0.0), haveShallowLine);
            direction.y += select(0.0, 1.0, haveShallowLine);
            direction.x += select(0.0, 1.0, haveSteepLine);
        }

        let blendPix = mix(H, D, select(0.0, 1.0, DistYCbCr(E, H) >= DistYCbCr(E, D)));
        res = mix(res, blendPix, get_left_ratio(pos, origin, direction, scale));
    }

    // Pixel Tap Mapping: -|-|x|x|-
    //                    -|A|B|C|x
    //                    -|D|E|F|x
    //                    -|-|H|I|-
    //                    -|-|-|-|-
    if (blendResult.y != BLEND_NONE)
    {
        let dist_B_I = DistYCbCr(B, I);
        let dist_F_A = DistYCbCr(F, A);
        let doLineBlend = (blendResult.y == BLEND_DOMINANT ||
                           !((blendResult.x != BLEND_NONE && !IsPixEqual(E, I)) || (blendResult.z != BLEND_NONE && !IsPixEqual(E, A)) ||
                             (IsPixEqual(I, F) && IsPixEqual(F, C) && IsPixEqual(C, B) && IsPixEqual(B, A) && !IsPixEqual(E, C))));

        var origin    = vec2f(1.0 / sqrt(2.0), 0.0);
        var direction = vec2f(-1.0, -1.0);

        if (doLineBlend)
        {
            let haveShallowLine = (STEEP_DIRECTION_THRESHOLD * dist_B_I <= dist_F_A) && pixNeq(E, I) && pixNeq(H, I);
            let haveSteepLine   = (STEEP_DIRECTION_THRESHOLD * dist_F_A <= dist_B_I) && pixNeq(E, A) && pixNeq(D, A);

            origin       = select(vec2f(0.5, 0.0), vec2f(0.25, 0.0), haveShallowLine);
            direction.y -= select(0.0, 1.0, haveShallowLine);
            direction.x -= select(0.0, 1.0, haveSteepLine);
        }

        let blendPix = mix(F, B, select(0.0, 1.0, DistYCbCr(E, F) >= DistYCbCr(E, B)));
        res = mix(res, blendPix, get_left_ratio(pos, origin, direction, scale));
    }

    // Pixel Tap Mapping: -|x|x|-|-
    //                    x|A|B|C|-
    //                    x|D|E|F|-
    //                    -|G|H|-|-
    //                    -|-|-|-|-
    if (blendResult.x != BLEND_NONE)
    {
        let dist_D_C = DistYCbCr(D, C);
        let dist_B_G = DistYCbCr(B, G);
        let doLineBlend = (blendResult.x == BLEND_DOMINANT ||
                           !((blendResult.w != BLEND_NONE && !IsPixEqual(E, C)) || (blendResult.y != BLEND_NONE && !IsPixEqual(E, G)) ||
                             (IsPixEqual(C, B) && IsPixEqual(B, A) && IsPixEqual(A, D) && IsPixEqual(D, G) && !IsPixEqual(E, A))));

        var origin    = vec2f(0.0, -1.0 / sqrt(2.0));
        var direction = vec2f(-1.0, 1.0);

        if (doLineBlend)
        {
            let haveShallowLine = (STEEP_DIRECTION_THRESHOLD * dist_D_C <= dist_B_G) && pixNeq(E, C) && pixNeq(F, C);
            let haveSteepLine   = (STEEP_DIRECTION_THRESHOLD * dist_B_G <= dist_D_C) && pixNeq(E, G) && pixNeq(H, G);

            origin       = select(vec2f(0.0, -0.5), vec2f(0.0, -0.25), haveShallowLine);
            direction.x -= select(0.0, 1.0, haveShallowLine);
            direction.y += select(0.0, 1.0, haveSteepLine);
        }

        let blendPix = mix(D, B, select(0.0, 1.0, DistYCbCr(E, D) >= DistYCbCr(E, B)));
        res = mix(res, blendPix, get_left_ratio(pos, origin, direction, scale));
    }

    return vec4f(res, 1.0);
}
