/*
	Copyright (C) 2013-2015 DeSmuME team

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

#include "fsnitroView.h"
#include <shlobj.h>

#include "../path.h"
#include "../common.h"
#include "../NDSSystem.h"
#include "../MMU.h"
#include "../utils/fsnitro.h"

#include "resource.h"
#include "CWindow.h"
#include "main.h"
#include "memView.h"

//not available on old SDK versions
#ifndef ILC_HIGHQUALITYSCALE
#define ILC_HIGHQUALITYSCALE 0x00020000
#endif

HMENU	popupMenu = NULL;
HWND	hBar = NULL;
FS_NITRO *fs = NULL;
u32		currentFileID = 0xFFFFFFFF;

INT CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lp, LPARAM pData) 
{
	TCHAR szDir[MAX_PATH];

	switch(uMsg)
	{
		case BFFM_INITIALIZED: 
			SendMessage(hwnd, BFFM_SETSELECTION, TRUE, (LPARAM)path.getpath(path.SLOT1D).c_str());
		break;

		case BFFM_SELCHANGED: 
			if (SHGetPathFromIDList((LPITEMIDLIST) lp, szDir))
			{
				SendMessage(hwnd,BFFM_SETSTATUSTEXT, 0, (LPARAM)szDir);
			}
		break;
	}
	return 0;
}

BOOL CALLBACK ProgressWnd(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_INITDIALOG)
	{
		hBar = GetDlgItem(hWnd, IDC_PROGRESS_BAR);
		SendMessage(hBar, PBM_SETRANGE32, 0, fs->getNumFiles());
		SendMessage(hBar, PBM_SETPOS, 0, 0);
		SendMessage(hBar, PBM_SETSTEP, 1, 0);
		SetWindowText(GetDlgItem(hWnd, IDC_MESSAGE), "Please, wait...");
		SetWindowText(hWnd, "Extracting...");
		return FALSE;
	}
	return FALSE;
}

void extractCallback(u32 current, u32 num)
{
	if (hBar)
		SendMessage(hBar, PBM_SETPOS, current, 0);
}

void refreshQView(HWND hWnd, u16 id)
{
	HWND ctrl = GetDlgItem(hWnd, IDC_FILE_QVIEW);

	if (id < 0xF000)
	{
		char buf[256] = {0};
		memset(buf, 0, sizeof(buf));
		u32 len = std::min<u32>(sizeof(buf), fs->getFileSizeById(id));
		u32 start = fs->getStartAddrById(id);

		memcpy(&buf[0], &gameInfo.romdata[start], len);

		for (u32 i = 0; i < len; i++)
			if (buf[i] < 0x20) buf[i] = 0x20;
		
		SetWindowText(ctrl, buf);
	}
	else
		SetWindowText(ctrl, "");
}

BOOL CALLBACK ViewFSNitroProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_INITDIALOG:
			{
				fs = new FS_NITRO(gameInfo.romdata);

				if (!fs)
				{
					msgbox->error("Error reading FS from ROM");
					SendMessage(hWnd, WM_CLOSE, 0, 0);
					return TRUE;
				}

				HWND tree = GetDlgItem(hWnd, IDC_FILES_TREE);
				SendMessage(tree, WM_SETREDRAW, (WPARAM)false, 0);

				//HBITMAP hBmp;
				HICON hIcon = NULL;
				HIMAGELIST hIml = ImageList_Create(16, 16, ILC_COLORDDB | ILC_HIGHQUALITYSCALE, 3, 0);
				// Open folder icon
				hIcon = LoadIcon(hAppInst, MAKEINTRESOURCE(IDI_FOLDER_OPEN));
				int iFolderOpen = ImageList_AddIcon(hIml, hIcon);
				DeleteObject(hIcon);
				// Closed folder icon
				hIcon = LoadIcon(hAppInst, MAKEINTRESOURCE(IDI_FOLDER_CLOSED));
				int iFolderClosed = ImageList_AddIcon(hIml, hIcon);
				DeleteObject(hIcon);
				// Binary file icon
				hIcon = LoadIcon(hAppInst, MAKEINTRESOURCE(IDI_FILE_BINARY));
				int iFileBinary = ImageList_AddIcon(hIml, hIcon);
				DeleteObject(hIcon);

				TreeView_SetImageList(tree, hIml, TVSIL_NORMAL);
				
				const u32 numDirs = fs->getNumDirs();
				const u32 numFiles = fs->getNumFiles();
				HTREEITEM *dirs = new HTREEITEM[numDirs];
				memset(dirs, 0, sizeof(HTREEITEM) * numDirs);

				//printf("Dirs: %d\n", numDirs);
				//printf("Files: %d\n", numFiles);

				TVINSERTSTRUCT item;
				// data folder
				memset(&item, 0, sizeof(TVINSERTSTRUCT));
				item.hParent = TVI_ROOT;
				item.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM;;
				item.item.pszText = (LPSTR)"data";
				item.item.iImage = iFolderOpen;
				item.item.iSelectedImage = iFolderClosed;
				item.item.lParam = 0xFFFF;
				HTREEITEM hData = TreeView_InsertItem(tree, &item);
				
				// overlay folder
				memset(&item, 0, sizeof(TVINSERTSTRUCT));
				item.hParent = TVI_ROOT;
				item.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM;;
				item.item.pszText = (LPSTR)"overlay";
				item.item.iImage = iFolderOpen;
				item.item.iSelectedImage = iFolderClosed;
				item.item.lParam = 0xFFFE;
				HTREEITEM hOverlay = TreeView_InsertItem(tree, &item);

				for (u32 i = 1; i < numDirs; i++)
				{
					u16 id = (i | 0xF000);
					u16 parent = fs->getDirParrentByID(id) & 0x0FFF;

					string name = fs->getDirNameByID(id);
					//printf("%s\n", name.c_str());
					TVINSERTSTRUCT item;
					memset(&item, 0, sizeof(item));
					if (parent == 0)
						item.hParent = hData;
					else
						item.hParent = dirs[parent];

					item.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM;
					item.item.pszText = (LPSTR)name.c_str();
					item.item.iImage = iFolderOpen;
					item.item.iSelectedImage = iFolderClosed;
					item.item.lParam = id;

					dirs[i] = TreeView_InsertItem(tree, &item);
				}

				for (u32 i = 0; i < numFiles; i++)
				{
					u16 parent = fs->getFileParentById(i);
					if (parent == 0xFFFF) continue;

					parent &= 0x0FFF;
					
					string name = fs->getFileNameByID(i);
					TVINSERTSTRUCT item;
					memset(&item, 0, sizeof(item));
					if (fs->isOverlay(i))
					{
						item.hParent = hOverlay;
					}
					else
					{
						if (parent == 0)
							item.hParent = hData;
						else
							item.hParent = dirs[parent];
					}

					item.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM;
					item.item.pszText = (LPSTR)name.c_str();
					item.item.iImage = iFileBinary;
					item.item.iSelectedImage = iFileBinary;
					item.item.lParam = i;
					TreeView_InsertItem(tree, &item);
				}
				delete [] dirs;
				SendMessage(tree, WM_SETREDRAW, (WPARAM)true, 0);

				popupMenu = LoadMenu(hAppInst, MAKEINTRESOURCE(MENU_FSNITRO));
			}
		return FALSE;

		case WM_CLOSE:
			if (fs)
			{
				delete fs;
				fs = NULL;
			}
			if (popupMenu)
			{
				DestroyMenu(popupMenu);
				popupMenu = NULL;
			}
			PostQuitMessage(0);
		return TRUE;

		case WM_COMMAND:
			switch(LOWORD(wParam))
			{
				case IDCANCEL:
					SendMessage(hWnd, WM_CLOSE, 0, 0);
				return TRUE;

				case ID_FSNITRO_VIEW:
					if (currentFileID < 0xF000)
					{
						if (RegWndClass("MemView_ViewBox", MemView_ViewBoxProc, 0, sizeof(CMemView*)))
							OpenToolWindow(new CMemView(MEMVIEW_ROM, fs->getStartAddrById(currentFileID)));
						return TRUE;
					}
				return FALSE;

				case ID_EXTRACTFILE:
				case ID_EXTRACTALL:
					{
						char tmp_path[MAX_PATH] = {0};
						
						BROWSEINFO bp={0};
						memset(&bp, 0, sizeof(bp));
						bp.hwndOwner = hWnd;
						bp.pidlRoot = NULL;
						bp.pszDisplayName = NULL;
						bp.lpszTitle = "Select destination directory";
						bp.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_USENEWUI;
						bp.lpfn = BrowseCallbackProc;
		
						LPITEMIDLIST tmp = SHBrowseForFolder((LPBROWSEINFO)&bp);
						if (tmp)
						{
							memset(tmp_path, 0, sizeof(tmp_path));
							SHGetPathFromIDList(tmp, tmp_path);
							if (tmp_path[strlen(tmp_path)-1] != '\\')
								tmp_path[strlen(tmp_path)] = '\\';

							if (LOWORD(wParam) == ID_EXTRACTALL)
							{
								string tmp = (string)tmp_path + (string)path.GetRomNameWithoutExtension() + (string)"\\";
								mkdir(tmp.c_str());
								HWND progress = CreateDialog(hAppInst, MAKEINTRESOURCE(IDD_PROGRESS_WND), NULL, (DLGPROC)ProgressWnd);
								ShowWindow(progress, SW_SHOW);
								UpdateWindow(progress);
								fs->extractAll(tmp, extractCallback);
								DestroyWindow(progress);
							}
							else
							{
								if (currentFileID < 0xF000)
									fs->extractFile(currentFileID, (string)tmp_path);
							}
						}
					}
				return TRUE;
			}
		return FALSE;

		case WM_NOTIFY:
			{
				LPNMHDR notify = (LPNMHDR)lParam;
				if (wParam == IDC_FILES_TREE)
				{
					switch (notify->code)
					{
						case NM_DBLCLK:
							if (currentFileID < 0xF000)
							{
								if (RegWndClass("MemView_ViewBox", MemView_ViewBoxProc, 0, sizeof(CMemView*)))
									OpenToolWindow(new CMemView(MEMVIEW_ROM, fs->getStartAddrById(currentFileID)));
								return TRUE;
							}
							return FALSE;

						case NM_RCLICK:
							{
								DWORD tmp = GetMessagePos();
								POINTS pos = MAKEPOINTS(tmp);
								HTREEITEM hItem = TreeView_GetNextItem(notify->hwndFrom, 0, TVGN_DROPHILITE);
								if(hItem)
									TreeView_SelectItem(notify->hwndFrom, hItem);
								TVITEM pitem;
								HTREEITEM item = TreeView_GetSelection(GetDlgItem(hWnd, IDC_FILES_TREE));

								memset(&pitem, 0, sizeof(pitem));
								pitem.hItem = item;
								pitem.mask = TVIF_PARAM;
								TreeView_GetItem(GetDlgItem(hWnd, IDC_FILES_TREE), &pitem);
								
								currentFileID = pitem.lParam;
								refreshQView(hWnd, currentFileID);
								EnableMenuItem(popupMenu, ID_EXTRACTFILE, MF_BYCOMMAND | ((currentFileID < 0xF000)?MF_ENABLED:MF_GRAYED));
								EnableMenuItem(popupMenu, ID_FSNITRO_VIEW, MF_BYCOMMAND | ((currentFileID < 0xF000)?MF_ENABLED:MF_GRAYED));
								SetMenuDefaultItem(GetSubMenu(popupMenu, 0), ID_FSNITRO_VIEW, FALSE);
								TrackPopupMenu(GetSubMenu(popupMenu, 0), TPM_LEFTALIGN | TPM_RIGHTBUTTON, pos.x, pos.y, NULL, hWnd, NULL);
							}
						break;

						case TVN_SELCHANGED:
							{
								LPNMTREEVIEW sel = (LPNMTREEVIEW)lParam;
								char buf[256] = {0};
								
								currentFileID = sel->itemNew.lParam;
								refreshQView(hWnd, currentFileID);

								if ((currentFileID & 0xF000) == 0xF000)
								{
									SetWindowText(GetDlgItem(hWnd, IDC_FILE_INFO), "");
								}
								else
								{
									u32 start = fs->getStartAddrById(currentFileID);
									u32 end = fs->getEndAddrById(currentFileID);
									u32 size = (end - start);
									sprintf(buf, "[%08X-%08X] size %u", start, end, size);
									SetWindowText(GetDlgItem(hWnd, IDC_FILE_INFO), buf);
								}
							}
						break;
					}
				}
			}
		return FALSE;
	}
	return FALSE;
}