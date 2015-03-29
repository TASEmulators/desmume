struct t_cuesheet_index_list
{
	enum {count = 100};
	inline t_cuesheet_index_list() {reset();}
	inline t_cuesheet_index_list(const t_cuesheet_index_list & p_source) {*this=p_source;}
	void reset() {for(unsigned n=0;n<count;n++) m_positions[n]=0;}

	void to_infos(file_info & p_out) const;
	
	//returns false when there was nothing relevant in infos
	bool from_infos(file_info const & p_in,double p_base);

	double m_positions[count];

	inline double start() const {return m_positions[1];}
	bool is_empty() const;

	bool is_valid() const;
};

unsigned cuesheet_parse_index_time_ticks_e(const char * p_string,t_size p_length);
double cuesheet_parse_index_time_e(const char * p_string,t_size p_length);

class cuesheet_format_index_time
{
public:
	cuesheet_format_index_time(double p_time);
	inline operator const char*() const {return m_buffer;}
private:
	pfc::string_formatter m_buffer;
};
