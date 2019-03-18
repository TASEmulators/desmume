/*
	Copyright (C) 2017-2018 DeSmuME team
 
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

#include "ClientDisplayView.h"
#include "../../common.h"

#include <sstream>

ClientDisplayPresenter::ClientDisplayPresenter()
{
	_fetchObject = NULL;
	
	ClientDisplayPresenterProperties defaultProperty;
	defaultProperty.normalWidth		= GPU_FRAMEBUFFER_NATIVE_WIDTH;
	defaultProperty.normalHeight	= GPU_FRAMEBUFFER_NATIVE_HEIGHT * 2.0;
	defaultProperty.clientWidth		= defaultProperty.normalWidth;
	defaultProperty.clientHeight	= defaultProperty.normalHeight;
	defaultProperty.rotation		= 0.0;
	defaultProperty.viewScale		= 1.0;
	defaultProperty.gapScale		= 0.0;
	defaultProperty.gapDistance		= DS_DISPLAY_UNSCALED_GAP * defaultProperty.gapScale;
	defaultProperty.mode			= ClientDisplayMode_Dual;
	defaultProperty.layout			= ClientDisplayLayout_Vertical;
	defaultProperty.order			= ClientDisplayOrder_MainFirst;
	
	__InstanceInit(defaultProperty);
}

ClientDisplayPresenter::ClientDisplayPresenter(const ClientDisplayPresenterProperties &props)
{
	__InstanceInit(props);
}

ClientDisplayPresenter::~ClientDisplayPresenter()
{
	delete this->_vf[NDSDisplayID_Main];
	delete this->_vf[NDSDisplayID_Touch];
	free_aligned(this->_vfMasterDstBuffer);
	this->_vfMasterDstBufferSize = 0;
	
	if (this->_ftLibrary != NULL)
	{
		FT_Done_FreeType(this->_ftLibrary);
		this->_ftLibrary = NULL;
	}
	
	pthread_mutex_destroy(&this->_mutexHUDString);
}

void ClientDisplayPresenter::__InstanceInit(const ClientDisplayPresenterProperties &props)
{
	_stagedProperty = props;
	_renderProperty = _stagedProperty;
	
	_useDeposterize = false;
	_pixelScaler = VideoFilterTypeID_None;
	_outputFilter = OutputFilterTypeID_Bilinear;
	
	_displaySourceSelect[NDSDisplayID_Main]  = ClientDisplaySource_DeterminedByNDS;
	_displaySourceSelect[NDSDisplayID_Touch] = ClientDisplaySource_DeterminedByNDS;
	_isSelectedDisplayEnabled[NDSDisplayID_Main]  = false;
	_isSelectedDisplayEnabled[NDSDisplayID_Touch] = false;
	_selectedSourceForDisplay[NDSDisplayID_Main]  = NDSDisplayID_Main;
	_selectedSourceForDisplay[NDSDisplayID_Touch] = NDSDisplayID_Touch;
	
	_scaleFactor = 1.0;
	
	_hudObjectScale = 1.0;
	_isHUDVisible = false;
	_showExecutionSpeed = false;
	_showVideoFPS = true;
	_showRender3DFPS = false;
	_showFrameIndex = false;
	_showLagFrameCount = false;
	_showCPULoadAverage = false;
	_showRTC = false;
	_showInputs = false;
	
	_hudColorExecutionSpeed = LE_TO_LOCAL_32(0xFFFFFFFF);
	_hudColorVideoFPS = LE_TO_LOCAL_32(0xFFFFFFFF);
	_hudColorRender3DFPS = LE_TO_LOCAL_32(0xFFFFFFFF);
	_hudColorFrameIndex = LE_TO_LOCAL_32(0xFFFFFFFF);
	_hudColorLagFrameCount = LE_TO_LOCAL_32(0xFFFFFFFF);
	_hudColorCPULoadAverage = LE_TO_LOCAL_32(0xFFFFFFFF);
	_hudColorRTC = LE_TO_LOCAL_32(0xFFFFFFFF);
	_hudColorInputAppliedAndPending = LE_TO_LOCAL_32(0xFFFFFFFF);
	_hudColorInputAppliedOnly = LE_TO_LOCAL_32(0xFF3030FF);
	_hudColorInputPendingOnly = LE_TO_LOCAL_32(0xFF00C000);
	
	_clientFrameInfo.videoFPS = 0;
	_ndsFrameInfo.clear();
	
	memset(&_emuDisplayInfo, 0, sizeof(_emuDisplayInfo));
	_hudString = "\x01"; // Char value 0x01 will represent the "text box" character, which will always be first in the string.
	_outHudString = _hudString;
	_hudInputString = "<^>vABXYLRSsgf x:000 y:000";
	_hudNeedsUpdate = true;
	
	FT_Error error = FT_Init_FreeType(&_ftLibrary);
	if (error)
	{
		printf("ClientDisplayPresenter: FreeType failed to init!\n");
	}
	
	memset(_glyphInfo, 0, sizeof(_glyphInfo));
	_glyphTileSize = ((double)HUD_TEXTBOX_BASEGLYPHSIZE * _scaleFactor) + 0.0001;
	_glyphSize = ((double)_glyphTileSize * 0.75) + 0.0001;
	
	// Set up the text box, which resides at glyph position 1.
	GlyphInfo &boxInfo = _glyphInfo[1];
	boxInfo.width = _glyphTileSize;
	boxInfo.texCoord[0] = 1.0f/16.0f;		boxInfo.texCoord[1] = 0.0f;
	boxInfo.texCoord[2] = 2.0f/16.0f;		boxInfo.texCoord[3] = 0.0f;
	boxInfo.texCoord[4] = 2.0f/16.0f;		boxInfo.texCoord[5] = 1.0f/16.0f;
	boxInfo.texCoord[6] = 1.0f/16.0f;		boxInfo.texCoord[7] = 1.0f/16.0f;
	
	// Set up the CPU pixel scaler objects.
	_vfMasterDstBufferSize = GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * 2 * sizeof(uint32_t);
	_vfMasterDstBuffer = (uint32_t *)malloc_alignedPage(_vfMasterDstBufferSize);
	memset(_vfMasterDstBuffer, 0, _vfMasterDstBufferSize);
	
	_vf[NDSDisplayID_Main]  = new VideoFilter(GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT, VideoFilterTypeID_None, 0);
	_vf[NDSDisplayID_Touch] = new VideoFilter(GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT, VideoFilterTypeID_None, 0);
	_vf[NDSDisplayID_Main]->SetDstBufferPtr(_vfMasterDstBuffer);
	_vf[NDSDisplayID_Touch]->SetDstBufferPtr(_vfMasterDstBuffer + (_vf[NDSDisplayID_Main]->GetDstWidth() * _vf[NDSDisplayID_Main]->GetDstHeight()));
	
	pthread_mutex_init(&_mutexHUDString, NULL);
}

void ClientDisplayPresenter::Init()
{
	// Do nothing. This is implementation dependent.
}

void ClientDisplayPresenter::_UpdateHUDString()
{
	std::ostringstream ss;
	ss << "\x01"; // This represents the text box. It must always be the first character.
	
	if (this->_showExecutionSpeed)
	{
		if (this->_ndsFrameInfo.executionSpeed < 0.0001)
		{
			ss << "Execution Speed: -----%\n";
		}
		else
		{
			char buffer[48];
			memset(buffer, 0, sizeof(buffer));
			snprintf(buffer, 47, "Execution Speed: %3.01f%%\n", this->_ndsFrameInfo.executionSpeed);
			
			ss << buffer;
		}
	}
	
	if (this->_showVideoFPS)
	{
		ss << "Video FPS: " << this->_clientFrameInfo.videoFPS << "\n";
	}
	
	if (this->_showRender3DFPS)
	{
		ss << "3D Rendering FPS: " << this->_ndsFrameInfo.render3DFPS << "\n";
	}
	
	if (this->_showFrameIndex)
	{
		ss << "Frame Index: " << this->_ndsFrameInfo.frameIndex << "\n";
	}
	
	if (this->_showLagFrameCount)
	{
		ss << "Lag Frame Count: " << this->_ndsFrameInfo.lagFrameCount << "\n";
	}
	
	if (this->_showCPULoadAverage)
	{
		char buffer[32];
		memset(buffer, 0, sizeof(buffer));
		snprintf(buffer, 25, "CPU Load Avg: %02d%% / %02d%%\n", this->_ndsFrameInfo.cpuLoadAvgARM9, this->_ndsFrameInfo.cpuLoadAvgARM7);
		
		ss << buffer;
	}
	
	if (this->_showRTC)
	{
		ss << "RTC: " << this->_ndsFrameInfo.rtcString << "\n";
	}
	
	if (this->_showInputs)
	{
		if ( (this->_ndsFrameInfo.inputStatesApplied.Touch == 0) || (this->_ndsFrameInfo.inputStatesPending.Touch == 0) )
		{
			char inputBuffer[HUD_INPUT_ELEMENT_LENGTH + 1];
			uint8_t touchLocX = this->_ndsFrameInfo.touchLocXApplied;
			uint8_t touchLocY = this->_ndsFrameInfo.touchLocYApplied;
			
			if ( (this->_ndsFrameInfo.inputStatesApplied.Touch != 0) && (this->_ndsFrameInfo.inputStatesPending.Touch == 0) )
			{
				touchLocX = this->_ndsFrameInfo.touchLocXPending;
				touchLocY = this->_ndsFrameInfo.touchLocYPending;
			}
			
			snprintf(inputBuffer, HUD_INPUT_ELEMENT_LENGTH + 1, "<^>vABXYLRSsgf x:%.3u y:%.3u", touchLocX, touchLocY);
			this->_hudInputString = inputBuffer;
		}
	}
	
	pthread_mutex_lock(&this->_mutexHUDString);
	this->_hudString = ss.str();
	this->_hudNeedsUpdate = true;
	pthread_mutex_unlock(&this->_mutexHUDString);
}

void ClientDisplayPresenter::_SetHUDShowInfoItem(bool &infoItemFlag, const bool visibleState)
{
	if (infoItemFlag == visibleState)
	{
		return;
	}
	
	infoItemFlag = visibleState;
	this->_UpdateHUDString();
}

double ClientDisplayPresenter::GetScaleFactor() const
{
	return this->_scaleFactor;
}

void ClientDisplayPresenter::SetScaleFactor(const double scaleFactor)
{
	const bool willChangeScaleFactor = (this->_scaleFactor != scaleFactor);
	this->_scaleFactor = scaleFactor;
	
	if (willChangeScaleFactor)
	{
		this->_glyphTileSize = (double)HUD_TEXTBOX_BASEGLYPHSIZE * scaleFactor;
		this->_glyphSize = (double)this->_glyphTileSize * 0.75;
		
		this->LoadHUDFont();
	}
}

void ClientDisplayPresenter::_UpdateClientSize()
{
	pthread_mutex_lock(&this->_mutexHUDString);
	this->_hudNeedsUpdate = true;
	pthread_mutex_unlock(&this->_mutexHUDString);
}

void ClientDisplayPresenter::_UpdateViewScale()
{
	double checkWidth = this->_renderProperty.normalWidth;
	double checkHeight = this->_renderProperty.normalHeight;
	ClientDisplayPresenter::ConvertNormalToTransformedBounds(1.0, this->_renderProperty.rotation, checkWidth, checkHeight);
	this->_renderProperty.viewScale = ClientDisplayPresenter::GetMaxScalarWithinBounds(checkWidth, checkHeight, this->_renderProperty.clientWidth, this->_renderProperty.clientHeight);
	
	const double logicalClientWidth = this->_renderProperty.clientWidth / this->_scaleFactor;
	
	this->_hudObjectScale = logicalClientWidth / this->_renderProperty.normalWidth;
	if (this->_hudObjectScale > 1.74939175)
	{
		// If the view scale is <= 1.74939175, we scale the HUD objects linearly. Otherwise, we scale
		// the HUD objects logarithmically, up to a maximum scale of 3.0.
		this->_hudObjectScale = (-12000.0 * pow(this->_hudObjectScale+4.5075, -5.0)) + 3.0;
	}
	
	this->_hudObjectScale *= this->_scaleFactor;
}

// NDS screen layout
const ClientDisplayPresenterProperties& ClientDisplayPresenter::GetPresenterProperties() const
{
	return this->_renderProperty;
}

void ClientDisplayPresenter::CommitPresenterProperties(const ClientDisplayPresenterProperties &props)
{
	this->_stagedProperty = props;
}

void ClientDisplayPresenter::SetupPresenterProperties()
{
	// Validate the staged properties.
	this->_stagedProperty.gapDistance = (double)DS_DISPLAY_UNSCALED_GAP * this->_stagedProperty.gapScale;
	ClientDisplayPresenter::CalculateNormalSize(this->_stagedProperty.mode, this->_stagedProperty.layout, this->_stagedProperty.gapScale, this->_stagedProperty.normalWidth, this->_stagedProperty.normalHeight);
	
	const bool didNormalSizeChange = (this->_renderProperty.mode != this->_stagedProperty.mode) ||
	                                 (this->_renderProperty.layout != this->_stagedProperty.layout) ||
	                                 (this->_renderProperty.gapScale != this->_stagedProperty.gapScale);
	
	const bool didOrderChange = (this->_renderProperty.order != this->_stagedProperty.order);
	const bool didRotationChange = (this->_renderProperty.rotation != this->_stagedProperty.rotation);
	const bool didClientSizeChange = (this->_renderProperty.clientWidth != this->_stagedProperty.clientWidth) || (this->_renderProperty.clientHeight != this->_stagedProperty.clientHeight);
	
	// Copy the staged properties to the current rendering properties.
	this->_renderProperty = this->_stagedProperty;
	
	// Update internal states based on the new render properties
	this->_UpdateViewScale();
	
	if (didNormalSizeChange)
	{
		this->_UpdateNormalSize();
	}
	
	if (didOrderChange)
	{
		this->_UpdateOrder();
		
		if (this->_showInputs)
		{
			pthread_mutex_lock(&this->_mutexHUDString);
			this->_hudNeedsUpdate = true;
			pthread_mutex_unlock(&this->_mutexHUDString);
		}
	}
	
	if (didRotationChange)
	{
		this->_UpdateRotation();
	}
	
	if (didClientSizeChange)
	{
		this->_UpdateClientSize();
	}
	
	this->UpdateLayout();
}

double ClientDisplayPresenter::GetRotation() const
{
	return this->_renderProperty.rotation;
}

double ClientDisplayPresenter::GetViewScale() const
{
	return this->_renderProperty.viewScale;
}

ClientDisplayMode ClientDisplayPresenter::GetMode() const
{
	return this->_renderProperty.mode;
}

ClientDisplayLayout ClientDisplayPresenter::GetLayout() const
{
	return this->_renderProperty.layout;
}

ClientDisplayOrder ClientDisplayPresenter::GetOrder() const
{
	return this->_renderProperty.order;
}

double ClientDisplayPresenter::GetGapScale() const
{
	return this->_renderProperty.gapScale;
}

double ClientDisplayPresenter::GetGapDistance() const
{
	return this->_renderProperty.gapDistance;
}

ClientDisplaySource ClientDisplayPresenter::GetDisplayVideoSource(const NDSDisplayID displayID) const
{
	return this->_displaySourceSelect[displayID];
}

void ClientDisplayPresenter::SetDisplayVideoSource(const NDSDisplayID displayID, ClientDisplaySource displaySrc)
{
	this->_displaySourceSelect[displayID] = displaySrc;
}

NDSDisplayID ClientDisplayPresenter::GetSelectedDisplaySourceForDisplay(const NDSDisplayID displayID) const
{
	return this->_selectedSourceForDisplay[displayID];
}

bool ClientDisplayPresenter::IsSelectedDisplayEnabled(const NDSDisplayID displayID) const
{
	return this->_isSelectedDisplayEnabled[displayID];
}

// NDS screen filters
bool ClientDisplayPresenter::GetSourceDeposterize()
{
	return this->_useDeposterize;
}

void ClientDisplayPresenter::SetSourceDeposterize(const bool useDeposterize)
{
	this->_useDeposterize = useDeposterize;
}

OutputFilterTypeID ClientDisplayPresenter::GetOutputFilter() const
{
	return this->_outputFilter;
}

void ClientDisplayPresenter::SetOutputFilter(const OutputFilterTypeID filterID)
{
	this->_outputFilter = filterID;
}

VideoFilterTypeID ClientDisplayPresenter::GetPixelScaler() const
{
	return this->_pixelScaler;
}

void ClientDisplayPresenter::SetPixelScaler(const VideoFilterTypeID filterID)
{
	std::string cpuTypeIDString = std::string( VideoFilter::GetTypeStringByID(filterID) );
	const VideoFilterTypeID newFilterID = (cpuTypeIDString != std::string(VIDEOFILTERTYPE_UNKNOWN_STRING)) ? filterID : VideoFilterTypeID_None;
	
	const VideoFilterAttributes newFilterAttr = VideoFilter::GetAttributesByID(newFilterID);
	const size_t oldDstBufferWidth  =  this->_vf[NDSDisplayID_Main]->GetDstWidth()  + this->_vf[NDSDisplayID_Touch]->GetDstWidth();
	const size_t oldDstBufferHeight =  this->_vf[NDSDisplayID_Main]->GetDstHeight() + this->_vf[NDSDisplayID_Touch]->GetDstHeight();
	const size_t newDstBufferWidth  = (this->_vf[NDSDisplayID_Main]->GetSrcWidth()  + this->_vf[NDSDisplayID_Touch]->GetSrcWidth())  * newFilterAttr.scaleMultiply / newFilterAttr.scaleDivide;
	const size_t newDstBufferHeight = (this->_vf[NDSDisplayID_Main]->GetSrcHeight() + this->_vf[NDSDisplayID_Touch]->GetSrcHeight()) * newFilterAttr.scaleMultiply / newFilterAttr.scaleDivide;
	
	uint32_t *oldMasterBuffer = NULL;
	
	if ( (oldDstBufferWidth != newDstBufferWidth) || (oldDstBufferHeight != newDstBufferHeight) )
	{
		oldMasterBuffer = this->_vfMasterDstBuffer;
		this->_ResizeCPUPixelScaler(newFilterID);
	}
	
	this->_vf[NDSDisplayID_Main]->ChangeFilterByID(newFilterID);
	this->_vf[NDSDisplayID_Touch]->ChangeFilterByID(newFilterID);
	
	this->_pixelScaler = newFilterID;
	
	free_aligned(oldMasterBuffer);
}

VideoFilter* ClientDisplayPresenter::GetPixelScalerObject(const NDSDisplayID displayID)
{
	return this->_vf[displayID];
}

// HUD appearance
const char* ClientDisplayPresenter::GetHUDFontPath() const
{
	return this->_lastFontFilePath;
}

void ClientDisplayPresenter::SetHUDFontPath(const char *filePath)
{
	this->_lastFontFilePath = filePath;
}

void ClientDisplayPresenter::LoadHUDFont()
{
	if (this->_lastFontFilePath == NULL)
	{
		return;
	}
	
	FT_Face fontFace;
	FT_Error error = FT_Err_Ok;
	
	error = FT_New_Face(this->_ftLibrary, this->_lastFontFilePath, 0, &fontFace);
	if (error == FT_Err_Unknown_File_Format)
	{
		printf("ClientDisplayPresenter: FreeType failed to load font face because it is in an unknown format from:\n%s\n", this->_lastFontFilePath);
		return;
	}
	else if (error)
	{
		printf("ClientDisplayPresenter: FreeType failed to load font face with an unknown error from:\n%s\n", this->_lastFontFilePath);
		return;
	}
	
	this->CopyHUDFont(fontFace, this->_glyphSize, this->_glyphTileSize, this->_glyphInfo);
	
	FT_Done_Face(fontFace);
}

void ClientDisplayPresenter::CopyHUDFont(const FT_Face &fontFace, const size_t glyphSize, const size_t glyphTileSize, GlyphInfo *glyphInfo)
{
	// Do nothing. This is implementation dependent.
}

void ClientDisplayPresenter::SetHUDInfo(const ClientFrameInfo &clientFrameInfo, const NDSFrameInfo &ndsFrameInfo)
{
	this->_clientFrameInfo.videoFPS = clientFrameInfo.videoFPS;
	this->_ndsFrameInfo.copyFrom(ndsFrameInfo);
	
	this->_UpdateHUDString();
}

const std::string& ClientDisplayPresenter::GetHUDString()
{
	pthread_mutex_lock(&this->_mutexHUDString);
	this->_outHudString = this->_hudString;
	pthread_mutex_unlock(&this->_mutexHUDString);
	
	return this->_outHudString;
}

float ClientDisplayPresenter::GetHUDObjectScale() const
{
	return this->_hudObjectScale;
}

void ClientDisplayPresenter::SetHUDObjectScale(float objectScale)
{
	this->_hudObjectScale = objectScale;
}

bool ClientDisplayPresenter::GetHUDVisibility() const
{
	return this->_isHUDVisible;
}

void ClientDisplayPresenter::SetHUDVisibility(const bool visibleState)
{
	this->_isHUDVisible = visibleState;
	this->UpdateLayout();
}

bool ClientDisplayPresenter::GetHUDShowExecutionSpeed() const
{
	return this->_showExecutionSpeed;
}

void ClientDisplayPresenter::SetHUDShowExecutionSpeed(const bool visibleState)
{
	this->_SetHUDShowInfoItem(this->_showExecutionSpeed, visibleState);
	this->UpdateLayout();
}

bool ClientDisplayPresenter::GetHUDShowVideoFPS() const
{
	return this->_showVideoFPS;
}

void ClientDisplayPresenter::SetHUDShowVideoFPS(const bool visibleState)
{
	this->_SetHUDShowInfoItem(this->_showVideoFPS, visibleState);
	this->UpdateLayout();
}

bool ClientDisplayPresenter::GetHUDShowRender3DFPS() const
{
	return this->_showRender3DFPS;
}

void ClientDisplayPresenter::SetHUDShowRender3DFPS(const bool visibleState)
{
	this->_SetHUDShowInfoItem(this->_showRender3DFPS, visibleState);
	this->UpdateLayout();
}

bool ClientDisplayPresenter::GetHUDShowFrameIndex() const
{
	return this->_showFrameIndex;
}

void ClientDisplayPresenter::SetHUDShowFrameIndex(const bool visibleState)
{
	this->_SetHUDShowInfoItem(this->_showFrameIndex, visibleState);
	this->UpdateLayout();
}

bool ClientDisplayPresenter::GetHUDShowLagFrameCount() const
{
	return this->_showLagFrameCount;
}

void ClientDisplayPresenter::SetHUDShowLagFrameCount(const bool visibleState)
{
	this->_SetHUDShowInfoItem(this->_showLagFrameCount, visibleState);
	this->UpdateLayout();
}

bool ClientDisplayPresenter::GetHUDShowCPULoadAverage() const
{
	return this->_showCPULoadAverage;
}

void ClientDisplayPresenter::SetHUDShowCPULoadAverage(const bool visibleState)
{
	this->_SetHUDShowInfoItem(this->_showCPULoadAverage, visibleState);
	this->UpdateLayout();
}

bool ClientDisplayPresenter::GetHUDShowRTC() const
{
	return this->_showRTC;
}

void ClientDisplayPresenter::SetHUDShowRTC(const bool visibleState)
{
	this->_SetHUDShowInfoItem(this->_showRTC, visibleState);
	this->UpdateLayout();
}

bool ClientDisplayPresenter::GetHUDShowInput() const
{
	return this->_showInputs;
}

void ClientDisplayPresenter::SetHUDShowInput(const bool visibleState)
{
	this->_SetHUDShowInfoItem(this->_showInputs, visibleState);
	this->UpdateLayout();
}

uint32_t ClientDisplayPresenter::GetHUDColorExecutionSpeed() const
{
	return this->_hudColorExecutionSpeed;
}

void ClientDisplayPresenter::SetHUDColorExecutionSpeed(uint32_t color32)
{
	this->_hudColorExecutionSpeed = color32;
	
	pthread_mutex_lock(&this->_mutexHUDString);
	this->_hudNeedsUpdate = true;
	pthread_mutex_unlock(&this->_mutexHUDString);
	
	this->UpdateLayout();
}

uint32_t ClientDisplayPresenter::GetHUDColorVideoFPS() const
{
	return this->_hudColorVideoFPS;
}

void ClientDisplayPresenter::SetHUDColorVideoFPS(uint32_t color32)
{
	this->_hudColorVideoFPS = color32;
	
	pthread_mutex_lock(&this->_mutexHUDString);
	this->_hudNeedsUpdate = true;
	pthread_mutex_unlock(&this->_mutexHUDString);
	
	this->UpdateLayout();
}

uint32_t ClientDisplayPresenter::GetHUDColorRender3DFPS() const
{
	return this->_hudColorRender3DFPS;
}

void ClientDisplayPresenter::SetHUDColorRender3DFPS(uint32_t color32)
{
	this->_hudColorRender3DFPS = color32;
	
	pthread_mutex_lock(&this->_mutexHUDString);
	this->_hudNeedsUpdate = true;
	pthread_mutex_unlock(&this->_mutexHUDString);
	
	this->UpdateLayout();
}

uint32_t ClientDisplayPresenter::GetHUDColorFrameIndex() const
{
	return this->_hudColorFrameIndex;
}

void ClientDisplayPresenter::SetHUDColorFrameIndex(uint32_t color32)
{
	this->_hudColorFrameIndex = color32;
	
	pthread_mutex_lock(&this->_mutexHUDString);
	this->_hudNeedsUpdate = true;
	pthread_mutex_unlock(&this->_mutexHUDString);
	
	this->UpdateLayout();
}

uint32_t ClientDisplayPresenter::GetHUDColorLagFrameCount() const
{
	return this->_hudColorLagFrameCount;
}

void ClientDisplayPresenter::SetHUDColorLagFrameCount(uint32_t color32)
{
	this->_hudColorLagFrameCount = color32;
	
	pthread_mutex_lock(&this->_mutexHUDString);
	this->_hudNeedsUpdate = true;
	pthread_mutex_unlock(&this->_mutexHUDString);
	
	this->UpdateLayout();
}

uint32_t ClientDisplayPresenter::GetHUDColorCPULoadAverage() const
{
	return this->_hudColorCPULoadAverage;
}

void ClientDisplayPresenter::SetHUDColorCPULoadAverage(uint32_t color32)
{
	this->_hudColorCPULoadAverage = color32;
	
	pthread_mutex_lock(&this->_mutexHUDString);
	this->_hudNeedsUpdate = true;
	pthread_mutex_unlock(&this->_mutexHUDString);
	
	this->UpdateLayout();
}

uint32_t ClientDisplayPresenter::GetHUDColorRTC() const
{
	return this->_hudColorRTC;
}

void ClientDisplayPresenter::SetHUDColorRTC(uint32_t color32)
{
	this->_hudColorRTC = color32;
	
	pthread_mutex_lock(&this->_mutexHUDString);
	this->_hudNeedsUpdate = true;
	pthread_mutex_unlock(&this->_mutexHUDString);
	
	this->UpdateLayout();
}

uint32_t ClientDisplayPresenter::GetHUDColorInputPendingAndApplied() const
{
	return this->_hudColorInputAppliedAndPending;
}

void ClientDisplayPresenter::SetHUDColorInputPendingAndApplied(uint32_t color32)
{
	this->_hudColorInputAppliedAndPending = color32;
	
	pthread_mutex_lock(&this->_mutexHUDString);
	this->_hudNeedsUpdate = true;
	pthread_mutex_unlock(&this->_mutexHUDString);
	
	this->UpdateLayout();
}

uint32_t ClientDisplayPresenter::GetHUDColorInputAppliedOnly() const
{
	return this->_hudColorInputAppliedOnly;
}

void ClientDisplayPresenter::SetHUDColorInputAppliedOnly(uint32_t color32)
{
	this->_hudColorInputAppliedOnly = color32;
	
	pthread_mutex_lock(&this->_mutexHUDString);
	this->_hudNeedsUpdate = true;
	pthread_mutex_unlock(&this->_mutexHUDString);
	
	this->UpdateLayout();
}

uint32_t ClientDisplayPresenter::GetHUDColorInputPendingOnly() const
{
	return this->_hudColorInputPendingOnly;
}

void ClientDisplayPresenter::SetHUDColorInputPendingOnly(uint32_t color32)
{
	this->_hudColorInputPendingOnly = color32;
	
	pthread_mutex_lock(&this->_mutexHUDString);
	this->_hudNeedsUpdate = true;
	pthread_mutex_unlock(&this->_mutexHUDString);
	
	this->UpdateLayout();
}

uint32_t ClientDisplayPresenter::GetInputColorUsingStates(bool pendingState, bool appliedState)
{
	uint32_t color = LE_TO_LOCAL_32(0x80808080);
	
	if (pendingState && appliedState)
	{
		color = this->_hudColorInputAppliedAndPending;
	}
	else if (appliedState)
	{
		color = this->_hudColorInputAppliedOnly;
	}
	else if (pendingState)
	{
		color = this->_hudColorInputPendingOnly;
	}
	
	return color;
}

bool ClientDisplayPresenter::HUDNeedsUpdate()
{
	pthread_mutex_lock(&this->_mutexHUDString);
	const bool needsUpdate = this->_hudNeedsUpdate;
	pthread_mutex_unlock(&this->_mutexHUDString);
	
	return needsUpdate;
}

void ClientDisplayPresenter::ClearHUDNeedsUpdate()
{
	pthread_mutex_lock(&this->_mutexHUDString);
	this->_hudNeedsUpdate = false;
	pthread_mutex_unlock(&this->_mutexHUDString);
}

// NDS GPU Interface
const GPUClientFetchObject& ClientDisplayPresenter::GetFetchObject() const
{
	return *this->_fetchObject;
}

void ClientDisplayPresenter::SetFetchObject(GPUClientFetchObject *fetchObject)
{
	this->_fetchObject = fetchObject;
}

void ClientDisplayPresenter::_LoadNativeDisplayByID(const NDSDisplayID displayID)
{
	// Do nothing. This is implementation dependent.
}

void ClientDisplayPresenter::_LoadCustomDisplayByID(const NDSDisplayID displayID)
{
	// Do nothing. This is implementation dependent.
}

void ClientDisplayPresenter::LoadDisplays()
{
	this->_emuDisplayInfo = this->_fetchObject->GetFetchDisplayInfoForBufferIndex(this->_fetchObject->GetLastFetchIndex());
	
	this->_selectedSourceForDisplay[NDSDisplayID_Main]  = NDSDisplayID_Main;
	this->_selectedSourceForDisplay[NDSDisplayID_Touch] = NDSDisplayID_Touch;
	this->_isSelectedDisplayEnabled[NDSDisplayID_Main]  = false;
	this->_isSelectedDisplayEnabled[NDSDisplayID_Touch] = false;
	
	// Select the display source
	const ClientDisplaySource mainDisplaySrc  = this->_displaySourceSelect[NDSDisplayID_Main];
	const ClientDisplaySource touchDisplaySrc = this->_displaySourceSelect[NDSDisplayID_Touch];
	
	switch (mainDisplaySrc)
	{
		case ClientDisplaySource_DeterminedByNDS:
			this->_selectedSourceForDisplay[NDSDisplayID_Main] = NDSDisplayID_Main;
			this->_isSelectedDisplayEnabled[NDSDisplayID_Main] = this->_emuDisplayInfo.isDisplayEnabled[NDSDisplayID_Main];
			break;
			
		case ClientDisplaySource_EngineMain:
		{
			if (this->_emuDisplayInfo.engineID[NDSDisplayID_Main] == GPUEngineID_Main)
			{
				this->_selectedSourceForDisplay[NDSDisplayID_Main] = NDSDisplayID_Main;
				this->_isSelectedDisplayEnabled[NDSDisplayID_Main] = this->_emuDisplayInfo.isDisplayEnabled[NDSDisplayID_Main];
			}
			else
			{
				this->_selectedSourceForDisplay[NDSDisplayID_Main] = NDSDisplayID_Touch;
				this->_isSelectedDisplayEnabled[NDSDisplayID_Main] = this->_emuDisplayInfo.isDisplayEnabled[NDSDisplayID_Touch];
			}
			break;
		}
			
		case ClientDisplaySource_EngineSub:
		{
			if (this->_emuDisplayInfo.engineID[NDSDisplayID_Main] == GPUEngineID_Sub)
			{
				this->_selectedSourceForDisplay[NDSDisplayID_Main] = NDSDisplayID_Main;
				this->_isSelectedDisplayEnabled[NDSDisplayID_Main] = this->_emuDisplayInfo.isDisplayEnabled[NDSDisplayID_Main];
			}
			else
			{
				this->_selectedSourceForDisplay[NDSDisplayID_Main] = NDSDisplayID_Touch;
				this->_isSelectedDisplayEnabled[NDSDisplayID_Main] = this->_emuDisplayInfo.isDisplayEnabled[NDSDisplayID_Touch];
			}
			break;
		}
			
		case ClientDisplaySource_None:
		default:
			this->_isSelectedDisplayEnabled[NDSDisplayID_Main] = false;
			break;
	}
	
	switch (touchDisplaySrc)
	{
		case ClientDisplaySource_DeterminedByNDS:
			this->_selectedSourceForDisplay[NDSDisplayID_Touch] = NDSDisplayID_Touch;
			this->_isSelectedDisplayEnabled[NDSDisplayID_Touch] = this->_emuDisplayInfo.isDisplayEnabled[NDSDisplayID_Touch];
			break;
			
		case ClientDisplaySource_EngineMain:
		{
			if (this->_emuDisplayInfo.engineID[NDSDisplayID_Touch] == GPUEngineID_Main)
			{
				this->_selectedSourceForDisplay[NDSDisplayID_Touch] = NDSDisplayID_Touch;
				this->_isSelectedDisplayEnabled[NDSDisplayID_Touch] = this->_emuDisplayInfo.isDisplayEnabled[NDSDisplayID_Touch];
			}
			else
			{
				this->_selectedSourceForDisplay[NDSDisplayID_Touch] = NDSDisplayID_Main;
				this->_isSelectedDisplayEnabled[NDSDisplayID_Touch] = this->_emuDisplayInfo.isDisplayEnabled[NDSDisplayID_Main];
			}
			break;
		}
			
		case ClientDisplaySource_EngineSub:
		{
			if (this->_emuDisplayInfo.engineID[NDSDisplayID_Touch] == GPUEngineID_Sub)
			{
				this->_selectedSourceForDisplay[NDSDisplayID_Touch] = NDSDisplayID_Touch;
				this->_isSelectedDisplayEnabled[NDSDisplayID_Touch] = this->_emuDisplayInfo.isDisplayEnabled[NDSDisplayID_Touch];
			}
			else
			{
				this->_selectedSourceForDisplay[NDSDisplayID_Touch] = NDSDisplayID_Main;
				this->_isSelectedDisplayEnabled[NDSDisplayID_Touch] = this->_emuDisplayInfo.isDisplayEnabled[NDSDisplayID_Main];
			}
			break;
		}
			
		case ClientDisplaySource_None:
		default:
			this->_isSelectedDisplayEnabled[NDSDisplayID_Touch] = false;
			break;
	}
	
	const bool loadMainScreen  = this->_isSelectedDisplayEnabled[NDSDisplayID_Main]  && ((this->_renderProperty.mode == ClientDisplayMode_Main)  || (this->_renderProperty.mode == ClientDisplayMode_Dual));
	const bool loadTouchScreen = this->_isSelectedDisplayEnabled[NDSDisplayID_Touch] && ((this->_renderProperty.mode == ClientDisplayMode_Touch) || (this->_renderProperty.mode == ClientDisplayMode_Dual)) && (this->_selectedSourceForDisplay[NDSDisplayID_Main] != this->_selectedSourceForDisplay[NDSDisplayID_Touch]);
	
	if (loadMainScreen)
	{
		if (!this->_emuDisplayInfo.didPerformCustomRender[this->_selectedSourceForDisplay[NDSDisplayID_Main]])
		{
			this->_LoadNativeDisplayByID(this->_selectedSourceForDisplay[NDSDisplayID_Main]);
		}
		else
		{
			this->_LoadCustomDisplayByID(this->_selectedSourceForDisplay[NDSDisplayID_Main]);
		}
	}
	
	if (loadTouchScreen)
	{
		if (!this->_emuDisplayInfo.didPerformCustomRender[this->_selectedSourceForDisplay[NDSDisplayID_Touch]])
		{
			this->_LoadNativeDisplayByID(this->_selectedSourceForDisplay[NDSDisplayID_Touch]);
		}
		else
		{
			this->_LoadCustomDisplayByID(this->_selectedSourceForDisplay[NDSDisplayID_Touch]);
		}
	}
}

void ClientDisplayPresenter::_ResizeCPUPixelScaler(const VideoFilterTypeID filterID)
{
	const VideoFilterAttributes newFilterAttr = VideoFilter::GetAttributesByID(filterID);
	const size_t newDstBufferWidth  = (this->_vf[NDSDisplayID_Main]->GetSrcWidth()  + this->_vf[NDSDisplayID_Touch]->GetSrcWidth())  * newFilterAttr.scaleMultiply / newFilterAttr.scaleDivide;
	const size_t newDstBufferHeight = (this->_vf[NDSDisplayID_Main]->GetSrcHeight() + this->_vf[NDSDisplayID_Touch]->GetSrcHeight()) * newFilterAttr.scaleMultiply / newFilterAttr.scaleDivide;
	
	size_t newMasterBufferSize = newDstBufferWidth * newDstBufferHeight * sizeof(uint32_t);
	uint32_t *newMasterBuffer = (uint32_t *)malloc_alignedPage(newMasterBufferSize);
	memset(newMasterBuffer, 0, newMasterBufferSize);
	
	const size_t newDstBufferSingleWidth  = this->_vf[NDSDisplayID_Main]->GetSrcWidth()  * newFilterAttr.scaleMultiply / newFilterAttr.scaleDivide;
	const size_t newDstBufferSingleHeight = this->_vf[NDSDisplayID_Main]->GetSrcHeight() * newFilterAttr.scaleMultiply / newFilterAttr.scaleDivide;
	
	this->_vf[NDSDisplayID_Main]->SetDstBufferPtr(newMasterBuffer);
	this->_vf[NDSDisplayID_Touch]->SetDstBufferPtr(newMasterBuffer + (newDstBufferSingleWidth * newDstBufferSingleHeight));
	
	this->_vfMasterDstBuffer = newMasterBuffer;
	this->_vfMasterDstBufferSize = newMasterBufferSize;
}

void ClientDisplayPresenter::ProcessDisplays()
{
	// Do nothing. This is implementation dependent.
}

void ClientDisplayPresenter::UpdateLayout()
{
	// Do nothing. This is implementation dependent.
}

void ClientDisplayPresenter::CopyFrameToBuffer(uint32_t *dstBuffer)
{
	// Do nothing. This is implementation dependent.
}

const NDSDisplayInfo& ClientDisplayPresenter::GetEmuDisplayInfo() const
{
	return this->_emuDisplayInfo;
}

void ClientDisplayPresenter::SetEmuDisplayInfo(const NDSDisplayInfo &ndsDisplayInfo)
{
	this->_emuDisplayInfo = ndsDisplayInfo;
}

/********************************************************************************************
	ConvertNormalToTransformedBounds()

	Returns the bounds of a normalized 2D surface using affine transformations.

	Takes:
		scalar - The scalar used to transform the 2D surface.
		angleDegrees - The rotation angle, in degrees, to transform the 2D surface.
 		inoutWidth - The width of the normal 2D surface.
 		inoutHeight - The height of the normal 2D surface.

	Returns:
		The bounds of a normalized 2D surface using affine transformations.

	Details:
		The returned bounds is always a normal rectangle. Ignoring the scaling, the
		returned bounds will always be at its smallest when the angle is at 0, 90, 180,
		or 270 degrees, and largest when the angle is at 45, 135, 225, or 315 degrees.
 ********************************************************************************************/
