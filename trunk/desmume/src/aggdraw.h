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

class AggDrawTarget
{
public:
	virtual void set_color(int r, int g, int b, int a) = 0;
	virtual void set_gamma(int gamma) = 0;
	virtual void set_font(const std::string& name) = 0;
	
	virtual void render_text(int x, int y, const std::string& str) = 0;
	virtual void solid_ellipse(int x, int y, int rx, int ry) = 0;
	virtual void solid_rectangle(int x1, int y1, int x2, int y2) = 0;
	virtual void solid_triangle(int x1, int y1, int x2, int y2, int x3, int y3) = 0;

	static const agg::int8u* lookupFont(const std::string& name);
};

template<typename pixfmt> 
class AggDrawTargetImplementation : public AggDrawTarget
{
public:	
	// The AGG base 
	typedef agg::renderer_base<pixfmt> RendererBase;

	// The AGG primitives renderer
	typedef agg::renderer_primitives<RendererBase> RendererPrimitives;

	// The AGG solid renderer
	typedef agg::renderer_scanline_aa_solid<RendererBase> RendererSolid;


	//the order of declaration matters in order to make these variables get setup correctly
	agg::rendering_buffer rBuf;
	pixfmt pixf;
	RendererBase rbase;
	RendererPrimitives rprim;

	AggDrawTargetImplementation(agg::int8u* buf, int width, int height, int stride)
		: rBuf(buf,width,height,stride)
		, pixf(rBuf)
		, rbase(pixf)
		, rprim(rbase)
	{
	}

	typedef typename pixfmt::color_type color_type;
	
	struct TRenderState
	{
		TRenderState()
			: color(0,0,0,255)
			, gamma(99999)
			, font(NULL)
		{}
		color_type color;
		int gamma;
		const agg::int8u* font;
	} renderState;

	virtual void set_color(int r, int g, int b, int a) { renderState.color = color_type(r,g,b,a); }
	virtual void set_gamma(int gamma) { renderState.gamma = gamma; }
	virtual void set_font(const std::string& name) { renderState.font = lookupFont(name); }

	virtual void render_text(int x, int y, const std::string& str)
	{
		typedef agg::renderer_base<pixfmt> ren_base;
		typedef agg::glyph_raster_bin<agg::rgba8> glyph_gen;
		glyph_gen glyph(0);

		ren_base rb(pixf);
		agg::renderer_raster_htext_solid<ren_base, glyph_gen> rt(rb, glyph);
		rt.color(renderState.color);

		glyph.font(renderState.font);
		rt.render_text(x, y, str.c_str(), true); //flipy
	}
	
	virtual void solid_ellipse(int x, int y, int rx, int ry)
	{
		rprim.fill_color(renderState.color);
		rprim.solid_ellipse(x, y, rx, ry);
	}

	virtual void solid_rectangle(int x1, int y1, int x2, int y2)
	{
		rprim.fill_color(renderState.color);
		rprim.solid_rectangle(x1, y1, x2, y2);
	}

	virtual void solid_triangle(int x1, int y1, int x2, int y2, int x3, int y3)
	{
		RendererSolid ren_aa(rbase);
		agg::rasterizer_scanline_aa<> m_ras;
		agg::scanline_p8 m_sl_p8;

		agg::path_storage path;

		path.move_to(x1, y1);
		path.line_to(x2, y2);
		path.line_to(x3, y3);
		path.close_polygon();

		ren_aa.color(renderState.color);

		m_ras.gamma(agg::gamma_power(renderState.gamma * 2.0));
		m_ras.add_path(path);
		agg::render_scanlines(m_ras, m_sl_p8, ren_aa);
	}

};


class AggDraw
{
public:
	AggDraw()
		: target(NULL)
	{}
	AggDrawTarget *target;
};

//specialized instance for desmume; should eventually move to another file
class AggDraw_Desmume : public AggDraw
{
};

extern AggDraw_Desmume aggDraw;

void Agg_init();


#endif
