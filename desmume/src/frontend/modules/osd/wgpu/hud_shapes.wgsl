// OSD shape rasterizer for the WebGPU OSD backend (stage 2).
//
// One instance per primitive. The vertex stage expands the instance's bounding
// box into two triangles; the fragment stage evaluates a signed distance field
// and returns a premultiplied BGRA color for the pipeline's
// One / OneMinusSrcAlpha blend state.
//
// Coordinates are pixels, origin at the top-left of the 256x384 HUD target,
// exactly like the AggDrawTarget they replace. Do not round or offset them:
// agg_osd.cpp already aligns its coordinates to half pixels itself.
//
// Keep this file in sync with kShapeShader in wgpu_draw.cpp. The self-test
// compares the two.

struct Instance
{
	bbox: vec4f,       // minX, minY, maxX, maxY after stroke expansion
	shape: vec4f,      // rect: center + half size; ellipse: center + radii;
	                   // line: x0, y0, x1, y1
	params: vec4f,     // radius, strokeWidth, shapeType, polygon point count
	fill0: vec4f,      // straight RGBA
	fill1: vec4f,
	line0: vec4f,
	line1: vec4f,
	fillGrad: vec4f,   // x0, y0, x1, y1
	lineGrad: vec4f,
	flags: vec4f,      // fillGradient, lineGradient, hasFill, hasStroke
	pts: array<vec2f, 64>,
};

struct Uniforms
{
	size: vec4f,       // target width, height, 0, 0
};

@group(0) @binding(0) var<storage, read> instances: array<Instance>;
@group(0) @binding(1) var<uniform> uniforms: Uniforms;

struct VSOut
{
	@builtin(position) position: vec4f,
	@location(0) pixel: vec2f,
	@location(1) @interpolate(flat) index: u32,
};

@vertex
fn vsShape(@builtin(vertex_index) vertexIndex: u32,
           @builtin(instance_index) instanceIndex: u32) -> VSOut
{
	let b = instances[instanceIndex].bbox;

	var corners = array<vec2f, 4>(
		vec2f(b.x, b.y),
		vec2f(b.z, b.y),
		vec2f(b.z, b.w),
		vec2f(b.x, b.w));
	var order = array<u32, 6>(0u, 1u, 2u, 0u, 2u, 3u);

	var result: VSOut;
	let p = corners[order[vertexIndex]];
	result.position = vec4f(
		p.x / uniforms.size.x * 2.0 - 1.0,
		1.0 - p.y / uniforms.size.y * 2.0,
		0.0,
		1.0);
	result.pixel = p;
	result.index = instanceIndex;
	return result;
}

fn sdBox(p: vec2f, c: vec2f, h: vec2f) -> f32
{
	let q = abs(p - c) - h;
	return length(max(q, vec2f(0.0))) + min(max(q.x, q.y), 0.0);
}

fn sdRoundBox(p: vec2f, c: vec2f, h: vec2f, r: f32) -> f32
{
	let q = abs(p - c) - h + vec2f(r);
	return length(max(q, vec2f(0.0))) + min(max(q.x, q.y), 0.0) - r;
}

// Radial distance divided by the gradient magnitude of the implicit function,
// so the flat sides of a non-circular ellipse get the right coverage. For a
// circle this reduces to (length - 1) * r, which is exact.
fn sdEllipse(p: vec2f, c: vec2f, r: vec2f) -> f32
{
	let rr = max(r, vec2f(1e-4));
	let q = (p - c) / rr;
	let l = length(q);
	let g = max(length(q / rr), 1e-6);
	return (l - 1.0) * l / g;
}

fn sdSegment(p: vec2f, a: vec2f, b: vec2f) -> f32
{
	let pa = p - a;
	let ba = b - a;
	let h = clamp(dot(pa, ba) / max(dot(ba, ba), 1e-6), 0.0, 1.0);
	return length(pa - ba * h);
}

