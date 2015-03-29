class dynamic_bitrate_helper
{
public:
	dynamic_bitrate_helper();
	void on_frame(double p_duration,t_size p_bits);
	bool on_update(file_info & p_out, double & p_timestamp_delta);
	void reset();
private:
	void init();
	double m_last_duration;
	t_size m_update_bits;
	double m_update_time;
	double m_update_interval;
	bool m_inited, m_enabled;
};