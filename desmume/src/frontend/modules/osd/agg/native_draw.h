/*
The MIT License

Copyright (C) 2009-2026 DeSmuME team

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

// ---------------------------------------------------------------------------
// NativeDrawTarget -- a Windows GDI+ backed implementation of AggDrawTarget.
//
// This is the "route 1" replacement for the AGG rasteriser: it keeps the exact
// same AggDrawTarget contract (and therefore the same 256x384 BGRA32 hud
// framebuffer), but draws every primitive with the platform 2D API instead of
// Agg2D.  Text is still blitted from the AGG embedded bitmap fonts so that the
// font names stored in desmume.ini and used by Lua scripts keep working.
//
// Enable with DESMUME_NATIVE_DRAW (Windows only).  When the macro is not
// defined, nothing in this file is compiled and the AGG backend is untouched,
// which keeps the posix/GTK/CLI and cocoa frontends working as before.
// ---------------------------------------------------------------------------

#ifndef _NATIVE_DRAW_H_
#define _NATIVE_DRAW_H_

#if defined(DESMUME_NATIVE_DRAW) && defined(WIN32)

#include "aggdraw.h"

#include <vector>
#include <string>

namespace Gdiplus
{
	class Bitmap;
	class Graphics;
	class GraphicsPath;
	class Brush;
	class Pen;
	class Font;
	class PrivateFontCollection;
}

class NativeDrawTarget : public AggDrawTarget
{
public:
	NativeDrawTarget(agg::int8u* buf, int width, int height, int stride);
	virtual ~NativeDrawTarget();

	// -- framebuffer -------------------------------------------------------
	virtual void clear();
	virtual void setDrawTargetDims(agg::int8u* buf, int width, int height, int stride);
	virtual agg::rendering_buffer& buf();
	virtual void attach(unsigned char* buf, unsigned width, unsigned height, int stride);

	virtual void clipBox(double x1, double y1, double x2, double y2);
	virtual Agg2DBase::RectD clipBox() const;
	virtual void clearAll(AggColor c);
	virtual void clearAll(unsigned r, unsigned g, unsigned b, unsigned a = 255);
	virtual void clearClipBox(AggColor c);
	virtual void clearClipBox(unsigned r, unsigned g, unsigned b, unsigned a = 255);

	// -- conversions -------------------------------------------------------
	virtual void worldToScreen(double& x, double& y) const;
	virtual void screenToWorld(double& x, double& y) const;
	virtual double worldToScreen(double scalar) const;
	virtual double screenToWorld(double scalar) const;
	virtual void alignPoint(double& x, double& y) const;
	virtual bool inBox(double worldX, double worldY) const;

	// -- attributes --------------------------------------------------------
	virtual void blendMode(Agg2DBase::BlendMode m);
	virtual Agg2DBase::BlendMode blendMode();
	virtual void imageBlendMode(Agg2DBase::BlendMode m);
	virtual Agg2DBase::BlendMode imageBlendMode();
	virtual void imageBlendColor(AggColor c);
	virtual void imageBlendColor(unsigned r, unsigned g, unsigned b, unsigned a = 255);
	virtual AggColor imageBlendColor();
	virtual void masterAlpha(double a);
	virtual double masterAlpha();
	virtual void antiAliasGamma(double g);
	virtual double antiAliasGamma();

	virtual void fillColor(AggColor c);
	virtual void fillColor(unsigned r, unsigned g, unsigned b, unsigned a = 255);
	virtual void noFill();
	virtual void lineColor(AggColor c);
	virtual void lineColor(unsigned r, unsigned g, unsigned b, unsigned a = 255);
	virtual void noLine();
	virtual AggColor fillColor();
	virtual AggColor lineColor();

	virtual void transformImage(const Agg2DBase::Image<T_AGG_PF_RGBA>& img,
		double dstX1, double dstY1, double dstX2, double dstY2);

	virtual void fillLinearGradient(double x1, double y1, double x2, double y2, AggColor c1, AggColor c2, double profile = 1.0);
	virtual void lineLinearGradient(double x1, double y1, double x2, double y2, AggColor c1, AggColor c2, double profile = 1.0);
	virtual void fillRadialGradient(double x, double y, double r, AggColor c1, AggColor c2, double profile = 1.0);
	virtual void lineRadialGradient(double x, double y, double r, AggColor c1, AggColor c2, double profile = 1.0);
	virtual void fillRadialGradient(double x, double y, double r, AggColor c1, AggColor c2, AggColor c3);
	virtual void lineRadialGradient(double x, double y, double r, AggColor c1, AggColor c2, AggColor c3);
	virtual void fillRadialGradient(double x, double y, double r);
	virtual void lineRadialGradient(double x, double y, double r);

	virtual void lineWidth(double w);
	virtual double lineWidth();
	virtual void lineCap(Agg2DBase::LineCap cap);
	virtual Agg2DBase::LineCap lineCap();
	virtual void lineJoin(Agg2DBase::LineJoin join);
	virtual Agg2DBase::LineJoin lineJoin();
	virtual void fillEvenOdd(bool evenOddFlag);
	virtual bool fillEvenOdd();

	// -- transformations ---------------------------------------------------
	virtual Agg2DBase::Transformations transformations();
	virtual void transformations(const Agg2DBase::Transformations& tr);
	virtual const Agg2DBase::Affine& affine();
	virtual void resetTransformations();
	virtual void matrix(const Agg2DBase::Affine& tr);
	virtual void matrix(const Agg2DBase::Transformations& tr);
	virtual void rotate(double angle);
	virtual void rotate(double angle, double cx, double cy);
	virtual void scale(double s);
	virtual void scale(double sx, double sy);
	virtual void skew(double sx, double sy);
	virtual void translate(double x, double y);
	virtual void parallelogram(double x1, double y1, double x2, double y2, const double* para);
	virtual void viewport(double worldX1, double worldY1, double worldX2, double worldY2,
		double screenX1, double screenY1, double screenX2, double screenY2,
		Agg2DBase::ViewportOption opt = Agg2DBase::XMidYMid,
		Agg2DBase::WindowFitLogic fl = Agg2DBase::WindowFitLogic_meet);

	// -- shapes ------------------------------------------------------------
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
	virtual void curve(double x1, double y1, double x2, double y2, double x3, double y3, double x4, double y4);
	virtual void polygon(double* xy, int numPoints);
	virtual void polyline(double* xy, int numPoints);

	// -- path --------------------------------------------------------------
	virtual void resetPath();
	virtual void moveTo(double x, double y);
	virtual void moveRel(double dx, double dy);
	virtual void lineTo(double x, double y);
	virtual void lineRel(double dx, double dy);
	virtual void horLineTo(double x);
	virtual void horLineRel(double dx);
	virtual void verLineTo(double y);
	virtual void verLineRel(double dy);
	virtual void arcTo(double rx, double ry, double angle, bool largeArcFlag, bool sweepFlag, double x, double y);
	virtual void arcRel(double rx, double ry, double angle, bool largeArcFlag, bool sweepFlag, double dx, double dy);
	virtual void quadricCurveTo(double xCtrl, double yCtrl, double xTo, double yTo);
	virtual void quadricCurveRel(double dxCtrl, double dyCtrl, double dxTo, double dyTo);
	virtual void quadricCurveTo(double xTo, double yTo);
	virtual void quadricCurveRel(double dxTo, double dyTo);
	virtual void cubicCurveTo(double xCtrl1, double yCtrl1, double xCtrl2, double yCtrl2, double xTo, double yTo);
	virtual void cubicCurveRel(double dxCtrl1, double dyCtrl1, double dxCtrl2, double dyCtrl2, double dxTo, double dyTo);
	virtual void cubicCurveTo(double xCtrl2, double yCtrl2, double xTo, double yTo);
	virtual void cubicCurveRel(double xCtrl2, double yCtrl2, double dxTo, double dyTo);
	virtual void addEllipse(double cx, double cy, double rx, double ry, Agg2DBase::Direction dir);
	virtual void closePolygon();
	virtual void drawPath(Agg2DBase::DrawPathFlag flag = Agg2DBase::FillAndStroke);

	// -- images / text -----------------------------------------------------
	virtual void imageFilter(Agg2DBase::ImageFilter f);
	virtual Agg2DBase::ImageFilter imageFilter();
	virtual void imageResample(Agg2DBase::ImageResample f);
	virtual Agg2DBase::ImageResample imageResample();
	virtual void setFont(const std::string& name);
#ifdef AGG2D_USE_VECTORFONTS
	virtual void setVectorFont(const std::string& fileName, int size, bool bold);
#endif
	virtual void renderText(double dstX, double dstY, const std::string& str);
#ifdef AGG2D_USE_VECTORFONTS
	virtual void renderVectorFontText(double dstX, double dstY, const std::string& str);
#endif

private:
	struct Pt { double x, y; };
	struct GradientState
	{
		int kind;              // 0 = solid, 1 = linear, 2 = radial
		double x1, y1, x2, y2; // linear endpoints / radial centre + radius
		AggColor c1, c2, c3;
		double profile;
		GradientState() : kind(0), x1(0), y1(0), x2(0), y2(0),
			c1(0, 0, 0, 255), c2(0, 0, 0, 255), c3(0, 0, 0, 255), profile(1.0) {}
	};

	agg::rendering_buffer m_rbuf;
	agg::int8u* m_pixels;
	int m_width, m_height, m_stride;

	Gdiplus::Bitmap* m_bitmap;
	Gdiplus::Graphics* m_graphics;
	Gdiplus::Graphics* m_active;      // graphics the current primitive draws into

	// Non-default blend modes are implemented by rasterising the primitive
	// into a fully transparent scratch surface and compositing it with an
	// explicit Porter-Duff / separable blend formula.  Only allocated on
	// demand (the HUD never leaves BlendAlpha).
	Gdiplus::Bitmap* m_scratchBitmap;
	Gdiplus::Graphics* m_scratchGraphics;
	std::vector<unsigned char> m_scratchPixels;
	bool m_scratchActive;

#ifdef AGG2D_USE_VECTORFONTS
	Gdiplus::Font* m_vectorFont;
	Gdiplus::PrivateFontCollection* m_fontCollection;
#endif

	AggColor m_fill, m_line;
	bool m_hasFill, m_hasLine;
	double m_lineWidth;
	GradientState m_fillGradient, m_lineGradient;
	Agg2DBase::LineCap m_lineCap;
	Agg2DBase::LineJoin m_lineJoin;
	bool m_fillEvenOdd;
	Agg2DBase::BlendMode m_blendMode, m_imageBlendMode;
	AggColor m_imageBlendColor;
	double m_masterAlpha, m_antiAliasGamma;
	Agg2DBase::ImageFilter m_imageFilter;
	Agg2DBase::ImageResample m_imageResample;
	Agg2DBase::RectD m_clip;
	bool m_hasClip;
	Agg2DBase::Affine m_affine;
	double m_xform[6];          // combined world->screen: m_affineX * m_viewportX
	double m_affineX[6];        // user affine (rotate/scale/translate/matrix)
	double m_viewportX[6];      // viewport affine (viewport/parallelogram)
	const agg::int8u* m_font;

	std::vector<Pt> m_pathPoints;
	std::vector<unsigned char> m_pathTypes;
	Pt m_pathCursor;

	bool EnsureSurface();
	void ReleaseSurface();
	bool EnsureScratch();
	void ReleaseScratch();
	void CompositeScratch(Agg2DBase::BlendMode mode);
	Gdiplus::Graphics* BeginPrimitive();
	void EndPrimitive();
	void ApplyClip();
	bool Touch();
	Gdiplus::Brush* MakeBrush(bool useLineColor);
	void ConfigurePen(Gdiplus::Pen& pen);
	void PaintPath(Gdiplus::GraphicsPath& path, Agg2DBase::DrawPathFlag flag);
	void BlitText(double dstX, double dstY, const std::string& str, unsigned char* pixels);
	void PathMoveTo(double x, double y);
	void PathLineTo(double x, double y);
	void PathBezierTo(double x1, double y1, double x2, double y2, double x3, double y3);
	void AppendArcToPath(double x1, double y1, double rx, double ry, double angle,
		bool largeArcFlag, bool sweepFlag, double x2, double y2);
	void AppendEllipseArc(double cx, double cy, double rx, double ry,
		double start, double sweep, bool moveFirst);
	void TransformPoint(double& x, double& y) const;
	void ComposeTransform();
	void PremultiplyAffine(double a, double b, double c, double d, double e, double f);
};

// CPU alpha composite + optional nearest-neighbour scale, used by the
// DirectDraw/WebGPU paths which cannot blend a second texture on the GPU.
// Replaces the T_AGG_RGBA::transformImage call in display.cpp.
void NativeDraw_CompositeHUD(unsigned char* dst, int dstW, int dstH, int dstStride,
	const unsigned char* hud, int hudW, int hudH, int hudStride,
	bool srcPremultiplied = true);

// One-time GDI+ startup + loading of the RCDATA bitmap font payloads.
// Safe to call repeatedly.  Font resources live in the module (the .exe)
// and LockResource pointers stay valid for the process lifetime.
void NativeDraw_Init();

// Look up a font by its DeSmuME name (e.g. "verdana18_bold").  Returns NULL
// when the resource is missing or has not been loaded yet.
const agg::int8u* NativeDraw_FontData(const char* name);
int NativeDraw_FontSize(const char* name);

// Agg2D's pixfmt_bgra32 blender stores premultiplied RGB with straight alpha
// (a 50% red fill becomes R=127,A=128), while GDI+ writes straight alpha
// (R=255,A=127).  Keep the AGG convention by default so the existing OpenGL
// HUD texture path and Lua direct writes see the same bytes as before.
// Passing false produces the "more correct" straight-alpha look.
// Call before the first draw on a target (the pixel format is chosen when the
// GDI+ surface is created).
void NativeDraw_SetPremultipliedOutput(bool enabled);

#endif // DESMUME_NATIVE_DRAW && WIN32
#endif // _NATIVE_DRAW_H_
