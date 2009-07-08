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

#include "agg_renderer_outline_aa.h"

namespace agg
{

    //---------------------------------------------------------------------
    void line_profile_aa::width(double w)
    {
        if(w < 0.0) w = 0.0;

        if(w < m_smoother_width) w += w;
        else                     w += m_smoother_width;

        w *= 0.5;

        w -= m_smoother_width;
        double s = m_smoother_width;
        if(w < 0.0) 
        {
            s += w;
            w = 0.0;
        }
        set(w, s);
    }


    //---------------------------------------------------------------------
    line_profile_aa::value_type* line_profile_aa::profile(double w)
    {
        m_subpixel_width = uround(w * subpixel_scale);
        unsigned size = m_subpixel_width + subpixel_scale * 6;
        if(size > m_profile.size())
        {
            m_profile.resize(size);
        }
        return &m_profile[0];
    }


    //---------------------------------------------------------------------
    void line_profile_aa::set(double center_width, double smoother_width)
    {
        double base_val = 1.0;
        if(center_width == 0.0)   center_width = 1.0 / subpixel_scale;
        if(smoother_width == 0.0) smoother_width = 1.0 / subpixel_scale;

        double width = center_width + smoother_width;
        if(width < m_min_width)
        {
            double k = width / m_min_width;
            base_val *= k;
            center_width /= k;
            smoother_width /= k;
        }

        value_type* ch = profile(center_width + smoother_width);

        unsigned subpixel_center_width = unsigned(center_width * subpixel_scale);
        unsigned subpixel_smoother_width = unsigned(smoother_width * subpixel_scale);

        value_type* ch_center   = ch + subpixel_scale*2;
        value_type* ch_smoother = ch_center + subpixel_center_width;

        unsigned i;

        unsigned val = m_gamma[unsigned(base_val * aa_mask)];
        ch = ch_center;
        for(i = 0; i < subpixel_center_width; i++)
        {
            *ch++ = (value_type)val;
        }

        for(i = 0; i < subpixel_smoother_width; i++)
        {
            *ch_smoother++ = 
                m_gamma[unsigned((base_val - 
                                  base_val * 
                                  (double(i) / subpixel_smoother_width)) * aa_mask)];
        }

        unsigned n_smoother = profile_size() - 
                              subpixel_smoother_width - 
                              subpixel_center_width - 
                              subpixel_scale*2;

        val = m_gamma[0];
        for(i = 0; i < n_smoother; i++)
        {
            *ch_smoother++ = (value_type)val;
        }

        ch = ch_center;
        for(i = 0; i < subpixel_scale*2; i++)
        {
            *--ch = *ch_center++;
        }
    }


}

