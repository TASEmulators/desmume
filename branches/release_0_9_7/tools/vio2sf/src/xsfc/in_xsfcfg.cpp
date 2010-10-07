#define WIN32_LEAN_AND_MEAN
#include "leakchk.h"

#include <windows.h>
#include <windowsx.h>
#include <strsafe.h>
#include <commctrl.h>

/* Winamp SDK headers */
#include <winamp/wa_ipc.h>

#include "../pversion.h"
#include "in_xsfcfg.h"
#include "xsfui.rh"

#if _MSC_VER >= 1200
#pragma warning(disable:4290)
#endif

extern unsigned long dwInterpolation;

namespace
{

xsfc::TAutoPtr<IConfigIO> pcfg;
prefsDlgRec prefwork;
int prefreg = 0;
HMODULE hDLL = 0;

}

class IniConfig : public IConfigIO
{
	xsfc::TString sIniPath;
protected:
	IniConfig() throw()
	{
	}

	bool Initialize(HWND hwndWinamp) throw(xsfc::EShortOfMemory)
	{
		char *ini = (char *)(::SendMessageA(hwndWinamp, WM_WA_IPC, 0, IPC_GETINIFILE));
		if (ini && int(ini) != 1 && !IsBadStringPtrA(ini, MAX_PATH))
			sIniPath = xsfc::TString(ini);
		else
			sIniPath = xsfc::TWin32::ExtractPath(xsfc::TWin32::ModulePath(NULL)) + "winamp.ini";
		return true;
	}

public:
	~IniConfig() throw()
	{
	}

	void SetULong(const wchar_t *name, const unsigned long value) throw()
	{
		try
		{
			xsfc::TWin32::SetPrivateProfile(sIniPath, xsfc::TString(XSFDRIVER_SIMPLENAME), name, xsfc::TString(value));
		}
		catch (xsfc::EShortOfMemory e)
		{
		}
	}
	unsigned long GetULong(const wchar_t *name, const unsigned long defaultvalue = 0) throw()
	{
		try
		{
			xsfc::TString value = xsfc::TWin32::GetPrivateProfile(sIniPath, xsfc::TString(XSFDRIVER_SIMPLENAME), name, xsfc::TString(defaultvalue));
			if (value[0]) return value.GetULong();
		}
		catch (xsfc::EShortOfMemory e)
		{
		}
		return defaultvalue;
	}
	void SetFloat(const wchar_t *name, const double value) throw()
	{
		try
		{
			xsfc::TWin32::SetPrivateProfile(sIniPath, xsfc::TString(XSFDRIVER_SIMPLENAME), name, xsfc::TString(value));
		}
		catch (xsfc::EShortOfMemory e)
		{
		}
	}
	double GetFloat(const wchar_t *name, const double defaultvalue = 0) throw()
	{
		try
		{
			xsfc::TString value = xsfc::TWin32::GetPrivateProfile(sIniPath, xsfc::TString(XSFDRIVER_SIMPLENAME), name, xsfc::TString(defaultvalue));
			if (value[0]) return value.GetFloat();
		}
		catch (xsfc::EShortOfMemory e)
		{
		}
		return defaultvalue;
	}
	void SetString(const wchar_t *name, const wchar_t *value) throw()
	{
		try
		{
			xsfc::TWin32::SetPrivateProfile(sIniPath, xsfc::TString(XSFDRIVER_SIMPLENAME), name, value);
		}
		catch (xsfc::EShortOfMemory e)
		{
		}
	}
	xsfc::TString GetString(const wchar_t *name, const wchar_t *defaultvalue = 0) throw(xsfc::EShortOfMemory)
	{
		return xsfc::TWin32::GetPrivateProfile(sIniPath, xsfc::TString(XSFDRIVER_SIMPLENAME), name, defaultvalue);
	}

	static LPIConfigIO Create(HWND hwndWinamp) throw(xsfc::EShortOfMemory)
	{
		xsfc::TAutoPtr<IniConfig> cfg(new(xsfc::nothrow) IniConfig);
		return (cfg && cfg->Initialize(hwndWinamp)) ? cfg.Detach() : 0;
	}
};

