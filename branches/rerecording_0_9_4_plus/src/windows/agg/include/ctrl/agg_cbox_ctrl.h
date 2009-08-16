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

#ifndef AGG_CBOX_CTRL_INCLUDED
#define AGG_CBOX_CTRL_INCLUDED

#include "agg_basics.h"
#include "agg_conv_stroke.h"
#include "agg_gsv_text.h"
#include "agg_trans_affine.h"
#include "agg_color_rgba.h"
#include "agg_ctrl.h"



namespace agg
{

    //----------------------------------------------------------cbox_ctrl_impl
    class cbox_ctrl_impl : public ctrl
    {
    public:
        cbox_ctrl_impl(double x, double y, const char* label, bool flip_y=false);

        void text_thickness(double t)  { m_text_thickness = t; }
        void text_size(double h, double w=0.0);

        const char* label() { return m_label; }
        void label(const char* l);

        bool status() const { return m_status; }
        void status(bool st) { m_status = st; }

        virtual bool in_rect(double x, double y) const;
        virtual bool on_mouse_button_down(double x, double y);
        virtual bool on_mouse_button_up(double x, double y);
        virtual bool on_mouse_move(double x, double y, bool button_flag);
        virtual bool on_arrow_keys(bool left, bool right, bool down, bool up);

        // Vertex soutce interface
        unsigned num_paths() { return 3; };
        void     rewind(unsigned path_id);
        unsigned vertex(double* x, double* y);

    private:
        double   m_text_thickness;
        double   m_text_height;
        double   m_text_width;
        char     m_label[128];
        bool     m_status;
        double   m_vx[32];
        double   m_vy[32];

        gsv_text              m_text;
        conv_stroke<gsv_text> m_text_poly;

        unsigned m_idx;
        unsigned m_vertex;
    };


    //----------------------------------------------------------cbox_ctrl_impl
    template<class ColorT> class cbox_ctrl : public cbox_ctrl_impl
    {
    public:
        cbox_ctrl(double x, double y, const char* label, bool flip_y=false) :
            cbox_ctrl_impl(x, y, label, flip_y),
            m_text_color(rgba(0.0, 0.0, 0.0)),
            m_inactive_color(rgba(0.0, 0.0, 0.0)),
            m_active_color(rgba(0.4, 0.0, 0.0))
        {
            m_colors[0] = &m_inactive_color;
            m_colors[1] = &m_text_color;
            m_colors[2] = &m_active_color;
        }
          
        void text_color(const ColorT& c) { m_text_color = c; }
        void inactive_color(const ColorT& c) { m_inactive_color = c; }
        void active_color(const ColorT& c) { m_active_color = c; }

        const ColorT& color(unsigned i) const { return *m_colors[i]; } 

    private:
        cbox_ctrl(const cbox_ctrl<ColorT>&);
        const cbox_ctrl<ColorT>& operator = (const cbox_ctrl<ColorT>&);

        ColorT m_text_color;
        ColorT m_inactive_color;
        ColorT m_active_color;
        ColorT* m_colors[3];
    };


}

#endif