void ClientDisplayPresenter::ConvertNormalToTransformedBounds(const double scalar,
															  const double angleDegrees,
															  double &inoutWidth, double &inoutHeight)
{
	const double angleRadians = angleDegrees * (M_PI/180.0);
	
	// The points are as follows:
	//
	// (x[3], y[3])    (x[2], y[2])
	//
	//
	//
	// (x[0], y[0])    (x[1], y[1])
	
	// Do our scale and rotate transformations.
#ifdef __ACCELERATE__
	
	// Note that although we only need to calculate 3 points, we include 4 points
	// here because Accelerate prefers 16-byte alignment.
	double x[] = {0.0, inoutWidth, inoutWidth, 0.0};
	double y[] = {0.0, 0.0, inoutHeight, inoutHeight};
	
	cblas_drot(4, x, 1, y, 1, scalar * cos(angleRadians), scalar * sin(angleRadians));
	
#else // Keep a C-version of this transformation for reference purposes.
	
	const double w = scalar * inoutWidth;
	const double h = scalar * inoutHeight;
	const double d = hypot(w, h);
	const double dAngle = atan2(h, w);
	
	const double px = w * cos(angleRadians);
	const double py = w * sin(angleRadians);
	const double qx = d * cos(dAngle + angleRadians);
	const double qy = d * sin(dAngle + angleRadians);
	const double rx = h * cos((M_PI/2.0) + angleRadians);
	const double ry = h * sin((M_PI/2.0) + angleRadians);
	
	const double x[] = {0.0, px, qx, rx};
	const double y[] = {0.0, py, qy, ry};
	
#endif
	
	// Determine the transformed width, which is dependent on the location of
	// the x-coordinate of point (x[2], y[2]).
	if (x[2] > 0.0)
	{
		if (x[2] < x[3])
		{
			inoutWidth = x[3] - x[1];
		}
		else if (x[2] < x[1])
		{
			inoutWidth = x[1] - x[3];
		}
		else
		{
			inoutWidth = x[2];
		}
	}
	else if (x[2] < 0.0)
	{
		if (x[2] > x[3])
		{
			inoutWidth = -(x[3] - x[1]);
		}
		else if (x[2] > x[1])
		{
			inoutWidth = -(x[1] - x[3]);
		}
		else
		{
			inoutWidth = -x[2];
		}
	}
	else
	{
		inoutWidth = fabs(x[1] - x[3]);
	}
	
	// Determine the transformed height, which is dependent on the location of
	// the y-coordinate of point (x[2], y[2]).
	if (y[2] > 0.0)
	{
		if (y[2] < y[3])
		{
			inoutHeight = y[3] - y[1];
		}
		else if (y[2] < y[1])
		{
			inoutHeight = y[1] - y[3];
		}
		else
		{
			inoutHeight = y[2];
		}
	}
	else if (y[2] < 0.0)
	{
		if (y[2] > y[3])
		{
			inoutHeight = -(y[3] - y[1]);
		}
		else if (y[2] > y[1])
		{
			inoutHeight = -(y[1] - y[3]);
		}
		else
		{
			inoutHeight = -y[2];
		}
	}
	else
	{
		inoutHeight = fabs(y[3] - y[1]);
	}
}

