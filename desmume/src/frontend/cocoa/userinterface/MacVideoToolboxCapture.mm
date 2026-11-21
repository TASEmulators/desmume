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

#import <Cocoa/Cocoa.h>
#import <Accelerate/Accelerate.h>
#import <AudioToolbox/AudioToolbox.h>
#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#import <VideoToolbox/VideoToolbox.h>

#include <unistd.h>
#include <string>

#import "MacVideoToolboxCapture.h"

// Write to both stdout (Xcode console) and the unified log (Console.app), so a
// recording failure can always be diagnosed after the fact.
#define VTLOG(FMT, ...) do { \
	printf("DeSmuME[AVCapture]: " FMT "\n", ##__VA_ARGS__); \
	NSLog(@"DeSmuME[AVCapture]: " FMT, ##__VA_ARGS__); \
} while (0)

// DeSmuME's NDS LCD refresh rate, expressed exactly.  Keeping the timestamps
// tied to a frame counter (rather than a wall clock) is what keeps audio and
// video locked together when the emulator stalls or drops a frame.
static const int64_t kVTFrameDurationValue = 560190LL;
static const int32_t kVTFrameTimeScale = 33513982;

static const uint32_t kVTDefaultVideoBitrate = 8000000;
static const uint32_t kVTDefaultAudioBitrate = 192000;

bool MacCodecIsH264OrHEVC(uint32_t codecType)
{
	return (codecType == kCMVideoCodecType_H264) || (codecType == kCMVideoCodecType_HEVC);
}


#pragma mark - Encoder enumeration

@implementation MacVideoEncoderChoice

+ (NSString *) labelForHardware:(bool)isHardware
{
	return isHardware ? @"Hardware" : @"Software";
}

+ (NSArray *) availableVideoEncoders
{
	NSMutableArray *result = [NSMutableArray array];
	CFArrayRef encoderList = NULL;
	const OSStatus listStatus = VTCopyVideoEncoderList(NULL, &encoderList);

	if ( (listStatus == noErr) && (encoderList != NULL) )
	{
		const CFIndex count = CFArrayGetCount(encoderList);

		for (CFIndex i = 0; i < count; i++)
		{
			CFDictionaryRef info = (CFDictionaryRef)CFArrayGetValueAtIndex(encoderList, i);
			if (info == NULL)
			{
				continue;
			}

			CFNumberRef codecNumber = (CFNumberRef)CFDictionaryGetValue(info, kVTVideoEncoderList_CodecType);
			CFStringRef codecNameRef = (CFStringRef)CFDictionaryGetValue(info, kVTVideoEncoderList_CodecName);
			CFStringRef encoderNameRef = (CFStringRef)CFDictionaryGetValue(info, kVTVideoEncoderList_EncoderName);
			CFStringRef displayNameRef = (CFStringRef)CFDictionaryGetValue(info, kVTVideoEncoderList_DisplayName);
			CFStringRef encoderIDRef = (CFStringRef)CFDictionaryGetValue(info, kVTVideoEncoderList_EncoderID);

			uint32_t codecType = 0;
			if (codecNumber != NULL)
			{
				CFNumberGetValue(codecNumber, kCFNumberSInt32Type, &codecType);
			}

			if ( (codecType == 0) || (encoderIDRef == NULL) )
			{
				continue;
			}

			// kVTVideoEncoderList_IsHardwareAccelerated is only declared in the
			// 10.14+ SDK.  Looking the key up by its literal value works on
			// every OS version and avoids a weak-link/availability problem.
			CFBooleanRef hardwareRef = (CFBooleanRef)CFDictionaryGetValue(info, CFSTR("IsHardwareAccelerated"));
			const bool isHardware = (hardwareRef != NULL) && CFBooleanGetValue(hardwareRef);

			MacVideoEncoderChoice *choice = [[[MacVideoEncoderChoice alloc] init] autorelease];
			choice->codecType = codecType;
			choice->codecName = (codecNameRef != NULL) ? [(NSString *)codecNameRef copy] : [@"Unknown" copy];
			choice->encoderName = (encoderNameRef != NULL) ? [(NSString *)encoderNameRef copy] : [choice->codecName copy];
			choice->displayName = (displayNameRef != NULL) ? [(NSString *)displayNameRef copy] : [choice->encoderName copy];
			choice->encoderID = [(NSString *)encoderIDRef copy];
			choice->isHardware = isHardware;
			choice->isAudioOnly = NO;

			[result addObject:choice];
		}

		CFRelease(encoderList);
	}
	else
	{
		VTLOG("VTCopyVideoEncoderList failed with status %d.", (int)listStatus);
	}

	// Hardware encoders first, then ordered by codec and encoder name.  This
	// mirrors the Windows recorder, which puts the hardware row for each codec
	// at the top of its encoder list.
	[result sortUsingComparator:^NSComparisonResult(MacVideoEncoderChoice *a, MacVideoEncoderChoice *b) {
		if (a->isHardware != b->isHardware)
		{
			return a->isHardware ? NSOrderedAscending : NSOrderedDescending;
		}

		NSComparisonResult order = [a->codecName compare:b->codecName];
		if (order != NSOrderedSame)
		{
			return order;
		}

		return [a->encoderName compare:b->encoderName];
	}];

	// The Windows recorder ends its list with a "None" row so that the user can
	// record audio only.  Keep that behaviour here too.
	MacVideoEncoderChoice *noneChoice = [[[MacVideoEncoderChoice alloc] init] autorelease];
	noneChoice->codecType = 0;
	noneChoice->codecName = [@"None" copy];
	noneChoice->encoderName = [@"Audio only" copy];
	noneChoice->displayName = [@"None (audio only, AAC)" copy];
	noneChoice->encoderID = [@"" copy];
	noneChoice->isHardware = NO;
	noneChoice->isAudioOnly = YES;
	[result addObject:noneChoice];

	return result;
}

- (MacVideoEncoderSpec) encoderSpec
{
	MacVideoEncoderSpec spec;
	spec.codecType = codecType;
	spec.encoderID = (encoderID != nil) ? [encoderID UTF8String] : "";
	spec.displayName = (displayName != nil) ? [displayName UTF8String] : "";
	spec.isHardware = isHardware;
	spec.isAudioOnly = isAudioOnly;
	spec.container = MacCaptureContainer_Auto;	// resolved by the caller / Open()
	spec.bitrate = kVTDefaultVideoBitrate;
	return spec;
}

- (NSString *) description
{
	if (isAudioOnly)
	{
		return displayName;
	}

	// A per-frame JPEG stream IS Motion JPEG: QuickTime writes a 'jpeg' sample
	// entry, MP4 writes 'mjpg'.  Label it so users know what they are getting.
	if (codecType == kCMVideoCodecType_JPEG)
	{
		return [NSString stringWithFormat:@"%@ - Motion JPEG / MJPEG (%@)", codecName,
										  [MacVideoEncoderChoice labelForHardware:isHardware]];
	}

	return [NSString stringWithFormat:@"%@ - %@ (%@)", codecName, encoderName,
									  [MacVideoEncoderChoice labelForHardware:isHardware]];
}

- (void) dealloc
{
	[codecName release];
	[encoderName release];
	[displayName release];
	[encoderID release];
	[super dealloc];
}

@end


#pragma mark - Private writer object

@interface MacVTFinishFlag : NSObject
{
	NSCondition *_condition;
	BOOL _done;
}
- (void) signalDone;
- (void) waitUntilDone;
@end

@implementation MacVTFinishFlag

- (id) init
{
	self = [super init];
	if (self != nil)
	{
		_condition = [[NSCondition alloc] init];
		_done = NO;
	}
	return self;
}

- (void) signalDone
{
	[_condition lock];
	_done = YES;
	[_condition signal];
	[_condition unlock];
}

- (void) waitUntilDone
{
	[_condition lock];
	while (!_done)
	{
		[_condition waitUntilDate:[NSDate dateWithTimeIntervalSinceNow:30.0]];
		if (!_done)
		{
			// Bail out rather than hanging the close thread forever if the
			// writer never calls back.
			break;
		}
	}
	[_condition unlock];
}

- (void) dealloc
{
	[_condition release];
	[super dealloc];
}

@end


@interface MacVTWriter : NSObject
{
	AVAssetWriter *_assetWriter;
	AVAssetWriterInput *_videoInput;
	AVAssetWriterInput *_audioInput;

	VTCompressionSessionRef _compressionSession;
	NSDictionary *_pixelBufferAttributes;
	CMAudioFormatDescriptionRef _audioFormatDesc;

	size_t _encodeWidth;
	size_t _encodeHeight;
	MacCaptureContainer _container;
	NSString *_fileName;

	NSLock *_lock;
	BOOL _valid;
	BOOL _failed;
	BOOL _didStartWriting;
	BOOL _didFinish;
}

- (id) initWithSpec:(const MacVideoEncoderSpec &)spec
		  fileName:(NSString *)fileName
			  width:(size_t)width
			 height:(size_t)height
			  error:(ClientAVCaptureError *)outError;

// Container selection is a runtime probe, not a codec allow-list: MP4 is
// tried first (so AV1 on newer machines is not blocked), and QuickTime Movie
// is used only when AVFoundation actually rejects the combination.
+ (NSDictionary *) audioSettingsForCodec:(MacAudioCodec)codec;
+ (BOOL) _containerAcceptsSpec:(const MacVideoEncoderSpec &)spec
					 container:(MacCaptureContainer)container
						 width:(size_t)width
						height:(size_t)height;
+ (MacCaptureContainer) resolveContainerForSpec:(const MacVideoEncoderSpec &)spec
										  width:(size_t)width
										 height:(size_t)height;

- (BOOL) isValid;
- (BOOL) __waitForInput:(AVAssetWriterInput *)input budgetMS:(unsigned int)budgetMS;
- (void) appendPixelBuffer:(CVPixelBufferRef)pixelBuffer frameNumber:(int64_t)frameNumber;
- (void) appendAudio:(const uint8_t *)pcm size:(size_t)size sampleOffset:(int64_t)sampleOffset;
- (void) appendEncodedSample:(CMSampleBufferRef)sampleBuffer;
- (void) markFailed;
- (void) finish;

@end

static void MacVTEncodeOutputCallback(void *outputCallbackRefCon,
									 void *sourceFrameRefCon,
									 OSStatus status,
									 VTEncodeInfoFlags infoFlags,
									 CMSampleBufferRef sampleBuffer)
{
	MacVTWriter *writer = (MacVTWriter *)outputCallbackRefCon;

	if ( (status != noErr) || (sampleBuffer == NULL) || (infoFlags & kVTEncodeInfo_FrameDropped) )
	{
		VTLOG("VideoToolbox encode failed with status %d.", (int)status);
		[writer markFailed];
		return;
	}

	[writer appendEncodedSample:sampleBuffer];
}

static CMSampleBufferRef MacVTCreateAudioSampleBuffer(const uint8_t *srcData,
													 size_t srcSize,
													 int64_t sampleOffset,
													 CMAudioFormatDescriptionRef formatDesc)
{
	const size_t blockSize = sizeof(int16_t) * 2;
	const size_t frameCount = srcSize / blockSize;
	if ( (frameCount == 0) || (formatDesc == NULL) )
	{
		return NULL;
	}

	CMBlockBufferRef blockBuffer = NULL;
	// Allocate the block buffer's own storage: srcData points into the capture
	// object's ring buffer, which is reused as soon as this call returns.
	CMBlockBufferCreateWithMemoryBlock(kCFAllocatorDefault, NULL, srcSize,
									   kCFAllocatorDefault, NULL, 0, srcSize,
									   kCMBlockBufferAssureMemoryNowFlag, &blockBuffer);
	if (blockBuffer == NULL)
	{
		return NULL;
	}

	if (CMBlockBufferReplaceDataBytes(srcData, blockBuffer, 0, srcSize) != kCMBlockBufferNoErr)
	{
		CFRelease(blockBuffer);
		return NULL;
	}

	CMSampleTimingInfo timing;
	timing.duration = CMTimeMake(1, DESMUME_SAMPLE_RATE);
	timing.presentationTimeStamp = CMTimeMake(sampleOffset, DESMUME_SAMPLE_RATE);
	timing.decodeTimeStamp = kCMTimeInvalid;

	CMSampleBufferRef sampleBuffer = NULL;
	const OSStatus status = CMSampleBufferCreate(kCFAllocatorDefault, blockBuffer, true,
												NULL, NULL, formatDesc,
												frameCount, 1, &timing,
												0, NULL, &sampleBuffer);
	CFRelease(blockBuffer);

	if (status != noErr)
	{
		return NULL;
	}

	return sampleBuffer;
}

@implementation MacVTWriter

#pragma mark - Runtime container probing

+ (NSDictionary *) audioSettingsForCodec:(MacAudioCodec)codec
{
	if (codec == MacAudioCodec_ALAC)
	{
		// ALAC is Apple's compressed lossless format.  AVEncoderBitDepthHintKey
		// is mandatory: omitting it makes initWithMediaType:outputSettings:
		// throw NSInvalidArgumentException.
		return [NSDictionary dictionaryWithObjectsAndKeys:
			[NSNumber numberWithUnsignedInt:kAudioFormatAppleLossless], AVFormatIDKey,
			[NSNumber numberWithDouble:(double)DESMUME_SAMPLE_RATE], AVSampleRateKey,
			[NSNumber numberWithUnsignedInt:2], AVNumberOfChannelsKey,
			[NSNumber numberWithUnsignedInt:16], AVEncoderBitDepthHintKey,
			nil];
	}

	if (codec == MacAudioCodec_LPCM)
	{
		// Raw, uncompressed 16-bit stereo PCM: bit-exact and lossless.
		return [NSDictionary dictionaryWithObjectsAndKeys:
			[NSNumber numberWithUnsignedInt:kAudioFormatLinearPCM], AVFormatIDKey,
			[NSNumber numberWithDouble:(double)DESMUME_SAMPLE_RATE], AVSampleRateKey,
			[NSNumber numberWithUnsignedInt:2], AVNumberOfChannelsKey,
			[NSNumber numberWithUnsignedInt:16], AVLinearPCMBitDepthKey,
			[NSNumber numberWithBool:NO], AVLinearPCMIsBigEndianKey,
			[NSNumber numberWithBool:NO], AVLinearPCMIsFloatKey,
			[NSNumber numberWithBool:NO], AVLinearPCMIsNonInterleavedKey,
			nil];
	}

	return [NSDictionary dictionaryWithObjectsAndKeys:
		[NSNumber numberWithUnsignedInt:kAudioFormatMPEG4AAC], AVFormatIDKey,
		[NSNumber numberWithDouble:(double)DESMUME_SAMPLE_RATE], AVSampleRateKey,
		[NSNumber numberWithUnsignedInt:2], AVNumberOfChannelsKey,
		[NSNumber numberWithUnsignedInt:kVTDefaultAudioBitrate], AVEncoderBitRateKey,
		nil];
}

+ (BOOL) _containerAcceptsSpec:(const MacVideoEncoderSpec &)spec
					 container:(MacCaptureContainer)container
						 width:(size_t)width
						height:(size_t)height
{
	AVFileType fileType = (container == MacCaptureContainer_MP4)
						  ? AVFileTypeMPEG4 : AVFileTypeQuickTimeMovie;

	// A throw-away writer is enough: canAddInput: is where AVFoundation
	// decides whether the file type can carry the given codec.
	NSString *probePath = [NSString stringWithFormat:@"%@desmume-vt-probe-%d.%@",
						   NSTemporaryDirectory(), (int)getpid(),
						   (container == MacCaptureContainer_MP4) ? @"mp4" : @"mov"];
	[[NSFileManager defaultManager] removeItemAtPath:probePath error:nil];

	NSError *nsError = nil;
	AVAssetWriter *probeWriter = [[AVAssetWriter alloc]
								  initWithURL:[NSURL fileURLWithPath:probePath]
									 fileType:fileType error:&nsError];
	if ((probeWriter == nil) || (nsError != nil))
	{
		[probeWriter release];
		[[NSFileManager defaultManager] removeItemAtPath:probePath error:nil];
		return NO;
	}

	BOOL accepted = NO;
	@try
	{
		AVAssetWriterInput *audioInput = [[[AVAssetWriterInput alloc]
			initWithMediaType:AVMediaTypeAudio
			   outputSettings:[self audioSettingsForCodec:spec.audioCodec]] autorelease];
		const BOOL audioOK = [probeWriter canAddInput:audioInput];
		if (audioOK)
		{
			[probeWriter addInput:audioInput];
		}

		BOOL videoOK = YES;
		if (!spec.isAudioOnly)
		{
			CMFormatDescriptionRef hint = NULL;
			const OSStatus hintStatus = CMVideoFormatDescriptionCreate(kCFAllocatorDefault,
											(CMVideoCodecType)spec.codecType,
											(int32_t)width, (int32_t)height, NULL, &hint);
			AVAssetWriterInput *videoInput = nil;
			if ((hintStatus == noErr) && (hint != NULL))
			{
				videoInput = [[[AVAssetWriterInput alloc] initWithMediaType:AVMediaTypeVideo
															 outputSettings:nil
														   sourceFormatHint:hint] autorelease];
				CFRelease(hint);
			}
			else
			{
				videoInput = [[[AVAssetWriterInput alloc] initWithMediaType:AVMediaTypeVideo
															 outputSettings:nil] autorelease];
			}
			videoOK = [probeWriter canAddInput:videoInput];
		}

		accepted = audioOK && videoOK;
	}
	@catch (NSException *exception)
	{
		VTLOG("Container probe threw %s: %s",
			  [[exception name] UTF8String], [[exception reason] UTF8String]);
		accepted = NO;
	}

	[probeWriter release];
	[[NSFileManager defaultManager] removeItemAtPath:probePath error:nil];

	VTLOG("Container probe: %s -> %s (codec=0x%08x, audio=%d)",
		  [fileType UTF8String], accepted ? "ACCEPTED" : "rejected",
		  (unsigned)spec.codecType, (int)spec.audioCodec);
	return accepted;
}

+ (MacCaptureContainer) resolveContainerForSpec:(const MacVideoEncoderSpec &)spec
										  width:(size_t)width
										 height:(size_t)height
{
	// Uncompressed PCM is not a standard MP4/M4A audio track.
	if (spec.audioCodec == MacAudioCodec_LPCM)
	{
		return MacCaptureContainer_MOV;
	}

	// Audio-only files use the M4A (MP4 family) container, mapped later.
	if (spec.isAudioOnly)
	{
		return MacCaptureContainer_MP4;
	}

	MacCaptureContainer order[2];
	size_t count = 0;
	if (spec.container == MacCaptureContainer_MOV)
	{
		// Explicit user override: only QuickTime Movie.
		order[count++] = MacCaptureContainer_MOV;
	}
	else
	{
		// Auto and an explicit MP4 request both try MP4 first.  There is no
		// codec allow-list, so AV1 (and anything else AVFoundation learns to
		// mux into MP4) automatically works on machines that support it.
		order[count++] = MacCaptureContainer_MP4;
		order[count++] = MacCaptureContainer_MOV;
	}

	for (size_t i = 0; i < count; i++)
	{
		if ([self _containerAcceptsSpec:spec container:order[i] width:width height:height])
		{
			return order[i];
		}
		if (order[i] == MacCaptureContainer_MP4)
		{
			VTLOG("MP4 does not accept this encoder/audio combination; falling back to QuickTime Movie.");
		}
	}

	return MacCaptureContainer_MOV;
}

- (id) initWithSpec:(const MacVideoEncoderSpec &)spec
		  fileName:(NSString *)fileName
			  width:(size_t)width
			 height:(size_t)height
			  error:(ClientAVCaptureError *)outError
{
	self = [super init];
	if (self == nil)
	{
		if (outError != NULL) *outError = ClientAVCaptureError_GenericError;
		return nil;
	}

	_lock = [[NSLock alloc] init];
	_valid = NO;
	_failed = NO;
	_didStartWriting = NO;
	_didFinish = NO;
	_fileName = [fileName copy];

	// VideoToolboxFileStream::Open() already ran the runtime container probe
	// and stored the winner in spec.container.  This is only a defensive
	// fallback for a stream constructed without going through Open().
	_container = (spec.container == MacCaptureContainer_Auto)
				 ? MacCaptureContainer_MOV : spec.container;
	if (spec.audioCodec == MacAudioCodec_LPCM)
	{
		_container = MacCaptureContainer_MOV;
	}

	// H.264/HEVC both carry 4:2:0, which cannot describe odd dimensions.  The
	// Windows recorder forces even sizes for exactly the same reason.
	_encodeWidth = width & ~(size_t)1;
	_encodeHeight = height & ~(size_t)1;

	NSFileManager *fileManager = [NSFileManager defaultManager];
	[fileManager removeItemAtPath:fileName error:nil];

	NSURL *fileURL = [NSURL fileURLWithPath:fileName];
	NSError *nsError = nil;
	AVFileType fileType = AVFileTypeQuickTimeMovie;
	if (spec.isAudioOnly && (spec.audioCodec != MacAudioCodec_LPCM))
	{
		fileType = AVFileTypeAppleM4A;
	}
	else if (_container == MacCaptureContainer_MP4)
	{
		fileType = AVFileTypeMPEG4;
	}

	const char *audioName = (spec.audioCodec == MacAudioCodec_ALAC) ? "ALAC (lossless)" :
						   ((spec.audioCodec == MacAudioCodec_LPCM) ? "LPCM (uncompressed)" : "AAC");
	VTLOG("Opening %s (fileType=%s, video=%s, audio=%s, %lux%lu)",
		  [fileName UTF8String], [fileType UTF8String], spec.encoderID.c_str(), audioName,
		  (unsigned long)_encodeWidth, (unsigned long)_encodeHeight);

	_assetWriter = [[AVAssetWriter alloc] initWithURL:fileURL fileType:fileType error:&nsError];

	if ( (_assetWriter == nil) || (nsError != nil) )
	{
		VTLOG("Could not create AVAssetWriter: %s", [[nsError description] UTF8String]);
		if (outError != NULL) *outError = ClientAVCaptureError_FileOpenError;
		return self;
	}

	// The audio input is always present.  AVAssetWriter encodes the LPCM
	// samples we append into AAC, so no separate audio encoder is needed.
	AudioStreamBasicDescription asbd;
	memset(&asbd, 0, sizeof(asbd));
	asbd.mSampleRate = (Float64)DESMUME_SAMPLE_RATE;
	asbd.mFormatID = kAudioFormatLinearPCM;
	asbd.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
	asbd.mBytesPerPacket = sizeof(int16_t) * 2;
	asbd.mFramesPerPacket = 1;
	asbd.mBytesPerFrame = sizeof(int16_t) * 2;
	asbd.mChannelsPerFrame = 2;
	asbd.mBitsPerChannel = 16;
	CMAudioFormatDescriptionCreate(kCFAllocatorDefault, &asbd, 0, NULL, 0, NULL, NULL, &_audioFormatDesc);

	NSDictionary *audioSettings = [MacVTWriter audioSettingsForCodec:spec.audioCodec];
	_audioInput = [[AVAssetWriterInput alloc] initWithMediaType:AVMediaTypeAudio outputSettings:audioSettings];
	// Audio arrives continuously from the emulator while it runs, i.e. in real
	// time.  With NO, AVAssetWriter buffers for optimal interleaving and stops
	// accepting samples whenever the other track lags -- which permanently
	// stalled the video input about four seconds into every recording.
	[_audioInput setExpectsMediaDataInRealTime:YES];

	BOOL canAddAudio = NO;
	@try
	{
		canAddAudio = [_assetWriter canAddInput:_audioInput];
	}
	@catch (NSException *exception)
	{
		VTLOG("canAddInput(audio) threw %s: %s",
			  [[exception name] UTF8String], [[exception reason] UTF8String]);
	}

	if (!canAddAudio)
	{
		VTLOG("AVAssetWriter rejected the audio input (codec=%d).", (int)spec.audioCodec);
		if (outError != NULL) *outError = ClientAVCaptureError_AudioStreamError;
		return self;
	}
	[_assetWriter addInput:_audioInput];

	if (!spec.isAudioOnly)
	{
		// A nil outputSettings dictionary means "pass through the samples
		// VideoToolbox already encoded".  The MP4 muxer *requires* a
		// sourceFormatHint for such an input: without it canAddInput: returns
		// NO and AVFoundation throws.  QuickTime Movie can infer the format
		// from the first sample, but supplying the hint is harmless there and
		// makes both containers use exactly the same code path.
		CMFormatDescriptionRef formatHint = NULL;
		const OSStatus hintStatus = CMVideoFormatDescriptionCreate(kCFAllocatorDefault,
										(CMVideoCodecType)spec.codecType,
										(int32_t)_encodeWidth, (int32_t)_encodeHeight,
										NULL, &formatHint);
		if ( (hintStatus == noErr) && (formatHint != NULL) )
		{
			_videoInput = [[AVAssetWriterInput alloc] initWithMediaType:AVMediaTypeVideo
														outputSettings:nil
													  sourceFormatHint:formatHint];
			CFRelease(formatHint);
		}
		else
		{
			// MP4 cannot accept a nil-settings input without the hint -- it
			// would throw from canAddInput:.  Refuse the recording instead.
			if (_container == MacCaptureContainer_MP4)
			{
				VTLOG("MP4 requires a video sourceFormatHint, which could not be built (status %d).", (int)hintStatus);
				if (outError != NULL) *outError = ClientAVCaptureError_VideoStreamError;
				return self;
			}
			VTLOG("Could not build a video sourceFormatHint (status %d); relying on format inference.", (int)hintStatus);
			_videoInput = [[AVAssetWriterInput alloc] initWithMediaType:AVMediaTypeVideo
														outputSettings:nil];
		}
		[_videoInput setExpectsMediaDataInRealTime:YES];

		BOOL canAdd = NO;
		@try
		{
			canAdd = [_assetWriter canAddInput:_videoInput];
		}
		@catch (NSException *exception)
		{
			VTLOG("canAddInput(video) threw %s: %s",
				  [[exception name] UTF8String], [[exception reason] UTF8String]);
		}

		if (!canAdd)
		{
			VTLOG("AVAssetWriter rejected the video input (fileType=%s).", [fileType UTF8String]);
			if (outError != NULL) *outError = ClientAVCaptureError_VideoStreamError;
			return self;
		}
		[_assetWriter addInput:_videoInput];

		_pixelBufferAttributes = [[NSDictionary alloc] initWithObjectsAndKeys:
			[NSNumber numberWithUnsignedInt:kCVPixelFormatType_32BGRA], (id)kCVPixelBufferPixelFormatTypeKey,
			[NSNumber numberWithUnsignedInteger:_encodeWidth], (id)kCVPixelBufferWidthKey,
			[NSNumber numberWithUnsignedInteger:_encodeHeight], (id)kCVPixelBufferHeightKey,
			nil];

		CFMutableDictionaryRef encoderSpec = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
											&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		CFStringRef encoderID = CFStringCreateWithCString(kCFAllocatorDefault,
														 spec.encoderID.c_str(), kCFStringEncodingUTF8);
		// This is what makes the user's choice real: VideoToolbox is told to use
		// the exact encoder (hardware or software) that was picked from the list,
		// instead of picking one for us like the Windows sink writer does.
		CFDictionarySetValue(encoderSpec, kVTVideoEncoderSpecification_EncoderID, encoderID);
		CFRelease(encoderID);

		const OSStatus sessionStatus = VTCompressionSessionCreate(NULL,
									(int32_t)_encodeWidth, (int32_t)_encodeHeight,
									(CMVideoCodecType)spec.codecType,
									encoderSpec,
									(CFDictionaryRef)_pixelBufferAttributes,
									NULL, &MacVTEncodeOutputCallback, self,
									&_compressionSession);
		CFRelease(encoderSpec);

		if (sessionStatus != noErr)
		{
			VTLOG("Could not create the VideoToolbox compression session (status %d).", (int)sessionStatus);
			if (outError != NULL) *outError = ClientAVCaptureError_InvalidVideoCodec;
			return self;
		}

		VTSessionSetProperty(_compressionSession, kVTCompressionPropertyKey_RealTime, kCFBooleanFalse);
		VTSessionSetProperty(_compressionSession, kVTCompressionPropertyKey_AllowFrameReordering, kCFBooleanFalse);

		int32_t keyFrameInterval = 60;
		CFNumberRef keyFrameIntervalRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &keyFrameInterval);
		VTSessionSetProperty(_compressionSession, kVTCompressionPropertyKey_MaxKeyFrameInterval, keyFrameIntervalRef);
		CFRelease(keyFrameIntervalRef);

		// Constant-rate codecs (ProRes, etc.) reject a bitrate; that is fine,
		// so the result of this call is deliberately ignored.
		SInt64 bitrate = (SInt64)spec.bitrate;
		CFNumberRef bitrateRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &bitrate);
		VTSessionSetProperty(_compressionSession, kVTCompressionPropertyKey_AverageBitRate, bitrateRef);
		CFRelease(bitrateRef);

		VTCompressionSessionPrepareToEncodeFrames(_compressionSession);
	}

	if (![_assetWriter startWriting])
	{
		VTLOG("AVAssetWriter could not start writing: %s", [[_assetWriter.error description] UTF8String]);
		if (outError != NULL) *outError = ClientAVCaptureError_FileOpenError;
		return self;
	}

	[_assetWriter startSessionAtSourceTime:kCMTimeZero];
	_didStartWriting = YES;
	_valid = YES;

	if (outError != NULL) *outError = ClientAVCaptureError_None;
	return self;
}

