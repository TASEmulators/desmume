/*
	Copyright (C) 2026 DeSmuME team

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

/*
 * GPU video capture for the Metal front end.
 *
 * The whole point of this class is what it does NOT do: it never reads a frame
 * back to the CPU.  A dedicated presenter shares the display's Metal device,
 * command queue and fetch textures, renders the capture geometry through the
 * same shaders the on-screen view uses, and writes straight into an
 * IOSurface-backed CVPixelBuffer that VideoToolbox can encode in place.
 *
 * That is the difference between this and MacScreenshotCaptureTool, which
 * builds the same kind of presenter but then calls -renderToBuffer: and pays
 * for a per-frame texture allocation, a GPU->CPU blit, a blocking wait on the
 * GPU and a full-frame memcpy.  Those costs are irrelevant for a screenshot
 * and fatal for a 60 fps recording.
 */

#import "MacMetalVideoCapture.h"
#import "MacMetalDisplayView.h"

#include <stdio.h>
#include <string.h>

// The pool bounds how many frames may exist at once.  When VideoToolbox falls
// behind we would rather drop a frame than grow without limit or block the
// display thread, so the pool doubles as the backpressure valve.
#define CAPTURE_POOL_MIN_BUFFERS		6
#define CAPTURE_POOL_ALLOCATION_LIMIT	12

// How many capture frames may be in flight on the GPU.  One is the default for
// a presented drawable; a capture presenter renders back-to-back and needs a
// couple more so it does not serialise on the previous frame's completion.
#define CAPTURE_FRAMES_IN_FLIGHT			3

// The HQnx pixel scalers need a lookup table, and that table lives on the
// shared Metal data where the live view also keeps its own.  Mirror the
// selection -setPixelScaler: makes so we can bind ours only while encoding our
// own filter pass.
static id<MTLTexture> MacMetalCaptureLUTForFilter(MetalDisplayViewSharedData *sharedData, const VideoFilterTypeID filterID)
{
	switch (filterID)
	{
		case VideoFilterTypeID_LQ2X:
		case VideoFilterTypeID_LQ2XS:
			return [sharedData texLQ2xLUT];
			
		case VideoFilterTypeID_HQ2X:
		case VideoFilterTypeID_HQ2XS:
			return [sharedData texHQ2xLUT];
			
		case VideoFilterTypeID_HQ3X:
		case VideoFilterTypeID_HQ3XS:
			return [sharedData texHQ3xLUT];
			
		case VideoFilterTypeID_HQ4X:
		case VideoFilterTypeID_HQ4XS:
			return [sharedData texHQ4xLUT];
			
		default:
			break;
	}
	
	return nil;
}

#pragma mark -

@interface MacMetalVideoCapture : NSObject
{
	MacMetalDisplayPresenter *_presenter;
	MacMetalDisplayPresenterObject *_presenterObject;
	
	CVPixelBufferPoolRef _pool;
	CVMetalTextureCacheRef _cache;
	
	id<MTLTexture> _ownHQnxLUT;
	id<MTLTexture> _displayHQnxLUT;
	
	size_t _width;
	size_t _height;
	bool _isValid;
	
	unsigned int _renderedFrameCount;
	unsigned int _droppedFrameCount;
	bool _didLogFirstFrame;
}

- (id) initWithSharedData:(MetalDisplayViewSharedData *)sharedData geometry:(const MacVideoCaptureGeometry &)geometry;
- (bool) isValid;
- (void *) renderFrame;

@end

@implementation MacMetalVideoCapture

