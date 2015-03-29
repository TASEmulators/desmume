#ifndef _UTF8API_DLL__UTF8API_H_
#define _UTF8API_DLL__UTF8API_H_

#include "../../pfc/pfc.h"

#ifndef WIN32
#error N/A
#endif

#ifndef STRICT
#define STRICT
#endif

#include <windows.h>
#include <ddeml.h>
#include <commctrl.h>

#ifndef UTF8API_EXPORTS
#define UTF8API_EXPORT _declspec(dllimport)
#else
#define UTF8API_EXPORT _declspec(dllexport)
#endif

#define UTF8API_VERSION 3

extern "C" {

UTF8API_EXPORT BOOL IsUnicode();
UTF8API_EXPORT UINT UTF8API_GetVersion();
UTF8API_EXPORT LRESULT uSendMessage(HWND wnd,UINT msg,WPARAM wp,LPARAM lp);
UTF8API_EXPORT LRESULT uSendMessageText(HWND wnd,UINT msg,WPARAM wp,const char * text);
UTF8API_EXPORT LRESULT uSendDlgItemMessage(HWND wnd,UINT id,UINT msg,WPARAM wp,LPARAM lp);
UTF8API_EXPORT LRESULT uSendDlgItemMessageText(HWND wnd,UINT id,UINT msg,WPARAM wp,const char * text);
UTF8API_EXPORT LRESULT uSendMessageTimeout(HWND wnd,UINT msg,WPARAM wp,LPARAM lp,UINT flags,UINT timeout,LPDWORD result);
UTF8API_EXPORT BOOL uSendNotifyMessage(HWND wnd,UINT msg,WPARAM wp,LPARAM lp);
UTF8API_EXPORT BOOL uSendMessageCallback(HWND wnd,UINT msg,WPARAM wp,LPARAM lp,SENDASYNCPROC cb,DWORD data);
UTF8API_EXPORT BOOL uPostMessage(HWND wnd,UINT msg,WPARAM wp,LPARAM lp);
UTF8API_EXPORT BOOL uPostThreadMessage(DWORD id,UINT msg,WPARAM wp,LPARAM lp);
UTF8API_EXPORT BOOL uGetWindowText(HWND wnd,string_base & out);
UTF8API_EXPORT BOOL uSetWindowText(HWND wnd,const char * text);
UTF8API_EXPORT BOOL uGetDlgItemText(HWND wnd,UINT id,string_base & out);
UTF8API_EXPORT BOOL uSetDlgItemText(HWND wnd,UINT id,const char * text);
UTF8API_EXPORT HWND uCreateDialog(HINSTANCE hIns,UINT id,HWND parent,DLGPROC proc,long param=0);
UTF8API_EXPORT int uDialogBox(HINSTANCE hIns,UINT id,HWND parent,DLGPROC proc,long param=0);
UTF8API_EXPORT BOOL uBrowseForFolder(HWND parent,const char * title,string_base & out);
UTF8API_EXPORT long uGetWindowLong(HWND wnd,int idx);
UTF8API_EXPORT long uSetWindowLong(HWND wnd,int idx,long val);
UTF8API_EXPORT int uMessageBox(HWND wnd,const char * text,const char * caption,UINT type);
UTF8API_EXPORT void uOutputDebugString(const char * msg);
typedef LOGFONTA uLOGFONT;
UTF8API_EXPORT HFONT uCreateFontIndirect(const uLOGFONT * p_font);
UTF8API_EXPORT void uGetDefaultFont(uLOGFONT * p_font);
UTF8API_EXPORT BOOL uChooseFont(uLOGFONT * p_font,HWND parent);
UTF8API_EXPORT LRESULT uCallWindowProc(WNDPROC lpPrevWndFunc,HWND hWnd,UINT Msg,WPARAM wParam,LPARAM lParam);
UTF8API_EXPORT BOOL uAppendMenu(HMENU menu,UINT flags,UINT id,const char * content);
UTF8API_EXPORT BOOL uInsertMenu(HMENU menu,UINT position,UINT flags,UINT id,const char * content);
UTF8API_EXPORT int uStringCompare(const char * elem1, const char * elem2);
UTF8API_EXPORT HINSTANCE uLoadLibrary(const char * name);
UTF8API_EXPORT HANDLE uCreateEvent(LPSECURITY_ATTRIBUTES lpEventAttributes,BOOL bManualReset,BOOL bInitialState, const char * lpName);
UTF8API_EXPORT DWORD uGetModuleFileName(HMODULE hMod,string_base & out);
UTF8API_EXPORT BOOL uSetClipboardString(const char * ptr);
UTF8API_EXPORT LRESULT uDefWindowProc(HWND wnd,UINT msg,WPARAM wp,LPARAM lp);
UTF8API_EXPORT BOOL uIsDialogMessage(HWND dlg,LPMSG msg);
UTF8API_EXPORT BOOL uGetMessage(LPMSG msg,HWND wnd,UINT min,UINT max);
UTF8API_EXPORT BOOL uGetClassName(HWND wnd,string_base & out);
UTF8API_EXPORT UINT uCharLength(const char * src);
UTF8API_EXPORT BOOL uDragQueryFile(HDROP hDrop,UINT idx,string_base & out);
UTF8API_EXPORT UINT uDragQueryFileCount(HDROP hDrop);
UTF8API_EXPORT BOOL uGetTextExtentPoint32(HDC dc,const char * text,UINT cb,LPSIZE size);//note, cb is number of bytes, not actual unicode characters in the string (read: plain strlen() will do)
UTF8API_EXPORT BOOL uExtTextOut(HDC dc,int x,int y,UINT flags,const RECT * rect,const char * text,UINT cb,const int * lpdx);
UTF8API_EXPORT BOOL uTextOutColors(HDC dc,const char * src,UINT len,int x,int y,const RECT * clip,BOOL is_selected,DWORD default_color);
UTF8API_EXPORT BOOL uTextOutColorsTabbed(HDC dc,const char * src,UINT src_len,const RECT * item,int border,const RECT * clip,BOOL selected,DWORD default_color,BOOL use_columns);
UTF8API_EXPORT UINT uGetTextHeight(HDC dc);
UTF8API_EXPORT UINT uGetFontHeight(HFONT font);
UTF8API_EXPORT BOOL uChooseColor(DWORD * p_color,HWND parent,DWORD * p_custom_colors);
UTF8API_EXPORT BOOL uPeekMessage(LPMSG msg,HWND wnd,UINT min,UINT max,UINT flag);
UTF8API_EXPORT LONG uDispatchMessage(const MSG * msg);
UTF8API_EXPORT HCURSOR uLoadCursor(HINSTANCE hIns,const char * name);
UTF8API_EXPORT HICON uLoadIcon(HINSTANCE hIns,const char * name);
UTF8API_EXPORT HMENU uLoadMenu(HINSTANCE hIns,const char * name);
UTF8API_EXPORT BOOL uGetEnvironmentVariable(const char * name,string_base & out);
UTF8API_EXPORT HMODULE uGetModuleHandle(const char * name);
UTF8API_EXPORT UINT uRegisterWindowMessage(const char * name);
UTF8API_EXPORT BOOL uMoveFile(const char * src,const char * dst);
UTF8API_EXPORT BOOL uDeleteFile(const char * fn);
UTF8API_EXPORT DWORD uGetFileAttributes(const char * fn);
UTF8API_EXPORT BOOL uRemoveDirectory(const char * fn);
UTF8API_EXPORT HANDLE uCreateFile(const char * fn,DWORD access,DWORD share,LPSECURITY_ATTRIBUTES blah,DWORD creat,DWORD flags,HANDLE tmpl);
UTF8API_EXPORT HANDLE uCreateFileMapping(HANDLE hFile,LPSECURITY_ATTRIBUTES lpFileMappingAttributes,DWORD flProtect,DWORD dwMaximumSizeHigh,DWORD dwMaximumSizeLow,const char * lpName);
UTF8API_EXPORT BOOL uCreateDirectory(const char * fn,LPSECURITY_ATTRIBUTES blah);
UTF8API_EXPORT HANDLE uCreateMutex(LPSECURITY_ATTRIBUTES blah,BOOL bInitialOwner,const char * name);
UTF8API_EXPORT BOOL uGetLongPathName(const char * name,string_base & out);//may just fail to work on old windows versions, present on win98/win2k+
UTF8API_EXPORT BOOL uGetFullPathName(const char * name,string_base & out);
UTF8API_EXPORT void uGetCommandLine(string_base & out);
UTF8API_EXPORT BOOL uGetTempPath(string_base & out);
UTF8API_EXPORT BOOL uGetTempFileName(const char * path_name,const char * prefix,UINT unique,string_base & out);
UTF8API_EXPORT BOOL uGetOpenFileName(HWND parent,const char * p_ext_mask,unsigned def_ext_mask,const char * p_def_ext,const char * p_title,const char * p_directory,string_base & p_filename,BOOL b_save);
//note: uGetOpenFileName extension mask uses | as separator, not null
UTF8API_EXPORT HANDLE uLoadImage(HINSTANCE hIns,const char * name,UINT type,int x,int y,UINT flags);
UTF8API_EXPORT UINT uRegisterClipboardFormat(const char * name);
UTF8API_EXPORT BOOL uGetClipboardFormatName(UINT format,string_base & out);

UTF8API_EXPORT HANDLE uSortStringCreate(const char * src);
UTF8API_EXPORT int uSortStringCompare(HANDLE string1,HANDLE string2);
UTF8API_EXPORT int uSortStringCompareEx(HANDLE string1,HANDLE string2,DWORD flags);//flags - see win32 CompareString
UTF8API_EXPORT int uSortPathCompare(HANDLE string1,HANDLE string2);
UTF8API_EXPORT void uSortStringFree(HANDLE string);


UTF8API_EXPORT int uCompareString(DWORD flags,const char * str1,unsigned len1,const char * str2,unsigned len2);

class NOVTABLE uGetOpenFileNameMultiResult
{
public:
	virtual UINT GetCount()=0;
	virtual const char * GetFileName(UINT index)=0;
	virtual ~uGetOpenFileNameMultiResult() {}
};

UTF8API_EXPORT uGetOpenFileNameMultiResult * uGetOpenFileNameMulti(HWND parent,const char * p_ext_mask,unsigned def_ext_mask,const char * p_def_ext,const char * p_title,const char * p_directory);

class NOVTABLE uFindFile
{
protected:
	uFindFile() {}
public:
	virtual BOOL FindNext()=0;
	virtual const char * GetFileName()=0;
	virtual __int64 GetFileSize()=0;
	virtual DWORD GetAttributes()=0;
	virtual FILETIME GetCreationTime()=0;
	virtual FILETIME GetLastAccessTime()=0;
	virtual FILETIME GetLastWriteTime()=0;
	virtual ~uFindFile() {};
	inline bool IsDirectory() {return (GetAttributes() & FILE_ATTRIBUTE_DIRECTORY) ? true : false;}
};

UTF8API_EXPORT uFindFile * uFindFirstFile(const char * path);

UTF8API_EXPORT HINSTANCE uShellExecute(HWND wnd,const char * oper,const char * file,const char * params,const char * dir,int cmd);
UTF8API_EXPORT HWND uCreateStatusWindow(LONG style,const char * text,HWND parent,UINT id);

typedef WNDCLASSA uWNDCLASS;
UTF8API_EXPORT ATOM uRegisterClass(const uWNDCLASS * ptr);
UTF8API_EXPORT BOOL uUnregisterClass(const char * name,HINSTANCE ins);

UTF8API_EXPORT BOOL uShellNotifyIcon(DWORD dwMessage,HWND wnd,UINT id,UINT callbackmsg,HICON icon,const char * tip);
UTF8API_EXPORT BOOL uShellNotifyIconEx(DWORD dwMessage,HWND wnd,UINT id,UINT callbackmsg,HICON icon,const char * tip,const char * balloon_title,const char * balloon_msg);

UTF8API_EXPORT HWND uCreateWindowEx(DWORD dwExStyle,const char * lpClassName,const char * lpWindowName,DWORD dwStyle,int x,int y,int nWidth,int nHeight,HWND hWndParent,HMENU hMenu,HINSTANCE hInstance,LPVOID lpParam);

UTF8API_EXPORT BOOL uGetSystemDirectory(string_base & out); 
UTF8API_EXPORT BOOL uGetWindowsDirectory(string_base & out); 
UTF8API_EXPORT BOOL uSetCurrentDirectory(const char * path);
UTF8API_EXPORT BOOL uGetCurrentDirectory(string_base & out);
UTF8API_EXPORT BOOL uExpandEnvironmentStrings(const char * src,string_base & out);
UTF8API_EXPORT BOOL uGetUserName(string_base & out);
UTF8API_EXPORT BOOL uGetShortPathName(const char * src,string_base & out);

UTF8API_EXPORT HSZ uDdeCreateStringHandle(DWORD ins,const char * src);
UTF8API_EXPORT BOOL uDdeQueryString(DWORD ins,HSZ hsz,string_base & out);
UTF8API_EXPORT UINT uDdeInitialize(LPDWORD pidInst,PFNCALLBACK pfnCallback,DWORD afCmd,DWORD ulRes);
UTF8API_EXPORT BOOL uDdeAccessData_Text(HDDEDATA data,string_base & out);

UTF8API_EXPORT HIMAGELIST uImageList_LoadImage(HINSTANCE hi, const char * lpbmp, int cx, int cGrow, COLORREF crMask, UINT uType, UINT uFlags);

#define uDdeFreeStringHandle DdeFreeStringHandle
#define uDdeCmpStringHandles DdeCmpStringHandles
#define uDdeKeepStringHandle DdeKeepStringHandle
#define uDdeUninitialize DdeUninitialize
#define uDdeNameService DdeNameService
#define uDdeFreeDataHandle DdeFreeDataHandle


typedef TVINSERTSTRUCTA uTVINSERTSTRUCT;

UTF8API_EXPORT HTREEITEM uTreeView_InsertItem(HWND wnd,const uTVINSERTSTRUCT * param);

UTF8API_EXPORT HHOOK uSetWindowsHookEx(int idhook,HOOKPROC fn,HINSTANCE hMod,DWORD threadid);
#define uUnhookWindowsHookEx UnhookWindowsHookEx
#define uCallNextHookEx CallNextHookEx


typedef SHFILEOPSTRUCTA uSHFILEOPSTRUCT;
UTF8API_EXPORT int uSHFileOperation(uSHFILEOPSTRUCT * lpFileOp);

typedef STARTUPINFOA uSTARTUPINFO;

UTF8API_EXPORT BOOL uCreateProcess(
  const char * lpApplicationName,
  const char * lpCommandLine,
  LPSECURITY_ATTRIBUTES lpProcessAttributes,
  LPSECURITY_ATTRIBUTES lpThreadAttributes,
  BOOL bInheritHandles,
  DWORD dwCreationFlags,
  const char * lpEnvironment,
  const char * lpCurrentDirectory,
  const uSTARTUPINFO * lpStartupInfo,
  LPPROCESS_INFORMATION lpProcessInformation
);

UTF8API_EXPORT unsigned uOSStringEstimateSize(const char * src,unsigned len = -1);//return value in bytes
UTF8API_EXPORT unsigned uOSStringConvert(const char * src,void * dst,unsigned len = -1);//length of src (will cut if found null earlier)
/* usage:

  const char * src = "something";

  void * temp = malloc(uOSStringEstimateSize(src));
  uOSStringConvert(src,temp);
  //now temp contains OS-friendly (TCHAR) version of src
*/

typedef TCITEMA uTCITEM;
UTF8API_EXPORT int uTabCtrl_InsertItem(HWND wnd,int idx,const uTCITEM * item);
UTF8API_EXPORT int uTabCtrl_SetItem(HWND wnd,int idx,const uTCITEM * item);

UTF8API_EXPORT int uGetKeyNameText(LONG lparam,string_base & out);

UTF8API_EXPORT void uFixAmpersandChars(const char * src,string_base & out);//for systray
UTF8API_EXPORT void uFixAmpersandChars_v2(const char * src,string_base & out);//for other controls
//gotta hate MS for shoving ampersand=>underline conversion into random things AND making each of them use different rules for inserting ampersand char instead of underline


//deprecated
UTF8API_EXPORT UINT uPrintCrashInfo(LPEXCEPTION_POINTERS param,const char * extrainfo,char * out);
#define uPrintCrashInfo_max_length 1024

UTF8API_EXPORT void uPrintCrashInfo_Init(const char * name);//called only by exe on startup

UTF8API_EXPORT void uDumpCrashInfo(LPEXCEPTION_POINTERS param);

UTF8API_EXPORT BOOL uListBox_GetText(HWND listbox,UINT index,string_base & out);

UTF8API_EXPORT void uPrintf(string_base & out,const char * fmt,...);
UTF8API_EXPORT void uPrintfV(string_base & out,const char * fmt,va_list arglist);

class NOVTABLE uResource
{
public:
	virtual const void * GetPointer() = 0;
	virtual unsigned GetSize() = 0;
	virtual ~uResource() {}
};

UTF8API_EXPORT uResource * uLoadResource(HMODULE hMod,const char * name,const char * type,WORD wLang = MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL) );
UTF8API_EXPORT HRSRC uFindResource(HMODULE hMod,const char * name,const char * type,WORD wLang = MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL) );

