#ifndef _SHARED_DLL__SHARED_H_
#define _SHARED_DLL__SHARED_H_

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

#ifndef NOTHROW
#ifdef _MSC_VER
#define NOTHROW __declspec(nothrow)
#else
#define NOTHROW
#endif
#endif

#define SHARED_API /*NOTHROW*/ __stdcall

#ifndef SHARED_EXPORTS
#define SHARED_EXPORT __declspec(dllimport) SHARED_API
#else
#define SHARED_EXPORT __declspec(dllexport) SHARED_API
#endif

extern "C" {

//SHARED_EXPORT BOOL IsUnicode();
#ifdef UNICODE
#define IsUnicode() 1
#else
#define IsUnicode() 0
#endif

LRESULT SHARED_EXPORT uSendMessageText(HWND wnd,UINT msg,WPARAM wp,const char * text);
LRESULT SHARED_EXPORT uSendDlgItemMessageText(HWND wnd,UINT id,UINT msg,WPARAM wp,const char * text);
BOOL SHARED_EXPORT uGetWindowText(HWND wnd,pfc::string_base & out);
BOOL SHARED_EXPORT uSetWindowText(HWND wnd,const char * p_text);
BOOL SHARED_EXPORT uSetWindowTextEx(HWND wnd,const char * p_text,unsigned p_text_length);
BOOL SHARED_EXPORT uGetDlgItemText(HWND wnd,UINT id,pfc::string_base & out);
BOOL SHARED_EXPORT uSetDlgItemText(HWND wnd,UINT id,const char * p_text);
BOOL SHARED_EXPORT uSetDlgItemTextEx(HWND wnd,UINT id,const char * p_text,unsigned p_text_length);
BOOL SHARED_EXPORT uBrowseForFolder(HWND parent,const char * title,pfc::string_base & out);
BOOL SHARED_EXPORT uBrowseForFolderWithFile(HWND parent,const char * title,pfc::string_base & out,const char * p_file_to_find);
int SHARED_EXPORT uMessageBox(HWND wnd,const char * text,const char * caption,UINT type);
void SHARED_EXPORT uOutputDebugString(const char * msg);
BOOL SHARED_EXPORT uAppendMenu(HMENU menu,UINT flags,UINT_PTR id,const char * content);
BOOL SHARED_EXPORT uInsertMenu(HMENU menu,UINT position,UINT flags,UINT_PTR id,const char * content);
int SHARED_EXPORT uStringCompare(const char * elem1, const char * elem2);
int SHARED_EXPORT uCharCompare(t_uint32 p_char1,t_uint32 p_char2);
int SHARED_EXPORT uStringCompare_ConvertNumbers(const char * elem1,const char * elem2);
HINSTANCE SHARED_EXPORT uLoadLibrary(const char * name);
HANDLE SHARED_EXPORT uCreateEvent(LPSECURITY_ATTRIBUTES lpEventAttributes,BOOL bManualReset,BOOL bInitialState, const char * lpName);
DWORD SHARED_EXPORT uGetModuleFileName(HMODULE hMod,pfc::string_base & out);
BOOL SHARED_EXPORT uSetClipboardString(const char * ptr);
BOOL SHARED_EXPORT uGetClipboardString(pfc::string_base & out);
BOOL SHARED_EXPORT uSetClipboardRawData(UINT format,const void * ptr,t_size size);//does not empty the clipboard
BOOL SHARED_EXPORT uGetClassName(HWND wnd,pfc::string_base & out);
t_size SHARED_EXPORT uCharLength(const char * src);
BOOL SHARED_EXPORT uDragQueryFile(HDROP hDrop,UINT idx,pfc::string_base & out);
UINT SHARED_EXPORT uDragQueryFileCount(HDROP hDrop);
BOOL SHARED_EXPORT uGetTextExtentPoint32(HDC dc,const char * text,UINT cb,LPSIZE size);//note, cb is number of bytes, not actual unicode characters in the string (read: plain strlen() will do)
BOOL SHARED_EXPORT uExtTextOut(HDC dc,int x,int y,UINT flags,const RECT * rect,const char * text,UINT cb,const int * lpdx);
BOOL SHARED_EXPORT uTextOutColors(HDC dc,const char * src,UINT len,int x,int y,const RECT * clip,BOOL is_selected,DWORD default_color);
BOOL SHARED_EXPORT uTextOutColorsTabbed(HDC dc,const char * src,UINT src_len,const RECT * item,int border,const RECT * clip,BOOL selected,DWORD default_color,BOOL use_columns);
UINT SHARED_EXPORT uGetTextHeight(HDC dc);
UINT SHARED_EXPORT uGetFontHeight(HFONT font);
BOOL SHARED_EXPORT uChooseColor(DWORD * p_color,HWND parent,DWORD * p_custom_colors);
HCURSOR SHARED_EXPORT uLoadCursor(HINSTANCE hIns,const char * name);
HICON SHARED_EXPORT uLoadIcon(HINSTANCE hIns,const char * name);
HMENU SHARED_EXPORT uLoadMenu(HINSTANCE hIns,const char * name);
BOOL SHARED_EXPORT uGetEnvironmentVariable(const char * name,pfc::string_base & out);
HMODULE SHARED_EXPORT uGetModuleHandle(const char * name);
UINT SHARED_EXPORT uRegisterWindowMessage(const char * name);
BOOL SHARED_EXPORT uMoveFile(const char * src,const char * dst);
BOOL SHARED_EXPORT uDeleteFile(const char * fn);
DWORD SHARED_EXPORT uGetFileAttributes(const char * fn);
BOOL SHARED_EXPORT uFileExists(const char * fn);
BOOL SHARED_EXPORT uRemoveDirectory(const char * fn);
HANDLE SHARED_EXPORT uCreateFile(const char * p_path,DWORD p_access,DWORD p_sharemode,LPSECURITY_ATTRIBUTES p_security_attributes,DWORD p_createmode,DWORD p_flags,HANDLE p_template);
HANDLE SHARED_EXPORT uCreateFileMapping(HANDLE hFile,LPSECURITY_ATTRIBUTES lpFileMappingAttributes,DWORD flProtect,DWORD dwMaximumSizeHigh,DWORD dwMaximumSizeLow,const char * lpName);
BOOL SHARED_EXPORT uCreateDirectory(const char * fn,LPSECURITY_ATTRIBUTES blah);
HANDLE SHARED_EXPORT uCreateMutex(LPSECURITY_ATTRIBUTES blah,BOOL bInitialOwner,const char * name);
BOOL SHARED_EXPORT uGetLongPathName(const char * name,pfc::string_base & out);//may just fail to work on old windows versions, present on win98/win2k+
BOOL SHARED_EXPORT uGetFullPathName(const char * name,pfc::string_base & out);
BOOL SHARED_EXPORT uSearchPath(const char * path, const char * filename, const char * extension, pfc::string_base & p_out);
BOOL SHARED_EXPORT uFixPathCaps(const char * path,pfc::string_base & p_out);
void SHARED_EXPORT uGetCommandLine(pfc::string_base & out);
BOOL SHARED_EXPORT uGetTempPath(pfc::string_base & out);
BOOL SHARED_EXPORT uGetTempFileName(const char * path_name,const char * prefix,UINT unique,pfc::string_base & out);
BOOL SHARED_EXPORT uGetOpenFileName(HWND parent,const char * p_ext_mask,unsigned def_ext_mask,const char * p_def_ext,const char * p_title,const char * p_directory,pfc::string_base & p_filename,BOOL b_save);
//note: uGetOpenFileName extension mask uses | as separator, not null
HANDLE SHARED_EXPORT uLoadImage(HINSTANCE hIns,const char * name,UINT type,int x,int y,UINT flags);
UINT SHARED_EXPORT uRegisterClipboardFormat(const char * name);
BOOL SHARED_EXPORT uGetClipboardFormatName(UINT format,pfc::string_base & out);
BOOL SHARED_EXPORT uFormatSystemErrorMessage(pfc::string_base & p_out,DWORD p_code);

HANDLE SHARED_EXPORT uSortStringCreate(const char * src);
int SHARED_EXPORT uSortStringCompare(HANDLE string1,HANDLE string2);
int SHARED_EXPORT uSortStringCompareEx(HANDLE string1,HANDLE string2,DWORD flags);//flags - see win32 CompareString
int SHARED_EXPORT uSortPathCompare(HANDLE string1,HANDLE string2);
void SHARED_EXPORT uSortStringFree(HANDLE string);


int SHARED_EXPORT uCompareString(DWORD flags,const char * str1,unsigned len1,const char * str2,unsigned len2);

class NOVTABLE uGetOpenFileNameMultiResult : public pfc::list_base_const_t<const char*>
{
public:
	inline t_size GetCount() {return get_count();}
	inline const char * GetFileName(t_size index) {return get_item(index);}
	virtual ~uGetOpenFileNameMultiResult() {}
};

typedef uGetOpenFileNameMultiResult * puGetOpenFileNameMultiResult;

puGetOpenFileNameMultiResult SHARED_EXPORT uGetOpenFileNameMulti(HWND parent,const char * p_ext_mask,unsigned def_ext_mask,const char * p_def_ext,const char * p_title,const char * p_directory);

class NOVTABLE uFindFile
{
protected:
	uFindFile() {}
public:
	virtual BOOL FindNext()=0;
	virtual const char * GetFileName()=0;
	virtual t_uint64 GetFileSize()=0;
	virtual DWORD GetAttributes()=0;
	virtual FILETIME GetCreationTime()=0;
	virtual FILETIME GetLastAccessTime()=0;
	virtual FILETIME GetLastWriteTime()=0;
	virtual ~uFindFile() {};
	inline bool IsDirectory() {return (GetAttributes() & FILE_ATTRIBUTE_DIRECTORY) ? true : false;}
};

typedef uFindFile * puFindFile;

puFindFile SHARED_EXPORT uFindFirstFile(const char * path);

HINSTANCE SHARED_EXPORT uShellExecute(HWND wnd,const char * oper,const char * file,const char * params,const char * dir,int cmd);
HWND SHARED_EXPORT uCreateStatusWindow(LONG style,const char * text,HWND parent,UINT id);

BOOL SHARED_EXPORT uShellNotifyIcon(DWORD dwMessage,HWND wnd,UINT id,UINT callbackmsg,HICON icon,const char * tip);
BOOL SHARED_EXPORT uShellNotifyIconEx(DWORD dwMessage,HWND wnd,UINT id,UINT callbackmsg,HICON icon,const char * tip,const char * balloon_title,const char * balloon_msg);

HWND SHARED_EXPORT uCreateWindowEx(DWORD dwExStyle,const char * lpClassName,const char * lpWindowName,DWORD dwStyle,int x,int y,int nWidth,int nHeight,HWND hWndParent,HMENU hMenu,HINSTANCE hInstance,LPVOID lpParam);

BOOL SHARED_EXPORT uGetSystemDirectory(pfc::string_base & out); 
BOOL SHARED_EXPORT uGetWindowsDirectory(pfc::string_base & out); 
BOOL SHARED_EXPORT uSetCurrentDirectory(const char * path);
BOOL SHARED_EXPORT uGetCurrentDirectory(pfc::string_base & out);
BOOL SHARED_EXPORT uExpandEnvironmentStrings(const char * src,pfc::string_base & out);
BOOL SHARED_EXPORT uGetUserName(pfc::string_base & out);
BOOL SHARED_EXPORT uGetShortPathName(const char * src,pfc::string_base & out);

HSZ SHARED_EXPORT uDdeCreateStringHandle(DWORD ins,const char * src);
BOOL SHARED_EXPORT uDdeQueryString(DWORD ins,HSZ hsz,pfc::string_base & out);
UINT SHARED_EXPORT uDdeInitialize(LPDWORD pidInst,PFNCALLBACK pfnCallback,DWORD afCmd,DWORD ulRes);
BOOL SHARED_EXPORT uDdeAccessData_Text(HDDEDATA data,pfc::string_base & out);

HIMAGELIST SHARED_EXPORT uImageList_LoadImage(HINSTANCE hi, const char * lpbmp, int cx, int cGrow, COLORREF crMask, UINT uType, UINT uFlags);

#define uDdeFreeStringHandle DdeFreeStringHandle
#define uDdeCmpStringHandles DdeCmpStringHandles
#define uDdeKeepStringHandle DdeKeepStringHandle
#define uDdeUninitialize DdeUninitialize
#define uDdeNameService DdeNameService
#define uDdeFreeDataHandle DdeFreeDataHandle


typedef TVINSERTSTRUCTA uTVINSERTSTRUCT;

HTREEITEM SHARED_EXPORT uTreeView_InsertItem(HWND wnd,const uTVINSERTSTRUCT * param);
LPARAM SHARED_EXPORT uTreeView_GetUserData(HWND wnd,HTREEITEM item);
bool SHARED_EXPORT uTreeView_GetText(HWND wnd,HTREEITEM item,pfc::string_base & out);

#define uSetWindowsHookEx SetWindowsHookEx
#define uUnhookWindowsHookEx UnhookWindowsHookEx
#define uCallNextHookEx CallNextHookEx


/* usage:

  const char * src = "something";

  void * temp = malloc(uOSStringEstimateSize(src));
  uOSStringConvert(src,temp);
  //now temp contains OS-friendly (TCHAR) version of src
*/

typedef TCITEMA uTCITEM;
int SHARED_EXPORT uTabCtrl_InsertItem(HWND wnd,t_size idx,const uTCITEM * item);
int SHARED_EXPORT uTabCtrl_SetItem(HWND wnd,t_size idx,const uTCITEM * item);

int SHARED_EXPORT uGetKeyNameText(LONG lparam,pfc::string_base & out);

void SHARED_EXPORT uFixAmpersandChars(const char * src,pfc::string_base & out);//for systray
void SHARED_EXPORT uFixAmpersandChars_v2(const char * src,pfc::string_base & out);//for other controls

//deprecated
t_size SHARED_EXPORT uPrintCrashInfo(LPEXCEPTION_POINTERS param,const char * extrainfo,char * out);
enum {uPrintCrashInfo_max_length = 1024};

void SHARED_EXPORT uPrintCrashInfo_Init(const char * name);//called only by exe on startup
void SHARED_EXPORT uPrintCrashInfo_AddInfo(const char * p_info);//called only by exe on startup
void SHARED_EXPORT uPrintCrashInfo_SetDumpPath(const char * name);//called only by exe on startup


void SHARED_EXPORT uDumpCrashInfo(LPEXCEPTION_POINTERS param);

BOOL SHARED_EXPORT uListBox_GetText(HWND listbox,UINT index,pfc::string_base & out);

void SHARED_EXPORT uPrintfV(pfc::string_base & out,const char * fmt,va_list arglist);
static inline void uPrintf(pfc::string_base & out,const char * fmt,...) {va_list list;va_start(list,fmt);uPrintfV(out,fmt,list);va_end(list);}


class NOVTABLE uResource
{
public:
	virtual const void * GetPointer() = 0;
	virtual unsigned GetSize() = 0;
	virtual ~uResource() {}
};

typedef uResource* puResource;

puResource SHARED_EXPORT uLoadResource(HMODULE hMod,const char * name,const char * type,WORD wLang = MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL) );
puResource SHARED_EXPORT LoadResourceEx(HMODULE hMod,const TCHAR * name,const TCHAR * type,WORD wLang = MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL) );
HRSRC SHARED_EXPORT uFindResource(HMODULE hMod,const char * name,const char * type,WORD wLang = MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL) );

