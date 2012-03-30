//RamWatch dialog was copied and adapted from GENS11: http://code.google.com/p/gens-rerecording/
//Authors: adelikat, Upthorn, Nitsuja

/*
	Modifications Copyright (C) 2009-2010 DeSmuME team

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

#include "main.h"
#include "resource.h"
#include "common.h"
#include "NDSSystem.h"
#include "ramwatch.h"
#include "ram_search.h"
#include "cheatsWin.h"
#include <assert.h>
#include <windows.h>
#include <commctrl.h>
#include <string>
#include "path.h"

HWND RamWatchHWnd = NULL;

#define MESSAGEBOXPARENT (RamWatchHWnd ? RamWatchHWnd : MainWindow->getHWnd())

static char Str_Tmp[1024];
std::string Rom_Name;

static HMENU ramwatchmenu;
static HMENU rwrecentmenu;
static HACCEL RamWatchAccels = NULL;
char rw_recent_files[MAX_RECENT_WATCHES][1024];
char Watch_Dir[1024]="";
const unsigned int RW_MENU_FIRST_RECENT_FILE = 600;
bool RWfileChanged = false; //Keeps track of whether the current watch file has been changed, if so, ramwatch will prompt to save changes
bool AutoRWLoad = false;    //Keeps track of whether Auto-load is checked
bool RWSaveWindowPos = false; //Keeps track of whether Save Window position is checked
char currentWatch[1024];
int ramw_x, ramw_y;			//Used to store ramwatch dialog window positions
AddressWatcher rswatches[MAX_WATCH_COUNT];
int WatchCount=0;

bool QuickSaveWatches();
bool ResetWatches();

void RefreshWatchListSelectedCountControlStatus(HWND hDlg);

unsigned int GetCurrentValue(AddressWatcher& watch)
{
	char buf[4];
	MMU_DumpMemBlock(0, watch.Address, 4, (u8*)buf);

	u32 val_u32 = *(u32*)buf;
	u16 val_u16 = *(u16*)buf;
	u8 val_u8 = *(u8*)buf;

	switch (watch.Size)
	{
	case 'b': return val_u8;
	case 'w': return val_u16;
	case 'd': return val_u32;
	default: return 0;
	}
//	return ReadValueAtHardwareAddress(watch.Address, watch.Size == 'd' ? 4 : watch.Size == 'w' ? 2 : 1);
}

bool IsSameWatch(const AddressWatcher& l, const AddressWatcher& r)
{
	if (r.Size == 'S') return false;
	return ((l.Address == r.Address) && (l.Size == r.Size) && (l.Type == r.Type)/* && (l.WrongEndian == r.WrongEndian)*/);
}

bool VerifyWatchNotAlreadyAdded(const AddressWatcher& watch)
{
	for (int j = 0; j < WatchCount; j++)
	{
		if (IsSameWatch(rswatches[j], watch))
		{
			if(RamWatchHWnd)
				SetForegroundWindow(RamWatchHWnd);
			return false;
		}
	}
	return true;
}


bool InsertWatch(const AddressWatcher& Watch, char *Comment)
{
	if(!VerifyWatchNotAlreadyAdded(Watch))
		return false;

	if(WatchCount >= MAX_WATCH_COUNT)
		return false;

	int i = WatchCount++;
	AddressWatcher& NewWatch = rswatches[i];
	NewWatch = Watch;
	//if (NewWatch.comment) free(NewWatch.comment);
	NewWatch.comment = (char *) malloc(strlen(Comment)+2);
	NewWatch.CurValue = GetCurrentValue(NewWatch);
	strcpy(NewWatch.comment, Comment);
	ListView_SetItemCount(GetDlgItem(RamWatchHWnd,IDC_WATCHLIST),WatchCount);
	RWfileChanged=true;

	return true;
}

LRESULT CALLBACK PromptWatchNameProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) //Gets the description of a watched address
{
	RECT r;
	RECT r2;
	int dx1, dy1, dx2, dy2;

	switch(uMsg)
	{
		case WM_INITDIALOG:
			GetWindowRect(MainWindow->getHWnd(), &r);
			dx1 = (r.right - r.left) / 2;
			dy1 = (r.bottom - r.top) / 2;

			GetWindowRect(hDlg, &r2);
			dx2 = (r2.right - r2.left) / 2;
			dy2 = (r2.bottom - r2.top) / 2;

			SetWindowPos(hDlg, NULL, r.left, r.top, NULL, NULL, SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);
			strcpy(Str_Tmp,"Enter a name for this RAM address.");
			//SendDlgItemMessage(hDlg,IDC_PROMPT_TEXT,WM_SETTEXT,0,(LPARAM)Str_Tmp);
			strcpy(Str_Tmp,"");
			//SendDlgItemMessage(hDlg,IDC_PROMPT_TEXT2,WM_SETTEXT,0,(LPARAM)Str_Tmp);
			return true;
			break;

		case WM_COMMAND:
			switch(LOWORD(wParam))
			{
				case IDOK:
				{
					GetDlgItemText(hDlg,IDC_PROMPT_EDIT,Str_Tmp,80);
					InsertWatch(rswatches[WatchCount],Str_Tmp);
					EndDialog(hDlg, true);
					return true;
					break;
				}
				case IDCANCEL:

					EndDialog(hDlg, false);
					return false;
					break;
			}
			break;

		case WM_CLOSE:
			EndDialog(hDlg, false);
			return false;
			break;
	}

	return false;
}

