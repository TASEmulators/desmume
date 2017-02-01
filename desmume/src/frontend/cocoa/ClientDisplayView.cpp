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

#include "ClientDisplayView.h"

#include <sstream>

ClientDisplayView::ClientDisplayView()
{
	_fetchObject = NULL;
	
	ClientDisplayViewProperties defaultProperty;
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

ClientDisplayView::ClientDisplayView(const ClientDisplayViewProperties &props)
{
	__InstanceInit(props);
}

ClientDisplayView::~ClientDisplayView()
{
	delete _initialTouchInMajorDisplay;
	_initialTouchInMajorDisplay = NULL;
	
	if (this->_ftLibrary != NULL)
	{
		FT_Done_FreeType(this->_ftLibrary);
		this->_ftLibrary = NULL;
	}
}

void ClientDisplayView::__InstanceInit(const ClientDisplayViewProperties &props)
{
	_stagedProperty = props;
	_renderProperty = _stagedProperty;
	_initialTouchInMajorDisplay = new InitialTouchPressMap;
	
	_useDeposterize = false;
	_pixelScaler = VideoFilterTypeID_None;
	_outputFilter = OutputFilterTypeID_Bilinear;
	
	_useVerticalSync = false;
	_scaleFactor = 1.0;
	
	_hudObjectScale = 1.0;
	_isHUDVisible = true;
	_showVideoFPS = true;
	_showRender3DFPS = false;
	_showFrameIndex = false;
	_showLagFrameCount = false;
	_showCPULoadAverage = false;
	_showRTC = false;
	
	memset(&_emuDisplayInfo, 0, sizeof(_emuDisplayInfo));
	memset(&_emuFrameInfo, 0, sizeof(_emuFrameInfo));
	_hudString = "\x01"; // Char value 0x01 will represent the "text box" character, which will always be first in the string.
	_hudNeedsUpdate = true;
	_allowViewUpdates = true;
	
	FT_Error error = FT_Init_FreeType(&_ftLibrary);
	if (error)
	{
		printf("ClientDisplayView: FreeType failed to init!\n");
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
}

void ClientDisplayView::_UpdateHUDString()
{
	std::ostringstream ss;
	ss << "\x01"; // This represents the text box. It must always be the first character.
	
	if (this->_showVideoFPS)
	{
		ss << "Video FPS: " << this->_emuFrameInfo.videoFPS << "\n";
	}
	
	if (this->_showRender3DFPS)
	{
		ss << "3D Rendering FPS: " << this->_emuFrameInfo.render3DFPS << "\n";
	}
	
	if (this->_showFrameIndex)
	{
		ss << "Frame Index: " << this->_emuFrameInfo.frameIndex << "\n";
	}
	
	if (this->_showLagFrameCount)
	{
		ss << "Lag Frame Count: " << this->_emuFrameInfo.lagFrameCount << "\n";
	}
	
	if (this->_showCPULoadAverage)
	{
		static char buffer[32];
		memset(buffer, 0, sizeof(buffer));
		snprintf(buffer, 25, "CPU Load Avg: %02d%% / %02d%%\n", this->_emuFrameInfo.cpuLoadAvgARM9, this->_emuFrameInfo.cpuLoadAvgARM7);
		
		ss << buffer;
	}
	
	if (this->_showRTC)
	{
		ss << "RTC: " << this->_emuFrameInfo.rtcString << "\n";
	}
	
	this->_hudString = ss.str();
	this->_hudNeedsUpdate = true;
}

void ClientDisplayView::_SetHUDShowInfoItem(bool &infoItemFlag, const bool visibleState)
{
	if (infoItemFlag == visibleState)
	{
		return;
	}
	
	infoItemFlag = visibleState;
	this->_UpdateHUDString();
}

void ClientDisplayView::Init()
{
	// Do nothing. This is implementation dependent.
}

bool ClientDisplayView::GetUseVerticalSync() const
{
	return this->_useVerticalSync;
}

void ClientDisplayView::SetUseVerticalSync(const bool useVerticalSync)
{
	this->_useVerticalSync = useVerticalSync;
}

double ClientDisplayView::GetScaleFactor() const
{
	return this->_scaleFactor;
}

void ClientDisplayView::SetScaleFactor(const double scaleFactor)
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

void ClientDisplayView::_UpdateClientSize()
{
	this->_hudNeedsUpdate = true;
}

void ClientDisplayView::_UpdateViewScale()
{
	double checkWidth = this->_renderProperty.normalWidth;
	double checkHeight = this->_renderProperty.normalHeight;
	ClientDisplayView::ConvertNormalToTransformedBounds(1.0, this->_renderProperty.rotation, checkWidth, checkHeight);
	this->_renderProperty.viewScale = ClientDisplayView::GetMaxScalarWithinBounds(checkWidth, checkHeight, this->_renderProperty.clientWidth, this->_renderProperty.clientHeight);
	
	this->_hudObjectScale = this->_renderProperty.clientWidth / this->_renderProperty.normalWidth;
	if (this->_hudObjectScale > 2.0)
	{
		// If the view scale is <= 2.0, we scale the HUD objects linearly. Otherwise, we scale
		// the HUD objects logarithmically, up to a maximum scale of 3.0.
		this->_hudObjectScale = ( -1.0/((1.0/12000.0)*pow(this->_hudObjectScale+4.5438939, 5.0)) ) + 3.0;
	}
}

// NDS screen layout
const ClientDisplayViewProperties& ClientDisplayView::GetViewProperties() const
{
	return this->_renderProperty;
}

void ClientDisplayView::CommitViewProperties(const ClientDisplayViewProperties &props)
{
	this->_stagedProperty = props;
}

void ClientDisplayView::SetupViewProperties()
{
	// Validate the staged properties.
	this->_stagedProperty.gapDistance = (double)DS_DISPLAY_UNSCALED_GAP * this->_stagedProperty.gapScale;
	ClientDisplayView::CalculateNormalSize(this->_stagedProperty.mode, this->_stagedProperty.layout, this->_stagedProperty.gapScale, this->_stagedProperty.normalWidth, this->_stagedProperty.normalHeight);
	
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
	}
	
	if (didRotationChange)
	{
		this->_UpdateRotation();
	}
	
	if (didClientSizeChange)
	{
		this->_UpdateClientSize();
	}
	
	this->UpdateView();
}

double ClientDisplayView::GetRotation() const
{
	return this->_renderProperty.rotation;
}

double ClientDisplayView::GetViewScale() const
{
	return this->_renderProperty.viewScale;
}

ClientDisplayMode ClientDisplayView::GetMode() const
{
	return this->_renderProperty.mode;
}

ClientDisplayLayout ClientDisplayView::GetLayout() const
{
	return this->_renderProperty.layout;
}

ClientDisplayOrder ClientDisplayView::GetOrder() const
{
	return this->_renderProperty.order;
}

double ClientDisplayView::GetGapScale() const
{
	return this->_renderProperty.gapScale;
}

double ClientDisplayView::GetGapDistance() const
{
	return this->_renderProperty.gapDistance;
}

// NDS screen filters
bool ClientDisplayView::GetSourceDeposterize()
{
	return this->_useDeposterize;
}

void ClientDisplayView::SetSourceDeposterize(const bool useDeposterize)
{
	this->_useDeposterize = useDeposterize;
}

OutputFilterTypeID ClientDisplayView::GetOutputFilter() const
{
	return this->_outputFilter;
}

void ClientDisplayView::SetOutputFilter(const OutputFilterTypeID filterID)
{
	this->_outputFilter = filterID;
}

VideoFilterTypeID ClientDisplayView::GetPixelScaler() const
{
	return this->_pixelScaler;
}

void ClientDisplayView::SetPixelScaler(const VideoFilterTypeID filterID)
{
	this->_pixelScaler = filterID;
}

// HUD appearance
const char* ClientDisplayView::GetHUDFontPath() const
{
	return this->_lastFontFilePath;
}

void ClientDisplayView::SetHUDFontPath(const char *filePath)
{
	this->_lastFontFilePath = filePath;
}

void ClientDisplayView::LoadHUDFont()
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
		printf("ClientDisplayView: FreeType failed to load font face because it is in an unknown format from:\n%s\n", this->_lastFontFilePath);
		return;
	}
	else if (error)
	{
		printf("ClientDisplayView: FreeType failed to load font face with an unknown error from:\n%s\n", this->_lastFontFilePath);
		return;
	}
	
	this->CopyHUDFont(fontFace, this->_glyphSize, this->_glyphTileSize, this->_glyphInfo);
	
	FT_Done_Face(fontFace);
}

