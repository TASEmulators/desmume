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

#include "ClientGraphicsControl.h"

#include "utilities.h"
#include "../../NDSSystem.h"
#include "../../rasterize.h"


GPU3DInterface *core3DList[2] = {
	&gpu3DNull, // Index 0 is dynamically assigned whenever the 3D rendering engine is changed.
	NULL        // The last entry must always be NULL, representing the end of the list.
};

ClientGraphicsControl::ClientGraphicsControl()
{
	_clientObject = NULL;
	_fetchObject = NULL;
	
	_mutexApplyGPUSettings = slock_new();
	_mutexApplyRender3DSettings = slock_new();
	
	_isThreadCountAuto = true;
	_engineIDPending = RENDERID_NULL;
	_colorFormatPending = NDSColorFormat_BGR555_Rev;
	_cpuCoreCountRestoreValue = 0;
	_3DEngineCallbackStruct = gpu3DNull;
	
	if (core3DList[0] == NULL)
	{
		core3DList[0] = &_3DEngineCallbackStruct;
	}
	
	_2DSettingsPending.value = 0;
	_2DSettingsPending.MainEngineEnable   = 1;
	_2DSettingsPending.MainLayerBG0Enable = 1;
	_2DSettingsPending.MainLayerBG1Enable = 1;
	_2DSettingsPending.MainLayerBG2Enable = 1;
	_2DSettingsPending.MainLayerBG3Enable = 1;
	_2DSettingsPending.MainLayerOBJEnable = 1;
	_2DSettingsPending.SubEngineEnable    = 1;
	_2DSettingsPending.SubLayerBG0Enable  = 1;
	_2DSettingsPending.SubLayerBG1Enable  = 1;
	_2DSettingsPending.SubLayerBG2Enable  = 1;
	_2DSettingsPending.SubLayerBG3Enable  = 1;
	_2DSettingsPending.SubLayerOBJEnable  = 1;
	
	_3DSettingsPending.value = 0;
	_3DSettingsPending.RenderScalingFactor             = 1;
	_3DSettingsPending.TextureScalingFactor            = 1;
	_3DSettingsPending.ThreadCount                     = 4;
	_3DSettingsPending.TextureEnable                   = 1;
	_3DSettingsPending.TextureDeposterize              = 0;
	_3DSettingsPending.MulitisampleSize                = 0;
	_3DSettingsPending.EdgeMarkEnable                  = 1;
	_3DSettingsPending.FogEnable                       = 1;
	_3DSettingsPending.HighPrecisionColorInterpolation = 1;
	_3DSettingsPending.LineHackEnable                  = 1;
	_3DSettingsPending.FragmentSamplingHackEnable      = 0;
	_3DSettingsPending.TextureSmoothingEnable          = 0;
	_3DSettingsPending.ShadowPolygonEnable             = 1;
	_3DSettingsPending.SpecialZeroAlphaBlendEnable     = 1;
	_3DSettingsPending.NDSStyleDepthCalculationEnable  = 1;
	_3DSettingsPending.DepthLEqualPolygonFacingEnable  = 0;
	
	_2DSettingsApplied       = _2DSettingsPending;
	_3DSettingsApplied       = _3DSettingsPending;
	_engineIDApplied         = _engineIDPending;
	_engineClientIDApplied   = RENDERID_NULL;
	_engineClientNameApplied = gpu3DNull.name;
	_engineClientNameAppliedCopy = _engineClientNameApplied;
	
	_pageCountPending = 1;
	_pageCountApplied = 1;
	
	_did2DSettingsChange = true;
	_didColorFormatChange = true;
	_didWidthChange = true;
	_didHeightChange = true;
	_didPageCountChange = true;
	
	_multisampleMaxClientSize = 0;
	strncpy(_multisampleSizeString, "Off", sizeof(_multisampleSizeString));
}

ClientGraphicsControl::~ClientGraphicsControl()
{
	slock_free(this->_mutexApplyGPUSettings);
	slock_free(this->_mutexApplyRender3DSettings);
}

void ClientGraphicsControl::SetClientObject(void *clientObj)
{
	this->_clientObject = clientObj;
}

void* ClientGraphicsControl::GetClientObject()
{
	return this->_clientObject;
}

void ClientGraphicsControl::SetFetchObject(GPUClientFetchObject *fetchObject)
{
	this->_fetchObject = fetchObject;
}

GPUClientFetchObject* ClientGraphicsControl::GetFetchObject() const
{
	return this->_fetchObject;
}

void ClientGraphicsControl::SetFramebufferPageCount(size_t pageCount)
{
	slock_lock(this->_mutexApplyGPUSettings);
	
	this->_didPageCountChange = (pageCount != this->_pageCountApplied);
	this->_pageCountPending = pageCount;
	
	slock_unlock(this->_mutexApplyGPUSettings);
}

size_t ClientGraphicsControl::GetFramebufferPageCount()
{
	return this->_pageCountPending;
}

