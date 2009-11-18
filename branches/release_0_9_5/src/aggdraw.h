//The MIT License
//
//Copyright (c) 2009 DeSmuME team
//
//Permission is hereby granted, free of charge, to any person obtaining a copy
//of this software and associated documentation files (the "Software"), to deal
//in the Software without restriction, including without limitation the rights
//to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//copies of the Software, and to permit persons to whom the Software is
//furnished to do so, subject to the following conditions:
//
//The above copyright notice and this permission notice shall be included in
//all copies or substantial portions of the Software.
//
//THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//THE SOFTWARE.


//This file contains code designed to be used by hud and lua systems in any emulator


#ifndef _AGGDRAW_H_
#define _AGGDRAW_H_

#include "agg_color_rgba.h"
#include "agg_rendering_buffer.h"

#include "agg_renderer_base.h"
#include "agg_renderer_primitives.h"
#include "agg_renderer_scanline.h"
#include "agg_bounding_rect.h"

#include "agg_renderer_mclip.h"
#include "agg_renderer_outline_aa.h"
#include "agg_renderer_markers.h"

#include "agg_pixfmt_rgb.h"
#include "agg_pixfmt_rgba.h"
#include "agg_pixfmt_rgb_packed.h"

#include "agg2d.h"

typedef agg::rgba8 AggColor;

namespace agg
{
	//NOTE - these blenders are necessary to change the rgb order from the defaults, which are incorrect for us

	//this custom blender does more correct blending math than the default
	//which is necessary or else drawing transparent pixels on (31,31,31) will yield (30,30,30)
    struct my_blender_rgb555_pre
    {
        typedef rgba8 color_type;
        typedef color_type::value_type value_type;
        typedef color_type::calc_type calc_type;
        typedef int16u pixel_type;

        static AGG_INLINE void blend_pix(pixel_type* p, 
                                         unsigned cr, unsigned cg, unsigned cb,
                                         unsigned alpha, 
                                         unsigned cover)
        {
			//not sure whether this is right...
            alpha = color_type::base_mask - alpha;
            pixel_type rgb = *p;
            calc_type b = (rgb >> 10) & 31;
            calc_type g = (rgb >> 5) & 31;
            calc_type r = (rgb ) & 31;
			b = ((b+1)*(alpha+1) + (cb)*(cover)-1)>>8;
			g = ((g+1)*(alpha+1) + (cg)*(cover)-1)>>8;
			r = ((r+1)*(alpha+1) + (cr)*(cover)-1)>>8;
			*p = (b<<10)|(g<<5)|r;
        }

        static AGG_INLINE pixel_type make_pix(unsigned r, unsigned g, unsigned b)
        {
            return (pixel_type)(((b & 0xF8) << 7) | 
                                ((g & 0xF8) << 2) | 
                                 (r >> 3) | 0x8000);
        }

        static AGG_INLINE color_type make_color(pixel_type p)
        {
            return color_type((p << 3) & 0xF8,
                              (p >> 2) & 0xF8, 
                              (p >> 7) & 0xF8);
        }
    };

	 struct my_blender_rgb555
    {
        typedef rgba8 color_type;
        typedef color_type::value_type value_type;
        typedef color_type::calc_type calc_type;
        typedef int16u pixel_type;

        static AGG_INLINE void blend_pix(pixel_type* p, 
                                         unsigned cr, unsigned cg, unsigned cb,
                                         unsigned alpha, 
                                         unsigned)
        {
            pixel_type rgb = *p;
            calc_type b = (rgb >> 7) & 0xF8;
            calc_type g = (rgb >> 2) & 0xF8;
            calc_type r = (rgb << 3) & 0xF8;
            *p = (pixel_type)
               (((((cb - b) * alpha + (b << 8)) >> 1)  & 0x7C00) |
                ((((cg - g) * alpha + (g << 8)) >> 6)  & 0x03E0) |
                 (((cr - r) * alpha + (r << 8)) >> 11) | 0x8000);
        }

        static AGG_INLINE pixel_type make_pix(unsigned r, unsigned g, unsigned b)
        {
            return (pixel_type)(((b & 0xF8) << 7) | 
                                ((g & 0xF8) << 2) | 
                                 (r >> 3) | 0x8000);
        }

