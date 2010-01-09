#include "stdafx.h"

static bool is_numeric(char c) {return c>='0' && c<='9';}


bool t_cuesheet_index_list::is_valid() const {
	if (m_positions[1] < m_positions[0]) return false;
	for(t_size n = 2; n < count && m_positions[n] > 0; n++) {
		if (m_positions[n] < m_positions[n-1]) return false;
	}
	return true;
}

void t_cuesheet_index_list::to_infos(file_info & p_out) const
{
	double base = m_positions[1];

	if (base > 0) {
		p_out.info_set("referenced_offset",cuesheet_format_index_time(base));
	}
	
	if (m_positions[0] < base)
		p_out.info_set("pregap",cuesheet_format_index_time(base - m_positions[0]));
	else
		p_out.info_remove("pregap");

	p_out.info_remove("index 00");	
	p_out.info_remove("index 01");
	
	for(unsigned n=2;n<count;n++)
	{
		char namebuffer[16];
		sprintf_s(namebuffer,"index %02u",n);
		double position = m_positions[n] - base;
		if (position > 0)
			p_out.info_set(namebuffer,cuesheet_format_index_time(position));
		else
			p_out.info_remove(namebuffer);
	}
}

static bool parse_value(const char * p_name,double & p_out)
{
	if (p_name == NULL) return false;
	try {
		p_out = cuesheet_parse_index_time_e(p_name,strlen(p_name));
	} catch(std::exception const &) {return false;}
	return true;
}

bool t_cuesheet_index_list::from_infos(file_info const & p_in,double p_base)
{
	double pregap;
	bool found = false;
	if (!parse_value(p_in.info_get("pregap"),pregap)) pregap = 0;
	else found = true;
	m_positions[0] = p_base - pregap;
	m_positions[1] = p_base;
	for(unsigned n=2;n<count;n++)
	{
		char namebuffer[16];
		sprintf_s(namebuffer,"index %02u",n);
		double temp;
		if (parse_value(p_in.info_get(namebuffer),temp)) {
			m_positions[n] = temp + p_base; found = true;
		} else {
			m_positions[n] = 0;
		}
	}
	return found;	
}

cuesheet_format_index_time::cuesheet_format_index_time(double p_time)
{
	t_uint64 ticks = audio_math::time_to_samples(p_time,75);
	t_uint64 seconds = ticks / 75; ticks %= 75;
	t_uint64 minutes = seconds / 60; seconds %= 60;
	m_buffer << pfc::format_uint(minutes,2) << ":" << pfc::format_uint(seconds,2) << ":" << pfc::format_uint(ticks,2);
}

double cuesheet_parse_index_time_e(const char * p_string,t_size p_length)
{
	return (double) cuesheet_parse_index_time_ticks_e(p_string,p_length) / 75.0;
}

unsigned cuesheet_parse_index_time_ticks_e(const char * p_string,t_size p_length)
{
	p_length = pfc::strlen_max(p_string,p_length);
	t_size ptr = 0;
	t_size splitmarks[2];
	t_size splitptr = 0;
	for(ptr=0;ptr<p_length;ptr++)
	{
		if (p_string[ptr] == ':')
		{
			if (splitptr >= 2) throw std::exception("invalid INDEX time syntax",0);
			splitmarks[splitptr++] = ptr;
		}
		else if (!is_numeric(p_string[ptr])) throw std::exception("invalid INDEX time syntax",0);
	}
	
	t_size minutes_base = 0, minutes_length = 0, seconds_base = 0, seconds_length = 0, frames_base = 0, frames_length = 0;

	switch(splitptr)
	{
	case 0:
		frames_base = 0;
		frames_length = p_length;
		break;
	case 1:
		seconds_base = 0;
		seconds_length = splitmarks[0];
		frames_base = splitmarks[0] + 1;
		frames_length = p_length - frames_base;
		break;
	case 2:
		minutes_base = 0;
		minutes_length = splitmarks[0];
		seconds_base = splitmarks[0] + 1;
		seconds_length = splitmarks[1] - seconds_base;
		frames_base = splitmarks[1] + 1;
		frames_length = p_length - frames_base;
		break;
	}

	unsigned ret = 0;

	if (frames_length > 0) ret += pfc::atoui_ex(p_string + frames_base,frames_length);
	if (seconds_length > 0) ret += 75 * pfc::atoui_ex(p_string + seconds_base,seconds_length);
	if (minutes_length > 0) ret += 60 * 75 * pfc::atoui_ex(p_string + minutes_base,minutes_length);

	return ret;	
}


bool t_cuesheet_index_list::is_empty() const {
	for(unsigned n=0;n<count;n++) if (m_positions[n] != m_positions[1]) return false;
	return true;
}