void ClientGraphicsControl::SetFramebufferDimensions(size_t w, size_t h)
{
	if (w < GPU_FRAMEBUFFER_NATIVE_WIDTH)
	{
		w = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	}
	
	if (h < GPU_FRAMEBUFFER_NATIVE_HEIGHT)
	{
		h = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	}
	
	slock_lock(this->_mutexApplyGPUSettings);
	
	this->_didWidthChange            = (w != this->_widthApplied);
	this->_didHeightChange           = (h != this->_heightApplied);
	this->_widthPending              = w;
	this->_heightPending             = h;
	this->_widthScalePending         = (float)w / (float)GPU_FRAMEBUFFER_NATIVE_WIDTH;
	this->_heightScalePending        = (float)h / (float)GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	this->_scaleFactorIntegerPending = (size_t)( ((this->_widthScalePending + this->_heightScalePending) / 2.0f) + 0.001f );
	
	slock_unlock(this->_mutexApplyGPUSettings);
}

void ClientGraphicsControl::GetFramebufferDimensions(size_t &w, size_t &h)
{
	w = this->_widthPending;
	h = this->_heightPending;
}

void ClientGraphicsControl::SetFramebufferDimensionsByScaleFactor(float widthScale, float heightScale)
{
	if (widthScale < 1.0f)
	{
		widthScale = 1.0f;
	}
	
	if (heightScale < 1.0f)
	{
		heightScale = 1.0f;
	}
	
	slock_lock(this->_mutexApplyGPUSettings);
	
	this->_widthPending              = (size_t)( ((float)GPU_FRAMEBUFFER_NATIVE_WIDTH  * widthScale)  + 0.5f );
	this->_heightPending             = (size_t)( ((float)GPU_FRAMEBUFFER_NATIVE_HEIGHT * heightScale) + 0.5f );
	this->_didWidthChange            = (this->_widthPending  != this->_widthApplied);
	this->_didHeightChange           = (this->_heightPending != this->_heightApplied);
	this->_widthScalePending         = widthScale;
	this->_heightScalePending        = heightScale;
	this->_scaleFactorIntegerPending = (size_t)(((widthScale + heightScale) / 2.0f) + 0.001f);
	
	slock_unlock(this->_mutexApplyGPUSettings);
}

void ClientGraphicsControl::GetFramebufferDimensionsByScaleFactor(float &widthScale, float &heightScale)
{
	widthScale  = this->_widthScalePending;
	heightScale = this->_heightScalePending;
}

void ClientGraphicsControl::SetFramebufferDimensionsByScaleFactorInteger(size_t scaleFactor)
{
	if (scaleFactor < 1)
	{
		scaleFactor = 1;
	}
	
	slock_lock(this->_mutexApplyGPUSettings);
	
	this->_widthPending              = GPU_FRAMEBUFFER_NATIVE_WIDTH  * scaleFactor;
	this->_heightPending             = GPU_FRAMEBUFFER_NATIVE_HEIGHT * scaleFactor;
	this->_didWidthChange            = (this->_widthPending  != this->_widthApplied);
	this->_didHeightChange           = (this->_heightPending != this->_heightApplied);
	this->_widthScalePending         = (float)scaleFactor;
	this->_heightScalePending        = (float)scaleFactor;
	this->_scaleFactorIntegerPending = scaleFactor;
	
	slock_unlock(this->_mutexApplyGPUSettings);
}

size_t ClientGraphicsControl::GetFramebufferDimensionsByScaleFactorInteger()
{
	return this->_scaleFactorIntegerPending;
}

void ClientGraphicsControl::SetColorFormat(NDSColorFormat colorFormat)
{
	slock_lock(this->_mutexApplyGPUSettings);
	
	this->_didColorFormatChange = (colorFormat != this->_colorFormatApplied);
	this->_colorFormatPending = colorFormat;
	
	slock_unlock(this->_mutexApplyGPUSettings);
}

NDSColorFormat ClientGraphicsControl::GetColorFormat()
{
	return this->_colorFormatPending;
}

void ClientGraphicsControl::Set2DGraphicsSettings(Client2DGraphicsSettings settings2D)
{
	slock_lock(this->_mutexApplyGPUSettings);
	this->_did2DSettingsChange = (this->_did2DSettingsChange || (this->_2DSettingsPending.value != settings2D.value));
	this->_2DSettingsPending = settings2D;
	slock_unlock(this->_mutexApplyGPUSettings);
}

Client2DGraphicsSettings ClientGraphicsControl::Get2DGraphicsSettings()
{
	return this->_2DSettingsPending;
}

void ClientGraphicsControl::SetMainEngineEnable(bool theState)
{
	slock_lock(this->_mutexApplyGPUSettings);
	this->_did2DSettingsChange = (this->_did2DSettingsChange || (this->_2DSettingsPending.MainEngineEnable != theState));
	this->_2DSettingsPending.MainEngineEnable = (theState) ? 1 : 0;
	slock_unlock(this->_mutexApplyGPUSettings);
}

