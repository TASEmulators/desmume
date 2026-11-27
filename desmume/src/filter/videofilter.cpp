/*
	Copyright (C) 2011-2012 Roger Manuel
	Copyright (C) 2013-2025 DeSmuME team

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

#include "videofilter.h"
#include <string.h>
#include "../common.h"


// This function is called when running a filter in multithreaded mode.
static void* RunVideoFilterTask(void *arg);

// Attributes list of known video filters, indexed using VideoFilterTypeID.
// Use VideoFilter::GetAttributesByID() to retrieve a filter's attributes.
const VideoFilterAttributes VideoFilterAttributesList[] = {
	{VideoFilterTypeID_None,			"None",				NULL,							1,	1,	0},
	{VideoFilterTypeID_LQ2X,			"LQ2x",				&RenderLQ2X,					2,	1,	0},
	{VideoFilterTypeID_LQ2XS,			"LQ2xS",			&RenderLQ2XS,					2,	1,	0},
	{VideoFilterTypeID_HQ2X,			"HQ2x",				&RenderHQ2X,					2,	1,	0},
	{VideoFilterTypeID_HQ2XS,			"HQ2xS",			&RenderHQ2XS,					2,	1,	0},
	{VideoFilterTypeID_HQ4X,			"HQ4x",				&RenderHQ4X,					4,	1,	0},
	{VideoFilterTypeID_2xSaI,			"2xSaI",			&Render2xSaI,					2,	1,	0},
	{VideoFilterTypeID_Super2xSaI,		"Super 2xSaI",		&RenderSuper2xSaI,				2,	1,	0},
	{VideoFilterTypeID_SuperEagle,		"Super Eagle",		&RenderSuperEagle,				2,	1,	0},
	{VideoFilterTypeID_Scanline,		"Scanline",			&RenderScanline,				2,	1,	0},
	{VideoFilterTypeID_Bilinear,		"Bilinear",			&RenderBilinear,				2,	1,	0},
	{VideoFilterTypeID_Nearest2X,		"Nearest 2x",		&RenderNearest2X,				2,	1,	0},
	{VideoFilterTypeID_Nearest1_5X,		"Nearest 1.5x",		&RenderNearest_1Point5x,		3,	2,	0},
	{VideoFilterTypeID_NearestPlus1_5X,	"Nearest+ 1.5x",	&RenderNearestPlus_1Point5x,	3,	2,	0},
	{VideoFilterTypeID_EPX,				"EPX",				&RenderEPX,						2,	1,	0},
	{VideoFilterTypeID_EPXPlus,			"EPX+",				&RenderEPXPlus,					2,	1,	0},
	{VideoFilterTypeID_EPX1_5X,			"EPX 1.5x",			&RenderEPX_1Point5x,			3,	2,	0},
	{VideoFilterTypeID_EPXPlus1_5X,		"EPX+ 1.5x",		&RenderEPXPlus_1Point5x,		3,	2,	0},
	{VideoFilterTypeID_HQ4XS,			"HQ4xS",			&RenderHQ4XS,					4,	1,	0},
	{VideoFilterTypeID_2xBRZ,			"2xBRZ",			&Render2xBRZ,					2,	1,	0},
	{VideoFilterTypeID_3xBRZ,			"3xBRZ",			&Render3xBRZ,					3,	1,	0},
	{VideoFilterTypeID_4xBRZ,			"4xBRZ",			&Render4xBRZ,					4,	1,	0},
	{VideoFilterTypeID_5xBRZ,			"5xBRZ",			&Render5xBRZ,					5,	1,	0},
	{VideoFilterTypeID_HQ3X,			"HQ3x",				&RenderHQ3X,					3,	1,	0},
	{VideoFilterTypeID_HQ3XS,			"HQ3xS",			&RenderHQ3XS,					3,	1,	0},
	{VideoFilterTypeID_6xBRZ,			"6xBRZ",			&Render6xBRZ,					6,	1,	0} };

// Parameters for Scanline filter
int scanline_filter_a = 0;
int scanline_filter_b = 2;
int scanline_filter_c = 2;
int scanline_filter_d = 4;

// Parameters passed to the RunVideoFilterTask() thread task function
typedef struct
{
	void *index;
	VideoFilterParamType type;
} _VideoFilterParamAttributes;

static const _VideoFilterParamAttributes _VideoFilterParamAttributesList[] = {
	{&scanline_filter_a,	VF_INT},
	{&scanline_filter_b,	VF_INT},
	{&scanline_filter_c,	VF_INT},
	{&scanline_filter_d,	VF_INT},
};

/********************************************************************************************
	CLASS CONSTRUCTORS
 ********************************************************************************************/
VideoFilter::VideoFilter()
{
	__InstanceInit(32, 32, VideoFilterTypeID_None, 0);
}

VideoFilter::VideoFilter(size_t srcWidth,
						 size_t srcHeight,
						 VideoFilterTypeID typeID = VideoFilterTypeID_None,
						 size_t threadCount = 0)
{
	__InstanceInit(srcWidth, srcHeight, typeID, threadCount);
}

/********************************************************************************************
	CLASS DESTRUCTOR
 ********************************************************************************************/
