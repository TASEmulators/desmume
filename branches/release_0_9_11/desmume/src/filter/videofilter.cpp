/*
	Copyright (C) 2011-2012 Roger Manuel
	Copyright (C) 2013-2015 DeSmuME team

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
	{VideoFilterTypeID_5xBRZ,			"5xBRZ",			&Render5xBRZ,					5,	1,	0} };

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
VideoFilter::VideoFilter(size_t srcWidth,
						 size_t srcHeight,
						 VideoFilterTypeID typeID = VideoFilterTypeID_None,
						 size_t threadCount = 0)
{
	SSurface newSurface;
	newSurface.Surface = NULL;
	newSurface.Pitch = srcWidth*2;
	newSurface.Width = srcWidth;
	newSurface.Height = srcHeight;
	newSurface.userData = NULL;
	
	for (size_t i = 0; i < FILTER_MAX_WORKING_SURFACE_COUNT; i++)
	{
		newSurface.workingSurface[i] = NULL;
	}
	
	_vfSrcSurface = newSurface;
	_vfDstSurface = newSurface;
	
	_useInternalDstBuffer = true;
	_isFilterRunning = false;
	_vfSrcSurfacePixBuffer = NULL;
	
	if (typeID < VideoFilterTypeIDCount)
	{
		_vfAttributes = VideoFilterAttributesList[typeID];
	}
	else
	{
		_vfAttributes = VideoFilterAttributesList[VideoFilterTypeID_None];
	}
	
	ThreadLockInit(&_lockSrc);
	ThreadLockInit(&_lockDst);
	ThreadLockInit(&_lockAttributes);
	ThreadCondInit(&_condRunning);
	
	// Create all threads
	_vfThread.resize(threadCount);
	
	for (size_t i = 0; i < threadCount; i++)
	{
		_vfThread[i].param.srcSurface = _vfSrcSurface;
		_vfThread[i].param.dstSurface = _vfDstSurface;
		_vfThread[i].param.filterFunction = NULL;
		
		_vfThread[i].task = new Task;
		_vfThread[i].task->start(false);
	}
	
	_vfFunc = _vfAttributes.filterFunction;
	SetSourceSize(srcWidth, srcHeight);
}

/********************************************************************************************
	CLASS DESTRUCTOR
 ********************************************************************************************/
VideoFilter::~VideoFilter()
{
	// Destroy all threads first
	for (size_t i = 0; i < _vfThread.size(); i++)
	{
		_vfThread[i].task->finish();
		_vfThread[i].task->shutdown();
		
		delete _vfThread[i].task;
	}
	
	_vfThread.clear();
	
	// Destroy everything else
	ThreadLockLock(&_lockSrc);
	ThreadLockLock(&_lockDst);
	
	while (_isFilterRunning)
	{
		ThreadCondWait(&_condRunning, &_lockDst);
	}
	
	if (_useInternalDstBuffer)
	{
		free(_vfDstSurface.Surface);
		_vfDstSurface.Surface = NULL;
	}
	
	for (size_t i = 0; i < _vfAttributes.workingSurfaceCount; i++)
	{
		free(_vfDstSurface.workingSurface[i]);
		_vfDstSurface.workingSurface[i] = NULL;
	}
	
	ThreadLockUnlock(&_lockDst);
	
	free(_vfSrcSurfacePixBuffer);
	_vfSrcSurfacePixBuffer = NULL;
	_vfSrcSurface.Surface = NULL;
	
	ThreadLockUnlock(&_lockSrc);
	
	ThreadLockDestroy(&_lockSrc);
	ThreadLockDestroy(&_lockDst);
	ThreadLockDestroy(&_lockAttributes);
	ThreadCondDestroy(&_condRunning);
}

