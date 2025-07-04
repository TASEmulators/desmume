/*
	Copyright (C) 2017-2025 DeSmuME team

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
#import "../cocoa_globals.h"

#if !defined(MAC_OS_X_VERSION_10_9)
	#define NSOpenGLProfileVersion4_1Core 0x4100
#endif

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

MacOGLClientSharedData::MacOGLClientSharedData()
{
	_unfairlockTexFetch[NDSDisplayID_Main] = apple_unfairlock_create();
	_unfairlockTexFetch[NDSDisplayID_Touch] = apple_unfairlock_create();
}

MacOGLClientSharedData::~MacOGLClientSharedData()
{
	apple_unfairlock_destroy(this->_unfairlockTexFetch[NDSDisplayID_Main]);
	this->_unfairlockTexFetch[NDSDisplayID_Main] = NULL;
	apple_unfairlock_destroy(this->_unfairlockTexFetch[NDSDisplayID_Touch]);
	this->_unfairlockTexFetch[NDSDisplayID_Touch] = NULL;
}

GLuint MacOGLClientSharedData::GetFetchTexture(const NDSDisplayID displayID)
{
	apple_unfairlock_lock(this->_unfairlockTexFetch[displayID]);
	const GLuint texFetchID = this->OGLClientSharedData::GetFetchTexture(displayID);
	apple_unfairlock_unlock(this->_unfairlockTexFetch[displayID]);
	
	return texFetchID;
}

void MacOGLClientSharedData::SetFetchTexture(const NDSDisplayID displayID, GLuint texID)
{
	apple_unfairlock_lock(this->_unfairlockTexFetch[displayID]);
	this->OGLClientSharedData::SetFetchTexture(displayID, texID);
	apple_unfairlock_unlock(this->_unfairlockTexFetch[displayID]);
}

#pragma mark -

void MacOGLClientFetchObject::operator delete(void *ptr)
{
	MacOGLClientFetchObject *fetchObjectPtr = (MacOGLClientFetchObject *)ptr;
	CGLContextObj context = fetchObjectPtr->GetContext();
	
	if (context != NULL)
	{
		CGLContextObj prevContext = CGLGetCurrentContext();
		CGLSetCurrentContext(context);
		
		[fetchObjectPtr->GetNSContext() release];
		::operator delete(ptr);
		
		CGLSetCurrentContext(prevContext);
	}
}

MacOGLClientFetchObject::MacOGLClientFetchObject()
{
	const bool isMavericksSupported    = IsOSXVersionSupported(10, 9, 0);
	const bool isMountainLionSupported = (isMavericksSupported    || IsOSXVersionSupported(10, 8, 0));
	const bool isLionSupported         = (isMountainLionSupported || IsOSXVersionSupported(10, 7, 0));
	
	NSOpenGLPixelFormat *nsPixelFormat = nil;
	bool useContext_3_2 = false;
	
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
		NSOpenGLPFAAccelerated,
		NSOpenGLPFANoRecovery,
		(NSOpenGLPixelFormatAttribute)0, (NSOpenGLPixelFormatAttribute)0,
		(NSOpenGLPixelFormatAttribute)0
	};
	
	// Normally, supporting Automatic Graphics Switching and allowing the context
	// to use the integrated GPU requires NSOpenGLPFAAllowOfflineRenderers in the
	// pixel format attributes. However, GPUs that can't support Metal are also
	// too slow to run most of the video filters on OpenGL, and also show poor
	// performance when running multiple display windows with high GPU scaling
	// factors. Therefore, we will NOT allow OpenGL to run contexts on the
	// integrated GPU for performance reasons. -rogerman (2025/03/26)
	
#ifdef _OGLDISPLAYOUTPUT_3_2_H_
	// Request the latest context profile that is available on the system.
	if (isLionSupported)
	{
		attributes[10] = NSOpenGLPFAOpenGLProfile;
		if ( (nsPixelFormat == nil) && isMavericksSupported )
		{
			attributes[11] = (NSOpenGLPixelFormatAttribute)NSOpenGLProfileVersion4_1Core;
			nsPixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
			useContext_3_2 = true;
		}
		
		if ( (nsPixelFormat == nil) && isMountainLionSupported )
		{
			attributes[11] = (NSOpenGLPixelFormatAttribute)NSOpenGLProfileVersion3_2Core;
			nsPixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
			useContext_3_2 = true;
		}
		
		if ( (nsPixelFormat == nil) && isLionSupported )
		{
			attributes[11] = (NSOpenGLPixelFormatAttribute)NSOpenGLProfileVersionLegacy;
			nsPixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
			useContext_3_2 = false;
		}
	}
#endif
	
	if (nsPixelFormat == nil)
	{
		// Don't request any specific context version, which defaults to requesting a
		// legacy context.
		attributes[10] = (NSOpenGLPixelFormatAttribute)0;
		attributes[11] = (NSOpenGLPixelFormatAttribute)0;
		nsPixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
		useContext_3_2 = false;
	}
	
	if (nsPixelFormat == nil)
	{
		// Something is seriously wrong if we haven't gotten a context yet.
		// Fall back to the software renderer.
		attributes[8] = (NSOpenGLPixelFormatAttribute)0;
		attributes[9] = (NSOpenGLPixelFormatAttribute)0;
		nsPixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
		useContext_3_2 = false;
	}
	
	_nsContext = [[NSOpenGLContext alloc] initWithFormat:nsPixelFormat shareContext:nil];
	_context = (CGLContextObj)[_nsContext CGLContextObj];
	
	[nsPixelFormat release];
	
	CGLContextObj prevContext = CGLGetCurrentContext();
	CGLSetCurrentContext(_context);
	
	OGLContextInfo *newContextInfo = NULL;
	
#ifdef _OGLDISPLAYOUTPUT_3_2_H_
	if (useContext_3_2)
	{
		newContextInfo = new OGLContextInfo_3_2;
	}
	else
#endif
	{
		newContextInfo = new OGLContextInfo_Legacy;
	}
	
	CGLSetCurrentContext(prevContext);
	
	_id = GPUClientFetchObjectID_MacOpenGL;
	snprintf(_name, sizeof(_name) - 1, "macOS OpenGL v%i.%i", newContextInfo->GetVersionMajor(), newContextInfo->GetVersionMinor());
	strlcpy(_description, newContextInfo->GetRendererString(), sizeof(_description) - 1);
	
	_clientData = new MacOGLClientSharedData;
	((MacOGLClientSharedData *)_clientData)->SetContextInfo(newContextInfo);
}

MacOGLClientFetchObject::~MacOGLClientFetchObject()
{
	delete (MacOGLClientSharedData *)this->_clientData;
}

NSOpenGLContext* MacOGLClientFetchObject::GetNSContext() const
{
	return this->_nsContext;
}

CGLContextObj MacOGLClientFetchObject::GetContext() const
{
	return this->_context;
}

void MacOGLClientFetchObject::FetchNativeDisplayToSrcClone(const NDSDisplayID displayID, const u8 bufferIndex, bool needsLock)
{
	MacOGLClientSharedData *sharedData = (MacOGLClientSharedData *)this->_clientData;
	sharedData->FetchNativeDisplayToSrcClone(this->_fetchDisplayInfo, displayID, bufferIndex, needsLock);
}

void MacOGLClientFetchObject::FetchCustomDisplayToSrcClone(const NDSDisplayID displayID, const u8 bufferIndex, bool needsLock)
{
	MacOGLClientSharedData *sharedData = (MacOGLClientSharedData *)this->_clientData;
	sharedData->FetchCustomDisplayToSrcClone(this->_fetchDisplayInfo, displayID, bufferIndex, needsLock);
}

void MacOGLClientFetchObject::Init()
{
	MacOGLClientSharedData *sharedData = (MacOGLClientSharedData *)this->_clientData;
	
	CGLContextObj prevContext = CGLGetCurrentContext();
	CGLSetCurrentContext(this->_context);
	sharedData->InitOGL();
	CGLSetCurrentContext(prevContext);
	
	this->MacGPUFetchObjectDisplayLink::Init();
}

void MacOGLClientFetchObject::SetFetchBuffers(const NDSDisplayInfo &currentDisplayInfo)
{
	MacOGLClientSharedData *sharedData = (MacOGLClientSharedData *)this->_clientData;
	
	CGLLockContext(this->_context);
	CGLSetCurrentContext(this->_context);
	this->GPUClientFetchObject::SetFetchBuffers(currentDisplayInfo);
	sharedData->SetFetchBuffersOGL(this->_fetchDisplayInfo, currentDisplayInfo);
	CGLUnlockContext(this->_context);
}

void MacOGLClientFetchObject::FetchFromBufferIndex(const u8 index)
{
	MacOGLClientSharedData *sharedData = (MacOGLClientSharedData *)this->_clientData;
	
	const bool willUseDirectCPU = (this->GetNumberViewsUsingDirectToCPUFiltering() > 0);
	sharedData->SetUseDirectToCPUFilterPipeline(willUseDirectCPU);
	
	semaphore_wait( this->SemaphoreFramebufferPageAtIndex(index) );
	this->SetFramebufferState(ClientDisplayBufferState_Reading, index);
	
	CGLLockContext(this->_context);
	CGLSetCurrentContext(this->_context);
	
	this->GPUClientFetchObject::FetchFromBufferIndex(index);
	glFlush();
	
	const NDSDisplayInfo &currentDisplayInfo = this->GetFetchDisplayInfoForBufferIndex(index);
	sharedData->FetchFromBufferIndexOGL(index, currentDisplayInfo);
	
	CGLUnlockContext(this->_context);
	
	this->SetFramebufferState(ClientDisplayBufferState_Idle, index);
	semaphore_signal( this->SemaphoreFramebufferPageAtIndex(index) );
}

void MacOGLClientFetchObject::_FetchNativeDisplayByID(const NDSDisplayID displayID, const u8 bufferIndex)
{
	// This method is called from MacOGLClientFetchObject::FetchFromBufferIndex(), and so
	// we should have already been assigned the current context.
	MacOGLClientSharedData *sharedData = (MacOGLClientSharedData *)this->_clientData;
	sharedData->FetchNativeDisplayByID_OGL(this->_fetchDisplayInfo, displayID, bufferIndex);
}

void MacOGLClientFetchObject::_FetchCustomDisplayByID(const NDSDisplayID displayID, const u8 bufferIndex)
{
	// This method is called from MacOGLClientFetchObject::FetchFromBufferIndex(), and so
	// we should have already been assigned the current context.
	MacOGLClientSharedData *sharedData = (MacOGLClientSharedData *)this->_clientData;
	sharedData->FetchCustomDisplayByID_OGL(this->_fetchDisplayInfo, displayID, bufferIndex);
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

MacOGLDisplayPresenter::MacOGLDisplayPresenter(MacOGLClientFetchObject *fetchObject)
{
	__InstanceInit(fetchObject);
}

void MacOGLDisplayPresenter::__InstanceInit(MacOGLClientFetchObject *fetchObject)
{
	const bool isMavericksSupported    = IsOSXVersionSupported(10, 9, 0);
	const bool isMountainLionSupported = (isMavericksSupported    || IsOSXVersionSupported(10, 8, 0));
	const bool isLionSupported         = (isMountainLionSupported || IsOSXVersionSupported(10, 7, 0));
	
	_nsPixelFormat = nil;
	
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
		NSOpenGLPFAAccelerated,
		NSOpenGLPFANoRecovery,
		(NSOpenGLPixelFormatAttribute)0, (NSOpenGLPixelFormatAttribute)0,
		(NSOpenGLPixelFormatAttribute)0
	};
	
	// Normally, supporting Automatic Graphics Switching and allowing the context
	// to use the integrated GPU requires NSOpenGLPFAAllowOfflineRenderers in the
	// pixel format attributes. However, GPUs that can't support Metal are also
	// too slow to run most of the video filters on OpenGL, and also show poor
	// performance when running multiple display windows with high GPU scaling
	// factors. Therefore, we will NOT allow OpenGL to run contexts on the
	// integrated GPU for performance reasons. -rogerman (2025/03/26)
	
#ifdef _OGLDISPLAYOUTPUT_3_2_H_
	// Request the latest context profile that is available on the system.
	if (isLionSupported)
	{
		attributes[11] = NSOpenGLPFAOpenGLProfile;
		if ( (_nsPixelFormat == nil) && isMavericksSupported )
		{
			attributes[12] = (NSOpenGLPixelFormatAttribute)NSOpenGLProfileVersion4_1Core;
			_nsPixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
		}
		
		if ( (_nsPixelFormat == nil) && isMountainLionSupported )
		{
			attributes[12] = (NSOpenGLPixelFormatAttribute)NSOpenGLProfileVersion3_2Core;
			_nsPixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
		}
		
		if ( (_nsPixelFormat == nil) && isLionSupported )
		{
			attributes[12] = (NSOpenGLPixelFormatAttribute)NSOpenGLProfileVersionLegacy;
			_nsPixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
		}
	}
#endif
	
	_nsPixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
	if (_nsPixelFormat == nil)
	{
		// Don't request any specific context version, which defaults to requesting a
		// legacy context.
		attributes[11] = (NSOpenGLPixelFormatAttribute)0;
		attributes[12] = (NSOpenGLPixelFormatAttribute)0;
		_nsPixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
	}
	
	if (_nsPixelFormat == nil)
	{
		// Something is seriously wrong if we haven't gotten a context yet.
		// Fall back to the software renderer.
		attributes[9] = (NSOpenGLPixelFormatAttribute)0;
		attributes[10] = (NSOpenGLPixelFormatAttribute)0;
		_nsPixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
	}
	
	_pixelFormat = (CGLPixelFormatObj)[_nsPixelFormat CGLPixelFormatObj];
	
	_nsContext = nil;
	_context = nil;
	_unfairlockProcessedInfo = apple_unfairlock_create();
	
	SetFetchObject(fetchObject);
}

MacOGLDisplayPresenter::~MacOGLDisplayPresenter()
{
	apple_unfairlock_destroy(this->_unfairlockProcessedInfo);
	this->_unfairlockProcessedInfo = NULL;
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
	
	if ( (profileVersion == kCGLOGLPVersion_3_2_Core) || (profileVersion == 0x4100) )
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
	apple_unfairlock_lock(this->_unfairlockProcessedInfo);
	const OGLProcessedFrameInfo &processedInfo = this->OGLVideoOutput::GetProcessedFrameInfo();
	apple_unfairlock_unlock(this->_unfairlockProcessedInfo);
	
	return processedInfo;
}

void MacOGLDisplayPresenter::SetProcessedFrameInfo(const OGLProcessedFrameInfo &processedInfo)
{
	apple_unfairlock_lock(this->_unfairlockProcessedInfo);
	this->OGLVideoOutput::SetProcessedFrameInfo(processedInfo);
	apple_unfairlock_unlock(this->_unfairlockProcessedInfo);
}

void MacOGLDisplayPresenter::WriteLockEmuFramebuffer(const uint8_t bufferIndex)
{
	MacOGLClientFetchObject &fetchObj = (MacOGLClientFetchObject &)this->GetFetchObject();
	semaphore_wait( fetchObj.SemaphoreFramebufferPageAtIndex(bufferIndex) );
}

void MacOGLDisplayPresenter::ReadLockEmuFramebuffer(const uint8_t bufferIndex)
{
	MacOGLClientFetchObject &fetchObj = (MacOGLClientFetchObject &)this->GetFetchObject();
	semaphore_wait( fetchObj.SemaphoreFramebufferPageAtIndex(bufferIndex) );
}

void MacOGLDisplayPresenter::UnlockEmuFramebuffer(const uint8_t bufferIndex)
{
	MacOGLClientFetchObject &fetchObj = (MacOGLClientFetchObject &)this->GetFetchObject();
	semaphore_signal( fetchObj.SemaphoreFramebufferPageAtIndex(bufferIndex) );
}

#pragma mark -

MacOGLDisplayView::MacOGLDisplayView()
{
	__InstanceInit(nil);
}

MacOGLDisplayView::MacOGLDisplayView(MacOGLClientFetchObject *fetchObject)
{
	__InstanceInit(fetchObject);
}

MacOGLDisplayView::~MacOGLDisplayView()
{
	[this->_caLayer release];
	
	apple_unfairlock_destroy(this->_unfairlockViewNeedsFlush);
	this->_unfairlockViewNeedsFlush = NULL;
}

void MacOGLDisplayView::__InstanceInit(MacOGLClientFetchObject *fetchObject)
{
	_allowViewUpdates = false;
	_unfairlockViewNeedsFlush = apple_unfairlock_create();
	
	MacOGLDisplayPresenter *newOpenGLPresenter = new MacOGLDisplayPresenter(fetchObject);
	_presenter = newOpenGLPresenter;
	
	_caLayer = [[DisplayViewOpenGLLayer alloc] init];
	[_caLayer setClientDisplayView:this];
}

bool MacOGLDisplayView::GetViewNeedsFlush()
{
	apple_unfairlock_lock(this->_unfairlockViewNeedsFlush);
	const bool viewNeedsFlush = this->_viewNeedsFlush;
	apple_unfairlock_unlock(this->_unfairlockViewNeedsFlush);
	
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
		
		apple_unfairlock_lock(this->_unfairlockViewNeedsFlush);
		this->_viewNeedsFlush = true;
		apple_unfairlock_unlock(this->_unfairlockViewNeedsFlush);
	}
}

void MacOGLDisplayView::SetAllowViewFlushes(bool allowFlushes)
{
	CGDirectDisplayID displayID = (CGDirectDisplayID)this->GetDisplayViewID();
	MacOGLClientFetchObject &fetchObj = (MacOGLClientFetchObject &)this->_presenter->GetFetchObject();
	fetchObj.DisplayLinkStartUsingID(displayID);
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
	apple_unfairlock_lock(this->_unfairlockViewNeedsFlush);
	this->_viewNeedsFlush = false;
	apple_unfairlock_unlock(this->_unfairlockViewNeedsFlush);
	
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