bool ClientGraphicsControl::GetMainEngineEnable()
{
	return (this->_2DSettingsPending.MainEngineEnable != 0);
}

void ClientGraphicsControl::Set2DLayerEnableMainBG0(bool theState)
{
	slock_lock(this->_mutexApplyGPUSettings);
	this->_did2DSettingsChange = (this->_did2DSettingsChange || ((this->_2DSettingsPending.MainLayerBG0Enable != 0) != theState));
	this->_2DSettingsPending.MainLayerBG0Enable = (theState) ? 1 : 0;
	slock_unlock(this->_mutexApplyGPUSettings);
}

bool ClientGraphicsControl::Get2DLayerEnableMainBG0()
{
	return (this->_2DSettingsPending.MainLayerBG0Enable != 0);
}

void ClientGraphicsControl::Set2DLayerEnableMainBG1(bool theState)
{
	slock_lock(this->_mutexApplyGPUSettings);
	this->_did2DSettingsChange = (this->_did2DSettingsChange || ((this->_2DSettingsPending.MainLayerBG1Enable != 0) != theState));
	this->_2DSettingsPending.MainLayerBG1Enable = (theState) ? 1 : 0;
	slock_unlock(this->_mutexApplyGPUSettings);
}

bool ClientGraphicsControl::Get2DLayerEnableMainBG1()
{
	return (this->_2DSettingsPending.MainLayerBG1Enable != 0);
}

void ClientGraphicsControl::Set2DLayerEnableMainBG2(bool theState)
{
	slock_lock(this->_mutexApplyGPUSettings);
	this->_did2DSettingsChange = (this->_did2DSettingsChange || ((this->_2DSettingsPending.MainLayerBG2Enable != 0) != theState));
	this->_2DSettingsPending.MainLayerBG2Enable = (theState) ? 1 : 0;
	slock_unlock(this->_mutexApplyGPUSettings);
}

bool ClientGraphicsControl::Get2DLayerEnableMainBG2()
{
	return (this->_2DSettingsPending.MainLayerBG2Enable != 0);
}

void ClientGraphicsControl::Set2DLayerEnableMainBG3(bool theState)
{
	slock_lock(this->_mutexApplyGPUSettings);
	this->_did2DSettingsChange = (this->_did2DSettingsChange || ((this->_2DSettingsPending.MainLayerBG3Enable != 0) != theState));
	this->_2DSettingsPending.MainLayerBG3Enable = (theState) ? 1 : 0;
	slock_unlock(this->_mutexApplyGPUSettings);
}

bool ClientGraphicsControl::Get2DLayerEnableMainBG3()
{
	return (this->_2DSettingsPending.MainLayerBG3Enable != 0);
}

void ClientGraphicsControl::Set2DLayerEnableMainOBJ(bool theState)
{
	slock_lock(this->_mutexApplyGPUSettings);
	this->_did2DSettingsChange = (this->_did2DSettingsChange || ((this->_2DSettingsPending.MainLayerOBJEnable != 0) != theState));
	this->_2DSettingsPending.MainLayerOBJEnable = (theState) ? 1 : 0;
	slock_unlock(this->_mutexApplyGPUSettings);
}

bool ClientGraphicsControl::Get2DLayerEnableMainOBJ()
{
	return (this->_2DSettingsPending.MainLayerOBJEnable != 0);
}

void ClientGraphicsControl::SetSubEngineEnable(bool theState)
{
	slock_lock(this->_mutexApplyGPUSettings);
	this->_did2DSettingsChange = (this->_did2DSettingsChange || (this->_2DSettingsPending.SubEngineEnable != theState));
	this->_2DSettingsPending.SubEngineEnable = (theState) ? 1 : 0;
	slock_unlock(this->_mutexApplyGPUSettings);
}

bool ClientGraphicsControl::GetSubEngineEnable()
{
	return (this->_2DSettingsPending.SubEngineEnable != 0);
}

void ClientGraphicsControl::Set2DLayerEnableSubBG0(bool theState)
{
	slock_lock(this->_mutexApplyGPUSettings);
	this->_did2DSettingsChange = (this->_did2DSettingsChange || ((this->_2DSettingsPending.SubLayerBG0Enable != 0) != theState));
	this->_2DSettingsPending.SubLayerBG0Enable = (theState) ? 1 : 0;
	slock_unlock(this->_mutexApplyGPUSettings);
}

bool ClientGraphicsControl::Get2DLayerEnableSubBG0()
{
	return (this->_2DSettingsPending.SubLayerBG0Enable != 0);
}

void ClientGraphicsControl::Set2DLayerEnableSubBG1(bool theState)
{
	slock_lock(this->_mutexApplyGPUSettings);
	this->_did2DSettingsChange = (this->_did2DSettingsChange || ((this->_2DSettingsPending.SubLayerBG1Enable != 0) != theState));
	this->_2DSettingsPending.SubLayerBG1Enable = (theState) ? 1 : 0;
	slock_unlock(this->_mutexApplyGPUSettings);
}

