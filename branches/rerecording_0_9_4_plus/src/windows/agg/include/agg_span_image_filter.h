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

#ifndef AGG_SPAN_IMAGE_FILTER_INCLUDED
#define AGG_SPAN_IMAGE_FILTER_INCLUDED

#include "agg_basics.h"
#include "agg_image_filters.h"
#include "agg_span_interpolator_linear.h"

namespace agg
{

    //-------------------------------------------------------span_image_filter
    template<class Source, class Interpolator> class span_image_filter
    {
    public:
        typedef Source source_type;
        typedef Interpolator interpolator_type;

        //--------------------------------------------------------------------
        span_image_filter() {}
        span_image_filter(source_type& src, 
                          interpolator_type& interpolator,
                          const image_filter_lut* filter) : 
            m_src(&src),
            m_interpolator(&interpolator),
            m_filter(filter),
            m_dx_dbl(0.5),
            m_dy_dbl(0.5),
            m_dx_int(image_subpixel_scale / 2),
            m_dy_int(image_subpixel_scale / 2)
        {}
        void attach(source_type& v) { m_src = &v; }

        //--------------------------------------------------------------------
               source_type& source()            { return *m_src; }
        const  source_type& source()      const { return *m_src; }
        const  image_filter_lut& filter() const { return *m_filter; }
        int    filter_dx_int()            const { return m_dx_int; }
        int    filter_dy_int()            const { return m_dy_int; }
        double filter_dx_dbl()            const { return m_dx_dbl; }
        double filter_dy_dbl()            const { return m_dy_dbl; }

        //--------------------------------------------------------------------
        void interpolator(interpolator_type& v)  { m_interpolator = &v; }
        void filter(const image_filter_lut& v)   { m_filter = &v; }
        void filter_offset(double dx, double dy)
        {
            m_dx_dbl = dx;
            m_dy_dbl = dy;
            m_dx_int = iround(dx * image_subpixel_scale);
            m_dy_int = iround(dy * image_subpixel_scale);
        }
        void filter_offset(double d) { filter_offset(d, d); }

        //--------------------------------------------------------------------
        interpolator_type& interpolator() { return *m_interpolator; }

        //--------------------------------------------------------------------
        void prepare() {}

        //--------------------------------------------------------------------
    private:
        source_type*            m_src;
        interpolator_type*      m_interpolator;
        const image_filter_lut* m_filter;
        double   m_dx_dbl;
        double   m_dy_dbl;
        unsigned m_dx_int;
        unsigned m_dy_int;
    };




    //==============================================span_image_resample_affine
    template<class Source> 
    class span_image_resample_affine : 
    public span_image_filter<Source, span_interpolator_linear<trans_affine> >
    {
    public:
        typedef Source source_type;
        typedef span_interpolator_linear<trans_affine> interpolator_type;
        typedef span_image_filter<source_type, interpolator_type> base_type;

        //--------------------------------------------------------------------
        span_image_resample_affine() : 
            m_scale_limit(200.0),
            m_blur_x(1.0),
            m_blur_y(1.0)
        {}

        //--------------------------------------------------------------------
        span_image_resample_affine(source_type& src, 
                                   interpolator_type& inter,
                                   const image_filter_lut& filter) :
            base_type(src, inter, &filter),
            m_scale_limit(200.0),
            m_blur_x(1.0),
            m_blur_y(1.0)
        {}


        //--------------------------------------------------------------------
        int  scale_limit() const { return uround(m_scale_limit); }
        void scale_limit(int v)  { m_scale_limit = v; }

        //--------------------------------------------------------------------
        double blur_x() const { return m_blur_x; }
        double blur_y() const { return m_blur_y; }
        void blur_x(double v) { m_blur_x = v; }
        void blur_y(double v) { m_blur_y = v; }
        void blur(double v) { m_blur_x = m_blur_y = v; }

        //--------------------------------------------------------------------
        void prepare() 
        {
            double scale_x;
            double scale_y;

            base_type::interpolator().transformer().scaling_abs(&scale_x, &scale_y);

            if(scale_x * scale_y > m_scale_limit)
            {
                scale_x = scale_x * m_scale_limit / (scale_x * scale_y);
                scale_y = scale_y * m_scale_limit / (scale_x * scale_y);
            }

            if(scale_x < 1) scale_x = 1;
            if(scale_y < 1) scale_y = 1;

            if(scale_x > m_scale_limit) scale_x = m_scale_limit;
            if(scale_y > m_scale_limit) scale_y = m_scale_limit;

            scale_x *= m_blur_x;
            scale_y *= m_blur_y;

            if(scale_x < 1) scale_x = 1;
            if(scale_y < 1) scale_y = 1;

            m_rx     = uround(    scale_x * double(image_subpixel_scale));
            m_rx_inv = uround(1.0/scale_x * double(image_subpixel_scale));

            m_ry     = uround(    scale_y * double(image_subpixel_scale));
            m_ry_inv = uround(1.0/scale_y * double(image_subpixel_scale));
        }

    protected:
        int m_rx;
        int m_ry;
        int m_rx_inv;
        int m_ry_inv;

    private:
        double m_scale_limit;
        double m_blur_x;
        double m_blur_y;
    };



    //=====================================================span_image_resample
    template<class Source, class Interpolator> 
    class span_image_resample : 
    public span_image_filter<Source, Interpolator>
    {
    public:
        typedef Source source_type;
        typedef Interpolator interpolator_type;
        typedef span_image_filter<source_type, interpolator_type> base_type;

        //--------------------------------------------------------------------
        span_image_resample() : 
            m_scale_limit(20),
            m_blur_x(image_subpixel_scale),
            m_blur_y(image_subpixel_scale)
        {}

        //--------------------------------------------------------------------
        span_image_resample(source_type& src, 
                            interpolator_type& inter,
                            const image_filter_lut& filter) :
            base_type(src, inter, &filter),
            m_scale_limit(20),
            m_blur_x(image_subpixel_scale),
            m_blur_y(image_subpixel_scale)
        {}

        //--------------------------------------------------------------------
        int  scale_limit() const { return m_scale_limit; }
        void scale_limit(int v)  { m_scale_limit = v; }

        //--------------------------------------------------------------------
        double blur_x() const { return double(m_blur_x) / double(image_subpixel_scale); }
        double blur_y() const { return double(m_blur_y) / double(image_subpixel_scale); }
        void blur_x(double v) { m_blur_x = uround(v * double(image_subpixel_scale)); }
        void blur_y(double v) { m_blur_y = uround(v * double(image_subpixel_scale)); }
        void blur(double v)   { m_blur_x = 
                                m_blur_y = uround(v * double(image_subpixel_scale)); }

    protected:
        AGG_INLINE void adjust_scale(int* rx, int* ry)
        {
            if(*rx < image_subpixel_scale) *rx = image_subpixel_scale;
            if(*ry < image_subpixel_scale) *ry = image_subpixel_scale;
            if(*rx > image_subpixel_scale * m_scale_limit) 
            {
                *rx = image_subpixel_scale * m_scale_limit;
            }
            if(*ry > image_subpixel_scale * m_scale_limit) 
            {
                *ry = image_subpixel_scale * m_scale_limit;
            }
            *rx = (*rx * m_blur_x) >> image_subpixel_shift;
            *ry = (*ry * m_blur_y) >> image_subpixel_shift;
            if(*rx < image_subpixel_scale) *rx = image_subpixel_scale;
            if(*ry < image_subpixel_scale) *ry = image_subpixel_scale;
        }

        int m_scale_limit;
        int m_blur_x;
        int m_blur_y;
    };




}

#endif
