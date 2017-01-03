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

ClientDisplayView::ClientDisplayView()
{
	_property.normalWidth	= GPU_FRAMEBUFFER_NATIVE_WIDTH;
	_property.normalHeight	= GPU_FRAMEBUFFER_NATIVE_HEIGHT * 2.0;
	_property.clientWidth	= _property.normalWidth;
	_property.clientHeight	= _property.normalHeight;
	_property.rotation		= 0.0;
	_property.viewScale		= 1.0;
	_property.gapScale		= 0.0;
	_property.gapDistance	= DS_DISPLAY_UNSCALED_GAP * _property.gapScale;
	_property.mode			= ClientDisplayMode_Dual;
	_property.layout		= ClientDisplayLayout_Vertical;
	_property.order			= ClientDisplayOrder_MainFirst;
	
	_initialTouchInMajorDisplay = new InitialTouchPressMap;
}

ClientDisplayView::ClientDisplayView(const ClientDisplayViewProperties props)
{
	_property = props;
	_initialTouchInMajorDisplay = new InitialTouchPressMap;
}

ClientDisplayView::~ClientDisplayView()
{
	delete _initialTouchInMajorDisplay;
}

ClientDisplayViewProperties ClientDisplayView::GetProperties() const
{
	return this->_property;
}

void ClientDisplayView::SetProperties(const ClientDisplayViewProperties props)
{
	this->_property = props;
	
	ClientDisplayView::CalculateNormalSize(props.mode, props.layout, props.gapScale, this->_property.normalWidth, this->_property.normalHeight);
}

void ClientDisplayView::SetClientSize(const double w, const double h)
{
	this->_property.clientWidth = w;
	this->_property.clientHeight = h;
	this->_property.viewScale = ClientDisplayView::GetMaxScalarWithinBounds(this->_property.normalWidth, this->_property.normalHeight, w, h);
}

double ClientDisplayView::GetRotation() const
{
	return this->_property.rotation;
}

void ClientDisplayView::SetRotation(const double r)
{
	this->_property.rotation = r;
	
	double checkWidth = this->_property.normalWidth;
	double checkHeight = this->_property.normalHeight;
	ClientDisplayView::ConvertNormalToTransformedBounds(1.0, r, checkWidth, checkHeight);
	this->_property.viewScale = ClientDisplayView::GetMaxScalarWithinBounds(checkWidth, checkHeight, this->_property.clientWidth, this->_property.clientHeight);
}

double ClientDisplayView::GetViewScale() const
{
	return this->_property.viewScale;
}

ClientDisplayMode ClientDisplayView::GetMode() const
{
	return this->_property.mode;
}

void ClientDisplayView::SetMode(const ClientDisplayMode mode)
{
	this->_property.mode = mode;
	ClientDisplayView::CalculateNormalSize(this->_property.mode, this->_property.layout, this->_property.gapScale, this->_property.normalWidth, this->_property.normalHeight);
	this->_property.viewScale = ClientDisplayView::GetMaxScalarWithinBounds(this->_property.normalWidth, this->_property.normalHeight, this->_property.clientWidth, this->_property.clientHeight);
}

ClientDisplayLayout ClientDisplayView::GetLayout() const
{
	return this->_property.layout;
}

void ClientDisplayView::SetLayout(const ClientDisplayLayout layout)
{
	this->_property.layout = layout;
	ClientDisplayView::CalculateNormalSize(this->_property.mode, this->_property.layout, this->_property.gapScale, this->_property.normalWidth, this->_property.normalHeight);
	this->_property.viewScale = ClientDisplayView::GetMaxScalarWithinBounds(this->_property.normalWidth, this->_property.normalHeight, this->_property.clientWidth, this->_property.clientHeight);
}

ClientDisplayOrder ClientDisplayView::GetOrder() const
{
	return this->_property.order;
}

void ClientDisplayView::SetOrder(const ClientDisplayOrder order)
{
	this->_property.order = order;
}

double ClientDisplayView::GetGapScale() const
{
	return this->_property.gapScale;
}

void ClientDisplayView::SetGapWithScalar(const double gapScale)
{
	this->_property.gapScale = gapScale;
	this->_property.gapDistance = (double)DS_DISPLAY_UNSCALED_GAP * gapScale;
}

double ClientDisplayView::GetGapDistance() const
{
	return this->_property.gapDistance;
}

void ClientDisplayView::SetGapWithDistance(const double gapDistance)
{
	this->_property.gapScale = gapDistance / (double)DS_DISPLAY_UNSCALED_GAP;
	this->_property.gapDistance = gapDistance;
}

void ClientDisplayView::GetNDSPoint(const int inputID, const bool isInitialTouchPress,
									const double clientX, const double clientY,
									u8 &outX, u8 &outY) const
{
	double x = clientX;
	double y = clientY;
	double w = this->_property.normalWidth;
	double h = this->_property.normalHeight;
	
	ClientDisplayView::ConvertNormalToTransformedBounds(1.0, this->_property.rotation, w, h);
	const double s = ClientDisplayView::GetMaxScalarWithinBounds(w, h, this->_property.clientWidth, this->_property.clientHeight);
	
	ClientDisplayView::ConvertClientToNormalPoint(this->_property.normalWidth, this->_property.normalHeight,
												  this->_property.clientWidth, this->_property.clientHeight,
												  s,
												  360.0 - this->_property.rotation,
												  x, y);
	
	// Normalize the touch location to the DS.
	if (this->_property.mode == ClientDisplayMode_Dual)
	{
		switch (this->_property.layout)
		{
			case ClientDisplayLayout_Horizontal:
			{
				if (this->_property.order == ClientDisplayOrder_MainFirst)
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
					const bool isClickWithinMajorDisplay = (this->_property.order == ClientDisplayOrder_TouchFirst) && (x >= 0.0) && (x < 256.0);
					(*_initialTouchInMajorDisplay)[inputID] = isClickWithinMajorDisplay;
				}
				
				const bool handleClickInMajorDisplay = (*_initialTouchInMajorDisplay)[inputID];
				if (!handleClickInMajorDisplay)
				{
					const double minorDisplayScale = (this->_property.normalWidth - (double)GPU_FRAMEBUFFER_NATIVE_WIDTH) / (double)GPU_FRAMEBUFFER_NATIVE_WIDTH;
					x = (x - GPU_FRAMEBUFFER_NATIVE_WIDTH) / minorDisplayScale;
					y = y / minorDisplayScale;
				}
				break;
			}
				
			default: // Default to vertical orientation.
			{
				if (this->_property.order == ClientDisplayOrder_TouchFirst)
				{
					y -= ((double)GPU_FRAMEBUFFER_NATIVE_HEIGHT + this->_property.gapDistance);
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