BOOL SHARED_EXPORT uLoadString(HINSTANCE ins,UINT id,pfc::string_base & out);

UINT SHARED_EXPORT uCharLower(UINT c);
UINT SHARED_EXPORT uCharUpper(UINT c);

BOOL SHARED_EXPORT uGetMenuString(HMENU menu,UINT id,pfc::string_base & out,UINT flag);
BOOL SHARED_EXPORT uModifyMenu(HMENU menu,UINT id,UINT flags,UINT newitem,const char * data);
UINT SHARED_EXPORT uGetMenuItemType(HMENU menu,UINT position);


}//extern "C"

inline char * uCharNext(char * src) {return src+uCharLength(src);}
inline const char * uCharNext(const char * src) {return src+uCharLength(src);}


class string_utf8_from_window
{
public:
	string_utf8_from_window(HWND wnd)
	{
		uGetWindowText(wnd,m_data);
	}
	string_utf8_from_window(HWND wnd,UINT id)
	{
		uGetDlgItemText(wnd,id,m_data);
	}
	inline operator const char * () const {return m_data.get_ptr();}
	inline t_size length() const {return m_data.length();}
	inline bool is_empty() const {return length() == 0;}
	inline const char * get_ptr() const {return m_data.get_ptr();}
private:
	pfc::string8 m_data;
};

