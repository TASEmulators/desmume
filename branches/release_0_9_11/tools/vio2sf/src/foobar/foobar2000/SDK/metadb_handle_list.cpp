#include "foobar2000.h"
#include <shlwapi.h>

namespace {

	wchar_t * makeSortString(const char * in) {
		wchar_t * out = new wchar_t[pfc::stringcvt::estimate_utf8_to_wide(in) + 1];
		out[0] = ' ';//StrCmpLogicalW bug workaround.
		pfc::stringcvt::convert_utf8_to_wide_unchecked(out + 1, in);
		return out;
	}

	struct custom_sort_data {
		wchar_t * text;
		t_size index;
	};
}

template<int direction>
static int custom_sort_compare(const custom_sort_data & elem1, const custom_sort_data & elem2 ) {
	int ret = direction * StrCmpLogicalW(elem1.text,elem2.text);
	if (ret == 0) ret = pfc::sgn_t((t_ssize)elem1.index - (t_ssize)elem2.index);
	return ret;
}


template<int direction>
static int _cdecl _custom_sort_compare(const void * v1, const void * v2) {
	return custom_sort_compare<direction>(*reinterpret_cast<const custom_sort_data*>(v1),*reinterpret_cast<const custom_sort_data*>(v2));
}
void metadb_handle_list_helper::sort_by_format(metadb_handle_list_ref p_list,const char * spec,titleformat_hook * p_hook)
{
	service_ptr_t<titleformat_object> script;
	if (static_api_ptr_t<titleformat_compiler>()->compile(script,spec))
		sort_by_format(p_list,script,p_hook);
}

void metadb_handle_list_helper::sort_by_format_get_order(metadb_handle_list_cref p_list,t_size* order,const char * spec,titleformat_hook * p_hook)
{
	service_ptr_t<titleformat_object> script;
	if (static_api_ptr_t<titleformat_compiler>()->compile(script,spec))
		sort_by_format_get_order(p_list,order,script,p_hook);
}

void metadb_handle_list_helper::sort_by_format(metadb_handle_list_ref p_list,const service_ptr_t<titleformat_object> & p_script,titleformat_hook * p_hook, int direction)
{
	const t_size count = p_list.get_count();
	pfc::array_t<t_size> order; order.set_size(count);
	sort_by_format_get_order(p_list,order.get_ptr(),p_script,p_hook,direction);
	p_list.reorder(order.get_ptr());
}

namespace {

	class tfhook_sort : public titleformat_hook {
	public:
		tfhook_sort() {
			m_API->seed((unsigned)__rdtsc());
		}
		bool process_field(titleformat_text_out * p_out,const char * p_name,t_size p_name_length,bool & p_found_flag) {
			return false;
		}
		bool process_function(titleformat_text_out * p_out,const char * p_name,t_size p_name_length,titleformat_hook_function_params * p_params,bool & p_found_flag) {
			if (stricmp_utf8_ex(p_name, p_name_length, "rand", ~0) == 0) {
				t_size param_count = p_params->get_param_count();
				t_uint32 val;
				if (param_count == 1) {
					t_uint32 mod = (t_uint32)p_params->get_param_uint(0);
					if (mod > 0) {
						val = m_API->genrand(mod);
					} else {
						val = 0;
					}
				} else {
					val = m_API->genrand(0xFFFFFFFF);
				}
				p_out->write_int(titleformat_inputtypes::unknown, val);
				p_found_flag = true;
				return true;
			} else {
				return false;
			}
		}
	private:
		static_api_ptr_t<genrand_service> m_API;
	};

	class tfthread : public pfc::thread {
	public:
		tfthread(pfc::counter * walk, metadb_handle_list_cref items,custom_sort_data * out,titleformat_object::ptr script,titleformat_hook * hook) : m_walk(walk), m_items(items), m_out(out), m_script(script), m_hook(hook) {}
		~tfthread() {waitTillDone();}



		void threadProc() {
			TRACK_CALL_TEXT("metadb_handle sort helper thread");

			tfhook_sort myHook;
			titleformat_hook_impl_splitter hookSplitter(&myHook, m_hook);
			titleformat_hook * const hookPtr = m_hook ? pfc::safe_cast<titleformat_hook*>(&hookSplitter) : &myHook;

			pfc::string8_fastalloc temp; temp.prealloc(512);
			const t_size total = m_items.get_size();
			for(;;) {
				const t_size index = (*m_walk)++;
				if (index >= total) break;
				m_out[index].index = index;
				m_items[index]->format_title_nonlocking(hookPtr,temp,m_script,0);
				m_out[index].text = makeSortString(temp);
			}
		}
	private:
		pfc::counter * const m_walk;
		metadb_handle_list_cref m_items;
		custom_sort_data * const m_out;
		titleformat_object::ptr const m_script;
		titleformat_hook * const m_hook;
	};
}

