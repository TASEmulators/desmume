#include "foobar2000.h"

namespace {
	struct custom_sort_data
	{
		HANDLE text;
		int index;
		custom_sort_data(const char * p_text,int p_idx)
			: text(uSortStringCreate(p_text)), index(p_idx)
		{}
		~custom_sort_data() {uSortStringFree(text);}
	};
}
static int __cdecl custom_sort_compare(const custom_sort_data * &elem1, const custom_sort_data *&elem2 )
{//depends on unicode/ansi shit, nonportable (win32 lstrcmpi)
	return uSortStringCompare(elem1->text,elem2->text);//uStringCompare
}


void metadb_handle_list::sort_by_format_get_order(int * order,const char * spec,const char * extra_items) const
{
	ptr_list_simple<custom_sort_data> data;
	unsigned n;
	string8 temp;
	string8 temp2;
	temp.set_mem_logic(mem_block::ALLOC_FAST_DONTGODOWN);
	temp.prealloc(512);
	temp2.set_mem_logic(mem_block::ALLOC_FAST_DONTGODOWN);
	for(n=0;n<get_count();n++)
	{
		get_item(n)->handle_format_title(temp,spec,extra_items);
		const char * ptr = temp.get_ptr();
		if (strchr(ptr,3))
		{
			titleformat::remove_color_marks(ptr,temp2);
			ptr = temp2;
		}
		data.add_item(new custom_sort_data(ptr,n));
	}

	data.sort(custom_sort_compare);

	for(n=0;n<data.get_count();n++)
	{
		order[n]=data[n]->index;
	}

	data.delete_all();
}

void metadb_handle_list::sort_by_format(const char * spec,const char * extra_items)
{
	unsigned count = get_count();
	mem_block_t<int> order(count);
	sort_by_format_get_order(order,spec,extra_items);
	apply_order(order,count);
}

bool metadb_handle::handle_query_meta_field(const char * name,int num,string_base & out)
{
	bool rv = false;
	handle_lock();
	const file_info * info = handle_query_locked();
	if (info)
	{
		const char * val = info->meta_get(name,num);
		if (val)
		{
			out = val;
			rv = true;
		}
	}
	handle_unlock();
	return rv;
}

int metadb_handle::handle_query_meta_field_int(const char * name,int num)//returns 0 if not found
{
	int rv = 0;
	handle_lock();
	const file_info * info = handle_query_locked();
	if (info)
	{
		const char * val = info->meta_get(name,num);
		if (val)
		{
			rv = atoi(val);
		}
	}
	handle_unlock();
	return rv;
}

double metadb_handle::handle_get_length()
{
	double rv = 0;
	handle_lock();
	const file_info * info = handle_query_locked();
	if (info)
		rv = info->get_length();
	handle_unlock();
	return rv;
}

__int64 metadb_handle::handle_get_length_samples()
{
	__int64 rv = 0;
	handle_lock();
	const file_info * info = handle_query_locked();
	if (info)
		rv = info->info_get_length_samples();
	handle_unlock();
	return rv;
}


void metadb_handle_list::delete_item(metadb_handle * ptr)
{
	remove_item(ptr);
	ptr->handle_release();
} 

void metadb_handle_list::delete_by_idx(int idx)
{
	metadb_handle * ptr = remove_by_idx(idx);
	if (ptr) ptr->handle_release();
}

void metadb_handle_list::delete_all()
{
	if (get_count()>0)
	{
		metadb::get()->handle_release_multi(get_ptr(),get_count());
		remove_all();
	}
}

void metadb_handle_list::add_ref_all()
{
	if (get_count()>0) metadb::get()->handle_add_ref_multi(get_ptr(),get_count());
}

void metadb_handle_list::delete_mask(const bit_array & mask)
{
	unsigned n,m=get_count();
	for_each_bit_array(n,mask,true,0,m)
		get_item(n)->handle_release();
	remove_mask(mask);
}

void metadb_handle_list::filter_mask_del(const bit_array & mask)
{
	unsigned n,m=get_count();
	for_each_bit_array(n,mask,false,0,m)
		get_item(n)->handle_release();
	filter_mask(mask);	
}

void metadb_handle_list::remove_duplicates(bool b_release)
{
	unsigned count = get_count();
	if (count>0)
	{
		bit_array_bittable mask(count);
		mem_block_t<int> order(count);

		sort_by_format_get_order(order,"%_path_raw%|$num(%_subsong%,9)",0);

		unsigned n;
		for(n=0;n<count-1;n++)
		{
			if (get_item(order[n])==get_item(order[n+1]))
			{
				mask.set(order[n+1],true);
			}
		}

		if (b_release) delete_mask(mask);
		else remove_mask(mask);
	}
}

static int remove_duplicates_quick_compare(const metadb_handle * &val1,const metadb_handle * &val2)
{
	if (val1<val2) return -1;
	else if (val1>val2) return 1;
	else return 0;
}

void metadb_handle_list::remove_duplicates_quick(bool b_release)
{
	unsigned count = get_count();
	if (count>0)
	{
		sort(remove_duplicates_quick_compare);
		bit_array_bittable mask(count);
		unsigned n;
		bool b_found = false;
		for(n=0;n<count-1;n++)
		{
			if (get_item(n)==get_item(n+1))
			{
				b_found = true;
				mask.set(n+1,true);
			}
		}
		if (b_found)
		{
			if (b_release) delete_mask(mask);
			else remove_mask(mask);
		}
	}
}

void metadb_handle_list::duplicate_list(const ptr_list_base<metadb_handle> & source)
{
	unsigned n,m = source.get_count();
	for(n=0;n<m;n++) add_item(source[n]->handle_duplicate());
}

void metadb_handle_list::sort_by_path_quick()
{
	sort_by_format("%_path_raw%",0);
}