VideoFilter::~VideoFilter()
{
	// Destroy all threads first
	for (size_t i = 0; i < __vfThread.size(); i++)
	{
		__vfThread[i].task->finish();
		__vfThread[i].task->shutdown();
		
		delete __vfThread[i].task;
	}
	
	__vfThread.clear();
	
	// Destroy everything else
	ThreadLockLock(&_lockSrc);
	ThreadLockLock(&_lockDst);
	
	while (__isCPUFilterRunning)
	{
		ThreadCondWait(&__condCPUFilterRunning, &_lockDst);
	}
	
	if (_useInternalDstBuffer)
	{
		free_aligned(__vfDstSurface.Surface);
		__vfDstSurface.Surface = NULL;
	}
	
	for (size_t i = 0; i < _vfAttributes.workingSurfaceCount; i++)
	{
		free_aligned(__vfDstSurface.workingSurface[i]);
		__vfDstSurface.workingSurface[i] = NULL;
	}
	
	ThreadLockUnlock(&_lockDst);
	
	free_aligned(__vfSrcSurfacePixBuffer);
	__vfSrcSurfacePixBuffer = NULL;
	__vfSrcSurface.Surface = NULL;
	
	ThreadLockUnlock(&_lockSrc);
	
	ThreadLockDestroy(&_lockSrc);
	ThreadLockDestroy(&_lockDst);
	ThreadLockDestroy(&_lockAttributes);
	ThreadCondDestroy(&__condCPUFilterRunning);
}

void VideoFilter::__InstanceInit(size_t srcWidth, size_t srcHeight, VideoFilterTypeID typeID, size_t threadCount)
{
	SSurface newSurface;
	newSurface.Surface  = NULL;
	newSurface.Pitch    = (int)(srcWidth * 2);
	newSurface.Width    = (int)srcWidth;
	newSurface.Height   = (int)srcHeight;
	newSurface.userData = NULL;
	
	for (size_t i = 0; i < FILTER_MAX_WORKING_SURFACE_COUNT; i++)
	{
		newSurface.workingSurface[i] = NULL;
	}
	
	__vfSrcSurface = newSurface;
	__vfDstSurface = newSurface;
	
	_useInternalDstBuffer = true;
	__isCPUFilterRunning = false;
	__vfSrcSurfacePixBuffer = NULL;
	_requestedThreadCount = threadCount;
	
	if (typeID < VideoFilterTypeIDCount)
	{
		_vfAttributes = VideoFilterAttributesList[typeID];
	}
	else
	{
		_vfAttributes = VideoFilterAttributesList[VideoFilterTypeID_None];
	}
	
	_pixelScale = (float)_vfAttributes.scaleMultiply / (float)_vfAttributes.scaleDivide;
	__vfFunc = _vfAttributes.filterFunction;
	
	ThreadLockInit(&_lockSrc);
	ThreadLockInit(&_lockDst);
	ThreadLockInit(&_lockAttributes);
	ThreadCondInit(&__condCPUFilterRunning);
	
	// Only create worker threads if a CPU filter is actually active. When the
	// filter is "None", defer thread creation until the user selects a filter
	// so that no idle worker threads are left running.
	const size_t initialThreadCount = (__vfFunc != NULL) ? _requestedThreadCount : 0;
	__SetThreadCount(initialThreadCount, __vfFunc, (int)_vfAttributes.scaleDivide);
	
	SetSourceSize(srcWidth, srcHeight);
}

void VideoFilter::__SetThreadCount(const size_t threadCount, const VideoFilterFunc filterFunction, const int lineBlockSize)
{
	ThreadLockLock(&this->_lockSrc);
	ThreadLockLock(&this->_lockDst);
	
	// Destroy existing worker threads if the requested count changed. Holding
	// the source and destination locks guarantees that RunFilter() is not
	// dispatching work to these threads while they are being torn down.
	const size_t currentThreadCount = this->__vfThread.size();
	if (currentThreadCount != threadCount)
	{
		for (size_t i = 0; i < currentThreadCount; i++)
		{
			this->__vfThread[i].task->finish();
			this->__vfThread[i].task->shutdown();
			delete this->__vfThread[i].task;
			this->__vfThread[i].task = NULL;
		}
		
		this->__vfThread.clear();
		this->__vfThread.resize(threadCount);
		
		for (size_t i = 0; i < threadCount; i++)
		{
			this->__vfThread[i].task = NULL;
		}
	}
	
	for (size_t i = 0; i < threadCount; i++)
	{
		this->__vfThread[i].param.filterFunction = filterFunction;
		
		if (this->__vfThread[i].task == NULL)
		{
			char name[16];
			snprintf(name, 16, "video filter %d", (int)i);
			this->__vfThread[i].task = new Task;
			this->__vfThread[i].task->start(false, 0, name);
		}
	}
	
	this->__UpdateThreadSrcSurfaces(lineBlockSize);
	this->__UpdateThreadDstSurfaces();
	
	ThreadLockUnlock(&this->_lockDst);
	ThreadLockUnlock(&this->_lockSrc);
}