bool InsertWatch(const AddressWatcher& Watch, HWND parent)
{
	if(!VerifyWatchNotAlreadyAdded(Watch))
		return false;

	if(!parent)
		parent = RamWatchHWnd;
	if(!parent)
		parent = MainWindow->getHWnd();

	int prevWatchCount = WatchCount;

	rswatches[WatchCount] = Watch;
	rswatches[WatchCount].CurValue = GetCurrentValue(rswatches[WatchCount]);
	DialogBoxW(hAppInst, MAKEINTRESOURCEW(IDD_PROMPT), parent, (DLGPROC) PromptWatchNameProc);

	return WatchCount > prevWatchCount;
}

void Update_RAM_Watch()
{
	if (!RamWatchHWnd) return;

	// update cached values and detect changes to displayed listview items
	BOOL watchChanged[MAX_WATCH_COUNT] = {0};
	for(int i = 0; i < WatchCount; i++)
	{
		unsigned int prevCurValue = rswatches[i].CurValue;
		unsigned int newCurValue = GetCurrentValue(rswatches[i]);
		if(prevCurValue != newCurValue)
		{
			rswatches[i].CurValue = newCurValue;
			watchChanged[i] = TRUE;
		}
	}

	// refresh any visible parts of the listview box that changed
	HWND lv = GetDlgItem(RamWatchHWnd,IDC_WATCHLIST);
	int top = ListView_GetTopIndex(lv);
	int bottom = top + ListView_GetCountPerPage(lv) + 1; // +1 is so we will update a partially-displayed last item
	if(top < 0) top = 0;
	if(bottom > WatchCount) bottom = WatchCount;
	int start = -1;
	for(int i = top; i <= bottom; i++)
	{
		if(start == -1)
		{
			if(i != bottom && watchChanged[i])
			{
				start = i;
				//somethingChanged = true;
			}
		}
		else
		{
			if(i == bottom || !watchChanged[i])
			{
				ListView_RedrawItems(lv, start, i-1);
				start = -1;
			}
		}
	}
}

bool AskSave()
{
	//This function asks to save changes if the watch file contents have changed
	//returns false only if a save was attempted but failed or was cancelled
	if (RWfileChanged)
	{
		HWND Hwnd = MainWindow->getHWnd();
		int answer = MessageBox(Hwnd, "Save Changes?", "Ram Watch", MB_YESNOCANCEL);
		if(answer == IDYES)
			if(!QuickSaveWatches())
				return false;
		return (answer != IDCANCEL);
	}
	return true;
}


void UpdateRW_RMenu(HMENU menu, unsigned int mitem, unsigned int baseid)
{
	MENUITEMINFO moo;
	int x;

	moo.cbSize = sizeof(moo);
	moo.fMask = MIIM_SUBMENU | MIIM_STATE;

	GetMenuItemInfo(GetSubMenu(ramwatchmenu, 0), mitem, FALSE, &moo);
	moo.hSubMenu = menu;
	moo.fState = strlen(rw_recent_files[0]) ? MFS_ENABLED : MFS_GRAYED;

	SetMenuItemInfo(GetSubMenu(ramwatchmenu, 0), mitem, FALSE, &moo);

	// Remove all recent files submenus
	for(x = 0; x < MAX_RECENT_WATCHES; x++)
	{
		RemoveMenu(menu, baseid + x, MF_BYCOMMAND);
	}

	// Recreate the menus
	for(x = MAX_RECENT_WATCHES - 1; x >= 0; x--)
	{  
		char tmp[128 + 5];

		// Skip empty strings
		if(!strlen(rw_recent_files[x]))
		{
			continue;
		}

		moo.cbSize = sizeof(moo);
		moo.fMask = MIIM_DATA | MIIM_ID | MIIM_TYPE;

		// Fill in the menu text.
		if(strlen(rw_recent_files[x]) < 128)
		{
			sprintf(tmp, "&%d. %s", ( x + 1 ) % 10, rw_recent_files[x]);
		}
		else
		{
			sprintf(tmp, "&%d. %s", ( x + 1 ) % 10, rw_recent_files[x] + strlen( rw_recent_files[x] ) - 127);
		}

		// Insert the menu item
		moo.cch = strlen(tmp);
		moo.fType = 0;
		moo.wID = baseid + x;
		moo.dwTypeData = tmp;
		InsertMenuItem(menu, 0, 1, &moo);
	}
}

void UpdateRWRecentArray(const char* addString, unsigned int arrayLen, HMENU menu, unsigned int menuItem, unsigned int baseId)
{
	// Try to find out if the filename is already in the recent files list.
	for(unsigned int x = 0; x < arrayLen; x++)
	{
		if(strlen(rw_recent_files[x]))
		{
			if(!strcmp(rw_recent_files[x], addString))    // Item is already in list.
			{
				// If the filename is in the file list don't add it again.
				// Move it up in the list instead.

				int y;
				char tmp[1024];

				// Save pointer.
				strcpy(tmp,rw_recent_files[x]);
				
				for(y = x; y; y--)
				{
					// Move items down.
					strcpy(rw_recent_files[y],rw_recent_files[y - 1]);
				}

				// Put item on top.
				strcpy(rw_recent_files[0],tmp);

				// Update the recent files menu
				UpdateRW_RMenu(menu, menuItem, baseId);

				return;
			}
		}
	}

	// The filename wasn't found in the list. That means we need to add it.

	// Move the other items down.
	for(unsigned int x = arrayLen - 1; x; x--)
	{
		strcpy(rw_recent_files[x],rw_recent_files[x - 1]);
	}

	// Add the new item.
	strcpy(rw_recent_files[0], addString);

	// Update the recent files menu
	UpdateRW_RMenu(menu, menuItem, baseId);
}


