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

#include "ClientVideoOutput.h"

#include "ClientAVCaptureObject.h"
#include "ClientDisplayView.h"
#include "ClientExecutionControl.h"

#include "../../common.h"

#include "cocoa_globals.h"

#ifdef BOOL
#undef BOOL
#endif

ClientVideoOutput::ClientVideoOutput()
{
	_threadName = "Generic Video Output";
	_platformId = PlatformTypeID_Any;
	_typeId = OutputTypeID_Generic | OutputTypeID_Video;
	
	_ndsFrameInfo.clear();
	
	_receivedFrameIndex = 0;
	_currentReceivedFrameIndex = 0;
	_receivedFrameCount = 0;
	
	pthread_mutex_init(&_mutexReceivedFrameIndex, NULL);
	pthread_mutex_init(&_mutexNDSFrameInfo, NULL);
}

ClientVideoOutput::~ClientVideoOutput()
{
	pthread_mutex_destroy(&this->_mutexReceivedFrameIndex);
	pthread_mutex_destroy(&this->_mutexNDSFrameInfo);
}

void ClientVideoOutput::TakeFrameCount()
{
	pthread_mutex_lock(&this->_mutexReceivedFrameIndex);
	this->_receivedFrameCount = this->_receivedFrameIndex - this->_currentReceivedFrameIndex;
	this->_currentReceivedFrameIndex = this->_receivedFrameIndex;
	pthread_mutex_unlock(&this->_mutexReceivedFrameIndex);
}

void ClientVideoOutput::SetNDSFrameInfo(const NDSFrameInfo &ndsFrameInfo)
{
	pthread_mutex_lock(&this->_mutexNDSFrameInfo);
	this->_ndsFrameInfo.copyFrom(ndsFrameInfo);
	pthread_mutex_unlock(&this->_mutexNDSFrameInfo);
}

void ClientVideoOutput::_HandleDispatch(int32_t messageID)
{
	switch (messageID)
	{
		case MESSAGE_RECEIVE_GPU_FRAME:
			this->HandleReceiveGPUFrame();
			break;
			
		default:
			this->ClientEmulationThreadedOutput::_HandleDispatch(messageID);
			break;
	}
}

void ClientVideoOutput::HandleReceiveGPUFrame()
{
	pthread_mutex_lock(&this->_mutexReceivedFrameIndex);
	this->_receivedFrameIndex++;
	pthread_mutex_unlock(&this->_mutexReceivedFrameIndex);
}

ClientVideoCaptureOutput::ClientVideoCaptureOutput()
{
	_threadName = "Generic Video Capture Output";
	_platformId = PlatformTypeID_Any;
	_typeId = OutputTypeID_Generic | OutputTypeID_Video | OutputTypeID_Audio;
	
	_cdp = NULL;
	_avCaptureObject = NULL;
	_videoCaptureBuffer = NULL;
	
	pthread_mutex_init(&_mutexCaptureBuffer, NULL);
}

ClientVideoCaptureOutput::~ClientVideoCaptureOutput()
{
	pthread_mutex_lock(&this->_mutexCaptureBuffer);
	free_aligned(this->_videoCaptureBuffer);
	pthread_mutex_unlock(&this->_mutexCaptureBuffer);
	
	pthread_mutex_destroy(&this->_mutexCaptureBuffer);
}

void ClientVideoCaptureOutput::SetPresenter(ClientDisplay3DPresenter *cdp)
{
	this->_cdp = cdp;
}

ClientDisplay3DPresenter* ClientVideoCaptureOutput::GetPresenter()
{
	return this->_cdp;
}

void ClientVideoCaptureOutput::SetCaptureObject(ClientAVCaptureObject *captureObject)
{
	this->_avCaptureObject = captureObject;
}

ClientAVCaptureObject* ClientVideoCaptureOutput::GetCaptureObject()
{
	return this->_avCaptureObject;
}

void ClientVideoCaptureOutput::HandleReceiveGPUFrame()
{
	this->ClientVideoOutput::HandleReceiveGPUFrame();
	
	if ( (this->_cdp == NULL) || (this->_avCaptureObject == NULL) )
	{
		return;
	}
	
	const ClientDisplayPresenterProperties &cdpProperty = this->_cdp->GetPresenterProperties();
	
	this->_cdp->LoadDisplays();
	this->_cdp->ProcessDisplays();
	this->_cdp->UpdateLayout();
	
	pthread_mutex_lock(&this->_mutexCaptureBuffer);
	
	if (this->_videoCaptureBuffer == NULL)
	{
		this->_videoCaptureBuffer = (Color4u8 *)malloc_alignedPage(cdpProperty.clientWidth * cdpProperty.clientHeight * sizeof(Color4u8));
	}
	
	this->_cdp->CopyFrameToBuffer(this->_videoCaptureBuffer);
	this->_avCaptureObject->CaptureVideoFrame(this->_videoCaptureBuffer, (size_t)cdpProperty.clientWidth, (size_t)cdpProperty.clientHeight, NDSColorFormat_BGR888_Rev);
	this->_avCaptureObject->StreamWriteStart();
	
	pthread_mutex_unlock(&this->_mutexCaptureBuffer);
}

ClientDisplayViewOutput::ClientDisplayViewOutput()
{
	__InstanceInit(NULL);
}