bool ClientGraphicsControl::Get2DLayerEnableSubBG1()
{
	return (this->_2DSettingsPending.SubLayerBG1Enable != 0);
}

void ClientGraphicsControl::Set2DLayerEnableSubBG2(bool theState)
{
	slock_lock(this->_mutexApplyGPUSettings);
	this->_did2DSettingsChange = (this->_did2DSettingsChange || ((this->_2DSettingsPending.SubLayerBG2Enable != 0) != theState));
	this->_2DSettingsPending.SubLayerBG2Enable = (theState) ? 1 : 0;
	slock_unlock(this->_mutexApplyGPUSettings);
}

bool ClientGraphicsControl::Get2DLayerEnableSubBG2()
{
	return (this->_2DSettingsPending.SubLayerBG2Enable != 0);
}

void ClientGraphicsControl::Set2DLayerEnableSubBG3(bool theState)
{
	slock_lock(this->_mutexApplyGPUSettings);
	this->_did2DSettingsChange = (this->_did2DSettingsChange || ((this->_2DSettingsPending.SubLayerBG3Enable != 0) != theState));
	this->_2DSettingsPending.SubLayerBG3Enable = (theState) ? 1 : 0;
	slock_unlock(this->_mutexApplyGPUSettings);
}

bool ClientGraphicsControl::Get2DLayerEnableSubBG3()
{
	return (this->_2DSettingsPending.SubLayerBG3Enable != 0);
}

void ClientGraphicsControl::Set2DLayerEnableSubOBJ(bool theState)
{
	slock_lock(this->_mutexApplyGPUSettings);
	this->_did2DSettingsChange = (this->_did2DSettingsChange || ((this->_2DSettingsPending.SubLayerOBJEnable != 0) != theState));
	this->_2DSettingsPending.SubLayerOBJEnable = (theState) ? 1 : 0;
	slock_unlock(this->_mutexApplyGPUSettings);
}

bool ClientGraphicsControl::Get2DLayerEnableSubOBJ()
{
	return (this->_2DSettingsPending.SubLayerOBJEnable != 0);
}

void ClientGraphicsControl::Set3DEngineByID(int engineID)
{
	slock_lock(this->_mutexApplyRender3DSettings);
	
	switch (engineID)
	{
		case RENDERID_NULL:
			this->_3DEngineCallbackStruct = gpu3DNull;
			break;
			
		case RENDERID_SOFTRASTERIZER:
			this->_3DEngineCallbackStruct = gpu3DRasterize;
			break;
			
		default:
			puts("DeSmuME: Invalid 3D renderer chosen; falling back to SoftRasterizer.");
			engineID = RENDERID_SOFTRASTERIZER;
			this->_3DEngineCallbackStruct = gpu3DRasterize;
			break;
	}
	
	this->_engineIDPending = engineID;
	slock_unlock(this->_mutexApplyRender3DSettings);
}

int ClientGraphicsControl::Get3DEngineByID()
{
	return this->_engineIDPending;
}

int ClientGraphicsControl::Get3DEngineByIDApplied()
{
	slock_lock(this->_mutexApplyRender3DSettings);
	int engineIDApplied = this->_engineIDApplied;
	slock_unlock(this->_mutexApplyRender3DSettings);
	
	return engineIDApplied;
}

int64_t ClientGraphicsControl::Get3DEngineClientID()
{
	slock_lock(this->_mutexApplyRender3DSettings);
	int64_t engineClientIDApplied = this->_engineClientIDApplied;
	slock_unlock(this->_mutexApplyRender3DSettings);
	
	return engineClientIDApplied;
}

const char* ClientGraphicsControl::Get3DEngineClientName()
{
	slock_lock(this->_mutexApplyRender3DSettings);
	this->_engineClientNameAppliedCopy = this->_engineClientNameApplied;
	slock_unlock(this->_mutexApplyRender3DSettings);
	
	return this->_engineClientNameAppliedCopy.c_str();
}

void ClientGraphicsControl::Set3DScalingFactor(uint8_t scalingFactor)
{
	// Cap scalingFactor to 63 because storage is limited to only 6 bits.
	if (scalingFactor > 63)
	{
		scalingFactor = 63;
	}
	
	slock_lock(this->_mutexApplyRender3DSettings);
	this->_3DSettingsPending.RenderScalingFactor = scalingFactor;
	slock_unlock(this->_mutexApplyRender3DSettings);
}

uint8_t ClientGraphicsControl::Get3DScalingFactor()
{
	return (int)this->_3DSettingsPending.RenderScalingFactor;
}