void metadb_handle_list_helper::sort_by_format_get_order(metadb_handle_list_cref p_list,t_size* order,const service_ptr_t<titleformat_object> & p_script,titleformat_hook * p_hook,int p_direction)
{
//	pfc::hires_timer timer; timer.start();

	const t_size count = p_list.get_count();
	pfc::array_t<custom_sort_data> data; data.set_size(count);
	
	{
		in_metadb_sync sync;
		pfc::counter counter(0);
		pfc::array_t<pfc::rcptr_t<tfthread> > threads; threads.set_size(pfc::getOptimalWorkerThreadCountEx(p_list.get_count() / 128));
		PFC_ASSERT( threads.get_size() > 0 );
		for(t_size walk = 0; walk < threads.get_size(); ++walk) {
			threads[walk].new_t(&counter,p_list,data.get_ptr(),p_script,p_hook);
		}
		for(t_size walk = 1; walk < threads.get_size(); ++walk) threads[walk]->start();
		threads[0]->threadProc();
		for(t_size walk = 1; walk < threads.get_size(); ++walk) threads[walk]->waitTillDone();
	}
//	console::formatter() << "metadb_handle sort: prepared in " << pfc::format_time_ex(timer.query(),6);

	pfc::sort_t(data, p_direction > 0 ? custom_sort_compare<1> : custom_sort_compare<-1>,count);
	//qsort(data.get_ptr(),count,sizeof(custom_sort_data),p_direction > 0 ? _custom_sort_compare<1> : _custom_sort_compare<-1>);


//	console::formatter() << "metadb_handle sort: sorted in " << pfc::format_time_ex(timer.query(),6);

	for(t_size n=0;n<count;n++)
	{
		order[n]=data[n].index;
		delete[] data[n].text;
	}

//	console::formatter() << "metadb_handle sort: finished in " << pfc::format_time_ex(timer.query(),6);
}

void metadb_handle_list_helper::sort_by_relative_path(metadb_handle_list_ref p_list)
{
	const t_size count = p_list.get_count();
	pfc::array_t<t_size> order; order.set_size(count);
	sort_by_relative_path_get_order(p_list,order.get_ptr());
	p_list.reorder(order.get_ptr());
}

void metadb_handle_list_helper::sort_by_relative_path_get_order(metadb_handle_list_cref p_list,t_size* order)
{
	const t_size count = p_list.get_count();
	t_size n;
	pfc::array_t<custom_sort_data> data;
	data.set_size(count);
	static_api_ptr_t<library_manager> api;
	
	pfc::string8_fastalloc temp;
	temp.prealloc(512);
	for(n=0;n<count;n++)
	{
		metadb_handle_ptr item;
		p_list.get_item_ex(item,n);
		if (!api->get_relative_path(item,temp)) temp = "";
		data[n].index = n;
		data[n].text = makeSortString(temp);
		//data[n].subsong = item->get_subsong_index();
	}

	pfc::sort_t(data,custom_sort_compare<1>,count);
	//qsort(data.get_ptr(),count,sizeof(custom_sort_data),(int (__cdecl *)(const void *elem1, const void *elem2 ))custom_sort_compare);

	for(n=0;n<count;n++)
	{
		order[n]=data[n].index;
		delete[] data[n].text;
	}
}

void metadb_handle_list_helper::remove_duplicates(metadb_handle_list_ref p_list)
{
	t_size count = p_list.get_count();
	if (count>0)
	{
		bit_array_bittable mask(count);
		pfc::array_t<t_size> order; order.set_size(count);
		order_helper::g_fill(order);

		p_list.sort_get_permutation_t(pfc::compare_t<metadb_handle_ptr,metadb_handle_ptr>,order.get_ptr());
		
		t_size n;
		bool found = false;
		for(n=0;n<count-1;n++)
		{
			if (p_list.get_item(order[n])==p_list.get_item(order[n+1]))
			{
				found = true;
				mask.set(order[n+1],true);
			}
		}
		
		if (found) p_list.remove_mask(mask);
	}
}

void metadb_handle_list_helper::sort_by_pointer_remove_duplicates(metadb_handle_list_ref p_list)
{
	t_size count = p_list.get_count();
	if (count>0)
	{
		sort_by_pointer(p_list);
		bool b_found = false;
		t_size n;
		for(n=0;n<count-1;n++)
		{
			if (p_list.get_item(n)==p_list.get_item(n+1))
			{
				b_found = true;
				break;
			}
		}

		if (b_found)
		{
			bit_array_bittable mask(count);
			t_size n;
			for(n=0;n<count-1;n++)
			{
				if (p_list.get_item(n)==p_list.get_item(n+1))
					mask.set(n+1,true);
			}
			p_list.remove_mask(mask);
		}
	}
}

void metadb_handle_list_helper::sort_by_path_quick(metadb_handle_list_ref p_list)
{
	p_list.sort_t(metadb::path_compare_metadb_handle);
}


void metadb_handle_list_helper::sort_by_pointer(metadb_handle_list_ref p_list)
{
	//it seems MSVC71 /GL does something highly retarded here
	//p_list.sort_t(pfc::compare_t<metadb_handle_ptr,metadb_handle_ptr>);
	p_list.sort();
}

