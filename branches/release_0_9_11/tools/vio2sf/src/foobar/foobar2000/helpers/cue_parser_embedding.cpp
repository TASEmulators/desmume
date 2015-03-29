#include "stdafx.h"

using namespace cue_parser;
using namespace file_info_record_helper;
static void build_cue_meta_name(const char * p_name,unsigned p_tracknumber,pfc::string_base & p_out) {
	p_out.reset();
	p_out << "cue_track" << pfc::format_uint(p_tracknumber % 100,2) << "_" << p_name;
}

static bool is_reserved_meta_entry(const char * p_name) {
	return file_info::field_name_comparator::compare(p_name,"cuesheet") == 0;
}

static bool is_global_meta_entry(const char * p_name) {
	static const char header[] = "cue_track";
	return pfc::stricmp_ascii_ex(p_name,strlen(header),header,infinite) != 0;
}
static bool is_allowed_field(const char * p_name) {
	return !is_reserved_meta_entry(p_name) && is_global_meta_entry(p_name);
}
namespace {
	class __get_tag_cue_track_list_builder {
	public:
		__get_tag_cue_track_list_builder(cue_creator::t_entry_list & p_entries) : m_entries(p_entries) {}
		void operator() (unsigned p_trackno,const track_record & p_record) {
			if (p_trackno > 0) {
				cue_creator::t_entry_list::iterator iter = m_entries.insert_last();
				iter->m_file = p_record.m_file;
				iter->m_flags = p_record.m_flags;
				iter->m_index_list = p_record.m_index_list;
				iter->m_track_number = p_trackno;
				p_record.m_info.to_info(iter->m_infos);
			}
		}
	private:
		cue_creator::t_entry_list & m_entries;
	};

	typedef pfc::avltree_t<pfc::string8,file_info::field_name_comparator> field_name_list;

	class __get_tag__enum_fields_enumerator {
	public:
		__get_tag__enum_fields_enumerator(field_name_list & p_out) : m_out(p_out) {}
		void operator() (unsigned p_trackno,const track_record & p_record) {
			if (p_trackno > 0) p_record.m_info.enumerate_meta(*this);
		}
		template<typename t_value> void operator() (const char * p_name,const t_value & p_value) {
			m_out.add(p_name);
		}
	private:
		field_name_list & m_out;
	};


	class __get_tag__is_field_global_check {
	private:
		typedef file_info_record::t_meta_value t_value;
	public:
		__get_tag__is_field_global_check(const char * p_field) : m_field(p_field), m_value(NULL), m_state(true) {}
		
		void operator() (unsigned p_trackno,const track_record & p_record) {
			if (p_trackno > 0 && m_state) {
				const t_value * val = p_record.m_info.meta_query_ptr(m_field);
				if (val == NULL) {m_state = false; return;}
				if (m_value == NULL) {
					m_value = val;
				} else {
					if (pfc::comparator_list<pfc::comparator_strcmp>::compare(*m_value,*val) != 0) {
						m_state = false; return;
					}
				}
			}
		}
		void finalize(file_info_record::t_meta_map & p_globals) {
			if (m_state && m_value != NULL) {
				p_globals.set(m_field,*m_value);
			}
		}
	private:
		const char * const m_field;
		const t_value * m_value;
		bool m_state;
	};

	class __get_tag__filter_globals {
	public:
		__get_tag__filter_globals(track_record_list const & p_tracks,file_info_record::t_meta_map & p_globals) : m_tracks(p_tracks), m_globals(p_globals) {}

		void operator() (const char * p_field) {
			if (is_allowed_field(p_field)) {
				__get_tag__is_field_global_check wrapper(p_field);
				m_tracks.enumerate(wrapper);
				wrapper.finalize(m_globals);
			}
		}
	private:
		const track_record_list & m_tracks;
		file_info_record::t_meta_map & m_globals;
	};

	class __get_tag__local_field_filter {
	public:
		__get_tag__local_field_filter(const file_info_record::t_meta_map & p_globals,file_info_record::t_meta_map & p_output) : m_globals(p_globals), m_output(p_output), m_currenttrack(0) {}
		void operator() (unsigned p_trackno,const track_record & p_track) {
			if (p_trackno > 0) {
				m_currenttrack = p_trackno;
				p_track.m_info.enumerate_meta(*this);
			}
		}
		void operator() (const char * p_name,const file_info_record::t_meta_value & p_value) {
			PFC_ASSERT(m_currenttrack > 0);
			if (!m_globals.have_item(p_name)) {
				build_cue_meta_name(p_name,m_currenttrack,m_buffer);
				m_output.set(m_buffer,p_value);
			}
		}
	private:
		unsigned m_currenttrack;
		pfc::string8_fastalloc m_buffer;
		const file_info_record::t_meta_map & m_globals;
		file_info_record::t_meta_map & m_output;
	};
};

