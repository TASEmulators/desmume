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

#include "videofilter.h"

// Parameters for Scanline filter
int scanline_filter_a = 0;
int scanline_filter_b = 2;
int scanline_filter_c = 2;
int scanline_filter_d = 4;

/********************************************************************************************
	CLASS CONSTRUCTORS
 ********************************************************************************************/
VideoFilter::VideoFilter(unsigned int srcWidth = 1,
						 unsigned int srcHeight = 1,
						 VideoFilterTypeID typeID = VideoFilterTypeID_None,
						 unsigned int numberThreads = 0)
{
	SSurface newSurface = {NULL, srcWidth*2, srcWidth, srcHeight};
	_vfSrcSurface = newSurface;
	_vfDstSurface = newSurface;
	
	_isFilterRunning = false;
	_vfSrcSurfacePixBuffer = NULL;
	_vfTypeID = typeID;
	
	pthread_mutex_init(&_mutexSrc, NULL);
	pthread_mutex_init(&_mutexDst, NULL);
	pthread_mutex_init(&_mutexTypeID, NULL);
	pthread_mutex_init(&_mutexTypeString, NULL);
	pthread_cond_init(&_condRunning, NULL);
	
	// Create all threads
	_vfThread.resize(numberThreads);
	
	for (unsigned int i = 0; i < numberThreads; i++)
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
	for (unsigned int i = 0; i < _vfThread.size(); i++)
	{
		_vfThread[i].task->finish();
		_vfThread[i].task->shutdown();
		
		delete _vfThread[i].task;
	}
	
	_vfThread.clear();
	
	// Destroy everything else
	pthread_mutex_lock(&this->_mutexDst);
	
	while (this->_isFilterRunning)
	{
		pthread_cond_wait(&this->_condRunning, &this->_mutexDst);
	}
	
	free(_vfDstSurface.Surface);
	_vfDstSurface.Surface = NULL;
	
	pthread_mutex_unlock(&_mutexDst);
	
	pthread_mutex_lock(&_mutexSrc);
	
	free(_vfSrcSurfacePixBuffer);
	_vfSrcSurfacePixBuffer = NULL;
	_vfSrcSurface.Surface = NULL;
	
	pthread_mutex_unlock(&_mutexSrc);
	
	pthread_mutex_destroy(&_mutexSrc);
	pthread_mutex_destroy(&_mutexDst);
	pthread_mutex_destroy(&_mutexTypeID);
	pthread_mutex_destroy(&_mutexTypeString);
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
bool VideoFilter::SetSourceSize(const unsigned int width, const unsigned int height)
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
	
	for (unsigned int i = 0; i < threadCount; i++)
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
	
	result = this->ChangeFilterByID(this->GetTypeID());
	
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
	
	return this->ChangeFilterByAttributes(&VideoFilterAttributesList[typeID]);
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
	
	pthread_mutex_lock(&this->_mutexSrc);
	const unsigned int srcWidth = this->_vfSrcSurface.Width;
	const unsigned int srcHeight = this->_vfSrcSurface.Height;
	pthread_mutex_unlock(&this->_mutexSrc);
	
	const VideoFilterTypeID typeID = vfAttr->typeID;
	const unsigned int dstWidth = srcWidth * vfAttr->scaleMultiply / vfAttr->scaleDivide;
	const unsigned int dstHeight = srcHeight * vfAttr->scaleMultiply / vfAttr->scaleDivide;
	const char *typeString = vfAttr->typeString;
	const VideoFilterFunc filterFunction = vfAttr->filterFunction;
	
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
	size_t threadCount = this->_vfThread.size();
	
	for (unsigned int i = 0; i < threadCount; i++)
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
	
	this->SetTypeID(typeID);
	this->SetTypeString(typeString);
	
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
	pthread_mutex_lock(&this->_mutexDst);
	
	this->_isFilterRunning = true;
	uint32_t *destBufPtr = (uint32_t *)this->_vfDstSurface.Surface;
	
	pthread_mutex_lock(&this->_mutexSrc);
	
	if (this->_vfFunc == NULL)
	{
		memcpy(this->_vfDstSurface.Surface, this->_vfSrcSurface.Surface, this->_vfDstSurface.Width * this->_vfDstSurface.Height * sizeof(uint32_t));
	}
	else
	{
		size_t threadCount = this->_vfThread.size();
		
		if (threadCount > 0)
		{
			for (unsigned int i = 0; i < threadCount; i++)
			{
				this->_vfThread[i].task->execute(&RunVideoFilterTask, &this->_vfThread[i].param);
			}
			
			for (unsigned int i = 0; i < threadCount; i++)
			{
				this->_vfThread[i].task->finish();
			}
		}
		else
		{
			this->_vfFunc(this->_vfSrcSurface, this->_vfDstSurface);
		}
	}
	
	pthread_mutex_unlock(&this->_mutexSrc);
	
	this->_isFilterRunning = false;
	pthread_cond_signal(&this->_condRunning);
	pthread_mutex_unlock(&this->_mutexDst);
	
	return destBufPtr;
}

