// xBR lv3 multipass, pass 2 of 2, the shader xBR pixel filter of the WebGPU
// renderer.
//
// The same filter as XBR_Lv3_Pass1.hlsl and xbr-lv3-pass1.glsl: it reads the
// edge data the first pass packed into a texel at the frame size and
// interpolates the frame along the line the angle of that edge asks for.  The
// OpenGL 3.3 renderer runs the upstream file as it is, from the resource
// XBR_LV3_PASS1_GLSL; Direct3D 11 runs the HLSL port, and this is the WGSL one.
//
//   pass 0  the edge data of every pixel, at the frame size
//   pass 1  the interpolation, at whatever size the target has
//
// The licence of the ported shader:
//
//   Hyllian's xBR level 3 pass1 Shader
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
// This is the pass that reads two textures: the edge data the pass before it
// made, at binding 0, and the frame at binding 3.  Upstream rounds with
// floor(x + 0.5) - it says so in a #define - and the values here are exactly
// halfway often enough that the two roundings differ, so the port rounds the
// same way rather than with WGSL's round().

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

// The twelve lines the interpolation can run along, as the direction along them
// and the distance from the pixel centre to where the line starts.
const lines = array<vec3f, 12>(
    vec3f( 1.0,  1.0, 0.75),
    vec3f( 1.0,  1.0, 0.5),
    vec3f( 2.0,  1.0, 0.5),
    vec3f( 1.0,  2.0, 0.5),
    vec3f( 3.0,  1.0, 0.5),
    vec3f( 1.0,  3.0, 0.5),

    vec3f(-1.0,  2.0, 0.5),
    vec3f( 2.0, -1.0, 0.5),
    vec3f(-1.0,  3.0, 0.5),
    vec3f( 3.0, -1.0, 0.5),

    vec3f( 3.0,  1.0, 1.5),
    vec3f( 1.0,  3.0, 1.5));

fn roundHalf(x : f32) -> f32
{
    return floor(x + 0.5);
}

fn remapFrom01(v : f32, high : f32) -> f32
{
    return high * v + 0.5;
}

// modf(value / 2, whole): the fraction, and the whole the caller keeps.  The
// shader strips the flags of a byte with this, one bit at a time.
fn halve(value : f32) -> vec2f
{
    let half = value / 2.0;
    return vec2f(half - trunc(half), trunc(half));
}

fn unpackInfo(value : f32) -> vec4f
{
    var result = vec4f(0.0);
    var left   = value;
    var step   = halve(left);

    result.x = roundHalf(step.x); left = step.y;
    step = halve(left);
    result.y = roundHalf(step.x); left = step.y;
    step = halve(left);
    result.z = roundHalf(step.x); left = step.y;

    result.w = left;
    return result;
}

fn flagTexel(coord : vec2f) -> vec4f
{
    let size  = vec2i(constants.sourceSize.xy);
    let index = clamp(vec2i(floor(coord * constants.sourceSize.xy)), vec2i(0), size - vec2i(1));

    return textureLoad(sourceTexture, index, 0);
}

fn frameTexel(coord : vec2f) -> vec4f
{
    let size  = vec2i(constants.secondSize.xy);
    let index = clamp(vec2i(floor(coord * constants.secondSize.xy)), vec2i(0), size - vec2i(1));

    return textureLoad(secondTexture, index, 0);
}