bool VideoFilter::AllocateDstBuffer(const size_t dstWidth, const size_t dstHeight, const size_t workingSurfaceCount)
{
	bool result = false;
	unsigned char *newSurfaceBuffer = NULL;
	
	// Allocate the destination buffer if necessary.
	if (_useInternalDstBuffer)
	{
		newSurfaceBuffer = (unsigned char *)calloc(dstWidth * dstHeight, sizeof(uint32_t));
		if (newSurfaceBuffer == NULL)
		{
			return result;
		}
	}
	
	ThreadLockLock(&this->_lockDst);
	
	for (size_t i = 0; i < FILTER_MAX_WORKING_SURFACE_COUNT; i++)
	{
		unsigned char *oldWorkingSurface = this->_vfDstSurface.workingSurface[i];
		this->_vfDstSurface.workingSurface[i] = (i < workingSurfaceCount) ? (unsigned char *)calloc(dstWidth * dstHeight, sizeof(uint32_t)) : NULL;
		free(oldWorkingSurface);
	}
	
	// Set up SSurface structure.
	this->_vfDstSurface.Width = dstWidth;
	this->_vfDstSurface.Height = dstHeight;
	this->_vfDstSurface.Pitch = dstWidth * 2;
	
	if (_useInternalDstBuffer)
	{
		unsigned char *oldBuffer = this->_vfDstSurface.Surface;
		this->_vfDstSurface.Surface = newSurfaceBuffer;
		free(oldBuffer);
	}
	
	// Update the surfaces on threads.
	const size_t threadCount = this->_vfThread.size();
	const unsigned int linesPerThread = (threadCount > 1) ? dstHeight/threadCount : dstHeight;
	unsigned int remainingLines = dstHeight;
	
	for (size_t i = 0; i < threadCount; i++)
	{
		SSurface &threadDstSurface = this->_vfThread[i].param.dstSurface;
		threadDstSurface = this->_vfDstSurface;
		threadDstSurface.Height = (linesPerThread < remainingLines) ? linesPerThread : remainingLines;
		remainingLines -= threadDstSurface.Height;
		
		// Add any remaining lines to the last thread.
		if (i == threadCount-1)
		{
			threadDstSurface.Height += remainingLines;
		}
		
		if (i > 0)
		{
			SSurface &prevThreadDstSurface = this->_vfThread[i - 1].param.dstSurface;
			threadDstSurface.Surface = (unsigned char *)((uint32_t *)prevThreadDstSurface.Surface + (prevThreadDstSurface.Width * prevThreadDstSurface.Height));
			
			for (size_t j = 0; j < workingSurfaceCount; j++)
			{
				threadDstSurface.workingSurface[j] = (unsigned char *)((uint32_t *)prevThreadDstSurface.workingSurface[j] + (prevThreadDstSurface.Width * prevThreadDstSurface.Height));
			}
		}
	}
	
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
	
	ThreadLockLock(&this->_lockSrc);
	
	// Overallocate the source buffer by 8 rows of pixels to account for out-of-bounds
	// memory reads done by some filters.
	uint32_t *newPixBuffer = (uint32_t *)calloc(width * (height + 8), sizeof(uint32_t));
	if (newPixBuffer == NULL)
	{
		return result;
	}
	
	if (this->_vfSrcSurface.Surface == NULL || this->_vfSrcSurface.Width != width || this->_vfSrcSurface.Height != height)
	{
		sizeChanged = true;
	}
	
	this->_vfSrcSurface.Width = width;
	this->_vfSrcSurface.Height = height;
	this->_vfSrcSurface.Pitch = width * 2;
	// Set the working source buffer pointer so that the working memory block is padded
	// with 4 pixel rows worth of memory on both sides.
	this->_vfSrcSurface.Surface = (unsigned char *)(newPixBuffer + (width * 4));
	
	free(this->_vfSrcSurfacePixBuffer);
	this->_vfSrcSurfacePixBuffer = newPixBuffer;
	
	// Update the surfaces on threads.
	size_t threadCount = this->_vfThread.size();
	const unsigned int linesPerThread = (threadCount > 1) ? this->_vfSrcSurface.Height/threadCount : this->_vfSrcSurface.Height;
	unsigned int remainingLines = this->_vfSrcSurface.Height;
	
	for (size_t i = 0; i < threadCount; i++)
	{
		SSurface &threadSrcSurface = this->_vfThread[i].param.srcSurface;
		threadSrcSurface = this->_vfSrcSurface;
		threadSrcSurface.Height = (linesPerThread < remainingLines) ? linesPerThread : remainingLines;
		remainingLines -= threadSrcSurface.Height;
		
		// Add any remaining lines to the last thread.
		if (i == threadCount-1)
		{
			threadSrcSurface.Height += remainingLines;
		}
		
		if (i > 0)
		{
			SSurface &prevThreadSrcSurface = this->_vfThread[i - 1].param.srcSurface;
			threadSrcSurface.Surface = (unsigned char *)((uint32_t *)prevThreadSrcSurface.Surface + (prevThreadSrcSurface.Width * prevThreadSrcSurface.Height));
		}
	}
	
	ThreadLockUnlock(&this->_lockSrc);
	
	if (sizeChanged)
	{
		const VideoFilterAttributes vfAttr = this->GetAttributes();
		const size_t dstWidth = width * vfAttr.scaleMultiply / vfAttr.scaleDivide;
		const size_t dstHeight = height * vfAttr.scaleMultiply / vfAttr.scaleDivide;
		
		result = this->AllocateDstBuffer(dstWidth, dstHeight, vfAttr.workingSurfaceCount);
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
bool VideoFilter::ChangeFilterByAttributes(const VideoFilterAttributes &vfAttr)
{
	bool result = false;
	
	if (vfAttr.scaleMultiply == 0 || vfAttr.scaleDivide < 1)
	{
		return result;
	}
	
	ThreadLockLock(&this->_lockDst);
	unsigned char *dstSurface = this->_vfDstSurface.Surface;
	ThreadLockUnlock(&this->_lockDst);
	
	const VideoFilterAttributes currentAttr = this->GetAttributes();
	const size_t threadCount = this->_vfThread.size();
	
	if (dstSurface != NULL &&
		currentAttr.scaleMultiply == vfAttr.scaleMultiply &&
		currentAttr.scaleDivide == vfAttr.scaleDivide &&
		currentAttr.workingSurfaceCount == vfAttr.workingSurfaceCount)
	{
		// If we have existing buffers and the new size is identical to the old size, we
		// can skip the costly construction of the buffers and simply clear them instead.
		
		ThreadLockLock(&this->_lockDst);
		
		const size_t bufferSizeBytes = this->_vfDstSurface.Width * this->_vfDstSurface.Height * sizeof(uint32_t);
		
		memset(this->_vfDstSurface.Surface, 0, bufferSizeBytes);
		for (size_t i = 0; i < currentAttr.workingSurfaceCount; i++)
		{
			memset(this->_vfDstSurface.workingSurface[i], 0, bufferSizeBytes);
		}
		
		this->_vfFunc = vfAttr.filterFunction;
		for (size_t i = 0; i < threadCount; i++)
		{
			this->_vfThread[i].param.filterFunction = this->_vfFunc;
		}
		
		ThreadLockUnlock(&this->_lockDst);
	}
	else
	{
		// Construct a new destination buffer per filter attributes.
		ThreadLockLock(&this->_lockSrc);
		const size_t dstWidth = this->_vfSrcSurface.Width * vfAttr.scaleMultiply / vfAttr.scaleDivide;
		const size_t dstHeight = this->_vfSrcSurface.Height * vfAttr.scaleMultiply / vfAttr.scaleDivide;
		ThreadLockUnlock(&this->_lockSrc);
		
		ThreadLockLock(&this->_lockDst);
		
		this->_vfFunc = vfAttr.filterFunction;
		for (size_t i = 0; i < threadCount; i++)
		{
			this->_vfThread[i].param.filterFunction = this->_vfFunc;
		}
		
		ThreadLockUnlock(&this->_lockDst);
		
		result = this->AllocateDstBuffer(dstWidth, dstHeight, vfAttr.workingSurfaceCount);
		if (!result)
		{
			return result;
		}
	}
	
	this->SetAttributes(vfAttr);
	
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
	
	this->_isFilterRunning = true;
	
	if (this->_vfFunc == NULL)
	{
		memcpy(this->_vfDstSurface.Surface, this->_vfSrcSurface.Surface, this->_vfDstSurface.Width * this->_vfDstSurface.Height * sizeof(uint32_t));
	}
	else
	{
		const size_t threadCount = this->_vfThread.size();
		if (threadCount > 0)
		{
			for (size_t i = 0; i < threadCount; i++)
			{
				this->_vfThread[i].task->execute(&RunVideoFilterTask, &this->_vfThread[i].param);
			}
			
			for (size_t i = 0; i < threadCount; i++)
			{
				this->_vfThread[i].task->finish();
			}
		}
		else
		{
			this->_vfFunc(this->_vfSrcSurface, this->_vfDstSurface);
		}
	}
	
	this->_isFilterRunning = false;
	ThreadCondSignal(&this->_condRunning);
	
	ThreadLockUnlock(&this->_lockDst);
	ThreadLockUnlock(&this->_lockSrc);
	
	return (uint32_t *)this->_vfDstSurface.Surface;
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
	srcSurface.Surface = (unsigned char *)srcBuffer;
	srcSurface.Pitch = srcWidth*2;
	srcSurface.Width = srcWidth;
	srcSurface.Height = srcHeight;
	
	SSurface dstSurface;
	dstSurface.Surface = (unsigned char *)dstBuffer;
	dstSurface.Pitch = dstWidth*2;
	dstSurface.Width = dstWidth;
	dstSurface.Height = dstHeight;
	
	if (filterFunction == NULL)
	{
		memcpy(dstBuffer, srcBuffer, dstWidth * dstHeight * sizeof(uint32_t));
	}
	else
	{
		for (size_t i = 0; i < vfAttr.workingSurfaceCount; i++)
		{
			dstSurface.workingSurface[i] = (unsigned char *)calloc(dstWidth * dstHeight, sizeof(uint32_t));
		}
		
		filterFunction(srcSurface, dstSurface);
		
		for (size_t i = 0; i < vfAttr.workingSurfaceCount; i++)
		{
			free(dstSurface.workingSurface[i]);
		}
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

void VideoFilter::SetAttributes(const VideoFilterAttributes &vfAttr)
{
	ThreadLockLock(&this->_lockAttributes);
	this->_vfAttributes = vfAttr;
	ThreadLockUnlock(&this->_lockAttributes);
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

uint32_t* VideoFilter::GetSrcBufferPtr()
{
	ThreadLockLock(&this->_lockSrc);
	uint32_t *ptr = (uint32_t *)this->_vfSrcSurface.Surface;
	ThreadLockUnlock(&this->_lockSrc);
	
	return ptr;
}

uint32_t* VideoFilter::GetDstBufferPtr()
{
	ThreadLockLock(&this->_lockDst);
	uint32_t *ptr = (uint32_t *)this->_vfDstSurface.Surface;
	ThreadLockUnlock(&this->_lockDst);
	
	return ptr;
}

/********************************************************************************************
	SetDstBufferPtr()

	Returns a C-string representation of the passed in video filter type.

	Takes:
		theBuffer - A pointer to the destination pixel buffer. If a value of NULL is
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
void VideoFilter::SetDstBufferPtr(uint32_t *theBuffer)
{
	ThreadLockLock(&this->_lockDst);
	
	if (theBuffer == NULL)
	{
		this->_useInternalDstBuffer = true;
	}
	else
	{
		unsigned char *oldDstBuffer = this->_vfDstSurface.Surface;
		this->_vfDstSurface.Surface = (unsigned char *)theBuffer;
		
		if (this->_useInternalDstBuffer)
		{
			free(oldDstBuffer);
		}
		
		this->_useInternalDstBuffer = false;
	}
	
	ThreadLockUnlock(&this->_lockDst);
	
	this->AllocateDstBuffer(this->_vfDstSurface.Width, this->_vfDstSurface.Height, this->_vfAttributes.workingSurfaceCount);
}

size_t VideoFilter::GetSrcWidth()
{
	ThreadLockLock(&this->_lockSrc);
	size_t width = this->_vfSrcSurface.Width;
	ThreadLockUnlock(&this->_lockSrc);
	
	return width;
}

size_t VideoFilter::GetSrcHeight()
{
	ThreadLockLock(&this->_lockSrc);
	size_t height = this->_vfSrcSurface.Height;
	ThreadLockUnlock(&this->_lockSrc);
	
	return height;
}

size_t VideoFilter::GetDstWidth()
{
	ThreadLockLock(&this->_lockDst);
	size_t width = this->_vfDstSurface.Width;
	ThreadLockUnlock(&this->_lockDst);
	
	return width;
}

size_t VideoFilter::GetDstHeight()
{
	ThreadLockLock(&this->_lockDst);
	size_t height = this->_vfDstSurface.Height;
	ThreadLockUnlock(&this->_lockDst);
	
	return height;
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
