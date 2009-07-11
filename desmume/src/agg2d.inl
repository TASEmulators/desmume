//----------------------------------------------------------------------------
// Agg2D - Version 1.0
// Based on Anti-Grain Geometry
// Copyright (C) 2005 Maxim Shemanarev (http://www.antigrain.com)
//
// Permission to copy, use, modify, sell and distribute this software
// is granted provided this copyright notice appears in all copies.
// This software is provided "as is" without express or implied
// warranty, and with no claim as to its suitability for any purpose.
//
//----------------------------------------------------------------------------
// Contact: mcseem@antigrain.com
//          mcseemagg@yahoo.com
//          http://www.antigrain.com
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//
//	25 Jan 2007 - Ported to AGG 2.4 Jerry Evans (jerry@novadsp.com)
//
//----------------------------------------------------------------------------
#include "GPU.h"
#include "agg2d.h"

static const double g_approxScale = 2.0;

#define TAGG2D Agg2D AGG2D_TEMPLATE_ARG
#define TAGG2DRENDERER Agg2DRenderer AGG2D_TEMPLATE_ARG

//AGG2D_TEMPLATE inline bool operator == (const TAGG2D::Color& c1, const TAGG2D::Color& c2)
//{
//   return c1.r == c2.r && c1.g == c2.g && c1.b == c2.b && c1.a == c2.a;
//}
//
//AGG2D_TEMPLATE inline bool operator != (const Agg2D<PixFormat>::Color& c1, const Agg2D<PixFormat>::Color& c2)
//{
//   return !(c1 == c2);
//}

AGG2D_TEMPLATE inline TAGG2D::~Agg2D()
{
#ifndef AGG2D_USE_FREETYPE
    ::ReleaseDC(0, m_fontDC);
#endif
}

AGG2D_TEMPLATE inline TAGG2D::Agg2D() :
    m_rbuf(),
    m_pixFormat(m_rbuf),
    m_pixFormatComp(m_rbuf),
    m_pixFormatPre(m_rbuf),
    m_pixFormatCompPre(m_rbuf),
    m_renBase(m_pixFormat),
    m_renBaseComp(m_pixFormatComp),
    m_renBasePre(m_pixFormatPre),
    m_renBaseCompPre(m_pixFormatCompPre),
    m_renSolid(m_renBase),
    m_renSolidComp(m_renBaseComp),

    m_allocator(),
    m_clipBox(0,0,0,0),

    m_blendMode(BlendAlpha),
    m_imageBlendMode(BlendDst),
    m_imageBlendColor(0,0,0),

    m_scanline(),
    m_rasterizer(),

    m_masterAlpha(1.0),
    m_antiAliasGamma(1.0),

    m_fillColor(255, 255, 255),
    m_lineColor(0,   0,   0),
    m_fillGradient(),
    m_lineGradient(),

    m_lineCap(CAP_ROUND),
    m_lineJoin(JOIN_ROUND),

    m_fillGradientFlag(Solid),
    m_lineGradientFlag(Solid),
    m_fillGradientMatrix(),
    m_lineGradientMatrix(),
    m_fillGradientD1(0.0),
    m_lineGradientD1(0.0),
    m_fillGradientD2(100.0),
    m_lineGradientD2(100.0),

    m_textAngle(0.0),
    m_textAlignX(AlignLeft),
    m_textAlignY(AlignBottom),
    m_textHints(true),
    m_fontHeight(0.0),
    m_fontAscent(0.0),
    m_fontDescent(0.0),
    m_fontCacheType(RasterFontCache),

    m_imageFilter(Bilinear),
    m_imageResample(NoResample),
    m_imageFilterLut(agg::image_filter_bilinear(), true),

    m_fillGradientInterpolator(m_fillGradientMatrix),
    m_lineGradientInterpolator(m_lineGradientMatrix),

    m_linearGradientFunction(),
    m_radialGradientFunction(),

    m_lineWidth(1),
    m_evenOddFlag(false),

    m_start_x(0.0),
    m_start_y(0.0),

    m_path(),
    m_transform(),

    m_convCurve(m_path),
    m_convStroke(m_convCurve),

    m_pathTransform(m_convCurve, m_transform),
    m_strokeTransform(m_convStroke, m_transform),

#ifdef AGG2D_USE_FREETYPE
    m_fontEngine(),
#else
    m_fontDC(::GetDC(0)),
    m_fontEngine(m_fontDC),
#endif

    m_fontCacheManager(m_fontEngine)
{
    lineCap(m_lineCap);
    lineJoin(m_lineJoin);
}

AGG2D_TEMPLATE void Agg2D AGG2D_TEMPLATE_ARG ::saveStateTo(State& st)
{

  st.m_clipBox             = m_clipBox;

  st.m_blendMode           = m_blendMode;
  st.m_imageBlendMode      = m_imageBlendMode;
  st.m_imageBlendColor     = m_imageBlendColor;

  st.m_masterAlpha         = m_masterAlpha;
  st.m_antiAliasGamma      = m_antiAliasGamma;

  st.m_fillColor           = m_fillColor;
  st.m_lineColor           = m_lineColor;
  st.m_fillGradient        = m_fillGradient;
  st.m_lineGradient        = m_lineGradient;

  st.m_lineCap             = m_lineCap;
  st.m_lineJoin            = m_lineJoin;

  st.m_fillGradientFlag    = m_fillGradientFlag;
  st.m_lineGradientFlag    = m_lineGradientFlag;
  st.m_fillGradientMatrix  = m_fillGradientMatrix;
  st.m_lineGradientMatrix  = m_lineGradientMatrix;
  st.m_fillGradientD1      = m_fillGradientD1;
  st.m_lineGradientD1      = m_lineGradientD1;
  st.m_fillGradientD2      = m_fillGradientD2;
  st.m_lineGradientD2      = m_lineGradientD2;

  st.m_textAngle           = m_textAngle;
  st.m_textAlignX          = m_textAlignX;
  st.m_textAlignY          = m_textAlignY;
  st.m_textHints           = m_textHints;
  st.m_fontHeight          = m_fontHeight;
  st.m_fontAscent          = m_fontAscent;
  st.m_fontDescent         = m_fontDescent;
  st.m_fontCacheType       = m_fontCacheType;

  st.m_lineWidth           = m_lineWidth;
  st.m_evenOddFlag         = m_evenOddFlag;

  st.m_transform           = m_transform;
  st.m_affine              = m_affine;

}

AGG2D_TEMPLATE void Agg2D AGG2D_TEMPLATE_ARG ::restoreStateFrom(const State& st)
{

  m_clipBox             = st.m_clipBox;

  m_blendMode           = st.m_blendMode;
  m_imageBlendMode      = st.m_imageBlendMode;
  m_imageBlendColor     = st.m_imageBlendColor;

  m_masterAlpha         = st.m_masterAlpha;
  m_antiAliasGamma      = st.m_antiAliasGamma;

  m_fillColor           = st.m_fillColor;
  m_lineColor           = st.m_lineColor;
  m_fillGradient        = st.m_fillGradient;
  m_lineGradient        = st.m_lineGradient;

  m_lineCap             = st.m_lineCap;
  m_lineJoin            = st.m_lineJoin;

  m_fillGradientFlag    = st.m_fillGradientFlag;
  m_lineGradientFlag    = st.m_lineGradientFlag;
  m_fillGradientMatrix  = st.m_fillGradientMatrix;
  m_lineGradientMatrix  = st.m_lineGradientMatrix;
  m_fillGradientD1      = st.m_fillGradientD1;
  m_lineGradientD1      = st.m_lineGradientD1;
  m_fillGradientD2      = st.m_fillGradientD2;
  m_lineGradientD2      = st.m_lineGradientD2;

  m_textAngle           = st.m_textAngle;
  m_textAlignX          = st.m_textAlignX;
  m_textAlignY          = st.m_textAlignY;
  m_textHints           = st.m_textHints;
  m_fontHeight          = st.m_fontHeight;
  m_fontAscent          = st.m_fontAscent;
  m_fontDescent         = st.m_fontDescent;
  m_fontCacheType       = st.m_fontCacheType;

  m_lineWidth           = st.m_lineWidth;
  m_evenOddFlag         = st.m_evenOddFlag;

  m_affine              = st.m_affine;
  m_transform           = st.m_transform;

}



