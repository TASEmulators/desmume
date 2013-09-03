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

#include "videofilter.h"
#include <string.h>

// Parameters for Scanline filter
int scanline_filter_a = 0;
int scanline_filter_b = 2;
int scanline_filter_c = 2;
int scanline_filter_d = 4;

// 
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
	
	_vfSrcSurface = newSurface;
	_vfDstSurface = newSurface;
	
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
	
	pthread_mutex_init(&_mutexSrc, NULL);
	pthread_mutex_init(&_mutexDst, NULL);
	pthread_mutex_init(&_mutexAttributes, NULL);
	pthread_cond_init(&_condRunning, NULL);
	
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
	pthread_mutex_lock(&_mutexSrc);
	pthread_mutex_lock(&this->_mutexDst);
	
	while (this->_isFilterRunning)
	{
		pthread_cond_wait(&this->_condRunning, &this->_mutexDst);
	}
	
	free(_vfDstSurface.Surface);
	_vfDstSurface.Surface = NULL;
	
	pthread_mutex_unlock(&_mutexDst);
	
	free(_vfSrcSurfacePixBuffer);
	_vfSrcSurfacePixBuffer = NULL;
	_vfSrcSurface.Surface = NULL;
	
	pthread_mutex_unlock(&_mutexSrc);
	
	pthread_mutex_destroy(&_mutexSrc);
	pthread_mutex_destroy(&_mutexDst);
	pthread_mutex_destroy(&_mutexAttributes);
	pthread_cond_destroy(&_condRunning);
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
	
	pthread_mutex_lock(&this->_mutexSrc);
	
	// Overallocate the source buffer by 8 rows of pixels to account for out-of-bounds
	// memory reads done by some filters.
	uint32_t *newPixBuffer = (uint32_t *)calloc(width * (height + 8), sizeof(uint32_t));
	if (newPixBuffer == NULL)
	{
		return result;
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
	
	for (size_t i = 0; i < threadCount; i++)
	{
		SSurface &threadSrcSurface = this->_vfThread[i].param.srcSurface;
		threadSrcSurface = this->_vfSrcSurface;
		threadSrcSurface.Height /= threadCount;
		
		if (i > 0)
		{
			SSurface &prevThreadSrcSurface = this->_vfThread[i - 1].param.srcSurface;
			threadSrcSurface.Surface = (unsigned char *)((uint32_t *)prevThreadSrcSurface.Surface + (prevThreadSrcSurface.Width * prevThreadSrcSurface.Height));
		}
	}
	
	pthread_mutex_unlock(&this->_mutexSrc);
	
	const VideoFilterAttributes vfAttr = this->GetAttributes();
	result = this->ChangeFilterByAttributes(&vfAttr);
	
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
	
	result = this->ChangeFilterByAttributes(&VideoFilterAttributesList[typeID]);
	
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
bool VideoFilter::ChangeFilterByAttributes(const VideoFilterAttributes *vfAttr)
{
	bool result = false;
	
	if (vfAttr == NULL)
	{
		return result;
	}
	
	if (vfAttr->scaleMultiply == 0 || vfAttr->scaleDivide < 1)
	{
		return result;
	}
	
	this->SetAttributes(*vfAttr);
	VideoFilterAttributes &workingAttributes = this->_vfAttributes;
	
	pthread_mutex_lock(&this->_mutexSrc);
	const size_t srcWidth = this->_vfSrcSurface.Width;
	const size_t srcHeight = this->_vfSrcSurface.Height;
	pthread_mutex_unlock(&this->_mutexSrc);
	
	const size_t dstWidth = srcWidth * workingAttributes.scaleMultiply / workingAttributes.scaleDivide;
	const size_t dstHeight = srcHeight * workingAttributes.scaleMultiply / workingAttributes.scaleDivide;
	const VideoFilterFunc filterFunction = workingAttributes.filterFunction;
	
	pthread_mutex_lock(&this->_mutexDst);
	
	uint32_t *newSurfaceBuffer = (uint32_t *)calloc(dstWidth * dstHeight, sizeof(uint32_t));
	if (newSurfaceBuffer == NULL)
	{
		return result;
	}
	
	this->_vfFunc = filterFunction;
	this->_vfDstSurface.Width = dstWidth;
	this->_vfDstSurface.Height = dstHeight;
	this->_vfDstSurface.Pitch = dstWidth * 2;
	
	free(this->_vfDstSurface.Surface);
	this->_vfDstSurface.Surface = (unsigned char *)newSurfaceBuffer;
	
	// Update the surfaces on threads.
	const size_t threadCount = this->_vfThread.size();
	
	for (size_t i = 0; i < threadCount; i++)
	{
		SSurface &threadDstSurface = this->_vfThread[i].param.dstSurface;
		threadDstSurface = this->_vfDstSurface;
		threadDstSurface.Height /= threadCount;
		
		if (i > 0)
		{
			SSurface &prevThreadDstSurface = this->_vfThread[i - 1].param.dstSurface;
			threadDstSurface.Surface = (unsigned char *)((uint32_t *)prevThreadDstSurface.Surface + (prevThreadDstSurface.Width * prevThreadDstSurface.Height));
		}
		
		this->_vfThread[i].param.filterFunction = this->_vfFunc;
	}
	
	pthread_mutex_unlock(&this->_mutexDst);
	
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
	pthread_mutex_lock(&this->_mutexSrc);
	pthread_mutex_lock(&this->_mutexDst);
	
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
	pthread_cond_signal(&this->_condRunning);
	
	pthread_mutex_unlock(&this->_mutexDst);
	pthread_mutex_unlock(&this->_mutexSrc);
	
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
void VideoFilter::RunFilterCustomByID(const uint32_t *__restrict__ srcBuffer, uint32_t *__restrict__ dstBuffer,
									  const size_t srcWidth, const size_t srcHeight,
									  const VideoFilterTypeID typeID)
{
	if (typeID >= VideoFilterTypeIDCount)
	{
		return;
	}
	
	VideoFilter::RunFilterCustomByAttributes(srcBuffer, dstBuffer, srcWidth, srcHeight, &VideoFilterAttributesList[typeID]);
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
void VideoFilter::RunFilterCustomByAttributes(const uint32_t *__restrict__ srcBuffer, uint32_t *__restrict__ dstBuffer,
											  const size_t srcWidth, const size_t srcHeight,
											  const VideoFilterAttributes *vfAttr)
{
	// Parameter check
	if (srcBuffer == NULL ||
		dstBuffer == NULL ||
		srcWidth < 1 ||
		srcHeight < 1 ||
		vfAttr == NULL ||
		vfAttr->scaleMultiply == 0 ||
		vfAttr->scaleDivide < 1)
	{
		return;
	}
	
	// Get the filter attributes
	const size_t dstWidth = srcWidth * vfAttr->scaleMultiply / vfAttr->scaleDivide;
	const size_t dstHeight = srcHeight * vfAttr->scaleMultiply / vfAttr->scaleDivide;
	const VideoFilterFunc filterFunction = vfAttr->filterFunction;
	
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
		filterFunction(srcSurface, dstSurface);
	}
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
	pthread_mutex_lock(&this->_mutexAttributes);
	VideoFilterAttributes vfAttr = this->_vfAttributes;
	pthread_mutex_unlock(&this->_mutexAttributes);
	
	return vfAttr;
}

void VideoFilter::SetAttributes(const VideoFilterAttributes vfAttr)
{
	pthread_mutex_lock(&this->_mutexAttributes);
	this->_vfAttributes = vfAttr;
	pthread_mutex_unlock(&this->_mutexAttributes);
}

VideoFilterTypeID VideoFilter::GetTypeID()
{
	pthread_mutex_lock(&this->_mutexAttributes);
	VideoFilterTypeID typeID = this->_vfAttributes.typeID;
	pthread_mutex_unlock(&this->_mutexAttributes);
	
	return typeID;
}

const char* VideoFilter::GetTypeString()
{
	pthread_mutex_lock(&this->_mutexAttributes);
	const char *typeString = this->_vfAttributes.typeString;
	pthread_mutex_unlock(&this->_mutexAttributes);
	
	return typeString;
}

uint32_t* VideoFilter::GetSrcBufferPtr()
{
	pthread_mutex_lock(&this->_mutexSrc);
	uint32_t *ptr = (uint32_t *)this->_vfSrcSurface.Surface;
	pthread_mutex_unlock(&this->_mutexSrc);
	
	return ptr;
}

uint32_t* VideoFilter::GetDstBufferPtr()
{
	pthread_mutex_lock(&this->_mutexDst);
	uint32_t *ptr = (uint32_t *)this->_vfDstSurface.Surface;
	pthread_mutex_unlock(&this->_mutexDst);
	
	return ptr;
}

size_t VideoFilter::GetSrcWidth()
{
	pthread_mutex_lock(&this->_mutexSrc);
	size_t width = this->_vfSrcSurface.Width;
	pthread_mutex_unlock(&this->_mutexSrc);
	
	return width;
}

size_t VideoFilter::GetSrcHeight()
{
	pthread_mutex_lock(&this->_mutexSrc);
	size_t height = this->_vfSrcSurface.Height;
	pthread_mutex_unlock(&this->_mutexSrc);
	
	return height;
}

size_t VideoFilter::GetDstWidth()
{
	pthread_mutex_lock(&this->_mutexDst);
	size_t width = this->_vfDstSurface.Width;
	pthread_mutex_unlock(&this->_mutexDst);
	
	return width;
}

size_t VideoFilter::GetDstHeight()
{
	pthread_mutex_lock(&this->_mutexDst);
	size_t height = this->_vfDstSurface.Height;
	pthread_mutex_unlock(&this->_mutexDst);
	
	return height;
}

VideoFilterParamType VideoFilter::GetFilterParameterType(VideoFilterParamID paramID)
{
	return _VideoFilterParamAttributesList[paramID].type;
}

int VideoFilter::GetFilterParameteri(VideoFilterParamID paramID)
{
	int value = 0;
	
	pthread_mutex_lock(&this->_mutexDst);
	
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
	
	pthread_mutex_unlock(&this->_mutexDst);
	
	return value;
}

unsigned int VideoFilter::GetFilterParameterui(VideoFilterParamID paramID)
{
	unsigned int value = 0;
	
	pthread_mutex_lock(&this->_mutexDst);
	
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
	
	pthread_mutex_unlock(&this->_mutexDst);
	
	return value;
}

float VideoFilter::GetFilterParameterf(VideoFilterParamID paramID)
{
	float value = 0.0f;
	
	pthread_mutex_lock(&this->_mutexDst);
	
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
	
	pthread_mutex_unlock(&this->_mutexDst);
	
	return value;
}

void VideoFilter::SetFilterParameteri(VideoFilterParamID paramID, int value)
{
	if (paramID >= VideoFilterParamIDCount)
	{
		return;
	}
	
	pthread_mutex_lock(&this->_mutexDst);
	
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
	
	pthread_mutex_unlock(&this->_mutexDst);
}

void VideoFilter::SetFilterParameterui(VideoFilterParamID paramID, unsigned int value)
{
	if (paramID >= VideoFilterParamIDCount)
	{
		return;
	}
	
	pthread_mutex_lock(&this->_mutexDst);
	
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
	
	pthread_mutex_unlock(&this->_mutexDst);
}

void VideoFilter::SetFilterParameterf(VideoFilterParamID paramID, float value)
{
	if (paramID >= VideoFilterParamIDCount)
	{
		return;
	}
	
	pthread_mutex_lock(&this->_mutexDst);
	
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
	
	pthread_mutex_unlock(&this->_mutexDst);
}

// Task function for multithreaded filtering
static void* RunVideoFilterTask(void *arg)
{
	VideoFilterThreadParam *param = (VideoFilterThreadParam *)arg;
	
	param->filterFunction(param->srcSurface, param->dstSurface);
	
	return NULL;
}
