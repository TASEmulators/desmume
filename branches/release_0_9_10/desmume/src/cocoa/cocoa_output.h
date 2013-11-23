/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2011-2013 DeSmuME team

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

#import <Foundation/Foundation.h>
#include <pthread.h>
#include <libkern/OSAtomic.h>

#import "cocoa_util.h"

@class NSImage;
@class NSBitmapImageRep;

typedef struct
{
	double	scale;
	double	rotation;		// Angle is in degrees
	double	translationX;
	double	translationY;
	double	translationZ;
} DisplayOutputTransformData;

typedef struct
{
	NSInteger displayModeID;
	size_t width;			// Measured in pixels
	size_t height;			// Measured in pixels
} DisplaySrcPixelAttributes;

@interface CocoaDSOutput : CocoaDSThread
{
	BOOL isStateChanged;
	NSUInteger frameCount;
	NSData *frameData;
	NSData *frameAttributesData;
	NSMutableDictionary *property;
	
	pthread_mutex_t *mutexProducer;
	pthread_mutex_t *mutexConsume;
}

@property (assign) BOOL isStateChanged;
@property (assign) NSUInteger frameCount;
@property (retain) NSData *frameData;
@property (retain) NSData *frameAttributesData;
@property (readonly) NSMutableDictionary *property;
@property (assign) pthread_mutex_t *mutexProducer;
@property (readonly) pthread_mutex_t *mutexConsume;

- (void) doCoreEmuFrame;
- (void) handleEmuFrameProcessed:(NSData *)mainData attributes:(NSData *)attributesData;

@end

@interface CocoaDSSpeaker : CocoaDSOutput
{
	NSUInteger bufferSize;
	
	OSSpinLock spinlockAudioOutputEngine;
	OSSpinLock spinlockVolume;
	OSSpinLock spinlockSpuAdvancedLogic;
	OSSpinLock spinlockSpuInterpolationMode;
	OSSpinLock spinlockSpuSyncMode;
	OSSpinLock spinlockSpuSyncMethod;
}

@property (assign) NSUInteger bufferSize;

- (id) init;
- (id) initWithVolume:(CGFloat)vol;
- (void) dealloc;

- (void) setVolume:(float)vol;
- (float) volume;
- (void) setAudioOutputEngine:(NSInteger)methodID;
- (NSInteger) audioOutputEngine;
- (void) setSpuAdvancedLogic:(BOOL)state;
- (BOOL) spuAdvancedLogic;
- (void) setSpuInterpolationMode:(NSInteger)modeID;
- (NSInteger) spuInterpolationMode;
- (void) setSpuSyncMode:(NSInteger)modeID;
- (NSInteger) spuSyncMode;
- (void) setSpuSyncMethod:(NSInteger)methodID;
- (NSInteger) spuSyncMethod;

- (BOOL) mute;
- (void) setMute:(BOOL)mute;
- (NSInteger) filter;
- (void) setFilter:(NSInteger)filter;
- (NSString *) audioOutputEngineString;
- (NSString *) spuInterpolationModeString;
- (NSString *) spuSyncMethodString;
- (void) handleSetVolume:(NSData *)volumeData;
- (void) handleSetAudioOutputEngine:(NSData *)methodIdData;
- (void) handleSetSpuAdvancedLogic:(NSData *)stateData;
- (void) handleSetSpuSyncMode:(NSData *)modeIdData;
- (void) handleSetSpuSyncMethod:(NSData *)methodIdData;
- (void) handleSetSpuInterpolationMode:(NSData *)modeIdData;

@end

@class CocoaVideoFilter;

@protocol CocoaDSDisplayDelegate <NSObject>

@required
- (void) doDisplayModeChanged:(NSInteger)displayModeID;

@property (assign) BOOL isHudEnabled;
@property (assign) BOOL isHudEditingModeEnabled;

@end

@protocol CocoaDSDisplayVideoDelegate <CocoaDSDisplayDelegate>

@required
- (void) doInitVideoOutput:(NSDictionary *)properties;
- (void) doProcessVideoFrame:(const void *)videoFrameData displayMode:(const NSInteger)displayModeID width:(const NSInteger)frameWidth height:(const NSInteger)frameHeight;

@optional
- (void) doResizeView:(NSRect)rect;
- (void) doTransformView:(const DisplayOutputTransformData *)transformData;
- (void) doRedraw;
- (void) doDisplayOrientationChanged:(NSInteger)displayOrientationID;
- (void) doDisplayOrderChanged:(NSInteger)displayOrderID;
- (void) doDisplayGapChanged:(float)displayGapScalar;
- (void) doBilinearOutputChanged:(BOOL)useBilinear;
- (void) doVerticalSyncChanged:(BOOL)useVerticalSync;
- (void) doVideoFilterChanged:(NSInteger)videoFilterTypeID frameSize:(NSSize)videoFilterDestSize;

@end


@interface CocoaDSDisplay : CocoaDSOutput
{
	id <CocoaDSDisplayDelegate> delegate;
	NSInteger displayMode;
	NSSize frameSize;
	
	OSSpinLock spinlockDelegate;
	OSSpinLock spinlockDisplayType;
}

@property (retain) id <CocoaDSDisplayDelegate> delegate;
@property (assign) NSInteger displayMode;
@property (readonly) NSSize frameSize;

- (void) handleChangeDisplayMode:(NSData *)displayModeData;
- (void) handleSetViewToBlack;
- (void) handleSetViewToWhite;
- (void) handleRequestScreenshot:(NSData *)fileURLStringData fileTypeData:(NSData *)fileTypeData;
- (void) handleCopyToPasteboard;

- (void) fillVideoFrameWithColor:(UInt16)colorValue;

- (NSImage *) image;
- (NSBitmapImageRep *) bitmapImageRep;

@end

@interface CocoaDSDisplayVideo : CocoaDSDisplay
{
	CocoaVideoFilter *vf;
	id <CocoaDSDisplayVideoDelegate> videoDelegate;
	NSInteger lastDisplayMode;
		
	OSSpinLock spinlockVideoFilterType;
	OSSpinLock spinlockVFBuffers;
}

@property (retain) id <CocoaDSDisplayVideoDelegate> delegate;
@property (assign) CocoaVideoFilter *vf;

- (void) handleResizeView:(NSData *)rectData;
- (void) handleTransformView:(NSData *)transformData;
- (void) handleRedrawView;
- (void) handleChangeDisplayOrientation:(NSData *)displayOrientationIdData;
- (void) handleChangeDisplayOrder:(NSData *)displayOrderIdData;
- (void) handleChangeDisplayGap:(NSData *)displayGapScalarData;
- (void) handleChangeBilinearOutput:(NSData *)bilinearStateData;
- (void) handleChangeVerticalSync:(NSData *)verticalSyncStateData;
- (void) handleChangeVideoFilter:(NSData *)videoFilterTypeIdData;

@end
