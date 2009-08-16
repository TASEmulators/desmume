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

#ifndef AGG_SPAN_INTERPOLATOR_ADAPTOR_INCLUDED
#define AGG_SPAN_INTERPOLATOR_ADAPTOR_INCLUDED

#include "agg_basics.h"

namespace agg
{

    //===============================================span_interpolator_adaptor
    template<class Interpolator, class Distortion>
    class span_interpolator_adaptor : public Interpolator
    {
    public:
        typedef Interpolator base_type;
        typedef typename base_type::trans_type trans_type;
        typedef Distortion distortion_type;

        //--------------------------------------------------------------------
        span_interpolator_adaptor() {}
        span_interpolator_adaptor(const trans_type& trans, 
                                  const distortion_type& dist) :
            base_type(trans),
            m_distortion(&dist)
        {   
        }

        //--------------------------------------------------------------------
        span_interpolator_adaptor(const trans_type& trans,
                                  const distortion_type& dist,
                                  double x, double y, unsigned len) :
            base_type(trans, x, y, len),
            m_distortion(&dist)
        {
        }

        //--------------------------------------------------------------------
        const distortion_type& distortion() const
        {
            return *m_distortion;
        }

        //--------------------------------------------------------------------
        void distortion(const distortion_type& dist)
        {
            m_distortion = dist;
        }

        //--------------------------------------------------------------------
        void coordinates(int* x, int* y) const
        {
            base_type::coordinates(x, y);
            m_distortion->calculate(x, y);
        }

    private:
        //--------------------------------------------------------------------
        const distortion_type* m_distortion;
    };
}


#endif
