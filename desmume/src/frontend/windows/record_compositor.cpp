/*
	Copyright (C) 2026 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "record_compositor.h"

#include <math.h>
#include <string.h>

#include <algorithm>

#include "common.h"
#include "video.h"

// Several CPU filters read a row above and below the surface they are handed,
// and EPX even reads the pixel to the left of the first one.  The buffers are
// over-allocated and the surface pointer starts a few rows in, so those reads
// stay inside the allocation.  The rows themselves stay tightly packed: a
// filter is given a Pitch of width * 2 and derives the real 32-bit stride
// itself, so padding between rows would shear the picture.
static const int kComposePadRows = 4;
static const int kComposePadPixels = 4;

static u32 *SurfacePointer(u32 *base, size_t width)
{
	return base + (size_t)kComposePadRows * width + (size_t)kComposePadPixels;
}

namespace
{
	bool FilterIsOneAndAHalf(int filter)
	{
		return (filter == VideoInfo::EPX1POINT5) || (filter == VideoInfo::EPXPLUS1POINT5) ||
		       (filter == VideoInfo::NEAREST1POINT5) || (filter == VideoInfo::NEARESTPLUS1POINT5);
	}

	double FilterScaleOf(int filter)
	{
		if (FilterIsOneAndAHalf(filter))
			return 1.5;

		switch (filter)
		{
			case VideoInfo::NONE:	  return 1.0;
			case VideoInfo::_3XBRZ:	  return 3.0;
			case VideoInfo::HQ4X:
			case VideoInfo::_4XBRZ:	  return 4.0;
			case VideoInfo::_5XBRZ:	  return 5.0;
			default:				  return 2.0;
		}
	}

	int NormalizeRotation(int rotation)
	{
		int value = rotation % 360;
		if (value < 0)
			value += 360;

		if (value == 90 || value == 180 || value == 270)
			return value;

		return 0;
	}

	size_t MakeEven(size_t value)
	{
		return value & ~(size_t)1;
	}

	// Rotates an axis-aligned rectangle by a multiple of 90 degrees inside a
	// board of boardW x boardH.  A quarter turn maps a rectangle to a
	// rectangle, which is what makes rotation a per-rectangle decision instead
	// of a second rendering path.
	void RotateRect(double x, double y, double w, double h, int rotation, double boardW, double boardH,
					double *outX, double *outY, double *outW, double *outH)
	{
		switch (rotation)
		{
			case 90:
				*outX = boardH - y - h;
				*outY = x;
				*outW = h;
				*outH = w;
				break;

			case 180:
				*outX = boardW - x - w;
				*outY = boardH - y - h;
				*outW = w;
				*outH = h;
				break;

			case 270:
				*outX = y;
				*outY = boardW - x - w;
				*outW = h;
				*outH = w;
				break;

			default:
				*outX = x;
				*outY = y;
				*outW = w;
				*outH = h;
				break;
		}
	}

	// The working buffer holds exactly the layout the display path hands its
	// own buffers (ColorspaceConvertBuffer555xTo8888Opaque<true, ...>), which
	// is B, G, R, A in memory -- so the struct's "r" member is the blue
	// channel.  Writing them in that order gives the 24bpp B, G, R the writers
	// in aviout.cpp expect, and matches what ColorspaceConvert888XTo888<false>
	// produces for the native path.
	inline void StoreBgr24(u32 pixel, u8 *dst)
	{
		Color4u8 color;
		color.value = pixel;

		dst[0] = color.r;
		dst[1] = color.g;
		dst[2] = color.b;
	}
}

// --- the two pieces of geometry the dialog also needs ----------------------

double recordCompose::FilterScale(int filter)
{
	return FilterScaleOf(filter);
}

void recordCompose::ContentSize(size_t screenWidth, size_t screenHeight, int layout, int screengap, int rotation,
                                double filterScale, size_t *outWidth, size_t *outHeight)
{
	const double width  = (double)screenWidth * filterScale;
	const double height = (double)screenHeight * filterScale;

	// video.scaledscreengap() is screengap * height / 384, with height being
	// the stacked pair, so one screen's height over 192 is the same number.
	const double gap = (double)screengap * height / 192.0;

	double contentW = width;
	double contentH = height;

	switch (layout)
	{
		case 1:
			contentW = (width * 2.0) + gap;
			contentH = height;
			break;

		case 2:
			contentW = width;
			contentH = height;
			break;

		default:
			contentW = width;
			contentH = (height * 2.0) + gap;
			break;
	}

	if (rotation == 90 || rotation == 270)
	{
		std::swap(contentW, contentH);
	}

	*outWidth  = (size_t)(contentW + 0.5);
	*outHeight = (size_t)(contentH + 0.5);
}

const char *recordCompose::FilterName(int filter)
{
	// In VideoInfo's order; the values are what the settings file stores, so
	// this table must not be reordered on its own.
	static const char *names[VideoInfo::NUM_FILTERS] = {
		"None",
		"HQ2X",
		"2xSaI",
		"Super2xSaI",
		"SuperEagle",
		"Scanline",
		"Bilinear",
		"Nearest 2x",
		"HQ2XS",
		"LQ2X",
		"LQ2XS",
		"EPX",
		"Nearest+ 1.5x",
		"Nearest 1.5x",
		"EPX+",
		"EPX 1.5x",
		"EPX+ 1.5x",
		"HQ4X",
		"2xBRZ",
		"3xBRZ",
		"4xBRZ",
		"5xBRZ",
	};

	if (filter < 0 || filter >= VideoInfo::NUM_FILTERS)
		return NULL;

	return names[filter];
}

int recordCompose::FilterCount()
{
	return VideoInfo::NUM_FILTERS;
}

// --- RecordCompositor ------------------------------------------------------

RecordCompositor::RecordCompositor()
{
	this->_srcWidth  = 0;
	this->_srcHeight = 0;
	this->_outWidth  = 0;
	this->_outHeight = 0;

	this->_rotation	   = 0;
	this->_gapColor	   = 0;
	this->_screenCount = 0;
	memset(this->_screen, 0, sizeof(this->_screen));
	memset(&this->_gapRect, 0, sizeof(this->_gapRect));

	this->_src			  = NULL;
	this->_srcSurface	  = NULL;
	this->_filtered		  = NULL;
	this->_filteredSurface = NULL;
	this->_filteredWidth  = 0;
	this->_filteredHeight = 0;
	this->_filter		  = VideoInfo::NONE;

	this->_ready = false;
}

RecordCompositor::~RecordCompositor()
{
	this->FreeBuffers();
}

void RecordCompositor::FreeBuffers()
{
	free_aligned(this->_src);
	this->_src		  = NULL;
	this->_srcSurface = NULL;

	free_aligned(this->_filtered);
	this->_filtered		   = NULL;
	this->_filteredSurface = NULL;

	this->_ready = false;
}

bool RecordCompositor::Configure(const RecordPicture &picture, size_t srcWidth, size_t srcHeight)
{
	this->FreeBuffers();

	if (srcWidth == 0 || srcHeight == 0)
		return false;

	this->_srcWidth	 = srcWidth;
	this->_srcHeight = srcHeight;
	this->_rotation	 = NormalizeRotation(picture.rotation);
	this->_gapColor	 = (u32)picture.gapColor;
	this->_filter	 = picture.filter;

	if (this->_filter < 0 || this->_filter >= VideoInfo::NUM_FILTERS)
		this->_filter = VideoInfo::NONE;

	const double filterScale = FilterScaleOf(this->_filter);

	// Everything below is in the composed picture's own coordinates, in
	// doubles, and only rounded on the way into a rectangle: rounding early
	// makes the two screens drift apart from the gap they are supposed to
	// share.
	const double screenW = (double)srcWidth * filterScale;
	const double screenH = (double)srcHeight * filterScale;
	const double gapPx	 = (double)picture.screengap * screenH / 192.0;

	const bool singleScreen = (picture.layout == 2);

	double contentW = screenW;
	double contentH = screenH;

	switch (picture.layout)
	{
		case 1:
			contentW = singleScreen ? screenW : (screenW * 2.0) + gapPx;
			contentH = screenH;
			break;

		case 2:
			contentW = screenW;
			contentH = screenH;
			break;

		default:
			contentW = screenW;
			contentH = (screenH * 2.0) + gapPx;
			break;
	}

	if (this->_rotation == 90 || this->_rotation == 270)
	{
		std::swap(contentW, contentH);
	}

	if (contentW < 1.0 || contentH < 1.0)
		return false;

	size_t outW = 0;
	size_t outH = 0;

	if (picture.size == RECORD_SIZE_WINDOW)
	{
		outW = (picture.width > 0) ? (size_t)picture.width : 0;
		outH = (picture.height > 0) ? (size_t)picture.height : 0;

		if (outW == 0 || outH == 0)
		{
			// A minimised or not-yet-shown window has no client area; rather
			// than refuse to record, fall back to a whole multiple.
			outW = (size_t)(contentW * 2.0 + 0.5);
			outH = (size_t)(contentH * 2.0 + 0.5);
		}
	}
	else
	{
		int scale = picture.size;
		if (scale < 1)
			scale = 1;
		if (scale > RECORD_SIZE_MAX)
			scale = RECORD_SIZE_MAX;

		outW = (size_t)(contentW * (double)scale + 0.5);
		outH = (size_t)(contentH * (double)scale + 0.5);
	}

	// H.264 and HEVC both carry 4:2:0, which cannot describe an odd width or
	// height: the encoder refuses the stream at the first frame.  Rounding an
	// odd window down by one pixel is cheaper than failing.
	outW = MakeEven(std::min<size_t>(std::max<size_t>(outW, RECORD_CANVAS_MIN), RECORD_CANVAS_MAX));
	outH = MakeEven(std::min<size_t>(std::max<size_t>(outH, RECORD_CANVAS_MIN), RECORD_CANVAS_MAX));

	this->_outWidth	 = outW;
	this->_outHeight = outH;

	// The picture keeps its aspect ratio inside the canvas, so a window that is
	// not the same shape as the DS picture gets bars -- the same bars the user
	// sees on screen.
	const double scale	 = std::min((double)outW / contentW, (double)outH / contentH);
	const double boardW	 = contentW * scale;
	const double boardH	 = contentH * scale;
	const double offsetX = ((double)outW - boardW) * 0.5;
	const double offsetY = ((double)outH - boardH) * 0.5;

	// video.swap 2 and 3 follow whichever engine draws the main screen.  That
	// is decided once, here: a swap can only change when the game moves its
	// engines mid-scene, and a recording that reshuffles its two halves
	// partway through is worse than one that keeps the arrangement it began
	// with.
	bool slotIsMain = true;
	switch (picture.swap)
	{
		case 1:
			slotIsMain = false;
			break;
		case 2:
			slotIsMain = picture.mainScreenIsEngineMain;
			break;
		case 3:
			slotIsMain = !picture.mainScreenIsEngineMain;
			break;
		default:
			slotIsMain = true;
			break;
	}

	this->_screenCount = singleScreen ? 1 : 2;

	for (int i = 0; i < this->_screenCount; ++i)
	{
		const bool isMain = singleScreen ? slotIsMain : ((i == 0) == slotIsMain);

		double x = 0.0;
		double y = 0.0;

		if (picture.layout == 1)
		{
			x = (i == 0) ? 0.0 : (screenW + gapPx);
			y = 0.0;
		}
		else if (!singleScreen)
		{
			x = 0.0;
			y = (i == 0) ? 0.0 : (screenH + gapPx);
		}

		// Rotate the unscaled rectangle inside the unscaled picture, then
		// scale and centre it: scaling commutes with a quarter turn, so this
		// avoids rounding twice.
		double rx = 0.0;
		double ry = 0.0;
		double rw = 0.0;
		double rh = 0.0;
		RotateRect(x, y, screenW, screenH, this->_rotation, contentW, contentH, &rx, &ry, &rw, &rh);

		ScreenRect &screen = this->_screen[i];
		screen.srcX = 0;
		screen.srcY = isMain ? 0 : (int)(screenH + 0.5);
		screen.srcW = (int)(screenW + 0.5);
		screen.srcH = (int)(screenH + 0.5);
		screen.dstX = (int)(offsetX + rx * scale + 0.5);
		screen.dstY = (int)(offsetY + ry * scale + 0.5);
		screen.dstW = (int)(screenW * scale + 0.5); // unrotated size, used for sampling
		screen.dstH = (int)(screenH * scale + 0.5);
	}

	// The gap is drawn as a rectangle of the window's gap colour.  It is only
	// non-empty when there is a gap and two screens to put it between.
	this->_gapRect.dstW = 0;
	this->_gapRect.dstH = 0;

	if (!singleScreen && picture.screengap > 0)
	{
		double gx = 0.0;
		double gy = 0.0;
		double gw = 0.0;
		double gh = 0.0;

		if (picture.layout == 1)
		{
			gx = screenW;
			gy = 0.0;
			gw = gapPx;
			gh = screenH;
		}
		else
		{
			gx = 0.0;
			gy = screenH;
			gw = screenW;
			gh = gapPx;
		}

		double rx = 0.0;
		double ry = 0.0;
		double rw = 0.0;
		double rh = 0.0;
		RotateRect(gx, gy, gw, gh, this->_rotation, contentW, contentH, &rx, &ry, &rw, &rh);

		this->_gapRect.dstX = (int)(offsetX + rx * scale + 0.5);
		this->_gapRect.dstY = (int)(offsetY + ry * scale + 0.5);
		this->_gapRect.dstW = (int)(rw * scale + 0.5);
		this->_gapRect.dstH = (int)(rh * scale + 0.5);
	}

	// Working buffers.  The source surface always covers both screens stacked,
	// because that is the shape a frame arrives in.
	const size_t srcRows = (srcHeight * 2) + (size_t)(kComposePadRows * 2);
	const size_t srcCols = srcWidth + (size_t)(kComposePadPixels * 2);

	this->_src = (u32 *)malloc_alignedCacheLine(srcCols * srcRows * sizeof(u32));
	if (this->_src == NULL)
		return false;

	this->_srcSurface = SurfacePointer(this->_src, srcWidth);

	if (this->_filter != VideoInfo::NONE)
	{
		this->_filteredWidth  = (size_t)(screenW + 0.5);
		this->_filteredHeight = (size_t)(screenH * 2.0 + 0.5);

		const size_t filteredRows = this->_filteredHeight + (size_t)(kComposePadRows * 2);
		const size_t filteredCols = this->_filteredWidth + (size_t)(kComposePadPixels * 2);

		this->_filtered = (u32 *)malloc_alignedCacheLine(filteredCols * filteredRows * sizeof(u32));
		if (this->_filtered == NULL)
		{
			this->FreeBuffers();
			return false;
		}

		this->_filteredSurface = SurfacePointer(this->_filtered, this->_filteredWidth);
	}

	this->_ready = true;
	return true;
}

void RecordCompositor::BeginFrame(const void *src, const NDSColorFormat colorFormat,
                                  float mainIntensity, float touchIntensity)
{
	if (!this->_ready || src == NULL)
		return;

	const size_t width	= this->_srcWidth;
	const size_t height = this->_srcHeight * 2;

	u32 *dstBase = this->_srcSurface;

	// Row by row so the working buffer can carry its padding.  This is the same
	// conversion the display path runs on its own copy of the frame, which is
	// what keeps the recording and the window the same colours.
	for (size_t y = 0; y < height; ++y)
	{
		u32 *dstRow = dstBase + y * width;

		if (colorFormat == NDSColorFormat_BGR555_Rev)
		{
			const u16 *srcRow = (const u16 *)src + y * width;
			ColorspaceConvertBuffer555xTo8888Opaque<true, false, BESwapNone>(srcRow, dstRow, width);
		}
		else
		{
			const u32 *srcRow = (const u32 *)src + y * width;
			ColorspaceConvertBuffer888xTo8888Opaque<true, false>(srcRow, dstRow, width);
		}

		// The LCD brightness is part of the picture, and games use it for
		// fades, so it belongs in the frame as well.
		const float intensity = (y < this->_srcHeight) ? mainIntensity : touchIntensity;
		if (intensity < 1.0f)
		{
			ColorspaceApplyIntensityToBuffer32<false, false>(dstRow, width, intensity);
		}
	}

	if (this->_filter != VideoInfo::NONE)
	{
		this->FilterFrame();
	}
}

void RecordCompositor::FilterFrame()
{
	// SSurface::Pitch is the "16-bit" pitch, not bytes: every filter in
	// src/filter scales it itself (>> 1 for a 32-bit pixel index, * 2 for a
	// byte offset).  Handing it a real byte pitch is what turned the picture
	// into a doubled image, because each filter then walked two rows at a
	// time.  The rows behind these surfaces are tightly packed for the same
	// reason.
	SSurface src;
	memset(&src, 0, sizeof(src));
	src.Surface = (u8 *)this->_srcSurface;
	src.Pitch	= (int)(this->_srcWidth * 2);
	src.Width	= (int)this->_srcWidth;
	src.Height	= (int)(this->_srcHeight * 2);

	SSurface dst;
	memset(&dst, 0, sizeof(dst));
	dst.Surface = (u8 *)this->_filteredSurface;
	dst.Pitch	= (int)(this->_filteredWidth * 2);
	dst.Width	= (int)this->_filteredWidth;
	dst.Height	= (int)this->_filteredHeight;

	switch (this->_filter)
	{
		case VideoInfo::HQ2X:				RenderHQ2X(src, dst); break;
		case VideoInfo::_2XSAI:				Render2xSaI(src, dst); break;
		case VideoInfo::SUPER2XSAI:			RenderSuper2xSaI(src, dst); break;
		case VideoInfo::SUPEREAGLE:			RenderSuperEagle(src, dst); break;
		case VideoInfo::SCANLINE:			RenderScanline(src, dst); break;
		case VideoInfo::BILINEAR:			RenderBilinear(src, dst); break;
		case VideoInfo::NEAREST2X:			RenderNearest2X(src, dst); break;
		case VideoInfo::HQ2XS:				RenderHQ2XS(src, dst); break;
		case VideoInfo::LQ2X:				RenderLQ2X(src, dst); break;
		case VideoInfo::LQ2XS:				RenderLQ2XS(src, dst); break;
		case VideoInfo::EPX:				RenderEPX(src, dst); break;
		case VideoInfo::NEARESTPLUS1POINT5:	RenderNearestPlus_1Point5x(src, dst); break;
		case VideoInfo::NEAREST1POINT5:		RenderNearest_1Point5x(src, dst); break;
		case VideoInfo::EPXPLUS:			RenderEPXPlus(src, dst); break;
		case VideoInfo::EPX1POINT5:			RenderEPX_1Point5x(src, dst); break;
		case VideoInfo::EPXPLUS1POINT5:		RenderEPXPlus_1Point5x(src, dst); break;
		case VideoInfo::HQ4X:				RenderHQ4X(src, dst); break;
		case VideoInfo::_2XBRZ:				Render2xBRZ(src, dst); break;
		case VideoInfo::_3XBRZ:				Render3xBRZ(src, dst); break;
		case VideoInfo::_4XBRZ:				Render4xBRZ(src, dst); break;
		case VideoInfo::_5XBRZ:				Render5xBRZ(src, dst); break;

		default: break;
	}
}

void RecordCompositor::ComposeRows(u8 *dstBase, size_t firstLine, size_t lastLine) const
{
	if (!this->_ready || dstBase == NULL || firstLine > lastLine)
		return;

	if (this->_filter != VideoInfo::NONE)
	{
		this->ComposeRowsTo(dstBase, firstLine, lastLine, this->_filteredSurface, this->_filteredWidth);
	}
	else
	{
		this->ComposeRowsTo(dstBase, firstLine, lastLine, this->_srcSurface, this->_srcWidth);
	}
}

void RecordCompositor::ComposeRowsTo(u8 *dstBase, size_t firstLine, size_t lastLine,
                                     const u32 *src, size_t srcPitchPx) const
{
	const size_t outWidth  = this->_outWidth;
	const size_t outHeight = this->_outHeight;
	const int	 rotation  = this->_rotation;

	const u8 gapB = (u8)((this->_gapColor >> 0) & 0xFF);
	const u8 gapG = (u8)((this->_gapColor >> 8) & 0xFF);
	const u8 gapR = (u8)((this->_gapColor >> 16) & 0xFF);

	for (size_t y = firstLine; y <= lastLine && y < outHeight; ++y)
	{
		// Rows run bottom-up in the destination, which is what the writers in
		// aviout.cpp have always been handed.
		u8 *row = dstBase + (outHeight - 1 - y) * outWidth * 3;

		memset(row, 0, outWidth * 3);

		for (int i = 0; i < this->_screenCount; ++i)
		{
			const ScreenRect &screen = this->_screen[i];

			if (screen.dstW <= 0 || screen.dstH <= 0)
				continue;

			const bool upright = (rotation == 0 || rotation == 180);
			const int rotatedW = upright ? screen.dstW : screen.dstH;
			const int rotatedH = upright ? screen.dstH : screen.dstW;

			if (y < (size_t)screen.dstY || y >= (size_t)(screen.dstY + rotatedH))
				continue;

			const int localY = (int)y - screen.dstY;

			// Inside the rotated rectangle one source coordinate is fixed for
			// the whole row and the other walks with x, so every case is a
			// single scanline loop.
			int	 fixedIndex = 0;
			bool fixedIsRow = true;
			bool reverse	= false;

			switch (rotation)
			{
				case 90:
					fixedIndex	= localY;					   // sx0 = dy
					fixedIsRow	= false;					   // the column is fixed
					reverse		= true;						   // sy0 = dstH-1-dx
					break;

				case 180:
					fixedIndex	= screen.dstH - 1 - localY;	   // sy0
					fixedIsRow	= true;
					reverse		= true;						   // sx0 = dstW-1-dx
					break;

				case 270:
					fixedIndex	= screen.dstW - 1 - localY;	   // sx0 = dstW-1-dy
					fixedIsRow	= false;
					reverse		= false;					   // sy0 = dx
					break;

				default:
					fixedIndex	= localY;					   // sy0 = dy
					fixedIsRow	= true;
					reverse		= false;					   // sx0 = dx
					break;
			}

			if (screen.dstX < 0 || screen.dstX >= (int)outWidth)
				continue;

			// Rounding the canvas can leave the last column one pixel past the
			// edge; clipping it here keeps every scanline inside its row.
			const int drawW = std::min(rotatedW, (int)outWidth - screen.dstX);

			u8 *out = row + (size_t)screen.dstX * 3;

			if (fixedIsRow)
			{
				const size_t srcW	= (size_t)screen.srcW;
				const size_t v		= (size_t)screen.srcY + (size_t)fixedIndex * (size_t)screen.srcH / (size_t)screen.dstH;
				const u32   *srcRow = src + v * srcPitchPx;

				const long long step = ((long long)srcW << 16) / (long long)screen.dstW;
				long long acc = reverse ? ((long long)(screen.dstW - 1) * step) : 0;
				const long long delta = reverse ? -step : step;

				for (int x = 0; x < drawW; ++x, out += 3, acc += delta)
				{
					const size_t u = (size_t)screen.srcX + (size_t)(acc >> 16);
					StoreBgr24(srcRow[u], out);
				}
			}
			else
			{
				const size_t srcH = (size_t)screen.srcH;
				const size_t u	  = (size_t)screen.srcX + (size_t)fixedIndex * (size_t)screen.srcW / (size_t)screen.dstW;

				const long long step = ((long long)srcH << 16) / (long long)screen.dstH;
				long long acc = reverse ? ((long long)(screen.dstH - 1) * step) : 0;
				const long long delta = reverse ? -step : step;

				for (int x = 0; x < drawW; ++x, out += 3, acc += delta)
				{
					const size_t v = (size_t)screen.srcY + (size_t)(acc >> 16);
					StoreBgr24(src[v * srcPitchPx + u], out);
				}
			}
		}

		// The gap between the screens is painted with the window's gap colour;
		// everything else the screens do not cover stays black.
		if (this->_gapRect.dstW > 0 && this->_gapRect.dstH > 0 &&
			this->_gapRect.dstX >= 0 && this->_gapRect.dstX < (int)outWidth &&
			y >= (size_t)this->_gapRect.dstY && y < (size_t)(this->_gapRect.dstY + this->_gapRect.dstH))
		{
			const int gapW = std::min(this->_gapRect.dstW, (int)outWidth - this->_gapRect.dstX);

			u8 *gap = row + (size_t)this->_gapRect.dstX * 3;
			for (int x = 0; x < gapW; ++x, gap += 3)
			{
				gap[0] = gapB;
				gap[1] = gapG;
				gap[2] = gapR;
			}
		}
	}
}