void VideoFilter::__UpdateThreadSrcSurfaces(const int lineBlockSize)
{
	const size_t threadCount = this->__vfThread.size();
	if (threadCount == 0)
	{
		return;
	}
	
	// Some filters (the 1.5x ones) process the source in blocks of lines
	// rather than one line at a time. Give each thread a whole number of
	// blocks so that the number of destination lines it produces always
	// matches its assigned destination range.
	const int blockSize = (lineBlockSize > 1) ? lineBlockSize : 1;
	const int srcHeight = this->__vfSrcSurface.Height;
	const int totalBlocks = srcHeight / blockSize;
	const int blocksPerThread = (threadCount > 1) ? totalBlocks/(int)threadCount : totalBlocks;
	const int remainingLinesAtEnd = srcHeight % blockSize;
	int remainingBlocks = totalBlocks;
	int srcStart = 0;
	
	for (size_t i = 0; i < threadCount; i++)
	{
		int blockCount = (blocksPerThread < remainingBlocks) ? blocksPerThread : remainingBlocks;
		remainingBlocks -= blockCount;
		
		// Add any remaining blocks to the last thread.
		if (i == threadCount-1)
		{
			blockCount += remainingBlocks;
		}
		
		int srcLines = blockCount * blockSize;
		if (i == threadCount-1)
		{
			srcLines += remainingLinesAtEnd;
		}
		
		SSurface &threadSrcSurface = this->__vfThread[i].param.srcSurface;
		threadSrcSurface = this->__vfSrcSurface;
		threadSrcSurface.Height = srcLines;
		threadSrcSurface.Surface = (this->__vfSrcSurface.Surface != NULL)
		                         ? (unsigned char *)((uint32_t *)this->__vfSrcSurface.Surface + ((size_t)srcStart * (size_t)this->__vfSrcSurface.Width))
		                         : NULL;
		
		srcStart += srcLines;
	}
}

void VideoFilter::__UpdateThreadDstSurfaces()
{
	const size_t threadCount = this->__vfThread.size();
	if (threadCount == 0)
	{
		return;
	}
	
	const int srcHeight = this->__vfSrcSurface.Height;
	const int dstHeight = this->__vfDstSurface.Height;
	const int dstWidth  = this->__vfDstSurface.Width;
	
	// Derive each thread's destination range from its source range. Dividing
	// the destination height independently would make the source and
	// destination ranges disagree whenever the height isn't evenly divisible
	// by the thread count, causing filters to write outside their assigned
	// destination region.
	int srcStart = 0;
	
	for (size_t i = 0; i < threadCount; i++)
	{
		const int srcLines = this->__vfThread[i].param.srcSurface.Height;
		const int srcEnd = srcStart + srcLines;
		const int dstStart = (srcHeight > 0) ? (int)(((long long)srcStart * dstHeight) / srcHeight) : 0;
		const int dstEnd   = (srcHeight > 0) ? (int)(((long long)srcEnd   * dstHeight) / srcHeight) : 0;
		
		SSurface &threadDstSurface = this->__vfThread[i].param.dstSurface;
		threadDstSurface = this->__vfDstSurface;
		threadDstSurface.Height = dstEnd - dstStart;
		
		if (this->__vfDstSurface.Surface != NULL)
		{
			threadDstSurface.Surface = (unsigned char *)((uint32_t *)this->__vfDstSurface.Surface + ((size_t)dstStart * (size_t)dstWidth));
			
			for (size_t j = 0; j < FILTER_MAX_WORKING_SURFACE_COUNT; j++)
			{
				threadDstSurface.workingSurface[j] = (this->__vfDstSurface.workingSurface[j] != NULL)
				                                   ? (unsigned char *)((uint32_t *)this->__vfDstSurface.workingSurface[j] + ((size_t)dstStart * (size_t)dstWidth))
				                                   : NULL;
			}
		}
		else
		{
			threadDstSurface.Surface = NULL;
			
			for (size_t j = 0; j < FILTER_MAX_WORKING_SURFACE_COUNT; j++)
			{
				threadDstSurface.workingSurface[j] = NULL;
			}
		}
		
		srcStart = srcEnd;
	}
}

bool VideoFilter::__AllocateDstBuffer(const size_t dstWidth, const size_t dstHeight, const size_t workingSurfaceCount)
{
	bool result = false;
	unsigned char *newSurfaceBuffer = NULL;
	
	// Allocate the destination buffer if necessary.
	if (_useInternalDstBuffer)
	{
		newSurfaceBuffer = (unsigned char *)malloc_alignedPage(dstWidth * dstHeight * sizeof(uint32_t));
		if (newSurfaceBuffer == NULL)
		{
			return result;
		}
		memset(newSurfaceBuffer, 0, dstWidth * dstHeight * sizeof(uint32_t));
	}
	
	ThreadLockLock(&this->_lockDst);
	
	for (size_t i = 0; i < FILTER_MAX_WORKING_SURFACE_COUNT; i++)
	{
		unsigned char *oldWorkingSurface = this->__vfDstSurface.workingSurface[i];
		this->__vfDstSurface.workingSurface[i] = (i < workingSurfaceCount) ? (unsigned char *)malloc_alignedPage(dstWidth * dstHeight * sizeof(uint32_t)) : NULL;
		free_aligned(oldWorkingSurface);
		
		if (this->__vfDstSurface.workingSurface[i] != NULL)
		{
			memset(this->__vfDstSurface.workingSurface[i], 0, dstWidth * dstHeight * sizeof(uint32_t));
		}
	}
	
	// Set up SSurface structure.
	this->__vfDstSurface.Width  = (int)dstWidth;
	this->__vfDstSurface.Height = (int)dstHeight;
	this->__vfDstSurface.Pitch  = (int)(dstWidth * 2);
	
	if (_useInternalDstBuffer)
	{
		unsigned char *oldBuffer = this->__vfDstSurface.Surface;
		this->__vfDstSurface.Surface = newSurfaceBuffer;
		free_aligned(oldBuffer);
	}
	
	// Update the surfaces on threads.
	this->__UpdateThreadDstSurfaces();
	
	ThreadLockUnlock(&this->_lockDst);
	
	result = true;
	return result;
}

