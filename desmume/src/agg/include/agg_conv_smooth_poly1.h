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

#ifndef AGG_CONV_SMOOTH_POLY1_INCLUDED
#define AGG_CONV_SMOOTH_POLY1_INCLUDED

#include "agg_basics.h"
#include "agg_vcgen_smooth_poly1.h"
#include "agg_conv_adaptor_vcgen.h"
#include "agg_conv_curve.h"


namespace agg
{

    //-------------------------------------------------------conv_smooth_poly1
    template<class VertexSource> 
    struct conv_smooth_poly1 : 
    public conv_adaptor_vcgen<VertexSource, vcgen_smooth_poly1>
    {
        typedef conv_adaptor_vcgen<VertexSource, vcgen_smooth_poly1> base_type;

        conv_smooth_poly1(VertexSource& vs) : 
            conv_adaptor_vcgen<VertexSource, vcgen_smooth_poly1>(vs)
        {
        }

        void   smooth_value(double v) { base_type::generator().smooth_value(v); }
        double smooth_value() const { return base_type::generator().smooth_value(); }

    private:
        conv_smooth_poly1(const conv_smooth_poly1<VertexSource>&);
        const conv_smooth_poly1<VertexSource>& 
            operator = (const conv_smooth_poly1<VertexSource>&);
    };



    //-------------------------------------------------conv_smooth_poly1_curve
    template<class VertexSource> 
    struct conv_smooth_poly1_curve : 
    public conv_curve<conv_smooth_poly1<VertexSource> >
    {
        conv_smooth_poly1_curve(VertexSource& vs) :
            conv_curve<conv_smooth_poly1<VertexSource> >(m_smooth),
            m_smooth(vs)
        {
        }

        void   smooth_value(double v) { m_smooth.generator().smooth_value(v); }
        double smooth_value() const { return m_smooth.generator().smooth_value(); }

    private:
        conv_smooth_poly1_curve(const conv_smooth_poly1_curve<VertexSource>&);
        const conv_smooth_poly1_curve<VertexSource>& 
            operator = (const conv_smooth_poly1_curve<VertexSource>&);

        conv_smooth_poly1<VertexSource> m_smooth;
    };

}


#endif

