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

#ifndef AGG_SPAN_CONVERTER_INCLUDED
#define AGG_SPAN_CONVERTER_INCLUDED

#include "agg_basics.h"

namespace agg
{
    //----------------------------------------------------------span_converter
    template<class SpanGenerator, class SpanConverter> class span_converter
    {
    public:
        typedef typename SpanGenerator::color_type color_type;

        span_converter(SpanGenerator& span_gen, SpanConverter& span_cnv) : 
            m_span_gen(&span_gen), m_span_cnv(&span_cnv) {}

        void attach_generator(SpanGenerator& span_gen) { m_span_gen = &span_gen; }
        void attach_converter(SpanConverter& span_cnv) { m_span_cnv = &span_cnv; }

        //--------------------------------------------------------------------
        void prepare() 
        { 
            m_span_gen->prepare(); 
            m_span_cnv->prepare();
        }

        //--------------------------------------------------------------------
        void generate(color_type* span, int x, int y, unsigned len)
        {
            m_span_gen->generate(span, x, y, len);
            m_span_cnv->generate(span, x, y, len);
        }

    private:
        SpanGenerator* m_span_gen;
        SpanConverter* m_span_cnv;
    };

}

#endif