void RWAddRecentFile(const char *filename)
{
	UpdateRWRecentArray(filename, MAX_RECENT_WATCHES, rwrecentmenu, RAMMENU_FILE_RECENT, RW_MENU_FIRST_RECENT_FILE);
}

void OpenRWRecentFile(int memwRFileNumber)
{
	if(!ResetWatches())
		return;

	int rnum = memwRFileNumber;
	if ((unsigned int)rnum >= MAX_RECENT_WATCHES)
		return; //just in case

	char* x;

	while(true)
	{
		x = rw_recent_files[rnum];
		if (!*x) 
			return;		//If no recent files exist just return.  Useful for Load last file on startup (or if something goes screwy)

		if (rnum) //Change order of recent files if not most recent
		{
			RWAddRecentFile(x);
			rnum = 0;
		}
		else
		{
			break;
		}
	}

	strcpy(currentWatch,x);
	strcpy(Str_Tmp,currentWatch);

	//loadwatches here
	FILE *WatchFile = fopen(Str_Tmp,"rb");
	if (!WatchFile)
	{
		int answer = MessageBox(MESSAGEBOXPARENT,"Error opening file.","ERROR",MB_OKCANCEL);
		if (answer == IDOK)
		{
			rw_recent_files[rnum][0] = '\0';	//Clear file from list 
			if (rnum)							//Update the ramwatch list
				RWAddRecentFile(rw_recent_files[0]); 
			else
				RWAddRecentFile(rw_recent_files[1]);
		}
		return;
	}
	const char DELIM = '\t';
	AddressWatcher Temp;
	char mode;
	fgets(Str_Tmp,1024,WatchFile);
	sscanf(Str_Tmp,"%c%*s",&mode);
	int WatchAdd;
	fgets(Str_Tmp,1024,WatchFile);
	sscanf(Str_Tmp,"%d%*s",&WatchAdd);
	WatchAdd+=WatchCount;
	for (int i = WatchCount; i < WatchAdd; i++)
	{
		while (i < 0)
			i++;
		do {
			fgets(Str_Tmp,1024,WatchFile);
		} while (Str_Tmp[0] == '\n');
		sscanf(Str_Tmp,"%*05X%*c%08X%*c%c%*c%c%*c%d",&(Temp.Address),&(Temp.Size),&(Temp.Type),&(Temp.WrongEndian));
		Temp.WrongEndian = 0;
		char *Comment = strrchr(Str_Tmp,DELIM) + 1;
		*strrchr(Comment,'\n') = '\0';
		InsertWatch(Temp,Comment);
	}

	fclose(WatchFile);
	if (RamWatchHWnd)
		ListView_SetItemCount(GetDlgItem(RamWatchHWnd,IDC_WATCHLIST),WatchCount);
	RWfileChanged=false;
	return;
}

char Gens_Path[64]= "M:\\"; //TODO

int Change_File_S(char *Dest, char *Dir, char *Titre, char *Filter, char *Ext, HWND hwnd)
{
	OPENFILENAME ofn;

	SetCurrentDirectory(Gens_Path);

	if (!strcmp(Dest, ""))
	{
		strcpy(Dest, "default.");
		strcat(Dest, Ext);
	}

	memset(&ofn, 0, sizeof(OPENFILENAME));

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

	if (GetSaveFileName(&ofn)) return 1;

	return 0;
}


bool Save_Watches()
{
	Rom_Name = path.GetRomName();
	strncpy(Str_Tmp,Rom_Name.c_str(),512);
	strcat(Str_Tmp,".wch");
	if(Change_File_S(Str_Tmp, Gens_Path, "Save Watches", "DeSmuME Watchlist\0*.wch\0All Files\0*.*\0\0", "wch", RamWatchHWnd))
	{
		FILE *WatchFile = fopen(Str_Tmp,"r+b");
		if (!WatchFile) WatchFile = fopen(Str_Tmp,"w+b");
		fputc('\n',WatchFile);
		strcpy(currentWatch,Str_Tmp);
		RWAddRecentFile(currentWatch);
		sprintf(Str_Tmp,"%d\n",WatchCount);
		fputs(Str_Tmp,WatchFile);
		const char DELIM = '\t';
		for (int i = 0; i < WatchCount; i++)
		{
			sprintf(Str_Tmp,"%05X%c%08X%c%c%c%c%c%d%c%s\n",i,DELIM,rswatches[i].Address,DELIM,rswatches[i].Size,DELIM,rswatches[i].Type,DELIM,rswatches[i].WrongEndian,DELIM,rswatches[i].comment);
			fputs(Str_Tmp,WatchFile);
		}
		
		fclose(WatchFile);
		RWfileChanged=false;
		return true;
	}
	return false;
}

