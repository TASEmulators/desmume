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

#ifndef _OPENEMU_DISPLAYOUTPUT_H_
#define _OPENEMU_DISPLAYOUTPUT_H_

#import <Cocoa/Cocoa.h>
#import <OpenGL/OpenGL.h>
#include "../utilities.h"

#import "../cocoa_GPU.h"
#import "../cocoa_util.h"
#include "../ClientDisplayView.h"
#include "../OGLDisplayOutput_3_2.h"

#ifdef BOOL
#undef BOOL
#endif


class OE_OGLClientSharedData : public OGLClientSharedData
{
protected:
	apple_unfairlock_t _unfairlockTexFetch[2];
	
public:
	OE_OGLClientSharedData();
	~OE_OGLClientSharedData();
	
	virtual GLuint GetFetchTexture(const NDSDisplayID displayID);
	virtual void SetFetchTexture(const NDSDisplayID displayID, GLuint texID);
};

class OE_OGLClientFetchObject : public GPUClientFetchObject
{
protected:
	CGLContextObj _context;
	
	// GPUClientFetchObject methods
	virtual void _FetchNativeDisplayByID(const NDSDisplayID displayID, const u8 bufferIndex);
	virtual void _FetchCustomDisplayByID(const NDSDisplayID displayID, const u8 bufferIndex);
	
public:
	OE_OGLClientFetchObject();
	~OE_OGLClientFetchObject();
	
	CGLContextObj GetContext() const;
	
	// GPUClientFetchObject methods
	virtual void Init();
	virtual void SetFetchBuffers(const NDSDisplayInfo &currentDisplayInfo);
	virtual void FetchFromBufferIndex(const u8 index);
};

class OE_OGLDisplayPresenter : public OGLDisplayPresenter
{
private:
	void __InstanceInit(OE_OGLClientFetchObject *fetchObject);
	
protected:
	CGLContextObj _context;
	
public:
	OE_OGLDisplayPresenter();
	OE_OGLDisplayPresenter(OE_OGLClientFetchObject *fetchObject);
	
	virtual void Init();
	
	CGLContextObj GetContext() const;
};

#endif // _OPENEMU_DISPLAYOUTPUT_H_
