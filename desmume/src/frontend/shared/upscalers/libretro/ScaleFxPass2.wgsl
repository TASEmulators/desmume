// ScaleFX, pass 3 of 5 of the pixel filter of the WebGPU renderer.
//
// The same shader as ScaleFX_Pass2.hlsl and scalefx-pass2.glsl, all three
// ported from the libretro GLSL shader "scalefx-pass2.glsl" (libretro/glsl-
// shaders, scalefx/shaders).
//
// The junctions of the corners pass 1 measured, the dominance between them, and
// the orientation of the edges.  It reads two textures: the strengths pass 1
// made (binding 0) and the metrics of pass 0 (binding 3), which upstream names
// PassOutput0.
//
// The bvec arithmetic the upstream writes with step() is kept as arithmetic on
// vectors of ones and zeros, which is the convention the HLSL port follows too.
//
// The licence of the ported shader:
//
//   ScaleFX - Pass 2
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
@group(0) @binding(3) var metricsTexture : texture_2d<f32>;
@group(0) @binding(4) var metricsSampler : sampler;

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

// The texel at an offset of whole texels, from either texture.
fn TEX(tex : texture_2d<f32>, uv : vec2f, x : i32, y : i32) -> vec4f
{
    let size  = vec2i(constants.sourceSize.xy);
    let index = clamp(vec2i(floor(uv * constants.sourceSize.xy)) + vec2i(x, y), vec2i(0), size - vec2i(1));

    return textureLoad(tex, index, 0);
}

// WGSL has no overloads, so the four comparisons the upstream writes once and
// uses on vectors and on floats are two functions here, and the suffix says
// which.  They are the upstream's, arithmetic rather than branches: one minus
// the step, or the step itself.
fn LEv(x : vec4f, y : vec4f) -> vec4f  { return vec4f(1.0) - step(y, x); }
fn GEv(x : vec4f, y : vec4f) -> vec4f  { return vec4f(1.0) - step(x, y); }
fn LEQv(x : vec4f, y : vec4f) -> vec4f { return step(x, y); }
fn GEQv(x : vec4f, y : vec4f) -> vec4f { return step(y, x); }
fn NOTv(x : vec4f) -> vec4f           { return vec4f(1.0) - x; }

fn GEs(x : f32, y : f32) -> f32 { return 1.0 - step(x, y); }
fn NOTs(x : f32) -> f32          { return 1.0 - x; }

// corner dominance at junctions
fn dom(x : vec3f, y : vec3f, z : vec3f, w : vec3f) -> vec4f
{
    return 2.0 * vec4f(x.y, y.y, z.y, w.y) - (vec4f(x.x, y.x, z.x, w.x) + vec4f(x.z, y.z, z.z, w.z));
}

// necessary but not sufficient junction condition for orthogonal edges
fn clear(crn : vec2f, a : vec2f, b : vec2f) -> f32
{
    let condition = (crn.x >= max(min(a.x, a.y), min(b.x, b.y))) && (crn.y >= max(min(a.x, b.y), min(b.x, a.y)));

    return select(0.0, 1.0, condition);
}