- (BOOL) isValid
{
	[_lock lock];
	const BOOL isValid = _valid && !_failed && (_assetWriter != nil) &&
						 (_assetWriter.status != AVAssetWriterStatusFailed);
	[_lock unlock];
	return isValid;
}

- (void) markFailed
{
	[_lock lock];
	_failed = YES;
	[_lock unlock];
}

- (BOOL) __waitForInput:(AVAssetWriterInput *)input budgetMS:(unsigned int)budgetMS
{
	// Deliberately does NOT hold _lock.  The previous version polled for up to
	// 2 s while holding it, which blocked appendAudio: on the file-write
	// thread for the same 2 s.  Audio then stopped reaching the muxer, so the
	// video input never became ready again: one hiccup became a permanent
	// stall and every later sample was dropped.  A short, lock-free budget
	// keeps one slow sample from poisoning the whole pipeline.
	for (unsigned int attempt = 0; attempt < budgetMS; attempt++)
	{
		if (_assetWriter.status == AVAssetWriterStatusFailed)
		{
			return NO;
		}
		if ([input isReadyForMoreMediaData])
		{
			return YES;
		}
		usleep(1000);
	}

	VTLOG("Timed out waiting for an AVAssetWriterInput to become ready (status=%ld).", (long)_assetWriter.status);
	return NO;
}