        static AGG_INLINE color_type make_color(pixel_type p)
        {
            return color_type((p << 3) & 0xF8,
                              (p >> 2) & 0xF8, 
                              (p >> 7) & 0xF8);
			
        }
    };

	 //this is a prototype span generator which should be able to generate 8888 and 555 spans
	 //it would be used in agg2d.inl renderImage()
	 //but it isn't being used yet.
	 //this will need to be completed before we can use 555 as a source imge
    template<class Order> class span_simple_blur_rgb24
    {
    public:
        //--------------------------------------------------------------------
        typedef rgba8 color_type;
        
        //--------------------------------------------------------------------
        span_simple_blur_rgb24() : m_source_image(0) {}

        //--------------------------------------------------------------------
        span_simple_blur_rgb24(const rendering_buffer& src) : 
            m_source_image(&src) {}

        //--------------------------------------------------------------------
        void source_image(const rendering_buffer& src) { m_source_image = &src; }
        const rendering_buffer& source_image() const { return *m_source_image; }

        //--------------------------------------------------------------------
        void prepare() {}

        //--------------------------------------------------------------------
        void generate(color_type* span, int x, int y, int len)
        {
            if(y < 1 || y >= int(m_source_image->height() - 1))
            {
                do
                {
                    *span++ = rgba8(0,0,0,0);
                }
                while(--len);
                return;
            }

            do
            {
                int color[4];
                color[0] = color[1] = color[2] = color[3] = 0;
                if(x > 0 && x < int(m_source_image->width()-1))
                {
                    int i = 3;
                    do
                    {
                        const int8u* ptr = m_source_image->row_ptr(y - i + 2) + (x - 1) * 3;

                        color[0] += *ptr++;
                        color[1] += *ptr++;
                        color[2] += *ptr++;
                        color[3] += 255;

                        color[0] += *ptr++;
                        color[1] += *ptr++;
                        color[2] += *ptr++;
                        color[3] += 255;

                        color[0] += *ptr++;
                        color[1] += *ptr++;
                        color[2] += *ptr++;
                        color[3] += 255;
                    }
                    while(--i);
                    color[0] /= 9;
                    color[1] /= 9;
                    color[2] /= 9;
                    color[3] /= 9;
                }
                *span++ = rgba8(color[Order::R], color[Order::G], color[Order::B], color[3]);
                ++x;
            }
            while(--len);
        }

    private:
        const rendering_buffer* m_source_image;
    };

	typedef pixfmt_alpha_blend_rgb_packed<my_blender_rgb555_pre, rendering_buffer> my_pixfmt_rgb555_pre; //----pixfmt_rgb555_pre

	typedef pixfmt_alpha_blend_rgb_packed<my_blender_rgb555, rendering_buffer> my_pixfmt_rgb555; //----pixfmt_rgb555_pre
}



typedef PixFormatSetDeclaration<agg::my_pixfmt_rgb555,agg::my_pixfmt_rgb555_pre,agg::span_simple_blur_rgb24<agg::order_rgba> > T_AGG_PF_RGB555;
typedef PixFormatSetDeclaration<agg::pixfmt_bgra32,agg::pixfmt_bgra32_pre,agg::span_simple_blur_rgb24<agg::order_rgba> > T_AGG_PF_RGBA;
class AggDrawTarget
{
public:
	AggDrawTarget()
		: empty(true)
	{
	}

protected:
	void dirty() { empty = false; }
	void undirty() { empty = true; }

public:

	bool empty;

	virtual void clear() = 0;

	virtual agg::rendering_buffer & buf() = 0;

	//returns an image for this target. you must provide the pixel type, but if it is wrong,
	//then you have just created trouble for yourself
	AGG2D_IMAGE_TEMPLATE TIMAGE image() { return TIMAGE(buf()); }

    // Setup
	virtual void  attach(unsigned char* buf, unsigned width, unsigned height, int stride) = 0;
//	virtual void  attach(Agg2DBase::Image& img) {attach(img);};

    virtual void  clipBox(double x1, double y1, double x2, double y2) = 0;
	virtual Agg2DBase::RectD clipBox() const = 0;

	virtual void  clearAll(AggColor c) = 0;
	virtual void  clearAll(unsigned r, unsigned g, unsigned b, unsigned a = 255) = 0;

