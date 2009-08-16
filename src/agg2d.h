//----------------------------------------------------------------------------
// Agg2D - Version 1.0
// Based on Anti-Grain Geometry
// Copyright (C) 2005 Maxim Shemanarev (http://www.antigrain.com)
//
// Permission to copy, use, modify, sell and distribute this software
// is granted provided this copyright notice appears in all copies.
// This software is provided "as is" without express or implied
// warranty, and with no claim as to its suitability for any purpose.
//
//----------------------------------------------------------------------------
// Contact: mcseem@antigrain.com
//          mcseemagg@yahoo.com
//          http://www.antigrain.com
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//
//	25 Jan 2007 - Ported to AGG 2.4 Jerry Evans (jerry@novadsp.com)
//	11 Jul 2009 - significant refactors for introduction to desmume
//
//----------------------------------------------------------------------------

#ifndef AGG2D_INCLUDED
#define AGG2D_INCLUDED

//we're not using vector fonts right now.
//#define AGG2D_USE_VECTORFONTS

#ifdef AGG2D_USE_VECTORFONTS
#if defined(UNDER_CE) || not defined(WIN32)
#define AGG2D_USE_FREETYPE
#endif
#endif

#ifdef _MSC_VER
#pragma warning(disable: 4786)
#endif

// JME
#include "agg_basics.h"
#include "agg_math_stroke.h"
#include "agg_trans_affine.h"
#include "agg_trans_viewport.h"
#include "agg_path_storage.h"
#include "agg_conv_stroke.h"
#include "agg_conv_transform.h"
#include "agg_conv_curve.h"
#include "agg_rendering_buffer.h"
#include "agg_renderer_base.h"
#include "agg_renderer_scanline.h"
#include "agg_span_gradient.h"
#include "agg_span_image_filter_rgba.h"

//+ JME
#include "agg_span_allocator.h"
#include "agg_span_converter.h"
#include "agg_span_interpolator_linear.h"
#include "agg_rasterizer_scanline_aa.h"
#include "agg_gamma_functions.h"
#include "agg_scanline_u.h"
#include "agg_bezier_arc.h"
#include "agg_rounded_rect.h"
#include "agg_font_cache_manager.h"

#include "agg_glyph_raster_bin.h"
#include "agg_renderer_raster_text.h"
#include "agg_embedded_raster_fonts.h"


#include <string>

#ifdef AGG2D_USE_VECTORFONTS
#ifdef AGG2D_USE_FREETYPE
#ifndef WIN32
#include "agg_font_freetype.h"
#else
#include "../font_freetype/agg_font_freetype.h"
#endif
#else
#include "../font_win32_tt/agg_font_win32_tt.h"
#endif
#endif

#include "agg_pixfmt_rgba.h"
//+ JME
#include "agg_image_accessors.h"

#define AGG2D_TEMPLATE template<typename PixFormatSet>
#define AGG2D_TEMPLATE_WITH_IMAGE template<typename PixFormatSet, typename ImagePixFormatSet>
#define AGG2D_TEMPLATE_ARG <PixFormatSet>
#define AGG2D_IMAGE_TEMPLATE template<typename ImagePixFormatSet>
#define AGG2D_IMAGE_TEMPLATE_ARG <ImagePixFormatSet>
#define TIMAGE Agg2DBase :: Image AGG2D_IMAGE_TEMPLATE_ARG


template<typename Main, typename Pre, typename SG>
class PixFormatSetDeclaration
{
public:
	typedef Main PixFormat;
	typedef Pre PixFormatPre;
	typedef SG SpanGenerator;
};

class Agg2DBase
{
public:
	// JME
	//typedef agg::rect         Rect;
	typedef agg::rect_i       Rect;
	typedef agg::rect_d       RectD;
	typedef agg::trans_affine Affine;

	enum LineJoin
	{
		JOIN_MITER = agg::miter_join,
		JOIN_ROUND = agg::round_join,
		JOIN_BEVEL = agg::bevel_join
	};

	enum LineCap
	{
		CAP_BUTT    = agg::butt_cap,
		CAP_SQUARE  = agg::square_cap,
		CAP_ROUND   = agg::round_cap
	};

