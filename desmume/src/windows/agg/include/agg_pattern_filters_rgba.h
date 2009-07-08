//----------------------------------------------------------------------------
// Anti-Grain Geometry (AGG) - Version 2.5
// A high quality rendering engine for C++
// Copyright (C) 2002-2006 Maxim Shemanarev
// Contact: mcseem@antigrain.com
//          mcseemagg@yahoo.com
//          http://antigrain.com
// 
// AGG is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
// 
// AGG is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with AGG; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, 
// MA 02110-1301, USA.
//----------------------------------------------------------------------------

#ifndef AGG_PATTERN_FILTERS_RGBA8_INCLUDED
#define AGG_PATTERN_FILTERS_RGBA8_INCLUDED

#include "agg_basics.h"
#include "agg_line_aa_basics.h"
#include "agg_color_rgba.h"


namespace agg
{

    //=======================================================pattern_filter_nn
    template<class ColorT> struct pattern_filter_nn
    {
        typedef ColorT color_type;
        static unsigned dilation() { return 0; }

        static void AGG_INLINE pixel_low_res(color_type const* const* buf, 
                                             color_type* p, int x, int y)
        {
            *p = buf[y][x];
        }

        static void AGG_INLINE pixel_high_res(color_type const* const* buf, 
                                              color_type* p, int x, int y)
        {
            *p = buf[y >> line_subpixel_shift]
                    [x >> line_subpixel_shift];
        }
    };

    typedef pattern_filter_nn<rgba8>  pattern_filter_nn_rgba8;
    typedef pattern_filter_nn<rgba16> pattern_filter_nn_rgba16;


    //===========================================pattern_filter_bilinear_rgba
    template<class ColorT> struct pattern_filter_bilinear_rgba
    {
        typedef ColorT color_type;
        typedef typename color_type::value_type value_type;
        typedef typename color_type::calc_type calc_type;


        static unsigned dilation() { return 1; }

        static AGG_INLINE void pixel_low_res(color_type const* const* buf, 
                                             color_type* p, int x, int y)
        {
            *p = buf[y][x];
        }

        static AGG_INLINE void pixel_high_res(color_type const* const* buf, 
                                              color_type* p, int x, int y)
        {
            calc_type r, g, b, a;
            r = g = b = a = line_subpixel_scale * line_subpixel_scale / 2;

            calc_type weight;
            int x_lr = x >> line_subpixel_shift;
            int y_lr = y >> line_subpixel_shift;

            x &= line_subpixel_mask;
            y &= line_subpixel_mask;
            const color_type* ptr = buf[y_lr] + x_lr;

            weight = (line_subpixel_scale - x) * 
                     (line_subpixel_scale - y);
            r += weight * ptr->r;
            g += weight * ptr->g;
            b += weight * ptr->b;
            a += weight * ptr->a;

            ++ptr;

            weight = x * (line_subpixel_scale - y);
            r += weight * ptr->r;
            g += weight * ptr->g;
            b += weight * ptr->b;
            a += weight * ptr->a;

            ptr = buf[y_lr + 1] + x_lr;

            weight = (line_subpixel_scale - x) * y;
            r += weight * ptr->r;
            g += weight * ptr->g;
            b += weight * ptr->b;
            a += weight * ptr->a;

            ++ptr;

            weight = x * y;
            r += weight * ptr->r;
            g += weight * ptr->g;
            b += weight * ptr->b;
            a += weight * ptr->a;

            p->r = (value_type)(r >> line_subpixel_shift * 2);
            p->g = (value_type)(g >> line_subpixel_shift * 2);
            p->b = (value_type)(b >> line_subpixel_shift * 2);
            p->a = (value_type)(a >> line_subpixel_shift * 2);
        }
    };

    typedef pattern_filter_bilinear_rgba<rgba8>  pattern_filter_bilinear_rgba8;
    typedef pattern_filter_bilinear_rgba<rgba16> pattern_filter_bilinear_rgba16;
}

#endif