	virtual void  clearClipBox(AggColor c) = 0;
	virtual void  clearClipBox(unsigned r, unsigned g, unsigned b, unsigned a = 255) = 0;

//	virtual unsigned width()  const { return Agg2DBase::m_rbuf.width();  }
//  virtual unsigned height() const { return Agg2DBase::m_rbuf.height(); }

    // Conversions
	virtual void   worldToScreen(double& x, double& y) const = 0;
	virtual void   screenToWorld(double& x, double& y) const = 0;
	virtual double worldToScreen(double scalar) const = 0;
	virtual double screenToWorld(double scalar) const = 0;
	virtual void   alignPoint(double& x, double& y) const = 0;
	virtual bool   inBox(double worldX, double worldY) const = 0;
	
    // General Attributes
	virtual void blendMode(Agg2DBase::BlendMode m) = 0;
	virtual Agg2DBase::BlendMode blendMode() = 0;

    virtual void imageBlendMode(Agg2DBase::BlendMode m) = 0;
    virtual Agg2DBase::BlendMode imageBlendMode() = 0;

	virtual void imageBlendColor(AggColor c) = 0;
    virtual void imageBlendColor(unsigned r, unsigned g, unsigned b, unsigned a = 255) = 0;
    virtual AggColor imageBlendColor() = 0;

    virtual void masterAlpha(double a) = 0;
    virtual double masterAlpha() = 0;

    virtual void antiAliasGamma(double g) = 0;
    virtual double antiAliasGamma() = 0;

//	virtual void font(const agg::int8u* font) { m_font = font; }
//	const agg::int8u* font() { return m_font; }

    virtual void fillColor(AggColor c) = 0;
    virtual void fillColor(unsigned r, unsigned g, unsigned b, unsigned a = 255) = 0;
    virtual void noFill() = 0;

    virtual void lineColor(AggColor c) = 0;
    virtual void lineColor(unsigned r, unsigned g, unsigned b, unsigned a = 255) = 0;
    virtual void noLine() = 0;

	virtual void transformImage(const Agg2DBase::Image<T_AGG_PF_RGBA> &img, double dstX1, double dstY1, double dstX2, double dstY2) = 0;
	//virtual void transformImage(const Agg2DBase::Image<T_AGG_PF_RGB555> &img, double dstX1, double dstY1, double dstX2, double dstY2) = 0;

    virtual AggColor fillColor() = 0;
    virtual AggColor lineColor() = 0;

    virtual void fillLinearGradient(double x1, double y1, double x2, double y2, AggColor c1, AggColor c2, double profile=1.0) = 0;
    virtual void lineLinearGradient(double x1, double y1, double x2, double y2, AggColor c1, AggColor c2, double profile=1.0) = 0;

    virtual void fillRadialGradient(double x, double y, double r, AggColor c1, AggColor c2, double profile=1.0) = 0;
    virtual void lineRadialGradient(double x, double y, double r, AggColor c1, AggColor c2, double profile=1.0) = 0;

    virtual void fillRadialGradient(double x, double y, double r, AggColor c1, AggColor c2, AggColor c3) = 0;
    virtual void lineRadialGradient(double x, double y, double r, AggColor c1, AggColor c2, AggColor c3) = 0;

    virtual void fillRadialGradient(double x, double y, double r) = 0;
    virtual void lineRadialGradient(double x, double y, double r) = 0;

    virtual void lineWidth(double w) = 0;
    virtual double lineWidth() = 0;

	virtual void lineCap(Agg2DBase::LineCap cap) = 0;
    virtual Agg2DBase::LineCap lineCap() = 0;

    virtual void lineJoin(Agg2DBase::LineJoin join) = 0;
    virtual Agg2DBase::LineJoin lineJoin() = 0;

    virtual void fillEvenOdd(bool evenOddFlag) = 0;
    virtual bool fillEvenOdd() = 0;

    // Transformations
    virtual Agg2DBase::Transformations transformations() = 0;
    virtual void transformations(const Agg2DBase::Transformations& tr) = 0;

    virtual const Agg2DBase::Affine& affine() = 0;
//    virtual void affine(const Agg2DBase::Affine&) = 0;