/********************************************************************************************
	SetSourceSize()

	Sets the rectangular size of the source image. This method reallocates memory based
	on the input size, and therefore returns a bool value upon success or failure of
	resizing.

	Takes:
		width - The width of the source image in pixels.
		height - The height of the source image in pixels.

	Returns:
		A bool that reports if the resizing was successful. A value of true means success,
		while a value of false means failure.
 ********************************************************************************************/
bool VideoFilter::SetSourceSize(const size_t width, const size_t height)
{
	bool result = false;
	bool sizeChanged = false;
	const VideoFilterAttributes vfAttr = this->GetAttributes();
	
	ThreadLockLock(&this->_lockSrc);
	
	// Overallocate the source buffer by 8 rows of pixels to account for out-of-bounds
	// memory reads done by some filters.
	uint32_t *newPixBuffer = (uint32_t *)malloc_alignedPage(width * (height + 8) * sizeof(uint32_t));
	if (newPixBuffer == NULL)
	{
		ThreadLockUnlock(&this->_lockSrc);
		return result;
	}
	memset(newPixBuffer, 0, width * (height + 8) * sizeof(uint32_t));
	
	if ( (this->__vfSrcSurface.Surface == NULL) || ((size_t)this->__vfSrcSurface.Width != width) || ((size_t)this->__vfSrcSurface.Height != height) )
	{
		sizeChanged = true;
	}
	
	this->__vfSrcSurface.Width  = (int)width;
	this->__vfSrcSurface.Height = (int)height;
	this->__vfSrcSurface.Pitch  = (int)(width * 2);
	// Set the working source buffer pointer so that the working memory block is padded
	// with 4 pixel rows worth of memory on both sides.
	this->__vfSrcSurface.Surface = (unsigned char *)(newPixBuffer + (width * 4));
	
	free_aligned(this->__vfSrcSurfacePixBuffer);
	this->__vfSrcSurfacePixBuffer = newPixBuffer;
	
	// Update the surfaces on threads.
	this->__UpdateThreadSrcSurfaces((int)vfAttr.scaleDivide);
	
	ThreadLockUnlock(&this->_lockSrc);
	
	if (sizeChanged)
	{
		const size_t dstWidth  = width  * vfAttr.scaleMultiply / vfAttr.scaleDivide;
		const size_t dstHeight = height * vfAttr.scaleMultiply / vfAttr.scaleDivide;
		
		this->_pixelScale = (float)vfAttr.scaleMultiply / (float)vfAttr.scaleDivide;
		
		result = this->__AllocateDstBuffer(dstWidth, dstHeight, vfAttr.workingSurfaceCount);
		if (!result)
		{
			return result;
		}
	}
	
	result = true;
	return result;
}

/********************************************************************************************
	ChangeFilterByID()

	Changes the video filter type using a VideoFilterTypeID.

	Takes:
		typeID - The type ID of the video filter. See the VideoFilterTypeID
			enumeration for possible values.

	Returns:
		A bool that reports if the filter change was successful. A value of true means
		success, while a value of false means failure.
 ********************************************************************************************/
bool VideoFilter::ChangeFilterByID(const VideoFilterTypeID typeID)
{
	bool result = false;
	
	if (typeID >= VideoFilterTypeIDCount)
	{
		return result;
	}
	
	result = this->ChangeFilterByAttributes(VideoFilterAttributesList[typeID]);
	
	return result;
}

/********************************************************************************************
	ChangeFilterByAttributes()

	Changes the video filter type using video filter attributes.

	Takes:
		vfAttr - The video filter's attributes.

	Returns:
		A bool that reports if the filter change was successful. A value of true means
		success, while a value of false means failure.
 ********************************************************************************************/
void VideoFilter::SetThreadCount(const size_t threadCount)
{
	ThreadLockLock(&this->_lockAttributes);
	this->_requestedThreadCount = threadCount;
	ThreadLockUnlock(&this->_lockAttributes);
	
	// Only keep worker threads alive while a CPU filter is active. When the
	// filter is "None", remember the requested count for the next filter.
	const VideoFilterAttributes vfAttr = this->GetAttributes();
	const size_t activeThreadCount = (vfAttr.filterFunction != NULL) ? threadCount : 0;
	this->__SetThreadCount(activeThreadCount, vfAttr.filterFunction, (int)vfAttr.scaleDivide);
}