static BOOL CALLBACK DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_COMMAND:
		switch (GET_WM_COMMAND_ID(wParam, lParam))
		{
		case IDC_COMBO_INTERPOLATION:
			if (GET_WM_COMMAND_CMD(wParam, lParam) == CBN_SELCHANGE)
			{
				dwInterpolation = ComboBox_GetCurSel(GetDlgItem(hwndDlg, IDC_COMBO_INTERPOLATION));
			}
			 break;
		case IDOK:
			CFGUpdate(pcfg, hwndDlg);
			CFGSave(pcfg);
			::EndDialog(hwndDlg, IDOK);
			break;
		case IDCANCEL:
			::EndDialog(hwndDlg, IDCANCEL);
			break;
		case 0x206:
			if (GET_WM_COMMAND_CMD(wParam, lParam) == LBN_SELCHANGE)
			{
				CFGMuteChange(hwndDlg, 0x206);
			}
			break;
		default:
			return FALSE;
		}
		break;
	case WM_INITDIALOG:
		::SetWindowTextA(hwndDlg, WINAMPPLUGIN_NAME);
		::ShowWindow(GetDlgItem(hwndDlg, IDOK), SW_SHOWNA);
		::ShowWindow(GetDlgItem(hwndDlg, IDCANCEL), SW_SHOWNA);
		CFGLoad(pcfg);
		CFGReset(pcfg, hwndDlg);
		return TRUE;
	}
	return FALSE;
}


int msgBox(TCHAR* stringFormat, ...)
{
	va_list valist;
	va_start( valist, stringFormat);
	TCHAR msgPassed[2000];
	StringCbVPrintf(msgPassed, sizeof(msgPassed), stringFormat, valist);
	return MessageBox(NULL, msgPassed, "msgBox", MB_OK | MB_SYSTEMMODAL);
}
BOOL vsSetDlgItemText(HWND hDlg, int nIDDlgItem, TCHAR* stringFormat, ...)
{
	va_list valist;
	va_start( valist, stringFormat);
	TCHAR msgPassed[2000];
	StringCbVPrintf(msgPassed, sizeof(msgPassed), stringFormat, valist);
	return SetDlgItemText(hDlg, nIDDlgItem, msgPassed);
}
unsigned long GetDlgItemHex(HWND wnd, UINT wndMsg)
{
	TCHAR read[20]; unsigned long res;
	GetDlgItemText(wnd, wndMsg, read, sizeof(read)/sizeof(TCHAR));
	size_t len; StringCbLength(read, sizeof(read), &len);
	if (len>=1) sscanf_s(read, "%X", &res); else res=0;
	return res;
}

HWND dlgInstrumentSelection=0;
HWND dlgMixerInstrument[50];

class Instrument
{
public:
	unsigned long addr;
	int type;
	BOOL selectedStatus;
	BOOL muteStatus;
	unsigned long volume;
	TCHAR name[100];
};
Instrument instrument[50];
int nbInstruments=0;

TCHAR* getInstrumentName(unsigned long addr) 
{
	for (int i=0; i<nbInstruments; i++) 
		if (instrument[i].addr == addr) return instrument[i].name;
	return "[Instrument address not in the list]";
} 
void setInstrumentName(unsigned long addr, TCHAR* stringFormat, ...)
{ 
	va_list valist;
	va_start( valist, stringFormat);
	for (int i=0; i<nbInstruments; i++) 
		if (instrument[i].addr == addr) StringCbVPrintf(instrument[i].name, sizeof(instrument[i].name),  stringFormat, valist);
}

int semaphore = 0;
typedef void (*TOpenSoundView)(void* hinst);
struct CallbackSet
{
	TOpenSoundView doOpenSoundView;
	TOpenSoundView killSoundView;
} soundViewCallbacks = {NULL,NULL};
void openSoundView(void* callback)
{
	soundViewCallbacks = *(CallbackSet*)callback;
}
void killSoundView()
{
	soundViewCallbacks.killSoundView(NULL);
	soundViewCallbacks.doOpenSoundView = NULL;
	soundViewCallbacks.killSoundView = NULL;
}

