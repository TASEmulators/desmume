// 2xBR, pass 1 of 2, the shader xBR pixel filter of the WebGPU renderer.
//
// The same filter as XBR_2x_Pass0.hlsl and 2xbr-lv1-c-pass0.glsl: Hyllian's
// 2xBR, which decides per pixel whether an edge runs through it and which of
// its two neighbours it leans to, and packs both answers into one texel for the
// second pass to blend with.  The OpenGL 3.3 renderer runs the upstream file as
// it is, from the resource XBR_2X_PASS0_GLSL; Direct3D 11 runs the HLSL port,
// and this is the WGSL one.
//
//   pass 0  the flags of every pixel, at the frame size
//   pass 1  the blend, at whatever size the target has
//
// Upstream carries no licence statement and no copyright line, only the name of
// the shader: 2xBR v1.0, by Hyllian.
//
// Upstream computes the seven tap rectangles in its vertex shader and hands
// them on as varyings; here they are computed in the pixel shader, as in the
// other ports, and the taps are read with textureLoad and integer coordinates,
// which is the texel a point sampler would have given.
//
// Upstream works in bvec4, and WGSL has no boolean vector: every one of them is
// a vec4 of ones and zeros here, with and, or and not as the arithmetic that
// gives the same answers.

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

const threshold = 15.0;
const bin       = vec4f(1.0, 2.0, 4.0, 8.0);
const maximo    = vec4f(255.0);

// The luma row of the weighted colour space the shader compares pixels in,
// which is yuv_weighted[0].
const rgbw = vec3f(14.352, 28.176, 5.472);

// The texel nearest to a corner of the output pixel, by index: the taps are
// offsets of whole texels from the sample, so the index is the floor of the
// coordinate in texels, clamped to the edge the way a clamped sampler is.
fn tap(coord : vec2f, x : f32, y : f32) -> vec3f
{
    let size  = vec2f(constants.sourceSize.xy);
    let uv    = coord + constants.sourceSize.zw * vec2f(x, y);
    let index = clamp(vec2i(floor(uv * size)), vec2i(0), vec2i(size) - vec2i(1));

    return textureLoad(sourceTexture, index, 0).rgb;
}

// Difference between vector components.
fn df(A : vec4f, B : vec4f) -> vec4f
{
    return abs(A - B);
}

// Whether two vector components differ: a 1 or a 0 a component, which is what
// GLSL builds out of a bvec4 with notEqual().
fn diff(A : vec4f, B : vec4f) -> vec4f
{
    return abs(sign(A - B));
}

// Whether two vector components are closer to each other than the threshold.
// Upstream compares with lessThan(), so a difference of exactly the threshold
// is not close.
fn close(A : vec4f, B : vec4f) -> vec4f
{
    return 1.0 - step(vec4f(threshold), df(A, B));
}

// The boolean algebra the shader does with bvec4: with 0 and 1 as the two
// values, AND is a product, OR is the same clamped, and NOT is what is left.
fn bAnd(A : vec4f, B : vec4f) -> vec4f
{
    return A * B;
}

fn bOr(A : vec4f, B : vec4f) -> vec4f
{
    return min(A + B, vec4f(1.0));
}

fn bNot(A : vec4f) -> vec4f
{
    return vec4f(1.0) - A;
}

// Comparisons, as GLSL writes them: 1 where the comparison holds.
fn lt(A : vec4f, B : vec4f) -> vec4f { return 1.0 - step(B, A); } // A <  B
fn le(A : vec4f, B : vec4f) -> vec4f { return step(A, B); }       // A <= B

fn weighted_distance(a : vec4f, b : vec4f, c : vec4f, d : vec4f, e : vec4f,
                     f : vec4f, g : vec4f, h : vec4f) -> vec4f
{
    return df(a, b) + df(a, c) + df(d, e) + df(d, f) + 4.0 * df(g, h);
}

