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

static void Agg_init_fonts()
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

	for(u32 i=0;i<ARRAY_SIZE(fonts);i++)
		font_table[fonts[i].name] = fonts[i].font;
}

AggDraw_Desmume aggDraw;

#ifdef WIN32
T_AGG_RGBA agg_targetScreen(0, 256, 384, 1024);
#else
T_AGG_RGB555 agg_targetScreen(0, 256, 384, 1512);
#endif

static u32 luaBuffer[256*192*2];
T_AGG_RGBA agg_targetLua((u8*)luaBuffer, 256, 384, 1024);

static u32 hudBuffer[256*192*2];
T_AGG_RGBA agg_targetHud((u8*)hudBuffer, 256, 384, 1024);

static AggDrawTarget* targets[] = {
	&agg_targetScreen,
	&agg_targetHud,
	&agg_targetLua,
};

void Agg_init()
{
	Agg_init_fonts();
	aggDraw.screen = targets[0];
	aggDraw.hud = targets[1];
	aggDraw.lua = targets[2];

	aggDraw.target = targets[0];

	//if we're single core, we don't want to waste time compositing
	if(CommonSettings.single_core())
		aggDraw.hud = &agg_targetScreen;

	//and the more clever compositing isnt supported in non-windows
	#ifndef WIN32
	aggDraw.hud = &agg_targetScreen;
	#endif

	aggDraw.hud->setFont("verdana18_bold");
}

void AggDraw_Desmume::setTarget(AggTarget newTarget)
{
	target = targets[newTarget];
}



////temporary, just for testing the lib
//void AGGDraw() {
////
//	aggDraw.setTarget(AggTarget_Screen);
//
////	aggDraw.target->clear();
////
////	aggDraw.target->clipBox(0,0,255,383);
////
//	aggDraw.target->lineColor(0, 0, 255, 128);
//	aggDraw.target->fillColor(0, 0, 255, 128);
////	//aggDraw.target->noFill();
//	aggDraw.target->lineWidth(1.0);
//	aggDraw.target->roundedRect(10,30,256-10,192-10,4);
////
////	aggDraw.target->setFont("verdana18_bold");
////	aggDraw.target->renderText(60,60, "testing testing testing");
////
////        // Gradients (Aqua Buttons)
////        //=======================================
//////        m_graphics.font("Verdana", 20.0, false, false, TAGG2D::VectorFontCache);
////        double xb1 = 10;
////        double yb1 = 80;
////        double xb2 = xb1 + 150;
////        double yb2 = yb1 + 36;
////
////        aggDraw.target->fillColor(0,50,180,180);
////        aggDraw.target->lineColor(0,0,80, 255);
////        aggDraw.target->lineWidth(1.0);
////        aggDraw.target->roundedRect(xb1, yb1, xb2, yb2, 12, 18);
////
////        aggDraw.target->lineColor(0,0,0,0);
////        aggDraw.target->fillLinearGradient(xb1, yb1, xb1, yb1+30, 
////                                      agg::rgba8(100,200,255,255), 
////                                      agg::rgba8(255,255,255,0));
////        aggDraw.target->roundedRect(xb1+3, yb1+2.5, xb2-3, yb1+30, 9, 18, 1, 1);
////
////		aggDraw.target->fillColor(0,0,50, 200);
////        aggDraw.target->noLine();
/////*        m_graphics.textAlignment(TAGG2D::AlignCenter, TAGG2D::AlignCenter);
////        m_graphics.text((xb1 + xb2) / 2.0, (yb1 + yb2) / 2.0, "Aqua Button", true, 0.0, 0.0);
////*/
////        aggDraw.target->fillLinearGradient(xb1, yb2-20, xb1, yb2-3, 
////                                      agg::rgba8(0,  0,  255,0),
////                                      agg::rgba8(100,255,255,255)); 
////        aggDraw.target->roundedRect(xb1+3, yb2-20, xb2-3, yb2-2, 1, 1, 9, 18);
////
////        // Basic Shapes -- Ellipse
////        //===========================================
////        aggDraw.target->lineWidth(3.5);
////        aggDraw.target->lineColor(20,  80,  80);
////        aggDraw.target->fillColor(200, 255, 80, 200);
////        aggDraw.target->ellipse(150, 200, 50, 90);
////
////        // Paths
////        //===========================================
////        aggDraw.target->resetPath();
////        aggDraw.target->fillColor(255, 0, 0, 100);
////        aggDraw.target->lineColor(0, 0, 255, 100);
////        aggDraw.target->lineWidth(2);
////        aggDraw.target->moveTo(300/2, 200/2);
////        aggDraw.target->horLineRel(-150/2);
////        aggDraw.target->arcRel(150/2, 150/2, 0, 1, 0, 150/2, -150/2);
////        aggDraw.target->closePolygon();
////        aggDraw.target->drawPath();
////
////        aggDraw.target->resetPath();
////        aggDraw.target->fillColor(255, 255, 0, 100);
////        aggDraw.target->lineColor(0, 0, 255, 100);
////        aggDraw.target->lineWidth(2);
////        aggDraw.target->moveTo(275/2, 175/2);
////        aggDraw.target->verLineRel(-150/2);
////        aggDraw.target->arcRel(150/2, 150/2, 0, 0, 0, -150/2, 150/2);
////        aggDraw.target->closePolygon();
////        aggDraw.target->drawPath();
////
////        aggDraw.target->resetPath();
////        aggDraw.target->noFill();
////        aggDraw.target->lineColor(127, 0, 0);
////        aggDraw.target->lineWidth(5);
////        aggDraw.target->moveTo(600/2, 350/2);
////        aggDraw.target->lineRel(50/2, -25/2);
////        aggDraw.target->arcRel(25/2, 25/2, aggDraw.target->deg2Rad(-30), 0, 1, 50/2, -25/2);
////        aggDraw.target->lineRel(50/2, -25/2);
////        aggDraw.target->arcRel(25/2, 50/2, aggDraw.target->deg2Rad(-30), 0, 1, 50/2, -25/2);
////        aggDraw.target->lineRel(50/2, -25/2);
////        aggDraw.target->arcRel(25/2, 75/2, aggDraw.target->deg2Rad(-30), 0, 1, 50/2, -25/2);
////        aggDraw.target->lineRel(50, -25);
////        aggDraw.target->arcRel(25/2, 100/2, aggDraw.target->deg2Rad(-30), 0, 1, 50/2, -25/2);
////        aggDraw.target->lineRel(50/2, -25/2);
////        aggDraw.target->drawPath();
//}
////


