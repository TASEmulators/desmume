// PT SkyWalker541, the pixel transparency filter of the WebGPU renderer.
//
// The same filter as PT_SkyWalker541.hlsl and PT_SkyWalker541.glsl: it brings
// back the look of the reflective backing of a Game Boy screen, so that the
// unlit pixels of the frame show the grid of the panel behind them instead of
// the flat colour the emulator hands over, and the lit ones keep a little of
// it.  The OpenGL 3.3 renderer runs the upstream file as it is, from the
// resource PT_SKYWALKER541_GLSL; Direct3D 11 runs the HLSL port, and this is
// the WGSL one.
//
// Upstream carries no licence statement, only the credit below and the note
// that the LCD dot effect follows Themaister's dot shader, which is in the
// public domain.
//
//   PT SkyWalker541 v2.1.0, by SkyWalker541.
//
// The renderer has no parameter menu, so the settings are the defaults the
// #pragma parameter lines of the upstream file ship with, written out as
// constants; the branches that test them are kept, and naga folds them away.
// Upstream reads two textures - the corrected frame and the raw one - because
// RetroArch hands the raw frame to the palette stages.  The frame of this
// emulator carries no palette, so both taps read the same texture, which is
// what Direct3D 11 binds as its second texture for this shader.
//
// The taps go through textureLoad with integer coordinates, as they do in the
// other ports: upstream samples with the point sampler, and the texel that
// lands on is the floor of the coordinate in texels, clamped to the edge.

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

// The settings of the #pragma parameter lines, at their defaults.
const PT_GAME_SYSTEM                 = 2.0;   // GBC
const PT_MANUAL_WHITE_DETECTION_LEVEL = 0.85;
const PT_TRANSPARENT_PIXELS          = 2.0;   // All
const PT_TRANSPARENCY_AMOUNT         = 0.10;
const PT_MINIMUM_WHITE_TRANSPARENCY  = 0.20;
const PT_BACKING_COLOR               = 1.0;   // Pocket
const PT_BACKING_COLOR_STRENGTH      = 2.0;
const PT_GLARE_AND_SHEEN             = 0.0;   // Off
const PT_GLARE_STRENGTH              = 0.15;
const PT_GLARE_SPREAD                = 0.35;
const PT_SHEEN_STRENGTH              = 0.12;
const PT_SHEEN_BAND_DENSITY          = 2.0;
const PT_SHEEN_RIPPLE                = 0.0;
const PT_SHEEN_RIPPLE_SIZE           = 0.6;
const PT_DARKEN_PICTURE              = 0.0;   // Off
const PT_PIXEL_EFFECT                = 1.0;   // Grid
const PT_GAP_COLOR                   = 0.0;   // Backing
const PT_GRID_LINE_WIDTH             = 0.25;
const PT_GRID_LINE_OPACITY           = 0.50;
const PT_DOT_SIZE                    = 0.90;
const PT_DOT_EDGE_SOFTNESS           = 1.0;
const PT_DOT_GAP_OPACITY             = 0.50;
const PT_MIN_BRIGHTNESS_FOR_DOTS     = 0.15;
const PT_DROP_SHADOW_OPACITY         = 0.30;
const PT_SHADOW_DISTANCE             = 1.0;
const PT_SHADOW_DIRECTION            = 0.0;   // Down Right
const PT_BEZEL_EDGE_SHADOW           = 0.40;

// The light point of the glare and sheen feature, worked out in the vertex
// stage upstream: it sits at its home and does not drift, because the feature
// is off and its drift is what FrameCount feeds.
const shimmerLight = vec3f(0.5, 0.45, 0.0);

const LUMA_R = 0.2126;
const LUMA_G = 0.7152;
const LUMA_B = 0.0722;

// One texel of the frame, by index.  Upstream reads its textures with the
// point sampler, so the texel a coordinate lands on is the floor of it in
// texels, and clamping that index is what the clamped sampler does at the
// edge.
fn frameTexel(coord : vec2f) -> vec4f
{
    let size  = vec2i(constants.sourceSize.xy);
    let index = clamp(vec2i(floor(coord * constants.sourceSize.xy)), vec2i(0), size - vec2i(1));

    return textureLoad(sourceTexture, index, 0);
}