void ClientDisplayView::CopyHUDFont(const FT_Face &fontFace, const size_t glyphSize, const size_t glyphTileSize, GlyphInfo *glyphInfo)
{
	// Do nothing. This is implementation dependent.
}

void ClientDisplayView::SetHUDInfo(const NDSFrameInfo &frameInfo)
{
	this->_emuFrameInfo = frameInfo;
	this->_UpdateHUDString();
}

const std::string& ClientDisplayView::GetHUDString() const
{
	return this->_hudString;
}

float ClientDisplayView::GetHUDObjectScale() const
{
	return this->_hudObjectScale;
}

void ClientDisplayView::SetHUDObjectScale(float objectScale)
{
	this->_hudObjectScale = objectScale;
}

bool ClientDisplayView::GetHUDVisibility() const
{
	return this->_isHUDVisible;
}

void ClientDisplayView::SetHUDVisibility(const bool visibleState)
{
	this->_isHUDVisible = visibleState;
	this->UpdateView();
}

bool ClientDisplayView::GetHUDShowVideoFPS() const
{
	return this->_showVideoFPS;
}

void ClientDisplayView::SetHUDShowVideoFPS(const bool visibleState)
{
	this->_SetHUDShowInfoItem(this->_showVideoFPS, visibleState);
	this->UpdateView();
}

