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

#ifndef AGG_CONV_CONCAT_INCLUDED
#define AGG_CONV_CONCAT_INCLUDED

#include "agg_basics.h"

namespace agg
{
    //=============================================================conv_concat
    // Concatenation of two paths. Usually used to combine lines or curves 
    // with markers such as arrowheads
    template<class VS1, class VS2> class conv_concat
    {
    public:
        conv_concat(VS1& source1, VS2& source2) :
            m_source1(&source1), m_source2(&source2), m_status(2) {}
        void attach1(VS1& source) { m_source1 = &source; }
        void attach2(VS2& source) { m_source2 = &source; }


        void rewind(unsigned path_id)
        { 
            m_source1->rewind(path_id);
            m_source2->rewind(0);
            m_status = 0;
        }

        unsigned vertex(double* x, double* y)
        {
            unsigned cmd;
            if(m_status == 0)
            {
                cmd = m_source1->vertex(x, y);
                if(!is_stop(cmd)) return cmd;
                m_status = 1;
            }
            if(m_status == 1)
            {
                cmd = m_source2->vertex(x, y);
                if(!is_stop(cmd)) return cmd;
                m_status = 2;
            }
            return path_cmd_stop;
        }

    private:
        conv_concat(const conv_concat<VS1, VS2>&);
        const conv_concat<VS1, VS2>& 
            operator = (const conv_concat<VS1, VS2>&);

        VS1* m_source1;
        VS2* m_source2;
        int  m_status;

    };
}


#endif
