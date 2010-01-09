#include "stdafx.h"

namespace {
	PFC_DECLARE_EXCEPTION(exception_cue,pfc::exception,"Invalid cuesheet");
}

static bool is_numeric(char c) {return c>='0' && c<='9';}


static bool is_spacing(char c)
{
	return c == ' ' || c == '\t';
}

static bool is_linebreak(char c)
{
	return c == '\n' || c == '\r';
}

static void validate_file_type(const char * p_type,t_size p_type_length) {
	if (
		//standard types
		stricmp_utf8_ex(p_type,p_type_length,"WAVE",infinite) != 0 && 
		stricmp_utf8_ex(p_type,p_type_length,"MP3",infinite) != 0 && 
		stricmp_utf8_ex(p_type,p_type_length,"AIFF",infinite) != 0 && 
		//common user-entered types
		stricmp_utf8_ex(p_type,p_type_length,"APE",infinite) != 0 && 
		stricmp_utf8_ex(p_type,p_type_length,"FLAC",infinite) != 0 &&
		stricmp_utf8_ex(p_type,p_type_length,"WV",infinite) != 0 &&
		stricmp_utf8_ex(p_type,p_type_length,"WAVPACK",infinite) != 0
		)
		throw exception_cue(pfc::string_formatter() << "expected WAVE, MP3 or AIFF, got : \"" << pfc::string8(p_type,p_type_length) << "\"");
}

namespace {
	
	class NOVTABLE cue_parser_callback
	{
	public:
		virtual void on_file(const char * p_file,t_size p_file_length,const char * p_type,t_size p_type_length) = 0;
		virtual void on_track(unsigned p_index,const char * p_type,t_size p_type_length) = 0;
		virtual void on_pregap(unsigned p_value) = 0;
		virtual void on_index(unsigned p_index,unsigned p_value) = 0;
		virtual void on_title(const char * p_title,t_size p_title_length) = 0;
		virtual void on_performer(const char * p_performer,t_size p_performer_length) = 0;
		virtual void on_songwriter(const char * p_songwriter,t_size p_songwriter_length) = 0;
		virtual void on_isrc(const char * p_isrc,t_size p_isrc_length) = 0;
		virtual void on_catalog(const char * p_catalog,t_size p_catalog_length) = 0;
		virtual void on_comment(const char * p_comment,t_size p_comment_length) = 0;
		virtual void on_flags(const char * p_flags,t_size p_flags_length) = 0;
	};

	class NOVTABLE cue_parser_callback_meta : public cue_parser_callback
	{
	public:
		virtual void on_file(const char * p_file,t_size p_file_length,const char * p_type,t_size p_type_length) = 0;
		virtual void on_track(unsigned p_index,const char * p_type,t_size p_type_length) = 0;
		virtual void on_pregap(unsigned p_value) = 0;
		virtual void on_index(unsigned p_index,unsigned p_value) = 0;
		virtual void on_meta(const char * p_name,t_size p_name_length,const char * p_value,t_size p_value_length) = 0;
	protected:
		static bool is_known_meta(const char * p_name,t_size p_length)
		{
			static const char * metas[] = {"genre","date","discid","comment","replaygain_track_gain","replaygain_track_peak","replaygain_album_gain","replaygain_album_peak"};
			for(t_size n=0;n<tabsize(metas);n++) {
				if (!stricmp_utf8_ex(p_name,p_length,metas[n],infinite)) return true;
			}
			return false;
		}

		void on_comment(const char * p_comment,t_size p_comment_length)
		{
			unsigned ptr = 0;
			while(ptr < p_comment_length && !is_spacing(p_comment[ptr])) ptr++;
			if (is_known_meta(p_comment, ptr))
			{
				unsigned name_length = ptr;
				while(ptr < p_comment_length && is_spacing(p_comment[ptr])) ptr++;
				if (ptr < p_comment_length)
				{
					if (p_comment[ptr] == '\"')
					{
						ptr++;
						unsigned value_base = ptr;
						while(ptr < p_comment_length && p_comment[ptr] != '\"') ptr++;
						if (ptr == p_comment_length) throw exception_cue("invalid REM syntax",0);
						if (ptr > value_base) on_meta(p_comment,name_length,p_comment + value_base,ptr - value_base);
					}
					else
					{
						unsigned value_base = ptr;
						while(ptr < p_comment_length /*&& !is_spacing(p_comment[ptr])*/) ptr++;
						if (ptr > value_base) on_meta(p_comment,name_length,p_comment + value_base,ptr - value_base);
					}
				}
			}
		}
		void on_title(const char * p_title,t_size p_title_length)
		{
			on_meta("title",infinite,p_title,p_title_length);
		}
		void on_songwriter(const char * p_songwriter,t_size p_songwriter_length) {
			on_meta("songwriter",infinite,p_songwriter,p_songwriter_length);
		}
		void on_performer(const char * p_performer,t_size p_performer_length)
		{
			on_meta("artist",infinite,p_performer,p_performer_length);
		}