// GPU capture path.  The Metal presenter already rendered the finished frame
// into an IOSurface-backed BGRA pixel buffer, which is exactly the format the
// compression session was configured for, so there is no repack here at all --
// the buffer goes straight into VideoToolbox.
- (void) appendPixelBuffer:(CVPixelBufferRef)pixelBuffer frameNumber:(int64_t)frameNumber
{
	if ( (pixelBuffer == NULL) || (_compressionSession == NULL) )
	{
		return;
	}

	const size_t width  = CVPixelBufferGetWidth(pixelBuffer);
	const size_t height = CVPixelBufferGetHeight(pixelBuffer);
	if ( (width != _encodeWidth) || (height != _encodeHeight) )
	{
		VTLOG("Rejected a GPU frame of %zux%zu: the encoder expects %zux%zu.",
			  width, height, _encodeWidth, _encodeHeight);
		[self markFailed];
		return;
	}

	const CMTime presentationTime = CMTimeMake(frameNumber * kVTFrameDurationValue, kVTFrameTimeScale);
	const CMTime frameDuration = CMTimeMake(kVTFrameDurationValue, kVTFrameTimeScale);
	const OSStatus encodeStatus = VTCompressionSessionEncodeFrame(_compressionSession, pixelBuffer,
																presentationTime, frameDuration,
																NULL, NULL, NULL);

	if (encodeStatus != noErr)
	{
		VTLOG("VTCompressionSessionEncodeFrame (IOSurface) failed with status %d.", (int)encodeStatus);
		[self markFailed];
	}
}