/********************************************************************************************
	GetMaxScalarWithinBounds()

	Returns the maximum scalar that a rectangle can grow, while maintaining its aspect
	ratio, within a boundary.

	Takes:
		normalBoundsWidth - The width of the normal 2D surface.
		normalBoundsHeight - The height of the normal 2D surface.
		keepInBoundsWidth - The width of the keep-in 2D surface.
		keepInBoundsHeight - The height of the keep-in 2D surface.

	Returns:
		The maximum scalar that a rectangle can grow, while maintaining its aspect ratio,
		within a boundary.

	Details:
		If keepInBoundsWidth or keepInBoundsHeight are less than or equal to zero, the
		returned scalar will be zero.
 ********************************************************************************************/
double ClientDisplayPresenter::GetMaxScalarWithinBounds(const double normalBoundsWidth, const double normalBoundsHeight,
														const double keepInBoundsWidth, const double keepInBoundsHeight)
{
	const double maxX = (normalBoundsWidth <= 0.0) ? 0.0 : keepInBoundsWidth / normalBoundsWidth;
	const double maxY = (normalBoundsHeight <= 0.0) ? 0.0 : keepInBoundsHeight / normalBoundsHeight;
	
	return (maxX <= maxY) ? maxX : maxY;
}

void ClientDisplayPresenter::CalculateNormalSize(const ClientDisplayMode mode, const ClientDisplayLayout layout, const double gapScale,
												 double &outWidth, double &outHeight)
{
	if (mode == ClientDisplayMode_Dual)
	{
		switch (layout)
		{
			case ClientDisplayLayout_Horizontal:
				outWidth  = (double)GPU_FRAMEBUFFER_NATIVE_WIDTH*2.0;
				outHeight = (double)GPU_FRAMEBUFFER_NATIVE_HEIGHT;
				break;
				
			case ClientDisplayLayout_Hybrid_2_1:
				outWidth  = (double)GPU_FRAMEBUFFER_NATIVE_WIDTH + (128.0);
				outHeight = (double)GPU_FRAMEBUFFER_NATIVE_HEIGHT;
				break;
				
			case ClientDisplayLayout_Hybrid_16_9:
				outWidth  = (double)GPU_FRAMEBUFFER_NATIVE_WIDTH + (64.0 * 4.0 / 3.0);
				outHeight = (double)GPU_FRAMEBUFFER_NATIVE_HEIGHT;
				break;
				
			case ClientDisplayLayout_Hybrid_16_10:
				outWidth  = (double)GPU_FRAMEBUFFER_NATIVE_WIDTH + (51.2);
				outHeight = (double)GPU_FRAMEBUFFER_NATIVE_HEIGHT;
				break;
				
			default: // Default to vertical orientation.
				outWidth  = (double)GPU_FRAMEBUFFER_NATIVE_WIDTH;
				outHeight = (double)GPU_FRAMEBUFFER_NATIVE_HEIGHT*2.0 + (DS_DISPLAY_UNSCALED_GAP*gapScale);
				break;
		}
	}
	else
	{
		outWidth  = (double)GPU_FRAMEBUFFER_NATIVE_WIDTH;
		outHeight = (double)GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	}
}