		void on_isrc(const char * p_isrc,t_size p_isrc_length)
		{
			on_meta("isrc",infinite,p_isrc,p_isrc_length);
		}
		void on_catalog(const char * p_catalog,t_size p_catalog_length)
		{
			on_meta("catalog",infinite,p_catalog,p_catalog_length);
		}
		void on_flags(const char * p_flags,t_size p_flags_length) {}
	};


	class cue_parser_callback_retrievelist : public cue_parser_callback
	{
	public:
		cue_parser_callback_retrievelist(cue_parser::t_cue_entry_list & p_out) : m_out(p_out), m_track(0), m_pregap(0), m_index0_set(false), m_index1_set(false)
		{
		}
		
		void on_file(const char * p_file,t_size p_file_length,const char * p_type,t_size p_type_length)
		{
			validate_file_type(p_type,p_type_length);
			m_file.set_string(p_file,p_file_length);
		}
		
		void on_track(unsigned p_index,const char * p_type,t_size p_type_length)
		{
			if (stricmp_utf8_ex(p_type,p_type_length,"audio",infinite)) throw exception_cue("only tracks of type AUDIO supported",0);
			//if (p_index != m_track + 1) throw exception_cue("cuesheet tracks out of order");
			if (m_track != 0) finalize_track();
			if (m_file.is_empty()) throw exception_cue("declaring a track with no file set",0);
			m_trackfile = m_file;
			m_track = p_index;
		}

		void on_pregap(unsigned p_value) {m_pregap = (double) p_value / 75.0;}

		void on_index(unsigned p_index,unsigned p_value)
		{
			if (p_index < t_cuesheet_index_list::count)
			{
				switch(p_index)
				{
				case 0: m_index0_set = true; break;
				case 1: m_index1_set = true; break;
				}
				m_index_list.m_positions[p_index] = (double) p_value / 75.0;
			}
		}

		void on_title(const char * p_title,t_size p_title_length) {}
		void on_performer(const char * p_performer,t_size p_performer_length) {}
		void on_songwriter(const char * p_songwriter,t_size p_songwriter_length) {}
		void on_isrc(const char * p_isrc,t_size p_isrc_length) {}
		void on_catalog(const char * p_catalog,t_size p_catalog_length) {}
		void on_comment(const char * p_comment,t_size p_comment_length) {}
		void on_flags(const char * p_flags,t_size p_flags_length) {}

		void finalize()
		{
			if (m_track != 0)
			{
				finalize_track();
				m_track = 0;
			}
		}

	private:
		void finalize_track()
		{
			if (!m_index1_set) throw exception_cue("INDEX 01 not set",0);
			if (!m_index0_set) m_index_list.m_positions[0] = m_index_list.m_positions[1] - m_pregap;
			if (!m_index_list.is_valid()) throw exception_cue("invalid index list");

			cue_parser::t_cue_entry_list::iterator iter;
			iter = m_out.insert_last();
			if (m_trackfile.is_empty()) throw exception_cue("track has no file assigned",0);
			iter->m_file = m_trackfile;
			iter->m_track_number = m_track;
			iter->m_indexes = m_index_list;
			
			m_index_list.reset();
			m_index0_set = false;
			m_index1_set = false;
			m_pregap = 0;
		}

		bool m_index0_set,m_index1_set;
		t_cuesheet_index_list m_index_list;
		double m_pregap;
		unsigned m_track;
		pfc::string8 m_file,m_trackfile;
		cue_parser::t_cue_entry_list & m_out;
	};