// The usual even-odd polygon distance. agg_osd.cpp's D-pad outline is concave,
// so a convex test would be wrong.
fn sdPolygon(p: vec2f, n: u32, inst: Instance) -> f32
{
	var d = dot(p - inst.pts[0], p - inst.pts[0]);
	var s = 1.0;

	for (var i = 0u; i < n; i = i + 1u)
	{
		let j = (i + n - 1u) % n;
		let vi = inst.pts[i];
		let vj = inst.pts[j];
		let e = vj - vi;
		let w = p - vi;
		let b = w - e * clamp(dot(w, e) / max(dot(e, e), 1e-6), 0.0, 1.0);
		d = min(d, dot(b, b));

		let c1 = p.y >= vi.y;
		let c2 = p.y < vj.y;
		let c3 = e.x * w.y > e.y * w.x;

		if ((c1 && c2 && c3) || (!c1 && !c2 && !c3))
		{
			s = -s;
		}
	}

	return s * sqrt(d);
}

// Open path: distance to the segments only, no closing edge. Used for Lua's
// stroked arcs and quadratic curves, which AGG draws without a chord.
fn sdPolyline(p: vec2f, n: u32, inst: Instance) -> f32
{
	var d = 1e20;

	for (var i = 1u; i < n; i = i + 1u)
	{
		d = min(d, sdSegment(p, inst.pts[i - 1u], inst.pts[i]));
	}

	return d;
}

fn shapeDistance(p: vec2f, inst: Instance) -> f32
{
	let shapeType = i32(inst.params.z + 0.5);
	let c = inst.shape.xy;

	if (shapeType == 1)
	{
		return sdRoundBox(p, c, inst.shape.zw, inst.params.x);
	}
	if (shapeType == 2)
	{
		return sdEllipse(p, c, inst.shape.zw);
	}
	if (shapeType == 3)
	{
		return sdPolygon(p, u32(inst.params.w + 0.5), inst);
	}
	if (shapeType == 4)
	{
		return sdSegment(p, inst.shape.xy, inst.shape.zw);
	}
	if (shapeType == 5)
	{
		return sdPolyline(p, u32(inst.params.w + 0.5), inst);
	}

	return sdBox(p, c, inst.shape.zw);
}

fn gradientColor(c0: vec4f, c1: vec4f, endpoints: vec4f, p: vec2f, enabled: bool) -> vec4f
{
	if (!enabled)
	{
		return c0;
	}

	let d = endpoints.zw - endpoints.xy;
	let dd = dot(d, d);
	var t = 0.0;

	if (dd > 1e-6)
	{
		t = clamp(dot(p - endpoints.xy, d) / dd, 0.0, 1.0);
	}

	return mix(c0, c1, t);
}

// AGG's rasterizer antialiases over roughly one pixel; match that width.
fn coverage(d: f32) -> f32
{
	return clamp(0.5 - d, 0.0, 1.0);
}

@fragment
fn fsShape(input: VSOut) -> @location(0) vec4f
{
	let inst = instances[input.index];
	let d = shapeDistance(input.pixel, inst);
	let halfStroke = inst.params.y * 0.5;

	var fillAlpha = 0.0;
	var fillColor = vec4f(0.0);

	if (inst.flags.z > 0.5)
	{
		fillColor = gradientColor(inst.fill0, inst.fill1, inst.fillGrad, input.pixel,
		                          inst.flags.x > 0.5);
		fillAlpha = fillColor.a * coverage(d);
	}

	var strokeAlpha = 0.0;
	var strokeColor = vec4f(0.0);

	if (inst.flags.w > 0.5)
	{
		strokeColor = gradientColor(inst.line0, inst.line1, inst.lineGrad, input.pixel,
		                            inst.flags.y > 0.5);
		strokeAlpha = strokeColor.a * coverage(abs(d) - halfStroke);
	}

	// Fill and stroke both feed a single premultiplied source-over result, so a
	// FillAndStroke primitive costs one fragment shader evaluation.
	let fill = vec4f(fillColor.rgb * fillAlpha, fillAlpha);
	let stroke = vec4f(strokeColor.rgb * strokeAlpha, strokeAlpha);

	return vec4f(stroke.rgb + fill.rgb * (1.0 - strokeAlpha),
	             strokeAlpha + fillAlpha * (1.0 - strokeAlpha));
}