bool QuickSaveWatches()
{
if (RWfileChanged==false) return true; //If file has not changed, no need to save changes
if (currentWatch[0] == NULL)			//If there is no currently loaded file, run to Save as and then return
	{
		return Save_Watches();
	}
		
		strcpy(Str_Tmp,currentWatch);
		FILE *WatchFile = fopen(Str_Tmp,"r+b");
		if (!WatchFile) WatchFile = fopen(Str_Tmp,"w+b");
		fputc('\n',WatchFile);
		sprintf(Str_Tmp,"%d\n",WatchCount);
		fputs(Str_Tmp,WatchFile);
		const char DELIM = '\t';
		for (int i = 0; i < WatchCount; i++)
		{
			sprintf(Str_Tmp,"%05X%c%08X%c%c%c%c%c%d%c%s\n",i,DELIM,rswatches[i].Address,DELIM,rswatches[i].Size,DELIM,rswatches[i].Type,DELIM,rswatches[i].WrongEndian,DELIM,rswatches[i].comment);
			fputs(Str_Tmp,WatchFile);
		}
		fclose(WatchFile);
		RWfileChanged=false;
		return true;
}

bool Load_Watches(bool clear, const char* filename)
{
	const char DELIM = '\t';
	FILE* WatchFile = fopen(filename,"rb");
	if (!WatchFile)
	{
		MessageBox(MESSAGEBOXPARENT,"Error opening file.","ERROR",MB_OK);
		return false;
	}
	if(clear)
	{
		if(!ResetWatches())
		{
			fclose(WatchFile);
			return false;
		}
	}
	strcpy(currentWatch,filename);
	RWAddRecentFile(currentWatch);
	AddressWatcher Temp;
	char mode;
	fgets(Str_Tmp,1024,WatchFile);
	sscanf(Str_Tmp,"%c%*s",&mode);
	int WatchAdd;
	fgets(Str_Tmp,1024,WatchFile);
	sscanf(Str_Tmp,"%d%*s",&WatchAdd);
	WatchAdd+=WatchCount;
	for (int i = WatchCount; i < WatchAdd; i++)
	{
		while (i < 0)
			i++;
		do {
			fgets(Str_Tmp,1024,WatchFile);
		} while (Str_Tmp[0] == '\n');
		sscanf(Str_Tmp,"%*05X%*c%08X%*c%c%*c%c%*c%d",&(Temp.Address),&(Temp.Size),&(Temp.Type),&(Temp.WrongEndian));
		Temp.WrongEndian = 0;
		char *Comment = strrchr(Str_Tmp,DELIM) + 1;
		*strrchr(Comment,'\n') = '\0';
		InsertWatch(Temp,Comment);
	}
	
	fclose(WatchFile);
	if (RamWatchHWnd) {
		ListView_SetItemCount(GetDlgItem(RamWatchHWnd,IDC_WATCHLIST),WatchCount);
		RefreshWatchListSelectedCountControlStatus(RamWatchHWnd);
	}
	RWfileChanged=false;
	return true;
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

	memset(&ofn, 0, sizeof(OPENFILENAME));

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

	if (GetOpenFileName(&ofn)) return 1;

	return 0;
}

bool Load_Watches(bool clear)
{
	Rom_Name = path.GetRomName();
	strncpy(Str_Tmp,Rom_Name.c_str(),512);
	strcat(Str_Tmp,".wch");
	if(Change_File_L(Str_Tmp, Watch_Dir, "Load Watches", "DeSmuME Watchlist\0*.wch\0All Files\0*.*\0\0", "wch", RamWatchHWnd))
	{
		return Load_Watches(clear, Str_Tmp);
	}
	return false;
}

bool ResetWatches()
{
	if(!AskSave())
		return false;
	for (;WatchCount>=0;WatchCount--)
	{
		free(rswatches[WatchCount].comment);
		rswatches[WatchCount].comment = NULL;
	}
	WatchCount++;
	if (RamWatchHWnd) {
		ListView_SetItemCount(GetDlgItem(RamWatchHWnd,IDC_WATCHLIST),WatchCount);
		RefreshWatchListSelectedCountControlStatus(RamWatchHWnd);
	}
	RWfileChanged = false;
	currentWatch[0] = NULL;
	return true;
}

void RemoveWatch(int watchIndex)
{
	free(rswatches[watchIndex].comment);
	rswatches[watchIndex].comment = NULL;
	for (int i = watchIndex; i <= WatchCount; i++)
		rswatches[i] = rswatches[i+1];
	WatchCount--;
}

