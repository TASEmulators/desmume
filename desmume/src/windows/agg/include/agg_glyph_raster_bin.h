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

#ifndef AGG_GLYPH_RASTER_BIN_INCLUDED
#define AGG_GLYPH_RASTER_BIN_INCLUDED

#include <string.h>
#include "agg_basics.h"

namespace agg
{

    //========================================================glyph_raster_bin
    template<class ColorT> class glyph_raster_bin
    {
    public:
        typedef ColorT color_type;

        //--------------------------------------------------------------------
        struct glyph_rect
        {
            int x1,y1,x2,y2;
            double dx, dy;
        };

        //--------------------------------------------------------------------
        glyph_raster_bin(const int8u* font) :
            m_font(font),
            m_big_endian(false)
        {
            int t = 1;
            if(*(char*)&t == 0) m_big_endian = true;
            memset(m_span, 0, sizeof(m_span));
        }

        //--------------------------------------------------------------------
        const int8u* font() const { return m_font; }
        void font(const int8u* f) { m_font = f; }

        //--------------------------------------------------------------------
        double height()    const { return m_font[0]; }
        double base_line() const { return m_font[1]; }

        //--------------------------------------------------------------------
        template<class CharT>
        double width(const CharT* str) const
        {
            unsigned start_char = m_font[2];
            unsigned num_chars = m_font[3];

            unsigned w = 0;
            while(*str)
            {
                unsigned glyph = *str;
                const int8u* bits = m_font + 4 + num_chars * 2 + 
                                    value(m_font + 4 + (glyph - start_char) * 2);
                w += *bits;
                ++str;
            }
            return w;
        }

        //--------------------------------------------------------------------
        void prepare(glyph_rect* r, double x, double y, unsigned glyph, bool flip)
        {
            unsigned start_char = m_font[2];
            unsigned num_chars = m_font[3];

            m_bits = m_font + 4 + num_chars * 2 + 
                     value(m_font + 4 + (glyph - start_char) * 2);

            m_glyph_width = *m_bits++;
            m_glyph_byte_width = (m_glyph_width + 7) >> 3;

            r->x1 = int(x);
            r->x2 = r->x1 + m_glyph_width - 1;
            if(flip)
            {
                r->y1 = int(y) - m_font[0] + m_font[1];
                r->y2 = r->y1 + m_font[0] - 1;
            }
            else
            {
                r->y1 = int(y) - m_font[1] + 1;
                r->y2 = r->y1 + m_font[0] - 1;
            }
            r->dx = m_glyph_width; 
            r->dy = 0;
        }

        //--------------------------------------------------------------------
        const cover_type* span(unsigned i)
        {
            i = m_font[0] - i - 1;
            const int8u* bits = m_bits + i * m_glyph_byte_width;
            unsigned j;
            unsigned val = *bits;
            unsigned nb = 0;
            for(j = 0; j < m_glyph_width; ++j)
            {
                m_span[j] = (cover_type)((val & 0x80) ? cover_full : cover_none);
                val <<= 1;
                if(++nb >= 8)
                {
                    val = *++bits;
                    nb = 0;
                }
            }
            return m_span;
        }

    private:
        //--------------------------------------------------------------------
        int16u value(const int8u* p) const
        {
            int16u v;
            if(m_big_endian)
            {
                 *(int8u*)&v      = p[1];
                *((int8u*)&v + 1) = p[0];
            }
            else
            {
                 *(int8u*)&v      = p[0];
                *((int8u*)&v + 1) = p[1];
            }
            return v;
        }


        //--------------------------------------------------------------------
        const int8u* m_font;
        bool m_big_endian;
        cover_type m_span[32];
        const int8u* m_bits;
        unsigned m_glyph_width;
        unsigned m_glyph_byte_width;
    };


}

#endif
