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

void MacOGLDisplayView::operator delete(void *ptr)
{
	CGLContextObj context = ((MacOGLDisplayView *)ptr)->GetContext();
	
	CGLContextObj prevContext = CGLGetCurrentContext();
	CGLSetCurrentContext(context);
	::operator delete(ptr);
	CGLSetCurrentContext(prevContext);
	
	CGLReleaseContext(context);
}

void MacOGLDisplayView::Init()
{
	// Initialize the OpenGL context
	CGLPixelFormatAttribute attributes[] = {
		kCGLPFAColorSize, (CGLPixelFormatAttribute)24,
		kCGLPFAAlphaSize, (CGLPixelFormatAttribute)8,
		kCGLPFADepthSize, (CGLPixelFormatAttribute)0,
		kCGLPFAStencilSize, (CGLPixelFormatAttribute)0,
		kCGLPFADoubleBuffer,
		(CGLPixelFormatAttribute)0, (CGLPixelFormatAttribute)0,
		(CGLPixelFormatAttribute)0 };
	
	OGLInfoCreate_Func = &OGLInfoCreate_Legacy;
	
#ifdef _OGLDISPLAYOUTPUT_3_2_H_
	// If we can support a 3.2 Core Profile context, then request that in our
	// pixel format attributes.
	if (IsOSXVersionSupported(10, 7, 0))
	{
		attributes[9] = kCGLPFAOpenGLProfile;
		attributes[10] = (CGLPixelFormatAttribute)kCGLOGLPVersion_3_2_Core;
		OGLInfoCreate_Func = &OGLInfoCreate_3_2;
	}
#endif
	
	CGLPixelFormatObj cglPixFormat = NULL;
	GLint virtualScreenCount = 0;
	CGLChoosePixelFormat(attributes, &cglPixFormat, &virtualScreenCount);
	
	if (cglPixFormat == NULL)
	{
		// If we can't get a 3.2 Core Profile context, then switch to using a
		// legacy context instead.
		attributes[9] = (CGLPixelFormatAttribute)0;
		attributes[10] = (CGLPixelFormatAttribute)0;
		OGLInfoCreate_Func = &OGLInfoCreate_Legacy;
		CGLChoosePixelFormat(attributes, &cglPixFormat, &virtualScreenCount);
	}
	
	CGLCreateContext(cglPixFormat, NULL, &this->_context);
	CGLReleasePixelFormat(cglPixFormat);
	
	CGLContextObj prevContext = CGLGetCurrentContext();
	CGLSetCurrentContext(this->_context);
	this->OGLVideoOutput::Init();
	CGLSetCurrentContext(prevContext);
}

void MacOGLDisplayView::_FrameRenderAndFlush()
{
	this->FrameRender();
	CGLFlushDrawable(this->_context);
}

CGLContextObj MacOGLDisplayView::GetContext() const
{
	return this->_context;
}

void MacOGLDisplayView::SetContext(CGLContextObj context)
{
	this->_context = context;
}

void MacOGLDisplayView::SetHUDFontUsingPath(const char *filePath)
{
	CGLLockContext(this->_context);
	CGLSetCurrentContext(this->_context);
	this->OGLVideoOutput::SetHUDFontUsingPath(filePath);
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

void MacOGLDisplayView::SetupViewProperties()
{
	CGLLockContext(this->_context);
	CGLSetCurrentContext(this->_context);
	this->OGLVideoOutput::SetupViewProperties();
	this->_FrameRenderAndFlush();
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

void MacOGLDisplayView::SetHUDVisibility(const bool visibleState)
{
	CGLLockContext(this->_context);
	CGLSetCurrentContext(this->_context);
	this->OGLVideoOutput::SetHUDVisibility(visibleState);
	this->_FrameRenderAndFlush();
	CGLUnlockContext(this->_context);
}

void MacOGLDisplayView::SetHUDShowVideoFPS(const bool visibleState)
{
	CGLLockContext(this->_context);
	CGLSetCurrentContext(this->_context);
	this->OGLVideoOutput::SetHUDShowVideoFPS(visibleState);
	this->_FrameRenderAndFlush();
	CGLUnlockContext(this->_context);
}

void MacOGLDisplayView::SetHUDShowRender3DFPS(const bool visibleState)
{
	CGLLockContext(this->_context);
	CGLSetCurrentContext(this->_context);
	this->OGLVideoOutput::SetHUDShowRender3DFPS(visibleState);
	this->_FrameRenderAndFlush();
	CGLUnlockContext(this->_context);
}

void MacOGLDisplayView::SetHUDShowFrameIndex(const bool visibleState)
{
	CGLLockContext(this->_context);
	CGLSetCurrentContext(this->_context);
	this->OGLVideoOutput::SetHUDShowFrameIndex(visibleState);
	this->_FrameRenderAndFlush();
	CGLUnlockContext(this->_context);
}

void MacOGLDisplayView::SetHUDShowLagFrameCount(const bool visibleState)
{
	CGLLockContext(this->_context);
	CGLSetCurrentContext(this->_context);
	this->OGLVideoOutput::SetHUDShowLagFrameCount(visibleState);
	this->_FrameRenderAndFlush();
	CGLUnlockContext(this->_context);
}

void MacOGLDisplayView::SetHUDShowCPULoadAverage(const bool visibleState)
{
	CGLLockContext(this->_context);
	CGLSetCurrentContext(this->_context);
	this->OGLVideoOutput::SetHUDShowCPULoadAverage(visibleState);
	this->_FrameRenderAndFlush();
	CGLUnlockContext(this->_context);
}

void MacOGLDisplayView::SetHUDShowRTC(const bool visibleState)
{
	CGLLockContext(this->_context);
	CGLSetCurrentContext(this->_context);
	this->OGLVideoOutput::SetHUDShowRTC(visibleState);
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

void MacOGLDisplayView::HandleGPUFrameEndEvent(const bool isMainSizeNative, const bool isTouchSizeNative)
{
	CGLLockContext(this->_context);
	CGLSetCurrentContext(this->_context);
	this->FrameLoadGPU(isMainSizeNative, isTouchSizeNative);
	this->FrameProcessGPU();
	CGLUnlockContext(this->_context);
}

void MacOGLDisplayView::HandleEmulatorFrameEndEvent(const NDSFrameInfo &frameInfo)
{
	this->SetHUDInfo(frameInfo);
	
	CGLLockContext(this->_context);
	CGLSetCurrentContext(this->_context);
	this->FrameProcessHUD();
	this->_FrameRenderAndFlush();
	CGLUnlockContext(this->_context);
}

void MacOGLDisplayView::UpdateView()
{
	CGLLockContext(this->_context);
	CGLSetCurrentContext(this->_context);
	this->_FrameRenderAndFlush();
	CGLUnlockContext(this->_context);
}
