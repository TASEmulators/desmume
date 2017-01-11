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

#ifndef _DISPLAYVIEWCALAYER_H
#define _DISPLAYVIEWCALAYER_H

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>

class ClientDisplay3DView;

@protocol DisplayViewCALayer <NSObject>

@required
- (ClientDisplay3DView *) clientDisplay3DView;

@end

class DisplayViewCALayerInterface
{
private:
	CALayer<DisplayViewCALayer> *_frontendLayer;
	
public:
	DisplayViewCALayerInterface();
	
	CALayer<DisplayViewCALayer>* GetFrontendLayer() const;
	void SetFrontendLayer(CALayer<DisplayViewCALayer> *layer);
	void CALayerDisplay();
};

#endif // _DISPLAYVIEWCALAYER_H
