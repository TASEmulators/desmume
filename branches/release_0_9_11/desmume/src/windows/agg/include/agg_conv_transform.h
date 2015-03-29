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

#ifndef AGG_CONV_TRANSFORM_INCLUDED
#define AGG_CONV_TRANSFORM_INCLUDED

#include "agg_basics.h"
#include "agg_trans_affine.h"

namespace agg
{

    //----------------------------------------------------------conv_transform
    template<class VertexSource, class Transformer=trans_affine> class conv_transform
    {
    public:
        conv_transform(VertexSource& source, const Transformer& tr) :
            m_source(&source), m_trans(&tr) {}
        void attach(VertexSource& source) { m_source = &source; }

        void rewind(unsigned path_id) 
        { 
            m_source->rewind(path_id); 
        }

        unsigned vertex(double* x, double* y)
        {
            unsigned cmd = m_source->vertex(x, y);
            if(is_vertex(cmd))
            {
                m_trans->transform(x, y);
            }
            return cmd;
        }

        void transformer(const Transformer& tr)
        {
            m_trans = &tr;
        }

    private:
        conv_transform(const conv_transform<VertexSource>&);
        const conv_transform<VertexSource>& 
            operator = (const conv_transform<VertexSource>&);

        VertexSource*      m_source;
        const Transformer* m_trans;
    };


}

#endif
