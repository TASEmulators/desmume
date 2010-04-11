//HINT: for info on how to generate an embedded cuesheet enabled input, see the end of this header.

//to be moved somewhere else later
namespace file_info_record_helper {

	class __file_info_record__info__enumerator {
	public:
		__file_info_record__info__enumerator(file_info & p_out) : m_out(p_out) {}
		void operator() (const char * p_name,const char * p_value) {m_out.__info_add_unsafe(p_name,p_value);}
	private:
		file_info & m_out;
	};

	class __file_info_record__meta__enumerator {
	public:
		__file_info_record__meta__enumerator(file_info & p_out) : m_out(p_out) {}
		template<typename t_value> void operator() (const char * p_name,const t_value & p_value) {
			t_size index = infinite;
			for(typename t_value::const_iterator iter = p_value.first(); iter.is_valid(); ++iter) {
				if (index == infinite) index = m_out.__meta_add_unsafe(p_name,*iter);
				else m_out.meta_add_value(index,*iter);
			}
		}
	private:
		file_info & m_out;
	};

	class file_info_record {
	public:
		typedef pfc::chain_list_v2_t<pfc::string8> t_meta_value;
		typedef pfc::map_t<pfc::string8,t_meta_value,file_info::field_name_comparator> t_meta_map;
		typedef pfc::map_t<pfc::string8,pfc::string8,file_info::field_name_comparator> t_info_map;

		file_info_record() : m_replaygain(replaygain_info_invalid), m_length(0) {}

		replaygain_info get_replaygain() const {return m_replaygain;}
		void set_replaygain(const replaygain_info & p_replaygain) {m_replaygain = p_replaygain;}
		double get_length() const {return m_length;}
		void set_length(double p_length) {m_length = p_length;}

		void reset() {
			m_meta.remove_all(); m_info.remove_all();
			m_length = 0;
			m_replaygain = replaygain_info_invalid;
		}

		void from_info_overwrite_info(const file_info & p_info) {
			for(t_size infowalk = 0, infocount = p_info.info_get_count(); infowalk < infocount; ++infowalk) {
				m_info.set(p_info.info_enum_name(infowalk),p_info.info_enum_value(infowalk));
			}
		}
		void from_info_overwrite_meta(const file_info & p_info) {
			for(t_size metawalk = 0, metacount = p_info.meta_get_count(); metawalk < metacount; ++metawalk) {
				const t_size valuecount = p_info.meta_enum_value_count(metawalk);
				if (valuecount > 0) {
					t_meta_value & entry = m_meta.find_or_add(p_info.meta_enum_name(metawalk));
					entry.remove_all();
					for(t_size valuewalk = 0; valuewalk < valuecount; ++valuewalk) {
						entry.add_item(p_info.meta_enum_value(metawalk,valuewalk));
					}
				}
			}
		}

		void from_info_overwrite_rg(const file_info & p_info) {
			m_replaygain = replaygain_info::g_merge(m_replaygain,p_info.get_replaygain());
		}

		template<typename t_source>
		void overwrite_meta(const t_source & p_meta) {
			m_meta.overwrite(p_meta);
		}
		template<typename t_source>
		void overwrite_info(const t_source & p_info) {
			m_info.overwrite(p_info);
		}

		void merge_overwrite(const file_info & p_info) {
			from_info_overwrite_info(p_info);
			from_info_overwrite_meta(p_info);
			from_info_overwrite_rg(p_info);
		}

		void transfer_meta_entry(const char * p_name,const file_info & p_info,t_size p_index) {
			const t_size count = p_info.meta_enum_value_count(p_index);
			if (count == 0) {
				m_meta.remove(p_name);
			} else {
				t_meta_value & val = m_meta.find_or_add(p_name);
				val.remove_all();
				for(t_size walk = 0; walk < count; ++walk) {
					val.add_item(p_info.meta_enum_value(p_index,walk));
				}
			}
		}

		void meta_set(const char * p_name,const char * p_value) {
			m_meta.find_or_add(p_name).set_single(p_value);
		}

		const t_meta_value * meta_query_ptr(const char * p_name) const {
			return m_meta.query_ptr(p_name);
		}


		void from_info_set_meta(const file_info & p_info) {
			m_meta.remove_all();
			from_info_overwrite_meta(p_info);
		}