UTF8API_EXPORT BOOL uLoadString(HINSTANCE ins,UINT id,string_base & out);

UTF8API_EXPORT UINT uCharLower(UINT c);
UTF8API_EXPORT UINT uCharUpper(UINT c);

UTF8API_EXPORT BOOL uGetMenuString(HMENU menu,UINT id,string_base & out,UINT flag);
UTF8API_EXPORT BOOL uModifyMenu(HMENU menu,UINT id,UINT flags,UINT newitem,const char * data);
UTF8API_EXPORT UINT uGetMenuItemType(HMENU menu,UINT position);


}//extern "C"

inline char * uCharNext(char * src) {return src+uCharLength(src);}
inline const char * uCharNext(const char * src) {return src+uCharLength(src);}


class string_utf8_from_window : public string8
{
public:
	string_utf8_from_window(HWND wnd)
	{
		uGetWindowText(wnd,*this);
	}
	string_utf8_from_window(HWND wnd,UINT id)
	{
		uGetDlgItemText(wnd,id,*this);
	}
};

#define uMAKEINTRESOURCE(x) ((const char*)LOWORD(x))

class critical_section
{
private:
	CRITICAL_SECTION sec;
	int count;
public:
	int enter() {EnterCriticalSection(&sec);return ++count;}
	int leave() {int rv = --count;LeaveCriticalSection(&sec);return rv;}
	int get_lock_count() {return count;}
	int get_lock_count_check() {enter();return leave();}
	inline void assert_locked() {assert(get_lock_count_check()>0);}
	inline void assert_not_locked() {assert(get_lock_count_check()==0);}
	critical_section() {InitializeCriticalSection(&sec);count=0;}
	~critical_section() {DeleteCriticalSection(&sec);}
#ifdef CRITICAL_SECTION_HAVE_TRYENTER
	bool try_enter() {return !!TryEnterCriticalSection(&sec);}
#endif
};

