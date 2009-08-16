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

#ifndef AGG_VCGEN_MARKERS_TERM_INCLUDED
#define AGG_VCGEN_MARKERS_TERM_INCLUDED

#include "agg_basics.h"
#include "agg_vertex_sequence.h"

namespace agg
{

    //======================================================vcgen_markers_term
    //
    // See Implemantation agg_vcgen_markers_term.cpp
    // Terminal markers generator (arrowhead/arrowtail)
    //
    //------------------------------------------------------------------------
    class vcgen_markers_term
    {
    public:
        vcgen_markers_term() : m_curr_id(0), m_curr_idx(0) {}

        // Vertex Generator Interface
        void remove_all();
        void add_vertex(double x, double y, unsigned cmd);

        // Vertex Source Interface
        void rewind(unsigned path_id);
        unsigned vertex(double* x, double* y);

    private:
        vcgen_markers_term(const vcgen_markers_term&);
        const vcgen_markers_term& operator = (const vcgen_markers_term&);

        struct coord_type
        {
            double x, y;

            coord_type() {}
            coord_type(double x_, double y_) : x(x_), y(y_) {}
        };

        typedef pod_bvector<coord_type, 6> coord_storage; 

        coord_storage m_markers;
        unsigned      m_curr_id;
        unsigned      m_curr_idx;
    };


}

#endif
