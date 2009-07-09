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

const agg::int8u* AggDrawTarget::lookupFont(const std::string& name)
{ 
	TAgg_Font_Table::iterator it(font_table.find(name));
	if(it == font_table.end()) return NULL;
	else return it->second;
}

void Agg_init_fonts()
{
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
	};

	for(int i=0;i<ARRAY_SIZE(fonts);i++)
		font_table[fonts[i].name] = fonts[i].font;
}

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
	Agg_init_fonts();
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

	img_source_type img_src(agg_targetLua.pixf, T_AGG_RGBA::pixfmt::color_type(0,255,0,0));

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

static int ctr=0;

//temporary, just for testing the lib
void AGGDraw() {

	aggDraw.setTarget(AggTarget_Lua);

	aggDraw.target->clear();

	ctr++;

	aggDraw.target->set_color(0, 255, 0, 128);
	int add = (int)(40*cos((double)ctr/20.0f));
	aggDraw.target->solid_rectangle(100 +add , 100, 200 + add, 200);

	aggDraw.target->set_gamma(99999);
	aggDraw.target->set_color(255, 64, 64, 128);
	aggDraw.target->solid_triangle(0, 60, 200, 170, 100, 310);

	aggDraw.target->set_color(255, 0, 0, 128);
	aggDraw.target->solid_ellipse(70, 80, 50, 50);
	
	aggDraw.target->set_font("verdana18_bold");
	aggDraw.target->set_color(255, 0, 255, 255);
	aggDraw.target->render_text(60,60, "testing testing testing");

	aggDraw.target->line(60, 90, 100, 100, 4);
	//
	//agg_draw_line_pattern(64, 19, 14, 126, 118, 266, 19, 265, .76, 4.69, "C:\\7.bmp");
}