static void strip_redundant_track_meta(unsigned p_tracknumber,const file_info & p_cueinfo,file_info_record::t_meta_map & p_meta,const char * p_metaname) {
	t_size metaindex = p_cueinfo.meta_find(p_metaname);
	if (metaindex == infinite) return;
	pfc::string_formatter namelocal;
	build_cue_meta_name(p_metaname,p_tracknumber,namelocal);
	{
		const file_info_record::t_meta_value * val = p_meta.query_ptr(namelocal);
		if (val == NULL) return;
		file_info_record::t_meta_value::const_iterator iter = val->first();
		for(t_size valwalk = 0, valcount = p_cueinfo.meta_enum_value_count(metaindex); valwalk < valcount; ++valwalk) {
			if (iter.is_empty()) return;
			if (strcmp(*iter,p_cueinfo.meta_enum_value(metaindex,valwalk)) != 0) return;
			++iter;
		}
		if (!iter.is_empty()) return;
	}
	//success
	p_meta.remove(namelocal);
}

void embeddedcue_metadata_manager::get_tag(file_info & p_info) const {
	if (!have_cuesheet()) {
		m_content.query_ptr((unsigned)0)->m_info.to_info(p_info);
		p_info.meta_remove_field("cuesheet");
	} else {
		cue_creator::t_entry_list entries;
		m_content.enumerate(__get_tag_cue_track_list_builder(entries));
		pfc::string_formatter cuesheet;
		cue_creator::create(cuesheet,entries);
		entries.remove_all();
		//parse it back to see what info got stored in the cuesheet and what needs to be stored outside cuesheet in the tags
		cue_parser::parse_full(cuesheet,entries);
		file_info_record output;

		
		

		{
			file_info_record::t_meta_map globals;
			//1. find global infos and forward them
			{
				field_name_list fields;
				m_content.enumerate(__get_tag__enum_fields_enumerator(fields));
				fields.enumerate(__get_tag__filter_globals(m_content,globals));
			}
			
			output.overwrite_meta(globals);

			//2. find local infos
			m_content.enumerate(__get_tag__local_field_filter(globals,output.m_meta));
		}
		

		//strip redundant titles and tracknumbers that the cuesheet already contains
		for(cue_creator::t_entry_list::const_iterator iter = entries.first(); iter.is_valid(); ++iter) {
			strip_redundant_track_meta(iter->m_track_number,iter->m_infos,output.m_meta,"tracknumber");
			strip_redundant_track_meta(iter->m_track_number,iter->m_infos,output.m_meta,"title");
		}


		//add tech infos etc

		{
			const track_record * rec = m_content.query_ptr((unsigned)0);
			if (rec != NULL) {
				output.set_length(rec->m_info.get_length());
				output.set_replaygain(rec->m_info.get_replaygain());
				output.overwrite_info(rec->m_info.m_info);
			}
		}
		output.meta_set("cuesheet",cuesheet);
		output.to_info(p_info);
	}
}

static bool resolve_cue_meta_name(const char * p_name,pfc::string_base & p_outname,unsigned & p_tracknumber) {
	//"cue_trackNN_fieldname"
	static const char header[] = "cue_track";
	if (pfc::stricmp_ascii_ex(p_name,strlen(header),header,infinite) != 0) return false;
	p_name += strlen(header);
	if (!pfc::char_is_numeric(p_name[0]) || !pfc::char_is_numeric(p_name[1]) || p_name[2] != '_') return false;
	unsigned tracknumber = pfc::atoui_ex(p_name,2);
	if (tracknumber == 0) return false;
	p_name += 3;
	p_tracknumber = tracknumber;
	p_outname = p_name;
	return true;
}


namespace {
	class __set_tag_global_field_relay {
	public:
		__set_tag_global_field_relay(const file_info & p_info,t_size p_index) : m_info(p_info), m_index(p_index) {}
		void operator() (unsigned p_trackno,track_record & p_record) {
			if (p_trackno > 0) {
				p_record.m_info.transfer_meta_entry(m_info.meta_enum_name(m_index),m_info,m_index);
			}
		}
	private:
		const file_info & m_info;
		const t_size m_index;
	};
}