ClientDisplayViewInterface::ClientDisplayViewInterface()
{
	_displayViewID = 0;
	_useVerticalSync = false;
	_viewNeedsFlush = false;
	_allowViewUpdates = true;
	_allowViewFlushes = true;
	
	_initialTouchInMajorDisplay = new InitialTouchPressMap;
}

ClientDisplayViewInterface::~ClientDisplayViewInterface()
{
	delete this->_initialTouchInMajorDisplay;
	this->_initialTouchInMajorDisplay = NULL;
}

int64_t ClientDisplayViewInterface::GetDisplayViewID()
{
	return this->_displayViewID;
}

void ClientDisplayViewInterface::SetDisplayViewID(int64_t displayViewID)
{
	// This implementation-dependent value will never be used internally.
	this->_displayViewID = displayViewID;
}

bool ClientDisplayViewInterface::GetViewNeedsFlush()
{
	return this->_viewNeedsFlush;
}

void ClientDisplayViewInterface::SetViewNeedsFlush()
{
	// Do nothing. This is implementation dependent.
	this->_viewNeedsFlush = true;
}

bool ClientDisplayViewInterface::GetUseVerticalSync() const
{
	return this->_useVerticalSync;
}

void ClientDisplayViewInterface::SetUseVerticalSync(const bool useVerticalSync)
{
	this->_useVerticalSync = useVerticalSync;
}

bool ClientDisplayViewInterface::GetAllowViewUpdates() const
{
	return this->_allowViewUpdates;
}

void ClientDisplayViewInterface::SetAllowViewUpdates(bool allowUpdates)
{
	this->_allowViewUpdates = allowUpdates;
}

bool ClientDisplayViewInterface::GetAllowViewFlushes() const
{
	return this->_allowViewFlushes;
}

void ClientDisplayViewInterface::SetAllowViewFlushes(bool allowFlushes)
{
	this->_allowViewFlushes = allowFlushes;
}

void ClientDisplayViewInterface::FlushView(void *userData)
{
	// Do nothing. This is implementation dependent.
	this->_viewNeedsFlush = false;
}

void ClientDisplayViewInterface::FinalizeFlush(void *userData, uint64_t outputTime)
{
	// Do nothing. This is implementation dependent.
}

void ClientDisplayViewInterface::FlushAndFinalizeImmediate()
{
	this->FlushView(NULL);
	this->FinalizeFlush(NULL, 0);
}

// Touch screen input handling
void ClientDisplayViewInterface::GetNDSPoint(const ClientDisplayPresenterProperties &props,
											 const double logicalClientWidth, const double logicalClientHeight,
											 const int inputID, const bool isInitialTouchPress,
											 const double clientX, const double clientY,
											 uint8_t &outX, uint8_t &outY) const
{
	double x = clientX;
	double y = clientY;
	double w = props.normalWidth;
	double h = props.normalHeight;
	
	ClientDisplayPresenter::ConvertNormalToTransformedBounds(1.0, props.rotation, w, h);
	const double s = ClientDisplayPresenter::GetMaxScalarWithinBounds(w, h, logicalClientWidth, logicalClientHeight);
	
	ClientDisplayViewInterface::ConvertClientToNormalPoint(props.normalWidth, props.normalHeight,
														   logicalClientWidth, logicalClientHeight,
														   s,
														   props.rotation,
														   x, y);
	
	// Normalize the touch location to the DS.
	if (props.mode == ClientDisplayMode_Dual)
	{
		switch (props.layout)
		{
			case ClientDisplayLayout_Horizontal:
			{
				if (props.order == ClientDisplayOrder_MainFirst)
				{
					x -= GPU_FRAMEBUFFER_NATIVE_WIDTH;
				}
				break;
			}
				
			case ClientDisplayLayout_Hybrid_2_1:
			case ClientDisplayLayout_Hybrid_16_9:
			case ClientDisplayLayout_Hybrid_16_10:
			{
				if (isInitialTouchPress)
				{
					const bool isClickWithinMajorDisplay = (props.order == ClientDisplayOrder_TouchFirst) && (x >= 0.0) && (x < 256.0);
					(*_initialTouchInMajorDisplay)[inputID] = isClickWithinMajorDisplay;
				}
				
				const bool handleClickInMajorDisplay = (*_initialTouchInMajorDisplay)[inputID];
				if (!handleClickInMajorDisplay)
				{
					const double minorDisplayScale = (props.normalWidth - (double)GPU_FRAMEBUFFER_NATIVE_WIDTH) / (double)GPU_FRAMEBUFFER_NATIVE_WIDTH;
					x = (x - GPU_FRAMEBUFFER_NATIVE_WIDTH) / minorDisplayScale;
					y = y / minorDisplayScale;
				}
				break;
			}
				
			default: // Default to vertical orientation.
			{
				if (props.order == ClientDisplayOrder_TouchFirst)
				{
					y -= ((double)GPU_FRAMEBUFFER_NATIVE_HEIGHT + props.gapDistance);
				}
				break;
			}
		}
	}
	
	y = GPU_FRAMEBUFFER_NATIVE_HEIGHT - y;
	
	// Constrain the touch point to the DS dimensions.
	if (x < 0.0)
	{
		x = 0.0;
	}
	else if (x > (GPU_FRAMEBUFFER_NATIVE_WIDTH - 1.0))
	{
		x = (GPU_FRAMEBUFFER_NATIVE_WIDTH - 1.0);
	}
	
	if (y < 0.0)
	{
		y = 0.0;
	}
	else if (y > (GPU_FRAMEBUFFER_NATIVE_HEIGHT - 1.0))
	{
		y = (GPU_FRAMEBUFFER_NATIVE_HEIGHT - 1.0);
	}
	
	outX = (u8)(x + 0.001);
	outY = (u8)(y + 0.001);
}

/********************************************************************************************
	ConvertClientToNormalPoint()

	Returns a normalized point from a point from a 2D transformed surface.

	Takes:
		normalWidth - The width of the normal 2D surface.
		normalHeight - The height of the normal 2D surface.
		clientWidth - The width of the client 2D surface.
		clientHeight - The height of the client 2D surface.
		scalar - The scalar used on the transformed 2D surface.
		angleDegrees - The rotation angle, in degrees, of the transformed 2D surface.
 		inoutX - The X coordinate of a 2D point as it exists on a 2D transformed surface.
 		inoutY - The Y coordinate of a 2D point as it exists on a 2D transformed surface.
	
	Returns:
		A normalized point from a point from a 2D transformed surface.

	Details:
		It may help to call GetMaxScalarWithinBounds() for the scalar parameter.
 ********************************************************************************************/
void ClientDisplayViewInterface::ConvertClientToNormalPoint(const double normalWidth, const double normalHeight,
															const double clientWidth, const double clientHeight,
															const double scalar,
															const double angleDegrees,
															double &inoutX, double &inoutY)
{
	// Get the coordinates of the client point and translate the coordinate
	// system so that the origin becomes the center.
	const double clientX = inoutX - (clientWidth / 2.0);
	const double clientY = inoutY - (clientHeight / 2.0);
	
	// Perform rect-polar conversion.
	
	// Get the radius r with respect to the origin.
	const double r = hypot(clientX, clientY) / scalar;
	
	// Get the angle theta with respect to the origin.
	double theta = 0.0;
	
	if (clientX == 0.0)
	{
		if (clientY > 0.0)
		{
			theta = M_PI / 2.0;
		}
		else if (clientY < 0.0)
		{
			theta = M_PI * 1.5;
		}
	}
	else if (clientX < 0.0)
	{
		theta = M_PI - atan2(clientY, -clientX);
	}
	else if (clientY < 0.0)
	{
		theta = atan2(clientY, clientX) + (M_PI * 2.0);
	}
	else
	{
		theta = atan2(clientY, clientX);
	}
	
	// Get the normalized angle and use it to rotate about the origin.
	// Then do polar-rect conversion and translate back to normal coordinates
	// with a 0 degree rotation.
	const double angleRadians = angleDegrees * (M_PI/180.0);
	const double normalizedAngle = theta - angleRadians;
	inoutX = (r * cos(normalizedAngle)) + (normalWidth / 2.0);
	inoutY = (r * sin(normalizedAngle)) + (normalHeight / 2.0);
}

ClientDisplay3DPresenter::ClientDisplay3DPresenter()
{
	__InstanceInit();
}

ClientDisplay3DPresenter::ClientDisplay3DPresenter(const ClientDisplayPresenterProperties &props) : ClientDisplayPresenter(props)
{
	__InstanceInit();
}

void ClientDisplay3DPresenter::__InstanceInit()
{
	_canFilterOnGPU = false;
	_willFilterOnGPU = false;
	_filtersPreferGPU = false;
}

bool ClientDisplay3DPresenter::CanFilterOnGPU() const
{
	return this->_canFilterOnGPU;
}

bool ClientDisplay3DPresenter::WillFilterOnGPU() const
{
	return this->_willFilterOnGPU;
}

bool ClientDisplay3DPresenter::GetFiltersPreferGPU() const
{
	return this->_filtersPreferGPU;
}

