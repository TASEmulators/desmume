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

#ifndef AGG_ROUNDED_RECT_INCLUDED
#define AGG_ROUNDED_RECT_INCLUDED

#include "agg_basics.h"
#include "agg_arc.h"

namespace agg
{
    //------------------------------------------------------------rounded_rect
    //
    // See Implemantation agg_rounded_rect.cpp
    //
    class rounded_rect
    {
    public:
        rounded_rect() {}
        rounded_rect(double x1, double y1, double x2, double y2, double r);

        void rect(double x1, double y1, double x2, double y2);
        void radius(double r);
        void radius(double rx, double ry);
        void radius(double rx_bottom, double ry_bottom, double rx_top, double ry_top);
        void radius(double rx1, double ry1, double rx2, double ry2, 
                    double rx3, double ry3, double rx4, double ry4);
        void normalize_radius();

        void approximation_scale(double s) { m_arc.approximation_scale(s); }
        double approximation_scale() const { return m_arc.approximation_scale(); }

        void rewind(unsigned);
        unsigned vertex(double* x, double* y);

    private:
        double m_x1;
        double m_y1;
        double m_x2;
        double m_y2;
        double m_rx1;
        double m_ry1;
        double m_rx2;
        double m_ry2;
        double m_rx3;
        double m_ry3;
        double m_rx4;
        double m_ry4;
        unsigned m_status;
        arc      m_arc;
    };

}

#endif

