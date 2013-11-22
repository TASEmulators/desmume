#include "utf8api.h"


#include <shlobj.h>
#include <lmcons.h>



UTF8API_EXPORT UINT UTF8API_GetVersion()
{
	return UTF8API_VERSION;
}


class param_os_from_utf8 : public string_os_from_utf8
{
	bool is_null;
	WORD low_word;
public:
	param_os_from_utf8(const char * p) : 
		is_null(p==0), 
		low_word( HIWORD((DWORD)p)==0 ? LOWORD((DWORD)p) : 0),
		string_os_from_utf8( p && HIWORD((DWORD)p)!=0 ?p:"") 
		{}
	inline operator const TCHAR *()
	{
		return get_ptr();
	}
	const TCHAR * get_ptr()
	{
		return low_word ? (const TCHAR*)(DWORD)low_word : is_null ? 0 : string_os_from_utf8::get_ptr();
	}
	
};


static void strncpy_addnull(char * dest,const char * src,int max)
{
	int n;
	for(n=0;n<max-1 && src[n];n++)
		dest[n] = src[n];
	dest[n] = 0;
}

static void wcsncpy_addnull(WCHAR * dest,const WCHAR * src,int max)
{
	int n;
	for(n=0;n<max-1 && src[n];n++)
		dest[n] = src[n];
	dest[n] = 0;
}

#ifdef UNICODE
#define tcsncpy_addnull wcsncpy_addnull
#else
#define tcsncpy_addnull strncpy_addnull
#endif

static int _stdcall browse_proc(HWND wnd,UINT msg,LPARAM lp,LPARAM dat)
{
	if (msg==BFFM_INITIALIZED) SendMessage(wnd,BFFM_SETSELECTION,1,dat);
	return 0;
}

static int browse_for_dir(HWND parent,const TCHAR * title,TCHAR * out)
{
	IMalloc* pMalloc=0;
	
	SHGetMalloc(&pMalloc);
	if (!pMalloc) return 0;


	BROWSEINFO bi=
	{
		parent,
		0,
		0,
		title,
		BIF_RETURNONLYFSDIRS,
		browse_proc,
		(long)(const TCHAR*)out,
		0
	};
	int rv = 0;
	ITEMIDLIST* li=SHBrowseForFolder(&bi);
	if (li)
	{
		SHGetPathFromIDList(li,out);

		pMalloc->Free(li);
		rv = 1;
	}

	pMalloc->Release();
	return rv;
}

