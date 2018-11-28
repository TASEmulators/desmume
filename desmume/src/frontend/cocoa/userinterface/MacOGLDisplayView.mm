/*
	Copyright (C) 2017-2018 DeSmuME team

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
#include <mach/semaphore.h>
#include "../utilities.h"


@implementation DisplayViewOpenGLLayer

@synthesize _cdv;

- (id)init
{
	self = [super init];
	if(self == nil)
	{
		return nil;
	}
	
	_cdv = NULL;
	
    [self setBounds:CGRectMake(0.0f, 0.0f, (float)GPU_FRAMEBUFFER_NATIVE_WIDTH, (float)GPU_FRAMEBUFFER_NATIVE_HEIGHT)];
	[self setAsynchronous:NO];
	[self setOpaque:YES];
	
	return self;
}

- (OGLContextInfo *) contextInfo
{
	return ((MacOGLDisplayPresenter *)(_cdv->Get3DPresenter()))->GetContextInfo();
}

- (BOOL)isAsynchronous
{
	return NO;
}

- (CGLPixelFormatObj)copyCGLPixelFormatForDisplayMask:(uint32_t)mask
{
	return ((MacOGLDisplayPresenter *)(_cdv->Get3DPresenter()))->GetPixelFormat();
}

- (CGLContextObj)copyCGLContextForPixelFormat:(CGLPixelFormatObj)pixelFormat
{
	return ((MacOGLDisplayPresenter *)(_cdv->Get3DPresenter()))->GetContext();
}

- (void)drawInCGLContext:(CGLContextObj)glContext pixelFormat:(CGLPixelFormatObj)pixelFormat forLayerTime:(CFTimeInterval)timeInterval displayTime:(const CVTimeStamp *)timeStamp
{
	CGLLockContext(glContext);
	CGLSetCurrentContext(glContext);
	((MacOGLDisplayPresenter *)(_cdv->Get3DPresenter()))->RenderFrameOGL(false);
	[super drawInCGLContext:glContext pixelFormat:pixelFormat forLayerTime:timeInterval displayTime:timeStamp];
	CGLUnlockContext(glContext);
}

@end

#pragma mark -

void MacOGLClientFetchObject::operator delete(void *ptr)
{
	MacOGLClientFetchObject *fetchObjectPtr = (MacOGLClientFetchObject *)ptr;
	[(MacClientSharedObject *)(fetchObjectPtr->GetClientData()) release];
	
	CGLContextObj context = fetchObjectPtr->GetContext();
	
	if (context != NULL)
	{
		OGLContextInfo *contextInfo = fetchObjectPtr->GetContextInfo();
		CGLContextObj prevContext = CGLGetCurrentContext();
		CGLSetCurrentContext(context);
		
		[fetchObjectPtr->GetNSContext() release];
		delete contextInfo;
		::operator delete(ptr);
		
		CGLSetCurrentContext(prevContext);
	}
}

MacOGLClientFetchObject::MacOGLClientFetchObject()
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
		(NSOpenGLPixelFormatAttribute)0, (NSOpenGLPixelFormatAttribute)0,
		(NSOpenGLPixelFormatAttribute)0
	};
	
#ifdef _OGLDISPLAYOUTPUT_3_2_H_
	// If we can support a 3.2 Core Profile context, then request that in our
	// pixel format attributes.
	useContext_3_2 = IsOSXVersionSupported(10, 7, 0);
	if (useContext_3_2)
	{
		attributes[8] = NSOpenGLPFAOpenGLProfile;
		attributes[9] = (NSOpenGLPixelFormatAttribute)NSOpenGLProfileVersion3_2Core;
	}
#endif
	
	NSOpenGLPixelFormat *nsPixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
	if (nsPixelFormat == nil)
	{
		// If we can't get a 3.2 Core Profile context, then switch to using a
		// legacy context instead.
		useContext_3_2 = false;
		attributes[9] = (NSOpenGLPixelFormatAttribute)0;
		attributes[10] = (NSOpenGLPixelFormatAttribute)0;
		nsPixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
	}
	
	_nsContext = [[NSOpenGLContext alloc] initWithFormat:nsPixelFormat shareContext:nil];
	_context = (CGLContextObj)[_nsContext CGLContextObj];
	
	[nsPixelFormat release];
	
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
	
	_clientData = [[MacClientSharedObject alloc] init];
	
	_spinlockTexFetch[NDSDisplayID_Main]  = OS_SPINLOCK_INIT;
	_spinlockTexFetch[NDSDisplayID_Touch] = OS_SPINLOCK_INIT;
}

NSOpenGLContext* MacOGLClientFetchObject::GetNSContext() const
{
	return this->_nsContext;
}

CGLContextObj MacOGLClientFetchObject::GetContext() const
{
	return this->_context;
}

void MacOGLClientFetchObject::Init()
{
	[(MacClientSharedObject *)this->_clientData setGPUFetchObject:this];
	
	CGLContextObj prevContext = CGLGetCurrentContext();
	CGLSetCurrentContext(_context);
	this->OGLClientFetchObject::Init();
	CGLSetCurrentContext(prevContext);
}

void MacOGLClientFetchObject::SetFetchBuffers(const NDSDisplayInfo &currentDisplayInfo)
{
	CGLLockContext(this->_context);
	CGLSetCurrentContext(this->_context);
	this->OGLClientFetchObject::SetFetchBuffers(currentDisplayInfo);
	CGLUnlockContext(this->_context);
}

void MacOGLClientFetchObject::FetchFromBufferIndex(const u8 index)
{
	MacClientSharedObject *sharedViewObject = (MacClientSharedObject *)this->_clientData;
	this->_useDirectToCPUFilterPipeline = ([sharedViewObject numberViewsUsingDirectToCPUFiltering] > 0);
	
	semaphore_wait([sharedViewObject semaphoreFramebufferPageAtIndex:index]);
	[sharedViewObject setFramebufferState:ClientDisplayBufferState_Reading index:index];
	
	CGLLockContext(this->_context);
	CGLSetCurrentContext(this->_context);
	this->OGLClientFetchObject::FetchFromBufferIndex(index);
	CGLUnlockContext(this->_context);
	
	[sharedViewObject setFramebufferState:ClientDisplayBufferState_Idle index:index];
	semaphore_signal([sharedViewObject semaphoreFramebufferPageAtIndex:index]);
}

GLuint MacOGLClientFetchObject::GetFetchTexture(const NDSDisplayID displayID)
{
	OSSpinLockLock(&this->_spinlockTexFetch[displayID]);
	const GLuint texFetchID = this->OGLClientFetchObject::GetFetchTexture(displayID);
	OSSpinLockUnlock(&this->_spinlockTexFetch[displayID]);
	
	return texFetchID;
}

void MacOGLClientFetchObject::SetFetchTexture(const NDSDisplayID displayID, GLuint texID)
{
	OSSpinLockLock(&this->_spinlockTexFetch[displayID]);
	this->OGLClientFetchObject::SetFetchTexture(displayID, texID);
	OSSpinLockUnlock(&this->_spinlockTexFetch[displayID]);
}

#pragma mark -

void MacOGLDisplayPresenter::operator delete(void *ptr)
{
	CGLContextObj context = ((MacOGLDisplayPresenter *)ptr)->GetContext();
	
	if (context != NULL)
	{
		OGLContextInfo *contextInfo = ((MacOGLDisplayPresenter *)ptr)->GetContextInfo();
		CGLContextObj prevContext = CGLGetCurrentContext();
		CGLSetCurrentContext(context);
		
		[((MacOGLDisplayPresenter *)ptr)->GetNSContext() release];
		[((MacOGLDisplayPresenter *)ptr)->GetNSPixelFormat() release];
		delete contextInfo;
		::operator delete(ptr);
		
		CGLSetCurrentContext(prevContext);
	}
}

MacOGLDisplayPresenter::MacOGLDisplayPresenter()
{
	__InstanceInit(nil);
}

MacOGLDisplayPresenter::MacOGLDisplayPresenter(MacClientSharedObject *sharedObject) : MacDisplayPresenterInterface(sharedObject)
{
	__InstanceInit(sharedObject);
}

void MacOGLDisplayPresenter::__InstanceInit(MacClientSharedObject *sharedObject)
{
	// Initialize the OpenGL context.
	//
	// We create an NSOpenGLContext and extract the CGLContextObj from it because
	// [NSOpenGLContext CGLContextObj] is available on macOS 10.5 Leopard, but
	// [NSOpenGLContext initWithCGLContextObj:] is only available on macOS 10.6
	// Snow Leopard.
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
	bool useContext_3_2 = IsOSXVersionSupported(10, 7, 0);
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
		attributes[9] = (NSOpenGLPixelFormatAttribute)0;
		attributes[10] = (NSOpenGLPixelFormatAttribute)0;
		_nsPixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
	}
	
	_pixelFormat = (CGLPixelFormatObj)[_nsPixelFormat CGLPixelFormatObj];
	
	_nsContext = nil;
	_context = nil;
	_spinlockProcessedInfo = OS_SPINLOCK_INIT;
	
	if (sharedObject != nil)
	{
		SetFetchObject([sharedObject GPUFetchObject]);
	}
}

void MacOGLDisplayPresenter::Init()
{
	this->_nsContext = [[NSOpenGLContext alloc] initWithFormat:this->_nsPixelFormat
												  shareContext:((MacOGLClientFetchObject *)this->_fetchObject)->GetNSContext()];
	this->_context = (CGLContextObj)[this->_nsContext CGLContextObj];
	
	CGLContextObj prevContext = CGLGetCurrentContext();
	CGLSetCurrentContext(this->_context);
	
#ifdef _OGLDISPLAYOUTPUT_3_2_H_
	GLint profileVersion = 0;
	CGLDescribePixelFormat(this->_pixelFormat, 0, kCGLPFAOpenGLProfile, &profileVersion);
	
	if (profileVersion == kCGLOGLPVersion_3_2_Core)
	{
		this->_contextInfo = new OGLContextInfo_3_2;
	}
	else
#endif
	{
		this->_contextInfo = new OGLContextInfo_Legacy;
	}
	
	this->OGLVideoOutput::Init();
	CGLSetCurrentContext(prevContext);
}

NSOpenGLPixelFormat* MacOGLDisplayPresenter::GetNSPixelFormat() const
{
	return this->_nsPixelFormat;
}

NSOpenGLContext* MacOGLDisplayPresenter::GetNSContext() const
{
	return this->_nsContext;
}

CGLPixelFormatObj MacOGLDisplayPresenter::GetPixelFormat() const
{
	return this->_pixelFormat;
}

CGLContextObj MacOGLDisplayPresenter::GetContext() const
{
	return this->_context;
}

void MacOGLDisplayPresenter::LoadHUDFont()
{
	CGLLockContext(this->_context);
	CGLSetCurrentContext(this->_context);
	this->OGLVideoOutput::LoadHUDFont();
	CGLUnlockContext(this->_context);
}

void MacOGLDisplayPresenter::SetScaleFactor(const double scaleFactor)
{
	CGLLockContext(this->_context);
	CGLSetCurrentContext(this->_context);
	this->OGLVideoOutput::SetScaleFactor(scaleFactor);
	CGLUnlockContext(this->_context);
}

void MacOGLDisplayPresenter::SetFiltersPreferGPU(const bool preferGPU)
{
	CGLLockContext(this->_context);
	CGLSetCurrentContext(this->_context);
	this->OGLVideoOutput::SetFiltersPreferGPU(preferGPU);
	CGLUnlockContext(this->_context);
}

void MacOGLDisplayPresenter::SetOutputFilter(const OutputFilterTypeID filterID)
{
	CGLLockContext(this->_context);
	CGLSetCurrentContext(this->_context);
	this->OGLVideoOutput::SetOutputFilter(filterID);
	CGLUnlockContext(this->_context);
}

void MacOGLDisplayPresenter::SetPixelScaler(const VideoFilterTypeID filterID)
{
	CGLLockContext(this->_context);
	CGLSetCurrentContext(this->_context);
	this->OGLVideoOutput::SetPixelScaler(filterID);
	CGLUnlockContext(this->_context);
}

// NDS GPU Interface
void MacOGLDisplayPresenter::LoadDisplays()
{
	CGLLockContext(this->_context);
	CGLSetCurrentContext(this->_context);
	this->OGLVideoOutput::LoadDisplays();
	CGLUnlockContext(this->_context);
}

void MacOGLDisplayPresenter::ProcessDisplays()
{
	CGLLockContext(this->_context);
	CGLSetCurrentContext(this->_context);
	this->OGLVideoOutput::ProcessDisplays();
	CGLUnlockContext(this->_context);
}

void MacOGLDisplayPresenter::CopyFrameToBuffer(uint32_t *dstBuffer)
{
	CGLLockContext(this->_context);
	CGLSetCurrentContext(this->_context);
	this->OGLVideoOutput::CopyFrameToBuffer(dstBuffer);
	CGLUnlockContext(this->_context);
}

const OGLProcessedFrameInfo& MacOGLDisplayPresenter::GetProcessedFrameInfo()
{
	OSSpinLockLock(&this->_spinlockProcessedInfo);
	const OGLProcessedFrameInfo &processedInfo = this->OGLVideoOutput::GetProcessedFrameInfo();
	OSSpinLockUnlock(&this->_spinlockProcessedInfo);
	
	return processedInfo;
}

void MacOGLDisplayPresenter::SetProcessedFrameInfo(const OGLProcessedFrameInfo &processedInfo)
{
	OSSpinLockLock(&this->_spinlockProcessedInfo);
	this->OGLVideoOutput::SetProcessedFrameInfo(processedInfo);
	OSSpinLockUnlock(&this->_spinlockProcessedInfo);
}

void MacOGLDisplayPresenter::WriteLockEmuFramebuffer(const uint8_t bufferIndex)
{
	const GPUClientFetchObject &fetchObj = this->GetFetchObject();
	MacClientSharedObject *sharedViewObject = (MacClientSharedObject *)fetchObj.GetClientData();
	
	semaphore_wait([sharedViewObject semaphoreFramebufferPageAtIndex:bufferIndex]);
}

void MacOGLDisplayPresenter::ReadLockEmuFramebuffer(const uint8_t bufferIndex)
{
	const GPUClientFetchObject &fetchObj = this->GetFetchObject();
	MacClientSharedObject *sharedViewObject = (MacClientSharedObject *)fetchObj.GetClientData();
	
	semaphore_wait([sharedViewObject semaphoreFramebufferPageAtIndex:bufferIndex]);
}

void MacOGLDisplayPresenter::UnlockEmuFramebuffer(const uint8_t bufferIndex)
{
	const GPUClientFetchObject &fetchObj = this->GetFetchObject();
	MacClientSharedObject *sharedViewObject = (MacClientSharedObject *)fetchObj.GetClientData();
	
	semaphore_signal([sharedViewObject semaphoreFramebufferPageAtIndex:bufferIndex]);
}

#pragma mark -

MacOGLDisplayView::MacOGLDisplayView()
{
	__InstanceInit(nil);
}

MacOGLDisplayView::MacOGLDisplayView(MacClientSharedObject *sharedObject)
{
	__InstanceInit(sharedObject);
}

MacOGLDisplayView::~MacOGLDisplayView()
{
	[this->_caLayer release];
}

void MacOGLDisplayView::__InstanceInit(MacClientSharedObject *sharedObject)
{
	_allowViewUpdates = false;
	_spinlockViewNeedsFlush = OS_SPINLOCK_INIT;
	
	MacOGLDisplayPresenter *newOpenGLPresenter = new MacOGLDisplayPresenter(sharedObject);
	_presenter = newOpenGLPresenter;
	
	_caLayer = [[DisplayViewOpenGLLayer alloc] init];
	[_caLayer setClientDisplayView:this];
}

bool MacOGLDisplayView::GetViewNeedsFlush()
{
	OSSpinLockLock(&this->_spinlockViewNeedsFlush);
	const bool viewNeedsFlush = this->_viewNeedsFlush;
	OSSpinLockUnlock(&this->_spinlockViewNeedsFlush);
	
	return viewNeedsFlush;
}

void MacOGLDisplayView::SetViewNeedsFlush()
{
	if (!this->_allowViewUpdates || (this->_presenter == nil) || (this->_caLayer == nil))
	{
		return;
	}
	
	if (this->GetRenderToCALayer())
	{
		[this->_caLayer setNeedsDisplay];
		[CATransaction flush];
	}
	else
	{
		// For every update, ensure that the CVDisplayLink is started so that the update
		// will eventually get flushed.
		this->SetAllowViewFlushes(true);
		
		OSSpinLockLock(&this->_spinlockViewNeedsFlush);
		this->_viewNeedsFlush = true;
		OSSpinLockUnlock(&this->_spinlockViewNeedsFlush);
	}
}

void MacOGLDisplayView::SetAllowViewFlushes(bool allowFlushes)
{
	CGDirectDisplayID displayID = (CGDirectDisplayID)this->GetDisplayViewID();
	MacClientSharedObject *sharedData = ((MacOGLDisplayPresenter *)this->_presenter)->GetSharedData();
	[sharedData displayLinkStartUsingID:displayID];
}

void MacOGLDisplayView::SetUseVerticalSync(const bool useVerticalSync)
{
	CGLContextObj context = ((MacOGLDisplayPresenter *)this->_presenter)->GetContext();
	const GLint swapInt = (useVerticalSync) ? 1 : 0;
	
	CGLLockContext(context);
	CGLSetCurrentContext(context);
	CGLSetParameter(context, kCGLCPSwapInterval, &swapInt);
	MacDisplayLayeredView::SetUseVerticalSync(useVerticalSync);
	CGLUnlockContext(context);
}

void MacOGLDisplayView::FlushView(void *userData)
{
	OSSpinLockLock(&this->_spinlockViewNeedsFlush);
	this->_viewNeedsFlush = false;
	OSSpinLockUnlock(&this->_spinlockViewNeedsFlush);
	
	CGLContextObj context = ((MacOGLDisplayPresenter *)this->_presenter)->GetContext();
	
	CGLLockContext(context);
	CGLSetCurrentContext(context);
	((MacOGLDisplayPresenter *)this->_presenter)->RenderFrameOGL(false);
	CGLUnlockContext(context);
}

void MacOGLDisplayView::FinalizeFlush(void *userData, uint64_t outputTime)
{
	CGLContextObj context = ((MacOGLDisplayPresenter *)this->_presenter)->GetContext();
	
	CGLLockContext(context);
	CGLSetCurrentContext(context);
	CGLFlushDrawable(context);
	CGLUnlockContext(context);
}