		void from_info(const file_info & p_info) {
			reset();
			m_length = p_info.get_length();
			m_replaygain = p_info.get_replaygain();
			from_info_overwrite_meta(p_info);
			from_info_overwrite_info(p_info);
		}
		void to_info(file_info & p_info) const {
			p_info.reset();
			p_info.set_length(m_length);
			p_info.set_replaygain(m_replaygain);
			m_info.enumerate(__file_info_record__info__enumerator(p_info));
			m_meta.enumerate(__file_info_record__meta__enumerator(p_info));
		}

		template<typename t_callback> void enumerate_meta(t_callback & p_callback) const {m_meta.enumerate(p_callback);}
		template<typename t_callback> void enumerate_meta(t_callback & p_callback) {m_meta.enumerate(p_callback);}

	//private:
		t_meta_map m_meta;
		t_info_map m_info;
		replaygain_info m_replaygain;
		double m_length;
	};

}//namespace file_info_record_helper


namespace cue_parser
{
	struct cue_entry {
		pfc::string8 m_file;
		unsigned m_track_number;
		t_cuesheet_index_list m_indexes;
	};

	typedef pfc::chain_list_v2_t<cue_entry> t_cue_entry_list;


	PFC_DECLARE_EXCEPTION(exception_bad_cuesheet,exception_io_data,"Invalid cuesheet");

	//! Throws exception_bad_cuesheet on failure.
	void parse(const char *p_cuesheet,t_cue_entry_list & p_out);
	//! Throws exception_bad_cuesheet on failure.
	void parse_info(const char *p_cuesheet,file_info & p_info,unsigned p_index);
	//! Throws exception_bad_cuesheet on failure.
	void parse_full(const char * p_cuesheet,cue_creator::t_entry_list & p_out);

	

	struct track_record {
		file_info_record_helper::file_info_record m_info;
		pfc::string8 m_file,m_flags;
		t_cuesheet_index_list m_index_list;
	};

	typedef pfc::map_t<unsigned,track_record> track_record_list;

	class embeddedcue_metadata_manager {
	public:
		void get_tag(file_info & p_info) const;
		void set_tag(file_info const & p_info);

		void get_track_info(unsigned p_track,file_info & p_info) const;
		void set_track_info(unsigned p_track,file_info const & p_info);
		void query_track_offsets(unsigned p_track,double & p_begin,double & p_length) const;
		bool have_cuesheet() const;
		unsigned remap_trackno(unsigned p_index) const;
		t_size get_cue_track_count() const;
	private:
		track_record_list m_content;
	};






	class _decoder_wrapper {
	public:
		virtual bool run(audio_chunk & p_chunk,abort_callback & p_abort) = 0;
		virtual void seek(double p_seconds,abort_callback & p_abort) = 0;
		virtual bool get_dynamic_info(file_info & p_out, double & p_timestamp_delta) = 0;
		virtual bool get_dynamic_info_track(file_info & p_out, double & p_timestamp_delta) = 0;
		virtual void on_idle(abort_callback & p_abort) = 0;
		virtual bool run_raw(audio_chunk & p_chunk, mem_block_container & p_raw, abort_callback & p_abort) = 0;
		virtual void set_logger(event_logger::ptr ptr) = 0;
		virtual ~_decoder_wrapper() {}
	};

	template<typename t_input>
	class _decoder_wrapper_simple : public _decoder_wrapper {
	public:
		void initialize(service_ptr_t<file> p_filehint,const char * p_path,unsigned p_flags,abort_callback & p_abort) {
			m_input.open(p_filehint,p_path,input_open_decode,p_abort);
			m_input.decode_initialize(p_flags,p_abort);
		}
		bool run(audio_chunk & p_chunk,abort_callback & p_abort) {return m_input.decode_run(p_chunk,p_abort);}
		void seek(double p_seconds,abort_callback & p_abort) {m_input.decode_seek(p_seconds,p_abort);}
		bool get_dynamic_info(file_info & p_out, double & p_timestamp_delta) {return m_input.decode_get_dynamic_info(p_out,p_timestamp_delta);}
		bool get_dynamic_info_track(file_info & p_out, double & p_timestamp_delta) {return m_input.decode_get_dynamic_info_track(p_out,p_timestamp_delta);}
		void on_idle(abort_callback & p_abort) {m_input.decode_on_idle(p_abort);}
		bool run_raw(audio_chunk & p_chunk, mem_block_container & p_raw, abort_callback & p_abort) {return m_input.decode_run_raw(p_chunk, p_raw, p_abort);}
		void set_logger(event_logger::ptr ptr) {m_input.set_logger(ptr);}
	private:
		t_input m_input;
	};