extern "C" {

UTF8API_EXPORT BOOL IsUnicode()
{
#ifdef UNICODE
	return TRUE;
#else
	return FALSE;
#endif
}

UTF8API_EXPORT LRESULT uSendMessage(HWND wnd,UINT msg,WPARAM wp,LPARAM lp)
{
	return SendMessage(wnd,msg,wp,lp);
}

UTF8API_EXPORT LRESULT uSendMessageText(HWND wnd,UINT msg,WPARAM wp,const char * text)
{//this is called somewhat often (playlist search listbox...), so lets avoid mallocing
	if (text==0) return SendMessage(wnd,msg,wp,0);
	else
	{
		unsigned temp_len = estimate_utf8_to_os(text);
		mem_block_t<TCHAR> temp_block;
		TCHAR * temp = (temp_len * sizeof(TCHAR) <= PFC_ALLOCA_LIMIT) ? (TCHAR*)alloca(temp_len * sizeof(TCHAR)) : temp_block.set_size(temp_len);
		assert(temp);
		convert_utf8_to_os(text,temp);

		return SendMessage(wnd,msg,wp,(long)(const TCHAR*)temp);
	}
	//return SendMessage(wnd,msg,wp,(long)(const TCHAR*)param_os_from_utf8(text));
}

UTF8API_EXPORT LRESULT uSendDlgItemMessage(HWND wnd,UINT id,UINT msg,WPARAM wp,LPARAM lp)
{
	return SendDlgItemMessage(wnd,id,msg,wp,lp);
}

UTF8API_EXPORT LRESULT uSendDlgItemMessageText(HWND wnd,UINT id,UINT msg,WPARAM wp,const char * text)
{
	return uSendMessageText(uGetDlgItem(wnd,id),msg,wp,text);//SendDlgItemMessage(wnd,id,msg,wp,(long)(const TCHAR*)string_os_from_utf8(text));
}

UTF8API_EXPORT BOOL uPostMessage(HWND wnd,UINT msg,WPARAM wp,LPARAM lp)
{
	return PostMessage(wnd,msg,wp,lp);
}

UTF8API_EXPORT BOOL uPostThreadMessage(DWORD id,UINT msg,WPARAM wp,LPARAM lp)
{
	return PostThreadMessage(id,msg,wp,lp);
}

UTF8API_EXPORT BOOL uGetWindowText(HWND wnd,string_base & out)
{
	int len = GetWindowTextLength(wnd);
	if (len>0)
	{
		len++;
		mem_block_t<TCHAR> temp(len);
		temp[0]=0;		
		if (GetWindowText(wnd,temp,len)>0)
		{
			temp[len-1]=0;
			out.set_string_os(temp);
			return TRUE;
		}
	}
	return FALSE;
}

UTF8API_EXPORT BOOL uSetWindowText(HWND wnd,const char * text)
{
	unsigned temp_len = estimate_utf8_to_os(text);
	mem_block_t<TCHAR> temp_block;
	TCHAR * temp = (temp_len * sizeof(TCHAR) <= PFC_ALLOCA_LIMIT) ? (TCHAR*)alloca(temp_len * sizeof(TCHAR)) : temp_block.set_size(temp_len);
	assert(temp);

	convert_utf8_to_os(text,temp);
	return SetWindowText(wnd,temp);
}


UTF8API_EXPORT BOOL uGetDlgItemText(HWND wnd,UINT id,string_base & out)
{
	return uGetWindowText(GetDlgItem(wnd,id),out);
}

UTF8API_EXPORT BOOL uSetDlgItemText(HWND wnd,UINT id,const char * text)
{
	unsigned temp_len = estimate_utf8_to_os(text);
	mem_block_t<TCHAR> temp_block;
	TCHAR * temp = (temp_len * sizeof(TCHAR) <= PFC_ALLOCA_LIMIT) ? (TCHAR*)alloca(temp_len * sizeof(TCHAR)) : temp_block.set_size(temp_len);
	assert(temp);

	convert_utf8_to_os(text,temp);
	return SetDlgItemText(wnd,id,temp);
}

UTF8API_EXPORT HWND uCreateDialog(HINSTANCE hIns,UINT id,HWND parent,DLGPROC proc,long param)
{
	return CreateDialogParam(hIns,MAKEINTRESOURCE(id),parent,proc,param);
}

UTF8API_EXPORT int uDialogBox(HINSTANCE hIns,UINT id,HWND parent,DLGPROC proc,long param)
{
	return DialogBoxParam(hIns,MAKEINTRESOURCE(id),parent,proc,param);
}

UTF8API_EXPORT BOOL uBrowseForFolder(HWND parent,const char * title,string_base & out)
{
	TCHAR temp[MAX_PATH];
	tcsncpy_addnull(temp,string_os_from_utf8(out),tabsize(temp));
	BOOL rv = browse_for_dir(parent,string_os_from_utf8(title),temp);
	if (rv)
	{
		temp[tabsize(temp)-1]=0;
		out.set_string(string_utf8_from_os(temp));
	}
	return rv;
}

UTF8API_EXPORT long uGetWindowLong(HWND wnd,int idx)
{
	return GetWindowLong(wnd,idx);
}

UTF8API_EXPORT long uSetWindowLong(HWND wnd,int idx,long val)
{
	return SetWindowLong(wnd,idx,val);
}

UTF8API_EXPORT int uMessageBox(HWND wnd,const char * text,const char * caption,UINT type)
{
	return MessageBox(wnd,param_os_from_utf8(text),param_os_from_utf8(caption),type);
}

UTF8API_EXPORT void uOutputDebugString(const char * msg) {OutputDebugString(string_os_from_utf8(msg));}

static void font_os_from_utf8(LOGFONT * dst,const uLOGFONT * src)
{
	memcpy(dst,src,sizeof(*src)-sizeof(src->lfFaceName));
	tcsncpy_addnull(dst->lfFaceName,string_os_from_utf8(src->lfFaceName),tabsize(dst->lfFaceName));
}

static void font_utf8_from_os(uLOGFONT * dst,const LOGFONT * src)
{
	memcpy(dst,src,sizeof(*src)-sizeof(src->lfFaceName));
	strncpy_addnull(dst->lfFaceName,string_utf8_from_os(src->lfFaceName),tabsize(dst->lfFaceName));
}

UTF8API_EXPORT HFONT uCreateFontIndirect(const uLOGFONT * p_font)
{
	LOGFONT font_os;
	font_os_from_utf8(&font_os,p_font);
	return CreateFontIndirect(&font_os);
}

UTF8API_EXPORT void uGetDefaultFont(uLOGFONT * p_font)
{
	LOGFONT font_os;
	GetObject(GetStockObject(DEFAULT_GUI_FONT),sizeof(font_os),&font_os);
	font_utf8_from_os(p_font,&font_os);
}

UTF8API_EXPORT BOOL uChooseFont(uLOGFONT * p_font,HWND parent)
{
	LOGFONT font_os;
	font_os_from_utf8(&font_os,p_font);

	CHOOSEFONT cf;
	memset(&cf,0,sizeof(cf));
	cf.lStructSize = sizeof(cf);
	cf.hwndOwner=parent;
	cf.lpLogFont=&font_os;
	cf.Flags=CF_SCREENFONTS|CF_FORCEFONTEXIST|CF_INITTOLOGFONTSTRUCT;
	cf.nFontType=SCREEN_FONTTYPE;				
	BOOL rv = ChooseFont(&cf);
	if (rv) font_utf8_from_os(p_font,&font_os);
	return rv;
}

UTF8API_EXPORT LRESULT uCallWindowProc(WNDPROC lpPrevWndFunc,HWND hWnd,UINT Msg,WPARAM wParam,LPARAM lParam)
{
	return CallWindowProc(lpPrevWndFunc,hWnd,Msg,wParam,lParam);
}

UTF8API_EXPORT BOOL uAppendMenu(HMENU menu,UINT flags,UINT id,const char * content)
{
	return AppendMenu(menu,flags,id,param_os_from_utf8(content));
}

UTF8API_EXPORT BOOL uInsertMenu(HMENU menu,UINT position,UINT flags,UINT id,const char * content)
{
	return InsertMenu(menu,position,flags,id,param_os_from_utf8(content));
}

UTF8API_EXPORT int uStringCompare(const char * elem1, const char * elem2)
{
#ifdef UNICODE
	WCHAR temp1[4],temp2[4];
	for(;;)
	{
		UINT c1,c2,l1,l2;
		l1 = utf8_decode_char(elem1,&c1);
		l2 = utf8_decode_char(elem2,&c2);
		if (l1==0 && l2==0) return 0;
		if (c1!=c2)
		{
			temp1[utf16_encode_char(c1,temp1)]=0;
			temp2[utf16_encode_char(c2,temp2)]=0;
			int test = lstrcmpiW(temp1,temp2);
			if (test) return test;
		}
		elem1 += l1;
		elem2 += l2;
	}
#else
	WCHAR temp1[4],temp2[4];
	char ctemp1[20],ctemp2[20];
	for(;;)
	{
		UINT c1,c2,l1,l2;
		l1 = utf8_decode_char(elem1,&c1);
		l2 = utf8_decode_char(elem2,&c2);
		if (l1==0 && l2==0) return 0;
		if (c1!=c2)
		{
			temp1[utf16_encode_char(c1,temp1)]=0;
			temp2[utf16_encode_char(c2,temp2)]=0;
			WideCharToMultiByte(CP_ACP,0,temp1,-1,ctemp1,tabsize(ctemp1),0,0);
			WideCharToMultiByte(CP_ACP,0,temp2,-1,ctemp2,tabsize(ctemp2),0,0);
			int test = lstrcmpiA(ctemp1,ctemp2);
			if (test) return test;
		}
		elem1 += l1;
		elem2 += l2;
	}
#endif
}

UTF8API_EXPORT HINSTANCE uLoadLibrary(const char * name)
{
	return LoadLibrary(param_os_from_utf8(name));
}

UTF8API_EXPORT HANDLE uCreateEvent(LPSECURITY_ATTRIBUTES lpEventAttributes,BOOL bManualReset,BOOL bInitialState, const char * lpName)
{
	return CreateEvent(lpEventAttributes,bManualReset,bInitialState, param_os_from_utf8(lpName));
}


UTF8API_EXPORT DWORD uGetModuleFileName(HMODULE hMod,string_base & out)
{
	TCHAR temp[MAX_PATH];
	temp[0] = 0;
	DWORD ret = GetModuleFileName(hMod,temp,tabsize(temp));
	if (ret)
	{
		temp[tabsize(temp)-1] = 0;
		out.set_string_os(temp);
	}
	return ret;
}

UTF8API_EXPORT BOOL uSetClipboardString(const char * ptr)
{
	if (OpenClipboard(0))
	{
		EmptyClipboard();

		string_os_from_utf8 temp(ptr);

		HANDLE h_global = GlobalAlloc(GMEM_DDESHARE, (_tcslen(temp) + 1) * sizeof(TCHAR)); 
		if (h_global!=0)
		{
			TCHAR * ptr = (TCHAR*)GlobalLock(h_global);
			_tcscpy(ptr,temp);
			GlobalUnlock(h_global);
			SetClipboardData(
#ifdef UNICODE
				CF_UNICODETEXT
#else
				CF_TEXT
#endif
				,h_global);
		}
		CloseClipboard();
		return TRUE;
	}
	else return FALSE;
}

UTF8API_EXPORT LRESULT uDefWindowProc(HWND wnd,UINT msg,WPARAM wp,LPARAM lp)
{
	return DefWindowProc(wnd,msg,wp,lp);
}

UTF8API_EXPORT BOOL uIsDialogMessage(HWND dlg,LPMSG msg)
{
	return IsDialogMessage(dlg,msg);
}

UTF8API_EXPORT BOOL uGetMessage(LPMSG msg,HWND wnd,UINT min,UINT max)
{
	return GetMessage(msg,wnd,min,max);
}

UTF8API_EXPORT BOOL uGetClassName(HWND wnd,string_base & out)
{
	TCHAR temp[512];
	temp[0]=0;
	if (GetClassName(wnd,temp,tabsize(temp))>0)
	{
		temp[tabsize(temp)-1]=0;
		out.set_string_os(temp);
		return TRUE;
	}
	else return FALSE;
}

UTF8API_EXPORT UINT uCharLength(const char * src) {return utf8_char_len(src);}

UTF8API_EXPORT BOOL uDragQueryFile(HDROP hDrop,UINT idx,string_base & out)
{
	UINT len = DragQueryFile(hDrop,idx,0,0);
	if (len>0 && len!=(UINT)(-1))
	{
		len++;
		mem_block_t<TCHAR> temp(len);
		if (DragQueryFile(hDrop,idx,temp,len)>0)
		{
			out.set_string_os(temp);
			return TRUE;
		}
	}
	return FALSE;
}

UTF8API_EXPORT UINT uDragQueryFileCount(HDROP hDrop)
{
	return DragQueryFile(hDrop,-1,0,0);
}


UTF8API_EXPORT LRESULT uSendMessageTimeout(HWND wnd,UINT msg,WPARAM wp,LPARAM lp,UINT flags,UINT timeout,LPDWORD result)
{
	return SendMessageTimeout(wnd,msg,wp,lp,flags,timeout,result);
}

UTF8API_EXPORT BOOL uGetTextExtentPoint32(HDC dc,const char * text,UINT cb,LPSIZE size)
{
	string_os_from_utf8 temp(text,cb);
	return GetTextExtentPoint32(dc,temp,_tcslen(temp),size);
}

UTF8API_EXPORT BOOL uExtTextOut(HDC dc,int x,int y,UINT flags,const RECT * rect,const char * text,UINT cb,const int * lpdx)
{
	string_os_from_utf8 temp(text,cb);
	return ExtTextOut(dc,x,y,flags,rect,temp,_tcslen(temp),lpdx);
}

UTF8API_EXPORT BOOL uChooseColor(DWORD * p_color,HWND parent,DWORD * p_custom_colors)
{
	CHOOSECOLOR cc;
	memset(&cc,0,sizeof(cc));
	cc.lStructSize = sizeof(cc);
	cc.hwndOwner = parent;
	cc.rgbResult = *p_color;
	cc.lpCustColors = p_custom_colors;
	cc.Flags = CC_ANYCOLOR|CC_FULLOPEN|CC_RGBINIT;
	BOOL rv = ChooseColor(&cc);
	if (rv)
	{
		*p_color = cc.rgbResult;
		return TRUE;
	}
	else return FALSE;
}

UTF8API_EXPORT BOOL uPeekMessage(LPMSG msg,HWND wnd,UINT min,UINT max,UINT flag)
{
	return PeekMessage(msg,wnd,min,max,flag);
}

UTF8API_EXPORT LONG uDispatchMessage(const MSG * msg)
{
	return DispatchMessage(msg);
}

UTF8API_EXPORT HCURSOR uLoadCursor(HINSTANCE hIns,const char * name)
{
	return LoadCursor(hIns,param_os_from_utf8(name));
}

UTF8API_EXPORT HICON uLoadIcon(HINSTANCE hIns,const char * name)
{
	return LoadIcon(hIns,param_os_from_utf8(name));
}

UTF8API_EXPORT HMENU uLoadMenu(HINSTANCE hIns,const char * name)
{
	return LoadMenu(hIns,param_os_from_utf8(name));
}



UTF8API_EXPORT BOOL uGetEnvironmentVariable(const char * name,string_base & out)
{
	string_os_from_utf8 name_t(name);
	DWORD size = GetEnvironmentVariable(name_t,0,0);
	if (size>0)
	{
		size++;
		mem_block_t<TCHAR> temp(size);
		temp[0]=0;
		if (GetEnvironmentVariable(name_t,temp,size)>0)
		{
			temp[size-1]=0;
			out.set_string_os(temp);
			return TRUE;
		}
	}
	return FALSE;
}

UTF8API_EXPORT HMODULE uGetModuleHandle(const char * name)
{
	return GetModuleHandle(param_os_from_utf8(name));
}

UTF8API_EXPORT UINT uRegisterWindowMessage(const char * name)
{
	return RegisterWindowMessage(string_os_from_utf8(name));
}

UTF8API_EXPORT BOOL uMoveFile(const char * src,const char * dst)
{
	return MoveFile(string_os_from_utf8(src),string_os_from_utf8(dst));
}

UTF8API_EXPORT BOOL uDeleteFile(const char * fn)
{
	return DeleteFile(string_os_from_utf8(fn));
}

UTF8API_EXPORT DWORD uGetFileAttributes(const char * fn)
{
	return GetFileAttributes(string_os_from_utf8(fn));
}

UTF8API_EXPORT BOOL uRemoveDirectory(const char * fn)
{
	return RemoveDirectory(string_os_from_utf8(fn));
}

UTF8API_EXPORT HANDLE uCreateFile(const char * fn,DWORD access,DWORD share,LPSECURITY_ATTRIBUTES blah,DWORD creat,DWORD flags,HANDLE tmpl)
{
	return CreateFile(string_os_from_utf8(fn),access,share,blah,creat,flags,tmpl);
}

UTF8API_EXPORT BOOL uCreateDirectory(const char * fn,LPSECURITY_ATTRIBUTES blah)
{
	return CreateDirectory(string_os_from_utf8(fn),blah);
}

UTF8API_EXPORT HANDLE uCreateMutex(LPSECURITY_ATTRIBUTES blah,BOOL bInitialOwner,const char * name)
{
	return name ? CreateMutex(blah,bInitialOwner,string_os_from_utf8(name)) : CreateMutex(blah,bInitialOwner,0);
}

UTF8API_EXPORT BOOL uGetFullPathName(const char * name,string_base & out)
{
	string_os_from_utf8 name_os(name);
	unsigned len = GetFullPathName(name_os,0,0,0);
	if (len==0) return FALSE;
	mem_block_t<TCHAR> temp;
	if (!temp.set_size(len+1)) return FALSE;
	TCHAR * blah;
	if (GetFullPathName(name_os,len+1,temp,&blah)==0) return FALSE;
	temp[len-1]=0;
	out.set_string_os(temp);
	return TRUE;
}

UTF8API_EXPORT BOOL uGetLongPathName(const char * name,string_base & out)
{
	static bool inited;
	typedef DWORD (_stdcall * PGETLONGPATHNAME)(LPCTSTR lpszShortPath, LPTSTR lpszLongPath, DWORD cchBuffer);
	static PGETLONGPATHNAME pGetLongPathName;
	if (!inited)
	{
		pGetLongPathName = (PGETLONGPATHNAME)GetProcAddress(uGetModuleHandle("kernel32.dll"),
#ifdef UNICODE
			"GetLongPathNameW"
#else
			"GetLongPathNameA"
#endif
			);
		inited = true;
	}
	if (pGetLongPathName)
	{
		TCHAR temp[4096];
		temp[0]=0;
		if (pGetLongPathName(string_os_from_utf8(name),temp,tabsize(temp)))
		{
			temp[tabsize(temp)-1]=0;
			out.set_string_os(temp);
			return TRUE;
		}
	}
	return FALSE;
}

UTF8API_EXPORT void uGetCommandLine(string_base & out)
{
	out.set_string_os(GetCommandLine());
}

UTF8API_EXPORT BOOL uGetTempPath(string_base & out)
{
	TCHAR temp[MAX_PATH+1];
	temp[0]=0;
	if (GetTempPath(tabsize(temp),temp))
	{
		temp[tabsize(temp)-1]=0;
		out.set_string_os(temp);
		return TRUE;
	}
	return FALSE;

}
UTF8API_EXPORT BOOL uGetTempFileName(const char * path_name,const char * prefix,UINT unique,string_base & out)
{
	if (path_name==0 || prefix==0) return FALSE;
	TCHAR temp[MAX_PATH+1];
	temp[0]=0;
	if (GetTempFileName(string_os_from_utf8(path_name),string_os_from_utf8(prefix),unique,temp))
	{
		temp[tabsize(temp)-1]=0;
		out.set_string_os(temp);
		return TRUE;
	}
	return FALSE;
}

class uFindFile_i : public uFindFile
{
	string8 fn;
	WIN32_FIND_DATA fd;
	HANDLE hFF;
public:
	uFindFile_i() {hFF = INVALID_HANDLE_VALUE;}
	bool FindFirst(const char * path)
	{
		hFF = FindFirstFile(string_os_from_utf8(path),&fd);
		if (hFF==INVALID_HANDLE_VALUE) return false;
		fn.set_string_os(fd.cFileName);
		return true;
	}
	virtual BOOL FindNext()
	{
		if (hFF==INVALID_HANDLE_VALUE) return FALSE;
		BOOL rv = FindNextFile(hFF,&fd);
		if (rv) fn.set_string_os(fd.cFileName);
		return rv;
	}

	virtual const char * GetFileName()
	{
		return fn;
	}

	virtual __int64 GetFileSize()
	{
		union
		{
			__int64 val64;
			struct
			{
				DWORD lo,hi;
			};
		} ret;
		
		ret.hi = fd.nFileSizeHigh;
		ret.lo = fd.nFileSizeLow;
		return ret.val64;

	}
	virtual DWORD GetAttributes()
	{
		return fd.dwFileAttributes;
	}

	virtual FILETIME GetCreationTime()
	{
		return fd.ftCreationTime;
	}
	virtual FILETIME GetLastAccessTime()
	{
		return fd.ftLastAccessTime;
	}
	virtual FILETIME GetLastWriteTime()
	{
		return fd.ftLastWriteTime;
	}
	virtual ~uFindFile_i()
	{
		if (hFF!=INVALID_HANDLE_VALUE) FindClose(hFF);
	}
};

UTF8API_EXPORT uFindFile * uFindFirstFile(const char * path)
{
	uFindFile_i * ptr = new uFindFile_i;
	if (!ptr->FindFirst(path))
	{
		delete ptr;
		ptr = 0;
	}
	return ptr;
}

UTF8API_EXPORT BOOL uGetOpenFileName(HWND parent,const char * p_ext_mask,unsigned def_ext_mask,const char * p_def_ext,const char * p_title,const char * p_directory,string_base & p_filename,BOOL b_save)
{
	string_os_from_utf8 ext_mask_t(p_ext_mask);
	mem_block_t<TCHAR> ext_mask(_tcslen(ext_mask_t)+2);
	ext_mask.zeromemory();
	_tcscpy(ext_mask,ext_mask_t);

	{
		UINT n;
		for(n=0;n<ext_mask.get_size();n++)
			if (ext_mask[n]=='|')
				ext_mask[n]=0;
	}
	
	TCHAR buffer[4096];

	tcsncpy_addnull(buffer,string_os_from_utf8(p_filename),tabsize(buffer));

	string_os_from_utf8 def_ext(p_def_ext ? p_def_ext : ""),title(p_title ? p_title : ""),
		directory(p_directory ? p_directory : "");

	OPENFILENAME ofn;

	memset(&ofn,0,sizeof(ofn));
	ofn.lStructSize=sizeof(ofn);
	ofn.hwndOwner = parent;
	ofn.lpstrFilter = ext_mask;
	ofn.nFilterIndex = def_ext_mask + 1;
	ofn.lpstrFile = buffer;
	ofn.lpstrInitialDir = directory;
	ofn.nMaxFile = tabsize(buffer);
	ofn.Flags = b_save ? OFN_EXPLORER|OFN_HIDEREADONLY|OFN_PATHMUSTEXIST|OFN_OVERWRITEPROMPT : OFN_EXPLORER|OFN_FILEMUSTEXIST|OFN_HIDEREADONLY|OFN_PATHMUSTEXIST;
	ofn.lpstrDefExt = *(const TCHAR*)def_ext ? (const TCHAR*)def_ext : 0;
	ofn.lpstrTitle = *(const TCHAR*)title ? (const TCHAR*)title : 0;
	if (b_save ? GetSaveFileName(&ofn) : GetOpenFileName(&ofn))
	{
		buffer[tabsize(buffer)-1]=0;

		{
			unsigned ptr = _tcslen(buffer);
			while(ptr>0 && buffer[ptr-1]==' ') buffer[--ptr] = 0;
		}

		p_filename.set_string_os(buffer);
		return TRUE;
	}
	else return FALSE;
}

class uGetOpenFileNameMultiResult_i : public uGetOpenFileNameMultiResult
{
	ptr_list_simple<char> items;	
public:
	void AddItem(const char * param) {items.add_item(strdup(param));}
	virtual UINT GetCount() {return items.get_count();}
	virtual const char * GetFileName(UINT index) {return index>=0 && index<(UINT)items.get_count() ? items[index] : 0;}
	virtual ~uGetOpenFileNameMultiResult_i() {items.free_all();}
};

UTF8API_EXPORT uGetOpenFileNameMultiResult * uGetOpenFileNameMulti(HWND parent,const char * p_ext_mask,unsigned def_ext_mask,const char * p_def_ext,const char * p_title,const char * p_directory)
{
	string_os_from_utf8 ext_mask_t(p_ext_mask);
	mem_block_t<TCHAR> ext_mask(_tcslen(ext_mask_t)+2);
	ext_mask.zeromemory();
	_tcscpy(ext_mask,ext_mask_t);

	{
		UINT n;
		for(n=0;n<ext_mask.get_size();n++)
			if (ext_mask[n]=='|')
				ext_mask[n]=0;
	}
	
	TCHAR buffer[0x4000];
	buffer[0]=0;

	string_os_from_utf8 def_ext(p_def_ext ? p_def_ext : ""),title(p_title ? p_title : ""),
		directory(p_directory ? p_directory : "");

	OPENFILENAME ofn;

	memset(&ofn,0,sizeof(ofn));
	ofn.lStructSize=sizeof(ofn);
	ofn.hwndOwner = parent;
	ofn.lpstrFilter = ext_mask;
	ofn.nFilterIndex = def_ext_mask + 1;
	ofn.lpstrFile = buffer;
	ofn.lpstrInitialDir = directory;
	ofn.nMaxFile = tabsize(buffer);
	ofn.Flags = OFN_EXPLORER|OFN_FILEMUSTEXIST|OFN_HIDEREADONLY|OFN_PATHMUSTEXIST|OFN_ALLOWMULTISELECT;
	ofn.lpstrDefExt = *(const TCHAR*)def_ext ? (const TCHAR*)def_ext : 0;
	ofn.lpstrTitle = *(const TCHAR*)title ? (const TCHAR*)title : 0;
	if (GetOpenFileName(&ofn))
	{
		buffer[tabsize(buffer)-1]=0;
		buffer[tabsize(buffer)-2]=0;

		uGetOpenFileNameMultiResult_i * result = new uGetOpenFileNameMultiResult_i;

		TCHAR * p=buffer;
		while(*p) p++;
		p++;
		if (!*p)
		{
			{
				unsigned ptr = _tcslen(buffer);
				while(ptr>0 && buffer[ptr-1]==' ') buffer[--ptr] = 0;
			}

			result->AddItem(string_utf8_from_os(buffer));
		}
		else
		{
			string8 s;
			s.set_string_os(buffer);
			int ofs=s.length();
			if (s[ofs-1]!='\\') {s.add_char('\\');ofs++;}
			while(*p)
			{
				s.truncate(ofs);
				s.add_string_os(p);
				s.skip_trailing_char(' ');
				result->AddItem(s);
				while(*p) p++;
				p++;
			}
		}
		return result;
	}
	else return 0;
}

UTF8API_EXPORT HINSTANCE uShellExecute(HWND wnd,const char * oper,const char * file,const char * params,const char * dir,int cmd)
{
	return ShellExecute(wnd,param_os_from_utf8(oper),param_os_from_utf8(file),param_os_from_utf8(params),param_os_from_utf8(dir),cmd);
}

UTF8API_EXPORT HWND uCreateStatusWindow(LONG style,const char * text,HWND parent,UINT id)
{
	return CreateStatusWindow(style,param_os_from_utf8(text),parent,id);
}

UTF8API_EXPORT ATOM uRegisterClass(const uWNDCLASS * ptr)
{
	param_os_from_utf8 str_menu(ptr->lpszMenuName),str_class(ptr->lpszClassName);
	WNDCLASS cl;
	cl.style = ptr->style;
	cl.lpfnWndProc = ptr->lpfnWndProc;
	cl.cbClsExtra = ptr->cbClsExtra;
	cl.cbWndExtra = ptr->cbWndExtra;
	cl.hInstance = ptr->hInstance;
	cl.hIcon = ptr->hIcon;
	cl.hCursor = ptr->hCursor;
	cl.hbrBackground = ptr->hbrBackground;
	cl.lpszMenuName = str_menu;
	cl.lpszClassName = str_class;

	return RegisterClass(&cl);
}

UTF8API_EXPORT HWND uCreateWindowEx(DWORD dwExStyle,const char * lpClassName,const char * lpWindowName,DWORD dwStyle,int x,int y,int nWidth,int nHeight,HWND hWndParent,HMENU hMenu,HINSTANCE hInstance,LPVOID lpParam)
{
	return CreateWindowEx(dwExStyle,param_os_from_utf8(lpClassName),param_os_from_utf8(lpWindowName),dwStyle,x,y,nWidth,nHeight,hWndParent,hMenu,hInstance,lpParam);
}

UTF8API_EXPORT HANDLE uLoadImage(HINSTANCE hIns,const char * name,UINT type,int x,int y,UINT flags)
{
	return LoadImage(hIns,param_os_from_utf8(name),type,x,y,flags);
}

UTF8API_EXPORT BOOL uGetSystemDirectory(string_base & out)
{
	UINT len = GetSystemDirectory(0,0);
	if (len==0) len = MAX_PATH;
	len++;
	mem_block_t<TCHAR> temp;
	if (!temp.set_size(len)) return FALSE;
	if (GetSystemDirectory(temp,len)==0) return FALSE;
	temp[len-1]=0;
	out.set_string_os(temp);
	return TRUE;	
}

UTF8API_EXPORT BOOL uGetWindowsDirectory(string_base & out)
{
	UINT len = GetWindowsDirectory(0,0);
	if (len==0) len = MAX_PATH;
	len++;
	mem_block_t<TCHAR> temp;
	if (!temp.set_size(len)) return FALSE;
	if (GetWindowsDirectory(temp,len)==0) return FALSE;
	temp[len-1]=0;
	out.set_string_os(temp);
	return TRUE;	
}

UTF8API_EXPORT BOOL uSetCurrentDirectory(const char * path)
{
	return SetCurrentDirectory(string_os_from_utf8(path));
}

UTF8API_EXPORT BOOL uGetCurrentDirectory(string_base & out)
{
	UINT len = GetCurrentDirectory(0,0);
	if (len==0) len = MAX_PATH;
	len++;
	mem_block_t<TCHAR> temp;
	if (!temp.set_size(len)) return FALSE;
	if (GetCurrentDirectory(len,temp)==0) return FALSE;
	temp[len-1]=0;
	out.set_string_os(temp);
	return TRUE;		
}

UTF8API_EXPORT BOOL uExpandEnvironmentStrings(const char * src,string_base & out)
{
	string_os_from_utf8 src_os(src);
	UINT len = ExpandEnvironmentStrings(src_os,0,0);
	if (len==0) len = 256;
	len++;
	mem_block_t<TCHAR> temp;
	if (!temp.set_size(len)) return FALSE;
	if (ExpandEnvironmentStrings(src_os,temp,len)==0) return FALSE;
	temp[len-1]=0;
	out.set_string_os(temp);
	return TRUE;
}

UTF8API_EXPORT BOOL uGetUserName(string_base & out)
{
	TCHAR temp[UNLEN+1];
	DWORD len = tabsize(temp);
	if (GetUserName(temp,&len))
	{
		temp[tabsize(temp)-1]=0;
		out.set_string_os(temp);
		return TRUE;
	}
	else return FALSE;
}

UTF8API_EXPORT BOOL uGetShortPathName(const char * src,string_base & out)
{
	string_os_from_utf8 src_os(src);
	UINT len = GetShortPathName(src_os,0,0);
	if (len==0) len = MAX_PATH;
	len++;
	mem_block_t<TCHAR> temp(len);
	if (GetShortPathName(src_os,temp,len))
	{
		temp[len-1]=0;
		out.set_string_os(temp);
		return TRUE;
	}
	else return FALSE;
}

UTF8API_EXPORT BOOL uUnregisterClass(const char * name,HINSTANCE ins)
{
	return UnregisterClass(param_os_from_utf8(name),ins);
}

#ifdef UNICODE
#define DDE_CODEPAGE CP_WINUNICODE
#else
#define DDE_CODEPAGE CP_WINANSI
#endif


UTF8API_EXPORT HSZ uDdeCreateStringHandle(DWORD ins,const char * src)
{
	return DdeCreateStringHandle(ins,string_os_from_utf8(src),DDE_CODEPAGE);
}

UTF8API_EXPORT BOOL uDdeQueryString(DWORD ins,HSZ hsz,string_base & out)
{
	mem_block_t<TCHAR> temp;
	UINT len = DdeQueryString(ins,hsz,0,0,DDE_CODEPAGE);
	if (len==0) len = MAX_PATH;
	len++;
	if (temp.set_size(len)==0) return FALSE;
	if (DdeQueryString(ins,hsz,temp,len,DDE_CODEPAGE))
	{
		temp[len-1]=0;
		out.set_string_os(temp);
		return TRUE;
	}
	else return FALSE;
}

UTF8API_EXPORT UINT uDdeInitialize(LPDWORD pidInst,PFNCALLBACK pfnCallback,DWORD afCmd,DWORD ulRes)
{
	return DdeInitialize(pidInst,pfnCallback,afCmd,ulRes);
}

UTF8API_EXPORT BOOL uDdeAccessData_Text(HDDEDATA data,string_base & out)
{
	const TCHAR * ptr = (const TCHAR*) DdeAccessData(data,0);
	if (ptr)
	{
		out.set_string_os(ptr);
		return TRUE;
	}
	else return FALSE;
}

UTF8API_EXPORT HANDLE uSortStringCreate(const char * src)
{
	TCHAR * ret = mem_ops<TCHAR>::alloc(estimate_utf8_to_os(src));
	if (ret)
	{
		convert_utf8_to_os(src,ret);
		ret = mem_ops<TCHAR>::realloc(ret,_tcslen(ret)+1);
	}
	return (HANDLE)ret;
}

UTF8API_EXPORT int uSortStringCompareEx(HANDLE string1,HANDLE string2,DWORD flags)
{
	return CompareString(LOCALE_USER_DEFAULT,flags,reinterpret_cast<const TCHAR*>(string1),-1,reinterpret_cast<const TCHAR*>(string2),-1);
}

UTF8API_EXPORT int uSortStringCompare(HANDLE string1,HANDLE string2)
{
	return lstrcmpi(reinterpret_cast<const TCHAR*>(string1),reinterpret_cast<const TCHAR*>(string2));
}

UTF8API_EXPORT void uSortStringFree(HANDLE string)
{
	mem_ops<TCHAR>::free(reinterpret_cast<TCHAR*>(string));
}

static void stringlist_os_from_utf8(const char * src,mem_block_t<TCHAR> & out)
{
	unsigned p_out = 0;
	if (src)
	{
		while(*src)
		{
			unsigned src_len = strlen(src);
			out.check_size(p_out + estimate_utf8_to_os(src,src_len));
			p_out += 1 + convert_utf8_to_os(src,out.get_ptr() + p_out,src_len);
			src += src_len + 1;
		}
	}
	out.copy(TEXT("\0"),2,p_out);
}

UTF8API_EXPORT int uSHFileOperation(uSHFILEOPSTRUCT * p_op)
{
	mem_block_t<TCHAR> temp1,temp2;
	stringlist_os_from_utf8(p_op->pFrom,temp1);
	stringlist_os_from_utf8(p_op->pTo,temp2);

	param_os_from_utf8 progress_title(p_op->lpszProgressTitle);

	SHFILEOPSTRUCT op;
	op.hwnd = p_op->hwnd;
	op.wFunc = p_op->wFunc;
	op.pFrom = temp1;
	op.pTo = temp2;
	op.fFlags = p_op->fFlags;
	op.fAnyOperationsAborted = p_op->fAnyOperationsAborted;
	op.hNameMappings = p_op->hNameMappings;
	op.lpszProgressTitle = progress_title;
	
	int ret = SHFileOperation(&op);
	p_op->fAnyOperationsAborted = op.fAnyOperationsAborted;
	return ret;
}

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
)
{
	if (lpStartupInfo->cb!=sizeof(uSTARTUPINFO)) return FALSE;
	STARTUPINFO si;
	param_os_from_utf8 si_lpDesktop(lpStartupInfo->lpDesktop);
	param_os_from_utf8 si_lpTitle(lpStartupInfo->lpTitle);
	si.cb = sizeof(si);
	si.lpReserved = 0;
	si.lpDesktop = const_cast<TCHAR*>(si_lpDesktop.get_ptr());
	si.lpTitle = const_cast<TCHAR*>(si_lpTitle.get_ptr());
	si.dwX = lpStartupInfo->dwX;
	si.dwY = lpStartupInfo->dwY;
	si.dwXSize = lpStartupInfo->dwXSize;
	si.dwYSize = lpStartupInfo->dwYSize;
	si.dwXCountChars = lpStartupInfo->dwXCountChars;
	si.dwYCountChars = lpStartupInfo->dwYCountChars;
	si.dwFillAttribute = lpStartupInfo->dwFillAttribute;
	si.dwFlags = lpStartupInfo->dwFlags;
	si.wShowWindow = lpStartupInfo->wShowWindow;
	si.cbReserved2 = 0;
	si.lpReserved2 = 0;
	si.hStdInput = lpStartupInfo->hStdInput;
	si.hStdOutput = lpStartupInfo->hStdOutput;
	si.hStdError = lpStartupInfo->hStdError;
	
	mem_block_fastalloc<TCHAR> env;
	if (lpEnvironment)
	{
		unsigned env_out = 0;
		const char * p_env = lpEnvironment;
		do {
			env.check_size(env_out + estimate_utf8_to_os(p_env));
			env_out += convert_utf8_to_os(p_env,env+env_out) + 1;
			p_env += strlen(p_env) + 1;
		} while(*p_env);
		env.check_size(env_out+1);
		env[env_out]=0;
	}

	mem_block_t<TCHAR> gay;
	gay.set_size(estimate_utf8_to_os(lpCommandLine)+MAX_PATH);
	convert_utf8_to_os(lpCommandLine,gay);

	return CreateProcess(param_os_from_utf8(lpApplicationName),
		gay,
		lpProcessAttributes,lpThreadAttributes,
		bInheritHandles,
#ifdef UNICODE
		dwCreationFlags|CREATE_UNICODE_ENVIRONMENT,
#else
		dwCreationFlags&~CREATE_UNICODE_ENVIRONMENT,
#endif
		lpEnvironment ? env.get_ptr() : 0,
		param_os_from_utf8(lpCurrentDirectory),
		&si,
		lpProcessInformation
		);
}