void addInstrument(unsigned long addr, int type)
{ 
	if (nbInstruments >= 50) return;

	while (semaphore != 0) Sleep(0); /* thread safety (exchangeInstruments) */
	
	for (int i=0; i<nbInstruments; i++) 
		if (instrument[i].addr == addr) return;
	instrument[nbInstruments].addr = addr; 
	instrument[nbInstruments].type = type;
	instrument[nbInstruments].selectedStatus = FALSE;
	instrument[nbInstruments].muteStatus = FALSE;
	instrument[nbInstruments].volume = 100;
	StringCbPrintf(instrument[nbInstruments].name, sizeof(instrument[nbInstruments].name), "Instrument %i", nbInstruments+1);
	nbInstruments++;
	
	if (dlgInstrumentSelection) 
		PostMessage(dlgInstrumentSelection, WM_APP, 0, 0);
}
int getNbInstruments() { return nbInstruments; }
unsigned long getInstrumentAddr(int i) 
{ 
	if (i<nbInstruments) return instrument[i].addr; else return 0; 
} 
int getInstrumentIndex(unsigned long addr) 
{ 
	for (int i=0; i<nbInstruments; i++) 
		if (instrument[i].addr == addr) return i; 
	return -1; 
}
int getInstrumentType(unsigned long addr) 
{ 
	for (int i=0; i<nbInstruments; i++) 
		if (instrument[i].addr == addr) return instrument[i].type; 
	return -1; 
}
BOOL isInstrumentSelected(unsigned long addr) 
{ 
	for (int i=0; i<nbInstruments; i++) 
		if (instrument[i].addr == addr) return instrument[i].selectedStatus; 
	return FALSE; 
}
BOOL isInstrumentMuted(unsigned long addr) 
{ 
	for (int i=0; i<nbInstruments; i++) 
		if (instrument[i].addr == addr) 
			return instrument[i].muteStatus; 
	return FALSE; 
}
unsigned long getInstrumentVolume(unsigned long addr) 
{ 
	for (int i=0; i<nbInstruments; i++) 
		if (instrument[i].addr == addr) 
			return instrument[i].volume; 
	return 100; 
}
void clearInstrumentList() { nbInstruments = 0; } 
void setInstrumentSelectedStatus(unsigned long addr, BOOL selected)
{ 
	for (int i=0; i<nbInstruments; i++) 
		if (instrument[i].addr == addr) instrument[i].selectedStatus = selected;
}
void setInstrumentMuteStatus(unsigned long addr, BOOL muted) 
{ 
	for (int i=0; i<nbInstruments; i++) 
		if (instrument[i].addr == addr) instrument[i].muteStatus = muted;
}
void setInstrumentVolume(unsigned long addr, unsigned long volume) 
{ 
	for (int i=0; i<nbInstruments; i++) 
		if (instrument[i].addr == addr) instrument[i].volume = volume;
}
void exchangeInstruments(unsigned long addr1, unsigned long addr2)
{
	int a,b;
	for (a=0; a<nbInstruments; a++) if (instrument[a].addr == addr1) break;
	for (b=0; b<nbInstruments; b++) if (instrument[b].addr == addr2) break;
	if (a == nbInstruments || b == nbInstruments || a==b) 
	{ 
		MessageBeep(MB_ICONEXCLAMATION); 
		return; 
	}

	semaphore++;

	Instrument tmp;
	CopyMemory(&tmp, &instrument[a], sizeof(Instrument));
	CopyMemory(&instrument[a], &instrument[b], sizeof(Instrument));
	CopyMemory(&instrument[b], &tmp, sizeof(Instrument));

	semaphore--;
}


