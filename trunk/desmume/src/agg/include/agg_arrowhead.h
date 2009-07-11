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

#ifndef AGG_ARROWHEAD_INCLUDED
#define AGG_ARROWHEAD_INCLUDED

#include "agg_basics.h"

namespace agg
{

    //===============================================================arrowhead
    //
    // See implementation agg_arrowhead.cpp 
    //
    class arrowhead
    {
    public:
        arrowhead();

        void head(double d1, double d2, double d3, double d4)
        {
            m_head_d1 = d1;
            m_head_d2 = d2;
            m_head_d3 = d3;
            m_head_d4 = d4;
            m_head_flag = true;
        }

        void head()    { m_head_flag = true; }
        void no_head() { m_head_flag = false; }

        void tail(double d1, double d2, double d3, double d4)
        {
            m_tail_d1 = d1;
            m_tail_d2 = d2;
            m_tail_d3 = d3;
            m_tail_d4 = d4;
            m_tail_flag = true;
        }

        void tail()    { m_tail_flag = true;  }
        void no_tail() { m_tail_flag = false; }

        void rewind(unsigned path_id);
        unsigned vertex(double* x, double* y);

    private:
        double   m_head_d1;
        double   m_head_d2;
        double   m_head_d3;
        double   m_head_d4;
        double   m_tail_d1;
        double   m_tail_d2;
        double   m_tail_d3;
        double   m_tail_d4;
        bool     m_head_flag;
        bool     m_tail_flag;
        double   m_coord[16];
        unsigned m_cmd[8];
        unsigned m_curr_id;
        unsigned m_curr_coord;
    };

}

#endif
