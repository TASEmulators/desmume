#include "foobar2000.h"

typedef pfc::avltree_t<pfc::string8,pfc::io::path::comparator> t_fnList;

static void formatMaskList(pfc::string_base & out, t_fnList const & in) {
	pfc::const_iterator<pfc::string8> walk = in.first();
	if (walk.is_valid()) {
		out << *walk; ++walk;
		while(walk.is_valid()) {
			out << ";" << *walk; ++walk;
		}
	}
}
static void formatMaskList(pfc::string_base & out, t_fnList const & in, const char * label) {
	if (in.get_count() > 0) {
		out << label << "|";
		formatMaskList(out,in);
		out << "|";
	}
}

void input_file_type::build_openfile_mask(pfc::string_base & out, bool b_include_playlists)
{	
	t_fnList extensionsAll, extensionsPl;;
	
	if (b_include_playlists) {
		service_enum_t<playlist_loader> e; service_ptr_t<playlist_loader> ptr;
		while(e.next(ptr)) {
			if (ptr->is_associatable()) {
				pfc::string_formatter temp; temp << "*." << ptr->get_extension();
				extensionsPl += temp;
				extensionsAll += temp;
			}
		}
	}

	typedef pfc::map_t<pfc::string8,t_fnList,pfc::string::comparatorCaseInsensitive> t_masks;
	t_masks masks;
	{
		service_enum_t<input_file_type> e;
		service_ptr_t<input_file_type> ptr;
		pfc::string_formatter name, mask;
		while(e.next(ptr)) {
			const unsigned count = ptr->get_count();
			for(unsigned n=0;n<count;n++) {
				name.reset();
				mask.reset();
				if (ptr->get_name(n,name) && ptr->get_mask(n,mask)) {
					if (!strchr(name,'|') && !strchr(mask,'|')) {
						masks.find_or_add(name) += mask;
						extensionsAll += mask;
					}
				}
			}
		}
	}
	pfc::string_formatter outBuf;
	outBuf << "All files|*.*|";
	formatMaskList(outBuf, extensionsAll, "All supported types");
	formatMaskList(outBuf, extensionsPl, "Playlists");
	for(t_masks::const_iterator walk = masks.first(); walk.is_valid(); ++walk) {
		formatMaskList(outBuf,walk->m_value,walk->m_key);			
	}
	out = outBuf;
}
