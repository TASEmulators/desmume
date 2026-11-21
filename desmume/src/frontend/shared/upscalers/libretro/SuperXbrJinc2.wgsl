// super-xBR, pass 3 of 3 of the pixel filter of the WebGPU renderer.
//
// The same shader as SuperXBR_Jinc2.hlsl and custom-jinc2-sharper.glsl, all
// three ported from the libretro GLSL shader "custom-jinc2-sharper.glsl"
// (libretro/glsl-shaders, super-xbr/shaders).  The OpenGL 3.3 renderer runs
// the upstream file as it is, Direct3D 11 runs the HLSL port, and this is the
// WGSL one.
//
// Hyllian's windowed jinc 2-lobe filter with anti-ringing, over the picture
// the two passes in front of it made.  The four by four it reads is snapped to
// the texel centre the sample belongs to, so every tap is a whole number of
// texels away and is read with textureLoad, the way the other ports read
// theirs; the anti-ringing taps that follow are one texel off the sample and
// are read the same way.
//
// The licence of the ported shader:
//
//   Hyllian's jinc windowed-jinc 2-lobe sharper with anti-ringing Shader
//
//   Copyright (C) 2011-2014 Hyllian/Jararaca - sergiogdb@gmail.com
//
//   This program is free software; you can redistribute it and/or modify it
//   under the terms of the GNU General Public License as published by the Free
//   Software Foundation; either version 2 of the License, or (at your option)
//   any later version.
//
//   This program is distributed in the hope that it will be useful, but WITHOUT
//   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
//   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
//   more details.
//
//   You should have received a copy of the GNU General Public License along
//   with this program; if not, write to the Free Software Foundation, Inc.,
//   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

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
const JINC2_WINDOW_SINC = 0.42;
const JINC2_SINC        = 0.92;
const JINC2_AR_STRENGTH = 1.0;

const pi = 3.1415926535897932384626433832795;

// A=0.5, B=0.825 is the best jinc approximation for x<2.5; with B=1.0 this is
// a lanczos filter instead.  The upstream spells the two as wa and wb.
const wa = JINC2_WINDOW_SINC * pi;
const wb = JINC2_SINC * pi;

fn df(A : f32, B : f32) -> f32
{
    return abs(A - B);
}

// Calculates the distance between two points
fn distance(pt1 : vec2f, pt2 : vec2f) -> f32
{
    let v = pt2 - pt1;
    return sqrt(dot(v, v));
}

fn min4(a : vec3f, b : vec3f, c : vec3f, d : vec3f) -> vec3f
{
    return min(a, min(b, min(c, d)));
}

fn max4(a : vec3f, b : vec3f, c : vec3f, d : vec3f) -> vec3f
{
    return max(a, max(b, max(c, d)));
}

// The approximation of Jinc(x)*Jinc(x*r1/r2) for x < 2.5, taken at the four
// distances of one row of the four by four.
fn resampler(x : vec4f) -> vec4f
{
    let taps = sin(x * wa) * sin(x * wb) / (x * x);

    var res : vec4f;
    res.x = select(taps.x, wa * wb, x.x == 0.0);
    res.y = select(taps.y, wa * wb, x.y == 0.0);
    res.z = select(taps.z, wa * wb, x.z == 0.0);
    res.w = select(taps.w, wa * wb, x.w == 0.0);
    return res;
}

// The vec4 * mat4x3 the upstream writes as mul(w, mat4x3(c0, c1, c2, c3)).
fn mul4x3(w : vec4f, c0 : vec3f, c1 : vec3f, c2 : vec3f, c3 : vec3f) -> vec3f
{
    return w.x * c0 + w.y * c1 + w.z * c2 + w.w * c3;
}

fn texel(uv : vec2f) -> vec3f
{
    let size  = vec2f(constants.sourceSize.xy);
    let index = clamp(vec2i(floor(uv * size)), vec2i(0), vec2i(size) - vec2i(1));

    return textureLoad(sourceTexture, index, 0).rgb;
}