- (void) appendEncodedSample:(CMSampleBufferRef)sampleBuffer{
	[_lock lock];
	AVAssetWriterInput *input = [_videoInput retain];
	const BOOL canAppend = (_valid && !_failed && (input != nil));
	[_lock unlock];
	
	if (!canAppend)
	{
		[input release];
		return;
	}
	
	if (![self __waitForInput:input budgetMS:250])
	{
		// Drop only this sample.  Killing the stream on a single stall used to
		// turn one late frame into a permanently dead recording.
		[input release];
		VTLOG("Dropped an encoded video sample: encoder input never became ready.");
		return;
	}
	
	[_lock lock];
	if (_valid && !_failed)
	{
		// AVAssetWriter throws NSInternalInconsistencyException rather than
		// returning NO when the input stops accepting data (a race with
		// finishWriting, or a failed writer).  An uncaught exception here
		// kills the encode callback thread and leaves a 0-byte file.
		@try
		{
			if (![input appendSampleBuffer:sampleBuffer])
			{
				VTLOG("Could not append an encoded video sample: %s",
					   [[_assetWriter.error description] UTF8String]);
				_failed = YES;
			}
		}
		@catch (NSException *exception)
		{
			VTLOG("Video append threw %s: %s",
				  [[exception name] UTF8String], [[exception reason] UTF8String]);
			_failed = YES;
		}
	}
	[_lock unlock];
	
	[input release];
}