	enum TextAlignment
	{
		AlignLeft,
		AlignRight,
		AlignCenter,
		AlignBaseline,
		AlignTop = AlignRight,
		AlignBottom = AlignLeft
	};


	enum DrawPathFlag
	{
		FillOnly,
		StrokeOnly,
		FillAndStroke,
		FillWithLineColor
	};

	enum ViewportOption
	{
		Anisotropic,
		XMinYMin,
		XMidYMin,
		XMaxYMin,
		XMinYMid,
		XMidYMid,
		XMaxYMid,
		XMinYMax,
		XMidYMax,
		XMaxYMax
	};

	enum WindowFitLogic
	{
		WindowFitLogic_meet,
		WindowFitLogic_slice
	};

	struct Transformations
	{
		double affineMatrix[6];
	};

	AGG2D_IMAGE_TEMPLATE struct Image
	{
		agg::rendering_buffer renBuf;

		Image(const agg::rendering_buffer& srcBuf)
			: renBuf(srcBuf)
		{}
		Image(unsigned char* buf, unsigned width, unsigned height, int stride) :
		renBuf(buf, width, height, stride) {}
		void attach(unsigned char* buf, unsigned width, unsigned height, int stride)
		{
			renBuf.attach(buf, width, height, stride);
		}
		int width()  const { return renBuf.width(); }
		int height() const { return renBuf.height(); }

		AGG2D_TEMPLATE void premultiply()
		{
			typename PixFormatSet::PixFormat pixf(renBuf);
			pixf.premultiply();
		}

		AGG2D_TEMPLATE void demultiply()
		{
			typename PixFormatSet::PixFormat pixf(renBuf);
			pixf.demultiply();
		}

	};

	enum ImageFilter
	{
		NoFilter,
		Bilinear,
		Hanning,
		Hermite,
		Quadric,
		Bicubic,
		Catrom,
		Spline16,
		Spline36,
		Blackman144
	};

	enum ImageResample
	{
		NoResample,
		ResampleAlways,
		ResampleOnZoomOut
	};

	enum FontCacheType
	{
		RasterFontCache,
		VectorFontCache
	};

	enum BlendMode
	{
		BlendAlpha      = agg::end_of_comp_op_e,
		BlendClear      = agg::comp_op_clear,
		BlendSrc        = agg::comp_op_src,
		BlendDst        = agg::comp_op_dst,
		BlendSrcOver    = agg::comp_op_src_over,
		BlendDstOver    = agg::comp_op_dst_over,
		BlendSrcIn      = agg::comp_op_src_in,
		BlendDstIn      = agg::comp_op_dst_in,
		BlendSrcOut     = agg::comp_op_src_out,
		BlendDstOut     = agg::comp_op_dst_out,
		BlendSrcAtop    = agg::comp_op_src_atop,
		BlendDstAtop    = agg::comp_op_dst_atop,
		BlendXor        = agg::comp_op_xor,
		BlendAdd        = agg::comp_op_plus,
		BlendSub        = agg::comp_op_minus,
		BlendMultiply   = agg::comp_op_multiply,
		BlendScreen     = agg::comp_op_screen,
		BlendOverlay    = agg::comp_op_overlay,
		BlendDarken     = agg::comp_op_darken,
		BlendLighten    = agg::comp_op_lighten,
		BlendColorDodge = agg::comp_op_color_dodge,
		BlendColorBurn  = agg::comp_op_color_burn,
		BlendHardLight  = agg::comp_op_hard_light,
		BlendSoftLight  = agg::comp_op_soft_light,
		BlendDifference = agg::comp_op_difference,
		BlendExclusion  = agg::comp_op_exclusion,
		BlendContrast   = agg::comp_op_contrast
	};

	enum Direction
	{
		CW, CCW
	};

};

template<typename,typename> class Agg2DRenderer;