ClientDisplayViewOutput::ClientDisplayViewOutput(ClientDisplay3DView *cdv)
{
	__InstanceInit(cdv);
}

void ClientDisplayViewOutput::__InstanceInit(ClientDisplay3DView *cdv)
{
	_threadName = "Generic Display View Output";
	_platformId = PlatformTypeID_Any;
	_typeId = OutputTypeID_Generic | OutputTypeID_Video | OutputTypeID_HostDisplay;
	
	_cdv = cdv;
	memset(&_intermediateViewProps, 0, sizeof(_intermediateViewProps));
	
	pthread_mutex_init(&_mutexViewProperties, NULL);
	pthread_mutex_init(&_mutexIsHUDVisible, NULL);
	pthread_mutex_init(&_mutexUseVerticalSync, NULL);
	pthread_mutex_init(&_mutexVideoFiltersPreferGPU, NULL);
	pthread_mutex_init(&_mutexOutputFilter, NULL);
	pthread_mutex_init(&_mutexSourceDeposterize, NULL);
	pthread_mutex_init(&_mutexPixelScaler, NULL);
	pthread_mutex_init(&_mutexDisplayVideoSource, NULL);
	pthread_mutex_init(&_mutexDisplayID, NULL);
}

ClientDisplayViewOutput::~ClientDisplayViewOutput()
{
	pthread_mutex_destroy(&this->_mutexViewProperties);
	pthread_mutex_destroy(&this->_mutexIsHUDVisible);
	pthread_mutex_destroy(&this->_mutexUseVerticalSync);
	pthread_mutex_destroy(&this->_mutexVideoFiltersPreferGPU);
	pthread_mutex_destroy(&this->_mutexOutputFilter);
	pthread_mutex_destroy(&this->_mutexSourceDeposterize);
	pthread_mutex_destroy(&this->_mutexPixelScaler);
	pthread_mutex_destroy(&this->_mutexDisplayVideoSource);
	pthread_mutex_destroy(&this->_mutexDisplayID);
}

void ClientDisplayViewOutput::SetClientDisplayView(ClientDisplay3DView *cdv)
{
	this->_cdv = cdv;
}

ClientDisplay3DView* ClientDisplayViewOutput::GetClientDisplayView()
{
	return this->_cdv;
}

void ClientDisplayViewOutput::SetFetchObject(GPUClientFetchObject *fetchObj)
{
	this->_fetchObj = fetchObj;
}

GPUClientFetchObject* ClientDisplayViewOutput::GetFetchObject()
{
	return this->_fetchObj;
}

void ClientDisplayViewOutput::SetVideoProcessingPrefersGPU(bool theState)
{
	pthread_mutex_lock(&this->_mutexVideoFiltersPreferGPU);
	this->_cdv->Get3DPresenter()->SetFiltersPreferGPU(theState);
	pthread_mutex_unlock(&this->_mutexVideoFiltersPreferGPU);
}

bool ClientDisplayViewOutput::VideoProcessingPrefersGPU()
{
	pthread_mutex_lock(&this->_mutexVideoFiltersPreferGPU);
	const bool theState = this->_cdv->Get3DPresenter()->GetFiltersPreferGPU();
	pthread_mutex_unlock(&this->_mutexVideoFiltersPreferGPU);
	
	return theState;
}

bool ClientDisplayViewOutput::CanProcessVideoOnGPU()
{
	return this->_cdv->Get3DPresenter()->CanFilterOnGPU();
}

bool ClientDisplayViewOutput::WillFilterOnGPU()
{
	return this->_cdv->Get3DPresenter()->WillFilterOnGPU();
}

void ClientDisplayViewOutput::SetAllowViewUpdates(bool allowUpdates)
{
	this->_cdv->SetAllowViewUpdates(allowUpdates);
}

bool ClientDisplayViewOutput::AllowViewUpdates()
{
	return this->_cdv->GetAllowViewUpdates();
}

void ClientDisplayViewOutput::SetHUDVisible(bool isVisible)
{
	pthread_mutex_lock(&this->_mutexIsHUDVisible);
	this->_cdv->Get3DPresenter()->SetHUDVisibility(isVisible);
	pthread_mutex_unlock(&this->_mutexIsHUDVisible);
	
	this->_cdv->SetViewNeedsFlush();
}

bool ClientDisplayViewOutput::IsHUDVisible()
{
	pthread_mutex_lock(&this->_mutexIsHUDVisible);
	const bool isVisible = this->_cdv->Get3DPresenter()->GetHUDVisibility();
	pthread_mutex_unlock(&this->_mutexIsHUDVisible);
	
	return isVisible;
}

void ClientDisplayViewOutput::SetHUDExecutionSpeedVisible(bool isVisible)
{
	pthread_mutex_lock(&this->_mutexIsHUDVisible);
	this->_cdv->Get3DPresenter()->SetHUDShowExecutionSpeed(isVisible);
	pthread_mutex_unlock(&this->_mutexIsHUDVisible);
	
	this->_cdv->SetViewNeedsFlush();
}

bool ClientDisplayViewOutput::IsHUDExecutionSpeedVisible()
{
	pthread_mutex_lock(&this->_mutexIsHUDVisible);
	const bool isVisible = this->_cdv->Get3DPresenter()->GetHUDShowExecutionSpeed();
	pthread_mutex_unlock(&this->_mutexIsHUDVisible);
	
	return isVisible;
}

