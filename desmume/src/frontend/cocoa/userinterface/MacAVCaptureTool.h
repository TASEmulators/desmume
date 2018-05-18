/*
	Copyright (C) 2018 DeSmuME team
 
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

#ifndef _MAC_AVCAPTURETOOL_H_
#define _MAC_AVCAPTURETOOL_H_

#include <string>

#import "MacBaseCaptureTool.h"
#include "../ClientAVCaptureObject.h"
#include "../ClientExecutionControl.h"


enum VideoSizeOptionID
{
	VideoSizeOption_UseEmulatorSettings = 0,
	
	VideoSizeOption_NTSC_480p			= 1000,
	VideoSizeOption_PAL_576p			= 1001,
	
	VideoSizeOption_nHD					= 2000,
	VideoSizeOption_qHD					= 2001,
	VideoSizeOption_HD_720p				= 2002,
	VideoSizeOption_HDPlus				= 2003,
	VideoSizeOption_FHD_1080p			= 2004,
	VideoSizeOption_QHD					= 2005,
	VideoSizeOption_4K_UHD				= 2006,
	
	VideoSizeOption_QVGA				= 10000,
	VideoSizeOption_HVGA				= 10001,
	VideoSizeOption_VGA					= 10002,
	VideoSizeOption_SVGA				= 10003,
	VideoSizeOption_XGA					= 10004,
	VideoSizeOption_SXGA				= 10005,
	VideoSizeOption_UXGA				= 10006,
	VideoSizeOption_QXGA				= 10007,
	VideoSizeOption_HXGA				= 10008,
	
	VideoSizeOption_Custom				= 32000
};

struct AVStream;
struct AVCodec;
struct AVCodecContext;
struct AVOutputFormat;
struct AVFormatContext;
struct AVFrame;
struct AVPacket;
struct AVRational;
struct SwsContext;
struct SwrContext;
class ClientExecutionControl;
@class CocoaDSCore;
@class CocoaDSVideoCapture;

// a wrapper around a single output AVStream
struct OutputStream
{
	AVStream *stream;
	AVCodecContext *enc;
	
	/* pts of the next frame that will be generated */
	int64_t next_pts;
	int samples_count;
	
	AVFrame *frame;
	AVFrame *tmp_frame;
	
	size_t inBufferOffset;
	
	SwsContext *sws_ctx;
	SwrContext *swr_ctx;
};
typedef struct OutputStream OutputStream;

struct VideoSize
{
	size_t width;
	size_t height;
};

class FFmpegFileStream : public ClientAVCaptureFileStream
{
private:
	static bool __needFFmpegLibraryInit;

protected:
	OutputStream _videoStream;
	OutputStream _audioStream;
	AVOutputFormat *_outputFormat;
	AVFormatContext *_outputCtx;
	AVCodec *_audioCodec;
	AVCodec *_videoCodec;
	
	AVFrame* _InitVideoFrame(const int pixFormat, const int videoWidth, const int videoHeight);
	AVFrame* _InitAudioFrame(const int sampleFormat, const uint64_t channelLayout, const uint32_t sampleRate, const uint32_t nbSamples);
	void _CloseStream(OutputStream *outStream);
	void _CloseStreams();
	ClientAVCaptureError _SendStreamFrame(OutputStream *outStream);
	void _LogPacket(const AVPacket *thePacket);
	
public:
	FFmpegFileStream();
	virtual ~FFmpegFileStream();
	
	virtual ClientAVCaptureError Open();
	virtual void Close(FileStreamCloseAction theAction);
	virtual bool IsValid();
	
	virtual ClientAVCaptureError FlushVideo(uint8_t *srcBuffer, const size_t bufferSize);
	virtual ClientAVCaptureError FlushAudio(uint8_t *srcBuffer, const size_t bufferSize);
	virtual ClientAVCaptureError WriteOneFrame(const AVStreamWriteParam &param);
};

@interface MacAVCaptureToolDelegate : MacBaseCaptureToolDelegate
{
	ClientAVCaptureObject *_captureObject;
	ClientExecutionControl *execControl;
	CocoaDSCore *cdsCore;
	CocoaDSVideoCapture *_videoCaptureOutput;
	
	NSButton *recordButton;
	
	NSInteger videoSizeOption;
	NSInteger videoWidth;
	NSInteger videoHeight;
	
	BOOL isUsingCustomSize;
	BOOL isFileStreamClosing;
}

@property (readonly) IBOutlet NSButton *recordButton;
@property (assign) ClientExecutionControl *execControl;
@property (assign) CocoaDSCore *cdsCore;
@property (assign) NSInteger videoSizeOption;
@property (assign) NSInteger videoWidth;
@property (assign) NSInteger videoHeight;

@property (assign) BOOL isUsingCustomSize;
@property (assign) BOOL isRecording;
@property (assign) BOOL isFileStreamClosing;

- (void) setVideoSizeUsingEmulatorSettings;
- (void) readUserDefaults;
- (void) writeUserDefaults;
- (void) openFileStream;
- (void) closeFileStream;
- (void) avFileStreamCloseFinish:(NSNotification *)aNotification;

- (IBAction) toggleRecordingState:(id)sender;

@end

static void* RunAVCaptureCloseThread(void *arg);

#endif // _MAC_AVCAPTURETOOL_H_
