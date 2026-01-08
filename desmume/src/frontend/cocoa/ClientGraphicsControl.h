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

#ifndef _CLIENT_GRAPHICS_CONTROL_
#define _CLIENT_GRAPHICS_CONTROL_

#include <string>
#include "rthreads.h"
#include "../../GPU.h"
#include "../../render3D.h"


union Client2DGraphicsSettings
{
	uint32_t value;
	
	struct
	{
#ifndef MSB_FIRST
		uint8_t MainEngineEnable:1;
		uint8_t MainLayerBG0Enable:1;
		uint8_t MainLayerBG1Enable:1;
		uint8_t MainLayerBG2Enable:1;
		uint8_t MainLayerBG3Enable:1;
		uint8_t MainLayerOBJEnable:1;
		uint8_t SubEngineEnable:1;
		uint8_t SubLayerBG0Enable:1;
		
		uint8_t SubLayerBG1Enable:1;
		uint8_t SubLayerBG2Enable:1;
		uint8_t SubLayerBG3Enable:1;
		uint8_t SubLayerOBJEnable:1;
		uint8_t :1;
		uint8_t :1;
		uint8_t :1;
		uint8_t :1;
		
		uint8_t :8;
		uint8_t :8;
#else
		uint8_t SubLayerBG0Enable:1;
		uint8_t SubEngineEnable:1;
		uint8_t MainLayerOBJEnable:1;
		uint8_t MainLayerBG3Enable:1;
		uint8_t MainLayerBG2Enable:1;
		uint8_t MainLayerBG1Enable:1;
		uint8_t MainLayerBG0Enable:1;
		uint8_t MainEngineEnable:1;
		
		uint8_t :1;
		uint8_t :1;
		uint8_t :1;
		uint8_t :1;
		uint8_t SubLayerOBJEnable:1;
		uint8_t SubLayerBG3Enable:1;
		uint8_t SubLayerBG2Enable:1;
		uint8_t SubLayerBG1Enable:1;
		
		uint8_t :8;
		uint8_t :8;
#endif
	};
};
typedef union Client2DGraphicsSettings Client2DGraphicsSettings;

union Client3DRenderSettings
{
	uint32_t value;
	
	struct
	{
#ifndef MSB_FIRST
		uint8_t RenderScalingFactor:6;
		uint8_t TextureScalingFactor:2;
		
		uint8_t ThreadCount:6;
		uint8_t TextureEnable:1;
		uint8_t TextureDeposterize:1;
		
		uint8_t MulitisampleSize:6;
		uint8_t EdgeMarkEnable:1;
		uint8_t FogEnable:1;
		
		uint8_t HighPrecisionColorInterpolation:1;
		uint8_t LineHackEnable:1;
		uint8_t FragmentSamplingHackEnable:1;
		uint8_t TextureSmoothingEnable:1;
		uint8_t ShadowPolygonEnable:1;
		uint8_t SpecialZeroAlphaBlendEnable:1;
		uint8_t NDSStyleDepthCalculationEnable:1;
		uint8_t DepthLEqualPolygonFacingEnable:1;
#else
		uint8_t TextureScalingFactor:2;
		uint8_t RenderScalingFactor:6;
		
		uint8_t TextureDeposterize:1;
		uint8_t TextureEnable:1;
		uint8_t ThreadCount:6;
		
		uint8_t FogEnable:1;
		uint8_t EdgeMarkEnable:1;
		uint8_t MulitisampleSize:6;
		
		uint8_t DepthLEqualPolygonFacingEnable:1;
		uint8_t NDSStyleDepthCalculationEnable:1;
		uint8_t SpecialZeroAlphaBlendEnable:1;
		uint8_t ShadowPolygonEnable:1;
		uint8_t TextureSmoothingEnable:1;
		uint8_t FragmentSamplingHackEnable:1;
		uint8_t LineHackEnable:1;
		uint8_t HighPrecisionColorInterpolation:1;
#endif
	};
};
typedef union Client3DRenderSettings Client3DRenderSettings;

class ClientGraphicsControl : public GPUEventHandlerDefault
{
protected:
	void *_clientObject;
	GPUClientFetchObject *_fetchObject;
	
	Client2DGraphicsSettings _2DSettingsPending;
	Client2DGraphicsSettings _2DSettingsApplied;
	Client3DRenderSettings _3DSettingsPending;
	Client3DRenderSettings _3DSettingsApplied;
	
	int _engineIDPending;
	int _engineIDApplied;
	
	size_t _pageCountPending;
	size_t _pageCountApplied;
	
	size_t _widthPending;
	size_t _widthApplied;
	size_t _heightPending;
	size_t _heightApplied;
	
	float _widthScalePending;
	float _widthScaleApplied;
	float _heightScalePending;
	float _heightScaleApplied;
	
	size_t _scaleFactorIntegerPending;
	size_t _scaleFactorIntegerApplied;
	
	NDSColorFormat _colorFormatPending;
	NDSColorFormat _colorFormatApplied;
	
	bool _did2DSettingsChange;
	bool _didColorFormatChange;
	bool _didWidthChange;
	bool _didHeightChange;
	bool _didPageCountChange;
	
	bool _isThreadCountAuto;
	int _cpuCoreCountRestoreValue;
	GPU3DInterface _3DEngineCallbackStruct;
	
	int64_t _engineClientIDApplied;
	std::string _engineClientNameApplied;
	std::string _engineClientNameAppliedCopy;
	
