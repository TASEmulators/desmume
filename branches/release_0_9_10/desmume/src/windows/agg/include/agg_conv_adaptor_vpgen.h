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

#ifndef AGG_CONV_ADAPTOR_VPGEN_INCLUDED
#define AGG_CONV_ADAPTOR_VPGEN_INCLUDED

#include "agg_basics.h"

namespace agg
{

    //======================================================conv_adaptor_vpgen
    template<class VertexSource, class VPGen> class conv_adaptor_vpgen
    {
    public:
        explicit conv_adaptor_vpgen(VertexSource& source) : m_source(&source) {}
        void attach(VertexSource& source) { m_source = &source; }

        VPGen& vpgen() { return m_vpgen; }
        const VPGen& vpgen() const { return m_vpgen; }

        void rewind(unsigned path_id);
        unsigned vertex(double* x, double* y);

    private:
        conv_adaptor_vpgen(const conv_adaptor_vpgen<VertexSource, VPGen>&);
        const conv_adaptor_vpgen<VertexSource, VPGen>& 
            operator = (const conv_adaptor_vpgen<VertexSource, VPGen>&);

        VertexSource* m_source;
        VPGen         m_vpgen;
        double        m_start_x;
        double        m_start_y;
        unsigned      m_poly_flags;
        int           m_vertices;
    };



    //------------------------------------------------------------------------
    template<class VertexSource, class VPGen>
    void conv_adaptor_vpgen<VertexSource, VPGen>::rewind(unsigned path_id) 
    { 
        m_source->rewind(path_id);
        m_vpgen.reset();
        m_start_x    = 0;
        m_start_y    = 0;
        m_poly_flags = 0;
        m_vertices   = 0;
    }


    //------------------------------------------------------------------------
    template<class VertexSource, class VPGen>
    unsigned conv_adaptor_vpgen<VertexSource, VPGen>::vertex(double* x, double* y)
    {
        unsigned cmd = path_cmd_stop;
        for(;;)
        {
            cmd = m_vpgen.vertex(x, y);
            if(!is_stop(cmd)) break;

            if(m_poly_flags && !m_vpgen.auto_unclose())
            {
                *x = 0.0;
                *y = 0.0;
                cmd = m_poly_flags;
                m_poly_flags = 0;
                break;
            }

            if(m_vertices < 0)
            {
                if(m_vertices < -1) 
                {
                    m_vertices = 0;
                    return path_cmd_stop;
                }
                m_vpgen.move_to(m_start_x, m_start_y);
                m_vertices = 1;
                continue;
            }

            double tx, ty;
            cmd = m_source->vertex(&tx, &ty);
            if(is_vertex(cmd))
            {
                if(is_move_to(cmd)) 
                {
                    if(m_vpgen.auto_close() && m_vertices > 2)
                    {
                        m_vpgen.line_to(m_start_x, m_start_y);
                        m_poly_flags = path_cmd_end_poly | path_flags_close;
                        m_start_x    = tx;
                        m_start_y    = ty;
                        m_vertices   = -1;
                        continue;
                    }
                    m_vpgen.move_to(tx, ty);
                    m_start_x  = tx;
                    m_start_y  = ty;
                    m_vertices = 1;
                }
                else 
                {
                    m_vpgen.line_to(tx, ty);
                    ++m_vertices;
                }
            }
            else
            {
                if(is_end_poly(cmd))
                {
                    m_poly_flags = cmd;
                    if(is_closed(cmd) || m_vpgen.auto_close())
                    {
                        if(m_vpgen.auto_close()) m_poly_flags |= path_flags_close;
                        if(m_vertices > 2)
                        {
                            m_vpgen.line_to(m_start_x, m_start_y);
                        }
                        m_vertices = 0;
                    }
                }
                else
                {
                    // path_cmd_stop
                    if(m_vpgen.auto_close() && m_vertices > 2)
                    {
                        m_vpgen.line_to(m_start_x, m_start_y);
                        m_poly_flags = path_cmd_end_poly | path_flags_close;
                        m_vertices   = -2;
                        continue;
                    }
                    break;
                }
            }
        }
        return cmd;
    }


}


#endif

