/*
Copyright (C) 2009-2025 DeSmuME team

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

#include "filter/videofilter.h"
#include "common.h"

class VideoInfo
{
public:

	VideoInfo()
		: buffer(NULL)
		, filteredbuffer(NULL)
		, pixelFilter(NULL)
		, filterThreadCount(0)
		, appliedFilterThreads(-1)
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

	u8* srcBuffer;
	size_t srcBufferSize;
	u32 *buffer;
	u32 *filteredbuffer;
	VideoFilter *pixelFilter;
	volatile LONG filterThreadCount;
	int appliedFilterThreads;

	void SetFilterThreadCount(int count)
	{
		InterlockedExchange(&filterThreadCount, count);
	}

	int GetFilterThreadCount()
	{
		return (int)InterlockedCompareExchange(&filterThreadCount, 0, 0);
	}

	void InitFilter()
	{
		pixelFilter = new VideoFilter(32, 32, VideoFilterTypeID_None, 0);
	}

	void ShutdownFilter()
	{
		delete pixelFilter;
		pixelFilter = NULL;
		buffer = NULL;
		filteredbuffer = NULL;
	}

	void SetPrescale(int prescaleHD, int prescalePost)
	{
		if (this->prescaleHD != prescaleHD || this->prescalePost != prescalePost)
		{
			this->prescaleHD = prescaleHD;
			this->prescalePost = prescalePost;
			prefilterWidth = 256 * prescaleHD;
			prefilterHeight = 192 * 2 * prescaleHD;

			prescaleTotal = prescaleHD;

			setfilter(currentfilter);
		}
	}

	bool ResizeBuffers()
	{
		const int threads = GetFilterThreadCount();
		pixelFilter->SetThreadCount((size_t)threads);
		if (!pixelFilter->ChangeFilterByID((VideoFilterTypeID)currentfilter) ||
		    !pixelFilter->SetSourceSize(prefilterWidth, prefilterHeight))
		{
			buffer = NULL;
			filteredbuffer = NULL;
			return false;
		}
		buffer = pixelFilter->GetSrcBufferPtr();
		filteredbuffer = pixelFilter->GetDstBufferPtr();
		appliedFilterThreads = threads;
		return true;
	}

	void clear()
	{
		if (buffer != NULL)
			memset(buffer, 0, pixelFilter->GetSrcWidth() * pixelFilter->GetSrcHeight() * sizeof(u32));
		if (filteredbuffer != NULL)
			memset(filteredbuffer, 0, pixelFilter->GetDstWidth() * pixelFilter->GetDstHeight() * sizeof(u32));
	}

	void setfilter(int filter) {

		if (filter < 0 || filter >= VideoFilterTypeIDCount)
			filter = VideoFilterTypeID_None;

		currentfilter = filter;

		const VideoFilterAttributes attr = VideoFilter::GetAttributesByID((VideoFilterTypeID)filter);
		width = (int)(prefilterWidth * attr.scaleMultiply / attr.scaleDivide);
		height = (int)(prefilterHeight * attr.scaleMultiply / attr.scaleDivide);
	}

	u16* finalBuffer() const
	{
		if (currentfilter == VideoFilterTypeID_None)
			return (u16*)buffer;
		else return (u16*)filteredbuffer;
	}

	void filter() {
		pixelFilter->RunFilter();
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
