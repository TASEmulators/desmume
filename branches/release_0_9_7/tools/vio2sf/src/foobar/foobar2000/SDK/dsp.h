//! Interface to a DSP chunk list. A DSP chunk list object is passed to the DSP chain each time, since DSPs are allowed to remove processed chunks or insert new ones.
class NOVTABLE dsp_chunk_list {
public:
	virtual t_size get_count() const = 0;
	virtual audio_chunk * get_item(t_size n) const = 0;
	virtual void remove_by_idx(t_size idx) = 0;
	virtual void remove_mask(const bit_array & mask) = 0;
	virtual audio_chunk * insert_item(t_size idx,t_size hint_size=0) = 0;

	audio_chunk * add_item(t_size hint_size=0) {return insert_item(get_count(),hint_size);}

	void remove_all() {remove_mask(bit_array_true());}

	double get_duration() {
		double rv = 0;
		t_size n,m = get_count();
		for(n=0;n<m;n++) rv += get_item(n)->get_duration();
		return rv;
	}

	void add_chunk(const audio_chunk * chunk) {
		audio_chunk * dst = insert_item(get_count(),chunk->get_data_length());
		if (dst) dst->copy(*chunk);
	}

	void remove_bad_chunks();
protected:
	dsp_chunk_list() {}
	~dsp_chunk_list() {}
};

class dsp_chunk_list_impl : public dsp_chunk_list//implementation
{
	pfc::list_t<pfc::rcptr_t<audio_chunk> > m_data, m_recycled;
public:
	t_size get_count() const;
	audio_chunk * get_item(t_size n) const;
	void remove_by_idx(t_size idx);
	void remove_mask(const bit_array & mask);
	audio_chunk * insert_item(t_size idx,t_size hint_size=0);
};

//! Instance of a DSP.\n
//! Implementation: Derive from dsp_impl_base instead of deriving from dsp directly.\n
//! Instantiation: Use dsp_entry static helper methods to instantiate DSPs, or dsp_chain_config / dsp_manager to deal with entire DSP chains.
class NOVTABLE dsp : public service_base {
public:
	enum {
		//! Flush whatever you need to when tracks change.
		END_OF_TRACK = 1,
		//! Flush everything.
		FLUSH = 2	
	};

	//! @param p_chunk_list List of chunks to process. The implementation may alter the list in any way, inserting chunks of different sample rate / channel configuration etc.
	//! @param p_cur_file Optional, location of currently decoded file. May be null.
	//! @param p_flags Flags. Can be null, or a combination of END_OF_TRACK and FLUSH constants.
	virtual void run(dsp_chunk_list * p_chunk_list,const metadb_handle_ptr & p_cur_file,int p_flags)=0;

	//! Flushes the DSP (reinitializes / drops any buffered data). Called after seeking, etc.
	virtual void flush() = 0;

	//! Retrieves amount of data buffered by the DSP, for syncing visualisation.
	//! @returns Amount of buffered audio data, in seconds.
	virtual double get_latency() = 0;
	//! Returns true if DSP needs to know exact track change point (eg. for crossfading, removing silence).\n
	//! Signaling this will force-flush any DSPs placed before this DSP so when it gets END_OF_TRACK, relevant chunks contain last samples of the track.\n
	//! Signaling this will often break regular gapless playback so don't use it unless you have reasons to.
	virtual bool need_track_change_mark() = 0;

	void run_abortable(dsp_chunk_list * p_chunk_list,const metadb_handle_ptr & p_cur_file,int p_flags,abort_callback & p_abort);

	FB2K_MAKE_SERVICE_INTERFACE(dsp,service_base);
};

//! Backwards-compatible extension to dsp interface, allows abortable operation. Introduced in 0.9.2.
class NOVTABLE dsp_v2 : public dsp {
public:
	//! Abortable version of dsp::run(). See dsp::run() for descriptions of parameters.
	virtual void run_v2(dsp_chunk_list * p_chunk_list,const metadb_handle_ptr & p_cur_file,int p_flags,abort_callback & p_abort) = 0;
private:
	void run(dsp_chunk_list * p_chunk_list,const metadb_handle_ptr & p_cur_file,int p_flags) {
		run_v2(p_chunk_list,p_cur_file,p_flags,abort_callback_dummy());
	}