void updateInstrumentsSelectedStatus(HWND dlg) 
{
	unsigned long trackBarVolume = SendDlgItemMessage(dlg, IDC_SLIDER_VOLUME, TBM_GETPOS , TRUE, 0);
	for (int i=0; i<SendDlgItemMessage(dlg, IDC_LISTINSTRUMENTS, LB_GETCOUNT , 0, 0); i++)
	{
		unsigned long addr = SendDlgItemMessage(dlg, IDC_LISTINSTRUMENTS, LB_GETITEMDATA, i, 0);
		setInstrumentSelectedStatus(addr, !(SendDlgItemMessage(dlg, IDC_LISTINSTRUMENTS, LB_GETSEL, i, 0) == 0));
		setInstrumentVolume(addr, (SendDlgItemMessage(dlg, IDC_LISTINSTRUMENTS, LB_GETSEL, i, 0) == 0 ? 100 : trackBarVolume));
	}
}
void updateInstrumentListbox(HWND dlg)
{
	SendDlgItemMessage(dlg, IDC_LISTINSTRUMENTS, LB_RESETCONTENT, 0, 0);
	/*SendDlgItemMessage(dlg, IDC_LISTVIEWINSTRUMENTS, LVM_DELETEALLITEMS, 0, 0);*/
	for (int i=0; i<getNbInstruments(); i++)
	{
		/*LVITEM lvi; lvi.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM | LVIF_STATE; lvi.state = 0; lvi.stateMask = 0;
		lvi.iItem = lvi.iImage = i; lvi.iSubItem =0; lvi.pszText = getInstrumentName(getInstrumentAddr(i)); lvi.lParam = getInstrumentAddr(i);
		SendDlgItemMessage(dlg, IDC_LISTVIEWINSTRUMENTS, LVM_INSERTITEM, 0, (LPARAM)&lvi);*/
		SendDlgItemMessage(dlg, IDC_LISTINSTRUMENTS, LB_ADDSTRING, 0, (LPARAM)getInstrumentName(getInstrumentAddr(i)));
		SendDlgItemMessage(dlg, IDC_LISTINSTRUMENTS, LB_SETITEMDATA , i, getInstrumentAddr(i));
		SendDlgItemMessage(dlg, IDC_LISTINSTRUMENTS, LB_SELITEMRANGE, isInstrumentSelected(getInstrumentAddr(i)), MAKELPARAM(i, i));
	}
	vsSetDlgItemText(dlg, IDC_STATICNBINSTRUMENTSDISPLAYED, "%i instruments found", getNbInstruments());
}
int searchInstrument(HWND dlg, unsigned long addr)
{
	for (int i=0; i<SendDlgItemMessage(dlg, IDC_LISTINSTRUMENTS, LB_GETCOUNT , 0, 0); i++)
		if (addr == SendDlgItemMessage(dlg, IDC_LISTINSTRUMENTS, LB_GETITEMDATA, i, 0)) return i;
	return -1;
}

