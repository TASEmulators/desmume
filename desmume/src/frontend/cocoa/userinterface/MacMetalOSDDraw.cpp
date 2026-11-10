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

#include "MacMetalOSDDraw.h"

#include <math.h>
#include <pthread.h>
#include <string.h>
#include <algorithm>

static const double kMetalOSDPi = 3.14159265358979323846;

// ---------------------------------------------------------------------------
// MacMetalOSDDrawTarget
// ---------------------------------------------------------------------------

static const uint8_t *LookupEmbeddedFont(const std::string &name)
{
	for (int i = 0; i < MacMetalOSDEmbeddedFontCount; i++)
	{
		if (name == MacMetalOSDEmbeddedFonts[i].name)
		{
			return MacMetalOSDEmbeddedFonts[i].data;
		}
	}
	return NULL;
}

MacMetalOSDDrawTarget::MacMetalOSDDrawTarget()
	: m_hasFill(true)
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
	, m_fontData(LookupEmbeddedFont("verdana18_bold"))
{
}

void MacMetalOSDDrawTarget::clear()
{
	m_frame.shapes.clear();
	m_frame.points.clear();
	m_frame.runs.clear();
	m_frame.text.clear();

	if (!m_frame.cpuPixels.empty())
	{
		memset(m_frame.cpuPixels.data(), 0, m_frame.cpuPixels.size() * sizeof(uint32_t));
	}
	m_frame.hasCpuLayer = false;
}

uint32_t *MacMetalOSDDrawTarget::pixelBuffer()
{
	if (m_frame.cpuPixels.empty())
	{
		m_frame.cpuPixels.assign((size_t)METAL_OSD_WIDTH * METAL_OSD_HEIGHT, 0);
	}
	m_frame.hasCpuLayer = true;
	return m_frame.cpuPixels.data();
}

uint32_t MacMetalOSDDrawTarget::PackColor(MetalOSDColor c) const
{
	// Mirrors WGPUOSDAggTarget::PackColor: masterAlpha affects alpha only.
	double alpha = (double)c.a * m_masterAlpha;
	if (alpha < 0.0)   alpha = 0.0;
	if (alpha > 255.0) alpha = 255.0;

	return ((uint32_t)c.r << 24) | ((uint32_t)c.g << 16) |
	       ((uint32_t)c.b << 8)  | (uint32_t)(alpha + 0.5);
}

void MacMetalOSDDrawTarget::fillColor(MetalOSDColor c)
{
	// Agg2D (and WGPUOSDAggTarget) re-enable the fill here, so a script that
	// called noFill() earlier starts filling again as soon as it sets a color.
	m_hasFill = true;
	m_fill0 = m_fill1 = c;
	m_fillGradient = false;
}

void MacMetalOSDDrawTarget::fillColor(unsigned r, unsigned g, unsigned b, unsigned a)
{
	fillColor(MetalOSDColor(r, g, b, a));
}

void MacMetalOSDDrawTarget::lineColor(MetalOSDColor c)
{
	m_hasStroke = true;
	m_line0 = m_line1 = c;
	m_lineGradient = false;
}

void MacMetalOSDDrawTarget::lineColor(unsigned r, unsigned g, unsigned b, unsigned a)
{
	lineColor(MetalOSDColor(r, g, b, a));
}

void MacMetalOSDDrawTarget::noFill()   { m_hasFill = false; }
void MacMetalOSDDrawTarget::noLine()   { m_hasStroke = false; }

void MacMetalOSDDrawTarget::lineWidth(double w)  { m_strokeWidth = (w > 0.0) ? w : 0.0; }
void MacMetalOSDDrawTarget::masterAlpha(double a) { m_masterAlpha = a; }

void MacMetalOSDDrawTarget::fillLinearGradient(double x1, double y1, double x2, double y2,
                                               MetalOSDColor c1, MetalOSDColor c2, double profile)
{
	(void)profile; // only the default AGG profile is supported, as in wgpu_draw.cpp
	m_hasFill = true;
	m_fill0 = c1;
	m_fill1 = c2;
	m_gradientX0 = x1; m_gradientY0 = y1;
	m_gradientX1 = x2; m_gradientY1 = y2;
	m_fillGradient = true;
}

