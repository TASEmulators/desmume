#include "foobar2000.h"

void playlist_oper::move_items(int delta)
{
	if (delta==0) return;
	
	int count = get_count();
	int n;

	mem_block_t<int> order(count);
	mem_block_t<bool> selection(count);
	
	for(n=0;n<count;n++)
		order[n]=n;

	get_sel_mask(selection,count);

	if (delta<0)
	{
		for(;delta<0;delta++)
		{
			int idx;
			for(idx=1;idx<count;idx++)
			{
				if (selection[idx] && !selection[idx-1])
				{
					order.swap(idx,idx-1);
					selection.swap(idx,idx-1);
				}
			}
		}
	}
	else
	{
		for(;delta>0;delta--)
		{
			int idx;
			for(idx=count-2;idx>=0;idx--)
			{
				if (selection[idx] && !selection[idx+1])
				{
					order.swap(idx,idx+1);
					selection.swap(idx,idx+1);
				}
			}
		}
	}

	apply_order(order,count);
}
/*
void playlist_callback::on_order(const int * order,int count)
{
	int n;
	for(n=0;n<count;n++) if (order[n]!=n) on_modified(n);
	int focus = playlist_oper::get()->get_focus();
	if (focus>=0 && order[focus]!=focus) on_focus_change(order[focus],focus);
}
*/
playlist_oper * playlist_oper::get()
{//since this is called really often, lets cache a pointer instead of flooding service core
	static playlist_oper * g_ptr;
	if (g_ptr==0) g_ptr = service_enum_create_t(playlist_oper,0);
	return g_ptr;
}

void playlist_oper::remove_sel(bool crop)
{
	bit_array_bittable mask(get_count());
	get_sel_mask(mask);
	if (crop) remove_mask(bit_array_not(mask));
	else remove_mask(mask);
}

unsigned playlist_switcher::find_playlist(const char * name)
{
	unsigned n,max = get_num_playlists();
	string8_fastalloc temp;
	for(n=0;n<max;n++)
	{
		temp.reset();
		if (get_playlist_name(n,temp))
		{
			if (!stricmp_utf8(temp,name)) return n;
		}
	}
	return (unsigned)(-1);
}

unsigned playlist_switcher::find_or_create_playlist(const char * name)
{
	unsigned idx = find_playlist(name);
	if (idx==(unsigned)(-1))
	{
		metadb_handle_list dummy;
		idx = create_playlist_fixname(string8(name),dummy);
	}
	return idx;
}


playlist_switcher * playlist_switcher::get()
{
	return service_enum_create_t(playlist_switcher,0);
}

bool playlist_switcher::delete_playlist_autoswitch(unsigned idx)
{
	unsigned num,active;
	num = get_num_playlists();
	active = get_active_playlist();
	if (idx>=num || num<=1) return false;
	if (idx==active)
	{
		if (idx+1>=num) active = idx-1;
		else active = idx+1;
		set_active_playlist(active);
	}
	return delete_playlist(idx);
}

bool playlist_switcher::find_or_create_playlist_activate(const char * name)
{
	bool rv = false;
	unsigned idx = find_or_create_playlist(name);
	if (idx != (unsigned)(-1))
		rv = set_active_playlist(idx);
	return rv;
}

int playback_flow_control::get_next(int previous_index,int focus_item,int total,int advance,bool follow_focus,bool user_advance,unsigned playlist)
{
	int rv = -1;
	playback_flow_control_v2 * this_v2 = service_query_t(playback_flow_control_v2,this);
	if (this_v2)
	{
		rv = this_v2->get_next(previous_index,focus_item,total,advance,follow_focus,user_advance,playlist);
		this_v2->service_release();
	}
	else
	{
		console::error(uStringPrintf("\"%s\" needs to be updated to 0.8 SDK or newer in order to function.",get_name()));
	}
	return rv;
}