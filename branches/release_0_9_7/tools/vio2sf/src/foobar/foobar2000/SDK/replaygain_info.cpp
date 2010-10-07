#include "foobar2000.h"

bool replaygain_info::g_format_gain(float p_value,char p_buffer[text_buffer_size])
{
	fpu_control_roundnearest bah;
	if (p_value == gain_invalid)
	{
		p_buffer[0] = 0;
		return false;
	}
	else
	{
		pfc::float_to_string(p_buffer,text_buffer_size - 4,p_value,2,true);
		strcat(p_buffer," dB");
		return true;
	}
}

bool replaygain_info::g_format_peak(float p_value,char p_buffer[text_buffer_size])
{
	fpu_control_roundnearest bah;
	if (p_value == peak_invalid)
	{
		p_buffer[0] = 0;
		return false;
	}
	else
	{
		pfc::float_to_string(p_buffer,text_buffer_size,p_value,6,false);
		return true;
	}
}

void replaygain_info::reset()
{
	m_album_gain = gain_invalid;
	m_track_gain = gain_invalid;
	m_album_peak = peak_invalid;
	m_track_peak = peak_invalid;
}

static const char meta_album_gain[] = "replaygain_album_gain", meta_album_peak[] = "replaygain_album_peak", meta_track_gain[] = "replaygain_track_gain", meta_track_peak[] = "replaygain_track_peak";

bool replaygain_info::g_is_meta_replaygain(const char * p_name,t_size p_name_len)
{
	return 
		stricmp_utf8_ex(p_name,p_name_len,meta_album_gain,infinite) == 0 ||
		stricmp_utf8_ex(p_name,p_name_len,meta_album_peak,infinite) == 0 ||
		stricmp_utf8_ex(p_name,p_name_len,meta_track_gain,infinite) == 0 ||
		stricmp_utf8_ex(p_name,p_name_len,meta_track_peak,infinite) == 0;
}

bool replaygain_info::set_from_meta_ex(const char * p_name,t_size p_name_len,const char * p_value,t_size p_value_len)
{
	fpu_control_roundnearest bah;
	if (stricmp_utf8_ex(p_name,p_name_len,meta_album_gain,infinite) == 0)
	{
		m_album_gain = (float)pfc::string_to_float(p_value,p_value_len);
		return true;
	}
	else if (stricmp_utf8_ex(p_name,p_name_len,meta_album_peak,infinite) == 0)
	{
		m_album_peak = (float)pfc::string_to_float(p_value,p_value_len);
		if (m_album_peak < 0) m_album_peak = 0;
		return true;
	}
	else if (stricmp_utf8_ex(p_name,p_name_len,meta_track_gain,infinite) == 0)
	{
		m_track_gain = (float)pfc::string_to_float(p_value,p_value_len);
		return true;
	}
	else if (stricmp_utf8_ex(p_name,p_name_len,meta_track_peak,infinite) == 0)
	{
		m_track_peak = (float)pfc::string_to_float(p_value,p_value_len);
		if (m_track_peak < 0) m_track_peak = 0;
		return true;
	}
	else return false;
}


t_size replaygain_info::get_value_count()
{
	t_size ret = 0;
	if (is_album_gain_present()) ret++;
	if (is_album_peak_present()) ret++;
	if (is_track_gain_present()) ret++;
	if (is_track_peak_present()) ret++;
	return ret;
}

void replaygain_info::set_album_gain_text(const char * p_text,t_size p_text_len)
{
	fpu_control_roundnearest bah;
	if (p_text != 0 && p_text_len > 0 && *p_text != 0)
		m_album_gain = (float)pfc::string_to_float(p_text,p_text_len);
	else
		remove_album_gain();
}

void replaygain_info::set_track_gain_text(const char * p_text,t_size p_text_len)
{
	fpu_control_roundnearest bah;
	if (p_text != 0 && p_text_len > 0 && *p_text != 0)
		m_track_gain = (float)pfc::string_to_float(p_text,p_text_len);
	else
		remove_track_gain();
}

void replaygain_info::set_album_peak_text(const char * p_text,t_size p_text_len)
{
	fpu_control_roundnearest bah;
	if (p_text != 0 && p_text_len > 0 && *p_text != 0)
		m_album_peak = (float)pfc::string_to_float(p_text,p_text_len);
	else
		remove_album_peak();
}

void replaygain_info::set_track_peak_text(const char * p_text,t_size p_text_len)
{
	fpu_control_roundnearest bah;
	if (p_text != 0 && p_text_len > 0 && *p_text != 0)
		m_track_peak = (float)pfc::string_to_float(p_text,p_text_len);
	else
		remove_track_peak();
}

replaygain_info replaygain_info::g_merge(replaygain_info r1,replaygain_info r2)
{
	replaygain_info ret = r1;
	if (!ret.is_album_gain_present()) ret.m_album_gain = r2.m_album_gain;
	if (!ret.is_album_peak_present()) ret.m_album_peak = r2.m_album_peak;
	if (!ret.is_track_gain_present()) ret.m_track_gain = r2.m_track_gain;
	if (!ret.is_track_peak_present()) ret.m_track_peak = r2.m_track_peak;
	return ret;
}
