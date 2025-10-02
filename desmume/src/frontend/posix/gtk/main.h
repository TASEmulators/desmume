 /*
	Copyright (C) 2007 Pascal Giard (evilynux)
	Copyright (C) 2006-2025 DeSmuME team
	
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

#ifndef __DESMUME_GTK_MAIN_H__
#define __DESMUME_GTK_MAIN_H__

#include "GPU.h"
#include "rthreads/rthreads.h"

#define GTK3_FRAMEBUFFER_PAGE_COUNT 3


class Gtk3GPUEventHandler : public GPUEventHandlerDefault
{
protected:
	NDSColorFormat _colorFormatPending;
	size_t _widthPending;
	size_t _heightPending;
	size_t _pageCountPending;
	
	bool _didColorFormatChange;
	bool _didWidthChange;
	bool _didHeightChange;
	bool _didPageCountChange;
	
	u8 _latestBufferIndex;
	
	slock_t *_mutexApplyGPUSettings;
	slock_t *_mutexVideoFetch;
	slock_t *_mutexFramebufferPage[GTK3_FRAMEBUFFER_PAGE_COUNT];
	
public:
	Gtk3GPUEventHandler();
	~Gtk3GPUEventHandler();
	
	void SetFramebufferDimensions(size_t w, size_t h);
	void GetFramebufferDimensions(size_t &outWidth, size_t &outHeight);
	
	void SetColorFormat(NDSColorFormat colorFormat);
	NDSColorFormat GetColorFormat();
	
	void VideoFetchLock();
	void VideoFetchUnlock();
	
	// GPUEventHandler methods
	virtual void DidFrameEnd(bool isFrameSkipped, const NDSDisplayInfo &latestDisplayInfo);
	virtual void DidApplyGPUSettingsBegin();
	virtual void DidApplyGPUSettingsEnd();
};

void Pause(GSimpleAction *action, GVariant *parameter, gpointer user_data);
void Launch(GSimpleAction *action, GVariant *parameter, gpointer user_data);

#endif