bool VideoFilter::ChangeFilterByAttributes(const VideoFilterAttributes &vfAttr)
{
	bool result = false;
	
	if (vfAttr.scaleMultiply == 0 || vfAttr.scaleDivide < 1)
	{
		return result;
	}
	
	// Worker threads are only needed for an actual CPU filter. Create them when
	// a filter is selected, and tear them down again when switching back to
	// VideoFilterTypeID_None.
	const size_t threadCount = (vfAttr.filterFunction != NULL) ? this->_requestedThreadCount : 0;
	this->__SetThreadCount(threadCount, vfAttr.filterFunction, (int)vfAttr.scaleDivide);
	
	ThreadLockLock(&this->_lockDst);
	unsigned char *dstSurface = this->__vfDstSurface.Surface;
	ThreadLockUnlock(&this->_lockDst);
	
	const VideoFilterAttributes currentAttr = this->GetAttributes();
	
	if (dstSurface != NULL &&
		currentAttr.scaleMultiply == vfAttr.scaleMultiply &&
		currentAttr.scaleDivide == vfAttr.scaleDivide &&
		currentAttr.workingSurfaceCount == vfAttr.workingSurfaceCount)
	{
		// If we have existing buffers and the new size is identical to the old size, we
		// can skip the costly construction of the buffers and simply clear them instead.
		
		ThreadLockLock(&this->_lockDst);
		
		const size_t bufferSizeBytes = this->__vfDstSurface.Width * this->__vfDstSurface.Height * sizeof(uint32_t);
		
		memset(this->__vfDstSurface.Surface, 0, bufferSizeBytes);
		for (size_t i = 0; i < currentAttr.workingSurfaceCount; i++)
		{
			memset(this->__vfDstSurface.workingSurface[i], 0, bufferSizeBytes);
		}
		
		this->__vfFunc = vfAttr.filterFunction;
		
		ThreadLockUnlock(&this->_lockDst);
	}
	else
	{
		// Construct a new destination buffer per filter attributes.
		ThreadLockLock(&this->_lockSrc);
		const size_t dstWidth = this->__vfSrcSurface.Width * vfAttr.scaleMultiply / vfAttr.scaleDivide;
		const size_t dstHeight = this->__vfSrcSurface.Height * vfAttr.scaleMultiply / vfAttr.scaleDivide;
		ThreadLockUnlock(&this->_lockSrc);
		
		ThreadLockLock(&this->_lockDst);
		
		this->__vfFunc = vfAttr.filterFunction;
		
		ThreadLockUnlock(&this->_lockDst);
		
		result = this->__AllocateDstBuffer(dstWidth, dstHeight, vfAttr.workingSurfaceCount);
		if (!result)
		{
			return result;
		}
	}
	
	ThreadLockLock(&this->_lockAttributes);
	this->_vfAttributes = vfAttr;
	ThreadLockUnlock(&this->_lockAttributes);
	
	result = true;
	return result;
}

/********************************************************************************************
	RunFilter()

	Runs the pixels in the source buffer through the video filter, and then stores
	the resulting pixels into the destination buffer.

	Takes:
		Nothing.

	Returns:
		A pointer to the destination buffer.
 ********************************************************************************************/
uint32_t* VideoFilter::RunFilter()
{
	ThreadLockLock(&this->_lockSrc);
	ThreadLockLock(&this->_lockDst);
	
	this->__isCPUFilterRunning = true;
	
	if (this->__vfFunc == NULL)
	{
		memcpy(this->__vfDstSurface.Surface, this->__vfSrcSurface.Surface, this->__vfDstSurface.Width * this->__vfDstSurface.Height * sizeof(uint32_t));
	}
	else
	{
		const size_t threadCount = this->__vfThread.size();
		if (threadCount > 0)
		{
			// Skip threads that were given no work. Some filters write whole
			// line pairs unconditionally, so calling them with a zero-height
			// surface would write past the end of their assigned range.
			for (size_t i = 0; i < threadCount; i++)
			{
				VideoFilterThreadParam &param = this->__vfThread[i].param;
				if (param.srcSurface.Height > 0 && param.dstSurface.Height > 0)
				{
					this->__vfThread[i].task->execute(&RunVideoFilterTask, &param);
				}
			}
			
			for (size_t i = 0; i < threadCount; i++)
			{
				VideoFilterThreadParam &param = this->__vfThread[i].param;
				if (param.srcSurface.Height > 0 && param.dstSurface.Height > 0)
				{
					this->__vfThread[i].task->finish();
				}
			}
		}
		else
		{
			this->__vfFunc(this->__vfSrcSurface, this->__vfDstSurface);
		}
	}
	
	this->__isCPUFilterRunning = false;
	ThreadCondSignal(&this->__condCPUFilterRunning);
	
	ThreadLockUnlock(&this->_lockDst);
	ThreadLockUnlock(&this->_lockSrc);
	
	return (uint32_t *)this->__vfDstSurface.Surface;
}

/********************************************************************************************
	RunFilterCustomByID() - STATIC
	
	Runs the pixels from srcBuffer through the video filter, and then stores the
	resulting pixels into dstBuffer. This function is useful for when your source or
	destination buffers are already established, or when you want to run a filter once
	without having to instantiate a new filter object.
	
	Takes:
		srcBuffer - A pointer to the source pixel buffer. The caller is responsible
			for ensuring that this buffer is valid. Also note that certain video filters
			may do out-of-bounds reads, so the caller is responsible for overallocating
			this buffer in order to avoid crashing on certain platforms.
		
		dstBuffer - A pointer to the destination pixel buffer. The caller is responsible
			for ensuring that this buffer is valid and large enough to store all of the
			destination pixels.
		
		srcWidth - The source surface width in pixels.
		
		srcHeight - The source surface height in pixels.
		
		typeID - The type ID of the video filter. See the VideoFilterTypeID
			enumeration for possible values.
	
	Returns:
		Nothing.
 ********************************************************************************************/