UTF8API_EXPORT HTREEITEM uTreeView_InsertItem(HWND wnd,const uTVINSERTSTRUCT * param)
{
	mem_block_t<TCHAR> temp;
	unsigned needed_length = estimate_utf8_to_os(param->item.pszText);
	TCHAR * pszText = (needed_length * sizeof(TCHAR) <= PFC_ALLOCA_LIMIT) ? (TCHAR*)alloca(needed_length  * sizeof(TCHAR)) : temp.set_size(PFC_ALLOCA_LIMIT);
	convert_utf8_to_os(param->item.pszText,pszText);
	

	TVINSERTSTRUCT l_param;
	memset(&l_param,0,sizeof(l_param));
	l_param.hParent = param->hParent;
	l_param.hInsertAfter = param->hInsertAfter;
	l_param.item.mask = param->item.mask;
	l_param.item.hItem = param->item.hItem;
	l_param.item.state = param->item.state;
	l_param.item.stateMask = param->item.stateMask;
	l_param.item.pszText = pszText;
	l_param.item.cchTextMax = 0;
	l_param.item.iImage = param->item.iImage;
	l_param.item.iSelectedImage = param->item.iImage;
	l_param.item.cChildren = param->item.cChildren;
	l_param.item.lParam = param->item.lParam;
	if (param->item.mask & TVIF_INTEGRAL)
	{
		l_param.itemex.iIntegral = param->itemex.iIntegral;
	}

	return (HTREEITEM) uSendMessage(wnd,TVM_INSERTITEM,0,(LPARAM)&l_param);
}