	FB2K_MAKE_SERVICE_INTERFACE(dsp_v2,dsp);
};

//! Helper class for implementing dsps. You should derive from dsp_impl_base instead of from dsp directly.\n
//! The dsp_impl_base_t template allows you to use a custom interface class as a base class for your implementation, in case you provide extended functionality.\n
//! Use dsp_factory_t<> template to register your dsp implementation.
//! The implementation - as required by dsp_factory_t<> template - must also provide following methods:\n
//! A constructor taking const dsp_preset&, initializing the DSP with specified preset data.\n
//! static void g_get_name(pfc::string_base &); - retrieving human-readable name of the DSP to display.\n
//! static bool g_get_default_preset(dsp_preset &); - retrieving default preset for this DSP. Return value is reserved for future use and should always be true.\n
//! static GUID g_get_guid(); - retrieving GUID of your DSP implementation, to be used to identify it when storing DSP chain configuration.\n
//! static bool g_have_config_popup(); - retrieving whether your DSP implementation supplies a popup dialog for configuring it.\n
//! static void g_show_config_popup(const dsp_preset & p_data,HWND p_parent, dsp_preset_edit_callback & p_callback); - displaying your DSP's settings dialog; called only when g_have_config_popup() returns true; call p_callback.on_preset_changed() whenever user has made adjustments to the preset data.\n
template<class t_baseclass>
class dsp_impl_base_t : public t_baseclass {
private:
	typedef dsp_impl_base_t<t_baseclass> t_self;
	dsp_chunk_list * m_list;
	t_size m_chunk_ptr;
	metadb_handle* m_cur_file;
	void run_v2(dsp_chunk_list * p_list,const metadb_handle_ptr & p_cur_file,int p_flags,abort_callback & p_abort);
protected:
	bool get_cur_file(metadb_handle_ptr & p_out) {p_out = m_cur_file; return p_out.is_valid();}// call only from on_chunk / on_endoftrack (on_endoftrack will give info on track being finished); may return null !!
	
	dsp_impl_base_t() : m_list(NULL), m_cur_file(NULL), m_chunk_ptr(0) {}
	
	audio_chunk * insert_chunk(t_size p_hint_size = 0)	//call only from on_endoftrack / on_endofplayback / on_chunk
	{//hint_size - optional, amout of buffer space you want to use
		PFC_ASSERT(m_list != NULL);
		return m_list->insert_item(m_chunk_ptr++,p_hint_size);
	}


	//! To be overridden by a DSP implementation.\n
	//! Called on track change. You can use insert_chunk() to dump any data you have to flush. \n
	//! Note that you must implement need_track_change_mark() to return true if you need this method called.
	virtual void on_endoftrack(abort_callback & p_abort) = 0;
	//! To be overridden by a DSP implementation.\n
	//! Called at the end of played stream, typically at the end of last played track, to allow the DSP to return all data it has buffered-ahead.\n
	//! Use insert_chunk() to return any data you have buffered.\n
	//! Note that this call does not imply that the DSP will be destroyed next. \n
	//! This is also called on track changes if some DSP placed after your DSP requests track change marks.
	virtual void on_endofplayback(abort_callback & p_abort) = 0;
	//! To be overridden by a DSP implementation.\n
	//! Processes a chunk of audio data.\n
	//! You can call insert_chunk() from inside on_chunk() to insert any audio data before currently processed chunk.\n
	//! @param p_chunk Current chunk being processed. You can alter it in any way you like.
	//! @returns True to keep p_chunk (with alterations made inside on_chunk()) in the stream, false to remove it.
	virtual bool on_chunk(audio_chunk * p_chunk,abort_callback & p_abort) = 0;

public:
	//! To be overridden by a DSP implementation.\n
	//! Flushes the DSP (reinitializes / drops any buffered data). Called after seeking, etc.
	virtual void flush() = 0;
	//! To be overridden by a DSP implementation.\n
	//! Retrieves amount of data buffered by the DSP, for syncing visualisation.
	//! @returns Amount of buffered audio data, in seconds.
	virtual double get_latency() = 0;
	//! To be overridden by a DSP implementation.\n
	//! Returns true if DSP needs to know exact track change point (eg. for crossfading, removing silence).\n
	//! Signaling this will force-flush any DSPs placed before this DSP so when it gets on_endoftrack(), relevant chunks contain last samples of the track.\n
	//! Signaling this may interfere with gapless playback in certain scenarios (forces flush of DSPs placed before you) so don't use it unless you have reasons to.
	virtual bool need_track_change_mark() = 0;
private:
	dsp_impl_base_t(const t_self&) {throw pfc::exception_bug_check_v2();}
	const t_self & operator=(const t_self &) {throw pfc::exception_bug_check_v2();}
};

