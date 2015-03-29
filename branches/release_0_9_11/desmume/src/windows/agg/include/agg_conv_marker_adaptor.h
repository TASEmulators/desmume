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

#ifndef AGG_CONV_MARKER_ADAPTOR_INCLUDED
#define AGG_CONV_MARKER_ADAPTOR_INCLUDED

#include "agg_basics.h"
#include "agg_conv_adaptor_vcgen.h"
#include "agg_vcgen_vertex_sequence.h"

namespace agg
{

    //=====================================================conv_marker_adaptor
    template<class VertexSource, class Markers=null_markers>
    struct conv_marker_adaptor : 
    public conv_adaptor_vcgen<VertexSource, vcgen_vertex_sequence, Markers>
    {
        typedef Markers marker_type;
        typedef conv_adaptor_vcgen<VertexSource, vcgen_vertex_sequence, Markers> base_type;

        conv_marker_adaptor(VertexSource& vs) : 
            conv_adaptor_vcgen<VertexSource, vcgen_vertex_sequence, Markers>(vs)
        {
        }

        void shorten(double s) { base_type::generator().shorten(s); }
        double shorten() const { return base_type::generator().shorten(); }

    private:
        conv_marker_adaptor(const conv_marker_adaptor<VertexSource, Markers>&);
        const conv_marker_adaptor<VertexSource, Markers>& 
            operator = (const conv_marker_adaptor<VertexSource, Markers>&);
    };


}

#endif
