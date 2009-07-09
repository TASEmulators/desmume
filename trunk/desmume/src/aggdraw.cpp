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

#include "ctrl/agg_bezier_ctrl.h"
#include "platform/agg_platform_support.h"
#include "agg_pattern_filters_rgba.h"
#include "agg_renderer_outline_image.h"
#include "agg_rasterizer_outline_aa.h"

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

static agg::int8u brightness_to_alpha[256 * 3] = 
{
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 254, 254, 254, 254, 254, 254, 
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
    254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 253, 253, 
    253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 252, 
    252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 251, 251, 251, 251, 251, 
    251, 251, 251, 251, 250, 250, 250, 250, 250, 250, 250, 250, 249, 249, 249, 249, 
    249, 249, 249, 248, 248, 248, 248, 248, 248, 248, 247, 247, 247, 247, 247, 246, 
    246, 246, 246, 246, 246, 245, 245, 245, 245, 245, 244, 244, 244, 244, 243, 243, 
    243, 243, 243, 242, 242, 242, 242, 241, 241, 241, 241, 240, 240, 240, 239, 239, 
    239, 239, 238, 238, 238, 238, 237, 237, 237, 236, 236, 236, 235, 235, 235, 234, 
    234, 234, 233, 233, 233, 232, 232, 232, 231, 231, 230, 230, 230, 229, 229, 229, 
    228, 228, 227, 227, 227, 226, 226, 225, 225, 224, 224, 224, 223, 223, 222, 222, 
    221, 221, 220, 220, 219, 219, 219, 218, 218, 217, 217, 216, 216, 215, 214, 214, 
    213, 213, 212, 212, 211, 211, 210, 210, 209, 209, 208, 207, 207, 206, 206, 205, 
    204, 204, 203, 203, 202, 201, 201, 200, 200, 199, 198, 198, 197, 196, 196, 195, 
    194, 194, 193, 192, 192, 191, 190, 190, 189, 188, 188, 187, 186, 186, 185, 184, 
    183, 183, 182, 181, 180, 180, 179, 178, 177, 177, 176, 175, 174, 174, 173, 172, 
    171, 171, 170, 169, 168, 167, 166, 166, 165, 164, 163, 162, 162, 161, 160, 159, 
    158, 157, 156, 156, 155, 154, 153, 152, 151, 150, 149, 148, 148, 147, 146, 145, 
    144, 143, 142, 141, 140, 139, 138, 137, 136, 135, 134, 133, 132, 131, 130, 129, 
    128, 128, 127, 125, 124, 123, 122, 121, 120, 119, 118, 117, 116, 115, 114, 113, 
    112, 111, 110, 109, 108, 107, 106, 105, 104, 102, 101, 100,  99,  98,  97,  96,  
     95,  94,  93,  91,  90,  89,  88,  87,  86,  85,  84,  82,  81,  80,  79,  78, 
     77,  75,  74,  73,  72,  71,  70,  69,  67,  66,  65,  64,  63,  61,  60,  59, 
     58,  57,  56,  54,  53,  52,  51,  50,  48,  47,  46,  45,  44,  42,  41,  40, 
     39,  37,  36,  35,  34,  33,  31,  30,  29,  28,  27,  25,  24,  23,  22,  20, 
     19,  18,  17,  15,  14,  13,  12,  11,   9,   8,   7,   6,   4,   3,   2,   1
};

template<class Pattern,class Rasterizer,class Renderer,class PatternSource,class VertexSource>
void draw_curve(Pattern& patt,Rasterizer& ras,Renderer& ren,PatternSource& src,VertexSource& vs, double scale, double start)
{
	patt.create(src);
	ren.scale_x(scale);
	ren.start_x(start);
	ras.add_path(vs);
}

class pattern_src_brightness_to_alpha_rgba8
{
public:
    pattern_src_brightness_to_alpha_rgba8(agg::rendering_buffer& rb) : 
        m_rb(&rb), m_pf(*m_rb) {}

    unsigned width()  const { return m_pf.width();  }
    unsigned height() const { return m_pf.height(); }
    agg::rgba8 pixel(int x, int y) const
    {
        agg::rgba8 c = m_pf.pixel(x, y);
        c.a = brightness_to_alpha[c.r + c.g + c.b];
        return c;
    }

private:
    agg::rendering_buffer* m_rb;
    pixfmt m_pf;
};

 void agg_draw_line_pattern(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4, double scale, double start, char* filename){

	 int flipy = 0;

	 agg::platform_support platsup(agg::pix_format_rgb555, flipy);

	 platsup.load_img(0, filename);

	 agg::bezier_ctrl<agg::rgba8> m_curve1;

	 typedef agg::rasterizer_scanline_aa<> rasterizer_scanline;
	 typedef agg::scanline_p8 scanline;

	 m_curve1.curve(x1, y1, x2, y2, x3, y3, x4, y4);
	 m_curve1.no_transform();

	 rBuf.attach(GPU_tempScreen, width, height, stride);

	 pixfmt pixf(rBuf);
	 RendererBase ren_base(pixf);
	 RendererSolid ren(ren_base);

	 rasterizer_scanline ras;
	 scanline sl;

	 // Pattern source. Must have an interface:
	 // width() const
	 // height() const
	 // pixel(int x, int y) const
	 // Any agg::renderer_base<> or derived
	 // is good for the use as a source.
	 //-----------------------------------
	 pattern_src_brightness_to_alpha_rgba8 p1(platsup.rbuf_img(0));

	 agg::pattern_filter_bilinear_rgba8 fltr; // Filtering functor

	 // agg::line_image_pattern is the main container for the patterns. It creates
	 // a copy of the patterns extended according to the needs of the filter.
	 // agg::line_image_pattern can operate with arbitrary image width, but if the
	 // width of the pattern is power of 2, it's better to use the modified
	 // version agg::line_image_pattern_pow2 because it works about 15-25 percent
	 // faster than agg::line_image_pattern (because of using simple masking instead
	 // of expensive '%' operation).
	 typedef agg::line_image_pattern<agg::pattern_filter_bilinear_rgba8> pattern_type;
	 typedef agg::renderer_base<pixfmt> base_ren_type;
	 typedef agg::renderer_outline_image<base_ren_type, pattern_type> renderer_type;
	 typedef agg::rasterizer_outline_aa<renderer_type> rasterizer_type;

	 //-- Create uninitialized and set the source
	 pattern_type patt(fltr);
	 renderer_type ren_img(ren_base, patt);
	 rasterizer_type ras_img(ren_img);

	 draw_curve(patt, ras_img, ren_img, p1, m_curve1.curve(), scale, start);
 }

//temporary, just for testing the lib
void AGGDraw() {

	agg_draw_solid_triangle(0, 60, 200, 170, 100, 310, 255, 64, 64, 128, 99999);
	agg_draw_solid_rectangle(100, 100, 200, 200, 0, 255, 0, 128);
	agg_draw_solid_ellipse(70, 80, 50, 50, 255, 0, 0, 128);
	agg_draw_raster_font(60, 60, 255, 0, 255, 128, "testing testing testing", "verdana18_bold");
	agg_draw_line_pattern(64, 19, 14, 126, 118, 266, 19, 265, .76, 4.69, "C:\\7.bmp");
}