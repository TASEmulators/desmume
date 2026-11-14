// ScaleFX, pass 4 of 5 of the pixel filter of the WebGPU renderer.
//
// The same shader as ScaleFX_Pass3.hlsl and scalefx-pass3.glsl, all three
// ported from the libretro GLSL shader "scalefx-pass3.glsl" (libretro/glsl-
// shaders, scalefx/shaders).
//
// The edge levels of every corner and mid, up to six deep, and the subpixel each
// of them resolves to.  The four channels of what pass 2 wrote are four sets of
// flags packed into bytes - corners, horizontal edges, vertical edges and
// orientation - and upstream unpacks them by multiplying by 15, 7.5, 3.75 and
// 1.875 and taking the parity.  The flags are kept as vec4<bool> here, which is
// the second of the two conventions a GLSL bvec4 can be spelled with; the same
// shader cannot mix them, and this one has no arithmetic on the flags at all.
//
// The licence of the ported shader:
//
//   ScaleFX - Pass 3
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

// The setting the shader ships with, from the #pragma parameter line of the
// upstream file.
const SFX_SCN = 1.0;

// GLSL's mod() is the Euclidean one, x - y * floor(x / y), and positive for the
// positive arguments here; WGSL has no mod() of its own.
fn modv(x : vec4f, y : f32) -> vec4f
{
    return x - y * floor(x / y);
}

// extract first bool4 from float4 - corners
fn loadCorn(x : vec4f) -> vec4<bool>
{
    return floor(modv(x * 15.0 + 0.5, 2.0)) >= vec4f(0.5);
}

// extract second bool4 from float4 - horizontal edges
fn loadHori(x : vec4f) -> vec4<bool>
{
    return floor(modv(x * 7.5 + 0.25, 2.0)) >= vec4f(0.5);
}

// extract third bool4 from float4 - vertical edges
fn loadVert(x : vec4f) -> vec4<bool>
{
    return floor(modv(x * 3.75 + 0.125, 2.0)) >= vec4f(0.5);
}

// extract fourth bool4 from float4 - orientation
fn loadOr(x : vec4f) -> vec4<bool>
{
    return floor(modv(x * 1.875 + 0.0625, 2.0)) >= vec4f(0.5);
}

