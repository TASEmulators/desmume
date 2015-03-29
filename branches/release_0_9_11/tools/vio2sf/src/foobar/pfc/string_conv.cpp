#include "pfc.h"


#ifdef _WINDOWS

namespace {
	template<typename t_char, bool isChecked = true> 
	class string_writer_t {
	public:
		string_writer_t(t_char * p_buffer,t_size p_size) : m_buffer(p_buffer), m_size(p_size), m_writeptr(0) {}
		
		void write(t_char p_char) {
			if (isChecked) {
				if (m_writeptr < m_size) {
					m_buffer[m_writeptr++] = p_char;
				}
			} else {
				m_buffer[m_writeptr++] = p_char;
			}
		}
		void write_multi(const t_char * p_buffer,t_size p_count) {
			if (isChecked) {
				const t_size delta = pfc::min_t<t_size>(p_count,m_size-m_writeptr);
				for(t_size n=0;n<delta;n++) {
					m_buffer[m_writeptr++] = p_buffer[n];
				}
			} else {
				for(t_size n = 0; n < p_count; ++n) {
					m_buffer[m_writeptr++] = p_buffer[n];
				}
			}
		}

		void write_as_utf8(unsigned p_char) {
			if (isChecked) {
				char temp[6];
				t_size n = pfc::utf8_encode_char(p_char,temp);
				write_multi(temp,n);
			} else {
				m_writeptr += pfc::utf8_encode_char(p_char, m_buffer + m_writeptr);
			}
		}

		void write_as_wide(unsigned p_char) {
			if (isChecked) {
				wchar_t temp[2];
				t_size n = pfc::utf16_encode_char(p_char,temp);
				write_multi(temp,n);
			} else {
				m_writeptr += pfc::utf16_encode_char(p_char, m_buffer + m_writeptr);
			}
		}

		t_size finalize() {
			if (isChecked) {
				if (m_size == 0) return 0;
				t_size terminator = pfc::min_t<t_size>(m_writeptr,m_size-1);
				m_buffer[terminator] = 0;
				return terminator;
			} else {
				m_buffer[m_writeptr] = 0;
				return m_writeptr;
			}
		}
		bool is_overrun() const {
			return m_writeptr >= m_size;
		}
	private:
		t_char * m_buffer;
		t_size m_size;
		t_size m_writeptr;
	};



};

namespace pfc {
	namespace stringcvt {


		t_size convert_utf8_to_wide(wchar_t * p_out,t_size p_out_size,const char * p_in,t_size p_in_size) {
			const t_size insize = p_in_size;
			t_size inptr = 0;
			string_writer_t<wchar_t> writer(p_out,p_out_size);

			while(inptr < insize && !writer.is_overrun()) {
				unsigned newchar = 0;
				t_size delta = utf8_decode_char(p_in + inptr,newchar,insize - inptr);
				if (delta == 0 || newchar == 0) break;
				PFC_ASSERT(inptr + delta <= insize);
				inptr += delta;
				writer.write_as_wide(newchar);
			}

			return writer.finalize();
		}

		t_size convert_utf8_to_wide_unchecked(wchar_t * p_out,const char * p_in) {
			t_size inptr = 0;
			string_writer_t<wchar_t,false> writer(p_out,~0);

			while(!writer.is_overrun()) {
				unsigned newchar = 0;
				t_size delta = utf8_decode_char(p_in + inptr,newchar);
				if (delta == 0 || newchar == 0) break;
				inptr += delta;
				writer.write_as_wide(newchar);
			}

			return writer.finalize();
		}

		t_size convert_wide_to_utf8(char * p_out,t_size p_out_size,const wchar_t * p_in,t_size p_in_size) {
			const t_size insize = p_in_size;
			t_size inptr = 0;
			string_writer_t<char> writer(p_out,p_out_size);

			while(inptr < insize && !writer.is_overrun()) {
				unsigned newchar = 0;
				t_size delta = utf16_decode_char(p_in + inptr,&newchar,insize - inptr);
				if (delta == 0 || newchar == 0) break;
				PFC_ASSERT(inptr + delta <= insize);
				inptr += delta;
				writer.write_as_utf8(newchar);
			}

			return writer.finalize();
		}

		t_size estimate_utf8_to_wide(const char * p_in) {
			t_size inptr = 0;
			t_size retval = 1;//1 for null terminator
			for(;;) {
				unsigned newchar = 0;
				t_size delta = utf8_decode_char(p_in + inptr,newchar);
				if (delta == 0 || newchar == 0) break;
				inptr += delta;
				
				{
					wchar_t temp[2];
					retval += utf16_encode_char(newchar,temp);
				}
			}
			return retval;
		}

		t_size estimate_utf8_to_wide(const char * p_in,t_size p_in_size) {
			const t_size insize = p_in_size;
			t_size inptr = 0;
			t_size retval = 1;//1 for null terminator
			while(inptr < insize) {
				unsigned newchar = 0;
				t_size delta = utf8_decode_char(p_in + inptr,newchar,insize - inptr);
				if (delta == 0 || newchar == 0) break;
				PFC_ASSERT(inptr + delta <= insize);
				inptr += delta;
				
				{
					wchar_t temp[2];
					retval += utf16_encode_char(newchar,temp);
				}
			}
			return retval;
		}

		t_size estimate_wide_to_utf8(const wchar_t * p_in,t_size p_in_size) {
			const t_size insize = p_in_size;
			t_size inptr = 0;
			t_size retval = 1;//1 for null terminator
			while(inptr < insize) {
				unsigned newchar = 0;
				t_size delta = utf16_decode_char(p_in + inptr,&newchar,insize - inptr);
				if (delta == 0 || newchar == 0) break;
				PFC_ASSERT(inptr + delta <= insize);
				inptr += delta;
				
				{
					char temp[6];
					delta = utf8_encode_char(newchar,temp);
					if (delta == 0) break;
					retval += delta;
				}
			}
			return retval;
		}


		t_size convert_codepage_to_wide(unsigned p_codepage,wchar_t * p_out,t_size p_out_size,const char * p_source,t_size p_source_size) {
			if (p_out_size == 0) return 0;
			memset(p_out,0,p_out_size * sizeof(*p_out));
			MultiByteToWideChar(p_codepage,0,p_source,p_source_size,p_out,p_out_size);
			p_out[p_out_size-1] = 0;
			return wcslen(p_out);
		}

		t_size convert_wide_to_codepage(unsigned p_codepage,char * p_out,t_size p_out_size,const wchar_t * p_source,t_size p_source_size) {
			if (p_out_size == 0) return 0;
			memset(p_out,0,p_out_size * sizeof(*p_out));
			WideCharToMultiByte(p_codepage,0,p_source,p_source_size,p_out,p_out_size,0,FALSE);
			p_out[p_out_size-1] = 0;
			return strlen(p_out);
		}

		t_size estimate_codepage_to_wide(unsigned p_codepage,const char * p_source,t_size p_source_size) {
			return MultiByteToWideChar(p_codepage,0,p_source,strlen_max(p_source,p_source_size),0,0) + 1;
		}
		t_size estimate_wide_to_codepage(unsigned p_codepage,const wchar_t * p_source,t_size p_source_size) {
			return WideCharToMultiByte(p_codepage,0,p_source,wcslen_max(p_source,p_source_size),0,0,0,FALSE) + 1;
		}
	}

}

#endif //_WINDOWS