void MacMetalOSDDrawTarget::lineLinearGradient(double x1, double y1, double x2, double y2,
                                               MetalOSDColor c1, MetalOSDColor c2, double profile)
{
	(void)profile;
	m_hasStroke = true;
	m_line0 = c1;
	m_line1 = c2;
	m_lineGradientX0 = x1; m_lineGradientY0 = y1;
	m_lineGradientX1 = x2; m_lineGradientY1 = y2;
	m_lineGradient = true;
}

void MacMetalOSDDrawTarget::setFont(const std::string &name)
{
	const uint8_t *font = LookupEmbeddedFont(name);
	if (font != NULL)
	{
		m_fontData = font;
	}
	// Unknown names keep the previous font, matching Agg2D::setFont/lookupFont.
}

void MacMetalOSDDrawTarget::RecordShape(uint32_t type,
                                        double x0, double y0, double x1, double y1,
                                        double radius)
{
	MetalOSDShape s;
	memset(&s, 0, sizeof(s));

	s.type           = type;
	s.hasFill        = m_hasFill   ? 1 : 0;
	s.hasStroke      = m_hasStroke ? 1 : 0;
	s.fillGradient   = m_fillGradient ? 1 : 0;
	s.lineGradient   = m_lineGradient ? 1 : 0;
	s.lineWidth      = m_strokeWidth;
	s.x0 = x0; s.y0 = y0; s.x1 = x1; s.y1 = y1;
	s.radius         = radius;

	s.fillX0 = m_gradientX0; s.fillY0 = m_gradientY0;
	s.fillX1 = m_gradientX1; s.fillY1 = m_gradientY1;
	s.lineX0 = m_lineGradientX0; s.lineY0 = m_lineGradientY0;
	s.lineX1 = m_lineGradientX1; s.lineY1 = m_lineGradientY1;

	s.fill0 = PackColor(m_fill0);
	s.fill1 = PackColor(m_fill1);
	s.line0 = PackColor(m_line0);
	s.line1 = PackColor(m_line1);

	// Open paths must never be filled.
	if ((type == MetalOSDShape_Line) || (type == MetalOSDShape_Polyline))
	{
		s.hasFill = 0;
	}

	m_frame.shapes.push_back(s);
}

void MacMetalOSDDrawTarget::RecordPolygon(const MetalOSDPoint *points, int count)
{
	if ((points == NULL) || (count < 3))
	{
		return;
	}

	RecordShape(MetalOSDShape_Polygon, 0.0, 0.0, 0.0, 0.0, 0.0);
	MetalOSDShape &s = m_frame.shapes.back();
	s.pointOffset = (uint32_t)m_frame.points.size();
	s.pointCount  = (uint32_t)count;
	m_frame.points.insert(m_frame.points.end(), points, points + count);
}

void MacMetalOSDDrawTarget::RecordPolyline(const MetalOSDPoint *points, int count)
{
	if ((points == NULL) || (count < 2))
	{
		return;
	}

	RecordShape(MetalOSDShape_Polyline, 0.0, 0.0, 0.0, 0.0, 0.0);
	MetalOSDShape &s = m_frame.shapes.back();
	s.pointOffset = (uint32_t)m_frame.points.size();
	s.pointCount  = (uint32_t)count;
	m_frame.points.insert(m_frame.points.end(), points, points + count);
}

void MacMetalOSDDrawTarget::RecordText(double dstX, double dstY, const std::string &str)
{
	MetalOSDTextRun run;
	memset(&run, 0, sizeof(run));

	const MetalOSDColor c = m_line0;
	run.font       = m_fontData;
	run.x          = dstX;
	run.y          = dstY;   // PackGlyphs computes yTop = int(y) - base
	run.color      = ((uint32_t)c.r << 24) | ((uint32_t)c.g << 16) |
	                 ((uint32_t)c.b << 8)  | (uint32_t)c.a;
	run.charOffset = (uint32_t)m_frame.text.size();
	run.charCount  = (uint32_t)str.size();

	m_frame.text.insert(m_frame.text.end(), str.begin(), str.end());
	m_frame.runs.push_back(run);
}