/********************************************************************************************
	RunFilterCustom() - STATIC

	Runs the pixels from srcBuffer through the video filter, and then stores the
	resulting pixels into dstBuffer.

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
void VideoFilter::RunFilterCustom(const uint32_t *__restrict__ srcBuffer, uint32_t *__restrict__ dstBuffer,
								  const unsigned int srcWidth, const unsigned int srcHeight,
								  const VideoFilterTypeID typeID)
{
	if (typeID >= VideoFilterTypeIDCount)
	{
		return;
	}
	
	const VideoFilterAttributes *vfAttr = &VideoFilterAttributesList[typeID];
	const unsigned int dstWidth = srcWidth * vfAttr->scaleMultiply / vfAttr->scaleDivide;
	const unsigned int dstHeight = srcHeight * vfAttr->scaleMultiply / vfAttr->scaleDivide;
	const VideoFilterFunc filterFunction = vfAttr->filterFunction;
	
	SSurface srcSurface = {(unsigned char *)srcBuffer, srcWidth*2, srcWidth, srcHeight};
	SSurface dstSurface = {(unsigned char *)dstBuffer, dstWidth*2, dstWidth, dstHeight};
	
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
VideoFilterTypeID VideoFilter::GetTypeID()
{
	pthread_mutex_lock(&this->_mutexTypeID);
	VideoFilterTypeID typeID = this->_vfTypeID;
	pthread_mutex_unlock(&this->_mutexTypeID);
	
	return typeID;
}

void VideoFilter::SetTypeID(const VideoFilterTypeID typeID)
{
	pthread_mutex_lock(&this->_mutexTypeID);
	this->_vfTypeID = typeID;
	pthread_mutex_unlock(&this->_mutexTypeID);
}

const char* VideoFilter::GetTypeString()
{
	pthread_mutex_lock(&this->_mutexTypeString);
	const char *typeString = this->_vfTypeString.c_str();
	pthread_mutex_unlock(&this->_mutexTypeString);
	
	return typeString;
}

void VideoFilter::SetTypeString(const char *typeString)
{
	this->SetTypeString(std::string(typeString));
}

void VideoFilter::SetTypeString(std::string typeString)
{
	pthread_mutex_lock(&this->_mutexTypeString);
	this->_vfTypeString = typeString;
	pthread_mutex_unlock(&this->_mutexTypeString);
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

unsigned int VideoFilter::GetSrcWidth()
{
	pthread_mutex_lock(&this->_mutexSrc);
	unsigned int width = this->_vfSrcSurface.Width;
	pthread_mutex_unlock(&this->_mutexSrc);
	
	return width;
}

unsigned int VideoFilter::GetSrcHeight()
{
	pthread_mutex_lock(&this->_mutexSrc);
	unsigned int height = this->_vfSrcSurface.Height;
	pthread_mutex_unlock(&this->_mutexSrc);
	
	return height;
}

unsigned int VideoFilter::GetDstWidth()
{
	pthread_mutex_lock(&this->_mutexDst);
	unsigned int width = this->_vfDstSurface.Width;
	pthread_mutex_unlock(&this->_mutexDst);
	
	return width;
}

unsigned int VideoFilter::GetDstHeight()
{
	pthread_mutex_lock(&this->_mutexDst);
	unsigned int height = this->_vfDstSurface.Height;
	pthread_mutex_unlock(&this->_mutexDst);
	
	return height;
}

// Task function for multithreaded filtering
static void* RunVideoFilterTask(void *arg)
{
	VideoFilterThreadParam *param = (VideoFilterThreadParam *)arg;
	
	param->filterFunction(param->srcSurface, param->dstSurface);
	
	return NULL;
}
