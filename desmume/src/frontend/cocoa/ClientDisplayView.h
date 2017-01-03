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
#include "../../GPU.h"

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

typedef std::map<int, bool> InitialTouchPressMap;  // Key = An ID number of the host input, Value = Flag that indicates if the initial touch press was in the major display

typedef struct
{
	double normalWidth;
	double normalHeight;
	double clientWidth;
	double clientHeight;
	double rotation;
	double viewScale;
	double gapScale;
	double gapDistance;
	ClientDisplayMode mode;
	ClientDisplayLayout layout;
	ClientDisplayOrder order;
} ClientDisplayViewProperties;

class ClientDisplayView
{
protected:
	ClientDisplayViewProperties _property;
	InitialTouchPressMap *_initialTouchInMajorDisplay;
	
public:
	ClientDisplayView();
	ClientDisplayView(const ClientDisplayViewProperties props);
	~ClientDisplayView();
	
	ClientDisplayViewProperties GetProperties() const;
	void SetProperties(const ClientDisplayViewProperties properties);
	
	void SetClientSize(const double w, const double h);
	
	double GetRotation() const;
	void SetRotation(const double r);
	double GetViewScale() const;
	ClientDisplayMode GetMode() const;
	void SetMode(const ClientDisplayMode mode);
	ClientDisplayLayout GetLayout() const;
	void SetLayout(const ClientDisplayLayout layout);
	ClientDisplayOrder GetOrder() const;
	void SetOrder(const ClientDisplayOrder order);
	
	double GetGapScale() const;
	void SetGapWithScalar(const double gapScale);
	double GetGapDistance() const;
	void SetGapWithDistance(const double gapDistance);
	
	void GetNDSPoint(const int inputID, const bool isInitialTouchPress,
					 const double clientX, const double clientY,
					 u8 &outX, u8 &outY) const;
	
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

#endif // _CLIENT_DISPLAY_VIEW_H_