class c_insync
{
private:
	critical_section * ptr;
public:
	c_insync(critical_section * p) {ptr=p;ptr->enter();}
	c_insync(critical_section & p) {ptr=&p;ptr->enter();}
	~c_insync() {ptr->leave();}
};

#define insync(X) c_insync blah____sync(X)


class critical_section2	//smarter version, has try_enter()
{
private:
	HANDLE hMutex;
	int count;
public:
	int enter() {return enter_timeout(INFINITE);}
	int leave() {int rv = --count;ReleaseMutex(hMutex);return rv;}
	int get_lock_count() {return count;}
	int get_lock_count_check()
	{
		int val = try_enter();
		if (val>0) val = leave();
		return val;
	}
	int enter_timeout(DWORD t) {return WaitForSingleObject(hMutex,t)==WAIT_OBJECT_0 ? ++count : 0;}
	int try_enter() {return enter_timeout(0);}
	int check_count() {enter();return leave();}
	critical_section2()
	{
		hMutex = uCreateMutex(0,0,0);
		count=0;
	}
	~critical_section2() {CloseHandle(hMutex);}

	inline void assert_locked() {assert(get_lock_count_check()>0);}
	inline void assert_not_locked() {assert(get_lock_count_check()==0);}

};

class c_insync2
{
private:
	critical_section2 * ptr;
public:
	c_insync2(critical_section2 * p) {ptr=p;ptr->enter();}
	c_insync2(critical_section2 & p) {ptr=&p;ptr->enter();}
	~c_insync2() {ptr->leave();}
};