void ClientDisplay3DPresenter::SetFiltersPreferGPU(const bool preferGPU)
{
	this->_filtersPreferGPU = preferGPU;
}

void ClientDisplay3DPresenter::SetSourceDeposterize(bool useDeposterize)
{
	this->_useDeposterize = (this->_canFilterOnGPU) ? useDeposterize : false;
}

void ClientDisplay3DPresenter::SetHUDPositionVertices(float viewportWidth, float viewportHeight, float *vtxPositionBufferPtr)
{
	pthread_mutex_lock(&this->_mutexHUDString);
	std::string hudString = this->_hudString;
	pthread_mutex_unlock(&this->_mutexHUDString);
	
	const char *cString = hudString.c_str();
	const size_t textLength = (hudString.length() <= HUD_TEXT_MAX_CHARACTERS) ? hudString.length() : HUD_TEXT_MAX_CHARACTERS;
	const float charSize = this->_glyphSize;
	const float lineHeight = charSize * 0.8f;
	const float textBoxTextOffset = charSize * 0.25f;
	float charLocX = textBoxTextOffset;
	float charLocY = -textBoxTextOffset - lineHeight;
	float textBoxWidth = 0.0f;
	
	// First, calculate the vertices of the text box.
	// The text box should always be the first character in the string.
	vtxPositionBufferPtr[0] = 0.0f;			vtxPositionBufferPtr[1] = 0.0f;
	vtxPositionBufferPtr[2] = charLocX;		vtxPositionBufferPtr[3] = 0.0f;
	vtxPositionBufferPtr[4] = charLocX;		vtxPositionBufferPtr[5] = -textBoxTextOffset;
	vtxPositionBufferPtr[6] = 0.0f;			vtxPositionBufferPtr[7] = -textBoxTextOffset;
	
	// Calculate the vertices of the remaining characters in the string.
	size_t i = 1;
	size_t j = 8;
	
	for (; i < textLength; i++, j+=8)
	{
		const char c = cString[i];
		
		if (c == '\n')
		{
			if (charLocX > textBoxWidth)
			{
				textBoxWidth = charLocX;
			}
			
			vtxPositionBufferPtr[5] -= lineHeight;
			vtxPositionBufferPtr[7] -= lineHeight;
			
			charLocX = textBoxTextOffset;
			charLocY -= lineHeight;
			continue;
		}
		
		const float charWidth = this->_glyphInfo[c].width * charSize / (float)this->_glyphTileSize;
		
		vtxPositionBufferPtr[j+0] = charLocX;					vtxPositionBufferPtr[j+1] = charLocY + charSize;	// Top Left
		vtxPositionBufferPtr[j+2] = charLocX + charWidth;		vtxPositionBufferPtr[j+3] = charLocY + charSize;	// Top Right
		vtxPositionBufferPtr[j+4] = charLocX + charWidth;		vtxPositionBufferPtr[j+5] = charLocY;				// Bottom Right
		vtxPositionBufferPtr[j+6] = charLocX;					vtxPositionBufferPtr[j+7] = charLocY;				// Bottom Left
		charLocX += (charWidth + (charSize * 0.03f) + 0.10f);
	}
	
	float textBoxScale = HUD_TEXTBOX_BASE_SCALE * this->_hudObjectScale;
	if (textBoxScale < (HUD_TEXTBOX_BASE_SCALE * HUD_TEXTBOX_MIN_SCALE))
	{
		textBoxScale = HUD_TEXTBOX_BASE_SCALE * HUD_TEXTBOX_MIN_SCALE;
	}
	
	textBoxScale /= this->_scaleFactor;
	
	float boxOffset = 8.0f * HUD_TEXTBOX_BASE_SCALE * this->_hudObjectScale;
	if (boxOffset < 1.0f)
	{
		boxOffset = 1.0f;
	}
	else if (boxOffset > 8.0f)
	{
		boxOffset = 8.0f;
	}
	
	boxOffset *= this->_scaleFactor;
	
	// Set the width of the text box
	vtxPositionBufferPtr[2] += textBoxWidth;
	vtxPositionBufferPtr[4] += textBoxWidth;
	
	// Scale and translate the box
	charLocX = textBoxTextOffset;
	charLocY = 0.0;
	
	for (size_t k = 0; k < (textLength * 8); k+=2)
	{
		// Scale
		vtxPositionBufferPtr[k+0] *= textBoxScale;
		vtxPositionBufferPtr[k+1] *= textBoxScale;
		
		// Translate
		vtxPositionBufferPtr[k+0] += boxOffset - (viewportWidth / 2.0f);
		vtxPositionBufferPtr[k+1] += (viewportHeight / 2.0f) - boxOffset;
	}
	
	// Fill in the vertices for the inputs.
	const char *hudInputString = this->_hudInputString.c_str();
	
	for (size_t k = 0; k < HUD_INPUT_ELEMENT_LENGTH; i++, j+=8, k++)
	{
		const char c = hudInputString[k];
		const float charWidth = this->_glyphInfo[c].width * charSize / (float)this->_glyphTileSize;
		
		vtxPositionBufferPtr[j+0] = charLocX;					vtxPositionBufferPtr[j+1] = charLocY + charSize;	// Top Left
		vtxPositionBufferPtr[j+2] = charLocX + charWidth;		vtxPositionBufferPtr[j+3] = charLocY + charSize;	// Top Right
		vtxPositionBufferPtr[j+4] = charLocX + charWidth;		vtxPositionBufferPtr[j+5] = charLocY;				// Bottom Right
		vtxPositionBufferPtr[j+6] = charLocX;					vtxPositionBufferPtr[j+7] = charLocY;				// Bottom Left
		charLocX += (charWidth + (charSize * 0.03f) + 0.10f);
	}
	
	float inputScale = 0.45 * this->_hudObjectScale;
	if (inputScale < (0.45 * 1.0))
	{
		inputScale = 0.45 * 1.0;
	}
	
	inputScale /= this->_scaleFactor;
	
	for (size_t k = (textLength * 8); k < ((textLength + HUD_INPUT_ELEMENT_LENGTH) * 8); k+=2)
	{
		// Scale
		vtxPositionBufferPtr[k+0] *= inputScale;
		vtxPositionBufferPtr[k+1] *= inputScale;
		
		// Translate
		vtxPositionBufferPtr[k+0] += boxOffset - (viewportWidth / 2.0f);
		vtxPositionBufferPtr[k+1] += -(viewportHeight / 2.0f);
	}
	
	this->SetHUDTouchLinePositionVertices(vtxPositionBufferPtr + j);
}

