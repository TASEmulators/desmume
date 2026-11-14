/*
	Copyright (C) 2026 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "wgpu_osd_draw.h"

#include <math.h>
#include <string.h>
#include <algorithm>

static const double kWGPUOSDPi = 3.14159265358979323846;
static const int kWGPUOSDMaxShapePoints = 64;

// ---------------------------------------------------------------------------
// Construction / frame lifecycle
// ---------------------------------------------------------------------------

WGPUOSDDrawTarget::WGPUOSDDrawTarget(int width, int height)
	: m_width(0)
	, m_height(0)
	, m_stride(0)
	, m_hasCpuLayer(false)
	, m_hasFill(true)
	, m_hasStroke(true)
	, m_fillGradient(false)
	, m_lineGradient(false)
	, m_fill0(255, 255, 255, 255)
	, m_fill1(255, 255, 255, 255)
	, m_line0(0, 0, 0, 255)
	, m_line1(0, 0, 0, 255)
	, m_gradientX0(0.0), m_gradientY0(0.0), m_gradientX1(0.0), m_gradientY1(0.0)
	, m_lineGradientX0(0.0), m_lineGradientY0(0.0), m_lineGradientX1(0.0), m_lineGradientY1(0.0)
	, m_strokeWidth(1.0)
	, m_masterAlpha(1.0)
	, m_fontData(NULL)
	, m_blendMode(Agg2DBase::BlendAlpha)
	, m_imageBlendMode(Agg2DBase::BlendAlpha)
	, m_imageBlendColor(0, 0, 0, 255)
	, m_lineCap(Agg2DBase::CAP_BUTT)
	, m_lineJoin(Agg2DBase::JOIN_MITER)
	, m_imageFilter(Agg2DBase::NoFilter)
	, m_imageResample(Agg2DBase::NoResample)
	, m_gamma(1.0)
	, m_fillEvenOdd(false)
{
	memset(&m_transformations, 0, sizeof(m_transformations));
	m_transformations.affineMatrix[0] = 1.0;
	m_transformations.affineMatrix[3] = 1.0;

	m_clipBox.x1 = 0.0;
	m_clipBox.y1 = 0.0;
	m_clipBox.x2 = (double)width - 1.0;
	m_clipBox.y2 = (double)height - 1.0;

	Resize(width, height);
}

WGPUOSDDrawTarget::~WGPUOSDDrawTarget()
{
	m_renderer.kill();
}

void WGPUOSDDrawTarget::Resize(int width, int height)
{
	if ((width <= 0) || (height <= 0))
	{
		return;
	}

	if ((width != m_width) || (height != m_height) || m_pixels.empty())
	{
		m_pixels.assign((size_t)width * (size_t)height, 0);

		m_width = width;
		m_height = height;
		m_stride = width * 4;

		m_rbuf.attach((unsigned char *)m_pixels.data(),
		              (unsigned)width, (unsigned)height, m_stride);

		// The GPU target texture is size specific.  begin() is idempotent, so
		// releasing here only means the next beginGPU() rebuilds it.  kill() is
		// NULL-guarded and safe on a renderer that never began (constructor).
		m_renderer.kill();

		m_clipBox.x2 = (double)width - 1.0;
		m_clipBox.y2 = (double)height - 1.0;

		// Recorded geometry is in the old coordinate space, so it cannot
		// survive a real resize.
		m_shapes.clear();
		m_points.clear();
		m_textRuns.clear();
		m_textBytes.clear();
		m_hasCpuLayer = false;
		undirty();
	}
}

void WGPUOSDDrawTarget::setDrawTargetDims(agg::int8u *, int width, int height, int)
{
	// The GPU backend owns its CPU layer.  External buffers are ignored so that
	// buf(), pixelBuffer() and the renderer always agree on one allocation.
	Resize(width, height);
}

void WGPUOSDDrawTarget::attach(unsigned char *, unsigned width, unsigned height, int)
{
	Resize((int)width, (int)height);
}

uint32_t *WGPUOSDDrawTarget::pixelBuffer()
{
	dirty();
	m_hasCpuLayer = true;
	return m_pixels.empty() ? NULL : m_pixels.data();
}

void WGPUOSDDrawTarget::FillPixels(const WGPUOSDColor &c)
{
	if (m_pixels.empty())
	{
		return;
	}

	const unsigned a = (c.a > 255) ? 255u : c.a;
	const unsigned r = ((c.r > 255 ? 255u : c.r) * a) / 255u;
	const unsigned g = ((c.g > 255 ? 255u : c.g) * a) / 255u;
	const unsigned b = ((c.b > 255 ? 255u : c.b) * a) / 255u;
	const uint32_t pixel = (a << 24) | (r << 16) | (g << 8) | b;

	for (size_t i = 0; i < m_pixels.size(); i++)
	{
		m_pixels[i] = pixel;
	}

	m_hasCpuLayer = true;
	dirty();
}

void WGPUOSDDrawTarget::clear()
{
	m_shapes.clear();
	m_points.clear();
	m_textRuns.clear();
	m_textBytes.clear();

	if (!m_pixels.empty())
	{
		memset(m_pixels.data(), 0, m_pixels.size() * sizeof(uint32_t));
	}

	m_hasCpuLayer = false;
	undirty();
}

void WGPUOSDDrawTarget::clearAll(AggColor c) { FillPixels(WGPUOSDColor(c)); }
void WGPUOSDDrawTarget::clearAll(unsigned r, unsigned g, unsigned b, unsigned a)
{
	FillPixels(WGPUOSDColor(r, g, b, a));
}
void WGPUOSDDrawTarget::clearClipBox(AggColor c) { FillPixels(WGPUOSDColor(c)); }
void WGPUOSDDrawTarget::clearClipBox(unsigned r, unsigned g, unsigned b, unsigned a)
{
	FillPixels(WGPUOSDColor(r, g, b, a));
}

bool WGPUOSDDrawTarget::hasContent() const
{
	return !m_shapes.empty() || !m_textRuns.empty() || m_hasCpuLayer;
}

// ---------------------------------------------------------------------------
// GPU side
// ---------------------------------------------------------------------------

bool WGPUOSDDrawTarget::beginGPU(void *instance, void *device, void *queue)
{
	return m_renderer.begin(instance, device, queue, m_width, m_height);
}

bool WGPUOSDDrawTarget::setFonts(const WGPUOSDFont *fonts, int count)
{
	m_fonts.clear();

	if ((fonts != NULL) && (count > 0))
	{
		m_fonts.reserve((size_t)count);
		for (int i = 0; i < count; i++)
		{
			if ((fonts[i].name == NULL) || (fonts[i].data == NULL))
			{
				continue;
			}

			WGPUOSDFontEntry entry;
			entry.name = fonts[i].name;
			entry.data = fonts[i].data;
			m_fonts.push_back(entry);
		}
	}

	if (m_fontData == NULL)
	{
		// Agg_init() uses verdana18_bold as the default HUD font.
		setFont("verdana18_bold");
	}

	return m_renderer.setFonts(fonts, count);
}

bool WGPUOSDDrawTarget::render()
{
	if (!m_renderer.isReady())
	{
		return false;
	}

	if ((m_renderer.width() != m_width) || (m_renderer.height() != m_height))
	{
		return false;
	}

	// The CPU layer is uploaded every frame, matching the old AGG-based
	// recorder.  It is zeroed by clear(), so an unused layer is a no-op blend.
	return m_renderer.render(m_pixels.empty() ? NULL : m_pixels.data(), m_stride,
	                         m_shapes.empty() ? NULL : &m_shapes[0], (int)m_shapes.size(),
	                         m_points.empty() ? NULL : &m_points[0], (int)m_points.size(),
	                         m_textRuns.empty() ? NULL : &m_textRuns[0], (int)m_textRuns.size(),
	                         m_textBytes.empty() ? NULL : &m_textBytes[0], (int)m_textBytes.size());
}

bool WGPUOSDDrawTarget::readPixels(uint8_t *dst, int dstStrideBytes)
{
	return m_renderer.readPixels(dst, dstStrideBytes);
}

void *WGPUOSDDrawTarget::textureView() const
{
	return m_renderer.textureView();
}

// ---------------------------------------------------------------------------
// Paint state
// ---------------------------------------------------------------------------

uint32_t WGPUOSDDrawTarget::PackColor(const WGPUOSDColor &c) const
{
	double alpha = (double)c.a * m_masterAlpha;
	if (alpha < 0.0) alpha = 0.0;
	if (alpha > 255.0) alpha = 255.0;

	return ((uint32_t)c.r << 24) |
	       ((uint32_t)c.g << 16) |
	       ((uint32_t)c.b << 8) |
	       (uint32_t)(alpha + 0.5);
}

void WGPUOSDDrawTarget::fillColor(AggColor c) { fillColor(c.r, c.g, c.b, c.a); }

void WGPUOSDDrawTarget::fillColor(unsigned r, unsigned g, unsigned b, unsigned a)
{
	// Agg2D re-enables the fill here.  Scripts that call noFill() must start
	// filling again as soon as a color is set (wgpu-agg.md 18.6).
	m_hasFill = true;
	m_fillGradient = false;
	m_fill0 = m_fill1 = WGPUOSDColor(r, g, b, a);
}

void WGPUOSDDrawTarget::noFill()
{
	m_hasFill = false;
	m_fillGradient = false;
}

AggColor WGPUOSDDrawTarget::fillColor() { return m_fill0.toAgg(); }

void WGPUOSDDrawTarget::lineColor(AggColor c) { lineColor(c.r, c.g, c.b, c.a); }

void WGPUOSDDrawTarget::lineColor(unsigned r, unsigned g, unsigned b, unsigned a)
{
	m_hasStroke = true;
	m_lineGradient = false;
	m_line0 = m_line1 = WGPUOSDColor(r, g, b, a);
}

void WGPUOSDDrawTarget::noLine()
{
	m_hasStroke = false;
	m_lineGradient = false;
}

AggColor WGPUOSDDrawTarget::lineColor() { return m_line0.toAgg(); }

void WGPUOSDDrawTarget::lineWidth(double w)
{
	m_strokeWidth = (w > 0.0) ? w : 0.0;
}
double WGPUOSDDrawTarget::lineWidth() { return m_strokeWidth; }

void WGPUOSDDrawTarget::masterAlpha(double a) { m_masterAlpha = a; }
double WGPUOSDDrawTarget::masterAlpha() { return m_masterAlpha; }

void WGPUOSDDrawTarget::fillLinearGradient(double x1, double y1, double x2, double y2,
                                           AggColor c1, AggColor c2, double profile)
{
	(void)profile; // only the default AGG profile is supported
	m_hasFill = true;
	m_fillGradient = true;
	m_fill0 = WGPUOSDColor(c1);
	m_fill1 = WGPUOSDColor(c2);
	m_gradientX0 = x1; m_gradientY0 = y1;
	m_gradientX1 = x2; m_gradientY1 = y2;
}

void WGPUOSDDrawTarget::lineLinearGradient(double x1, double y1, double x2, double y2,
                                           AggColor c1, AggColor c2, double profile)
{
	(void)profile;
	m_hasStroke = true;
	m_lineGradient = true;
	m_line0 = WGPUOSDColor(c1);
	m_line1 = WGPUOSDColor(c2);
	m_lineGradientX0 = x1; m_lineGradientY0 = y1;
	m_lineGradientX1 = x2; m_lineGradientY1 = y2;
}

void WGPUOSDDrawTarget::setFont(const std::string &name)
{
	for (size_t i = 0; i < m_fonts.size(); i++)
	{
		if (m_fonts[i].name == name)
		{
			m_fontData = m_fonts[i].data;
			return;
		}
	}

	// Unknown names keep the previous font, matching Agg2D::setFont/lookupFont.
}

// ---------------------------------------------------------------------------
// Recording
// ---------------------------------------------------------------------------

void WGPUOSDDrawTarget::RecordShape(uint32_t type,
                                     double x0, double y0, double x1, double y1,
                                     double radius)
{
	WGPUOSDShape s;
	memset(&s, 0, sizeof(s));

	s.type = type;
	s.hasFill = m_hasFill ? 1 : 0;
	s.hasStroke = m_hasStroke ? 1 : 0;
	s.fillGradient = m_fillGradient ? 1 : 0;
	s.lineGradient = m_lineGradient ? 1 : 0;
	s.lineWidth = m_strokeWidth;

	s.x0 = x0; s.y0 = y0; s.x1 = x1; s.y1 = y1;
	s.radius = radius;

	s.fillX0 = m_gradientX0; s.fillY0 = m_gradientY0;
	s.fillX1 = m_gradientX1; s.fillY1 = m_gradientY1;
	s.lineX0 = m_lineGradientX0; s.lineY0 = m_lineGradientY0;
	s.lineX1 = m_lineGradientX1; s.lineY1 = m_lineGradientY1;

	s.fill0 = PackColor(m_fill0);
	s.fill1 = PackColor(m_fill1);
	s.line0 = PackColor(m_line0);
	s.line1 = PackColor(m_line1);

	// Open paths must never be filled.
	if ((type == WGPUOSDShape_Line) || (type == WGPUOSDShape_Polyline))
	{
		s.hasFill = 0;
	}

	m_shapes.push_back(s);
	dirty();
}

void WGPUOSDDrawTarget::RecordPolygon(const WGPUOSDPoint *points, int count)
{
	if ((points == NULL) || (count < 3) || (count > kWGPUOSDMaxShapePoints))
	{
		return;
	}

	RecordShape(WGPUOSDShape_Polygon, 0.0, 0.0, 0.0, 0.0, 0.0);
	WGPUOSDShape &s = m_shapes.back();
	s.pointOffset = (uint32_t)m_points.size();
	s.pointCount = (uint32_t)count;
	m_points.insert(m_points.end(), points, points + count);
}

void WGPUOSDDrawTarget::RecordPolyline(const WGPUOSDPoint *points, int count)
{
	if ((points == NULL) || (count < 2) || (count > kWGPUOSDMaxShapePoints))
	{
		return;
	}

	RecordShape(WGPUOSDShape_Polyline, 0.0, 0.0, 0.0, 0.0, 0.0);
	WGPUOSDShape &s = m_shapes.back();
	s.pointOffset = (uint32_t)m_points.size();
	s.pointCount = (uint32_t)count;
	m_points.insert(m_points.end(), points, points + count);
}

void WGPUOSDDrawTarget::RecordText(double dstX, double dstY, const std::string &str)
{
	WGPUOSDTextRun run;
	memset(&run, 0, sizeof(run));

	const WGPUOSDColor &c = m_line0;
	run.font = m_fontData;
	run.x = dstX;
	run.y = dstY;   // PackGlyphs computes yTop = int(y) - base
	run.color = ((uint32_t)c.r << 24) | ((uint32_t)c.g << 16) |
	            ((uint32_t)c.b << 8)  | (uint32_t)c.a;
	run.charOffset = (uint32_t)m_textBytes.size();
	run.charCount = (uint32_t)str.size();

	m_textBytes.insert(m_textBytes.end(), str.begin(), str.end());
	m_textRuns.push_back(run);
	dirty();
}

void WGPUOSDDrawTarget::renderText(double dstX, double dstY, const std::string &str)
{
	// There is no CPU text rasterizer any more.  An unregistered font is
	// skipped rather than silently drawn with the wrong glyph set.
	if (str.empty() || (m_fontData == NULL) || !m_renderer.hasFont(m_fontData))
	{
		return;
	}

	RecordText(dstX, dstY, str);
}

void WGPUOSDDrawTarget::renderTextDropshadowed(double dstX, double dstY, const std::string &str)
{
	// Same nine-draw sequence as AggDrawTarget::renderTextDropshadowed.  Each
	// call records one text run.  lineColor() re-enables the stroke and clears
	// the line gradient, so the whole stroke state is saved and restored
	// (wgpu-agg.md 18.6).
	const WGPUOSDColor old0 = m_line0;
	const WGPUOSDColor old1 = m_line1;
	const bool oldStroke = m_hasStroke;
	const bool oldGradient = m_lineGradient;
	const double oldGX0 = m_lineGradientX0, oldGY0 = m_lineGradientY0;
	const double oldGX1 = m_lineGradientX1, oldGY1 = m_lineGradientY1;

	if (old0.r + old0.g + old0.b < 192)
	{
		lineColor(255 - old0.r, 255 - old0.g, 255 - old0.b);
	}
	else
	{
		lineColor(0, 0, 0);
	}

	renderText(dstX - 1, dstY - 1, str);
	renderText(dstX,     dstY - 1, str);
	renderText(dstX + 1, dstY - 1, str);
	renderText(dstX - 1, dstY,     str);
	renderText(dstX + 1, dstY,     str);
	renderText(dstX - 1, dstY + 1, str);
	renderText(dstX,     dstY + 1, str);
	renderText(dstX + 1, dstY + 1, str);

	m_line0 = old0;
	m_line1 = old1;
	m_hasStroke = oldStroke;
	m_lineGradient = oldGradient;
	m_lineGradientX0 = oldGX0; m_lineGradientY0 = oldGY0;
	m_lineGradientX1 = oldGX1; m_lineGradientY1 = oldGY1;
	renderText(dstX, dstY, str);
}

// ---------------------------------------------------------------------------
// Primitives
// ---------------------------------------------------------------------------

void WGPUOSDDrawTarget::line(double x1, double y1, double x2, double y2)
{
	if (!m_hasStroke) return;
	RecordShape(WGPUOSDShape_Line, x1, y1, x2, y2, 0.0);
}

void WGPUOSDDrawTarget::rectangle(double x1, double y1, double x2, double y2)
{
	RecordShape(WGPUOSDShape_Rectangle, x1, y1, x2, y2, 0.0);
}

void WGPUOSDDrawTarget::roundedRect(double x1, double y1, double x2, double y2, double r)
{
	RecordShape(WGPUOSDShape_RoundedRect, x1, y1, x2, y2, r);
}

void WGPUOSDDrawTarget::roundedRect(double x1, double y1, double x2, double y2,
                                     double rx, double ry)
{
	// The SDF supports one radius.  Use the smaller one, as the Metal backend
	// does; the old WGPU recorder fell back to AGG here, which is no longer
	// available.
	RecordShape(WGPUOSDShape_RoundedRect, x1, y1, x2, y2, std::min(rx, ry));
}

void WGPUOSDDrawTarget::roundedRect(double x1, double y1, double x2, double y2,
                                     double rxBottom, double ryBottom,
                                     double rxTop, double ryTop)
{
	const double r = std::min(std::min(rxBottom, ryBottom), std::min(rxTop, ryTop));
	RecordShape(WGPUOSDShape_RoundedRect, x1, y1, x2, y2, r);
}

void WGPUOSDDrawTarget::ellipse(double cx, double cy, double rx, double ry)
{
	RecordShape(WGPUOSDShape_Ellipse, cx, cy, rx, ry, 0.0);
}

void WGPUOSDDrawTarget::triangle(double x1, double y1, double x2, double y2,
                                  double x3, double y3)
{
	WGPUOSDPoint pts[3] = {
		{ (float)x1, (float)y1 },
		{ (float)x2, (float)y2 },
		{ (float)x3, (float)y3 }
	};
	RecordPolygon(pts, 3);
}

void WGPUOSDDrawTarget::arc(double cx, double cy, double rx, double ry,
                             double start, double sweep)
{
	// An arc is an open path, so it only ever shows up as a stroke.
	if (!m_hasStroke) return;

	const double radius = (fabs(rx) + fabs(ry)) * 0.5;
	const double length = fabs(sweep) * radius;
	int segments = (int)ceil(length / 4.0);

	if (segments < 4) segments = 4;
	if (segments > kWGPUOSDMaxShapePoints - 1) segments = kWGPUOSDMaxShapePoints - 1;

	WGPUOSDPoint pts[kWGPUOSDMaxShapePoints];
	for (int i = 0; i <= segments; i++)
	{
		const double a = start + sweep * ((double)i / (double)segments);
		pts[i].x = (float)(cx + cos(a) * rx);
		pts[i].y = (float)(cy + sin(a) * ry);
	}
	RecordPolyline(pts, segments + 1);
}

void WGPUOSDDrawTarget::star(double cx, double cy, double r1, double r2,
                              double startAngle, int numRays)
{
	if ((numRays <= 0) || (numRays * 2 > kWGPUOSDMaxShapePoints)) return;

	// Same vertex order as Agg2D::star: alternating outer r2 and inner r1.
	const double da = kWGPUOSDPi / (double)numRays;
	double a = startAngle;

	WGPUOSDPoint pts[kWGPUOSDMaxShapePoints];
	for (int i = 0; i < numRays; i++)
	{
		pts[i * 2 + 0].x = (float)(cos(a) * r2 + cx);
		pts[i * 2 + 0].y = (float)(sin(a) * r2 + cy);
		a += da;
		pts[i * 2 + 1].x = (float)(cos(a) * r1 + cx);
		pts[i * 2 + 1].y = (float)(sin(a) * r1 + cy);
		a += da;
	}
	RecordPolygon(pts, numRays * 2);
}

void WGPUOSDDrawTarget::curve(double x1, double y1, double x2, double y2,
                               double x3, double y3)
{
	if (!m_hasStroke) return;

	const int segments = 16;
	WGPUOSDPoint pts[17];
	for (int i = 0; i <= segments; i++)
	{
		const double t = (double)i / (double)segments;
		const double mt = 1.0 - t;
		pts[i].x = (float)(mt * mt * x1 + 2.0 * mt * t * x2 + t * t * x3);
		pts[i].y = (float)(mt * mt * y1 + 2.0 * mt * t * y2 + t * t * y3);
	}
	RecordPolyline(pts, segments + 1);
}

void WGPUOSDDrawTarget::curve(double x1, double y1, double x2, double y2,
                               double x3, double y3, double x4, double y4)
{
	if (!m_hasStroke) return;

	const int segments = 16;
	WGPUOSDPoint pts[17];
	for (int i = 0; i <= segments; i++)
	{
		const double t = (double)i / (double)segments;
		const double mt = 1.0 - t;
		pts[i].x = (float)(mt * mt * mt * x1 + 3.0 * mt * mt * t * x2 +
		                   3.0 * mt * t * t * x3 + t * t * t * x4);
		pts[i].y = (float)(mt * mt * mt * y1 + 3.0 * mt * mt * t * y2 +
		                   3.0 * mt * t * t * y3 + t * t * t * y4);
	}
	RecordPolyline(pts, segments + 1);
}

void WGPUOSDDrawTarget::polygon(double *xy, int numPoints)
{
	if ((xy == NULL) || (numPoints < 3) || (numPoints > kWGPUOSDMaxShapePoints)) return;

	WGPUOSDPoint pts[kWGPUOSDMaxShapePoints];
	for (int i = 0; i < numPoints; i++)
	{
		pts[i].x = (float)xy[i * 2 + 0];
		pts[i].y = (float)xy[i * 2 + 1];
	}
	RecordPolygon(pts, numPoints);
}

void WGPUOSDDrawTarget::polyline(double *xy, int numPoints)
{
	if ((xy == NULL) || (numPoints < 2) || (numPoints > kWGPUOSDMaxShapePoints)) return;

	WGPUOSDPoint pts[kWGPUOSDMaxShapePoints];
	for (int i = 0; i < numPoints; i++)
	{
		pts[i].x = (float)xy[i * 2 + 0];
		pts[i].y = (float)xy[i * 2 + 1];
	}
	RecordPolyline(pts, numPoints);
}
