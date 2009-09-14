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

#ifndef AGG_PATH_LENGTH_INCLUDED
#define AGG_PATH_LENGTH_INCLUDED

#include "agg_math.h"

namespace agg
{
    template<class VertexSource> 
    double path_length(VertexSource& vs, unsigned path_id = 0)
    {
        double len = 0.0;
        double start_x = 0.0;
        double start_y = 0.0;
        double x1 = 0.0;
        double y1 = 0.0;
        double x2 = 0.0;
        double y2 = 0.0;
        bool first = true;

        unsigned cmd;
        vs.rewind(path_id);
        while(!is_stop(cmd = vs.vertex(&x2, &y2)))
        {
            if(is_vertex(cmd))
            {
                if(first || is_move_to(cmd))
                {
                    start_x = x2;
                    start_y = y2;
                }
                else
                {
                    len += calc_distance(x1, y1, x2, y2);
                }
                x1 = x2;
                y1 = y2;
                first = false;
            }
            else
            {
                if(is_close(cmd) && !first)
                {
                    len += calc_distance(x1, y1, start_x, start_y);
                }
            }
        }
        return len;
    }
}

#endif
