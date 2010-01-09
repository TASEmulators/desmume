namespace pfc {

	//deprecated

template<typename t_callback>
inline bool bsearch_inline_t(t_size p_count, const t_callback & p_callback,t_size & p_result)
{
	t_size max = p_count;
	t_size min = 0;
	t_size ptr;
	while(min<max)
	{
		ptr = min + ( (max-min) >> 1);
		int result = p_callback.test(ptr);
		if (result<0) min = ptr + 1;
		else if (result>0) max = ptr;
		else 
		{
			p_result = ptr;
			return true;
		}
	}
	p_result = min;
	return false;
}

template<typename t_buffer,typename t_value>
inline bool bsearch_simple_inline_t(const t_buffer & p_buffer,t_size p_count,t_value const & p_value,t_size & p_result)
{
	t_size max = p_count;
	t_size min = 0;
	t_size ptr;
	while(min<max)
	{
		ptr = min + ( (max-min) >> 1);
		if (p_value > p_buffer[ptr]) min = ptr + 1;
		else if (p_value < p_buffer[ptr]) max = ptr;
		else 
		{
			p_result = ptr;
			return true;
		}
	}
	p_result = min;
	return false;
}

}
