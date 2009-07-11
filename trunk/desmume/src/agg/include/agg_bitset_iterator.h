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

#ifndef AGG_BITSET_ITERATOR_INCLUDED
#define AGG_BITSET_ITERATOR_INCLUDED

#include "agg_basics.h"

namespace agg
{
    
    class bitset_iterator
    {
    public:
        bitset_iterator(const int8u* bits, unsigned offset = 0) :
            m_bits(bits + (offset >> 3)),
            m_mask(0x80 >> (offset & 7))
        {}

        void operator ++ ()
        {
            m_mask >>= 1;
            if(m_mask == 0)
            {
                ++m_bits;
                m_mask = 0x80;
            }
        }

        unsigned bit() const
        {
            return (*m_bits) & m_mask;
        }

    private:
        const int8u* m_bits;
        int8u        m_mask;
    };

}

#endif
