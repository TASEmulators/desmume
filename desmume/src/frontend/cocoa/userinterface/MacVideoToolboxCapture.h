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

#ifndef _MAC_VIDEOTOOLBOX_CAPTURE_H_
#define _MAC_VIDEOTOOLBOX_CAPTURE_H_

#include <stdint.h>
#include <string>

#include "../ClientAVCaptureObject.h"

/*
 * macOS native AV capture back end.
 *
 * This is the macOS counterpart of the Windows Media Foundation recorder in
 * frontend/windows/videoencoders.cpp / mfvideowriter.cpp.  Like that recorder,
 * it hands the user a live list of the encoders the platform actually has and
 * lets the user pick one (hardware or software); unlike the old FFmpegFileStream
 * in MacAVCaptureTool.mm, no third-party libraries are involved.
 *
 * Capture, colorspace conversion and the bounded write queue are already
 * handled by ClientAVCaptureObject.  This class only has to encode and mux,
 * which is why it inherits ClientAVCaptureFileStream rather than replacing the
 * capture pipeline.
 */

// Only H.264 and HEVC can be muxed into an MP4 by AVAssetWriter.  ProRes,
// JPEG, raw RGB and the depth/alpha HEVC variants require QuickTime Movie.
enum MacCaptureContainer
{
	MacCaptureContainer_Auto = 0,	// probe MP4 first, fall back to MOV if AVFoundation rejects it
	MacCaptureContainer_MP4  = 1,
	MacCaptureContainer_MOV  = 2
};

// AVFoundation has no Vorbis/Opus encoder and no Ogg muxer, so the available
// audio formats are AAC (lossy), ALAC (lossless, compressed) and raw LPCM
// (lossless, uncompressed).  FLAC is decode-only in AVAssetWriter.
enum MacAudioCodec
{
	MacAudioCodec_AAC  = 0,
	MacAudioCodec_ALAC = 1,
	MacAudioCodec_LPCM = 2
};

// Plain C++ description of one encoder row.  Kept free of Objective-C so that
// the file stream can be driven from the C++ capture thread without bridging.
struct MacVideoEncoderSpec
{
	uint32_t codecType;		// CMVideoCodecType, e.g. 'avc1', 'hvc1', 'apcn'
	std::string encoderID;	// kVTVideoEncoderList_EncoderID, passed straight to the compression session
	std::string displayName;
	bool isHardware;
	bool isAudioOnly;		// the final "None" row, mirrors the Windows recorder
	MacCaptureContainer container;	// resolved, never Auto, by the time Open() runs
	MacAudioCodec audioCodec;
	uint32_t bitrate;		// bits per second; ignored by constant-rate codecs such as ProRes

	MacVideoEncoderSpec()
		: codecType(0), isHardware(false), isAudioOnly(false),
		  container(MacCaptureContainer_MOV), audioCodec(MacAudioCodec_AAC),
		  bitrate(8000000) {}
};

// UI hint only: which codecs make a sensible default encoder.  Deliberately
// NOT used to decide the container -- AVFoundation itself decides that at
// runtime (see +[MacVTWriter resolveContainerForSpec:]), so newer codecs such
// as AV1 are never blocked by a hard-coded allow-list.
bool MacCodecIsH264OrHEVC(uint32_t codecType);

#ifdef __OBJC__
#import <Foundation/Foundation.h>

// One row of the "Encoder" pop-up.  +availableVideoEncoders calls
// VTCopyVideoEncoderList() and returns every encoder VideoToolbox reports,
// both hardware and software, followed by an audio-only "None" row.
@interface MacVideoEncoderChoice : NSObject
{
@public
	uint32_t codecType;
	NSString *codecName;
	NSString *encoderName;
	NSString *displayName;
	NSString *encoderID;
	bool isHardware;
	bool isAudioOnly;
}

+ (NSArray *) availableVideoEncoders;
+ (NSString *) labelForHardware:(bool)isHardware;
- (MacVideoEncoderSpec) encoderSpec;

@end
#endif // __OBJC__

class VideoToolboxFileStream : public ClientAVCaptureFileStream
{
private:
	void *_writer;	// MacVTWriter *, deliberately opaque: the public header is C++-safe
	MacVideoEncoderSpec _spec;

public:
	explicit VideoToolboxFileStream(const MacVideoEncoderSpec &spec);
	virtual ~VideoToolboxFileStream();

	virtual ClientAVCaptureError Open();
	virtual void Close(FileStreamCloseAction theAction);
	virtual bool IsValid();

	virtual ClientAVCaptureError WriteOneFrame(const AVStreamWriteParam &param);
};

#endif // _MAC_VIDEOTOOLBOX_CAPTURE_H_