template<class t_baseclass>
void dsp_impl_base_t<t_baseclass>::run_v2(dsp_chunk_list * p_list,const metadb_handle_ptr & p_cur_file,int p_flags,abort_callback & p_abort) {
	pfc::vartoggle_t<dsp_chunk_list*> l_list_toggle(m_list,p_list);
	pfc::vartoggle_t<metadb_handle*> l_cur_file_toggle(m_cur_file,p_cur_file.get_ptr());
	
	for(m_chunk_ptr = 0;m_chunk_ptr<m_list->get_count();m_chunk_ptr++) {
		audio_chunk * c = m_list->get_item(m_chunk_ptr);
		if (c->is_empty() || !on_chunk(c,p_abort))
			m_list->remove_by_idx(m_chunk_ptr--);
	}

	if (p_flags & FLUSH) {
		on_endofplayback(p_abort);
	} else if (p_flags & END_OF_TRACK) {
		if (need_track_change_mark()) on_endoftrack(p_abort);
	}
}


typedef dsp_impl_base_t<dsp_v2> dsp_impl_base;

class NOVTABLE dsp_preset {
public:
	virtual GUID get_owner() const = 0;
	virtual void set_owner(const GUID & p_owner) = 0;
	virtual const void * get_data() const = 0;
	virtual t_size get_data_size() const = 0;
	virtual void set_data(const void * p_data,t_size p_data_size) = 0;
	virtual void set_data_from_stream(stream_reader * p_stream,t_size p_bytes,abort_callback & p_abort) = 0;

	const dsp_preset & operator=(const dsp_preset & p_source) {copy(p_source); return *this;}

	void copy(const dsp_preset & p_source) {set_owner(p_source.get_owner());set_data(p_source.get_data(),p_source.get_data_size());}

	void contents_to_stream(stream_writer * p_stream,abort_callback & p_abort) const;
	void contents_from_stream(stream_reader * p_stream,abort_callback & p_abort);
	static void g_contents_from_stream_skip(stream_reader * p_stream,abort_callback & p_abort);

	bool operator==(const dsp_preset & p_other) const {
		if (get_owner() != p_other.get_owner()) return false;
		if (get_data_size() != p_other.get_data_size()) return false;
		if (memcmp(get_data(),p_other.get_data(),get_data_size()) != 0) return false;
		return true;
	}
	bool operator!=(const dsp_preset & p_other) const {
		return !(*this == p_other);
	}
protected:
	dsp_preset() {}
	~dsp_preset() {}
};

class dsp_preset_writer : public stream_writer {
public:
	void write(const void * p_buffer,t_size p_bytes,abort_callback & p_abort) {
		p_abort.check();
		m_data.append_fromptr((const t_uint8 *) p_buffer,p_bytes);
	}
	void flush(dsp_preset & p_preset) {
		p_preset.set_data(m_data.get_ptr(),m_data.get_size());
		m_data.set_size(0);
	}
private:
	pfc::array_t<t_uint8,pfc::alloc_fast_aggressive> m_data;
};

