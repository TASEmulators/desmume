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

#ifndef _WGPU_OSD_DRAW_H_
#define _WGPU_OSD_DRAW_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "wgpu_draw.h"
#include "../agg/aggdraw.h"

// ---------------------------------------------------------------------------
// AGG-free recorder for the WebGPU OSD backend (wgpu-agg.md 20).
//
// This is the WebGPU counterpart of MacMetalOSDDrawTarget.  It records the
// primitives that agg_osd.cpp and Lua's agg.* table emit and hands the frame to
// WGPUOSDRenderer, which rasterizes shapes with SDF WGSL and text with the R8
// glyph atlas.  There is no CPU vector rasterizer and no AGG fallback path.
//
// Unlike the Metal version this class DOES derive from AggDrawTarget.  That is
// a Windows requirement: display.cpp installs the recorder through
// Agg_setHudTarget(AggDrawTarget*) and agg_osd.cpp / lua-engine.cpp reach it
// via aggDraw.hud.  Only the abstract interface is inherited -- none of
// Agg2DBase / AggDrawTargetImplementation<T> is instantiated, so this object
// file has no undefined agg::* symbols and the default build no longer needs
// agg-2.5*.lib.  All the signature types below (AggColor, RectD, Affine,
// Transformations, the Agg2DBase enums, rendering_buffer) are header-only PODs.
//
// The CPU layer is a 256x384 premultiplied BGRA8 buffer exposed through buf().
// Lua's gui.* writes straight into it; WGPUOSDRenderer uploads it as the last
// layer, exactly like the previous WGPUOSDAggTarget did.
// ---------------------------------------------------------------------------

struct WGPUOSDColor
{
	unsigned r, g, b, a;
	WGPUOSDColor(unsigned r_ = 255, unsigned g_ = 255, unsigned b_ = 255, unsigned a_ = 255)
		: r(r_), g(g_), b(b_), a(a_) {}
	explicit WGPUOSDColor(AggColor c) : r(c.r), g(c.g), b(c.b), a(c.a) {}
	AggColor toAgg() const { return AggColor(r, g, b, a); }
};

struct WGPUOSDFontEntry
{
	std::string name;
	const uint8_t *data;
};

class WGPUOSDDrawTarget : public AggDrawTarget
{
public:
	WGPUOSDDrawTarget(int width, int height);
	~WGPUOSDDrawTarget();

	// --- GPU side (not part of AggDrawTarget) ---
	bool beginGPU(void *instance, void *device, void *queue);
	bool setFonts(const WGPUOSDFont *fonts, int count);
	bool render();
	bool readPixels(uint8_t *dst, int dstStrideBytes);
	void *textureView() const;
	int targetWidth() const { return m_width; }
	int targetHeight() const { return m_height; }
	bool hasContent() const;

	// Direct pixel access for the Lua gui.* CPU layer.  buf() and pixelBuffer()
	// both mark the layer dirty for this frame.
	virtual agg::rendering_buffer &buf()
	{
		dirty();
		m_hasCpuLayer = true;
		return m_rbuf;
	}
	uint32_t *pixelBuffer();
	int pixelStrideBytes() const { return m_stride; }

	// --- Recorded AggDrawTarget operations ---
	virtual void clear();
	virtual void setDrawTargetDims(agg::int8u *buf, int width, int height, int stride);
	virtual void attach(unsigned char *buf, unsigned width, unsigned height, int stride);

	virtual void clearAll(AggColor c);
	virtual void clearAll(unsigned r, unsigned g, unsigned b, unsigned a = 255);
	virtual void clearClipBox(AggColor c);
	virtual void clearClipBox(unsigned r, unsigned g, unsigned b, unsigned a = 255);

	virtual void fillColor(AggColor c);
	virtual void fillColor(unsigned r, unsigned g, unsigned b, unsigned a = 255);
	virtual void noFill();
	virtual AggColor fillColor();

	virtual void lineColor(AggColor c);
	virtual void lineColor(unsigned r, unsigned g, unsigned b, unsigned a = 255);
	virtual void noLine();
	virtual AggColor lineColor();

	virtual void lineWidth(double w);
	virtual double lineWidth();
	virtual void masterAlpha(double a);
	virtual double masterAlpha();

	virtual void fillLinearGradient(double x1, double y1, double x2, double y2,
	                                AggColor c1, AggColor c2, double profile = 1.0);
	virtual void lineLinearGradient(double x1, double y1, double x2, double y2,
	                                AggColor c1, AggColor c2, double profile = 1.0);

	virtual void setFont(const std::string &name);
	virtual void renderText(double dstX, double dstY, const std::string &str);
	virtual void renderTextDropshadowed(double dstX, double dstY, const std::string &str);