	class cue_parser_callback_retrieveinfo : public cue_parser_callback_meta
	{
	public:
		cue_parser_callback_retrieveinfo(file_info & p_out,unsigned p_wanted_track) : m_out(p_out), m_wanted_track(p_wanted_track), m_track(0), m_is_va(false), m_index0_set(false), m_index1_set(false), m_pregap(0), m_totaltracks(0) {}

		void on_file(const char * p_file,t_size p_file_length,const char * p_type,t_size p_type_length) {}

		void on_track(unsigned p_index,const char * p_type,t_size p_type_length)
		{
			if (p_index == 0) throw exception_cue("invalid TRACK index",0);
			if (p_index == m_wanted_track)
			{
				if (stricmp_utf8_ex(p_type,p_type_length,"audio",infinite)) throw exception_cue("only tracks of type AUDIO supported",0);
			}
			m_track = p_index;
			m_totaltracks++;
		}

		void on_pregap(unsigned p_value) {if (m_track == m_wanted_track) m_pregap = (double) p_value / 75.0;}

		void on_index(unsigned p_index,unsigned p_value)
		{
			if (m_track == m_wanted_track && p_index < t_cuesheet_index_list::count)
			{
				switch(p_index)
				{
				case 0:	m_index0_set = true; break;
				case 1: m_index1_set = true; break;
				}
				m_indexes.m_positions[p_index] = (double) p_value / 75.0;
			}
		}

		
		void on_meta(const char * p_name,t_size p_name_length,const char * p_value,t_size p_value_length)
		{
			t_meta_list::iterator iter;
			if (m_track == 0) //globals
			{
				//convert global title to album
				if (!stricmp_utf8_ex(p_name,p_name_length,"title",infinite))
				{
					p_name = "album";
					p_name_length = 5;
				}
				else if (!stricmp_utf8_ex(p_name,p_name_length,"artist",infinite))
				{
					m_album_artist.set_string(p_value,p_value_length);
				}

				iter = m_globals.insert_last();
			}
			else
			{
				if (!m_is_va)
				{
					if (!stricmp_utf8_ex(p_name,p_name_length,"artist",infinite))
					{
						if (!m_album_artist.is_empty())
						{
							if (stricmp_utf8_ex(p_value,p_value_length,m_album_artist,m_album_artist.length())) m_is_va = true;
						}
					}
				}

				if (m_track == m_wanted_track) //locals
				{
					iter = m_locals.insert_last();
				}
			}
			if (iter.is_valid())
			{
				iter->m_name.set_string(p_name,p_name_length);
				iter->m_value.set_string(p_value,p_value_length);
			}
		}

		void finalize()
		{
			if (!m_index1_set) throw exception_cue("INDEX 01 not set",0);
			if (!m_index0_set) m_indexes.m_positions[0] = m_indexes.m_positions[1] - m_pregap;
			m_indexes.to_infos(m_out);

			replaygain_info rg;
			rg.reset();
			t_meta_list::const_iterator iter;

			if (m_is_va)
			{
				//clean up VA mess

				t_meta_list::const_iterator iter_global,iter_local;

				iter_global = find_first_field(m_globals,"artist");
				iter_local = find_first_field(m_locals,"artist");
				if (iter_global.is_valid())
				{
					m_out.meta_set("album artist",iter_global->m_value);
					if (iter_local.is_valid()) m_out.meta_set("artist",iter_local->m_value);
					else m_out.meta_set("artist",iter_global->m_value);
				}
				else
				{
					if (iter_local.is_valid()) m_out.meta_set("artist",iter_local->m_value);
				}
				

				wipe_field(m_globals,"artist");
				wipe_field(m_locals,"artist");
				
			}

			for(iter=m_globals.first();iter.is_valid();iter++)
			{
				if (!rg.set_from_meta(iter->m_name,iter->m_value))
					m_out.meta_set(iter->m_name,iter->m_value);
			}
			for(iter=m_locals.first();iter.is_valid();iter++)
			{
				if (!rg.set_from_meta(iter->m_name,iter->m_value))
					m_out.meta_set(iter->m_name,iter->m_value);
			}
			m_out.meta_set("tracknumber",pfc::string_formatter() << m_wanted_track);
			m_out.meta_set("totaltracks", pfc::string_formatter() << m_totaltracks);
			m_out.set_replaygain(rg);

		}
	private:
		struct t_meta_entry {
			pfc::string8 m_name,m_value;
		};
		typedef pfc::chain_list_v2_t<t_meta_entry> t_meta_list;