#define insync2(X) c_insync2 blah____sync2(X)


//other
#define uEndDialog EndDialog
#define uDestroyWindow DestroyWindow
#define uGetDlgItem GetDlgItem
#define uEnableWindow EnableWindow
#define uGetDlgItemInt GetDlgItemInt
#define uSetDlgItemInt SetDlgItemInt
#define uHookWindowProc(WND,PROC) ((WNDPROC)uSetWindowLong(WND,GWL_WNDPROC,(long)(PROC)))
#define uCreateToolbarEx CreateToolbarEx
#define uIsBadStringPtr IsBadStringPtrA


class string_print_crash
{
	char block[uPrintCrashInfo_max_length];
public:
	inline operator const char * () const {return block;}
	inline const char * get_ptr() const {return block;}
	inline unsigned length() {return strlen(block);}
	inline string_print_crash(LPEXCEPTION_POINTERS param,const char * extrainfo = 0) {uPrintCrashInfo(param,extrainfo,block);}
};

class uStringPrintf : private string8_fastalloc
{
public:
	inline explicit uStringPrintf(const char * fmt,...)
	{
		va_list list;
		va_start(list,fmt);
		uPrintfV(*this,fmt,list);
		va_end(list);
	}
	operator const char * () {return get_ptr();}
};