void ClientDisplayViewOutput::SetHUDVideoFPSVisible(bool isVisible)
{
	pthread_mutex_lock(&this->_mutexIsHUDVisible);
	this->_cdv->Get3DPresenter()->SetHUDShowVideoFPS(isVisible);
	pthread_mutex_unlock(&this->_mutexIsHUDVisible);
	
	this->_cdv->SetViewNeedsFlush();
}

bool ClientDisplayViewOutput::IsHUDVideoFPSVisible()
{
	pthread_mutex_lock(&this->_mutexIsHUDVisible);
	const bool isVisible = this->_cdv->Get3DPresenter()->GetHUDShowVideoFPS();
	pthread_mutex_unlock(&this->_mutexIsHUDVisible);
	
	return isVisible;
}

void ClientDisplayViewOutput::SetHUDRender3DFPSVisible(bool isVisible)
{
	pthread_mutex_lock(&this->_mutexIsHUDVisible);
	this->_cdv->Get3DPresenter()->SetHUDShowRender3DFPS(isVisible);
	pthread_mutex_unlock(&this->_mutexIsHUDVisible);
	
	this->_cdv->SetViewNeedsFlush();
}

bool ClientDisplayViewOutput::IsHUDRender3DFPSVisible()
{
	pthread_mutex_lock(&this->_mutexIsHUDVisible);
	const bool isVisible = this->_cdv->Get3DPresenter()->GetHUDShowRender3DFPS();
	pthread_mutex_unlock(&this->_mutexIsHUDVisible);
	
	return isVisible;
}

void ClientDisplayViewOutput::SetHUDFrameIndexVisible(bool isVisible)
{
	pthread_mutex_lock(&this->_mutexIsHUDVisible);
	this->_cdv->Get3DPresenter()->SetHUDShowFrameIndex(isVisible);
	pthread_mutex_unlock(&this->_mutexIsHUDVisible);
	
	this->_cdv->SetViewNeedsFlush();
}

bool ClientDisplayViewOutput::IsHUDFrameIndexVisible()
{
	pthread_mutex_lock(&this->_mutexIsHUDVisible);
	const bool isVisible = this->_cdv->Get3DPresenter()->GetHUDShowFrameIndex();
	pthread_mutex_unlock(&this->_mutexIsHUDVisible);
	
	return isVisible;
}

void ClientDisplayViewOutput::SetHUDLagFrameCounterVisible(bool isVisible)
{
	pthread_mutex_lock(&this->_mutexIsHUDVisible);
	this->_cdv->Get3DPresenter()->SetHUDShowLagFrameCount(isVisible);
	pthread_mutex_unlock(&this->_mutexIsHUDVisible);
	
	this->_cdv->SetViewNeedsFlush();
}

bool ClientDisplayViewOutput::IsHUDLagFrameCounterVisible()
{
	pthread_mutex_lock(&this->_mutexIsHUDVisible);
	const bool isVisible = this->_cdv->Get3DPresenter()->GetHUDShowLagFrameCount();
	pthread_mutex_unlock(&this->_mutexIsHUDVisible);
	
	return isVisible;
}

void ClientDisplayViewOutput::SetHUDCPULoadAverageVisible(bool isVisible)
{
	pthread_mutex_lock(&this->_mutexIsHUDVisible);
	this->_cdv->Get3DPresenter()->SetHUDShowCPULoadAverage(isVisible);
	pthread_mutex_unlock(&this->_mutexIsHUDVisible);
	
	this->_cdv->SetViewNeedsFlush();
}

bool ClientDisplayViewOutput::IsHUDCPULoadAverageVisible()
{
	pthread_mutex_lock(&this->_mutexIsHUDVisible);
	const bool isVisible = this->_cdv->Get3DPresenter()->GetHUDShowCPULoadAverage();
	pthread_mutex_unlock(&this->_mutexIsHUDVisible);
	
	return isVisible;
}

void ClientDisplayViewOutput::SetHUDRealTimeClockVisible(bool isVisible)
{
	pthread_mutex_lock(&this->_mutexIsHUDVisible);
	this->_cdv->Get3DPresenter()->SetHUDShowRTC(isVisible);
	pthread_mutex_unlock(&this->_mutexIsHUDVisible);
	
	this->_cdv->SetViewNeedsFlush();
}

bool ClientDisplayViewOutput::IsHUDRealTimeClockVisible()
{
	pthread_mutex_lock(&this->_mutexIsHUDVisible);
	const bool isVisible = this->_cdv->Get3DPresenter()->GetHUDShowRTC();
	pthread_mutex_unlock(&this->_mutexIsHUDVisible);
	
	return isVisible;
}

void ClientDisplayViewOutput::SetHUDInputVisible(bool isVisible)
{
	pthread_mutex_lock(&this->_mutexIsHUDVisible);
	this->_cdv->Get3DPresenter()->SetHUDShowInput(isVisible);
	pthread_mutex_unlock(&this->_mutexIsHUDVisible);
	
	this->_cdv->SetViewNeedsFlush();
}

bool ClientDisplayViewOutput::IsHUDInputVisible()
{
	pthread_mutex_lock(&this->_mutexIsHUDVisible);
	const bool isVisible = this->_cdv->Get3DPresenter()->GetHUDShowInput();
	pthread_mutex_unlock(&this->_mutexIsHUDVisible);
	
	return isVisible;
}