class dsp_preset_reader : public stream_reader {
public:
	dsp_preset_reader() : m_walk(0) {}
	dsp_preset_reader(const dsp_preset_reader & p_source) : m_walk(0) {*this = p_source;}
	void init(const dsp_preset & p_preset) {
		m_data.set_data_fromptr( (const t_uint8*) p_preset.get_data(), p_preset.get_data_size() );
		m_walk = 0;
	}
	t_size read(void * p_buffer,t_size p_bytes,abort_callback & p_abort) {
		p_abort.check();
		t_size todo = pfc::min_t<t_size>(p_bytes,m_data.get_size()-m_walk);
		memcpy(p_buffer,m_data.get_ptr()+m_walk,todo);
		m_walk += todo;
		return todo;
	}
	bool is_finished() {return m_walk == m_data.get_size();}
private:
	t_size m_walk;
	pfc::array_t<t_uint8> m_data;
};

class dsp_preset_impl : public dsp_preset
{
public:
	dsp_preset_impl() {}
	dsp_preset_impl(const dsp_preset_impl & p_source) {copy(p_source);}
	dsp_preset_impl(const dsp_preset & p_source) {copy(p_source);}

	const dsp_preset_impl& operator=(const dsp_preset_impl & p_source) {copy(p_source); return *this;}
	const dsp_preset_impl& operator=(const dsp_preset & p_source) {copy(p_source); return *this;}

	GUID get_owner() const {return m_owner;}
	void set_owner(const GUID & p_owner) {m_owner = p_owner;}
	const void * get_data() const {return m_data.get_ptr();}
	t_size get_data_size() const {return m_data.get_size();}
	void set_data(const void * p_data,t_size p_data_size) {m_data.set_data_fromptr((const t_uint8*)p_data,p_data_size);}
	void set_data_from_stream(stream_reader * p_stream,t_size p_bytes,abort_callback & p_abort);
private:
	GUID m_owner;
	pfc::array_t<t_uint8> m_data;
};

class NOVTABLE dsp_preset_edit_callback {
public:
	virtual void on_preset_changed(const dsp_preset &) = 0;
private:
	dsp_preset_edit_callback(const dsp_preset_edit_callback&) {throw pfc::exception_not_implemented();}
	const dsp_preset_edit_callback & operator=(const dsp_preset_edit_callback &) {throw pfc::exception_not_implemented();}
protected:
	dsp_preset_edit_callback() {}
	~dsp_preset_edit_callback() {}
};

class NOVTABLE dsp_entry : public service_base {
public:
	virtual void get_name(pfc::string_base & p_out) = 0;
	virtual bool get_default_preset(dsp_preset & p_out) = 0;
	virtual bool instantiate(service_ptr_t<dsp> & p_out,const dsp_preset & p_preset) = 0;	
	virtual GUID get_guid() = 0;
	virtual bool have_config_popup() = 0;
	virtual bool show_config_popup(dsp_preset & p_data,HWND p_parent) = 0;


	static bool g_get_interface(service_ptr_t<dsp_entry> & p_out,const GUID & p_guid);
	static bool g_instantiate(service_ptr_t<dsp> & p_out,const dsp_preset & p_preset);
	static bool g_instantiate_default(service_ptr_t<dsp> & p_out,const GUID & p_guid);
	static bool g_name_from_guid(pfc::string_base & p_out,const GUID & p_guid);
	static bool g_dsp_exists(const GUID & p_guid);
	static bool g_get_default_preset(dsp_preset & p_out,const GUID & p_guid);
	static bool g_have_config_popup(const GUID & p_guid);
	static bool g_have_config_popup(const dsp_preset & p_preset);
	static bool g_show_config_popup(dsp_preset & p_preset,HWND p_parent);