		static t_meta_list::const_iterator find_first_field(t_meta_list const & p_list,const char * p_field)
		{
			t_meta_list::const_iterator iter;
			for(iter=p_list.first();iter.is_valid();++iter)
			{
				if (!stricmp_utf8(p_field,iter->m_name)) return iter;
			}
			return t_meta_list::const_iterator();//null iterator
		}

		static void wipe_field(t_meta_list & p_list,const char * p_field)
		{
			t_meta_list::iterator iter;
			for(iter=p_list.first();iter.is_valid();)
			{
				if (!stricmp_utf8(p_field,iter->m_name))
				{
					t_meta_list::iterator temp = iter;
					++temp;
					p_list.remove_single(iter);
					iter = temp;
				}
				else
				{
					++iter;
				}
			}
		}
		
		t_meta_list m_globals,m_locals;
		file_info & m_out;
		unsigned m_wanted_track, m_track,m_totaltracks;
		pfc::string8 m_album_artist;
		bool m_is_va;
		t_cuesheet_index_list m_indexes;
		bool m_index0_set,m_index1_set;
		double m_pregap;
	};

};


static void g_parse_cue_line(const char * p_line,t_size p_line_length,cue_parser_callback & p_callback)
{
	t_size ptr = 0;
	while(ptr < p_line_length && !is_spacing(p_line[ptr])) ptr++;
	if (!stricmp_utf8_ex(p_line,ptr,"file",infinite))
	{
		while(ptr < p_line_length && is_spacing(p_line[ptr])) ptr++;
		t_size file_base,file_length, type_base,type_length;
		
		if (p_line[ptr] == '\"')
		{
			ptr++;
			file_base = ptr;
			while(ptr < p_line_length && p_line[ptr] != '\"') ptr++;
			if (ptr == p_line_length) throw exception_cue("invalid FILE syntax",0);
			file_length = ptr - file_base;
			ptr++;
			while(ptr < p_line_length && is_spacing(p_line[ptr])) ptr++;
		}
		else
		{
			file_base = ptr;
			while(ptr < p_line_length && !is_spacing(p_line[ptr])) ptr++;
			file_length = ptr - file_base;
			while(ptr < p_line_length && is_spacing(p_line[ptr])) ptr++;
		}

		type_base = ptr;
		while(ptr < p_line_length && !is_spacing(p_line[ptr])) ptr++;
		type_length = ptr - type_base;
		while(ptr < p_line_length && is_spacing(p_line[ptr])) ptr++;

		if (ptr != p_line_length || file_length == 0 || type_length == 0) throw exception_cue("invalid FILE syntax",0);

		p_callback.on_file(p_line + file_base, file_length, p_line + type_base, type_length);
	}
	else if (!stricmp_utf8_ex(p_line,ptr,"track",infinite))
	{
		while(ptr < p_line_length && is_spacing(p_line[ptr])) ptr++;

		t_size track_base = ptr, track_length;
		while(ptr < p_line_length && !is_spacing(p_line[ptr]))
		{
			if (!is_numeric(p_line[ptr])) throw exception_cue("invalid TRACK syntax",0);
			ptr++;
		}
		track_length = ptr - track_base;
		while(ptr < p_line_length && is_spacing(p_line[ptr])) ptr++;
		
		t_size type_base = ptr, type_length;
		while(ptr < p_line_length && !is_spacing(p_line[ptr])) ptr++;
		type_length = ptr - type_base;

		while(ptr < p_line_length && is_spacing(p_line[ptr])) ptr++;
		if (ptr != p_line_length || type_length == 0) throw exception_cue("invalid TRACK syntax",0);
		unsigned track = pfc::atoui_ex(p_line+track_base,track_length);
		if (track < 1 || track > 99) throw exception_cue("invalid track number",0);

		p_callback.on_track(track,p_line + type_base, type_length);
	}
	else if (!stricmp_utf8_ex(p_line,ptr,"index",infinite))
	{
		while(ptr < p_line_length && is_spacing(p_line[ptr])) ptr++;

		t_size index_base,index_length, time_base,time_length;
		index_base = ptr;
		while(ptr < p_line_length && !is_spacing(p_line[ptr]))
		{
			if (!is_numeric(p_line[ptr])) throw exception_cue("invalid INDEX syntax",0);
			ptr++;
		}
		index_length = ptr - index_base;
		
		while(ptr < p_line_length && is_spacing(p_line[ptr])) ptr++;
		time_base = ptr;
		while(ptr < p_line_length && !is_spacing(p_line[ptr]))
		{
			if (!is_numeric(p_line[ptr]) && p_line[ptr] != ':')
				throw exception_cue("invalid INDEX syntax",0);
			ptr++;
		}
		time_length = ptr - time_base;

		while(ptr < p_line_length && is_spacing(p_line[ptr])) ptr++;
		
		if (ptr != p_line_length || index_length == 0 || time_length == 0)
			throw exception_cue("invalid INDEX syntax",0);

		unsigned index = pfc::atoui_ex(p_line+index_base,index_length);
		if (index > 99) throw exception_cue("invalid INDEX syntax",0);
		unsigned time = cuesheet_parse_index_time_ticks_e(p_line + time_base,time_length);
		
		p_callback.on_index(index,time);
	}
	else if (!stricmp_utf8_ex(p_line,ptr,"pregap",infinite))
	{
		while(ptr < p_line_length && is_spacing(p_line[ptr])) ptr++;

		t_size time_base, time_length;
		time_base = ptr;
		while(ptr < p_line_length && !is_spacing(p_line[ptr]))
		{
			if (!is_numeric(p_line[ptr]) && p_line[ptr] != ':')
				throw exception_cue("invalid PREGAP syntax",0);
			ptr++;
		}
		time_length = ptr - time_base;

		while(ptr < p_line_length && is_spacing(p_line[ptr])) ptr++;
		
		if (ptr != p_line_length || time_length == 0)
			throw exception_cue("invalid PREGAP syntax",0);

		unsigned time = cuesheet_parse_index_time_ticks_e(p_line + time_base,time_length);
		
		p_callback.on_pregap(time);
	}
	else if (!stricmp_utf8_ex(p_line,ptr,"title",infinite))
	{
		while(ptr < p_line_length && is_spacing(p_line[ptr])) ptr++;
		if (ptr == p_line_length) throw exception_cue("invalid TITLE syntax",0);
		if (p_line[ptr] == '\"')
		{
			ptr++;
			t_size base = ptr;
			while(ptr < p_line_length && p_line[ptr] != '\"') ptr++;
			if (ptr == p_line_length) throw exception_cue("invalid TITLE syntax",0);
			t_size length = ptr-base;
			ptr++;
			while(ptr < p_line_length && is_spacing(p_line[ptr])) ptr++;
			if (ptr != p_line_length) throw exception_cue("invalid TITLE syntax",0);
			p_callback.on_title(p_line+base,length);
		}
		else
		{
			p_callback.on_title(p_line+ptr,p_line_length-ptr);
		}
	}
	else if (!stricmp_utf8_ex(p_line,ptr,"performer",infinite))
	{
		while(ptr < p_line_length && is_spacing(p_line[ptr])) ptr++;
		if (ptr == p_line_length) throw exception_cue("invalid PERFORMER syntax",0);
		if (p_line[ptr] == '\"')
		{
			ptr++;
			t_size base = ptr;
			while(ptr < p_line_length && p_line[ptr] != '\"') ptr++;
			if (ptr == p_line_length) throw exception_cue("invalid PERFORMER syntax",0);
			t_size length = ptr-base;
			ptr++;
			while(ptr < p_line_length && is_spacing(p_line[ptr])) ptr++;
			if (ptr != p_line_length) throw exception_cue("invalid PERFORMER syntax",0);
			p_callback.on_performer(p_line+base,length);
		}
		else
		{
			p_callback.on_performer(p_line+ptr,p_line_length-ptr);
		}
	}
	else if (!stricmp_utf8_ex(p_line,ptr,"songwriter",infinite))
	{
		while(ptr < p_line_length && is_spacing(p_line[ptr])) ptr++;
		if (ptr == p_line_length) throw exception_cue("invalid SONGWRITER syntax",0);
		if (p_line[ptr] == '\"')
		{
			ptr++;
			t_size base = ptr;
			while(ptr < p_line_length && p_line[ptr] != '\"') ptr++;
			if (ptr == p_line_length) throw exception_cue("invalid SONGWRITER syntax",0);
			t_size length = ptr-base;
			ptr++;
			while(ptr < p_line_length && is_spacing(p_line[ptr])) ptr++;
			if (ptr != p_line_length) throw exception_cue("invalid SONGWRITER syntax",0);
			p_callback.on_songwriter(p_line+base,length);
		}
		else
		{
			p_callback.on_songwriter(p_line+ptr,p_line_length-ptr);
		}
	}
	else if (!stricmp_utf8_ex(p_line,ptr,"isrc",infinite))
	{
		while(ptr < p_line_length && is_spacing(p_line[ptr])) ptr++;
		t_size length = p_line_length - ptr;
		if (length == 0) throw exception_cue("invalid ISRC syntax",0);
		p_callback.on_isrc(p_line+ptr,length);
	}
	else if (!stricmp_utf8_ex(p_line,ptr,"catalog",infinite))
	{
		while(ptr < p_line_length && is_spacing(p_line[ptr])) ptr++;
		t_size length = p_line_length - ptr;
		if (length == 0) throw exception_cue("invalid CATALOG syntax",0);
		p_callback.on_catalog(p_line+ptr,length);
	}
	else if (!stricmp_utf8_ex(p_line,ptr,"flags",infinite))
	{
		while(ptr < p_line_length && is_spacing(p_line[ptr])) ptr++;
		if (ptr < p_line_length)
			p_callback.on_flags(p_line + ptr, p_line_length - ptr);
	}
	else if (!stricmp_utf8_ex(p_line,ptr,"rem",infinite))
	{
		while(ptr < p_line_length && is_spacing(p_line[ptr])) ptr++;
		if (ptr < p_line_length)
			p_callback.on_comment(p_line + ptr, p_line_length - ptr);
	}
	else if (!stricmp_utf8_ex(p_line,ptr,"postgap",infinite)) {
		throw exception_cue("POSTGAP is not supported",0);
	} else if (!stricmp_utf8_ex(p_line,ptr,"cdtextfile",infinite)) {
		//do nothing
	}
	else throw exception_cue("unknown cuesheet item",0);
}