void ClientDisplayViewOutput::SetHUDColorExecutionSpeed(Color4u8 color32)
{
	pthread_mutex_lock(&this->_mutexIsHUDVisible);
	this->_cdv->Get3DPresenter()->SetHUDColorExecutionSpeed(color32);
	pthread_mutex_unlock(&this->_mutexIsHUDVisible);
	
	this->_cdv->SetViewNeedsFlush();
}

Color4u8 ClientDisplayViewOutput::GetHUDColorExecutionSpeed()
{
	pthread_mutex_lock(&this->_mutexIsHUDVisible);
	const Color4u8 color32 = this->_cdv->Get3DPresenter()->GetHUDColorExecutionSpeed();
	pthread_mutex_unlock(&this->_mutexIsHUDVisible);
	
	return color32;
}

void ClientDisplayViewOutput::SetHUDColorVideoFPS(Color4u8 color32)
{
	pthread_mutex_lock(&this->_mutexIsHUDVisible);
	this->_cdv->Get3DPresenter()->SetHUDColorVideoFPS(color32);
	pthread_mutex_unlock(&this->_mutexIsHUDVisible);
	
	this->_cdv->SetViewNeedsFlush();
}

Color4u8 ClientDisplayViewOutput::GetHUDColorVideoFPS()
{
	pthread_mutex_lock(&this->_mutexIsHUDVisible);
	const Color4u8 color32 = this->_cdv->Get3DPresenter()->GetHUDColorVideoFPS();
	pthread_mutex_unlock(&this->_mutexIsHUDVisible);
	
	return color32;
}

void ClientDisplayViewOutput::SetHUDColorRender3DFPS(Color4u8 color32)
{
	pthread_mutex_lock(&this->_mutexIsHUDVisible);
	this->_cdv->Get3DPresenter()->SetHUDColorRender3DFPS(color32);
	pthread_mutex_unlock(&this->_mutexIsHUDVisible);
	
	this->_cdv->SetViewNeedsFlush();
}

Color4u8 ClientDisplayViewOutput::GetHUDColorRender3DFPS()
{
	pthread_mutex_lock(&this->_mutexIsHUDVisible);
	const Color4u8 color32 = this->_cdv->Get3DPresenter()->GetHUDColorRender3DFPS();
	pthread_mutex_unlock(&this->_mutexIsHUDVisible);
	
	return color32;
}

void ClientDisplayViewOutput::SetHUDColorFrameIndex(Color4u8 color32)
{
	pthread_mutex_lock(&this->_mutexIsHUDVisible);
	this->_cdv->Get3DPresenter()->SetHUDColorFrameIndex(color32);
	pthread_mutex_unlock(&this->_mutexIsHUDVisible);
	
	this->_cdv->SetViewNeedsFlush();
}

Color4u8 ClientDisplayViewOutput::GetHUDColorFrameIndex()
{
	pthread_mutex_lock(&this->_mutexIsHUDVisible);
	const Color4u8 color32 = this->_cdv->Get3DPresenter()->GetHUDColorFrameIndex();
	pthread_mutex_unlock(&this->_mutexIsHUDVisible);
	
	return color32;
}

void ClientDisplayViewOutput::SetHUDColorLagFrameCount(Color4u8 color32)
{
	pthread_mutex_lock(&this->_mutexIsHUDVisible);
	this->_cdv->Get3DPresenter()->SetHUDColorLagFrameCount(color32);
	pthread_mutex_unlock(&this->_mutexIsHUDVisible);
	
	this->_cdv->SetViewNeedsFlush();
}

Color4u8 ClientDisplayViewOutput::GetHUDColorLagFrameCount()
{
	pthread_mutex_lock(&this->_mutexIsHUDVisible);
	const Color4u8 color32 = this->_cdv->Get3DPresenter()->GetHUDColorLagFrameCount();
	pthread_mutex_unlock(&this->_mutexIsHUDVisible);
	
	return color32;
}

void ClientDisplayViewOutput::SetHUDColorCPULoadAverage(Color4u8 color32)
{
	pthread_mutex_lock(&this->_mutexIsHUDVisible);
	this->_cdv->Get3DPresenter()->SetHUDColorCPULoadAverage(color32);
	pthread_mutex_unlock(&this->_mutexIsHUDVisible);
	
	this->_cdv->SetViewNeedsFlush();
}

Color4u8 ClientDisplayViewOutput::GetHUDColorCPULoadAverage()
{
	pthread_mutex_lock(&this->_mutexIsHUDVisible);
	const Color4u8 color32 = this->_cdv->Get3DPresenter()->GetHUDColorCPULoadAverage();
	pthread_mutex_unlock(&this->_mutexIsHUDVisible);
	
	return color32;
}

void ClientDisplayViewOutput::SetHUDColorRealTimeClock(Color4u8 color32)
{
	pthread_mutex_lock(&this->_mutexIsHUDVisible);
	this->_cdv->Get3DPresenter()->SetHUDColorRTC(color32);
	pthread_mutex_unlock(&this->_mutexIsHUDVisible);
	
	this->_cdv->SetViewNeedsFlush();
}

