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
VideoFilter::VideoFilter()
{
	_threadCount = 1;
	
	SSurface *newSrcSurface = (SSurface *)malloc(sizeof(SSurface));
	if (newSrcSurface == NULL)
	{
		throw;
	}
	
	SSurface *newDestSurface = (SSurface *)malloc(sizeof(SSurface));
	if (newDestSurface == NULL)
	{
		free(newSrcSurface);
		newSrcSurface = NULL;
		throw;
	}
	
	_srcSurfaceMaster = newSrcSurface;
	_destSurfaceMaster = newDestSurface;
	_srcSurfaceThread = _srcSurfaceMaster;
	_destSurfaceThread = _destSurfaceMaster;
	
	_isFilterRunning = false;
	_srcSurfaceBufferMaster = NULL;
	_typeID = VideoFilterTypeID_None;
	
	pthread_mutex_init(&_mutexSrc, NULL);
	pthread_mutex_init(&_mutexDest, NULL);
	pthread_mutex_init(&_mutexTypeID, NULL);
	pthread_mutex_init(&_mutexTypeString, NULL);
	pthread_cond_init(&_condRunning, NULL);
	
	_vfThread = (pthread_t *)malloc(sizeof(pthread_t));
	if (_vfThread == NULL)
	{
		throw;
	}
	
	_vfThreadParam = (VideoFilterThreadParam *)malloc(sizeof(VideoFilterThreadParam));
	if (_vfThreadParam == NULL)
	{
		free(_vfThreadParam);
		_vfThreadParam = NULL;
		throw;
	}
	
	_condVFThreadFinish = (pthread_cond_t *)malloc(sizeof(pthread_cond_t));
	if (_condVFThreadFinish == NULL)
	{
		free(_vfThread);
		_vfThread = NULL;
		
		free(_vfThreadParam);
		_vfThreadParam = NULL;
		throw;
	}
	
	_mutexVFThreadFinish = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
	if (_mutexVFThreadFinish == NULL)
	{
		free(_vfThread);
		_vfThread = NULL;
		
		free(_vfThreadParam);
		_vfThreadParam = NULL;
		
		free(_condVFThreadFinish);
		_condVFThreadFinish = NULL;
		throw;
	}
	
	_isFilterRunningThread = (bool *)malloc(sizeof(bool));
	if (_isFilterRunningThread == NULL)
	{
		free(_vfThread);
		_vfThread = NULL;
		
		free(_vfThreadParam);
		_vfThreadParam = NULL;
		
		free(_condVFThreadFinish);
		_condVFThreadFinish = NULL;
		
		free(_mutexVFThreadFinish);
		_mutexVFThreadFinish = NULL;
		throw;
	}
	
	_vfThreadParam->exitThread = false;
	pthread_mutex_init(&_vfThreadParam->mutexThreadExecute, NULL);
	pthread_cond_init(&_vfThreadParam->condThreadExecute, NULL);
	
	pthread_cond_init(_condVFThreadFinish, NULL);
	_vfThreadParam->condThreadFinish = _condVFThreadFinish;
	
	pthread_mutex_init(_mutexVFThreadFinish, NULL);
	_vfThreadParam->mutexThreadFinish = _mutexVFThreadFinish;
	
	*_isFilterRunningThread = false;
	_vfThreadParam->isFilterRunning = _isFilterRunningThread;
	
	pthread_create(_vfThread, NULL, &RunVideoFilterThread, _vfThreadParam);
	
	_srcSurfaceMaster->Surface = NULL;
	_destSurfaceMaster->Surface = NULL;
	SetSourceSize(1, 1);
}

