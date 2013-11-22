#ifndef _PFC_BYTE_ORDER_HELPER_
#define _PFC_BYTE_ORDER_HELPER_

namespace pfc {
	void byteswap_raw(void * p_buffer,t_size p_bytes);

	template<typename T> T byteswap_t(T p_source);

	template<> inline char byteswap_t<char>(char p_source) {return p_source;}
	template<> inline unsigned char byteswap_t<unsigned char>(unsigned char p_source) {return p_source;}

#ifdef _MSC_VER//does this even help with performance/size?
	template<> inline wchar_t byteswap_t<wchar_t>(wchar_t p_source) {return _byteswap_ushort(p_source);}

	template<> inline short byteswap_t<short>(short p_source) {return _byteswap_ushort(p_source);}
	template<> inline unsigned short byteswap_t<unsigned short>(unsigned short p_source) {return _byteswap_ushort(p_source);}

	template<> inline int byteswap_t<int>(int p_source) {return _byteswap_ulong(p_source);}
	template<> inline unsigned int byteswap_t<unsigned int>(unsigned int p_source) {return _byteswap_ulong(p_source);}

	template<> inline long byteswap_t<long>(long p_source) {return _byteswap_ulong(p_source);}
	template<> inline unsigned long byteswap_t<unsigned long>(unsigned long p_source) {return _byteswap_ulong(p_source);}

	template<> inline long long byteswap_t<long long>(long long p_source) {return _byteswap_uint64(p_source);}
	template<> inline unsigned long long byteswap_t<unsigned long long>(unsigned long long p_source) {return _byteswap_uint64(p_source);}
#else
	template<> inline t_uint16 byteswap_t<t_uint16>(t_uint16 p_source) {return ((p_source & 0xFF00) >> 8) | ((p_source & 0x00FF) << 8);}
	template<> inline t_int16 byteswap_t<t_int16>(t_int16 p_source) {return byteswap_t<t_uint16>(p_source);}

	template<> inline t_uint32 byteswap_t<t_uint32>(t_uint32 p_source) {return ((p_source & 0xFF000000) >> 24) | ((p_source & 0x00FF0000) >> 8) | ((p_source & 0x0000FF00) << 8) | ((p_source & 0x000000FF) << 24);}
	template<> inline t_int32 byteswap_t<t_int32>(t_int32 p_source) {return byteswap_t<t_uint32>(p_source);}

	template<> inline t_uint64 byteswap_t<t_uint64>(t_uint64 p_source) {
		//optimizeme
		byteswap_raw(&p_source,sizeof(p_source));
		return p_source;		
	}
	template<> inline t_int64 byteswap_t<t_int64>(t_int64 p_source) {return byteswap_t<t_uint64>(p_source);}

	template<> inline wchar_t byteswap_t<wchar_t>(wchar_t p_source) {
		return byteswap_t<pfc::sized_int_t<sizeof(wchar_t)>::t_unsigned>(p_source);
	}
#endif

	template<> inline float byteswap_t<float>(float p_source) {
		float ret;
		*(t_uint32*) &ret = byteswap_t(*(const t_uint32*)&p_source );
		return ret;
	}

	template<> inline double byteswap_t<double>(double p_source) {
		double ret;
		*(t_uint64*) &ret = byteswap_t(*(const t_uint64*)&p_source );
		return ret;
	}

	//blargh at GUID byteswap issue
	template<> inline GUID byteswap_t<GUID>(GUID p_guid) {
		GUID ret;
		ret.Data1 = pfc::byteswap_t(p_guid.Data1);
		ret.Data2 = pfc::byteswap_t(p_guid.Data2);
		ret.Data3 = pfc::byteswap_t(p_guid.Data3);
		ret.Data4[0] = p_guid.Data4[0];
		ret.Data4[1] = p_guid.Data4[1];
		ret.Data4[2] = p_guid.Data4[2];
		ret.Data4[3] = p_guid.Data4[3];
		ret.Data4[4] = p_guid.Data4[4];
		ret.Data4[5] = p_guid.Data4[5];
		ret.Data4[6] = p_guid.Data4[6];
		ret.Data4[7] = p_guid.Data4[7];
		return ret;
	}


	
};

#ifdef _MSC_VER

#if defined(_M_IX86) || defined(_M_X64)
#define PFC_BYTE_ORDER_IS_BIG_ENDIAN 0
#endif

#else//_MSC_VER

#include <endian.h>
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define PFC_BYTE_ORDER_IS_BIG_ENDIAN 0
#else
#define PFC_BYTE_ORDER_IS_BIG_ENDIAN 1
#endif

#endif//_MSC_VER

#ifdef PFC_BYTE_ORDER_IS_BIG_ENDIAN
#define PFC_BYTE_ORDER_IS_LITTLE_ENDIAN (!(PFC_BYTE_ORDER_IS_BIG_ENDIAN))
#else
#error please update byte order #defines
#endif


namespace pfc {
	enum {
		byte_order_is_big_endian = PFC_BYTE_ORDER_IS_BIG_ENDIAN,
		byte_order_is_little_endian = PFC_BYTE_ORDER_IS_LITTLE_ENDIAN,
	};