Color4u8 ClientDisplayViewOutput::GetHUDColorRealTimeClock()
{
	pthread_mutex_lock(&this->_mutexIsHUDVisible);
	const Color4u8 color32 = this->_cdv->Get3DPresenter()->GetHUDColorRTC();
	pthread_mutex_unlock(&this->_mutexIsHUDVisible);
	
	return color32;
}

void ClientDisplayViewOutput::SetHUDColorInputPendingAndApplied(Color4u8 color32)
{
	pthread_mutex_lock(&this->_mutexIsHUDVisible);
	this->_cdv->Get3DPresenter()->SetHUDColorInputPendingAndApplied(color32);
	pthread_mutex_unlock(&this->_mutexIsHUDVisible);
	
	this->_cdv->SetViewNeedsFlush();
}

Color4u8 ClientDisplayViewOutput::GetHUDColorInputPendingAndApplied()
{
	pthread_mutex_lock(&this->_mutexIsHUDVisible);
	const Color4u8 color32 = this->_cdv->Get3DPresenter()->GetHUDColorInputPendingAndApplied();
	pthread_mutex_unlock(&this->_mutexIsHUDVisible);
	
	return color32;
}

void ClientDisplayViewOutput::SetHUDColorInputPendingOnly(Color4u8 color32)
{
	pthread_mutex_lock(&this->_mutexIsHUDVisible);
	this->_cdv->Get3DPresenter()->SetHUDColorInputPendingOnly(color32);
	pthread_mutex_unlock(&this->_mutexIsHUDVisible);
	
	this->_cdv->SetViewNeedsFlush();
}

Color4u8 ClientDisplayViewOutput::GetHUDColorInputPendingOnly()
{
	pthread_mutex_lock(&this->_mutexIsHUDVisible);
	const Color4u8 color32 = this->_cdv->Get3DPresenter()->GetHUDColorInputPendingOnly();
	pthread_mutex_unlock(&this->_mutexIsHUDVisible);
	
	return color32;
}

void ClientDisplayViewOutput::SetHUDColorInputAppliedOnly(Color4u8 color32)
{
	pthread_mutex_lock(&this->_mutexIsHUDVisible);
	this->_cdv->Get3DPresenter()->SetHUDColorInputAppliedOnly(color32);
	pthread_mutex_unlock(&this->_mutexIsHUDVisible);
	
	this->_cdv->SetViewNeedsFlush();
}

Color4u8 ClientDisplayViewOutput::GetHUDColorInputAppliedOnly()
{
	pthread_mutex_lock(&this->_mutexIsHUDVisible);
	const Color4u8 color32 = this->_cdv->Get3DPresenter()->GetHUDColorInputAppliedOnly();
	pthread_mutex_unlock(&this->_mutexIsHUDVisible);
	
	return color32;
}

void ClientDisplayViewOutput::SetDisplayMainVideoSource(ClientDisplaySource srcID)
{
	pthread_mutex_lock(&this->_mutexDisplayVideoSource);
	this->_cdv->Get3DPresenter()->SetDisplayVideoSource(NDSDisplayID_Main, srcID);
	pthread_mutex_unlock(&this->_mutexDisplayVideoSource);
	
	this->Dispatch(MESSAGE_RELOAD_REPROCESS_REDRAW);
}

ClientDisplaySource ClientDisplayViewOutput::GetDisplayMainVideoSource()
{
	pthread_mutex_lock(&this->_mutexDisplayVideoSource);
	const ClientDisplaySource srcID = this->_cdv->Get3DPresenter()->GetDisplayVideoSource(NDSDisplayID_Main);
	pthread_mutex_unlock(&this->_mutexDisplayVideoSource);
	
	return srcID;
}

void ClientDisplayViewOutput::SetDisplayTouchVideoSource(ClientDisplaySource srcID)
{
	pthread_mutex_lock(&this->_mutexDisplayVideoSource);
	this->_cdv->Get3DPresenter()->SetDisplayVideoSource(NDSDisplayID_Touch, srcID);
	pthread_mutex_unlock(&this->_mutexDisplayVideoSource);
	
	this->Dispatch(MESSAGE_RELOAD_REPROCESS_REDRAW);
}

ClientDisplaySource ClientDisplayViewOutput::GetDisplayTouchVideoSource()
{
	pthread_mutex_lock(&this->_mutexDisplayVideoSource);
	const ClientDisplaySource srcID = this->_cdv->Get3DPresenter()->GetDisplayVideoSource(NDSDisplayID_Touch);
	pthread_mutex_unlock(&this->_mutexDisplayVideoSource);
	
	return srcID;
}

void ClientDisplayViewOutput::SetDisplayViewID(int32_t displayID)
{
	pthread_mutex_lock(&this->_mutexDisplayID);
	this->_cdv->ClientDisplay3DView::SetDisplayViewID(displayID);
	pthread_mutex_unlock(&this->_mutexDisplayID);
}

int32_t ClientDisplayViewOutput::GetDisplayViewID()
{
	pthread_mutex_lock(&this->_mutexDisplayID);
	const int32_t displayID = this->_cdv->ClientDisplay3DView::GetDisplayViewID();
	pthread_mutex_unlock(&this->_mutexDisplayID);
	
	return displayID;
}

void ClientDisplayViewOutput::SetVerticalSync(bool theState)
{
	pthread_mutex_lock(&this->_mutexUseVerticalSync);
	this->_cdv->ClientDisplay3DView::SetUseVerticalSync(theState);
	pthread_mutex_unlock(&this->_mutexUseVerticalSync);
}