    virtual void resetTransformations() = 0;
    virtual void matrix(const Agg2DBase::Affine& tr) = 0;
    virtual void matrix(const Agg2DBase::Transformations& tr) = 0;
    virtual void rotate(double angle) = 0;
    virtual void rotate(double angle, double cx, double cy) = 0;
    virtual void scale(double s) = 0;
    virtual void scale(double sx, double sy) = 0;
    virtual void skew(double sx, double sy) = 0;
    virtual void translate(double x, double y) = 0;
    virtual void parallelogram(double x1, double y1, double x2, double y2, const double* para) = 0;
    virtual void viewport(double worldX1,  double worldY1,  double worldX2,  double worldY2, double screenX1, double screenY1, double screenX2, double screenY2, Agg2DBase::ViewportOption opt=Agg2DBase::XMidYMid, Agg2DBase::WindowFitLogic fl = Agg2DBase::WindowFitLogic_meet) = 0;

    // Basic Shapes
    virtual void line(double x1, double y1, double x2, double y2) = 0;
    virtual void triangle(double x1, double y1, double x2, double y2, double x3, double y3) = 0;
    virtual void rectangle(double x1, double y1, double x2, double y2) = 0;
    virtual void roundedRect(double x1, double y1, double x2, double y2, double r) = 0;
    virtual void roundedRect(double x1, double y1, double x2, double y2, double rx, double ry) = 0;
    virtual void roundedRect(double x1, double y1, double x2, double y2, double rxBottom, double ryBottom, double rxTop,    double ryTop) = 0;
    virtual void ellipse(double cx, double cy, double rx, double ry) = 0;
    virtual void arc(double cx, double cy, double rx, double ry, double start, double sweep) = 0;
    virtual void star(double cx, double cy, double r1, double r2, double startAngle, int numRays) = 0;
    virtual void curve(double x1, double y1, double x2, double y2, double x3, double y3) = 0;
    virtual void curve(double x1, double y1, double x2, double y2, double x3, double y3, double x4, double y4) = 0;
    virtual void polygon(double* xy, int numPoints) = 0;
    virtual void polyline(double* xy, int numPoints) = 0;

	// Path commands
    virtual void resetPath() = 0;

    virtual void moveTo(double x, double y)= 0;
    virtual void moveRel(double dx, double dy) = 0;

    virtual void lineTo(double x, double y) = 0;
    virtual void lineRel(double dx, double dy) = 0;

    virtual void horLineTo(double x) = 0;
    virtual void horLineRel(double dx) = 0;

    virtual void verLineTo(double y) = 0;
    virtual void verLineRel(double dy) = 0;

    virtual void arcTo(double rx, double ry,double angle, bool largeArcFlag,bool sweepFlag,double x, double y) = 0;

    virtual void arcRel(double rx, double ry,double angle, bool largeArcFlag,bool sweepFlag,double dx, double dy) = 0;

    virtual void quadricCurveTo(double xCtrl, double yCtrl,double xTo,   double yTo) = 0;
    virtual void quadricCurveRel(double dxCtrl, double dyCtrl,double dxTo,   double dyTo) = 0;
    virtual void quadricCurveTo(double xTo, double yTo) = 0;
    virtual void quadricCurveRel(double dxTo, double dyTo) = 0;

    virtual void cubicCurveTo(double xCtrl1, double yCtrl1,double xCtrl2, double yCtrl2,double xTo,    double yTo) = 0;
    virtual void cubicCurveRel(double dxCtrl1, double dyCtrl1,double dxCtrl2, double dyCtrl2,double dxTo,    double dyTo) = 0;
    virtual void cubicCurveTo(double xCtrl2, double yCtrl2,double xTo,    double yTo) = 0;
    virtual void cubicCurveRel(double xCtrl2, double yCtrl2,double xTo,    double yTo) = 0;

	virtual void addEllipse(double cx, double cy, double rx, double ry, Agg2DBase::Direction dir) = 0;
    virtual void closePolygon() = 0;

    virtual void drawPath(Agg2DBase::DrawPathFlag flag = Agg2DBase::FillAndStroke) = 0;
//	virtual void drawPathNoTransform(Agg2DBase::DrawPathFlag flag = Agg2DBase::FillAndStroke) = 0;

    // Image Transformations
    virtual void imageFilter(Agg2DBase::ImageFilter f) = 0;
    virtual Agg2DBase::ImageFilter imageFilter() = 0;

