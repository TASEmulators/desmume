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

#ifndef AGG_VCGEN_SMOOTH_POLY1_INCLUDED
#define AGG_VCGEN_SMOOTH_POLY1_INCLUDED

#include "agg_basics.h"
#include "agg_vertex_sequence.h"


namespace agg
{

    //======================================================vcgen_smooth_poly1
    //
    // See Implementation agg_vcgen_smooth_poly1.cpp 
    // Smooth polygon generator
    //
    //------------------------------------------------------------------------
    class vcgen_smooth_poly1
    {
        enum status_e
        {
            initial,
            ready,
            polygon,
            ctrl_b,
            ctrl_e,
            ctrl1,
            ctrl2,
            end_poly,
            stop
        };

    public:
        typedef vertex_sequence<vertex_dist, 6> vertex_storage;

        vcgen_smooth_poly1();

        void   smooth_value(double v) { m_smooth_value = v * 0.5; }
        double smooth_value() const { return m_smooth_value * 2.0; }

        // Vertex Generator Interface
        void remove_all();
        void add_vertex(double x, double y, unsigned cmd);

        // Vertex Source Interface
        void     rewind(unsigned path_id);
        unsigned vertex(double* x, double* y);

    private:
        vcgen_smooth_poly1(const vcgen_smooth_poly1&);
        const vcgen_smooth_poly1& operator = (const vcgen_smooth_poly1&);

        void calculate(const vertex_dist& v0, 
                       const vertex_dist& v1, 
                       const vertex_dist& v2,
                       const vertex_dist& v3);

        vertex_storage m_src_vertices;
        double         m_smooth_value;
        unsigned       m_closed;
        status_e       m_status;
        unsigned       m_src_vertex;
        double         m_ctrl1_x;
        double         m_ctrl1_y;
        double         m_ctrl2_x;
        double         m_ctrl2_y;
    };

}


#endif

