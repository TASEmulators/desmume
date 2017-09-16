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

#import "DisplayViewCALayer.h"
#import "../cocoa_GPU.h"

DisplayViewCALayerInterface::DisplayViewCALayerInterface()
{
	_nsView = nil;
	_frontendLayer = nil;
	_sharedData = nil;
	_willRenderToCALayer = false;
}

NSView* DisplayViewCALayerInterface::GetNSView() const
{
	return this->_nsView;
}

void DisplayViewCALayerInterface::SetNSView(NSView *theView)
{
	this->_nsView = theView;
}

CALayer<DisplayViewCALayer>* DisplayViewCALayerInterface::GetFrontendLayer() const
{
	return this->_frontendLayer;
}

void DisplayViewCALayerInterface::SetFrontendLayer(CALayer<DisplayViewCALayer> *layer)
{
	this->_frontendLayer = layer;
}

void DisplayViewCALayerInterface::CALayerDisplay()
{
	[this->_frontendLayer setNeedsDisplay];
}

bool DisplayViewCALayerInterface::GetRenderToCALayer() const
{
	return this->_willRenderToCALayer;
}

void DisplayViewCALayerInterface::SetRenderToCALayer(const bool renderToLayer)
{
	this->_willRenderToCALayer = renderToLayer;
}

MacClientSharedObject* DisplayViewCALayerInterface::GetSharedData()
{
	return this->_sharedData;
}

void DisplayViewCALayerInterface::SetSharedData(MacClientSharedObject *sharedObject)
{
	this->_sharedData = sharedObject;
}