- (id) initWithSharedData:(MetalDisplayViewSharedData *)sharedData geometry:(const MacVideoCaptureGeometry &)geometry
{
	self = [super init];
	if (self == nil)
	{
		return nil;
	}
	
	_isValid = false;
	_pool = NULL;
	_cache = NULL;
	_presenter = NULL;
	_presenterObject = nil;
	_ownHQnxLUT = nil;
	_displayHQnxLUT = nil;
	_renderedFrameCount = 0;
	_droppedFrameCount = 0;
	_didLogFirstFrame = false;
	
	_width  = geometry.width;
	_height = geometry.height;
	
	if ( (sharedData == nil) || ([sharedData device] == nil) || (_width == 0) || (_height == 0) )
	{
		[self release];
		return nil;
	}
	
	// The HQnx LUT is shared state.  Snapshot what the live view is using now,
	// because configuring our own pixel scaler is about to overwrite it.
	_displayHQnxLUT = [[sharedData texCurrentHQnxLUT] retain];
	
	// The upstream refactor of ClientDisplayPresenterProperties removed the raw
	// clientWidth/clientHeight, viewScale, normalWidth/normalHeight and
	// gapDistance fields.  Every derived value is now computed from the display
	// mode plus the actual viewport size, so fill the struct through the same
	// helper the screenshot and display-window paths use.  The capture viewport
	// is exactly the CVPixelBuffer size we are about to allocate.
	ClientDisplayPresenterProperties props = ClientDisplayPresenter::GetDefaultProperties();
	props.mode     = geometry.mode;
	props.layout   = geometry.layout;
	props.order    = geometry.order;
	props.gapScale = geometry.gapScale;
	props.rotation = geometry.rotation;
	ClientDisplayPresenter::CalculateViewPropertiesWithActualViewportSizeZoomed(props, (double)_width, (double)_height, 1.0);
	
	_presenter = new MacMetalDisplayPresenter(sharedData, props);
	_presenterObject = [_presenter->GetPresenterObject() retain];
	
	// Render straight into BGRA so VideoToolbox can take the frame as-is.  This
	// must happen before -setup, which builds the output pipeline against the
	// drawable format, and before -setOutputFilter:, which rebuilds it.
	[_presenterObject setDrawableFormat:MTLPixelFormatBGRA8Unorm];
	
	_presenter->Init();
	_presenter->CommitPresenterProperties(props);
	_presenter->SetupPresenterProperties();
	
	_presenter->SetFiltersPreferGPU(true);
	_presenter->SetHUDVisibility(false);
	_presenter->SetSourceDeposterize(geometry.useDeposterize);
	_presenter->SetOutputFilter(geometry.outputFilterID);
	_presenter->SetPixelScaler(geometry.pixelScalerID);
	
	// -setPixelScaler: has just replaced the shared LUT with ours; put the
	// display's back immediately.
	[sharedData setTexCurrentHQnxLUT:_displayHQnxLUT];
	_ownHQnxLUT = [MacMetalCaptureLUTForFilter(sharedData, geometry.pixelScalerID) retain];
	
	// SetupPresenterProperties() only flags what actually changed, and a capture
	// presenter must always end up with its vertices, properties buffer and
	// viewport built.
	[_presenterObject setNeedsViewportUpdate:YES];
	[_presenterObject setNeedsRotationScaleUpdate:YES];
	[_presenterObject setNeedsScreenVerticesUpdate:YES];
	_presenter->UpdateLayout();
	
	[_presenterObject setRenderBufferingCapacity:CAPTURE_FRAMES_IN_FLIGHT];
	
	NSDictionary *poolAttributes = [NSDictionary dictionaryWithObjectsAndKeys:
									[NSNumber numberWithInt:CAPTURE_POOL_MIN_BUFFERS],      (id)kCVPixelBufferPoolMinimumBufferCountKey,
									[NSNumber numberWithInt:CAPTURE_POOL_ALLOCATION_LIMIT], (id)kCVPixelBufferPoolAllocationThresholdKey,
									nil];
	
	NSDictionary *bufferAttributes = [NSDictionary dictionaryWithObjectsAndKeys:
									  [NSNumber numberWithUnsignedInt:(unsigned int)kCVPixelFormatType_32BGRA], (id)kCVPixelBufferPixelFormatTypeKey,
									  [NSNumber numberWithUnsignedLong:(unsigned long)_width],  (id)kCVPixelBufferWidthKey,
									  [NSNumber numberWithUnsignedLong:(unsigned long)_height], (id)kCVPixelBufferHeightKey,
									  [NSDictionary dictionary],                               (id)kCVPixelBufferIOSurfacePropertiesKey,
									  [NSNumber numberWithBool:YES],                           (id)kCVPixelBufferMetalCompatibilityKey,
									  nil];
	
	if (CVPixelBufferPoolCreate(kCFAllocatorDefault, (CFDictionaryRef)poolAttributes, (CFDictionaryRef)bufferAttributes, &_pool) != kCVReturnSuccess)
	{
		printf("DeSmuME[AVCapture]: Could not create the capture pixel buffer pool.\n");
		fflush(stdout);
		
		[self release];
		return nil;
	}
	
	if (CVMetalTextureCacheCreate(kCFAllocatorDefault, NULL, [sharedData device], NULL, &_cache) != kCVReturnSuccess)
	{
		printf("DeSmuME[AVCapture]: Could not create the Metal texture cache.\n");
		fflush(stdout);
		
		[self release];
		return nil;
	}
	
	_isValid = true;
	
	printf("DeSmuME[AVCapture]: GPU capture ready: %zux%zu mode=%d layout=%d order=%d gapScale=%.3f rotation=%.1f deposterize=%d outputFilter=%d pixelScaler=%s\n",
		   _width, _height, (int)geometry.mode, (int)geometry.layout, (int)geometry.order,
		   geometry.gapScale, geometry.rotation, (int)geometry.useDeposterize,
		   (int)geometry.outputFilterID, VideoFilter::GetTypeStringByID(geometry.pixelScalerID));
	fflush(stdout);
	
	return self;
}