//------------------------------------------------------------------------
AGG2D_TEMPLATE void Agg2D AGG2D_TEMPLATE_ARG ::attach(unsigned char* buf, unsigned width, unsigned height, int stride)
{
    m_rbuf.attach(buf, width, height, stride);

    m_renBase.reset_clipping(true);
    m_renBaseComp.reset_clipping(true);
    m_renBasePre.reset_clipping(true);
    m_renBaseCompPre.reset_clipping(true);

    resetTransformations();
    lineWidth(1.0),
    lineColor(0,0,0);
    fillColor(255,255,255);
    textAlignment(AlignLeft, AlignBottom);
    clipBox(0, 0, width, height);
    lineCap(CAP_ROUND);
    lineJoin(JOIN_ROUND);
    flipText(false);
    imageFilter(Bilinear);
    imageResample(NoResample);
    m_masterAlpha = 1.0;
    m_antiAliasGamma = 1.0;
    m_rasterizer.gamma(agg::gamma_none());
    m_blendMode = BlendAlpha;
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE void Agg2D AGG2D_TEMPLATE_ARG ::attach(Image& img)
{
    attach(img.renBuf.buf(), img.renBuf.width(), img.renBuf.height(), img.renBuf.stride());
}

//------------------------------------------------------------------------
AGG2D_TEMPLATE void Agg2D AGG2D_TEMPLATE_ARG ::clipBox(double x1, double y1, double x2, double y2)
{
    m_viewport.transform(&x1, &y1); // 
    m_viewport.transform(&x2, &y2); // see: http://article.gmane.org/gmane.comp.graphics.agg/3543

    m_clipBox = RectD(x1, y1, x2, y2);

    int rx1 = int(x1);
    int ry1 = int(y1);
    int rx2 = int(x2);
    int ry2 = int(y2);

    m_renBase.clip_box(rx1, ry1, rx2, ry2);
    m_renBaseComp.clip_box(rx1, ry1, rx2, ry2);
    m_renBasePre.clip_box(rx1, ry1, rx2, ry2);
    m_renBaseCompPre.clip_box(rx1, ry1, rx2, ry2);

    // m_rasterizer.clip_box(x1, y1, x2, y2);
    m_rasterizer.clip_box(m_renBase.xmin(),   m_renBase.ymin(), 
                          m_renBase.xmax()+1, m_renBase.ymax()+1); // see link above

}

//------------------------------------------------------------------------
AGG2D_TEMPLATE void Agg2D AGG2D_TEMPLATE_ARG ::blendMode(BlendMode m)
{
    m_blendMode = m;
    m_pixFormatComp.comp_op(m);
    m_pixFormatCompPre.comp_op(m);
}

//------------------------------------------------------------------------
AGG2D_TEMPLATE typename Agg2D AGG2D_TEMPLATE_ARG ::BlendMode Agg2D AGG2D_TEMPLATE_ARG ::blendMode() const
{
    return m_blendMode;
}

//------------------------------------------------------------------------
AGG2D_TEMPLATE void Agg2D AGG2D_TEMPLATE_ARG ::imageBlendMode(BlendMode m)
{
    m_imageBlendMode = m;
}

//------------------------------------------------------------------------
AGG2D_TEMPLATE typename Agg2D AGG2D_TEMPLATE_ARG ::BlendMode Agg2D AGG2D_TEMPLATE_ARG ::imageBlendMode() const
{
    return m_imageBlendMode;
}

//------------------------------------------------------------------------
AGG2D_TEMPLATE void Agg2D AGG2D_TEMPLATE_ARG ::imageBlendColor(Color c)
{
    m_imageBlendColor = c;
}

//------------------------------------------------------------------------
AGG2D_TEMPLATE void Agg2D AGG2D_TEMPLATE_ARG ::imageBlendColor(unsigned r, unsigned g, unsigned b, unsigned a)
{
    imageBlendColor(Color(r, g, b, a));
}

//------------------------------------------------------------------------
AGG2D_TEMPLATE typename Agg2D AGG2D_TEMPLATE_ARG ::Color Agg2D AGG2D_TEMPLATE_ARG ::imageBlendColor() const
{
    return m_imageBlendColor;
}

//------------------------------------------------------------------------
AGG2D_TEMPLATE void Agg2D AGG2D_TEMPLATE_ARG ::masterAlpha(double a)
{
    m_masterAlpha = a;
    updateRasterizerGamma();
}

//------------------------------------------------------------------------
AGG2D_TEMPLATE double Agg2D AGG2D_TEMPLATE_ARG ::masterAlpha() const
{
    return m_masterAlpha;
}

//------------------------------------------------------------------------+
AGG2D_TEMPLATE void Agg2D AGG2D_TEMPLATE_ARG ::antiAliasGamma(double g)
{
    m_antiAliasGamma = g;
    updateRasterizerGamma();
}

//------------------------------------------------------------------------
AGG2D_TEMPLATE double Agg2D AGG2D_TEMPLATE_ARG ::antiAliasGamma() const
{
    return m_antiAliasGamma;
}

//------------------------------------------------------------------------
AGG2D_TEMPLATE Agg2DBase::RectD Agg2D AGG2D_TEMPLATE_ARG ::clipBox() const
{
    return m_clipBox;
}

//------------------------------------------------------------------------
AGG2D_TEMPLATE void Agg2D AGG2D_TEMPLATE_ARG ::clearAll(Color c)
{
    m_renBase.clear(c);
}

//------------------------------------------------------------------------
AGG2D_TEMPLATE void Agg2D AGG2D_TEMPLATE_ARG ::clearAll(unsigned r, unsigned g, unsigned b, unsigned a)
{
    clearAll(Color(r, g, b, a));
}

//------------------------------------------------------------------------
AGG2D_TEMPLATE void Agg2D AGG2D_TEMPLATE_ARG ::clearClipBox(Color c)
{
    m_renBase.copy_bar(0, 0, m_renBase.width(), m_renBase.height(), c);
}

//------------------------------------------------------------------------
AGG2D_TEMPLATE void Agg2D AGG2D_TEMPLATE_ARG ::clearClipBox(unsigned r, unsigned g, unsigned b, unsigned a)
{
    clearClipBox(Color(r, g, b, a));
}

//------------------------------------------------------------------------
AGG2D_TEMPLATE void Agg2D AGG2D_TEMPLATE_ARG ::worldToScreen(double& x, double& y) const
{
    m_transform.transform(&x, &y);
}

//------------------------------------------------------------------------
AGG2D_TEMPLATE void Agg2D AGG2D_TEMPLATE_ARG ::screenToWorld(double& x, double& y) const
{
    m_transform.inverse_transform(&x, &y);
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE double Agg2D AGG2D_TEMPLATE_ARG ::worldToScreen(double scalar) const
{
    double x1 = 0;
    double y1 = 0;
    double x2 = scalar;
    double y2 = scalar;
    worldToScreen(x1, y1);
    worldToScreen(x2, y2);
    return sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1)) * 0.7071068;
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE double Agg2D AGG2D_TEMPLATE_ARG ::screenToWorld(double scalar) const
{
    double x1 = 0;
    double y1 = 0;
    double x2 = scalar;
    double y2 = scalar;
    screenToWorld(x1, y1);
    screenToWorld(x2, y2);
    return sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1)) * 0.7071068;
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE void Agg2D AGG2D_TEMPLATE_ARG ::alignPoint(double& x, double& y) const
{
    worldToScreen(x, y);
    x = floor(x) + 0.5;
    y = floor(y) + 0.5;
    screenToWorld(x, y);
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE bool Agg2D AGG2D_TEMPLATE_ARG ::inBox(double worldX, double worldY) const
{
    worldToScreen(worldX, worldY);
    return m_renBase.inbox(int(worldX), int(worldY));
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE Agg2DBase::Transformations Agg2D AGG2D_TEMPLATE_ARG ::transformations() const
{
    Transformations tr;
    m_transform.store_to(tr.affineMatrix);
    return tr;
}
//------------------------------------------------------------------------
AGG2D_TEMPLATE const Agg2DBase::Affine& Agg2D AGG2D_TEMPLATE_ARG ::affine() const
{
    return m_affine;
}
//------------------------------------------------------------------------
AGG2D_TEMPLATE void Agg2D AGG2D_TEMPLATE_ARG ::affine(const Affine& af)
{
    m_affine = af;
    updateTransformations();
}
//------------------------------------------------------------------------
AGG2D_TEMPLATE void Agg2D AGG2D_TEMPLATE_ARG ::transformations(const Transformations& tr)
{
    m_transform.load_from(tr.affineMatrix);
    m_convCurve.approximation_scale(worldToScreen(1.0) * g_approxScale);
    m_convStroke.approximation_scale(worldToScreen(1.0) * g_approxScale);
}
//------------------------------------------------------------------------
AGG2D_TEMPLATE void Agg2D AGG2D_TEMPLATE_ARG ::resetTransformations()
{
    m_transform.reset();
}
//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::updateTransformations()
{
   m_transform  = m_affine;
   m_transform *= m_viewport;
   m_convCurve.approximation_scale(worldToScreen(1.0) * g_approxScale);
   m_convStroke.approximation_scale(worldToScreen(1.0) * g_approxScale);
}
//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::rotate(double angle)
{
    m_affine.premultiply(agg::trans_affine_rotation(angle));
    updateTransformations();
}
//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::rotate(double angle, double cx, double cy)
{
    agg::trans_affine_translation m(-cx, -cy);
    m *= agg::trans_affine_rotation(angle);
    m *= agg::trans_affine_translation(cx, cy);
    m_affine.premultiply(m);
    updateTransformations();
}
//------------------------------------------------------------------------
AGG2D_TEMPLATE void Agg2D AGG2D_TEMPLATE_ARG ::skew(double sx, double sy)
{
    m_affine.premultiply(agg::trans_affine_skewing(sx, sy));
    updateTransformations();
}
//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::translate(double x, double y)
{
    m_affine.premultiply(agg::trans_affine_translation(x, y));
    updateTransformations();
}
//------------------------------------------------------------------------
AGG2D_TEMPLATE void Agg2D AGG2D_TEMPLATE_ARG ::matrix(const Affine& tr)
{
    m_affine.premultiply(tr);
    updateTransformations();
}
//------------------------------------------------------------------------
AGG2D_TEMPLATE void Agg2D AGG2D_TEMPLATE_ARG ::matrix(const Transformations& tr)
{
    matrix(agg::trans_affine(tr.affineMatrix[0], tr.affineMatrix[1], tr.affineMatrix[2],
                             tr.affineMatrix[3], tr.affineMatrix[4], tr.affineMatrix[5]));
}
//------------------------------------------------------------------------
AGG2D_TEMPLATE void Agg2D AGG2D_TEMPLATE_ARG ::scale(double s)
{
    m_affine.premultiply(agg::trans_affine_scaling(s));
    updateTransformations();
}
//------------------------------------------------------------------------
AGG2D_TEMPLATE void Agg2D AGG2D_TEMPLATE_ARG ::scale(double sx, double sy)
{
    m_affine.premultiply(agg::trans_affine_scaling(sx, sy));
    updateTransformations();
}
//------------------------------------------------------------------------
AGG2D_TEMPLATE void Agg2D AGG2D_TEMPLATE_ARG ::parallelogram(double x1, double y1, double x2, double y2, const double* para)
{
    m_affine.premultiply(agg::trans_affine(x1, y1, x2, y2, para));
    updateTransformations();
}
//------------------------------------------------------------------------
AGG2D_TEMPLATE void Agg2D AGG2D_TEMPLATE_ARG ::viewport(double worldX1,  double worldY1,  double worldX2,  double worldY2,
                     double screenX1, double screenY1, double screenX2, double screenY2,
                     ViewportOption opt, WindowFitLogic fl)
{
    agg::trans_viewport vp;

    agg::aspect_ratio_e ar =
        (fl == WindowFitLogic_meet) ? agg::aspect_ratio_meet :
                                      agg::aspect_ratio_slice;
    switch(opt)
    {
        case Anisotropic: vp.preserve_aspect_ratio(0.0, 0.0, agg::aspect_ratio_stretch); break;
        case XMinYMin:    vp.preserve_aspect_ratio(0.0, 0.0, ar);    break;
        case XMidYMin:    vp.preserve_aspect_ratio(0.5, 0.0, ar);    break;
        case XMaxYMin:    vp.preserve_aspect_ratio(1.0, 0.0, ar);    break;
        case XMinYMid:    vp.preserve_aspect_ratio(0.0, 0.5, ar);    break;
        case XMidYMid:    vp.preserve_aspect_ratio(0.5, 0.5, ar);    break;
        case XMaxYMid:    vp.preserve_aspect_ratio(1.0, 0.5, ar);    break;
        case XMinYMax:    vp.preserve_aspect_ratio(0.0, 1.0, ar);    break;
        case XMidYMax:    vp.preserve_aspect_ratio(0.5, 1.0, ar);    break;
        case XMaxYMax:    vp.preserve_aspect_ratio(1.0, 1.0, ar);    break;
    }
    vp.world_viewport(worldX1,   worldY1,  worldX2,  worldY2);
    vp.device_viewport(screenX1, screenY1, screenX2, screenY2);

    m_viewport = vp.to_affine();
    updateTransformations();
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE void Agg2D AGG2D_TEMPLATE_ARG ::fillColor(Color c)
{
    m_fillColor = c;
    m_fillGradientFlag = Solid;
}

//------------------------------------------------------------------------
AGG2D_TEMPLATE void Agg2D AGG2D_TEMPLATE_ARG ::fillColor(unsigned r, unsigned g, unsigned b, unsigned a)
{
    fillColor(Color(r, g, b, a));
}

//------------------------------------------------------------------------
AGG2D_TEMPLATE void Agg2D AGG2D_TEMPLATE_ARG ::noFill()
{
    fillColor(Color(0, 0, 0, 0));
}

//------------------------------------------------------------------------
AGG2D_TEMPLATE void Agg2D AGG2D_TEMPLATE_ARG ::lineColor(Color c)
{
    m_lineColor = c;
    m_lineGradientFlag = Solid;
}

//------------------------------------------------------------------------
AGG2D_TEMPLATE void Agg2D AGG2D_TEMPLATE_ARG ::lineColor(unsigned r, unsigned g, unsigned b, unsigned a)
{
    lineColor(Color(r, g, b, a));
}

//------------------------------------------------------------------------
AGG2D_TEMPLATE void Agg2D AGG2D_TEMPLATE_ARG ::noLine()
{
    lineColor(Color(0, 0, 0, 0));
}

//------------------------------------------------------------------------
AGG2D_TEMPLATE typename TAGG2D::Color TAGG2D::fillColor() const
{
    return m_fillColor;
}

//------------------------------------------------------------------------
AGG2D_TEMPLATE typename TAGG2D::Color TAGG2D::lineColor() const
{
    return m_lineColor;
}

//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::fillLinearGradient(double x1, double y1, double x2, double y2, Color c1, Color c2, double profile)
{
    int i;
    int startGradient = 128 - int(profile * 127.0);
    int endGradient   = 128 + int(profile * 127.0);
    if (endGradient <= startGradient) endGradient = startGradient + 1;
    double k = 1.0 / double(endGradient - startGradient);
    for (i = 0; i < startGradient; i++)
    {
        m_fillGradient[i] = c1;
    }
    for (; i < endGradient; i++)
    {
        m_fillGradient[i] = c1.gradient(c2, double(i - startGradient) * k);
    }
    for (; i < 256; i++)
    {
        m_fillGradient[i] = c2;
    }
    double angle = atan2(y2-y1, x2-x1);
    m_fillGradientMatrix.reset();
    m_fillGradientMatrix *= agg::trans_affine_rotation(angle);
    m_fillGradientMatrix *= agg::trans_affine_translation(x1, y1);
    m_fillGradientMatrix *= m_transform;
    m_fillGradientMatrix.invert();
    m_fillGradientD1 = 0.0;
    m_fillGradientD2 = sqrt((x2-x1) * (x2-x1) + (y2-y1) * (y2-y1));
    m_fillGradientFlag = Linear;
    m_fillColor = Color(0,0,0);  // Set some real color
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::lineLinearGradient(double x1, double y1, double x2, double y2, Color c1, Color c2, double profile)
{
    int i;
    int startGradient = 128 - int(profile * 128.0);
    int endGradient   = 128 + int(profile * 128.0);
    if (endGradient <= startGradient) endGradient = startGradient + 1;
    double k = 1.0 / double(endGradient - startGradient);
    for (i = 0; i < startGradient; i++)
    {
        m_lineGradient[i] = c1;
    }
    for (; i < endGradient; i++)
    {
        m_lineGradient[i] = c1.gradient(c2, double(i - startGradient) * k);
    }
    for (; i < 256; i++)
    {
        m_lineGradient[i] = c2;
    }
    double angle = atan2(y2-y1, x2-x1);
    m_lineGradientMatrix.reset();
    m_lineGradientMatrix *= agg::trans_affine_rotation(angle);
    m_lineGradientMatrix *= agg::trans_affine_translation(x1, y1);
    m_fillGradientMatrix *= m_transform;
    m_lineGradientMatrix.invert();
    m_lineGradientD1 = 0;
    m_lineGradientD2 = sqrt((x2-x1) * (x2-x1) + (y2-y1) * (y2-y1));
    m_lineGradientFlag = Linear;
    m_lineColor = Color(0,0,0);  // Set some real color
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::fillRadialGradient(double x, double y, double r, Color c1, Color c2, double profile)
{
    int i;
    int startGradient = 128 - int(profile * 127.0);
    int endGradient   = 128 + int(profile * 127.0);
    if (endGradient <= startGradient) endGradient = startGradient + 1;
    double k = 1.0 / double(endGradient - startGradient);
    for (i = 0; i < startGradient; i++)
    {
        m_fillGradient[i] = c1;
    }
    for (; i < endGradient; i++)
    {
        m_fillGradient[i] = c1.gradient(c2, double(i - startGradient) * k);
    }
    for (; i < 256; i++)
    {
        m_fillGradient[i] = c2;
    }
    m_fillGradientD2 = worldToScreen(r);
    worldToScreen(x, y);
    m_fillGradientMatrix.reset();
    m_fillGradientMatrix *= agg::trans_affine_translation(x, y);
    m_fillGradientMatrix.invert();
    m_fillGradientD1 = 0;
    m_fillGradientFlag = Radial;
    m_fillColor = Color(0,0,0);  // Set some real color
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::lineRadialGradient(double x, double y, double r, Color c1, Color c2, double profile)
{
    int i;
    int startGradient = 128 - int(profile * 128.0);
    int endGradient   = 128 + int(profile * 128.0);
    if (endGradient <= startGradient) endGradient = startGradient + 1;
    double k = 1.0 / double(endGradient - startGradient);
    for (i = 0; i < startGradient; i++)
    {
        m_lineGradient[i] = c1;
    }
    for (; i < endGradient; i++)
    {
        m_lineGradient[i] = c1.gradient(c2, double(i - startGradient) * k);
    }
    for (; i < 256; i++)
    {
        m_lineGradient[i] = c2;
    }
    m_lineGradientD2 = worldToScreen(r);
    worldToScreen(x, y);
    m_lineGradientMatrix.reset();
    m_lineGradientMatrix *= agg::trans_affine_translation(x, y);
    m_lineGradientMatrix.invert();
    m_lineGradientD1 = 0;
    m_lineGradientFlag = Radial;
    m_lineColor = Color(0,0,0);  // Set some real color
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::fillRadialGradient(double x, double y, double r, Color c1, Color c2, Color c3)
{
    int i;
    for (i = 0; i < 128; i++)
    {
        m_fillGradient[i] = c1.gradient(c2, double(i) / 127.0);
    }
    for (; i < 256; i++)
    {
        m_fillGradient[i] = c2.gradient(c3, double(i - 128) / 127.0);
    }
    m_fillGradientD2 = worldToScreen(r);
    worldToScreen(x, y);
    m_fillGradientMatrix.reset();
    m_fillGradientMatrix *= agg::trans_affine_translation(x, y);
    m_fillGradientMatrix.invert();
    m_fillGradientD1 = 0;
    m_fillGradientFlag = Radial;
    m_fillColor = Color(0,0,0);  // Set some real color
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::lineRadialGradient(double x, double y, double r, Color c1, Color c2, Color c3)
{
    int i;
    for (i = 0; i < 128; i++)
    {
        m_lineGradient[i] = c1.gradient(c2, double(i) / 127.0);
    }
    for (; i < 256; i++)
    {
        m_lineGradient[i] = c2.gradient(c3, double(i - 128) / 127.0);
    }
    m_lineGradientD2 = worldToScreen(r);
    worldToScreen(x, y);
    m_lineGradientMatrix.reset();
    m_lineGradientMatrix *= agg::trans_affine_translation(x, y);
    m_lineGradientMatrix.invert();
    m_lineGradientD1 = 0;
    m_lineGradientFlag = Radial;
    m_lineColor = Color(0,0,0);  // Set some real color
}


AGG2D_TEMPLATE void TAGG2D::fillRadialGradient(double x, double y, double r)
{
    m_fillGradientD2 = worldToScreen(r);
    worldToScreen(x, y);
    m_fillGradientMatrix.reset();
    m_fillGradientMatrix *= agg::trans_affine_translation(x, y);
    m_fillGradientMatrix.invert();
    m_fillGradientD1 = 0;
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::lineRadialGradient(double x, double y, double r)
{
    m_lineGradientD2 = worldToScreen(r);
    worldToScreen(x, y);
    m_lineGradientMatrix.reset();
    m_lineGradientMatrix *= agg::trans_affine_translation(x, y);
    m_lineGradientMatrix.invert();
    m_lineGradientD1 = 0;
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::lineWidth(double w)
{
    m_lineWidth = w;
    m_convStroke.width(w);
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE double TAGG2D::lineWidth(double w) const
{
    return m_lineWidth;
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::fillEvenOdd(bool evenOddFlag)
{
    m_evenOddFlag = evenOddFlag;
    m_rasterizer.filling_rule(evenOddFlag ? agg::fill_even_odd : agg::fill_non_zero);
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE bool TAGG2D::fillEvenOdd() const
{
    return m_evenOddFlag;
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::lineCap(LineCap cap)
{
    m_lineCap = cap;
    m_convStroke.line_cap((agg::line_cap_e)cap);
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE Agg2DBase::LineCap TAGG2D::lineCap() const
{
    return m_lineCap;
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::lineJoin(LineJoin join)
{
    m_lineJoin = join;
    m_convStroke.line_join((agg::line_join_e)join);
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE Agg2DBase::LineJoin TAGG2D::lineJoin() const
{
    return m_lineJoin;
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::addLine(double x1, double y1, double x2, double y2)
{
    m_path.move_to(x1, y1);
    m_path.line_to(x2, y2);
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::line(double x1, double y1, double x2, double y2)
{
    m_path.remove_all();
    addLine(x1, y1, x2, y2);
    drawPath(StrokeOnly);
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::triangle(double x1, double y1, double x2, double y2, double x3, double y3)
{
    m_path.remove_all();
    m_path.move_to(x1, y1);
    m_path.line_to(x2, y2);
    m_path.line_to(x3, y3);
    m_path.close_polygon();
    drawPath(FillAndStroke);
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::rectangle(double x1, double y1, double x2, double y2)
{
    m_path.remove_all();
    m_path.move_to(x1, y1);
    m_path.line_to(x2, y1);
    m_path.line_to(x2, y2);
    m_path.line_to(x1, y2);
    m_path.close_polygon();
    drawPath(FillAndStroke);
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::roundedRect(double x1, double y1, double x2, double y2, double r)
{
    m_path.remove_all();
    agg::rounded_rect rc(x1, y1, x2, y2, r);
    rc.normalize_radius();
    rc.approximation_scale(worldToScreen(1.0) * g_approxScale);
    // JME audit
    //m_path.add_path(rc, 0, false);
    m_path.concat_path(rc,0);
    drawPath(FillAndStroke);
}



//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::roundedRect(double x1, double y1, double x2, double y2, double rx, double ry)
{
    m_path.remove_all();
    agg::rounded_rect rc;
    rc.rect(x1, y1, x2, y2);
    rc.radius(rx, ry);
    rc.normalize_radius();
    //m_path.add_path(rc, 0, false);
    m_path.concat_path(rc,0); // JME
    drawPath(FillAndStroke);
}



//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::roundedRect(double x1, double y1, double x2, double y2,
                        double rx_bottom, double ry_bottom,
                        double rx_top,    double ry_top)
{
    m_path.remove_all();
    agg::rounded_rect rc;
    rc.rect(x1, y1, x2, y2);
    rc.radius(rx_bottom, ry_bottom, rx_top, ry_top);
    rc.normalize_radius();
    rc.approximation_scale(worldToScreen(1.0) * g_approxScale);
    //m_path.add_path(rc, 0, false);
    m_path.concat_path(rc,0); // JME
    drawPath(FillAndStroke);
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::ellipse(double cx, double cy, double rx, double ry)
{
    m_path.remove_all();
    agg::bezier_arc arc(cx, cy, rx, ry, 0, 2*pi());
    //m_path.add_path(arc, 0, false);
    m_path.concat_path(arc,0); // JME
    m_path.close_polygon();
    drawPath(FillAndStroke);
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::arc(double cx, double cy, double rx, double ry, double start, double sweep)
{
    m_path.remove_all();
    agg::bezier_arc arc(cx, cy, rx, ry, start, sweep);
    //m_path.add_path(arc, 0, false);
    m_path.concat_path(arc,0); // JME
    drawPath(StrokeOnly);
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::star(double cx, double cy, double r1, double r2, double startAngle, int numRays)
{
    m_path.remove_all();
    double da = agg::pi / double(numRays);
    double a = startAngle;
    int i;
    for (i = 0; i < numRays; i++)
    {
        double x = cos(a) * r2 + cx;
        double y = sin(a) * r2 + cy;
        if (i) m_path.line_to(x, y);
        else   m_path.move_to(x, y);
        a += da;
        m_path.line_to(cos(a) * r1 + cx, sin(a) * r1 + cy);
        a += da;
    }
    closePolygon();
    drawPath(FillAndStroke);
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::curve(double x1, double y1, double x2, double y2, double x3, double y3)
{
    m_path.remove_all();
    m_path.move_to(x1, y1);
    m_path.curve3(x2, y2, x3, y3);
    drawPath(StrokeOnly);
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::curve(double x1, double y1, double x2, double y2, double x3, double y3, double x4, double y4)
{
    m_path.remove_all();
    m_path.move_to(x1, y1);
    m_path.curve4(x2, y2, x3, y3, x4, y4);
    drawPath(StrokeOnly);
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::polygon(double* xy, int numPoints)
{
    m_path.remove_all();
    //m_path.add_poly(xy, numPoints);
    m_path.concat_poly(xy,numPoints,true); // JME, AF
    closePolygon();
    drawPath(FillAndStroke);
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::polyline(double* xy, int numPoints)
{
    m_path.remove_all();
    //m_path.add_poly(xy, numPoints);
	  m_path.concat_poly(xy,numPoints,true); // JME, AF
    drawPath(StrokeOnly);
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::flipText(bool flip)
{
    m_fontEngine.flip_y(flip);
}

//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::font(const char* fontName,
                 double height,
                 bool bold,
                 bool italic,
                 FontCacheType ch,
                 double angle)
{
    m_textAngle = angle;
    m_fontHeight = height;
    m_fontCacheType = ch;


#ifdef AGG2D_USE_FREETYPE
    m_fontEngine.load_font(fontName,
                           0,
                           (ch == VectorFontCache) ?
                                agg::glyph_ren_outline :
                                agg::glyph_ren_agg_gray8);
    m_fontEngine.hinting(m_textHints);
    m_fontEngine.height((ch == VectorFontCache) ? height : worldToScreen(height));
#else
    m_fontEngine.hinting(m_textHints);

    m_fontEngine.create_font(fontName,
                             (ch == VectorFontCache) ?
                                agg::glyph_ren_outline :
                                agg::glyph_ren_agg_gray8,
                              (ch == VectorFontCache) ? height : worldToScreen(height),
                              0.0,
                              bold ? 700 : 400,
                              italic);
#endif

}


//------------------------------------------------------------------------
AGG2D_TEMPLATE double TAGG2D::fontHeight() const
{
   return m_fontHeight;
}

//------------------------------------------------------------------------
AGG2D_TEMPLATE double TAGG2D::fontAscent() const
{
   return m_fontAscent;
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::textAlignment(TextAlignment alignX, TextAlignment alignY)
{
   m_textAlignX = alignX;
   m_textAlignY = alignY;
}

//------------------------------------------------------------------------
AGG2D_TEMPLATE double TAGG2D::textWidth(const char* str, unsigned int len)
{
#if defined( UNDER_CE )
  return 0;
#else
    double x = 0;
    double y = 0;
    bool first = true;
    while(*str && len)
    {
        const agg::glyph_cache* glyph = m_fontCacheManager.glyph(*str);
        if(glyph)
        {
            if(!first) m_fontCacheManager.add_kerning(&x, &y);
            x += glyph->advance_x;
            y += glyph->advance_y;
            first = true;
        }
        ++str; --len;
    }
    return (m_fontCacheType == VectorFontCache) ? x : screenToWorld(x);
#endif
}

//------------------------------------------------------------------------
AGG2D_TEMPLATE double TAGG2D::textWidth(const wchar_t* str, unsigned int len)
{
#if defined( UNDER_CE )
  return 0;
#else
    double x = 0;
    double y = 0;
    bool first = true;
    while(*str && len)
    {
        const agg::glyph_cache* glyph = m_fontCacheManager.glyph(*str);
        if(glyph)
        {
            if(!first) m_fontCacheManager.add_kerning(&x, &y);
            x += glyph->advance_x;
            y += glyph->advance_y;
            first = true;
        }
        ++str; --len;
    }
    return (m_fontCacheType == VectorFontCache) ? x : screenToWorld(x);
#endif
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE bool TAGG2D::textHints() const
{
   return m_textHints;
}

//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::textHints(bool hints)
{
   m_textHints = hints;
}



//------------------------------------------------------------------------

AGG2D_TEMPLATE void TAGG2D::text(double x, double y, const char* str, unsigned int len, bool roundOff, double ddx, double ddy)
{

   double dx = 0.0;
   double dy = 0.0;

   switch(m_textAlignX)
   {
       case AlignCenter:  dx = -textWidth(str,len) * 0.5; break;
       case AlignRight:   dx = -textWidth(str,len);       break;
       default: break;
   }


   double asc = fontHeight();
   const agg::glyph_cache* glyph = m_fontCacheManager.glyph('H');
   if(glyph)
   {
       asc = glyph->bounds.y2 - glyph->bounds.y1;
   }

   if(m_fontCacheType == RasterFontCache)
   {
       asc = screenToWorld(asc);
   }

   switch(m_textAlignY)
   {
       case AlignCenter:    dy = -asc * 0.5; break;
       case AlignTop:       dy = -asc;       break;
       case AlignBaseline:  dy = -fontAscent();   break;
       default: break;
   }

   if(m_fontEngine.flip_y()) dy = -dy;

   agg::trans_affine  mtx;

    double start_x = x + dx;
    double start_y = y + dy;

    if (roundOff)
    {
        start_x = int(start_x);
        start_y = int(start_y);
    }
    start_x += ddx;
    start_y += ddy;

    mtx *= agg::trans_affine_translation(-x, -y);
    mtx *= agg::trans_affine_rotation(m_textAngle);
    mtx *= agg::trans_affine_translation(x, y);

    agg::conv_transform<FontCacheManager::path_adaptor_type> tr(m_fontCacheManager.path_adaptor(), mtx);

    if(m_fontCacheType == RasterFontCache)
    {
        worldToScreen(start_x, start_y);
    }

    unsigned int i;
    for (i = 0; i < len && str[i]; i++)
    {
        glyph = m_fontCacheManager.glyph(str[i]);
        if(glyph)
        {
            if(i) m_fontCacheManager.add_kerning(&x, &y);
            m_fontCacheManager.init_embedded_adaptors(glyph, start_x, start_y);

            if(glyph->data_type == agg::glyph_data_outline)
            {
                m_path.remove_all();
                m_path.concat_path(tr, 0);
                drawPath();
            }

            if(glyph->data_type == agg::glyph_data_gray8)
            {
                render(m_fontCacheManager.gray8_adaptor(),
                       m_fontCacheManager.gray8_scanline());
            }
            start_x += glyph->advance_x;
            start_y += glyph->advance_y;
        }
    }

}

AGG2D_TEMPLATE void TAGG2D::text(double x, double y, const wchar_t* str, unsigned int len, bool roundOff, double ddx, double ddy)
{

   double dx = 0.0;
   double dy = 0.0;

   switch(m_textAlignX)
   {
       case AlignCenter:  dx = -textWidth(str,len) * 0.5; break;
       case AlignRight:   dx = -textWidth(str,len);       break;
       default: break;
   }

   double asc = fontHeight();
   const agg::glyph_cache* glyph = m_fontCacheManager.glyph('H');
   if(glyph)
   {
       asc = glyph->bounds.y2 - glyph->bounds.y1;
   }

   if(m_fontCacheType == RasterFontCache)
   {
       asc = screenToWorld(asc);
   }

   switch(m_textAlignY)
   {
       case AlignCenter:    dy = -asc * 0.5; break;
       case AlignTop:       dy = -asc;       break;
       case AlignBaseline:  dy = -fontAscent();   break;
       default: break;
   }

   if(m_fontEngine.flip_y()) dy = -dy;

   agg::trans_affine  mtx;

    double start_x = x + dx;
    double start_y = y + dy;

    if (roundOff)
    {
        start_x = int(start_x);
        start_y = int(start_y);
    }
    start_x += ddx;
    start_y += ddy;

    mtx *= agg::trans_affine_translation(-x, -y);
    mtx *= agg::trans_affine_rotation(m_textAngle);
    mtx *= agg::trans_affine_translation(x, y);

    agg::conv_transform<FontCacheManager::path_adaptor_type> tr(m_fontCacheManager.path_adaptor(), mtx);

    if(m_fontCacheType == RasterFontCache)
    {
        worldToScreen(start_x, start_y);
    }

    unsigned int i;
    for (i = 0; i < len && str[i]; i++)
    {
        glyph = m_fontCacheManager.glyph(str[i]);
        if(glyph)
        {
            if(i) m_fontCacheManager.add_kerning(&x, &y);
            m_fontCacheManager.init_embedded_adaptors(glyph, start_x, start_y);

            if(glyph->data_type == agg::glyph_data_outline)
            {
                m_path.remove_all();
                //m_path.add_path(tr, 0, false);
				        m_path.concat_path(tr,0); // JME
                drawPath();
            }

            if(glyph->data_type == agg::glyph_data_gray8)
            {
                render(m_fontCacheManager.gray8_adaptor(),
                       m_fontCacheManager.gray8_scanline());
            }
            start_x += glyph->advance_x;
            start_y += glyph->advance_y;
        }
    }

}


//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::resetPath() { m_path.remove_all(); }

//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::moveTo(double x, double y)
{
    m_start_x = x;
    m_start_y = y;

    m_path.move_to(x, y);
}

//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::moveRel(double dx, double dy)
{
    if(m_path.vertices().total_vertices())
    {
        double x2;
        double y2;
        m_path.vertices().last_vertex(&x2, &y2);

        dx += x2;
        dy += y2;
    }

    moveTo(dx, dy);
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::lineTo(double x, double y)
{
    m_path.line_to(x, y);
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::lineRel(double dx, double dy)
{
    m_path.line_rel(dx, dy);
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::horLineTo(double x)
{
    m_path.hline_to(x);
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::horLineRel(double dx)
{
    m_path.hline_rel(dx);
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::verLineTo(double y)
{
    m_path.vline_to(y);
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::verLineRel(double dy)
{
    m_path.vline_rel(dy);
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::arcTo(double rx, double ry,
                  double angle,
                  bool largeArcFlag,
                  bool sweepFlag,
                  double x, double y)
{
    m_path.arc_to(rx, ry, angle, largeArcFlag, sweepFlag, x, y);
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::arcRel(double rx, double ry,
                   double angle,
                   bool largeArcFlag,
                   bool sweepFlag,
                   double dx, double dy)
{
    m_path.arc_rel(rx, ry, angle, largeArcFlag, sweepFlag, dx, dy);
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::quadricCurveTo(double xCtrl, double yCtrl,
                           double xTo,   double yTo)
{
    m_path.curve3(xCtrl, yCtrl, xTo, yTo);
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::quadricCurveRel(double dxCtrl, double dyCtrl,
                            double dxTo,   double dyTo)
{
    m_path.curve3_rel(dxCtrl, dyCtrl, dxTo, dyTo);
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::quadricCurveTo(double xTo, double yTo)
{
    m_path.curve3(xTo, yTo);
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::quadricCurveRel(double dxTo, double dyTo)
{
    m_path.curve3_rel(dxTo, dyTo);
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::cubicCurveTo(double xCtrl1, double yCtrl1,
                         double xCtrl2, double yCtrl2,
                         double xTo,    double yTo)
{
    m_path.curve4(xCtrl1, yCtrl1, xCtrl2, yCtrl2, xTo, yTo);
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::cubicCurveRel(double dxCtrl1, double dyCtrl1,
                          double dxCtrl2, double dyCtrl2,
                          double dxTo,    double dyTo)
{
    m_path.curve4_rel(dxCtrl1, dyCtrl1, dxCtrl2, dyCtrl2, dxTo, dyTo);
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::cubicCurveTo(double xCtrl2, double yCtrl2,
                         double xTo,    double yTo)
{
    m_path.curve4(xCtrl2, yCtrl2, xTo, yTo);
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::cubicCurveRel(double xCtrl2, double yCtrl2,
                          double xTo,    double yTo)
{
    m_path.curve4_rel(xCtrl2, yCtrl2, xTo, yTo);
}

//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::addEllipse(double cx, double cy, double rx, double ry, Direction dir)
{
    agg::bezier_arc arc(cx, cy, rx, ry, 0, (dir == CCW) ? 2*pi() : -2*pi());
    //m_path.add_path(arc, 0, false);
	  m_path.concat_path(arc,0); // JME
    m_path.close_polygon();
}

//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::closePolygon()
{
    if(agg::is_vertex(m_path.vertices().last_command()))
    {
        m_path.vertices().add_vertex(m_start_x, m_start_y,
            agg::path_cmd_end_poly | agg::path_flags_close);
    }
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::imageFilter(ImageFilter f)
{
    m_imageFilter = f;
    switch(f)
    {
        case NoFilter:    break;
        case Bilinear:    m_imageFilterLut.calculate(agg::image_filter_bilinear(),    true); break;
        case Hanning:     m_imageFilterLut.calculate(agg::image_filter_hanning(),     true); break;
        case Hermite:     m_imageFilterLut.calculate(agg::image_filter_hermite(),     true); break;
        case Quadric:     m_imageFilterLut.calculate(agg::image_filter_quadric(),     true); break;
        case Bicubic:     m_imageFilterLut.calculate(agg::image_filter_bicubic(),     true); break;
        case Catrom:      m_imageFilterLut.calculate(agg::image_filter_catrom(),      true); break;
        case Spline16:    m_imageFilterLut.calculate(agg::image_filter_spline16(),    true); break;
        case Spline36:    m_imageFilterLut.calculate(agg::image_filter_spline36(),    true); break;
        case Blackman144: m_imageFilterLut.calculate(agg::image_filter_blackman144(), true); break;
    }
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE Agg2DBase::ImageFilter TAGG2D::imageFilter() const
{
    return m_imageFilter;
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::imageResample(ImageResample f)
{
    m_imageResample = f;
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE Agg2DBase::ImageResample TAGG2D::imageResample() const
{
    return m_imageResample;
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::transformImage(const Image& img,    int imgX1,    int imgY1,    int imgX2,    int imgY2,
                                             double dstX1, double dstY1, double dstX2, double dstY2)
{
    resetPath();
    moveTo(dstX1, dstY1);
    lineTo(dstX2, dstY1);
    lineTo(dstX2, dstY2);
    lineTo(dstX1, dstY2);
    closePolygon();
    double parallelogram[6] = { dstX1, dstY1, dstX2, dstY1, dstX2, dstY2 };
    renderImage(img, imgX1, imgY1, imgX2, imgY2, parallelogram);
}

//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::transformImage(const Image& img, double dstX1, double dstY1, double dstX2, double dstY2)
{
    resetPath();
    moveTo(dstX1, dstY1);
    lineTo(dstX2, dstY1);
    lineTo(dstX2, dstY2);
    lineTo(dstX1, dstY2);
    closePolygon();
    double parallelogram[6] = { dstX1, dstY1, dstX2, dstY1, dstX2, dstY2 };
    renderImage(img, 0, 0, img.renBuf.width(), img.renBuf.height(), parallelogram);
}

//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::transformImage(const Image& img, int imgX1, int imgY1, int imgX2, int imgY2,
                           const double* parallelogram)
{
    resetPath();
    moveTo(parallelogram[0], parallelogram[1]);
    lineTo(parallelogram[2], parallelogram[3]);
    lineTo(parallelogram[4], parallelogram[5]);
    lineTo(parallelogram[0] + parallelogram[4] - parallelogram[2],
           parallelogram[1] + parallelogram[5] - parallelogram[3]);
    closePolygon();
    renderImage(img, imgX1, imgY1, imgX2, imgY2, parallelogram);
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::transformImage(const Image& img, const double* parallelogram)
{
    resetPath();
    moveTo(parallelogram[0], parallelogram[1]);
    lineTo(parallelogram[2], parallelogram[3]);
    lineTo(parallelogram[4], parallelogram[5]);
    lineTo(parallelogram[0] + parallelogram[4] - parallelogram[2],
           parallelogram[1] + parallelogram[5] - parallelogram[3]);
    closePolygon();
    renderImage(img, 0, 0, img.renBuf.width(), img.renBuf.height(), parallelogram);
}

//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::transformImagePath(const Image& img,    int imgX1,    int imgY1,    int imgX2,    int imgY2,
                                                 double dstX1, double dstY1, double dstX2, double dstY2)
{
    double parallelogram[6] = { dstX1, dstY1, dstX2, dstY1, dstX2, dstY2 };
    renderImage(img, imgX1, imgY1, imgX2, imgY2, parallelogram);
}

//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::transformImagePath(const Image& img, double dstX1, double dstY1, double dstX2, double dstY2)
{
    double parallelogram[6] = { dstX1, dstY1, dstX2, dstY1, dstX2, dstY2 };
    renderImage(img, 0, 0, img.renBuf.width(), img.renBuf.height(), parallelogram);
}

//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::transformImagePath(const Image& img, int imgX1, int imgY1, int imgX2, int imgY2,
                               const double* parallelogram)
{
    renderImage(img, imgX1, imgY1, imgX2, imgY2, parallelogram);
}

//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::transformImagePath(const Image& img, const double* parallelogram)
{
    renderImage(img, 0, 0, img.renBuf.width(), img.renBuf.height(), parallelogram);
}



//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::drawPath(DrawPathFlag flag)
{
    m_rasterizer.reset();
    switch(flag)
    {
    case FillOnly:
        if (m_fillColor.a)
        {
            m_rasterizer.add_path(m_pathTransform);
            render(true);
        }
        break;

    case StrokeOnly:
        if (m_lineColor.a && m_lineWidth > 0.0)
        {
            m_rasterizer.add_path(m_strokeTransform);
            render(false);
        }
        break;

    case FillAndStroke:
        if (m_fillColor.a)
        {
            m_rasterizer.add_path(m_pathTransform);
            render(true);
        }

        if (m_lineColor.a && m_lineWidth > 0.0)
        {
            m_rasterizer.add_path(m_strokeTransform);
            render(false);
        }
        break;

    case FillWithLineColor:
        if (m_lineColor.a)
        {
            m_rasterizer.add_path(m_pathTransform);
            render(false);
        }
        break;
    }
}



//------------------------------------------------------------------------
AGG2D_TEMPLATE class Agg2DRenderer
{
public:
	typedef typename TAGG2D::Color Color;
	typedef typename TAGG2D::Image Image;
    //--------------------------------------------------------------------
    template<class BaseRenderer, class SolidRenderer>
    void static render(TAGG2D& gr, BaseRenderer& renBase, SolidRenderer& renSolid, bool fillColor)
    {
		// JME
		typedef agg::span_allocator<agg::rgba8> span_allocator_type;
        //- typedef agg::renderer_scanline_aa<BaseRenderer, TAGG2D::LinearGradientSpan> RendererLinearGradient;
        typedef agg::renderer_scanline_aa<BaseRenderer,
										span_allocator_type,
										TAGG2D::LinearGradientSpan> RendererLinearGradient;
        //- typedef agg::renderer_scanline_aa<BaseRenderer, TAGG2D::RadialGradientSpan> RendererRadialGradient;
		typedef agg::renderer_scanline_aa<BaseRenderer,
										span_allocator_type,
										TAGG2D::RadialGradientSpan> RendererRadialGradient;

        if ((fillColor && gr.m_fillGradientFlag == TAGG2D::Linear) ||
           (!fillColor && gr.m_lineGradientFlag == TAGG2D::Linear))
        {
            if (fillColor)
            {
                TAGG2D::LinearGradientSpan span(/*gr.m_allocator, */
                                               gr.m_fillGradientInterpolator,
                                               gr.m_linearGradientFunction,
                                               gr.m_fillGradient,
                                               gr.m_fillGradientD1,
                                               gr.m_fillGradientD2);
				//-RendererLinearGradient ren(renBase,span);
                RendererLinearGradient ren(renBase,gr.m_allocator,span);
                agg::render_scanlines(gr.m_rasterizer, gr.m_scanline, ren);
            }
            else
            {
                TAGG2D::LinearGradientSpan span(/*gr.m_allocator,*/
                                               gr.m_lineGradientInterpolator,
                                               gr.m_linearGradientFunction,
                                               gr.m_lineGradient,
                                               gr.m_lineGradientD1,
                                               gr.m_lineGradientD2);
                //- RendererLinearGradient ren(renBase, span);
                RendererLinearGradient ren(renBase,gr.m_allocator,span);
                agg::render_scanlines(gr.m_rasterizer, gr.m_scanline, ren);
            }
        }
        else
        {
            if ((fillColor && gr.m_fillGradientFlag == TAGG2D::Radial) ||
               (!fillColor && gr.m_lineGradientFlag == TAGG2D::Radial))
            {
                if (fillColor)
                {
                    TAGG2D::RadialGradientSpan span(/*gr.m_allocator, */
                                                   gr.m_fillGradientInterpolator,
                                                   gr.m_radialGradientFunction,
                                                   gr.m_fillGradient,
                                                   gr.m_fillGradientD1,
                                                   gr.m_fillGradientD2);
                    //-RendererRadialGradient ren(renBase, span);
                    RendererRadialGradient ren(renBase,gr.m_allocator,span);
                    agg::render_scanlines(gr.m_rasterizer, gr.m_scanline, ren);
                }
                else
                {
                    TAGG2D::RadialGradientSpan span(/*gr.m_allocator,*/
                                                   gr.m_lineGradientInterpolator,
                                                   gr.m_radialGradientFunction,
                                                   gr.m_lineGradient,
                                                   gr.m_lineGradientD1,
                                                   gr.m_lineGradientD2);
                    //-RendererRadialGradient ren(renBase, span);
                    RendererRadialGradient ren(renBase,gr.m_allocator,span);
                    agg::render_scanlines(gr.m_rasterizer, gr.m_scanline, ren);
                }
            }
            else
            {
                renSolid.color(fillColor ? gr.m_fillColor : gr.m_lineColor);
                agg::render_scanlines(gr.m_rasterizer, gr.m_scanline, renSolid);
            }
        }
    }


    //--------------------------------------------------------------------
    class SpanConvImageBlend
    {
    public:
        SpanConvImageBlend(Agg2D::BlendMode m, Color c) :
            m_mode(m), m_color(c)
        {}

        void convert(Color* span, int x, int y, unsigned len) const
        {
            unsigned l2;
            TAGG2D::Color* s2;
            if(m_mode != TAGG2D::BlendDst)
            {
                l2 = len;
                s2 = span;
                typedef agg::comp_op_adaptor_clip_to_dst_rgba_pre<TAGG2D::Color, agg::order_rgba> OpType;
                do
                {
                    OpType::blend_pix(m_mode,
                                      (TAGG2D::Color::value_type*)s2,
                                      m_color.r,
                                      m_color.g,
                                      m_color.b,
                                      TAGG2D::Color::base_mask,
                                      agg::cover_full);
                    ++s2;
                }
                while(--l2);
            }
            if(m_color.a < TAGG2D::Color::base_mask)
            {
                l2 = len;
                s2 = span;
                unsigned a = m_color.a;
                do
                {
                    s2->r = (s2->r * a) >> TAGG2D::Color::base_shift;
                    s2->g = (s2->g * a) >> TAGG2D::Color::base_shift;
                    s2->b = (s2->b * a) >> TAGG2D::Color::base_shift;
                    s2->a = (s2->a * a) >> TAGG2D::Color::base_shift;
                    ++s2;
                }
                while(--l2);
            }
        }

    private:
		Agg2DBase::BlendMode m_mode;
        Color     m_color;
    };




    //--------------------------------------------------------------------
    template<class BaseRenderer, class SolidRenderer, class Rasterizer, class Scanline>
    void static render(TAGG2D& gr, BaseRenderer& renBase, SolidRenderer& renSolid, Rasterizer& ras, Scanline& sl)
    {
		// JME
		typedef agg::span_allocator<agg::rgba8> span_allocator_type;
        typedef agg::renderer_scanline_aa<BaseRenderer,span_allocator_type,TAGG2D::LinearGradientSpan> RendererLinearGradient;
        typedef agg::renderer_scanline_aa<BaseRenderer,span_allocator_type,TAGG2D::RadialGradientSpan> RendererRadialGradient;

        if(gr.m_fillGradientFlag == TAGG2D::Linear)
        {
            TAGG2D::LinearGradientSpan span(
                                           gr.m_fillGradientInterpolator,
                                           gr.m_linearGradientFunction,
                                           gr.m_fillGradient,
                                           gr.m_fillGradientD1,
                                           gr.m_fillGradientD2);
            RendererLinearGradient ren(renBase,gr.m_allocator,span);
            agg::render_scanlines(ras, sl, ren);
        }
        else
        {
            if(gr.m_fillGradientFlag == TAGG2D::Radial)
            {
                TAGG2D::RadialGradientSpan span(
                                               gr.m_fillGradientInterpolator,
                                               gr.m_radialGradientFunction,
                                               gr.m_fillGradient,
                                               gr.m_fillGradientD1,
                                               gr.m_fillGradientD2);
                RendererRadialGradient ren(renBase,gr.m_allocator,span);
                agg::render_scanlines(ras, sl, ren);
            }
            else
            {
                renSolid.color(gr.m_fillColor);
                agg::render_scanlines(ras, sl, renSolid);
            }
        }
    }



    //--------------------------------------------------------------------
    //! JME - this is where the bulk of the changes have taken place.
    template<class BaseRenderer, class Interpolator>
    static void renderImage(TAGG2D& gr, const Image& img,
                            BaseRenderer& renBase, Interpolator& interpolator)
    {
		//! JME - have not quite figured which part of this is not const-correct
		// hence the cast.
		Image& imgc = const_cast<Image&>(img);
		TAGG2D::PixFormat img_pixf(imgc.renBuf);
		typedef agg::image_accessor_clone<TAGG2D::PixFormat> img_source_type;
		img_source_type source(img_pixf);

        SpanConvImageBlend blend(gr.m_imageBlendMode, gr.m_imageBlendColor);
        if(gr.m_imageFilter == TAGG2D::NoFilter)
        {

			typedef agg::span_image_filter_rgba_nn<img_source_type,Interpolator> SpanGenType;
			typedef agg::span_converter<SpanGenType,SpanConvImageBlend> SpanConvType;
			typedef agg::renderer_scanline_aa<BaseRenderer,TAGG2D::SpanAllocator,SpanGenType> RendererType;

			SpanGenType sg(source,interpolator);
            SpanConvType sc(sg, blend);
			RendererType ri(renBase,gr.m_allocator,sg);
            agg::render_scanlines(gr.m_rasterizer, gr.m_scanline, ri);
        }
        else
        {
            bool resample = (gr.m_imageResample == TAGG2D::ResampleAlways);
            if(gr.m_imageResample == TAGG2D::ResampleOnZoomOut)
            {
                double sx, sy;
                interpolator.transformer().scaling_abs(&sx, &sy);
                if (sx > 1.125 || sy > 1.125)
                {
					resample = true;
                }
            }

            if(resample)
            {
                typedef agg::span_image_resample_rgba_affine<img_source_type> SpanGenType;
                typedef agg::span_converter<SpanGenType,SpanConvImageBlend> SpanConvType;
                typedef agg::renderer_scanline_aa<BaseRenderer,TAGG2D::SpanAllocator,SpanGenType> RendererType;

                SpanGenType sg(source,interpolator,gr.m_imageFilterLut);
                SpanConvType sc(sg, blend);
                RendererType ri(renBase,gr.m_allocator,sg);
                agg::render_scanlines(gr.m_rasterizer, gr.m_scanline, ri);
            }
            else
            {
				// this is the AGG2D default
                if(gr.m_imageFilter == TAGG2D::Bilinear)
                {
                    typedef agg::span_image_filter_rgba_bilinear<img_source_type,Interpolator> SpanGenType;
                    typedef agg::span_converter<SpanGenType,SpanConvImageBlend> SpanConvType;
					typedef agg::renderer_scanline_aa<BaseRenderer,TAGG2D::SpanAllocator,SpanGenType> RendererType;

					SpanGenType sg(source,interpolator);
                    SpanConvType sc(sg, blend);
					RendererType ri(renBase,gr.m_allocator,sg);
                    agg::render_scanlines(gr.m_rasterizer, gr.m_scanline, ri);
                }
                else
                {
                    if(gr.m_imageFilterLut.diameter() == 2)
                    {
                        typedef agg::span_image_filter_rgba_2x2<img_source_type,Interpolator> SpanGenType;
                        typedef agg::span_converter<SpanGenType,SpanConvImageBlend> SpanConvType;
                        typedef agg::renderer_scanline_aa<BaseRenderer,TAGG2D::SpanAllocator,SpanGenType> RendererType;

                        SpanGenType sg(source,interpolator,gr.m_imageFilterLut);
                        SpanConvType sc(sg, blend);
                        RendererType ri(renBase,gr.m_allocator,sg);
                        agg::render_scanlines(gr.m_rasterizer, gr.m_scanline, ri);
                    }
                    else
                    {
                        typedef agg::span_image_filter_rgba<img_source_type,Interpolator> SpanGenType;
                        typedef agg::span_converter<SpanGenType,SpanConvImageBlend> SpanConvType;
						typedef agg::renderer_scanline_aa<BaseRenderer,TAGG2D::SpanAllocator,SpanGenType> RendererType;
                        SpanGenType sg(source,interpolator,gr.m_imageFilterLut);
                        SpanConvType sc(sg, blend);
						RendererType ri(renBase,gr.m_allocator,sg);
                        agg::render_scanlines(gr.m_rasterizer, gr.m_scanline, ri);
                    }
                }
            }
        }
    }
};


//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::render(bool fillColor)
{
    if(m_blendMode == BlendAlpha)
    {
        TAGG2DRENDERER::render(*this, m_renBase, m_renSolid, fillColor);
    }
    else
    {
        TAGG2DRENDERER::render(*this, m_renBaseComp, m_renSolidComp, fillColor);
    }
}
#if !defined( UNDER_CE )

//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::render(FontRasterizer& ras, FontScanline& sl)
{
    if(m_blendMode == BlendAlpha)
    {
        Agg2DRenderer::render(*this, m_renBase, m_renSolid, ras, sl);
    }
    else
    {
        Agg2DRenderer::render(*this, m_renBaseComp, m_renSolidComp, ras, sl);
    }
}

#endif


//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::renderImage(const Image& img, int x1, int y1, int x2, int y2,
                        const double* parl)
{
    agg::trans_affine mtx((double)x1,
                          (double)y1,
                          (double)x2,
                          (double)y2,
                          parl);
    mtx *= m_transform;
    mtx.invert();

    m_rasterizer.reset();
    m_rasterizer.add_path(m_pathTransform);

    typedef agg::span_interpolator_linear<agg::trans_affine> Interpolator;
    Interpolator interpolator(mtx);

    if(m_blendMode == BlendAlpha)
    {
		// JME audit -
        Agg2DRenderer::renderImage(*this, img, m_renBasePre, interpolator);
    }
    else
    {
        Agg2DRenderer::renderImage(*this, img, m_renBaseCompPre, interpolator);
    }
}

//------------------------------------------------------------------------
AGG2D_TEMPLATE struct Agg2DRasterizerGamma
{

    Agg2DRasterizerGamma(double alpha, double gamma) :
        m_alpha(alpha), m_gamma(gamma) {}

    double operator() (double x) const
    {
        return m_alpha(m_gamma(x));
    }
    agg::gamma_multiply m_alpha;
    agg::gamma_power    m_gamma;
};

//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::updateRasterizerGamma()
{
    m_rasterizer.gamma(Agg2DRasterizerGamma(m_masterAlpha, m_antiAliasGamma));
}

//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::blendImage(Image& img,
                       int imgX1, int imgY1, int imgX2, int imgY2,
                       double dstX, double dstY, unsigned alpha)
{
    worldToScreen(dstX, dstY);
    PixFormat pixF(img.renBuf);
    // JME
    //agg::rect r(imgX1, imgY1, imgX2, imgY2);
    Rect r(imgX1, imgY1, imgX2, imgY2);
    if(m_blendMode == BlendAlpha)
    {
        m_renBasePre.blend_from(pixF, &r, int(dstX)-imgX1, int(dstY)-imgY1, alpha);
    }
    else
    {
        m_renBaseCompPre.blend_from(pixF, &r, int(dstX)-imgX1, int(dstY)-imgY1, alpha);
    }
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::blendImage(Image& img, double dstX, double dstY, unsigned alpha)
{
    worldToScreen(dstX, dstY);
    PixFormat pixF(img.renBuf);
    m_renBasePre.blend_from(pixF, 0, int(dstX), int(dstY), alpha);
    if(m_blendMode == BlendAlpha)
    {
        m_renBasePre.blend_from(pixF, 0, int(dstX), int(dstY), alpha);
    }
    else
    {
        m_renBaseCompPre.blend_from(pixF, 0, int(dstX), int(dstY), alpha);
    }
}


//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::copyImage(Image& img,
                      int imgX1, int imgY1, int imgX2, int imgY2,
                      double dstX, double dstY)
{
    worldToScreen(dstX, dstY);
    // JME
    //agg::rect r(imgX1, imgY1, imgX2, imgY2);
    Rect r(imgX1, imgY1, imgX2, imgY2);
    m_renBase.copy_from(img.renBuf, &r, int(dstX)-imgX1, int(dstY)-imgY1);
}

//------------------------------------------------------------------------
AGG2D_TEMPLATE void TAGG2D::copyImage(Image& img, double dstX, double dstY)
{
    worldToScreen(dstX, dstY);
    m_renBase.copy_from(img.renBuf, 0, int(dstX), int(dstY));
}

//------------------------------------------------------------------------
AGG2D_TEMPLATE void Agg2DBase::Image::premultiply()
{
    PixFormat pixf(renBuf);
    pixf.premultiply();
}

//------------------------------------------------------------------------
AGG2D_TEMPLATE void Agg2DBase::Image::demultiply()
{
    PixFormat pixf(renBuf);
    pixf.demultiply();
}


//
////========================
////testing stufff
//
//int width = 256;
//int height = 384;
//
//Agg2D m_graphics;
//
//void AGGDraw(unsigned char * buffer)
//    {
//        m_graphics.attach(buffer, 
//                          256, 
//                          384,
//                          512);
//
//        m_graphics.clearAll(255, 255, 255);
//        //m_graphics.clearAll(0, 0, 0);
//
//        //m_graphics.blendMode(TAGG2D::BlendSub);
//        //m_graphics.blendMode(TAGG2D::BlendAdd);
//
//        m_graphics.antiAliasGamma(1.4);
//
//        // Set flipText(true) if you have the Y axis upside down.
//        //m_graphics.flipText(true);
//
//
//        // ClipBox.
//        //m_graphics.clipBox(50, 50, rbuf_window().width() - 50, rbuf_window().height() - 50);
//
//        // Transfornations - Rotate around (300,300) to 5 degree
//        //m_graphics.translate(-300, -300);
//        //m_graphics.rotate(TAGG2D::deg2Rad(5.0));
//        //m_graphics.translate(300, 300);
//
//        // Viewport - set 0,0,600,600 to the actual window size 
//        // preserving aspect ratio and placing the viewport in the center.
//        // To ignore aspect ratio use TAGG2D::Anisotropic
//        // Note that the viewport just adds transformations to the current
//        // affine matrix. So that, set the viewport *after* all transformations!
//        m_graphics.viewport(0, 0, 600, 600, 
//                            0, 0, width, height, 
//                            //TAGG2D::Anisotropic);
//                            TAGG2D::XMidYMid);
//
//
//        // Rounded Rect
//        m_graphics.lineColor(0, 0, 0);
//        m_graphics.noFill();
//        m_graphics.roundedRect(0.5, 0.5, 600-0.5, 600-0.5, 20.0);
///*
//
//        // Reglar Text
//        m_graphics.font("Times New Roman", 14.0, false, false);
//        m_graphics.fillColor(0, 0, 0);
//        m_graphics.noLine();
//        m_graphics.text(100, 20, "Regular Raster Text -- Fast, but can't be rotated");
//
//        // Outlined Text
//        m_graphics.font("Times New Roman", 50.0, false, false, TAGG2D::VectorFontCache);
//        m_graphics.lineColor(50, 0, 0);
//        m_graphics.fillColor(180, 200, 100);
//        m_graphics.lineWidth(1.0);
//        m_graphics.text(100.5, 50.5, "Outlined Text");
//
//        // Text Alignment
//        m_graphics.line(250.5-150, 150.5,    250.5+150, 150.5);
//        m_graphics.line(250.5,     150.5-20, 250.5,     150.5+20);
//        m_graphics.line(250.5-150, 200.5,    250.5+150, 200.5);
//        m_graphics.line(250.5,     200.5-20, 250.5,     200.5+20);
//        m_graphics.line(250.5-150, 250.5,    250.5+150, 250.5);
//        m_graphics.line(250.5,     250.5-20, 250.5,     250.5+20);
//        m_graphics.line(250.5-150, 300.5,    250.5+150, 300.5);
//        m_graphics.line(250.5,     300.5-20, 250.5,     300.5+20);
//        m_graphics.line(250.5-150, 350.5,    250.5+150, 350.5);
//        m_graphics.line(250.5,     350.5-20, 250.5,     350.5+20);
//        m_graphics.line(250.5-150, 400.5,    250.5+150, 400.5);
//        m_graphics.line(250.5,     400.5-20, 250.5,     400.5+20);
//        m_graphics.line(250.5-150, 450.5,    250.5+150, 450.5);
//        m_graphics.line(250.5,     450.5-20, 250.5,     450.5+20);
//        m_graphics.line(250.5-150, 500.5,    250.5+150, 500.5);
//        m_graphics.line(250.5,     500.5-20, 250.5,     500.5+20);
//        m_graphics.line(250.5-150, 550.5,    250.5+150, 550.5);
//        m_graphics.line(250.5,     550.5-20, 250.5,     550.5+20);
//*/
///*
//        m_graphics.fillColor(100, 50, 50);
//        m_graphics.noLine();
//        //m_graphics.textHints(false);
//        m_graphics.font("Times New Roman", 40.0, false, false, TAGG2D::VectorFontCache);
//
//        m_graphics.textAlignment(TAGG2D::AlignLeft, TAGG2D::AlignBottom);
//        m_graphics.text(250.0,     150.0, "Left-Bottom", true, 0, 0);
//
//        m_graphics.textAlignment(TAGG2D::AlignCenter, TAGG2D::AlignBottom);
//        m_graphics.text(250.0,     200.0, "Center-Bottom", true, 0, 0);
//
//        m_graphics.textAlignment(TAGG2D::AlignRight, TAGG2D::AlignBottom);
//        m_graphics.text(250.0,     250.0, "Right-Bottom", true, 0, 0);
//
//        m_graphics.textAlignment(TAGG2D::AlignLeft, TAGG2D::AlignCenter);
//        m_graphics.text(250.0,     300.0, "Left-Center", true, 0, 0);
//
//        m_graphics.textAlignment(TAGG2D::AlignCenter, TAGG2D::AlignCenter);
//        m_graphics.text(250.0,     350.0, "Center-Center", true, 0, 0);
//
//        m_graphics.textAlignment(TAGG2D::AlignRight, TAGG2D::AlignCenter);
//        m_graphics.text(250.0,     400.0, "Right-Center", true, 0, 0);
//
//        m_graphics.textAlignment(TAGG2D::AlignLeft, TAGG2D::AlignTop);
//        m_graphics.text(250.0,     450.0, "Left-Top", true, 0, 0);
//
//        m_graphics.textAlignment(TAGG2D::AlignCenter, TAGG2D::AlignTop);
//        m_graphics.text(250.0,     500.0, "Center-Top", true, 0, 0);
//
//        m_graphics.textAlignment(TAGG2D::AlignRight, TAGG2D::AlignTop);
//        m_graphics.text(250.0,     550.0, "Right-Top", true, 0, 0);
//
//*/
//        // Gradients (Aqua Buttons)
//        //=======================================
//        m_graphics.font("Verdana", 20.0, false, false, TAGG2D::VectorFontCache);
//        double xb1 = 400;
//        double yb1 = 80;
//        double xb2 = xb1 + 150;
//        double yb2 = yb1 + 36;
//
//        m_graphics.fillColor(TAGG2D::Color(0,50,180,180));
//        m_graphics.lineColor(TAGG2D::Color(0,0,80, 255));
//        m_graphics.lineWidth(1.0);
//        m_graphics.roundedRect(xb1, yb1, xb2, yb2, 12, 18);
//
//        m_graphics.lineColor(TAGG2D::Color(0,0,0,0));
//        m_graphics.fillLinearGradient(xb1, yb1, xb1, yb1+30, 
//                                      TAGG2D::Color(100,200,255,255), 
//                                      TAGG2D::Color(255,255,255,0));
//        m_graphics.roundedRect(xb1+3, yb1+2.5, xb2-3, yb1+30, 9, 18, 1, 1);
//
//        m_graphics.fillColor(TAGG2D::Color(0,0,50, 200));
//        m_graphics.noLine();
///*        m_graphics.textAlignment(TAGG2D::AlignCenter, TAGG2D::AlignCenter);
//        m_graphics.text((xb1 + xb2) / 2.0, (yb1 + yb2) / 2.0, "Aqua Button", true, 0.0, 0.0);
//*/
//        m_graphics.fillLinearGradient(xb1, yb2-20, xb1, yb2-3, 
//                                      TAGG2D::Color(0,  0,  255,0),
//                                      TAGG2D::Color(100,255,255,255)); 
//        m_graphics.roundedRect(xb1+3, yb2-20, xb2-3, yb2-2, 1, 1, 9, 18);
//
//
//        // Aqua Button Pressed
//        xb1 = 400;
//        yb1 = 30;
//        xb2 = xb1 + 150;
//        yb2 = yb1 + 36;
//
//        m_graphics.fillColor(TAGG2D::Color(0,50,180,180));
//        m_graphics.lineColor(TAGG2D::Color(0,0,0,  255));
//        m_graphics.lineWidth(2.0);
//        m_graphics.roundedRect(xb1, yb1, xb2, yb2, 12, 18);
//
//        m_graphics.lineColor(TAGG2D::Color(0,0,0,0));
//        m_graphics.fillLinearGradient(xb1, yb1+2, xb1, yb1+25, 
//                                      TAGG2D::Color(60, 160,255,255), 
//                                      TAGG2D::Color(100,255,255,0));
//        m_graphics.roundedRect(xb1+3, yb1+2.5, xb2-3, yb1+30, 9, 18, 1, 1);
//
//        m_graphics.fillColor(TAGG2D::Color(0,0,50, 255));
//        m_graphics.noLine();
///*        m_graphics.textAlignment(TAGG2D::AlignCenter, TAGG2D::AlignCenter);
//        m_graphics.text((xb1 + xb2) / 2.0, (yb1 + yb2) / 2.0, "Aqua Pressed", 0.0, 0.0);
//*/
//        m_graphics.fillLinearGradient(xb1, yb2-25, xb1, yb2-5, 
//                                      TAGG2D::Color(0,  180,255,0),
//                                      TAGG2D::Color(0,  200,255,255)); 
//        m_graphics.roundedRect(xb1+3, yb2-25, xb2-3, yb2-2, 1, 1, 9, 18);
//
//
//
//
//        // Basic Shapes -- Ellipse
//        //===========================================
//        m_graphics.lineWidth(3.5);
//        m_graphics.lineColor(20,  80,  80);
//        m_graphics.fillColor(200, 255, 80, 200);
//        m_graphics.ellipse(450, 200, 50, 90);
//
//
//        // Paths
//        //===========================================
//        m_graphics.resetPath();
//        m_graphics.fillColor(255, 0, 0, 100);
//        m_graphics.lineColor(0, 0, 255, 100);
//        m_graphics.lineWidth(2);
//        m_graphics.moveTo(300/2, 200/2);
//        m_graphics.horLineRel(-150/2);
//        m_graphics.arcRel(150/2, 150/2, 0, 1, 0, 150/2, -150/2);
//        m_graphics.closePolygon();
//        m_graphics.drawPath();
//
//        m_graphics.resetPath();
//        m_graphics.fillColor(255, 255, 0, 100);
//        m_graphics.lineColor(0, 0, 255, 100);
//        m_graphics.lineWidth(2);
//        m_graphics.moveTo(275/2, 175/2);
//        m_graphics.verLineRel(-150/2);
//        m_graphics.arcRel(150/2, 150/2, 0, 0, 0, -150/2, 150/2);
//        m_graphics.closePolygon();
//        m_graphics.drawPath();
//
//
//        m_graphics.resetPath();
//        m_graphics.noFill();
//        m_graphics.lineColor(127, 0, 0);
//        m_graphics.lineWidth(5);
//        m_graphics.moveTo(600/2, 350/2);
//        m_graphics.lineRel(50/2, -25/2);
//        m_graphics.arcRel(25/2, 25/2, TAGG2D::deg2Rad(-30), 0, 1, 50/2, -25/2);
//        m_graphics.lineRel(50/2, -25/2);
//        m_graphics.arcRel(25/2, 50/2, TAGG2D::deg2Rad(-30), 0, 1, 50/2, -25/2);
//        m_graphics.lineRel(50/2, -25/2);
//        m_graphics.arcRel(25/2, 75/2, TAGG2D::deg2Rad(-30), 0, 1, 50/2, -25/2);
//        m_graphics.lineRel(50, -25);
//        m_graphics.arcRel(25/2, 100/2, TAGG2D::deg2Rad(-30), 0, 1, 50/2, -25/2);
//        m_graphics.lineRel(50/2, -25/2);
//        m_graphics.drawPath();
//
//
//        // Master Alpha. From now on everything will be translucent
//        //===========================================
//        m_graphics.masterAlpha(0.85);
//
//
//        // Image Transformations
//        //===========================================
///*        TAGG2D::Image img(rbuf_img(0).buf(), 
//                         rbuf_img(0).width(), 
//                         rbuf_img(0).height(), 
//                         rbuf_img(0).stride());
//        m_graphics.imageFilter(TAGG2D::Bilinear);
//
//        //m_graphics.imageResample(TAGG2D::NoResample);
//        //m_graphics.imageResample(TAGG2D::ResampleAlways);
//        m_graphics.imageResample(TAGG2D::ResampleOnZoomOut);
//
//        // Set the initial image blending operation as BlendDst, that actually 
//        // does nothing. 
//        //-----------------
//        m_graphics.imageBlendMode(TAGG2D::BlendDst);
//
//
//        // Transform the whole image to the destination rectangle
//        //-----------------
//        //m_graphics.transformImage(img, 450, 200, 595, 350);
//
//        // Transform the rectangular part of the image to the destination rectangle
//        //-----------------
//        //m_graphics.transformImage(img, 60, 60, img.width()-60, img.height()-60,
//        //                          450, 200, 595, 350);
//
//        // Transform the whole image to the destination parallelogram
//        //-----------------
//        //double parl[6] = { 450, 200, 595, 220, 575, 350 };
//        //m_graphics.transformImage(img, parl);
//
//        // Transform the rectangular part of the image to the destination parallelogram
//        //-----------------
//        //double parl[6] = { 450, 200, 595, 220, 575, 350 };
//        //m_graphics.transformImage(img, 60, 60, img.width()-60, img.height()-60, parl);
//
//        // Transform image to the destination path. The scale is determined by a rectangle
//        //-----------------
//        //m_graphics.resetPath();
//        //m_graphics.moveTo(450, 200);
//        //m_graphics.cubicCurveTo(595, 220, 575, 350, 595, 350);
//        //m_graphics.lineTo(470, 340);
//        //m_graphics.transformImagePath(img, 450, 200, 595, 350);
//
//
//        // Transform image to the destination path.
//        // The scale is determined by a rectangle
//        //-----------------
//        m_graphics.resetPath();
//        m_graphics.moveTo(450, 200);
//        m_graphics.cubicCurveTo(595, 220, 575, 350, 595, 350);
//        m_graphics.lineTo(470, 340);
//        m_graphics.transformImagePath(img, 60, 60, img.width()-60, img.height()-60,
//                                      450, 200, 595, 350);
//
//        // Transform image to the destination path. 
//        // The transformation is determined by a parallelogram
//        //m_graphics.resetPath();
//        //m_graphics.moveTo(450, 200);
//        //m_graphics.cubicCurveTo(595, 220, 575, 350, 595, 350);
//        //m_graphics.lineTo(470, 340);
//        //double parl[6] = { 450, 200, 595, 220, 575, 350 };
//        //m_graphics.transformImagePath(img, parl);
//
//        // Transform the rectangular part of the image to the destination path. 
//        // The transformation is determined by a parallelogram
//        //m_graphics.resetPath();
//        //m_graphics.moveTo(450, 200);
//        //m_graphics.cubicCurveTo(595, 220, 575, 350, 595, 350);
//        //m_graphics.lineTo(470, 340);
//        //double parl[6] = { 450, 200, 595, 220, 575, 350 };
//        //m_graphics.transformImagePath(img, 60, 60, img.width()-60, img.height()-60, parl);
//*/
//
//        // Add/Sub/Contrast Blending Modes
//        m_graphics.noLine();
//        m_graphics.fillColor(70, 70, 0);
//        m_graphics.blendMode(TAGG2D::BlendAdd);
//        m_graphics.ellipse(500, 280, 20, 40);
//
//        m_graphics.fillColor(255, 255, 255);
//        m_graphics.blendMode(TAGG2D::BlendContrast);
//        m_graphics.ellipse(500+40, 280, 20, 40);
//
//
//
//        // Radial gradient.
//        m_graphics.blendMode(TAGG2D::BlendAlpha);
//        m_graphics.fillRadialGradient(400, 500, 40, 
//                                      TAGG2D::Color(255, 255, 0, 0),
//                                      TAGG2D::Color(0, 0, 127),
//                                      TAGG2D::Color(0, 255, 0, 0));
//        m_graphics.ellipse(400, 500, 40, 40);
//
//    }
//