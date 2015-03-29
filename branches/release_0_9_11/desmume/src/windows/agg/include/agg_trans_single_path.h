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

#ifndef AGG_TRANS_SINGLE_PATH_INCLUDED
#define AGG_TRANS_SINGLE_PATH_INCLUDED

#include "agg_basics.h"
#include "agg_vertex_sequence.h"

namespace agg
{

    // See also: agg_trans_single_path.cpp
    //
    //-------------------------------------------------------trans_single_path
    class trans_single_path
    {
        enum status_e
        {
            initial,
            making_path,
            ready
        };

    public:
        typedef vertex_sequence<vertex_dist, 6> vertex_storage;

        trans_single_path();

        //--------------------------------------------------------------------
        void   base_length(double v)  { m_base_length = v; }
        double base_length() const { return m_base_length; }

        //--------------------------------------------------------------------
        void preserve_x_scale(bool f) { m_preserve_x_scale = f;    }
        bool preserve_x_scale() const { return m_preserve_x_scale; }

        //--------------------------------------------------------------------
        void reset();
        void move_to(double x, double y);
        void line_to(double x, double y);
        void finalize_path();

        //--------------------------------------------------------------------
        template<class VertexSource> 
        void add_path(VertexSource& vs, unsigned path_id=0)
        {
            double x;
            double y;

            unsigned cmd;
            vs.rewind(path_id);
            while(!is_stop(cmd = vs.vertex(&x, &y)))
            {
                if(is_move_to(cmd)) 
                {
                    move_to(x, y);
                }
                else 
                {
                    if(is_vertex(cmd))
                    {
                        line_to(x, y);
                    }
                }
            }
            finalize_path();
        }

        //--------------------------------------------------------------------
        double total_length() const;
        void transform(double *x, double *y) const;

    private:
        vertex_storage m_src_vertices;
        double         m_base_length;
        double         m_kindex;
        status_e       m_status;
        bool           m_preserve_x_scale;
    };


}

#endif