    virtual void imageResample(Agg2DBase::ImageResample f) = 0;
    virtual Agg2DBase::ImageResample imageResample() = 0;
	static const agg::int8u* lookupFont(const std::string& name);
	virtual void setFont(const std::string& name) = 0;
	virtual void renderText(double dstX, double dstY, const std::string& str) = 0;
	virtual void renderTextDropshadowed(double dstX, double dstY, const std::string& str)
	{
		AggColor lineColorOld = lineColor();
		if(lineColorOld.r+lineColorOld.g+lineColorOld.b<192)
			lineColor(255-lineColorOld.r,255-lineColorOld.g,255-lineColorOld.b);
		else
			lineColor(0,0,0);
		renderText(dstX-1,dstY-1,str);
		renderText(dstX,dstY-1,str);
		renderText(dstX+1,dstY-1,str);
		renderText(dstX-1,dstY,str);
		renderText(dstX+1,dstY,str);
		renderText(dstX-1,dstY+1,str);
		renderText(dstX,dstY+1,str);
		renderText(dstX+1,dstY+1,str);
		lineColor(lineColorOld);
		renderText(dstX,dstY,str);
	}


	// Auxiliary
    virtual double pi() { return agg::pi; }
    virtual double deg2Rad(double v) { return v * agg::pi / 180.0; }
    virtual double rad2Deg(double v) { return v * 180.0 / agg::pi; }
};


template<typename PixFormatSet> 
class AggDrawTargetImplementation : public AggDrawTarget, public Agg2D<PixFormatSet>
{
public:
	typedef typename PixFormatSet::PixFormat pixfmt;
	typedef typename pixfmt::color_type color_type;

	typedef Agg2D<PixFormatSet> BASE;
	AggDrawTargetImplementation(agg::int8u* buf, int width, int height, int stride)
	{
		BASE::attach(buf,width,height,stride);

		BASE::viewport(0, 0, width-1, height-1, 0, 0, width-1, height-1, TAGG2D::Anisotropic);
	}

	virtual void clear() { 
		if(!empty)
		{
			BASE::clearAll(0,0,0,0);
			undirty();
		}
	}

	virtual agg::rendering_buffer & buf() { return BASE::buf(); }
	typename BASE::MyImage image() { return BASE::MyImage(buf()); }

    // Setup
	virtual void  attach(unsigned char* buf, unsigned width, unsigned height, int stride) {BASE::attach(buf, width, height, stride);};
//	virtual void  attach(Agg2DBase::Image& img) {attach(img);};

    virtual void  clipBox(double x1, double y1, double x2, double y2) { BASE::clipBox(x1,y1,x2,y2); }
	virtual Agg2DBase::RectD clipBox() const {return BASE::clipBox();};

	virtual void  clearAll(AggColor c) {BASE::clearAll(c);};
	virtual void  clearAll(unsigned r, unsigned g, unsigned b, unsigned a = 255) {BASE::clearAll(r, g, b, a);};

	virtual void  clearClipBox(AggColor c) {BASE::clearClipBox(c);};
	virtual void  clearClipBox(unsigned r, unsigned g, unsigned b, unsigned a = 255) {BASE::clearClipBox(r, g, b, a);};

//	virtual unsigned width()  const { return Agg2DBase::m_rbuf.width();  }
//  virtual unsigned height() const { return Agg2DBase::m_rbuf.height(); }

    // Conversions
	virtual void   worldToScreen(double& x, double& y) const {BASE::worldToScreen(x, y);};
	virtual void   screenToWorld(double& x, double& y) const {BASE::screenToWorld(x, y);};
	virtual double worldToScreen(double scalar) const {return BASE::worldToScreen(scalar);};
	virtual double screenToWorld(double scalar) const {return BASE::screenToWorld(scalar);};
	virtual void   alignPoint(double& x, double& y) const {BASE::alignPoint(x, y);};
	virtual bool   inBox(double worldX, double worldY) const {return BASE::inBox(worldX, worldY);};

    // General Attributes
	virtual void blendMode(Agg2DBase::BlendMode m) {BASE::blendMode(m);};
	virtual Agg2DBase::BlendMode blendMode() {return BASE::blendMode();};

	virtual void imageBlendMode(Agg2DBase::BlendMode m) {BASE::imageBlendMode(m);};
	virtual Agg2DBase::BlendMode imageBlendMode() {return BASE::imageBlendMode();};

