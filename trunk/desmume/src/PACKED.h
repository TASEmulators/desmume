#ifndef __GNUC__
#pragma pack(push, 1)
#pragma warning(disable : 4103)
#endif

#ifndef __PACKED
	#ifdef __GNUC__
	#define __PACKED __attribute__((__packed__))
	#else
	#define __PACKED
	#endif
#endif