fn getBrightness(c : vec3f) -> f32
{
    return LUMA_R * c.r + LUMA_G * c.g + LUMA_B * c.b;
}

fn resolveThreshold() -> f32
{
    if (PT_GAME_SYSTEM < 0.5) { return PT_MANUAL_WHITE_DETECTION_LEVEL; }
    if (PT_GAME_SYSTEM < 1.5) { return 0.90; }
    if (PT_GAME_SYSTEM < 2.5) { return 0.85; }
    if (PT_GAME_SYSTEM < 3.5) { return 0.80; }
    return 0.75;
}

fn isWhitePixel(pixel : vec3f, brightness : f32, threshold : f32) -> f32
{
    let minCh = min(pixel.r, min(pixel.g, pixel.b));

    if (brightness > threshold && minCh > threshold * 0.9) { return 1.0; }
    return 0.0;
}

fn applyDarkFilter(c : vec3f, level : f32) -> vec3f
{
    let strength = level * 0.01;
    let luma     = getBrightness(c);
    let factor   = max(1.0 - strength * luma, 0.0);

    return c * factor;
}

fn noiseHash(p : vec2f) -> f32
{
    var p3 = fract(vec3f(p.xyx) * 0.1031);
    p3 = p3 + vec3f(dot(p3, p3.yzx + 33.33));
    return fract((p3.x + p3.y) * p3.z);
}

fn proceduralBackground(uv : vec2f) -> vec3f
{
    let grain  = noiseHash(uv * 128.0) * 0.875;
    let offset = (grain - 0.4375) * 0.065;

    return vec3f(0.478 + offset);
}

// Smooth fade-in from black.  The threshold is clamped away from zero because
// smoothstep() is undefined when both edges are equal.
fn blackLevelFade(rawLuma : f32) -> f32
{
    return smoothstep(0.0, max(PT_MIN_BRIGHTNESS_FOR_DOTS, 0.001), rawLuma);
}

// -- PIXEL EFFECT - MODE 1 : GRID ---------------------------------------------
// Thin lines along the borders between source pixels.  No texture samples.
// PT_GRID_LINE_WIDTH sets the line thickness (as a fraction of one source
// pixel); PT_GRID_LINE_OPACITY sets how strongly the gap color covers the
// pixel underneath the line.

fn applyGrid(color : vec3f, coord : vec2f, gapColor : vec3f) -> vec3f
{
    let cellUV = fract(coord * constants.sourceSize.xy);

    // Distance from this point to the nearest pixel border:
    // 0 on the border, 0.5 at the pixel centre.
    let edgeDist = min(min(cellUV.x, 1.0 - cellUV.x), min(cellUV.y, 1.0 - cellUV.y));

    // Each pixel holds half of the line on every side of it.  A small fixed
    // soft edge keeps the line from looking jagged.  Width 0 gives no line.
    let halfWidth = PT_GRID_LINE_WIDTH * 0.5;
    let lineMask  = 1.0 - smoothstep(halfWidth - 0.04, halfWidth, edgeDist);

    return mix(color, gapColor, lineMask * PT_GRID_LINE_OPACITY);
}

// -- PIXEL EFFECT - MODE 2 : LCD DOT ------------------------------------------
// Round dot with an adjustable edge and brightness-dependent dot sizing.
// Single sample.  Only the dot parameters, the minimum brightness and the
// shared gap color are evaluated.