bool ClientDisplayView::GetHUDShowRender3DFPS() const
{
	return this->_showRender3DFPS;
}

void ClientDisplayView::SetHUDShowRender3DFPS(const bool visibleState)
{
	this->_SetHUDShowInfoItem(this->_showRender3DFPS, visibleState);
	this->UpdateView();
}

bool ClientDisplayView::GetHUDShowFrameIndex() const
{
	return this->_showFrameIndex;
}

void ClientDisplayView::SetHUDShowFrameIndex(const bool visibleState)
{
	this->_SetHUDShowInfoItem(this->_showFrameIndex, visibleState);
	this->UpdateView();
}

bool ClientDisplayView::GetHUDShowLagFrameCount() const
{
	return this->_showLagFrameCount;
}

void ClientDisplayView::SetHUDShowLagFrameCount(const bool visibleState)
{
	this->_SetHUDShowInfoItem(this->_showLagFrameCount, visibleState);
	this->UpdateView();
}

bool ClientDisplayView::GetHUDShowCPULoadAverage() const
{
	return this->_showCPULoadAverage;
}

void ClientDisplayView::SetHUDShowCPULoadAverage(const bool visibleState)
{
	this->_SetHUDShowInfoItem(this->_showCPULoadAverage, visibleState);
	this->UpdateView();
}

bool ClientDisplayView::GetHUDShowRTC() const
{
	return this->_showRTC;
}

void ClientDisplayView::SetHUDShowRTC(const bool visibleState)
{
	this->_SetHUDShowInfoItem(this->_showRTC, visibleState);
	this->UpdateView();
}

bool ClientDisplayView::HUDNeedsUpdate() const
{
	return this->_hudNeedsUpdate;
}

void ClientDisplayView::ClearHUDNeedsUpdate()
{
	this->_hudNeedsUpdate = false;
}

// NDS GPU Interface
const GPUClientFetchObject& ClientDisplayView::GetFetchObject() const
{
	return *this->_fetchObject;
}

void ClientDisplayView::SetFetchObject(GPUClientFetchObject *fetchObject)
{
	this->_fetchObject = fetchObject;
}

bool ClientDisplayView::GetAllowViewUpdates() const
{
	return this->_allowViewUpdates;
}

void ClientDisplayView::SetAllowViewUpdates(const bool allowUpdates)
{
	this->_allowViewUpdates = allowUpdates;
}

void ClientDisplayView::_LoadNativeDisplayByID(const NDSDisplayID displayID)
{
	// Do nothing. This is implementation dependent.
}

void ClientDisplayView::_LoadCustomDisplayByID(const NDSDisplayID displayID)
{
	// Do nothing. This is implementation dependent.
}

