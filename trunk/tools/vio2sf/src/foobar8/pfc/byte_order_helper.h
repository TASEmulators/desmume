#ifndef _PFC_BYTE_ORDER_HELPER_
#define _PFC_BYTE_ORDER_HELPER_

namespace byte_order_helper {

#ifdef _M_IX86
#define PFC_BYTE_ORDER_IS_BIG_ENDIAN 0
#endif

#ifdef PFC_BYTE_ORDER_IS_BIG_ENDIAN
#define PFC_BYTE_ORDER_IS_LITTLE_ENDIAN (!(PFC_BYTE_ORDER_IS_BIG_ENDIAN))
#endif
	//if not defined, use machine_is_big_endian() to detect in runtime

#ifndef PFC_BYTE_ORDER_IS_BIG_ENDIAN
	bool machine_is_big_endian();
#else
	inline bool machine_is_big_endian() {return PFC_BYTE_ORDER_IS_BIG_ENDIAN;}
#endif

	inline bool machine_is_little_endian() {return !machine_is_big_endian();}


	void swap_order(void * ptr,unsigned bytes);
	void swap_guid(GUID & g);
	WORD swap_word(WORD);
	DWORD swap_dword(DWORD);

#ifdef PFC_BYTE_ORDER_IS_BIG_ENDIAN
#if PFC_BYTE_ORDER_IS_BIG_ENDIAN
	inline void swap_order_if_le(void * ptr,unsigned bytes) {}
	inline void swap_order_if_be(void * ptr,unsigned bytes) {swap_order(ptr,bytes);}
	inline WORD swap_word_if_le(WORD p) {return p;}
	inline WORD swap_word_if_be(WORD p) {return swap_word(p);}
	inline DWORD swap_dword_if_le(DWORD p) {return p;}
	inline DWORD swap_dword_if_be(DWORD p) {return swap_dword(p);}
#else
	inline void swap_order_if_le(void * ptr,unsigned bytes) {swap_order(ptr,bytes);}
	inline void swap_order_if_be(void * ptr,unsigned bytes) {}
	inline WORD swap_word_if_le(WORD p) {return swap_word(p);}
	inline WORD swap_word_if_be(WORD p) {return p;}
	inline DWORD swap_dword_if_le(DWORD p) {return swap_dword(p);}
	inline DWORD swap_dword_if_be(DWORD p) {return p;}
#endif
#else
	void swap_order_if_le(void * ptr,unsigned bytes);
	void swap_order_if_be(void * ptr,unsigned bytes);
	WORD swap_word_if_le(WORD p);
	WORD swap_word_if_be(WORD p);
	DWORD swap_dword_if_le(DWORD p);
	DWORD swap_dword_if_be(DWORD p);
#endif

	inline void order_be_to_native(void * ptr,unsigned bytes) {swap_order_if_le(ptr,bytes);}
	inline void order_le_to_native(void * ptr,unsigned bytes) {swap_order_if_be(ptr,bytes);}
	inline void order_native_to_be(void * ptr,unsigned bytes) {swap_order_if_le(ptr,bytes);}
	inline void order_native_to_le(void * ptr,unsigned bytes) {swap_order_if_be(ptr,bytes);}

	inline QWORD swap_qword(QWORD param) {swap_order(&param,sizeof(param));return param;}

	inline QWORD qword_be_to_native(QWORD param) {swap_order_if_le(&param,sizeof(param));return param;}
	inline QWORD qword_le_to_native(QWORD param) {swap_order_if_be(&param,sizeof(param));return param;}
	inline QWORD qword_native_to_be(QWORD param) {swap_order_if_le(&param,sizeof(param));return param;}
	inline QWORD qword_native_to_le(QWORD param) {swap_order_if_be(&param,sizeof(param));return param;}

	inline DWORD dword_be_to_native(DWORD param) {return swap_dword_if_le(param);}
	inline DWORD dword_le_to_native(DWORD param) {return swap_dword_if_be(param);}
	inline DWORD dword_native_to_be(DWORD param) {return swap_dword_if_le(param);}
	inline DWORD dword_native_to_le(DWORD param) {return swap_dword_if_be(param);}

	inline WORD word_be_to_native(WORD param) {return swap_word_if_le(param);}
	inline WORD word_le_to_native(WORD param) {return swap_word_if_be(param);}
	inline WORD word_native_to_be(WORD param) {return swap_word_if_le(param);}
	inline WORD word_native_to_le(WORD param) {return swap_word_if_be(param);}

	void guid_native_to_le(GUID &param);//big F-U to whoever wrote original GUID declaration, writing it to files depends on byte order
	void guid_native_to_be(GUID &param);
	void guid_le_to_native(GUID &param);
	void guid_be_to_native(GUID &param);
};

#endif
