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
// Horizontal span interpolator for use with an arbitrary transformer
// The efficiency highly depends on the operations done in the transformer
//
//----------------------------------------------------------------------------

#ifndef AGG_SPAN_INTERPOLATOR_TRANS_INCLUDED
#define AGG_SPAN_INTERPOLATOR_TRANS_INCLUDED

#include "agg_basics.h"

namespace agg
{
    //=================================================span_interpolator_trans
    template<class Transformer, unsigned SubpixelShift = 8> 
    class span_interpolator_trans
    {
    public:
        typedef Transformer trans_type;
        enum subpixel_scale_e
        {
            subpixel_shift = SubpixelShift,
            subpixel_scale = 1 << subpixel_shift
        };

        //--------------------------------------------------------------------
        span_interpolator_trans() {}
        span_interpolator_trans(const trans_type& trans) : m_trans(&trans) {}
        span_interpolator_trans(const trans_type& trans,
                                double x, double y, unsigned) :
            m_trans(&trans)
        {
            begin(x, y, 0);
        }

        //----------------------------------------------------------------
        const trans_type& transformer() const { return *m_trans; }
        void transformer(const trans_type& trans) { m_trans = &trans; }

        //----------------------------------------------------------------
        void begin(double x, double y, unsigned)
        {
            m_x = x;
            m_y = y;
            m_trans->transform(&x, &y);
            m_ix = iround(x * subpixel_scale);
            m_iy = iround(y * subpixel_scale);
        }

        //----------------------------------------------------------------
        void operator++()
        {
            m_x += 1.0;
            double x = m_x;
            double y = m_y;
            m_trans->transform(&x, &y);
            m_ix = iround(x * subpixel_scale);
            m_iy = iround(y * subpixel_scale);
        }

        //----------------------------------------------------------------
        void coordinates(int* x, int* y) const
        {
            *x = m_ix;
            *y = m_iy;
        }

    private:
        const trans_type* m_trans;
        double            m_x;
        double            m_y;
        int               m_ix;
        int               m_iy;
    };

}

#endif
