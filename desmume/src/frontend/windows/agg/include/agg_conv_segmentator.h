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

#ifndef AGG_CONV_SEGMENTATOR_INCLUDED
#define AGG_CONV_SEGMENTATOR_INCLUDED

#include "agg_basics.h"
#include "agg_conv_adaptor_vpgen.h"
#include "agg_vpgen_segmentator.h"

namespace agg
{

    //========================================================conv_segmentator
    template<class VertexSource> 
    struct conv_segmentator : public conv_adaptor_vpgen<VertexSource, vpgen_segmentator>
    {
        typedef conv_adaptor_vpgen<VertexSource, vpgen_segmentator> base_type;

        conv_segmentator(VertexSource& vs) : 
            conv_adaptor_vpgen<VertexSource, vpgen_segmentator>(vs) {}

        void approximation_scale(double s) { base_type::vpgen().approximation_scale(s);        }
        double approximation_scale() const { return base_type::vpgen().approximation_scale();  }

    private:
        conv_segmentator(const conv_segmentator<VertexSource>&);
        const conv_segmentator<VertexSource>& 
            operator = (const conv_segmentator<VertexSource>&);
    };


}

#endif