UTF8API_EXPORT UINT uGetFontHeight(HFONT font)
{
	UINT ret;
	HDC dc = CreateCompatibleDC(0);
	SelectObject(dc,font);
	ret = uGetTextHeight(dc);
	DeleteDC(dc);
	return ret;
}

UTF8API_EXPORT HHOOK uSetWindowsHookEx(int idhook,HOOKPROC fn,HINSTANCE hMod,DWORD threadid)
{
	return SetWindowsHookEx(idhook,fn,hMod,threadid);
}

UTF8API_EXPORT HIMAGELIST uImageList_LoadImage(HINSTANCE hi, const char * lpbmp, int cx, int cGrow, COLORREF crMask, UINT uType, UINT uFlags)
{
	return ImageList_LoadImage(hi,param_os_from_utf8(lpbmp),cx,cGrow,crMask,uType,uFlags);
}

UTF8API_EXPORT BOOL uSendNotifyMessage(HWND wnd,UINT msg,WPARAM wp,LPARAM lp)
{
	return SendNotifyMessage(wnd,msg,wp,lp);
}

UTF8API_EXPORT BOOL uSendMessageCallback(HWND wnd,UINT msg,WPARAM wp,LPARAM lp,SENDASYNCPROC cb,DWORD data)
{
	return SendMessageCallback(wnd,msg,wp,lp,cb,data);
}

