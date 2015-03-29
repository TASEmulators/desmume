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

#include "agg_vpgen_clip_polyline.h"
#include "agg_clip_liang_barsky.h"

namespace agg
{
    //----------------------------------------------------------------------------
    void vpgen_clip_polyline::reset()
    {
        m_vertex = 0;
        m_num_vertices = 0;
        m_move_to = false;
    }

    //----------------------------------------------------------------------------
    void vpgen_clip_polyline::move_to(double x, double y)
    {
        m_vertex = 0;
        m_num_vertices = 0;
        m_x1 = x;
        m_y1 = y;
        m_move_to = true;
    }

    //----------------------------------------------------------------------------
    void vpgen_clip_polyline::line_to(double x, double y)
    {
        double x2 = x;
        double y2 = y;
        unsigned flags = clip_line_segment(&m_x1, &m_y1, &x2, &y2, m_clip_box);

        m_vertex = 0;
        m_num_vertices = 0;
        if((flags & 4) == 0)
        {
            if((flags & 1) != 0 || m_move_to)
            {
                m_x[0] = m_x1;
                m_y[0] = m_y1;
                m_cmd[0] = path_cmd_move_to;
                m_num_vertices = 1;
            }
            m_x[m_num_vertices] = x2;
            m_y[m_num_vertices] = y2;
            m_cmd[m_num_vertices++] = path_cmd_line_to;
            m_move_to = (flags & 2) != 0;
        }
        m_x1 = x;
        m_y1 = y;
    }

    //----------------------------------------------------------------------------
    unsigned vpgen_clip_polyline::vertex(double* x, double* y)
    {
        if(m_vertex < m_num_vertices)
        {
            *x = m_x[m_vertex];
            *y = m_y[m_vertex];
            return m_cmd[m_vertex++];
        }
        return path_cmd_stop;
    }
}
