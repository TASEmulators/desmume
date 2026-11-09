// 2xBR, pass 2 of 2, the shader xBR pixel filter of the WebGPU renderer.
//
// The same filter as XBR_2x_Pass1.hlsl and 2xbr-lv1-c-pass1.glsl: it reads the
// two flags the first pass packed into every texel - whether an edge runs
// through the pixel, and which of its two neighbours it leans to - and blends
// the frame with itself along that edge.  The OpenGL 3.3 renderer runs the
// upstream file as it is, from the resource XBR_2X_PASS1_GLSL; Direct3D 11 runs
// the HLSL port, and this is the WGSL one.
//
//   pass 0  the flags of every pixel, at the frame size
//   pass 1  the blend, at whatever size the target has
//
// Upstream carries no licence statement and no copyright line, only the name of
// the shader: 2xBR v1.0, by Hyllian.
//
// This is the pass that reads two textures: the flags the pass before it made,
// at binding 0, and the frame at binding 3, which is what the chain hands over
// as its second texture.  Both taps are whole texels, so both go through
// textureLoad, but they are texels of textures of their own - the flags are at
// the frame size and the frame is the frame - so each size comes from its own
// field of the block.

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

// The four corners, as the vector the pixel direction is tested against.
const sym_vectors = mat4x2<f32>(vec2f( 1.0,  1.0), vec2f( 1.0, -1.0),
                                vec2f(-1.0, -1.0), vec2f(-1.0,  1.0));

fn remapFrom01(v : f32, high : f32) -> f32
{
    return high * v + 0.5;
}

// One texel of the flags the first pass made, and one of the frame, by index.
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
    let TEX0 = input.uv;

    // The centre of the frame texel this sample covers: upstream rounds the
    // coordinate down and puts it back in the middle of that texel, so the taps
    // below are whole texels from a texel centre.
    let OriginalCoord = (floor(constants.secondSize.xy * TEX0) + 0.5) * constants.secondSize.zw;

    // The pixel position within the source pixel, and the direction the sample
    // leans to, which picks the three taps below out of the ones around it.
    let pos = fract(TEX0 * constants.sourceSize.xy) - vec2f(0.5, 0.5);
    let dir = sign(pos);

    // t1 = (dx, 0, 0, dy), the two texels of the passing direction.
    let g1 = dir * vec2f(constants.sourceSize.z, 0.0);
    let g2 = dir * vec2f(0.0, constants.sourceSize.w);

    let F = frameTexel(OriginalCoord + g1).rgb;
    let H = frameTexel(OriginalCoord + g2).rgb;
    let E = frameTexel(OriginalCoord).rgb;

    // The flags: two bits per channel, chosen by the direction, read back out
    // of the 0..3 the first pass stored, divided by 255 there and multiplied
    // back here.
    let icomp = round(clamp(dir * sym_vectors, vec4f(0.0), vec4f(1.0)));
    let info  = remapFrom01(dot(flagTexel(TEX0), icomp), 255.0);

    // unpack_info(): the fraction of half the value is the edge flag, the
    // integer part of it is the neighbour flag.
    let half = info / 2.0;
    let edr  = round(half - floor(half));
    let px   = floor(half);

    let color = mix(E, mix(H, F, px), edr * 0.5);

    return vec4f(color, 1.0);
}