LRESULT CALLBACK EditWatchProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) //Gets info for a RAM Watch, and then inserts it into the Watch List
{
	RECT r;
	RECT r2;
	int dx1, dy1, dx2, dy2;
	static int index;
	static char s,t = s = 0;

	switch(uMsg)
	{
		case WM_INITDIALOG:
			GetWindowRect(MainWindow->getHWnd(), &r);
			dx1 = (r.right - r.left) / 2;
			dy1 = (r.bottom - r.top) / 2;

			GetWindowRect(hDlg, &r2);
			dx2 = (r2.right - r2.left) / 2;
			dy2 = (r2.bottom - r2.top) / 2;

			SetWindowPos(hDlg, NULL, r.left, r.top, NULL, NULL, SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);
			index = (int)lParam;
			sprintf(Str_Tmp,"%08X",rswatches[index].Address);
			SetDlgItemText(hDlg,IDC_EDIT_COMPAREADDRESS,Str_Tmp);
			if (rswatches[index].comment != NULL)
				SetDlgItemText(hDlg,IDC_PROMPT_EDIT,rswatches[index].comment);
			s = rswatches[index].Size;
			t = rswatches[index].Type;
			switch (s)
			{
				case 'b':
					SendDlgItemMessage(hDlg, IDC_1_BYTE, BM_SETCHECK, BST_CHECKED, 0);
					break;
				case 'w':
					SendDlgItemMessage(hDlg, IDC_2_BYTES, BM_SETCHECK, BST_CHECKED, 0);
					break;
				case 'd':
					SendDlgItemMessage(hDlg, IDC_4_BYTES, BM_SETCHECK, BST_CHECKED, 0);
					break;
				default:
					s = 0;
					break;
			}
			switch (t)
			{
				case 's':
					SendDlgItemMessage(hDlg, IDC_SIGNED, BM_SETCHECK, BST_CHECKED, 0);
					break;
				case 'u':
					SendDlgItemMessage(hDlg, IDC_UNSIGNED, BM_SETCHECK, BST_CHECKED, 0);
					break;
				case 'h':
					SendDlgItemMessage(hDlg, IDC_HEX, BM_SETCHECK, BST_CHECKED, 0);
					break;
				case 'f':
					SendDlgItemMessage(hDlg, IDC_2012, BM_SETCHECK, BST_CHECKED, 0);
					break;
				default:
					t = 0;
					break;
			}

			return true;
			break;
		
		case WM_COMMAND:
			switch(LOWORD(wParam))
			{
				case IDC_SIGNED:
					t='s';
					return true;
				case IDC_UNSIGNED:
					t='u';
					return true;
				case IDC_HEX:
					t='h';
					return true;
				case IDC_2012:
					t='f';
					return true;
				case IDC_1_BYTE:
					s = 'b';
					return true;
				case IDC_2_BYTES:
					s = 'w';
					return true;
				case IDC_4_BYTES:
					s = 'd';
					return true;
				case IDOK:
				{
					if (s && t)
					{
						AddressWatcher Temp;
						Temp.Size = s;
						Temp.Type = t;
						Temp.WrongEndian = false; //replace this when I get little endian working properly
						GetDlgItemText(hDlg,IDC_EDIT_COMPAREADDRESS,Str_Tmp,1024);
						char *addrstr = Str_Tmp;
						if (strlen(Str_Tmp) > 8) addrstr = &(Str_Tmp[strlen(Str_Tmp) - 9]);
						for(int i = 0; addrstr[i]; i++) {if(toupper(addrstr[i]) == 'O') addrstr[i] = '0';}
						sscanf(addrstr,"%08X",&(Temp.Address));

						if((Temp.Address & ~0xFFFFFF) == ~0xFFFFFF)
							Temp.Address &= 0xFFFFFF;

						if(IsHardwareRAMAddressValid(Temp.Address))
						{
							GetDlgItemText(hDlg,IDC_PROMPT_EDIT,Str_Tmp,80);
							if (index < WatchCount) RemoveWatch(index);
							InsertWatch(Temp,Str_Tmp);
							if(RamWatchHWnd)
							{
								ListView_SetItemCount(GetDlgItem(RamWatchHWnd,IDC_WATCHLIST),WatchCount);
							}
							EndDialog(hDlg, true);
						}
						else
						{
							MessageBox(hDlg,"Invalid Address","ERROR",MB_OK);
						}
					}
					else
					{
						strcpy(Str_Tmp,"Error:");
						if (!s)
							strcat(Str_Tmp," Size must be specified.");
						if (!t)
							strcat(Str_Tmp," Type must be specified.");
						MessageBox(hDlg,Str_Tmp,"ERROR",MB_OK);
					}
					RWfileChanged=true;
					return true;
					break;
				}
				case IDCANCEL:
					EndDialog(hDlg, false);
					return false;
					break;
			}
			break;

		case WM_CLOSE:
			EndDialog(hDlg, false);
			return false;
			break;
	}

	return false;
}

void init_list_box(HWND Box, const char* Strs[], int numColumns, int *columnWidths) //initializes the ram search and/or ram watch listbox
{
	LVCOLUMN Col;
	Col.mask = LVCF_FMT | LVCF_ORDER | LVCF_SUBITEM | LVCF_TEXT | LVCF_WIDTH;
	Col.fmt = LVCFMT_CENTER;
	for (int i = 0; i < numColumns; i++)
	{
		Col.iOrder = i;
		Col.iSubItem = i;
		Col.pszText = (LPSTR)(Strs[i]);
		Col.cx = columnWidths[i];
		ListView_InsertColumn(Box,i,&Col);
	}

	ListView_SetExtendedListViewStyle(Box, LVS_EX_FULLROWSELECT);
}

void RamWatchEnableCommand(HWND hDlg, HMENU hMenu, UINT uIDEnableItem, bool enable)
{
	EnableWindow(GetDlgItem(hDlg, uIDEnableItem), (enable?TRUE:FALSE));
	if (hMenu != NULL) {
		if (uIDEnableItem == ID_WATCHES_UPDOWN) {
			EnableMenuItem(hMenu, IDC_C_WATCH_UP, MF_BYCOMMAND | (enable?MF_ENABLED:MF_GRAYED));
			EnableMenuItem(hMenu, IDC_C_WATCH_DOWN, MF_BYCOMMAND | (enable?MF_ENABLED:MF_GRAYED));
		}
		else
			EnableMenuItem(hMenu, uIDEnableItem, MF_BYCOMMAND | (enable?MF_ENABLED:MF_GRAYED));
	}
}