static pfc::string uGetWindowText(HWND wnd) {
	pfc::string8 temp;
	if (!uGetWindowText(wnd,temp)) return "";
	return temp.toString();
}
static pfc::string uGetDlgItemText(HWND wnd,UINT id) {
	pfc::string8 temp;
	if (!uGetDlgItemText(wnd,id,temp)) return "";
	return temp.toString();
}

#define uMAKEINTRESOURCE(x) ((const char*)LOWORD(x))

#ifdef _DEBUG
class critical_section {
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
private:
	critical_section(const critical_section&) {throw pfc::exception_not_implemented();}
	const critical_section & operator=(const critical_section &) {throw pfc::exception_not_implemented();}
};
#else
class critical_section {
private:
	CRITICAL_SECTION sec;
public:
	void enter() throw() {EnterCriticalSection(&sec);}
	void leave() throw() {LeaveCriticalSection(&sec);}
	critical_section() {InitializeCriticalSection(&sec);}
	~critical_section() {DeleteCriticalSection(&sec);}
private:
	critical_section(const critical_section&) {throw pfc::exception_not_implemented();}
	const critical_section & operator=(const critical_section &) {throw pfc::exception_not_implemented();}
};
#endif
class c_insync
{
private:
	critical_section & m_section;
public:
	c_insync(critical_section * p_section) throw() : m_section(*p_section) {m_section.enter();}
	c_insync(critical_section & p_section) throw() : m_section(p_section) {m_section.enter();}
	~c_insync() throw() {m_section.leave();}
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

#define uIsDialogMessage IsDialogMessage
#define uGetMessage GetMessage
#define uPeekMessage PeekMessage
#define uDispatchMessage DispatchMessage

#define uCallWindowProc CallWindowProc
#define uDefWindowProc DefWindowProc
#define uGetWindowLong GetWindowLong
#define uSetWindowLong SetWindowLong

#define uEndDialog EndDialog
#define uDestroyWindow DestroyWindow
#define uGetDlgItem GetDlgItem
#define uEnableWindow EnableWindow
#define uGetDlgItemInt GetDlgItemInt
#define uSetDlgItemInt SetDlgItemInt

#define _uHookWindowProc(WND,PROC) ((WNDPROC)SetWindowLongPtr(WND,GWLP_WNDPROC,(LONG_PTR)(PROC)))
static WNDPROC uHookWindowProc(HWND p_wnd,WNDPROC p_proc) {return _uHookWindowProc(p_wnd,p_proc);}

#define uSendMessage SendMessage
#define uSendDlgItemMessage SendDlgItemMessage
#define uSendMessageTimeout SendMessageTimeout
#define uSendNotifyMessage SendNotifyMessage
#define uSendMessageCallback SendMessageCallback
#define uPostMessage PostMessage
#define uPostThreadMessage PostThreadMessage


class uStringPrintf
{
public:
	inline explicit uStringPrintf(const char * fmt,...)
	{
		va_list list;
		va_start(list,fmt);
		uPrintfV(m_data,fmt,list);
		va_end(list);
	}
	inline operator const char * () const {return m_data.get_ptr();}
	inline t_size length() const {return m_data.length();}
	inline bool is_empty() const {return length() == 0;}
	inline const char * get_ptr() const {return m_data.get_ptr();}
	const char * toString() const {return get_ptr();}
private:
	pfc::string8_fastalloc m_data;
};
#pragma deprecated(uStringPrintf, uPrintf, uPrintfV)

inline LRESULT uButton_SetCheck(HWND wnd,UINT id,bool state) {return uSendDlgItemMessage(wnd,id,BM_SETCHECK,state ? BST_CHECKED : BST_UNCHECKED,0); }
inline bool uButton_GetCheck(HWND wnd,UINT id) {return uSendDlgItemMessage(wnd,id,BM_GETCHECK,0,0) == BST_CHECKED;}

class uCallStackTracker
{
	t_size param;
public:
	explicit SHARED_EXPORT uCallStackTracker(const char * name);
	SHARED_EXPORT ~uCallStackTracker();
};

extern "C"
{
	LPCSTR SHARED_EXPORT uGetCallStackPath();
}

namespace pfc {
	class formatBugCheck : public string_formatter {
	public:
		formatBugCheck(const char * msg) {
			*this << msg;
			const char * path = uGetCallStackPath();
			if (*path) {
				*this << " (at: " << path << ")";
			}
		}
	};
	class exception_bug_check_v2 : public exception_bug_check {
	public:
		exception_bug_check_v2(const char * msg = exception_bug_check::g_what()) : exception_bug_check(formatBugCheck(msg)) {
			PFC_ASSERT(!"exception_bug_check_v2 triggered");
		}
	};
}

#if 1
#define TRACK_CALL(X) uCallStackTracker TRACKER__##X(#X)
#define TRACK_CALL_TEXT(X) uCallStackTracker TRACKER__BLAH(X)
#define TRACK_CODE(description,code) {uCallStackTracker __call_tracker(description); code;}
#else
#define TRACK_CALL(X)
#define TRACK_CALL_TEXT(X)
#define TRACK_CODE(description,code) {code;}
#endif

extern "C" {
int SHARED_EXPORT stricmp_utf8(const char * p1,const char * p2) throw();
int SHARED_EXPORT stricmp_utf8_ex(const char * p1,t_size len1,const char * p2,t_size len2) throw();
int SHARED_EXPORT stricmp_utf8_stringtoblock(const char * p1,const char * p2,t_size p2_bytes) throw();
int SHARED_EXPORT stricmp_utf8_partial(const char * p1,const char * p2,t_size num = ~0) throw();
int SHARED_EXPORT stricmp_utf8_max(const char * p1,const char * p2,t_size p1_bytes) throw();
t_size SHARED_EXPORT uReplaceStringAdd(pfc::string_base & out,const char * src,t_size src_len,const char * s1,t_size len1,const char * s2,t_size len2,bool casesens);
t_size SHARED_EXPORT uReplaceCharAdd(pfc::string_base & out,const char * src,t_size src_len,unsigned c1,unsigned c2,bool casesens);
//all lengths in uReplaceString functions are optional, set to -1 if parameters is a simple null-terminated string
void SHARED_EXPORT uAddStringLower(pfc::string_base & out,const char * src,t_size len = ~0);
void SHARED_EXPORT uAddStringUpper(pfc::string_base & out,const char * src,t_size len = ~0);
}

class comparator_stricmp_utf8 {
public:
	static int compare(const char * p_string1,const char * p_string2) throw() {return stricmp_utf8(p_string1,p_string2);}
};

inline void uStringLower(pfc::string_base & out,const char * src,t_size len = ~0) {out.reset();uAddStringLower(out,src,len);}
inline void uStringUpper(pfc::string_base & out,const char * src,t_size len = ~0) {out.reset();uAddStringUpper(out,src,len);}

inline t_size uReplaceString(pfc::string_base & out,const char * src,t_size src_len,const char * s1,t_size len1,const char * s2,t_size len2,bool casesens)
{
	out.reset();
	return uReplaceStringAdd(out,src,src_len,s1,len1,s2,len2,casesens);
}

inline t_size uReplaceChar(pfc::string_base & out,const char * src,t_size src_len,unsigned c1,unsigned c2,bool casesens)
{
	out.reset();
	return uReplaceCharAdd(out,src,src_len,c1,c2,casesens);
}

class string_lower
{
public:
	explicit string_lower(const char * ptr,t_size p_count = ~0) {uAddStringLower(m_data,ptr,p_count);}
	inline operator const char * () const {return m_data.get_ptr();}
	inline t_size length() const {return m_data.length();}
	inline bool is_empty() const {return length() == 0;}
	inline const char * get_ptr() const {return m_data.get_ptr();}
private:
	pfc::string8 m_data;
};

class string_upper
{
public:
	explicit string_upper(const char * ptr,t_size p_count = ~0) {uAddStringUpper(m_data,ptr,p_count);}
	inline operator const char * () const {return m_data.get_ptr();}
	inline t_size length() const {return m_data.length();}
	inline bool is_empty() const {return length() == 0;}
	inline const char * get_ptr() const {return m_data.get_ptr();}
private:
	pfc::string8 m_data;
};


inline BOOL uGetLongPathNameEx(const char * name,pfc::string_base & out)
{
	if (uGetLongPathName(name,out)) return TRUE;
	return uGetFullPathName(name,out);
}

struct t_font_description
{
	enum
	{
		m_facename_length = LF_FACESIZE*2,
		m_height_dpi = 480,
	};