- (void) appendAudio:(const uint8_t *)pcm size:(size_t)size sampleOffset:(int64_t)sampleOffset
{
	CMSampleBufferRef sampleBuffer = MacVTCreateAudioSampleBuffer(pcm, size, sampleOffset, _audioFormatDesc);
	if (sampleBuffer == NULL)
	{
		[self markFailed];
		return;
	}

	[_lock lock];
	AVAssetWriterInput *input = [_audioInput retain];
	const BOOL canAppend = (_valid && !_failed && (input != nil));
	[_lock unlock];
	
	if (!canAppend)
	{
		[input release];
		CFRelease(sampleBuffer);
		return;
	}
	
	if (![self __waitForInput:input budgetMS:250])
	{
		[input release];
		VTLOG("Dropped an audio sample: encoder input never became ready.");
		CFRelease(sampleBuffer);
		return;
	}
	
	[_lock lock];
	if (_valid && !_failed)
	{
		// See appendEncodedSample: appendSampleBuffer: throws on a race
		// with the writer; never let that escape onto the capture thread.
		@try
		{
			if (![input appendSampleBuffer:sampleBuffer])
			{
				VTLOG("Could not append an audio sample: %s",
					   [[_assetWriter.error description] UTF8String]);
				_failed = YES;
			}
		}
		@catch (NSException *exception)
		{
			VTLOG("Audio append threw %s: %s",
				  [[exception name] UTF8String], [[exception reason] UTF8String]);
			_failed = YES;
		}
	}
	[_lock unlock];
	
	[input release];
	CFRelease(sampleBuffer);
}

