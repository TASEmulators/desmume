/*
	Copyright (C) 2025-2026 DeSmuME team

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
#include "userinterface/MacMetalVideoCapture.h"

#include "../../GPU.h"
#include "../../common.h"
#include "../../utils/colorspacehandler/colorspacehandler.h"

#include "cocoa_globals.h"

#include <stdio.h>

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
	_avCaptureObject = NULL;
	_targetWidth = 0;
	_targetHeight = 0;
	_mode = ClientDisplayMode_Dual;
	_layout = ClientDisplayLayout_Vertical;
	_order = ClientDisplayOrder_MainFirst;
	_gapScale = 0.0;
	_rotation = 0.0;
	_useDeposterize = false;
	_outputFilterID = OutputFilterTypeID_Bilinear;
	_pixelScalerID = VideoFilterTypeID_None;
	_gpuCapture = NULL;
	_gpuCaptureDisabled = false;
	_gpuDroppedFrameCount = 0;
	_gpuFrameIndex = 0;
	
	pthread_mutex_init(&_mutexCaptureBuffer, NULL);
}

ClientVideoCaptureOutput::~ClientVideoCaptureOutput()
{
	// Tear the GPU presenter down first: SetActiveVideoCaptureOutput(NULL) has
	// already guaranteed that no frame is in flight, so this cannot race with
	// ProcessFrame().
	MacMetalVideoCaptureDestroy(this->_gpuCapture);
	this->_gpuCapture = NULL;
	
	pthread_mutex_destroy(&this->_mutexCaptureBuffer);
}

void ClientVideoCaptureOutput::SetProperties(size_t width, size_t height,
											 ClientDisplayMode mode, ClientDisplayLayout layout,
											 ClientDisplayOrder order, double gapScale, double rotation,
											 bool useDeposterize, OutputFilterTypeID outputFilterID,
											 VideoFilterTypeID pixelScalerID)
{
	pthread_mutex_lock(&this->_mutexCaptureBuffer);
	this->_targetWidth = width;
	this->_targetHeight = height;
	this->_mode = mode;
	this->_layout = layout;
	this->_order = order;
	this->_gapScale = gapScale;
	this->_rotation = rotation;
	this->_useDeposterize = useDeposterize;
	this->_outputFilterID = outputFilterID;
	this->_pixelScalerID = pixelScalerID;
	pthread_mutex_unlock(&this->_mutexCaptureBuffer);
	
	// Echo the geometry the recording will use.  If a screen is black or the
	// two screens look swapped, this line says immediately whether the
	// requested mode/layout/order actually reached the capture path.
	printf("DeSmuME[AVCapture]: Capture geometry: mode=%d layout=%d order=%d gapScale=%.3f rotation=%.1f deposterize=%d outputFilter=%d scaler=%d target=%zux%zu source=%zux%zu\n",
		   (int)mode, (int)layout, (int)order, gapScale,
		   rotation, (int)useDeposterize, (int)outputFilterID, (int)pixelScalerID,
		   width, height,
		   (size_t)GPU_FRAMEBUFFER_NATIVE_WIDTH, (size_t)GPU_FRAMEBUFFER_NATIVE_HEIGHT);
	fflush(stdout);
}

void ClientVideoCaptureOutput::SetCaptureObject(ClientAVCaptureObject *captureObject)
{
	this->_avCaptureObject = captureObject;
}

ClientAVCaptureObject* ClientVideoCaptureOutput::GetCaptureObject()
{
	return this->_avCaptureObject;
}

// The only capture path.  There is deliberately no CPU compositor: it used to
// hand VideoToolbox a raw RGB888 pointer into a staging buffer, and that
// pointer is what crashed the encoder thread when the buffer was gone
// (vImageConvert_RGB888toBGRA8888 dereferencing NULL + slot offset).  It also
// duplicated the presenter's geometry maths, so a CPU frame and a GPU frame of
// the same settings did not even agree on framing.
//
// A finished frame can only reach the encoder as a CVPixelBufferRef now, which
// carries its own dimensions and lifetime.
void ClientVideoCaptureOutput::ProcessFrame(ClientDisplay3DPresenter *srcPresenter)
{
	if ( (srcPresenter == NULL) || (this->_avCaptureObject == NULL) || this->_gpuCaptureDisabled )
	{
		return;
	}
	
	if (this->_gpuCapture == NULL)
	{
		MacVideoCaptureGeometry geometry;
		
		pthread_mutex_lock(&this->_mutexCaptureBuffer);
		geometry.width          = this->_targetWidth;
		geometry.height         = this->_targetHeight;
		geometry.mode           = this->_mode;
		geometry.layout         = this->_layout;
		geometry.order          = this->_order;
		geometry.gapScale       = this->_gapScale;
		geometry.rotation       = this->_rotation;
		geometry.useDeposterize = this->_useDeposterize;
		geometry.outputFilterID = this->_outputFilterID;
		geometry.pixelScalerID  = this->_pixelScalerID;
		pthread_mutex_unlock(&this->_mutexCaptureBuffer);
		
		// Built here, on the display thread, on the first captured frame: a
		// presenter created on the UI thread would retain the shared fetch
		// textures while the fetch object is replacing them, which is what the
		// earlier second-presenter design crashed on.
		void *sharedData = MacMetalPresenterGetSharedData(srcPresenter);
		if ( (sharedData == NULL) || (geometry.width == 0) || (geometry.height == 0) )
		{
			this->_gpuCaptureDisabled = true;
			
			printf("DeSmuME[AVCapture]: Recording is unavailable: %s.  Recording requires the Metal display path.\n",
				   (sharedData == NULL) ? "the active display is not using Metal" : "the capture size is zero");
			fflush(stdout);
			return;
		}
		
		this->_gpuCapture = MacMetalVideoCaptureCreate(sharedData, geometry);
		if (this->_gpuCapture == NULL)
		{
			this->_gpuCaptureDisabled = true;
			
			printf("DeSmuME[AVCapture]: Recording is unavailable: could not set up the Metal capture presenter.\n");
			fflush(stdout);
			return;
		}
	}
	
	void *pixelBuffer = MacMetalVideoCaptureRenderFrame(this->_gpuCapture);
	if (pixelBuffer == NULL)
	{
		// Still consume a capture tick: the timeline must keep advancing with
		// real time even while frames are being dropped.
		this->_gpuFrameIndex++;
		
		if ( (++this->_gpuDroppedFrameCount <= 3) || ((this->_gpuDroppedFrameCount % 180) == 0) )
		{
			printf("DeSmuME[AVCapture]: Dropped a GPU frame (count=%u); the encoder is behind.\n",
				   this->_gpuDroppedFrameCount);
			fflush(stdout);
		}
		return;
	}
	
	if (!this->_avCaptureObject->QueuePixelBufferFrame(pixelBuffer, this->_gpuFrameIndex))
	{
		// The capture object rejected it (stream closing), so the buffer is
		// still ours to release.
		MacMetalVideoCaptureReleasePixelBuffer(pixelBuffer);
	}
	
	this->_gpuFrameIndex++;
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

// AV capture frames are produced on the display output thread, immediately
// after that thread finishes presenting the main view.  Holding this lock
// while a frame is copied also prevents closeFileStream() from deleting the
// capture output out from under a running frame.
static pthread_mutex_t _mutexActiveVideoCaptureOutput = PTHREAD_MUTEX_INITIALIZER;
static ClientVideoCaptureOutput *_activeVideoCaptureOutput = NULL;
static int32_t _captureFramePending = 0;

void SetActiveVideoCaptureOutput(ClientVideoCaptureOutput *output)
{
	pthread_mutex_lock(&_mutexActiveVideoCaptureOutput);
	_activeVideoCaptureOutput = output;
	__atomic_store_n(&_captureFramePending, 0, __ATOMIC_RELEASE);
	pthread_mutex_unlock(&_mutexActiveVideoCaptureOutput);
}

// Called once per display link tick, on the fetch thread, right before the
// display outputs are signalled.  Whichever display output thread arrives
// first consumes the request, so even with several windows open we record
// exactly one frame per tick instead of one frame per view.
static void RequestVideoCaptureFrame()
{
	__atomic_store_n(&_captureFramePending, 1, __ATOMIC_RELEASE);
}

static void RunActiveVideoCaptureOutput(ClientDisplay3DPresenter *mainPresenter)
{
	if (__atomic_exchange_n(&_captureFramePending, 0, __ATOMIC_ACQ_REL) == 0)
	{
		return;
	}
	
	pthread_mutex_lock(&_mutexActiveVideoCaptureOutput);
	if (_activeVideoCaptureOutput != NULL)
	{
		_activeVideoCaptureOutput->ProcessFrame(mainPresenter);
	}
	pthread_mutex_unlock(&_mutexActiveVideoCaptureOutput);
}

void ClientDisplayViewOutput::HandleReceiveGPUFrame()
{
	this->ClientVideoOutput::HandleReceiveGPUFrame();
	
	ClientDisplay3DPresenter *mainPresenter = this->_cdv->Get3DPresenter();
	mainPresenter->LoadDisplays();
	mainPresenter->ProcessDisplays();
	
	// The capture consumer only reads GetEmuDisplayInfo() from the main
	// presenter (plain CPU framebuffer memory).  No second presenter, no
	// CopyFrameToBuffer, no extra Metal command buffer or semaphore signal.
	RunActiveVideoCaptureOutput(mainPresenter);
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
	RequestVideoCaptureFrame();
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