fn remapTo01(v : vec4f, high : vec4f) -> vec4f
{
    return v / high;
}

@fragment
fn fsMain(input : VertexOutput) -> @location(0) vec4f
{
    let coord = input.uv;

    //     A1 B1 C1
    //  A0  A  B  C C4
    //  D0  D  E  F F4
    //  G0  G  H  I I4
    //     G5 H5 I5

    let A1 = tap(coord, -1.0, -2.0);
    let B1 = tap(coord,  0.0, -2.0);
    let C1 = tap(coord,  1.0, -2.0);
    let A  = tap(coord, -1.0, -1.0);
    let B  = tap(coord,  0.0, -1.0);
    let C  = tap(coord,  1.0, -1.0);
    let D  = tap(coord, -1.0,  0.0);
    let E  = tap(coord,  0.0,  0.0);
    let F  = tap(coord,  1.0,  0.0);
    let G  = tap(coord, -1.0,  1.0);
    let H  = tap(coord,  0.0,  1.0);
    let I  = tap(coord,  1.0,  1.0);
    let G5 = tap(coord, -1.0,  2.0);
    let H5 = tap(coord,  0.0,  2.0);
    let I5 = tap(coord,  1.0,  2.0);
    let A0 = tap(coord, -2.0, -1.0);
    let D0 = tap(coord, -2.0,  0.0);
    let G0 = tap(coord, -2.0,  1.0);
    let C4 = tap(coord,  2.0, -1.0);
    let F4 = tap(coord,  2.0,  0.0);
    let I4 = tap(coord,  2.0,  1.0);

    // The four neighbours of E, in the order the swizzles below read them.
    let bdhf = vec4f(dot(B, rgbw), dot(D, rgbw), dot(H, rgbw), dot(F, rgbw));
    let cagi = vec4f(dot(C, rgbw), dot(A, rgbw), dot(G, rgbw), dot(I, rgbw));
    let e    = vec4f(dot(E, rgbw));
    let i4   = vec4f(dot(I4, rgbw), dot(C4, rgbw), dot(A0, rgbw), dot(G0, rgbw));
    let i5   = vec4f(dot(I5, rgbw), dot(C1, rgbw), dot(A1, rgbw), dot(G5, rgbw));
    let h5x  = vec4f(dot(H5, rgbw), dot(F4, rgbw), dot(B1, rgbw), dot(D0, rgbw));

    let b = bdhf.xzzx;
    let d = bdhf.yyww;
    let h = bdhf.zxxz;
    let f = bdhf.wwyy;
    let c = cagi.xwzy;
    let a = cagi.yzwx;
    let g = cagi.zyxw;
    let i = cagi.wxyz;

    let f4 = h5x.yyww;
    let h5 = h5x.xzzx;

    let r1 = bAnd(diff(e, f), diff(e, h));
    let r2 = bAnd(bNot(close(f, b)), bNot(close(f, c)));
    let r3 = bAnd(bNot(close(h, d)), bNot(close(h, g)));
    let r4 = bAnd(bNot(close(f, f4)), bNot(close(f, i4)));
    let r5 = bAnd(bNot(close(h, h5)), bNot(close(h, i5)));
    let r6 = bAnd(close(e, i), bOr(r4, r5));
    let r7 = bOr(close(e, g), close(e, c));

    let interp_restriction_lv1 = bAnd(r1, bOr(bOr(bOr(r2, r3), r6), r7));

    let edr = bAnd(lt(weighted_distance(e, c, g, i, h5, f4, h, f),
                      weighted_distance(h, d, i5, f, i4, b, e, i)),
                   interp_restriction_lv1);

    let px = le(df(e, f), df(e, h));

    // bin * mat4(edr.x, px.x, 0, 0, edr.y, px.y, 0, 0, ...) is
    // bin.x * edr + bin.y * px, which is 1 * edr + 2 * px.
    let info = bin.x * edr + bin.y * px;

    return vec4f(remapTo01(info, maximo));
}
