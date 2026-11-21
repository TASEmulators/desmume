/*
The MIT License

Copyright (C) 2009-2026 DeSmuME team

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include "native_draw.h"

#if defined(DESMUME_NATIVE_DRAW) && defined(WIN32)

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <gdiplus.h>

#include <math.h>
#include <string.h>
#include <algorithm>

#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

// ---------------------------------------------------------------------------
// GDI+ lifetime.  The session is intentionally never shut down: the HUD
// targets are global objects, and their destructors may run after the CRT has
// torn down function-local statics.  One leaked token at process exit is
// harmless and avoids a use-after-shutdown of Bitmap/Graphics.
// ---------------------------------------------------------------------------
struct NativeGdiSession
{
	ULONG_PTR token;
	GdiplusStartupInput input;
	NativeGdiSession() : token(0) { GdiplusStartup(&token, &input, NULL); }
};
static void EnsureGdiplus()
{
	static NativeGdiSession* s = new NativeGdiSession();
	(void)s;
}

static bool s_premultipliedOutput = true;

// ---------------------------------------------------------------------------
// Bitmap fonts.  The payloads are RCDATA resources linked into the .exe (see
// frontend/windows/osd_fonts/); this translation unit only carries the name
// -> resource-id map.  The names and their order match fonts_list[] in
// aggdraw.cpp, so old INI settings and Lua scripts keep resolving identically.
// ---------------------------------------------------------------------------
#include "../../../windows/osd_fonts/osd_fonts.h"

namespace
{
	struct FontEntry
	{
		const char* name;
		int resId;
		const agg::int8u* data;
		int size;
	};

	static FontEntry s_fonts[] =
	{
		{ "gse4x6",                 IDR_OSD_FONT_GSE4X6,                NULL, 0 },
		{ "gse4x8",                 IDR_OSD_FONT_GSE4X8,                NULL, 0 },
		{ "gse5x7",                 IDR_OSD_FONT_GSE5X7,                NULL, 0 },
		{ "gse5x9",                 IDR_OSD_FONT_GSE5X9,                NULL, 0 },
		{ "gse6x9",                 IDR_OSD_FONT_GSE6X9,                NULL, 0 },
		{ "gse6x12",                IDR_OSD_FONT_GSE6X12,               NULL, 0 },
		{ "gse7x11",                IDR_OSD_FONT_GSE7X11,               NULL, 0 },
		{ "gse7x11_bold",           IDR_OSD_FONT_GSE7X11_BOLD,          NULL, 0 },
		{ "gse7x15",                IDR_OSD_FONT_GSE7X15,               NULL, 0 },
		{ "gse7x15_bold",           IDR_OSD_FONT_GSE7X15_BOLD,          NULL, 0 },
		{ "gse8x16",                IDR_OSD_FONT_GSE8X16,               NULL, 0 },
		{ "gse8x16_bold",           IDR_OSD_FONT_GSE8X16_BOLD,          NULL, 0 },
		{ "mcs11_prop",             IDR_OSD_FONT_MCS11_PROP,            NULL, 0 },
		{ "mcs11_prop_condensed",   IDR_OSD_FONT_MCS11_PROP_CONDENSED,   NULL, 0 },
		{ "mcs12_prop",             IDR_OSD_FONT_MCS12_PROP,            NULL, 0 },
		{ "mcs13_prop",             IDR_OSD_FONT_MCS13_PROP,            NULL, 0 },
		{ "mcs5x10_mono",           IDR_OSD_FONT_MCS5X10_MONO,          NULL, 0 },
		{ "mcs5x11_mono",           IDR_OSD_FONT_MCS5X11_MONO,          NULL, 0 },
		{ "mcs6x10_mono",           IDR_OSD_FONT_MCS6X10_MONO,          NULL, 0 },
		{ "mcs6x11_mono",           IDR_OSD_FONT_MCS6X11_MONO,          NULL, 0 },
		{ "mcs7x12_mono_high",      IDR_OSD_FONT_MCS7X12_MONO_HIGH,     NULL, 0 },
		{ "mcs7x12_mono_low",       IDR_OSD_FONT_MCS7X12_MONO_LOW,      NULL, 0 },
		{ "verdana12",              IDR_OSD_FONT_VERDANA12,             NULL, 0 },
		{ "verdana12_bold",         IDR_OSD_FONT_VERDANA12_BOLD,        NULL, 0 },
		{ "verdana13",              IDR_OSD_FONT_VERDANA13,             NULL, 0 },
		{ "verdana13_bold",         IDR_OSD_FONT_VERDANA13_BOLD,        NULL, 0 },
		{ "verdana14",              IDR_OSD_FONT_VERDANA14,             NULL, 0 },
		{ "verdana14_bold",         IDR_OSD_FONT_VERDANA14_BOLD,        NULL, 0 },
		{ "verdana16",              IDR_OSD_FONT_VERDANA16,             NULL, 0 },
		{ "verdana16_bold",         IDR_OSD_FONT_VERDANA16_BOLD,        NULL, 0 },
		{ "verdana17",              IDR_OSD_FONT_VERDANA17,             NULL, 0 },
		{ "verdana17_bold",         IDR_OSD_FONT_VERDANA17_BOLD,        NULL, 0 },
		{ "verdana18",              IDR_OSD_FONT_VERDANA18,             NULL, 0 },
		{ "verdana18_bold",         IDR_OSD_FONT_VERDANA18_BOLD,        NULL, 0 },
	};
	const size_t s_fontCount = sizeof(s_fonts) / sizeof(s_fonts[0]);

	// Length of an AGG embedded raster font blob, derived from its own offset
	// table.  Used to validate that the resource wasn't truncated.
	int ExpectedFontLength(const agg::int8u* f, int available)
	{
		if (!f || available < 4) return 0;
		const int height = f[0];
		const int count = f[3] ? f[3] : 256;
		if (height <= 0 || height > 64 || count <= 0 || count > 256) return 0;
		const int tableEnd = 4 + count * 2;
		if (available < tableEnd) return 0;
		int maxEnd = 0;
		for (int i = 0; i < count; ++i)
		{
			const unsigned off = (unsigned)f[4 + i * 2] | ((unsigned)f[5 + i * 2] << 8);
			const int glyph = tableEnd + (int)off;
			if (glyph >= available) continue;
			const int w = f[glyph];
			if (w < 0 || w > 64) continue;
			const int end = glyph + 1 + ((w + 7) >> 3) * height;
			if (end <= available && end > maxEnd) maxEnd = end;
		}
		return maxEnd;
	}

	int LoadFontResources()
	{
		static bool loaded = false;
		static int count = 0;
		if (loaded) return count;
		loaded = true;
		HMODULE mod = GetModuleHandle(NULL);
		if (!mod) return 0;
		for (size_t i = 0; i < s_fontCount; ++i)
		{
			HRSRC h = FindResource(mod, MAKEINTRESOURCE(s_fonts[i].resId), RT_RCDATA);
			if (!h) continue;
			const DWORD bytes = SizeofResource(mod, h);
			if (bytes < 4) continue;
			HGLOBAL hg = LoadResource(mod, h);
			if (!hg) continue;
			const void* p = LockResource(hg);
			if (!p) continue;
			const agg::int8u* data = (const agg::int8u*)p;
			if (ExpectedFontLength(data, (int)bytes) < 4) continue;
			s_fonts[i].data = data;
			s_fonts[i].size = (int)bytes;
			++count;
		}
		return count;
	}

	const FontEntry* FindFont(const char* name)
	{
		if (!name) return NULL;
		LoadFontResources();
		for (size_t i = 0; i < s_fontCount; ++i)
			if (strcmp(name, s_fonts[i].name) == 0) return &s_fonts[i];
		return NULL;
	}
}

void NativeDraw_Init()
{
	EnsureGdiplus();
	LoadFontResources();
}
void NativeDraw_SetPremultipliedOutput(bool enabled) { s_premultipliedOutput = enabled; }

const agg::int8u* NativeDraw_FontData(const char* name)
{
	const FontEntry* e = FindFont(name);
	return e ? e->data : NULL;
}
int NativeDraw_FontSize(const char* name)
{
	const FontEntry* e = FindFont(name);
	return e ? e->size : 0;
}

namespace
{
	inline BYTE ClampByte(int v) { return (BYTE)(v < 0 ? 0 : (v > 255 ? 255 : v)); }

	Color ToGdiColor(const AggColor& c, double masterAlpha)
	{
		return Color(ClampByte((int)(c.a * masterAlpha + 0.5)),
		             ClampByte(c.r), ClampByte(c.g), ClampByte(c.b));
	}

	// straight-alpha source-over, BGRA byte order
	inline void BlendBGRA(unsigned char* p, int r, int g, int b, int a, bool premultiplied)
	{
		const int sa = a;
		const int da = p[3];
		if (sa <= 0) return;
		if (premultiplied)
		{
			// Agg2D/pixfmt_bgra32 convention: RGB is stored premultiplied.
			// For a fully transparent destination a 50% red becomes (127,0,0,128).
			const int inv = 255 - sa;
			p[0] = (unsigned char)((b * sa + p[0] * inv) / 255);
			p[1] = (unsigned char)((g * sa + p[1] * inv) / 255);
			p[2] = (unsigned char)((r * sa + p[2] * inv) / 255);
			p[3] = (unsigned char)(sa + (da * inv) / 255);
			return;
		}
		const int ws = sa * 255;
		const int wd = da * (255 - sa);
		const int den = ws + wd;
		if (den <= 0) { p[0] = p[1] = p[2] = p[3] = 0; return; }
		p[0] = (unsigned char)((b * ws + p[0] * wd) / den);
		p[1] = (unsigned char)((g * ws + p[1] * wd) / den);
		p[2] = (unsigned char)((r * ws + p[2] * wd) / den);
		p[3] = (unsigned char)((den + 127) / 255);
	}

	inline double Deg(double rad) { return rad * 180.0 / 3.14159265358979323846; }

	void BuildRoundedRect(GraphicsPath& path, double x1, double y1, double x2, double y2,
		double rxTL, double ryTL, double rxTR, double ryTR,
		double rxBR, double ryBR, double rxBL, double ryBL)
	{
		const double w = fabs(x2 - x1), h = fabs(y2 - y1);
		const double lx = x1 < x2 ? x1 : x2, ly = y1 < y2 ? y1 : y2;
		const double maxRx = w * 0.5, maxRy = h * 0.5;
		rxTL = std::min(rxTL, maxRx); rxTR = std::min(rxTR, maxRx);
		rxBR = std::min(rxBR, maxRx); rxBL = std::min(rxBL, maxRx);
		ryTL = std::min(ryTL, maxRy); ryTR = std::min(ryTR, maxRy);
		ryBR = std::min(ryBR, maxRy); ryBL = std::min(ryBL, maxRy);
		if (rxTL < 0) rxTL = 0;
		if (rxTR < 0) rxTR = 0;
		if (rxBR < 0) rxBR = 0;
		if (rxBL < 0) rxBL = 0;
		if (ryTL < 0) ryTL = 0;
		if (ryTR < 0) ryTR = 0;
		if (ryBR < 0) ryBR = 0;
		if (ryBL < 0) ryBL = 0;

		path.AddArc((REAL)lx, (REAL)ly, (REAL)(2 * rxTL), (REAL)(2 * ryTL), 180.0f, 90.0f);
		path.AddArc((REAL)(lx + w - 2 * rxTR), (REAL)ly, (REAL)(2 * rxTR), (REAL)(2 * ryTR), 270.0f, 90.0f);
		path.AddArc((REAL)(lx + w - 2 * rxBR), (REAL)(ly + h - 2 * ryBR),
		            (REAL)(2 * rxBR), (REAL)(2 * ryBR), 0.0f, 90.0f);
		path.AddArc((REAL)lx, (REAL)(ly + h - 2 * ryBL),
		            (REAL)(2 * rxBL), (REAL)(2 * ryBL), 90.0f, 90.0f);
		path.CloseFigure();
	}

	// -----------------------------------------------------------------------
	// Affine helpers (AGG row-vector convention: [sx,shy,shx,sy,tx,ty]).
	// -----------------------------------------------------------------------
	void MatIdentity(double m[6])
	{
		m[0] = m[3] = 1.0;
		m[1] = m[2] = m[4] = m[5] = 0.0;
	}
	void MatMul(const double a[6], const double b[6], double out[6])
	{
		const double r[6] = {
			a[0] * b[0] + a[1] * b[2],
			a[0] * b[1] + a[1] * b[3],
			a[2] * b[0] + a[3] * b[2],
			a[2] * b[1] + a[3] * b[3],
			a[4] * b[0] + a[5] * b[2] + b[4],
			a[4] * b[1] + a[5] * b[3] + b[5]
		};
		for (int i = 0; i < 6; ++i) out[i] = r[i];
	}
	void MatInvert(const double m[6], double out[6])
	{
		const double det = m[0] * m[3] - m[1] * m[2];
		if (fabs(det) < 1e-14) { MatIdentity(out); return; }
		const double d = 1.0 / det;
		const double t0 = m[3] * d;
		const double sy = m[0] * d;
		const double shy = -m[1] * d;
		const double shx = -m[2] * d;
		out[0] = t0; out[1] = shy; out[2] = shx; out[3] = sy;
		out[4] = -m[4] * t0 - m[5] * shx;
		out[5] = -m[4] * shy - m[5] * sy;
	}

	// -----------------------------------------------------------------------
	// Blend modes.  GDI+ only offers SourceOver / SourceCopy, so every other
	// Agg2D comp-op is reproduced by drawing into a scratch surface and
	// compositing it here.  Inputs and outputs are BGRA in the target's
	// storage convention; the maths uses premultiplied RGBA floats.
	// -----------------------------------------------------------------------
	inline double Clamp01(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }
	inline double Byte(int v) { return (double)ClampByte(v); }

	double BlendFunc(Agg2DBase::BlendMode mode, double cb, double cs)
	{
		switch (mode)
		{
		case Agg2DBase::BlendMultiply:   return cb * cs;
		case Agg2DBase::BlendScreen:     return cb + cs - cb * cs;
		case Agg2DBase::BlendOverlay:    return (cs <= 0.5) ? 2.0 * cs * cb : 1.0 - 2.0 * (1.0 - cs) * (1.0 - cb);
		case Agg2DBase::BlendDarken:     return (cb < cs) ? cb : cs;
		case Agg2DBase::BlendLighten:    return (cb > cs) ? cb : cs;
		case Agg2DBase::BlendColorDodge:
			if (cb <= 0.0) return 0.0;
			if (cs >= 1.0) return 1.0;
			return Clamp01(cb / (1.0 - cs));
		case Agg2DBase::BlendColorBurn:
			if (cb >= 1.0) return 1.0;
			if (cs <= 0.0) return 0.0;
			return Clamp01(1.0 - (1.0 - cb) / cs);
		case Agg2DBase::BlendHardLight:  return (cs <= 0.5) ? 2.0 * cs * cb : 1.0 - 2.0 * (1.0 - cs) * (1.0 - cb);
		case Agg2DBase::BlendSoftLight:
		{
			if (cs <= 0.5) return cb - (1.0 - 2.0 * cs) * cb * (1.0 - cb);
			const double d = (cb <= 0.25) ? ((16.0 * cb - 12.0) * cb + 4.0) * cb : sqrt(cb);
			return cb + (2.0 * cs - 1.0) * (d - cb);
		}
		case Agg2DBase::BlendDifference: return fabs(cb - cs);
		case Agg2DBase::BlendExclusion:  return Clamp01(cb + cs - 2.0 * cb * cs);
		default:                         return cs;
		}
	}

	bool IsSeparableBlend(Agg2DBase::BlendMode mode)
	{
		return mode == Agg2DBase::BlendMultiply || mode == Agg2DBase::BlendScreen ||
		       mode == Agg2DBase::BlendOverlay || mode == Agg2DBase::BlendDarken ||
		       mode == Agg2DBase::BlendLighten || mode == Agg2DBase::BlendColorDodge ||
		       mode == Agg2DBase::BlendColorBurn || mode == Agg2DBase::BlendHardLight ||
		       mode == Agg2DBase::BlendSoftLight || mode == Agg2DBase::BlendDifference ||
		       mode == Agg2DBase::BlendExclusion;
	}

	// s = source (scratch), d = destination, out = result.  Both are BGRA.
	void BlendPixelBGRA(Agg2DBase::BlendMode mode, const unsigned char* s,
		const unsigned char* d, unsigned char* out, bool premultiplied)
	{
		const double sa = Byte(s[3]) / 255.0;
		const double da = Byte(d[3]) / 255.0;
		double sp[3], dp[3];  // R,G,B premultiplied
		for (int c = 0; c < 3; ++c)
		{
			const int idx = 2 - c;  // BGRA -> RGB
			const double v = Byte(s[idx]) / 255.0;
			const double w = Byte(d[idx]) / 255.0;
			sp[c] = premultiplied ? v : v * sa;
			dp[c] = premultiplied ? w : w * da;
		}
		double rp[3] = { 0, 0, 0 }, ra = 0.0;

		if (mode == Agg2DBase::BlendAdd)
		{
			for (int c = 0; c < 3; ++c) rp[c] = Clamp01(sp[c] + dp[c]);
			ra = Clamp01(sa + da);
		}
		else if (IsSeparableBlend(mode))
		{
			const double cs[3] = { sa > 0 ? Clamp01(sp[0] / sa) : 0,
			                       sa > 0 ? Clamp01(sp[1] / sa) : 0,
			                       sa > 0 ? Clamp01(sp[2] / sa) : 0 };
			const double cb[3] = { da > 0 ? Clamp01(dp[0] / da) : 0,
			                       da > 0 ? Clamp01(dp[1] / da) : 0,
			                       da > 0 ? Clamp01(dp[2] / da) : 0 };
			for (int c = 0; c < 3; ++c)
				rp[c] = Clamp01(sa * (1.0 - da) * cs[c] + da * (1.0 - sa) * cb[c]
				                + sa * da * BlendFunc(mode, cb[c], cs[c]));
			ra = Clamp01(sa + da - sa * da);
		}
		else
		{
			switch (mode)
			{
			case Agg2DBase::BlendClear:    ra = 0.0; break;
			case Agg2DBase::BlendSrc:
				for (int c = 0; c < 3; ++c) rp[c] = sp[c];  ra = sa; break;
			case Agg2DBase::BlendDst:
				for (int c = 0; c < 3; ++c) rp[c] = dp[c];  ra = da; break;
			case Agg2DBase::BlendDstOver:
				for (int c = 0; c < 3; ++c) rp[c] = dp[c] + sp[c] * (1.0 - da);
				ra = da + sa * (1.0 - da); break;
			case Agg2DBase::BlendSrcIn:
				for (int c = 0; c < 3; ++c) rp[c] = sp[c] * da;  ra = sa * da; break;
			case Agg2DBase::BlendDstIn:
				for (int c = 0; c < 3; ++c) rp[c] = dp[c] * sa;  ra = da * sa; break;
			case Agg2DBase::BlendSrcOut:
				for (int c = 0; c < 3; ++c) rp[c] = sp[c] * (1.0 - da);  ra = sa * (1.0 - da); break;
			case Agg2DBase::BlendDstOut:
				for (int c = 0; c < 3; ++c) rp[c] = dp[c] * (1.0 - sa);  ra = da * (1.0 - sa); break;
			case Agg2DBase::BlendSrcAtop:
				for (int c = 0; c < 3; ++c) rp[c] = sp[c] * da + dp[c] * (1.0 - sa);  ra = da; break;
			case Agg2DBase::BlendDstAtop:
				for (int c = 0; c < 3; ++c) rp[c] = dp[c] * sa + sp[c] * (1.0 - da);  ra = sa; break;
			case Agg2DBase::BlendXor:
				for (int c = 0; c < 3; ++c) rp[c] = sp[c] * (1.0 - da) + dp[c] * (1.0 - sa);
				ra = sa * (1.0 - da) + da * (1.0 - sa); break;
			default:  // BlendAlpha / BlendSrcOver
				for (int c = 0; c < 3; ++c) rp[c] = sp[c] + dp[c] * (1.0 - sa);
				ra = sa + da * (1.0 - sa); break;
			}
		}

		const int A = (int)(Clamp01(ra) * 255.0 + 0.5);
		for (int c = 0; c < 3; ++c)
		{
			const double premul = Clamp01(rp[c]);
			double v = premul;
			if (!premultiplied) v = A > 0 ? Clamp01(premul / ra) : 0.0;
			out[2 - c] = ClampByte((int)(v * 255.0 + 0.5));
		}
		out[3] = (unsigned char)A;
	}
}

// ---------------------------------------------------------------------------
// construction / framebuffer
// ---------------------------------------------------------------------------
NativeDrawTarget::NativeDrawTarget(agg::int8u* buf, int width, int height, int stride)
	: m_pixels(buf), m_width(width), m_height(height), m_stride(stride),
	  m_bitmap(NULL), m_graphics(NULL), m_active(NULL),
	  m_scratchBitmap(NULL), m_scratchGraphics(NULL), m_scratchActive(false),
#ifdef AGG2D_USE_VECTORFONTS
	  m_vectorFont(NULL), m_fontCollection(NULL),
#endif
	  m_fill(255, 255, 255, 255), m_line(0, 0, 0, 255),
	  m_hasFill(true), m_hasLine(true), m_lineWidth(1.0),
	  m_lineCap(Agg2DBase::CAP_ROUND), m_lineJoin(Agg2DBase::JOIN_ROUND),
	  m_fillEvenOdd(false),
	  m_blendMode(Agg2DBase::BlendAlpha), m_imageBlendMode(Agg2DBase::BlendDst),
	  m_imageBlendColor(0, 0, 0, 255),
	  m_masterAlpha(1.0), m_antiAliasGamma(1.0),
	  m_imageFilter(Agg2DBase::NoFilter), m_imageResample(Agg2DBase::NoResample),
	  m_clip(0, 0, 0, 0), m_hasClip(false),
	  m_font(NULL)
{
	m_rbuf.attach(buf, (unsigned)width, (unsigned)height, stride);
	m_clip = Agg2DBase::RectD(0, 0, width, height);
	MatIdentity(m_xform);
	MatIdentity(m_affineX);
	MatIdentity(m_viewportX);
	m_pathCursor.x = m_pathCursor.y = 0.0;
}

NativeDrawTarget::~NativeDrawTarget()
{
	ReleaseScratch();
	ReleaseSurface();
#ifdef AGG2D_USE_VECTORFONTS
	delete m_vectorFont;
	delete m_fontCollection;
#endif
}

void NativeDrawTarget::ReleaseSurface()
{
	if (m_graphics) { m_graphics->Flush(); delete m_graphics; m_graphics = NULL; }
	m_active = NULL;
	if (m_bitmap) { delete m_bitmap; m_bitmap = NULL; }
}

bool NativeDrawTarget::EnsureSurface()
{
	if (!m_pixels || m_width <= 0 || m_height <= 0) return false;
	if (m_graphics) { m_active = m_graphics; return true; }
	EnsureGdiplus();
	m_bitmap = new Bitmap((INT)m_width, (INT)m_height, (INT)m_stride,
	                      s_premultipliedOutput ? PixelFormat32bppPARGB : PixelFormat32bppARGB,
	                      (BYTE*)m_pixels);
	if (m_bitmap->GetLastStatus() != Ok) { delete m_bitmap; m_bitmap = NULL; return false; }
	m_graphics = Graphics::FromImage(m_bitmap);
	if (!m_graphics) return false;
	m_graphics->SetCompositingMode(CompositingModeSourceOver);
	m_graphics->SetCompositingQuality(CompositingQualityHighQuality);
	m_graphics->SetSmoothingMode(SmoothingModeAntiAlias);
	m_graphics->SetPixelOffsetMode(PixelOffsetModeHalf);
	m_graphics->SetInterpolationMode(InterpolationModeNearestNeighbor);
	m_graphics->SetTextRenderingHint(TextRenderingHintSingleBitPerPixel);
	m_active = m_graphics;
	ApplyClip();
	return true;
}

// ---------------------------------------------------------------------------
// Non-default blend modes: scratch surface + explicit per-pixel composite.
// ---------------------------------------------------------------------------
bool NativeDrawTarget::EnsureScratch()
{
	if (m_scratchGraphics) return true;
	if (!m_pixels || m_width <= 0 || m_height <= 0) return false;
	const size_t bytes = (size_t)(m_stride < 0 ? -m_stride : m_stride) * (size_t)m_height;
	m_scratchPixels.assign(bytes, 0);
	m_scratchBitmap = new Bitmap((INT)m_width, (INT)m_height, (INT)m_stride,
	                             s_premultipliedOutput ? PixelFormat32bppPARGB : PixelFormat32bppARGB,
	                             (BYTE*)m_scratchPixels.data());
	if (m_scratchBitmap->GetLastStatus() != Ok) { delete m_scratchBitmap; m_scratchBitmap = NULL; return false; }
	m_scratchGraphics = Graphics::FromImage(m_scratchBitmap);
	if (!m_scratchGraphics) { delete m_scratchBitmap; m_scratchBitmap = NULL; return false; }
	m_scratchGraphics->SetCompositingMode(CompositingModeSourceOver);
	m_scratchGraphics->SetCompositingQuality(CompositingQualityHighQuality);
	m_scratchGraphics->SetSmoothingMode(SmoothingModeAntiAlias);
	m_scratchGraphics->SetPixelOffsetMode(PixelOffsetModeHalf);
	return true;
}

void NativeDrawTarget::ReleaseScratch()
{
	if (m_scratchGraphics) { delete m_scratchGraphics; m_scratchGraphics = NULL; }
	if (m_scratchBitmap) { delete m_scratchBitmap; m_scratchBitmap = NULL; }
	m_scratchPixels.clear();
	m_scratchActive = false;
}

void NativeDrawTarget::CompositeScratch(Agg2DBase::BlendMode mode)
{
	if (!m_pixels || m_scratchPixels.empty()) return;
	const int absStride = m_stride < 0 ? -m_stride : m_stride;
	for (int y = 0; y < m_height; ++y)
	{
		const unsigned char* srow = m_scratchPixels.data() + (size_t)y * (size_t)absStride;
		unsigned char* drow = m_pixels + (size_t)y * (size_t)m_stride;
		for (int x = 0; x < m_width; ++x)
		{
			const unsigned char* s = srow + (size_t)x * 4;
			unsigned char* d = drow + (size_t)x * 4;
			if (s[3] == 0) continue;
			unsigned char out[4];
			BlendPixelBGRA(mode, s, d, out, s_premultipliedOutput);
			memcpy(d, out, 4);
		}
	}
	memset(m_scratchPixels.data(), 0, m_scratchPixels.size());
}

Gdiplus::Graphics* NativeDrawTarget::BeginPrimitive()
{
	if (!Touch()) return NULL;
	const bool normal = (m_blendMode == Agg2DBase::BlendAlpha ||
	                     m_blendMode == Agg2DBase::BlendSrcOver);
	if (normal)
	{
		m_scratchActive = false;
		m_active = m_graphics;
		return m_graphics;
	}
	if (m_graphics) m_graphics->Flush();
	if (!EnsureScratch()) { m_active = m_graphics; return m_graphics; }
	memset(m_scratchPixels.data(), 0, m_scratchPixels.size());
	m_scratchActive = true;
	m_active = m_scratchGraphics;
	ApplyClip();
	return m_scratchGraphics;
}

void NativeDrawTarget::EndPrimitive()
{
	if (m_scratchActive)
	{
		if (m_scratchGraphics) m_scratchGraphics->Flush();
		CompositeScratch(m_blendMode);
		m_scratchActive = false;
	}
	m_active = m_graphics;
}

bool NativeDrawTarget::Touch()
{
	dirty();
	return EnsureSurface();
}

void NativeDrawTarget::ApplyClip()
{
	Graphics* targets[2] = { m_graphics, m_scratchGraphics };
	for (int i = 0; i < 2; ++i)
	{
		Graphics* g = targets[i];
		if (!g) continue;
		if (!m_hasClip) { g->ResetClip(); continue; }
		g->SetClip(RectF((REAL)m_clip.x1, (REAL)m_clip.y1,
		                 (REAL)(m_clip.x2 - m_clip.x1),
		                 (REAL)(m_clip.y2 - m_clip.y1)), CombineModeReplace);
	}
}

void NativeDrawTarget::TransformPoint(double& x, double& y) const
{
	const double nx = x * m_xform[0] + y * m_xform[2] + m_xform[4];
	const double ny = x * m_xform[1] + y * m_xform[3] + m_xform[5];
	x = nx; y = ny;
}

void NativeDrawTarget::ComposeTransform()
{
	MatMul(m_affineX, m_viewportX, m_xform);
	m_affine = Agg2DBase::Affine(m_affineX[0], m_affineX[1], m_affineX[2],
	                             m_affineX[3], m_affineX[4], m_affineX[5]);
}

// Agg2D uses premultiply() for its transform helpers: the new matrix is
// applied before the existing user affine.
void NativeDrawTarget::PremultiplyAffine(double a, double b, double c, double d, double e, double f)
{
	const double m[6] = { a, b, c, d, e, f };
	double r[6];
	MatMul(m, m_affineX, r);
	for (int i = 0; i < 6; ++i) m_affineX[i] = r[i];
	ComposeTransform();
}

void NativeDrawTarget::clear()
{
	if (empty) return;
	if (m_graphics) m_graphics->Flush();
	if (m_pixels && m_width > 0 && m_height > 0)
	{
		const int stride = m_stride < 0 ? -m_stride : m_stride;
		for (int y = 0; y < m_height; ++y)
			memset(m_pixels + (size_t)y * (size_t)(m_stride < 0 ? -m_stride : m_stride), 0, (size_t)stride);
	}
	undirty();
}

void NativeDrawTarget::setDrawTargetDims(agg::int8u* buf, int width, int height, int stride)
{
	attach(buf, (unsigned)width, (unsigned)height, stride);
}

void NativeDrawTarget::attach(unsigned char* buf, unsigned width, unsigned height, int stride)
{
	if (m_graphics) m_graphics->Flush();
	ReleaseScratch();
	ReleaseSurface();
	m_pixels = buf; m_width = (int)width; m_height = (int)height; m_stride = stride;
	m_rbuf.attach(buf, width, height, stride);
	m_clip = Agg2DBase::RectD(0, 0, width, height);
	m_hasClip = false;
}

agg::rendering_buffer& NativeDrawTarget::buf()
{
	if (m_graphics) m_graphics->Flush();
	dirty();
	return m_rbuf;
}

void NativeDrawTarget::clipBox(double x1, double y1, double x2, double y2)
{
	m_clip = Agg2DBase::RectD(x1, y1, x2, y2);
	m_hasClip = true;
	ApplyClip();
}

Agg2DBase::RectD NativeDrawTarget::clipBox() const { return m_clip; }

void NativeDrawTarget::clearAll(AggColor c) { clearAll(c.r, c.g, c.b, c.a); }
void NativeDrawTarget::clearAll(unsigned r, unsigned g, unsigned b, unsigned a)
{
	if (!m_pixels) return;
	if (m_graphics) m_graphics->Flush();
	const unsigned char rr = (unsigned char)(s_premultipliedOutput ? (r * a) / 255 : r);
	const unsigned char gg = (unsigned char)(s_premultipliedOutput ? (g * a) / 255 : g);
	const unsigned char bb = (unsigned char)(s_premultipliedOutput ? (b * a) / 255 : b);
	const unsigned char px[4] = { bb, gg, rr, (unsigned char)a };
	const size_t rowBytes = (size_t)(m_stride < 0 ? -m_stride : m_stride);
	for (int y = 0; y < m_height; ++y)
	{
		unsigned char* row = m_pixels + (size_t)y * (size_t)m_stride;
		for (int x = 0; x < m_width; ++x) memcpy(row + x * 4, px, 4);
	}
	(void)rowBytes;
	dirty();
}

void NativeDrawTarget::clearClipBox(AggColor c) { clearClipBox(c.r, c.g, c.b, c.a); }
void NativeDrawTarget::clearClipBox(unsigned r, unsigned g, unsigned b, unsigned a)
{
	if (!m_pixels) return;
	if (m_graphics) m_graphics->Flush();
	int x1 = m_hasClip ? (int)m_clip.x1 : 0, y1 = m_hasClip ? (int)m_clip.y1 : 0;
	int x2 = m_hasClip ? (int)m_clip.x2 : m_width, y2 = m_hasClip ? (int)m_clip.y2 : m_height;
	x1 = std::max(0, x1); y1 = std::max(0, y1);
	x2 = std::min(m_width, x2); y2 = std::min(m_height, y2);
	const unsigned char rr = (unsigned char)(s_premultipliedOutput ? (r * a) / 255 : r);
	const unsigned char gg = (unsigned char)(s_premultipliedOutput ? (g * a) / 255 : g);
	const unsigned char bb = (unsigned char)(s_premultipliedOutput ? (b * a) / 255 : b);
	const unsigned char px[4] = { bb, gg, rr, (unsigned char)a };
	for (int y = y1; y < y2; ++y)
		for (int x = x1; x < x2; ++x)
			memcpy(m_pixels + (size_t)y * (size_t)m_stride + (size_t)x * 4, px, 4);
	dirty();
}

// ---------------------------------------------------------------------------
// conversions
// ---------------------------------------------------------------------------
void NativeDrawTarget::worldToScreen(double& x, double& y) const { TransformPoint(x, y); }
void NativeDrawTarget::screenToWorld(double& x, double& y) const
{
	const double det = m_xform[0] * m_xform[3] - m_xform[1] * m_xform[2];
	if (fabs(det) < 1e-12) return;
	const double dx = x - m_xform[4], dy = y - m_xform[5];
	x = (dx * m_xform[3] - dy * m_xform[2]) / det;
	y = (dy * m_xform[0] - dx * m_xform[1]) / det;
}
double NativeDrawTarget::worldToScreen(double scalar) const { return scalar * m_xform[0]; }
double NativeDrawTarget::screenToWorld(double scalar) const
{
	return fabs(m_xform[0]) < 1e-12 ? scalar : scalar / m_xform[0];
}
void NativeDrawTarget::alignPoint(double&, double&) const {}
bool NativeDrawTarget::inBox(double wx, double wy) const
{
	return wx >= m_clip.x1 && wx <= m_clip.x2 && wy >= m_clip.y1 && wy <= m_clip.y2;
}

// ---------------------------------------------------------------------------
// attributes
// ---------------------------------------------------------------------------
void NativeDrawTarget::blendMode(Agg2DBase::BlendMode m)
{
	m_blendMode = m;
	// BlendAlpha / BlendSrcOver use GDI+'s native SourceOver; everything else
	// is routed through the scratch surface in BeginPrimitive().
	if (m_graphics) m_graphics->SetCompositingMode(CompositingModeSourceOver);
}
Agg2DBase::BlendMode NativeDrawTarget::blendMode() { return m_blendMode; }
void NativeDrawTarget::imageBlendMode(Agg2DBase::BlendMode m) { m_imageBlendMode = m; }
Agg2DBase::BlendMode NativeDrawTarget::imageBlendMode() { return m_imageBlendMode; }
void NativeDrawTarget::imageBlendColor(AggColor c) { m_imageBlendColor = c; }
void NativeDrawTarget::imageBlendColor(unsigned r, unsigned g, unsigned b, unsigned a) { m_imageBlendColor = AggColor(r, g, b, a); }
AggColor NativeDrawTarget::imageBlendColor() { return m_imageBlendColor; }
void NativeDrawTarget::masterAlpha(double a) { m_masterAlpha = a; }
double NativeDrawTarget::masterAlpha() { return m_masterAlpha; }
void NativeDrawTarget::antiAliasGamma(double g) { m_antiAliasGamma = g; }
double NativeDrawTarget::antiAliasGamma() { return m_antiAliasGamma; }

void NativeDrawTarget::fillColor(AggColor c) { m_fill = c; m_hasFill = true; m_fillGradient.kind = 0; }
void NativeDrawTarget::fillColor(unsigned r, unsigned g, unsigned b, unsigned a) { fillColor(AggColor(r, g, b, a)); }
void NativeDrawTarget::noFill() { m_hasFill = false; }
void NativeDrawTarget::lineColor(AggColor c) { m_line = c; m_hasLine = true; m_lineGradient.kind = 0; }
void NativeDrawTarget::lineColor(unsigned r, unsigned g, unsigned b, unsigned a) { lineColor(AggColor(r, g, b, a)); }
void NativeDrawTarget::noLine() { m_hasLine = false; }
AggColor NativeDrawTarget::fillColor() { return m_fill; }
AggColor NativeDrawTarget::lineColor() { return m_line; }

void NativeDrawTarget::lineWidth(double w) { m_lineWidth = w < 0.01 ? 0.01 : w; }
double NativeDrawTarget::lineWidth() { return m_lineWidth; }
void NativeDrawTarget::lineCap(Agg2DBase::LineCap cap) { m_lineCap = cap; }
Agg2DBase::LineCap NativeDrawTarget::lineCap() { return m_lineCap; }
void NativeDrawTarget::lineJoin(Agg2DBase::LineJoin join) { m_lineJoin = join; }
Agg2DBase::LineJoin NativeDrawTarget::lineJoin() { return m_lineJoin; }
void NativeDrawTarget::fillEvenOdd(bool f) { m_fillEvenOdd = f; }
bool NativeDrawTarget::fillEvenOdd() { return m_fillEvenOdd; }

void NativeDrawTarget::imageFilter(Agg2DBase::ImageFilter f) { m_imageFilter = f; }
Agg2DBase::ImageFilter NativeDrawTarget::imageFilter() { return m_imageFilter; }
void NativeDrawTarget::imageResample(Agg2DBase::ImageResample f) { m_imageResample = f; }
Agg2DBase::ImageResample NativeDrawTarget::imageResample() { return m_imageResample; }

void NativeDrawTarget::ConfigurePen(Pen& pen)
{
	LineCap cap = LineCapRound;
	if (m_lineCap == Agg2DBase::CAP_BUTT) cap = LineCapFlat;
	else if (m_lineCap == Agg2DBase::CAP_SQUARE) cap = LineCapSquare;
	LineJoin join = LineJoinRound;
	if (m_lineJoin == Agg2DBase::JOIN_MITER) join = LineJoinMiter;
	else if (m_lineJoin == Agg2DBase::JOIN_BEVEL) join = LineJoinBevel;
	pen.SetLineCap(cap, cap, DashCapFlat);
	pen.SetLineJoin(join);
	pen.SetAlignment(PenAlignmentCenter);
}

Brush* NativeDrawTarget::MakeBrush(bool useLineColor)
{
	const GradientState& g = useLineColor ? m_lineGradient : m_fillGradient;
	const AggColor solid = useLineColor ? m_line : m_fill;
	if (g.kind == 0)
		return new SolidBrush(ToGdiColor(solid, m_masterAlpha));

	if (g.kind == 1)
	{
		PointF p1((REAL)g.x1, (REAL)g.y1), p2((REAL)g.x2, (REAL)g.y2);
		// GDI+ requires distinct endpoints.
		if (fabs(g.x1 - g.x2) < 1e-4 && fabs(g.y1 - g.y2) < 1e-4) p2.X += 1.0f;
		LinearGradientBrush* b = new LinearGradientBrush(p1, p2,
			ToGdiColor(g.c1, m_masterAlpha), ToGdiColor(g.c2, m_masterAlpha));
		b->SetWrapMode(WrapModeClamp);
		// Agg2D builds a 256 entry LUT: c1 for [0,t0], lerp to c2 on [t0,t1],
		// c2 afterwards.  With the default profile=1 this is plain linear.
		REAL t0 = (REAL)((128.0 - g.profile * 127.0) / 255.0);
		REAL t1 = (REAL)((128.0 + g.profile * 127.0) / 255.0);
		if (t0 < 0) t0 = 0; if (t0 > 1) t0 = 1;
		if (t1 < t0) t1 = t0; if (t1 > 1) t1 = 1;
		Color cols[4] = { ToGdiColor(g.c1, m_masterAlpha), ToGdiColor(g.c1, m_masterAlpha),
		                  ToGdiColor(g.c2, m_masterAlpha), ToGdiColor(g.c2, m_masterAlpha) };
		REAL pos[4] = { 0.0f, t0, t1, 1.0f };
		b->SetInterpolationColors(cols, pos, 4);
		return b;
	}

	// radial: a path gradient over the circle (x1,y1,r=x2)
	GraphicsPath gp;
	const REAL r = (REAL)(g.x2 > 0 ? g.x2 : 1);
	gp.AddEllipse((REAL)(g.x1 - r), (REAL)(g.y1 - r), 2 * r, 2 * r);
	PathGradientBrush* b = new PathGradientBrush(&gp);
	b->SetCenterColor(ToGdiColor(g.c1, m_masterAlpha));
	Color surround = ToGdiColor(g.c2, m_masterAlpha);
	INT count = 1;
	b->SetSurroundColors(&surround, &count);
	return b;
}

void NativeDrawTarget::PaintPath(GraphicsPath& path, Agg2DBase::DrawPathFlag flag)
{
	Graphics* g = BeginPrimitive();
	if (!g) return;
	path.SetFillMode(m_fillEvenOdd ? FillModeAlternate : FillModeWinding);
	const bool doFill = (flag == Agg2DBase::FillOnly || flag == Agg2DBase::FillAndStroke ||
	                     flag == Agg2DBase::FillWithLineColor);
	const bool doStroke = (flag == Agg2DBase::StrokeOnly || flag == Agg2DBase::FillAndStroke);
	if (doFill && m_hasFill)
	{
		Brush* b = MakeBrush(flag == Agg2DBase::FillWithLineColor);
		if (b) { g->FillPath(b, &path); delete b; }
	}
	if (doStroke && m_hasLine)
	{
		Pen pen(ToGdiColor(m_line, m_masterAlpha), (REAL)m_lineWidth);
		ConfigurePen(pen);
		g->DrawPath(&pen, &path);
	}
	EndPrimitive();
}

// ---------------------------------------------------------------------------
// gradients
// ---------------------------------------------------------------------------
void NativeDrawTarget::fillLinearGradient(double x1, double y1, double x2, double y2, AggColor c1, AggColor c2, double profile)
{
	m_fillGradient.kind = 1;
	m_fillGradient.x1 = x1; m_fillGradient.y1 = y1; m_fillGradient.x2 = x2; m_fillGradient.y2 = y2;
	m_fillGradient.c1 = c1; m_fillGradient.c2 = c2; m_fillGradient.profile = profile;
	m_hasFill = true;
}
void NativeDrawTarget::lineLinearGradient(double x1, double y1, double x2, double y2, AggColor c1, AggColor c2, double profile)
{
	m_lineGradient.kind = 1;
	m_lineGradient.x1 = x1; m_lineGradient.y1 = y1; m_lineGradient.x2 = x2; m_lineGradient.y2 = y2;
	m_lineGradient.c1 = c1; m_lineGradient.c2 = c2; m_lineGradient.profile = profile;
	m_hasLine = true;
}
void NativeDrawTarget::fillRadialGradient(double x, double y, double r, AggColor c1, AggColor c2, double profile)
{
	m_fillGradient.kind = 2;
	m_fillGradient.x1 = x; m_fillGradient.y1 = y; m_fillGradient.x2 = r;
	m_fillGradient.c1 = c1; m_fillGradient.c2 = c2; m_fillGradient.profile = profile;
	m_hasFill = true;
}
void NativeDrawTarget::lineRadialGradient(double x, double y, double r, AggColor c1, AggColor c2, double profile)
{
	m_lineGradient.kind = 2;
	m_lineGradient.x1 = x; m_lineGradient.y1 = y; m_lineGradient.x2 = r;
	m_lineGradient.c1 = c1; m_lineGradient.c2 = c2; m_lineGradient.profile = profile;
	m_hasLine = true;
}
void NativeDrawTarget::fillRadialGradient(double x, double y, double r, AggColor c1, AggColor c2, AggColor c3)
{
	fillRadialGradient(x, y, r, c1, c3, 1.0);
	m_fillGradient.c3 = c2;
}
void NativeDrawTarget::lineRadialGradient(double x, double y, double r, AggColor c1, AggColor c2, AggColor c3)
{
	lineRadialGradient(x, y, r, c1, c3, 1.0);
	m_lineGradient.c3 = c2;
}
void NativeDrawTarget::fillRadialGradient(double, double, double) { m_fillGradient.kind = 0; }
void NativeDrawTarget::lineRadialGradient(double, double, double) { m_lineGradient.kind = 0; }

// ---------------------------------------------------------------------------
// transformations
// ---------------------------------------------------------------------------
Agg2DBase::Transformations NativeDrawTarget::transformations()
{
	Agg2DBase::Transformations t;
	for (int i = 0; i < 6; ++i) t.affineMatrix[i] = m_xform[i];
	return t;
}
void NativeDrawTarget::transformations(const Agg2DBase::Transformations& tr) { matrix(tr); }
const Agg2DBase::Affine& NativeDrawTarget::affine() { return m_affine; }
void NativeDrawTarget::resetTransformations()
{
	MatIdentity(m_affineX);
	MatIdentity(m_viewportX);
	ComposeTransform();
}
void NativeDrawTarget::matrix(const Agg2DBase::Affine& tr)
{
	const double m[6] = { tr.sx, tr.shy, tr.shx, tr.sy, tr.tx, tr.ty };
	for (int i = 0; i < 6; ++i) m_affineX[i] = m[i];
	ComposeTransform();
}
void NativeDrawTarget::matrix(const Agg2DBase::Transformations& tr)
{
	for (int i = 0; i < 6; ++i) m_affineX[i] = tr.affineMatrix[i];
	MatIdentity(m_viewportX);
	m_xform[0] = m_affineX[0]; m_xform[1] = m_affineX[1];
	m_xform[2] = m_affineX[2]; m_xform[3] = m_affineX[3];
	m_xform[4] = m_affineX[4]; m_xform[5] = m_affineX[5];
	m_affine = Agg2DBase::Affine(m_affineX);
}
void NativeDrawTarget::rotate(double angle) { PremultiplyAffine(cos(angle), sin(angle), -sin(angle), cos(angle), 0, 0); }
void NativeDrawTarget::rotate(double angle, double cx, double cy)
{
	// Agg2D builds T(-c) * R * T(c) as a single matrix and premultiplies it.
	const double t1[6] = { 1, 0, 0, 1, -cx, -cy };
	const double r[6] = { cos(angle), sin(angle), -sin(angle), cos(angle), 0, 0 };
	const double t2[6] = { 1, 0, 0, 1, cx, cy };
	double tmp[6], m[6];
	MatMul(t1, r, tmp);
	MatMul(tmp, t2, m);
	PremultiplyAffine(m[0], m[1], m[2], m[3], m[4], m[5]);
}
void NativeDrawTarget::scale(double s) { scale(s, s); }
void NativeDrawTarget::scale(double sx, double sy) { PremultiplyAffine(sx, 0, 0, sy, 0, 0); }
void NativeDrawTarget::skew(double sx, double sy) { PremultiplyAffine(1, tan(sy), tan(sx), 1, 0, 0); }
void NativeDrawTarget::translate(double x, double y) { PremultiplyAffine(1, 0, 0, 1, x, y); }

void NativeDrawTarget::parallelogram(double x1, double y1, double x2, double y2, const double* para)
{
	if (!para) return;
	// agg::trans_affine(x1,y1,x2,y2,parl) == inverse(rect) * parallelogram.
	const double rect[6] = { x2 - x1, 0.0, x2 - x1, y2 - y1, x1, y1 };
	const double parl[6] = {
		para[2] - para[0], para[3] - para[1],
		para[4] - para[0], para[5] - para[1],
		para[0], para[1]
	};
	double inv[6], m[6];
	MatInvert(rect, inv);
	MatMul(inv, parl, m);
	PremultiplyAffine(m[0], m[1], m[2], m[3], m[4], m[5]);
}

void NativeDrawTarget::viewport(double worldX1, double worldY1, double worldX2, double worldY2,
	double screenX1, double screenY1, double screenX2, double screenY2,
	Agg2DBase::ViewportOption opt, Agg2DBase::WindowFitLogic fl)
{
	// Faithful port of agg::trans_viewport::update() / to_affine().
	const double epsilon = 1e-30;
	double wx1 = worldX1, wy1 = worldY1, wx2 = worldX2, wy2 = worldY2;
	double kx = 1.0, ky = 1.0;
	if (fabs(worldX1 - worldX2) < epsilon || fabs(worldY1 - worldY2) < epsilon ||
	    fabs(screenX1 - screenX2) < epsilon || fabs(screenY1 - screenY2) < epsilon)
	{
		wx2 = wx1 + 1.0; wy2 = wy1 + 1.0;
	}
	else
	{
		double ax = 0.5, ay = 0.5;
		switch (opt)
		{
		case Agg2DBase::XMinYMin: ax = 0.0; ay = 0.0; break;
		case Agg2DBase::XMidYMin: ax = 0.5; ay = 0.0; break;
		case Agg2DBase::XMaxYMin: ax = 1.0; ay = 0.0; break;
		case Agg2DBase::XMinYMid: ax = 0.0; ay = 0.5; break;
		case Agg2DBase::XMidYMid: ax = 0.5; ay = 0.5; break;
		case Agg2DBase::XMaxYMid: ax = 1.0; ay = 0.5; break;
		case Agg2DBase::XMinYMax: ax = 0.0; ay = 1.0; break;
		case Agg2DBase::XMidYMax: ax = 0.5; ay = 1.0; break;
		case Agg2DBase::XMaxYMax: ax = 1.0; ay = 1.0; break;
		default: break;  // Anisotropic: no letterboxing
		}
		const bool stretch = (opt == Agg2DBase::Anisotropic);
		const double sx0 = (screenX2 - screenX1) / (worldX2 - worldX1);
		const double sy0 = (screenY2 - screenY1) / (worldY2 - worldY1);
		if (!stretch)
		{
			const bool useMeet = (fl == Agg2DBase::WindowFitLogic_meet);
			if (useMeet == (sx0 < sy0))
			{
				const double d = (worldY2 - worldY1) * sy0 / sx0;
				wy1 += (worldY2 - worldY1 - d) * ay;
				wy2 = wy1 + d;
			}
			else
			{
				const double d = (worldX2 - worldX1) * sx0 / sy0;
				wx1 += (worldX2 - worldX1 - d) * ax;
				wx2 = wx1 + d;
			}
		}
		kx = (screenX2 - screenX1) / (wx2 - wx1);
		ky = (screenY2 - screenY1) / (wy2 - wy1);
	}
	// to_affine() = T(-w) * S(k) * T(d)
	m_viewportX[0] = kx; m_viewportX[1] = 0.0;
	m_viewportX[2] = 0.0; m_viewportX[3] = ky;
	m_viewportX[4] = screenX1 - wx1 * kx;
	m_viewportX[5] = screenY1 - wy1 * ky;
	ComposeTransform();
}

// ---------------------------------------------------------------------------
// shapes
// ---------------------------------------------------------------------------
void NativeDrawTarget::line(double x1, double y1, double x2, double y2)
{
	if (!m_hasLine) return;
	Graphics* g = BeginPrimitive();
	if (!g) return;
	TransformPoint(x1, y1); TransformPoint(x2, y2);
	Pen pen(ToGdiColor(m_line, m_masterAlpha), (REAL)m_lineWidth);
	ConfigurePen(pen);
	g->DrawLine(&pen, PointF((REAL)x1, (REAL)y1), PointF((REAL)x2, (REAL)y2));
	EndPrimitive();
}

void NativeDrawTarget::triangle(double x1, double y1, double x2, double y2, double x3, double y3)
{
	double xy[6] = { x1, y1, x2, y2, x3, y3 };
	polygon(xy, 3);
}

void NativeDrawTarget::rectangle(double x1, double y1, double x2, double y2)
{
	double xy[8] = { x1, y1, x2, y1, x2, y2, x1, y2 };
	polygon(xy, 4);
}

void NativeDrawTarget::roundedRect(double x1, double y1, double x2, double y2, double r)
{
	roundedRect(x1, y1, x2, y2, r, r);
}

void NativeDrawTarget::roundedRect(double x1, double y1, double x2, double y2, double rx, double ry)
{
	roundedRect(x1, y1, x2, y2, rx, ry, rx, ry);
}

void NativeDrawTarget::roundedRect(double x1, double y1, double x2, double y2,
                                   double rxBottom, double ryBottom, double rxTop, double ryTop)
{
	if (!Touch()) return;
	double a1 = x1, b1 = y1, a2 = x2, b2 = y2;
	TransformPoint(a1, b1); TransformPoint(a2, b2);
	GraphicsPath path;
	BuildRoundedRect(path, a1, b1, a2, b2,
		rxTop, ryTop, rxTop, ryTop, rxBottom, ryBottom, rxBottom, ryBottom);
	PaintPath(path, Agg2DBase::FillAndStroke);
}

void NativeDrawTarget::ellipse(double cx, double cy, double rx, double ry)
{
	if (!Touch()) return;
	TransformPoint(cx, cy);
	GraphicsPath path;
	path.AddEllipse((REAL)(cx - rx), (REAL)(cy - ry), (REAL)(2 * rx), (REAL)(2 * ry));
	PaintPath(path, Agg2DBase::FillAndStroke);
}

void NativeDrawTarget::arc(double cx, double cy, double rx, double ry, double start, double sweep)
{
	// Agg2D uses bezier_arc + drawPath(StrokeOnly); route through our own
	// path machinery so arcTo/blend mode/transform all behave consistently.
	resetPath();
	AppendEllipseArc(cx, cy, rx, ry, start, sweep, true);
	drawPath(Agg2DBase::StrokeOnly);
}

void NativeDrawTarget::star(double cx, double cy, double r1, double r2, double startAngle, int numRays)
{
	if (!Touch()) return;
	if (numRays < 1) return;
	std::vector<PointF> pts;
	const double da = 3.14159265358979323846 / (double)numRays;
	double a = startAngle;
	for (int i = 0; i < numRays; ++i)
	{
		double x = cos(a) * r2 + cx, y = sin(a) * r2 + cy;
		TransformPoint(x, y);
		pts.push_back(PointF((REAL)x, (REAL)y));
		a += da;
		x = cos(a) * r1 + cx; y = sin(a) * r1 + cy;
		TransformPoint(x, y);
		pts.push_back(PointF((REAL)x, (REAL)y));
		a += da;
	}
	GraphicsPath path;
	path.AddPolygon(&pts[0], (INT)pts.size());
	PaintPath(path, Agg2DBase::FillAndStroke);
}

void NativeDrawTarget::curve(double x1, double y1, double x2, double y2, double x3, double y3)
{
	resetPath(); moveTo(x1, y1); quadricCurveTo(x2, y2, x3, y3);
	drawPath(Agg2DBase::StrokeOnly);
}

void NativeDrawTarget::curve(double x1, double y1, double x2, double y2,
                             double x3, double y3, double x4, double y4)
{
	resetPath(); moveTo(x1, y1); cubicCurveTo(x2, y2, x3, y3, x4, y4);
	drawPath(Agg2DBase::StrokeOnly);
}

void NativeDrawTarget::polygon(double* xy, int numPoints)
{
	if (!Touch() || !xy || numPoints < 2) return;
	std::vector<PointF> pts((size_t)numPoints);
	for (int i = 0; i < numPoints; ++i)
	{
		double x = xy[i * 2], y = xy[i * 2 + 1];
		TransformPoint(x, y);
		pts[i] = PointF((REAL)x, (REAL)y);
	}
	GraphicsPath path;
	path.AddPolygon(&pts[0], (INT)numPoints);
	PaintPath(path, Agg2DBase::FillAndStroke);
}

void NativeDrawTarget::polyline(double* xy, int numPoints)
{
	if (!Touch() || !xy || numPoints < 2) return;
	std::vector<PointF> pts((size_t)numPoints);
	for (int i = 0; i < numPoints; ++i)
	{
		double x = xy[i * 2], y = xy[i * 2 + 1];
		TransformPoint(x, y);
		pts[i] = PointF((REAL)x, (REAL)y);
	}
	GraphicsPath path;
	path.AddLines(&pts[0], (INT)numPoints);
	PaintPath(path, Agg2DBase::StrokeOnly);
}

// ---------------------------------------------------------------------------
// path commands
// ---------------------------------------------------------------------------
void NativeDrawTarget::resetPath()
{
	m_pathPoints.clear(); m_pathTypes.clear();
	m_pathCursor.x = m_pathCursor.y = 0.0;
}
void NativeDrawTarget::PathMoveTo(double x, double y)
{
	m_pathPoints.push_back(Pt()); m_pathPoints.back().x = x; m_pathPoints.back().y = y;
	m_pathTypes.push_back((unsigned char)PathPointTypeStart);
	m_pathCursor.x = x; m_pathCursor.y = y;
}
void NativeDrawTarget::PathLineTo(double x, double y)
{
	if (m_pathPoints.empty()) { PathMoveTo(x, y); return; }
	m_pathPoints.push_back(Pt()); m_pathPoints.back().x = x; m_pathPoints.back().y = y;
	m_pathTypes.push_back((unsigned char)PathPointTypeLine);
	m_pathCursor.x = x; m_pathCursor.y = y;
}
void NativeDrawTarget::PathBezierTo(double x1, double y1, double x2, double y2, double x3, double y3)
{
	if (m_pathPoints.empty()) PathMoveTo(x1, y1);
	double p[6] = { x1, y1, x2, y2, x3, y3 };
	for (int i = 0; i < 3; ++i)
	{
		m_pathPoints.push_back(Pt());
		m_pathPoints.back().x = p[i * 2]; m_pathPoints.back().y = p[i * 2 + 1];
		m_pathTypes.push_back((unsigned char)PathPointTypeBezier);
	}
	m_pathCursor.x = x3; m_pathCursor.y = y3;
}
void NativeDrawTarget::moveTo(double x, double y) { PathMoveTo(x, y); }
void NativeDrawTarget::moveRel(double dx, double dy) { PathMoveTo(m_pathCursor.x + dx, m_pathCursor.y + dy); }
void NativeDrawTarget::lineTo(double x, double y) { PathLineTo(x, y); }
void NativeDrawTarget::lineRel(double dx, double dy) { PathLineTo(m_pathCursor.x + dx, m_pathCursor.y + dy); }
void NativeDrawTarget::horLineTo(double x) { PathLineTo(x, m_pathCursor.y); }
void NativeDrawTarget::horLineRel(double dx) { PathLineTo(m_pathCursor.x + dx, m_pathCursor.y); }
void NativeDrawTarget::verLineTo(double y) { PathLineTo(m_pathCursor.x, y); }
void NativeDrawTarget::verLineRel(double dy) { PathLineTo(m_pathCursor.x, m_pathCursor.y + dy); }
// SVG-style endpoint-to-centre elliptical arc, flattened into cubic beziers.
// Matches agg::bezier_arc / agg::path_storage::arc_to semantics.
void NativeDrawTarget::AppendArcToPath(double x1, double y1, double rx, double ry,
	double angle, bool largeArcFlag, bool sweepFlag, double x2, double y2)
{
	rx = fabs(rx); ry = fabs(ry);
	const double dx2 = (x1 - x2) * 0.5, dy2 = (y1 - y2) * 0.5;
	const double cosPhi = cos(angle), sinPhi = sin(angle);
	const double x1p = cosPhi * dx2 + sinPhi * dy2;
	const double y1p = -sinPhi * dx2 + cosPhi * dy2;
	if (rx < 1e-12 || ry < 1e-12) { PathLineTo(x2, y2); return; }
	double lambda = (x1p * x1p) / (rx * rx) + (y1p * y1p) / (ry * ry);
	if (lambda > 1.0)
	{
		const double s = sqrt(lambda);
		rx *= s; ry *= s; lambda = 1.0;
	}
	const double sign = (largeArcFlag == sweepFlag) ? -1.0 : 1.0;
	const double rx2 = rx * rx, ry2 = ry * ry;
	const double num = rx2 * ry2 - rx2 * y1p * y1p - ry2 * x1p * x1p;
	const double den = rx2 * y1p * y1p + ry2 * x1p * x1p;
	const double co = (den > 0.0) ? sign * sqrt((num > 0.0 ? num : 0.0) / den) : 0.0;
	const double cxp = co * (rx * y1p / ry);
	const double cyp = co * (-ry * x1p / rx);
	const double cx = cosPhi * cxp - sinPhi * cyp + (x1 + x2) * 0.5;
	const double cy = sinPhi * cxp + cosPhi * cyp + (y1 + y2) * 0.5;

	const double ux = (x1p - cxp) / rx, uy = (y1p - cyp) / ry;
	const double vx = (-x1p - cxp) / rx, vy = (-y1p - cyp) / ry;
	const double theta1 = atan2(uy, ux);
	double delta = atan2(ux * vy - uy * vx, ux * vx + uy * vy);
	if (!sweepFlag && delta > 0.0) delta -= 2.0 * 3.14159265358979323846;
	if (sweepFlag && delta < 0.0) delta += 2.0 * 3.14159265358979323846;

	const int segments = (int)ceil(fabs(delta) / (3.14159265358979323846 * 0.5));
	const int n = segments < 1 ? 1 : (segments > 64 ? 64 : segments);
	for (int i = 0; i < n; ++i)
	{
		const double t1 = theta1 + delta * i / n;
		const double t2 = theta1 + delta * (i + 1) / n;
		const double dt = t2 - t1;
		const double k = (4.0 / 3.0) * tan(dt * 0.25);
		const double c1 = cos(t1), s1 = sin(t1), c2 = cos(t2), s2 = sin(t2);
		// E(t) and E'(t) rotated by angle
		const double ex1 = cx + rx * cosPhi * c1 - ry * sinPhi * s1;
		const double ey1 = cy + rx * sinPhi * c1 + ry * cosPhi * s1;
		const double dx1 = -rx * cosPhi * s1 - ry * sinPhi * c1;
		const double dy1 = -rx * sinPhi * s1 + ry * cosPhi * c1;
		const double ex2 = cx + rx * cosPhi * c2 - ry * sinPhi * s2;
		const double ey2 = cy + rx * sinPhi * c2 + ry * cosPhi * s2;
		const double dx2e = -rx * cosPhi * s2 - ry * sinPhi * c2;
		const double dy2e = -rx * sinPhi * s2 + ry * cosPhi * c2;
		PathBezierTo(ex1 + k * dx1, ey1 + k * dy1,
		             ex2 - k * dx2e, ey2 - k * dy2e, ex2, ey2);
	}
}

// Axis-aligned (rotated by m_xform at draw time) elliptical arc from start
// angle through sweep radians, flattened into cubic beziers.
void NativeDrawTarget::AppendEllipseArc(double cx, double cy, double rx, double ry,
	double start, double sweep, bool moveFirst)
{
	if (rx <= 0.0 || ry <= 0.0) return;
	const double pi = 3.14159265358979323846;
	const double e0x = cx + rx * cos(start), e0y = cy + ry * sin(start);
	if (moveFirst) PathMoveTo(e0x, e0y);
	else PathLineTo(e0x, e0y);
	int n = (int)ceil(fabs(sweep) / (pi * 0.5));
	if (n < 1) n = 1;
	if (n > 64) n = 64;
	for (int i = 0; i < n; ++i)
	{
		const double t1 = start + sweep * i / n;
		const double t2 = start + sweep * (i + 1) / n;
		const double dt = t2 - t1;
		const double k = (4.0 / 3.0) * tan(dt * 0.25);
		const double c1 = cos(t1), s1 = sin(t1), c2 = cos(t2), s2 = sin(t2);
		PathBezierTo(cx + rx * c1 - k * rx * s1, cy + ry * s1 + k * ry * c1,
		             cx + rx * c2 + k * rx * s2, cy + ry * s2 - k * ry * c2,
		             cx + rx * c2, cy + ry * s2);
	}
}

void NativeDrawTarget::arcTo(double rx, double ry, double angle,
	bool largeArcFlag, bool sweepFlag, double x, double y)
{
	const double x1 = m_pathCursor.x, y1 = m_pathCursor.y;
	if (m_pathPoints.empty()) PathMoveTo(x1, y1);
	AppendArcToPath(x1, y1, rx, ry, angle, largeArcFlag, sweepFlag, x, y);
}
void NativeDrawTarget::arcRel(double rx, double ry, double angle,
	bool largeArcFlag, bool sweepFlag, double dx, double dy)
{
	arcTo(rx, ry, angle, largeArcFlag, sweepFlag,
	      m_pathCursor.x + dx, m_pathCursor.y + dy);
}
void NativeDrawTarget::quadricCurveTo(double xc, double yc, double xt, double yt)
{
	const double px = m_pathCursor.x, py = m_pathCursor.y;
	PathBezierTo(px + (2.0 / 3.0) * (xc - px), py + (2.0 / 3.0) * (yc - py),
	             xt + (2.0 / 3.0) * (xc - xt), yt + (2.0 / 3.0) * (yc - yt), xt, yt);
}
void NativeDrawTarget::quadricCurveRel(double dxc, double dyc, double dxt, double dyt)
{
	quadricCurveTo(m_pathCursor.x + dxc, m_pathCursor.y + dyc,
	               m_pathCursor.x + dxt, m_pathCursor.y + dyt);
}
void NativeDrawTarget::quadricCurveTo(double xTo, double yTo)
{
	quadricCurveTo(m_pathCursor.x, m_pathCursor.y, xTo, yTo);
}
void NativeDrawTarget::quadricCurveRel(double dxTo, double dyTo)
{
	quadricCurveTo(m_pathCursor.x, m_pathCursor.y, m_pathCursor.x + dxTo, m_pathCursor.y + dyTo);
}
void NativeDrawTarget::cubicCurveTo(double x1, double y1, double x2, double y2, double x3, double y3)
{
	PathBezierTo(x1, y1, x2, y2, x3, y3);
}
void NativeDrawTarget::cubicCurveRel(double x1, double y1, double x2, double y2, double x3, double y3)
{
	const double px = m_pathCursor.x, py = m_pathCursor.y;
	PathBezierTo(px + x1, py + y1, px + x2, py + y2, px + x3, py + y3);
}
void NativeDrawTarget::cubicCurveTo(double x2, double y2, double xTo, double yTo)
{
	// smooth cubic: reflect the previous control point about the current point
	double c1x = m_pathCursor.x, c1y = m_pathCursor.y;
	if (m_pathPoints.size() >= 2 && (m_pathTypes.back() & PathPointTypePathTypeMask) == PathPointTypeBezier)
	{
		const Pt& prev = m_pathPoints[m_pathPoints.size() - 2];
		c1x = 2 * m_pathCursor.x - prev.x;
		c1y = 2 * m_pathCursor.y - prev.y;
	}
	PathBezierTo(c1x, c1y, x2, y2, xTo, yTo);
}
void NativeDrawTarget::cubicCurveRel(double x2, double y2, double dxTo, double dyTo)
{
	const double px = m_pathCursor.x, py = m_pathCursor.y;
	cubicCurveTo(px + x2, py + y2, px + dxTo, py + dyTo);
}
void NativeDrawTarget::addEllipse(double cx, double cy, double rx, double ry, Agg2DBase::Direction dir)
{
	// Agg2D: bezier_arc(cx,cy,rx,ry,0,(dir==CCW) ? 2*pi : -2*pi)
	const double sweep = (dir == Agg2DBase::CCW) ? 2.0 * 3.14159265358979323846
	                                             : -2.0 * 3.14159265358979323846;
	AppendEllipseArc(cx, cy, rx, ry, 0.0, sweep, m_pathPoints.empty());
}
void NativeDrawTarget::closePolygon()
{
	if (!m_pathTypes.empty()) m_pathTypes.back() |= (unsigned char)PathPointTypeCloseSubpath;
}
void NativeDrawTarget::drawPath(Agg2DBase::DrawPathFlag flag)
{
	if (!Touch() || m_pathPoints.empty()) return;
	std::vector<PointF> pts(m_pathPoints.size());
	for (size_t i = 0; i < m_pathPoints.size(); ++i)
	{
		double x = m_pathPoints[i].x, y = m_pathPoints[i].y;
		TransformPoint(x, y);
		pts[i] = PointF((REAL)x, (REAL)y);
	}
	GraphicsPath path(&pts[0], &m_pathTypes[0], (INT)pts.size(),
		m_fillEvenOdd ? FillModeAlternate : FillModeWinding);
	PaintPath(path, flag);
}

// ---------------------------------------------------------------------------
// transformImage (interface) / CPU compositor (display.cpp)
// ---------------------------------------------------------------------------
void NativeDraw_CompositeHUD(unsigned char* dst, int dstW, int dstH, int dstStride,
	const unsigned char* hud, int hudW, int hudH, int hudStride,
	bool srcPremultiplied)
{
	if (!dst || !hud || dstW <= 0 || dstH <= 0 || hudW <= 0 || hudH <= 0) return;
	const int hudAbsStride = hudStride < 0 ? -hudStride : hudStride;
	for (int y = 0; y < dstH; ++y)
	{
		unsigned char* drow = dst + (size_t)y * (size_t)dstStride;
		const int sy = (int)((int64_t)y * hudH / dstH);
		const unsigned char* srow = hud + (size_t)sy * (size_t)hudAbsStride;
		for (int x = 0; x < dstW; ++x)
		{
			const int sx = (int)((int64_t)x * hudW / dstW);
			const unsigned char* s = srow + (size_t)sx * 4;
			const int a = s[3];
			if (a == 0) continue;
			unsigned char* d = drow + (size_t)x * 4;
			if (a == 255) { d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; }
			else if (srcPremultiplied)
			{
				// HUD RGB is already premultiplied by its own alpha.
				const int inv = 255 - a;
				for (int c = 0; c < 3; ++c)
				{
					int v = (int)s[c] + (d[c] * inv) / 255;
					d[c] = (unsigned char)(v > 255 ? 255 : v);
				}
			}
			else
			{
				const int inv = 255 - a;
				d[0] = (unsigned char)((s[0] * a + d[0] * inv) / 255);
				d[1] = (unsigned char)((s[1] * a + d[1] * inv) / 255);
				d[2] = (unsigned char)((s[2] * a + d[2] * inv) / 255);
			}
		}
	}
}

// Agg2D's SpanConvImageBlend applies the image blend mode to the *source*
// span against a constant colour, scales by that colour's alpha, and then
// source-over composites the result.  Reproduce that here.
static void CompositeImageScaled(unsigned char* dst, int dstW, int dstH, int dstStride,
	const unsigned char* src, int srcW, int srcH, int srcStride,
	Agg2DBase::BlendMode mode, const AggColor& blendColor, bool premultiplied)
{
	if (!dst || !src || dstW <= 0 || dstH <= 0 || srcW <= 0 || srcH <= 0) return;
	const unsigned char tint[4] = {
		(unsigned char)blendColor.b, (unsigned char)blendColor.g,
		(unsigned char)blendColor.r, (unsigned char)blendColor.a
	};
	const int srcAbs = srcStride < 0 ? -srcStride : srcStride;
	for (int y = 0; y < dstH; ++y)
	{
		unsigned char* drow = dst + (size_t)y * (size_t)dstStride;
		const int sy = (int)((int64_t)y * srcH / dstH);
		const unsigned char* srow = src + (size_t)sy * (size_t)srcAbs;
		for (int x = 0; x < dstW; ++x)
		{
			const int sx = (int)((int64_t)x * srcW / dstW);
			const unsigned char* s = srow + (size_t)sx * 4;
			if (s[3] == 0 && mode == Agg2DBase::BlendDst) continue;
			unsigned char pixel[4];
			if (mode == Agg2DBase::BlendDst) memcpy(pixel, s, 4);
			else BlendPixelBGRA(mode, s, tint, pixel, premultiplied);
			if (blendColor.a < 255)
			{
				const int a = blendColor.a;
				for (int c = 0; c < 4; ++c) pixel[c] = (unsigned char)((pixel[c] * a) / 255);
			}
			unsigned char out[4];
			unsigned char* d = drow + (size_t)x * 4;
			BlendPixelBGRA(Agg2DBase::BlendSrcOver, pixel, d, out, premultiplied);
			memcpy(d, out, 4);
		}
	}
}

void NativeDrawTarget::transformImage(const Agg2DBase::Image<T_AGG_PF_RGBA>& img,
	double dstX1, double dstY1, double dstX2, double dstY2)
{
	const agg::rendering_buffer& src = img.renBuf;
	if (m_imageBlendMode == Agg2DBase::BlendDst && m_imageBlendColor.a == 255)
	{
		NativeDraw_CompositeHUD(
			m_pixels + (int)dstY1 * m_stride + (int)dstX1 * 4,
			(int)(dstX2 - dstX1), (int)(dstY2 - dstY1), m_stride,
			(const unsigned char*)src.buf(), (int)src.width(), (int)src.height(), src.stride_abs(),
			s_premultipliedOutput);
		return;
	}
	CompositeImageScaled(
		m_pixels + (int)dstY1 * m_stride + (int)dstX1 * 4,
		(int)(dstX2 - dstX1), (int)(dstY2 - dstY1), m_stride,
		(const unsigned char*)src.buf(), (int)src.width(), (int)src.height(), src.stride_abs(),
		m_imageBlendMode, m_imageBlendColor, s_premultipliedOutput);
}

// ---------------------------------------------------------------------------
// text: AGG embedded raster font blit (identical geometry to
// Agg2D::renderText -> renderer_raster_htext_solid)
// ---------------------------------------------------------------------------
void NativeDrawTarget::setFont(const std::string& name)
{
	const agg::int8u* f = NativeDraw_FontData(name.c_str());
	if (f) m_font = f;
}
#ifdef AGG2D_USE_VECTORFONTS
void NativeDrawTarget::setVectorFont(const std::string& fileName, int size, bool bold)
{
	EnsureGdiplus();
	delete m_vectorFont; m_vectorFont = NULL;
	delete m_fontCollection; m_fontCollection = NULL;
	if (fileName.empty() || size <= 0) return;
	const int wlen = MultiByteToWideChar(CP_UTF8, 0, fileName.c_str(), -1, NULL, 0);
	if (wlen <= 0) return;
	std::vector<WCHAR> wide((size_t)wlen);
	MultiByteToWideChar(CP_UTF8, 0, fileName.c_str(), -1, &wide[0], wlen);
	PrivateFontCollection* collection = new PrivateFontCollection();
	if (collection->AddFontFile(&wide[0]) != Ok) { delete collection; return; }
	INT found = 0;
	FontFamily family;
	collection->GetFamilies(1, &family, &found);
	if (found <= 0) { delete collection; return; }
	m_vectorFont = new Font(&family, (REAL)size,
		bold ? FontStyleBold : FontStyleRegular, UnitPixel);
	if (!m_vectorFont || m_vectorFont->GetLastStatus() != Ok)
	{
		delete m_vectorFont; m_vectorFont = NULL;
		delete collection;
		return;
	}
	m_fontCollection = collection;   // must outlive the Font
}

void NativeDrawTarget::renderVectorFontText(double dstX, double dstY, const std::string& str)
{
	if (!m_vectorFont) { renderText(dstX, dstY, str); return; }
	Graphics* g = BeginPrimitive();
	if (!g) return;
	Matrix previous;
	g->GetTransform(&previous);
	Matrix world((REAL)m_xform[0], (REAL)m_xform[1], (REAL)m_xform[2],
	             (REAL)m_xform[3], (REAL)m_xform[4], (REAL)m_xform[5]);
	g->SetTransform(&world);
	const int wlen = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
	std::vector<WCHAR> wide(wlen > 0 ? (size_t)wlen : 1);
	if (wlen > 0) MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wide[0], wlen);
	SolidBrush brush(ToGdiColor(m_line, m_masterAlpha));
	g->SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
	g->DrawString(&wide[0], wlen > 0 ? wlen - 1 : 0, m_vectorFont,
	              PointF((REAL)dstX, (REAL)(dstY - m_vectorFont->GetSize())),
	              &brush);
	g->SetTextRenderingHint(TextRenderingHintSingleBitPerPixel);
	g->SetTransform(&previous);
	EndPrimitive();
}
#endif

void NativeDrawTarget::BlitText(double dstX, double dstY, const std::string& str, unsigned char* pixels)
{
	if (!pixels || !m_font) return;
	const agg::int8u* f = m_font;
	const int height = f[0];
	const int base = f[1];
	const int first = f[2];
	const int count = f[3] ? f[3] : 256;
	if (height <= 0 || height > 64) return;

	const int alpha = ClampByte((int)(m_line.a * m_masterAlpha + 0.5));
	if (alpha == 0) return;

	TransformPoint(dstX, dstY);
	int x0 = (int)dstX;
	int y0 = (int)dstY - base;
	const int strideAbs = m_stride < 0 ? -m_stride : m_stride;

	for (size_t i = 0; i < str.size(); ++i)
	{
		const unsigned ch = (unsigned char)str[i];
		if (ch < (unsigned)first || (int)ch - first >= count) continue;
		const int idx = (int)ch - first;
		const unsigned off = (unsigned)f[4 + idx * 2] | ((unsigned)f[5 + idx * 2] << 8);
		const agg::int8u* glyph = f + 4 + count * 2 + off;
		const int w = glyph[0];
		if (w <= 0 || w > 64) { continue; }
		const int rowBytes = (w + 7) >> 3;
		const agg::int8u* bits = glyph + 1;
		for (int row = 0; row < height; ++row)
		{
			const int py = y0 + row;
			if (py < 0 || py >= m_height) continue;
			unsigned char* dstRow = pixels + (size_t)py * (size_t)m_stride;
			const agg::int8u* rowBits = bits + row * rowBytes;
			for (int col = 0; col < w; ++col)
			{
				if (!(rowBits[col >> 3] & (0x80 >> (col & 7)))) continue;
				const int px = x0 + col;
				if (px < 0 || px >= m_width) continue;
				BlendBGRA(dstRow + px * 4, m_line.r, m_line.g, m_line.b, alpha,
				          s_premultipliedOutput);
			}
		}
		x0 += w;
	}
	(void)strideAbs;
}

void NativeDrawTarget::renderText(double dstX, double dstY, const std::string& str)
{
	// Text is blitted straight into the framebuffer from the RCDATA raster
	// font.  No GDI+ flush is needed here: renderTextDropshadowed() calls this
	// nine times per string, and a flush on every pass is pure overhead.
	dirty();
	if (!m_font) m_font = NativeDraw_FontData("verdana18_bold");
	const bool normal = (m_blendMode == Agg2DBase::BlendAlpha ||
	                     m_blendMode == Agg2DBase::BlendSrcOver);
	if (normal)
	{
		BlitText(dstX, dstY, str, m_pixels);
		return;
	}
	// Non-default blend mode: blit into a scratch surface and composite.
	if (!EnsureScratch()) { BlitText(dstX, dstY, str, m_pixels); return; }
	memset(m_scratchPixels.data(), 0, m_scratchPixels.size());
	BlitText(dstX, dstY, str, m_scratchPixels.data());
	CompositeScratch(m_blendMode);
}

#endif // DESMUME_NATIVE_DRAW && WIN32