@fragment
fn fsMain(input : VertexOutput) -> @location(0) vec4f
{
    // Upstream's vertex stage nudges the texcoord by 1.0001 and hands that on,
    // and this pass is one where it shows: the four by four is snapped with
    // floor(pc - 0.5), so a sample that lands on a half exactly - which every
    // third output pixel here does - falls on the wrong side of the floor
    // without the nudge, and the whole window is then one texel off.  The two
    // other renderers compile the nudge as well.
    let vTexCoord = input.uv * 1.0001;

    let dx = vec2f(1.0, 0.0);
    let dy = vec2f(0.0, 1.0);

    let pc = vTexCoord * constants.sourceSize.xy;

    let tc = floor(pc - vec2f(0.5, 0.5)) + vec2f(0.5, 0.5);

    let weights0 = resampler(vec4f(distance(pc, tc - dx - dy), distance(pc, tc - dy), distance(pc, tc + dx - dy), distance(pc, tc + 2.0 * dx - dy)));
    let weights1 = resampler(vec4f(distance(pc, tc - dx),      distance(pc, tc),      distance(pc, tc + dx),      distance(pc, tc + 2.0 * dx)));
    let weights2 = resampler(vec4f(distance(pc, tc - dx + dy), distance(pc, tc + dy), distance(pc, tc + dx + dy), distance(pc, tc + 2.0 * dx + dy)));
    let weights3 = resampler(vec4f(distance(pc, tc - dx + 2.0 * dy), distance(pc, tc + 2.0 * dy), distance(pc, tc + dx + 2.0 * dy), distance(pc, tc + 2.0 * dx + 2.0 * dy)));

    let oneTexelX = dx / constants.sourceSize.xy;
    let oneTexelY = dy / constants.sourceSize.xy;
    let centre    = tc / constants.sourceSize.xy;

    // reading the texels
    let c00 = texel(centre - oneTexelX - oneTexelY);
    let c10 = texel(centre            - oneTexelY);
    let c20 = texel(centre + oneTexelX - oneTexelY);
    let c30 = texel(centre + 2.0 * oneTexelX - oneTexelY);
    let c01 = texel(centre - oneTexelX            );
    let c11 = texel(centre                        );
    let c21 = texel(centre + oneTexelX            );
    let c31 = texel(centre + 2.0 * oneTexelX      );
    let c02 = texel(centre - oneTexelX + oneTexelY);
    let c12 = texel(centre            + oneTexelY);
    let c22 = texel(centre + oneTexelX + oneTexelY);
    let c32 = texel(centre + 2.0 * oneTexelX + oneTexelY);
    let c03 = texel(centre - oneTexelX + 2.0 * oneTexelY);
    let c13 = texel(centre            + 2.0 * oneTexelY);
    let c23 = texel(centre + oneTexelX + 2.0 * oneTexelY);
    let c33 = texel(centre + 2.0 * oneTexelX + 2.0 * oneTexelY);

    var color = mul4x3(weights0, c00, c10, c20, c30);
    color += mul4x3(weights1, c01, c11, c21, c31);
    color += mul4x3(weights2, c02, c12, c22, c32);
    color += mul4x3(weights3, c03, c13, c23, c33);

    // The sum of all sixteen weights, which is what dot(mul(weights, vec4(1)),
    // vec4(1)) works out to.
    let total = dot(weights0, vec4f(1.0)) + dot(weights1, vec4f(1.0)) +
                dot(weights2, vec4f(1.0)) + dot(weights3, vec4f(1.0));
    color = color / total;

    // Anti-ringing
    //  Get min/max samples
    let pc0 = vTexCoord;

    let a00 = texel(pc0);
    let a11 = texel(pc0 + oneTexelX);
    let a21 = texel(pc0 - oneTexelX);
    let a12 = texel(pc0 + oneTexelY);
    let a22 = texel(pc0 - oneTexelY);

    var min_sample = min4(a11, a21, a12, a22);
    var max_sample = max4(a11, a21, a12, a22);
    min_sample = min(min_sample, a00);
    max_sample = max(max_sample, a00);

    let aux = color;

    color = clamp(color, min_sample, max_sample);
    color = mix(aux, color, JINC2_AR_STRENGTH);

    return vec4f(color, 1.0);
}
