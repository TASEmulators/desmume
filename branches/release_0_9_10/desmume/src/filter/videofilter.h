/*
	Copyright (C) 2011-2012 Roger Manuel
	Copyright (C) 2013 DeSmuME team

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

#ifdef HOST_WINDOWS
	typedef unsigned __int32 uint32_t;
	#include <windows.h>
	typedef CRITICAL_SECTION ThreadLock;
	typedef HANDLE ThreadCond;
#else
	#include <stdint.h>
	#include <pthread.h>
	typedef pthread_mutex_t ThreadLock;
	typedef pthread_cond_t ThreadCond;
#endif

#include <stdlib.h>
#include <string>
#include <vector>
#include "filter.h"
#include "../utils/task.h"


#define VIDEOFILTERTYPE_UNKNOWN_STRING "Unknown"

typedef void (*VideoFilterFunc)(SSurface Src, SSurface Dst);

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
	VideoFilterTypeID_EPXPlus1_5X,
	VideoFilterTypeID_HQ4XS,
	
	VideoFilterTypeIDCount // Make sure this one is always last
};

typedef struct
{
	VideoFilterTypeID typeID;
	const char *typeString;
	VideoFilterFunc filterFunction;
	size_t scaleMultiply;
	size_t scaleDivide;
} VideoFilterAttributes;

// Attributes list of known video filters, indexed using VideoFilterTypeID.
const VideoFilterAttributes VideoFilterAttributesList[] = {
	{VideoFilterTypeID_None,			"None",				NULL,							1,	1},
	{VideoFilterTypeID_LQ2X,			"LQ2x",				&RenderLQ2X,					2,	1},
	{VideoFilterTypeID_LQ2XS,			"LQ2xS",			&RenderLQ2XS,					2,	1},
	{VideoFilterTypeID_HQ2X,			"HQ2x",				&RenderHQ2X,					2,	1},
	{VideoFilterTypeID_HQ2XS,			"HQ2xS",			&RenderHQ2XS,					2,	1},
	{VideoFilterTypeID_HQ4X,			"HQ4x",				&RenderHQ4X,					4,	1},
	{VideoFilterTypeID_2xSaI,			"2xSaI",			&Render2xSaI,					2,	1},
	{VideoFilterTypeID_Super2xSaI,		"Super 2xSaI",		&RenderSuper2xSaI,				2,	1},
	{VideoFilterTypeID_SuperEagle,		"Super Eagle",		&RenderSuperEagle,				2,	1},
	{VideoFilterTypeID_Scanline,		"Scanline",			&RenderScanline,				2,	1},
	{VideoFilterTypeID_Bilinear,		"Bilinear",			&RenderBilinear,				2,	1},
	{VideoFilterTypeID_Nearest2X,		"Nearest 2x",		&RenderNearest2X,				2,	1},
	{VideoFilterTypeID_Nearest1_5X,		"Nearest 1.5x",		&RenderNearest_1Point5x,		3,	2},
	{VideoFilterTypeID_NearestPlus1_5X,	"Nearest+ 1.5x",	&RenderNearestPlus_1Point5x,	3,	2},
	{VideoFilterTypeID_EPX,				"EPX",				&RenderEPX,						2,	1},
	{VideoFilterTypeID_EPXPlus,			"EPX+",				&RenderEPXPlus,					2,	1},
	{VideoFilterTypeID_EPX1_5X,			"EPX 1.5x",			&RenderEPX_1Point5x,			3,	2},
	{VideoFilterTypeID_EPXPlus1_5X,		"EPX+ 1.5x",		&RenderEPXPlus_1Point5x,		3,	2},
	{VideoFilterTypeID_HQ4XS,			"HQ4xS",			&RenderHQ4XS,					4,	1} };

// VIDEO FILTER PARAMETER DATA TYPES
enum VideoFilterParamType
{
	VF_INT = 0,
	VF_UINT,
	VF_FLOAT
};

// VIDEO FILTER PARAMETERS
// These tokens are used with the (Get/Set)FilterParameter family of methods.
enum VideoFilterParamID
{
	VF_PARAM_SCANLINE_A = 0,	// Must always start at 0
	VF_PARAM_SCANLINE_B,
	VF_PARAM_SCANLINE_C,
	VF_PARAM_SCANLINE_D,
	
	VideoFilterParamIDCount		// Make sure this one is always last
};

// Parameters struct for IPC
typedef struct
{
	SSurface srcSurface;
	SSurface dstSurface;
	VideoFilterFunc filterFunction;
} VideoFilterThreadParam;

typedef struct
{
	Task *task;
	VideoFilterThreadParam param;
} VideoFilterThread;

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
	   a pointer to the destination buffer. Alternatively, GetDstBufferPtr() can be
	   used to get the pointer.
 
	Thread Safety:
		All methods are thread-safe.
 ********************************************************************************************/
