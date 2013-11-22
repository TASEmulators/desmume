//! Generic interface for a memory block; used by various other interfaces to return memory blocks while allowing caller to allocate.
class NOVTABLE mem_block_container {
public:
	virtual const void * get_ptr() const = 0;
	virtual void * get_ptr() = 0;
	virtual t_size get_size() const = 0;
	virtual void set_size(t_size p_size) = 0;
	
	void from_stream(stream_reader * p_stream,t_size p_bytes,abort_callback & p_abort);

	void set(const void * p_buffer,t_size p_size);
	void set(const mem_block_container & source) {copy(source);}
	template<typename t_source> void set(const t_source & source) {
		pfc::static_assert<sizeof(source[0]) == 1>();
		set(source.get_ptr(), source.get_size());
	}

	inline void copy(const mem_block_container & p_source) {set(p_source.get_ptr(),p_source.get_size());}
	inline void reset() {set_size(0);}

	const mem_block_container & operator=(const mem_block_container & p_source) {copy(p_source);return *this;}

protected:
	mem_block_container() {}
	~mem_block_container() {}
};

//! mem_block_container implementation.
template<template<typename> class t_alloc = pfc::alloc_standard>
class mem_block_container_impl_t : public mem_block_container {
public:
	const void * get_ptr() const {return m_data.get_ptr();}
	void * get_ptr() {return m_data.get_ptr();}
	t_size get_size() const {return m_data.get_size();}
	void set_size(t_size p_size) {
		m_data.set_size(p_size);
	}
private:
	pfc::array_t<t_uint8,t_alloc> m_data;
};

typedef mem_block_container_impl_t<> mem_block_container_impl;

class mem_block_container_temp_impl : public mem_block_container {
public:
	mem_block_container_temp_impl(void * p_buffer,t_size p_size) : m_buffer(p_buffer), m_buffer_size(p_size), m_size(0) {}
	const void * get_ptr() const {return m_buffer;}
	void * get_ptr() {return m_buffer;}
	t_size get_size() const {return m_size;}
	void set_size(t_size p_size) {if (p_size > m_buffer_size) throw pfc::exception_overflow(); m_size = p_size;}
private:
	t_size m_size,m_buffer_size;
	void * m_buffer;
};

template<typename t_ref>
class mem_block_container_ref_impl : public mem_block_container {
public:
	mem_block_container_ref_impl(t_ref & ref) : m_ref(ref) {
		pfc::static_assert<sizeof(ref[0]) == 1>();
	}
	const void * get_ptr() const {return m_ref.get_ptr();}
	void * get_ptr() {return m_ref.get_ptr();}
	t_size get_size() const {return m_ref.get_size();}
	void set_size(t_size p_size) {m_ref.set_size(p_size);}
private:
	t_ref & m_ref;
};