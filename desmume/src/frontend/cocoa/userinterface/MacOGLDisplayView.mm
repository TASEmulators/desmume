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

#include "MacOGLDisplayView.h"
#include "../utilities.h"

@implementation DisplayViewOpenGLLayer

- (id)init
{
	self = [super init];
	if(self == nil)
	{
		return nil;
	}
	
	_cdv = new MacOGLDisplayView();
	_cdv->SetFrontendLayer(self);
	_cdv->Init();
	
	[self setAsynchronous:NO];
	[self setOpaque:YES];
	
	return self;
}

- (void)dealloc
{
	delete _cdv;
	
	[super dealloc];
}

- (OGLContextInfo *) contextInfo
{
	return _cdv->GetContextInfo();
}

- (ClientDisplay3DView *)clientDisplay3DView
{
	return _cdv;
}

- (BOOL)isAsynchronous
{
	return NO;
}

- (CGLPixelFormatObj)copyCGLPixelFormatForDisplayMask:(uint32_t)mask
{
	return _cdv->GetPixelFormat();
}

- (CGLContextObj)copyCGLContextForPixelFormat:(CGLPixelFormatObj)pixelFormat
{
	return _cdv->GetContext();
}

- (void)drawInCGLContext:(CGLContextObj)glContext pixelFormat:(CGLPixelFormatObj)pixelFormat forLayerTime:(CFTimeInterval)timeInterval displayTime:(const CVTimeStamp *)timeStamp
{
	_cdv->RenderViewOGL();
	[super drawInCGLContext:glContext pixelFormat:pixelFormat forLayerTime:timeInterval displayTime:timeStamp];
}

@end

void MacOGLDisplayView::operator delete(void *ptr)
{
	CGLPixelFormatObj pixelFormat = ((MacOGLDisplayView *)ptr)->GetPixelFormat();
	CGLContextObj context = ((MacOGLDisplayView *)ptr)->GetContext();
	OGLContextInfo *contextInfo = ((MacOGLDisplayView *)ptr)->GetContextInfo();
	
	if (context != NULL)
	{
		CGLContextObj prevContext = CGLGetCurrentContext();
		CGLSetCurrentContext(context);
		::operator delete(ptr);
		CGLSetCurrentContext(prevContext);
		
		delete contextInfo;
		CGLReleaseContext(context);
		CGLReleasePixelFormat(pixelFormat);
	}
}

MacOGLDisplayView::MacOGLDisplayView()
{
	// Initialize the OpenGL context
	bool useContext_3_2 = false;
	CGLPixelFormatAttribute attributes[] = {
		kCGLPFAColorSize, (CGLPixelFormatAttribute)24,
		kCGLPFAAlphaSize, (CGLPixelFormatAttribute)8,
		kCGLPFADepthSize, (CGLPixelFormatAttribute)0,
		kCGLPFAStencilSize, (CGLPixelFormatAttribute)0,
		kCGLPFADoubleBuffer,
		(CGLPixelFormatAttribute)0, (CGLPixelFormatAttribute)0,
		(CGLPixelFormatAttribute)0
	};
	
#ifdef _OGLDISPLAYOUTPUT_3_2_H_
	// If we can support a 3.2 Core Profile context, then request that in our
	// pixel format attributes.
	useContext_3_2 = IsOSXVersionSupported(10, 7, 0);
	if (useContext_3_2)
	{
		attributes[9] = kCGLPFAOpenGLProfile;
		attributes[10] = (CGLPixelFormatAttribute)kCGLOGLPVersion_3_2_Core;
	}
#endif
	
	GLint virtualScreenCount = 0;
	CGLChoosePixelFormat(attributes, &_pixelFormat, &virtualScreenCount);
	
	if (_pixelFormat == NULL)
	{
		// If we can't get a 3.2 Core Profile context, then switch to using a
		// legacy context instead.
		useContext_3_2 = false;
		attributes[9] = (CGLPixelFormatAttribute)0;
		attributes[10] = (CGLPixelFormatAttribute)0;
		CGLChoosePixelFormat(attributes, &_pixelFormat, &virtualScreenCount);
	}
	
	CGLCreateContext(_pixelFormat, NULL, &_context);
	
	CGLContextObj prevContext = CGLGetCurrentContext();
	CGLSetCurrentContext(_context);
	
#ifdef _OGLDISPLAYOUTPUT_3_2_H_
	if (useContext_3_2)
	{
		_contextInfo = new OGLContextInfo_3_2;
	}
	else
#endif
	{
		_contextInfo = new OGLContextInfo_Legacy;
	}
	
	CGLSetCurrentContext(prevContext);
	
	_willRenderToCALayer = false;
}