static void g_parse_cue(const char * p_cuesheet,cue_parser_callback & p_callback)
{
	const char * parseptr = p_cuesheet;
	t_size lineidx = 1;
	while(*parseptr)
	{
		while(is_spacing(*parseptr)) *parseptr++;
		if (*parseptr)
		{
			t_size length = 0;
			while(parseptr[length] && !is_linebreak(parseptr[length])) length++;
			if (length > 0) {
				try {
					g_parse_cue_line(parseptr,length,p_callback);
				} catch(exception_cue const & e) {//rethrow with line info
					throw exception_cue(pfc::string_formatter() << e.what() << " (line " << lineidx << ")");
				}
			}
			parseptr += length;
			while(is_linebreak(*parseptr)) {
				if (*parseptr == '\n') lineidx++;
				parseptr++;
			}
		}
	}
}

void cue_parser::parse(const char *p_cuesheet,t_cue_entry_list & p_out) {
	try {
		cue_parser_callback_retrievelist callback(p_out);
		g_parse_cue(p_cuesheet,callback);
		callback.finalize();
	} catch(exception_cue const & e) {
		throw exception_bad_cuesheet(pfc::string_formatter() << "Error parsing cuesheet: " << e.what());
	}
}
void cue_parser::parse_info(const char * p_cuesheet,file_info & p_info,unsigned p_index) {
	try {
		cue_parser_callback_retrieveinfo callback(p_info,p_index);
		g_parse_cue(p_cuesheet,callback);
		callback.finalize();
	} catch(exception_cue const & e) {
		throw exception_bad_cuesheet(pfc::string_formatter() << "Error parsing cuesheet: " << e.what());
	}
}