	class _decoder_wrapper_cue : public _decoder_wrapper {
	public:
		void open(service_ptr_t<file> p_filehint,const playable_location & p_location,unsigned p_flags,abort_callback & p_abort,double p_start,double p_length) {
			m_input.open(p_filehint,p_location,p_flags,p_abort,p_start,p_length);
		}
		bool run(audio_chunk & p_chunk,abort_callback & p_abort) {return m_input.run(p_chunk,p_abort);}
		void seek(double p_seconds,abort_callback & p_abort) {m_input.seek(p_seconds,p_abort);}
		bool get_dynamic_info(file_info & p_out, double & p_timestamp_delta) {return m_input.get_dynamic_info(p_out,p_timestamp_delta);}
		bool get_dynamic_info_track(file_info & p_out, double & p_timestamp_delta) {return m_input.get_dynamic_info_track(p_out,p_timestamp_delta);}
		void on_idle(abort_callback & p_abort) {m_input.on_idle(p_abort);}
		bool run_raw(audio_chunk & p_chunk, mem_block_container & p_raw, abort_callback & p_abort) {return m_input.run_raw(p_chunk, p_raw, p_abort);}
		void set_logger(event_logger::ptr ptr) {m_input.set_logger(ptr);}
	private:
		input_helper_cue m_input;
	};

	template<typename t_base>
	class input_wrapper_cue_t {
	public:
		input_wrapper_cue_t() {}
		~input_wrapper_cue_t() {}

		void open(service_ptr_t<file> p_filehint,const char * p_path,t_input_open_reason p_reason,abort_callback & p_abort) {
			m_path = p_path;

			m_file = p_filehint;
			input_open_file_helper(m_file,p_path,p_reason,p_abort);

			{
				file_info_impl info;
				t_base instance;
				instance.open(m_file,p_path,p_reason,p_abort);
				instance.get_info(info,p_abort);
				m_meta.set_tag(info);
			}
		}

		t_uint32 get_subsong_count() {
			return m_meta.have_cuesheet() ? m_meta.get_cue_track_count() : 1;
		}

		t_uint32 get_subsong(t_uint32 p_index) {
			return m_meta.have_cuesheet() ? m_meta.remap_trackno(p_index) : 0;
		}

		void get_info(t_uint32 p_subsong,file_info & p_info,abort_callback & p_abort) {
			if (p_subsong == 0) {
				m_meta.get_tag(p_info);
			} else {
				m_meta.get_track_info(p_subsong,p_info);
			}
		}

		t_filestats get_file_stats(abort_callback & p_abort) {return m_file->get_stats(p_abort);}

		void decode_initialize(t_uint32 p_subsong,unsigned p_flags,abort_callback & p_abort) {
			m_decoder.release();
			if (p_subsong == 0) {
				pfc::rcptr_t<_decoder_wrapper_simple<t_base> > temp;
				temp.new_t();
				m_file->reopen(p_abort);
				temp->initialize(m_file,m_path,p_flags,p_abort);
				m_decoder = temp;
			} else {
				double start,length;
				m_meta.query_track_offsets(p_subsong,start,length);

				pfc::rcptr_t<_decoder_wrapper_cue> temp;
				temp.new_t();
				m_file->reopen(p_abort);
				temp->open(m_file,make_playable_location(m_path,0),p_flags & ~input_flag_no_seeking,p_abort,start,length);
				m_decoder = temp;
			}
			if (m_logger.is_valid()) m_decoder->set_logger(m_logger);
		}

		bool decode_run(audio_chunk & p_chunk,abort_callback & p_abort) {
			return m_decoder->run(p_chunk,p_abort);
		}
		
		void decode_seek(double p_seconds,abort_callback & p_abort) {
			m_decoder->seek(p_seconds,p_abort);
		}
		
		bool decode_can_seek() {return true;}
		
		bool decode_run_raw(audio_chunk & p_chunk, mem_block_container & p_raw, abort_callback & p_abort) {
			return m_decoder->run_raw(p_chunk, p_raw, p_abort);
		}
		void set_logger(event_logger::ptr ptr) {
			m_logger = ptr;
			if (m_decoder.is_valid()) m_decoder->set_logger(ptr);
		}

		bool decode_get_dynamic_info(file_info & p_out, double & p_timestamp_delta) {
			return m_decoder->get_dynamic_info(p_out,p_timestamp_delta);
		}