BOOL CALLBACK dlgProcEnterInstrumentName(HWND dlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	static unsigned long doubleClickedInstrumentAddr = 0;
	switch (message)
	{
	case WM_INITDIALOG:
		{
		doubleClickedInstrumentAddr = lParam;
		SetDlgItemText(dlg, IDC_EDIT_ENTERINSTRUMENTNAME, getInstrumentName(doubleClickedInstrumentAddr));
		int type = getInstrumentType(doubleClickedInstrumentAddr);
		int index = getInstrumentIndex(doubleClickedInstrumentAddr);
		vsSetDlgItemText(dlg, IDC_EDIT_ENTERINSTRUMENTNAMEADDRESS, "Address of instrument (%s): %X", (type == 0? "8 bit data" : (type == 1? "16 bit data" : (type == 2? "ADPCM" : "PSG"))), doubleClickedInstrumentAddr);
		SendDlgItemMessage(dlg, IDC_EDIT_ENTERINSTRUMENTNAME, EM_SETSEL, 0, -1);
		SetFocus(GetDlgItem(dlg, IDC_EDIT_ENTERINSTRUMENTNAME));
		return FALSE ;
		}

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			TCHAR name[sizeof(instrument[0].name)];
			GetDlgItemText(dlg, IDC_EDIT_ENTERINSTRUMENTNAME, name, sizeof(name));
			setInstrumentName(doubleClickedInstrumentAddr, name);
			EndDialog(dlg, IDOK);
			break;
		case IDCANCEL:
			EndDialog(dlg, IDCANCEL);
			break;

			default: return FALSE;
		}
		break;
	}
	return FALSE;
}
BOOL CALLBACK dlgProcMixerInstrument(HWND dlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{	
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{				
		case IDC_STATIC_INSTRUMENTNAME:
			if (HIWORD(wParam)==STN_DBLCLK) 
			{
				unsigned long addr;
				int i;
				for (i=0; i<getNbInstruments(); i++) 
					if (dlg == dlgMixerInstrument[i]) break;
				addr = getInstrumentAddr(i);
				DialogBoxParam(hDLL, MAKEINTRESOURCE(IDD_DIALOGENTERINSTRUMENTNAME), dlg, (DLGPROC)dlgProcEnterInstrumentName, addr);
			}
			break;
		
		default: return FALSE;
		}
	}
	return FALSE;
}
BOOL CALLBACK dlgProcInstrumentSelection(HWND dlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
		{
		updateInstrumentListbox(dlg);
		SendDlgItemMessage(dlg, IDC_SLIDER_VOLUME, TBM_SETRANGE , TRUE, MAKELONG(0,100));
		SendDlgItemMessage(dlg, IDC_SLIDER_VOLUME, TBM_SETTICFREQ , 10, 0);
		SendDlgItemMessage(dlg, IDC_SLIDER_VOLUME, TBM_SETPOS , TRUE, 20);

		/*
		
		//SendDlgItemMessage(dlg, IDC_TREE_INSTRUMENTNAMES, TVM_SETBKCOLOR, 0, RGB(240,240,240));
		//RECT treeViewScreenRect; GetWindowRect(GetDlgItem(dlg, IDC_TREE_INSTRUMENTNAMES), &treeViewScreenRect); 
		//POINT treeViewClientPosTopLeft = {treeViewScreenRect.left, treeViewScreenRect.top}; 
		//POINT treeViewClientPosBottomRight = {treeViewScreenRect.right, treeViewScreenRect.bottom}; 
		//ScreenToClient(dlg, &treeViewClientPosTopLeft);
		//ScreenToClient(dlg, &treeViewClientPosBottomRight);

		int positionShift=0;
		for (int i=0; i<getNbInstruments();i++)
		{
			unsigned long addr = getInstrumentAddr(i);
			dlgMixerInstrument[i] = CreateDialogParam(hDLL, MAKEINTRESOURCE(IDD_DIALOG_MIXERINSTRUMENT), dlg, (DLGPROC)dlgProcMixerInstrument, (LPARAM)addr);
			SetWindowPos(dlgMixerInstrument[i], HWND_TOP, 550, positionShift, 0,0,SWP_SHOWWINDOW | SWP_NOSIZE);
			RECT rectMixerInstrument; GetClientRect(dlgMixerInstrument[i], &rectMixerInstrument);
			positionShift += rectMixerInstrument.bottom;
			//SetWindowLong(GetDlgItem(dlgMixerInstrument[i], IDC_SLIDER_INSTRUMENTVOLUME), GWL_STYLE, TBS_DOWNISLEFT);
			SendDlgItemMessage(dlgMixerInstrument[i], IDC_SLIDER_INSTRUMENTVOLUME, TBM_SETRANGE , TRUE, MAKELONG(0,130));
			SendDlgItemMessage(dlgMixerInstrument[i], IDC_SLIDER_INSTRUMENTVOLUME, TBM_SETTICFREQ , 10, 0);
			SendDlgItemMessage(dlgMixerInstrument[i], IDC_SLIDER_INSTRUMENTVOLUME, TBM_SETPOS , TRUE, getInstrumentVolume(addr));
			if (!isInstrumentMuted(addr))	CheckDlgButton(dlgMixerInstrument[i], IDC_CHECK_INSTRUMENTMUTE, BST_UNCHECKED);
			else							CheckDlgButton(dlgMixerInstrument[i], IDC_CHECK_INSTRUMENTMUTE, BST_CHECKED);
			SetDlgItemText(dlgMixerInstrument[i], IDC_STATIC_INSTRUMENTNAME, getInstrumentName(addr));
			//TVINSERTSTRUCT tvinsert; tvinsert.hParent = TVI_ROOT; tvinsert.hInsertAfter = TVI_LAST;
			//tvinsert.item.mask = TVIF_TEXT | TVIF_PARAM; tvinsert.item.pszText = getInstrumentName(addr); tvinsert.item.lParam = addr;
			//SendDlgItemMessage(dlg, IDC_TREE_INSTRUMENTNAMES, TVM_INSERTITEM, 0, (LPARAM)&tvinsert);
		}*/
		
		return TRUE;
		}

	case WM_HSCROLL:
		if (lParam == (LPARAM)GetDlgItem(dlg, IDC_SLIDER_VOLUME)) updateInstrumentsSelectedStatus(dlg);
		break;

	case WM_APP:
		updateInstrumentListbox(dlg);
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{		
		case IDC_BUTTONCLEARLIST:
			if (HIWORD(wParam)==BN_CLICKED)
			{
				clearInstrumentList();
				SendDlgItemMessage(dlg, IDC_LISTINSTRUMENTS, LB_RESETCONTENT, 0, 0);
			}
			break;

		case IDC_BUTTONSELECTALL:
			if (HIWORD(wParam)==BN_CLICKED)
			{
				SendDlgItemMessage(dlg, IDC_LISTINSTRUMENTS, LB_SELITEMRANGE, 1, SendDlgItemMessage(dlg, IDC_LISTINSTRUMENTS, LB_GETCOUNT , 0, 0));
				updateInstrumentsSelectedStatus(dlg);
			}
			break;
		case IDC_BUTTONSELECTNONE:
			if (HIWORD(wParam)==BN_CLICKED)
			{	
				SendDlgItemMessage(dlg, IDC_LISTINSTRUMENTS, LB_SELITEMRANGE, 0, SendDlgItemMessage(dlg, IDC_LISTINSTRUMENTS, LB_GETCOUNT , 0, 0));
				updateInstrumentsSelectedStatus(dlg);
			}
			break;

		case IDC_LISTINSTRUMENTS:
			if (HIWORD(wParam)==LBN_SELCHANGE) updateInstrumentsSelectedStatus(dlg); 
			if (HIWORD(wParam)==LBN_DBLCLK)
			{
				int doubleClickedInstrumentIndex = SendDlgItemMessage(dlg, IDC_LISTINSTRUMENTS, LB_GETCARETINDEX, 0, 0);
				SendDlgItemMessage(dlg, IDC_LISTINSTRUMENTS, LB_SETSEL, FALSE, doubleClickedInstrumentIndex);
				updateInstrumentsSelectedStatus(dlg);
				unsigned long doubleClickedInstrumentAddr = SendDlgItemMessage(dlg, IDC_LISTINSTRUMENTS, LB_GETITEMDATA, doubleClickedInstrumentIndex, 0);
				DialogBoxParam(hDLL, MAKEINTRESOURCE(IDD_DIALOGENTERINSTRUMENTNAME), dlg, (DLGPROC)dlgProcEnterInstrumentName, (LPARAM)doubleClickedInstrumentAddr);
				updateInstrumentListbox(dlg);
				SendDlgItemMessage(dlg, IDC_LISTINSTRUMENTS, LB_SETCARETINDEX, doubleClickedInstrumentIndex, TRUE);
			}
			break;

		case IDC_BUTTONMOVEINSTRUMENTUP:
			{
			int currentInstrumentIndex = SendDlgItemMessage(dlg, IDC_LISTINSTRUMENTS, LB_GETCARETINDEX, 0, 0);
			if (currentInstrumentIndex==0) break;
			unsigned long addr1 = SendDlgItemMessage(dlg, IDC_LISTINSTRUMENTS, LB_GETITEMDATA, currentInstrumentIndex, 0);
			unsigned long addr2 = SendDlgItemMessage(dlg, IDC_LISTINSTRUMENTS, LB_GETITEMDATA, currentInstrumentIndex-1, 0);
			exchangeInstruments(addr1, addr2);
			updateInstrumentListbox(dlg);
			SendDlgItemMessage(dlg, IDC_LISTINSTRUMENTS, LB_SETCARETINDEX, currentInstrumentIndex-1, TRUE);
			}
			break;
		case IDC_BUTTONMOVEINSTRUMENTDOWN:
			{
			int currentInstrumentIndex = SendDlgItemMessage(dlg, IDC_LISTINSTRUMENTS, LB_GETCARETINDEX, 0, 0);
			if (currentInstrumentIndex >= SendDlgItemMessage(dlg, IDC_LISTINSTRUMENTS, LB_GETCOUNT , 0, 0)-1) break;
			unsigned long addr1 = SendDlgItemMessage(dlg, IDC_LISTINSTRUMENTS, LB_GETITEMDATA, currentInstrumentIndex, 0);
			unsigned long addr2 = SendDlgItemMessage(dlg, IDC_LISTINSTRUMENTS, LB_GETITEMDATA, currentInstrumentIndex+1, 0);
			exchangeInstruments(addr1, addr2);
			updateInstrumentListbox(dlg);
			SendDlgItemMessage(dlg, IDC_LISTINSTRUMENTS, LB_SETCARETINDEX, currentInstrumentIndex+1, TRUE);
			}
			break;

		case IDOK:
			{
			updateInstrumentListbox(dlg);
			int index = searchInstrument(dlg, GetDlgItemHex(dlg, IDC_EDIT_ADDRESSSEARCH));
			if (index != -1) 
			{
				SendDlgItemMessage(dlg, IDC_LISTINSTRUMENTS, LB_SETCARETINDEX, index, TRUE);
				SendMessage(dlg, WM_COMMAND, MAKEWPARAM(IDC_LISTINSTRUMENTS, LBN_DBLCLK), (LPARAM)GetDlgItem(dlg, IDC_LISTINSTRUMENTS));
			}
			else MessageBeep(MB_ICONEXCLAMATION);
			}
			break;
		
		default: return FALSE;
		}
	break;

	case WM_CLOSE: DestroyWindow(dlg); 
		return TRUE;
	case WM_DESTROY: dlgInstrumentSelection = 0; 
		return TRUE;
	}
	return FALSE;
}
static BOOL CALLBACK DialogProcPref(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static bool initialized = false;
	switch (uMsg)
	{
	case WM_COMMAND:
		switch (GET_WM_COMMAND_ID(wParam, lParam))
		{
		case IDC_BUTTON_VIEWSPU:
				if(soundViewCallbacks.doOpenSoundView) soundViewCallbacks.doOpenSoundView(hDLL);
			break;
			
		case IDC_BUTTON_INSTRUMENTSELECTION:
			if (HIWORD(wParam)==BN_CLICKED)
			{
				if (dlgInstrumentSelection == 0)
				{
					dlgInstrumentSelection=CreateDialog(hDLL, MAKEINTRESOURCE(IDD_INSTRUMENTDLG), hwndDlg, (DLGPROC)dlgProcInstrumentSelection);
					if (dlgInstrumentSelection==0) MessageBeep(MB_ICONEXCLAMATION);
					ShowWindow(dlgInstrumentSelection, SW_SHOW);
				}
				else
				{
					HWND tmp=dlgInstrumentSelection; /* thread safety (PostMessage in addInstrument) */
					dlgInstrumentSelection=0;
					DestroyWindow(tmp);
				}
			}
			break;
		case IDC_COMBO_INTERPOLATION:
			if (GET_WM_COMMAND_CMD(wParam, lParam) == CBN_SELCHANGE)
			{
				dwInterpolation = ComboBox_GetCurSel(GetDlgItem(hwndDlg, IDC_COMBO_INTERPOLATION));
				CFGSave(pcfg);
			}
			 break;

		case 0x200: case 0x201: case 0x202:
		case 0x203: case 0x204: case 0x205:
			if (initialized)
			{
				CFGUpdate(pcfg, hwndDlg);
				CFGSave(pcfg);
			}
			break;
		case 0x206:
			if (GET_WM_COMMAND_CMD(wParam, lParam) == LBN_SELCHANGE)
			{
				CFGMuteChange(hwndDlg, 0x206);
			}
			break;
		default:
			return FALSE;
		}
		break;
	case WM_INITDIALOG:
		initialized = false;
		CFGLoad(pcfg);
		CFGReset(pcfg, hwndDlg);
		initialized = true;
		return TRUE;
	}
	return FALSE;
}