namespace {

	class cue_parser_callback_retrievecount : public cue_parser_callback
	{
	public:
		cue_parser_callback_retrievecount() : m_count(0) {}
		unsigned get_count() const {return m_count;}
		void on_file(const char * p_file,t_size p_file_length,const char * p_type,t_size p_type_length) {}
		void on_track(unsigned p_index,const char * p_type,t_size p_type_length) {m_count++;}
		void on_pregap(unsigned p_value) {}
		void on_index(unsigned p_index,unsigned p_value) {}
		void on_title(const char * p_title,t_size p_title_length) {}
		void on_performer(const char * p_performer,t_size p_performer_length) {}
		void on_isrc(const char * p_isrc,t_size p_isrc_length) {}
		void on_catalog(const char * p_catalog,t_size p_catalog_length) {}
		void on_comment(const char * p_comment,t_size p_comment_length) {}
		void on_flags(const char * p_flags,t_size p_flags_length) {}
	private:
		unsigned m_count;
	};

	class cue_parser_callback_retrievecreatorentries : public cue_parser_callback
	{
	public:
		cue_parser_callback_retrievecreatorentries(cue_creator::t_entry_list & p_out) : m_out(p_out), m_track(0), m_pregap(0), m_index0_set(false), m_index1_set(false) {}