// The texel at an offset of whole texels.
fn TEX(uv : vec2f, x : i32, y : i32) -> vec4f
{
    let size  = vec2i(constants.sourceSize.xy);
    let index = clamp(vec2i(floor(uv * constants.sourceSize.xy)) + vec2i(x, y), vec2i(0), size - vec2i(1));

    return textureLoad(sourceTexture, index, 0);
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
    let E = TEX(uv,  0,  0);
    let D = TEX(uv, -1,  0); let D0 = TEX(uv, -2,  0); let D1 = TEX(uv, -3, 0);
    let F = TEX(uv,  1,  0); let F0 = TEX(uv,  2,  0); let F1 = TEX(uv,  3, 0);
    let B = TEX(uv,  0, -1); let B0 = TEX(uv,  0, -2); let B1 = TEX(uv,  0, -3);
    let H = TEX(uv,  0,  1); let H0 = TEX(uv,  0,  2); let H1 = TEX(uv,  0,  3);

    // extract data
    let Ec = loadCorn(E); let Eh = loadHori(E); let Ev = loadVert(E); let Eo = loadOr(E);
    let Dc = loadCorn(D); let Dh = loadHori(D); let Do = loadOr(D); let D0c = loadCorn(D0); let D0h = loadHori(D0); let D1h = loadHori(D1);
    let Fc = loadCorn(F); let Fh = loadHori(F); let Fo = loadOr(F); let F0c = loadCorn(F0); let F0h = loadHori(F0); let F1h = loadHori(F1);
    let Bc = loadCorn(B); let Bv = loadVert(B); let Bo = loadOr(B); let B0c = loadCorn(B0); let B0v = loadVert(B0); let B1v = loadVert(B1);
    let Hc = loadCorn(H); let Hv = loadVert(H); let Ho = loadOr(H); let H0c = loadCorn(H0); let H0v = loadVert(H0); let H1v = loadVert(H1);

    // lvl1 corners (hori, vert)
    let lvl1x = Ec.x && (Dc.z || Bc.z || SFX_SCN == 1.0);
    let lvl1y = Ec.y && (Fc.w || Bc.w || SFX_SCN == 1.0);
    let lvl1z = Ec.z && (Fc.x || Hc.x || SFX_SCN == 1.0);
    let lvl1w = Ec.w && (Dc.y || Hc.y || SFX_SCN == 1.0);

    // lvl2 mid (left, right / up, down)
    let lvl2x = vec2<bool>((Ec.x && Eh.y) && Dc.z, (Ec.y && Eh.x) && Fc.w);
    let lvl2y = vec2<bool>((Ec.y && Ev.z) && Bc.w, (Ec.z && Ev.y) && Hc.x);
    let lvl2z = vec2<bool>((Ec.w && Eh.z) && Dc.y, (Ec.z && Eh.w) && Fc.x);
    let lvl2w = vec2<bool>((Ec.x && Ev.w) && Bc.z, (Ec.w && Ev.x) && Hc.y);

    // lvl3 corners (hori, vert)
    let lvl3x = vec2<bool>(lvl2x.y && (Dh.y && Dh.x) && Fh.z, lvl2w.y && (Bv.w && Bv.x) && Hv.z);
    let lvl3y = vec2<bool>(lvl2x.x && (Fh.x && Fh.y) && Dh.w, lvl2y.y && (Bv.z && Bv.y) && Hv.w);
    let lvl3z = vec2<bool>(lvl2z.x && (Fh.w && Fh.z) && Dh.x, lvl2y.x && (Hv.y && Hv.z) && Bv.x);
    let lvl3w = vec2<bool>(lvl2z.y && (Dh.z && Dh.w) && Fh.y, lvl2w.x && (Hv.x && Hv.w) && Bv.y);

    // lvl4 corners (hori, vert)
    let lvl4x = vec2<bool>((Dc.x && Dh.y && Eh.x && Eh.y && Fh.x && Fh.y) && (D0c.z && D0h.w), (Bc.x && Bv.w && Ev.x && Ev.w && Hv.x && Hv.w) && (B0c.z && B0v.y));
    let lvl4y = vec2<bool>((Fc.y && Fh.x && Eh.y && Eh.x && Dh.y && Dh.x) && (F0c.w && F0h.z), (Bc.y && Bv.z && Ev.y && Ev.z && Hv.y && Hv.z) && (B0c.w && B0v.x));
    let lvl4z = vec2<bool>((Fc.z && Fh.w && Eh.z && Eh.w && Dh.z && Dh.w) && (F0c.x && F0h.y), (Hc.z && Hv.y && Ev.z && Ev.y && Bv.z && Bv.y) && (H0c.x && H0v.w));
    let lvl4w = vec2<bool>((Dc.w && Dh.z && Eh.w && Eh.z && Fh.w && Fh.z) && (D0c.y && D0h.x), (Hc.w && Hv.x && Ev.w && Ev.x && Bv.w && Bv.x) && (H0c.y && H0v.z));

    // lvl5 mid (left, right / up, down)
    let lvl5x = vec2<bool>(lvl4x.x && (F0h.x && F0h.y) && (D1h.z && D1h.w), lvl4y.x && (D0h.y && D0h.x) && (F1h.w && F1h.z));
    let lvl5y = vec2<bool>(lvl4y.y && (H0v.y && H0v.z) && (B1v.w && B1v.x), lvl4z.y && (B0v.z && B0v.y) && (H1v.x && H1v.w));
    let lvl5z = vec2<bool>(lvl4w.x && (F0h.w && F0h.z) && (D1h.y && D1h.x), lvl4z.x && (D0h.z && D0h.w) && (F1h.x && F1h.y));
    let lvl5w = vec2<bool>(lvl4x.y && (H0v.x && H0v.w) && (B1v.z && B1v.y), lvl4w.y && (B0v.w && B0v.x) && (H1v.y && H1v.z));

    // lvl6 corners (hori, vert)
    let lvl6x = vec2<bool>(lvl5x.y && (D1h.y && D1h.x), lvl5w.y && (B1v.w && B1v.x));
    let lvl6y = vec2<bool>(lvl5x.x && (F1h.x && F1h.y), lvl5y.y && (B1v.z && B1v.y));
    let lvl6z = vec2<bool>(lvl5z.x && (F1h.w && F1h.z), lvl5y.x && (H1v.y && H1v.z));
    let lvl6w = vec2<bool>(lvl5z.y && (D1h.z && D1h.w), lvl5w.x && (H1v.x && H1v.w));

    // subpixels - 0 = E, 1 = D, 2 = D0, 3 = F, 4 = F0, 5 = B, 6 = B0, 7 = H, 8 = H0

    // Each of the four corners and the four mids is the chain of tests the
    // upstream writes as nested ternaries; an if chain says the same and reads
    // closer to it than a nest of select() would.
    var crn : vec4f;
    if (lvl1x && Eo.x || lvl3x.x && Eo.y || lvl4x.x && Do.x || lvl6x.x && Fo.y) { crn.x = 5.0; }
    else if (lvl1x || lvl3x.y && !Eo.w || lvl4x.y && !Bo.x || lvl6x.y && !Ho.w) { crn.x = 1.0; }
    else if (lvl3x.x) { crn.x = 3.0; }
    else if (lvl3x.y) { crn.x = 7.0; }
    else if (lvl4x.x) { crn.x = 2.0; }
    else if (lvl4x.y) { crn.x = 6.0; }
    else if (lvl6x.x) { crn.x = 4.0; }
    else if (lvl6x.y) { crn.x = 8.0; }
    else { crn.x = 0.0; }

    if (lvl1y && Eo.y || lvl3y.x && Eo.x || lvl4y.x && Fo.y || lvl6y.x && Do.x) { crn.y = 5.0; }
    else if (lvl1y || lvl3y.y && !Eo.z || lvl4y.y && !Bo.y || lvl6y.y && !Ho.z) { crn.y = 3.0; }
    else if (lvl3y.x) { crn.y = 1.0; }
    else if (lvl3y.y) { crn.y = 7.0; }
    else if (lvl4y.x) { crn.y = 4.0; }
    else if (lvl4y.y) { crn.y = 6.0; }
    else if (lvl6y.x) { crn.y = 2.0; }
    else if (lvl6y.y) { crn.y = 8.0; }
    else { crn.y = 0.0; }

    if (lvl1z && Eo.z || lvl3z.x && Eo.w || lvl4z.x && Fo.z || lvl6z.x && Do.w) { crn.z = 7.0; }
    else if (lvl1z || lvl3z.y && !Eo.y || lvl4z.y && !Ho.z || lvl6z.y && !Bo.y) { crn.z = 3.0; }
    else if (lvl3z.x) { crn.z = 1.0; }
    else if (lvl3z.y) { crn.z = 5.0; }
    else if (lvl4z.x) { crn.z = 4.0; }
    else if (lvl4z.y) { crn.z = 8.0; }
    else if (lvl6z.x) { crn.z = 2.0; }
    else if (lvl6z.y) { crn.z = 6.0; }
    else { crn.z = 0.0; }

    if (lvl1w && Eo.w || lvl3w.x && Eo.z || lvl4w.x && Do.w || lvl6w.x && Fo.z) { crn.w = 7.0; }
    else if (lvl1w || lvl3w.y && !Eo.x || lvl4w.y && !Ho.w || lvl6w.y && !Bo.x) { crn.w = 1.0; }
    else if (lvl3w.x) { crn.w = 3.0; }
    else if (lvl3w.y) { crn.w = 5.0; }
    else if (lvl4w.x) { crn.w = 2.0; }
    else if (lvl4w.y) { crn.w = 8.0; }
    else if (lvl6w.x) { crn.w = 4.0; }
    else if (lvl6w.y) { crn.w = 6.0; }
    else { crn.w = 0.0; }

    var mid : vec4f;
    if (lvl2x.x && Eo.x || lvl2x.y && Eo.y || lvl5x.x && Do.x || lvl5x.y && Fo.y) { mid.x = 5.0; }
    else if (lvl2x.x) { mid.x = 1.0; }
    else if (lvl2x.y) { mid.x = 3.0; }
    else if (lvl5x.x) { mid.x = 2.0; }
    else if (lvl5x.y) { mid.x = 4.0; }
    else if (Ec.x && Dc.z && Ec.y && Fc.w) { mid.x = select(1.0, select(3.0, 5.0, Eo.y), Eo.x); }
    else { mid.x = 0.0; }

    if (lvl2y.x && !Eo.y || lvl2y.y && !Eo.z || lvl5y.x && !Bo.y || lvl5y.y && !Ho.z) { mid.y = 3.0; }
    else if (lvl2y.x) { mid.y = 5.0; }
    else if (lvl2y.y) { mid.y = 7.0; }
    else if (lvl5y.x) { mid.y = 6.0; }
    else if (lvl5y.y) { mid.y = 8.0; }
    else if (Ec.y && Bc.w && Ec.z && Hc.x) { mid.y = select(5.0, select(7.0, 3.0, !Eo.z), !Eo.y); }
    else { mid.y = 0.0; }

    if (lvl2z.x && Eo.w || lvl2z.y && Eo.z || lvl5z.x && Do.w || lvl5z.y && Fo.z) { mid.z = 7.0; }
    else if (lvl2z.x) { mid.z = 1.0; }
    else if (lvl2z.y) { mid.z = 3.0; }
    else if (lvl5z.x) { mid.z = 2.0; }
    else if (lvl5z.y) { mid.z = 4.0; }
    else if (Ec.z && Fc.x && Ec.w && Dc.y) { mid.z = select(3.0, select(1.0, 7.0, Eo.w), Eo.z); }
    else { mid.z = 0.0; }

    if (lvl2w.x && !Eo.x || lvl2w.y && !Eo.w || lvl5w.x && !Bo.x || lvl5w.y && !Ho.w) { mid.w = 1.0; }
    else if (lvl2w.x) { mid.w = 5.0; }
    else if (lvl2w.y) { mid.w = 7.0; }
    else if (lvl5w.x) { mid.w = 6.0; }
    else if (lvl5w.y) { mid.w = 8.0; }
    else if (Ec.w && Hc.y && Ec.x && Bc.z) { mid.w = select(7.0, select(5.0, 1.0, !Eo.x), !Eo.w); }
    else { mid.w = 0.0; }

    // ouput
    return (crn + 9.0 * mid) / 80.0;
}