t_size metadb_handle_list_helper::bsearch_by_pointer(metadb_handle_list_cref p_list,const metadb_handle_ptr & val)
{
	t_size blah;
	if (p_list.bsearch_t(pfc::compare_t<metadb_handle_ptr,metadb_handle_ptr>,val,blah)) return blah;
	else return ~0;
}


void metadb_handle_list_helper::sorted_by_pointer_extract_difference(metadb_handle_list const & p_list_1,metadb_handle_list const & p_list_2,metadb_handle_list & p_list_1_specific,metadb_handle_list & p_list_2_specific)
{
	t_size found_1, found_2;
	const t_size count_1 = p_list_1.get_count(), count_2 = p_list_2.get_count();
	t_size ptr_1, ptr_2;

	found_1 = found_2 = 0;
	ptr_1 = ptr_2 = 0;
	while(ptr_1 < count_1 || ptr_2 < count_2)
	{
		while(ptr_1 < count_1 && (ptr_2 == count_2 || p_list_1[ptr_1] < p_list_2[ptr_2]))
		{
			found_1++;
			t_size ptr_1_new = ptr_1 + 1;
			while(ptr_1_new < count_1 && p_list_1[ptr_1_new] == p_list_1[ptr_1]) ptr_1_new++;
			ptr_1 = ptr_1_new;
		}
		while(ptr_2 < count_2 && (ptr_1 == count_1 || p_list_2[ptr_2] < p_list_1[ptr_1]))
		{
			found_2++;
			t_size ptr_2_new = ptr_2 + 1;
			while(ptr_2_new < count_2 && p_list_2[ptr_2_new] == p_list_2[ptr_2]) ptr_2_new++;
			ptr_2 = ptr_2_new;
		}
		while(ptr_1 < count_1 && ptr_2 < count_2 && p_list_1[ptr_1] == p_list_2[ptr_2]) {ptr_1++; ptr_2++;}
	}

	

	p_list_1_specific.set_count(found_1);
	p_list_2_specific.set_count(found_2);
	if (found_1 > 0 || found_2 > 0)
	{
		found_1 = found_2 = 0;
		ptr_1 = ptr_2 = 0;

		while(ptr_1 < count_1 || ptr_2 < count_2)
		{
			while(ptr_1 < count_1 && (ptr_2 == count_2 || p_list_1[ptr_1] < p_list_2[ptr_2]))
			{
				p_list_1_specific[found_1++] = p_list_1[ptr_1];
				t_size ptr_1_new = ptr_1 + 1;
				while(ptr_1_new < count_1 && p_list_1[ptr_1_new] == p_list_1[ptr_1]) ptr_1_new++;
				ptr_1 = ptr_1_new;
			}
			while(ptr_2 < count_2 && (ptr_1 == count_1 || p_list_2[ptr_2] < p_list_1[ptr_1]))
			{
				p_list_2_specific[found_2++] = p_list_2[ptr_2];
				t_size ptr_2_new = ptr_2 + 1;
				while(ptr_2_new < count_2 && p_list_2[ptr_2_new] == p_list_2[ptr_2]) ptr_2_new++;
				ptr_2 = ptr_2_new;
			}
			while(ptr_1 < count_1 && ptr_2 < count_2 && p_list_1[ptr_1] == p_list_2[ptr_2]) {ptr_1++; ptr_2++;}
		}

	}
}

double metadb_handle_list_helper::calc_total_duration(metadb_handle_list_cref p_list)
{
	double ret = 0;
	t_size n, m = p_list.get_count();
	for(n=0;n<m;n++)
	{
		double temp = p_list.get_item(n)->get_length();
		if (temp > 0) ret += temp;
	}
	return ret;
}

void metadb_handle_list_helper::sort_by_path(metadb_handle_list_ref p_list)
{
	sort_by_format(p_list,"%path_sort%",NULL);
}


t_filesize metadb_handle_list_helper::calc_total_size(metadb_handle_list_cref p_list, bool skipUnknown) {
	metadb_handle_list list(p_list);
	list.sort_t(metadb::path_compare_metadb_handle);

	t_filesize ret = 0;
	t_size n, m = list.get_count();
	for(n=0;n<m;n++) {
		if (n==0 || metadb::path_compare(list[n-1]->get_path(),list[n]->get_path())) {
			t_filesize t = list[n]->get_filesize();
			if (t == filesize_invalid) {
				if (!skipUnknown) return filesize_invalid;
			} else {
				ret += t;
			}
		}
	}
	return ret;
}

t_filesize metadb_handle_list_helper::calc_total_size_ex(metadb_handle_list_cref p_list, bool & foundUnknown) {
	foundUnknown = false;
	metadb_handle_list list(p_list);
	list.sort_t(metadb::path_compare_metadb_handle);

	t_filesize ret = 0;
	t_size n, m = list.get_count();
	for(n=0;n<m;n++) {
		if (n==0 || metadb::path_compare(list[n-1]->get_path(),list[n]->get_path())) {
			t_filesize t = list[n]->get_filesize();
			if (t == filesize_invalid) {
				foundUnknown = true;
			} else {
				ret += t;
			}
		}
	}
	return ret;
}