//
////========================
////testing stufff
//
//int width = 256;
//int height = 384;
//
//Agg2D m_graphics;
//
//void AGGDraw(unsigned char * buffer)
//    {
//        m_graphics.attach(buffer, 
//                          256, 
//                          384,
//                          512);
//
//        m_graphics.clearAll(255, 255, 255);
//        //m_graphics.clearAll(0, 0, 0);
//
//        //m_graphics.blendMode(TAGG2D::BlendSub);
//        //m_graphics.blendMode(TAGG2D::BlendAdd);
//
//        m_graphics.antiAliasGamma(1.4);
//
//        // Set flipText(true) if you have the Y axis upside down.
//        //m_graphics.flipText(true);
//
//
//        // ClipBox.
//        //m_graphics.clipBox(50, 50, rbuf_window().width() - 50, rbuf_window().height() - 50);
//
//        // Transfornations - Rotate around (300,300) to 5 degree
//        //m_graphics.translate(-300, -300);
//        //m_graphics.rotate(TAGG2D::deg2Rad(5.0));
//        //m_graphics.translate(300, 300);
//
//        // Viewport - set 0,0,600,600 to the actual window size 
//        // preserving aspect ratio and placing the viewport in the center.
//        // To ignore aspect ratio use TAGG2D::Anisotropic
//        // Note that the viewport just adds transformations to the current
//        // affine matrix. So that, set the viewport *after* all transformations!
//        m_graphics.viewport(0, 0, 600, 600, 
//                            0, 0, width, height, 
//                            //TAGG2D::Anisotropic);
//                            TAGG2D::XMidYMid);
//
//
//        // Rounded Rect
//        m_graphics.lineColor(0, 0, 0);
//        m_graphics.noFill();
//        m_graphics.roundedRect(0.5, 0.5, 600-0.5, 600-0.5, 20.0);
///*
//
//        // Reglar Text
//        m_graphics.font("Times New Roman", 14.0, false, false);
//        m_graphics.fillColor(0, 0, 0);
//        m_graphics.noLine();
//        m_graphics.text(100, 20, "Regular Raster Text -- Fast, but can't be rotated");
//
//        // Outlined Text
//        m_graphics.font("Times New Roman", 50.0, false, false, TAGG2D::VectorFontCache);
//        m_graphics.lineColor(50, 0, 0);
//        m_graphics.fillColor(180, 200, 100);
//        m_graphics.lineWidth(1.0);
//        m_graphics.text(100.5, 50.5, "Outlined Text");
//
//        // Text Alignment
//        m_graphics.line(250.5-150, 150.5,    250.5+150, 150.5);
//        m_graphics.line(250.5,     150.5-20, 250.5,     150.5+20);
//        m_graphics.line(250.5-150, 200.5,    250.5+150, 200.5);
//        m_graphics.line(250.5,     200.5-20, 250.5,     200.5+20);
//        m_graphics.line(250.5-150, 250.5,    250.5+150, 250.5);
//        m_graphics.line(250.5,     250.5-20, 250.5,     250.5+20);
//        m_graphics.line(250.5-150, 300.5,    250.5+150, 300.5);
//        m_graphics.line(250.5,     300.5-20, 250.5,     300.5+20);
//        m_graphics.line(250.5-150, 350.5,    250.5+150, 350.5);
//        m_graphics.line(250.5,     350.5-20, 250.5,     350.5+20);
//        m_graphics.line(250.5-150, 400.5,    250.5+150, 400.5);
//        m_graphics.line(250.5,     400.5-20, 250.5,     400.5+20);
//        m_graphics.line(250.5-150, 450.5,    250.5+150, 450.5);
//        m_graphics.line(250.5,     450.5-20, 250.5,     450.5+20);
//        m_graphics.line(250.5-150, 500.5,    250.5+150, 500.5);
//        m_graphics.line(250.5,     500.5-20, 250.5,     500.5+20);
//        m_graphics.line(250.5-150, 550.5,    250.5+150, 550.5);
//        m_graphics.line(250.5,     550.5-20, 250.5,     550.5+20);
//*/
///*
//        m_graphics.fillColor(100, 50, 50);
//        m_graphics.noLine();
//        //m_graphics.textHints(false);
//        m_graphics.font("Times New Roman", 40.0, false, false, TAGG2D::VectorFontCache);
//
//        m_graphics.textAlignment(TAGG2D::AlignLeft, TAGG2D::AlignBottom);
//        m_graphics.text(250.0,     150.0, "Left-Bottom", true, 0, 0);
//
//        m_graphics.textAlignment(TAGG2D::AlignCenter, TAGG2D::AlignBottom);
//        m_graphics.text(250.0,     200.0, "Center-Bottom", true, 0, 0);
//
//        m_graphics.textAlignment(TAGG2D::AlignRight, TAGG2D::AlignBottom);
//        m_graphics.text(250.0,     250.0, "Right-Bottom", true, 0, 0);
//
//        m_graphics.textAlignment(TAGG2D::AlignLeft, TAGG2D::AlignCenter);
//        m_graphics.text(250.0,     300.0, "Left-Center", true, 0, 0);
//
//        m_graphics.textAlignment(TAGG2D::AlignCenter, TAGG2D::AlignCenter);
//        m_graphics.text(250.0,     350.0, "Center-Center", true, 0, 0);
//
//        m_graphics.textAlignment(TAGG2D::AlignRight, TAGG2D::AlignCenter);
//        m_graphics.text(250.0,     400.0, "Right-Center", true, 0, 0);
//
//        m_graphics.textAlignment(TAGG2D::AlignLeft, TAGG2D::AlignTop);
//        m_graphics.text(250.0,     450.0, "Left-Top", true, 0, 0);
//
//        m_graphics.textAlignment(TAGG2D::AlignCenter, TAGG2D::AlignTop);
//        m_graphics.text(250.0,     500.0, "Center-Top", true, 0, 0);
//
//        m_graphics.textAlignment(TAGG2D::AlignRight, TAGG2D::AlignTop);
//        m_graphics.text(250.0,     550.0, "Right-Top", true, 0, 0);
//
//*/
//        // Gradients (Aqua Buttons)
//        //=======================================
//        m_graphics.font("Verdana", 20.0, false, false, TAGG2D::VectorFontCache);
//        double xb1 = 400;
//        double yb1 = 80;
//        double xb2 = xb1 + 150;
//        double yb2 = yb1 + 36;
//
//        m_graphics.fillColor(TAGG2D::Color(0,50,180,180));
//        m_graphics.lineColor(TAGG2D::Color(0,0,80, 255));
//        m_graphics.lineWidth(1.0);
//        m_graphics.roundedRect(xb1, yb1, xb2, yb2, 12, 18);
//
//        m_graphics.lineColor(TAGG2D::Color(0,0,0,0));
//        m_graphics.fillLinearGradient(xb1, yb1, xb1, yb1+30, 
//                                      TAGG2D::Color(100,200,255,255), 
//                                      TAGG2D::Color(255,255,255,0));
//        m_graphics.roundedRect(xb1+3, yb1+2.5, xb2-3, yb1+30, 9, 18, 1, 1);
//
//        m_graphics.fillColor(TAGG2D::Color(0,0,50, 200));
//        m_graphics.noLine();
///*        m_graphics.textAlignment(TAGG2D::AlignCenter, TAGG2D::AlignCenter);
//        m_graphics.text((xb1 + xb2) / 2.0, (yb1 + yb2) / 2.0, "Aqua Button", true, 0.0, 0.0);
//*/
//        m_graphics.fillLinearGradient(xb1, yb2-20, xb1, yb2-3, 
//                                      TAGG2D::Color(0,  0,  255,0),
//                                      TAGG2D::Color(100,255,255,255)); 
//        m_graphics.roundedRect(xb1+3, yb2-20, xb2-3, yb2-2, 1, 1, 9, 18);
//
//
//        // Aqua Button Pressed
//        xb1 = 400;
//        yb1 = 30;
//        xb2 = xb1 + 150;
//        yb2 = yb1 + 36;
//
//        m_graphics.fillColor(TAGG2D::Color(0,50,180,180));
//        m_graphics.lineColor(TAGG2D::Color(0,0,0,  255));
//        m_graphics.lineWidth(2.0);
//        m_graphics.roundedRect(xb1, yb1, xb2, yb2, 12, 18);
//
//        m_graphics.lineColor(TAGG2D::Color(0,0,0,0));
//        m_graphics.fillLinearGradient(xb1, yb1+2, xb1, yb1+25, 
//                                      TAGG2D::Color(60, 160,255,255), 
//                                      TAGG2D::Color(100,255,255,0));
//        m_graphics.roundedRect(xb1+3, yb1+2.5, xb2-3, yb1+30, 9, 18, 1, 1);
//
//        m_graphics.fillColor(TAGG2D::Color(0,0,50, 255));
//        m_graphics.noLine();
///*        m_graphics.textAlignment(TAGG2D::AlignCenter, TAGG2D::AlignCenter);
//        m_graphics.text((xb1 + xb2) / 2.0, (yb1 + yb2) / 2.0, "Aqua Pressed", 0.0, 0.0);
//*/
//        m_graphics.fillLinearGradient(xb1, yb2-25, xb1, yb2-5, 
//                                      TAGG2D::Color(0,  180,255,0),
//                                      TAGG2D::Color(0,  200,255,255)); 
//        m_graphics.roundedRect(xb1+3, yb2-25, xb2-3, yb2-2, 1, 1, 9, 18);
//
//
//
//
//        // Basic Shapes -- Ellipse
//        //===========================================
//        m_graphics.lineWidth(3.5);
//        m_graphics.lineColor(20,  80,  80);
//        m_graphics.fillColor(200, 255, 80, 200);
//        m_graphics.ellipse(450, 200, 50, 90);
//
//
//        // Paths
//        //===========================================
//        m_graphics.resetPath();
//        m_graphics.fillColor(255, 0, 0, 100);
//        m_graphics.lineColor(0, 0, 255, 100);
//        m_graphics.lineWidth(2);
//        m_graphics.moveTo(300/2, 200/2);
//        m_graphics.horLineRel(-150/2);
//        m_graphics.arcRel(150/2, 150/2, 0, 1, 0, 150/2, -150/2);
//        m_graphics.closePolygon();
//        m_graphics.drawPath();
//
//        m_graphics.resetPath();
//        m_graphics.fillColor(255, 255, 0, 100);
//        m_graphics.lineColor(0, 0, 255, 100);
//        m_graphics.lineWidth(2);
//        m_graphics.moveTo(275/2, 175/2);
//        m_graphics.verLineRel(-150/2);
//        m_graphics.arcRel(150/2, 150/2, 0, 0, 0, -150/2, 150/2);
//        m_graphics.closePolygon();
//        m_graphics.drawPath();
//
//
//        m_graphics.resetPath();
//        m_graphics.noFill();
//        m_graphics.lineColor(127, 0, 0);
//        m_graphics.lineWidth(5);
//        m_graphics.moveTo(600/2, 350/2);
//        m_graphics.lineRel(50/2, -25/2);
//        m_graphics.arcRel(25/2, 25/2, TAGG2D::deg2Rad(-30), 0, 1, 50/2, -25/2);
//        m_graphics.lineRel(50/2, -25/2);
//        m_graphics.arcRel(25/2, 50/2, TAGG2D::deg2Rad(-30), 0, 1, 50/2, -25/2);
//        m_graphics.lineRel(50/2, -25/2);
//        m_graphics.arcRel(25/2, 75/2, TAGG2D::deg2Rad(-30), 0, 1, 50/2, -25/2);
//        m_graphics.lineRel(50, -25);
//        m_graphics.arcRel(25/2, 100/2, TAGG2D::deg2Rad(-30), 0, 1, 50/2, -25/2);
//        m_graphics.lineRel(50/2, -25/2);
//        m_graphics.drawPath();
//
//
//        // Master Alpha. From now on everything will be translucent
//        //===========================================
//        m_graphics.masterAlpha(0.85);
//
//
//        // Image Transformations
//        //===========================================
///*        TAGG2D::Image img(rbuf_img(0).buf(), 
//                         rbuf_img(0).width(), 
//                         rbuf_img(0).height(), 
//                         rbuf_img(0).stride());
//        m_graphics.imageFilter(TAGG2D::Bilinear);
//
//        //m_graphics.imageResample(TAGG2D::NoResample);
//        //m_graphics.imageResample(TAGG2D::ResampleAlways);
//        m_graphics.imageResample(TAGG2D::ResampleOnZoomOut);
//
//        // Set the initial image blending operation as BlendDst, that actually 
//        // does nothing. 
//        //-----------------
//        m_graphics.imageBlendMode(TAGG2D::BlendDst);
//
//
//        // Transform the whole image to the destination rectangle
//        //-----------------
//        //m_graphics.transformImage(img, 450, 200, 595, 350);
//
//        // Transform the rectangular part of the image to the destination rectangle
//        //-----------------
//        //m_graphics.transformImage(img, 60, 60, img.width()-60, img.height()-60,
//        //                          450, 200, 595, 350);
//
//        // Transform the whole image to the destination parallelogram
//        //-----------------
//        //double parl[6] = { 450, 200, 595, 220, 575, 350 };
//        //m_graphics.transformImage(img, parl);
//
//        // Transform the rectangular part of the image to the destination parallelogram
//        //-----------------
//        //double parl[6] = { 450, 200, 595, 220, 575, 350 };
//        //m_graphics.transformImage(img, 60, 60, img.width()-60, img.height()-60, parl);
//
//        // Transform image to the destination path. The scale is determined by a rectangle
//        //-----------------
//        //m_graphics.resetPath();
//        //m_graphics.moveTo(450, 200);
//        //m_graphics.cubicCurveTo(595, 220, 575, 350, 595, 350);
//        //m_graphics.lineTo(470, 340);
//        //m_graphics.transformImagePath(img, 450, 200, 595, 350);
//
//
//        // Transform image to the destination path.
//        // The scale is determined by a rectangle
//        //-----------------
//        m_graphics.resetPath();
//        m_graphics.moveTo(450, 200);
//        m_graphics.cubicCurveTo(595, 220, 575, 350, 595, 350);
//        m_graphics.lineTo(470, 340);
//        m_graphics.transformImagePath(img, 60, 60, img.width()-60, img.height()-60,
//                                      450, 200, 595, 350);
//
//        // Transform image to the destination path. 
//        // The transformation is determined by a parallelogram
//        //m_graphics.resetPath();
//        //m_graphics.moveTo(450, 200);
//        //m_graphics.cubicCurveTo(595, 220, 575, 350, 595, 350);
//        //m_graphics.lineTo(470, 340);
//        //double parl[6] = { 450, 200, 595, 220, 575, 350 };
//        //m_graphics.transformImagePath(img, parl);
//
//        // Transform the rectangular part of the image to the destination path. 
//        // The transformation is determined by a parallelogram
//        //m_graphics.resetPath();
//        //m_graphics.moveTo(450, 200);
//        //m_graphics.cubicCurveTo(595, 220, 575, 350, 595, 350);
//        //m_graphics.lineTo(470, 340);
//        //double parl[6] = { 450, 200, 595, 220, 575, 350 };
//        //m_graphics.transformImagePath(img, 60, 60, img.width()-60, img.height()-60, parl);
//*/
//
//        // Add/Sub/Contrast Blending Modes
//        m_graphics.noLine();
//        m_graphics.fillColor(70, 70, 0);
//        m_graphics.blendMode(TAGG2D::BlendAdd);
//        m_graphics.ellipse(500, 280, 20, 40);
//
//        m_graphics.fillColor(255, 255, 255);
//        m_graphics.blendMode(TAGG2D::BlendContrast);
//        m_graphics.ellipse(500+40, 280, 20, 40);
//
//
//
//        // Radial gradient.
//        m_graphics.blendMode(TAGG2D::BlendAlpha);
//        m_graphics.fillRadialGradient(400, 500, 40, 
//                                      TAGG2D::Color(255, 255, 0, 0),
//                                      TAGG2D::Color(0, 0, 127),
//                                      TAGG2D::Color(0, 255, 0, 0));
//        m_graphics.ellipse(400, 500, 40, 40);
//
//    }
//