		bool decode_get_dynamic_info_track(file_info & p_out, double & p_timestamp_delta) {
			return m_decoder->get_dynamic_info_track(p_out,p_timestamp_delta);
		}

		void decode_on_idle(abort_callback & p_abort) {
			m_decoder->on_idle(p_abort);
		}

		void retag_set_info(t_uint32 p_subsong,const file_info & p_info,abort_callback & p_abort) {
			pfc::dynamic_assert(m_decoder.is_empty());
			if (p_subsong == 0) {
				m_meta.set_tag(p_info);
			} else {
				m_meta.set_track_info(p_subsong,p_info);
			}
		}

		void retag_commit(abort_callback & p_abort) {
			pfc::dynamic_assert(m_decoder.is_empty());

			file_info_impl info;
			m_meta.get_tag(info);
			
			{
				t_base instance;
				m_file->reopen(p_abort);
				instance.open(m_file,m_path,input_open_info_write,p_abort);
				instance.retag(pfc::safe_cast<const file_info&>(info),p_abort);
				info.reset();
				instance.get_info(info,p_abort);
				m_meta.set_tag(info);
			}
		}

		inline static bool g_is_our_content_type(const char * p_content_type) {return t_base::g_is_our_content_type(p_content_type);}
		inline static bool g_is_our_path(const char * p_path,const char * p_extension) {return t_base::g_is_our_path(p_path,p_extension);}

	private:
		pfc::rcptr_t<_decoder_wrapper> m_decoder;

		file_info_impl m_info;
		pfc::string8 m_path;
		service_ptr_t<file> m_file;

		embeddedcue_metadata_manager m_meta;
		event_logger::ptr m_logger;
	};

	template<typename I>
	class chapterizer_impl_t : public chapterizer
	{
	public:
		bool is_our_file(const char * p_path,abort_callback & p_abort) 
		{
			return I::g_is_our_path(p_path,pfc::string_extension(p_path));
		}

		void set_chapters(const char * p_path,chapter_list const & p_list,abort_callback & p_abort) {
			
			
			input_wrapper_cue_t<I> instance;
			instance.open(0,p_path,input_open_info_write,p_abort);

			//stamp the cuesheet first
			{
				file_info_impl info;
				instance.get_info(0,info,p_abort);

				pfc::string_formatter cuesheet;
							
				{
					cue_creator::t_entry_list entries;
					t_size n, m = p_list.get_chapter_count();
									
					double offset_acc = 0;
					for(n=0;n<m;n++)
					{
						cue_creator::t_entry_list::iterator entry;
						entry = entries.insert_last();
						entry->m_infos = p_list.get_info(n);
						entry->m_file = "CDImage.wav";
						entry->m_track_number = (unsigned)(n+1);
						entry->m_index_list.from_infos(entry->m_infos,offset_acc);
						offset_acc += entry->m_infos.get_length();
					}
					cue_creator::create(cuesheet,entries);
				}

				info.meta_set("cuesheet",cuesheet);

				instance.retag_set_info(0,info,p_abort);
			}
			//stamp per-chapter infos
			for(t_size walk = 0, total = p_list.get_chapter_count(); walk < total; ++walk) {
				instance.retag_set_info(walk + 1, p_list.get_info(walk),p_abort);
			}

			instance.retag_commit(p_abort);
		}

		void get_chapters(const char * p_path,chapter_list & p_list,abort_callback & p_abort) {

			input_wrapper_cue_t<I> instance;


			instance.open(0,p_path,input_open_info_read,p_abort);

			const t_uint32 total = instance.get_subsong_count();

			p_list.set_chapter_count(total);
			for(t_uint32 walk = 0; walk < total; ++walk) {
				file_info_impl info;
				instance.get_info(instance.get_subsong(walk),info,p_abort);
				p_list.set_info(walk,info);
			}
		}
	};

};

//! Wrapper template for generating embedded cuesheet enabled inputs.
//! t_input_impl is a singletrack input implementation (see input_singletrack_impl for method declarations).
//! To declare an embedded cuesheet enabled input, change your input declaration from input_singletrack_factory_t<myinput> to input_cuesheet_factory_t<myinput>.
template<typename t_input_impl>
class input_cuesheet_factory_t {
public:
	input_factory_ex_t<cue_parser::input_wrapper_cue_t<t_input_impl>,0,input_decoder_v2> m_input_factory;
	service_factory_single_t<cue_parser::chapterizer_impl_t<t_input_impl> > m_chapterizer_factory;	
};