	t_uint32 m_height;
	t_uint32 m_weight;
	t_uint8 m_italic;
	t_uint8 m_charset;
	char m_facename[m_facename_length];

	HFONT SHARED_EXPORT create() const;
	bool SHARED_EXPORT popup_dialog(HWND p_parent);
	void SHARED_EXPORT from_font(HFONT p_font);
	static t_font_description SHARED_EXPORT g_from_font(HFONT p_font);

	template<typename t_stream,typename t_abort> void to_stream(t_stream p_stream,t_abort & p_abort) const;
	template<typename t_stream,typename t_abort> void from_stream(t_stream p_stream,t_abort & p_abort);
};

template<typename t_stream,typename t_abort> void t_font_description::to_stream(t_stream p_stream,t_abort & p_abort) const {
	p_stream->write_lendian_t(m_height,p_abort);
	p_stream->write_lendian_t(m_weight,p_abort);
	p_stream->write_lendian_t(m_italic,p_abort);
	p_stream->write_lendian_t(m_charset,p_abort);
	p_stream->write_string(m_facename,tabsize(m_facename),p_abort);
}

template<typename t_stream,typename t_abort> void t_font_description::from_stream(t_stream p_stream,t_abort & p_abort) {
	p_stream->read_lendian_t(m_height,p_abort);
	p_stream->read_lendian_t(m_weight,p_abort);
	p_stream->read_lendian_t(m_italic,p_abort);
	p_stream->read_lendian_t(m_charset,p_abort);
	pfc::string8 temp;
	p_stream->read_string(temp,p_abort);
	strncpy_s(m_facename,temp,tabsize(m_facename));
}


struct t_modal_dialog_entry
{
	HWND m_wnd_to_poke;
	bool m_in_use;
};

extern "C" {
	void SHARED_EXPORT ModalDialog_Switch(t_modal_dialog_entry & p_entry);
	void SHARED_EXPORT ModalDialog_PokeExisting();
	bool SHARED_EXPORT ModalDialog_CanCreateNew();

	HWND SHARED_EXPORT FindOwningPopup(HWND p_wnd);
	void SHARED_EXPORT PokeWindow(HWND p_wnd);
};

//! The purpose of modal_dialog_scope is to help to avoid the modal dialog recursion problem. Current toplevel modal dialog handle is stored globally, so when creation of a new modal dialog is blocked, it can be activated to indicate the reason for the task being blocked.
class modal_dialog_scope {
public:
	//! This constructor initializes the modal dialog scope with specified dialog handle.
	inline modal_dialog_scope(HWND p_wnd) throw() : m_initialized(false) {initialize(p_wnd);}
	//! This constructor leaves the scope uninitialized (you can call initialize() later with your window handle).
	inline modal_dialog_scope() throw() : m_initialized(false) {}
	inline ~modal_dialog_scope() throw() {deinitialize();}

