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
//
// polyline clipping converter
// There an optimized Liang-Basky algorithm is used. 
// The algorithm doesn't optimize the degenerate edges, i.e. it will never
// break a closed polyline into two or more ones, instead, there will be 
// degenerate edges coinciding with the respective clipping boundaries.
// This is a sub-optimal solution, because that optimization would require 
// extra, rather expensive math while the rasterizer tolerates it quite well, 
// without any considerable overhead.
//
//----------------------------------------------------------------------------
#ifndef AGG_CONV_CLIP_polyline_INCLUDED
#define AGG_CONV_CLIP_polyline_INCLUDED

#include "agg_basics.h"
#include "agg_conv_adaptor_vpgen.h"
#include "agg_vpgen_clip_polyline.h"

namespace agg
{

    //=======================================================conv_clip_polyline
    template<class VertexSource> 
    struct conv_clip_polyline : public conv_adaptor_vpgen<VertexSource, vpgen_clip_polyline>
    {
        typedef conv_adaptor_vpgen<VertexSource, vpgen_clip_polyline> base_type;

        conv_clip_polyline(VertexSource& vs) : 
            conv_adaptor_vpgen<VertexSource, vpgen_clip_polyline>(vs) {}

        void clip_box(double x1, double y1, double x2, double y2)
        {
            base_type::vpgen().clip_box(x1, y1, x2, y2);
        }

        double x1() const { return base_type::vpgen().x1(); }
        double y1() const { return base_type::vpgen().y1(); }
        double x2() const { return base_type::vpgen().x2(); }
        double y2() const { return base_type::vpgen().y2(); }

    private:
        conv_clip_polyline(const conv_clip_polyline<VertexSource>&);
        const conv_clip_polyline<VertexSource>& 
            operator = (const conv_clip_polyline<VertexSource>&);
    };

}

#endif