VideoFilter::VideoFilter(unsigned int srcWidth, unsigned int srcHeight)
{
	_threadCount = 1;
	
	SSurface *newSrcSurface = (SSurface *)malloc(sizeof(SSurface));
	if (newSrcSurface == NULL)
	{
		throw;
	}
	
	SSurface *newDestSurface = (SSurface *)malloc(sizeof(SSurface));
	if (newDestSurface == NULL)
	{
		free(newSrcSurface);
		newSrcSurface = NULL;
		throw;
	}
	
	_srcSurfaceMaster = newSrcSurface;
	_destSurfaceMaster = newDestSurface;
	_srcSurfaceThread = _srcSurfaceMaster;
	_destSurfaceThread = _destSurfaceMaster;
	
	_isFilterRunning = false;
	_srcSurfaceBufferMaster = NULL;
	_typeID = VideoFilterTypeID_None;
	
	pthread_mutex_init(&_mutexSrc, NULL);
	pthread_mutex_init(&_mutexDest, NULL);
	pthread_mutex_init(&_mutexTypeID, NULL);
	pthread_mutex_init(&_mutexTypeString, NULL);
	pthread_cond_init(&_condRunning, NULL);
	
	_vfThread = (pthread_t *)malloc(sizeof(pthread_t));
	if (_vfThread == NULL)
	{
		throw;
	}
	
	_vfThreadParam = (VideoFilterThreadParam *)malloc(sizeof(VideoFilterThreadParam));
	if (_vfThreadParam == NULL)
	{
		free(_vfThreadParam);
		_vfThreadParam = NULL;
		throw;
	}
	
	_condVFThreadFinish = (pthread_cond_t *)malloc(sizeof(pthread_cond_t));
	if (_condVFThreadFinish == NULL)
	{
		free(_vfThread);
		_vfThread = NULL;
		
		free(_vfThreadParam);
		_vfThreadParam = NULL;
		throw;
	}
	
	_mutexVFThreadFinish = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
	if (_mutexVFThreadFinish == NULL)
	{
		free(_vfThread);
		_vfThread = NULL;
		
		free(_vfThreadParam);
		_vfThreadParam = NULL;
		
		free(_condVFThreadFinish);
		_condVFThreadFinish = NULL;
		throw;
	}
	
	_isFilterRunningThread = (bool *)malloc(sizeof(bool));
	if (_isFilterRunningThread == NULL)
	{
		free(_vfThread);
		_vfThread = NULL;
		
		free(_vfThreadParam);
		_vfThreadParam = NULL;
		
		free(_condVFThreadFinish);
		_condVFThreadFinish = NULL;
		
		free(_mutexVFThreadFinish);
		_mutexVFThreadFinish = NULL;
		throw;
	}
	
	_vfThreadParam->exitThread = false;
	pthread_mutex_init(&_vfThreadParam->mutexThreadExecute, NULL);
	pthread_cond_init(&_vfThreadParam->condThreadExecute, NULL);
	
	pthread_cond_init(_condVFThreadFinish, NULL);
	_vfThreadParam->condThreadFinish = _condVFThreadFinish;
	
	pthread_mutex_init(_mutexVFThreadFinish, NULL);
	_vfThreadParam->mutexThreadFinish = _mutexVFThreadFinish;
	
	*_isFilterRunningThread = false;
	_vfThreadParam->isFilterRunning = _isFilterRunningThread;
	
	pthread_create(_vfThread, NULL, &RunVideoFilterThread, _vfThreadParam);
	
	_srcSurfaceMaster->Surface = NULL;
	_destSurfaceMaster->Surface = NULL;
	SetSourceSize(srcWidth, srcHeight);
}