	//! Returns whether creation of a new modal dialog is allowed (false when there's another one active).\n
	//! NOTE: when calling context is already inside a modal dialog that you own, you should not be checking this before creating a new modal dialog.
	inline static bool can_create() throw(){return ModalDialog_CanCreateNew();}
	//! Activates the top-level modal dialog existing, if one exists.
	inline static void poke_existing() throw() {ModalDialog_PokeExisting();}

	//! Initializes the scope with specified window handle.
	void initialize(HWND p_wnd) throw()
	{
		if (!m_initialized)
		{
			m_initialized = true;
			m_entry.m_in_use = true;
			m_entry.m_wnd_to_poke = p_wnd;
			ModalDialog_Switch(m_entry);
		}
	}

	void deinitialize() throw()
	{
		if (m_initialized)
		{
			ModalDialog_Switch(m_entry);
			m_initialized = false;
		}
	}

	

private:
	modal_dialog_scope(const modal_dialog_scope & p_scope) {assert(0);}
	const modal_dialog_scope & operator=(const modal_dialog_scope &) {assert(0); return *this;}

	t_modal_dialog_entry m_entry;

	bool m_initialized;
};

class format_win32_error {
public:
	format_win32_error(DWORD p_code) {
		if (p_code == 0) m_buffer = "Undefined error";
		else if (!uFormatSystemErrorMessage(m_buffer,p_code)) m_buffer << "Unknown error code (" << (unsigned)p_code << ")";
	}

