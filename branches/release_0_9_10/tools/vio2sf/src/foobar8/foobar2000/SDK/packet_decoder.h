#ifndef _FOOBAR2000_PACKET_DECODER_H_
#define _FOOBAR2000_PACKET_DECODER_H_

#include "service.h"
#include "audio_chunk.h"

class packet_decoder : public service_base
{
public:
	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}

	virtual bool open_stream(const GUID & owner,unsigned param1,const void * param2,unsigned param2size,file_info * info)=0;//owner - container GUID, param1/2 - contents depend on owner
	virtual unsigned set_stream_property(const GUID & type,unsigned param1,const void * param2,unsigned param2size,file_info * info) {return 0;}//optional, return values depend on guid, return 0 by default

	virtual unsigned get_max_frame_dependency()=0;
	virtual double get_max_frame_dependency_time()=0;

	virtual void reset_after_seek()=0;
	virtual bool decode(const void * buffer,unsigned bytes,audio_chunk * out)=0;//may return empty chunk (decoder delay etc), caller must check for it (and not crash on samplerate==0 etc)
	virtual const char * get_name()=0;//for diagnostic purposes, codec list, etc

	static packet_decoder * g_open(const GUID & owner,unsigned param1,const void * param2,unsigned param2size,file_info * info);

	static const GUID owner_MP4,owner_matroska,owner_MP3;

	struct matroska_setup
	{
		const char * codec_id;
		unsigned sample_rate,sample_rate_output;
		unsigned channels;
		unsigned codec_private_size;
		const void * codec_private;
	};
	//owner_MP4: param1 - codec ID (MP4 audio type), param2 - MP4 codec initialization data
	//owner_MP3: raw MP3/MP2 file, parameters ignored
	//owner_matroska: param2 = matroska_setup struct, param2size size must be equal to sizeof(matroska_setup)


	//these are used to initialize PCM decoder
	static const GUID property_samplerate,property_bitspersample,property_channels,property_byteorder,property_signed;
	//property_samplerate : param1 == sample rate in hz
	//property_bitspersample : param1 == bits per sample
	//property_channels : param1 == channel count
	//property_byteorder : if (param1) little_endian; else big_endian;
	//property_signed : if (param1) signed; else unsigned;
};


template<class T>
class packet_decoder_factory : public service_factory_t<packet_decoder,T> {};


#endif //_FOOBAR2000_PACKET_DECODER_H_