UTF8API_EXPORT unsigned uOSStringEstimateSize(const char * src,unsigned len)
{
	return estimate_utf8_to_os(src,len) * sizeof(TCHAR);
}

UTF8API_EXPORT unsigned uOSStringConvert(const char * src,void * dst,unsigned len)
{
	return convert_utf8_to_os(src,(TCHAR*)dst,len);
}

UTF8API_EXPORT int uTabCtrl_InsertItem(HWND wnd,int idx,const uTCITEM * item)
{
	param_os_from_utf8 text((item->mask & TCIF_TEXT) ? item->pszText : 0);
	TCITEM l_item;
	assert(sizeof(l_item)==sizeof(*item));//meh lazy
	memcpy(&l_item,item,sizeof(l_item));
	l_item.pszText = const_cast<TCHAR*>(text.get_ptr());
	l_item.cchTextMax = 0;
	return TabCtrl_InsertItem(wnd,idx,&l_item);
}

UTF8API_EXPORT int uTabCtrl_SetItem(HWND wnd,int idx,const uTCITEM * item)
{
	param_os_from_utf8 text((item->mask & TCIF_TEXT) ? item->pszText : 0);
	TCITEM l_item;
	assert(sizeof(l_item)==sizeof(*item));//meh lazy
	memcpy(&l_item,item,sizeof(l_item));
	l_item.pszText = const_cast<TCHAR*>(text.get_ptr());
	l_item.cchTextMax = 0;
	return TabCtrl_SetItem(wnd,idx,&l_item);
}