	const char * get_ptr() const {return m_buffer.get_ptr();}
	operator const char*() const {return m_buffer.get_ptr();}
private:
	pfc::string8 m_buffer;
};

struct exception_win32 : public std::exception {
	exception_win32(DWORD p_code) : std::exception(format_win32_error(p_code)), m_code(p_code) {}
	DWORD get_code() const {return m_code;}
private:
	DWORD m_code;
};

class uDebugLog : public pfc::string_formatter {
public:
	~uDebugLog() {*this << "\n"; uOutputDebugString(get_ptr());}
};

static void uAddWindowStyle(HWND p_wnd,LONG p_style) {
	SetWindowLong(p_wnd,GWL_STYLE, GetWindowLong(p_wnd,GWL_STYLE) | p_style);
}

static void uRemoveWindowStyle(HWND p_wnd,LONG p_style) {
	SetWindowLong(p_wnd,GWL_STYLE, GetWindowLong(p_wnd,GWL_STYLE) & ~p_style);
}

static void uAddWindowExStyle(HWND p_wnd,LONG p_style) {
	SetWindowLong(p_wnd,GWL_EXSTYLE, GetWindowLong(p_wnd,GWL_EXSTYLE) | p_style);
}

static void uRemoveWindowExStyle(HWND p_wnd,LONG p_style) {
	SetWindowLong(p_wnd,GWL_EXSTYLE, GetWindowLong(p_wnd,GWL_EXSTYLE) & ~p_style);
}

static unsigned MapDialogWidth(HWND p_dialog,unsigned p_value) {
	RECT temp;
	temp.left = 0; temp.right = p_value; temp.top = temp.bottom = 0;
	if (!MapDialogRect(p_dialog,&temp)) return 0;
	return temp.right;
}

static bool IsKeyPressed(unsigned vk) {
	return (GetKeyState(vk) & 0x8000) ? true : false;
}

//! Returns current modifier keys pressed, using win32 MOD_* flags.
static unsigned GetHotkeyModifierFlags() {
	unsigned ret = 0;
	if (IsKeyPressed(VK_CONTROL)) ret |= MOD_CONTROL;
	if (IsKeyPressed(VK_SHIFT)) ret |= MOD_SHIFT;
	if (IsKeyPressed(VK_MENU)) ret |= MOD_ALT;
	if (IsKeyPressed(VK_LWIN) || IsKeyPressed(VK_RWIN)) ret |= MOD_WIN;
	return ret;
}

class CClipboardOpenScope {
public:
	CClipboardOpenScope() : m_open(false) {}
	~CClipboardOpenScope() {Close();}
	bool Open(HWND p_owner) {
		Close();
		if (OpenClipboard(p_owner)) {
			m_open = true;
			return true;
		} else {
			return false;
		}
	}
	void Close() {
		if (m_open) {
			m_open = false;
			CloseClipboard();
		}
	}
private:
	bool m_open;
	
