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
int scanline_filter_a = 2;
int scanline_filter_b = 4;


/********************************************************************************************
	CLASS CONSTRUCTORS
 ********************************************************************************************/
VideoFilter::VideoFilter()
{
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
	
	pthread_mutex_init(&_mutexSrc, NULL);
	pthread_mutex_init(&_mutexDest, NULL);
	pthread_mutex_init(&_mutexTypeID, NULL);
	pthread_mutex_init(&_mutexTypeString, NULL);
	pthread_cond_init(&_condRunning, NULL);
	
	_isFilterRunning = false;
	_srcSurfaceBufferMaster = NULL;
	newSrcSurface->Surface = NULL;
	newDestSurface->Surface = NULL;
	
	_srcSurface = newSrcSurface;
	_destSurface = newDestSurface;
	_typeID = VideoFilterTypeID_None;
	
	SetSourceSize(1, 1);
}

VideoFilter::VideoFilter(unsigned int srcWidth, unsigned int srcHeight)
{
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
	
	pthread_mutex_init(&_mutexSrc, NULL);
	pthread_mutex_init(&_mutexDest, NULL);
	pthread_mutex_init(&_mutexTypeID, NULL);
	pthread_mutex_init(&_mutexTypeString, NULL);
	pthread_cond_init(&_condRunning, NULL);
	
	_isFilterRunning = false;
	_srcSurfaceBufferMaster = NULL;
	newSrcSurface->Surface = NULL;
	newDestSurface->Surface = NULL;
	
	_srcSurface = newSrcSurface;
	_destSurface = newDestSurface;
	_typeID = VideoFilterTypeID_None;
	
	SetSourceSize(srcWidth, srcHeight);
}

VideoFilter::VideoFilter(unsigned int srcWidth, unsigned int srcHeight, VideoFilterTypeID typeID)
{
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
	
	pthread_mutex_init(&_mutexSrc, NULL);
	pthread_mutex_init(&_mutexDest, NULL);
	pthread_mutex_init(&_mutexTypeID, NULL);
	pthread_mutex_init(&_mutexTypeString, NULL);
	pthread_cond_init(&_condRunning, NULL);
	
	_isFilterRunning = false;
	_srcSurfaceBufferMaster = NULL;
	newSrcSurface->Surface = NULL;
	newDestSurface->Surface = NULL;
	
	_srcSurface = newSrcSurface;
	_destSurface = newDestSurface;
	_typeID = typeID;
	
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
	
	free(_destSurface->Surface);
	_destSurface->Surface = NULL;
	free(_destSurface);
	_destSurface = NULL;
	
	pthread_mutex_unlock(&_mutexDest);
	
	pthread_mutex_lock(&_mutexSrc);
	
	free(_srcSurfaceBufferMaster);
	_srcSurfaceBufferMaster = NULL;
	_srcSurface->Surface = NULL;
	free(_srcSurface);
	_srcSurface = NULL;
	
	pthread_mutex_unlock(&_mutexSrc);
	
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
bool VideoFilter::SetSourceSize(unsigned int width, unsigned int height)
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
	
	this->_srcSurface->Width = width;
	this->_srcSurface->Height = height;
	this->_srcSurface->Pitch = width * 2;
	// Set the working source buffer pointer so that the working memory block is padded
	// with 4 pixel rows worth of memory on both sides.
	this->_srcSurface->Surface = (unsigned char *)(newSurfaceBuffer + (width * 4));
	
	free(this->_srcSurfaceBufferMaster);
	this->_srcSurfaceBufferMaster = newSurfaceBuffer;
	
	pthread_mutex_unlock(&this->_mutexSrc);
	
	result = this->ChangeFilter(this->GetTypeID());
	
	return result;
}

/********************************************************************************************
	ChangeFilter()

	Changes the video filter type.

	Takes:
		typeID - The type ID of the video filter. See the VideoFilterTypeID
			enumeration for possible values.

	Returns:
		A bool that reports if the filter change was successful. A value of true means
		success, while a value of false means failure.
 ********************************************************************************************/