UTF8API_EXPORT int uGetKeyNameText(LONG lparam,string_base & out)
{
	TCHAR temp[256];
	temp[0]=0;
	if (!GetKeyNameText(lparam,temp,tabsize(temp))) return 0;
	temp[tabsize(temp)-1]=0;
	out.set_string(string_utf8_from_os(temp));
	return 1;
}

UTF8API_EXPORT HANDLE uCreateFileMapping(HANDLE hFile,LPSECURITY_ATTRIBUTES lpFileMappingAttributes,DWORD flProtect,DWORD dwMaximumSizeHigh,DWORD dwMaximumSizeLow,const char * lpName)
{
	return CreateFileMapping(hFile,lpFileMappingAttributes,flProtect,dwMaximumSizeHigh,dwMaximumSizeLow,param_os_from_utf8(lpName));
}

UTF8API_EXPORT BOOL uListBox_GetText(HWND listbox,UINT index,string_base & out)
{
	unsigned len = uSendMessage(listbox,LB_GETTEXTLEN,index,0);
	if (len==LB_ERR || len>16*1024*1024) return FALSE;
	if (len==0) {out.reset();return TRUE;}

	mem_block_t<TCHAR> temp(len+1);
	if (temp.get_ptr()==0) return FALSE;
	temp.zeromemory();
	len = uSendMessage(listbox,LB_GETTEXT,index,(LPARAM)temp.get_ptr());
	if (len==LB_ERR) return false;
	out.set_string(string_utf8_from_os(temp));
	return TRUE;
}

