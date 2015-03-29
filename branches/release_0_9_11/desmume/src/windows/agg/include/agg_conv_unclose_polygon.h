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

#ifndef AGG_CONV_UNCLOSE_POLYGON_INCLUDED
#define AGG_CONV_UNCLOSE_POLYGON_INCLUDED

#include "agg_basics.h"

namespace agg
{
    //====================================================conv_unclose_polygon
    template<class VertexSource> class conv_unclose_polygon
    {
    public:
        explicit conv_unclose_polygon(VertexSource& vs) : m_source(&vs) {}
        void attach(VertexSource& source) { m_source = &source; }

        void rewind(unsigned path_id)
        {
            m_source->rewind(path_id);
        }

        unsigned vertex(double* x, double* y)
        {
            unsigned cmd = m_source->vertex(x, y);
            if(is_end_poly(cmd)) cmd &= ~path_flags_close;
            return cmd;
        }

    private:
        conv_unclose_polygon(const conv_unclose_polygon<VertexSource>&);
        const conv_unclose_polygon<VertexSource>& 
            operator = (const conv_unclose_polygon<VertexSource>&);

        VertexSource* m_source;
    };

}

#endif