- (void) dealloc
{
	// Capture command buffers are created with unretained references, so the
	// presenter's textures must outlive the GPU work that reads them.  The
	// command queue is serial, so committing one extra empty buffer and
	// waiting for it drains every capture frame that is still in flight.
	if (_presenterObject != nil)
	{
		id<MTLCommandQueue> commandQueue = [[_presenterObject sharedData] commandQueue];
		if (commandQueue != nil)
		{
			id<MTLCommandBuffer> drainBuffer = [commandQueue commandBuffer];
			[drainBuffer commit];
			[drainBuffer waitUntilCompleted];
		}
	}
	
	if (_cache != NULL)
	{
		CVMetalTextureCacheFlush(_cache, 0);
		CFRelease(_cache);
		_cache = NULL;
	}
	
	if (_pool != NULL)
	{
		CVPixelBufferPoolFlush(_pool, kCVPixelBufferPoolFlushExcessBuffers);
		CFRelease(_pool);
		_pool = NULL;
	}
	
	// Deleting the presenter drops one reference to its Objective-C companion;
	// the extra reference taken in -init is dropped right after.
	delete _presenter;
	_presenter = NULL;
	
	[_presenterObject release];
	_presenterObject = nil;
	
	[_ownHQnxLUT release];
	[_displayHQnxLUT release];
	
	printf("DeSmuME[AVCapture]: GPU capture released: %u frames rendered, %u dropped.\n",
		   _renderedFrameCount, _droppedFrameCount);
	fflush(stdout);
	
	[super dealloc];
}

- (bool) isValid
{
	return _isValid;
}

- (void *) renderFrame
{
	if ( !_isValid || (_pool == NULL) || (_cache == NULL) || (_presenter == NULL) )
	{
		return NULL;
	}
	
	CVPixelBufferRef pixelBuffer = NULL;
	if (CVPixelBufferPoolCreatePixelBuffer(kCFAllocatorDefault, _pool, &pixelBuffer) != kCVReturnSuccess)
	{
		_droppedFrameCount++;
		return NULL;
	}
	
	if (pixelBuffer == NULL)
	{
		_droppedFrameCount++;
		return NULL;
	}
	
	// Same order the display uses: pick up this frame's fetch results, run the
	// filter chain into our own textures, then composite.
	_presenter->LoadDisplays();
	
	// The HQnx lookup table is shared with the live view.  Bind ours only for
	// the duration of our own filter encode, then put back whatever the display
	// had at that moment -- not a snapshot taken at setup time, because the
	// user can change the on-screen scaler while a recording is running.
	// (Textures are bound at encode time, so a same-thread swap is enough.)
	if (_ownHQnxLUT != nil)
	{
		id<MTLTexture> previousLUT = [[_presenterObject sharedData] texCurrentHQnxLUT];
		[[_presenterObject sharedData] setTexCurrentHQnxLUT:_ownHQnxLUT];
		_presenter->ProcessDisplays();
		[[_presenterObject sharedData] setTexCurrentHQnxLUT:previousLUT];
	}
	else
	{
		_presenter->ProcessDisplays();
	}
	
	if (![_presenterObject renderToPixelBuffer:pixelBuffer textureCache:_cache])
	{
		CFRelease(pixelBuffer);
		_droppedFrameCount++;
		return NULL;
	}
	
	_renderedFrameCount++;
	
	if (!_didLogFirstFrame)
	{
		_didLogFirstFrame = true;
		
		printf("DeSmuME[AVCapture]: GPU capture rendered its first frame (%zux%zu, viewScale=%.3f).\n",
			   _width, _height, _presenter->GetViewScale());
		fflush(stdout);
	}
	
	return (void *)pixelBuffer;
}

@end

#pragma mark -

void *MacMetalPresenterGetSharedData(void *presenter)
{
	if (presenter == NULL)
	{
		return NULL;
	}
	
	MacMetalDisplayPresenter *metalPresenter = dynamic_cast<MacMetalDisplayPresenter *>((ClientDisplay3DPresenter *)presenter);
	if (metalPresenter == NULL)
	{
		return NULL;
	}
	
	return (void *)metalPresenter->GetSharedData();
}

void *MacMetalVideoCaptureCreate(void *metalSharedData, const MacVideoCaptureGeometry &geometry)
{
	@autoreleasepool
	{
		MacMetalVideoCapture *capture = [[MacMetalVideoCapture alloc] initWithSharedData:(MetalDisplayViewSharedData *)metalSharedData
																			   geometry:geometry];
		return (void *)capture;
	}
}

void MacMetalVideoCaptureDestroy(void *capture)
{
	if (capture == NULL)
	{
		return;
	}
	
	@autoreleasepool
	{
		[(MacMetalVideoCapture *)capture release];
	}
}

bool MacMetalVideoCaptureIsValid(void *capture)
{
	if (capture == NULL)
	{
		return false;
	}
	
	return [(MacMetalVideoCapture *)capture isValid];
}

void *MacMetalVideoCaptureRenderFrame(void *capture)
{
	if (capture == NULL)
	{
		return NULL;
	}
	
	@autoreleasepool
	{
		return [(MacMetalVideoCapture *)capture renderFrame];
	}
}

void MacMetalVideoCaptureReleasePixelBuffer(void *pixelBuffer)
{
	if (pixelBuffer == NULL)
	{
		return;
	}
	
	CFRelease((CVPixelBufferRef)pixelBuffer);
}
