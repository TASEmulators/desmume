//----------------------------------------------------------------------------
// Anti-Grain Geometry (AGG) - Version 2.5
// A high quality rendering engine for C++
// Copyright (C) 2002-2006 Maxim Shemanarev
// Copyright (C) 2002 Hansruedi Baer (MacOS support)
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

#ifndef AGG_MAC_PMAP_INCLUDED
#define AGG_MAC_PMAP_INCLUDED


#include <stdio.h>
#include <Carbon.h>


namespace agg
{
    enum org_e
    {
        org_mono8   = 8,
        org_color16 = 16,
        org_color24 = 24,
        org_color32 = 32
    };

    class pixel_map
    {
    public:
        ~pixel_map();
        pixel_map();

    public:
        void        destroy();
        void        create(unsigned width, 
                           unsigned height, 
                           org_e    org,
                           unsigned clear_val=255);

        void        clear(unsigned clear_val=255);
        bool        load_from_qt(const char* filename);
        bool        save_as_qt(const char* filename) const;

        void        draw(WindowRef window, 
                         const Rect* device_rect=0, 
                         const Rect* bmp_rect=0) const;
        void        draw(WindowRef window, int x, int y, double scale=1.0) const;
        void        blend(WindowRef window, 
                          const Rect* device_rect=0, 
                          const Rect* bmp_rect=0) const;
        void        blend(WindowRef window, int x, int y, double scale=1.0) const;

        unsigned char* buf();
        unsigned       width() const;
        unsigned       height() const;
        int            row_bytes() const;
        unsigned       bpp() const { return m_bpp; }

        //Auxiliary static functions
        static unsigned calc_row_len(unsigned width, unsigned bits_per_pixel);
    private:
        pixel_map(const pixel_map&);
        const pixel_map& operator = (const pixel_map&);

    private:
        GWorldPtr      m_pmap;
        unsigned char* m_buf;
        unsigned       m_bpp;
        unsigned       m_img_size;
    };

}


#endif
