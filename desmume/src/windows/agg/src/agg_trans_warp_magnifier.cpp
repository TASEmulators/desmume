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

#include <math.h>
#include "agg_trans_warp_magnifier.h"

namespace agg
{

    //------------------------------------------------------------------------
    void trans_warp_magnifier::transform(double* x, double* y) const
    {
        double dx = *x - m_xc;
        double dy = *y - m_yc;
        double r = sqrt(dx * dx + dy * dy);
        if(r < m_radius)
        {
            *x = m_xc + dx * m_magn;
            *y = m_yc + dy * m_magn;
            return;
        }

        double m = (r + m_radius * (m_magn - 1.0)) / r;
        *x = m_xc + dx * m;
        *y = m_yc + dy * m;
    }

    //------------------------------------------------------------------------
    void trans_warp_magnifier::inverse_transform(double* x, double* y) const
    {
        // New version by Andrew Skalkin
        //-----------------
        double dx = *x - m_xc;
        double dy = *y - m_yc;
        double r = sqrt(dx * dx + dy * dy);

        if(r < m_radius * m_magn) 
        {
            *x = m_xc + dx / m_magn;
            *y = m_yc + dy / m_magn;
        }
        else
        {
            double rnew = r - m_radius * (m_magn - 1.0);
            *x = m_xc + rnew * dx / r; 
            *y = m_yc + rnew * dy / r;
        }

        // Old version
        //-----------------
        //trans_warp_magnifier t(*this);
        //t.magnification(1.0 / m_magn);
        //t.radius(m_radius * m_magn);
        //t.transform(x, y);
    }


}