fn applyLCDDot(color : vec3f, coord : vec2f, rawLuma : f32, gapColor : vec3f) -> vec3f
{
    // Gate: uses raw input frame brightness so color correction cannot cause
    // unlit pixels to appear lit.  Dot structure fades in smoothly as raw
    // pixel brightness rises above black.
    if (rawLuma < 0.01) { return color; }

    let lumaFade = blackLevelFade(rawLuma);
    let luma     = getBrightness(color);

    let cellUV = fract(coord * constants.sourceSize.xy);
    let delta  = cellUV - 0.5;
    let dist   = sqrt(dot(delta, delta));

    // Brightness-dependent radius: bright pixels get slightly larger dots,
    // dark pixels slightly smaller - consistent with physical LCD behaviour.
    let bloomBias = mix(0.0, 0.08, luma);
    let radius    = PT_DOT_SIZE * 0.5 + bloomBias;

    // Dot edge softness sets how wide the soft edge is, as a fraction of the
    // dot radius: 0 = hard-edged disk, 1 = the edge fades all the way in to
    // the centre.  A tiny minimum keeps a hard edge from being jagged.
    let softEdge = max(PT_DOT_EDGE_SOFTNESS * radius, 0.02);
    let dotMask  = clamp((radius - dist) / softEdge, 0.0, 1.0);

    // Outside the dot, cover the pixel with the gap color at
    // PT_DOT_GAP_OPACITY.  lumaFade ensures dark pixels blend back to the
    // original color.
    let dotResult = mix(color, gapColor, (1.0 - dotMask) * PT_DOT_GAP_OPACITY);
    let result    = mix(color, dotResult, lumaFade);

    return result;
}

// -- UNIFIED PIXEL EFFECT DISPATCHER ------------------------------------------
// Strictly gated - each branch only evaluates its own mode's parameters.

fn resolveGapColor(bg : vec3f) -> vec3f
{
    if (PT_GAP_COLOR < 0.5) { return bg; }            // Backing texture
    if (PT_GAP_COLOR < 1.5) { return vec3f(0.0); }    // Black
    return vec3f(1.0);                                // White
}

fn applyPixelEffect(color : vec3f, coord : vec2f, rawLuma : f32, bg : vec3f) -> vec3f
{
    if (PT_PIXEL_EFFECT < 0.5) { return color; }

    let gapColor = resolveGapColor(bg);

    if (PT_PIXEL_EFFECT < 1.5) { return applyGrid(color, coord, gapColor); }
    return applyLCDDot(color, coord, rawLuma, gapColor);
}

// -- BEZEL SHADOW -------------------------------------------------------------

fn applyBezelShadow(color : vec3f, coord : vec2f) -> vec3f
{
    if (PT_BEZEL_EDGE_SHADOW < 0.001) { return color; }

    var scale = 35.0;
    if      (PT_GAME_SYSTEM < 1.5) { scale = 22.0; }
    else if (PT_GAME_SYSTEM < 2.5) { scale = 35.0; }
    else if (PT_GAME_SYSTEM < 3.5) { scale = 70.0; }
    else                           { scale = 55.0; }

    // InputSize and TextureSize are the same here - the frame has no padding -
    // so upstream's division of one by the other is one.
    let gameUV = coord;
    let uv     = gameUV * 2.0 - 1.0;
    let shadow = clamp((1.0 - abs(uv.x)) * scale, 0.0, 1.0) *
                 clamp((1.0 - abs(uv.y)) * scale, 0.0, 1.0);

    return color * mix(1.0 - PT_BEZEL_EDGE_SHADOW, 1.0, shadow);
}

// -- GLARE AND SHEEN ----------------------------------------------------------
// Applied to the backing material only.  Two cheap effects around one light
// point (shimmerLight, worked out in the vertex shader upstream):
//   Sheen - a faint rainbow tint in bands around the light, built from three
//           phase-shifted triangle waves (no cos, no noise function).  The tint
//           has zero average, so it shifts the color without changing how
//           bright the backing is.
//   Glare - a soft white hotspot, a smooth bump 1 / (1 + q^2) that is wide and
//           flat in the middle and falls off quickly at the edge.

