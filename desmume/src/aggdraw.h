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
	
	virtual void lineColor(unsigned r, unsigned g, unsigned b, unsigned a) = 0;
	virtual void fillColor(unsigned r, unsigned g, unsigned b, unsigned a) = 0;
	virtual void noFill() = 0;
	virtual void noLine() = 0;
	virtual void lineWidth(double w) = 0;
	virtual double lineWidth() = 0;
	
	virtual void roundedRect(double x1, double y1, double x2, double y2,double rx_bottom, double ry_bottom,double rx_top,    double ry_top) = 0;
	virtual void roundedRect(double x1, double y1, double x2, double y2, double r) = 0;
    virtual void roundedRect(double x1, double y1, double x2, double y2, double rx, double ry) = 0;

	static const agg::int8u* lookupFont(const std::string& name);
	virtual void setFont(const std::string& name) = 0;
	virtual void renderText(double dstX, double dstY, const std::string& str) = 0;
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
		attach(buf,width,height,stride);

		BASE::viewport(0, 0, width-1, height-1, 0, 0, width-1, height-1, TAGG2D::Anisotropic);
	}

	virtual void clear() { 
		if(!empty)
		{
			BASE::clearAll(0,0,0,0);
			undirty();
		}
	}

	virtual void lineColor(unsigned r, unsigned g, unsigned b, unsigned a) { BASE::lineColor(r,g,b,a); }
	virtual void fillColor(unsigned r, unsigned g, unsigned b, unsigned a) { BASE::fillColor(r,g,b,a); }
	virtual void noFill() { BASE::noFill(); }
	virtual void noLine() { BASE::noLine(); }
	virtual void lineWidth(double w) { BASE::lineWidth(w); }
	virtual double lineWidth() { return BASE::lineWidth(); }

	virtual void roundedRect(double x1, double y1, double x2, double y2,double rx_bottom, double ry_bottom,double rx_top,double ry_top) { dirty(); BASE::roundedRect(x1,y1,x2,y2,rx_bottom,ry_bottom,rx_top,ry_top); }
	virtual void roundedRect(double x1, double y1, double x2, double y2, double r) { dirty(); BASE::roundedRect(x1,y1,x2,y2,r); }
    virtual void roundedRect(double x1, double y1, double x2, double y2, double rx, double ry)  { dirty(); BASE::roundedRect(x1,y1,x2,y2,rx,ry); }

	virtual void setFont(const std::string& name) { BASE::font(lookupFont(name)); }
	virtual void renderText(double dstX, double dstY, const std::string& str) { dirty(); BASE::renderText(dstX, dstY, str); }
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
	void composite(void* dest);
};

extern AggDraw_Desmume aggDraw;

void Agg_init();


#endif
