/*
	Copyright (C) 2025 DeSmuME team

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

#ifndef _CLIENT_OUTPUT_DISPLAY_H_
#define _CLIENT_OUTPUT_DISPLAY_H_

#include "ClientEmulationOutput.h"
#include "ClientExecutionControl.h"
#include "ClientDisplayView.h"
#include "../../filter/videofilter.h"

#include "../../types.h"
#undef BOOL

//#include "utilities.h"


class ClientVideoOutput : public ClientEmulationThreadedOutput
{
protected:
	uint32_t _receivedFrameIndex;
	uint32_t _currentReceivedFrameIndex;
	uint32_t _receivedFrameCount;
	
	NDSFrameInfo _ndsFrameInfo;
	
	pthread_mutex_t _mutexReceivedFrameIndex;
	pthread_mutex_t _mutexNDSFrameInfo;
	
	virtual void _HandleDispatch(int32_t messageID);
	
public:
	ClientVideoOutput();
	~ClientVideoOutput();
	
	void TakeFrameCount();
	void SetNDSFrameInfo(const NDSFrameInfo &ndsFrameInfo);
	
	virtual void HandleReceiveGPUFrame();
};

// AV capture frame consumer.  Deliberately NOT a ClientEmulationOutput: it
// owns no thread and receives no dispatched messages.  ProcessFrame() runs
// synchronously on the display output thread right after the main view has
// presented.
//
// Video frames reach the encoder in exactly one way: a dedicated Metal capture
// presenter renders the frame through the same shaders the on-screen view uses
// and writes it straight into an IOSurface-backed CVPixelBuffer, which
// VideoToolbox encodes in place.  No CPU copy, no colorspace conversion, and
// every display option (rotation, view scale, hybrid layouts, output filter,
// pixel scaler, deposterize, backlight) behaves exactly as it does on screen.
//
// There is intentionally no CPU fallback any more.  The CPU compositor used to
// hand the encoder a raw RGB888 pointer into a staging buffer, and when that
// buffer was not there the encoder thread crashed inside vImage
// (vImageConvert_RGB888toBGRA8888 on NULL + slot offset).  It also re-implemented
// the presenter's geometry, so CPU and GPU frames of the same settings did not
// even share a framing.  Recording now requires the Metal display path; on a
// non-Metal build or an OpenGL fallback it reports that it is unavailable and
// produces no video.
//
// The capture presenter must NEVER be created from a non-display thread and
// must never block on the GPU: a presenter built off the display thread
// retains/releases the same shared texPairFetch textures the fetch object is
// replacing, which corrupts command buffer completion handlers, and any
// per-frame waitUntilCompleted destroys frame pacing.
class ClientVideoCaptureOutput
{
private:
	ClientAVCaptureObject *_avCaptureObject;
	size_t _targetWidth;
	size_t _targetHeight;
	ClientDisplayMode _mode;
	ClientDisplayLayout _layout;
	ClientDisplayOrder _order;
	double _gapScale;
	double _rotation;
	bool _useDeposterize;
	OutputFilterTypeID _outputFilterID;
	VideoFilterTypeID _pixelScalerID;
	
	// Opaque MacMetalVideoCapture.  Created lazily on the display thread.  When
	// creation fails, recording reports that it is unavailable and stops
	// feeding frames; there is no other path to fall back to.
	void *_gpuCapture;
	bool _gpuCaptureDisabled;
	unsigned int _gpuDroppedFrameCount;
	
	// Capture tick counter.  Advances once per captured frame, whether or not
	// that frame could be rendered, so dropped frames leave a gap in the video
	// timeline instead of drifting against the audio.
	int64_t _gpuFrameIndex;
	
	pthread_mutex_t _mutexCaptureBuffer;
	
public:
	ClientVideoCaptureOutput();
	~ClientVideoCaptureOutput();
	
	void SetProperties(size_t width, size_t height,
					   ClientDisplayMode mode, ClientDisplayLayout layout,
					   ClientDisplayOrder order, double gapScale, double rotation,
					   bool useDeposterize, OutputFilterTypeID outputFilterID,
					   VideoFilterTypeID pixelScalerID);
	
	void SetCaptureObject(ClientAVCaptureObject *captureObject);
	ClientAVCaptureObject* GetCaptureObject();
	
	void ProcessFrame(ClientDisplay3DPresenter *srcPresenter);
};

// Exactly one capture output may be active at a time; pass NULL to clear it.
// This is driven synchronously on the display output thread -- no manager
// registration, no thread and no message dispatch.
void SetActiveVideoCaptureOutput(ClientVideoCaptureOutput *output);

class ClientDisplayViewOutput : public ClientVideoOutput
{
private:
	void __InstanceInit(ClientDisplay3DView *cdv);
	
protected:
	ClientDisplay3DView *_cdv;
	GPUClientFetchObject *_fetchObj;
	ClientDisplayPresenterProperties _intermediateViewProps;
	
	pthread_mutex_t _mutexViewProperties;
	pthread_mutex_t _mutexIsHUDVisible;
	pthread_mutex_t _mutexUseVerticalSync;
	pthread_mutex_t _mutexVideoFiltersPreferGPU;
	pthread_mutex_t _mutexOutputFilter;
	pthread_mutex_t _mutexSourceDeposterize;
	pthread_mutex_t _mutexPixelScaler;
	pthread_mutex_t _mutexDisplayVideoSource;
	pthread_mutex_t _mutexDisplayID;
	
	virtual void _HandleDispatch(int32_t messageID);
	
public:
	ClientDisplayViewOutput();
	ClientDisplayViewOutput(ClientDisplay3DView *cdv);
	~ClientDisplayViewOutput();
	
	virtual void SetClientDisplayView(ClientDisplay3DView *cdv);
	virtual ClientDisplay3DView* GetClientDisplayView();
	
	virtual GPUClientFetchObject* GetFetchObject();
	virtual void SetFetchObject(GPUClientFetchObject *fetchObj);
	
	virtual void SetVideoProcessingPrefersGPU(bool theState);
	virtual bool VideoProcessingPrefersGPU();
	
	bool CanProcessVideoOnGPU();
	bool WillFilterOnGPU();
	
	void SetAllowViewUpdates(bool allowUpdates);
	bool AllowViewUpdates();
	
	void SetHUDVisible(bool isVisible);
	bool IsHUDVisible();
	
	void SetHUDExecutionSpeedVisible(bool isVisible);
	bool IsHUDExecutionSpeedVisible();
	
	void SetHUDVideoFPSVisible(bool isVisible);
	bool IsHUDVideoFPSVisible();
	
	void SetHUDRender3DFPSVisible(bool isVisible);
	bool IsHUDRender3DFPSVisible();
	
	void SetHUDFrameIndexVisible(bool isVisible);
	bool IsHUDFrameIndexVisible();
	
	void SetHUDLagFrameCounterVisible(bool isVisible);
	bool IsHUDLagFrameCounterVisible();
	
	void SetHUDCPULoadAverageVisible(bool isVisible);
	bool IsHUDCPULoadAverageVisible();
	
	void SetHUDRealTimeClockVisible(bool isVisible);
	bool IsHUDRealTimeClockVisible();
	
	void SetHUDInputVisible(bool isVisible);
	bool IsHUDInputVisible();
	
	void SetHUDColorExecutionSpeed(Color4u8 color32);
	Color4u8 GetHUDColorExecutionSpeed();
	
	void SetHUDColorVideoFPS(Color4u8 color32);
	Color4u8 GetHUDColorVideoFPS();
	
	void SetHUDColorRender3DFPS(Color4u8 color32);
	Color4u8 GetHUDColorRender3DFPS();
	
	void SetHUDColorFrameIndex(Color4u8 color32);
	Color4u8 GetHUDColorFrameIndex();
	
	void SetHUDColorLagFrameCount(Color4u8 color32);
	Color4u8 GetHUDColorLagFrameCount();
	
	void SetHUDColorCPULoadAverage(Color4u8 color32);
	Color4u8 GetHUDColorCPULoadAverage();
	
	void SetHUDColorRealTimeClock(Color4u8 color32);
	Color4u8 GetHUDColorRealTimeClock();
	
	void SetHUDColorInputPendingAndApplied(Color4u8 color32);
	Color4u8 GetHUDColorInputPendingAndApplied();
	
	void SetHUDColorInputPendingOnly(Color4u8 color32);
	Color4u8 GetHUDColorInputPendingOnly();
	
	void SetHUDColorInputAppliedOnly(Color4u8 color32);
	Color4u8 GetHUDColorInputAppliedOnly();
	
	void SetDisplayMainVideoSource(ClientDisplaySource srcID);
	ClientDisplaySource GetDisplayMainVideoSource();
	
	void SetDisplayTouchVideoSource(ClientDisplaySource srcID);
	ClientDisplaySource GetDisplayTouchVideoSource();
	
	virtual void SetDisplayViewID(int32_t displayViewID);
	virtual int32_t GetDisplayViewID();
	
	void SetVerticalSync(bool theState);
	bool UseVerticalSync();
	
	virtual void SetSourceDeposterize(bool theState);
	virtual bool UseSourceDeposterize();
	
	void SetOutputFilterByID(OutputFilterTypeID filterID);
	OutputFilterTypeID GetOutputFilterID();
	
	virtual void SetPixelScalerByID(VideoFilterTypeID filterID);
	virtual VideoFilterTypeID GetPixelScalerID();
	
	void SetScaleFactor(double scaleFactor);
	double GetScaleFactor();
	
	void CommitPresenterProperties(const ClientDisplayPresenterProperties &viewProps, bool needsFlush);
	void UpdateHUD();
	virtual void SetViewNeedsFlush();
	virtual void FlushAndFinalizeImmediate();
	virtual void HandleChangeViewProperties(bool willFlush);
	virtual void HandleReloadReprocessRedraw();
	virtual void HandleReprocessRedraw();
	virtual void HandleRedraw();
	virtual void HandleCopyToPasteboard();
	
	void SetNDSFrameInfoWithInputHint(const NDSFrameInfo &ndsFrameInfo, const bool hintOnlyInputWasModified);
	
	// ClientVideoOutput methods
	virtual void HandleReceiveGPUFrame();
	
	// ClientEmulationOutput methods
	virtual void HandleEmuFrameProcessed();
};

class ClientDisplayViewOutputManager : public ClientEmulationOutputManager
{
public:
	ClientDisplayViewOutputManager() {};
	~ClientDisplayViewOutputManager() {};
	
	void TakeFrameCountAll();
	void SetNDSFrameInfoToAll(const NDSFrameInfo &frameInfo);
	void GenerateViewListAll(std::vector<ClientDisplayViewInterface *> &outFinalizeList);
	void GenerateFlushListForDisplay(int32_t displayID, std::vector<ClientDisplayViewInterface *> &outFlushList);
};

class ClientGPUFetchObjectMultiDisplayView
{
protected:
	ClientDisplayViewOutputManager *_outputManager;
	volatile int32_t _numberViewsPreferringCPUVideoProcessing;
	volatile int32_t _numberViewsUsingDirectToCPUFiltering;
	
public:
	ClientGPUFetchObjectMultiDisplayView();
	~ClientGPUFetchObjectMultiDisplayView();
	
	ClientDisplayViewOutputManager* GetOutputManager();
	
	void RegisterOutputDisplay(ClientDisplayViewOutput *outputDisplay);
	void UnregisterOutputDisplay(ClientDisplayViewOutput *outputDisplay);
	
	volatile int32_t GetNumberViewsPreferringCPUVideoProcessing() const;
	volatile int32_t GetNumberViewsUsingDirectToCPUFiltering() const;
	
	void IncrementViewsPreferringCPUVideoProcessing();
	void DecrementViewsPreferringCPUVideoProcessing();
	
	void IncrementViewsUsingDirectToCPUFiltering();
	void DecrementViewsUsingDirectToCPUFiltering();
	
	void PushVideoDataToAllDisplayViews();
	virtual void FlushAllViewsOnDisplay(int32_t displayID, uint64_t timeStampNow, uint64_t timeStampOutput);
	virtual void FlushMultipleViews(const std::vector<ClientDisplayViewInterface *> &cdvFlushList, uint64_t timeStampNow, uint64_t timeStampOutput);
	virtual void FinalizeAllViews();
};

#endif // _CLIENT_OUTPUT_DISPLAY_H_