	virtual void imageBlendColor(AggColor c) {BASE::imageBlendColor(c);};
	virtual void imageBlendColor(unsigned r, unsigned g, unsigned b, unsigned a = 255) {BASE::imageBlendColor(r, g, b, a);};
	virtual AggColor imageBlendColor() {return BASE::imageBlendColor();};

	virtual void masterAlpha(double a) {BASE::masterAlpha(a);};
	virtual double masterAlpha() {return BASE::masterAlpha();};

	virtual void antiAliasGamma(double g) {BASE::antiAliasGamma(g);};
	virtual double antiAliasGamma() {return BASE::antiAliasGamma();};

//	virtual void font(const agg::int8u* font) { m_font = font; }
//	const agg::int8u* font() { return m_font; }

	virtual void fillColor(AggColor c) {BASE::fillColor(c);};
	virtual void fillColor(unsigned r, unsigned g, unsigned b, unsigned a = 255) {BASE::fillColor(r,g,b,a);};
	virtual void noFill() {BASE::noFill();};

	virtual void lineColor(AggColor c) {BASE::lineColor(c);};
	virtual void lineColor(unsigned r, unsigned g, unsigned b, unsigned a = 255) {BASE::lineColor(r,g,b,a);};
	virtual void noLine() {BASE::noLine();};

	virtual AggColor fillColor() {return BASE::fillColor();};
	virtual AggColor lineColor() {return BASE::lineColor();};

	virtual void fillLinearGradient(double x1, double y1, double x2, double y2, AggColor c1, AggColor c2, double profile=1.0) {BASE::fillLinearGradient(x1, y1, x2, y2, c1, c2, profile);};
	virtual void lineLinearGradient(double x1, double y1, double x2, double y2, AggColor c1, AggColor c2, double profile=1.0) {BASE::lineLinearGradient(x1, y1, x2, y2, c1, c2, profile);};

	virtual void fillRadialGradient(double x, double y, double r, AggColor c1, AggColor c2, double profile=1.0) {BASE::fillRadialGradient(x, y, r, c1, c2, profile);};
	virtual void lineRadialGradient(double x, double y, double r, AggColor c1, AggColor c2, double profile=1.0) {BASE::lineRadialGradient(x, y, r, c1, c2, profile);};

	virtual void fillRadialGradient(double x, double y, double r, AggColor c1, AggColor c2, AggColor c3) {BASE::fillRadialGradient(x, y, r, c1, c2, c3);};
	virtual void lineRadialGradient(double x, double y, double r, AggColor c1, AggColor c2, AggColor c3) {BASE::lineRadialGradient(x, y, r, c1, c2, c3);};
	virtual void fillRadialGradient(double x, double y, double r) {BASE::fillRadialGradient(x, y, r);};
	virtual void lineRadialGradient(double x, double y, double r) {BASE::lineRadialGradient(x, y, r);};

	virtual void lineWidth(double w) {BASE::lineWidth(w);};
	virtual double lineWidth() {return BASE::lineWidth();};

	virtual void lineCap(Agg2DBase::LineCap cap) {BASE::lineCap(cap);};
	virtual Agg2DBase::LineCap lineCap() {return BASE::lineCap();};

	virtual void lineJoin(Agg2DBase::LineJoin join) {BASE::lineJoin(join);};
	virtual Agg2DBase::LineJoin lineJoin() {return BASE::lineJoin();};

	virtual void fillEvenOdd(bool evenOddFlag) {BASE::fillEvenOdd(evenOddFlag);};
	virtual bool fillEvenOdd() {return BASE::fillEvenOdd();};

	virtual void transformImage(const Agg2DBase::Image<T_AGG_PF_RGBA>& img, double dstX1, double dstY1, double dstX2, double dstY2) { BASE::transformImage(img,dstX1,dstY1,dstX2,dstY2); }
	//virtual void transformImage(const Agg2DBase::Image<T_AGG_PF_RGB555> &img, double dstX1, double dstY1, double dstX2, double dstY2)  { BASE::transformImage(img,dstX1,dstY1,dstX2,dstY2); }

    // Transformations
	virtual Agg2DBase::Transformations transformations() {return BASE::transformations();};
	virtual void transformations(const Agg2DBase::Transformations& tr) {BASE::transformations(tr);};