VideoFilter::VideoFilter(unsigned int srcWidth, unsigned int srcHeight, VideoFilterTypeID typeID)
{
	_threadCount = 1;
	
	SSurface *newSrcSurface = (SSurface *)malloc(sizeof(SSurface));
	if (newSrcSurface == NULL)
	{
		throw;
	}
	
	SSurface *newDestSurface = (SSurface *)malloc(sizeof(SSurface));
	if (newDestSurface == NULL)
	{
		free(newSrcSurface);
		newSrcSurface = NULL;
		throw;
	}
	
	_srcSurfaceMaster = newSrcSurface;
	_destSurfaceMaster = newDestSurface;
	_srcSurfaceThread = _srcSurfaceMaster;
	_destSurfaceThread = _destSurfaceMaster;
	
	_isFilterRunning = false;
	_srcSurfaceBufferMaster = NULL;
	_typeID = typeID;
	
	pthread_mutex_init(&_mutexSrc, NULL);
	pthread_mutex_init(&_mutexDest, NULL);
	pthread_mutex_init(&_mutexTypeID, NULL);
	pthread_mutex_init(&_mutexTypeString, NULL);
	pthread_cond_init(&_condRunning, NULL);
	
	_vfThread = (pthread_t *)malloc(sizeof(pthread_t));
	if (_vfThread == NULL)
	{
		throw;
	}
	
	_vfThreadParam = (VideoFilterThreadParam *)malloc(sizeof(VideoFilterThreadParam));
	if (_vfThreadParam == NULL)
	{
		free(_vfThreadParam);
		_vfThreadParam = NULL;
		throw;
	}
	
	_condVFThreadFinish = (pthread_cond_t *)malloc(sizeof(pthread_cond_t));
	if (_condVFThreadFinish == NULL)
	{
		free(_vfThread);
		_vfThread = NULL;
		
		free(_vfThreadParam);
		_vfThreadParam = NULL;
		throw;
	}
	
	_mutexVFThreadFinish = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
	if (_mutexVFThreadFinish == NULL)
	{
		free(_vfThread);
		_vfThread = NULL;
		
		free(_vfThreadParam);
		_vfThreadParam = NULL;
		
		free(_condVFThreadFinish);
		_condVFThreadFinish = NULL;
		throw;
	}
	
	_isFilterRunningThread = (bool *)malloc(sizeof(bool));
	if (_isFilterRunningThread == NULL)
	{
		free(_vfThread);
		_vfThread = NULL;
		
		free(_vfThreadParam);
		_vfThreadParam = NULL;
		
		free(_condVFThreadFinish);
		_condVFThreadFinish = NULL;
		
		free(_mutexVFThreadFinish);
		_mutexVFThreadFinish = NULL;
		throw;
	}
	
	_vfThreadParam->exitThread = false;
	pthread_mutex_init(&_vfThreadParam->mutexThreadExecute, NULL);
	pthread_cond_init(&_vfThreadParam->condThreadExecute, NULL);
	
	pthread_cond_init(_condVFThreadFinish, NULL);
	_vfThreadParam->condThreadFinish = _condVFThreadFinish;
	
	pthread_mutex_init(_mutexVFThreadFinish, NULL);
	_vfThreadParam->mutexThreadFinish = _mutexVFThreadFinish;
	
	*_isFilterRunningThread = false;
	_vfThreadParam->isFilterRunning = _isFilterRunningThread;
	
	pthread_create(_vfThread, NULL, &RunVideoFilterThread, _vfThreadParam);
	
	_srcSurfaceMaster->Surface = NULL;
	_destSurfaceMaster->Surface = NULL;
	SetSourceSize(srcWidth, srcHeight);
}

