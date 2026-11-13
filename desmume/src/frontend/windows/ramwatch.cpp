//RamWatch dialog was copied and adapted from GENS11: http://code.google.com/p/gens-rerecording/
//Authors: adelikat, Upthorn, Nitsuja

/*
	Modifications Copyright (C) 2009-2026 DeSmuME team

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

// Win32 host for RAM Watch. The document itself (watch list, .wch parsing,
// memory reads, formatting, recent files, preferences) lives in
// ../shared/ramwatch_core.cpp and is shared with the Cocoa frontend.

#include "ramwatch.h"

#include <assert.h>
#include <commctrl.h>
#include <stdio.h>
#include <string>
#include <vector>

#include "NDSSystem.h"
#include "MMU.h"
#include "path.h"

#include "resource.h"
#include "main.h"
#include "cheatsWin.h"
#include "DPI.h"

HWND RamWatchHWnd = NULL;

#define MESSAGEBOXPARENT (RamWatchHWnd ? RamWatchHWnd : MainWindow->getHWnd())

static char Str_Tmp[1024];
static std::string Rom_Name;

static HMENU ramwatchmenu = NULL;
static HMENU rwrecentmenu = NULL;
static HACCEL RamWatchAccels = NULL;
static char Watch_Dir[1024] = "";
static char Gens_Path[64] = "M:\\"; // TODO: use a real default directory

static const unsigned int RW_MENU_FIRST_RECENT_FILE = 600;

// Watch being created by the "New"/"Duplicate"/"prompt for comment" dialogs.
// The old code abused rswatches[WatchCount] as scratch storage, which could
// overflow once the list was full.
static AddressWatcher s_draftWatch;

static void RefreshWatchListCount(HWND hDlg);
static void RefreshRWRecentMenu(void);

static RAMWatchCore& Core()
{
	return RAMWatchCore::Get();
}

// ---------------------------------------------------------------------------
// List view helpers
// ---------------------------------------------------------------------------

static void RefreshWatchListCount(HWND hDlg)
{
	if (hDlg == NULL)
	{
		hDlg = RamWatchHWnd;
	}
	if (hDlg == NULL)
	{
		return;
	}

	HWND list = GetDlgItem(hDlg, IDC_WATCHLIST);
	if (list != NULL)
	{
		ListView_SetItemCount(list, (int)Core().Count());
	}
}

void init_list_box(HWND Box, const char* Strs[], int numColumns, int *columnWidths)
{
	LVCOLUMN Col;
	Col.mask = LVCF_FMT | LVCF_ORDER | LVCF_SUBITEM | LVCF_TEXT | LVCF_WIDTH;
	Col.fmt = LVCFMT_CENTER;
	for (int i = 0; i < numColumns; i++)
	{
		Col.iOrder = i;
		Col.iSubItem = i;
		Col.pszText = (LPSTR)(Strs[i]);
		Col.cx = DPI_ListColumnWidth(Box, Strs[i], columnWidths[i]);
		ListView_InsertColumn(Box, i, &Col);
	}

	ListView_SetExtendedListViewStyle(Box, LVS_EX_FULLROWSELECT);
}

static void RamWatchEnableCommand(HWND hDlg, HMENU hMenu, UINT uIDEnableItem, bool enable)
{
	EnableWindow(GetDlgItem(hDlg, uIDEnableItem), (enable ? TRUE : FALSE));
	if (hMenu != NULL)
	{
		if (uIDEnableItem == ID_WATCHES_UPDOWN)
		{
			EnableMenuItem(hMenu, IDC_C_WATCH_UP, MF_BYCOMMAND | (enable ? MF_ENABLED : MF_GRAYED));
			EnableMenuItem(hMenu, IDC_C_WATCH_DOWN, MF_BYCOMMAND | (enable ? MF_ENABLED : MF_GRAYED));
		}
		else
		{
			EnableMenuItem(hMenu, uIDEnableItem, MF_BYCOMMAND | (enable ? MF_ENABLED : MF_GRAYED));
		}
	}
}

static void RefreshWatchListSelectedCountControlStatus(HWND hDlg)
{
	static int prevSelCount = -1;
	const int selCount = ListView_GetSelectedCount(GetDlgItem(hDlg, IDC_WATCHLIST));
	if (selCount != prevSelCount)
	{
		if (selCount < 2 || prevSelCount < 2)
		{
			RamWatchEnableCommand(hDlg, ramwatchmenu, IDC_C_WATCH_EDIT, selCount == 1);
			RamWatchEnableCommand(hDlg, ramwatchmenu, IDC_C_WATCH_REMOVE, selCount >= 1);
			RamWatchEnableCommand(hDlg, ramwatchmenu, IDC_C_WATCH_DUPLICATE, selCount == 1);
			RamWatchEnableCommand(hDlg, ramwatchmenu, IDC_C_ADDCHEAT, selCount == 1);
			RamWatchEnableCommand(hDlg, ramwatchmenu, ID_WATCHES_UPDOWN, selCount == 1);
		}
		prevSelCount = selCount;
	}
}

// ---------------------------------------------------------------------------
// Recent files menu (host side; the list itself belongs to the core)
// ---------------------------------------------------------------------------

static void UpdateRW_RMenu(HMENU menu, unsigned int mitem, unsigned int baseid)
{
	MENUITEMINFO moo;
	ZeroMemory(&moo, sizeof(moo));
	moo.cbSize = sizeof(moo);
	moo.fMask = MIIM_SUBMENU | MIIM_STATE;

	HMENU fileMenu = GetSubMenu(ramwatchmenu, 0);
	if (fileMenu != NULL)
	{
		GetMenuItemInfo(fileMenu, mitem, FALSE, &moo);
		moo.hSubMenu = menu;
		moo.fState = Core().RecentFiles().empty() ? MFS_GRAYED : MFS_ENABLED;
		SetMenuItemInfo(fileMenu, mitem, FALSE, &moo);
	}

	for (unsigned int x = 0; x < RAMWatchCore::kMaxRecentFiles; x++)
	{
		RemoveMenu(menu, baseid + x, MF_BYCOMMAND);
	}

	const std::vector<std::string>& recent = Core().RecentFiles();
	for (int x = (int)recent.size() - 1; x >= 0; x--)
	{
		char tmp[128 + 5];
		const char *path = recent[x].c_str();
		const size_t len = strlen(path);
		if (len < 128)
		{
			sprintf(tmp, "&%d. %s", (x + 1) % 10, path);
		}
		else
		{
			sprintf(tmp, "&%d. %s", (x + 1) % 10, path + len - 127);
		}

		ZeroMemory(&moo, sizeof(moo));
		moo.cbSize = sizeof(moo);
		moo.fMask = MIIM_DATA | MIIM_ID | MIIM_TYPE;
		moo.cch = (UINT)strlen(tmp);
		moo.fType = 0;
		moo.wID = baseid + x;
		moo.dwTypeData = tmp;
		InsertMenuItem(menu, 0, 1, &moo);
	}
}

static void RefreshRWRecentMenu(void)
{
	if (rwrecentmenu != NULL)
	{
		UpdateRW_RMenu(rwrecentmenu, RAMMENU_FILE_RECENT, RW_MENU_FIRST_RECENT_FILE);
	}
}

// ---------------------------------------------------------------------------
// File dialogs
// ---------------------------------------------------------------------------

static int Change_File_S(char *Dest, char *Dir, char *Titre, char *Filter, char *Ext, HWND hwnd)
{
	OPENFILENAME ofn;
	SetCurrentDirectory(Gens_Path);

	if (!strcmp(Dest, ""))
	{
		strcpy(Dest, "default.");
		strcat(Dest, Ext);
	}

	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hwnd;
	ofn.hInstance = hAppInst;
	ofn.lpstrFile = Dest;
	ofn.nMaxFile = 2047;
	ofn.lpstrFilter = Filter;
	ofn.nFilterIndex = 1;
	ofn.lpstrInitialDir = Dir;
	ofn.lpstrTitle = Titre;
	ofn.lpstrDefExt = Ext;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_NOREADONLYRETURN;

	return GetSaveFileName(&ofn) ? 1 : 0;
}

static int Change_File_L(char *Dest, char *Dir, char *Titre, char *Filter, char *Ext, HWND hwnd)
{
	OPENFILENAME ofn;
	SetCurrentDirectory(Gens_Path);

	if (!strcmp(Dest, ""))
	{
		strcpy(Dest, "default.");
		strcat(Dest, Ext);
	}

	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hwnd;
	ofn.hInstance = hAppInst;
	ofn.lpstrFile = Dest;
	ofn.nMaxFile = 2047;
	ofn.lpstrFilter = Filter;
	ofn.nFilterIndex = 1;
	ofn.lpstrInitialDir = Dir;
	ofn.lpstrTitle = Titre;
	ofn.lpstrDefExt = Ext;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

	return GetOpenFileName(&ofn) ? 1 : 0;
}

// ---------------------------------------------------------------------------
// Core wrappers exposed to main.cpp / ram_search.cpp
// ---------------------------------------------------------------------------

bool InsertWatch(const AddressWatcher& watch, const char *comment, int atIndex)
{
	AddressWatcher newWatch = watch;
	newWatch.Comment = (comment != NULL) ? comment : "";

	const size_t index = (atIndex < 0) ? RAMWatchCore::kNoIndex : (size_t)atIndex;
	const bool inserted = Core().Insert(newWatch, index);
	if (inserted)
	{
		RefreshWatchListCount(NULL);
	}
	else if (RamWatchHWnd != NULL)
	{
		// Duplicate or full list: bring the window forward, as before.
		SetForegroundWindow(RamWatchHWnd);
	}
	return inserted;
}

LRESULT CALLBACK PromptWatchNameProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_INITDIALOG:
			strcpy(Str_Tmp, "");
			return true;

		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case IDOK:
					GetDlgItemText(hDlg, IDC_PROMPT_EDIT, Str_Tmp, 80);
					InsertWatch(s_draftWatch, Str_Tmp);
					EndDialog(hDlg, true);
					return true;

				case IDCANCEL:
					EndDialog(hDlg, false);
					return false;
			}
			break;

		case WM_CLOSE:
			EndDialog(hDlg, false);
			return false;
	}
	return false;
}

bool InsertWatch(const AddressWatcher& watch, HWND parent)
{
	if (parent == NULL)
	{
		parent = RamWatchHWnd;
	}
	if (parent == NULL)
	{
		parent = MainWindow->getHWnd();
	}

	s_draftWatch = watch;
	const size_t before = Core().Count();
	DialogBoxW(hAppInst, MAKEINTRESOURCEW(IDD_PROMPT), parent, (DLGPROC)PromptWatchNameProc);
	return Core().Count() > before;
}

void Update_RAM_Watch()
{
	if (RamWatchHWnd == NULL)
	{
		return;
	}

	std::vector<bool> changed;
	if (!Core().RefreshValues(changed))
	{
		return;
	}

	// Redraw only the changed items that are currently visible, in contiguous
	// runs, exactly like the original owner-data list view refresh.
	HWND lv = GetDlgItem(RamWatchHWnd, IDC_WATCHLIST);
	int top = ListView_GetTopIndex(lv);
	int bottom = top + ListView_GetCountPerPage(lv) + 1;
	if (top < 0) top = 0;
	if (bottom > (int)Core().Count()) bottom = (int)Core().Count();

	int start = -1;
	for (int i = top; i <= bottom; i++)
	{
		const bool isChanged = (i < bottom) && (i < (int)changed.size()) && changed[i];
		if (start == -1)
		{
			if (isChanged) start = i;
		}
		else if (!isChanged)
		{
			ListView_RedrawItems(lv, start, i - 1);
			start = -1;
		}
	}
}

bool AskSave()
{
	RAMWatchCore& core = Core();
	if (!core.IsDirty())
	{
		return true;
	}

	const int answer = MessageBox(MainWindow->getHWnd(), "Save Changes?", "RAM Watch", MB_YESNOCANCEL);
	if (answer == IDYES)
	{
		return QuickSaveWatches();
	}
	return (answer != IDCANCEL);
}

bool QuickSaveWatches()
{
	RAMWatchCore& core = Core();
	if (!core.IsDirty())
	{
		return true;
	}

	if (core.CurrentFile().empty())
	{
		return Save_Watches();
	}

	std::string error;
	if (!core.SaveCurrentFile(error))
	{
		MessageBox(MESSAGEBOXPARENT, error.c_str(), "ERROR", MB_OK);
		return false;
	}
	return true;
}

bool Save_Watches()
{
	Rom_Name = path.GetRomName();
	snprintf(Str_Tmp, sizeof(Str_Tmp), "%s.wch", Rom_Name.c_str());

	if (!Change_File_S(Str_Tmp, Gens_Path, "Save Watches",
	                   "DeSmuME Watchlist\0*.wch\0All Files\0*.*\0\0", "wch", RamWatchHWnd))
	{
		return false;
	}

	std::string error;
	if (!Core().SaveAsFile(Str_Tmp, error))
	{
		MessageBox(MESSAGEBOXPARENT, error.c_str(), "ERROR", MB_OK);
		return false;
	}
	RefreshRWRecentMenu();
	return true;
}

bool Load_Watches(bool clear, const char* filename)
{
	// The clear variant replaces the list, so honour unsaved changes first.
	if (clear && !AskSave())
	{
		return false;
	}

	std::string error;
	if (!Core().LoadFile(filename, !clear, error))
	{
		MessageBox(MESSAGEBOXPARENT, error.c_str(), "ERROR", MB_OK);
		return false;
	}

	RefreshRWRecentMenu();
	RefreshWatchListCount(NULL);
	if (RamWatchHWnd != NULL)
	{
		RefreshWatchListSelectedCountControlStatus(RamWatchHWnd);
	}
	return true;
}

bool Load_Watches(bool clear)
{
	Rom_Name = path.GetRomName();
	snprintf(Str_Tmp, sizeof(Str_Tmp), "%s.wch", Rom_Name.c_str());

	if (Change_File_L(Str_Tmp, Watch_Dir, "Load Watches",
	                  "DeSmuME Watchlist\0*.wch\0All Files\0*.*\0\0", "wch", RamWatchHWnd))
	{
		return Load_Watches(clear, Str_Tmp);
	}
	return false;
}

bool ResetWatches()
{
	if (!AskSave())
	{
		return false;
	}

	Core().Clear();
	RefreshWatchListCount(NULL);
	if (RamWatchHWnd != NULL)
	{
		RefreshWatchListSelectedCountControlStatus(RamWatchHWnd);
	}
	return true;
}

void OpenRWRecentFile(int recentFileNumber)
{
	if (!AskSave())
	{
		return;
	}

	std::string error;
	if (!Core().OpenRecentFile((size_t)recentFileNumber, error))
	{
		if (!error.empty())
		{
			MessageBox(MESSAGEBOXPARENT, error.c_str(), "ERROR", MB_OK);
		}
	}

	RefreshRWRecentMenu();
	RefreshWatchListCount(NULL);
	if (RamWatchHWnd != NULL)
	{
		RefreshWatchListSelectedCountControlStatus(RamWatchHWnd);
	}
}

// ---------------------------------------------------------------------------
// Edit Watch dialog
// ---------------------------------------------------------------------------

LRESULT CALLBACK EditWatchProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static int index = 0;
	static char s = 0;
	static char t = 0;

	switch (uMsg)
	{
		case WM_INITDIALOG:
		{
			index = (int)lParam;
			const RAMWatchCore& core = Core();
			const AddressWatcher& watch = (index < (int)core.Count()) ? core.At((size_t)index) : s_draftWatch;

			snprintf(Str_Tmp, sizeof(Str_Tmp), "%08X", (unsigned)watch.Address);
			SetDlgItemText(hDlg, IDC_EDIT_COMPAREADDRESS, Str_Tmp);
			SetDlgItemText(hDlg, IDC_PROMPT_EDIT, watch.Comment.c_str());

			s = watch.Size;
			t = watch.Type;
			switch (s)
			{
				case 'b': SendDlgItemMessage(hDlg, IDC_1_BYTE, BM_SETCHECK, BST_CHECKED, 0); break;
				case 'w': SendDlgItemMessage(hDlg, IDC_2_BYTES, BM_SETCHECK, BST_CHECKED, 0); break;
				case 'd': SendDlgItemMessage(hDlg, IDC_4_BYTES, BM_SETCHECK, BST_CHECKED, 0); break;
				default:  s = 0; break;
			}
			switch (t)
			{
				case 's': SendDlgItemMessage(hDlg, IDC_SIGNED, BM_SETCHECK, BST_CHECKED, 0); break;
				case 'u': SendDlgItemMessage(hDlg, IDC_UNSIGNED, BM_SETCHECK, BST_CHECKED, 0); break;
				case 'h': SendDlgItemMessage(hDlg, IDC_HEX, BM_SETCHECK, BST_CHECKED, 0); break;
				case 'f': SendDlgItemMessage(hDlg, IDC_2012, BM_SETCHECK, BST_CHECKED, 0); break;
				default:  t = 0; break;
			}
			return true;
		}

		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case IDC_SIGNED:   t = 's'; return true;
				case IDC_UNSIGNED: t = 'u'; return true;
				case IDC_HEX:      t = 'h'; return true;
				case IDC_2012:     t = 'f'; return true;
				case IDC_1_BYTE:   s = 'b'; return true;
				case IDC_2_BYTES:  s = 'w'; return true;
				case IDC_4_BYTES:  s = 'd'; return true;

				case IDOK:
				{
					if (s && t)
					{
						const RAMWatchCore& core = Core();
						AddressWatcher temp = (index < (int)core.Count()) ? core.At((size_t)index) : s_draftWatch;
						temp.Size = s;
						temp.Type = t;
						temp.WrongEndian = false; // little endian support is still TODO

						GetDlgItemText(hDlg, IDC_EDIT_COMPAREADDRESS, Str_Tmp, 1024);
						bool valid = false;
						temp.Address = RAMDebugCore::ParseAddress(Str_Tmp, &valid);
						RAMDebugCore::NormalizeAddress(temp.Address);

						if (valid && RAMDebugCore::IsRAMAddressValid(temp.Address))
						{
							GetDlgItemText(hDlg, IDC_PROMPT_EDIT, Str_Tmp, 80);
							temp.Comment = Str_Tmp;

							RAMWatchCore& core = Core();
							const bool ok = (index < (int)core.Count())
							              ? core.Replace((size_t)index, temp)
							              : InsertWatch(temp, temp.Comment.c_str());
							if (ok)
							{
								RefreshWatchListCount(NULL);
								EndDialog(hDlg, true);
								return true;
							}

							MessageBox(hDlg, "A watch with this address, size and type already exists.",
							           "RAM Watch", MB_OK);
							return true;
						}
						MessageBox(hDlg, "Invalid Address", "ERROR", MB_OK);
						return true;
					}

					strcpy(Str_Tmp, "Error:");
					if (!s) strcat(Str_Tmp, " Size must be specified.");
					if (!t) strcat(Str_Tmp, " Type must be specified.");
					MessageBox(hDlg, Str_Tmp, "ERROR", MB_OK);
					return true;
				}

				case IDCANCEL:
					EndDialog(hDlg, false);
					return false;
			}
			break;

		case WM_CLOSE:
			EndDialog(hDlg, false);
			return false;
	}
	return false;
}

// ---------------------------------------------------------------------------
// Main RAM Watch dialog
// ---------------------------------------------------------------------------

LRESULT CALLBACK RamWatchProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static int watchIndex = 0;

	Update_RAM_Watch();

	switch (uMsg)
	{
		case WM_MOVE:
		{
			RECT wrect;
			GetWindowRect(hDlg, &wrect);
			Core().SetWindowPosition(wrect.left, wrect.top);
			break;
		}

		case WM_INITDIALOG:
		{
			RECT r, r2;
			GetWindowRect(MainWindow->getHWnd(), &r);
			GetWindowRect(hDlg, &r2);

			const int width = (r.right - r.left);
			const int width2 = (r2.right - r2.left);
			const int height = (r.bottom - r.top);

			// Push the tool window away from the main window when there is room.
			if (r.left + width2 + width < GetSystemMetrics(SM_CXSCREEN))
			{
				r.right += width;
				r.left += width;
			}
			else if ((int)r.left - width2 > 0)
			{
				r.right -= width2;
				r.left -= width2;
			}

			// Restore the saved position only when it is plausibly on screen.
			// The original used || here, which made both checks always true.
			RAMWatchCore& core = Core();
			if (core.SaveWindowPos())
			{
				const int maxX = width * 2 + GetSystemMetrics(SM_CXSCREEN);
				const int maxY = height * 2 + GetSystemMetrics(SM_CYSCREEN);
				if ((core.WindowX() > -width * 2) && (core.WindowX() < maxX)) r.left = core.WindowX();
				if ((core.WindowY() > -height * 2) && (core.WindowY() < maxY)) r.top = core.WindowY();
			}

			SetWindowPos(hDlg, NULL, r.left, r.top, NULL, NULL, SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);

			ramwatchmenu = GetMenu(hDlg);
			rwrecentmenu = CreateMenu();
			RefreshRWRecentMenu();

			const char* names[3] = { "Address", "Value", "Notes" };
			int widths[3] = { 62, 64, 64 + 51 + 53 };
			init_list_box(GetDlgItem(hDlg, IDC_WATCHLIST), names, 3, widths);

			RefreshWatchListCount(hDlg);
			RamWatchAccels = LoadAccelerators(hAppInst, MAKEINTRESOURCE(IDR_ACCELERATOR1));

			// The arrow button width from the resource gets ignored on Windows.
			SetWindowPos(GetDlgItem(hDlg, ID_WATCHES_UPDOWN), 0, 0, 0, 30, 60, SWP_NOMOVE);
			Update_RAM_Watch();
			DragAcceptFiles(hDlg, TRUE);
			RefreshWatchListSelectedCountControlStatus(hDlg);
			return true;
		}

		case WM_INITMENU:
			CheckMenuItem(ramwatchmenu, RAMMENU_FILE_AUTOLOAD, Core().AutoLoad() ? MF_CHECKED : MF_UNCHECKED);
			CheckMenuItem(ramwatchmenu, RAMMENU_FILE_SAVEWINDOW, Core().SaveWindowPos() ? MF_CHECKED : MF_UNCHECKED);
			break;

		case WM_MENUSELECT:
		case WM_ENTERSIZEMOVE:
			break;

		case WM_NOTIFY:
		{
			switch (wParam)
			{
				case ID_WATCHES_UPDOWN:
				{
					if (((LPNMUPDOWN)lParam)->hdr.code == UDN_DELTAPOS)
					{
						const int delta = ((LPNMUPDOWN)lParam)->iDelta;
						SendMessage(hDlg, WM_COMMAND, delta < 0 ? IDC_C_WATCH_UP : IDC_C_WATCH_DOWN, 0);
					}
					break;
				}

				default:
				{
					LPNMHDR lP = (LPNMHDR)lParam;
					switch (lP->code)
					{
						case LVN_ITEMCHANGED:
						{
							NM_LISTVIEW *pNMListView = (NM_LISTVIEW *)lP;
							if ((pNMListView->uNewState & LVIS_FOCUSED) ||
							    ((pNMListView->uNewState ^ pNMListView->uOldState) & LVIS_SELECTED))
							{
								RefreshWatchListSelectedCountControlStatus(hDlg);
							}
							break;
						}

						case LVN_GETDISPINFO:
						{
							LV_DISPINFO *Item = (LV_DISPINFO *)lParam;
							Item->item.mask = LVIF_TEXT;
							Item->item.state = 0;
							Item->item.iImage = 0;
							const unsigned int iNum = Item->item.iItem;
							static char num[64];

							const RAMWatchCore& core = Core();
							if (iNum >= core.Count())
							{
								return false;
							}
							const AddressWatcher& watch = core.At(iNum);

							switch (Item->item.iSubItem)
							{
								case 0:
									snprintf(num, sizeof(num), "%s", RAMWatchCore::DisplayAddress(watch).c_str());
									Item->item.pszText = num;
									return true;

								case 1:
									snprintf(num, sizeof(num), "%s", core.DisplayValueAt(iNum).c_str());
									Item->item.pszText = num;
									return true;

								case 2:
									// Owner-data list views keep the pointer only until the
									// next message; the core string outlives this call.
									Item->item.pszText = (char *)watch.Comment.c_str();
									return true;

								default:
									return false;
							}
						}

						case LVN_ODFINDITEM:
							// Disable type-to-search: it conflicts with the accelerators.
							SetWindowLongPtr(hDlg, DWLP_MSGRESULT, ListView_GetSelectionMark(GetDlgItem(hDlg, IDC_WATCHLIST)));
							return 1;
					}
					break;
				}
			}
			break;
		}

		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case RAMMENU_FILE_SAVE:    QuickSaveWatches(); break;
				case RAMMENU_FILE_SAVEAS:  return Save_Watches();
				case RAMMENU_FILE_OPEN:    return Load_Watches(true);
				case RAMMENU_FILE_APPEND:  return Load_Watches(false);
				case RAMMENU_FILE_NEW:     ResetWatches(); return true;

				case IDC_C_WATCH_REMOVE:
				{
					HWND watchListControl = GetDlgItem(hDlg, IDC_WATCHLIST);
					std::vector<int> indexes;
					int i = ListView_GetNextItem(watchListControl, -1, LVNI_ALL | LVNI_SELECTED);
					while (i >= 0)
					{
						indexes.push_back(i);
						i = ListView_GetNextItem(watchListControl, i, LVNI_ALL | LVNI_SELECTED);
					}
					// Delete back-to-front so the remaining indexes stay valid.
					for (size_t n = indexes.size(); n > 0; n--)
					{
						const int idx = indexes[n - 1];
						Core().Remove((size_t)idx);
						ListView_DeleteItem(watchListControl, idx);
					}
					Core().SetDirty(true);
					SetFocus(watchListControl);
					return true;
				}

				case IDC_C_WATCH_EDIT:
					watchIndex = ListView_GetSelectionMark(GetDlgItem(hDlg, IDC_WATCHLIST));
					if (watchIndex != -1)
					{
						DialogBoxParam(hAppInst, MAKEINTRESOURCE(IDD_EDITWATCH), hDlg,
						               (DLGPROC)EditWatchProc, (LPARAM)watchIndex);
						SetFocus(GetDlgItem(hDlg, IDC_WATCHLIST));
					}
					return true;

				case IDC_C_WATCH:
					s_draftWatch = AddressWatcher();
					DialogBoxParam(hAppInst, MAKEINTRESOURCE(IDD_EDITWATCH), hDlg,
					               (DLGPROC)EditWatchProc, (LPARAM)Core().Count());
					SetFocus(GetDlgItem(hDlg, IDC_WATCHLIST));
					return true;

				case IDC_C_WATCH_DUPLICATE:
					watchIndex = ListView_GetSelectionMark(GetDlgItem(hDlg, IDC_WATCHLIST));
					if (watchIndex != -1)
					{
						s_draftWatch = Core().At((size_t)watchIndex);
						DialogBoxParam(hAppInst, MAKEINTRESOURCE(IDD_EDITWATCH), hDlg,
						               (DLGPROC)EditWatchProc, (LPARAM)Core().Count());
						SetFocus(GetDlgItem(hDlg, IDC_WATCHLIST));
					}
					return true;

				case IDC_C_WATCH_SEPARATE:
					Core().AddSeparator();
					RefreshWatchListCount(hDlg);
					SetFocus(GetDlgItem(hDlg, IDC_WATCHLIST));
					return true;

				case IDC_C_WATCH_UP:
					watchIndex = ListView_GetSelectionMark(GetDlgItem(hDlg, IDC_WATCHLIST));
					if (watchIndex > 0 && Core().MoveUp((size_t)watchIndex))
					{
						ListView_SetItemState(GetDlgItem(hDlg, IDC_WATCHLIST), watchIndex, 0, LVIS_FOCUSED | LVIS_SELECTED);
						ListView_SetSelectionMark(GetDlgItem(hDlg, IDC_WATCHLIST), watchIndex - 1);
						ListView_SetItemState(GetDlgItem(hDlg, IDC_WATCHLIST), watchIndex - 1,
						                      LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
						RefreshWatchListCount(hDlg);
					}
					return true;

				case IDC_C_WATCH_DOWN:
					watchIndex = ListView_GetSelectionMark(GetDlgItem(hDlg, IDC_WATCHLIST));
					if (watchIndex != -1 && Core().MoveDown((size_t)watchIndex))
					{
						ListView_SetItemState(GetDlgItem(hDlg, IDC_WATCHLIST), watchIndex, 0, LVIS_FOCUSED | LVIS_SELECTED);
						ListView_SetSelectionMark(GetDlgItem(hDlg, IDC_WATCHLIST), watchIndex + 1);
						ListView_SetItemState(GetDlgItem(hDlg, IDC_WATCHLIST), watchIndex + 1,
						                      LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
						RefreshWatchListCount(hDlg);
					}
					return true;

				case RAMMENU_FILE_AUTOLOAD:
					Core().SetAutoLoad(!Core().AutoLoad());
					CheckMenuItem(ramwatchmenu, RAMMENU_FILE_AUTOLOAD, Core().AutoLoad() ? MF_CHECKED : MF_UNCHECKED);
					break;

				case RAMMENU_FILE_SAVEWINDOW:
					Core().SetSaveWindowPos(!Core().SaveWindowPos());
					CheckMenuItem(ramwatchmenu, RAMMENU_FILE_SAVEWINDOW, Core().SaveWindowPos() ? MF_CHECKED : MF_UNCHECKED);
					break;

				case IDC_C_ADDCHEAT:
				{
					watchIndex = ListView_GetSelectionMark(GetDlgItem(hDlg, IDC_WATCHLIST));
					if (watchIndex != -1)
					{
						const AddressWatcher& watch = Core().At((size_t)watchIndex);
						const u32 address = (u32)watch.Address;
						const u8 size = (u8)RAMDebugCore::SizeInBytes(watch.Size);
						const u32 value = (u32)watch.CurValue;
						CheatsAddDialog(hDlg, address, value, size, watch.Comment.c_str());
					}
					break;
				}

				case IDCANCEL:
					RamWatchHWnd = NULL;
					DragAcceptFiles(hDlg, FALSE);
					EndDialog(hDlg, true);
					return true;

				default:
					if ((LOWORD(wParam) >= RW_MENU_FIRST_RECENT_FILE) &&
					    (LOWORD(wParam) < RW_MENU_FIRST_RECENT_FILE + RAMWatchCore::kMaxRecentFiles))
					{
						OpenRWRecentFile(LOWORD(wParam) - RW_MENU_FIRST_RECENT_FILE);
					}
					break;
			}
			break;

		case WM_KEYDOWN:
		{
			SetFocus(GetDlgItem(hDlg, IDC_WATCHLIST));
			MSG msg;
			msg.hwnd = hDlg;
			msg.message = uMsg;
			msg.wParam = wParam;
			msg.lParam = lParam;
			if (RamWatchAccels && TranslateAccelerator(hDlg, RamWatchAccels, &msg))
			{
				return true;
			}
			break;
		}

		case WM_CLOSE:
			RamWatchHWnd = NULL;
			DragAcceptFiles(hDlg, FALSE);
			EndDialog(hDlg, true);
			return true;

		case WM_DROPFILES:
		{
			HDROP hDrop = (HDROP)wParam;
			DragQueryFile(hDrop, 0, Str_Tmp, 1024);
			DragFinish(hDrop);
			return Load_Watches(true, Str_Tmp);
		}
	}
	return false;
}