bool ClientDisplayViewOutput::UseVerticalSync()
{
	pthread_mutex_lock(&this->_mutexUseVerticalSync);
	const bool theState = this->_cdv->ClientDisplay3DView::GetUseVerticalSync();
	pthread_mutex_unlock(&this->_mutexUseVerticalSync);
	
	return theState;
}

void ClientDisplayViewOutput::SetSourceDeposterize(bool theState)
{
	pthread_mutex_lock(&this->_mutexSourceDeposterize);
	this->_cdv->Get3DPresenter()->SetSourceDeposterize(theState);
	pthread_mutex_unlock(&this->_mutexSourceDeposterize);
}

bool ClientDisplayViewOutput::UseSourceDeposterize()
{
	pthread_mutex_lock(&this->_mutexSourceDeposterize);
	const bool theState = this->_cdv->Get3DPresenter()->GetSourceDeposterize();
	pthread_mutex_unlock(&this->_mutexSourceDeposterize);
	
	return theState;
}

void ClientDisplayViewOutput::SetOutputFilterByID(OutputFilterTypeID filterID)
{
	pthread_mutex_lock(&this->_mutexOutputFilter);
	this->_cdv->Get3DPresenter()->SetOutputFilter(filterID);
	pthread_mutex_unlock(&this->_mutexOutputFilter);
	
	this->Dispatch(MESSAGE_REDRAW_VIEW);
}

OutputFilterTypeID ClientDisplayViewOutput::GetOutputFilterID()
{
	pthread_mutex_lock(&this->_mutexOutputFilter);
	const OutputFilterTypeID filterID = this->_cdv->Get3DPresenter()->GetOutputFilter();
	pthread_mutex_unlock(&this->_mutexOutputFilter);
	
	return filterID;
}

void ClientDisplayViewOutput::SetPixelScalerByID(VideoFilterTypeID filterID)
{
	pthread_mutex_lock(&this->_mutexPixelScaler);
	this->_cdv->Get3DPresenter()->SetPixelScaler(filterID);
	pthread_mutex_unlock(&this->_mutexPixelScaler);
	
	this->Dispatch(MESSAGE_REPROCESS_AND_REDRAW);
}

VideoFilterTypeID ClientDisplayViewOutput::GetPixelScalerID()
{
	pthread_mutex_lock(&this->_mutexPixelScaler);
	const VideoFilterTypeID filterID = this->_cdv->Get3DPresenter()->GetPixelScaler();
	pthread_mutex_unlock(&this->_mutexPixelScaler);
	
	return filterID;
}

void ClientDisplayViewOutput::SetScaleFactor(double scaleFactor)
{
	pthread_mutex_lock(&this->_mutexIsHUDVisible);
	this->_cdv->Get3DPresenter()->SetScaleFactor(scaleFactor);
	pthread_mutex_unlock(&this->_mutexIsHUDVisible);
}

double ClientDisplayViewOutput::GetScaleFactor()
{
	pthread_mutex_lock(&this->_mutexIsHUDVisible);
	const double scaleFactor = this->_cdv->Get3DPresenter()->GetScaleFactor();
	pthread_mutex_unlock(&this->_mutexIsHUDVisible);
	
	return scaleFactor;
}

void ClientDisplayViewOutput::CommitPresenterProperties(const ClientDisplayPresenterProperties &viewProps, bool needsFlush)
{
	pthread_mutex_lock(&this->_mutexViewProperties);
	this->_intermediateViewProps = viewProps;
	pthread_mutex_unlock(&this->_mutexViewProperties);
	
	this->HandleChangeViewProperties(needsFlush);
}

void ClientDisplayViewOutput::UpdateHUD()
{
	pthread_mutex_lock(&this->_mutexReceivedFrameIndex);
	ClientFrameInfo clientFrameInfo;
	clientFrameInfo.videoFPS = this->_receivedFrameCount;
	pthread_mutex_unlock(&this->_mutexReceivedFrameIndex);
	
	pthread_mutex_lock(&this->_mutexNDSFrameInfo);
	this->_cdv->Get3DPresenter()->SetHUDInfo(clientFrameInfo, this->_ndsFrameInfo);
	pthread_mutex_unlock(&this->_mutexNDSFrameInfo);
}

void ClientDisplayViewOutput::SetViewNeedsFlush()
{
	this->_cdv->SetViewNeedsFlush();
}

void ClientDisplayViewOutput::FlushAndFinalizeImmediate()
{
	this->_cdv->FlushAndFinalizeImmediate();
}

void ClientDisplayViewOutput::_HandleDispatch(int32_t messageID)
{
	switch (messageID)
	{
		case MESSAGE_CHANGE_VIEW_PROPERTIES:
			this->HandleChangeViewProperties(true);
			break;
			
		case MESSAGE_RELOAD_REPROCESS_REDRAW:
			this->HandleReloadReprocessRedraw();
			break;
			
		case MESSAGE_REPROCESS_AND_REDRAW:
			this->HandleReprocessRedraw();
			break;
			
		case MESSAGE_REDRAW_VIEW:
			this->HandleRedraw();
			break;
			
		case MESSAGE_COPY_TO_PASTEBOARD:
			this->HandleCopyToPasteboard();
			break;
			
		default:
			this->ClientVideoOutput::_HandleDispatch(messageID);
			break;
	}
}