- (void) finish
{
	// An encode error can call Close() from inside WriteOneFrame before the
	// normal close thread runs. Finalizing twice would corrupt the file, so
	// make this idempotent.
	[_lock lock];
	if (_didFinish)
	{
		[_lock unlock];
		return;
	}
	_didFinish = YES;
	[_lock unlock];

	// CompleteFrames blocks until every pending output callback has run, so by
	// the time it returns all encoded samples have been appended to the writer.
	if (_compressionSession != NULL)
	{
		VTCompressionSessionCompleteFrames(_compressionSession, kCMTimeInvalid);
		VTCompressionSessionInvalidate(_compressionSession);
		CFRelease(_compressionSession);
		_compressionSession = NULL;
	}

	[_lock lock];
	const BOOL wasStarted = _didStartWriting;
	[_lock unlock];

	if (wasStarted)
	{
		if (_videoInput != nil) [_videoInput markAsFinished];
		[_audioInput markAsFinished];

		// Without finishWriting the container index is never written and the
		// resulting file is unplayable -- the same reason the Windows recorder
		// insists on IMFSinkWriter::Finalize().
		MacVTFinishFlag *flag = [[MacVTFinishFlag alloc] init];
		[_assetWriter finishWritingWithCompletionHandler:^{
			[flag signalDone];
		}];
		[flag waitUntilDone];
		[flag release];

		NSDictionary *attrs = [[NSFileManager defaultManager] attributesOfItemAtPath:_fileName error:nil];
		VTLOG("finishWriting: status=%ld (%s), error=%s, file size=%llu bytes",
			  (long)_assetWriter.status,
			  (_assetWriter.status == AVAssetWriterStatusCompleted) ? "success" : "FAILED",
			  _assetWriter.error ? [[_assetWriter.error description] UTF8String] : "(none)",
			  [attrs fileSize]);
	}
	else
	{
		VTLOG("Writer never started; cancelling and removing the empty file.");
		[_assetWriter cancelWriting];
		[[NSFileManager defaultManager] removeItemAtPath:_fileName error:nil];
	}

	[_lock lock];
	_valid = NO;
	[_lock unlock];
}