	template<typename T> T byteswap_if_be_t(T p_param) {return byte_order_is_big_endian ? byteswap_t(p_param) : p_param;}
	template<typename T> T byteswap_if_le_t(T p_param) {return byte_order_is_little_endian ? byteswap_t(p_param) : p_param;}
}

namespace byte_order {

#if PFC_BYTE_ORDER_IS_BIG_ENDIAN//big endian
	template<typename T> inline void order_native_to_le_t(T& param) {param = pfc::byteswap_t(param);}
	template<typename T> inline void order_native_to_be_t(T& param) {}
	template<typename T> inline void order_le_to_native_t(T& param) {param = pfc::byteswap_t(param);}
	template<typename T> inline void order_be_to_native_t(T& param) {}
#else//little endian
	template<typename T> inline void order_native_to_le_t(T& param) {}
	template<typename T> inline void order_native_to_be_t(T& param) {param = pfc::byteswap_t(param);}
	template<typename T> inline void order_le_to_native_t(T& param) {}
	template<typename T> inline void order_be_to_native_t(T& param) {param = pfc::byteswap_t(param);}
#endif
};



namespace pfc {
	template<typename TInt,unsigned width,bool IsBigEndian>
	class __EncodeIntHelper {
	public:
		inline static void Run(TInt p_value,t_uint8 * p_out) {
			*p_out = (t_uint8)(p_value);
			__EncodeIntHelper<TInt,width-1,IsBigEndian>::Run(p_value >> 8,p_out + (IsBigEndian ? -1 : 1));
		}
	};

	template<typename TInt,bool IsBigEndian>
	class __EncodeIntHelper<TInt,1,IsBigEndian> {
	public:
		inline static void Run(TInt p_value,t_uint8* p_out) {
			*p_out = (t_uint8)(p_value);
		}
	};
	template<typename TInt,bool IsBigEndian>
	class __EncodeIntHelper<TInt,0,IsBigEndian> {
	public:
		inline static void Run(TInt,t_uint8*) {}
	};

	template<typename TInt>
	inline void encode_little_endian(t_uint8 * p_buffer,TInt p_value) {
		__EncodeIntHelper<TInt,sizeof(TInt),false>::Run(p_value,p_buffer);
	}
	template<typename TInt>
	inline void encode_big_endian(t_uint8 * p_buffer,TInt p_value) {
		__EncodeIntHelper<TInt,sizeof(TInt),true>::Run(p_value,p_buffer + (sizeof(TInt) - 1));
	}


	template<typename TInt,unsigned width,bool IsBigEndian>
	class __DecodeIntHelper {
	public:
		inline static TInt Run(const t_uint8 * p_in) {
			return (__DecodeIntHelper<TInt,width-1,IsBigEndian>::Run(p_in + (IsBigEndian ? -1 : 1)) << 8) + *p_in;
		}
	};
	
	template<typename TInt,bool IsBigEndian>
	class __DecodeIntHelper<TInt,1,IsBigEndian> {
	public:
		inline static TInt Run(const t_uint8* p_in) {return *p_in;}
	};

	template<typename TInt,bool IsBigEndian>
	class __DecodeIntHelper<TInt,0,IsBigEndian> {
	public:
		inline static TInt Run(const t_uint8*) {return 0;}
	};

	template<typename TInt>
	inline void decode_little_endian(TInt & p_out,const t_uint8 * p_buffer) {
		p_out = __DecodeIntHelper<TInt,sizeof(TInt),false>::Run(p_buffer);
	}

	template<typename TInt>
	inline void decode_big_endian(TInt & p_out,const t_uint8 * p_buffer) {
		p_out = __DecodeIntHelper<TInt,sizeof(TInt),true>::Run(p_buffer + (sizeof(TInt) - 1));
	}

	template<typename TInt>
	inline TInt decode_little_endian(const t_uint8 * p_buffer) {
		TInt temp;
		decode_little_endian(temp,p_buffer);
		return temp;
	}

	template<typename TInt>
	inline TInt decode_big_endian(const t_uint8 * p_buffer) {
		TInt temp;
		decode_big_endian(temp,p_buffer);
		return temp;
	}

	template<bool IsBigEndian,typename TInt>
	inline void decode_endian(TInt & p_out,const t_uint8 * p_buffer) {
		if (IsBigEndian) decode_big_endian(p_out,p_buffer);
		else decode_little_endian(p_out,p_buffer);
	}
	template<bool IsBigEndian,typename TInt>
	inline void encode_endian(t_uint8 * p_buffer,TInt p_in) {
		if (IsBigEndian) encode_big_endian(p_in,p_value);
		else encode_little_endian(p_in,p_value);
	}



	template<unsigned width>
	inline static void reverse_bytes(t_uint8 * p_buffer) {
		pfc::swap_t(p_buffer[0],p_buffer[width-1]);
		reverse_bytes<width-2>(p_buffer+1);
	}

	template<> inline static void reverse_bytes<1>(t_uint8 * p_buffer) { }
	template<> inline static void reverse_bytes<0>(t_uint8 * p_buffer) { }

}



#endif
