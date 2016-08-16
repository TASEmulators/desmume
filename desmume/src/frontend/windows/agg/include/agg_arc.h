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

#ifndef AGG_ARC_INCLUDED
#define AGG_ARC_INCLUDED

#include <math.h>
#include "agg_basics.h"

namespace agg
{

    //=====================================================================arc
    //
    // See Implementation agg_arc.cpp 
    //
    class arc
    {
    public:
        arc() : m_scale(1.0), m_initialized(false) {}
        arc(double x,  double y, 
            double rx, double ry, 
            double a1, double a2, 
            bool ccw=true);

        void init(double x,  double y, 
                  double rx, double ry, 
                  double a1, double a2, 
                  bool ccw=true);

        void approximation_scale(double s);
        double approximation_scale() const { return m_scale;  }

        void rewind(unsigned);
        unsigned vertex(double* x, double* y);

    private:
        void normalize(double a1, double a2, bool ccw);

        double   m_x;
        double   m_y;
        double   m_rx;
        double   m_ry;
        double   m_angle;
        double   m_start;
        double   m_end;
        double   m_scale;
        double   m_da;
        bool     m_ccw;
        bool     m_initialized;
        unsigned m_path_cmd;
    };


}


#endif