- (void) dealloc
{
	// Last-resort finalization.  AVAssetWriter keeps everything buffered until
	// finishWriting, so a missed Close() leaves a 0-byte file on disk.  Make
	// sure releasing the stream always produces a playable movie.
	if (!_didFinish && (_assetWriter != nil))
	{
		VTLOG("Writer released without an explicit Close(); finalizing now.");
		[self finish];
	}

	if (_compressionSession != NULL)
	{
		VTCompressionSessionInvalidate(_compressionSession);
		CFRelease(_compressionSession);
	}

	if (_audioFormatDesc != NULL) CFRelease(_audioFormatDesc);
	[_pixelBufferAttributes release];
	[_videoInput release];
	[_audioInput release];
	[_assetWriter release];
	[_fileName release];
	[_lock release];
	[super dealloc];
}

@end


#pragma mark - ClientAVCaptureFileStream implementation

VideoToolboxFileStream::VideoToolboxFileStream(const MacVideoEncoderSpec &spec)
	: _writer(NULL), _spec(spec)
{
}

VideoToolboxFileStream::~VideoToolboxFileStream()
{
	if (_writer != NULL)
	{
		[(MacVTWriter *)_writer release];
		_writer = NULL;
	}
}

ClientAVCaptureError VideoToolboxFileStream::Open()
{
	// Ask AVFoundation which container really accepts this encoder + audio
	// format.  MP4 is always tried first -- there is no codec allow-list, so
	// AV1 (on machines that support it) is not artificially forced into MOV.
	// QuickTime Movie is used only when the system rejects the combination.
	// Resolving before the file name is built keeps the extension, fileType
	// and format hint in sync.
	NSAutoreleasePool *probePool = [[NSAutoreleasePool alloc] init];
	_spec.container = [MacVTWriter resolveContainerForSpec:_spec
													 width:this->_videoWidth
													height:this->_videoHeight];
	[probePool release];

	this->_baseFileNameExt = (_spec.isAudioOnly && (_spec.audioCodec != MacAudioCodec_LPCM)) ? "m4a" :
		(_spec.container == MacCaptureContainer_MP4 ? "mp4" : "mov");

	const std::string fullFileName = this->_baseFileName + "." + this->_baseFileNameExt;
	NSString *fileName = [[NSFileManager defaultManager]
						  stringWithFileSystemRepresentation:fullFileName.c_str()
													  length:fullFileName.length()];
	if (fileName == nil)
	{
		return ClientAVCaptureError_FileOpenError;
	}

	ClientAVCaptureError error = ClientAVCaptureError_None;
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

	MacVTWriter *newWriter = [[MacVTWriter alloc] initWithSpec:_spec
													 fileName:fileName
														 width:this->_videoWidth
														height:this->_videoHeight
														 error:&error];
	if ( (error != ClientAVCaptureError_None) || ![newWriter isValid] )
	{
		VTLOG("Open() FAILED (error=%d) for %s; removing the empty file.", (int)error, fullFileName.c_str());
		[newWriter release];
		[[NSFileManager defaultManager] removeItemAtPath:fileName error:nil];
		[pool release];
		return (error != ClientAVCaptureError_None) ? error : ClientAVCaptureError_FileOpenError;
	}

	this->_writer = (void *)newWriter;
	[pool release];

	VTLOG("Open() succeeded: %s", fullFileName.c_str());
	return ClientAVCaptureError_None;
}

