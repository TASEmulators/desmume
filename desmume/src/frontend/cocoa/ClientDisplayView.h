/*
	Copyright (C) 2017 DeSmuME team
 
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

#ifndef _CLIENT_DISPLAY_VIEW_H_
#define _CLIENT_DISPLAY_VIEW_H_

#include <map>
#include <string>
#include "../../filter/videofilter.h"
#include "../../GPU.h"

#include "ClientExecutionControl.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#define HUD_MAX_CHARACTERS							2048
#define HUD_VERTEX_ATTRIBUTE_BUFFER_SIZE			(sizeof(float) * HUD_MAX_CHARACTERS * (2 * 4))
#define HUD_TEXTBOX_BASEGLYPHSIZE					64.0
#define HUD_TEXTBOX_BASE_SCALE						(1.0/3.0)
#define HUD_TEXTBOX_MIN_SCALE						0.70

#define DS_DISPLAY_VERTICAL_GAP_TO_HEIGHT_RATIO		(21.0/46.0) // Based on the official DS specification: 21mm/46mm
#define DS_DISPLAY_UNSCALED_GAP						(GPU_FRAMEBUFFER_NATIVE_HEIGHT * DS_DISPLAY_VERTICAL_GAP_TO_HEIGHT_RATIO)

enum ClientDisplayMode
{
	ClientDisplayMode_Main = 0,
	ClientDisplayMode_Touch,
	ClientDisplayMode_Dual
};

enum ClientDisplayLayout
{
	ClientDisplayLayout_Vertical		= 0,
	ClientDisplayLayout_Horizontal		= 1,
	ClientDisplayLayout_Hybrid_2_1		= 1000,
	ClientDisplayLayout_Hybrid_16_9		= 1001,
	ClientDisplayLayout_Hybrid_16_10	= 1002
};

enum ClientDisplayOrder
{
	ClientDisplayOrder_MainFirst = 0,
	ClientDisplayOrder_TouchFirst
};

enum ClientDisplaySource
{
	ClientDisplaySource_None			= 0,
	ClientDisplaySource_DeterminedByNDS	= 1,
	ClientDisplaySource_EngineMain		= 2,
	ClientDisplaySource_EngineSub		= 3
};

enum OutputFilterTypeID
{
	OutputFilterTypeID_NearestNeighbor	= 0,
	OutputFilterTypeID_Bilinear			= 1,
	OutputFilterTypeID_BicubicBSpline	= 2,
	OutputFilterTypeID_BicubicMitchell	= 3,
	OutputFilterTypeID_Lanczos2			= 4,
	OutputFilterTypeID_Lanczos3			= 5
};

typedef std::map<int, bool> InitialTouchPressMap;  // Key = An ID number of the host input, Value = Flag that indicates if the initial touch press was in the major display

struct GlyphInfo
{
	float width;
	float texCoord[8];
};
typedef struct GlyphInfo GlyphInfo;

struct LUTValues
{
	uint8_t p0;
	uint8_t p1;
	uint8_t p2;
	uint8_t pad0;
	uint8_t w0;
	uint8_t w1;
	uint8_t w2;
	uint8_t pad1;
};
typedef struct LUTValues LUTValues;

struct ClientFrameInfo
{
	uint32_t videoFPS;
};
typedef struct ClientFrameInfo ClientFrameInfo;

struct ClientDisplayViewProperties
{
	ClientDisplayMode mode;
	ClientDisplayLayout layout;
	ClientDisplayOrder order;
	double gapScale;
	double rotation;
	double clientWidth;
	double clientHeight;
	
	double normalWidth;
	double normalHeight;
	double viewScale;
	double gapDistance;
};
typedef struct ClientDisplayViewProperties ClientDisplayViewProperties;

extern LUTValues *_LQ2xLUT;
extern LUTValues *_HQ2xLUT;
extern LUTValues *_HQ3xLUT;
extern LUTValues *_HQ4xLUT;
void InitHQnxLUTs();

class ClientDisplayView
{
private:
	void __InstanceInit(const ClientDisplayViewProperties &props);
	
protected:
	ClientDisplayViewProperties _renderProperty;
	ClientDisplayViewProperties _stagedProperty;
	InitialTouchPressMap *_initialTouchInMajorDisplay;
	GPUClientFetchObject *_fetchObject;
	
	bool _useDeposterize;
	VideoFilterTypeID _pixelScaler;
	OutputFilterTypeID _outputFilter;
	
	ClientDisplaySource _displaySourceSelect[2];
	bool _isSelectedDisplayEnabled[2];
	NDSDisplayID _selectedSourceForDisplay[2];
	
	bool _useVerticalSync;
	double _scaleFactor;
	
	double _hudObjectScale;
	bool _isHUDVisible;
	bool _showVideoFPS;
	bool _showRender3DFPS;
	bool _showFrameIndex;
	bool _showLagFrameCount;
	bool _showCPULoadAverage;
	bool _showRTC;
	
	ClientFrameInfo _clientFrameInfo;
	NDSFrameInfo _ndsFrameInfo;
	
	NDSDisplayInfo _emuDisplayInfo;
	std::string _hudString;
	bool _hudNeedsUpdate;
	bool _allowViewUpdates;
	
	FT_Library _ftLibrary;
	const char *_lastFontFilePath;
	GlyphInfo _glyphInfo[256];
	size_t _glyphSize;
	size_t _glyphTileSize;
	
	uint32_t *_vfMasterDstBuffer;
	size_t _vfMasterDstBufferSize;
	VideoFilter *_vf[2];
	
	void _UpdateHUDString();
	void _SetHUDShowInfoItem(bool &infoItemFlag, const bool visibleState);
	
	virtual void _UpdateNormalSize() = 0;
	virtual void _UpdateOrder() = 0;
	virtual void _UpdateRotation() = 0;
	virtual void _UpdateClientSize();
	virtual void _UpdateViewScale();
	
	virtual void _LoadNativeDisplayByID(const NDSDisplayID displayID);
	virtual void _LoadCustomDisplayByID(const NDSDisplayID displayID);
	
	virtual void _ResizeCPUPixelScaler(const VideoFilterTypeID filterID);
	
public:
	ClientDisplayView();
	ClientDisplayView(const ClientDisplayViewProperties &props);
	virtual ~ClientDisplayView();
	
	virtual void Init();
	
	bool GetUseVerticalSync() const;
	virtual void SetUseVerticalSync(const bool useVerticalSync);
	double GetScaleFactor() const;
	virtual void SetScaleFactor(const double scaleFactor);
		
	// NDS screen layout
	const ClientDisplayViewProperties& GetViewProperties() const;
	void CommitViewProperties(const ClientDisplayViewProperties &props);
	virtual void SetupViewProperties();
	
	double GetRotation() const;
	double GetViewScale() const;
	ClientDisplayMode GetMode() const;
	ClientDisplayLayout GetLayout() const;
	ClientDisplayOrder GetOrder() const;
	double GetGapScale() const;
	double GetGapDistance() const;
	
	ClientDisplaySource GetDisplayVideoSource(const NDSDisplayID displayID) const;
	virtual void SetDisplayVideoSource(const NDSDisplayID displayID, ClientDisplaySource displaySrc);
	NDSDisplayID GetSelectedDisplaySourceForDisplay(const NDSDisplayID displayID) const;
	bool IsSelectedDisplayEnabled(const NDSDisplayID displayID) const;
	
	// NDS screen filters
	bool GetSourceDeposterize();
	virtual void SetSourceDeposterize(const bool useDeposterize);
	OutputFilterTypeID GetOutputFilter() const;
	virtual void SetOutputFilter(const OutputFilterTypeID filterID);
	VideoFilterTypeID GetPixelScaler() const;
	virtual void SetPixelScaler(const VideoFilterTypeID filterID);
	VideoFilter* GetPixelScalerObject(const NDSDisplayID displayID);
	
	// HUD appearance
	const char* GetHUDFontPath() const;
	void SetHUDFontPath(const char *filePath);
	virtual void LoadHUDFont();
	virtual void CopyHUDFont(const FT_Face &fontFace, const size_t glyphSize, const size_t glyphTileSize, GlyphInfo *glyphInfo);
	virtual void SetHUDInfo(const ClientFrameInfo &clientFrameInfo, const NDSFrameInfo &ndsFrameInfo);
	
	const std::string& GetHUDString() const;
	float GetHUDObjectScale() const;
	virtual void SetHUDObjectScale(float objectScale);
	
	bool GetHUDVisibility() const;
	virtual void SetHUDVisibility(const bool visibleState);
	bool GetHUDShowVideoFPS() const;
	virtual void SetHUDShowVideoFPS(const bool visibleState);
	bool GetHUDShowRender3DFPS() const;
	virtual void SetHUDShowRender3DFPS(const bool visibleState);
	bool GetHUDShowFrameIndex() const;
	virtual void SetHUDShowFrameIndex(const bool visibleState);
	bool GetHUDShowLagFrameCount() const;
	virtual void SetHUDShowLagFrameCount(const bool visibleState);
	bool GetHUDShowCPULoadAverage() const;
	virtual void SetHUDShowCPULoadAverage(const bool visibleState);
	bool GetHUDShowRTC() const;
	virtual void SetHUDShowRTC(const bool visibleState);
	bool HUDNeedsUpdate() const;
	void ClearHUDNeedsUpdate();
	
	// Client view interface
	const GPUClientFetchObject& GetFetchObject() const;
	void SetFetchObject(GPUClientFetchObject *fetchObject);
	
	bool GetAllowViewUpdates() const;
	void SetAllowViewUpdates(const bool allowUpdates);
	
	virtual void LoadDisplays();
	virtual void ProcessDisplays();
	virtual void UpdateView();
	virtual void FinishFrameAtIndex(const u8 bufferIndex);
	
	// Emulator interface
	const NDSDisplayInfo& GetEmuDisplayInfo() const;
	void SetEmuDisplayInfo(const NDSDisplayInfo &ndsDisplayInfo);
	virtual void HandleEmulatorFrameEndEvent();
	
	// Touch screen input handling
	void GetNDSPoint(const int inputID, const bool isInitialTouchPress,
					 const double clientX, const double clientY,
					 u8 &outX, u8 &outY) const;
	
	// Utility methods
	static void ConvertNormalToTransformedBounds(const double scalar,
												 const double angleDegrees,
												 double &inoutWidth, double &inoutHeight);
	
	static double GetMaxScalarWithinBounds(const double normalBoundsWidth, const double normalBoundsHeight,
										   const double keepInBoundsWidth, const double keepInBoundsHeight);
	
	static void ConvertClientToNormalPoint(const double normalBoundsWidth, const double normalBoundsHeight,
										   const double transformBoundsWidth, const double transformBoundsHeight,
										   const double scalar,
										   const double angleDegrees,
										   double &inoutX, double &inoutY);
	
	static void CalculateNormalSize(const ClientDisplayMode mode, const ClientDisplayLayout layout, const double gapScale,
									double &outWidth, double &outHeight);
};

class ClientDisplay3DView : public ClientDisplayView
{
protected:
	bool _canFilterOnGPU;
	bool _willFilterOnGPU;
	bool _filtersPreferGPU;
	
public:
	ClientDisplay3DView();
	
	bool CanFilterOnGPU() const;
	bool GetFiltersPreferGPU() const;
	virtual void SetFiltersPreferGPU(const bool preferGPU);
	bool WillFilterOnGPU() const;
	
	virtual void SetSourceDeposterize(const bool useDeposterize);
	
	void SetHUDVertices(float viewportWidth, float viewportHeight, float *vtxBufferPtr);
	void SetHUDTextureCoordinates(float *texCoordBufferPtr);
	void SetScreenVertices(float *vtxBufferPtr);
	void SetScreenTextureCoordinates(float w0, float h0, float w1, float h1, float *texCoordBufferPtr);
};

#endif // _CLIENT_DISPLAY_VIEW_H_