@fragment
fn fsMain(input : VertexOutput) -> @location(0) vec4f
{
    /*  grid        metric      pattern

        A B C       x y z       x y
        D E F         o w       w z
        G H I
    */

    let uv = input.uv;

    // metric data
    let A = TEX(metricsTexture, uv, -1, -1); let B = TEX(metricsTexture, uv, 0, -1);
    let D = TEX(metricsTexture, uv, -1,  0); let E = TEX(metricsTexture, uv, 0,  0); let F = TEX(metricsTexture, uv, 1, 0);
    let G = TEX(metricsTexture, uv, -1,  1); let H = TEX(metricsTexture, uv, 0,  1); let I = TEX(metricsTexture, uv, 1, 1);

    // strength data
    let As = TEX(sourceTexture, uv, -1, -1); let Bs = TEX(sourceTexture, uv, 0, -1); let Cs = TEX(sourceTexture, uv, 1, -1);
    let Ds = TEX(sourceTexture, uv, -1,  0); let Es = TEX(sourceTexture, uv, 0,  0); let Fs = TEX(sourceTexture, uv, 1,  0);
    let Gs = TEX(sourceTexture, uv, -1,  1); let Hs = TEX(sourceTexture, uv, 0,  1); let Is = TEX(sourceTexture, uv, 1,  1);

    // strength & dominance junctions
    let jSx = vec4f(As.z, Bs.w, Es.x, Ds.y); let jDx = dom(As.yzw, Bs.zwx, Es.wxy, Ds.xyz);
    let jSy = vec4f(Bs.z, Cs.w, Fs.x, Es.y); let jDy = dom(Bs.yzw, Cs.zwx, Fs.wxy, Es.xyz);
    let jSz = vec4f(Es.z, Fs.w, Is.x, Hs.y); let jDz = dom(Es.yzw, Fs.zwx, Is.wxy, Hs.xyz);
    let jSw = vec4f(Ds.z, Es.w, Hs.x, Gs.y); let jDw = dom(Ds.yzw, Es.zwx, Hs.wxy, Gs.xyz);

    // majority vote for ambiguous dominance junctions
    let zero4 = vec4f(0.0);
    let jx = min(GEv(jDx, zero4) * (LEQv(jDx.yzwx, zero4) * LEQv(jDx.wxyz, zero4) + GEv(jDx + jDx.zwxy, jDx.yzwx + jDx.wxyz)), vec4f(1.0));
    let jy = min(GEv(jDy, zero4) * (LEQv(jDy.yzwx, zero4) * LEQv(jDy.wxyz, zero4) + GEv(jDy + jDy.zwxy, jDy.yzwx + jDy.wxyz)), vec4f(1.0));
    let jz = min(GEv(jDz, zero4) * (LEQv(jDz.yzwx, zero4) * LEQv(jDz.wxyz, zero4) + GEv(jDz + jDz.zwxy, jDz.yzwx + jDz.wxyz)), vec4f(1.0));
    let jw = min(GEv(jDw, zero4) * (LEQv(jDw.yzwx, zero4) * LEQv(jDw.wxyz, zero4) + GEv(jDw + jDw.zwxy, jDw.yzwx + jDw.wxyz)), vec4f(1.0));

    // inject strength without creating new contradictions
    var res : vec4f;
    res.x = min(jx.z + NOTs(jx.y) * NOTs(jx.w) * GEs(jSx.z, 0.0) * (jx.x + GEs(jSx.x + jSx.z, jSx.y + jSx.w)), 1.0);
    res.y = min(jy.w + NOTs(jy.z) * NOTs(jy.x) * GEs(jSy.w, 0.0) * (jy.y + GEs(jSy.y + jSy.w, jSy.x + jSy.z)), 1.0);
    res.z = min(jz.x + NOTs(jz.w) * NOTs(jz.y) * GEs(jSz.x, 0.0) * (jz.z + GEs(jSz.x + jSz.z, jSz.y + jSz.w)), 1.0);
    res.w = min(jw.y + NOTs(jw.x) * NOTs(jw.z) * GEs(jSw.y, 0.0) * (jw.w + GEs(jSw.y + jSw.w, jSw.x + jSw.z)), 1.0);

    // single pixel & end of line detection
    res = min(res * (vec4f(jx.z, jy.w, jz.x, jw.y) + NOTv(res.wxyz * res.yzwx)), vec4f(1.0));

    // output

    var clr : vec4f;
    clr.x = clear(vec2f(D.z, E.x), vec2f(D.w, E.y), vec2f(A.w, D.y));
    clr.y = clear(vec2f(F.x, E.z), vec2f(E.w, E.y), vec2f(B.w, F.y));
    clr.z = clear(vec2f(H.z, I.x), vec2f(E.w, H.y), vec2f(H.w, I.y));
    clr.w = clear(vec2f(H.x, G.z), vec2f(D.w, H.y), vec2f(G.w, G.y));

    let h = vec4f(min(D.w, A.w), min(E.w, B.w), min(E.w, H.w), min(D.w, G.w));
    let v = vec4f(min(E.y, D.y), min(E.y, F.y), min(H.y, I.y), min(H.y, G.y));

    let orient = GEv(h + vec4f(D.w, E.w, E.w, D.w), v + vec4f(E.y, E.y, H.y, H.y)); // orientation
    let hori = LEv(h, v) * clr; // horizontal edges
    let vert = GEv(h, v) * clr; // vertical edges

    return (res + 2.0 * hori + 4.0 * vert + 8.0 * orient) / 15.0;
}