void ClientDisplay3DPresenter::SetHUDTouchLinePositionVertices(float *vtxBufferPtr)
{
	const float x = (this->_ndsFrameInfo.inputStatesApplied.Touch == 0) ? this->_ndsFrameInfo.touchLocXApplied : this->_ndsFrameInfo.touchLocXPending;
	const float y = 191.0f - ((this->_ndsFrameInfo.inputStatesApplied.Touch == 0) ? this->_ndsFrameInfo.touchLocYApplied : this->_ndsFrameInfo.touchLocYPending);
	const float w = this->_renderProperty.normalWidth / 2.0f;
	const float h = this->_renderProperty.normalHeight / 2.0f;
	
	if (this->_renderProperty.mode == ClientDisplayMode_Dual)
	{
		switch (this->_renderProperty.layout)
		{
			case ClientDisplayLayout_Horizontal:
			{
				if (this->_renderProperty.order == ClientDisplayOrder_MainFirst)
				{
					vtxBufferPtr[0]		=  x;															vtxBufferPtr[1]		=  h;						// Vertical line, top left
					vtxBufferPtr[2]		=  x + 1.0f;													vtxBufferPtr[3]		=  h;						// Vertical line, top right
					vtxBufferPtr[4]		=  x + 1.0f;													vtxBufferPtr[5]		= -h;						// Vertical line, bottom right
					vtxBufferPtr[6]		=  x;															vtxBufferPtr[7]		= -h;						// Vertical line, bottom left
					
					vtxBufferPtr[8]		=  0.0f;														vtxBufferPtr[9]		= -h + y + 1.0f;			// Horizontal line, top left
					vtxBufferPtr[10]	=  w;															vtxBufferPtr[11]	= -h + y + 1.0f;			// Horizontal line, top right
					vtxBufferPtr[12]	=  w;															vtxBufferPtr[13]	= -h + y;					// Horizontal line, bottom right
					vtxBufferPtr[14]	=  0.0f;														vtxBufferPtr[15]	= -h + y;					// Horizontal line, bottom left
				}
				else
				{
					vtxBufferPtr[0]		= -w + x;														vtxBufferPtr[1]		=  h;						// Vertical line, top left
					vtxBufferPtr[2]		= -w + x + 1.0f;												vtxBufferPtr[3]		=  h;						// Vertical line, top right
					vtxBufferPtr[4]		= -w + x + 1.0f;												vtxBufferPtr[5]		= -h;						// Vertical line, bottom right
					vtxBufferPtr[6]		= -w + x;														vtxBufferPtr[7]		= -h;						// Vertical line, bottom left
					
					vtxBufferPtr[8]		= -w;															vtxBufferPtr[9]		= -h + y + 1.0f;			// Horizontal line, top left
					vtxBufferPtr[10]	=  0.0f;														vtxBufferPtr[11]	= -h + y + 1.0f;			// Horizontal line, top right
					vtxBufferPtr[12]	=  0.0f;														vtxBufferPtr[13]	= -h + y;					// Horizontal line, bottom right
					vtxBufferPtr[14]	= -w;															vtxBufferPtr[15]	= -h + y;					// Horizontal line, bottom left
				}
				
				memcpy(vtxBufferPtr + (2 * 8), vtxBufferPtr + (0 * 8), sizeof(float) * (2 * 8));							// Unused lines
				break;
			}
				
			case ClientDisplayLayout_Hybrid_2_1:
			{
				vtxBufferPtr[0]		= -w + (float)GPU_FRAMEBUFFER_NATIVE_WIDTH + (x/2.0f);				vtxBufferPtr[1]		= -h + 96.0f;				// Minor bottom display, vertical line, top left
				vtxBufferPtr[2]		= -w + (float)GPU_FRAMEBUFFER_NATIVE_WIDTH + ((x+1.0f)/2.0f);		vtxBufferPtr[3]		= -h + 96.0f;				// Minor bottom display, vertical line, top right
				vtxBufferPtr[4]		= -w + (float)GPU_FRAMEBUFFER_NATIVE_WIDTH + ((x+1.0f)/2.0f);		vtxBufferPtr[5]		= -h;						// Minor bottom display, vertical line, bottom right
				vtxBufferPtr[6]		= -w + (float)GPU_FRAMEBUFFER_NATIVE_WIDTH + (x/2.0f);				vtxBufferPtr[7]		= -h;						// Minor bottom display, vertical line, bottom left
				
				vtxBufferPtr[8]		= -w + (float)GPU_FRAMEBUFFER_NATIVE_WIDTH;							vtxBufferPtr[9]		= -h + ((y+1.0f)/2.0f);		// Minor bottom display, horizontal line, top left
				vtxBufferPtr[10]	=  w;																vtxBufferPtr[11]	= -h + ((y+1.0f)/2.0f);		// Minor bottom display, horizontal line, top right
				vtxBufferPtr[12]	=  w;																vtxBufferPtr[13]	= -h + (y/2.0f);			// Minor bottom display, horizontal line, bottom right
				vtxBufferPtr[14]	= -w + (float)GPU_FRAMEBUFFER_NATIVE_WIDTH;							vtxBufferPtr[15]	= -h + (y/2.0f);			// Minor bottom display, horizontal line, bottom left
				
				if (this->_renderProperty.order == ClientDisplayOrder_MainFirst)
				{
					memcpy(vtxBufferPtr + (2 * 8), vtxBufferPtr + (0 * 8), sizeof(float) * (2 * 8));													// Unused lines
				}
				else
				{
					vtxBufferPtr[16]	= -w + x;														vtxBufferPtr[17]	=  h;						// Major display, vertical line, top left
					vtxBufferPtr[18]	= -w + x + 1.0f;												vtxBufferPtr[19]	=  h;						// Major display, vertical line, top right
					vtxBufferPtr[20]	= -w + x + 1.0f;												vtxBufferPtr[21]	= -h;						// Major display, vertical line, bottom right
					vtxBufferPtr[22]	= -w + x;														vtxBufferPtr[23]	= -h;						// Major display, vertical line, bottom left
					
					vtxBufferPtr[24]	= -w;															vtxBufferPtr[25]	= -h + y + 1.0f;			// Major display, horizontal line, top left
					vtxBufferPtr[26]	= -w + (float)GPU_FRAMEBUFFER_NATIVE_WIDTH;						vtxBufferPtr[27]	= -h + y + 1.0f;			// Major display, horizontal line, top right
					vtxBufferPtr[28]	= -w + (float)GPU_FRAMEBUFFER_NATIVE_WIDTH;						vtxBufferPtr[29]	= -h + y;					// Major display, horizontal line, bottom right
					vtxBufferPtr[30]	= -w;															vtxBufferPtr[31]	= -h + y;					// Major display, horizontal line, bottom left
				}
				break;
			}
				
			case ClientDisplayLayout_Hybrid_16_9:
			{
				vtxBufferPtr[0]		= -w + (float)GPU_FRAMEBUFFER_NATIVE_WIDTH + (x/3.0f);				vtxBufferPtr[1]		= -h + 64.0f;				// Minor bottom display, vertical line, top left
				vtxBufferPtr[2]		= -w + (float)GPU_FRAMEBUFFER_NATIVE_WIDTH + ((x+1.0f)/3.0f);		vtxBufferPtr[3]		= -h + 64.0f;				// Minor bottom display, vertical line, top right
				vtxBufferPtr[4]		= -w + (float)GPU_FRAMEBUFFER_NATIVE_WIDTH + ((x+1.0f)/3.0f);		vtxBufferPtr[5]		= -h;						// Minor bottom display, vertical line, bottom right
				vtxBufferPtr[6]		= -w + (float)GPU_FRAMEBUFFER_NATIVE_WIDTH + (x/3.0f);				vtxBufferPtr[7]		= -h;						// Minor bottom display, vertical line, bottom left
				
				vtxBufferPtr[8]		= -w + (float)GPU_FRAMEBUFFER_NATIVE_WIDTH;							vtxBufferPtr[9]		= -h + ((y+1.0f)/3.0f);		// Minor bottom display, horizontal line, top left
				vtxBufferPtr[10]	=  w;																vtxBufferPtr[11]	= -h + ((y+1.0f)/3.0f);		// Minor bottom display, horizontal line, top right
				vtxBufferPtr[12]	=  w;																vtxBufferPtr[13]	= -h + (y/3.0f);			// Minor bottom display, horizontal line, bottom right
				vtxBufferPtr[14]	= -w + (float)GPU_FRAMEBUFFER_NATIVE_WIDTH;							vtxBufferPtr[15]	= -h + (y/3.0f);			// Minor bottom display, horizontal line, bottom left
				
				if (this->_renderProperty.order == ClientDisplayOrder_MainFirst)
				{
					memcpy(vtxBufferPtr + (2 * 8), vtxBufferPtr + (0 * 8), sizeof(float) * (2 * 8));													// Unused lines
				}
				else
				{
					vtxBufferPtr[16]	= -w + x;														vtxBufferPtr[17]	=  h;						// Major display, vertical line, top left
					vtxBufferPtr[18]	= -w + x + 1.0f;												vtxBufferPtr[19]	=  h;						// Major display, vertical line, top right
					vtxBufferPtr[20]	= -w + x + 1.0f;												vtxBufferPtr[21]	= -h;						// Major display, vertical line, bottom right
					vtxBufferPtr[22]	= -w + x;														vtxBufferPtr[23]	= -h;						// Major display, vertical line, bottom left
					
					vtxBufferPtr[24]	= -w;															vtxBufferPtr[25]	= -h + y + 1.0f;			// Major display, horizontal line, top left
					vtxBufferPtr[26]	= -w + (float)GPU_FRAMEBUFFER_NATIVE_WIDTH;						vtxBufferPtr[27]	= -h + y + 1.0f;			// Major display, horizontal line, top right
					vtxBufferPtr[28]	= -w + (float)GPU_FRAMEBUFFER_NATIVE_WIDTH;						vtxBufferPtr[29]	= -h + y;					// Major display, horizontal line, bottom right
					vtxBufferPtr[30]	= -w;															vtxBufferPtr[31]	= -h + y;					// Major display, horizontal line, bottom left
				}
				break;
			}
				
			case ClientDisplayLayout_Hybrid_16_10:
			{
				vtxBufferPtr[0]		= -w + (float)GPU_FRAMEBUFFER_NATIVE_WIDTH + (x/5.0f);				vtxBufferPtr[1]		= -h + 38.4f;				// Minor bottom display, vertical line, top left
				vtxBufferPtr[2]		= -w + (float)GPU_FRAMEBUFFER_NATIVE_WIDTH + ((x+1.0f)/5.0f);		vtxBufferPtr[3]		= -h + 38.4f;				// Minor bottom display, vertical line, top right
				vtxBufferPtr[4]		= -w + (float)GPU_FRAMEBUFFER_NATIVE_WIDTH + ((x+1.0f)/5.0f);		vtxBufferPtr[5]		= -h;						// Minor bottom display, vertical line, bottom right
				vtxBufferPtr[6]		= -w + (float)GPU_FRAMEBUFFER_NATIVE_WIDTH + (x/5.0f);				vtxBufferPtr[7]		= -h;						// Minor bottom display, vertical line, bottom left
				
				vtxBufferPtr[8]		= -w + (float)GPU_FRAMEBUFFER_NATIVE_WIDTH;							vtxBufferPtr[9]		= -h + ((y+1.0f)/5.0f);		// Minor bottom display, horizontal line, top left
				vtxBufferPtr[10]	=  w;																vtxBufferPtr[11]	= -h + ((y+1.0f)/5.0f);		// Minor bottom display, horizontal line, top right
				vtxBufferPtr[12]	=  w;																vtxBufferPtr[13]	= -h + (y/5.0f);			// Minor bottom display, horizontal line, bottom right
				vtxBufferPtr[14]	= -w + (float)GPU_FRAMEBUFFER_NATIVE_WIDTH;							vtxBufferPtr[15]	= -h + (y/5.0f);			// Minor bottom display, horizontal line, bottom left
				
				if (this->_renderProperty.order == ClientDisplayOrder_MainFirst)
				{
					memcpy(vtxBufferPtr + (2 * 8), vtxBufferPtr + (0 * 8), sizeof(float) * (2 * 8));													// Unused lines
				}
				else
				{
					vtxBufferPtr[16]	= -w + x;														vtxBufferPtr[17]	=  h;						// Major display, vertical line, top left
					vtxBufferPtr[18]	= -w + x + 1.0f;												vtxBufferPtr[19]	=  h;						// Major display, vertical line, top right
					vtxBufferPtr[20]	= -w + x + 1.0f;												vtxBufferPtr[21]	= -h;						// Major display, vertical line, bottom right
					vtxBufferPtr[22]	= -w + x;														vtxBufferPtr[23]	= -h;						// Major display, vertical line, bottom left
					
					vtxBufferPtr[24]	= -w;															vtxBufferPtr[25]	= -h + y + 1.0f;			// Major display, horizontal line, top left
					vtxBufferPtr[26]	= -w + (float)GPU_FRAMEBUFFER_NATIVE_WIDTH;						vtxBufferPtr[27]	= -h + y + 1.0f;			// Major display, horizontal line, top right
					vtxBufferPtr[28]	= -w + (float)GPU_FRAMEBUFFER_NATIVE_WIDTH;						vtxBufferPtr[29]	= -h + y;					// Major display, horizontal line, bottom right
					vtxBufferPtr[30]	= -w;															vtxBufferPtr[31]	= -h + y;					// Major display, horizontal line, bottom left
				}
				break;
			}
				
			default: // Default to vertical orientation.
			{
				const float g = (float)this->_renderProperty.gapDistance;
				
				if (this->_renderProperty.order == ClientDisplayOrder_MainFirst)
				{
					vtxBufferPtr[0]		= -w + x;														vtxBufferPtr[1]		= -g/2.0f;					// Vertical line, top left
					vtxBufferPtr[2]		= -w + x + 1.0f;												vtxBufferPtr[3]		= -g/2.0f;					// Vertical line, top right
					vtxBufferPtr[4]		= -w + x + 1.0f;												vtxBufferPtr[5]		= -h;						// Vertical line, bottom right
					vtxBufferPtr[6]		= -w + x;														vtxBufferPtr[7]		= -h;						// Vertical line, bottom left
					
					vtxBufferPtr[8]		= -w;															vtxBufferPtr[9]		= -h + y + 1.0f;			// Horizontal line, top left
					vtxBufferPtr[10]	=  w;															vtxBufferPtr[11]	= -h + y + 1.0f;			// Horizontal line, top right
					vtxBufferPtr[12]	=  w;															vtxBufferPtr[13]	= -h + y;					// Horizontal line, bottom right
					vtxBufferPtr[14]	= -w;															vtxBufferPtr[15]	= -h + y;					// Horizontal line, bottom left
				}
				else
				{
					vtxBufferPtr[0]		= -w + x;														vtxBufferPtr[1]		=  h;						// Vertical line, top left
					vtxBufferPtr[2]		= -w + x + 1.0f;												vtxBufferPtr[3]		=  h;						// Vertical line, top right
					vtxBufferPtr[4]		= -w + x + 1.0f;												vtxBufferPtr[5]		=  g/2.0f;					// Vertical line, bottom right
					vtxBufferPtr[6]		= -w + x;														vtxBufferPtr[7]		=  g/2.0f;					// Vertical line, bottom left
					
					vtxBufferPtr[8]		= -w;															vtxBufferPtr[9]		=  g/2.0f + y + 1.0f;		// Horizontal line, top left
					vtxBufferPtr[10]	=  w;															vtxBufferPtr[11]	=  g/2.0f + y + 1.0f;		// Horizontal line, top right
					vtxBufferPtr[12]	=  w;															vtxBufferPtr[13]	=  g/2.0f + y;				// Horizontal line, bottom right
					vtxBufferPtr[14]	= -w;															vtxBufferPtr[15]	=  g/2.0f + y;				// Horizontal line, bottom left
				}
				
				memcpy(vtxBufferPtr + (2 * 8), vtxBufferPtr + (0 * 8), sizeof(float) * (2 * 8));														// Unused lines
				break;
			}
		}
	}
	else // displayModeID == ClientDisplayMode_Main || displayModeID == ClientDisplayMode_Touch
	{
		vtxBufferPtr[0]		= -w + x;																	vtxBufferPtr[1]		=  h;						// Vertical line, top left
		vtxBufferPtr[2]		= -w + x + 1.0f;															vtxBufferPtr[3]		=  h;						// Vertical line, top right
		vtxBufferPtr[4]		= -w + x + 1.0f;															vtxBufferPtr[5]		= -h;						// Vertical line, bottom right
		vtxBufferPtr[6]		= -w + x;																	vtxBufferPtr[7]		= -h;						// Vertical line, bottom left
		
		vtxBufferPtr[8]		= -w;																		vtxBufferPtr[9]		= -h + y + 1.0f;			// Horizontal line, top left
		vtxBufferPtr[10]	=  w;																		vtxBufferPtr[11]	= -h + y + 1.0f;			// Horizontal line, top right
		vtxBufferPtr[12]	=  w;																		vtxBufferPtr[13]	= -h + y;					// Horizontal line, bottom right
		vtxBufferPtr[14]	= -w;																		vtxBufferPtr[15]	= -h + y;					// Horizontal line, bottom left
		
		memcpy(vtxBufferPtr + (2 * 8), vtxBufferPtr + (0 * 8), sizeof(float) * (2 * 8));																// Alternate lines
	}
}