void embeddedcue_metadata_manager::set_tag(file_info const & p_info) {
	m_content.remove_all();
	
	{
		track_record & track0 = m_content.find_or_add((unsigned)0);
		track0.m_info.from_info(p_info);
		track0.m_info.m_info.set("cue_embedded","no");
	}
	
	

	const char * cuesheet = p_info.meta_get("cuesheet",0);
	if (cuesheet == NULL) {
		return;
	}

	//processing order
	//1. cuesheet content
	//2. overwrite with global metadata from the tag
	//2. overwrite with local metadata from the tag

	{
		cue_creator::t_entry_list entries;
		try {
			cue_parser::parse_full(cuesheet,entries);
		} catch(exception_io_data const & e) {
			console::print(e.what());
			return;
		}

		for(cue_creator::t_entry_list::const_iterator iter = entries.first(); iter.is_valid(); ) {
			cue_creator::t_entry_list::const_iterator next = iter;
			++next;
			track_record & entry = m_content.find_or_add(iter->m_track_number);
			entry.m_file = iter->m_file;
			entry.m_flags = iter->m_flags;
			entry.m_index_list = iter->m_index_list;
			entry.m_info.from_info(iter->m_infos);
			entry.m_info.from_info_overwrite_info(p_info);
			entry.m_info.m_info.set("cue_embedded","yes");
			double begin = entry.m_index_list.start(), end = next.is_valid() ? next->m_index_list.start() : p_info.get_length();
			if (end <= begin) throw exception_io_data();
			entry.m_info.set_length(end - begin);
			iter = next;
		}
	}
	
	for(t_size metawalk = 0, metacount = p_info.meta_get_count(); metawalk < metacount; ++metawalk) {
		const char * name = p_info.meta_enum_name(metawalk);
		const t_size valuecount = p_info.meta_enum_value_count(metawalk);
		if (valuecount > 0 && !is_reserved_meta_entry(name) && is_global_meta_entry(name)) {
			__set_tag_global_field_relay relay(p_info,metawalk);
			m_content.enumerate(relay);
		}
	}

	{
		pfc::string8_fastalloc namebuffer;
		for(t_size metawalk = 0, metacount = p_info.meta_get_count(); metawalk < metacount; ++metawalk) {
			const char * name = p_info.meta_enum_name(metawalk);
			const t_size valuecount = p_info.meta_enum_value_count(metawalk);
			unsigned trackno;
			if (valuecount > 0 && !is_reserved_meta_entry(name) && resolve_cue_meta_name(name,namebuffer,trackno)) {
				track_record * rec = m_content.query_ptr(trackno);
				if (rec != NULL) {
					rec->m_info.transfer_meta_entry(namebuffer,p_info,metawalk);
				}
			}
		}
	}
}

void embeddedcue_metadata_manager::get_track_info(unsigned p_track,file_info & p_info) const {
	const track_record * rec = m_content.query_ptr(p_track);
	if (rec == NULL) throw exception_io_data();
	rec->m_info.to_info(p_info);
}

void embeddedcue_metadata_manager::set_track_info(unsigned p_track,file_info const & p_info) {
	track_record * rec = m_content.query_ptr(p_track);
	if (rec == NULL) throw exception_io_data();
	rec->m_info.from_info_set_meta(p_info);
	rec->m_info.set_replaygain(p_info.get_replaygain());
}

void embeddedcue_metadata_manager::query_track_offsets(unsigned p_track,double & p_begin,double & p_length) const {
	const track_record * rec = m_content.query_ptr(p_track);
	if (rec == NULL) throw exception_io_data();
	p_begin = rec->m_index_list.start();
	p_length = rec->m_info.get_length();
}

bool embeddedcue_metadata_manager::have_cuesheet() const {
	return m_content.get_count() > 1;
}

namespace {
	class __remap_trackno_enumerator {
	public:
		__remap_trackno_enumerator(unsigned p_index) : m_countdown(p_index), m_result(0) {}
		template<typename t_blah> void operator() (unsigned p_trackno,const t_blah&) {
			if (p_trackno > 0 && m_result == 0) {
				if (m_countdown == 0) {
					m_result = p_trackno;
				} else {
					--m_countdown;
				}
			}
		}
		unsigned result() const {return m_result;}
	private:
		unsigned m_countdown;
		unsigned m_result;
	};
};

unsigned embeddedcue_metadata_manager::remap_trackno(unsigned p_index) const {
	if (have_cuesheet()) {
		__remap_trackno_enumerator wrapper(p_index);
		m_content.enumerate(wrapper);
		return wrapper.result();
	} else {
		return 0;
	}	
}

t_size embeddedcue_metadata_manager::get_cue_track_count() const {
	return m_content.get_count() - 1;
}