template<typename PixFormatSet> class Agg2D : public Agg2DBase
{
public:
	typedef typename PixFormatSet::PixFormat PixFormat;
	typedef Image<PixFormatSet> MyImage;
	typedef agg::order_bgra ComponentOrder; // Platform dependent!

	typedef agg::rgba8                                               ColorType;
	typedef agg::blender_rgba<ColorType, ComponentOrder>             Blender;
	typedef agg::comp_op_adaptor_rgba<ColorType, ComponentOrder>     BlenderComp;
	typedef agg::blender_rgba_pre<ColorType, ComponentOrder>         BlenderPre;
	typedef agg::comp_op_adaptor_rgba_pre<ColorType, ComponentOrder> BlenderCompPre;

	// JME
	//typedef agg::pixel_formats_rgba<Blender, agg::pixel32_type>    PixFormat;
	//typedef agg::pixfmt_bgra32									PixFormat;
	// JME
	//typedef agg::pixfmt_custom_blend_rgba<BlenderComp,>             PixFormatComp;
	typedef agg::pixfmt_custom_blend_rgba<BlenderComp,agg::rendering_buffer>             PixFormatComp;
	// JME
	//typedef agg::pixel_formats_rgba<BlenderPre, agg::pixel32_type> PixFormatPre;
	typedef typename PixFormatSet::PixFormatPre PixFormatPre;
	// JME
	//typedef agg::pixfmt_custom_blend_rgba<BlenderCompPre>          PixFormatCompPre;
	typedef agg::pixfmt_custom_blend_rgba<BlenderCompPre,agg::rendering_buffer>          PixFormatCompPre;

	typedef agg::renderer_base<PixFormat>        RendererBase;
	typedef agg::renderer_base<PixFormatComp>    RendererBaseComp;
	typedef agg::renderer_base<PixFormatPre>     RendererBasePre;
	typedef agg::renderer_base<PixFormatCompPre> RendererBaseCompPre;

	typedef agg::renderer_scanline_aa_solid<RendererBase>     RendererSolid;
	typedef agg::renderer_scanline_aa_solid<RendererBaseComp> RendererSolidComp;

	typedef agg::span_allocator<ColorType> SpanAllocator;
	typedef agg::pod_auto_array<ColorType, 256> GradientArray;

	typedef agg::span_gradient<ColorType, agg::span_interpolator_linear<>, agg::gradient_x,      GradientArray> LinearGradientSpan;
	typedef agg::span_gradient<ColorType, agg::span_interpolator_linear<>, agg::gradient_circle, GradientArray> RadialGradientSpan;

#ifdef AGG2D_USE_VECTORFONTS
#ifdef AGG2D_USE_FREETYPE
	typedef agg::font_engine_freetype_int32       FontEngine;
#else
	typedef agg::font_engine_win32_tt_int32       FontEngine;
#endif

	typedef agg::font_cache_manager<FontEngine>   FontCacheManager;
	typedef FontCacheManager::gray8_adaptor_type  FontRasterizer;
	typedef FontCacheManager::gray8_scanline_type FontScanline;
#endif

	typedef agg::conv_curve<agg::path_storage>    ConvCurve;
	typedef agg::conv_stroke<ConvCurve>           ConvStroke;
	typedef agg::conv_transform<ConvCurve>        PathTransform;
	typedef agg::conv_transform<ConvStroke>       StrokeTransform;
	enum Gradient
	{
		Solid,
		Linear,
		Radial
	};

public:
	template<typename,typename> friend class Agg2DRenderer;

	typedef ColorType         Color;



	struct State
	{
		RectD                           m_clipBox;

		BlendMode                       m_blendMode;
		BlendMode                       m_imageBlendMode;
		Color                           m_imageBlendColor;

		double                          m_masterAlpha;
		double                          m_antiAliasGamma;

		const agg::int8u*               m_font;

		Color                           m_fillColor;
		Color                           m_lineColor;
		GradientArray                   m_fillGradient;
		GradientArray                   m_lineGradient;

		LineCap                         m_lineCap;
		LineJoin                        m_lineJoin;

		Gradient                        m_fillGradientFlag;
		Gradient                        m_lineGradientFlag;
		agg::trans_affine               m_fillGradientMatrix;
		agg::trans_affine               m_lineGradientMatrix;
		double                          m_fillGradientD1;
		double                          m_lineGradientD1;
		double                          m_fillGradientD2;
		double                          m_lineGradientD2;

		double                          m_textAngle;
		TextAlignment                   m_textAlignX;
		TextAlignment                   m_textAlignY;
		bool                            m_textHints;
		double                          m_fontHeight;
		double                          m_fontAscent;
		double                          m_fontDescent;
		FontCacheType                   m_fontCacheType;

		double                          m_lineWidth;
		bool                            m_evenOddFlag;

		agg::trans_affine               m_transform;
		agg::trans_affine               m_affine;

	};


	~Agg2D();
	Agg2D();

	// Setup
	//-----------------------
	void  attach(unsigned char* buf, unsigned width, unsigned height, int stride);
	void  attach(MyImage& img);

	void  clipBox(double x1, double y1, double x2, double y2);
	RectD clipBox() const;

	void  clearAll(Color c);
	void  clearAll(unsigned r, unsigned g, unsigned b, unsigned a = 255);

	void  clearClipBox(Color c);
	void  clearClipBox(unsigned r, unsigned g, unsigned b, unsigned a = 255);

	unsigned width()  const { return m_rbuf.width();  }
	unsigned height() const { return m_rbuf.height(); }
	unsigned stride() const { return m_rbuf.stride(); }

	// Conversions
	//-----------------------
	void   worldToScreen(double& x, double& y) const;
	void   screenToWorld(double& x, double& y) const;
	double worldToScreen(double scalar) const;
	double screenToWorld(double scalar) const;
	void   alignPoint(double& x, double& y) const;
	bool   inBox(double worldX, double worldY) const;

	// General Attributes
	//-----------------------
	void blendMode(BlendMode m);
	BlendMode blendMode() const;

	void imageBlendMode(BlendMode m);
	BlendMode imageBlendMode() const;

	void imageBlendColor(Color c);
	void imageBlendColor(unsigned r, unsigned g, unsigned b, unsigned a = 255);
	Color imageBlendColor() const;

	void masterAlpha(double a);
	double masterAlpha() const;

	void antiAliasGamma(double g);
	double antiAliasGamma() const;

	void font(const agg::int8u* font) { m_font = font; }
	const agg::int8u* font() { return m_font; }

	void fillColor(Color c);
	void fillColor(unsigned r, unsigned g, unsigned b, unsigned a = 255);
	void noFill();

	void lineColor(Color c);
	void lineColor(unsigned r, unsigned g, unsigned b, unsigned a = 255);
	void noLine();

	Color fillColor() const;
	Color lineColor() const;

	void fillLinearGradient(double x1, double y1, double x2, double y2, Color c1, Color c2, double profile=1.0);
	void lineLinearGradient(double x1, double y1, double x2, double y2, Color c1, Color c2, double profile=1.0);

	void fillRadialGradient(double x, double y, double r, Color c1, Color c2, double profile=1.0);
	void lineRadialGradient(double x, double y, double r, Color c1, Color c2, double profile=1.0);

	void fillRadialGradient(double x, double y, double r, Color c1, Color c2, Color c3);
	void lineRadialGradient(double x, double y, double r, Color c1, Color c2, Color c3);

	void fillRadialGradient(double x, double y, double r);
	void lineRadialGradient(double x, double y, double r);

	void lineWidth(double w);
	double lineWidth() const;

	void lineCap(LineCap cap);
	LineCap lineCap() const;

	void lineJoin(LineJoin join);
	LineJoin lineJoin() const;

	void fillEvenOdd(bool evenOddFlag);
	bool fillEvenOdd() const;

	// Transformations
	//-----------------------
	Transformations transformations() const;
	void transformations(const Transformations& tr);

	const Affine& affine() const;
	void          affine(const Affine&);

	void resetTransformations();
	void matrix(const Affine& tr);
	void matrix(const Transformations& tr);
	void rotate(double angle);
	void rotate(double angle, double cx, double cy);
	void scale(double s);
	void scale(double sx, double sy);
	void skew(double sx, double sy);
	void translate(double x, double y);
	void parallelogram(double x1, double y1, double x2, double y2, const double* para);
	void viewport(double worldX1,  double worldY1,  double worldX2,  double worldY2,
		double screenX1, double screenY1, double screenX2, double screenY2,
		ViewportOption opt=XMidYMid, WindowFitLogic fl = WindowFitLogic_meet);

	// Basic Shapes
	//-----------------------
	void line(double x1, double y1, double x2, double y2);
	void triangle(double x1, double y1, double x2, double y2, double x3, double y3);
	void rectangle(double x1, double y1, double x2, double y2);
	void roundedRect(double x1, double y1, double x2, double y2, double r);
	void roundedRect(double x1, double y1, double x2, double y2, double rx, double ry);
	void roundedRect(double x1, double y1, double x2, double y2,
		double rxBottom, double ryBottom,
		double rxTop,    double ryTop);
	void ellipse(double cx, double cy, double rx, double ry);
	void arc(double cx, double cy, double rx, double ry, double start, double sweep);
	void star(double cx, double cy, double r1, double r2, double startAngle, int numRays);
	void curve(double x1, double y1, double x2, double y2, double x3, double y3);
	void curve(double x1, double y1, double x2, double y2, double x3, double y3, double x4, double y4);
	void polygon(double* xy, int numPoints);
	void polyline(double* xy, int numPoints);


	// Text
	//-----------------------
#ifdef AGG2D_USE_VECTORFONTS
	void   flipText(bool flip);
	void   font(const char* fileName, double height,
		bool bold = false,
		bool italic = false,
		FontCacheType ch = RasterFontCache,
		double angle = 0.0);
	double fontHeight() const;
	double fontAscent() const;

	void   textAlignment(TextAlignment alignX, TextAlignment alignY);
	bool   textHints() const;
	void   textHints(bool hints);
	double textWidth(const char* str, unsigned int len);
	double textWidth(const wchar_t* str, unsigned int len);
	void   text(double x, double y, const char* str, unsigned int len, bool roundOff=false, double dx=0.0, double dy=0.0);
	void   text(double x, double y, const wchar_t* str, unsigned int len, bool roundOff=false, double dx=0.0, double dy=0.0);

	double textWidth(const char* str);
	void   text(double x, double y, const char* str, bool roundOff=false, double dx=0.0, double dy=0.0);
#endif

	// Path commands
	//-----------------------
	void resetPath();

	void moveTo(double x, double y);
	void moveRel(double dx, double dy);

	void lineTo(double x, double y);
	void lineRel(double dx, double dy);

	void horLineTo(double x);
	void horLineRel(double dx);

	void verLineTo(double y);
	void verLineRel(double dy);

	void arcTo(double rx, double ry,
		double angle,
		bool largeArcFlag,
		bool sweepFlag,
		double x, double y);

	void arcRel(double rx, double ry,
		double angle,
		bool largeArcFlag,
		bool sweepFlag,
		double dx, double dy);

	void quadricCurveTo(double xCtrl, double yCtrl,
		double xTo,   double yTo);
	void quadricCurveRel(double dxCtrl, double dyCtrl,
		double dxTo,   double dyTo);
	void quadricCurveTo(double xTo, double yTo);
	void quadricCurveRel(double dxTo, double dyTo);

	void cubicCurveTo(double xCtrl1, double yCtrl1,
		double xCtrl2, double yCtrl2,
		double xTo,    double yTo);

	void cubicCurveRel(double dxCtrl1, double dyCtrl1,
		double dxCtrl2, double dyCtrl2,
		double dxTo,    double dyTo);

	void cubicCurveTo(double xCtrl2, double yCtrl2,
		double xTo,    double yTo);

	void cubicCurveRel(double xCtrl2, double yCtrl2,
		double xTo,    double yTo);

	void addEllipse(double cx, double cy, double rx, double ry, Direction dir);
	void closePolygon();

	void drawPath(DrawPathFlag flag = FillAndStroke);
	void drawPathNoTransform(DrawPathFlag flag = FillAndStroke);


	// Image Transformations
	//-----------------------
	void imageFilter(ImageFilter f);
	ImageFilter imageFilter() const;

	void imageResample(ImageResample f);
	ImageResample imageResample() const;

	//---------
	//if anyone can figure out how to put these in the .inl file, theyre more than welcome to. I couldnt declare them correctly to match
	//the .h file declaration

	AGG2D_IMAGE_TEMPLATE void transformImage(const TIMAGE& img,
		int imgX1,    int imgY1,    int imgX2,    int imgY2,
		double dstX1, double dstY1, double dstX2, double dstY2)
	{
		resetPath();
		moveTo(dstX1, dstY1);
		lineTo(dstX2, dstY1);
		lineTo(dstX2, dstY2);
		lineTo(dstX1, dstY2);
		closePolygon();
		double parallelogram[6] = { dstX1, dstY1, dstX2, dstY1, dstX2, dstY2 };
		renderImage <typename ImagePixFormatSet::ImagePixFormat> (img, imgX1, imgY1, imgX2, imgY2, parallelogram);
	}

	AGG2D_IMAGE_TEMPLATE void transformImage(const TIMAGE& img,
		double dstX1, double dstY1, double dstX2, double dstY2)
	{
		resetPath();
		moveTo(dstX1, dstY1);
		lineTo(dstX2, dstY1);
		lineTo(dstX2, dstY2);
		lineTo(dstX1, dstY2);
		closePolygon();
		double parallelogram[6] = { dstX1, dstY1, dstX2, dstY1, dstX2, dstY2 };
		renderImage(img, 0, 0, img.renBuf.width(), img.renBuf.height(), parallelogram);
	}


	AGG2D_IMAGE_TEMPLATE void transformImage(const TIMAGE& img,
		int imgX1, int imgY1, int imgX2, int imgY2,
		const double* parallelogram)
	{
		resetPath();
		moveTo(parallelogram[0], parallelogram[1]);
		lineTo(parallelogram[2], parallelogram[3]);
		lineTo(parallelogram[4], parallelogram[5]);
		lineTo(parallelogram[0] + parallelogram[4] - parallelogram[2],
			parallelogram[1] + parallelogram[5] - parallelogram[3]);
		closePolygon();
		renderImage(img, imgX1, imgY1, imgX2, imgY2, parallelogram);
	}


	AGG2D_IMAGE_TEMPLATE void transformImage(const TIMAGE& img, const double* parallelogram)
	{
		resetPath();
		moveTo(parallelogram[0], parallelogram[1]);
		lineTo(parallelogram[2], parallelogram[3]);
		lineTo(parallelogram[4], parallelogram[5]);
		lineTo(parallelogram[0] + parallelogram[4] - parallelogram[2],
			parallelogram[1] + parallelogram[5] - parallelogram[3]);
		closePolygon();
		renderImage(img, 0, 0, img.renBuf.width(), img.renBuf.height(), parallelogram);
	}



	AGG2D_IMAGE_TEMPLATE void transformImagePath(const TIMAGE& img,
		int imgX1,    int imgY1,    int imgX2,    int imgY2,
		double dstX1, double dstY1, double dstX2, double dstY2)
	{
		double parallelogram[6] = { dstX1, dstY1, dstX2, dstY1, dstX2, dstY2 };
		renderImage(img, imgX1, imgY1, imgX2, imgY2, parallelogram);
	}


	AGG2D_IMAGE_TEMPLATE void transformImagePath(const TIMAGE& img,
		double dstX1, double dstY1, double dstX2, double dstY2)
	{
		double parallelogram[6] = { dstX1, dstY1, dstX2, dstY1, dstX2, dstY2 };
		renderImage(img, 0, 0, img.renBuf.width(), img.renBuf.height(), parallelogram);
	}



	AGG2D_IMAGE_TEMPLATE void transformImagePath(const TIMAGE& img,
		int imgX1, int imgY1, int imgX2, int imgY2,
		const double* parallelogram)
	{
		renderImage(img, imgX1, imgY1, imgX2, imgY2, parallelogram);
	}


	AGG2D_IMAGE_TEMPLATE  void transformImagePath(const TIMAGE& img, const double* parallelogram)
	{
		renderImage(img, 0, 0, img.renBuf.width(), img.renBuf.height(), parallelogram);
	}



	// Image Blending (no transformations available)
	AGG2D_IMAGE_TEMPLATE void blendImage(TIMAGE& img,
		int imgX1, int imgY1, int imgX2, int imgY2,
		double dstX, double dstY, unsigned alpha=255)
	{
		worldToScreen(dstX, dstY);
		PixFormat pixF(img.renBuf);
		// JME
		//agg::rect r(imgX1, imgY1, imgX2, imgY2);
		Rect r(imgX1, imgY1, imgX2, imgY2);
		if(m_blendMode == BlendAlpha)
		{
			m_renBasePre.blend_from(pixF, &r, int(dstX)-imgX1, int(dstY)-imgY1, alpha);
		}
		else
		{
			m_renBaseCompPre.blend_from(pixF, &r, int(dstX)-imgX1, int(dstY)-imgY1, alpha);
		}
	}


	void renderText(double dstX, double dstY, const std::string& str)
	{
		worldToScreen(dstX, dstY);
		PixFormat pixF(m_rbuf);
		//Rect r(imgX1, imgY1, imgX2, imgY2);

		typedef agg::glyph_raster_bin<agg::rgba8> glyph_gen;
		glyph_gen glyph(0);

		if(m_blendMode == BlendAlpha)
		{
			typedef agg::renderer_base<PixFormatPre> ren_base;
			agg::renderer_raster_htext_solid<ren_base, glyph_gen> rt(m_renBasePre,glyph);
			rt.color(m_lineColor);
			glyph.font(m_font);
			rt.render_text(dstX, dstY, str.c_str(), true); //flipy
		}
		else
		{
			typedef agg::renderer_base<PixFormatCompPre> ren_base;
			agg::renderer_raster_htext_solid<ren_base, glyph_gen> rt(m_renBaseCompPre,glyph);
			rt.color(m_lineColor);
			glyph.font(m_font);
			rt.render_text(dstX, dstY, str.c_str(), true); //flipy
		}
	}


	AGG2D_IMAGE_TEMPLATE void blendImage(TIMAGE& img, double dstX, double dstY, unsigned alpha=255)
	{
		worldToScreen(dstX, dstY);
		PixFormat pixF(img.renBuf);
		m_renBasePre.blend_from(pixF, 0, int(dstX), int(dstY), alpha);
		if(m_blendMode == BlendAlpha)
		{
			m_renBasePre.blend_from(pixF, 0, int(dstX), int(dstY), alpha);
		}
		else
		{
			m_renBaseCompPre.blend_from(pixF, 0, int(dstX), int(dstY), alpha);
		}
	}



	// Copy image directly, together with alpha-channel
	AGG2D_IMAGE_TEMPLATE void copyImage(TIMAGE& img,
		int imgX1, int imgY1, int imgX2, int imgY2,
		double dstX, double dstY)
	{
		worldToScreen(dstX, dstY);
		// JME
		//agg::rect r(imgX1, imgY1, imgX2, imgY2);
		Rect r(imgX1, imgY1, imgX2, imgY2);
		m_renBase.copy_from(img.renBuf, &r, int(dstX)-imgX1, int(dstY)-imgY1);
	}

	AGG2D_IMAGE_TEMPLATE void copyImage(TIMAGE& img, double dstX, double dstY)
	{
		worldToScreen(dstX, dstY);
		m_renBase.copy_from(img.renBuf, 0, int(dstX), int(dstY));
	}

	// State
	//-----------------------

	void saveStateTo(State& st);
	void restoreStateFrom(const State& st);



	// Auxiliary
	//-----------------------
	static double pi() { return agg::pi; }
	static double deg2Rad(double v) { return v * agg::pi / 180.0; }
	static double rad2Deg(double v) { return v * 180.0 / agg::pi; }

	PixFormat & pixFormat() { return m_pixFormat; }
	agg::rendering_buffer & buf() { return m_rbuf; }

private:
	void render(bool fillColor);

#ifdef AGG2D_USE_VECTORFONTS
#if !defined( UNDER_CE )
	void render(FontRasterizer& ras, FontScanline& sl);
#endif
#endif

	void addLine(double x1, double y1, double x2, double y2);
	void updateRasterizerGamma();
	AGG2D_IMAGE_TEMPLATE void renderImage(const TIMAGE& img, int x1, int y1, int x2, int y2, const double* parl)
	{
		agg::trans_affine mtx((double)x1,
			(double)y1,
			(double)x2,
			(double)y2,
			parl);
		mtx *= m_transform;
		mtx.invert();

		m_rasterizer.reset();
		m_rasterizer.add_path(m_pathTransform);

		typedef agg::span_interpolator_linear<agg::trans_affine> Interpolator;
		Interpolator interpolator(mtx);

		if(m_blendMode == BlendAlpha)
		{
			// JME audit -
			Agg2DRenderer<PixFormatSet,ImagePixFormatSet>::renderImage(*this, img, m_renBasePre, interpolator);
		}
		else
		{
			Agg2DRenderer<PixFormatSet,ImagePixFormatSet>::renderImage(*this, img, m_renBaseCompPre, interpolator);
		}
	}


	void updateTransformations();

	agg::rendering_buffer           m_rbuf;
	PixFormat                       m_pixFormat;
	PixFormatComp                   m_pixFormatComp;
	PixFormatPre                    m_pixFormatPre;
	PixFormatCompPre                m_pixFormatCompPre;
	RendererBase                    m_renBase;
	RendererBaseComp                m_renBaseComp;
	RendererBasePre                 m_renBasePre;
	RendererBaseCompPre             m_renBaseCompPre;
	RendererSolid                   m_renSolid;
	RendererSolidComp               m_renSolidComp;

	SpanAllocator                   m_allocator;
	RectD                           m_clipBox;

	BlendMode                       m_blendMode;
	BlendMode                       m_imageBlendMode;
	Color                           m_imageBlendColor;

	agg::scanline_u8                m_scanline;
	agg::rasterizer_scanline_aa<>   m_rasterizer;

	double                          m_masterAlpha;
	double                          m_antiAliasGamma;

	const agg::int8u*               m_font;

	Color                           m_fillColor;
	Color                           m_lineColor;
	GradientArray                   m_fillGradient;
	GradientArray                   m_lineGradient;

	LineCap                         m_lineCap;
	LineJoin                        m_lineJoin;

	Gradient                        m_fillGradientFlag;
	Gradient                        m_lineGradientFlag;
	agg::trans_affine               m_fillGradientMatrix;
	agg::trans_affine               m_lineGradientMatrix;
	double                          m_fillGradientD1;
	double                          m_lineGradientD1;
	double                          m_fillGradientD2;
	double                          m_lineGradientD2;

	double                          m_textAngle;
	TextAlignment                   m_textAlignX;
	TextAlignment                   m_textAlignY;
	bool                            m_textHints;
	double                          m_fontHeight;
	double                          m_fontAscent;
	double                          m_fontDescent;
	FontCacheType                   m_fontCacheType;

	ImageFilter                     m_imageFilter;
	ImageResample                   m_imageResample;
	agg::image_filter_lut           m_imageFilterLut;

	agg::span_interpolator_linear<> m_fillGradientInterpolator;
	agg::span_interpolator_linear<> m_lineGradientInterpolator;

	agg::gradient_x                 m_linearGradientFunction;
	agg::gradient_circle            m_radialGradientFunction;

	double                          m_lineWidth;
	bool                            m_evenOddFlag;

	double                          m_start_x;
	double                          m_start_y;

	agg::path_storage               m_path;
	agg::trans_affine               m_transform;

	agg::trans_affine               m_viewport;
	agg::trans_affine               m_affine;

	ConvCurve                       m_convCurve;
	ConvStroke                      m_convStroke;

	PathTransform                   m_pathTransform;
	StrokeTransform                 m_strokeTransform;

#ifdef AGG2D_USE_VECTORFONTS
#ifndef AGG2D_USE_FREETYPE
	HDC                             m_fontDC;
#endif
	FontEngine                      m_fontEngine;
	FontCacheManager                m_fontCacheManager;
#endif
};



#include "agg2d.inl"

#endif
