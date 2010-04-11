class win32_menu {
public:
	win32_menu(HMENU p_initval) : m_menu(p_initval) {}
	win32_menu() : m_menu(NULL) {}
	~win32_menu() {release();}
	void release() {
		if (m_menu != NULL) {
			DestroyMenu(m_menu);
			m_menu = NULL;
		}
	}
	void set(HMENU p_menu) {release(); m_menu = p_menu;}
	void create_popup() {
		release();
		SetLastError(NO_ERROR);
		m_menu = CreatePopupMenu();
		if (m_menu == NULL) throw exception_win32(GetLastError());
	}
	HMENU get() const {return m_menu;}
	HMENU detach() {return pfc::replace_t(m_menu,(HMENU)NULL);}
	
	bool is_valid() const {return m_menu != NULL;}
private:
	win32_menu(const win32_menu &) {throw pfc::exception_not_implemented();}
	const win32_menu & operator=(const win32_menu &) {throw pfc::exception_not_implemented();}

	HMENU m_menu;
};

class win32_font {
public:
	win32_font(HFONT p_initval) : m_font(p_initval) {}
	win32_font() : m_font(NULL) {}
	~win32_font() {release();}

	void release() {
		HFONT temp = detach();
		if (temp != NULL) DeleteObject(temp);
	}

	void set(HFONT p_font) {release(); m_font = p_font;}
	HFONT get() const {return m_font;}
	HFONT detach() {return pfc::replace_t(m_font,(HFONT)NULL);}

	void create(const t_font_description & p_desc) {
		SetLastError(NO_ERROR);
		HFONT temp = p_desc.create();
		if (temp == NULL) throw exception_win32(GetLastError());
		set(temp);
	}

	bool is_valid() const {return m_font != NULL;}

private:
	win32_font(const win32_font&) {throw pfc::exception_not_implemented();}
	const win32_font & operator=(const win32_font &) {throw pfc::exception_not_implemented();}

	HFONT m_font;
};

class win32_event {
public:
	win32_event() : m_handle(NULL) {}
	~win32_event() {release();}

	void create(bool p_manualreset,bool p_initialstate) {
		release();
		SetLastError(NO_ERROR);
		m_handle = CreateEvent(NULL,p_manualreset ? TRUE : FALSE, p_initialstate ? TRUE : FALSE,NULL);
		if (m_handle == NULL) throw exception_win32(GetLastError());
	}
	
	void set(HANDLE p_handle) {release(); m_handle = p_handle;}
	HANDLE get() const {return m_handle;}
	HANDLE detach() {return pfc::replace_t(m_handle,(HANDLE)NULL);}
	bool is_valid() const {return m_handle != NULL;}
	
	void release() {
		HANDLE temp = detach();
		if (temp != NULL) CloseHandle(temp);
	}


	//! Returns true when signaled, false on timeout
	bool wait_for(double p_timeout_seconds) {return g_wait_for(get(),p_timeout_seconds);}
	
	static DWORD g_calculate_wait_time(double p_seconds) {
		DWORD time = 0;
		if (p_seconds> 0) {
			time = audio_math::rint32((audio_sample)(p_seconds * 1000.0));
			if (time == 0) time = 1;
		} else if (p_seconds < 0) {
			time = INFINITE;
		}
		return time;
	}

	//! Returns true when signaled, false on timeout
	static bool g_wait_for(HANDLE p_event,double p_timeout_seconds) {
		SetLastError(NO_ERROR);
		DWORD status = WaitForSingleObject(p_event,g_calculate_wait_time(p_timeout_seconds));
		switch(status) {
		case WAIT_FAILED:
			throw exception_win32(GetLastError());
		default:
			throw pfc::exception_bug_check();
		case WAIT_OBJECT_0:
			return true;
		case WAIT_TIMEOUT:
			return false;
		}
	}

	void set_state(bool p_state) {
		PFC_ASSERT(m_handle != NULL);
		if (p_state) SetEvent(m_handle);
		else ResetEvent(m_handle);
	}

private:
	win32_event(const win32_event&) {throw pfc::exception_not_implemented();}
	const win32_event & operator=(const win32_event &) {throw pfc::exception_not_implemented();}

	HANDLE m_handle;
};