	uint8_t _multisampleMaxClientSize;
	char _multisampleSizeString[16];
	
	slock_t *_mutexApplyGPUSettings;
	slock_t *_mutexApplyRender3DSettings;
	
	virtual void _UpdateEngineClientProperties();
	
public:
	ClientGraphicsControl();
	virtual ~ClientGraphicsControl();
	
	void SetClientObject(void *clientObj);
	void* GetClientObject();
	
	void SetFetchObject(GPUClientFetchObject *fetchObject);
	GPUClientFetchObject* GetFetchObject() const;
	
	void SetFramebufferPageCount(size_t pageCount);
	size_t GetFramebufferPageCount();
	
	void SetFramebufferDimensions(size_t w, size_t h);
	void GetFramebufferDimensions(size_t &w, size_t &h);
	void SetFramebufferDimensionsByScaleFactor(float widthScale, float heightScale);
	void GetFramebufferDimensionsByScaleFactor(float &widthScale, float &heightScale);
	void SetFramebufferDimensionsByScaleFactorInteger(size_t scaleFactor);
	size_t GetFramebufferDimensionsByScaleFactorInteger();
	
	void SetColorFormat(NDSColorFormat colorFormat);
	NDSColorFormat GetColorFormat();
	
	void Set2DGraphicsSettings(Client2DGraphicsSettings settings2D);
	Client2DGraphicsSettings Get2DGraphicsSettings();
	
	void SetMainEngineEnable(bool theState);
	bool GetMainEngineEnable();
	
	void SetSubEngineEnable(bool theState);
	bool GetSubEngineEnable();
	
	void Set2DLayerEnableMainBG0(bool theState);
	bool Get2DLayerEnableMainBG0();
	
	void Set2DLayerEnableMainBG1(bool theState);
	bool Get2DLayerEnableMainBG1();
	
	void Set2DLayerEnableMainBG2(bool theState);
	bool Get2DLayerEnableMainBG2();
	
	void Set2DLayerEnableMainBG3(bool theState);
	bool Get2DLayerEnableMainBG3();
	
	void Set2DLayerEnableMainOBJ(bool theState);
	bool Get2DLayerEnableMainOBJ();
	
	void Set2DLayerEnableSubBG0(bool theState);
	bool Get2DLayerEnableSubBG0();
	
	void Set2DLayerEnableSubBG1(bool theState);
	bool Get2DLayerEnableSubBG1();
	
	void Set2DLayerEnableSubBG2(bool theState);
	bool Get2DLayerEnableSubBG2();
	
	void Set2DLayerEnableSubBG3(bool theState);
	bool Get2DLayerEnableSubBG3();
	
	void Set2DLayerEnableSubOBJ(bool theState);
	bool Get2DLayerEnableSubOBJ();
	
	virtual void Set3DEngineByID(int engineID);
	int Get3DEngineByID();
	int Get3DEngineByIDApplied();
	int64_t Get3DEngineClientID();
	const char* Get3DEngineClientName();
	
	void Set3DScalingFactor(uint8_t scalingFactor);
	uint8_t Get3DScalingFactor();
	
	void Set3DTextureScalingFactor(uint8_t scalingFactor);
	uint8_t Get3DTextureScalingFactor();
	
	void Set3DTextureDeposterize(bool theState);
	bool Get3DTextureDeposterize();
	
	void Set3DTextureEnable(bool theState);
	bool Get3DTextureEnable();
	
	void Set3DEdgeMarkEnable(bool theState);
	bool Get3DEdgeMarkEnable();
	
	void Set3DFogEnable(bool theState);
	bool Get3DFogEnable();
	
	void Set3DHighPrecisionColorInterpolation(bool theState);
	bool Get3DHighPrecisionColorInterpolation();
	
	void Set3DLineHackEnable(bool theState);
	bool Get3DLineHackEnable();
	
	void Set3DFragmentSamplingHackEnable(bool theState);
	bool Get3DFragmentSamplingHackEnable();
	
	void Set3DRenderingThreadCount(uint8_t threadCount);
	uint8_t Get3DRenderingThreadCount();
	
	void Set3DMultisampleMaxClientSize(uint8_t maxClientSize);
	uint8_t Get3DMultisampleMaxClientSize();
	
	uint8_t Set3DMultisampleSize(uint8_t msaaSize);
	uint8_t Get3DMultisampleSize();
	const char* Get3DMultisampleSizeString();
	
	void Set3DTextureSmoothingEnable(bool theState);
	bool Get3DTextureSmoothingEnable();
	
	void Set3DShadowPolygonEnable(bool theState);
	bool Get3DShadowPolygonEnable();
	
	void Set3DSpecialZeroAlphaBlendingEnable(bool theState);
	bool Get3DSpecialZeroAlphaBlendingEnable();
	
	void Set3DNDSDepthCalculationEnable(bool theState);
	bool Get3DNDSDepthCalculationEnable();
	
	void Set3DDepthLEqualPolygonFacingEnable(bool theState);
	bool Get3DDepthLEqualPolygonFacingEnable();
	
	// GPUEventHandlerDefault methods
	virtual void DidApplyGPUSettingsBegin();
	virtual void DidApplyGPUSettingsEnd();
	virtual void DidApplyRender3DSettingsBegin();
	virtual void DidApplyRender3DSettingsEnd();
};

#endif // _CLIENT_GRAPHICS_CONTROL_