void RefreshWatchListSelectedCountControlStatus(HWND hDlg)
{
	static int prevSelCount=-1;
	int selCount = ListView_GetSelectedCount(GetDlgItem(hDlg,IDC_WATCHLIST));
	if(selCount != prevSelCount)
	{
		if(selCount < 2 || prevSelCount < 2)
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

LRESULT CALLBACK RamWatchProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	RECT r;
	RECT r2;
	int dx1, dy1, dx2, dy2;
	static int watchIndex=0;

	Update_RAM_Watch();

	switch(uMsg)
	{
		case WM_MOVE: {
			RECT wrect;
			GetWindowRect(hDlg,&wrect);
			ramw_x = wrect.left;
			ramw_y = wrect.top;
			break;
			};
			
		case WM_INITDIALOG: {
			GetWindowRect(MainWindow->getHWnd(), &r);  //Ramwatch window
			dx1 = (r.right - r.left) / 2;
			dy1 = (r.bottom - r.top) / 2;

			GetWindowRect(hDlg, &r2); // Gens window
			dx2 = (r2.right - r2.left) / 2;
			dy2 = (r2.bottom - r2.top) / 2;

			
			// push it away from the main window if we can
			const int width = (r.right-r.left);
			const int height = (r.bottom - r.top);
			const int width2 = (r2.right-r2.left); 
			if(r.left+width2 + width < GetSystemMetrics(SM_CXSCREEN))
			{
				r.right += width;
				r.left += width;
			}
			else if((int)r.left - (int)width2 > 0)
			{
				r.right -= width2;
				r.left -= width2;
			}
			
			//-----------------------------------------------------------------------------------
			//If user has Save Window Pos selected, override default positioning
			if (RWSaveWindowPos)	
			{
				//If ramwindow is for some reason completely off screen, use default instead 
				if (ramw_x > (-width*2) || ramw_x < (width*2 + GetSystemMetrics(SM_CYSCREEN))   ) 
					r.left = ramw_x;	  //This also ignores cases of windows -32000 error codes
				//If ramwindow is for some reason completely off screen, use default instead 
				if (ramw_y > (0-height*2) ||ramw_y < (height*2 + GetSystemMetrics(SM_CYSCREEN))	)
					r.top = ramw_y;		  //This also ignores cases of windows -32000 error codes
			}
			//-------------------------------------------------------------------------------------
			SetWindowPos(hDlg, NULL, r.left, r.top, NULL, NULL, SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);
			
			ramwatchmenu=GetMenu(hDlg);
			rwrecentmenu=CreateMenu();
			UpdateRW_RMenu(rwrecentmenu, RAMMENU_FILE_RECENT, RW_MENU_FIRST_RECENT_FILE);
			
			const char* names[3] = {"Address","Value","Notes"};
			int widths[3] = {62,64,64+51+53};
			init_list_box(GetDlgItem(hDlg,IDC_WATCHLIST),names,3,widths);
/*			if (!ResultCount)  //TODO what do these do
				reset_address_info();
			else
				signal_new_frame();*/
			ListView_SetItemCount(GetDlgItem(hDlg,IDC_WATCHLIST),WatchCount);
//			if (!noMisalign) SendDlgItemMessage(hDlg, IDC_MISALIGN, BM_SETCHECK, BST_CHECKED, 0);
//			if (littleEndian) SendDlgItemMessage(hDlg, IDC_ENDIAN, BM_SETCHECK, BST_CHECKED, 0);

			RamWatchAccels = LoadAccelerators(hAppInst, MAKEINTRESOURCE(IDR_ACCELERATOR1));

			// due to some bug in windows, the arrow button width from the resource gets ignored, so we have to set it here
			SetWindowPos(GetDlgItem(hDlg,ID_WATCHES_UPDOWN), 0,0,0, 30,60, SWP_NOMOVE);
			Update_RAM_Watch();
			DragAcceptFiles(hDlg, TRUE);

			RefreshWatchListSelectedCountControlStatus(hDlg);
			return true;
			break;
		}
		
		case WM_INITMENU:
			CheckMenuItem(ramwatchmenu, RAMMENU_FILE_AUTOLOAD, AutoRWLoad ? MF_CHECKED : MF_UNCHECKED);
			CheckMenuItem(ramwatchmenu, RAMMENU_FILE_SAVEWINDOW, RWSaveWindowPos ? MF_CHECKED : MF_UNCHECKED);
			break;

		case WM_MENUSELECT:
 		case WM_ENTERSIZEMOVE:
			break;

		case WM_NOTIFY:
		{
			switch(wParam)
			{
				case ID_WATCHES_UPDOWN:
				{
					switch(((LPNMUPDOWN)lParam)->hdr.code)
					{
						case UDN_DELTAPOS:
							int delta = ((LPNMUPDOWN)lParam)->iDelta;
							SendMessage(hDlg, WM_COMMAND, delta<0 ? IDC_C_WATCH_UP : IDC_C_WATCH_DOWN,0);
							break;
					}
				}

				default:
				{
					LPNMHDR lP = (LPNMHDR) lParam;
					switch (lP->code)
					{
						case LVN_ITEMCHANGED: // selection changed event
						{
							NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)lP;
							if(pNMListView->uNewState & LVIS_FOCUSED ||
								(pNMListView->uNewState ^ pNMListView->uOldState) & LVIS_SELECTED)
							{
								// disable buttons that we don't have the right number of selected items for
								RefreshWatchListSelectedCountControlStatus(hDlg);
							}
						}	break;

						case LVN_GETDISPINFO:
						{
							LV_DISPINFO *Item = (LV_DISPINFO *)lParam;
							Item->item.mask = LVIF_TEXT;
							Item->item.state = 0;
							Item->item.iImage = 0;
							const unsigned int iNum = Item->item.iItem;
							static char num[11];
							switch (Item->item.iSubItem)
							{
								case 0:
									sprintf(num,"%08X",rswatches[iNum].Address);
									Item->item.pszText = num;
									return true;
								case 1: {
									int i = rswatches[iNum].CurValue;
									int t = rswatches[iNum].Type;
									int size = rswatches[iNum].Size;
									const char* formatString = ((t=='s') ? "%d" : (t=='u') ? "%u" : (size=='d' ? "%08X" : size=='w' ? "%04X" : "%02X"));
									if(t=='f')
									{
										sprintf(num,"%f",i/4096.0f);
									}
									else
										switch (size)
										{
											case 'b':
											default: sprintf(num, formatString, t=='s' ? (char)(i&0xff) : (unsigned char)(i&0xff)); break;
											case 'w': sprintf(num, formatString, t=='s' ? (short)(i&0xffff) : (unsigned short)(i&0xffff)); break;
											case 'd': sprintf(num, formatString, t=='s' ? (long)(i&0xffffffff) : (unsigned long)(i&0xffffffff)); break;
										}

									Item->item.pszText = num;
								}	return true;
								case 2:
									Item->item.pszText = rswatches[iNum].comment ? rswatches[iNum].comment : "";
									return true;

								default:
									return false;
							}
						}
						case LVN_ODFINDITEM:
						{	
							// disable search by keyboard typing,
							// because it interferes with some of the accelerators
							// and it isn't very useful here anyway
							SetWindowLongPtr(hDlg, DWLP_MSGRESULT, ListView_GetSelectionMark(GetDlgItem(hDlg,IDC_WATCHLIST)));
							return 1;
						}
					}
				}
			}
			break;
		}

		case WM_COMMAND:
			switch(LOWORD(wParam))
			{
				case RAMMENU_FILE_SAVE:
					QuickSaveWatches();
					break;

				case RAMMENU_FILE_SAVEAS:	
					return Save_Watches();
				case RAMMENU_FILE_OPEN:
					return Load_Watches(true);
				case RAMMENU_FILE_APPEND:
					return Load_Watches(false);
				case RAMMENU_FILE_NEW:
					ResetWatches();
					return true;
				case IDC_C_WATCH_REMOVE:
				{
					HWND watchListControl = GetDlgItem(hDlg, IDC_WATCHLIST);
					watchIndex = ListView_GetNextItem(watchListControl, -1, LVNI_ALL | LVNI_SELECTED);
					while (watchIndex >= 0)
					{
						RemoveWatch(watchIndex);
						ListView_DeleteItem(watchListControl, watchIndex);
						watchIndex = ListView_GetNextItem(watchListControl, -1, LVNI_ALL | LVNI_SELECTED);
					}
					RWfileChanged=true;
					SetFocus(GetDlgItem(hDlg,IDC_WATCHLIST));
					return true;
				}
				case IDC_C_WATCH_EDIT:
					watchIndex = ListView_GetSelectionMark(GetDlgItem(hDlg,IDC_WATCHLIST));
					if(watchIndex != -1)
					{
						DialogBoxParam(hAppInst, MAKEINTRESOURCE(IDD_EDITWATCH), hDlg, (DLGPROC) EditWatchProc,(LPARAM) watchIndex);
						SetFocus(GetDlgItem(hDlg,IDC_WATCHLIST));
					}
					return true;
				case IDC_C_WATCH:
					rswatches[WatchCount].Address = rswatches[WatchCount].WrongEndian = 0;
					rswatches[WatchCount].Size = 'b';
					rswatches[WatchCount].Type = 's';
					DialogBoxParam(hAppInst, MAKEINTRESOURCE(IDD_EDITWATCH), hDlg, (DLGPROC) EditWatchProc,(LPARAM) WatchCount);
					SetFocus(GetDlgItem(hDlg,IDC_WATCHLIST));
					return true;
				case IDC_C_WATCH_DUPLICATE:
					watchIndex = ListView_GetSelectionMark(GetDlgItem(hDlg,IDC_WATCHLIST));
					if(watchIndex != -1)
					{
						rswatches[WatchCount].Address = rswatches[watchIndex].Address;
						rswatches[WatchCount].WrongEndian = rswatches[watchIndex].WrongEndian;
						rswatches[WatchCount].Size = rswatches[watchIndex].Size;
						rswatches[WatchCount].Type = rswatches[watchIndex].Type;
						DialogBoxParam(hAppInst, MAKEINTRESOURCE(IDD_EDITWATCH), hDlg, (DLGPROC) EditWatchProc,(LPARAM) WatchCount);
						SetFocus(GetDlgItem(hDlg,IDC_WATCHLIST));
					}
					return true;

				case IDC_C_WATCH_SEPARATE:
					AddressWatcher separator;
					separator.Address = 0;
					separator.WrongEndian = false;
					separator.Size = 'S';
					separator.Type = 'S';
					InsertWatch(separator, "----------------------------");
					SetFocus(GetDlgItem(hDlg,IDC_WATCHLIST));
					return true;
				
				case IDC_C_WATCH_UP:
				{
					watchIndex = ListView_GetSelectionMark(GetDlgItem(hDlg,IDC_WATCHLIST));
					if (watchIndex == 0 || watchIndex == -1)
						return true;
					void *tmp = malloc(sizeof(AddressWatcher));
					memcpy(tmp,&(rswatches[watchIndex]),sizeof(AddressWatcher));
					memcpy(&(rswatches[watchIndex]),&(rswatches[watchIndex - 1]),sizeof(AddressWatcher));
					memcpy(&(rswatches[watchIndex - 1]),tmp,sizeof(AddressWatcher));
					free(tmp);
					ListView_SetItemState(GetDlgItem(hDlg,IDC_WATCHLIST),watchIndex,0,LVIS_FOCUSED|LVIS_SELECTED);
					ListView_SetSelectionMark(GetDlgItem(hDlg,IDC_WATCHLIST),watchIndex-1);
					ListView_SetItemState(GetDlgItem(hDlg,IDC_WATCHLIST),watchIndex-1,LVIS_FOCUSED|LVIS_SELECTED,LVIS_FOCUSED|LVIS_SELECTED);
					ListView_SetItemCount(GetDlgItem(hDlg,IDC_WATCHLIST),WatchCount);
					RWfileChanged=true;
					return true;
				}
				case IDC_C_WATCH_DOWN:
				{
					watchIndex = ListView_GetSelectionMark(GetDlgItem(hDlg,IDC_WATCHLIST));
					if (watchIndex >= WatchCount - 1 || watchIndex == -1)
						return true;
					void *tmp = malloc(sizeof(AddressWatcher));
					memcpy(tmp,&(rswatches[watchIndex]),sizeof(AddressWatcher));
					memcpy(&(rswatches[watchIndex]),&(rswatches[watchIndex + 1]),sizeof(AddressWatcher));
					memcpy(&(rswatches[watchIndex + 1]),tmp,sizeof(AddressWatcher));
					free(tmp);
					ListView_SetItemState(GetDlgItem(hDlg,IDC_WATCHLIST),watchIndex,0,LVIS_FOCUSED|LVIS_SELECTED);
					ListView_SetSelectionMark(GetDlgItem(hDlg,IDC_WATCHLIST),watchIndex+1);
					ListView_SetItemState(GetDlgItem(hDlg,IDC_WATCHLIST),watchIndex+1,LVIS_FOCUSED|LVIS_SELECTED,LVIS_FOCUSED|LVIS_SELECTED);
					ListView_SetItemCount(GetDlgItem(hDlg,IDC_WATCHLIST),WatchCount);
					RWfileChanged=true;
					return true;
				}
				case RAMMENU_FILE_AUTOLOAD:
				{
					AutoRWLoad ^= 1;
					CheckMenuItem(ramwatchmenu, RAMMENU_FILE_AUTOLOAD, AutoRWLoad ? MF_CHECKED : MF_UNCHECKED);
					break;
				}
				case RAMMENU_FILE_SAVEWINDOW:
				{
					RWSaveWindowPos ^=1;
					CheckMenuItem(ramwatchmenu, RAMMENU_FILE_SAVEWINDOW, RWSaveWindowPos ? MF_CHECKED : MF_UNCHECKED);
					break;
				}
				case IDC_C_ADDCHEAT:
				{
					watchIndex = ListView_GetSelectionMark(GetDlgItem(hDlg,IDC_WATCHLIST));
					if(watchIndex != -1)
					{
						u32 address = rswatches[watchIndex].Address;
						u8 size = (rswatches[watchIndex].Size=='b') ? 1 : (rswatches[watchIndex].Size=='w' ? 2 : 4);
						u32 value = rswatches[watchIndex].CurValue;
						const char* desc = rswatches[watchIndex].comment;
						CheatsAddDialog(hDlg, address, value, size, desc);
					}
					break;
				}
				case IDCANCEL:
					RamWatchHWnd = NULL;
					DragAcceptFiles(hDlg, FALSE);
					EndDialog(hDlg, true);
					return true;
				default:
					if (LOWORD(wParam) >= RW_MENU_FIRST_RECENT_FILE && LOWORD(wParam) < RW_MENU_FIRST_RECENT_FILE+MAX_RECENT_WATCHES)
					OpenRWRecentFile(LOWORD(wParam) - RW_MENU_FIRST_RECENT_FILE);
			}
			break;
		
		case WM_KEYDOWN: // handle accelerator keys
		{
			SetFocus(GetDlgItem(hDlg,IDC_WATCHLIST));
			MSG msg;
			msg.hwnd = hDlg;
			msg.message = uMsg;
			msg.wParam = wParam;
			msg.lParam = lParam;
			if(RamWatchAccels && TranslateAccelerator(hDlg, RamWatchAccels, &msg))
				return true;
		}	break;

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
		}	break;
	}

	return false;
}

