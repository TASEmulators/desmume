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
	MacOGLDisplayView *_cdv;
}
@end

class MacOGLClientFetchObject : public OGLClientFetchObject
{
protected:
	NSOpenGLContext *_nsContext;
	CGLContextObj _context;
	
public:
	void operator delete(void *ptr);
	MacOGLClientFetchObject();
	
	NSOpenGLContext* GetNSContext() const;
	CGLContextObj GetContext() const;
	
	virtual void Init();
	virtual void SetFetchBuffers(const NDSDisplayInfo &currentDisplayInfo);
	virtual void FetchFromBufferIndex(const u8 index);
};

class MacOGLDisplayView : public OGLVideoOutput, public DisplayViewCALayerInterface
{
protected:
	NSOpenGLContext *_nsContext;
	NSOpenGLPixelFormat *_nsPixelFormat;
	CGLContextObj _context;
	CGLPixelFormatObj _pixelFormat;
	bool _willRenderToCALayer;
		
public:
	void operator delete(void *ptr);
	MacOGLDisplayView();
	
	virtual void Init();
	
	NSOpenGLPixelFormat* GetNSPixelFormat() const;
	NSOpenGLContext* GetNSContext() const;
	CGLPixelFormatObj GetPixelFormat() const;
	CGLContextObj GetContext() const;
	
	bool GetRenderToCALayer() const;
	void SetRenderToCALayer(const bool renderToLayer);
		
	virtual void LoadHUDFont();
	
	virtual void SetUseVerticalSync(const bool useVerticalSync);
	virtual void SetScaleFactor(const double scaleFactor);
	
	// NDS screen filters
	virtual void SetFiltersPreferGPU(const bool preferGPU);
	virtual void SetOutputFilter(const OutputFilterTypeID filterID);
	virtual void SetPixelScaler(const VideoFilterTypeID filterID);
	
	// Client view interface
	virtual void LoadDisplays();
	virtual void ProcessDisplays();
	virtual void UpdateView();
	virtual void FinishFrameAtIndex(const u8 bufferIndex);
	virtual void LockDisplayTextures();
	virtual void UnlockDisplayTextures();
};

#endif // _MAC_OGLDISPLAYOUTPUT_H_
