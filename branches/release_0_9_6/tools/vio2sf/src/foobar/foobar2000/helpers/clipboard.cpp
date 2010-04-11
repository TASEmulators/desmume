#include "stdafx.h"

#ifdef UNICODE
#define CF_TCHAR CF_UNICODETEXT
#else
#define CF_TCHAR CF_TEXT
#endif

namespace ClipboardHelper {
	void SetRaw(UINT format,const void * data, t_size size) {
		HANDLE buffer = GlobalAlloc(GMEM_DDESHARE,size);
		if (buffer == NULL) throw std::bad_alloc();
		try {
			CGlobalLockScope lock(buffer);
			PFC_ASSERT(lock.GetSize() == size);
			memcpy(lock.GetPtr(),data,size);
		} catch(...) {
			GlobalFree(buffer); throw;
		}

		WIN32_OP(SetClipboardData(format,buffer) != NULL);
	}
	void SetString(const char * in) {
		pfc::stringcvt::string_os_from_utf8 temp(in);
		SetRaw(CF_TCHAR,temp.get_ptr(),(temp.length() + 1) * sizeof(TCHAR));
	}

	bool GetString(pfc::string_base & out) {
		pfc::array_t<t_uint8> temp;
		if (!GetRaw(CF_TCHAR,temp)) return false;
		out = pfc::stringcvt::string_utf8_from_os(reinterpret_cast<const TCHAR*>(temp.get_ptr()),temp.get_size() / sizeof(TCHAR));
		return true;
	}
	bool IsTextAvailable() {
		return IsClipboardFormatAvailable(CF_TCHAR) == TRUE;
	}
}
