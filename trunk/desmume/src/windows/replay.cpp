/*
	Copyright (C) 2008-2010 DeSmuME team

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

#include "types.h"
#include <winsock2.h>
#include <windows.h>
#include <commdlg.h>
#include <io.h>
#include <fstream>
#include <time.h>
#include "resource.h"
#include "replay.h"
#include "common.h"
#include "main.h"
#include "movie.h"
#include "rtc.h"
#include "utils/xstring.h"

bool replayreadonly=1;

//adelikat: TODO: put this in one of the header files
template<int BUFSIZE>
inline std::wstring GetDlgItemTextW(HWND hDlg, int nIDDlgItem) {
	wchar_t buf[BUFSIZE];
	GetDlgItemTextW(hDlg, nIDDlgItem, buf, BUFSIZE);
	return buf;
}

template<int BUFSIZE>
inline std::string GetDlgItemText(HWND hDlg, int nIDDlgItem) {
	char buf[BUFSIZE];
	GetDlgItemText(hDlg, nIDDlgItem, buf, BUFSIZE);
	return buf;
}


// tests if a file is readable at the given location
// without changing whatever is there
static BOOL IsFileReadable(char* filePath)
{
	if(!filePath || GetFileAttributes(filePath) == 0xFFFFFFFF)
		return false;
	FILE* file = fopen(filePath, "rb");
	if(file)
		fclose(file);
	return file != 0;
}

// tests if a file is writable at the given location
// without changing whatever is there
static BOOL IsFileWritable(char* filePath)
{
	if(!filePath)
		return false;
	bool didNotExist = GetFileAttributes(filePath) == 0xFFFFFFFF;
	FILE* file = fopen(filePath, "ab");
	if(file)
	{
		fclose(file);
		if(didNotExist)
			unlink(filePath);
		return true;
	}
	return false;
}

// if there's no directory (only a filename), make it an absolute path,
// otherwise the user won't have any clue where the file actually is...
void FixRelativeMovieFilename(HWND hwndDlg, DWORD dlgItem)
{
	char tempfname [MAX_PATH];
	char fullfname [MAX_PATH];
	GetDlgItemText(hwndDlg,dlgItem,tempfname,MAX_PATH);

	// NOTE: if this puts the wrong path in, then the solution is to call SetCurrentDirectory beforehand,
	// since if it's wrong then even without this code that's the wrong place it would get saved to.
	// also, single-letter filenames are passed by here in case the user is deleting the path from the right and goes from "c:" to "c"
	if(tempfname[0] && tempfname[1] && !strchr(tempfname, '/') && !strchr(tempfname, '\\') && !strchr(tempfname, ':')
	&& GetFullPathName(tempfname,MAX_PATH-4,fullfname,NULL))
	{
		// store cursor position
		int sel1=-1, sel2=0;
		SendMessage(GetDlgItem(hwndDlg, dlgItem), EM_GETSEL, (WPARAM)&sel1, (LPARAM)&sel2);

		// add dsm if no extension
		if(!strchr(fullfname,'.'))
			strcat(fullfname, ".dsm");

		// replace text with the absolute path + extension
		SetDlgItemText(hwndDlg, dlgItem, fullfname);

		// keep cursor where user was typing
		char* match = fullfname;
		while(strstr(match+1, tempfname)) match = strstr(match+1, tempfname); // strrstr
		if(match > fullfname)
		{
			sel1 += match - fullfname;
			sel2 += match - fullfname;
			SendMessage(GetDlgItem(hwndDlg, dlgItem), EM_SETSEL, sel1, sel2);
		}
	}
}



static char playfilename[MAX_PATH] = "";

void Describe(HWND hwndDlg)
{
	EMUFILE_FILE fp(playfilename,"rb");
	if(fp.fail()) return;
	MovieData md;
	LoadFM2(md, &fp, INT_MAX, false);

	u32 num_frames = md.records.size();

	double tempCount = (num_frames / (33513982.0/6/355/263)) + 0.005; // +0.005s for rounding
	int num_seconds = (int)tempCount;
	int fraction = (int)((tempCount - num_seconds) * 100);
	int seconds = num_seconds % 60;
	int minutes = (num_seconds / 60) % 60;
	int hours = (num_seconds / 60 / 60) % 60;
	char tmp[256];
	sprintf(tmp, "%02d:%02d:%02d.%02d", hours, minutes, seconds, fraction);

	SetDlgItemText(hwndDlg,IDC_MLENGTH,tmp);
	SetDlgItemInt(hwndDlg,IDC_MFRAMES,num_frames,FALSE);
	SetDlgItemInt(hwndDlg,IDC_MRERECORDCOUNT,md.rerecordCount,FALSE);
	SetDlgItemText(hwndDlg,IDC_MROM,md.romSerial.c_str());
}

//Replay movie dialog
INT_PTR CALLBACK ReplayDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	OPENFILENAME ofn;
	char szChoice[MAX_PATH]={0};
	char filename[MAX_PATH] = "";

	switch(uMsg)
	{
	case WM_INITDIALOG:
	{
		SendDlgItemMessage(hwndDlg, IDC_CHECK_READONLY, BM_SETCHECK, replayreadonly?BST_CHECKED:BST_UNCHECKED, 0);			
		
		//Clear fields
		SetWindowText(GetDlgItem(hwndDlg, IDC_MLENGTH), "");
		SetWindowText(GetDlgItem(hwndDlg, IDC_MFRAMES), "");
		SetWindowText(GetDlgItem(hwndDlg, IDC_MRERECORDCOUNT), "");
		SetWindowText(GetDlgItem(hwndDlg, IDC_MROM), "");

		extern char curMovieFilename[512];
		strncpy(playfilename, curMovieFilename, MAX_PATH);
		playfilename[MAX_PATH-1] = '\0';

		SetWindowText(GetDlgItem(hwndDlg, PM_FILENAME), playfilename);
		SetFocus(GetDlgItem(hwndDlg, PM_FILENAME));
		SendMessage(GetDlgItem(hwndDlg, PM_FILENAME), EM_SETSEL, 0, -1); // select all

	}	return FALSE;

	case WM_COMMAND:	
		int wID = LOWORD(wParam);
		switch(wID)
		{
			case ID_BROWSE:

				ZeroMemory(&ofn, sizeof(ofn));
				ofn.lStructSize = sizeof(ofn);
				ofn.hwndOwner = hwndDlg;
				ofn.lpstrFilter = "Desmume Movie File (*.dsm)\0*.dsm\0All files(*.*)\0*.*\0\0";
				ofn.nFilterIndex = 1;
				ofn.lpstrFile =  filename;
				ofn.lpstrTitle = "Replay Movie from File";
				ofn.nMaxFile = MAX_PATH;
				ofn.lpstrDefExt = "dsm";
				ofn.Flags = OFN_HIDEREADONLY | OFN_FILEMUSTEXIST;
				if(GetOpenFileName(&ofn))
					SetDlgItemText(hwndDlg, PM_FILENAME, filename);
				return true;
		
			case IDC_CHECK_READONLY:
				replayreadonly = IsDlgButtonChecked(hwndDlg, IDC_CHECK_READONLY) != 0;
				return true;

			case IDOK:	
				FCEUI_LoadMovie(playfilename, replayreadonly, false, 80000);
				ZeroMemory(&playfilename, sizeof(playfilename));
				EndDialog(hwndDlg, 0);
				return true;

			case IDCANCEL:
				ZeroMemory(&playfilename, sizeof(playfilename));
				EndDialog(hwndDlg, 0);
				return true;

			case PM_FILENAME:
				switch(HIWORD(wParam))
				{
					case EN_CHANGE:
					{
						FixRelativeMovieFilename(hwndDlg, PM_FILENAME);

						// disable the OK button if we can't read the file
						char filename [MAX_PATH];
						GetDlgItemText(hwndDlg,PM_FILENAME,filename,MAX_PATH);
						EnableWindow(GetDlgItem(hwndDlg, IDOK), IsFileReadable(filename));
						strcpy(playfilename, filename);
						Describe(hwndDlg);

						// force read-only to be checked if we can't write the file
						if(!IsFileWritable(filename))
						{
							CheckDlgButton(hwndDlg, IDC_CHECK_READONLY, BST_CHECKED);
							EnableWindow(GetDlgItem(hwndDlg, IDC_CHECK_READONLY), FALSE);
						}
						else
						{
							EnableWindow(GetDlgItem(hwndDlg, IDC_CHECK_READONLY), TRUE);
						}
					}
					break;
				}
				break;

		}
	}

	return false;
}


int flag=0;

//Record movie dialog
static INT_PTR CALLBACK RecordDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static struct CreateMovieParameters* p = NULL;
	std::wstring author = L"";
	std::string fname;
	SYSTEMTIME systime;
	switch(uMsg)
	{
		case WM_INITDIALOG: {
			CheckDlgButton(hwndDlg, IDC_START_FROM_SRAM, ((flag == 1) ? BST_CHECKED : BST_UNCHECKED));
			SetFocus(GetDlgItem(hwndDlg, IDC_EDIT_FILENAME));

			DateTime t = FCEUI_MovieGetRTCDefault();
			ZeroMemory(&systime, sizeof(SYSTEMTIME));
			systime.wYear = t.get_Year();
			systime.wMonth = t.get_Month();
			systime.wDay = t.get_Day();
			systime.wDayOfWeek = t.get_DayOfWeek();
			systime.wHour = t.get_Hour();
			systime.wMinute = t.get_Minute();
			systime.wSecond = t.get_Second();
			systime.wMilliseconds = t.get_Millisecond();
			DateTime_SetSystemtime(GetDlgItem(hwndDlg, IDC_DTP_DATE), GDT_VALID, &systime);
			DateTime_SetSystemtime(GetDlgItem(hwndDlg, IDC_DTP_TIME), GDT_VALID, &systime);

			union {
				struct { SYSTEMTIME rtcMin, rtcMax; };
				SYSTEMTIME rtcMinMax[2];
			};
			ZeroMemory(&rtcMin, sizeof(SYSTEMTIME));
			ZeroMemory(&rtcMax, sizeof(SYSTEMTIME));
			rtcMin.wYear = 2000;
			rtcMin.wMonth = 1;
			rtcMin.wDay = 1;
			rtcMin.wDayOfWeek = 6;
			rtcMax.wYear = 2099;
			rtcMax.wMonth = 12;
			rtcMax.wDay = 31;
			rtcMax.wDayOfWeek = 4;
			DateTime_SetRange(GetDlgItem(hwndDlg, IDC_DTP_DATE), GDTR_MIN, &rtcMinMax);
			DateTime_SetRange(GetDlgItem(hwndDlg, IDC_DTP_DATE), GDTR_MAX, &rtcMinMax);
			return false;
		}

		case WM_COMMAND:
			switch(LOWORD(wParam))
			{
			case IDOK: {
				author = GetDlgItemTextW<500>(hwndDlg,IDC_EDIT_AUTHOR);
				fname = GetDlgItemText<MAX_PATH>(hwndDlg,IDC_EDIT_FILENAME);
				std::string sramfname = GetDlgItemText<MAX_PATH>(hwndDlg,IDC_EDIT_SRAMFILENAME);
				if (fname.length())
				{
					struct tm t;

					DateTime_GetSystemtime(GetDlgItem(hwndDlg, IDC_DTP_DATE), &systime);
					t.tm_year = systime.wYear;
					t.tm_mon  = systime.wMonth;
					t.tm_mday = systime.wDay;
					t.tm_wday = systime.wDayOfWeek;
					DateTime_GetSystemtime(GetDlgItem(hwndDlg, IDC_DTP_TIME), &systime);
					t.tm_hour = systime.wHour;
					t.tm_min  = systime.wMinute;
					t.tm_sec  = systime.wSecond;
					DateTime rtcstart(t.tm_year,t.tm_mon,t.tm_mday,t.tm_hour,t.tm_min,t.tm_sec);

					FCEUI_SaveMovie(fname.c_str(), author, flag, sramfname, rtcstart);
					EndDialog(hwndDlg, 0);
				}
				return true;
			}

			case IDCANCEL:
				EndDialog(hwndDlg, 0);
				return true;
	
			case IDC_BUTTON_BROWSEFILE:
			{
				OPENFILENAME ofn;
				char szChoice[MAX_PATH]={0};
				GetDlgItemText(hwndDlg,IDC_EDIT_FILENAME,szChoice,MAX_PATH);

				// browse button
				ZeroMemory(&ofn, sizeof(ofn));
				ofn.lStructSize = sizeof(ofn);
				ofn.hwndOwner = hwndDlg;
				ofn.lpstrFilter = "Desmume Movie File (*.dsm)\0*.dsm\0All files(*.*)\0*.*\0\0";
				ofn.lpstrFile = szChoice;
				ofn.lpstrTitle = "Record a new movie";
				ofn.lpstrDefExt = "dsm";
				ofn.nMaxFile = MAX_PATH;
				ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOREADONLYRETURN | OFN_PATHMUSTEXIST;
				if(GetSaveFileName(&ofn))
				{
					fname = szChoice;
/* // windows does this automatically, since lpstrDefExt is set
					//If user did not specify an extension, add .dsm for them
					x = fname.find_last_of(".");
					if (x < 0)
						fname.append(".dsm");
*/
					SetDlgItemText(hwndDlg, IDC_EDIT_FILENAME, fname.c_str());
				}
				//if(GetSaveFileName(&ofn))
				//	UpdateRecordDialogPath(hwndDlg,szChoice);

				return true;
			}
			case IDC_BUTTON_BROWSESRAM:
			{
				OPENFILENAME ofn;
				char szChoice[MAX_PATH]={0};

				// browse button
				ZeroMemory(&ofn, sizeof(ofn));
				ofn.lStructSize = sizeof(ofn);
				ofn.hwndOwner = hwndDlg;
				ofn.lpstrFilter = "Desmume SRAM File (*.dsv)\0*.dsv\0All files(*.*)\0*.*\0\0";
				ofn.lpstrFile = szChoice;
				ofn.lpstrTitle = "Choose SRAM";
				ofn.lpstrDefExt = "dsv";
				ofn.nMaxFile = MAX_PATH;
				ofn.Flags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
				if(GetOpenFileName(&ofn))
				{
					fname = szChoice;
/* // windows does this automatically, since lpstrDefExt is set
				//If user did not specify an extension, add .dsv for them
				x = fname.find_last_of(".");
				if (x < 0)
					fname.append(".dsv");
*/
					SetDlgItemText(hwndDlg, IDC_EDIT_SRAMFILENAME, fname.c_str());
				}
				//if(GetSaveFileName(&ofn))
				//	UpdateRecordDialogPath(hwndDlg,szChoice);

				return true;
			}

			case IDC_EDIT_FILENAME:
				switch(HIWORD(wParam))
				{
					case EN_CHANGE:
					{
						FixRelativeMovieFilename(hwndDlg, IDC_EDIT_FILENAME);

						// disable the OK button if we can't write to the file
						char filename [MAX_PATH];
						GetDlgItemText(hwndDlg,IDC_EDIT_FILENAME,filename,MAX_PATH);
						EnableWindow(GetDlgItem(hwndDlg, IDOK), IsFileWritable(filename));
					}
					break;
				}
				break;
		}
	}

	HWND cur = GetDlgItem(hwndDlg, IDC_EDIT_SRAMFILENAME);
	
	IsDlgButtonChecked(hwndDlg, IDC_START_FROM_SRAM) ? flag=1 : flag=0;
	IsDlgButtonChecked(hwndDlg, IDC_START_FROM_SRAM) ? EnableWindow(cur, TRUE) : EnableWindow(cur, FALSE);

	cur = GetDlgItem(hwndDlg, IDC_BUTTON_BROWSESRAM);
	IsDlgButtonChecked(hwndDlg, IDC_START_FROM_SRAM) ? EnableWindow(cur, TRUE) : EnableWindow(cur, FALSE);

	return false;
}


//Show the play movie dialog and play a movie
void Replay_LoadMovie()
{
	char* fn = (char*)DialogBoxParamW(hAppInst, MAKEINTRESOURCEW(IDD_REPLAYINP), MainWindow->getHWnd(), ReplayDialogProc, false);

	if(fn)
	{
		FCEUI_LoadMovie(fn, movie_readonly, false, -1);

		free(fn);
	}
}

//Show the record movie dialog and record a movie.
void MovieRecordTo()
{
	DialogBoxParamW(hAppInst, MAKEINTRESOURCEW(IDD_RECORDMOVIE), MainWindow->getHWnd(), RecordDialogProc, (LPARAM)0);
}