void ClientDisplay3DPresenter::SetHUDColorVertices(uint32_t *vtxColorBufferPtr)
{
	pthread_mutex_lock(&this->_mutexHUDString);
	std::string hudString = this->_hudString;
	pthread_mutex_unlock(&this->_mutexHUDString);
	
	const char *cString = hudString.c_str();
	const size_t textLength = (hudString.length() <= HUD_TEXT_MAX_CHARACTERS) ? hudString.length() : HUD_TEXT_MAX_CHARACTERS;
	uint32_t currentColor = LE_TO_LOCAL_32(0x40000000);
	
	// First, calculate the color of the text box.
	// The text box should always be the first character in the string.
	vtxColorBufferPtr[0] = currentColor;
	vtxColorBufferPtr[1] = currentColor;
	vtxColorBufferPtr[2] = currentColor;
	vtxColorBufferPtr[3] = currentColor;
	
	// Calculate the colors of the remaining characters in the string.
	bool alreadyColoredExecutionSpeed = false;
	bool alreadyColoredVideoFPS = false;
	bool alreadyColoredRender3DFPS = false;
	bool alreadyColoredFrameIndex = false;
	bool alreadyColoredLagFrameCount = false;
	bool alreadyColoredCPULoadAverage = false;
	bool alreadyColoredRTC = false;
	
	if (this->_showExecutionSpeed)
	{
		currentColor = this->_hudColorExecutionSpeed;
		alreadyColoredExecutionSpeed = true;
	}
	else if (this->_showVideoFPS)
	{
		currentColor = this->_hudColorVideoFPS;
		alreadyColoredVideoFPS = true;
	}
	else if (this->_showRender3DFPS)
	{
		currentColor = this->_hudColorRender3DFPS;
		alreadyColoredRender3DFPS = true;
	}
	else if (this->_showFrameIndex)
	{
		currentColor = this->_hudColorFrameIndex;
		alreadyColoredFrameIndex = true;
	}
	else if (this->_showLagFrameCount)
	{
		currentColor = this->_hudColorLagFrameCount;
		alreadyColoredLagFrameCount = true;
	}
	else if (this->_showCPULoadAverage)
	{
		currentColor = this->_hudColorCPULoadAverage;
		alreadyColoredCPULoadAverage = true;
	}
	else if (this->_showRTC)
	{
		currentColor = this->_hudColorRTC;
		alreadyColoredRTC = true;
	}
	
	size_t i = 1;
	size_t j = 4;
	
	for (; i < textLength; i++, j+=4)
	{
		const char c = cString[i];
		
		if (c == '\n')
		{
			if (this->_showExecutionSpeed && !alreadyColoredExecutionSpeed)
			{
				currentColor = this->_hudColorExecutionSpeed;
				alreadyColoredExecutionSpeed = true;
			}
			else if (this->_showVideoFPS && !alreadyColoredVideoFPS)
			{
				currentColor = this->_hudColorVideoFPS;
				alreadyColoredVideoFPS = true;
			}
			else if (this->_showRender3DFPS && !alreadyColoredRender3DFPS)
			{
				currentColor = this->_hudColorRender3DFPS;
				alreadyColoredRender3DFPS = true;
			}
			else if (this->_showFrameIndex && !alreadyColoredFrameIndex)
			{
				currentColor = this->_hudColorFrameIndex;
				alreadyColoredFrameIndex = true;
			}
			else if (this->_showLagFrameCount && !alreadyColoredLagFrameCount)
			{
				currentColor = this->_hudColorLagFrameCount;
				alreadyColoredLagFrameCount = true;
			}
			else if (this->_showCPULoadAverage && !alreadyColoredCPULoadAverage)
			{
				currentColor = this->_hudColorCPULoadAverage;
				alreadyColoredCPULoadAverage = true;
			}
			else if (this->_showRTC && !alreadyColoredRTC)
			{
				currentColor = this->_hudColorRTC;
				alreadyColoredRTC = true;
			}
			
			continue;
		}
		
		vtxColorBufferPtr[j+0] = currentColor;		// Top Left
		vtxColorBufferPtr[j+1] = currentColor;		// Top Right
		vtxColorBufferPtr[j+2] = currentColor;		// Bottom Right
		vtxColorBufferPtr[j+3] = currentColor;		// Bottom Left
	}
	
	// Fill in the vertices for the inputs.
	// "<^>vABXYLRSsgf x:000 y:000";
	
	uint32_t inputColors[HUD_INPUT_ELEMENT_LENGTH];
	inputColors[ 0] = this->GetInputColorUsingStates( (this->_ndsFrameInfo.inputStatesPending.Left == 0),   (this->_ndsFrameInfo.inputStatesApplied.Left == 0) );
	inputColors[ 1] = this->GetInputColorUsingStates( (this->_ndsFrameInfo.inputStatesPending.Up == 0),     (this->_ndsFrameInfo.inputStatesApplied.Up == 0) );
	inputColors[ 2] = this->GetInputColorUsingStates( (this->_ndsFrameInfo.inputStatesPending.Right == 0),  (this->_ndsFrameInfo.inputStatesApplied.Right == 0) );
	inputColors[ 3] = this->GetInputColorUsingStates( (this->_ndsFrameInfo.inputStatesPending.Down == 0),   (this->_ndsFrameInfo.inputStatesApplied.Down == 0) );
	inputColors[ 4] = this->GetInputColorUsingStates( (this->_ndsFrameInfo.inputStatesPending.A == 0),      (this->_ndsFrameInfo.inputStatesApplied.A == 0) );
	inputColors[ 5] = this->GetInputColorUsingStates( (this->_ndsFrameInfo.inputStatesPending.B == 0),      (this->_ndsFrameInfo.inputStatesApplied.B == 0) );
	inputColors[ 6] = this->GetInputColorUsingStates( (this->_ndsFrameInfo.inputStatesPending.X == 0),      (this->_ndsFrameInfo.inputStatesApplied.X == 0) );
	inputColors[ 7] = this->GetInputColorUsingStates( (this->_ndsFrameInfo.inputStatesPending.Y == 0),      (this->_ndsFrameInfo.inputStatesApplied.Y == 0) );
	inputColors[ 8] = this->GetInputColorUsingStates( (this->_ndsFrameInfo.inputStatesPending.L == 0),      (this->_ndsFrameInfo.inputStatesApplied.L == 0) );
	inputColors[ 9] = this->GetInputColorUsingStates( (this->_ndsFrameInfo.inputStatesPending.R == 0),      (this->_ndsFrameInfo.inputStatesApplied.R == 0) );
	inputColors[10] = this->GetInputColorUsingStates( (this->_ndsFrameInfo.inputStatesPending.Start == 0),  (this->_ndsFrameInfo.inputStatesApplied.Start == 0) );
	inputColors[11] = this->GetInputColorUsingStates( (this->_ndsFrameInfo.inputStatesPending.Select == 0), (this->_ndsFrameInfo.inputStatesApplied.Select == 0) );
	inputColors[12] = this->GetInputColorUsingStates( (this->_ndsFrameInfo.inputStatesPending.Debug == 0),  (this->_ndsFrameInfo.inputStatesApplied.Debug == 0) );
	inputColors[13] = this->GetInputColorUsingStates( (this->_ndsFrameInfo.inputStatesPending.Lid == 0),    (this->_ndsFrameInfo.inputStatesApplied.Lid == 0) );
	inputColors[14] = this->GetInputColorUsingStates( (this->_ndsFrameInfo.inputStatesPending.Touch == 0),  (this->_ndsFrameInfo.inputStatesApplied.Touch == 0) );
	
	uint32_t touchColor = inputColors[14];
	
	for (size_t k = 15; k < HUD_INPUT_ELEMENT_LENGTH; k++)
	{
		inputColors[k] = touchColor;
	}
	
	for (size_t k = 0; k < HUD_INPUT_ELEMENT_LENGTH; i++, j+=4, k++)
	{
		vtxColorBufferPtr[j+0] = inputColors[k];		// Top Left
		vtxColorBufferPtr[j+1] = inputColors[k];		// Top Right
		vtxColorBufferPtr[j+2] = inputColors[k];		// Bottom Right
		vtxColorBufferPtr[j+3] = inputColors[k];		// Bottom Left
	}
	
	touchColor = ((this->_ndsFrameInfo.inputStatesPending.Touch != 0) && (this->_ndsFrameInfo.inputStatesApplied.Touch != 0)) ? 0x00000000 : (touchColor & LE_TO_LOCAL_32(0x00FFFFFF)) | LE_TO_LOCAL_32(0x60000000);
	
	for (size_t k = 0; k < HUD_INPUT_TOUCH_LINE_ELEMENTS; i++, j+=4, k++)
	{
		vtxColorBufferPtr[j+0] = touchColor;			// Top Left
		vtxColorBufferPtr[j+1] = touchColor;			// Top Right
		vtxColorBufferPtr[j+2] = touchColor;			// Bottom Right
		vtxColorBufferPtr[j+3] = touchColor;			// Bottom Left
	}
}

void ClientDisplay3DPresenter::SetHUDTextureCoordinates(float *texCoordBufferPtr)
{
	pthread_mutex_lock(&this->_mutexHUDString);
	std::string hudString = this->_hudString;
	pthread_mutex_unlock(&this->_mutexHUDString);
	
	const char *cString = hudString.c_str();
	const size_t textLength = hudString.length();
	size_t i = 0;
	
	for (; i < textLength; i++)
	{
		const char c = cString[i];
		const float *glyphTexCoord = this->_glyphInfo[c].texCoord;
		float *hudTexCoord = &texCoordBufferPtr[i * 8];
		
		hudTexCoord[0] = glyphTexCoord[0];
		hudTexCoord[1] = glyphTexCoord[1];
		hudTexCoord[2] = glyphTexCoord[2];
		hudTexCoord[3] = glyphTexCoord[3];
		hudTexCoord[4] = glyphTexCoord[4];
		hudTexCoord[5] = glyphTexCoord[5];
		hudTexCoord[6] = glyphTexCoord[6];
		hudTexCoord[7] = glyphTexCoord[7];
	}
	
	const char *hudInputString = this->_hudInputString.c_str();
	
	for (size_t k = 0; k < HUD_INPUT_ELEMENT_LENGTH; i++, k++)
	{
		const char c = hudInputString[k];
		const float *glyphTexCoord = this->_glyphInfo[c].texCoord;
		float *hudTexCoord = &texCoordBufferPtr[i * 8];
		
		hudTexCoord[0] = glyphTexCoord[0];
		hudTexCoord[1] = glyphTexCoord[1];
		hudTexCoord[2] = glyphTexCoord[2];
		hudTexCoord[3] = glyphTexCoord[3];
		hudTexCoord[4] = glyphTexCoord[4];
		hudTexCoord[5] = glyphTexCoord[5];
		hudTexCoord[6] = glyphTexCoord[6];
		hudTexCoord[7] = glyphTexCoord[7];
	}
	
	for (size_t k = 0; k < HUD_INPUT_TOUCH_LINE_ELEMENTS; i++, k++)
	{
		const char c = cString[0];
		const float *glyphTexCoord = this->_glyphInfo[c].texCoord;
		float *hudTexCoord = &texCoordBufferPtr[i * 8];
		
		hudTexCoord[0] = glyphTexCoord[0];
		hudTexCoord[1] = glyphTexCoord[1];
		hudTexCoord[2] = glyphTexCoord[2];
		hudTexCoord[3] = glyphTexCoord[3];
		hudTexCoord[4] = glyphTexCoord[4];
		hudTexCoord[5] = glyphTexCoord[5];
		hudTexCoord[6] = glyphTexCoord[6];
		hudTexCoord[7] = glyphTexCoord[7];
	}
}

void ClientDisplay3DPresenter::SetScreenVertices(float *vtxBufferPtr)
{
	const float w = this->_renderProperty.normalWidth / 2.0f;
	const float h = this->_renderProperty.normalHeight / 2.0f;
	const size_t f = (this->_renderProperty.order == ClientDisplayOrder_MainFirst) ? 0 : 8;
	
	if (this->_renderProperty.mode == ClientDisplayMode_Dual)
	{
		switch (this->_renderProperty.layout)
		{
			case ClientDisplayLayout_Horizontal:
			{
				vtxBufferPtr[0+f]	= -w;									vtxBufferPtr[1+f]	=  h;						// Left display, top left
				vtxBufferPtr[2+f]	=  0.0f;								vtxBufferPtr[3+f]	=  h;						// Left display, top right
				vtxBufferPtr[4+f]	= -w;									vtxBufferPtr[5+f]	= -h;						// Left display, bottom left
				vtxBufferPtr[6+f]	=  0.0f;								vtxBufferPtr[7+f]	= -h;						// Left display, bottom right
				
				vtxBufferPtr[8-f]	=  0.0f;								vtxBufferPtr[9-f]	=  h;						// Right display, top left
				vtxBufferPtr[10-f]	=  w;									vtxBufferPtr[11-f]	=  h;						// Right display, top right
				vtxBufferPtr[12-f]	=  0.0f;								vtxBufferPtr[13-f]	= -h;						// Right display, bottom left
				vtxBufferPtr[14-f]	=  w;									vtxBufferPtr[15-f]	= -h;						// Right display, bottom right
				
				memcpy(vtxBufferPtr + (2 * 8), vtxBufferPtr + (0 * 8), sizeof(float) * (2 * 8));							// Unused displays
				break;
			}
				
			case ClientDisplayLayout_Hybrid_2_1:
			{
				vtxBufferPtr[0]		= -w + (float)GPU_FRAMEBUFFER_NATIVE_WIDTH;		vtxBufferPtr[1]		= -h + (96.0f * 2.0f);		// Minor top display, top left
				vtxBufferPtr[2]		=  w;											vtxBufferPtr[3]		= -h + (96.0f * 2.0f);		// Minor top display, top right
				vtxBufferPtr[4]		= -w + (float)GPU_FRAMEBUFFER_NATIVE_WIDTH;		vtxBufferPtr[5]		= -h + 96.0f;				// Minor top display, bottom left
				vtxBufferPtr[6]		=  w;											vtxBufferPtr[7]		= -h + 96.0f;				// Minor top display, bottom right
				
				vtxBufferPtr[8]		= -w + (float)GPU_FRAMEBUFFER_NATIVE_WIDTH;		vtxBufferPtr[9]		= -h + 96.0f;				// Minor bottom display, top left
				vtxBufferPtr[10]	=  w;											vtxBufferPtr[11]	= -h + 96.0f;				// Minor bottom display, top right
				vtxBufferPtr[12]	= -w + (float)GPU_FRAMEBUFFER_NATIVE_WIDTH;		vtxBufferPtr[13]	= -h;						// Minor bottom display, bottom left
				vtxBufferPtr[14]	=  w;											vtxBufferPtr[15]	= -h;						// Minor bottom display, bottom right
				
				vtxBufferPtr[16]	= -w;											vtxBufferPtr[17]	=  h;						// Major display, top left
				vtxBufferPtr[18]	= -w + (float)GPU_FRAMEBUFFER_NATIVE_WIDTH;		vtxBufferPtr[19]	=  h;						// Major display, top right
				vtxBufferPtr[20]	= -w;											vtxBufferPtr[21]	= -h;						// Major display, bottom left
				vtxBufferPtr[22]	= -w + (float)GPU_FRAMEBUFFER_NATIVE_WIDTH;		vtxBufferPtr[23]	= -h;						// Major display, bottom right
				
				memcpy(vtxBufferPtr + (3 * 8), vtxBufferPtr + (2 * 8), sizeof(float) * (1 * 8));									// Major display (bottom screen)
				break;
			}
				
			case ClientDisplayLayout_Hybrid_16_9:
			{
				const float g = (float)this->_renderProperty.gapDistance * (this->_renderProperty.normalWidth - (float)GPU_FRAMEBUFFER_NATIVE_WIDTH) / (float)GPU_FRAMEBUFFER_NATIVE_WIDTH;
				
				vtxBufferPtr[0]		= -w + (float)GPU_FRAMEBUFFER_NATIVE_WIDTH;		vtxBufferPtr[1]		= -h + g + (64.0f * 2.0f);	// Minor top display, top left
				vtxBufferPtr[2]		=  w;											vtxBufferPtr[3]		= -h + g + (64.0f * 2.0f);	// Minor top display, top right
				vtxBufferPtr[4]		= -w + (float)GPU_FRAMEBUFFER_NATIVE_WIDTH;		vtxBufferPtr[5]		= -h + g + 64.0f;			// Minor top display, bottom left
				vtxBufferPtr[6]		=  w;											vtxBufferPtr[7]		= -h + g + 64.0f;			// Minor top display, bottom right
				
				vtxBufferPtr[8]		= -w + (float)GPU_FRAMEBUFFER_NATIVE_WIDTH;		vtxBufferPtr[9]		= -h + 64.0f;				// Minor bottom display, top left
				vtxBufferPtr[10]	=  w;											vtxBufferPtr[11]	= -h + 64.0f;				// Minor bottom display, top right
				vtxBufferPtr[12]	= -w + (float)GPU_FRAMEBUFFER_NATIVE_WIDTH;		vtxBufferPtr[13]	= -h;						// Minor bottom display, bottom left
				vtxBufferPtr[14]	=  w;											vtxBufferPtr[15]	= -h;						// Minor bottom display, bottom right
				
				vtxBufferPtr[16]	= -w;											vtxBufferPtr[17]	=  h;						// Major display, top left
				vtxBufferPtr[18]	= -w + (float)GPU_FRAMEBUFFER_NATIVE_WIDTH;		vtxBufferPtr[19]	=  h;						// Major display, top right
				vtxBufferPtr[20]	= -w;											vtxBufferPtr[21]	= -h;						// Major display, bottom left
				vtxBufferPtr[22]	= -w + (float)GPU_FRAMEBUFFER_NATIVE_WIDTH;		vtxBufferPtr[23]	= -h;						// Major display, bottom right
				
				memcpy(vtxBufferPtr + (3 * 8), vtxBufferPtr + (2 * 8), sizeof(float) * (1 * 8));									// Major display (bottom screen)
				break;
			}
				
			case ClientDisplayLayout_Hybrid_16_10:
			{
				const float g = (float)this->_renderProperty.gapDistance * (this->_renderProperty.normalWidth - (float)GPU_FRAMEBUFFER_NATIVE_WIDTH) / (float)GPU_FRAMEBUFFER_NATIVE_WIDTH;
				
				vtxBufferPtr[0]		= -w + (float)GPU_FRAMEBUFFER_NATIVE_WIDTH;		vtxBufferPtr[1]		= -h + g + (38.4f * 2.0f);	// Minor top display, top left
				vtxBufferPtr[2]		=  w;											vtxBufferPtr[3]		= -h + g + (38.4f * 2.0f);	// Minor top display, top right
				vtxBufferPtr[4]		= -w + (float)GPU_FRAMEBUFFER_NATIVE_WIDTH;		vtxBufferPtr[5]		= -h + g + 38.4f;			// Minor top display, bottom left
				vtxBufferPtr[6]		=  w;											vtxBufferPtr[7]		= -h + g + 38.4f;			// Minor top display, bottom right
				
				vtxBufferPtr[8]		= -w + (float)GPU_FRAMEBUFFER_NATIVE_WIDTH;		vtxBufferPtr[9]		= -h + 38.4f;				// Minor bottom display, top left
				vtxBufferPtr[10]	=  w;											vtxBufferPtr[11]	= -h + 38.4f;				// Minor bottom display, top right
				vtxBufferPtr[12]	= -w + (float)GPU_FRAMEBUFFER_NATIVE_WIDTH;		vtxBufferPtr[13]	= -h;						// Minor bottom display, bottom left
				vtxBufferPtr[14]	=  w;											vtxBufferPtr[15]	= -h;						// Minor bottom display, bottom right
				
				vtxBufferPtr[16]	= -w;											vtxBufferPtr[17]	=  h;						// Major display, top left
				vtxBufferPtr[18]	= -w + (float)GPU_FRAMEBUFFER_NATIVE_WIDTH;		vtxBufferPtr[19]	=  h;						// Major display, top right
				vtxBufferPtr[20]	= -w;											vtxBufferPtr[21]	= -h;						// Major display, bottom left
				vtxBufferPtr[22]	= -w + (float)GPU_FRAMEBUFFER_NATIVE_WIDTH;		vtxBufferPtr[23]	= -h;						// Major display, bottom right
				
				memcpy(vtxBufferPtr + (3 * 8), vtxBufferPtr + (2 * 8), sizeof(float) * (1 * 8));									// Major display (bottom screen)
				break;
			}
				
			default: // Default to vertical orientation.
			{
				const float g = (float)this->_renderProperty.gapDistance;
				
				vtxBufferPtr[0+f]	= -w;									vtxBufferPtr[1+f]	=  h;						// Top display, top left
				vtxBufferPtr[2+f]	=  w;									vtxBufferPtr[3+f]	=  h;						// Top display, top right
				vtxBufferPtr[4+f]	= -w;									vtxBufferPtr[5+f]	=  g/2.0f;					// Top display, bottom left
				vtxBufferPtr[6+f]	=  w;									vtxBufferPtr[7+f]	=  g/2.0f;					// Top display, bottom right
				
				vtxBufferPtr[8-f]	= -w;									vtxBufferPtr[9-f]	= -g/2.0f;					// Bottom display, top left
				vtxBufferPtr[10-f]	=  w;									vtxBufferPtr[11-f]	= -g/2.0f;					// Bottom display, top right
				vtxBufferPtr[12-f]	= -w;									vtxBufferPtr[13-f]	= -h;						// Bottom display, bottom left
				vtxBufferPtr[14-f]	=  w;									vtxBufferPtr[15-f]	= -h;						// Bottom display, bottom right
				
				memcpy(vtxBufferPtr + (2 * 8), vtxBufferPtr + (0 * 8), sizeof(float) * (2 * 8));							// Unused displays
				break;
			}
		}
	}
	else // displayModeID == ClientDisplayMode_Main || displayModeID == ClientDisplayMode_Touch
	{
		vtxBufferPtr[0]	= -w;										vtxBufferPtr[1]	=  h;							// First display, top left
		vtxBufferPtr[2]	=  w;										vtxBufferPtr[3]	=  h;							// First display, top right
		vtxBufferPtr[4]	= -w;										vtxBufferPtr[5]	= -h;							// First display, bottom left
		vtxBufferPtr[6]	=  w;										vtxBufferPtr[7]	= -h;							// First display, bottom right
		
		memcpy(vtxBufferPtr + (1 * 8), vtxBufferPtr + (0 * 8), sizeof(float) * (1 * 8));							// Second display
		memcpy(vtxBufferPtr + (2 * 8), vtxBufferPtr + (0 * 8), sizeof(float) * (1 * 8));							// Unused display
		memcpy(vtxBufferPtr + (3 * 8), vtxBufferPtr + (0 * 8), sizeof(float) * (1 * 8));							// Unused display
	}
}

