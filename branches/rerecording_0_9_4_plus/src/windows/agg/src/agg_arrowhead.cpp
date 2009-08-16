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

#include "agg_arrowhead.h"

namespace agg
{

    //------------------------------------------------------------------------
    arrowhead::arrowhead() :
        m_head_d1(1.0),
        m_head_d2(1.0),
        m_head_d3(1.0),
        m_head_d4(0.0),
        m_tail_d1(1.0),
        m_tail_d2(1.0),
        m_tail_d3(1.0),
        m_tail_d4(0.0),
        m_head_flag(false),
        m_tail_flag(false),
        m_curr_id(0),
        m_curr_coord(0)
    {
    }



    //------------------------------------------------------------------------
    void arrowhead::rewind(unsigned path_id)
    {
        m_curr_id = path_id;
        m_curr_coord = 0;
        if(path_id == 0)
        {
            if(!m_tail_flag)
            {
                m_cmd[0] = path_cmd_stop;
                return;
            }
            m_coord[0]  =  m_tail_d1;             m_coord[1]  =  0.0;
            m_coord[2]  =  m_tail_d1 - m_tail_d4; m_coord[3]  =  m_tail_d3;
            m_coord[4]  = -m_tail_d2 - m_tail_d4; m_coord[5]  =  m_tail_d3;
            m_coord[6]  = -m_tail_d2;             m_coord[7]  =  0.0;
            m_coord[8]  = -m_tail_d2 - m_tail_d4; m_coord[9]  = -m_tail_d3;
            m_coord[10] =  m_tail_d1 - m_tail_d4; m_coord[11] = -m_tail_d3;

            m_cmd[0] = path_cmd_move_to;
            m_cmd[1] = path_cmd_line_to;
            m_cmd[2] = path_cmd_line_to;
            m_cmd[3] = path_cmd_line_to;
            m_cmd[4] = path_cmd_line_to;
            m_cmd[5] = path_cmd_line_to;
            m_cmd[7] = path_cmd_end_poly | path_flags_close | path_flags_ccw;
            m_cmd[6] = path_cmd_stop;
            return;
        }

        if(path_id == 1)
        {
            if(!m_head_flag)
            {
                m_cmd[0] = path_cmd_stop;
                return;
            }
            m_coord[0]  = -m_head_d1;            m_coord[1]  = 0.0;
            m_coord[2]  = m_head_d2 + m_head_d4; m_coord[3]  = -m_head_d3;
            m_coord[4]  = m_head_d2;             m_coord[5]  = 0.0;
            m_coord[6]  = m_head_d2 + m_head_d4; m_coord[7]  = m_head_d3;

            m_cmd[0] = path_cmd_move_to;
            m_cmd[1] = path_cmd_line_to;
            m_cmd[2] = path_cmd_line_to;
            m_cmd[3] = path_cmd_line_to;
            m_cmd[4] = path_cmd_end_poly | path_flags_close | path_flags_ccw;
            m_cmd[5] = path_cmd_stop;
            return;
        }
    }


    //------------------------------------------------------------------------
    unsigned arrowhead::vertex(double* x, double* y)
    {
        if(m_curr_id < 2)
        {
            unsigned curr_idx = m_curr_coord * 2;
            *x = m_coord[curr_idx];
            *y = m_coord[curr_idx + 1];
            return m_cmd[m_curr_coord++];
        }
        return path_cmd_stop;
    }

}