	static void g_show_config_popup_v2(const dsp_preset & p_preset,HWND p_parent,dsp_preset_edit_callback & p_callback);

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(dsp_entry);
};

class NOVTABLE dsp_entry_v2 : public dsp_entry {
public:
	virtual void show_config_popup_v2(const dsp_preset & p_data,HWND p_parent,dsp_preset_edit_callback & p_callback) = 0;

private:
	bool show_config_popup(dsp_preset & p_data,HWND p_parent);

	FB2K_MAKE_SERVICE_INTERFACE(dsp_entry_v2,dsp_entry);
};

template<class T,class t_entry = dsp_entry>
class dsp_entry_impl_nopreset_t : public t_entry {
public:
	void get_name(pfc::string_base & p_out) {T::g_get_name(p_out);}
	bool get_default_preset(dsp_preset & p_out)
	{
		p_out.set_owner(T::g_get_guid());
		p_out.set_data(0,0);
		return true;
	}
	bool instantiate(service_ptr_t<dsp> & p_out,const dsp_preset & p_preset)
	{
		if (p_preset.get_owner() == T::g_get_guid() && p_preset.get_data_size() == 0)
		{
			p_out = new service_impl_t<T>();
			return p_out.is_valid();
		}
		else return false;
	}
	GUID get_guid() {return T::g_get_guid();}

	bool have_config_popup() {return false;}
	bool show_config_popup(dsp_preset & p_data,HWND p_parent) {return false;}
};

template<class T, class t_entry = dsp_entry_v2>
class dsp_entry_impl_t : public t_entry {
public:
	void get_name(pfc::string_base & p_out) {T::g_get_name(p_out);}
	bool get_default_preset(dsp_preset & p_out) {return T::g_get_default_preset(p_out);}
	bool instantiate(service_ptr_t<dsp> & p_out,const dsp_preset & p_preset) {
		if (p_preset.get_owner() == T::g_get_guid()) {
			p_out = new service_impl_t<T>(p_preset);
			return true;
		}
		else return false;
	}
	GUID get_guid() {return T::g_get_guid();}

	bool have_config_popup() {return T::g_have_config_popup();}
	bool show_config_popup(dsp_preset & p_data,HWND p_parent) {return T::g_show_config_popup(p_data,p_parent);}
	//void show_config_popup_v2(const dsp_preset & p_data,HWND p_parent,dsp_preset_edit_callback & p_callback) {T::g_show_config_popup(p_data,p_parent,p_callback);}
};

template<class T, class t_entry = dsp_entry_v2>
class dsp_entry_v2_impl_t : public t_entry {
public:
	void get_name(pfc::string_base & p_out) {T::g_get_name(p_out);}
	bool get_default_preset(dsp_preset & p_out) {return T::g_get_default_preset(p_out);}
	bool instantiate(service_ptr_t<dsp> & p_out,const dsp_preset & p_preset) {
		if (p_preset.get_owner() == T::g_get_guid()) {
			p_out = new service_impl_t<T>(p_preset);
			return true;
		}
		else return false;
	}
	GUID get_guid() {return T::g_get_guid();}

	bool have_config_popup() {return T::g_have_config_popup();}
	//bool show_config_popup(dsp_preset & p_data,HWND p_parent) {return T::g_show_config_popup(p_data,p_parent);}
	void show_config_popup_v2(const dsp_preset & p_data,HWND p_parent,dsp_preset_edit_callback & p_callback) {T::g_show_config_popup(p_data,p_parent,p_callback);}
};


template<class T>
class dsp_factory_nopreset_t : public service_factory_single_t<dsp_entry_impl_nopreset_t<T> > {};

template<class T>
class dsp_factory_t : public service_factory_single_t<dsp_entry_v2_impl_t<T> > {};

class NOVTABLE dsp_chain_config
{
public:
	virtual t_size get_count() const = 0;
	virtual const dsp_preset & get_item(t_size p_index) const = 0;
	virtual void replace_item(const dsp_preset & p_data,t_size p_index) = 0;
	virtual void insert_item(const dsp_preset & p_data,t_size p_index) = 0;
	virtual void remove_mask(const bit_array & p_mask) = 0;
	
