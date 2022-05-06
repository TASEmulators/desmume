/*
	Copyright (C) 2022 DeSmuME team

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

#include <mach/semaphore.h>
#import "OEDisplayView.h"
#import "../cocoa_globals.h"

OE_OGLClientSharedData::OE_OGLClientSharedData()
{
	_useDirectToCPUFilterPipeline = false;
	
	_unfairlockTexFetch[NDSDisplayID_Main] = apple_unfairlock_create();
	_unfairlockTexFetch[NDSDisplayID_Touch] = apple_unfairlock_create();
}

OE_OGLClientSharedData::~OE_OGLClientSharedData()
{
	apple_unfairlock_destroy(this->_unfairlockTexFetch[NDSDisplayID_Main]);
	this->_unfairlockTexFetch[NDSDisplayID_Main] = NULL;
	apple_unfairlock_destroy(this->_unfairlockTexFetch[NDSDisplayID_Touch]);
	this->_unfairlockTexFetch[NDSDisplayID_Touch] = NULL;
}

GLuint OE_OGLClientSharedData::GetFetchTexture(const NDSDisplayID displayID)
{
	apple_unfairlock_lock(this->_unfairlockTexFetch[displayID]);
	const GLuint texFetchID = this->OGLClientSharedData::GetFetchTexture(displayID);
	apple_unfairlock_unlock(this->_unfairlockTexFetch[displayID]);
	
	return texFetchID;
}

void OE_OGLClientSharedData::SetFetchTexture(const NDSDisplayID displayID, GLuint texID)
{
	apple_unfairlock_lock(this->_unfairlockTexFetch[displayID]);
	this->OGLClientSharedData::SetFetchTexture(displayID, texID);
	apple_unfairlock_unlock(this->_unfairlockTexFetch[displayID]);
}

#pragma mark -

OE_OGLClientFetchObject::OE_OGLClientFetchObject()
{
	_context = NULL;
	_clientData = NULL;
	_id = GPUClientFetchObjectID_OpenEmu;
}

OE_OGLClientFetchObject::~OE_OGLClientFetchObject()
{
	delete (OE_OGLClientSharedData *)this->_clientData;
}

CGLContextObj OE_OGLClientFetchObject::GetContext() const
{
	return this->_context;
}

void OE_OGLClientFetchObject::Init()
{
	if (this->_clientData == NULL)
	{
		OGLContextInfo *newContextInfo = new OGLContextInfo_3_2;
		if ( (newContextInfo->GetVersionMajor() <= 2) ||
			((newContextInfo->GetVersionMajor() == 3) && (newContextInfo->GetVersionMinor() <= 1)) )
		{
			delete newContextInfo;
			newContextInfo = new OGLContextInfo_Legacy;
		}
		
		snprintf(_name, sizeof(_name) - 1, "OpenEmu OpenGL v%i.%i", newContextInfo->GetVersionMajor(), newContextInfo->GetVersionMinor());
		strlcpy(_description, newContextInfo->GetRendererString(), sizeof(_description) - 1);
		
		this->_clientData = new OE_OGLClientSharedData;
		((OE_OGLClientSharedData *)this->_clientData)->SetContextInfo(newContextInfo);
	}
	
	if ( this->_context != CGLGetCurrentContext() )
	{
		this->_context = CGLGetCurrentContext();
		
		OE_OGLClientSharedData *sharedData = (OE_OGLClientSharedData *)this->_clientData;
		sharedData->InitOGL();
	}
}

void OE_OGLClientFetchObject::SetFetchBuffers(const NDSDisplayInfo &currentDisplayInfo)
{
	OE_OGLClientSharedData *sharedData = (OE_OGLClientSharedData *)this->_clientData;
	this->GPUClientFetchObject::SetFetchBuffers(currentDisplayInfo);
	sharedData->SetFetchBuffersOGL(this->_fetchDisplayInfo, currentDisplayInfo);
}

void OE_OGLClientFetchObject::FetchFromBufferIndex(const u8 index)
{
	OE_OGLClientSharedData *sharedData = (OE_OGLClientSharedData *)this->_clientData;
	this->GPUClientFetchObject::FetchFromBufferIndex(index);
	
	const NDSDisplayInfo &currentDisplayInfo = this->GetFetchDisplayInfoForBufferIndex(index);
	sharedData->FetchFromBufferIndexOGL(index, currentDisplayInfo);
}

void OE_OGLClientFetchObject::_FetchNativeDisplayByID(const NDSDisplayID displayID, const u8 bufferIndex)
{
	// This method is called from OE_OGLClientFetchObject::FetchFromBufferIndex(), and so
	// we should have already been assigned the current context.
	OE_OGLClientSharedData *sharedData = (OE_OGLClientSharedData *)this->_clientData;
	sharedData->FetchNativeDisplayByID_OGL(this->_fetchDisplayInfo, displayID, bufferIndex);
}

void OE_OGLClientFetchObject::_FetchCustomDisplayByID(const NDSDisplayID displayID, const u8 bufferIndex)
{
	// This method is called from OE_OGLClientFetchObject::FetchFromBufferIndex(), and so
	// we should have already been assigned the current context.
	OE_OGLClientSharedData *sharedData = (OE_OGLClientSharedData *)this->_clientData;
	sharedData->FetchCustomDisplayByID_OGL(this->_fetchDisplayInfo, displayID, bufferIndex);
}

#pragma mark -

OE_OGLDisplayPresenter::OE_OGLDisplayPresenter()
{
	__InstanceInit(NULL);
}

OE_OGLDisplayPresenter::OE_OGLDisplayPresenter(OE_OGLClientFetchObject *fetchObject)
{
	__InstanceInit(fetchObject);
}

void OE_OGLDisplayPresenter::__InstanceInit(OE_OGLClientFetchObject *fetchObject)
{
	if (fetchObject != NULL)
	{
		_context = fetchObject->GetContext();
		_contextInfo = ((OGLClientSharedData *)fetchObject->GetClientData())->GetContextInfo();
	}
	else
	{
		_context = CGLGetCurrentContext();
		_contextInfo = new OGLContextInfo_3_2;
	}
	
	SetFetchObject(fetchObject);
}

void OE_OGLDisplayPresenter::Init()
{
	
	if (this->_fetchObject != NULL)
	{
		this->_context = ((OE_OGLClientFetchObject *)this->_fetchObject)->GetContext();
	}
	else
	{
		this->_context = CGLGetCurrentContext();
	}
	
	this->OGLVideoOutput::Init();
}

CGLContextObj OE_OGLDisplayPresenter::GetContext() const
{
	return this->_context;
}
