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

#include <map>

#include "GPU.h"
#include "NDSSystem.h"

#include "aggdraw.h"

#include "agg_renderer_base.h"
#include "agg_renderer_primitives.h"
#include "agg_renderer_scanline.h"
#include "agg_bounding_rect.h"
#include "agg_trans_affine.h"
#include "agg_path_storage.h"
#include "agg_color_rgba.h"

#include "agg_pixfmt_rgb.h"
#include "agg_pixfmt_rgba.h"
#include "agg_pixfmt_rgb_packed.h"

#include "agg_rasterizer_scanline_aa.h"
#include "agg_scanline_u.h"
#include "agg_renderer_scanline.h"
#include "agg_scanline_p.h"

//raster text
#include "agg_glyph_raster_bin.h"
#include "agg_embedded_raster_fonts.h"
#include "agg_renderer_raster_text.h"

#include "ctrl/agg_bezier_ctrl.h"
#include "platform/agg_platform_support.h"
#include "agg_pattern_filters_rgba.h"
#include "agg_renderer_outline_image.h"
#include "agg_rasterizer_outline_aa.h"

#include "agg_image_accessors.h"
#include "agg_span_interpolator_linear.h"
#include "agg_span_image_filter_rgb.h"
#include "agg_span_image_filter_rgba.h"
#include "agg_span_image_filter_gray.h"

#include "agg_span_allocator.h"

typedef std::map<std::string, const agg::int8u*> TAgg_Font_Table;
static TAgg_Font_Table font_table;

AggDraw_Desmume aggDraw;

typedef AggDrawTargetImplementation<agg::pixfmt_rgb555> T_AGG_RGB555;
typedef AggDrawTargetImplementation<agg::pixfmt_bgra32> T_AGG_RGBA;

T_AGG_RGB555 agg_targetScreen(GPU_screen, 256, 384, 512);

static u32 luaBuffer[256*192*2];
T_AGG_RGBA agg_targetLua((u8*)luaBuffer, 256, 384, 1024);

static AggDrawTarget* targets[] = {
	&agg_targetScreen,
	&agg_targetLua
};

void Agg_init()
{
	//Agg_init_fonts();
	aggDraw.target = targets[0];
}

void AggDraw_Desmume::setTarget(AggTarget newTarget)
{
	target = targets[newTarget];
}

void AggDraw_Desmume::composite(void* dest)
{
	//!! oh what a mess !!

	agg::rendering_buffer rBuf;
	rBuf.attach((u8*)dest, 256, 384, 1024);


	typedef agg::image_accessor_clip<T_AGG_RGBA::pixfmt> img_source_type;

	img_source_type img_src(agg_targetLua.pixFormat(), T_AGG_RGBA::pixfmt::color_type(0,255,0,0));

	agg::trans_affine img_mtx;
	typedef agg::span_interpolator_linear<> interpolator_type;
	interpolator_type interpolator(img_mtx);
	typedef agg::span_image_filter_rgba_nn<img_source_type,interpolator_type> span_gen_type;
	span_gen_type sg(img_src, interpolator);

	agg::rasterizer_scanline_aa<> ras;
	//dont know whether this is necessary
	//ras.clip_box(0, 0, 256,384);
	agg::scanline_u8 sl;

	//I can't believe we're using a polygon for a rectangle.
	//there must be a better way
	agg::path_storage path;
	path.move_to(0, 0);
	path.line_to(255, 0);
	path.line_to(255, 383);
	path.line_to(0, 383);
	path.close_polygon();


	T_AGG_RGBA::pixfmt pixf(rBuf);
	T_AGG_RGBA::RendererBase rbase(pixf);
	agg::span_allocator<T_AGG_RGBA::color_type> sa;

	ras.add_path(path);
	agg::render_scanlines_bin(ras, sl, rbase, sa, sg);

}
//
//static int ctr=0;
//
////temporary, just for testing the lib
void AGGDraw() {
//
	aggDraw.setTarget(AggTarget_Lua);
//
//	aggDraw.target->clear();
//
//	ctr++;
//

	

	aggDraw.target->lineColor(0, 255, 0, 128);
	aggDraw.target->noFill();
//	int add = (int)(40*cos((double)ctr/20.0f));
	aggDraw.target->roundedRect(0.5, 0.5, 600-0.5, 600-0.5, 20.0);
//
//	aggDraw.target->set_gamma(99999);
//	aggDraw.target->set_color(255, 64, 64, 128);
//	aggDraw.target->solid_triangle(0, 60, 200, 170, 100, 310);
//
//	aggDraw.target->set_color(255, 0, 0, 128);
//	aggDraw.target->solid_ellipse(70, 80, 50, 50);
//	
//	aggDraw.target->set_font("verdana18_bold");
//	aggDraw.target->set_color(255, 0, 255, 255);
//	aggDraw.target->render_text(60,60, "testing testing testing");
//
//	aggDraw.target->line(60, 90, 100, 100, 4);
//
//	aggDraw.target->marker(200, 200, 40, 4);
//	aggDraw.target->marker(100, 300, 40, 3);
//	//
//	//agg_draw_line_pattern(64, 19, 14, 126, 118, 266, 19, 265, .76, 4.69, "C:\\7.bmp");
}

