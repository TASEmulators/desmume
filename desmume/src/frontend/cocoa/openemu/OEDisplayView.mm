/*
	Copyright (C) 2022-2026 DeSmuME team

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

#include "../../rasterize.h"
#include "../../OGLRender_3_2.h"
#include "../cgl_3Demu.h"


OE_OGLClientFetchObject::OE_OGLClientFetchObject()
{
	_context = NULL;
	_clientData = NULL;
	_id = GPUClientFetchObjectID_OpenEmu;
}

OE_OGLClientFetchObject::~OE_OGLClientFetchObject()
{
	delete (OGLClientSharedData *)this->_clientData;
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
		
		OGLClientSharedData *sharedData = new OGLClientSharedData;
		sharedData->SetUseDirectToCPUFilterPipeline(false);
		sharedData->SetContextInfo(newContextInfo);
		
		this->_clientData = sharedData;
	}
	
	if ( this->_context != CGLGetCurrentContext() )
	{
		this->_context = CGLGetCurrentContext();
		
		OGLClientSharedData *sharedData = (OGLClientSharedData *)this->_clientData;
		const NDSDisplayInfo &currentDisplayInfo = GPU->GetDisplayInfo();
		
		sharedData->InitOGL(this->_fetchDisplayInfo, currentDisplayInfo);
	}
}

void OE_OGLClientFetchObject::SetFetchBuffers(const NDSDisplayInfo &currentDisplayInfo)
{
	OGLClientSharedData *sharedData = (OGLClientSharedData *)this->_clientData;
	this->GPUClientFetchObject::SetFetchBuffers(currentDisplayInfo);
	sharedData->SetFetchBuffersOGL(this->_fetchDisplayInfo, currentDisplayInfo);
}

void OE_OGLClientFetchObject::FetchFromBufferIndex(const u8 index)
{
	OGLClientSharedData *sharedData = (OGLClientSharedData *)this->_clientData;
	this->GPUClientFetchObject::FetchFromBufferIndex(index);
	
	const NDSDisplayInfo &currentDisplayInfo = this->GetFetchDisplayInfoForBufferIndex(index);
	sharedData->FetchFromBufferIndexOGL(index, currentDisplayInfo);
}

void OE_OGLClientFetchObject::_FetchNativeDisplayByID(const NDSDisplayID displayID, const u8 bufferIndex)
{
	// This method is called from OE_OGLClientFetchObject::FetchFromBufferIndex(), and so
	// we should have already been assigned the current context.
	OGLClientSharedData *sharedData = (OGLClientSharedData *)this->_clientData;
	sharedData->FetchNativeDisplayByID_OGL(this->_fetchDisplayInfo, displayID, bufferIndex);
}

void OE_OGLClientFetchObject::_FetchCustomDisplayByID(const NDSDisplayID displayID, const u8 bufferIndex)
{
	// This method is called from OE_OGLClientFetchObject::FetchFromBufferIndex(), and so
	// we should have already been assigned the current context.
	OGLClientSharedData *sharedData = (OGLClientSharedData *)this->_clientData;
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
	
	this->OGLDisplayPresenter::Init();
}

CGLContextObj OE_OGLDisplayPresenter::GetContext() const
{
	return this->_context;
}

#pragma mark -

OEGraphicsControl::OEGraphicsControl()
{
	oglrender_init        = &cgl_initOpenGL_StandardAuto;
	oglrender_deinit      = &cgl_deinitOpenGL;
	oglrender_beginOpenGL = &cgl_beginOpenGL;
	oglrender_endOpenGL   = &cgl_endOpenGL;
	oglrender_framebufferDidResizeCallback = &cgl_framebufferDidResizeCallback;
	
#ifdef OGLRENDER_3_2_H
	OGLLoadEntryPoints_3_2_Func = &OGLLoadEntryPoints_3_2;
	OGLCreateRenderer_3_2_Func = &OGLCreateRenderer_3_2;
#endif
	
	bool isTempContextCreated = cgl_initOpenGL_StandardAuto();
	if (isTempContextCreated)
	{
		cgl_beginOpenGL();
		
		GLint maxSamplesOGL = 0;
		
#if defined(GL_MAX_SAMPLES)
		glGetIntegerv(GL_MAX_SAMPLES, &maxSamplesOGL);
#elif defined(GL_MAX_SAMPLES_EXT)
		glGetIntegerv(GL_MAX_SAMPLES_EXT, &maxSamplesOGL);
#endif
		_multisampleMaxClientSize = (uint8_t)maxSamplesOGL;
		
		cgl_endOpenGL();
		cgl_deinitOpenGL();
	}
	
	_fetchObject = NULL;
	
	if (_fetchObject == NULL)
	{
		_fetchObject = new OE_OGLClientFetchObject;
		_pageCountPending = OPENGL_FETCH_BUFFER_COUNT;
		GPU->SetWillPostprocessDisplays(false);
	}
	
	_fetchObject->Init();
	
	GPU->SetWillAutoResolveToCustomBuffer(false);
	GPU->SetEventHandler(this);
}

OEGraphicsControl::~OEGraphicsControl()
{
	GPU->SetEventHandler(NULL);
	
	delete this->_fetchObject;
	this->_fetchObject = NULL;
}

void OEGraphicsControl::Set3DEngineByID(int engineID)
{
	slock_lock(this->_mutexApplyRender3DSettings);
	
	switch (engineID)
	{
		case RENDERID_NULL:
			this->_3DEngineCallbackStruct = gpu3DNull;
			break;
			
		case RENDERID_SOFTRASTERIZER:
			this->_3DEngineCallbackStruct = gpu3DRasterize;
			break;
			
		case RENDERID_OPENGL_AUTO:
			oglrender_init = &cgl_initOpenGL_StandardAuto;
			this->_3DEngineCallbackStruct = gpu3Dgl;
			break;
			
		case RENDERID_OPENGL_LEGACY:
			oglrender_init = &cgl_initOpenGL_LegacyAuto;
			this->_3DEngineCallbackStruct = gpu3DglOld;
			break;
			
		case RENDERID_OPENGL_3_2:
			oglrender_init = &cgl_initOpenGL_3_2_CoreProfile;
			this->_3DEngineCallbackStruct = gpu3Dgl_3_2;
			break;
			
		default:
			puts("DeSmuME: Invalid 3D renderer chosen; falling back to SoftRasterizer.");
			engineID = RENDERID_SOFTRASTERIZER;
			this->_3DEngineCallbackStruct = gpu3DRasterize;
			break;
	}
	
	this->_engineIDPending = engineID;
	slock_unlock(this->_mutexApplyRender3DSettings);
}

void OEGraphicsControl::SetEngine3DClientIndex(int clientListIndex)
{
	if (clientListIndex < CORE3DLIST_NULL)
	{
		clientListIndex = CORE3DLIST_NULL;
	}
	
	int engineID = RENDERID_NULL;
	
	switch (clientListIndex)
	{
		case CORE3DLIST_NULL:
			engineID = RENDERID_NULL;
			break;
			
		case CORE3DLIST_SWRASTERIZE:
			engineID = RENDERID_SOFTRASTERIZER;
			break;
			
		case CORE3DLIST_OPENGL:
			engineID = RENDERID_OPENGL_AUTO;
			break;
			
		case RENDERID_OPENGL_LEGACY:
			engineID = RENDERID_OPENGL_LEGACY;
			break;
			
		case RENDERID_OPENGL_3_2:
			engineID = RENDERID_OPENGL_3_2;
			break;
			
		default:
			puts("DeSmuME: Invalid 3D renderer chosen; falling back to SoftRasterizer.");
			engineID = RENDERID_NULL;
			break;
	}
	
	this->Set3DEngineByID(engineID);
}

int OEGraphicsControl::GetEngine3DClientIndex()
{
	int clientListIndex = CORE3DLIST_NULL;
	int engineID = this->Get3DEngineByID();
	
	switch (engineID)
	{
		case RENDERID_NULL:
			clientListIndex = CORE3DLIST_NULL;
			break;
			
		case RENDERID_SOFTRASTERIZER:
			clientListIndex = CORE3DLIST_SWRASTERIZE;
			break;
			
		case RENDERID_OPENGL_AUTO:
			clientListIndex = CORE3DLIST_OPENGL;
			break;
			
		case RENDERID_OPENGL_LEGACY:
			clientListIndex = RENDERID_OPENGL_LEGACY;
			break;
			
		case RENDERID_OPENGL_3_2:
			clientListIndex = RENDERID_OPENGL_3_2;
			break;
			
		default:
			break;
	}
	
	return clientListIndex;
}

void OEGraphicsControl::DidApplyGPUSettingsEnd()
{
	if (this->_didWidthChange || this->_didHeightChange || this->_didColorFormatChange || this->_didPageCountChange)
	{
		const NDSDisplayInfo &displayInfo = GPU->GetDisplayInfo();
		this->_fetchObject->SetFetchBuffers(displayInfo);
	}
	
	this->ClientGraphicsControl::DidApplyGPUSettingsEnd();
}