fn applyGlareAndSheen(backing : vec3f, coord : vec2f) -> vec3f
{
    // Upstream writes into its bg parameter; a WGSL parameter is a value, so
    // the work is done on a copy of it.
    var bg = backing;

    // Position inside the picture (0..1), and its offset from the light point.
    // x is stretched by the picture's aspect ratio so the hotspot stays round.
    let g  = coord * (constants.sourceSize.xy / constants.sourceSize.xy);
    let d  = (g - shimmerLight.xy) * vec2f(constants.sourceSize.x / constants.sourceSize.y, 1.0);
    let d2 = dot(d, d);

    if (PT_SHEEN_STRENGTH > 0.001)
    {
        var ph = sqrt(d2) * PT_SHEEN_BAND_DENSITY + shimmerLight.z;

        if (PT_SHEEN_RIPPLE > 0.001)
        {
            // Uneven bands: three smoothed triangle waves running at unrelated
            // angles and spacings are added up and used to push the band phase
            // around, so the stripes look like uneven film instead of a
            // regular pattern.  No noise function, no trig.
            let f  = 3.0 / PT_SHEEN_RIPPLE_SIZE;
            let u  = g * f;
            var t3 = abs(fract(vec3f(u.x * 1.00 + u.y * 0.37,
                                     u.y * 1.13 - u.x * 0.61 + 0.31,
                                     u.x * 0.43 - u.y * 1.71 + 0.72)) * 2.0 - 1.0);
            t3 = t3 * t3 * (3.0 - 2.0 * t3);
            ph += (t3.x + t3.y + t3.z - 1.5) * PT_SHEEN_RIPPLE * 0.45;
        }

        let h = fract(ph + vec3f(0.0, 0.3333, 0.6667));
        let w = abs(h * 2.0 - 1.0);
        var r = w * w * (3.0 - 2.0 * w);
        r = r - vec3f(dot(r, vec3f(0.3333)));
        bg = bg + r * PT_SHEEN_STRENGTH;
    }

    if (PT_GLARE_STRENGTH > 0.001)
    {
        let q     = d2 / (PT_GLARE_SPREAD * PT_GLARE_SPREAD);
        let glare = PT_GLARE_STRENGTH / (1.0 + q * q);
        bg = mix(bg, vec3f(1.0), min(glare, 0.85));
    }

    return clamp(bg, vec3f(0.0), vec3f(1.0));
}

// -- BACKING MATERIAL ---------------------------------------------------------
// The color of the screen's backing at this pixel: procedural grain, a drop
// shadow cast by dark neighbouring pixels, the backing color tint, and the
// optional glare and sheen.  It shows through transparent pixels and, when Gap
// color is Backing, it also fills the gaps of the pixel effects.

fn computeBacking(threshold : f32, TEX0 : vec2f) -> vec3f
{
    // Procedural backing texture.
    var bg = proceduralBackground(TEX0);

    // Drop shadow - single tap on raw frame, before palette tint.
    if (PT_DROP_SHADOW_OPACITY > 0.001)
    {
        var shadowDir : vec2f;
        if      (PT_SHADOW_DIRECTION < 0.5) { shadowDir = vec2f(-1.0,  1.0); }  // Down Right
        else if (PT_SHADOW_DIRECTION < 1.5) { shadowDir = vec2f( 1.0,  1.0); }  // Down Left
        else if (PT_SHADOW_DIRECTION < 2.5) { shadowDir = vec2f(-1.0, -1.0); }  // Up Right
        else                                { shadowDir = vec2f( 1.0, -1.0); }  // Up Left

        let shadowPos      = TEX0 + shadowDir * PT_SHADOW_DISTANCE * constants.sourceSize.zw;
        let shadowSrc      = frameTexel(shadowPos).rgb;
        let shadowBright   = getBrightness(shadowSrc);
        let shadowSrcWhite = isWhitePixel(shadowSrc, shadowBright, threshold);

        if (shadowSrcWhite < 0.5)
        {
            let shadowStrength = (1.0 - shadowBright) * PT_DROP_SHADOW_OPACITY;
            bg = mix(bg, bg * 0.2, shadowStrength);
        }
    }

    // Palette tint - after shadow.
    if (PT_BACKING_COLOR > 0.5)
    {
        var tint : vec3f;
        if      (PT_BACKING_COLOR < 1.5) { tint = vec3f(0.651, 0.675, 0.518); }
        else if (PT_BACKING_COLOR < 2.5) { tint = vec3f(0.737, 0.737, 0.737); }
        else                             { tint = vec3f(1.0,   1.0,   1.0  ); }

        let tinted = clamp(tint + (bg * 2.0 - vec3f(1.0)), vec3f(0.0), vec3f(1.0));
        bg = mix(bg, tinted, PT_BACKING_COLOR_STRENGTH);
    }

    // Glare and sheen - last, on top of the tinted backing.
    if (PT_GLARE_AND_SHEEN > 0.5)
    {
        bg = applyGlareAndSheen(bg, TEX0);
    }

    return bg;
}

