namespace pfc {
	class comparator_default;

	template<typename t_comparator = comparator_default>
	class binarySearch {
	public:

		template<typename t_container,typename t_param>
		static bool run(const t_container & p_container,t_size p_base,t_size p_count,const t_param & p_param,t_size & p_result) {
			t_size max = p_base + p_count;
			t_size min = p_base;
			while(min<max) {
				t_size ptr = min + ( (max-min) >> 1);
				int state = t_comparator::compare(p_param,p_container[ptr]);
				if (state > 0) min = ptr + 1;
				else if (state < 0) max = ptr;
				else {
					p_result = ptr;
					return true;
				}
			}
			p_result = min;
			return false;
		}


		template<typename t_container,typename t_param>
		static bool runGroupBegin(const t_container & p_container,t_size p_base,t_size p_count,const t_param & p_param,t_size & p_result) {
			t_size max = p_base + p_count;
			t_size min = p_base;
			bool found = false;
			while(min<max) {
				t_size ptr = min + ( (max-min) >> 1);
				int state = t_comparator::compare(p_param,p_container[ptr]);
				if (state > 0) min = ptr + 1;
				else if (state < 0) max = ptr;
				else {
					found = true; max = ptr;
				}
			}
			p_result = min;
			return found;
		}

		template<typename t_container,typename t_param>
		static bool runGroupEnd(const t_container & p_container,t_size p_base,t_size p_count,const t_param & p_param,t_size & p_result) {
			t_size max = p_base + p_count;
			t_size min = p_base;
			bool found = false;
			while(min<max) {
				t_size ptr = min + ( (max-min) >> 1);
				int state = t_comparator::compare(p_param,p_container[ptr]);
				if (state > 0) min = ptr + 1;
				else if (state < 0) max = ptr;
				else {
					found = true; min = ptr + 1;
				}
			}
			p_result = min;
			return found;
		}
		
		template<typename t_container,typename t_param>
		static bool runGroup(const t_container & p_container,t_size p_base,t_size p_count,const t_param & p_param,t_size & p_result,t_size & p_resultCount) {
			if (!runGroupBegin(p_container,p_base,p_count,p_param,p_result)) {
				p_resultCount = 0;
				return false;
			}
			t_size groupEnd;
			if (!runGroupEnd(p_container,p_result,p_count - p_result,p_param,groupEnd)) {
				//should not happen..
				PFC_ASSERT(0);
				p_resultCount = 0;
				return false;
			}
			PFC_ASSERT(groupEnd > p_result);
			p_resultCount = groupEnd - p_result;
			return true;
		}
	};
};
