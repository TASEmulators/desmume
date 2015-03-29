#ifndef PIXEL_FORMATS_INCLUDED
#define PIXEL_FORMATS_INCLUDED

#if defined(AGG_GRAY8)

#include "agg_pixfmt_gray.h"
#define pix_format agg::pix_format_gray8
typedef agg::pixfmt_gray8 pixfmt;
typedef agg::pixfmt_gray8_pre pixfmt_pre;
typedef agg::gray8 color_type;

#elif defined(AGG_GRAY16)

#include "agg_pixfmt_gray.h"
#define pix_format agg::pix_format_gray16
typedef agg::pixfmt_gray16 pixfmt;
typedef agg::pixfmt_gray16_pre pixfmt_pre;
typedef agg::gray16 color_type;

#elif defined(AGG_BGR24)

#include "agg_pixfmt_rgb.h"
#define pix_format agg::pix_format_bgr24
typedef agg::pixfmt_bgr24 pixfmt;
typedef agg::pixfmt_bgr24_pre pixfmt_pre;
#define pixfmt_gamma agg::pixfmt_bgr24_gamma
typedef agg::rgba8 color_type;
typedef agg::order_bgr component_order;

#elif defined(AGG_RGB24)

#include "agg_pixfmt_rgb.h"
#define pix_format agg::pix_format_rgb24
typedef agg::pixfmt_rgb24 pixfmt;
typedef agg::pixfmt_rgb24_pre pixfmt_pre;
#define pixfmt_gamma agg::pixfmt_rgb24_gamma
typedef agg::rgba8 color_type;
typedef agg::order_rgb component_order;

#elif defined(AGG_BGR48)

#include "agg_pixfmt_rgb.h"
#define pix_format agg::pix_format_bgr48
typedef agg::pixfmt_bgr48 pixfmt;
typedef agg::pixfmt_bgr48_pre pixfmt_pre;
#define pixfmt_gamma agg::pixfmt_bgr48_gamma
typedef agg::rgba16 color_type;
typedef agg::order_bgr component_order;

#elif defined(AGG_RGB48)

#include "agg_pixfmt_rgb.h"
#define pix_format agg::pix_format_rgb48
typedef agg::pixfmt_rgb48 pixfmt;
typedef agg::pixfmt_rgb48_pre pixfmt_pre;
#define pixfmt_gamma agg::pixfmt_rgb48_gamma
typedef agg::rgba16 color_type;
typedef agg::order_rgb component_order;

#elif defined(AGG_BGRA32)

#include "agg_pixfmt_rgba.h"
#define pix_format agg::pix_format_bgra32
typedef agg::pixfmt_bgra32 pixfmt;
typedef agg::pixfmt_bgra32_pre pixfmt_pre;
typedef agg::rgba8 color_type;
typedef agg::order_bgra component_order;

#elif defined(AGG_RGBA32)

#include "agg_pixfmt_rgba.h"
#define pix_format agg::pix_format_rgba32
typedef agg::pixfmt_rgba32 pixfmt;
typedef agg::pixfmt_rgba32_pre pixfmt_pre;
typedef agg::rgba8 color_type;
typedef agg::order_rgba component_order;

#elif defined(AGG_ARGB32)

#include "agg_pixfmt_rgba.h"
#define pix_format agg::pix_format_argb32
typedef agg::pixfmt_argb32 pixfmt;
typedef agg::pixfmt_argb32_pre pixfmt_pre;
typedef agg::rgba8 color_type;
typedef agg::order_argb component_order;

#elif defined(AGG_ABGR32)

#include "agg_pixfmt_rgba.h"
#define pix_format agg::pix_format_abgr32
typedef agg::pixfmt_abgr32 pixfmt;
typedef agg::pixfmt_abgr32_pre pixfmt_pre;
typedef agg::rgba8 color_type;
typedef agg::order_argb component_order;

#elif defined(AGG_BGRA64)

#include "agg_pixfmt_rgba.h"
#define pix_format agg::pix_format_bgra64
typedef agg::pixfmt_bgra64 pixfmt;
typedef agg::pixfmt_bgra64_pre pixfmt_pre;
typedef agg::rgba16 color_type;
typedef agg::order_bgra component_order;

#elif defined(AGG_RGBA64)

#include "agg_pixfmt_rgba.h"
#define pix_format agg::pix_format_rgba64
typedef agg::pixfmt_rgba64 pixfmt;
typedef agg::pixfmt_rgba64_pre pixfmt_pre;
typedef agg::rgba16 color_type;
typedef agg::order_rgba component_order;

#elif defined(AGG_ARGB64)

#include "agg_pixfmt_rgba.h"
#define pix_format agg::pix_format_argb64
typedef agg::pixfmt_argb64 pixfmt;
typedef agg::pixfmt_argb64_pre pixfmt_pre;
typedef agg::rgba16 color_type;
typedef agg::order_argb component_order;

#elif defined(AGG_ABGR64)

#include "agg_pixfmt_rgba.h"
#define pix_format agg::pix_format_abgr64
typedef agg::pixfmt_abgr64 pixfmt;
typedef agg::pixfmt_abgr64_pre pixfmt_pre;
typedef agg::rgba16 color_type;
typedef agg::order_argb component_order;

#elif defined(AGG_RGB565)

#include "agg_pixfmt_rgb_packed.h"
#define pix_format agg::pix_format_rgb565
typedef agg::pixfmt_rgb565 pixfmt;
typedef agg::pixfmt_rgb565_pre pixfmt_pre;
typedef agg::rgba8 color_type;

#elif defined(AGG_RGB555)

#include "agg_pixfmt_rgb_packed.h"
#define pix_format agg::pix_format_rgb555
typedef agg::pixfmt_rgb555 pixfmt;
typedef agg::pixfmt_rgb555_pre pixfmt_pre;
typedef agg::rgba8 color_type;

#elif defined(AGG_RGB_AAA)

#include "agg_pixfmt_rgb_packed.h"
#define pix_format agg::pix_format_rgbAAA
typedef agg::pixfmt_rgbAAA pixfmt;
typedef agg::pixfmt_rgbAAA_pre pixfmt_pre;
typedef agg::rgba16 color_type;

#elif defined(AGG_BGR_AAA)

#include "agg_pixfmt_rgb_packed.h"
#define pix_format agg::pix_format_bgrAAA
typedef agg::pixfmt_bgrAAA pixfmt;
typedef agg::pixfmt_bgrAAA_pre pixfmt_pre;
typedef agg::rgba16 color_type;

#elif defined(AGG_RGB_BBA)

#include "agg_pixfmt_rgb_packed.h"
#define pix_format agg::pix_format_rgbBBA
typedef agg::pixfmt_rgbBBA pixfmt;
typedef agg::pixfmt_rgbBBA_pre pixfmt_pre;
typedef agg::rgba16 color_type;

#elif defined(AGG_BGR_ABB)

#include "agg_pixfmt_rgb_packed.h"
#define pix_format agg::pix_format_bgrABB
typedef agg::pixfmt_bgrABB pixfmt;
typedef agg::pixfmt_bgrABB_pre pixfmt_pre;
typedef agg::rgba16 color_type;

#else
#error Please define pixel format: AGG_GRAY8, AGG_BGR24, AGG_RGB24, etc. See this file above
#endif

#endif