void winamp_config_init(HINSTANCE hinstDLL)
{
	hDLL = hinstDLL;
}
void winamp_config_load(HWND hwndWinamp)
{
	if (!pcfg)
	{
		pcfg = IniConfig::Create(hwndWinamp);
		if (!pcfg) pcfg = NullConfig::Create();
		CFGLoad(pcfg);
	}
}


void winamp_config_add_prefs(HWND hwndWinamp)
{
	if (prefreg) return;

	if (((LONG)xsfc::TWin32::WndMsgSend(hwndWinamp, WM_WA_IPC, 0, IPC_GETVERSION) & 0xffffU) >= 0x2090)
	{
		/* 2.9+ */
		prefwork.hInst = hDLL;
		prefwork.dlgID = 1;
		prefwork.proc = (void *)DialogProcPref;
		prefwork.name = XSFDRIVER_SIMPLENAME;
		prefwork.where = 0;
		prefwork._id = 0;
		prefwork.next = 0;
		prefreg = !(LONG)xsfc::TWin32::WndMsgSend(hwndWinamp, WM_WA_IPC, &prefwork, IPC_ADD_PREFS_DLG);
	}
}

void winamp_config_remove_prefs(HWND hwndWinamp)
{

	if (pcfg) pcfg.Release();
	if (prefreg)
	{
		xsfc::TWin32::WndMsgSend(hwndWinamp, WM_WA_IPC, &prefwork, IPC_REMOVE_PREFS_DLG);
		prefreg = 0;
	}
}

