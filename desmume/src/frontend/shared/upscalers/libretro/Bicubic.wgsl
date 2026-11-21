// Bicubic, the Mitchell-Netravali resampler of the WebGPU renderer.
//
// The same filter as Bicubic.hlsl and bicubic.glsl: four taps per axis at
// texel centres, tied together by the cubic kernel of radius two, factor
// agnostic.  The taps are read with textureLoad and integer coordinates
// rather than through a sampler, because a tap that lands on a texel centre
// has no use for the filtering hardware: the integer read is exact, needs no
// sampler state, and is the cheaper of the two on every backend wgpu runs on.
// The index is clamped to the edge, which is what the clamped sampler of the
// other two renderers does with the same taps.
//
// The vertex stage makes the quad out of the vertex index: an upscaler pass
// covers its target, so it needs neither a vertex buffer nor an attribute.
//
// The upstream shader is GPL; its notice is in Bicubic.hlsl and bicubic.glsl.

// The block the renderer hands every pass, the one WgpuDisplay::prepareUpscaler()
// fills.  The same four float4s the Direct3D 11 backend puts in its constant
// buffer, so the two ports read the same numbers:
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

// Mitchell-Netravali, the two coefficients of the kernel.  Upstream's default,
// and the value its own no-parameter branch carries.
const BICUBIC_B = 0.3333;
const BICUBIC_C = 0.3333;

// The kernel at the distance x from the centre of it: a cubic of radius two
// that sums to one over the texels it covers.
//
// Upstream writes the square of x as pow(x, 2.0), and half of the four taps of
// an axis hand it a negative distance, where pow() is undefined.  The square
// is the product it means.
fn weight(x : f32) -> f32
{
    let ax = abs(x);
    let x2 = x * x;

    if (ax < 1.0)
    {
        return
            (
                x2 * ((12.0 - 9.0 * BICUBIC_B - 6.0 * BICUBIC_C) * ax + (-18.0 + 12.0 * BICUBIC_B + 6.0 * BICUBIC_C)) +
                (6.0 - 2.0 * BICUBIC_B)
            ) / 6.0;
    }

    if (ax < 2.0)
    {
        return
            (
                x2 * ((-BICUBIC_B - 6.0 * BICUBIC_C) * ax + (6.0 * BICUBIC_B + 30.0 * BICUBIC_C)) +
                (-12.0 * BICUBIC_B - 48.0 * BICUBIC_C) * ax + (8.0 * BICUBIC_B + 24.0 * BICUBIC_C)
            ) / 6.0;
    }

    return 0.0;
}

// The four taps of one axis, for the distance x between the sample and the
// third of them.
fn weight4(x : f32) -> vec4f
{
    return vec4f(weight(x - 2.0),
                 weight(x - 1.0),
                 weight(x),
                 weight(x + 1.0));
}

// One texel of the source, by index.  A tap of this shader is always at a texel
// centre, so its index is the floor of its coordinate in texels, and clamping
// that index to the texture is the edge handling of a clamped sampler.
fn sourceTexel(uv : vec2f) -> vec3f
{
    let size  = vec2i(constants.sourceSize.xy);
    let index = clamp(vec2i(floor(uv * constants.sourceSize.xy)), vec2i(0), size - vec2i(1));

    return textureLoad(sourceTexture, index, 0).rgb;
}

// One row of four pixels, weighted by the taps of the other axis.
fn lineRun(ypos : f32, xpos : vec4f, linetaps : vec4f) -> vec3f
{
    return sourceTexel(vec2f(xpos.x, ypos)) * linetaps.x
         + sourceTexel(vec2f(xpos.y, ypos)) * linetaps.y
         + sourceTexel(vec2f(xpos.z, ypos)) * linetaps.z
         + sourceTexel(vec2f(xpos.w, ypos)) * linetaps.w;
}

@fragment
fn fsMain(input : VertexOutput) -> @location(0) vec4f
{
    let stepxy = constants.sourceSize.zw;
    let pos = input.uv + stepxy * 0.5;
    let f = fract(pos / stepxy);

    var linetaps   = weight4(1.0 - f.x);
    var columntaps = weight4(1.0 - f.y);

    // Make sure all taps added together is exactly 1.0, otherwise some (very
    // small) distortion can occur.
    linetaps   /= linetaps.x + linetaps.y + linetaps.z + linetaps.w;
    columntaps /= columntaps.x + columntaps.y + columntaps.z + columntaps.w;

    // The first of the four taps, which sits two texels left of the texel the
    // sample starts in: upstream's `(-1.5 - f) * stepxy + pos`, with the
    // negated distance written as a subtraction.
    let start = pos - stepxy * (f + vec2f(1.5));

    let x = vec4f(start.x,
                  start.x + stepxy.x,
                  start.x + stepxy.x * 2.0,
                  start.x + stepxy.x * 3.0);

    let rgb = lineRun(start.y,                    x, linetaps) * columntaps.x
            + lineRun(start.y + stepxy.y,         x, linetaps) * columntaps.y
            + lineRun(start.y + stepxy.y * 2.0,   x, linetaps) * columntaps.z
            + lineRun(start.y + stepxy.y * 3.0,   x, linetaps) * columntaps.w;

    // The cubic overshoots its range at an edge; clamping keeps the three
    // ports equal whatever format the target has.
    return vec4f(saturate(rgb), 1.0);
}