VideoFilter::VideoFilter(unsigned int srcWidth, unsigned int srcHeight, VideoFilterTypeID typeID, unsigned int numberThreads)
{
	// Bounds check
	if (numberThreads <= 1)
	{
		numberThreads = 1;
	}
	
	_threadCount = numberThreads;
	
	// We maintain a master surface with our master settings, plus additional surfaces
	// specific to each thread that work off the master surface.
	size_t threadAlloc = numberThreads + 1;
	if (numberThreads == 1)
	{
		threadAlloc = 1;
	}
	
	SSurface *newSrcSurface = (SSurface *)malloc(sizeof(SSurface) * threadAlloc);
	if (newSrcSurface == NULL)
	{
		throw;
	}
	
	SSurface *newDestSurface = (SSurface *)malloc(sizeof(SSurface) * threadAlloc);
	if (newDestSurface == NULL)
	{
		free(newSrcSurface);
		newSrcSurface = NULL;
		throw;
	}
	
	// Index the master surface at 0. When freeing, always free the master surface.
	_srcSurfaceMaster = newSrcSurface;
	_destSurfaceMaster = newDestSurface;
	
	// If we're only using one thread, we can make the master surface and the thread
	// surface the same. Otherwise, index the thread surfaces starting at 1. Since
	// thread surfaces are pointers relative to the master surface, do not free any
	// thread surface directly!
	if (_threadCount <= 1)
	{
		_srcSurfaceThread = _srcSurfaceMaster;
		_destSurfaceThread = _destSurfaceMaster;
	}
	else
	{
		_srcSurfaceThread = _srcSurfaceMaster + 1;
		_destSurfaceThread = _destSurfaceMaster + 1;
	}
	
	_isFilterRunning = false;
	_srcSurfaceBufferMaster = NULL;
	_typeID = typeID;
	
	pthread_mutex_init(&_mutexSrc, NULL);
	pthread_mutex_init(&_mutexDest, NULL);
	pthread_mutex_init(&_mutexTypeID, NULL);
	pthread_mutex_init(&_mutexTypeString, NULL);
	pthread_cond_init(&_condRunning, NULL);
	
	_vfThread = (pthread_t *)malloc(sizeof(pthread_t) * _threadCount);
	if (_vfThread == NULL)
	{
		throw;
	}
	
	_vfThreadParam = (VideoFilterThreadParam *)malloc(sizeof(VideoFilterThreadParam) * _threadCount);
	if (_vfThreadParam == NULL)
	{
		free(_vfThread);
		_vfThread = NULL;
		throw;
	}
	
	_condVFThreadFinish = (pthread_cond_t *)malloc(sizeof(pthread_cond_t) * _threadCount);
	if (_condVFThreadFinish == NULL)
	{
		free(_vfThread);
		_vfThread = NULL;
		
		free(_vfThreadParam);
		_vfThreadParam = NULL;
		throw;
	}
	
	_mutexVFThreadFinish = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t) * _threadCount);
	if (_mutexVFThreadFinish == NULL)
	{
		free(_vfThread);
		_vfThread = NULL;
		
		free(_vfThreadParam);
		_vfThreadParam = NULL;
		
		free(_condVFThreadFinish);
		_condVFThreadFinish = NULL;
		throw;
	}
	
	_isFilterRunningThread = (bool *)malloc(sizeof(bool) * _threadCount);
	if (_isFilterRunningThread == NULL)
	{
		free(_vfThread);
		_vfThread = NULL;
		
		free(_vfThreadParam);
		_vfThreadParam = NULL;
		
		free(_condVFThreadFinish);
		_condVFThreadFinish = NULL;
		
		free(_mutexVFThreadFinish);
		_mutexVFThreadFinish = NULL;
		throw;
	}
	
	for (unsigned int i = 0; i < _threadCount; i++)
	{
		_vfThreadParam[i].exitThread = false;
		pthread_mutex_init(&_vfThreadParam[i].mutexThreadExecute, NULL);
		pthread_cond_init(&_vfThreadParam[i].condThreadExecute, NULL);
		
		pthread_cond_init(&_condVFThreadFinish[i], NULL);
		_vfThreadParam[i].condThreadFinish = &_condVFThreadFinish[i];
		
		pthread_mutex_init(&_mutexVFThreadFinish[i], NULL);
		_vfThreadParam[i].mutexThreadFinish = &_mutexVFThreadFinish[i];
		
		_isFilterRunningThread[i] = false;
		_vfThreadParam[i].isFilterRunning = &_isFilterRunningThread[i];
		
		pthread_create(&_vfThread[i], NULL, &RunVideoFilterThread, &_vfThreadParam[i]);
	}
	
	_srcSurfaceMaster->Surface = NULL;
	_destSurfaceMaster->Surface = NULL;
	SetSourceSize(srcWidth, srcHeight);
}

