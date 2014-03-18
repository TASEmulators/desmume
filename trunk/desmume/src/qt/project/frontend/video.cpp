/*
	Copyright (C) 2014 DeSmuME team
	Copyright (C) 2014 Alvin Wong

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

#include "video.h"
#include "GPU.h"
#include "GPU_osd.h"

#include <QDebug>

#ifdef HOST_WINDOWS
	typedef unsigned __int8 uint8_t;
	typedef unsigned __int16 uint16_t;
#	ifdef _MSC_VER
#	define __restrict__ __restrict
#	endif
#endif

namespace desmume {
namespace qt {

namespace {

// These conevrsion functions are adapted from Cocoa port.
static const uint8_t bits5to8[] = {
	0x00, 0x08, 0x10, 0x19, 0x21, 0x29, 0x31, 0x3A,
	0x42, 0x4A, 0x52, 0x5A, 0x63, 0x6B, 0x73, 0x7B,
	0x84, 0x8C, 0x94, 0x9C, 0xA5, 0xAD, 0xB5, 0xBD,
	0xC5, 0xCE, 0xD6, 0xDE, 0xE6, 0xEF, 0xF7, 0xFF
};

static inline uint32_t RGB555ToRGBA8888(const uint16_t color16)
{
	return	(bits5to8[((color16 >> 0) & 0x001F)] << 0) |
			(bits5to8[((color16 >> 5) & 0x001F)] << 8) |
			(bits5to8[((color16 >> 10) & 0x001F)] << 16) |
			0xFF000000;
}

static inline uint32_t RGB555ToBGRA8888(const uint16_t color16)
{
	return	(bits5to8[((color16 >> 10) & 0x001F)] << 0) |
			(bits5to8[((color16 >> 5) & 0x001F)] << 8) |
			(bits5to8[((color16 >> 0) & 0x001F)] << 16) |
			0xFF000000;
}

static inline void RGB555ToRGBA8888Buffer(const uint16_t *__restrict__ srcBuffer, uint32_t *__restrict__ destBuffer, size_t pixelCount)
{
	const uint32_t *__restrict__ destBufferEnd = destBuffer + pixelCount;

	while (destBuffer < destBufferEnd)
	{
		*destBuffer++ = RGB555ToRGBA8888(*srcBuffer++);
	}
}

static inline void RGB555ToBGRA8888Buffer(const uint16_t *__restrict__ srcBuffer, uint32_t *__restrict__ destBuffer, size_t pixelCount)
{
	const uint32_t *__restrict__ destBufferEnd = destBuffer + pixelCount;

	while (destBuffer < destBufferEnd)
	{
		*destBuffer++ = RGB555ToBGRA8888(*srcBuffer++);
	}
}

} /* namespace */

Video::Video(int numThreads, QObject* parent)
	: QObject(parent)
	, mFilter(256, 384, VideoFilterTypeID_None, numThreads)
{
}

bool Video::setFilter(VideoFilterTypeID filterID) {
	const VideoFilterAttributes& attrib = VideoFilterAttributesList[filterID];
	if (attrib.scaleDivide == 1 || attrib.scaleDivide == 2) {
		bool res = this->mFilter.ChangeFilterByID(filterID);
		runFilter();
		return res;
	} else {
		qWarning(
				"Filter %s has a `scaleDivide` of %d."
				"The code currently can't handle this well,"
				"so this filter will not be enabled.\n",
				attrib.typeString,
				(int)attrib.scaleDivide
		);
		return false;
	}
}

unsigned int* Video::runFilter() {
	RGB555ToRGBA8888Buffer((u16*)GPU_screen, this->mFilter.GetSrcBufferPtr(), 256 * 384);
	unsigned int* buf = this->mFilter.RunFilter();
	this->screenBufferUpdated(buf, this->getDstSize(), this->getDstScale());
	return buf;
}

unsigned int* Video::getDstBufferPtr() {
	return this->mFilter.GetDstBufferPtr();
}

int Video::getDstWidth() {
	return this->mFilter.GetDstWidth();
}

int Video::getDstHeight() {
	return this->mFilter.GetDstHeight();
}

QSize Video::getDstSize() {
	return QSize(this->mFilter.GetDstWidth(), this->mFilter.GetDstHeight());
}

qreal Video::getDstScale() {
	const VideoFilterAttributes& attrib = this->mFilter.GetAttributes();
	return (qreal)attrib.scaleMultiply / (qreal)attrib.scaleDivide;
}

} /* namespace qt */
} /* namespace desmume */
