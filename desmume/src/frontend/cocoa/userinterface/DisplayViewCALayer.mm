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

MacDisplayPresenterInterface::MacDisplayPresenterInterface()
{
	_sharedData = nil;
}

MacDisplayPresenterInterface::MacDisplayPresenterInterface(MacClientSharedObject *sharedObject)
{
	_sharedData = sharedObject;
}

MacClientSharedObject* MacDisplayPresenterInterface::GetSharedData()
{
	return this->_sharedData;
}

void MacDisplayPresenterInterface::SetSharedData(MacClientSharedObject *sharedObject)
{
	this->_sharedData = sharedObject;
}

#pragma mark -

MacDisplayLayeredView::MacDisplayLayeredView()
{
	__InstanceInit();
}

MacDisplayLayeredView::MacDisplayLayeredView(ClientDisplay3DPresenter *thePresenter) : ClientDisplay3DView(thePresenter)
{
	__InstanceInit();
}

void MacDisplayLayeredView::__InstanceInit()
{
	_caLayer = nil;
	_willRenderToCALayer = false;
}

CALayer<DisplayViewCALayer>* MacDisplayLayeredView::GetCALayer() const
{
	return this->_caLayer;
}

bool MacDisplayLayeredView::GetRenderToCALayer() const
{
	return this->_willRenderToCALayer;
}

void MacDisplayLayeredView::SetRenderToCALayer(const bool renderToLayer)
{
	this->_willRenderToCALayer = renderToLayer;
}