void ClientDisplayView::LoadDisplays()
{
	this->_emuDisplayInfo = this->_fetchObject->GetFetchDisplayInfoForBufferIndex(this->_fetchObject->GetLastFetchIndex());
	
	const bool loadMainScreen = this->_emuDisplayInfo.isDisplayEnabled[NDSDisplayID_Main] && ((this->_renderProperty.mode == ClientDisplayMode_Main) || (this->_renderProperty.mode == ClientDisplayMode_Dual));
	const bool loadTouchScreen = this->_emuDisplayInfo.isDisplayEnabled[NDSDisplayID_Touch] && ((this->_renderProperty.mode == ClientDisplayMode_Touch) || (this->_renderProperty.mode == ClientDisplayMode_Dual));
	
	if (loadMainScreen)
	{
		if (!this->_emuDisplayInfo.didPerformCustomRender[NDSDisplayID_Main])
		{
			this->_LoadNativeDisplayByID(NDSDisplayID_Main);
		}
		else
		{
			this->_LoadCustomDisplayByID(NDSDisplayID_Main);
		}
	}
	
	if (loadTouchScreen)
	{
		if (!this->_emuDisplayInfo.didPerformCustomRender[NDSDisplayID_Touch])
		{
			this->_LoadNativeDisplayByID(NDSDisplayID_Touch);
		}
		else
		{
			this->_LoadCustomDisplayByID(NDSDisplayID_Touch);
		}
	}
}

void ClientDisplayView::ProcessDisplays()
{
	// Do nothing. This is implementation dependent.
}

void ClientDisplayView::UpdateView()
{
	// Do nothing. This is implementation dependent.
}

const NDSDisplayInfo& ClientDisplayView::GetEmuDisplayInfo() const
{
	return this->_emuDisplayInfo;
}

void ClientDisplayView::SetEmuDisplayInfo(const NDSDisplayInfo &ndsDisplayInfo)
{
	this->_emuDisplayInfo = ndsDisplayInfo;
}

void ClientDisplayView::HandleEmulatorFrameEndEvent(const NDSFrameInfo &frameInfo)
{
	this->SetHUDInfo(frameInfo);
	this->UpdateView();
}