	virtual const Agg2DBase::Affine& affine() {return BASE::affine();};
//	virtual void affine(const Agg2DBase::Affine&) {BASE::affine();};

	virtual void resetTransformations() {BASE::resetTransformations();};
	virtual void matrix(const Agg2DBase::Affine& tr) {BASE::matrix(tr);};
	virtual void matrix(const Agg2DBase::Transformations& tr) {BASE::matrix(tr);};
	virtual void rotate(double angle) {BASE::rotate(angle);};
	virtual void rotate(double angle, double cx, double cy) {BASE::rotate(angle, cx, cy);};
	virtual void scale(double s) {BASE::scale(s);};
	virtual void scale(double sx, double sy) {BASE::scale(sx, sy);};
	virtual void skew(double sx, double sy) {BASE::skew(sx, sy);};
	virtual void translate(double x, double y) {BASE::translate(x, y);};
	virtual void parallelogram(double x1, double y1, double x2, double y2, const double* para) {BASE::parallelogram(x1, y1, x2, y2, para);};
	virtual void viewport(double worldX1,  double worldY1,  double worldX2,  double worldY2, double screenX1, double screenY1, double screenX2, double screenY2, Agg2DBase::ViewportOption opt=Agg2DBase::XMidYMid, Agg2DBase::WindowFitLogic fl = Agg2DBase::WindowFitLogic_meet) {BASE::viewport(worldX1, worldY1, worldX2, worldY2, screenX1, screenY1, screenX2, screenY2, opt, fl);};

	// Basic Shapes
	virtual void line(double x1, double y1, double x2, double y2) {BASE::line(x1, y1, x2, y2);};
	virtual void triangle(double x1, double y1, double x2, double y2, double x3, double y3) {BASE::triangle(x1, y1, x2, y2, x3, y3);};
	virtual void rectangle(double x1, double y1, double x2, double y2) {BASE::rectangle(x1, y1, x2, y2);};
	virtual void roundedRect(double x1, double y1, double x2, double y2, double r) { dirty(); BASE::roundedRect(x1,y1,x2,y2,r); }
    virtual void roundedRect(double x1, double y1, double x2, double y2, double rx, double ry)  { dirty(); BASE::roundedRect(x1,y1,x2,y2,rx,ry); }
	virtual void roundedRect(double x1, double y1, double x2, double y2,double rx_bottom, double ry_bottom,double rx_top,double ry_top) { dirty(); BASE::roundedRect(x1,y1,x2,y2,rx_bottom,ry_bottom,rx_top,ry_top); }
	virtual void ellipse(double cx, double cy, double rx, double ry) {BASE::ellipse(cx, cy, rx, ry);}
	virtual void arc(double cx, double cy, double rx, double ry, double start, double sweep) {BASE::arc(cx, cy, rx, ry, start, sweep);};
	virtual void star(double cx, double cy, double r1, double r2, double startAngle, int numRays) {BASE::star(cx, cy, r1, r2, startAngle, numRays);};
	virtual void curve(double x1, double y1, double x2, double y2, double x3, double y3) {BASE::curve(x1, y1, x2, y2, x3, y3);};
	virtual void curve(double x1, double y1, double x2, double y2, double x3, double y3, double x4, double y4) {BASE::curve(x1, y1, x2, y2, x3, y3, x4, y4);};
	virtual void polygon(double* xy, int numPoints) {BASE::polygon(xy, numPoints);};
	virtual void polyline(double* xy, int numPoints) {BASE::polyline(xy, numPoints);};

	virtual void setFont(const std::string& name) { BASE::font(lookupFont(name)); }
	virtual void renderText(double dstX, double dstY, const std::string& str) { 
		dirty(); 
		int height = BASE::font()[0];
		int base = BASE::font()[1];
		int offset = height-base*2;
		BASE::renderText(dstX, dstY + offset, str);
	}

    // Path commands
    virtual void resetPath() {BASE::resetPath();};

	virtual void moveTo(double x, double y) {BASE::moveTo(x, y);}
	virtual void moveRel(double dx, double dy) {BASE::moveRel(dx,dy);};

	virtual void lineTo(double x, double y) {BASE::lineTo(x, y);};
	virtual void lineRel(double dx, double dy) {BASE::lineRel(dx, dy);};