void VideoFilter::RunFilterCustomByID(const uint32_t *__restrict srcBuffer, uint32_t *__restrict dstBuffer,
									  const size_t srcWidth, const size_t srcHeight,
									  const VideoFilterTypeID typeID)
{
	if (typeID >= VideoFilterTypeIDCount)
	{
		return;
	}
	
	VideoFilter::RunFilterCustomByAttributes(srcBuffer, dstBuffer, srcWidth, srcHeight, VideoFilterAttributesList[typeID]);
}

/********************************************************************************************
	RunFilterCustomByAttributes() - STATIC

	Runs the pixels from srcBuffer through the video filter, and then stores the
	resulting pixels into dstBuffer. This function is useful for when your source or
	destination buffers are already established, or when you want to run a filter once
	without having to instantiate a new filter object.

	Takes:
		srcBuffer - A pointer to the source pixel buffer. The caller is responsible
			for ensuring that this buffer is valid. Also note that certain video filters
			may do out-of-bounds reads, so the caller is responsible for overallocating
			this buffer in order to avoid crashing on certain platforms.

		dstBuffer - A pointer to the destination pixel buffer. The caller is responsible
			for ensuring that this buffer is valid and large enough to store all of the
			destination pixels.

		srcWidth - The source surface width in pixels.

		srcHeight - The source surface height in pixels.

		vfAttr - The video filter's attributes.

	Returns:
		Nothing.
 ********************************************************************************************/
void VideoFilter::RunFilterCustomByAttributes(const uint32_t *__restrict srcBuffer, uint32_t *__restrict dstBuffer,
											  const size_t srcWidth, const size_t srcHeight,
											  const VideoFilterAttributes &vfAttr)
{
	// Parameter check
	if (srcBuffer == NULL ||
		dstBuffer == NULL ||
		srcWidth < 1 ||
		srcHeight < 1 ||
		vfAttr.scaleMultiply == 0 ||
		vfAttr.scaleDivide < 1)
	{
		return;
	}
	
	// Get the filter attributes
	const size_t dstWidth = srcWidth * vfAttr.scaleMultiply / vfAttr.scaleDivide;
	const size_t dstHeight = srcHeight * vfAttr.scaleMultiply / vfAttr.scaleDivide;
	const VideoFilterFunc filterFunction = vfAttr.filterFunction;
	
	// Assign the surfaces and run the filter
	SSurface srcSurface;
	memset(&srcSurface, 0, sizeof(srcSurface));
	srcSurface.Surface = (unsigned char *)srcBuffer;
	srcSurface.Pitch   = (int)(srcWidth * 2);
	srcSurface.Width   = (int)srcWidth;
	srcSurface.Height  = (int)srcHeight;
	
	SSurface dstSurface;
	memset(&dstSurface, 0, sizeof(dstSurface));
	dstSurface.Surface = (unsigned char *)dstBuffer;
	dstSurface.Pitch   = (int)(dstWidth * 2);
	dstSurface.Width   = (int)dstWidth;
	dstSurface.Height  = (int)dstHeight;
	
	if (filterFunction == NULL)
	{
		memcpy(dstBuffer, srcBuffer, dstWidth * dstHeight * sizeof(uint32_t));
	}
	else
	{
		filterFunction(srcSurface, dstSurface);
	}
}

/********************************************************************************************
	GetAttributesByID() - STATIC

	Returns the filter attributes associated with the given type ID.

	Takes:
		typeID - The type ID of the video filter. See the VideoFilterTypeID
			enumeration for possible values.

	Returns:
		A copy of the filter attributes of the given type ID. If typeID is
		invalid, this method returns the attributes of VideoFilterTypeID_None.
 ********************************************************************************************/
VideoFilterAttributes VideoFilter::GetAttributesByID(const VideoFilterTypeID typeID)
{
	if (typeID >= VideoFilterTypeIDCount)
	{
		return VideoFilterAttributesList[VideoFilterTypeID_None];
	}
	
	return VideoFilterAttributesList[typeID];
}

/********************************************************************************************
	GetTypeStringByID() - STATIC

	Returns a C-string representation of the passed in video filter type.

	Takes:
		typeID - The type ID of the video filter. See the VideoFilterTypeID
			enumeration for possible values.

	Returns:
		A C-string that represents the video filter type. If typeID is invalid,
		this method returns the string "Unknown".
 ********************************************************************************************/
const char* VideoFilter::GetTypeStringByID(const VideoFilterTypeID typeID)
{
	if (typeID >= VideoFilterTypeIDCount)
	{
		return VIDEOFILTERTYPE_UNKNOWN_STRING;
	}
	
	return VideoFilterAttributesList[typeID].typeString;
}

/********************************************************************************************
	ACCESSORS
 ********************************************************************************************/
VideoFilterAttributes VideoFilter::GetAttributes()
{
	ThreadLockLock(&this->_lockAttributes);
	VideoFilterAttributes vfAttr = this->_vfAttributes;
	ThreadLockUnlock(&this->_lockAttributes);
	
	return vfAttr;
}

VideoFilterTypeID VideoFilter::GetTypeID()
{
	ThreadLockLock(&this->_lockAttributes);
	VideoFilterTypeID typeID = this->_vfAttributes.typeID;
	ThreadLockUnlock(&this->_lockAttributes);
	
	return typeID;
}

