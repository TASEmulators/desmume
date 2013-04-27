#ifndef _PFC_GUID_H_
#define _PFC_GUID_H_

namespace pfc {

	class GUID_from_text : public GUID
	{
		unsigned read_hex(char c);
		unsigned read_byte(const char * ptr);
		unsigned read_word(const char * ptr);
		unsigned read_dword(const char * ptr);
		void read_bytes(unsigned char * out,unsigned num,const char * ptr);

	public:
		GUID_from_text(const char * text);
	};

	class print_guid
	{
	public:
		print_guid(const GUID & p_guid);
		inline operator const char * () const {return m_data;}
		inline const char * get_ptr() {return m_data;}
	private:
		char m_data[64];
	};

	inline int guid_compare(const GUID & g1,const GUID & g2) {return memcmp(&g1,&g2,sizeof(GUID));}

	inline bool guid_equal(const GUID & g1,const GUID & g2) {return (g1 == g2) ? true : false;}
	template<> inline int compare_t<GUID,GUID>(const GUID & p_item1,const GUID & p_item2) {return guid_compare(p_item1,p_item2);}

	extern const GUID guid_null;

	void print_hex_raw(const void * buffer,unsigned bytes,char * p_out);

	static GUID makeGUID(t_uint32 Data1, t_uint16 Data2, t_uint16 Data3, t_uint8 Data4_1, t_uint8 Data4_2, t_uint8 Data4_3, t_uint8 Data4_4, t_uint8 Data4_5, t_uint8 Data4_6, t_uint8 Data4_7, t_uint8 Data4_8) {
		GUID guid = { Data1, Data2, Data3, {Data4_1, Data4_2, Data4_3, Data4_4, Data4_5, Data4_6, Data4_7, Data4_8 } };
		return guid;
	}
	static GUID xorGUID(const GUID & v1, const GUID & v2) {
		GUID temp; memxor(&temp, &v1, &v1, sizeof(GUID)); return temp;
	}
}


#endif