@fragment
fn fsMain(input : VertexOutput) -> @location(0) vec4f
{
    // Upstream nudges the coordinate by 0.04% so that no tap lands exactly on
    // a texel edge; the taps below inherit it.
    let vTexCoord = input.uv * 1.0004;

    // The pixel position within the source pixel, and the direction the sample
    // leans to, which picks the taps below.
    let pos = fract(vTexCoord * constants.sourceSize.xy) - vec2f(0.5, 0.5);
    let dir = sign(pos);

    // t1 = (dx, 0, 0, dy) in texels of the frame.  Upstream picks the x or the
    // y arm with saturate(dir.x * dir.y), which is zero for both when the
    // sample sits exactly on a texel centre: then both shifts are zero.
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

    let F  = frameTexel(vTexCoord + g1).rgb;
    let B  = frameTexel(vTexCoord - g2).rgb;
    let D  = frameTexel(vTexCoord - g1).rgb;
    let H  = frameTexel(vTexCoord + g2).rgb;
    let E  = frameTexel(vTexCoord).rgb;

    let F4 = frameTexel(vTexCoord + 2.0 * g1).rgb;
    let I  = frameTexel(vTexCoord + g1 + g2).rgb;
    let H5 = frameTexel(vTexCoord + 2.0 * g2).rgb;

    // The four corners, as the vector the pixel direction is tested against:
    // mat2x4(1, 1, -1, -1, 1, -1, -1, 1) * dir, which is one dot per row.
    var icomp = vec4f(dot(dir, vec2f( 1.0,  1.0)),
                      dot(dir, vec2f( 1.0, -1.0)),
                      dot(dir, vec2f(-1.0, -1.0)),
                      dot(dir, vec2f(-1.0,  1.0)));
    icomp = floor(saturate(icomp) + 0.5);

    // The flags of this pixel and of its two neighbours, out of the channels of
    // the edge data: one channel per direction, 255 * it + 0.5 back to a byte.
    var info = remapFrom01(dot(flagTexel(vTexCoord), icomp), 255.0);

    var step = halve(info); info = step.y;                       // discard
    step = halve(info); info = step.y;                           // discard
    step = halve(info); let pxX = roundHalf(step.x); info = step.y;
    step = halve(info); let pxY = roundHalf(step.x); info = step.y;

    let flags = unpackInfo(info);

    var infoNr = remapFrom01(dot(flagTexel(vTexCoord + g1), icomp), 255.0);
    let edr3Nrl = roundHalf(halve(infoNr).x);
    step = halve(infoNr); infoNr = step.y;
    step = halve(infoNr); infoNr = step.y;
    let pxr = roundHalf(halve(infoNr).x);

    var infoNd = remapFrom01(dot(flagTexel(vTexCoord + g2), icomp), 255.0);
    step = halve(infoNd); infoNd = step.y;
    let edr3Ndu = roundHalf(halve(infoNd).x);
    step = halve(infoNd); infoNd = step.y;
    let pxd = roundHalf(halve(infoNd).x);

    let aux = roundHalf(dot(vec4f(8.0, 4.0, 2.0, 1.0), flags));

    // The upstream file picks the line with a chain of ternaries that ends at
    // the last one, which is the same as clamping the index to it.
    let index = u32(clamp(aux, 0.0, 11.0));

    // The index has to reach a var for WGSL to allow it: a const array can only
    // be indexed by a constant.
    var table = lines;
    let slep = table[index];

    let fp  = select(abs(pos.yx), abs(pos), dir.x * dir.y > 0.0);
    let fp1 = vec3f(fp.yx, -1.0);

    // InputSize over OutputSize, which is one over the scale, is how wide the
    // interpolation band is.
    let delta = 1.0 / constants.scale.x;

    var color = E;

    if (aux < 10.0)
    {
        let fx = saturate(dot(fp1, slep) / (2.0 * delta) + 0.5);
        color = mix(E, mix(mix(H, F, pxY), mix(D, B, pxY), pxX), fx);
    }
    else if (edr3Nrl == 1.0)
    {
        let fx = saturate(dot(fp1, table[10]) / (2.0 * delta) + 0.5);
        color = mix(E, mix(I, F4, pxr), fx);
    }
    else if (edr3Ndu == 1.0)
    {
        let fx = saturate(dot(fp1, table[11]) / (2.0 * delta) + 0.5);
        color = mix(E, mix(H5, I, pxd), fx);
    }

    return vec4f(color, 1.0);
}