void MacMetalOSDDrawTarget::renderText(double dstX, double dstY, const std::string &str)
{
	if (str.empty() || (m_fontData == NULL))
	{
		return;
	}
	RecordText(dstX, dstY, str);
}

void MacMetalOSDDrawTarget::renderTextDropshadowed(double dstX, double dstY, const std::string &str)
{
	// Same nine-draw sequence as AggDrawTarget::renderTextDropshadowed.  Each
	// call records one text run, so no separate shader path is needed.
	const MetalOSDColor old = m_line0;

	if (old.r + old.g + old.b < 192)
	{
		lineColor(255 - old.r, 255 - old.g, 255 - old.b);
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

	lineColor(old);
	renderText(dstX, dstY, str);
}

void MacMetalOSDDrawTarget::line(double x1, double y1, double x2, double y2)
{
	if (!m_hasStroke)
	{
		return;
	}
	RecordShape(MetalOSDShape_Line, x1, y1, x2, y2, 0.0);
}

void MacMetalOSDDrawTarget::rectangle(double x1, double y1, double x2, double y2)
{
	RecordShape(MetalOSDShape_Rectangle, x1, y1, x2, y2, 0.0);
}

void MacMetalOSDDrawTarget::roundedRect(double x1, double y1, double x2, double y2, double r)
{
	RecordShape(MetalOSDShape_RoundedRect, x1, y1, x2, y2, r);
}

void MacMetalOSDDrawTarget::roundedRect(double x1, double y1, double x2, double y2, double rx, double ry)
{
	// The SDF only supports one radius.  The uniform-radius case is exact; for
	// rx != ry use the smaller radius (the WGPU backend falls back to AGG here,
	// which cocoa does not have).
	RecordShape(MetalOSDShape_RoundedRect, x1, y1, x2, y2, std::min(rx, ry));
}

void MacMetalOSDDrawTarget::roundedRect(double x1, double y1, double x2, double y2,
                                        double rxBottom, double ryBottom, double rxTop, double ryTop)
{
	const double r = std::min(std::min(rxBottom, ryBottom), std::min(rxTop, ryTop));
	RecordShape(MetalOSDShape_RoundedRect, x1, y1, x2, y2, r);
}

void MacMetalOSDDrawTarget::ellipse(double cx, double cy, double rx, double ry)
{
	RecordShape(MetalOSDShape_Ellipse, cx, cy, rx, ry, 0.0);
}

void MacMetalOSDDrawTarget::triangle(double x1, double y1, double x2, double y2, double x3, double y3)
{
	MetalOSDPoint pts[3] =
	{
		{ (float)x1, (float)y1 },
		{ (float)x2, (float)y2 },
		{ (float)x3, (float)y3 }
	};
	RecordPolygon(pts, 3);
}

void MacMetalOSDDrawTarget::arc(double cx, double cy, double rx, double ry, double start, double sweep)
{
	// An arc is an open path, so it only ever shows up as a stroke.  Agg2D's
	// drawPath() skips the stroke when noLine() was called; do the same instead
	// of recording a polyline the shader would happily draw.
	if (!m_hasStroke)
	{
		return;
	}

	const double radius = (fabs(rx) + fabs(ry)) * 0.5;
	const double length = fabs(sweep) * radius;
	int segments = (int)ceil(length / 4.0);

	if (segments < 4)  segments = 4;
	if (segments > 63) segments = 63;

	MetalOSDPoint pts[METAL_OSD_MAX_SHAPE_POINTS];
	for (int i = 0; i <= segments; i++)
	{
		const double a = start + sweep * ((double)i / (double)segments);
		pts[i].x = (float)(cx + cos(a) * rx);
		pts[i].y = (float)(cy + sin(a) * ry);
	}
	RecordPolyline(pts, segments + 1);
}

void MacMetalOSDDrawTarget::star(double cx, double cy, double r1, double r2, double startAngle, int numRays)
{
	if ((numRays <= 0) || (numRays * 2 > METAL_OSD_MAX_SHAPE_POINTS))
	{
		return;
	}

	// Same vertex order as Agg2D::star: alternating outer r2 and inner r1.
	const double da = kMetalOSDPi / (double)numRays;
	double a = startAngle;

	MetalOSDPoint pts[METAL_OSD_MAX_SHAPE_POINTS];
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

void MacMetalOSDDrawTarget::curve(double x1, double y1, double x2, double y2, double x3, double y3)
{
	if (!m_hasStroke)
	{
		return;
	}

	const int segments = 16;
	MetalOSDPoint pts[17];

	for (int i = 0; i <= segments; i++)
	{
		const double t  = (double)i / (double)segments;
		const double mt = 1.0 - t;
		pts[i].x = (float)(mt * mt * x1 + 2.0 * mt * t * x2 + t * t * x3);
		pts[i].y = (float)(mt * mt * y1 + 2.0 * mt * t * y2 + t * t * y3);
	}
	RecordPolyline(pts, segments + 1);
}

void MacMetalOSDDrawTarget::polygon(double *xy, int numPoints)
{
	if ((xy == NULL) || (numPoints < 3) || (numPoints > METAL_OSD_MAX_SHAPE_POINTS))
	{
		return;
	}

	MetalOSDPoint pts[METAL_OSD_MAX_SHAPE_POINTS];
	for (int i = 0; i < numPoints; i++)
	{
		pts[i].x = (float)xy[i * 2 + 0];
		pts[i].y = (float)xy[i * 2 + 1];
	}
	RecordPolygon(pts, numPoints);
}

// ---------------------------------------------------------------------------
// Double-buffered frame exchange
// ---------------------------------------------------------------------------

static pthread_mutex_t s_osdMutex = PTHREAD_MUTEX_INITIALIZER;
static uint64_t s_frontGeneration = 0;

static MacMetalOSDDrawTarget &BackTarget()
{
	static MacMetalOSDDrawTarget target;
	return target;
}

static MetalOSDFrame &FrontFrame()
{
	static MetalOSDFrame frame;
	return frame;
}

MacMetalOSDDrawTarget *MacMetalOSD_BackTarget()
{
	return &BackTarget();
}

void MacMetalOSD_ClearBack()
{
	BackTarget().clear();
}

void MacMetalOSD_PresentBack()
{
	// Copy the whole recorded frame while holding the lock; the renderer then
	// works on its own snapshot and never touches emu-thread storage.
	pthread_mutex_lock(&s_osdMutex);
	FrontFrame() = BackTarget().frame();
	FrontFrame().generation = ++s_frontGeneration;
	pthread_mutex_unlock(&s_osdMutex);
}

const MetalOSDFrame *MacMetalOSD_LockFront()
{
	pthread_mutex_lock(&s_osdMutex);
	return &FrontFrame();
}

void MacMetalOSD_UnlockFront()
{
	pthread_mutex_unlock(&s_osdMutex);
}

uint64_t MacMetalOSD_FrontGeneration()
{
	pthread_mutex_lock(&s_osdMutex);
	const uint64_t gen = s_frontGeneration;
	pthread_mutex_unlock(&s_osdMutex);
	return gen;
}

void MacMetalOSD_AddFixedText(int x, int y, const char *text)
{
	if ((text == NULL) || (text[0] == '\0'))
	{
		return;
	}

	MacMetalOSDDrawTarget *target = MacMetalOSD_BackTarget();
	const MetalOSDColor savedColor = target->lineColor();
	const bool savedStroke = target->hasStroke();

	// gui.osdtext() has no color argument.  White with the standard black
	// drop shadow stays legible on both screens, matching the usual HUD look.
	target->lineColor(255, 255, 255, 255);
	target->renderTextDropshadowed((double)x, (double)y, std::string(text));
	target->lineColor(savedColor);
	target->setHasStroke(savedStroke);
}