/********************************************************************************************
	CLASS DESTRUCTOR
 ********************************************************************************************/
VideoFilter::~VideoFilter()
{
	pthread_mutex_lock(&this->_mutexDest);
	
	while (this->_isFilterRunning)
	{
		pthread_cond_wait(&this->_condRunning, &this->_mutexDest);
	}
	
	free(_destSurfaceMaster->Surface);
	_destSurfaceMaster->Surface = NULL;
	free(_destSurfaceMaster);
	_destSurfaceMaster = NULL;
	
	pthread_mutex_unlock(&_mutexDest);
	
	pthread_mutex_lock(&_mutexSrc);
	
	free(_srcSurfaceBufferMaster);
	_srcSurfaceBufferMaster = NULL;
	_srcSurfaceMaster->Surface = NULL;
	free(_srcSurfaceMaster);
	_srcSurfaceMaster = NULL;
	
	pthread_mutex_unlock(&_mutexSrc);
	
	for (unsigned int i = 0; i < _threadCount; i++)
	{
		pthread_mutex_lock(&_vfThreadParam[i].mutexThreadExecute);
		_vfThreadParam[i].exitThread = true;
		pthread_cond_signal(&_vfThreadParam[i].condThreadExecute);
		pthread_mutex_unlock(&_vfThreadParam[i].mutexThreadExecute);
		
		pthread_join(_vfThread[i], NULL);
		_vfThread[i] = NULL;
		pthread_mutex_destroy(&_vfThreadParam[i].mutexThreadExecute);
		pthread_cond_destroy(&_vfThreadParam[i].condThreadExecute);
		
		pthread_cond_destroy(&_condVFThreadFinish[i]);
		pthread_mutex_destroy(&_mutexVFThreadFinish[i]);
	}
	
	free(_vfThread);
	_vfThread = NULL;
	free(_vfThreadParam);
	_vfThreadParam = NULL;
	free(_condVFThreadFinish);
	_condVFThreadFinish = NULL;
	free(_mutexVFThreadFinish);
	_mutexVFThreadFinish = NULL;
	free(_isFilterRunningThread);
	_isFilterRunningThread = NULL;
	
	pthread_mutex_destroy(&_mutexSrc);
	pthread_mutex_destroy(&_mutexDest);
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
	uint32_t *newSurfaceBuffer = (uint32_t *)calloc(width * (height + 8), sizeof(uint32_t));
	if (newSurfaceBuffer == NULL)
	{
		return result;
	}
	
	this->_srcSurfaceMaster->Width = width;
	this->_srcSurfaceMaster->Height = height;
	this->_srcSurfaceMaster->Pitch = width * 2;
	// Set the working source buffer pointer so that the working memory block is padded
	// with 4 pixel rows worth of memory on both sides.
	this->_srcSurfaceMaster->Surface = (unsigned char *)(newSurfaceBuffer + (width * 4));
	
	free(this->_srcSurfaceBufferMaster);
	this->_srcSurfaceBufferMaster = newSurfaceBuffer;
	
	// Update the surfaces on threads.
	for (unsigned int i = 0; i < this->_threadCount; i++)
	{
		this->_srcSurfaceThread[i] = *this->_srcSurfaceMaster;
		this->_srcSurfaceThread[i].Height /= this->_threadCount;
		
		if (i > 0)
		{
			this->_srcSurfaceThread[i].Surface = (unsigned char *)((uint32_t *)this->_srcSurfaceThread[i - 1].Surface + (this->_srcSurfaceThread[i - 1].Width * this->_srcSurfaceThread[i - 1].Height));
		}
		
		this->_vfThreadParam[i].srcSurface = this->_srcSurfaceThread[i];
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
	const unsigned int srcWidth = this->_srcSurfaceMaster->Width;
	const unsigned int srcHeight = this->_srcSurfaceMaster->Height;
	pthread_mutex_unlock(&this->_mutexSrc);
	
	const VideoFilterTypeID typeID = vfAttr->typeID;
	const unsigned int dstWidth = srcWidth * vfAttr->scaleMultiply / vfAttr->scaleDivide;
	const unsigned int dstHeight = srcHeight * vfAttr->scaleMultiply / vfAttr->scaleDivide;
	const char *typeString = vfAttr->typeString;
	const VideoFilterCallback filterCallback = vfAttr->filterFunction;
	
	pthread_mutex_lock(&this->_mutexDest);
	
	uint32_t *newSurfaceBuffer = (uint32_t *)calloc(dstWidth * dstHeight, sizeof(uint32_t));
	if (newSurfaceBuffer == NULL)
	{
		return result;
	}
	
	this->_filterCallback = filterCallback;
	this->_destSurfaceMaster->Width = dstWidth;
	this->_destSurfaceMaster->Height = dstHeight;
	this->_destSurfaceMaster->Pitch = dstWidth * 2;
	
	free(this->_destSurfaceMaster->Surface);
	this->_destSurfaceMaster->Surface = (unsigned char*)newSurfaceBuffer;
	
	// Update the surfaces on threads.
	for (unsigned int i = 0; i < this->_threadCount; i++)
	{
		this->_destSurfaceThread[i] = *this->_destSurfaceMaster;
		this->_destSurfaceThread[i].Height /= this->_threadCount;
		
		if (i > 0)
		{
			this->_destSurfaceThread[i].Surface = (unsigned char *)((uint32_t *)this->_destSurfaceThread[i - 1].Surface + (this->_destSurfaceThread[i - 1].Width * this->_destSurfaceThread[i - 1].Height));
		}
		
		this->_vfThreadParam[i].destSurface = this->_destSurfaceThread[i];
		this->_vfThreadParam[i].filterCallback = this->_filterCallback;
	}
	
	pthread_mutex_unlock(&this->_mutexDest);
	
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
	pthread_mutex_lock(&this->_mutexDest);
	
	this->_isFilterRunning = true;
	uint32_t *destBufPtr = (uint32_t *)this->_destSurfaceMaster->Surface;
	
	pthread_mutex_lock(&this->_mutexSrc);
	
	if (this->_filterCallback == NULL)
	{
		memcpy(this->_destSurfaceMaster->Surface, this->_srcSurfaceMaster->Surface, this->_destSurfaceMaster->Width * this->_destSurfaceMaster->Height * sizeof(uint32_t));
	}
	else
	{
		for (unsigned int i = 0; i < this->_threadCount; i++)
		{
			pthread_mutex_lock(&this->_vfThreadParam[i].mutexThreadExecute);
			*this->_vfThreadParam[i].isFilterRunning = true;
			pthread_cond_signal(&this->_vfThreadParam[i].condThreadExecute);
			pthread_mutex_unlock(&this->_vfThreadParam[i].mutexThreadExecute);
		}
		
		for (unsigned int i = 0; i < this->_threadCount; i++)
		{
			pthread_mutex_lock(&this->_mutexVFThreadFinish[i]);
			
			while (this->_isFilterRunningThread[i])
			{
				pthread_cond_wait(&this->_condVFThreadFinish[i], &this->_mutexVFThreadFinish[i]);
			}
			
			pthread_mutex_unlock(&this->_mutexVFThreadFinish[i]);
		}
	}
	
	pthread_mutex_unlock(&this->_mutexSrc);
	
	this->_isFilterRunning = false;
	pthread_cond_signal(&this->_condRunning);
	pthread_mutex_unlock(&this->_mutexDest);
	
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
	const unsigned int dstHeight = dstWidth * vfAttr->scaleMultiply / vfAttr->scaleDivide;
	const VideoFilterCallback filterFunction = vfAttr->filterFunction;
	
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
	VideoFilterTypeID typeID = this->_typeID;
	pthread_mutex_unlock(&this->_mutexTypeID);
	
	return typeID;
}

void VideoFilter::SetTypeID(const VideoFilterTypeID typeID)
{
	pthread_mutex_lock(&this->_mutexTypeID);
	this->_typeID = typeID;
	pthread_mutex_unlock(&this->_mutexTypeID);
}

const char* VideoFilter::GetTypeString()
{
	pthread_mutex_lock(&this->_mutexTypeString);
	const char *typeString = this->_typeString.c_str();
	pthread_mutex_unlock(&this->_mutexTypeString);
	
	return typeString;
}

void VideoFilter::SetTypeString(const char *typeString)
{
	pthread_mutex_lock(&this->_mutexTypeString);
	this->_typeString = typeString;
	pthread_mutex_unlock(&this->_mutexTypeString);
}

void VideoFilter::SetTypeString(std::string typeString)
{
	pthread_mutex_lock(&this->_mutexTypeString);
	this->_typeString = typeString;
	pthread_mutex_unlock(&this->_mutexTypeString);
}

uint32_t* VideoFilter::GetSrcBufferPtr()
{
	pthread_mutex_lock(&this->_mutexSrc);
	uint32_t *ptr = (uint32_t *)this->_srcSurfaceMaster->Surface;
	pthread_mutex_unlock(&this->_mutexSrc);
	
	return ptr;
}

uint32_t* VideoFilter::GetDestBufferPtr()
{
	pthread_mutex_lock(&this->_mutexDest);
	uint32_t *ptr = (uint32_t *)this->_destSurfaceMaster->Surface;
	pthread_mutex_unlock(&this->_mutexDest);
	
	return ptr;
}

unsigned int VideoFilter::GetSrcWidth()
{
	pthread_mutex_lock(&this->_mutexSrc);
	unsigned int width = this->_srcSurfaceMaster->Width;
	pthread_mutex_unlock(&this->_mutexSrc);
	
	return width;
}

unsigned int VideoFilter::GetSrcHeight()
{
	pthread_mutex_lock(&this->_mutexSrc);
	unsigned int height = this->_srcSurfaceMaster->Height;
	pthread_mutex_unlock(&this->_mutexSrc);
	
	return height;
}

unsigned int VideoFilter::GetDestWidth()
{
	pthread_mutex_lock(&this->_mutexDest);
	unsigned int width = this->_destSurfaceMaster->Width;
	pthread_mutex_unlock(&this->_mutexDest);
	
	return width;
}

unsigned int VideoFilter::GetDestHeight()
{
	pthread_mutex_lock(&this->_mutexDest);
	unsigned int height = this->_destSurfaceMaster->Height;
	pthread_mutex_unlock(&this->_mutexDest);
	
	return height;
}

static void* RunVideoFilterThread(void *arg)
{
	VideoFilterThreadParam *param = (VideoFilterThreadParam *)arg;
	
	do
	{
		pthread_mutex_lock(&param->mutexThreadExecute);
		
		while (!*param->isFilterRunning && !param->exitThread)
		{
			pthread_cond_wait(&param->condThreadExecute, &param->mutexThreadExecute);
		}
		
		if (param->exitThread)
		{
			pthread_mutex_unlock(&param->mutexThreadExecute);
			break;
		}
		
		param->filterCallback(param->srcSurface, param->destSurface);
		
		pthread_mutex_lock(param->mutexThreadFinish);
		*param->isFilterRunning = false;
		pthread_cond_signal(param->condThreadFinish);
		pthread_mutex_unlock(param->mutexThreadFinish);
		
		pthread_mutex_unlock(&param->mutexThreadExecute);
		
	} while (!param->exitThread);
	
	return NULL;
}
