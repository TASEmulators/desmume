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

#include <math.h>
#include "agg_vpgen_segmentator.h"

namespace agg
{

    void vpgen_segmentator::move_to(double x, double y)
    {
        m_x1 = x;
        m_y1 = y;
        m_dx = 0.0;
        m_dy = 0.0;
        m_dl = 2.0;
        m_ddl = 2.0;
        m_cmd = path_cmd_move_to;
    }

    void vpgen_segmentator::line_to(double x, double y)
    {
        m_x1 += m_dx;
        m_y1 += m_dy;
        m_dx  = x - m_x1;
        m_dy  = y - m_y1;
        double len = sqrt(m_dx * m_dx + m_dy * m_dy) * m_approximation_scale;
        if(len < 1e-30) len = 1e-30;
        m_ddl = 1.0 / len;
        m_dl  = (m_cmd == path_cmd_move_to) ? 0.0 : m_ddl;
        if(m_cmd == path_cmd_stop) m_cmd = path_cmd_line_to;
    }

    unsigned vpgen_segmentator::vertex(double* x, double* y)
    {
        if(m_cmd == path_cmd_stop) return path_cmd_stop;

        unsigned cmd = m_cmd;
        m_cmd = path_cmd_line_to;
        if(m_dl >= 1.0 - m_ddl)
        {
            m_dl = 1.0;
            m_cmd = path_cmd_stop;
            *x = m_x1 + m_dx;
            *y = m_y1 + m_dy;
            return cmd;
        }
        *x = m_x1 + m_dx * m_dl;
        *y = m_y1 + m_dy * m_dl;
        m_dl += m_ddl;
        return cmd;
    }

}

