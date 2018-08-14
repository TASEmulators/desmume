/*
Copyright (C) 2009-2015 DeSmuME team

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

#ifndef _VIDEO_H_
#define _VIDEO_H_

#include "filter/filter.h"
#include "common.h"

class VideoInfo
{
public:

	VideoInfo()
		: buffer(NULL)
		, filteredbuffer(NULL)
	{
	}

	int width;
	int height;
	int prefilterWidth;
	int prefilterHeight;

	int rotation;
	int rotation_userset;
	int screengap;
	int layout;
	int layout_old;
	int	swap;

	int currentfilter;
	int prescaleHD;
	int prescalePost; //not supported yet
	int prescaleTotal;

	size_t scratchBufferSize;
	size_t rawBufferSize;
	u8* srcBuffer;
	size_t srcBufferSize;
	u32 *buffer, *buffer_raw;
	u32 *filteredbuffer;

	void SetPrescale(int prescaleHD, int prescalePost)
	{
		if (this->prescaleHD != prescaleHD || this->prescalePost != prescalePost)
		{
			free_aligned(buffer_raw);
			free_aligned(filteredbuffer);

			this->prescaleHD = prescaleHD;
			this->prescalePost = prescalePost;
			prefilterWidth = 256 * prescaleHD;
			prefilterHeight = 192 * 2 * prescaleHD;

			prescaleTotal = prescaleHD;

			ResizeBuffers();

			setfilter(currentfilter);
		}
	}

	void ResizeBuffers()
	{
		// all these stupid video filters read outside of their buffer. let's allocate too much and hope it stays filled with black. geeze
		const int kPadSize = 4;

		// raw buffer
		size_t rawBufferWidth = prefilterWidth + (kPadSize * 2);
		size_t rawBufferHeight = prefilterHeight + (kPadSize * 2);
		rawBufferSize = rawBufferWidth * rawBufferHeight * 4;
		buffer_raw = buffer = (u32*)malloc_alignedCacheLine(rawBufferSize);

		// display buffer
		size_t scratchBufferWidth = width + (kPadSize * 2);
		size_t scratchBufferHeight = height + (kPadSize * 2);
		scratchBufferSize = scratchBufferWidth * scratchBufferHeight * 4;
		filteredbuffer = (u32*)malloc_alignedCacheLine(scratchBufferSize);

		//move the buffer pointer inside it's padded area so that earlier reads won't go out of the buffer we allocated
		buffer += (kPadSize*rawBufferWidth + kPadSize) * 4;

		// clean the new buffers
		clear();
	}

	enum {
		NONE,
		HQ2X,
		_2XSAI,
		SUPER2XSAI,
		SUPEREAGLE,
		SCANLINE,
		BILINEAR,
		NEAREST2X,
		HQ2XS,
		LQ2X,
		LQ2XS,
		EPX,
		NEARESTPLUS1POINT5,
		NEAREST1POINT5,
		EPXPLUS,
		EPX1POINT5,
		EPXPLUS1POINT5,
		HQ4X,
		_2XBRZ,
		_3XBRZ,
		_4XBRZ,
		_5XBRZ,

		NUM_FILTERS,
	};

	void clear()
	{
		//I dont understand this...
		//if (srcBuffer)
		//{
		//	memset(srcBuffer, 0xFF, size() * 2);
		//}
		memset(buffer_raw, 0, rawBufferSize);
		memset(filteredbuffer, 0, scratchBufferSize);
	}

	void reset() {
		SetPrescale(1, 1); //should i do this here?
	}

	void setfilter(int filter) {

		if (filter < 0 || filter >= NUM_FILTERS)
			filter = NONE;

		currentfilter = filter;

		switch (filter) {

		case NONE:
			width = prefilterWidth;
			height = prefilterHeight;
			break;
		case EPX1POINT5:
		case EPXPLUS1POINT5:
		case NEAREST1POINT5:
		case NEARESTPLUS1POINT5:
			width = prefilterWidth * 3 / 2;
			height = prefilterHeight * 3 / 2;
			break;

		case _5XBRZ:
			width = prefilterWidth * 5;
			height = prefilterHeight * 5;
			break;

		case HQ4X:
		case _4XBRZ:
			width = prefilterWidth * 4;
			height = prefilterHeight * 4;
			break;

		case _3XBRZ:
			width = prefilterWidth * 3;
			height = prefilterHeight * 3;
			break;

		case _2XBRZ:
		default:
			width = prefilterWidth * 2;
			height = prefilterHeight * 2;
			break;
		}

		ResizeBuffers();
	}

	SSurface src;
	SSurface dst;

	u16* finalBuffer() const
	{
		if (currentfilter == NONE)
			return (u16*)buffer;
		else return (u16*)filteredbuffer;
	}

	void filter() {

		src.Height = 384 * prescaleHD;
		src.Width = 256 * prescaleHD;
		src.Pitch = src.Width * 2;
		src.Surface = (u8*)buffer;

		dst.Height = height * prescaleHD;
		dst.Width = width * prescaleHD;
		dst.Pitch = width * 2;
		dst.Surface = (u8*)filteredbuffer;

		switch (currentfilter)
		{
		case NONE:
			break;
		case LQ2X:
			RenderLQ2X(src, dst);
			break;
		case LQ2XS:
			RenderLQ2XS(src, dst);
			break;
		case HQ2X:
			RenderHQ2X(src, dst);
			break;
		case HQ4X:
			RenderHQ4X(src, dst);
			break;
		case HQ2XS:
			RenderHQ2XS(src, dst);
			break;
		case _2XSAI:
			Render2xSaI(src, dst);
			break;
		case SUPER2XSAI:
			RenderSuper2xSaI(src, dst);
			break;
		case SUPEREAGLE:
			RenderSuperEagle(src, dst);
			break;
		case SCANLINE:
			RenderScanline(src, dst);
			break;
		case BILINEAR:
			RenderBilinear(src, dst);
			break;
		case NEAREST2X:
			RenderNearest2X(src, dst);
			break;
		case EPX:
			RenderEPX(src, dst);
			break;
		case EPXPLUS:
			RenderEPXPlus(src, dst);
			break;
		case EPX1POINT5:
			RenderEPX_1Point5x(src, dst);
			break;
		case EPXPLUS1POINT5:
			RenderEPXPlus_1Point5x(src, dst);
			break;
		case NEAREST1POINT5:
			RenderNearest_1Point5x(src, dst);
			break;
		case NEARESTPLUS1POINT5:
			RenderNearestPlus_1Point5x(src, dst);
			break;
		case _2XBRZ:
			Render2xBRZ(src, dst);
			break;
		case _3XBRZ:
			Render3xBRZ(src, dst);
			break;
		case _4XBRZ:
			Render4xBRZ(src, dst);
			break;
		case _5XBRZ:
			Render5xBRZ(src, dst);
			break;
		}
	}

	int size() {
		return width * height;
	}

	int dividebyratio(int x) {
		return x * 256 / width;
	}

	int rotatedwidth() {
		switch (rotation) {
		case 0:
			return width;
		case 90:
			return height;
		case 180:
			return width;
		case 270:
			return height;
		default:
			return 0;
		}
	}

	int rotatedheight() {
		switch (rotation) {
		case 0:
			return height;
		case 90:
			return width;
		case 180:
			return height;
		case 270:
			return width;
		default:
			return 0;
		}
	}

	int rotatedwidthgap() {
		switch (rotation) {
		case 0:
			return width;
		case 90:
			return height + ((layout == 0) ? scaledscreengap() : 0);
		case 180:
			return width;
		case 270:
			return height + ((layout == 0) ? scaledscreengap() : 0);
		default:
			return 0;
		}
	}

	int rotatedheightgap() {
		switch (rotation) {
		case 0:
			return height + ((layout == 0) ? scaledscreengap() : 0);
		case 90:
			return width;
		case 180:
			return height + ((layout == 0) ? scaledscreengap() : 0);
		case 270:
			return width;
		default:
			return 0;
		}
	}

	int scaledscreengap() {
		return screengap * height / 384;
	}
};

#endif
