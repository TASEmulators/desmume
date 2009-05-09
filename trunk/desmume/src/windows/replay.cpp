#include <io.h>
#include "resource.h"
#include "replay.h"
#include "common.h"
#include "main.h"
#include "movie.h"
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


char playfilename[MAX_PATH] = "";

//Replay movie dialog
BOOL CALLBACK ReplayDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	OPENFILENAME ofn;
	char szChoice[MAX_PATH]={0};
	char filename[MAX_PATH] = "";

	switch(uMsg)
	{
	case WM_INITDIALOG:
		SendDlgItemMessage(hwndDlg, IDC_CHECK_READONLY, BM_SETCHECK, replayreadonly?BST_CHECKED:BST_UNCHECKED, 0);			
		return FALSE;

	case WM_COMMAND:	
		int wID = LOWORD(wParam);
		switch(wID)
		{
			case ID_BROWSE:

				ZeroMemory(&ofn, sizeof(ofn));
				ofn.lStructSize = sizeof(ofn);
				ofn.hwndOwner = MainWindow->getHWnd();
				ofn.lpstrFilter = "Desmume Movie File (*.dsm)\0*.dsm\0All files(*.*)\0*.*\0\0";
				ofn.nFilterIndex = 1;
				ofn.lpstrFile =  filename;
				ofn.lpstrTitle = "Replay Movie from File";
				ofn.nMaxFile = MAX_PATH;
				ofn.lpstrDefExt = "dsm";
				GetOpenFileName(&ofn);
				strcpy(playfilename, filename);
				SetDlgItemText(hwndDlg, PM_FILENAME, playfilename);
				return true;
		
			case IDC_CHECK_READONLY:
				replayreadonly ^= 1;
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
		}
	}

	return false;
}

//Record movie dialog
static BOOL CALLBACK RecordDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static struct CreateMovieParameters* p = NULL;
	std::wstring author = L"";
	switch(uMsg)
	{
	case WM_INITDIALOG:

		return false;
		case WM_COMMAND:
			switch(LOWORD(wParam))
			{
			case IDOK: {
				author = GetDlgItemTextW<500>(hwndDlg,IDC_EDIT_AUTHOR);
				std::string fname = GetDlgItemText<MAX_PATH>(hwndDlg,IDC_EDIT_FILENAME);
				FCEUI_SaveMovie(fname.c_str(), author);
				EndDialog(hwndDlg, 0);
				return true;
				}

			case IDCANCEL:
				EndDialog(hwndDlg, 0);
				return true;
	
			case IDC_BUTTON_BROWSEFILE:
			{
				OPENFILENAME ofn;
				char szChoice[MAX_PATH]={0};
				char recordfilename[MAX_PATH];

				// browse button
				ZeroMemory(&ofn, sizeof(ofn));
				ofn.lStructSize = sizeof(ofn);
				ofn.hwndOwner = MainWindow->getHWnd();
				ofn.lpstrFilter = "Desmume Movie File (*.dsm)\0*.dsm\0All files(*.*)\0*.*\0\0";
				ofn.lpstrFile = szChoice;
				ofn.lpstrTitle = "Record a new movie";
				ofn.lpstrDefExt = "dsm";
				ofn.nMaxFile = MAX_PATH;
				ofn.Flags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
				GetSaveFileName(&ofn);
				strcpy(recordfilename, szChoice);
				SetDlgItemText(hwndDlg, IDC_EDIT_FILENAME, recordfilename);
				//if(GetSaveFileName(&ofn))
				//	UpdateRecordDialogPath(hwndDlg,szChoice);

				return true;
			}

		}

	}
	
	return false;
}


//Show the play movie dialog and play a movie
void Replay_LoadMovie()
{
	char* fn = (char*)DialogBoxParam(hAppInst, "IDD_REPLAYINP", MainWindow->getHWnd(), ReplayDialogProc, false);

	if(fn)
	{
		FCEUI_LoadMovie(fn, movie_readonly, false, 100000);

		free(fn);
	}
}

//Show the record movie dialog and record a movie.
void MovieRecordTo()
{
	DialogBoxParam(hAppInst, MAKEINTRESOURCE(IDD_RECORDMOVIE), MainWindow->getHWnd(), RecordDialogProc, (LPARAM)0);
}