	virtual void line(double x1, double y1, double x2, double y2);
	virtual void triangle(double x1, double y1, double x2, double y2, double x3, double y3);
	virtual void rectangle(double x1, double y1, double x2, double y2);
	virtual void roundedRect(double x1, double y1, double x2, double y2, double r);
	virtual void roundedRect(double x1, double y1, double x2, double y2, double rx, double ry);
	virtual void roundedRect(double x1, double y1, double x2, double y2,
	                         double rxBottom, double ryBottom, double rxTop, double ryTop);
	virtual void ellipse(double cx, double cy, double rx, double ry);
	virtual void arc(double cx, double cy, double rx, double ry, double start, double sweep);
	virtual void star(double cx, double cy, double r1, double r2, double startAngle, int numRays);
	virtual void curve(double x1, double y1, double x2, double y2, double x3, double y3);
	virtual void curve(double x1, double y1, double x2, double y2,
	                   double x3, double y3, double x4, double y4);
	virtual void polygon(double *xy, int numPoints);
	virtual void polyline(double *xy, int numPoints);

	// --- AggDrawTarget methods outside the recorded subset ---
	// They are implemented as no-ops / default state so the class is concrete.
	// agg_osd.cpp never uses them; Lua's agg.* table does not expose them
	// either.  Keeping them explicit avoids silently inheriting AGG behaviour.
	virtual void clipBox(double, double, double, double) {}
	virtual Agg2DBase::RectD clipBox() const { return m_clipBox; }

	virtual void worldToScreen(double &, double &) const {}
	virtual void screenToWorld(double &, double &) const {}
	virtual double worldToScreen(double scalar) const { return scalar; }
	virtual double screenToWorld(double scalar) const { return scalar; }
	virtual void alignPoint(double &, double &) const {}
	virtual bool inBox(double worldX, double worldY) const
	{
		return (worldX >= m_clipBox.x1) && (worldX <= m_clipBox.x2) &&
		       (worldY >= m_clipBox.y1) && (worldY <= m_clipBox.y2);
	}

	virtual void blendMode(Agg2DBase::BlendMode m) { m_blendMode = m; }
	virtual Agg2DBase::BlendMode blendMode() { return m_blendMode; }
	virtual void imageBlendMode(Agg2DBase::BlendMode m) { m_imageBlendMode = m; }
	virtual Agg2DBase::BlendMode imageBlendMode() { return m_imageBlendMode; }
	virtual void imageBlendColor(AggColor c) { m_imageBlendColor = WGPUOSDColor(c); }
	virtual void imageBlendColor(unsigned r, unsigned g, unsigned b, unsigned a = 255)
	{
		m_imageBlendColor = WGPUOSDColor(r, g, b, a);
	}
	virtual AggColor imageBlendColor() { return m_imageBlendColor.toAgg(); }
	virtual void antiAliasGamma(double g) { m_gamma = g; }
	virtual double antiAliasGamma() { return m_gamma; }

	virtual void transformImage(const Agg2DBase::Image<T_AGG_PF_RGBA> &, double, double, double, double) {}

	virtual void fillRadialGradient(double, double, double, AggColor, AggColor, double = 1.0) {}
	virtual void lineRadialGradient(double, double, double, AggColor, AggColor, double = 1.0) {}
	virtual void fillRadialGradient(double, double, double, AggColor, AggColor, AggColor) {}
	virtual void lineRadialGradient(double, double, double, AggColor, AggColor, AggColor) {}
	virtual void fillRadialGradient(double, double, double) {}
	virtual void lineRadialGradient(double, double, double) {}

	virtual void lineCap(Agg2DBase::LineCap cap) { m_lineCap = cap; }
	virtual Agg2DBase::LineCap lineCap() { return m_lineCap; }
	virtual void lineJoin(Agg2DBase::LineJoin join) { m_lineJoin = join; }
	virtual Agg2DBase::LineJoin lineJoin() { return m_lineJoin; }
	virtual void fillEvenOdd(bool flag) { m_fillEvenOdd = flag; }
	virtual bool fillEvenOdd() { return m_fillEvenOdd; }

	virtual Agg2DBase::Transformations transformations() { return m_transformations; }
	virtual void transformations(const Agg2DBase::Transformations &tr) { m_transformations = tr; }
	virtual const Agg2DBase::Affine &affine() { return m_affine; }
	virtual void resetTransformations() {}
	virtual void matrix(const Agg2DBase::Affine &) {}
	virtual void matrix(const Agg2DBase::Transformations &tr) { m_transformations = tr; }
	virtual void rotate(double) {}
	virtual void rotate(double, double, double) {}
	virtual void scale(double) {}
	virtual void scale(double, double) {}
	virtual void skew(double, double) {}
	virtual void translate(double, double) {}
	virtual void parallelogram(double, double, double, double, const double *) {}
	virtual void viewport(double, double, double, double, double, double, double, double,
	                      Agg2DBase::ViewportOption = Agg2DBase::XMidYMid,
	                      Agg2DBase::WindowFitLogic = Agg2DBase::WindowFitLogic_meet) {}

