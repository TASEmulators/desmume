#include "GPU.h"
#include "NDSSystem.h"

#include "agg_renderer_base.h"
#include "agg_renderer_primitives.h"
#include "agg_renderer_scanline.h"
#include "agg_bounding_rect.h"
#include "agg_trans_affine.h"
#include "agg_path_storage.h"
#include "agg_color_rgba.h"

#include "agg_rasterizer_scanline_aa.h"
#include "agg_scanline_p.h"

//raster text
#include "agg_glyph_raster_bin.h"
#include "agg_embedded_raster_fonts.h"
#include "agg_renderer_raster_text.h"

#define AGG_RGB555 //pixfmt will be rgb555
#include "pixel_formats.h"  //this file is in agg/examples

// The AGG base renderer
typedef agg::renderer_base<pixfmt> RendererBase;

// The AGG primitives renderer
typedef agg::renderer_primitives<RendererBase> RendererPrimitives;

// The AGG solid renderer
typedef agg::renderer_scanline_aa_solid<RendererBase> RendererSolid;

// AGG always wants a pointer to the
// beginning of the buffer, no matter what the stride. (AGG does handle
// negative strides correctly.)
const int stride = 512;

//width and height of emu's rendering buffer
const int width = 256;
const int height = 384;

agg::rendering_buffer rBuf; // AGG's rendering buffer, pointing into the emu's rendering buffer

void agg_draw_solid_ellipse(int x, int y, int rx, int ry, int r, int g, int b, int a){

	rBuf.attach(GPU_tempScreen, width, height, stride);

	pixfmt pixf(rBuf);
	RendererBase rbase(pixf);
	RendererPrimitives rprim(rbase);

	rprim.fill_color(pixfmt::color_type(r, g, b, a));

	rprim.solid_ellipse(x, y, rx, ry);
	
}

void agg_draw_solid_rectangle(int x1, int y1, int x2, int y2, int r, int g, int b, int a){

	rBuf.attach(GPU_tempScreen, width, height, stride);

	pixfmt pixf(rBuf);
	RendererBase rbase(pixf);
	RendererPrimitives rprim(rbase);

	rprim.fill_color(pixfmt::color_type(r, g, b, a));

	rprim.solid_rectangle(x1, y1, x2, y2);

}

void agg_draw_solid_triangle(int x1, int y1, int x2, int y2, int x3, int y3, int r, int g, int b, int a, int gamma){

	rBuf.attach(GPU_tempScreen, width, height, stride);

	pixfmt pixf(rBuf);
	RendererBase rbase(pixf);

	RendererSolid ren_aa(rbase);
	agg::rasterizer_scanline_aa<> m_ras;
	agg::scanline_p8 m_sl_p8;

	agg::path_storage path;

	path.move_to(x1, y1);
	path.line_to(x2, y2);
	path.line_to(x3, y3);
	path.close_polygon();

	ren_aa.color(agg::rgba(r, g, b, a));

	m_ras.gamma(agg::gamma_power(gamma * 2.0));
	m_ras.add_path(path);
	agg::render_scanlines(m_ras, m_sl_p8, ren_aa);

}

void agg_draw_raster_font(int x, int y, int r, int g, int b, int a, char* strbuf, char* fontchoice){

	typedef agg::renderer_base<pixfmt> ren_base;
	typedef agg::glyph_raster_bin<agg::rgba8> glyph_gen;

	int flipy = 0;

	struct font_type
	{
		const agg::int8u* font;
		const char* name;
	}
	fonts[] =
	{
		{ agg::gse4x6, "gse4x6" },
		{ agg::gse4x8, "gse4x8" },
		{ agg::gse5x7, "gse5x7" },
		{ agg::gse5x9, "gse5x9" },
		{ agg::gse6x9, "gse6x9" },
		{ agg::gse6x12, "gse6x12" },
		{ agg::gse7x11, "gse7x11" },
		{ agg::gse7x11_bold, "gse7x11_bold" },
		{ agg::gse7x15, "gse7x15" },
		{ agg::gse7x15_bold, "gse7x15_bold" },
		{ agg::gse8x16, "gse8x16" },
		{ agg::gse8x16_bold, "gse8x16_bold" },
		{ agg::mcs11_prop, "mcs11_prop" },
		{ agg::mcs11_prop_condensed, "mcs11_prop_condensed" },
		{ agg::mcs12_prop, "mcs12_prop" },
		{ agg::mcs13_prop, "mcs13_prop" },
		{ agg::mcs5x10_mono, "mcs5x10_mono" },
		{ agg::mcs5x11_mono, "mcs5x11_mono" },
		{ agg::mcs6x10_mono, "mcs6x10_mono" },
		{ agg::mcs6x11_mono, "mcs6x11_mono" },
		{ agg::mcs7x12_mono_high, "mcs7x12_mono_high" },
		{ agg::mcs7x12_mono_low, "mcs7x12_mono_low" },
		{ agg::verdana12, "verdana12" },
		{ agg::verdana12_bold, "verdana12_bold" },
		{ agg::verdana13, "verdana13" },
		{ agg::verdana13_bold, "verdana13_bold" },
		{ agg::verdana14, "verdana14" },
		{ agg::verdana14_bold, "verdana14_bold" },
		{ agg::verdana16, "verdana16" },
		{ agg::verdana16_bold, "verdana16_bold" },
		{ agg::verdana17, "verdana17" },
		{ agg::verdana17_bold, "verdana17_bold" },
		{ agg::verdana18, "verdana18" },
		{ agg::verdana18_bold, "verdana18_bold" },
		0, 0
	};

	int fontnumber = 0;

	for(int i = 0; fonts[i].font; i++) {
		if(!strcmp(fonts[i].name, fontchoice))
			fontnumber = i;
	}

	glyph_gen glyph(0);

	rBuf.attach(GPU_tempScreen, width, height, stride);

	pixfmt pixf(rBuf);
	ren_base rb(pixf);

	agg::renderer_raster_htext_solid<ren_base, glyph_gen> rt(rb, glyph);

	rt.color(agg::rgba(r,g,b,a));

	glyph.font(fonts[fontnumber].font);
	rt.render_text(x, y, strbuf, !flipy);

}
//temporary, just for testing the lib
void AGGDraw() {

	agg_draw_solid_triangle(0, 60, 200, 170, 100, 310, 255, 64, 64, 128, 99999);
	agg_draw_solid_rectangle(100, 100, 200, 200, 0, 255, 0, 128);
	agg_draw_solid_ellipse(70, 80, 50, 50, 255, 0, 0, 128);
	agg_draw_raster_font(60, 60, 255, 0, 255, 128, "testing testing testing", "verdana18_bold");
}