// Touch screen input handling
void ClientDisplayView::GetNDSPoint(const int inputID, const bool isInitialTouchPress,
									const double clientX, const double clientY,
									u8 &outX, u8 &outY) const
{
	double x = clientX;
	double y = clientY;
	double w = this->_renderProperty.normalWidth;
	double h = this->_renderProperty.normalHeight;
	
	ClientDisplayView::ConvertNormalToTransformedBounds(1.0, this->_renderProperty.rotation, w, h);
	const double s = ClientDisplayView::GetMaxScalarWithinBounds(w, h, this->_renderProperty.clientWidth, this->_renderProperty.clientHeight);
	
	ClientDisplayView::ConvertClientToNormalPoint(this->_renderProperty.normalWidth, this->_renderProperty.normalHeight,
												  this->_renderProperty.clientWidth, this->_renderProperty.clientHeight,
												  s,
												  this->_renderProperty.rotation,
												  x, y);
	
	// Normalize the touch location to the DS.
	if (this->_renderProperty.mode == ClientDisplayMode_Dual)
	{
		switch (this->_renderProperty.layout)
		{
			case ClientDisplayLayout_Horizontal:
			{
				if (this->_renderProperty.order == ClientDisplayOrder_MainFirst)
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
					const bool isClickWithinMajorDisplay = (this->_renderProperty.order == ClientDisplayOrder_TouchFirst) && (x >= 0.0) && (x < 256.0);
					(*_initialTouchInMajorDisplay)[inputID] = isClickWithinMajorDisplay;
				}
				
				const bool handleClickInMajorDisplay = (*_initialTouchInMajorDisplay)[inputID];
				if (!handleClickInMajorDisplay)
				{
					const double minorDisplayScale = (this->_renderProperty.normalWidth - (double)GPU_FRAMEBUFFER_NATIVE_WIDTH) / (double)GPU_FRAMEBUFFER_NATIVE_WIDTH;
					x = (x - GPU_FRAMEBUFFER_NATIVE_WIDTH) / minorDisplayScale;
					y = y / minorDisplayScale;
				}
				break;
			}
				
			default: // Default to vertical orientation.
			{
				if (this->_renderProperty.order == ClientDisplayOrder_TouchFirst)
				{
					y -= ((double)GPU_FRAMEBUFFER_NATIVE_HEIGHT + this->_renderProperty.gapDistance);
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
void ClientDisplayView::ConvertNormalToTransformedBounds(const double scalar,
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
double ClientDisplayView::GetMaxScalarWithinBounds(const double normalBoundsWidth, const double normalBoundsHeight,
												   const double keepInBoundsWidth, const double keepInBoundsHeight)
{
	const double maxX = (normalBoundsWidth <= 0.0) ? 0.0 : keepInBoundsWidth / normalBoundsWidth;
	const double maxY = (normalBoundsHeight <= 0.0) ? 0.0 : keepInBoundsHeight / normalBoundsHeight;
	
	return (maxX <= maxY) ? maxX : maxY;
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
void ClientDisplayView::ConvertClientToNormalPoint(const double normalWidth, const double normalHeight,
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

void ClientDisplayView::CalculateNormalSize(const ClientDisplayMode mode, const ClientDisplayLayout layout, const double gapScale,
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

ClientDisplay3DView::ClientDisplay3DView()
{
	_canFilterOnGPU = false;
	_willFilterOnGPU = false;
	_filtersPreferGPU = false;
}

bool ClientDisplay3DView::CanFilterOnGPU() const
{
	return this->_canFilterOnGPU;
}

bool ClientDisplay3DView::WillFilterOnGPU() const
{
	return this->_willFilterOnGPU;
}

bool ClientDisplay3DView::GetFiltersPreferGPU() const
{
	return this->_filtersPreferGPU;
}

void ClientDisplay3DView::SetFiltersPreferGPU(const bool preferGPU)
{
	this->_filtersPreferGPU = preferGPU;
}

void ClientDisplay3DView::SetSourceDeposterize(bool useDeposterize)
{
	this->_useDeposterize = (this->_canFilterOnGPU) ? useDeposterize : false;
}

void ClientDisplay3DView::SetHUDVertices(float viewportWidth, float viewportHeight, float *vtxBufferPtr)
{
	const char *cString = this->_hudString.c_str();
	const size_t length = this->_hudString.length();
	const float charSize = this->_glyphSize;
	const float lineHeight = charSize * 0.8f;
	const float textBoxTextOffset = charSize * 0.25f;
	float charLocX = textBoxTextOffset;
	float charLocY = -textBoxTextOffset - lineHeight;
	float textBoxWidth = 0.0f;
	
	// First, calculate the vertices of the text box.
	// The text box should always be the first character in the string.
	vtxBufferPtr[0] = 0.0f;			vtxBufferPtr[1] = 0.0f;
	vtxBufferPtr[2] = charLocX;		vtxBufferPtr[3] = 0.0f;
	vtxBufferPtr[4] = charLocX;		vtxBufferPtr[5] = -textBoxTextOffset;
	vtxBufferPtr[6] = 0.0f;			vtxBufferPtr[7] = -textBoxTextOffset;
	
	// Calculate the vertices of the remaining characters in the string.
	for (size_t i = 1, j = 8; i < length; i++, j+=8)
	{
		const char c = cString[i];
		
		if (c == '\n')
		{
			if (charLocX > textBoxWidth)
			{
				textBoxWidth = charLocX;
			}
			
			vtxBufferPtr[5] -= lineHeight;
			vtxBufferPtr[7] -= lineHeight;
			
			charLocX = textBoxTextOffset;
			charLocY -= lineHeight;
			continue;
		}
		
		const float charWidth = this->_glyphInfo[c].width * charSize / (float)this->_glyphTileSize;
		
		vtxBufferPtr[j+0] = charLocX;					vtxBufferPtr[j+1] = charLocY + charSize;	// Top Left
		vtxBufferPtr[j+2] = charLocX + charWidth;		vtxBufferPtr[j+3] = charLocY + charSize;	// Top Right
		vtxBufferPtr[j+4] = charLocX + charWidth;		vtxBufferPtr[j+5] = charLocY;				// Bottom Right
		vtxBufferPtr[j+6] = charLocX;					vtxBufferPtr[j+7] = charLocY;				// Bottom Left
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
	vtxBufferPtr[2] += textBoxWidth;
	vtxBufferPtr[4] += textBoxWidth;
	
	// Scale and translate the box
	for (size_t i = 0; i < (length * 8); i+=2)
	{
		// Scale
		vtxBufferPtr[i+0] *= textBoxScale;
		vtxBufferPtr[i+1] *= textBoxScale;
		
		// Translate
		vtxBufferPtr[i+0] += boxOffset - (viewportWidth / 2.0f);
		vtxBufferPtr[i+1] += (viewportHeight / 2.0f) - boxOffset;
	}
}

void ClientDisplay3DView::SetHUDTextureCoordinates(float *texCoordBufferPtr)
{
	const char *cString = this->_hudString.c_str();
	const size_t length = this->_hudString.length();
	
	for (size_t i = 0; i < length; i++)
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
}

void ClientDisplay3DView::SetScreenVertices(float *vtxBufferPtr)
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

void ClientDisplay3DView::SetScreenTextureCoordinates(float w0, float h0, float w1, float h1, float *texCoordBufferPtr)
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
