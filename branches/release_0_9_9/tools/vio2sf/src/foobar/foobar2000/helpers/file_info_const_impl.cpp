#include "stdafx.h"

// presorted - do not change without a proper strcmp resort
static const char * const standard_fieldnames[] = {
	"ALBUM","ALBUM ARTIST","ARTIST","Album","Album Artist","Artist","COMMENT","Comment","DATE","DISCNUMBER","Date",
	"Discnumber","GENRE","Genre","TITLE","TOTALTRACKS","TRACKNUMBER","Title","TotalTracks","Totaltracks","TrackNumber",
	"Tracknumber","album","album artist","artist","comment","date","discnumber","genre","title","totaltracks","tracknumber",
};

// presorted - do not change without a proper strcmp resort
static const char * const standard_infonames[] = {
	"bitrate","bitspersample","channels","codec","codec_profile","encoding","samplerate","tagtype","tool",
};

static const char * optimize_fieldname(const char * p_string) {
	t_size index;
	if (!pfc::binarySearch<pfc::comparator_strcmp>::run(standard_fieldnames,0,tabsize(standard_fieldnames),p_string,index)) return NULL;
	return standard_fieldnames[index];
}

static const char * optimize_infoname(const char * p_string) {
	t_size index;
	if (!pfc::binarySearch<pfc::comparator_strcmp>::run(standard_infonames,0,tabsize(standard_infonames),p_string,index)) return NULL;
	return standard_infonames[index];
}

/*
order of things

  meta entries
  meta value map
  info entries 
  string buffer

*/

inline static char* stringbuffer_append(char * & buffer,const char * value)
{
	char * ret = buffer;
	while(*value) *(buffer++) = *(value++);
	*(buffer++) = 0;
	return ret;
}

#ifdef __file_info_const_impl_have_hintmap__

namespace {
	class sort_callback_hintmap_impl : public pfc::sort_callback
	{
	public:
		sort_callback_hintmap_impl(const file_info_const_impl::meta_entry * p_meta,file_info_const_impl::t_index * p_hintmap)
			: m_meta(p_meta), m_hintmap(p_hintmap)
		{
		}
		
		int compare(t_size p_index1, t_size p_index2) const
		{
//			profiler(sort_callback_hintmap_impl_compare);
			return pfc::stricmp_ascii(m_meta[m_hintmap[p_index1]].m_name,m_meta[m_hintmap[p_index2]].m_name);
		}
		
		void swap(t_size p_index1, t_size p_index2)
		{
			pfc::swap_t<file_info_const_impl::t_index>(m_hintmap[p_index1],m_hintmap[p_index2]);
		}
	private:
		const file_info_const_impl::meta_entry * m_meta;
		file_info_const_impl::t_index * m_hintmap;
	};

	class bsearch_callback_hintmap_impl// : public pfc::bsearch_callback
	{
	public:
		bsearch_callback_hintmap_impl(
			const file_info_const_impl::meta_entry * p_meta,
			const file_info_const_impl::t_index * p_hintmap,
			const char * p_name,
			t_size p_name_length)
			: m_meta(p_meta), m_hintmap(p_hintmap), m_name(p_name), m_name_length(p_name_length)
		{
		}

		inline int test(t_size p_index) const
		{
			return pfc::stricmp_ascii_ex(m_meta[m_hintmap[p_index]].m_name,infinite,m_name,m_name_length);
		}

	private:
		const file_info_const_impl::meta_entry * m_meta;
		const file_info_const_impl::t_index * m_hintmap;
		const char * m_name;
		t_size m_name_length;
	};
}

#endif//__file_info_const_impl_have_hintmap__

