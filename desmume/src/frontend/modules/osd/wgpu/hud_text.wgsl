// OSD bitmap-text rasterizer for the WebGPU OSD backend (stage 3).
//
// One instance per glyph. The vertex stage expands a unit quad to the glyph's
// pixel rectangle and picks the glyph's rectangle in the R8 coverage atlas; the
// fragment stage multiplies that coverage by the run's straight RGBA color and
// returns a premultiplied color for the pipeline's One / OneMinusSrcAlpha
// blend state.
//
// The atlas stores the same 1bpp bitmap fonts AGG's glyph_raster_bin used, so
// Nearest sampling reproduces the original pixel look. Coordinates are pixels,
// origin at the top-left of the 256x384 HUD target.
//
// Keep this file in sync with kGlyphShader in wgpu_draw.cpp. The self-test
// compares the two.

struct Uniforms
{
	size: vec4f,       // target width, height, 0, 0
};

@group(0) @binding(0) var<uniform> uniforms: Uniforms;
@group(0) @binding(1) var atlas: texture_2d<f32>;
@group(0) @binding(2) var atlasSampler: sampler;

struct VSOut
{
	@builtin(position) position: vec4f,
	@location(0) uv: vec2f,
	@location(1) color: vec4f,
};

@vertex
fn vsGlyph(@builtin(vertex_index) vertexIndex: u32,
           @location(0) posSize: vec4f,
           @location(1) uvRect: vec4f,
           @location(2) color: vec4f) -> VSOut
{
	var corners = array<vec2f, 6>(
		vec2f(0.0, 0.0), vec2f(1.0, 0.0), vec2f(1.0, 1.0),
		vec2f(0.0, 0.0), vec2f(1.0, 1.0), vec2f(0.0, 1.0));
	let c = corners[vertexIndex];
	let p = posSize.xy + c * posSize.zw;

	var result: VSOut;
	result.position = vec4f(
		p.x / uniforms.size.x * 2.0 - 1.0,
		1.0 - p.y / uniforms.size.y * 2.0,
		0.0,
		1.0);
	result.uv = vec2f(mix(uvRect.x, uvRect.z, c.x),
	                  mix(uvRect.y, uvRect.w, c.y));
	result.color = color;
	return result;
}

@fragment
fn fsGlyph(input: VSOut) -> @location(0) vec4f
{
	let cover = textureSampleLevel(atlas, atlasSampler, input.uv, 0.0).r;
	let alpha = input.color.a * cover;
	return vec4f(input.color.rgb * alpha, alpha);
}