UTF8API_EXPORT void uPrintf(string_base & out,const char * fmt,...)
{
	va_list list;
	va_start(list,fmt);
	uPrintfV(out,fmt,list);
	va_end(list);
}

UTF8API_EXPORT void uPrintfV(string_base & out,const char * fmt,va_list arglist)
{
	string_printf::g_run(out,fmt,arglist);
}

UTF8API_EXPORT int uCompareString(DWORD flags,const char * str1,unsigned len1,const char * str2,unsigned len2)
{
	return CompareString(LOCALE_USER_DEFAULT,flags,string_os_from_utf8(str1,len1),-1,string_os_from_utf8(str2,len2),-1);
}

class uResource_i : public uResource
{
	unsigned size;
	const void * ptr;
public:
	inline uResource_i(const void * p_ptr,unsigned p_size) : ptr(p_ptr), size(p_size)
	{
	}
	virtual const void * GetPointer()
	{
		return ptr;
	}
	virtual unsigned GetSize()
	{
		return size;
	}
	virtual ~uResource_i()
	{
	}
};

UTF8API_EXPORT uResource * uLoadResource(HMODULE hMod,const char * name,const char * type,WORD wLang)
{
	HRSRC res = uFindResource(hMod,name,type,wLang);
	if (res==0) return 0;
	HGLOBAL hglob = LoadResource(hMod,res);
	if (hglob)
	{
		void * ptr = LockResource(hglob);
		if (ptr)
		{
			return new uResource_i(ptr,SizeofResource(hMod,res));
		}
		else return 0;
	}
	else return 0;

}

