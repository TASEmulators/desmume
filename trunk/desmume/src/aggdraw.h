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

#include "agg2d.h"

typedef agg::rgba8 AggColor;

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

	virtual void  clipBox(double x1, double y1, double x2, double y2) = 0;
	virtual Agg2DBase::RectD clipBox() const = 0;
	
	virtual void lineColor(unsigned r, unsigned g, unsigned b, unsigned a = 255) = 0;
	virtual void lineColor(AggColor color) { lineColor(color.r,color.g,color.b,color.a); }
	virtual AggColor lineColor() = 0;
	virtual void fillColor(unsigned r, unsigned g, unsigned b, unsigned a = 255) = 0;
	virtual AggColor fillColor() = 0;
	virtual void noFill() = 0;
	virtual void noLine() = 0;
	virtual void lineWidth(double w) = 0;
	virtual double lineWidth() = 0;

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

	
	virtual void fillLinearGradient(double x1, double y1, double x2, double y2, agg::rgba8 c1, agg::rgba8 c2, double profile=1.0) = 0;

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

	static const agg::int8u* lookupFont(const std::string& name);
	virtual void setFont(const std::string& name) = 0;
	virtual void renderText(double dstX, double dstY, const std::string& str) = 0;
	virtual void renderTextDropshadowed(double dstX, double dstY, const std::string& str)
	{
		AggColor lineColorOld = lineColor();
		lineColor(255-lineColorOld.r,255-lineColorOld.g,255-lineColorOld.b);
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

	virtual void  clipBox(double x1, double y1, double x2, double y2) { BASE::clipBox(x1,y1,x2,y2); }
	virtual Agg2DBase::RectD clipBox() const { return BASE::clipBox(); }

	virtual void lineColor(unsigned r, unsigned g, unsigned b, unsigned a = 255) { BASE::lineColor(r,g,b,a); }
	virtual AggColor lineColor() { return BASE::lineColor(); }
	virtual void fillColor(unsigned r, unsigned g, unsigned b, unsigned a = 255) { BASE::fillColor(r,g,b,a); }
	virtual AggColor fillColor() { return BASE::fillColor(); }

	virtual void noFill() { BASE::noFill(); }
	virtual void noLine() { BASE::noLine(); }
	virtual void lineWidth(double w) { BASE::lineWidth(w); }
	virtual double lineWidth() { return BASE::lineWidth(); }

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

	virtual void fillLinearGradient(double x1, double y1, double x2, double y2, AggColor c1, AggColor c2, double profile=1.0) {BASE::fillLinearGradient(x1, y1, x2, y2, c1, c2, profile); }

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

	// Auxiliary
    virtual double pi() { return agg::pi; }
    virtual double deg2Rad(double v) { return v * agg::pi / 180.0; }
    virtual double rad2Deg(double v) { return v * 180.0 / agg::pi; }
};

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
