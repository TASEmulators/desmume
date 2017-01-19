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

#include <ft2build.h>
#include FT_FREETYPE_H

#define HUD_MAX_CHARACTERS							2048
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

struct NDSFrameInfo
{
	uint32_t videoFPS;
	uint32_t render3DFPS;
	uint32_t frameIndex;
	uint32_t lagFrameCount;
	uint32_t cpuLoadAvgARM9;
	uint32_t cpuLoadAvgARM7;
	char rtcString[25];
};
typedef struct NDSFrameInfo NDSFrameInfo;

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

class ClientDisplayView
{
private:
	void __InstanceInit(const ClientDisplayViewProperties &props);
	
protected:
	ClientDisplayViewProperties _renderProperty;
	ClientDisplayViewProperties _stagedProperty;
	InitialTouchPressMap *_initialTouchInMajorDisplay;
	
	bool _useDeposterize;
	VideoFilterTypeID _pixelScaler;
	OutputFilterTypeID _outputFilter;
	
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
	
	NDSDisplayInfo _emuDisplayInfo;
	NDSFrameInfo _emuFrameInfo;
	std::string _hudString;
	
	FT_Library _ftLibrary;
	const char *_lastFontFilePath;
	GlyphInfo _glyphInfo[256];
	size_t _glyphSize;
	size_t _glyphTileSize;
	
	void _UpdateHUDString();
	void _SetHUDShowInfoItem(bool &infoItemFlag, const bool visibleState);
	
	virtual void _UpdateNormalSize() = 0;
	virtual void _UpdateOrder() = 0;
	virtual void _UpdateRotation() = 0;
	virtual void _UpdateClientSize() = 0;
	virtual void _UpdateViewScale();
	
	virtual void _LoadNativeDisplayByID(const NDSDisplayID displayID);
	virtual void _LoadCustomDisplayByID(const NDSDisplayID displayID);
	
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
	
	// NDS screen filters
	bool GetSourceDeposterize();
	virtual void SetSourceDeposterize(const bool useDeposterize);
	OutputFilterTypeID GetOutputFilter() const;
	virtual void SetOutputFilter(const OutputFilterTypeID filterID);
	VideoFilterTypeID GetPixelScaler() const;
	virtual void SetPixelScaler(const VideoFilterTypeID filterID);
	
	// HUD appearance
	const char* GetHUDFontPath() const;
	void SetHUDFontPath(const char *filePath);
	virtual void LoadHUDFont();
	virtual void CopyHUDFont(const FT_Face &fontFace, const size_t glyphSize, const size_t glyphTileSize, GlyphInfo *glyphInfo);
	virtual void SetHUDInfo(const NDSFrameInfo &frameInfo);
	
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
	
	// NDS GPU interface
	virtual void LoadDisplays();
	virtual void ProcessDisplays();
	virtual void FrameProcessHUD();
	virtual void FrameRender() = 0;
	virtual void FrameFinish() = 0;
	
	// Emulator interface
	const NDSDisplayInfo& GetEmuDisplayInfo() const;
	virtual void HandleGPUFrameEndEvent(const NDSDisplayInfo &ndsDisplayInfo);
	virtual void HandleEmulatorFrameEndEvent(const NDSFrameInfo &frameInfo) = 0;
	
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
	
	virtual void SetVideoBuffers(const uint32_t colorFormat,
								 const void *videoBufferHead,
								 const void *nativeBuffer0,
								 const void *nativeBuffer1,
								 const void *customBuffer0, const size_t customWidth0, const size_t customHeight0,
								 const void *customBuffer1, const size_t customWidth1, const size_t customHeight1);
	
	void SetHUDVertices(float viewportWidth, float viewportHeight, float *vtxBufferPtr);
	void SetHUDTextureCoordinates(float *texCoordBufferPtr);
	void SetScreenVertices(float *vtxBufferPtr);
	void SetScreenTextureCoordinates(float w0, float h0, float w1, float h1, float *texCoordBufferPtr);
	
	virtual void UpdateView() = 0;
};

#endif // _CLIENT_DISPLAY_VIEW_H_
