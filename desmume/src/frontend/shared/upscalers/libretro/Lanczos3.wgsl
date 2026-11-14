// Lanczos3, the Lanczos upscaler of the WebGPU renderer.
//
// The same filter as Lanczos3.hlsl and lanczos3.glsl: three taps per axis at
// texel centres, three taps of the kernel per axis, factor agnostic.  The taps
// are read with textureLoad and integer coordinates rather than through a
// sampler, because a tap that lands on a texel centre has no use for the
// filtering hardware: the integer read is exact, needs no sampler state, and
// is the cheaper of the two on every backend wgpu runs on.  The index is
// clamped to the edge, which is what the clamped sampler of the other two
// renderers does with the same taps.
//
// The vertex stage makes the quad out of the vertex index: an upscaler pass
// covers its target, so it needs neither a vertex buffer nor an attribute.

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

const LANCZOS_PI     = 3.1415926535897932384626433832795;
const LANCZOS_RADIUS = 2.0;

// The three taps of one axis, for the distance x between the sample and the
// middle tap: the kernel at 1 + x, x and 1 - x, normalised to sum to one.  The
// max() keeps the middle term from dividing zero by zero at a texel centre,
// where sin() of a multiple of pi is not quite zero.
fn lanczosTaps(x : f32) -> vec3f
{
    let smpl = max(abs(LANCZOS_PI * vec3f(1.0 + x, x, 1.0 - x)), vec3f(0.000001));

    // No point in multiplying by the radius, the taps are normalised below.
    let taps = sin(smpl) * sin(smpl / LANCZOS_RADIUS) / (smpl * smpl);

    return taps / dot(taps, vec3f(1.0));
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

@fragment
fn fsMain(input : VertexOutput) -> @location(0) vec4f
{
    let onePixel = constants.sourceSize.zw;

    // Where the sample falls in the frame, in texels, and how far past the
    // texel centre it starts at it lies.
    let pos = input.uv + onePixel * 0.5;
    let f   = fract(pos * constants.sourceSize.xy);

    let linetaps   = lanczosTaps(f.x);
    let columntaps = lanczosTaps(f.y);

    // The first of the three taps of each axis, one texel left of the texel
    // the sample starts in.
    let start = pos - onePixel * (f + vec2f(1.5));

    let x = vec3f(start.x, start.x + onePixel.x, start.x + onePixel.x * 2.0);

    let y0 = start.y;
    let y1 = start.y + onePixel.y;
    let y2 = start.y + onePixel.y * 2.0;

    let row0 = sourceTexel(vec2f(x.x, y0)) * linetaps.x
             + sourceTexel(vec2f(x.y, y0)) * linetaps.y
             + sourceTexel(vec2f(x.z, y0)) * linetaps.z;
    let row1 = sourceTexel(vec2f(x.x, y1)) * linetaps.x
             + sourceTexel(vec2f(x.y, y1)) * linetaps.y
             + sourceTexel(vec2f(x.z, y1)) * linetaps.z;
    let row2 = sourceTexel(vec2f(x.x, y2)) * linetaps.x
             + sourceTexel(vec2f(x.y, y2)) * linetaps.y
             + sourceTexel(vec2f(x.z, y2)) * linetaps.z;

    let rgb = row0 * columntaps.x + row1 * columntaps.y + row2 * columntaps.z;

    return vec4f(saturate(rgb), 1.0);
}
