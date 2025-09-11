/*
	Copyright (C) 2017-2025 DeSmuME team

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

#ifndef _DISPLAYVIEWCALAYER_H
#define _DISPLAYVIEWCALAYER_H

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>

#import "../ClientDisplayView.h"

@class MacClientSharedObject;
class MacDisplayLayeredView;
class MacDisplayViewOutput;

@protocol DisplayViewCALayer <NSObject>

@required

@property (assign, nonatomic, getter=clientDisplayView, setter=setClientDisplayView:) MacDisplayLayeredView *_cdv;

@end

class MacDisplayPresenterInterface
{
protected:
	MacClientSharedObject *_sharedData;
	
public:
	MacDisplayPresenterInterface();
	MacDisplayPresenterInterface(MacClientSharedObject *sharedObject);
	virtual ~MacDisplayPresenterInterface() {}
	
	MacClientSharedObject* GetSharedData();
	virtual void SetSharedData(MacClientSharedObject *sharedObject);
};

class MacDisplayLayeredView : public ClientDisplay3DView
{
private:
	void __InstanceInit();
	
protected:
	CALayer<DisplayViewCALayer> *_caLayer;
	bool _willRenderToCALayer;
	
public:
	MacDisplayLayeredView();
	MacDisplayLayeredView(ClientDisplay3DPresenter *thePresenter);
	
	CALayer<DisplayViewCALayer>* GetCALayer() const;
	
	bool GetRenderToCALayer() const;
	void SetRenderToCALayer(const bool renderToLayer);
};

#endif // _DISPLAYVIEWCALAYER_H
