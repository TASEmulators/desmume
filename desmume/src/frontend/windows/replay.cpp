/*
	Copyright (C) 2008-2015 DeSmuME team

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

#include "replay.h"

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <io.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "types.h"
#include "movie.h"
#include "NDSSystem.h"
#include "path.h"
#include "utils/xstring.h"

// DSM header parsing, compatibility assessment and all display formatting live
// in the frontend-independent shared layer, so the Cocoa and (future) GTK
// previews show exactly the same values as this dialog.
#include "../shared/movie_info_core.h"

#include "resource.h"
#include "main.h"
#include "rtc.h"

static bool replayreadonly = true;
static START_FROM startFrom = START_BLANK;

//------------------------------------------------------------------------------
// Small Win32 helpers. All strings in this file are ANSI unless the W suffix is
// used; the DSM header itself is UTF-8, matching utils/xstring.h.
//------------------------------------------------------------------------------

static std::string GetTextA(HWND dlg, int id)
{
	char buf[1024];
	GetDlgItemTextA(dlg, id, buf, (int)sizeof(buf));
	return buf;
}

static std::wstring GetTextW(HWND dlg, int id)
{
	wchar_t buf[1024];
	GetDlgItemTextW(dlg, id, buf, (int)(sizeof(buf) / sizeof(buf[0])));
	return buf;
}

static void SetTextA(HWND dlg, int id, const std::string &text)
{
	SetDlgItemTextA(dlg, id, text.c_str());
}

static void SetTextW(HWND dlg, int id, const std::wstring &text)
{
	SetDlgItemTextW(dlg, id, text.c_str());
}

//------------------------------------------------------------------------------
// Movie file locations. Both dialogs resolve their default path through
// File > Path Settings > Movies. getpath() turns the configured value (default
// ".\\Movies") into an absolute path relative to the module directory and
// creates the directory, so the edit boxes always show a usable path.
//------------------------------------------------------------------------------

static std::string MovieDirectory()
{
	// ReadPathSettings() normally seeds pathToMovies, and its ini fallback is
	// ".\\Movies". Keep that default even if the settings have not been loaded
	// yet or the user cleared the field in Path Settings.
	if (path.pathToMovies[0] == '\0')
		strcpy(path.pathToMovies, ".\\Movies");

	std::string dir = path.getpath(path.MOVIES);
	if (dir.empty())
		return ".\\Movies\\";
	char last = dir[dir.size() - 1];
	if (last != '\\' && last != '/')
		dir += '\\';

	// Drop the ".\\" component so the displayed path is clean.
	char full[MAX_PATH] = "";
	if (GetFullPathNameA(dir.c_str(), MAX_PATH, full, NULL) && full[0])
	{
		std::string normalized = full;
		if (normalized[normalized.size() - 1] != '\\')
			normalized += '\\';
		return normalized;
	}
	return dir;
}

static std::string MovieBaseName()
{
	std::string base;
	if (romloaded)
		base = path.GetRomNameWithoutExtension();
	if (base.empty())
		base = "movie";
	return base;
}

// Returns only the file name shown in the edit box; the Movies directory is
// added implicitly by ResolveMoviePath(). The collision check still runs on
// the full path. unique=true is used by Record Movie (an existing movie is
// never overwritten: "ROM.dsm" -> "ROM - 1.dsm" -> "ROM - 2.dsm"); Play Movie
// always shows the plain default name.
static std::string DefaultMovieFilename(bool unique)
{
	return MovieInfo::DefaultMovieFileName(MovieBaseName(), MovieDirectory(), unique);
}

// tests if a file is readable at the given location without changing whatever is there
static BOOL IsFileReadable(const char *filePath)
{
	if (!filePath || !filePath[0] || GetFileAttributesA(filePath) == 0xFFFFFFFF)
		return FALSE;
	FILE *file = fopen(filePath, "rb");
	if (file)
		fclose(file);
	return file != NULL;
}

// tests if a file is writable at the given location without changing whatever is there
static BOOL IsFileWritable(const char *filePath)
{
	if (!filePath || !filePath[0])
		return FALSE;
	bool didNotExist = GetFileAttributesA(filePath) == 0xFFFFFFFF;
	FILE *file = fopen(filePath, "ab");
	if (file)
	{
		fclose(file);
		if (didNotExist)
			remove(filePath);
		return TRUE;
	}
	return FALSE;
}

static void EnsureDsmExtension(std::string &fname)
{
	fname = MovieInfo::EnsureDsmExtension(fname);
}

// The movie edit boxes show only a file name. A bare name is resolved against
// the Movies directory from Path Settings; a path (absolute, relative, or one
// returned by Browse) is used unchanged. This keeps the prefix implicit in the
// UI while still letting Browse pick a movie from any directory.
static std::string ResolveMoviePath(const std::string &text)
{
	if (text.empty())
		return text;
	std::string full = text;
	if (!strpbrk(full.c_str(), "\\/:"))
		full = MovieDirectory() + full;
	EnsureDsmExtension(full);
	return full;
}

// The Vista+ common item dialog ignores the directory portion of lpstrFile,
// so without lpstrInitialDir a browse dialog opens in the last folder used by
// any application instead of the folder shown in the movie edit box. Seed
// lpstrFile from the edit control and pass its directory explicitly.
static void SetFileDialogInitialDirFromEdit(HWND dlg, int editId, OPENFILENAMEA &ofn,
                                            char *file, int fileSize,
                                            char *dir, int dirSize,
                                            const std::string &fallbackDir)
{
	GetDlgItemTextA(dlg, editId, file, fileSize);
	dir[0] = '\0';

	const char *slash = strrchr(file, '\\');
	const char *fslash = strrchr(file, '/');
	const char *sep = (slash && fslash) ? (slash > fslash ? slash : fslash) : (slash ? slash : fslash);
	if (sep)
	{
		int len = (int)(sep - file);
		if (len > 0 && len < dirSize - 2)
		{
			memcpy(dir, file, (size_t)len);
			dir[len] = '\0';
			// "C:" alone means "current directory on C:"; make it explicit.
			if (len == 2 && dir[1] == ':')
			{
				dir[2] = '\\';
				dir[3] = '\0';
			}
		}
	}
	else if (file[0] && !fallbackDir.empty())
	{
		// The edit box holds a bare name, so its directory is implicit.
		std::string d = fallbackDir;
		while (!d.empty() && (d[d.size() - 1] == '\\' || d[d.size() - 1] == '/'))
			d.erase(d.size() - 1);
		strncpy(dir, d.c_str(), dirSize - 1);
		dir[dirSize - 1] = '\0';
	}
	ofn.lpstrInitialDir = dir[0] ? dir : NULL;
}

//------------------------------------------------------------------------------
// Win32 helpers
//------------------------------------------------------------------------------

static void SetCheckFromValue(HWND dlg, int id, const std::string &raw)
{
	CheckDlgButton(dlg, id, atoi(raw.c_str()) != 0 ? BST_CHECKED : BST_UNCHECKED);
}

//------------------------------------------------------------------------------
// Play movie dialog
//------------------------------------------------------------------------------

static std::string g_previewPath;
static MovieInfo::Header g_header;
static std::wstring g_originalDescription;
static bool g_coreMismatch = false;
static bool g_romMismatch = false;

static void ClearPlayFields(HWND dlg)
{
	SetTextA(dlg, IDC_MOVIE_ROM, "Movie ROM: -");
	SetTextA(dlg, IDC_YOUR_ROM, "Your ROM: -");
	SetTextW(dlg, IDC_EDIT_AUTHOR, L"");
	SetTextW(dlg, IDC_PLAY_DESCRIPTION, L"");
	SetTextA(dlg, IDC_MRECORD_DATE, "Unknown");
	SetTextA(dlg, IDC_MCORE, "-");
	SetTextA(dlg, IDC_MLENGTH, "-");
	SetTextA(dlg, IDC_MFRAMES, "-");
	SetTextA(dlg, IDC_MRERECORDCOUNT, "-");
	SetTextW(dlg, IDC_MNICKNAME, L"");
	SetTextA(dlg, IDC_MFIRMWARE, "-");
	SetTextA(dlg, IDC_MLANGUAGE, "-");

	CheckDlgButton(dlg, IDC_MREC_POWER, BST_UNCHECKED);
	CheckDlgButton(dlg, IDC_MREC_SRAM, BST_UNCHECKED);
	CheckDlgButton(dlg, IDC_MREC_SNAPSHOT, BST_UNCHECKED);
	SetCheckFromValue(dlg, IDC_MBIOSEXT, "");
	SetCheckFromValue(dlg, IDC_MBIOSWI, "");
	SetCheckFromValue(dlg, IDC_MFIRMEXT, "");
	SetCheckFromValue(dlg, IDC_MBOOTFW, "");

	EnableWindow(GetDlgItem(dlg, IDC_CHECK_PAUSEFRAME), FALSE);
	EnableWindow(GetDlgItem(dlg, IDC_EDIT_PAUSEFRAME), FALSE);
	SetTextA(dlg, IDC_EDIT_PAUSEFRAME, "");
	CheckDlgButton(dlg, IDC_CHECK_PAUSEFRAME, BST_UNCHECKED);

	g_originalDescription.clear();
	g_coreMismatch = false;
	g_romMismatch = false;
}

static void RefreshPlayDialog(HWND dlg, bool force)
{
	std::string filename = ResolveMoviePath(GetTextA(dlg, PM_FILENAME));
	bool readable = IsFileReadable(filename.c_str()) != FALSE;
	bool writable = IsFileWritable(filename.c_str()) != FALSE;

	EnableWindow(GetDlgItem(dlg, IDOK), readable);

	if (!writable)
	{
		CheckDlgButton(dlg, IDC_CHECK_READONLY, BST_CHECKED);
		EnableWindow(GetDlgItem(dlg, IDC_CHECK_READONLY), FALSE);
	}
	else
		EnableWindow(GetDlgItem(dlg, IDC_CHECK_READONLY), TRUE);

	bool readOnly = IsDlgButtonChecked(dlg, IDC_CHECK_READONLY) != 0;
	replayreadonly = readOnly;
	SendMessage(GetDlgItem(dlg, IDC_PLAY_DESCRIPTION), EM_SETREADONLY, readOnly ? TRUE : FALSE, 0);

	// Typing into the path fires EN_CHANGE for every character. Only re-read the
	// movie when the path actually changed (or the user pressed Refresh).
	if (!force && filename == g_previewPath)
		return;
	g_previewPath = filename;

	MovieInfo::Header hdr;
	if (!readable || !MovieInfo::ReadHeader(filename, hdr))
	{
		ClearPlayFields(dlg);
		return;
	}
	g_header = hdr;

	// ROM/core comparison and all display text come from the shared layer.
	const MovieInfo::RomIdentity rom = MovieInfo::CurrentRomIdentity();
	const MovieInfo::Assessment assessment = MovieInfo::Assess(hdr, rom, MovieInfo::CurrentCoreVersion());
	g_romMismatch = assessment.romMismatch;
	g_coreMismatch = assessment.coreMismatch;

	const std::string movieCrcText = assessment.crcComparable
	                                  ? hdr.romChecksum : std::string("n/a (legacy)");
	const std::string movieName = MovieInfo::BaseName(hdr.romFilename);
	SetTextA(dlg, IDC_MOVIE_ROM, MovieInfo::FormatRomLine("Movie ROM: ",
	         hdr.romSerial.empty() ? "-" : hdr.romSerial, movieCrcText,
	         "name=", movieName.empty() ? "-" : MovieInfo::Shorten(movieName, 32)));
	SetTextA(dlg, IDC_YOUR_ROM, MovieInfo::FormatRomLine("Your ROM:  ",
	         rom.serial.empty() ? "-" : rom.serial, MovieInfo::FormatHexCrc(rom.crc),
	         "title=", rom.internalName));

	std::string authorUtf8, descriptionUtf8;
	MovieInfo::SplitComments(hdr, authorUtf8, descriptionUtf8);
	const std::wstring author = mbstowcs(authorUtf8);
	const std::wstring description = mbstowcs(descriptionUtf8);
	SetTextW(dlg, IDC_EDIT_AUTHOR, author);
	SetTextW(dlg, IDC_PLAY_DESCRIPTION, description);
	g_originalDescription = description;

	SetTextA(dlg, IDC_MRECORD_DATE, MovieInfo::FormatRecordDate(hdr.recordDate));
	SetTextA(dlg, IDC_MCORE, MovieInfo::CoreVersionText(hdr, assessment));
	SetTextA(dlg, IDC_MLENGTH, MovieInfo::FormatLength(hdr.frames));
	SetDlgItemInt(dlg, IDC_MFRAMES, hdr.frames, FALSE);
	SetDlgItemInt(dlg, IDC_MRERECORDCOUNT, (UINT)MovieInfo::ParseUInt(hdr.rerecordCount), FALSE);

	// Recording start option, read-only here: playback cannot change where the
	// movie begins.
	bool fromSnapshot = hdr.savestate == "1";
	bool fromSram = !fromSnapshot && hdr.hasSram;
	CheckDlgButton(dlg, IDC_MREC_POWER, (!fromSnapshot && !fromSram) ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(dlg, IDC_MREC_SRAM, fromSram ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(dlg, IDC_MREC_SNAPSHOT, fromSnapshot ? BST_CHECKED : BST_UNCHECKED);

	// Console / firmware: DSM has no console-type flag. Show the fixed target
	// and the firmware settings the movie actually carries.
	SetTextA(dlg, IDC_MSYSTEM, "Nintendo DS");
	SetTextA(dlg, IDC_MFIRMWARE, MovieInfo::DescribeFirmware(hdr.useExtFirmware));
	SetTextA(dlg, IDC_MLANGUAGE, MovieInfo::DescribeLanguage(hdr.firmLanguage));
	SetTextW(dlg, IDC_MNICKNAME, hdr.firmNickname.empty() ? std::wstring(L"-") : mbstowcs(hdr.firmNickname));

	// Four independent DSM booleans, not VBA's single four-way BIOS choice.
	SetCheckFromValue(dlg, IDC_MBIOSEXT, hdr.useExtBios);
	SetCheckFromValue(dlg, IDC_MBIOSWI, hdr.swiFromBios);
	SetCheckFromValue(dlg, IDC_MFIRMEXT, hdr.useExtFirmware);
	SetCheckFromValue(dlg, IDC_MBOOTFW, hdr.bootFromFirmware);

	EnableWindow(GetDlgItem(dlg, IDC_CHECK_PAUSEFRAME), TRUE);
	bool pauseWanted = IsDlgButtonChecked(dlg, IDC_CHECK_PAUSEFRAME) != 0;
	EnableWindow(GetDlgItem(dlg, IDC_EDIT_PAUSEFRAME), pauseWanted);
	if (pauseWanted)
		SetDlgItemInt(dlg, IDC_EDIT_PAUSEFRAME, hdr.frames, FALSE);
	else
		SetTextA(dlg, IDC_EDIT_PAUSEFRAME, "");
}

static INT_PTR CALLBACK ReplayDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_INITDIALOG:
	{
		g_previewPath.clear();
		g_header = MovieInfo::Header();
		g_originalDescription.clear();
		g_coreMismatch = false;
		g_romMismatch = false;

		CheckDlgButton(hwndDlg, IDC_CHECK_READONLY, replayreadonly ? BST_CHECKED : BST_UNCHECKED);
		// Always start at Movies\ROM_NAME.dsm. The play dialog deliberately does
		// not pre-fill curMovieFilename (the movie currently being played).
		SetTextA(hwndDlg, PM_FILENAME, DefaultMovieFilename(false));
		SetFocus(GetDlgItem(hwndDlg, PM_FILENAME));
		SendMessage(GetDlgItem(hwndDlg, PM_FILENAME), EM_SETSEL, 0, -1);
		RefreshPlayDialog(hwndDlg, true);
		return FALSE;
	}

	case WM_CTLCOLORSTATIC:
	{
		HDC hdc = (HDC)wParam;
		HWND ctrl = (HWND)lParam;
		int id = GetDlgCtrlID(ctrl);
		if ((id == IDC_MCORE && g_coreMismatch) || (id == IDC_YOUR_ROM && g_romMismatch))
		{
			SetTextColor(hdc, RGB(220, 40, 40));
			SetBkMode(hdc, TRANSPARENT);
			return (INT_PTR)GetSysColorBrush(COLOR_BTNFACE);
		}
		return FALSE;
	}

	case WM_COMMAND:
	{
		int wID = LOWORD(wParam);
		int code = HIWORD(wParam);
		switch (wID)
		{
		case ID_BROWSE:
		{
			OPENFILENAMEA ofn;
			char filename[MAX_PATH] = "";
			char initialDir[MAX_PATH] = "";
			ZeroMemory(&ofn, sizeof(ofn));
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = hwndDlg;
			ofn.lpstrFilter = "DeSmuME Movie File (*.dsm)\0*.dsm\0All files (*.*)\0*.*\0\0";
			ofn.nFilterIndex = 1;
			ofn.lpstrFile = filename;
			SetFileDialogInitialDirFromEdit(hwndDlg, PM_FILENAME, ofn,
			                                filename, (int)sizeof(filename),
			                                initialDir, (int)sizeof(initialDir), MovieDirectory());
			ofn.lpstrTitle = "Play Movie from File";
			ofn.nMaxFile = MAX_PATH;
			ofn.lpstrDefExt = "dsm";
			ofn.Flags = OFN_HIDEREADONLY | OFN_FILEMUSTEXIST;
			if (GetOpenFileNameA(&ofn))
				SetTextA(hwndDlg, PM_FILENAME, filename);
			return TRUE;
		}

		case IDC_MOVIE_REFRESH:
			RefreshPlayDialog(hwndDlg, true);
			return TRUE;

		case IDC_CHECK_READONLY:
			replayreadonly = IsDlgButtonChecked(hwndDlg, IDC_CHECK_READONLY) != 0;
			SendMessage(GetDlgItem(hwndDlg, IDC_PLAY_DESCRIPTION), EM_SETREADONLY,
			            replayreadonly ? TRUE : FALSE, 0);
			return TRUE;

		case IDC_CHECK_PAUSEFRAME:
		{
			bool wanted = IsDlgButtonChecked(hwndDlg, IDC_CHECK_PAUSEFRAME) != 0;
			EnableWindow(GetDlgItem(hwndDlg, IDC_EDIT_PAUSEFRAME), wanted);
			if (wanted)
				SetDlgItemInt(hwndDlg, IDC_EDIT_PAUSEFRAME, g_header.frames, FALSE);
			else
				SetTextA(hwndDlg, IDC_EDIT_PAUSEFRAME, "");
			return TRUE;
		}

		case PM_FILENAME:
			if (code == EN_CHANGE)
			{
				RefreshPlayDialog(hwndDlg, false);
			}
			return TRUE;

		case IDOK:
		{
			std::string filename = ResolveMoviePath(GetTextA(hwndDlg, PM_FILENAME));
			if (!IsFileReadable(filename.c_str()))
			{
				MessageBoxA(hwndDlg, "The movie file could not be read.", "Failed to load movie", MB_OK | MB_ICONERROR);
				return TRUE;
			}

			bool readOnly = IsDlgButtonChecked(hwndDlg, IDC_CHECK_READONLY) != 0;
			replayreadonly = readOnly;

			// VBA lets the description be edited unless the movie is read-only.
			// Only comment lines are rewritten; the frame data is untouched.
			std::wstring description = GetTextW(hwndDlg, IDC_PLAY_DESCRIPTION);
			if (!readOnly && description != g_originalDescription)
			{
				// Re-read the header: the file may have changed since it was
				// previewed, and the frame-data offset must be current.
				MovieInfo::Header fresh;
				if (!MovieInfo::ReadHeader(filename, fresh)
				    || !MovieInfo::WriteComments(filename, fresh, wcstombs(description)))
					MessageBoxA(hwndDlg, "The comment could not be saved. Playback will continue with the original file.",
					            "Play Movie", MB_OK | MB_ICONWARNING);
			}

			int pauseFrame = MovieInfo::ParsePauseFrame(
				GetTextA(hwndDlg, IDC_EDIT_PAUSEFRAME),
				IsDlgButtonChecked(hwndDlg, IDC_CHECK_PAUSEFRAME) != 0);
			const char *error = FCEUI_LoadMovie(filename.c_str(), readOnly, false, pauseFrame);
			if (error)
				MessageBoxA(hwndDlg, error, "Failed to load movie", MB_OK | MB_ICONERROR);
			EndDialog(hwndDlg, 0);
			return TRUE;
		}

		case IDCANCEL:
			EndDialog(hwndDlg, 0);
			return TRUE;
		}
	}
	}

	return FALSE;
}

//------------------------------------------------------------------------------
// Record movie dialog
//------------------------------------------------------------------------------

// The DSM author field is free-form. Seeding it with the Windows account name
// is only a convenience; the user can replace or clear it before recording.
static std::wstring DefaultMovieAuthor()
{
	wchar_t name[256];
	DWORD len = (DWORD)(sizeof(name) / sizeof(name[0]));
	if (GetUserNameW(name, &len) && name[0])
		return name;
	return L"";
}

static void SetStartFrom(HWND dlg, START_FROM sf)
{
	startFrom = sf;
	CheckDlgButton(dlg, IDC_START_FROM_POWER_ON, sf == START_BLANK ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(dlg, IDC_START_FROM_SRAM, sf == START_SRAM ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(dlg, IDC_START_FROM_SAVESTATE, sf == START_SAVESTATE ? BST_CHECKED : BST_UNCHECKED);
	bool useSram = sf == START_SRAM;
	EnableWindow(GetDlgItem(dlg, IDC_EDIT_SRAMFILENAME), useSram);
	EnableWindow(GetDlgItem(dlg, IDC_BUTTON_BROWSESRAM), useSram);
}

static void UpdateRecordOkButton(HWND dlg)
{
	std::string fname = ResolveMoviePath(GetTextA(dlg, IDC_EDIT_FILENAME));
	EnableWindow(GetDlgItem(dlg, IDOK), IsFileWritable(fname.c_str()));
}

static INT_PTR CALLBACK RecordDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	SYSTEMTIME systime;
	switch (uMsg)
	{
	case WM_INITDIALOG:
	{
		SetStartFrom(hwndDlg, startFrom);

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
			struct { SYSTEMTIME rtcMin, rtcMax; } mm;
			SYSTEMTIME rtcMinMax[2];
		};
		ZeroMemory(&mm.rtcMin, sizeof(SYSTEMTIME));
		ZeroMemory(&mm.rtcMax, sizeof(SYSTEMTIME));
		mm.rtcMin.wYear = 2000;
		mm.rtcMin.wMonth = 1;
		mm.rtcMin.wDay = 1;
		mm.rtcMin.wDayOfWeek = 6;
		mm.rtcMax.wYear = 2099;
		mm.rtcMax.wMonth = 12;
		mm.rtcMax.wDay = 31;
		mm.rtcMax.wDayOfWeek = 4;
		DateTime_SetRange(GetDlgItem(hwndDlg, IDC_DTP_DATE), GDTR_MIN, &rtcMinMax);
		DateTime_SetRange(GetDlgItem(hwndDlg, IDC_DTP_DATE), GDTR_MAX, &rtcMinMax);

		// Current ROM, and the emulation settings the movie will record.
		const MovieInfo::RomIdentity rom = MovieInfo::CurrentRomIdentity();
		SetTextA(hwndDlg, IDC_REC_ROMINFO, MovieInfo::FormatRomLine("Your ROM:  ",
		         rom.serial, MovieInfo::FormatHexCrc(rom.crc), "title=", rom.internalName));
		const MovieInfo::EmulationFlags flags = MovieInfo::CurrentEmulationFlags();
		CheckDlgButton(hwndDlg, IDC_REC_BIOSEXT, flags.useExtBios ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hwndDlg, IDC_REC_BIOSWI, flags.swiFromBios ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hwndDlg, IDC_REC_FIRMEXT, flags.useExtFirmware ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hwndDlg, IDC_REC_BOOTFW, flags.bootFromFirmware ? BST_CHECKED : BST_UNCHECKED);

		// Both the suggested path and the author are defaults and stay editable.
		// The path is resolved through File > Path Settings > Movies.
		SetTextA(hwndDlg, IDC_EDIT_FILENAME, DefaultMovieFilename(true));
		std::wstring defaultAuthor = DefaultMovieAuthor();
		if (!defaultAuthor.empty())
			SetTextW(hwndDlg, IDC_EDIT_AUTHOR, defaultAuthor);
		UpdateRecordOkButton(hwndDlg);
		SetFocus(GetDlgItem(hwndDlg, IDC_EDIT_FILENAME));
		return FALSE;
	}

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
		{
			std::string entered = GetTextA(hwndDlg, IDC_EDIT_FILENAME);
			if (entered.empty())
				return TRUE;
			std::string fname = ResolveMoviePath(entered);
			if (!IsFileWritable(fname.c_str()))
				return TRUE;

			std::wstring author = GetTextW(hwndDlg, IDC_EDIT_AUTHOR);
			std::wstring description = GetTextW(hwndDlg, IDC_REC_DESCRIPTION);
			std::string sramfname = GetTextA(hwndDlg, IDC_EDIT_SRAMFILENAME);
			if (startFrom != START_SRAM)
				sramfname.clear();

			struct tm t;
			DateTime_GetSystemtime(GetDlgItem(hwndDlg, IDC_DTP_DATE), &systime);
			t.tm_year = systime.wYear;
			t.tm_mon = systime.wMonth;
			t.tm_mday = systime.wDay;
			t.tm_wday = systime.wDayOfWeek;
			DateTime_GetSystemtime(GetDlgItem(hwndDlg, IDC_DTP_TIME), &systime);
			t.tm_hour = systime.wHour;
			t.tm_min = systime.wMinute;
			t.tm_sec = systime.wSecond;
			DateTime rtcstart(t.tm_year, t.tm_mon, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);

			const char *recordError = FCEUI_SaveMovie(fname.c_str(), author, startFrom, sramfname, rtcstart, description);
			if (recordError != NULL)
			{
				MessageBoxA(hwndDlg, recordError, "Failed to record movie", MB_OK | MB_ICONERROR);
				return TRUE;  // keep the dialog open so the user can fix the path
			}
			EndDialog(hwndDlg, 0);
			return TRUE;
		}

		case IDCANCEL:
			EndDialog(hwndDlg, 0);
			return TRUE;

		case IDC_BUTTON_BROWSEFILE:
		{
			OPENFILENAMEA ofn;
			char szChoice[MAX_PATH] = "";
			char initialDir[MAX_PATH] = "";
			ZeroMemory(&ofn, sizeof(ofn));
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = hwndDlg;
			ofn.lpstrFilter = "DeSmuME Movie File (*.dsm)\0*.dsm\0All files (*.*)\0*.*\0\0";
			ofn.lpstrFile = szChoice;
			SetFileDialogInitialDirFromEdit(hwndDlg, IDC_EDIT_FILENAME, ofn,
			                                szChoice, (int)sizeof(szChoice),
			                                initialDir, (int)sizeof(initialDir), MovieDirectory());
			ofn.lpstrTitle = "Record a new movie";
			ofn.lpstrDefExt = "dsm";
			ofn.nMaxFile = MAX_PATH;
			ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOREADONLYRETURN | OFN_PATHMUSTEXIST;
			if (GetSaveFileNameA(&ofn))
			{
				SetTextA(hwndDlg, IDC_EDIT_FILENAME, szChoice);
				UpdateRecordOkButton(hwndDlg);
			}
			return TRUE;
		}

		case IDC_BUTTON_BROWSESRAM:
		{
			OPENFILENAMEA ofn;
			char szChoice[MAX_PATH] = "";
			char initialDir[MAX_PATH] = "";
			ZeroMemory(&ofn, sizeof(ofn));
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = hwndDlg;
			ofn.lpstrFilter = "DeSmuME SRAM File (*.dsv)\0*.dsv\0All files (*.*)\0*.*\0\0";
			ofn.lpstrFile = szChoice;
			SetFileDialogInitialDirFromEdit(hwndDlg, IDC_EDIT_SRAMFILENAME, ofn,
			                                szChoice, (int)sizeof(szChoice),
			                                initialDir, (int)sizeof(initialDir), std::string());
			ofn.lpstrTitle = "Choose SRAM";
			ofn.lpstrDefExt = "dsv";
			ofn.nMaxFile = MAX_PATH;
			ofn.Flags = OFN_HIDEREADONLY | OFN_FILEMUSTEXIST;
			if (GetOpenFileNameA(&ofn))
				SetTextA(hwndDlg, IDC_EDIT_SRAMFILENAME, szChoice);
			return TRUE;
		}

		case IDC_EDIT_FILENAME:
			if (HIWORD(wParam) == EN_CHANGE)
			{
				UpdateRecordOkButton(hwndDlg);
			}
			return TRUE;

		case IDC_START_FROM_POWER_ON:
			SetStartFrom(hwndDlg, START_BLANK);
			return TRUE;

		case IDC_START_FROM_SRAM:
			SetStartFrom(hwndDlg, START_SRAM);
			return TRUE;

		case IDC_START_FROM_SAVESTATE:
			SetStartFrom(hwndDlg, START_SAVESTATE);
			return TRUE;
		}
	}

	return FALSE;
}

// Show the play movie dialog and play a movie.
void Replay_LoadMovie()
{
	DialogBoxParamW(hAppInst, MAKEINTRESOURCEW(IDD_REPLAYINP), MainWindow->getHWnd(), ReplayDialogProc, 0);
}

// Show the record movie dialog and record a movie.
void MovieRecordTo()
{
	DialogBoxParamW(hAppInst, MAKEINTRESOURCEW(IDD_RECORDMOVIE), MainWindow->getHWnd(), RecordDialogProc, 0);
}