static void uSleepSeconds(double p_time,bool p_alertable) {
	SleepEx(win32_event::g_calculate_wait_time(p_time),p_alertable ? TRUE : FALSE);
}




class win32_icon {
public:
	win32_icon(HICON p_initval) : m_icon(p_initval) {}
	win32_icon() : m_icon(NULL) {}
	~win32_icon() {release();}

	void release() {
		HICON temp = detach();
		if (temp != NULL) DestroyIcon(temp);
	}

	void set(HICON p_icon) {release(); m_icon = p_icon;}
	HICON get() const {return m_icon;}
	HICON detach() {return pfc::replace_t(m_icon,(HICON)NULL);}

	bool is_valid() const {return m_icon != NULL;}

private:
	win32_icon(const win32_icon&) {throw pfc::exception_not_implemented();}
	const win32_icon & operator=(const win32_icon &) {throw pfc::exception_not_implemented();}

	HICON m_icon;
};

class win32_accelerator {
public:
	win32_accelerator() : m_accel(NULL) {}
	~win32_accelerator() {release();}
	HACCEL get() const {return m_accel;}

	void load(HINSTANCE p_inst,const TCHAR * p_id) {
		release();
		SetLastError(NO_ERROR);
		m_accel = LoadAccelerators(p_inst,p_id);
		if (m_accel == NULL) {
			throw exception_win32(GetLastError());
		}
	}
	
	void release() {
		if (m_accel != NULL) {
			DestroyAcceleratorTable(m_accel);
			m_accel = NULL;
		}
	}
private:
	HACCEL m_accel;
	PFC_CLASS_NOT_COPYABLE(win32_accelerator,win32_accelerator);
};


class SelectObjectScope {
public:
	SelectObjectScope(HDC p_dc,HGDIOBJ p_obj) throw() : m_dc(p_dc), m_obj(SelectObject(p_dc,p_obj)) {}
	~SelectObjectScope() throw() {SelectObject(m_dc,m_obj);}
private:
	PFC_CLASS_NOT_COPYABLE_EX(SelectObjectScope)
	HDC m_dc;
	HGDIOBJ m_obj;
};

class OffsetWindowOrgScope {
public:
	OffsetWindowOrgScope(HDC dc, const POINT & pt) throw() : m_dc(dc), m_pt(pt) {
		OffsetWindowOrgEx(m_dc, m_pt.x, m_pt.y, NULL);
	}
	~OffsetWindowOrgScope() throw() {
		OffsetWindowOrgEx(m_dc, -m_pt.x, -m_pt.y, NULL);
	}

private:
	const HDC m_dc;
	const POINT m_pt;
};
class DCStateScope {
public:
	DCStateScope(HDC p_dc) throw() : m_dc(p_dc) {
		m_state = SaveDC(m_dc);
	}
	~DCStateScope() throw() {
		RestoreDC(m_dc,m_state);
	}
private:
	const HDC m_dc;
	int m_state;
};


class exception_com : public std::exception {
public:
	exception_com(HRESULT p_code) : std::exception(format_win32_error(p_code)), m_code(p_code) {}
	HRESULT get_code() const {return m_code;}
private:
	HRESULT m_code;
};

// Same format as _WIN32_WINNT macro.
static WORD GetWindowsVersionCode() throw() {
	const DWORD ver = GetVersion();
	return (WORD)HIBYTE(LOWORD(ver)) | ((WORD)LOBYTE(LOWORD(ver)) << 8);
}

//! Simple implementation of a COM reference counter. The initial reference count is zero, so it can be used with pfc::com_ptr_t<> with plain operator=/constructor rather than attach().
template<typename TBase> class ImplementCOMRefCounter : public TBase {
public:
	TEMPLATE_CONSTRUCTOR_FORWARD_FLOOD(ImplementCOMRefCounter,TBase)
	ULONG STDMETHODCALLTYPE AddRef() {
		return ++m_refcounter;
	}
	ULONG STDMETHODCALLTYPE Release() {
		long val = --m_refcounter;
		if (val == 0) delete this;
		return val;
	}
protected:
	virtual ~ImplementCOMRefCounter() {}
private:
	pfc::refcounter m_refcounter;
};
