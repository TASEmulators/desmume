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

#ifndef AGG_WARP_MAGNIFIER_INCLUDED
#define AGG_WARP_MAGNIFIER_INCLUDED


namespace agg
{

    //----------------------------------------------------trans_warp_magnifier
    //
    // See Inmplementation agg_trans_warp_magnifier.cpp
    //
    class trans_warp_magnifier
    {
    public:
        trans_warp_magnifier() : m_xc(0.0), m_yc(0.0), m_magn(1.0), m_radius(1.0) {}
 
        void center(double x, double y) { m_xc = x; m_yc = y; }
        void magnification(double m)    { m_magn = m;         }
        void radius(double r)           { m_radius = r;       }

        double xc()            const { return m_xc; }
        double yc()            const { return m_yc; }
        double magnification() const { return m_magn;   }
        double radius()        const { return m_radius; }

        void transform(double* x, double* y) const;
        void inverse_transform(double* x, double* y) const;

    private:
        double m_xc;
        double m_yc;
        double m_magn;
        double m_radius;
    };


}


#endif