bool VideoFilter::ChangeFilter(VideoFilterTypeID typeID)
{
	bool result = false;
	
	pthread_mutex_lock(&this->_mutexSrc);
	const unsigned int srcWidth = this->_srcSurface->Width;
	const unsigned int srcHeight = this->_srcSurface->Height;
	pthread_mutex_unlock(&this->_mutexSrc);
	
	unsigned int destWidth = srcWidth;
	unsigned int destHeight = srcHeight;
	const char *typeString = VideoFilter::GetTypeStringByID(typeID);
	VideoFilterCallback filterCallback = NULL;
	
	switch (typeID)
	{
		case VideoFilterTypeID_None:
			break;
			
		case VideoFilterTypeID_LQ2X:
			destWidth = srcWidth * 2;
			destHeight = srcHeight * 2;
			filterCallback = &RenderLQ2X;
			break;
			
		case VideoFilterTypeID_LQ2XS:
			destWidth = srcWidth * 2;
			destHeight = srcHeight * 2;
			filterCallback = &RenderLQ2XS;
			break;
			
		case VideoFilterTypeID_HQ2X:
			destWidth = srcWidth * 2;
			destHeight = srcHeight * 2;
			filterCallback = &RenderHQ2X;
			break;
			
		case VideoFilterTypeID_HQ2XS:
			destWidth = srcWidth * 2;
			destHeight = srcHeight * 2;
			filterCallback = &RenderHQ2XS;
			break;
			
		case VideoFilterTypeID_HQ4X:
			destWidth = srcWidth * 4;
			destHeight = srcHeight * 4;
			filterCallback = &RenderHQ4X;
			break;
			
		case VideoFilterTypeID_2xSaI:
			destWidth = srcWidth * 2;
			destHeight = srcHeight * 2;
			filterCallback = &Render2xSaI;
			break;
			
		case VideoFilterTypeID_Super2xSaI:
			destWidth = srcWidth * 2;
			destHeight = srcHeight * 2;
			filterCallback = &RenderSuper2xSaI;
			break;
			
		case VideoFilterTypeID_SuperEagle:
			destWidth = srcWidth * 2;
			destHeight = srcHeight * 2;
			filterCallback = &RenderSuperEagle;
			break;
			
		case VideoFilterTypeID_Scanline:
			destWidth = srcWidth * 2;
			destHeight = srcHeight * 2;
			filterCallback = &RenderScanline;
			break;
			
		case VideoFilterTypeID_Bilinear:
			destWidth = srcWidth * 2;
			destHeight = srcHeight * 2;
			filterCallback = &RenderBilinear;
			break;
			
		case VideoFilterTypeID_Nearest2X:
			destWidth = srcWidth * 2;
			destHeight = srcHeight * 2;
			filterCallback = &RenderNearest2X;
			break;
			
		case VideoFilterTypeID_Nearest1_5X:
			destWidth = srcWidth * 3 / 2;
			destHeight = srcHeight * 3 / 2;
			filterCallback = &RenderNearest_1Point5x;
			break;
			
		case VideoFilterTypeID_NearestPlus1_5X:
			destWidth = srcWidth * 3 / 2;
			destHeight = srcHeight * 3 / 2;
			filterCallback = &RenderNearestPlus_1Point5x;
			break;
			
		case VideoFilterTypeID_EPX:
			destWidth = srcWidth * 2;
			destHeight = srcHeight * 2;
			filterCallback = &RenderEPX;
			break;
			
		case VideoFilterTypeID_EPXPlus:
			destWidth = srcWidth * 2;
			destHeight = srcHeight * 2;
			filterCallback = &RenderEPXPlus;
			break;
			
		case VideoFilterTypeID_EPX1_5X:
			destWidth = srcWidth * 3 / 2;
			destHeight = srcHeight * 3 / 2;
			filterCallback = &RenderEPX_1Point5x;
			break;
			
		case VideoFilterTypeID_EPXPlus1_5X:
			destWidth = srcWidth * 3 / 2;
			destHeight = srcHeight * 3 / 2;
			filterCallback = &RenderEPXPlus_1Point5x;
			break;
			
		default:
			break;
	}
	
	pthread_mutex_lock(&this->_mutexDest);
	
	uint32_t *newSurfaceBuffer = (uint32_t *)calloc(destWidth * destHeight, sizeof(uint32_t));
	if (newSurfaceBuffer == NULL)
	{
		return result;
	}
	
	this->_filterCallback = filterCallback;
	this->_destSurface->Width = destWidth;
	this->_destSurface->Height = destHeight;
	this->_destSurface->Pitch = destWidth * 2;
	
	free(this->_destSurface->Surface);
	this->_destSurface->Surface = (unsigned char*)newSurfaceBuffer;
	
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
	uint32_t *destBufPtr = (uint32_t *)this->_destSurface->Surface;
	
	pthread_mutex_lock(&this->_mutexSrc);
	
	if (this->_filterCallback == NULL)
	{
		memcpy(this->_destSurface->Surface, this->_srcSurface->Surface, this->_destSurface->Width * this->_destSurface->Height * sizeof(uint32_t));
	}
	else
	{
		this->_filterCallback(*this->_srcSurface, *this->_destSurface);
	}
	
	pthread_mutex_unlock(&this->_mutexSrc);
	
	this->_isFilterRunning = false;
	pthread_cond_signal(&this->_condRunning);
	pthread_mutex_unlock(&this->_mutexDest);
	
	return destBufPtr;
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
const char* VideoFilter::GetTypeStringByID(VideoFilterTypeID typeID)
{
	const char *typeString = "Unknown";
	
	switch (typeID)
	{
		case VideoFilterTypeID_None:
			typeString = VIDEOFILTERTYPE_NONE_STRING;
			break;
			
		case VideoFilterTypeID_LQ2X:
			typeString = VIDEOFILTERTYPE_LQ2X_STRING;
			break;
			
		case VideoFilterTypeID_LQ2XS:
			typeString = VIDEOFILTERTYPE_LQ2XS_STRING;
			break;
			
		case VideoFilterTypeID_HQ2X:
			typeString = VIDEOFILTERTYPE_HQ2X_STRING;
			break;
			
		case VideoFilterTypeID_HQ2XS:
			typeString = VIDEOFILTERTYPE_HQ2XS_STRING;
			break;
			
		case VideoFilterTypeID_HQ4X:
			typeString = VIDEOFILTERTYPE_HQ4X_STRING;
			break;
			
		case VideoFilterTypeID_2xSaI:
			typeString = VIDEOFILTERTYPE_2XSAI_STRING;
			break;
			
		case VideoFilterTypeID_Super2xSaI:
			typeString = VIDEOFILTERTYPE_SUPER_2XSAI_STRING;
			break;
			
		case VideoFilterTypeID_SuperEagle:
			typeString = VIDEOFILTERTYPE_SUPER_EAGLE_STRING;
			break;
			
		case VideoFilterTypeID_Scanline:
			typeString = VIDEOFILTERTYPE_SCANLINE_STRING;
			break;
			
		case VideoFilterTypeID_Bilinear:
			typeString = VIDEOFILTERTYPE_BILINEAR_STRING;
			break;
			
		case VideoFilterTypeID_Nearest2X:
			typeString = VIDEOFILTERTYPE_NEAREST_2X_STRING;
			break;
			
		case VideoFilterTypeID_Nearest1_5X:
			typeString = VIDEOFILTERTYPE_NEAREST_1_5X_STRING;
			break;
			
		case VideoFilterTypeID_NearestPlus1_5X:
			typeString = VIDEOFILTERTYPE_NEAREST_PLUS_1_5X_STRING;
			break;
			
		case VideoFilterTypeID_EPX:
			typeString = VIDEOFILTERTYPE_EPX_STRING;
			break;
			
		case VideoFilterTypeID_EPXPlus:
			typeString = VIDEOFILTERTYPE_EPX_PLUS_STRING;
			break;
			
		case VideoFilterTypeID_EPX1_5X:
			typeString = VIDEOFILTERTYPE_EPX_1_5X_STRING;
			break;
			
		case VideoFilterTypeID_EPXPlus1_5X:
			typeString = VIDEOFILTERTYPE_EPX_PLUS_1_5X_STRING;
			break;
			
		default:
			break;
	}
	
	return typeString;
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

void VideoFilter::SetTypeID(VideoFilterTypeID typeID)
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
	uint32_t *ptr = (uint32_t *)this->_srcSurface->Surface;
	pthread_mutex_unlock(&this->_mutexSrc);
	
	return ptr;
}

uint32_t* VideoFilter::GetDestBufferPtr()
{
	pthread_mutex_lock(&this->_mutexDest);
	uint32_t *ptr = (uint32_t *)this->_destSurface->Surface;
	pthread_mutex_unlock(&this->_mutexDest);
	
	return ptr;
}

unsigned int VideoFilter::GetSrcWidth()
{
	pthread_mutex_lock(&this->_mutexSrc);
	unsigned int width = this->_srcSurface->Width;
	pthread_mutex_unlock(&this->_mutexSrc);
	
	return width;
}

unsigned int VideoFilter::GetSrcHeight()
{
	pthread_mutex_lock(&this->_mutexSrc);
	unsigned int height = this->_srcSurface->Height;
	pthread_mutex_unlock(&this->_mutexSrc);
	
	return height;
}

unsigned int VideoFilter::GetDestWidth()
{
	pthread_mutex_lock(&this->_mutexDest);
	unsigned int width = this->_destSurface->Width;
	pthread_mutex_unlock(&this->_mutexDest);
	
	return width;
}

unsigned int VideoFilter::GetDestHeight()
{
	pthread_mutex_lock(&this->_mutexDest);
	unsigned int height = this->_destSurface->Height;
	pthread_mutex_unlock(&this->_mutexDest);
	
	return height;
}