const char* VideoFilter::GetTypeString()
{
	ThreadLockLock(&this->_lockAttributes);
	const char *typeString = this->_vfAttributes.typeString;
	ThreadLockUnlock(&this->_lockAttributes);
	
	return typeString;
}

size_t VideoFilter::GetThreadCount() const
{
	return this->__vfThread.size();
}

uint32_t* VideoFilter::GetSrcBufferPtr()
{
	ThreadLockLock(&this->_lockSrc);
	uint32_t *ptr = (uint32_t *)this->__vfSrcSurface.Surface;
	ThreadLockUnlock(&this->_lockSrc);
	
	return ptr;
}

uint32_t* VideoFilter::GetDstBufferPtr()
{
	ThreadLockLock(&this->_lockDst);
	uint32_t *ptr = (uint32_t *)this->__vfDstSurface.Surface;
	ThreadLockUnlock(&this->_lockDst);
	
	return ptr;
}

/********************************************************************************************
	SetDstBufferPtr()

	Sets a user-defined buffer for the video filter's target destination pixel
 	buffer.

	Takes:
		pageAlignedBuffer - A page-aligned pointer to a user-defined pixel buffer,
 			usually allocated by using malloc_alignedPage(). If a value of NULL is
			passed in, then VideoFilter will use its own internal buffer (this is the
			default behavior). Otherwise, VideoFilter will use the passed in pointer
			for the destination buffer. The caller is responsible for ensuring that
			this buffer is valid and large enough to store all of the destination
			pixels under all circumstances. Methods that may change the destination
			buffer's size requirements are:
				SetSourceSize()
				ChangeFilterByID()
				ChangeFilterByAttributes()

	Returns:
		Nothing.
 ********************************************************************************************/
void VideoFilter::SetDstBufferPtr(uint32_t *pageAlignedBuffer)
{
	ThreadLockLock(&this->_lockDst);
	
	if (pageAlignedBuffer == NULL)
	{
		this->_useInternalDstBuffer = true;
	}
	else
	{
		unsigned char *oldDstBuffer = this->__vfDstSurface.Surface;
		this->__vfDstSurface.Surface = (unsigned char *)pageAlignedBuffer;
		
		if (this->_useInternalDstBuffer)
		{
			free_aligned(oldDstBuffer);
		}
		
		this->_useInternalDstBuffer = false;
	}
	
	ThreadLockUnlock(&this->_lockDst);
	
	this->__AllocateDstBuffer(this->__vfDstSurface.Width, this->__vfDstSurface.Height, this->_vfAttributes.workingSurfaceCount);
}

size_t VideoFilter::GetSrcWidth()
{
	ThreadLockLock(&this->_lockSrc);
	size_t width = this->__vfSrcSurface.Width;
	ThreadLockUnlock(&this->_lockSrc);
	
	return width;
}

size_t VideoFilter::GetSrcHeight()
{
	ThreadLockLock(&this->_lockSrc);
	size_t height = this->__vfSrcSurface.Height;
	ThreadLockUnlock(&this->_lockSrc);
	
	return height;
}

size_t VideoFilter::GetDstWidth()
{
	ThreadLockLock(&this->_lockDst);
	size_t width = this->__vfDstSurface.Width;
	ThreadLockUnlock(&this->_lockDst);
	
	return width;
}

size_t VideoFilter::GetDstHeight()
{
	ThreadLockLock(&this->_lockDst);
	size_t height = this->__vfDstSurface.Height;
	ThreadLockUnlock(&this->_lockDst);
	
	return height;
}

float VideoFilter::GetPixelScale()
{
	return this->_pixelScale;
}

VideoFilterParamType VideoFilter::GetFilterParameterType(VideoFilterParamID paramID) const
{
	return _VideoFilterParamAttributesList[paramID].type;
}

int VideoFilter::GetFilterParameteri(VideoFilterParamID paramID)
{
	int value = 0;
	
	ThreadLockLock(&this->_lockDst);
	
	switch (_VideoFilterParamAttributesList[paramID].type)
	{
		case VF_INT:
			value = (int)(*((int *)_VideoFilterParamAttributesList[paramID].index));
			break;
			
		case VF_UINT:
			value = (int)(*((unsigned int *)_VideoFilterParamAttributesList[paramID].index));
			break;
			
		case VF_FLOAT:
			value = (int)(*((float *)_VideoFilterParamAttributesList[paramID].index));
			break;
			
		default:
			break;
	}
	
	ThreadLockUnlock(&this->_lockDst);
	
	return value;
}

unsigned int VideoFilter::GetFilterParameterui(VideoFilterParamID paramID)
{
	unsigned int value = 0;
	
	ThreadLockLock(&this->_lockDst);
	
	switch (_VideoFilterParamAttributesList[paramID].type)
	{
		case VF_INT:
			value = (unsigned int)(*((int *)_VideoFilterParamAttributesList[paramID].index));
			break;
			
		case VF_UINT:
			value = (unsigned int)(*((unsigned int *)_VideoFilterParamAttributesList[paramID].index));
			break;
			
		case VF_FLOAT:
			value = (unsigned int)(*((float *)_VideoFilterParamAttributesList[paramID].index));
			break;
			
		default:
			break;
	}
	
	ThreadLockUnlock(&this->_lockDst);
	
	return value;
}

