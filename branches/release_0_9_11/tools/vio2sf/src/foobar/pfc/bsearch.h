namespace pfc {

	//deprecated

	class NOVTABLE bsearch_callback
	{
	public:
		virtual int test(t_size n) const = 0;
	};

	bool bsearch(t_size p_count, bsearch_callback const & p_callback,t_size & p_result);

	template<typename t_container,typename t_compare, typename t_param>
	class bsearch_callback_impl_simple_t : public bsearch_callback {
	public:
		int test(t_size p_index) const {
			return m_compare(m_container[p_index],m_param);
		}
		bsearch_callback_impl_simple_t(const t_container & p_container,t_compare p_compare,const t_param & p_param)
			: m_container(p_container), m_compare(p_compare), m_param(p_param) 
		{
		}
	private:
		const t_container & m_container;
		t_compare m_compare;
		const t_param & m_param;
	};

	template<typename t_container,typename t_compare, typename t_param,typename t_permutation>
	class bsearch_callback_impl_permutation_t : public bsearch_callback {
	public:
		int test(t_size p_index) const {
			return m_compare(m_container[m_permutation[p_index]],m_param);
		}
		bsearch_callback_impl_permutation_t(const t_container & p_container,t_compare p_compare,const t_param & p_param,const t_permutation & p_permutation)
			: m_container(p_container), m_compare(p_compare), m_param(p_param), m_permutation(p_permutation)
		{
		}
	private:
		const t_container & m_container;
		t_compare m_compare;
		const t_param & m_param;
		const t_permutation & m_permutation;
	};


	template<typename t_container,typename t_compare, typename t_param>
	bool bsearch_t(t_size p_count,const t_container & p_container,t_compare p_compare,const t_param & p_param,t_size & p_index) {
		return bsearch(
			p_count,
			bsearch_callback_impl_simple_t<t_container,t_compare,t_param>(p_container,p_compare,p_param),
			p_index);
	}

	template<typename t_container,typename t_compare,typename t_param,typename t_permutation>
	bool bsearch_permutation_t(t_size p_count,const t_container & p_container,t_compare p_compare,const t_param & p_param,const t_permutation & p_permutation,t_size & p_index) {
		t_size index;
		if (bsearch(
			p_count,
			bsearch_callback_impl_permutation_t<t_container,t_compare,t_param,t_permutation>(p_container,p_compare,p_param,p_permutation),
			index))
		{
			p_index = p_permutation[index];
			return true;
		} else {
			return false;
		}
	}

	template<typename t_container,typename t_compare, typename t_param>
	bool bsearch_range_t(const t_size p_count,const t_container & p_container,t_compare p_compare,const t_param & p_param,t_size & p_range_base,t_size & p_range_count)
	{
		t_size probe;
		if (!bsearch(
			p_count,
			bsearch_callback_impl_simple_t<t_container,t_compare,t_param>(p_container,p_compare,p_param),
			probe)) return false;

		t_size base = probe, count = 1;
		while(base > 0 && p_compare(p_container[base-1],p_param) == 0) {base--; count++;}
		while(base + count < p_count && p_compare(p_container[base+count],p_param) == 0) {count++;}
		p_range_base = base;
		p_range_count = count;
		return true;
	}

}
