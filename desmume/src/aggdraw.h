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
	virtual void lineColor(unsigned r, unsigned g, unsigned b, unsigned a) = 0;
	virtual void noFill() = 0;
	virtual void roundedRect(double x1, double y1, double x2, double y2,double rx_bottom, double ry_bottom,double rx_top,    double ry_top) = 0;
	virtual void roundedRect(double x1, double y1, double x2, double y2, double r) = 0;
    virtual void roundedRect(double x1, double y1, double x2, double y2, double rx, double ry) = 0;
};


template<typename PIXFMT> 
class AggDrawTargetImplementation : public AggDrawTarget, public Agg2D<PIXFMT>
{
public:
	typedef PIXFMT pixfmt;
	typedef typename pixfmt::color_type color_type;

	typedef Agg2D<PIXFMT> BASE;
	AggDrawTargetImplementation(agg::int8u* buf, int width, int height, int stride)
	{
		attach(buf,width,height,stride);

		BASE::viewport(0, 0, 600, 600, 
                            0, 0, width, height, 
                            //TAGG2D::Anisotropic);
                            XMidYMid);
	}

	virtual void lineColor(unsigned r, unsigned g, unsigned b, unsigned a) { BASE::lineColor(r,g,b,a); }
	virtual void noFill() { BASE::noFill(); }
	virtual void roundedRect(double x1, double y1, double x2, double y2,double rx_bottom, double ry_bottom,double rx_top,double ry_top)
	{
		BASE::roundedRect(x1,y1,x2,y2,rx_bottom,ry_bottom,rx_top,ry_top);
	}
	virtual void roundedRect(double x1, double y1, double x2, double y2, double r) { BASE::roundedRect(x1,y1,x2,y2,r); }
    virtual void roundedRect(double x1, double y1, double x2, double y2, double rx, double ry)  { BASE::roundedRect(x1,y1,x2,y2,rx,ry); }
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
	AggTarget_Lua = 1
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