void VideoToolboxFileStream::Close(FileStreamCloseAction theAction)
{
	if (_writer == NULL)
	{
		return;
	}

	if (theAction == FSCA_WriteRemainingInQueue)
	{
		this->WriteAllFrames();
	}

	// WriteAllFrames() returns early on an encode error. Drain whatever is
	// left and hand the semaphore slots back, otherwise an emulator thread
	// already blocked in QueueWait() can never wake up.  Queued GPU frames own
	// a pixel buffer, so those references have to be dropped here as well.
	slock_lock(this->_mutexQueue);
	while (!this->_writeQueue.empty())
	{
		const AVStreamWriteParam &discarded = this->_writeQueue.front();
		if (discarded.srcPixelBuffer != NULL)
		{
			CFRelease((CVPixelBufferRef)discarded.srcPixelBuffer);
		}
		this->_writeQueue.pop();
		ssem_signal(this->_semQueue);
	}
	slock_unlock(this->_mutexQueue);

	VTLOG("Close(): action=%d, video frames=%llu, audio samples=%llu, payload bytes=%llu",
		  (int)theAction,
		  (unsigned long long)this->_writtenVideoFrameCount,
		  (unsigned long long)this->_writtenAudioSampleCount,
		  (unsigned long long)this->_writtenBytes);

	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	[(MacVTWriter *)_writer finish];
	[pool release];
}

bool VideoToolboxFileStream::IsValid()
{
	return (_writer != NULL) && [(MacVTWriter *)_writer isValid];
}

ClientAVCaptureError VideoToolboxFileStream::WriteOneFrame(const AVStreamWriteParam &param)
{
	CVPixelBufferRef pixelBuffer = (CVPixelBufferRef)param.srcPixelBuffer;

	if (!this->IsValid())
	{
		// The queue owns this buffer now, so it has to be released even when
		// the stream is already dead -- otherwise a failed recording leaks one
		// full frame per queued entry.
		if (pixelBuffer != NULL)
		{
			CFRelease(pixelBuffer);
		}
		return ClientAVCaptureError_FrameWriteError;
	}

	MacVTWriter *writer = (MacVTWriter *)_writer;
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

	// The GPU path supplies the capture tick so that a dropped frame leaves a
	// hole in the timeline instead of compressing the video against the audio.
	const int64_t frameNumber = (param.videoFrameIndex >= 0) ? param.videoFrameIndex
															: (int64_t)this->_writtenVideoFrameCount;

	// Video can only arrive as a finished pixel buffer.  There is no raw frame
	// pointer any more: the old RGB888 entry point is what dereferenced a
	// stale/NULL staging buffer inside vImage on the encoder thread.
	if (!_spec.isAudioOnly && (pixelBuffer != NULL))
	{
		[writer appendPixelBuffer:pixelBuffer frameNumber:frameNumber];
		this->_writtenVideoFrameCount++;
	}

	if ( (param.srcAudio != NULL) && (param.audioBufferSize > 0) )
	{
		[writer appendAudio:param.srcAudio
					   size:param.audioBufferSize
			   sampleOffset:(int64_t)this->_writtenAudioSampleCount];
		this->_writtenAudioSampleCount += param.audioBufferSize / (sizeof(int16_t) * 2);
	}

	// A GPU frame carries no CPU payload, so account for the pixels it stands
	// for instead of the (zero) buffer size.
	const size_t videoPayloadBytes = (pixelBuffer != NULL)
									 ? (this->_videoWidth * this->_videoHeight * 4)
									 : 0;

	this->_writtenBytes += videoPayloadBytes + param.audioBufferSize;

	if ( (pixelBuffer != NULL) &&
		 ((this->_writtenVideoFrameCount == 1) || ((this->_writtenVideoFrameCount % 60) == 0)) )
	{
		VTLOG("frame %llu enqueued for encode (video=%zu bytes, audio=%zu bytes, gpu=%d)",
			  (unsigned long long)this->_writtenVideoFrameCount,
			  videoPayloadBytes, param.audioBufferSize, (int)(pixelBuffer != NULL));
	}

	[pool release];

	// The queue handed ownership of the pixel buffer to us; VideoToolbox
	// retains it for the duration of the encode, so it can be released now.
	if (pixelBuffer != NULL)
	{
		CFRelease(pixelBuffer);
	}

	if (![writer isValid])
	{
		// Stop writing immediately so the file is finalized instead of leaving
		// a truncated, unplayable container behind.
		this->Close(FSCA_PurgeQueue);
		return ClientAVCaptureError_FrameWriteError;
	}

	return ClientAVCaptureError_None;
}
