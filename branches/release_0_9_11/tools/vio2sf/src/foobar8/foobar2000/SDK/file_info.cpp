#include "foobar2000.h"
#include <stdio.h>
#include <math.h>
#include <locale.h>

void file_info::copy(const file_info * src)
{
	meta_remove_all();
	info_remove_all();
	set_location(src->get_location());
	set_length(src->get_length());
	int n;
	for(n=0;n<src->meta_get_count();n++)
		meta_add(src->meta_enum_name(n),src->meta_enum_value(n));
	for(n=0;n<src->info_get_count();n++)
		info_set(src->info_enum_name(n),src->info_enum_value(n));
}

void file_info::info_set_int(const char * name,__int64 value)
{
	assert(is_valid_utf8(name));
	char temp[32];
	_i64toa(value,temp,10);
	info_set(name,temp);
}

void file_info::info_set_float(const char * name,double value,unsigned precision,bool force_sign,const char * unit)
{
	assert(is_valid_utf8(name));
	assert(unit==0 || strlen(unit) <= 64);
	char temp[128];
	pfc_float_to_string(temp,value,precision,force_sign);
	if (unit)
	{
		strcat(temp," ");
		strcat(temp,unit);
	}
	info_set(name,temp);
}

void file_info::info_set_replaygain_track_gain(double value)
{
	info_set_float("replaygain_track_gain",value,2,true,"dB");
}

void file_info::info_set_replaygain_album_gain(double value)
{
	info_set_float("replaygain_album_gain",value,2,true,"dB");
}

void file_info::info_set_replaygain_track_peak(double value)
{
	info_set_float("replaygain_track_peak",value,6,false);
}

void file_info::info_set_replaygain_album_peak(double value)
{
	info_set_float("replaygain_album_peak",value,6,false);
}


int file_info::info_get_idx(const char * name) const
{
	assert(is_valid_utf8(name));
	int n,m=info_get_count();
	for(n=0;n<m;n++)
	{
		if (!stricmp_utf8(name,info_enum_name(n)))
			return n;
	}
	return -1;
}

int file_info::meta_get_idx(const char * name,int num) const
{
	assert(is_valid_utf8(name));
	int n,m=meta_get_count();
	for(n=0;n<m;n++)
	{
		if (!stricmp_utf8(name,meta_enum_name(n)))
		{
			if (num==0) return n;
			num--;
		}
	}
	return -1;
}

__int64 file_info::info_get_int(const char * name) const
{
	assert(is_valid_utf8(name));
	const char * val = info_get(name);
	if (val==0) return 0;
	return _atoi64(val);
}

__int64 file_info::info_get_length_samples() const
{
	__int64 ret = 0;
	double len = get_length();
	__int64 srate = info_get_int("samplerate");

	if (srate>0 && len>0)
	{
		ret = dsp_util::duration_samples_from_time(len,(unsigned)srate);
	}
	return ret;
}





void file_info::meta_add_wide(const WCHAR * name,const WCHAR* value)
{//unicode (UTF-16) version
	meta_add(string_utf8_from_wide(name),string_utf8_from_wide(value));
}

void file_info::meta_add_ansi(const char * name,const char * value)
{//ANSI version
	meta_add(string_utf8_from_ansi(name),string_utf8_from_ansi(value));
}

void file_info::meta_set_wide(const WCHAR * name,const WCHAR* value)
{//widechar version
	meta_set(string_utf8_from_wide(name),string_utf8_from_wide(value));
}

void file_info::meta_set_ansi(const char * name,const char * value)
{//ANSI version
	meta_set(string_utf8_from_ansi(name),string_utf8_from_ansi(value));
}


void file_info::meta_add_n(const char * name,int name_len,const char * value,int value_len)
{
	meta_add(string_simple(name,name_len),string_simple(value,value_len));
}

void file_info::meta_remove_field(const char * name)//removes ALL fields of given name
{
	assert(is_valid_utf8(name));
	int n;
	for(n=meta_get_count()-1;n>=0;n--)
	{
		if (!stricmp_utf8(meta_enum_name(n),name))
			meta_remove(n);
	}

}

void file_info::meta_set(const char * name,const char * value)	//deletes all fields of given name (if any), then adds new one
{
	assert(is_valid_utf8(name));
	assert(is_valid_utf8(value));
	meta_remove_field(name);
	meta_add(name,value);
}

const char * file_info::meta_get(const char * name,int num) const
{
	assert(is_valid_utf8(name));
	int idx = meta_get_idx(name,num);
	if (idx<0) return 0;
	else return meta_enum_value(idx);
}

int file_info::meta_get_count_by_name(const char* name) const
{
	assert(is_valid_utf8(name));
	int n,m=meta_get_count();
	int rv=0;
	for(n=0;n<m;n++)
	{
		if (!stricmp_utf8(name,meta_enum_name(n)))
			rv++;
	}
	return rv;
}

const char * file_info::info_get(const char * name) const
{
	assert(is_valid_utf8(name));
	int idx = info_get_idx(name);
	if (idx<0) return 0;
	else return info_enum_value(idx);
}


void file_info::info_remove_field(const char * name)
{
	assert(is_valid_utf8(name));
	int n;
	for(n=info_get_count()-1;n>=0;n--)
	{
		if (!stricmp_utf8(name,info_enum_name(n)))
			info_remove(n);
	}

}

static bool is_valid_bps(__int64 val)
{
	return val>0 && val<=256;
}

unsigned file_info::info_get_decoded_bps()
{
	__int64 val = info_get_int("decoded_bitspersample");
	if (is_valid_bps(val)) return (unsigned)val;
	val = info_get_int("bitspersample");
	if (is_valid_bps(val)) return (unsigned)val;
	return 0;
}

double file_info::info_get_float(const char * name)
{
	const char * ptr = info_get(name);
	if (ptr) return pfc_string_to_float(ptr);
	else return 0;
}