UTF8API_EXPORT HRSRC uFindResource(HMODULE hMod,const char * name,const char * type,WORD wLang)
{
	return wLang ? FindResourceEx(hMod,param_os_from_utf8(name),param_os_from_utf8(type),wLang) : FindResource(hMod,param_os_from_utf8(name),param_os_from_utf8(type));
}

UTF8API_EXPORT BOOL uLoadString(HINSTANCE ins,UINT id,string_base & out)
{
	BOOL rv = FALSE;
	uResource * res = uLoadResource(ins,uMAKEINTRESOURCE(id),(const char*)(RT_STRING));
	if (res)
	{
		unsigned size = res->GetSize();
		const WCHAR * ptr = (const WCHAR*)res->GetPointer();
		if (size>=4)
		{
			unsigned len = *(const WORD*)(ptr+1);
			if (len * 2 + 4 <= size)
			{
				out = string_utf8_from_wide(ptr+2,len);
			}
		}
		
		delete res;
		rv = TRUE;
	}
	return rv;
}

UTF8API_EXPORT BOOL uGetMenuString(HMENU menu,UINT id,string_base & out,UINT flag)
{
	unsigned len = GetMenuString(menu,id,0,0,flag);
	if (len==0)
	{
		out.reset();
		return FALSE;
	}
	TCHAR * temp = (TCHAR*)malloc((len+1)*sizeof(TCHAR));
	if (temp==0)
	{
		out.reset();
		return FALSE;
	}
	if (GetMenuString(menu,id,temp,len+1,flag)==0)
	{
		free(temp);
		out.reset();
		return FALSE;
	}
	out = string_utf8_from_os(temp);
	free(temp);
	return TRUE;
}

UTF8API_EXPORT BOOL uModifyMenu(HMENU menu,UINT id,UINT flags,UINT newitem,const char * data)
{
	return ModifyMenu(menu,id,flags,newitem,param_os_from_utf8(data));
}

UTF8API_EXPORT UINT uGetMenuItemType(HMENU menu,UINT position)
{
	MENUITEMINFO info;
	memset(&info,0,sizeof(info));
	info.cbSize = sizeof(info);
	info.fMask = MIIM_TYPE;
	if (!GetMenuItemInfo(menu,position,TRUE,&info))
		return 0;
	return info.fType;
}

static inline bool i_is_path_separator(unsigned c)
{
	return c=='\\' || c=='/' || c=='|' || c==':';
}

UTF8API_EXPORT int uSortPathCompare(HANDLE string1,HANDLE string2)
{
	const TCHAR * s1 = reinterpret_cast<const TCHAR*>(string1);
	const TCHAR * s2 = reinterpret_cast<const TCHAR*>(string2);
	const TCHAR * p1, * p2;

	while (*s1 || *s2)
	{
		if (*s1 == *s2)
		{
			s1++;
			s2++;
			continue;
		}

		p1 = s1; while (*p1 && !i_is_path_separator(*p1)) p1++;
		p2 = s2; while (*p2 && !i_is_path_separator(*p2)) p2++;

		if ((!*p1 && !*p2) || (*p1 && *p2))
		{
			int test = CompareString(LOCALE_USER_DEFAULT, NORM_IGNORECASE | NORM_IGNOREKANATYPE | NORM_IGNOREWIDTH, s1, p1 - s1, s2, p2 - s2);
			if (test && test != 2) return test - 2;
			if (!*p1) return 0;
		}
		else
		{
			if (*p1) return -1;
			else return 1;
		}

		s1 = p1 + 1;
		s2 = p2 + 1;
	}
	
	return 0;
}

UTF8API_EXPORT UINT uRegisterClipboardFormat(const char * name)
{
	return RegisterClipboardFormat(string_os_from_utf8(name));
}

UTF8API_EXPORT BOOL uGetClipboardFormatName(UINT format,string_base & out)
{
	TCHAR temp[1024];
	if (!GetClipboardFormatName(format,temp,tabsize(temp))) return FALSE;
	temp[tabsize(temp)-1] = 0;
	out.set_string_os(temp);
	return TRUE;
}

}//extern "C"
#ifdef _MSC_VER
#if _MSC_VER==1200
#pragma comment(linker,"/opt:nowin98")
#endif
#endif