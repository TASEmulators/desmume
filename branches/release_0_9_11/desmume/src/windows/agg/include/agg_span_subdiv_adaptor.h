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

#ifndef AGG_SPAN_SUBDIV_ADAPTOR_INCLUDED
#define AGG_SPAN_SUBDIV_ADAPTOR_INCLUDED

#include "agg_basics.h"

namespace agg
{

    //=================================================span_subdiv_adaptor
    template<class Interpolator, unsigned SubpixelShift = 8> 
    class span_subdiv_adaptor
    {
    public:
        typedef Interpolator interpolator_type;
        typedef typename interpolator_type::trans_type trans_type;

        enum sublixel_scale_e
        {
            subpixel_shift = SubpixelShift,
            subpixel_scale = 1 << subpixel_shift
        };


        //----------------------------------------------------------------
        span_subdiv_adaptor() :
            m_subdiv_shift(4),
            m_subdiv_size(1 << m_subdiv_shift),
            m_subdiv_mask(m_subdiv_size - 1) {}

        span_subdiv_adaptor(interpolator_type& interpolator, 
                             unsigned subdiv_shift = 4) : 
            m_subdiv_shift(subdiv_shift),
            m_subdiv_size(1 << m_subdiv_shift),
            m_subdiv_mask(m_subdiv_size - 1),
            m_interpolator(&interpolator) {}

        span_subdiv_adaptor(interpolator_type& interpolator, 
                             double x, double y, unsigned len,
                             unsigned subdiv_shift = 4) :
            m_subdiv_shift(subdiv_shift),
            m_subdiv_size(1 << m_subdiv_shift),
            m_subdiv_mask(m_subdiv_size - 1),
            m_interpolator(&interpolator)
        {
            begin(x, y, len);
        }


        //----------------------------------------------------------------
        const interpolator_type& interpolator() const { return *m_interpolator; }
        void interpolator(interpolator_type& intr) { m_interpolator = &intr; }

        //----------------------------------------------------------------
        const trans_type& transformer() const 
        { 
            return *m_interpolator->transformer(); 
        }
        void transformer(const trans_type& trans) 
        { 
            m_interpolator->transformer(trans); 
        }

        //----------------------------------------------------------------
        unsigned subdiv_shift() const { return m_subdiv_shift; }
        void subdiv_shift(unsigned shift) 
        {
            m_subdiv_shift = shift;
            m_subdiv_size = 1 << m_subdiv_shift;
            m_subdiv_mask = m_subdiv_size - 1;
        }

        //----------------------------------------------------------------
        void begin(double x, double y, unsigned len)
        {
            m_pos   = 1;
            m_src_x = iround(x * subpixel_scale) + subpixel_scale;
            m_src_y = y;
            m_len   = len;
            if(len > m_subdiv_size) len = m_subdiv_size;
            m_interpolator->begin(x, y, len);
        }

        //----------------------------------------------------------------
        void operator++()
        {
            ++(*m_interpolator);
            if(m_pos >= m_subdiv_size)
            {
                unsigned len = m_len;
                if(len > m_subdiv_size) len = m_subdiv_size;
                m_interpolator->resynchronize(double(m_src_x) / double(subpixel_scale) + len, 
                                              m_src_y, 
                                              len);
                m_pos = 0;
            }
            m_src_x += subpixel_scale;
            ++m_pos;
            --m_len;
        }

        //----------------------------------------------------------------
        void coordinates(int* x, int* y) const
        {
            m_interpolator->coordinates(x, y);
        }

        //----------------------------------------------------------------
        void local_scale(int* x, int* y) const
        {
            m_interpolator->local_scale(x, y);
        }


    private:
        unsigned m_subdiv_shift;
        unsigned m_subdiv_size;
        unsigned m_subdiv_mask;
        interpolator_type* m_interpolator;
        int      m_src_x;
        double   m_src_y;
        unsigned m_pos;
        unsigned m_len;
    };

}

#endif
