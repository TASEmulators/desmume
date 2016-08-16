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

#ifndef AGG_SPAN_ALLOCATOR_INCLUDED
#define AGG_SPAN_ALLOCATOR_INCLUDED

#include "agg_array.h"

namespace agg
{
    //----------------------------------------------------------span_allocator
    template<class ColorT> class span_allocator
    {
    public:
        typedef ColorT color_type;

        //--------------------------------------------------------------------
        AGG_INLINE color_type* allocate(unsigned span_len)
        {
            if(span_len > m_span.size())
            {
                // To reduce the number of reallocs we align the 
                // span_len to 256 color elements. 
                // Well, I just like this number and it looks reasonable.
                //-----------------------
                m_span.resize(((span_len + 255) >> 8) << 8);
            }
            return &m_span[0];
        }

        AGG_INLINE color_type* span()               { return &m_span[0]; }
        AGG_INLINE unsigned    max_span_len() const { return m_span.size(); }

    private:
        pod_array<color_type> m_span;
    };
}


#endif