	virtual void horLineTo(double x) {BASE::horLineTo(x);};
	virtual void horLineRel(double dx) {BASE::horLineRel(dx);};

	virtual void verLineTo(double y) {BASE::verLineTo(y);};
	virtual void verLineRel(double dy) {BASE::verLineRel(dy);};

	virtual void arcTo(double rx, double ry, double angle, bool largeArcFlag, bool sweepFlag, double x, double y) {BASE::arcTo(rx, ry, angle, largeArcFlag, sweepFlag, x, y);};

	virtual void arcRel(double rx, double ry, double angle, bool largeArcFlag, bool sweepFlag, double dx, double dy) {BASE::arcRel(rx, ry, angle, largeArcFlag, sweepFlag, dx, dy);};

	virtual void quadricCurveTo(double xCtrl, double yCtrl, double xTo, double yTo) {BASE::quadricCurveTo(xCtrl, yCtrl, xTo, yTo);};
	virtual void quadricCurveRel(double dxCtrl, double dyCtrl, double dxTo, double dyTo) {BASE::quadricCurveRel(dxCtrl, dyCtrl, dxTo, dyTo);};
	virtual void quadricCurveTo(double xTo, double yTo) {BASE::quadricCurveTo(xTo, yTo);};
	virtual void quadricCurveRel(double dxTo, double dyTo) {BASE::quadricCurveRel(dxTo, dyTo);};

	virtual void cubicCurveTo(double xCtrl1, double yCtrl1, double xCtrl2, double yCtrl2, double xTo, double yTo) {BASE::cubicCurveTo(xCtrl1, yCtrl1, xCtrl2, yCtrl2, xTo, yTo);};
	virtual void cubicCurveRel(double dxCtrl1, double dyCtrl1, double dxCtrl2, double dyCtrl2, double dxTo, double dyTo) {BASE::cubicCurveRel(dxCtrl1, dyCtrl1, dxCtrl2, dyCtrl2, dxTo, dyTo);};
	virtual void cubicCurveTo(double xCtrl2, double yCtrl2, double xTo, double yTo) {BASE::cubicCurveTo(xCtrl2, yCtrl2, xTo, yTo);};
	virtual void cubicCurveRel(double xCtrl2, double yCtrl2, double xTo, double yTo) {BASE::cubicCurveRel(xCtrl2, yCtrl2, xTo, yTo);};

	virtual void addEllipse(double cx, double cy, double rx, double ry, Agg2DBase::Direction dir) {BASE::addEllipse(cx, cy, rx, ry, dir);};
	virtual void closePolygon() {BASE::closePolygon();};

	virtual void drawPath(Agg2DBase::DrawPathFlag flag = Agg2DBase::FillAndStroke) {BASE::drawPath(flag);};
//	virtual void drawPathNoTransform(DrawPathFlag flag = FillAndStroke) {BASE::drawPathNoTransform(flag);};

    // Image Transformations
	virtual void imageFilter(Agg2DBase::ImageFilter f) {BASE::imageFilter(f);};
	virtual Agg2DBase::ImageFilter imageFilter() {return BASE::imageFilter();};

	virtual void imageResample(Agg2DBase::ImageResample f) {BASE::imageResample(f);};
	virtual Agg2DBase::ImageResample imageResample() {return BASE::imageResample();};

	// Auxiliary
    virtual double pi() { return agg::pi; }
    virtual double deg2Rad(double v) { return v * agg::pi / 180.0; }
    virtual double rad2Deg(double v) { return v * 180.0 / agg::pi; }
};

//the main aggdraw targets for different pixel formats
typedef AggDrawTargetImplementation<T_AGG_PF_RGB555> T_AGG_RGB555;
typedef AggDrawTargetImplementation<T_AGG_PF_RGBA> T_AGG_RGBA;


class AggDraw
{
public:
	AggDraw()
		: target(NULL)
	{}
	AggDrawTarget *target;
};

enum AggTarget
{
	AggTarget_Screen = 0,
	AggTarget_Hud = 1,
	AggTarget_Lua = 2,
};

//specialized instance for desmume; should eventually move to another file
class AggDraw_Desmume : public AggDraw
{
public:
	void setTarget(AggTarget newTarget);
	//void composite(void* dest);

	AggDrawTarget *screen, *hud, *lua;
};

extern AggDraw_Desmume aggDraw;

void Agg_init();


#endif