		void on_file(const char * p_file,t_size p_file_length,const char * p_type,t_size p_type_length) {
			validate_file_type(p_type,p_type_length);
			m_file.set_string(p_file,p_file_length);
		}
		
		void on_track(unsigned p_index,const char * p_type,t_size p_type_length)
		{
			if (stricmp_utf8_ex(p_type,p_type_length,"audio",infinite)) throw exception_cue("only tracks of type AUDIO supported",0);
			//if (p_index != m_track + 1) throw exception_cue("cuesheet tracks out of order",0);
			if (m_track != 0) finalize_track();
			if (m_file.is_empty()) throw exception_cue("declaring a track with no file set",0);
			m_trackfile = m_file;
			m_track = p_index;
		}
		
		void on_pregap(unsigned p_value)
		{
			m_pregap = (double) p_value / 75.0;
		}

		void on_index(unsigned p_index,unsigned p_value)
		{
			if (p_index < t_cuesheet_index_list::count)
			{
				switch(p_index)
				{
				case 0:	m_index0_set = true; break;
				case 1: m_index1_set = true; break;
				}
				m_indexes.m_positions[p_index] = (double) p_value / 75.0;
			}
		}
		void on_title(const char * p_title,t_size p_title_length) {}
		void on_performer(const char * p_performer,t_size p_performer_length) {}
		void on_songwriter(const char * p_performer,t_size p_performer_length) {}
		void on_isrc(const char * p_isrc,t_size p_isrc_length) {}
		void on_catalog(const char * p_catalog,t_size p_catalog_length) {}
		void on_comment(const char * p_comment,t_size p_comment_length) {}		
		void finalize()
		{
			if (m_track != 0)
			{
				finalize_track(); 
				m_track = 0;
			}
		}
		void on_flags(const char * p_flags,t_size p_flags_length) {
			m_flags.set_string(p_flags,p_flags_length);
		}
	private:
		void finalize_track()
		{
			if (m_track < 1 || m_track > 99) throw exception_cue("track number out of range",0);
			if (!m_index1_set) throw exception_cue("INDEX 01 not set",0);
			if (!m_index0_set) m_indexes.m_positions[0] = m_indexes.m_positions[1] - m_pregap;
			if (!m_indexes.is_valid()) throw exception_cue("invalid index list");

			cue_creator::t_entry_list::iterator iter;
			iter = m_out.insert_last();
			iter->m_track_number = m_track;
			iter->m_file = m_trackfile;
			iter->m_index_list = m_indexes;			
			iter->m_flags = m_flags;
			m_pregap = 0;
			m_indexes.reset();
			m_index0_set = m_index1_set = false;
			m_flags.reset();
		}

		bool m_index0_set,m_index1_set;
		double m_pregap;
		unsigned m_track;
		cue_creator::t_entry_list & m_out;
		pfc::string8 m_file,m_trackfile,m_flags;
		t_cuesheet_index_list m_indexes;
	};
}

void cue_parser::parse_full(const char * p_cuesheet,cue_creator::t_entry_list & p_out) {
	try {
		{
			cue_parser_callback_retrievecreatorentries callback(p_out);
			g_parse_cue(p_cuesheet,callback);
			callback.finalize();
		}

		{
			cue_creator::t_entry_list::iterator iter;
			for(iter=p_out.first();iter.is_valid();++iter)
			{
				cue_parser_callback_retrieveinfo callback(iter->m_infos,iter->m_track_number);
				g_parse_cue(p_cuesheet,callback);
				callback.finalize();
			}
		}
	} catch(exception_cue const & e) {
		throw exception_bad_cuesheet(pfc::string_formatter() << "Error parsing cuesheet: " << e.what());
	}
}