void MacOGLDisplayView::Init()
{
	CGLContextObj prevContext = CGLGetCurrentContext();
	CGLSetCurrentContext(this->_context);
	this->OGLVideoOutput::Init();
	CGLSetCurrentContext(prevContext);
}

void MacOGLDisplayView::_FrameRenderAndFlush()
{
	if (this->_willRenderToCALayer)
	{
		this->CALayerDisplay();
	}
	else
	{
		this->RenderViewOGL();
		CGLFlushDrawable(this->_context);
	}
}

CGLPixelFormatObj MacOGLDisplayView::GetPixelFormat() const
{
	return this->_pixelFormat;
}

CGLContextObj MacOGLDisplayView::GetContext() const
{
	return this->_context;
}

bool MacOGLDisplayView::GetRenderToCALayer() const
{
	return this->_willRenderToCALayer;
}

void MacOGLDisplayView::SetRenderToCALayer(const bool renderToLayer)
{
	this->_willRenderToCALayer = renderToLayer;
}

void MacOGLDisplayView::LoadHUDFont()
{
	CGLLockContext(this->_context);
	CGLSetCurrentContext(this->_context);
	this->OGLVideoOutput::LoadHUDFont();
	CGLUnlockContext(this->_context);
}

void MacOGLDisplayView::SetVideoBuffers(const uint32_t colorFormat,
										const void *videoBufferHead,
										const void *nativeBuffer0,
										const void *nativeBuffer1,
										const void *customBuffer0, const size_t customWidth0, const size_t customHeight0,
										const void *customBuffer1, const size_t customWidth1, const size_t customHeight1)
{
	CGLLockContext(this->_context);
	CGLSetCurrentContext(this->_context);
	
	this->OGLVideoOutput::SetVideoBuffers(colorFormat,
										  videoBufferHead,
										  nativeBuffer0,
										  nativeBuffer1,
										  customBuffer0, customWidth0, customHeight0,
										  customBuffer1, customWidth1, customHeight1);
	
	CGLUnlockContext(this->_context);
}

void MacOGLDisplayView::SetUseVerticalSync(const bool useVerticalSync)
{
	const GLint swapInt = (useVerticalSync) ? 1 : 0;
	
	CGLLockContext(this->_context);
	CGLSetCurrentContext(this->_context);
	CGLSetParameter(this->_context, kCGLCPSwapInterval, &swapInt);
	this->OGLVideoOutput::SetUseVerticalSync(useVerticalSync);
	CGLUnlockContext(this->_context);
}

void MacOGLDisplayView::SetScaleFactor(const double scaleFactor)
{
	CGLLockContext(this->_context);
	CGLSetCurrentContext(this->_context);
	this->OGLVideoOutput::SetScaleFactor(scaleFactor);
	CGLUnlockContext(this->_context);
}

void MacOGLDisplayView::SetFiltersPreferGPU(const bool preferGPU)
{
	CGLLockContext(this->_context);
	CGLSetCurrentContext(this->_context);
	this->OGLVideoOutput::SetFiltersPreferGPU(preferGPU);
	CGLUnlockContext(this->_context);
}

void MacOGLDisplayView::SetOutputFilter(const OutputFilterTypeID filterID)
{
	CGLLockContext(this->_context);
	CGLSetCurrentContext(this->_context);
	this->OGLVideoOutput::SetOutputFilter(filterID);
	CGLUnlockContext(this->_context);
}

void MacOGLDisplayView::SetPixelScaler(const VideoFilterTypeID filterID)
{
	CGLLockContext(this->_context);
	CGLSetCurrentContext(this->_context);
	this->OGLVideoOutput::SetPixelScaler(filterID);
	CGLUnlockContext(this->_context);
}

void MacOGLDisplayView::UpdateView()
{
	CGLLockContext(this->_context);
	CGLSetCurrentContext(this->_context);
	this->_FrameRenderAndFlush();
	CGLUnlockContext(this->_context);
}

void MacOGLDisplayView::FrameFinish()
{
	CGLLockContext(this->_context);
	CGLSetCurrentContext(this->_context);
	this->OGLVideoOutput::FrameFinish();
	CGLUnlockContext(this->_context);
}

void MacOGLDisplayView::HandleGPUFrameEndEvent(const NDSDisplayInfo &ndsDisplayInfo)
{
	this->OGLVideoOutput::HandleGPUFrameEndEvent(ndsDisplayInfo);
	
	CGLLockContext(this->_context);
	CGLSetCurrentContext(this->_context);
	this->LoadDisplays();
	this->ProcessDisplays();
	CGLUnlockContext(this->_context);
}
