#include "stdafx.h"


#ifndef _WIN32
#error PORTME
#endif

static bool is_spacing(char c) {return c == ' ' || c==10 || c==13 || c == '\t';}

static bool is_spacing(const char * str, t_size len) {
	for(t_size walk = 0; walk < len; ++walk) if (!is_spacing(str[walk])) return false;
	return true;
}

typedef exception_io_data exception_time_error;

static unsigned ParseDateElem(const char * ptr, t_size len) {
	unsigned ret = 0;
	for(t_size walk = 0; walk < len; ++walk) {
		const char c = ptr[walk];
		if (c < '0' || c > '9') throw exception_time_error();
		ret = ret * 10 + (unsigned)(c - '0');
	}
	return ret;
}

t_filetimestamp foobar2000_io::filetimestamp_from_string(const char * date) {
	// Accepted format
	// YYYY-MM-DD HH:MM:SS
	try {
		t_size remaining = strlen(date);
		SYSTEMTIME st = {};
		st.wDay = 1; st.wMonth = 1;
		for(;;) {
#define ADVANCE(n) { PFC_ASSERT( remaining >= n); date += n; remaining -= n; }
#define ADVANCE_TEST(n) { if (remaining < n) throw exception_time_error(); }
#define PARSE(var, digits) { ADVANCE_TEST(digits); var = (WORD) ParseDateElem(date, digits); ADVANCE(digits) ; }
#define TEST_END( durationIncrement ) 
#define SKIP(c) { ADVANCE_TEST(1); if (date[0] != c) throw exception_time_error(); ADVANCE(1); }
#define SKIP_SPACING() {while(remaining > 0 && is_spacing(*date)) ADVANCE(1);}
			SKIP_SPACING();
			PARSE( st.wYear, 4 ); if (st.wYear < 1601) throw exception_time_error();
			TEST_END(wYear); SKIP('-');
			PARSE( st.wMonth, 2 ); if (st.wMonth < 1 || st.wMonth > 12) throw exception_time_error();
			TEST_END(wMonth); SKIP('-');
			PARSE( st.wDay, 2); if (st.wDay < 1 || st.wDay > 31) throw exception_time_error();
			TEST_END(wDay); SKIP(' ');
			PARSE( st.wHour, 2); if (st.wHour >= 24) throw exception_time_error();
			TEST_END(wHour); SKIP(':');
			PARSE( st.wMinute, 2); if (st.wMinute >= 60) throw exception_time_error();
			TEST_END(wMinute); SKIP(':');
			PARSE( st.wSecond, 2); if (st.wSecond >= 60) throw exception_time_error();
			SKIP_SPACING();
			TEST_END( wSecond );
#undef ADVANCE
#undef ADVANCE_TEST
#undef PARSE
#undef TEST_END
#undef SKIP
#undef SKIP_SPACING
			if (remaining > 0) throw exception_time_error();
			break;
		}
		t_filetimestamp base, out;
		if (!SystemTimeToFileTime(&st, (FILETIME*) &base)) throw exception_time_error();
		if (!LocalFileTimeToFileTime((const FILETIME*)&base, (FILETIME*)&out)) throw exception_time_error();
		return out;
	} catch(exception_time_error) {
		return filetimestamp_invalid;
	}
}



format_filetimestamp::format_filetimestamp(t_filetimestamp p_timestamp) {
	SYSTEMTIME st; FILETIME ft;
	if (FileTimeToLocalFileTime((FILETIME*)&p_timestamp,&ft)) {
		if (FileTimeToSystemTime(&ft,&st)) {
			m_buffer 
				<< pfc::format_uint(st.wYear,4) << "-" << pfc::format_uint(st.wMonth,2) << "-" << pfc::format_uint(st.wDay,2) << " " 
				<< pfc::format_uint(st.wHour,2) << ":" << pfc::format_uint(st.wMinute,2) << ":" << pfc::format_uint(st.wSecond,2);
		} else m_buffer << "<invalid timestamp>";
	} else m_buffer << "<invalid timestamp>";
}