void ClientGraphicsControl::Set3DTextureScalingFactor(uint8_t scalingFactor)
{
	slock_lock(this->_mutexApplyRender3DSettings);
	
	if ( (this->_3DSettingsPending.TextureScalingFactor == 3) && (scalingFactor == 3) )
	{
		scalingFactor = 2;
	}
	else if (scalingFactor > 3) // Cap scalingFactor to 3 because storage is limited to only 2 bits.
	{
		scalingFactor = 3; // Note that a value of "3" actually represents a 4x texture scaling factor.
	}
	else if (scalingFactor < 1)
	{
		scalingFactor = 1;
	}
	
	this->_3DSettingsPending.TextureScalingFactor = scalingFactor;
	slock_unlock(this->_mutexApplyRender3DSettings);
}

uint8_t ClientGraphicsControl::Get3DTextureScalingFactor()
{
	uint8_t scalingFactor = this->_3DSettingsPending.TextureScalingFactor;
	if (scalingFactor == 3)
	{
		scalingFactor = 4; // Note that a value of "3" actually represents a 4x texture scaling factor.
	}
	
	return scalingFactor;
}

void ClientGraphicsControl::Set3DTextureDeposterize(bool theState)
{
	slock_lock(this->_mutexApplyRender3DSettings);
	this->_3DSettingsPending.TextureDeposterize = (theState) ? 1 : 0;
	slock_unlock(this->_mutexApplyRender3DSettings);
}

bool ClientGraphicsControl::Get3DTextureDeposterize()
{
	return (this->_3DSettingsPending.TextureDeposterize != 0);
}

void ClientGraphicsControl::Set3DTextureEnable(bool theState)
{
	slock_lock(this->_mutexApplyRender3DSettings);
	this->_3DSettingsPending.TextureEnable = (theState) ? 1 : 0;
	slock_unlock(this->_mutexApplyRender3DSettings);
}

bool ClientGraphicsControl::Get3DTextureEnable()
{
	return (this->_3DSettingsPending.TextureEnable != 0);
}

void ClientGraphicsControl::Set3DEdgeMarkEnable(bool theState)
{
	slock_lock(this->_mutexApplyRender3DSettings);
	this->_3DSettingsPending.EdgeMarkEnable = (theState) ? 1 : 0;
	slock_unlock(this->_mutexApplyRender3DSettings);
}

bool ClientGraphicsControl::Get3DEdgeMarkEnable()
{
	return (this->_3DSettingsPending.EdgeMarkEnable != 0);
}

void ClientGraphicsControl::Set3DFogEnable(bool theState)
{
	slock_lock(this->_mutexApplyRender3DSettings);
	this->_3DSettingsPending.FogEnable = (theState) ? 1 : 0;
	slock_unlock(this->_mutexApplyRender3DSettings);
}

bool ClientGraphicsControl::Get3DFogEnable()
{
	return (this->_3DSettingsPending.FogEnable != 0);
}

void ClientGraphicsControl::Set3DHighPrecisionColorInterpolation(bool theState)
{
	slock_lock(this->_mutexApplyRender3DSettings);
	this->_3DSettingsPending.HighPrecisionColorInterpolation = (theState) ? 1 : 0;
	slock_unlock(this->_mutexApplyRender3DSettings);
}

bool ClientGraphicsControl::Get3DHighPrecisionColorInterpolation()
{
	return (this->_3DSettingsPending.HighPrecisionColorInterpolation != 0);
}

void ClientGraphicsControl::Set3DLineHackEnable(bool theState)
{
	slock_lock(this->_mutexApplyRender3DSettings);
	this->_3DSettingsPending.LineHackEnable = (theState) ? 1 : 0;
	slock_unlock(this->_mutexApplyRender3DSettings);
}

bool ClientGraphicsControl::Get3DLineHackEnable()
{
	return (this->_3DSettingsPending.LineHackEnable != 0);
}

void ClientGraphicsControl::Set3DFragmentSamplingHackEnable(bool theState)
{
	slock_lock(this->_mutexApplyRender3DSettings);
	this->_3DSettingsPending.FragmentSamplingHackEnable = (theState) ? 1 : 0;
	slock_unlock(this->_mutexApplyRender3DSettings);
}

bool ClientGraphicsControl::Get3DFragmentSamplingHackEnable()
{
	return (this->_3DSettingsPending.FragmentSamplingHackEnable != 0);
}

void ClientGraphicsControl::Set3DRenderingThreadCount(uint8_t threadCount)
{
	const uint8_t numberCores = (uint8_t)CommonSettings.num_cores;
	uint8_t newThreadCount = numberCores;
	
	if (threadCount == 0)
	{
		this->_isThreadCountAuto = true;
		if (numberCores < 2)
		{
			newThreadCount = 1;
		}
		else
		{
			const uint8_t reserveCoreCount = numberCores / 12; // For every 12 cores, reserve 1 core for the rest of the system.
			newThreadCount -= reserveCoreCount;
		}
	}
	else
	{
		this->_isThreadCountAuto = false;
		newThreadCount = threadCount;
	}
	
	// Cap newThreadCount to 63 because storage is limited to only 6 bits.
	if (newThreadCount > 63)
	{
		newThreadCount = 63;
	}
	
	slock_lock(this->_mutexApplyRender3DSettings);
	this->_3DSettingsPending.ThreadCount = newThreadCount;
	slock_unlock(this->_mutexApplyRender3DSettings);
}

