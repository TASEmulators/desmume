/*  Copyright (C) 2006 yopyop
    yopyop156@ifrance.com
    yopyop156.ifrance.com

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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef MEM_VIEW_H
#define MEM_VIEW_H

enum MemRegionType {
	MEMVIEW_ARM9 = 0,
	MEMVIEW_ARM7,
	MEMVIEW_FIRMWARE,
	MEMVIEW_ROM,
	MEMVIEW_FULL
};

INT_PTR CALLBACK MemView_DlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK MemView_ViewBoxProc(HWND hCtl, UINT uMsg, WPARAM wParam, LPARAM lParam);

class CMemView : public CToolWindow
{
public:
	CMemView(MemRegionType memRegion = MEMVIEW_ARM9, u32 start_address = 0xFFFFFFFF);
	~CMemView();

	HFONT font;

	u32 region;
	u32 address;
	u32 viewMode;

	BOOL sel;
	u32 selPart;
	u32 selAddress;
	u32 selNewVal;
};

#endif