void ClientDisplay3DPresenter::SetScreenTextureCoordinates(float w0, float h0, float w1, float h1, float *texCoordBufferPtr)
{
	texCoordBufferPtr[0]  = 0.0f;			texCoordBufferPtr[1]  = 0.0f;
	texCoordBufferPtr[2]  =   w0;			texCoordBufferPtr[3]  = 0.0f;
	texCoordBufferPtr[4]  = 0.0f;			texCoordBufferPtr[5]  =   h0;
	texCoordBufferPtr[6]  =   w0;			texCoordBufferPtr[7]  =   h0;
	
	texCoordBufferPtr[8]  = 0.0f;			texCoordBufferPtr[9]  = 0.0f;
	texCoordBufferPtr[10] =   w1;			texCoordBufferPtr[11] = 0.0f;
	texCoordBufferPtr[12] = 0.0f;			texCoordBufferPtr[13] =   h1;
	texCoordBufferPtr[14] =   w1;			texCoordBufferPtr[15] =   h1;
	
	memcpy(texCoordBufferPtr + (2 * 8), texCoordBufferPtr + (0 * 8), sizeof(float) * (2 * 8));
}

ClientDisplayView::ClientDisplayView()
{
	_presenter = NULL;
}

ClientDisplayView::ClientDisplayView(ClientDisplayPresenter *thePresenter)
{
	_presenter = thePresenter;
}

void ClientDisplayView::Init()
{
	if (this->_presenter != NULL)
	{
		this->_presenter->Init();
	}
}

ClientDisplayPresenter* ClientDisplayView::GetPresenter()
{
	return this->_presenter;
}

void ClientDisplayView::SetPresenter(ClientDisplayPresenter *thePresenter)
{
	this->_presenter = thePresenter;
}

ClientDisplay3DView::ClientDisplay3DView()
{
	_presenter = NULL;
}

ClientDisplay3DView::ClientDisplay3DView(ClientDisplay3DPresenter *thePresenter)
{
	_presenter = thePresenter;
}

void ClientDisplay3DView::Init()
{
	if (this->_presenter != NULL)
	{
		this->_presenter->Init();
	}
}

ClientDisplay3DPresenter* ClientDisplay3DView::Get3DPresenter()
{
	return this->_presenter;
}

void ClientDisplay3DView::Set3DPresenter(ClientDisplay3DPresenter *thePresenter)
{
	this->_presenter = thePresenter;
}

LUTValues *_LQ2xLUT = NULL;
LUTValues *_HQ2xLUT = NULL;
LUTValues *_HQ3xLUT = NULL;
LUTValues *_HQ4xLUT = NULL;

static LUTValues PackLUTValues(const uint8_t p0, const uint8_t p1, const uint8_t p2, const uint8_t w0, const uint8_t w1, const uint8_t w2)
{
	const uint8_t wR = 256 / (w0 + w1 + w2);
	return (LUTValues) {
		(uint8_t)(p0*31),
		(uint8_t)(p1*31),
		(uint8_t)(p2*31),
		(uint8_t)(0),
		(uint8_t)((w1 == 0 && w2 == 0) ? 255 : w0*wR),
		(uint8_t)(w1*wR),
		(uint8_t)(w2*wR),
		(uint8_t)(0)
	};
}

void InitHQnxLUTs()
{
	static bool lutValuesInited = false;
	
	if (lutValuesInited)
	{
		return;
	}
	
	lutValuesInited = true;
	
	_LQ2xLUT = (LUTValues *)malloc_alignedPage(256*(2*2)*16 * sizeof(LUTValues));
	_HQ2xLUT = (LUTValues *)malloc_alignedPage(256*(2*2)*16 * sizeof(LUTValues));
	_HQ3xLUT = (LUTValues *)malloc_alignedPage(256*(3*3)*16 * sizeof(LUTValues) + 2);
	_HQ4xLUT = (LUTValues *)malloc_alignedPage(256*(4*4)*16 * sizeof(LUTValues) + 4); // The bytes fix a mysterious crash that intermittently occurs. Don't know why this works... it just does.
	
#define MUR (compare & 0x01) // top-right
#define MDR (compare & 0x02) // bottom-right
#define MDL (compare & 0x04) // bottom-left
#define MUL (compare & 0x08) // top-left
#define IC(p0)			PackLUTValues(p0, p0, p0,  1, 0, 0)
#define I11(p0,p1)		PackLUTValues(p0, p1, p0,  1, 1, 0)
#define I211(p0,p1,p2)	PackLUTValues(p0, p1, p2,  2, 1, 1)
#define I31(p0,p1)		PackLUTValues(p0, p1, p0,  3, 1, 0)
#define I332(p0,p1,p2)	PackLUTValues(p0, p1, p2,  3, 3, 2)
#define I431(p0,p1,p2)	PackLUTValues(p0, p1, p2,  4, 3, 1)
#define I521(p0,p1,p2)	PackLUTValues(p0, p1, p2,  5, 2, 1)
#define I53(p0,p1)		PackLUTValues(p0, p1, p0,  5, 3, 0)
#define I611(p0,p1,p2)	PackLUTValues(p0, p1, p2,  6, 1, 1)
#define I71(p0,p1)		PackLUTValues(p0, p1, p0,  7, 1, 0)
#define I772(p0,p1,p2)	PackLUTValues(p0, p1, p2,  7, 7, 2)
#define I97(p0,p1)		PackLUTValues(p0, p1, p0,  9, 7, 0)
#define I1411(p0,p1,p2)	PackLUTValues(p0, p1, p2, 14, 1, 1)
#define I151(p0,p1)		PackLUTValues(p0, p1, p0, 15, 1, 0)
	
#define P0 _LQ2xLUT[pattern+(256*0)+(1024*compare)]
#define P1 _LQ2xLUT[pattern+(256*1)+(1024*compare)]
#define P2 _LQ2xLUT[pattern+(256*2)+(1024*compare)]
#define P3 _LQ2xLUT[pattern+(256*3)+(1024*compare)]
	for (size_t compare = 0; compare < 16; compare++)
	{
		for (size_t pattern = 0; pattern < 256; pattern++)
		{
			switch (pattern)
			{
				#include "../../filter/lq2x.h"
			}
		}
	}
#undef P0
#undef P1
#undef P2
#undef P3

#define P0 _HQ2xLUT[pattern+(256*0)+(1024*compare)]
#define P1 _HQ2xLUT[pattern+(256*1)+(1024*compare)]
#define P2 _HQ2xLUT[pattern+(256*2)+(1024*compare)]
#define P3 _HQ2xLUT[pattern+(256*3)+(1024*compare)]
	for (size_t compare = 0; compare < 16; compare++)
	{
		for (size_t pattern = 0; pattern < 256; pattern++)
		{
			switch (pattern)
			{
				#include "../../filter/hq2x.h"
			}
		}
	}
#undef P0
#undef P1
#undef P2
#undef P3

#define P(a, b)						_HQ3xLUT[pattern+(256*((b*3)+a))+(2304*compare)]
#define I1(p0)						PackLUTValues(p0, p0, p0,  1,  0,  0)
#define I2(i0, i1, p0, p1)			PackLUTValues(p0, p1, p0, i0, i1,  0)
#define I3(i0, i1, i2, p0, p1, p2)	PackLUTValues(p0, p1, p2, i0, i1, i2)
	for (size_t compare = 0; compare < 16; compare++)
	{
		for (size_t pattern = 0; pattern < 256; pattern++)
		{
			switch (pattern)
			{
				#include "../../filter/hq3x.dat"
			}
		}
	}
#undef P
#undef I1
#undef I2
#undef I3

#define P(a, b)						_HQ4xLUT[pattern+(256*((b*4)+a))+(4096*compare)]
#define I1(p0)						PackLUTValues(p0, p0, p0,  1,  0,  0)
#define I2(i0, i1, p0, p1)			PackLUTValues(p0, p1, p0, i0, i1,  0)
#define I3(i0, i1, i2, p0, p1, p2)	PackLUTValues(p0, p1, p2, i0, i1, i2)
	for (size_t compare = 0; compare < 16; compare++)
	{
		for (size_t pattern = 0; pattern < 256; pattern++)
		{
			switch (pattern)
			{
				#include "../../filter/hq4x.dat"
			}
		}
	}
#undef P
#undef I1
#undef I2
#undef I3
	
#undef MUR
#undef MDR
#undef MDL
#undef MUL
#undef IC
#undef I11
#undef I211
#undef I31
#undef I332
#undef I431
#undef I521
#undef I53
#undef I611
#undef I71
#undef I772
#undef I97
#undef I1411
#undef I151
}
