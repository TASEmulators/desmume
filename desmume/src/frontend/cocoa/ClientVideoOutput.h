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

class ClientVideoCaptureOutput : public ClientVideoOutput
{
protected:
	ClientDisplay3DPresenter *_cdp;
	ClientAVCaptureObject *_avCaptureObject;
	Color4u8 *_videoCaptureBuffer;
	
	pthread_mutex_t _mutexCaptureBuffer;
	
public:
	ClientVideoCaptureOutput();
	~ClientVideoCaptureOutput();
	
	void SetPresenter(ClientDisplay3DPresenter *cdp);
	ClientDisplay3DPresenter* GetPresenter();
	
	void SetCaptureObject(ClientAVCaptureObject *captureObject);
	ClientAVCaptureObject* GetCaptureObject();
	
	virtual void HandleReceiveGPUFrame();
};

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
};

#endif // _CLIENT_OUTPUT_DISPLAY_H_