// -- MAIN ---------------------------------------------------------------------
@fragment
fn fsMain(input : VertexOutput) -> @location(0) vec4f
{
    // Sample corrected frame and raw input frame.  The two are the same
    // texture here, and the coordinate upstream corrects with the sizes of the
    // two - TextureSize / InputSize and OrigInputSize / OrigTextureSize - is
    // the one the sample came in at, those not being padded or resized.
    let lcd      = frameTexel(input.uv);
    var pixel    = lcd.rgb;
    let rawPixel = frameTexel(input.uv).rgb;

    // Cache brightness.
    var pixBrightness = getBrightness(pixel);
    let rawBrightness = getBrightness(rawPixel);

    // Dark color filter - on corrected frame.
    if (PT_DARKEN_PICTURE > 0.5)
    {
        pixel         = applyDarkFilter(pixel, PT_DARKEN_PICTURE);
        pixBrightness = getBrightness(pixel);
    }

    // White detection - on raw frame.
    let threshold = resolveThreshold();
    let isWhite   = isWhitePixel(rawPixel, rawBrightness, threshold);

    // White mode - non-white pixels exit early, post effects still applied.
    if (PT_TRANSPARENT_PIXELS < 0.5 && isWhite < 0.5)
    {
        // These pixels get no transparency, but the pixel effect still runs on
        // them.  Build the backing only when the gaps actually use it (Gap
        // color = Backing), and not for pure black pixels that LCD Dot skips
        // anyway.
        var gapBacking = vec3f(0.0);
        if (PT_PIXEL_EFFECT > 0.5 && PT_GAP_COLOR < 0.5 &&
            !(PT_PIXEL_EFFECT > 1.5 && rawBrightness < 0.01))
        {
            gapBacking = computeBacking(threshold, input.uv);
        }

        var result = applyPixelEffect(pixel, input.uv, rawBrightness, gapBacking);
        result = applyBezelShadow(result, input.uv);

        return vec4f(result, lcd.a);
    }

    // Backing material (grain, shadow, tint).
    let bg = computeBacking(threshold, input.uv);

    // Transparency alpha.
    var alpha = 0.0;
    if (PT_TRANSPARENT_PIXELS < 0.5)
    {
        alpha = clamp((pixBrightness / 3.0) + PT_TRANSPARENCY_AMOUNT, 0.0, 1.0);
    }
    else if (PT_TRANSPARENT_PIXELS < 1.5)
    {
        alpha = clamp(PT_TRANSPARENCY_AMOUNT * pixBrightness * 2.665, 0.0, 1.0);
    }
    else
    {
        alpha = clamp((pixBrightness / 3.0) + PT_TRANSPARENCY_AMOUNT, 0.0, 1.0);
    }

    if (isWhite > 0.5)
    {
        alpha = max(alpha, PT_MINIMUM_WHITE_TRANSPARENCY);
    }

    var result = mix(pixel, bg, alpha);

    // Post-blend pixel effect and bezel - applied last.
    result = applyPixelEffect(result, input.uv, rawBrightness, bg);
    result = applyBezelShadow(result, input.uv);

    return vec4f(result, lcd.a);
}