void winamp_config_dialog(HWND hwndWinamp, HWND hwndParent)
{
	HRSRC hrsrc;
	DWORD dwSize;
	HGLOBAL hGlobal;
	xsfc::TSimpleArray<BYTE> resbuf;
	if (prefreg)
	{
		if (((LONG)xsfc::TWin32::WndMsgSend(hwndWinamp, WM_WA_IPC, 0, IPC_GETVERSION) & 0xffffU) >= 0x5000)
		{
			xsfc::TWin32::WndMsgSend(hwndWinamp, WM_WA_IPC, &prefwork, IPC_OPENPREFSTOPAGE);
			return; 
		}
	}

	hrsrc = ::FindResourceA(hDLL, MAKEINTRESOURCE(1), RT_DIALOG);
	if (!hrsrc) return;

	dwSize = ::SizeofResource(hDLL, hrsrc);
	hGlobal = ::LoadResource(hDLL, hrsrc);

	if (dwSize < 4 || !hGlobal) return;
	
	if (!resbuf.Resize(dwSize)) return;
	
	LPBYTE lpResource = resbuf.Ptr();

	::CopyMemory(lpResource, hGlobal, dwSize);

	lpResource[0] = '\xc0';
	lpResource[1] = '\x08';
	lpResource[2] = '\xc8';
	lpResource[3] = '\x80';

	xsfc::TWin32::DlgInvoke(::GetModuleHandleA(NULL), lpResource, NULL, DialogProc, NULL);
}