void file_info_const_impl::copy(const file_info & p_source)
{
//	profiler(file_info_const_impl__copy);
	t_size meta_size = 0;
	t_size info_size = 0;
	t_size valuemap_size = 0;
	t_size stringbuffer_size = 0;
#ifdef __file_info_const_impl_have_hintmap__
	t_size hintmap_size = 0;
#endif

	{
//		profiler(file_info_const_impl__copy__pass1);
		t_size index;
		m_meta_count = pfc::downcast_guarded<t_index>(p_source.meta_get_count());
		meta_size = m_meta_count * sizeof(meta_entry);
#ifdef __file_info_const_impl_have_hintmap__
		hintmap_size = (m_meta_count > hintmap_cutoff) ? m_meta_count * sizeof(t_index) : 0;
#endif//__file_info_const_impl_have_hintmap__
		for(index = 0; index < m_meta_count; index++ )
		{
			{
				const char * name = p_source.meta_enum_name(index);
				if (optimize_fieldname(name) == 0)
					stringbuffer_size += strlen(name) + 1;
			}

			t_size val; const t_size val_max = p_source.meta_enum_value_count(index);
			
			if (val_max == 1)
			{
				stringbuffer_size += strlen(p_source.meta_enum_value(index,0)) + 1;
			}
			else
			{
				valuemap_size += val_max * sizeof(char*);

				for(val = 0; val < val_max; val++ )
				{
					stringbuffer_size += strlen(p_source.meta_enum_value(index,val)) + 1;
				}
			}
		}

		m_info_count = pfc::downcast_guarded<t_index>(p_source.info_get_count());
		info_size = m_info_count * sizeof(info_entry);
		for(index = 0; index < m_info_count; index++ )
		{
			const char * name = p_source.info_enum_name(index);
			if (optimize_infoname(name) == NULL) stringbuffer_size += strlen(name) + 1;
			stringbuffer_size += strlen(p_source.info_enum_value(index)) + 1;
		}
	}


	{
//		profiler(file_info_const_impl__copy__alloc);
		m_buffer.set_size(
#ifdef __file_info_const_impl_have_hintmap__
			hintmap_size + 
#endif
			meta_size + info_size + valuemap_size + stringbuffer_size);
	}

	char * walk = m_buffer.get_ptr();

#ifdef __file_info_const_impl_have_hintmap__
	t_index* hintmap = (hintmap_size > 0) ? (t_index*) walk : NULL;
	walk += hintmap_size;
#endif
	meta_entry * meta = (meta_entry*) walk;
	walk += meta_size;
	char ** valuemap = (char**) walk;
	walk += valuemap_size;
	info_entry * info = (info_entry*) walk;
	walk += info_size;
	char * stringbuffer = walk;

	m_meta = meta;
	m_info = info;
#ifdef __file_info_const_impl_have_hintmap__
	m_hintmap = hintmap;
#endif

	{
//		profiler(file_info_const_impl__copy__pass2);
		t_size index;
		for( index = 0; index < m_meta_count; index ++ )
		{
			t_size val; const t_size val_max = p_source.meta_enum_value_count(index);

			{
				const char * name = p_source.meta_enum_name(index);
				const char * name_opt = optimize_fieldname(name);
				if (name_opt == NULL)
					meta[index].m_name = stringbuffer_append(stringbuffer, name );
				else
					meta[index].m_name = name_opt;
			}
			
			meta[index].m_valuecount = val_max;

			if (val_max == 1)
			{
				meta[index].m_valuemap = reinterpret_cast<const char * const *>(stringbuffer_append(stringbuffer, p_source.meta_enum_value(index,0) ));
			}
			else
			{
				meta[index].m_valuemap = valuemap;
				for( val = 0; val < val_max ; val ++ )
					*(valuemap ++ ) = stringbuffer_append(stringbuffer, p_source.meta_enum_value(index,val) );
			}
		}

		for( index = 0; index < m_info_count; index ++ )
		{
			const char * name = p_source.info_enum_name(index);
			const char * name_opt = optimize_infoname(name);
			if (name_opt == NULL)
				info[index].m_name = stringbuffer_append(stringbuffer, name );
			else
				info[index].m_name = name_opt;
			info[index].m_value = stringbuffer_append(stringbuffer, p_source.info_enum_value(index) );
		}
	}

	m_length = p_source.get_length();
	m_replaygain = p_source.get_replaygain();
#ifdef __file_info_const_impl_have_hintmap__
	if (hintmap != NULL) {
//		profiler(file_info_const_impl__copy__hintmap);
		for(t_size n=0;n<m_meta_count;n++) hintmap[n]=n;
		pfc::sort(sort_callback_hintmap_impl(meta,hintmap),m_meta_count);
	}
#endif//__file_info_const_impl_have_hintmap__
}


void file_info_const_impl::reset()
{
	m_meta_count = m_info_count = 0; m_length = 0; m_replaygain.reset();
}

t_size file_info_const_impl::meta_find_ex(const char * p_name,t_size p_name_length) const
{
#ifdef __file_info_const_impl_have_hintmap__
	if (m_hintmap != NULL) {
		t_size result = infinite;
		if (!pfc::bsearch_inline_t(m_meta_count,bsearch_callback_hintmap_impl(m_meta,m_hintmap,p_name,p_name_length),result)) return infinite;
		else return m_hintmap[result];
	} else {
		return file_info::meta_find_ex(p_name,p_name_length);
	}
#else
	return file_info::meta_find_ex(p_name,p_name_length);
#endif
}


t_size	file_info_const_impl::meta_enum_value_count(t_size p_index) const
{
	return m_meta[p_index].m_valuecount;
}

const char*	file_info_const_impl::meta_enum_value(t_size p_index,t_size p_value_number) const
{
	const meta_entry & entry = m_meta[p_index];
	if (entry.m_valuecount == 1)
		return reinterpret_cast<const char*>(entry.m_valuemap);
	else
		return entry.m_valuemap[p_value_number];
}
