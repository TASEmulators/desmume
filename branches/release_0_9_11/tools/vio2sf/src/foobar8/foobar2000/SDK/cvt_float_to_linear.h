#ifndef _CVT_FLOAT_TO_LINEAR_H_
#define _CVT_FLOAT_TO_LINEAR_H_

#include "service.h"
#include "audio_chunk.h"

//use this for converting floatingpoint -> linear PCM, one implementation in core, do not override
class NOVTABLE cvt_float_to_linear : public service_base
{
public:
	virtual int run(const audio_chunk * chunk,output_chunk * out_chunk,UINT out_bps,UINT use_dither,UINT use_pad=0)=0;
	//return 1 on success, 0 on failure (eg. unsupported bps)
	//use_pad = bps to pad to, 0 for auto

	//if you want to flush on seek or between tracks or whatever, just service_release() and recreate

	static cvt_float_to_linear * get()//helper
	{
		return service_enum_create_t(cvt_float_to_linear,0);
	}

	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}

};

/*

Notes on output data format.

Following win32 conventions, out_chunk will point to buffer containing signed integers of specified size (physically always aligned to 8bits, actual resolution might be smaller according to passed params).
EXception, if output format is 8bit, values in the buffer are unsigned (just like everywhere in win32).
If out_bps isn't a multiply of 8, it will be used as conversion resolution, but results will be padded to multiply of 8-bit (and out_chunk's bps member will be set to padded bps).
out_chunk pointers are maintained by cvt_float_to_linear object, and become invalid after releasing the object or calling run() again.
*/

#endif