void ClientDisplayViewOutput::HandleChangeViewProperties(bool willFlush)
{
	pthread_mutex_lock(&this->_mutexViewProperties);
	this->_cdv->Get3DPresenter()->CommitPresenterProperties(this->_intermediateViewProps);
	pthread_mutex_unlock(&this->_mutexViewProperties);
	
	this->_cdv->Get3DPresenter()->SetupPresenterProperties();
	
	if (willFlush && !this->IsIdle())
	{
		this->_cdv->SetViewNeedsFlush();
	}
}

void ClientDisplayViewOutput::HandleReloadReprocessRedraw()
{
	this->_cdv->Get3DPresenter()->LoadDisplays();
	this->_cdv->Get3DPresenter()->ProcessDisplays();
	
	this->HandleEmuFrameProcessed();
}

void ClientDisplayViewOutput::HandleReprocessRedraw()
{
	this->_cdv->Get3DPresenter()->ProcessDisplays();
	
	if (!this->IsIdle())
	{
		this->_cdv->SetViewNeedsFlush();
	}
}

void ClientDisplayViewOutput::HandleRedraw()
{
	if (!this->IsIdle())
	{
		this->_cdv->SetViewNeedsFlush();
	}
}

void ClientDisplayViewOutput::HandleCopyToPasteboard()
{
	// Do nothing. This is implementation dependent.
}

void ClientDisplayViewOutput::SetNDSFrameInfoWithInputHint(const NDSFrameInfo &ndsFrameInfo, const bool hintOnlyInputWasModified)
{
	this->ClientVideoOutput::SetNDSFrameInfo(ndsFrameInfo);
	this->UpdateHUD();
	
	if ( !this->IsIdle() && (!hintOnlyInputWasModified || this->IsHUDInputVisible()) )
	{
		this->_cdv->SetViewNeedsFlush();
	}
}

void ClientDisplayViewOutput::HandleReceiveGPUFrame()
{
	this->ClientVideoOutput::HandleReceiveGPUFrame();
	
	this->_cdv->Get3DPresenter()->LoadDisplays();
	this->_cdv->Get3DPresenter()->ProcessDisplays();
}

void ClientDisplayViewOutput::HandleEmuFrameProcessed()
{
	this->ClientVideoOutput::HandleEmuFrameProcessed();
	this->UpdateHUD();
	
	if (!this->IsIdle())
	{
		this->_cdv->SetViewNeedsFlush();
	}
}

void ClientDisplayViewOutputManager::TakeFrameCountAll()
{
	// Add all pending outputs to be registered.
	pthread_mutex_lock(&this->_pendingRegisterListMutex);
	this->_RegisterPending();
	pthread_mutex_unlock(&this->_pendingRegisterListMutex);
	
	// Remove all pending outputs to be unregistered.
	pthread_mutex_lock(&this->_pendingUnregisterListMutex);
	this->_UnregisterPending();
	
	// Finally, run the outputs.
	for (size_t i = 0; i < this->_runList.size(); i++)
	{
		ClientVideoOutput *runningOutput = (ClientVideoOutput *)this->_runList[i];
		runningOutput->TakeFrameCount();
	}
	
	pthread_mutex_unlock(&this->_pendingUnregisterListMutex);
}

void ClientDisplayViewOutputManager::SetNDSFrameInfoToAll(const NDSFrameInfo &frameInfo)
{
	// Add all pending outputs to be registered.
	pthread_mutex_lock(&this->_pendingRegisterListMutex);
	this->_RegisterPending();
	pthread_mutex_unlock(&this->_pendingRegisterListMutex);
	
	// Remove all pending outputs to be unregistered.
	pthread_mutex_lock(&this->_pendingUnregisterListMutex);
	this->_UnregisterPending();
	
	// Finally, run the outputs.
	for (size_t i = 0; i < this->_runList.size(); i++)
	{
		ClientDisplayViewOutput *runningOutput = (ClientDisplayViewOutput *)this->_runList[i];
		runningOutput->SetNDSFrameInfoWithInputHint(frameInfo, false);
	}
	
	pthread_mutex_unlock(&this->_pendingUnregisterListMutex);
}

void ClientDisplayViewOutputManager::GenerateViewListAll(std::vector<ClientDisplayViewInterface *> &outFinalizeList)
{
	// Add all pending outputs to be registered.
	pthread_mutex_lock(&this->_pendingRegisterListMutex);
	this->_RegisterPending();
	pthread_mutex_unlock(&this->_pendingRegisterListMutex);
	
	// Remove all pending outputs to be unregistered.
	pthread_mutex_lock(&this->_pendingUnregisterListMutex);
	this->_UnregisterPending();
	
	// Finally, run the outputs.
	for (size_t i = 0; i < this->_runList.size(); i++)
	{
		ClientDisplayViewOutput *runningOutput = (ClientDisplayViewOutput *)this->_runList[i];
		ClientDisplayViewInterface *cdv = (ClientDisplayViewInterface *)runningOutput->GetClientDisplayView();
		outFinalizeList.push_back(cdv);
	}
	
	pthread_mutex_unlock(&this->_pendingUnregisterListMutex);
}