uint8_t ClientGraphicsControl::Get3DRenderingThreadCount()
{
	return (this->_isThreadCountAuto) ? 0 : this->_3DSettingsPending.ThreadCount;
}

void ClientGraphicsControl::Set3DMultisampleMaxClientSize(uint8_t maxClientSize)
{
	this->_multisampleMaxClientSize = (uint8_t)GetNearestPositivePOT((uint32_t)maxClientSize);
}

uint8_t ClientGraphicsControl::Get3DMultisampleMaxClientSize()
{
	return this->_multisampleMaxClientSize;
}

uint8_t ClientGraphicsControl::Set3DMultisampleSize(uint8_t msaaSize)
{
	slock_lock(this->_mutexApplyRender3DSettings);
	
	const uint8_t currentMSAASize = this->_3DSettingsPending.MulitisampleSize;
	if (currentMSAASize != msaaSize)
	{
		switch (currentMSAASize)
		{
			case 0:
			{
				if (msaaSize == (currentMSAASize+1))
				{
					msaaSize = 2;
				}
				break;
			}
				
			case 2:
			{
				if (msaaSize == (currentMSAASize-1))
				{
					msaaSize = 0;
				}
				else if (msaaSize == (currentMSAASize+1))
				{
					msaaSize = 4;
				}
				break;
			}
				
			case 4:
			{
				if (msaaSize == (currentMSAASize-1))
				{
					msaaSize = 2;
				}
				else if (msaaSize == (currentMSAASize+1))
				{
					msaaSize = 8;
				}
				break;
			}
				
			case 8:
			{
				if (msaaSize == (currentMSAASize-1))
				{
					msaaSize = 4;
				}
				else if (msaaSize == (currentMSAASize+1))
				{
					msaaSize = 16;
				}
				break;
			}
				
			case 16:
			{
				if (msaaSize == (currentMSAASize-1))
				{
					msaaSize = 8;
				}
				else if (msaaSize == (currentMSAASize+1))
				{
					msaaSize = 32;
				}
				break;
			}
				
			case 32:
			{
				if (msaaSize == (currentMSAASize-1))
				{
					msaaSize = 16;
				}
				else if (msaaSize == (currentMSAASize+1))
				{
					msaaSize = 32;
				}
				break;
			}
				
			default:
			{
				// Cap msaaSize to 63 because storage is limited to only 6 bits.
				if (msaaSize > 63)
				{
					msaaSize = 63;
				}
				break;
			}
		}
		
		msaaSize = GetNearestPositivePOT((uint32_t)msaaSize);
		if (msaaSize > this->_multisampleMaxClientSize)
		{
			msaaSize = this->_multisampleMaxClientSize;
		}
		
		this->_3DSettingsPending.MulitisampleSize = (uint8_t)msaaSize;
		
		if (msaaSize == 0)
		{
			strncpy(this->_multisampleSizeString, "Off", sizeof(this->_multisampleSizeString));
		}
		else
		{
			snprintf(this->_multisampleSizeString, sizeof(this->_multisampleSizeString), "%dx", (int)msaaSize);
		}
	}
	
	slock_unlock(this->_mutexApplyRender3DSettings);
	
	return msaaSize;
}

uint8_t ClientGraphicsControl::Get3DMultisampleSize()
{
	return this->_3DSettingsPending.MulitisampleSize;
}

const char* ClientGraphicsControl::Get3DMultisampleSizeString()
{
	return this->_multisampleSizeString;
}

void ClientGraphicsControl::Set3DTextureSmoothingEnable(bool theState)
{
	slock_lock(this->_mutexApplyRender3DSettings);
	this->_3DSettingsPending.TextureSmoothingEnable = (theState) ? 1 : 0;
	slock_unlock(this->_mutexApplyRender3DSettings);
}

bool ClientGraphicsControl::Get3DTextureSmoothingEnable()
{
	return (this->_3DSettingsPending.TextureSmoothingEnable != 0);
}

void ClientGraphicsControl::Set3DShadowPolygonEnable(bool theState)
{
	slock_lock(this->_mutexApplyRender3DSettings);
	this->_3DSettingsPending.ShadowPolygonEnable = (theState) ? 1 : 0;
	slock_unlock(this->_mutexApplyRender3DSettings);
}

bool ClientGraphicsControl::Get3DShadowPolygonEnable()
{
	return (this->_3DSettingsPending.ShadowPolygonEnable != 0);
}

void ClientGraphicsControl::Set3DSpecialZeroAlphaBlendingEnable(bool theState)
{
	slock_lock(this->_mutexApplyRender3DSettings);
	this->_3DSettingsPending.SpecialZeroAlphaBlendEnable = (theState) ? 1 : 0;
	slock_unlock(this->_mutexApplyRender3DSettings);
}