	PFC_CLASS_NOT_COPYABLE_EX(CClipboardOpenScope)
};

class CGlobalLockScope {
public:
	CGlobalLockScope(HGLOBAL p_handle) : m_handle(p_handle), m_ptr(GlobalLock(p_handle)) {
		if (m_ptr == NULL) throw std::bad_alloc();
	}
	~CGlobalLockScope() {
		if (m_ptr != NULL) GlobalUnlock(m_handle);
	}
	void * GetPtr() const {return m_ptr;}
	t_size GetSize() const {return GlobalSize(m_handle);}
private:
	void * m_ptr;
	HGLOBAL m_handle;

	PFC_CLASS_NOT_COPYABLE_EX(CGlobalLockScope)
};

template<typename TItem> class CGlobalLockScopeT {
public:
	CGlobalLockScopeT(HGLOBAL handle) : m_scope(handle) {}
	TItem * GetPtr() const {return reinterpret_cast<TItem*>(m_scope.GetPtr());}
	t_size GetSize() const {
		const t_size val = m_scope.GetSize();
		PFC_ASSERT( val % sizeof(TItem) == 0 );
		return val / sizeof(TItem);
	}
private:
	CGlobalLockScope m_scope;
};


static bool IsPointInsideControl(const POINT& pt, HWND wnd) {
	HWND walk = WindowFromPoint(pt);
	for(;;) {
		if (walk == NULL) return false;
		if (walk == wnd) return true;
		if (GetWindowLong(walk,GWL_STYLE) & WS_POPUP) return false;
		walk = GetParent(walk);
	}
}

static bool IsWindowChildOf(HWND child, HWND parent) {
	HWND walk = child;
	while(walk != parent && walk != NULL && (GetWindowLong(walk,GWL_STYLE) & WS_CHILD) != 0) {
		walk = GetParent(walk);
	}
	return walk == parent;
}


#include "audio_math.h"
#include "win32_misc.h"

template<typename TPtr>
class CoTaskMemObject {
public:
	CoTaskMemObject() : m_ptr() {}

	~CoTaskMemObject() {CoTaskMemFree(m_ptr);}

	TPtr m_ptr;
	PFC_CLASS_NOT_COPYABLE(CoTaskMemObject, CoTaskMemObject<TPtr> );
};
#endif //_SHARED_DLL__SHARED_H_