void ClientDisplayViewOutputManager::GenerateFlushListForDisplay(int32_t displayID, std::vector<ClientDisplayViewInterface *> &outFlushList)
{
	// Add all pending outputs to be registered.
	pthread_mutex_lock(&this->_pendingRegisterListMutex);
	this->_RegisterPending();
	pthread_mutex_unlock(&this->_pendingRegisterListMutex);
	
	// Remove all pending outputs to be unregistered.
	pthread_mutex_lock(&this->_pendingUnregisterListMutex);
	this->_UnregisterPending();
	
	// Finally, run the outputs.
	for (size_t i = 0; i < this->_runList.size(); i++)
	{
		ClientDisplayViewOutput *runningOutput = (ClientDisplayViewOutput *)this->_runList[i];
		ClientDisplayViewInterface *cdv = (ClientDisplayViewInterface *)runningOutput->GetClientDisplayView();
		
		if (cdv->GetDisplayViewID() == displayID)
		{
			if (cdv->GetViewNeedsFlush())
			{
				outFlushList.push_back(cdv);
			}
		}
	}
	
	pthread_mutex_unlock(&this->_pendingUnregisterListMutex);
}

ClientGPUFetchObjectMultiDisplayView::ClientGPUFetchObjectMultiDisplayView()
{
	_outputManager = new ClientDisplayViewOutputManager;
	_numberViewsPreferringCPUVideoProcessing = 0;
	_numberViewsUsingDirectToCPUFiltering = 0;
}

ClientGPUFetchObjectMultiDisplayView::~ClientGPUFetchObjectMultiDisplayView()
{
	delete this->_outputManager;
	this->_outputManager = NULL;
}

ClientDisplayViewOutputManager* ClientGPUFetchObjectMultiDisplayView::GetOutputManager()
{
	return this->_outputManager;
}

void ClientGPUFetchObjectMultiDisplayView::RegisterOutputDisplay(ClientDisplayViewOutput *outputDisplay)
{
	_outputManager->Register(outputDisplay, NULL);
}

void ClientGPUFetchObjectMultiDisplayView::UnregisterOutputDisplay(ClientDisplayViewOutput *outputDisplay)
{
	_outputManager->Unregister(outputDisplay);
}

volatile int32_t ClientGPUFetchObjectMultiDisplayView::GetNumberViewsPreferringCPUVideoProcessing() const
{
	return this->_numberViewsPreferringCPUVideoProcessing;
}

volatile int32_t ClientGPUFetchObjectMultiDisplayView::GetNumberViewsUsingDirectToCPUFiltering() const
{
	return this->_numberViewsUsingDirectToCPUFiltering;
}

void ClientGPUFetchObjectMultiDisplayView::IncrementViewsPreferringCPUVideoProcessing()
{
	atomic_inc_32(&this->_numberViewsPreferringCPUVideoProcessing);
}

void ClientGPUFetchObjectMultiDisplayView::DecrementViewsPreferringCPUVideoProcessing()
{
	atomic_dec_32(&this->_numberViewsPreferringCPUVideoProcessing);
}

void ClientGPUFetchObjectMultiDisplayView::IncrementViewsUsingDirectToCPUFiltering()
{
	atomic_inc_32(&this->_numberViewsUsingDirectToCPUFiltering);
}

void ClientGPUFetchObjectMultiDisplayView::DecrementViewsUsingDirectToCPUFiltering()
{
	atomic_dec_32(&this->_numberViewsUsingDirectToCPUFiltering);
}

void ClientGPUFetchObjectMultiDisplayView::PushVideoDataToAllDisplayViews()
{
	this->_outputManager->DispatchAll(MESSAGE_RECEIVE_GPU_FRAME);
}

void ClientGPUFetchObjectMultiDisplayView::FlushAllViewsOnDisplay(int32_t displayID, uint64_t timeStampNow, uint64_t timeStampOutput)
{
	std::vector<ClientDisplayViewInterface *> cdvFlushList;
	this->_outputManager->GenerateFlushListForDisplay(displayID, cdvFlushList);
	
	const size_t listSize = cdvFlushList.size();
	if (listSize > 0)
	{
		this->FlushMultipleViews(cdvFlushList, timeStampNow, timeStampOutput);
	}
}

void ClientGPUFetchObjectMultiDisplayView::FlushMultipleViews(const std::vector<ClientDisplayViewInterface *> &cdvFlushList, uint64_t timeStampNow, uint64_t timeStampOutput)
{
	const size_t listSize = cdvFlushList.size();
	
	for (size_t i = 0; i < listSize; i++)
	{
		ClientDisplayViewInterface *cdv = cdvFlushList[i];
		cdv->FlushView(NULL);
	}
	
	for (size_t i = 0; i < listSize; i++)
	{
		ClientDisplayViewInterface *cdv = cdvFlushList[i];
		cdv->FinalizeFlush(NULL, timeStampOutput);
	}
}

void ClientGPUFetchObjectMultiDisplayView::FinalizeAllViews()
{
	std::vector<ClientDisplayViewInterface *> cdvList;
	this->_outputManager->GenerateViewListAll(cdvList);
	
	const size_t listSize = cdvList.size();
	for (size_t i = 0; i < listSize; i++)
	{
		ClientDisplayViewInterface *cdv = cdvList[i];
		cdv->FinalizeFlush(NULL, 0);
	}
}