	virtual void resetPath() {}
	virtual void moveTo(double, double) {}
	virtual void moveRel(double, double) {}
	virtual void lineTo(double, double) {}
	virtual void lineRel(double, double) {}
	virtual void horLineTo(double) {}
	virtual void horLineRel(double) {}
	virtual void verLineTo(double) {}
	virtual void verLineRel(double) {}
	virtual void arcTo(double, double, double, bool, bool, double, double) {}
	virtual void arcRel(double, double, double, bool, bool, double, double) {}
	virtual void quadricCurveTo(double, double, double, double) {}
	virtual void quadricCurveRel(double, double, double, double) {}
	virtual void quadricCurveTo(double, double) {}
	virtual void quadricCurveRel(double, double) {}
	virtual void cubicCurveTo(double, double, double, double, double, double) {}
	virtual void cubicCurveRel(double, double, double, double, double, double) {}
	virtual void cubicCurveTo(double, double, double, double) {}
	virtual void cubicCurveRel(double, double, double, double) {}
	virtual void addEllipse(double, double, double, double, Agg2DBase::Direction) {}
	virtual void closePolygon() {}
	virtual void drawPath(Agg2DBase::DrawPathFlag = Agg2DBase::FillAndStroke) {}

	virtual void imageFilter(Agg2DBase::ImageFilter f) { m_imageFilter = f; }
	virtual Agg2DBase::ImageFilter imageFilter() { return m_imageFilter; }
	virtual void imageResample(Agg2DBase::ImageResample f) { m_imageResample = f; }
	virtual Agg2DBase::ImageResample imageResample() { return m_imageResample; }

#ifdef AGG2D_USE_VECTORFONTS
	virtual void setVectorFont(const std::string &, int, bool) {}
	virtual void renderVectorFontText(double, double, const std::string &) {}
#endif

private:
	uint32_t PackColor(const WGPUOSDColor &c) const;
	void RecordShape(uint32_t type, double x0, double y0, double x1, double y1, double radius);
	void RecordPolygon(const WGPUOSDPoint *points, int count);
	void RecordPolyline(const WGPUOSDPoint *points, int count);
	void RecordText(double dstX, double dstY, const std::string &str);
	void FillPixels(const WGPUOSDColor &c);
	void Resize(int width, int height);

	std::vector<uint32_t> m_pixels;   // premultiplied BGRA8, top-down
	agg::rendering_buffer m_rbuf;
	int m_width;
	int m_height;
	int m_stride;
	bool m_hasCpuLayer;

	bool m_hasFill;
	bool m_hasStroke;
	bool m_fillGradient;
	bool m_lineGradient;
	WGPUOSDColor m_fill0, m_fill1;
	WGPUOSDColor m_line0, m_line1;
	double m_gradientX0, m_gradientY0, m_gradientX1, m_gradientY1;
	double m_lineGradientX0, m_lineGradientY0, m_lineGradientX1, m_lineGradientY1;
	double m_strokeWidth;
	double m_masterAlpha;

	const uint8_t *m_fontData;
	std::vector<WGPUOSDFontEntry> m_fonts;

	std::vector<WGPUOSDShape> m_shapes;
	std::vector<WGPUOSDPoint> m_points;
	std::vector<WGPUOSDTextRun> m_textRuns;
	std::vector<char> m_textBytes;

	WGPUOSDRenderer m_renderer;

	Agg2DBase::RectD m_clipBox;
	Agg2DBase::Affine m_affine;
	Agg2DBase::Transformations m_transformations;
	Agg2DBase::BlendMode m_blendMode;
	Agg2DBase::BlendMode m_imageBlendMode;
	WGPUOSDColor m_imageBlendColor;
	Agg2DBase::LineCap m_lineCap;
	Agg2DBase::LineJoin m_lineJoin;
	Agg2DBase::ImageFilter m_imageFilter;
	Agg2DBase::ImageResample m_imageResample;
	double m_gamma;
	bool m_fillEvenOdd;

	WGPUOSDDrawTarget(const WGPUOSDDrawTarget &);
	WGPUOSDDrawTarget &operator=(const WGPUOSDDrawTarget &);
};

#endif // _WGPU_OSD_DRAW_H_