bool ClientGraphicsControl::Get3DSpecialZeroAlphaBlendingEnable()
{
	return (this->_3DSettingsPending.SpecialZeroAlphaBlendEnable != 0);
}

void ClientGraphicsControl::Set3DNDSDepthCalculationEnable(bool theState)
{
	slock_lock(this->_mutexApplyRender3DSettings);
	this->_3DSettingsPending.NDSStyleDepthCalculationEnable = (theState) ? 1 : 0;
	slock_unlock(this->_mutexApplyRender3DSettings);
}

bool ClientGraphicsControl::Get3DNDSDepthCalculationEnable()
{
	return (this->_3DSettingsPending.NDSStyleDepthCalculationEnable != 0);
}

void ClientGraphicsControl::Set3DDepthLEqualPolygonFacingEnable(bool theState)
{
	slock_lock(this->_mutexApplyRender3DSettings);
	this->_3DSettingsPending.DepthLEqualPolygonFacingEnable = (theState) ? 1 : 0;
	slock_unlock(this->_mutexApplyRender3DSettings);
}

bool ClientGraphicsControl::Get3DDepthLEqualPolygonFacingEnable()
{
	return (this->_3DSettingsPending.DepthLEqualPolygonFacingEnable != 0);
}

void ClientGraphicsControl::DidApplyGPUSettingsBegin()
{
	slock_lock(this->_mutexApplyGPUSettings);
	
	if (this->_did2DSettingsChange)
	{
		this->_2DSettingsApplied = this->_2DSettingsPending;
		this->_did2DSettingsChange = false;
		
		GPU->GetEngineMain()->SetEnableState((this->_2DSettingsApplied.MainEngineEnable != 0) ? true : false);
		GPU->GetEngineMain()->SetLayerEnableState(GPULayerID_BG0, (this->_2DSettingsApplied.MainLayerBG0Enable != 0) ? true : false);
		GPU->GetEngineMain()->SetLayerEnableState(GPULayerID_BG1, (this->_2DSettingsApplied.MainLayerBG1Enable != 0) ? true : false);
		GPU->GetEngineMain()->SetLayerEnableState(GPULayerID_BG2, (this->_2DSettingsApplied.MainLayerBG2Enable != 0) ? true : false);
		GPU->GetEngineMain()->SetLayerEnableState(GPULayerID_BG3, (this->_2DSettingsApplied.MainLayerBG3Enable != 0) ? true : false);
		GPU->GetEngineMain()->SetLayerEnableState(GPULayerID_OBJ, (this->_2DSettingsApplied.MainLayerOBJEnable != 0) ? true : false);
		
		GPU->GetEngineSub()->SetEnableState((this->_2DSettingsApplied.SubEngineEnable != 0) ? true : false);
		GPU->GetEngineSub()->SetLayerEnableState(GPULayerID_BG0, (this->_2DSettingsApplied.SubLayerBG0Enable != 0) ? true : false);
		GPU->GetEngineSub()->SetLayerEnableState(GPULayerID_BG1, (this->_2DSettingsApplied.SubLayerBG1Enable != 0) ? true : false);
		GPU->GetEngineSub()->SetLayerEnableState(GPULayerID_BG2, (this->_2DSettingsApplied.SubLayerBG2Enable != 0) ? true : false);
		GPU->GetEngineSub()->SetLayerEnableState(GPULayerID_BG3, (this->_2DSettingsApplied.SubLayerBG3Enable != 0) ? true : false);
		GPU->GetEngineSub()->SetLayerEnableState(GPULayerID_OBJ, (this->_2DSettingsApplied.SubLayerOBJEnable != 0) ? true : false);
	}
	
	if (this->_didWidthChange || this->_didHeightChange || this->_didColorFormatChange || this->_didPageCountChange)
	{
		GPU->SetCustomFramebufferSize(this->_widthPending, this->_heightPending);
		GPU->SetColorFormat(this->_colorFormatPending);
		GPU->SetFramebufferPageCount(this->_pageCountPending);
		
		this->_widthApplied = this->_widthPending;
		this->_heightApplied = this->_heightPending;
		this->_widthScaleApplied = this->_widthScalePending;
		this->_heightScaleApplied = this->_heightScalePending;
		this->_scaleFactorIntegerApplied = this->_scaleFactorIntegerPending;
		this->_colorFormatApplied = this->_colorFormatPending;
		this->_pageCountApplied = this->_pageCountPending;
	}
}

void ClientGraphicsControl::DidApplyGPUSettingsEnd()
{
	if (this->_didWidthChange || this->_didHeightChange || this->_didColorFormatChange || this->_didPageCountChange)
	{
		// Update all the change flags now that settings are applied.
		// In theory, all these flags should get cleared.
		this->_didWidthChange       = (this->_widthPending       != this->_widthApplied);
		this->_didHeightChange      = (this->_heightPending      != this->_heightApplied);
		this->_didColorFormatChange = (this->_colorFormatPending != this->_colorFormatApplied);
		this->_didPageCountChange   = (this->_pageCountPending   != this->_pageCountApplied);
	}
	
	slock_unlock(this->_mutexApplyGPUSettings);
}