inline LRESULT uButton_SetCheck(HWND wnd,UINT id,bool state) {return uSendDlgItemMessage(wnd,id,BM_SETCHECK,state ? BST_CHECKED : BST_UNCHECKED,0); }
inline bool uButton_GetCheck(HWND wnd,UINT id) {return uSendDlgItemMessage(wnd,id,BM_GETCHECK,0,0) == BST_CHECKED;}

class UTF8API_EXPORT uCallStackTracker
{
	unsigned param;
public:
	explicit uCallStackTracker(const char * name);
	~uCallStackTracker();
};

extern "C"
{
	UTF8API_EXPORT const char * uGetCallStackPath();
}

#define TRACK_CALL(X) uCallStackTracker TRACKER__##X(#X)
#define TRACK_CALL_TEXT(X) uCallStackTracker TRACKER__BLAH(X)


extern "C" {
UTF8API_EXPORT int stricmp_utf8(const char * p1,const char * p2);
UTF8API_EXPORT int stricmp_utf8_ex(const char * p1,unsigned len1,const char * p2,unsigned len2);
UTF8API_EXPORT int stricmp_utf8_stringtoblock(const char * p1,const char * p2,unsigned p2_bytes);
UTF8API_EXPORT int stricmp_utf8_partial(const char * p1,const char * p2,unsigned num = (unsigned)(-1));
UTF8API_EXPORT int stricmp_utf8_max(const char * p1,const char * p2,unsigned p1_bytes);
UTF8API_EXPORT unsigned uReplaceStringAdd(string_base & out,const char * src,unsigned src_len,const char * s1,unsigned len1,const char * s2,unsigned len2,bool casesens);
UTF8API_EXPORT unsigned uReplaceCharAdd(string_base & out,const char * src,unsigned src_len,unsigned c1,unsigned c2,bool casesens);
//all lengths in uReplaceString functions are optional, set to -1 if parameters is a simple null-terminated string
UTF8API_EXPORT void uAddStringLower(string_base & out,const char * src,unsigned len = (unsigned)(-1));
UTF8API_EXPORT void uAddStringUpper(string_base & out,const char * src,unsigned len = (unsigned)(-1));

inline void uStringLower(string_base & out,const char * src,unsigned len = (unsigned)(-1)) {out.reset();uAddStringLower(out,src,len);}
inline void uStringUpper(string_base & out,const char * src,unsigned len = (unsigned)(-1)) {out.reset();uAddStringUpper(out,src,len);}

inline unsigned uReplaceString(string_base & out,const char * src,unsigned src_len,const char * s1,unsigned len1,const char * s2,unsigned len2,bool casesens)
{
	out.reset();
	return uReplaceStringAdd(out,src,src_len,s1,len1,s2,len2,casesens);
}

inline unsigned uReplaceChar(string_base & out,const char * src,unsigned src_len,unsigned c1,unsigned c2,bool casesens)
{
	out.reset();
	return uReplaceCharAdd(out,src,src_len,c1,c2,casesens);
}

class string_lower : private string8
{
public:
	explicit string_lower(const char * ptr,unsigned num=-1) {uAddStringLower(*this,ptr,num);}
	operator const char * () {return string8::get_ptr();}
};

class string_upper : private string8
{
public:
	explicit string_upper(const char * ptr,unsigned num=-1) {uAddStringUpper(*this,ptr,num);}
	operator const char * () {return string8::get_ptr();}
};


}

#define char_lower uCharLower
#define char_upper uCharUpper

inline BOOL uGetLongPathNameEx(const char * name,string_base & out)
{
	if (uGetLongPathName(name,out)) return TRUE;
	return uGetFullPathName(name,out);
}

#endif //_UTF8API_DLL__UTF8API_H_