class VideoFilter
{
private:
	VideoFilterAttributes _vfAttributes;
	
	SSurface _vfSrcSurface;
	SSurface _vfDstSurface;
	uint32_t *_vfSrcSurfacePixBuffer;
	VideoFilterFunc _vfFunc;
	std::vector<VideoFilterThread> _vfThread;
	
	bool _isFilterRunning;
	ThreadLock _lockSrc;
	ThreadLock _lockDst;
	ThreadLock _lockAttributes;
	ThreadCond _condRunning;
	
	void SetAttributes(const VideoFilterAttributes vfAttr);
	
public:
	VideoFilter(size_t srcWidth, size_t srcHeight, VideoFilterTypeID typeID, size_t threadCount);
	~VideoFilter();
	
	bool SetSourceSize(const size_t width, const size_t height);
	bool ChangeFilterByID(const VideoFilterTypeID typeID);
	bool ChangeFilterByAttributes(const VideoFilterAttributes *vfAttr);
	uint32_t* RunFilter();
	
	static void RunFilterCustomByID(const uint32_t *__restrict srcBuffer, uint32_t *__restrict dstBuffer, const size_t srcWidth, const size_t srcHeight, const VideoFilterTypeID typeID);
	static void RunFilterCustomByAttributes(const uint32_t *__restrict srcBuffer, uint32_t *__restrict dstBuffer, const size_t srcWidth, const size_t srcHeight, const VideoFilterAttributes *vfAttr);
	static const char* GetTypeStringByID(const VideoFilterTypeID typeID);
	
	VideoFilterAttributes GetAttributes();
	VideoFilterTypeID GetTypeID();
	const char* GetTypeString();
	uint32_t* GetSrcBufferPtr();
	uint32_t* GetDstBufferPtr();
	size_t GetSrcWidth();
	size_t GetSrcHeight();
	size_t GetDstWidth();
	size_t GetDstHeight();
	VideoFilterParamType GetFilterParameterType(VideoFilterParamID paramID);
	int GetFilterParameteri(VideoFilterParamID paramID);
	unsigned int GetFilterParameterui(VideoFilterParamID paramID);
	float GetFilterParameterf(VideoFilterParamID paramID);
	void SetFilterParameteri(VideoFilterParamID paramID, int value);
	void SetFilterParameterui(VideoFilterParamID paramID, unsigned int value);
	void SetFilterParameterf(VideoFilterParamID paramID, float value);
};

static void* RunVideoFilterTask(void *arg);

void ThreadLockInit(ThreadLock *theLock);
void ThreadLockDestroy(ThreadLock *theLock);
void ThreadLockLock(ThreadLock *theLock);
void ThreadLockUnlock(ThreadLock *theLock);

void ThreadCondInit(ThreadCond *theCondition);
void ThreadCondDestroy(ThreadCond *theCondition);
void ThreadCondWait(ThreadCond *theCondition, ThreadLock *conditionLock);
void ThreadCondSignal(ThreadCond *theCondition);

#endif