void ClientGraphicsControl::DidApplyRender3DSettingsBegin()
{
	slock_lock(this->_mutexApplyRender3DSettings);
	
	const bool needChange3DEngine = (this->_engineIDPending != this->_engineIDApplied);
	this->_engineIDApplied = this->_engineIDPending;
	
	const bool needChangeThreadCount = ( (this->_3DSettingsPending.ThreadCount != this->_3DSettingsApplied.ThreadCount) && (this->_engineIDApplied == RENDERID_SOFTRASTERIZER) );
	this->_3DSettingsApplied.ThreadCount = this->_3DSettingsPending.ThreadCount;
	
	const bool need3DSettingsChange = (this->_3DSettingsPending.value != this->_3DSettingsApplied.value);
	this->_3DSettingsApplied = this->_3DSettingsPending;
	
	if (needChange3DEngine || needChangeThreadCount)
	{
		if (needChangeThreadCount)
		{
			if (this->_3DSettingsApplied.ThreadCount < 1)
			{
				this->_cpuCoreCountRestoreValue = 0;
			}
			else
			{
				this->_cpuCoreCountRestoreValue = CommonSettings.num_cores;
				CommonSettings.num_cores = this->_3DSettingsApplied.ThreadCount;
			}
		}
		
		core3DList[0] = &this->_3DEngineCallbackStruct;
		GPU->Set3DRendererByID(0);
	}
	
	if (need3DSettingsChange)
	{
		switch (this->_3DSettingsApplied.TextureScalingFactor)
		{
			case 1:
			case 2:
				CommonSettings.GFX3D_Renderer_TextureScalingFactor = this->_3DSettingsApplied.TextureScalingFactor;
				break;
				
			case 3: // Note that a value of "3" actually represents a 4x texture scaling factor.
				CommonSettings.GFX3D_Renderer_TextureScalingFactor = 4;
				break;
				
			default: // The only valid values are 1, 2, and 4. Any other value is considered invalid and should fall back to 1.
				CommonSettings.GFX3D_Renderer_TextureScalingFactor = 1;
				break;
		}
		
		CommonSettings.GFX3D_Texture                     = (this->_3DSettingsApplied.TextureEnable != 0);
		CommonSettings.GFX3D_Renderer_TextureDeposterize = (this->_3DSettingsApplied.TextureDeposterize != 0);
		
		CommonSettings.GFX3D_Renderer_MultisampleSize = (int)this->_3DSettingsApplied.MulitisampleSize;
		CommonSettings.GFX3D_EdgeMark                 = (this->_3DSettingsApplied.EdgeMarkEnable != 0);
		CommonSettings.GFX3D_Fog                      = (this->_3DSettingsApplied.FogEnable != 0);
		
		CommonSettings.GFX3D_HighResolutionInterpolateColor      = (this->_3DSettingsApplied.HighPrecisionColorInterpolation != 0);
		CommonSettings.GFX3D_LineHack                            = (this->_3DSettingsApplied.LineHackEnable != 0);
		CommonSettings.GFX3D_TXTHack                             = (this->_3DSettingsApplied.FragmentSamplingHackEnable != 0);
		CommonSettings.GFX3D_Renderer_TextureSmoothing           = (this->_3DSettingsApplied.TextureSmoothingEnable != 0);
		CommonSettings.OpenGL_Emulation_ShadowPolygon            = (this->_3DSettingsApplied.ShadowPolygonEnable != 0);
		CommonSettings.OpenGL_Emulation_SpecialZeroAlphaBlending = (this->_3DSettingsApplied.SpecialZeroAlphaBlendEnable != 0);
		CommonSettings.OpenGL_Emulation_NDSDepthCalculation      = (this->_3DSettingsApplied.NDSStyleDepthCalculationEnable != 0);
		CommonSettings.OpenGL_Emulation_DepthLEqualPolygonFacing = (this->_3DSettingsApplied.DepthLEqualPolygonFacingEnable != 0);
	}
}

void ClientGraphicsControl::_UpdateEngineClientProperties()
{
	this->_engineClientIDApplied   = this->_engineIDApplied;
	this->_engineClientNameApplied = CurrentRenderer->GetName();
}

void ClientGraphicsControl::DidApplyRender3DSettingsEnd()
{
	this->_UpdateEngineClientProperties();
	
	if (this->_cpuCoreCountRestoreValue > 0)
	{
		CommonSettings.num_cores = this->_cpuCoreCountRestoreValue;
	}
	
	this->_cpuCoreCountRestoreValue = 0;
	slock_unlock(this->_mutexApplyRender3DSettings);
}