float VideoFilter::GetFilterParameterf(VideoFilterParamID paramID)
{
	float value = 0.0f;
	
	ThreadLockLock(&this->_lockDst);
	
	switch (_VideoFilterParamAttributesList[paramID].type)
	{
		case VF_INT:
			value = (float)(*((int *)_VideoFilterParamAttributesList[paramID].index));
			break;
			
		case VF_UINT:
			value = (float)(*((unsigned int *)_VideoFilterParamAttributesList[paramID].index));
			break;
			
		case VF_FLOAT:
			value = (float)(*((float *)_VideoFilterParamAttributesList[paramID].index));
			break;
			
		default:
			break;
	}
	
	ThreadLockUnlock(&this->_lockDst);
	
	return value;
}

void VideoFilter::SetFilterParameteri(VideoFilterParamID paramID, int value)
{
	if (paramID >= VideoFilterParamIDCount)
	{
		return;
	}
	
	ThreadLockLock(&this->_lockDst);
	
	switch (_VideoFilterParamAttributesList[paramID].type)
	{
		case VF_INT:
			*((int *)_VideoFilterParamAttributesList[paramID].index) = (int)value;
			break;
			
		case VF_UINT:
			*((unsigned int *)_VideoFilterParamAttributesList[paramID].index) = (unsigned int)value;
			break;
			
		case VF_FLOAT:
			*((float *)_VideoFilterParamAttributesList[paramID].index) = (float)value;
			break;
			
		default:
			break;
	}
	
	ThreadLockUnlock(&this->_lockDst);
}

void VideoFilter::SetFilterParameterui(VideoFilterParamID paramID, unsigned int value)
{
	if (paramID >= VideoFilterParamIDCount)
	{
		return;
	}
	
	ThreadLockLock(&this->_lockDst);
	
	switch (_VideoFilterParamAttributesList[paramID].type)
	{
		case VF_INT:
			*((int *)_VideoFilterParamAttributesList[paramID].index) = (int)value;
			break;
			
		case VF_UINT:
			*((unsigned int *)_VideoFilterParamAttributesList[paramID].index) = (unsigned int)value;
			break;
			
		case VF_FLOAT:
			*((float *)_VideoFilterParamAttributesList[paramID].index) = (float)value;
			break;
			
		default:
			break;
	}
	
	ThreadLockUnlock(&this->_lockDst);
}

void VideoFilter::SetFilterParameterf(VideoFilterParamID paramID, float value)
{
	if (paramID >= VideoFilterParamIDCount)
	{
		return;
	}
	
	ThreadLockLock(&this->_lockDst);
	
	switch (_VideoFilterParamAttributesList[paramID].type)
	{
		case VF_INT:
			*((int *)_VideoFilterParamAttributesList[paramID].index) = (int)value;
			break;
			
		case VF_UINT:
			*((unsigned int *)_VideoFilterParamAttributesList[paramID].index) = (unsigned int)value;
			break;
			
		case VF_FLOAT:
			*((float *)_VideoFilterParamAttributesList[paramID].index) = (float)value;
			break;
			
		default:
			break;
	}
	
	ThreadLockUnlock(&this->_lockDst);
}

// Task function for multithreaded filtering
static void* RunVideoFilterTask(void *arg)
{
	VideoFilterThreadParam *param = (VideoFilterThreadParam *)arg;
	
	param->filterFunction(param->srcSurface, param->dstSurface);
	
	return NULL;
}

#ifdef HOST_WINDOWS 
void ThreadLockInit(ThreadLock *theLock)
{
	InitializeCriticalSection(theLock);
}

void ThreadLockDestroy(ThreadLock *theLock)
{
	DeleteCriticalSection(theLock);
}

void ThreadLockLock(ThreadLock *theLock)
{
	EnterCriticalSection(theLock);
}

void ThreadLockUnlock(ThreadLock *theLock)
{
	LeaveCriticalSection(theLock);
}

void ThreadCondInit(ThreadCond *theCondition)
{
	*theCondition = CreateEvent(NULL, FALSE, FALSE, NULL);
}

void ThreadCondDestroy(ThreadCond *theCondition)
{
	CloseHandle(*theCondition);
}

void ThreadCondWait(ThreadCond *theCondition, ThreadLock *conditionLock)
{
	WaitForSingleObject(*theCondition, INFINITE);
}

void ThreadCondSignal(ThreadCond *theCondition)
{
	SetEvent(*theCondition);
}

#else
void ThreadLockInit(ThreadLock *theLock)
{
	pthread_mutex_init(theLock, NULL);
}

void ThreadLockDestroy(ThreadLock *theLock)
{
	pthread_mutex_destroy(theLock);
}

void ThreadLockLock(ThreadLock *theLock)
{
	pthread_mutex_lock(theLock);
}

void ThreadLockUnlock(ThreadLock *theLock)
{
	pthread_mutex_unlock(theLock);
}

void ThreadCondInit(ThreadCond *theCondition)
{
	pthread_cond_init(theCondition, NULL);
}

void ThreadCondDestroy(ThreadCond *theCondition)
{
	pthread_cond_destroy(theCondition);
}

void ThreadCondWait(ThreadCond *theCondition, ThreadLock *conditionLock)
{
	pthread_cond_wait(theCondition, conditionLock);
}

void ThreadCondSignal(ThreadCond *theCondition)
{
	pthread_cond_signal(theCondition);
}
#endif
