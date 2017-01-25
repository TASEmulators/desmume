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
	
    [self setBounds:CGRectMake(0.0f, 0.0f, (float)GPU_FRAMEBUFFER_NATIVE_WIDTH, (float)GPU_FRAMEBUFFER_NATIVE_HEIGHT)];
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
	CGLSetCurrentContext(glContext);
	CGLLockContext(glContext);
	_cdv->RenderViewOGL();
	[super drawInCGLContext:glContext pixelFormat:pixelFormat forLayerTime:timeInterval displayTime:timeStamp];
	CGLUnlockContext(glContext);
}

@end

void MacOGLDisplayView::operator delete(void *ptr)
{
	CGLContextObj context = ((MacOGLDisplayView *)ptr)->GetContext();
	OGLContextInfo *contextInfo = ((MacOGLDisplayView *)ptr)->GetContextInfo();
	
	if (context != NULL)
	{
		CGLContextObj prevContext = CGLGetCurrentContext();
		CGLSetCurrentContext(context);
		::operator delete(ptr);
		CGLSetCurrentContext(prevContext);
		
		delete contextInfo;
		[((MacOGLDisplayView *)ptr)->GetNSContext() release];
		[((MacOGLDisplayView *)ptr)->GetNSPixelFormat() release];
	}
}

MacOGLDisplayView::MacOGLDisplayView()
{
	// Initialize the OpenGL context.
	//
	// We create an NSOpenGLContext and extract the CGLContextObj from it because
	// [NSOpenGLContext CGLContextObj] is available on macOS 10.5 Leopard, but
	// [NSOpenGLContext initWithCGLContextObj:] is only available on macOS 10.6
	// Snow Leopard.
	bool useContext_3_2 = false;
	NSOpenGLPixelFormatAttribute attributes[] = {
		NSOpenGLPFAColorSize, (NSOpenGLPixelFormatAttribute)24,
		NSOpenGLPFAAlphaSize, (NSOpenGLPixelFormatAttribute)8,
		NSOpenGLPFADepthSize, (NSOpenGLPixelFormatAttribute)0,
		NSOpenGLPFAStencilSize, (NSOpenGLPixelFormatAttribute)0,
		NSOpenGLPFADoubleBuffer,
		(NSOpenGLPixelFormatAttribute)0, (NSOpenGLPixelFormatAttribute)0,
		(NSOpenGLPixelFormatAttribute)0
	};
	
#ifdef _OGLDISPLAYOUTPUT_3_2_H_
	// If we can support a 3.2 Core Profile context, then request that in our
	// pixel format attributes.
	useContext_3_2 = IsOSXVersionSupported(10, 7, 0);
	if (useContext_3_2)
	{
		attributes[9] = NSOpenGLPFAOpenGLProfile;
		attributes[10] = (NSOpenGLPixelFormatAttribute)NSOpenGLProfileVersion3_2Core;
	}
#endif
	
	_nsPixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
	if (_nsPixelFormat == nil)
	{
		// If we can't get a 3.2 Core Profile context, then switch to using a
		// legacy context instead.
		useContext_3_2 = false;
		attributes[9] = (NSOpenGLPixelFormatAttribute)0;
		attributes[10] = (NSOpenGLPixelFormatAttribute)0;
		_nsPixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
	}
	
	_nsContext = [[NSOpenGLContext alloc] initWithFormat:_nsPixelFormat shareContext:nil];
	_context = (CGLContextObj)[_nsContext CGLContextObj];
	_pixelFormat = (CGLPixelFormatObj)[_nsPixelFormat CGLPixelFormatObj];
	
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

NSOpenGLPixelFormat* MacOGLDisplayView::GetNSPixelFormat() const
{
	return this->_nsPixelFormat;
}

NSOpenGLContext* MacOGLDisplayView::GetNSContext() const
{
	return this->_nsContext;
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

// NDS GPU Interface
void MacOGLDisplayView::LoadDisplays()
{
	CGLLockContext(this->_context);
	CGLSetCurrentContext(this->_context);
	this->OGLVideoOutput::LoadDisplays();
	CGLUnlockContext(this->_context);
}

void MacOGLDisplayView::ProcessDisplays()
{
	CGLLockContext(this->_context);
	CGLSetCurrentContext(this->_context);
	this->OGLVideoOutput::ProcessDisplays();
	CGLUnlockContext(this->_context);
}

void MacOGLDisplayView::UpdateView()
{
	if (this->_willRenderToCALayer)
	{
		this->CALayerDisplay();
	}
	else
	{
		CGLLockContext(this->_context);
		CGLSetCurrentContext(this->_context);
		this->RenderViewOGL();
		CGLFlushDrawable(this->_context);
		CGLUnlockContext(this->_context);
	}
}

void MacOGLDisplayView::FrameFinish()
{
	CGLLockContext(this->_context);
	CGLSetCurrentContext(this->_context);
	this->OGLVideoOutput::FrameFinish();
	CGLUnlockContext(this->_context);
}
