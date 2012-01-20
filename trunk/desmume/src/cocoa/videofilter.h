/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2012 DeSmuME team

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

#ifndef _VIDEOFILTER_
#define _VIDEOFILTER_

#include <stdlib.h>
#include <string>
#include <pthread.h>
#include "types.h"
#include "../filter/filter.h"


// VIDEO FILTER TYPES
enum VideoFilterTypeID
{
	VideoFilterTypeID_None = 0,
	VideoFilterTypeID_LQ2X,
	VideoFilterTypeID_LQ2XS,
	VideoFilterTypeID_HQ2X,
	VideoFilterTypeID_HQ2XS,
	VideoFilterTypeID_HQ4X,
	VideoFilterTypeID_2xSaI,
	VideoFilterTypeID_Super2xSaI,
	VideoFilterTypeID_SuperEagle,
	VideoFilterTypeID_Scanline,
	VideoFilterTypeID_Bilinear,
	VideoFilterTypeID_Nearest2X,
	VideoFilterTypeID_Nearest1_5X,
	VideoFilterTypeID_NearestPlus1_5X,
	VideoFilterTypeID_EPX,
	VideoFilterTypeID_EPXPlus,
	VideoFilterTypeID_EPX1_5X,
	VideoFilterTypeID_EPXPlus1_5X
};

// VIDEO FILTER TYPE STRINGS
#define VIDEOFILTERTYPE_NONE_STRING					"None"
#define VIDEOFILTERTYPE_LQ2X_STRING					"LQ2X"
#define VIDEOFILTERTYPE_LQ2XS_STRING				"LQ2XS"
#define VIDEOFILTERTYPE_HQ2X_STRING					"HQ2X"
#define VIDEOFILTERTYPE_HQ2XS_STRING				"HQ2XS"
#define VIDEOFILTERTYPE_HQ4X_STRING					"HQ4X"
#define VIDEOFILTERTYPE_2XSAI_STRING				"2xSaI"
#define VIDEOFILTERTYPE_SUPER_2XSAI_STRING			"Super 2xSaI"
#define VIDEOFILTERTYPE_SUPER_EAGLE_STRING			"Super Eagle"
#define VIDEOFILTERTYPE_SCANLINE_STRING				"Scanline"
#define VIDEOFILTERTYPE_BILINEAR_STRING				"Bilinear"
#define VIDEOFILTERTYPE_NEAREST_2X_STRING			"Nearest 2x"
#define VIDEOFILTERTYPE_NEAREST_1_5X_STRING			"Nearest 1.5x"
#define VIDEOFILTERTYPE_NEAREST_PLUS_1_5X_STRING	"Nearest+ 1.5x"
#define VIDEOFILTERTYPE_EPX_STRING					"EPX"
#define VIDEOFILTERTYPE_EPX_PLUS_STRING				"EPX+"
#define VIDEOFILTERTYPE_EPX_1_5X_STRING				"EPX 1.5x"
#define VIDEOFILTERTYPE_EPX_PLUS_1_5X_STRING		"EPX+ 1.5x"
#define VIDEOFILTERTYPE_UNKNOWN_STRING				"Unknown"

typedef void (*VideoFilterCallback)(SSurface Src, SSurface Dst);

/********************************************************************************************
	VideoFilter - C++ CLASS

	This is a wrapper class for managing the video filter functions from filter.h.

	The steps to using a video filter are as follows:
	1. Instantiate this class.
	2. Set the rectangular size of the source image in pixels. (Can be done during
	   instantiation.)
	3. Set the video filter type. (Can be done during instantiation.)
	4. Fill in the source buffer with pixels in RGBA8888 format. This class provides
	   the GetSrcBufferPtr() accessor for the source buffer pointer.
	5. Call RunFilter(). This runs the source buffer pixels through the chosen filter,
	   and then stores the resulting pixels into the destination buffer in RGBA8888
	   format.
	6. At this point, the destination buffer pixels can be used. RunFilter() returns
	   a pointer to the destination buffer. Alternatively, GetDestBufferPtr() can be
	   used to get the pointer.
 
	Thread Safety:
		All methods are thread-safe.
 ********************************************************************************************/
class VideoFilter
{
private:
	VideoFilterTypeID _typeID;
	std::string _typeString;
	SSurface *_srcSurface;
	SSurface *_destSurface;
	uint32_t *_srcSurfaceBufferMaster;
	VideoFilterCallback _filterCallback;
	bool _isFilterRunning;
	
	pthread_mutex_t _mutexSrc;
	pthread_mutex_t _mutexDest;
	pthread_mutex_t _mutexTypeID;
	pthread_mutex_t _mutexTypeString;
	pthread_cond_t _condRunning;
	
	void SetTypeID(VideoFilterTypeID typeID);
	void SetTypeString(const char *typeString);
	void SetTypeString(std::string typeString);
	
public:
	VideoFilter();
	VideoFilter(unsigned int srcWidth, unsigned int srcHeight);
	VideoFilter(unsigned int srcWidth, unsigned int srcHeight, VideoFilterTypeID typeID);
	~VideoFilter();
	
	bool SetSourceSize(unsigned int width, unsigned int height);
	bool ChangeFilter(VideoFilterTypeID typeID);
	uint32_t* RunFilter();
	static const char* GetTypeStringByID(VideoFilterTypeID typeID);
	
	VideoFilterTypeID GetTypeID();
	const char* GetTypeString();
	uint32_t* GetSrcBufferPtr();
	uint32_t* GetDestBufferPtr();
	unsigned int GetSrcWidth();
	unsigned int GetSrcHeight();
	unsigned int GetDestWidth();
	unsigned int GetDestHeight();
};

#endif