	void remove_item(t_size p_index);
	void remove_all();
	void add_item(const dsp_preset & p_data);
	void copy(const dsp_chain_config & p_source);

	const dsp_chain_config & operator=(const dsp_chain_config & p_source) {copy(p_source); return *this;}

	void contents_to_stream(stream_writer * p_stream,abort_callback & p_abort) const;
	void contents_from_stream(stream_reader * p_stream,abort_callback & p_abort);

	void instantiate(service_list_t<dsp> & p_out);

	void get_name_list(pfc::string_base & p_out) const;
};

FB2K_STREAM_READER_OVERLOAD(dsp_chain_config) {
	value.contents_from_stream(&stream.m_stream, stream.m_abort); return stream;
}

FB2K_STREAM_WRITER_OVERLOAD(dsp_chain_config) {
	value.contents_to_stream(&stream.m_stream, stream.m_abort); return stream;
}

class dsp_chain_config_impl : public dsp_chain_config
{
public:
	dsp_chain_config_impl() {}
	dsp_chain_config_impl(const dsp_chain_config & p_source) {copy(p_source);}
	dsp_chain_config_impl(const dsp_chain_config_impl & p_source) {copy(p_source);}
	t_size get_count() const;
	const dsp_preset & get_item(t_size p_index) const;
	void replace_item(const dsp_preset & p_data,t_size p_index);
	void insert_item(const dsp_preset & p_data,t_size p_index);
	void remove_mask(const bit_array & p_mask);

	const dsp_chain_config_impl & operator=(const dsp_chain_config & p_source) {copy(p_source); return *this;}
	const dsp_chain_config_impl & operator=(const dsp_chain_config_impl & p_source) {copy(p_source); return *this;}

	~dsp_chain_config_impl();
private:
	pfc::ptr_list_t<dsp_preset_impl> m_data;
};

class cfg_dsp_chain_config : public cfg_var {
protected:
	void get_data_raw(stream_writer * p_stream,abort_callback & p_abort);
	void set_data_raw(stream_reader * p_stream,t_size p_sizehint,abort_callback & p_abort);
public:
	void reset();
	inline cfg_dsp_chain_config(const GUID & p_guid) : cfg_var(p_guid) {}
	t_size get_count() const {return m_data.get_count();}
	const dsp_preset & get_item(t_size p_index) const {return m_data.get_item(p_index);}
	bool get_data(dsp_chain_config & p_data) const;
	void set_data(const dsp_chain_config & p_data);
private:
	dsp_chain_config_impl m_data;
	
};




//! Helper.
class dsp_preset_parser : public stream_reader_formatter<> {
public:
	dsp_preset_parser(const dsp_preset & in) : m_data(in), _m_stream(in.get_data(),in.get_data_size()), stream_reader_formatter(_m_stream,_m_abort) {}

	void reset() {_m_stream.reset();}
	t_size get_remaining() const {return _m_stream.get_remaining();}

	void assume_empty() const {
		if (get_remaining() != 0) throw exception_io_data();
	}

	GUID get_owner() const {return m_data.get_owner();}
private:
	const dsp_preset & m_data;
	abort_callback_dummy _m_abort;
	stream_reader_memblock_ref _m_stream;
};

//! Helper.
class dsp_preset_builder : public stream_writer_formatter<> {
public:
	dsp_preset_builder() : stream_writer_formatter(_m_stream,_m_abort) {}
	void finish(const GUID & id, dsp_preset & out) {
		out.set_owner(id);
		out.set_data(_m_stream.m_buffer.get_ptr(), _m_stream.m_buffer.get_size());
	}
	void reset() {
		_m_stream.m_buffer.set_size(0);
	}
private:
	abort_callback_dummy _m_abort;
	stream_writer_buffer_simple _m_stream;
};