//static agg::int8u brightness_to_alpha[256 * 3] = 
//{
//    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
//    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
//    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
//    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
//    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
//    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
//    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
//    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
//    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
//    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 254, 254, 254, 254, 254, 254, 
//    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
//    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
//    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
//    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
//    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
//    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
//    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
//    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
//    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
//    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
//    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
//    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
//    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
//    254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 253, 253, 
//    253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 252, 
//    252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 251, 251, 251, 251, 251, 
//    251, 251, 251, 251, 250, 250, 250, 250, 250, 250, 250, 250, 249, 249, 249, 249, 
//    249, 249, 249, 248, 248, 248, 248, 248, 248, 248, 247, 247, 247, 247, 247, 246, 
//    246, 246, 246, 246, 246, 245, 245, 245, 245, 245, 244, 244, 244, 244, 243, 243, 
//    243, 243, 243, 242, 242, 242, 242, 241, 241, 241, 241, 240, 240, 240, 239, 239, 
//    239, 239, 238, 238, 238, 238, 237, 237, 237, 236, 236, 236, 235, 235, 235, 234, 
//    234, 234, 233, 233, 233, 232, 232, 232, 231, 231, 230, 230, 230, 229, 229, 229, 
//    228, 228, 227, 227, 227, 226, 226, 225, 225, 224, 224, 224, 223, 223, 222, 222, 
//    221, 221, 220, 220, 219, 219, 219, 218, 218, 217, 217, 216, 216, 215, 214, 214, 
//    213, 213, 212, 212, 211, 211, 210, 210, 209, 209, 208, 207, 207, 206, 206, 205, 
//    204, 204, 203, 203, 202, 201, 201, 200, 200, 199, 198, 198, 197, 196, 196, 195, 
//    194, 194, 193, 192, 192, 191, 190, 190, 189, 188, 188, 187, 186, 186, 185, 184, 
//    183, 183, 182, 181, 180, 180, 179, 178, 177, 177, 176, 175, 174, 174, 173, 172, 
//    171, 171, 170, 169, 168, 167, 166, 166, 165, 164, 163, 162, 162, 161, 160, 159, 
//    158, 157, 156, 156, 155, 154, 153, 152, 151, 150, 149, 148, 148, 147, 146, 145, 
//    144, 143, 142, 141, 140, 139, 138, 137, 136, 135, 134, 133, 132, 131, 130, 129, 
//    128, 128, 127, 125, 124, 123, 122, 121, 120, 119, 118, 117, 116, 115, 114, 113, 
//    112, 111, 110, 109, 108, 107, 106, 105, 104, 102, 101, 100,  99,  98,  97,  96,  
//     95,  94,  93,  91,  90,  89,  88,  87,  86,  85,  84,  82,  81,  80,  79,  78, 
//     77,  75,  74,  73,  72,  71,  70,  69,  67,  66,  65,  64,  63,  61,  60,  59, 
//     58,  57,  56,  54,  53,  52,  51,  50,  48,  47,  46,  45,  44,  42,  41,  40, 
//     39,  37,  36,  35,  34,  33,  31,  30,  29,  28,  27,  25,  24,  23,  22,  20, 
//     19,  18,  17,  15,  14,  13,  12,  11,   9,   8,   7,   6,   4,   3,   2,   1
//};
//
//template<class Pattern,class Rasterizer,class Renderer,class PatternSource,class VertexSource>
//void draw_curve(Pattern& patt,Rasterizer& ras,Renderer& ren,PatternSource& src,VertexSource& vs, double scale, double start)
//{
//	patt.create(src);
//	ren.scale_x(scale);
//	ren.start_x(start);
//	ras.add_path(vs);
//}
//
//class pattern_src_brightness_to_alpha_rgba8
//{
//public:
//    pattern_src_brightness_to_alpha_rgba8(agg::rendering_buffer& rb) : 
//        m_rb(&rb), m_pf(*m_rb) {}
//
//    unsigned width()  const { return m_pf.width();  }
//    unsigned height() const { return m_pf.height(); }
//    agg::rgba8 pixel(int x, int y) const
//    {
//        agg::rgba8 c = m_pf.pixel(x, y);
//        c.a = brightness_to_alpha[c.r + c.g + c.b];
//        return c;
//    }
//
//private:
//    agg::rendering_buffer* m_rb;
//    pixfmt m_pf;
//};
//
// void agg_draw_line_pattern(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4, double scale, double start, char* filename){
//
//	 int flipy = 0;
//
//	 agg::platform_support platsup(agg::pix_format_rgb555, flipy);
//
//	 platsup.load_img(0, filename);
//
//	 agg::bezier_ctrl<agg::rgba8> m_curve1;
//
//	 typedef agg::rasterizer_scanline_aa<> rasterizer_scanline;
//	 typedef agg::scanline_p8 scanline;
//
//	 m_curve1.curve(x1, y1, x2, y2, x3, y3, x4, y4);
//	 m_curve1.no_transform();
//
//	 rBuf.attach(GPU_tempScreen, width, height, stride);
//
//	 pixfmt pixf(rBuf);
//	 RendererBase ren_base(pixf);
//	 RendererSolid ren(ren_base);
//
//	 rasterizer_scanline ras;
//	 scanline sl;
//
//	 // Pattern source. Must have an interface:
//	 // width() const
//	 // height() const
//	 // pixel(int x, int y) const
//	 // Any agg::renderer_base<> or derived
//	 // is good for the use as a source.
//	 //-----------------------------------
//	 pattern_src_brightness_to_alpha_rgba8 p1(platsup.rbuf_img(0));
//
//	 agg::pattern_filter_bilinear_rgba8 fltr; // Filtering functor
//
//	 // agg::line_image_pattern is the main container for the patterns. It creates
//	 // a copy of the patterns extended according to the needs of the filter.
//	 // agg::line_image_pattern can operate with arbitrary image width, but if the
//	 // width of the pattern is power of 2, it's better to use the modified
//	 // version agg::line_image_pattern_pow2 because it works about 15-25 percent
//	 // faster than agg::line_image_pattern (because of using simple masking instead
//	 // of expensive '%' operation).
//	 typedef agg::line_image_pattern<agg::pattern_filter_bilinear_rgba8> pattern_type;
//	 typedef agg::renderer_base<pixfmt> base_ren_type;
//	 typedef agg::renderer_outline_image<base_ren_type, pattern_type> renderer_type;
//	 typedef agg::rasterizer_outline_aa<renderer_type> rasterizer_type;
//
//	 //-- Create uninitialized and set the source
//	 pattern_type patt(fltr);
//	 renderer_type ren_img(ren_base, patt);
//	 rasterizer_type ras_img(ren_img);
//
//	 draw_curve(patt, ras_img, ren_img, p1, m_curve1.curve(), scale, start);
// }
