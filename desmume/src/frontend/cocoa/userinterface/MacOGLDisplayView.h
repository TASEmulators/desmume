/*
	Copyright (C) 2017 DeSmuME team

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

#ifndef _MAC_OGLDISPLAYOUTPUT_H_
#define _MAC_OGLDISPLAYOUTPUT_H_

#import <Cocoa/Cocoa.h>
#import <OpenGL/OpenGL.h>
#include <libkern/OSAtomic.h>

#import "DisplayViewCALayer.h"
#import "../cocoa_GPU.h"

#ifdef MAC_OS_X_VERSION_10_7
	#include "../OGLDisplayOutput_3_2.h"
#else
	#include "../OGLDisplayOutput.h"
#endif

#ifdef BOOL
#undef BOOL
#endif

class MacOGLDisplayView;

@interface DisplayViewOpenGLLayer : CAOpenGLLayer <DisplayViewCALayer>
{
	MacDisplayLayeredView *_cdv;
}

@end

class MacOGLClientFetchObject : public OGLClientFetchObject
{
protected:
	NSOpenGLContext *_nsContext;
	CGLContextObj _context;
	
	OSSpinLock _spinlockTexFetch[2];
	
public:
	void operator delete(void *ptr);
	MacOGLClientFetchObject();
	
	NSOpenGLContext* GetNSContext() const;
	CGLContextObj GetContext() const;
	
	virtual void Init();
	virtual void SetFetchBuffers(const NDSDisplayInfo &currentDisplayInfo);
	virtual void FetchFromBufferIndex(const u8 index);
	
	virtual GLuint GetFetchTexture(const NDSDisplayID displayID);
	virtual void SetFetchTexture(const NDSDisplayID displayID, GLuint texID);
};

class MacOGLDisplayPresenter : public OGLVideoOutput, public MacDisplayPresenterInterface
{
private:
	void __InstanceInit(MacClientSharedObject *sharedObject);
	
protected:
	NSOpenGLContext *_nsContext;
	NSOpenGLPixelFormat *_nsPixelFormat;
	CGLContextObj _context;
	CGLPixelFormatObj _pixelFormat;
	OSSpinLock _spinlockProcessedInfo;
	
public:
	void operator delete(void *ptr);
	MacOGLDisplayPresenter();
	MacOGLDisplayPresenter(MacClientSharedObject *sharedObject);
	
	virtual void Init();
	
	NSOpenGLPixelFormat* GetNSPixelFormat() const;
	NSOpenGLContext* GetNSContext() const;
	CGLPixelFormatObj GetPixelFormat() const;
	CGLContextObj GetContext() const;
	
	virtual void LoadHUDFont();
	virtual void SetScaleFactor(const double scaleFactor);
	
	// NDS screen filters
	virtual void SetFiltersPreferGPU(const bool preferGPU);
	virtual void SetOutputFilter(const OutputFilterTypeID filterID);
	virtual void SetPixelScaler(const VideoFilterTypeID filterID);
	
	// Client view interface
	virtual void LoadDisplays();
	virtual void ProcessDisplays();
	virtual void CopyFrameToBuffer(uint32_t *dstBuffer);
	
	virtual const OGLProcessedFrameInfo& GetProcessedFrameInfo();
	virtual void SetProcessedFrameInfo(const OGLProcessedFrameInfo &processedInfo);
	
	virtual void WriteLockEmuFramebuffer(const uint8_t bufferIndex);
	virtual void ReadLockEmuFramebuffer(const uint8_t bufferIndex);
	virtual void UnlockEmuFramebuffer(const uint8_t bufferIndex);
};

class MacOGLDisplayView : public MacDisplayLayeredView
{
private:
	void __InstanceInit(MacClientSharedObject *sharedObject);
	
protected:
	OSSpinLock _spinlockViewNeedsFlush;
		
public:
	MacOGLDisplayView();
	MacOGLDisplayView(MacClientSharedObject *sharedObject);
	virtual ~MacOGLDisplayView();
	
	virtual bool GetViewNeedsFlush();
	virtual void SetViewNeedsFlush();
	
	virtual void SetAllowViewFlushes(bool allowFlushes);
	
	virtual void SetUseVerticalSync(const bool useVerticalSync);
	
	// Client view interface
	virtual void FlushView(void *userData);
	virtual void FinalizeFlush(void *userData, uint64_t outputTime);
};

#endif // _MAC_OGLDISPLAYOUTPUT_H_
