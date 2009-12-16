/*  driver.cpp

    Copyright (C) 2009 DeSmuME team

    This file is part of DeSmuME

    DeSmuME is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DeSmuME is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DeSmuME; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "types.h"
#include "driver.h"
#include "rasterize.h"
#include "gfx3d.h"


#ifdef HAVE_WX
#include "wx/wxprec.h"
#include "wx/wx.h"
#include "wxdlg/wxdlg3dViewer.h"

const int kViewportWidth = 256;
const int kViewportHeight = 192;

static SoftRasterizerEngine engine;
static Fragment _screen[kViewportWidth*kViewportHeight];
static FragmentColor _screenColor[kViewportWidth*kViewportHeight];

extern void _HACK_Viewer_ExecUnit(SoftRasterizerEngine* engine);

class Mywxdlg3dViewer : public wxdlg3dViewer
{
public:
	Mywxdlg3dViewer()
		: wxdlg3dViewer(NULL)
	{}

	virtual void RepaintPanel()
	{
		Refresh(false);
		Update();
	}

	void NewFrame()
	{
		listPolys->SetItemCount(viewer3d_state.polylist.count);
		labelUserPolycount->SetLabel(wxString::Format("%s: %d",_("User Polys"),viewer3d_state.polylist.count));
		labelFinalPolycount->SetLabel(wxString::Format("%s: %d",_("Final Polys"),viewer3d_state.polylist.count));
		tree->DeleteAllItems();
		wxTreeItemId root = tree->AddRoot("");
		for(int i=0;i<viewer3d_state.polylist.count;i++)
		{
			tree->AppendItem(root,"hai kirin");
		}
	}

	virtual wxString OnGetItemText(const wxListCtrl* list, long item, long column) const
	{
		return "hi";
	}

	void RedrawPanel(wxClientDC* dc)
	{
		//------------
		//do the 3d work..
		engine.polylist = &viewer3d_state.polylist;
		engine.vertlist = &viewer3d_state.vertlist;
		engine.indexlist = &viewer3d_state.indexlist;
		engine.screen = _screen;
		engine.screenColor = _screenColor;
		engine.width = kViewportWidth;
		engine.height = kViewportHeight;

		engine.updateFogTable();
	
		engine.initFramebuffer(kViewportWidth,kViewportHeight,gfx3d.state.enableClearImage);
		engine.updateToonTable();
		engine.updateFloatColors();
		engine.performClipping(checkMaterialInterpolate->IsChecked());
		engine.performViewportTransforms<true>(kViewportWidth,kViewportHeight);
		engine.performBackfaceTests();
		engine.performCoordAdjustment(false);
		engine.setupTextures(false);

		_HACK_Viewer_ExecUnit(&engine);
		//------------

		//dc.SetBackground(*wxGREEN_BRUSH); dc.Clear();
		u8 framebuffer[kViewportWidth*kViewportHeight*3];
		for(int y=0,i=0;y<kViewportHeight;y++)
			for(int x=0;x<kViewportWidth;x++,i++) {
				framebuffer[i*3] = _screenColor[i].r<<2;
				framebuffer[i*3+1] = _screenColor[i].g<<2;
				framebuffer[i*3+2] = _screenColor[i].b<<2;
			}
		wxImage image(kViewportWidth,kViewportHeight,framebuffer,true);
		wxBitmap bitmap(image);
		dc->DrawBitmap(bitmap,0,0);
	}

	virtual void _OnPaintPanel( wxPaintEvent& event )
	{
		wxPaintDC dc(wxDynamicCast(event.GetEventObject(), wxWindow));
		RedrawPanel(&dc);
	}
};

class VIEW3D_Driver_WX : public VIEW3D_Driver
{
public:
	VIEW3D_Driver_WX()
		: viewer(NULL)
	{}
	~VIEW3D_Driver_WX()
	{
		delete viewer;
	}

	virtual bool IsRunning() { return viewer != NULL; }

	virtual void Launch()
	{
		if(viewer) return;
		delete viewer;
		viewer = new Mywxdlg3dViewer();
		viewer->Show(true);
	}

	void Close()
	{
		delete viewer;
		viewer = NULL;
	}

	virtual void NewFrame()
	{
		if(!viewer) return;
		if(!viewer->IsShown()) {
			Close();
			return;
		}

		viewer->NewFrame();
		viewer->RepaintPanel();
	}

private:
	Mywxdlg3dViewer *viewer;
};

#endif

static VIEW3D_Driver nullView3d;
BaseDriver::BaseDriver()
: view3d(NULL)
{
	VIEW3D_Shutdown();
}

void BaseDriver::VIEW3D_Shutdown()
{
	if(view3d != &nullView3d) delete view3d;
	view3d = &nullView3d;
}

void BaseDriver::VIEW3D_Init()
{
	VIEW3D_Shutdown();
#ifdef HAVE_WX
	view3d = new VIEW3D_Driver_WX();
#endif
}

BaseDriver::~BaseDriver()
{
}

