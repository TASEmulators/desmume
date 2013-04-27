#include "stdafx.h"

static unsigned g_query_settings()
{
	t_int32 temp;
	try {
		config_object::g_get_data_int32(standard_config_objects::int32_dynamic_bitrate_display_rate,temp);
	} catch(std::exception const &) {return 9;}
	if (temp < 0) return 0;
	return (unsigned) temp;
}

dynamic_bitrate_helper::dynamic_bitrate_helper()
{
	reset();
}

void dynamic_bitrate_helper::init()
{
	if (!m_inited)
	{
		m_inited = true;
		unsigned temp = g_query_settings();
		if (temp > 0) {m_enabled = true; m_update_interval = 1.0 / (double) temp; }
		else {m_enabled = false; m_update_interval = 0; }
		m_last_duration = 0;
		m_update_bits = 0;
		m_update_time = 0;

	}
}

void dynamic_bitrate_helper::on_frame(double p_duration,t_size p_bits)
{
	init();
	m_last_duration = p_duration;
	m_update_time += p_duration;
	m_update_bits += p_bits;
}

bool dynamic_bitrate_helper::on_update(file_info & p_out, double & p_timestamp_delta)
{
	init();

	bool ret = false;
	if (m_enabled)
	{
		if (m_update_time > m_update_interval)
		{
			int val = (int) ( ((double)m_update_bits / m_update_time + 500.0) / 1000.0 );
			if (val != p_out.info_get_bitrate_vbr())
			{
				p_timestamp_delta = - (m_update_time - m_last_duration);	//relative to last frame beginning;
				p_out.info_set_bitrate_vbr(val);
				ret = true;
			}
			m_update_bits = 0;
			m_update_time = 0;
		}
	}
	else
	{
		m_update_bits = 0;
		m_update_time = 0;
	}

	return ret;

}

void dynamic_bitrate_helper::reset()
{
	m_inited = false;
}
