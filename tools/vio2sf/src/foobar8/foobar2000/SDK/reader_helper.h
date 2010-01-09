#ifndef _READER_HELPER_H_
#define _READER_HELPER_H_

//helper
class file_path_canonical : public string8
{
public:
	file_path_canonical(const char * src)
	{
		file::g_get_canonical_path(src,*this);
	}
	operator const char * () {return get_ptr();}
};

class file_path_display : public string8
{
public:
	file_path_display(const char * src)
	{
		file::g_get_display_path(src,*this);
	}
	operator const char * () {return get_ptr();}
};


//HELPER, allows short seek back on unseekable reader, readonly
template<class T>
class reader_seekback_t : public T
{
protected:
	bool aborting;
	mem_block_t<char> buffer;
	virtual ~reader_seekback_t() {}
	virtual int seekback_read(void * buffer,int size)=0;
	__int64 read_pos,reader_pos;
	int buffer_pos,buffered;
public:
	enum 
	{
		DEFAULT_SEEKBACK_SIZE = 0x10000
	};
	reader_seekback_t(int size = DEFAULT_SEEKBACK_SIZE) : buffer(size) {aborting=0;read_pos=0;reader_pos=0;buffered=0;buffer_pos=0;}
	virtual unsigned read(void *buffer, unsigned length);
	virtual unsigned write(const void *buffer, unsigned length) {return 0;}
//	virtual __int64 get_length() {return m_reader->get_length();}
	virtual __int64 get_position() {return read_pos;}
	virtual bool set_eof() {return false;}
	virtual bool seek(__int64 position);
	virtual bool can_seek() {return 0;}
	virtual void abort() {aborting=1;}
};

typedef reader_seekback_t< reader > reader_seekback;

class reader_seekback_wrap : public reader_seekback
{
protected:
	reader * m_reader;
	virtual int seekback_read(void * buffer,int size)
	{
		return m_reader->read(buffer,size);
	}
public:
	reader_seekback_wrap(reader * p_reader,int size)
		: reader_seekback(size), m_reader(p_reader) {}
	virtual bool open(const char *path,MODE mode) {return false;}//dummy
	virtual __int64 get_length() {return m_reader->get_length();}
	virtual void abort() {m_reader->abort();reader_seekback::abort();}
};


class reader_backbuffer_wrap : public reader_seekback_wrap
{
public:
	reader_backbuffer_wrap(reader * p_reader,int size)
		: reader_seekback_wrap(p_reader,size) {}
	virtual bool seek(__int64 position);
};


class reader_membuffer : public reader_filetime
{
public:
	mem_block_t<char> buffer;
	int buffer_pos;
	reader_membuffer(__int64 modtime) : m_modtime(modtime) {}
	virtual unsigned write(const void*, unsigned) {return 0;}
	__int64 m_modtime;
protected:
	bool init(reader * src)
	{
		if (src->is_in_memory()) return false;//already buffered
		__int64 size64 = src->get_length();
		if (size64<=0 || size64>0x7FFFFFFF) return false;
		unsigned size = (unsigned)size64;
		if (!buffer.set_size(size))
		{
			return false;
		}
		src->seek(0);
		buffer_pos=0;
		if (src->read(buffer.get_ptr(),size)!=size)
		{
			return false;
		}
		return true;
	}
public:
	static reader * create(reader * src,__int64 modtime = 0)
	{
		reader_membuffer * ptr = new service_impl_p1_t<reader_membuffer,__int64>(modtime);
		if (ptr)
		{
			if (!ptr->init(src)) {delete ptr; ptr = 0;}
		}
		return ptr;
	}

	virtual bool open(const char *path,MODE mode) {return false;}//dummy

	virtual unsigned read(void *out, unsigned length)
	{
		unsigned max = buffer.get_size() - buffer_pos;
		if (length>max) length = max;
		memcpy(out,buffer.get_ptr()+buffer_pos,length);
		buffer_pos += length;		
		return length;
	}

	virtual int write(const void *buffer, int length) {return 0;}
	virtual __int64 get_length() {return buffer.get_size();}
	virtual __int64 get_position() {return buffer_pos;}
	virtual bool set_eof() {return 0;}
	virtual bool seek(__int64 position)
	{
		if (position>(__int64)buffer.get_size())
			position = (__int64)buffer.get_size();
		else if (position<0) position=0;

		buffer_pos = (int)position;

		return true;
	}
	virtual bool can_seek() {return true;}
	virtual bool is_in_memory() {return true;}
	virtual __int64 get_modification_time() {return m_modtime;}
};

class reader_limited : public reader
{
	virtual bool open(const char * filename,MODE mode) {return false;}//dummy
	reader * r;
	__int64 begin;
	__int64 end;
	virtual unsigned write(const void * , unsigned) {return 0;}
public:
	reader_limited() {r=0;begin=0;end=0;}
	reader_limited(reader * p_r,__int64 p_begin,__int64 p_end)
	{
		r = p_r;
		begin = p_begin;
		end = p_end;
		r->seek(begin);
	}

	void init(reader * p_r,__int64 p_begin,__int64 p_end)
	{
		r = p_r;
		begin = p_begin;
		end = p_end;
		r->seek(begin);
	}

	virtual unsigned read(void *buffer, unsigned length)
	{
		__int64 pos = r->get_position();
		if ((__int64)length > end - pos) length = (unsigned)(end - pos);
		return r->read(buffer,length);
	}

	virtual __int64 get_length()
	{
		return end-begin;
	}
	virtual __int64 get_position()
	{
		return r->get_position()-begin;
	}

	virtual bool seek(__int64 position)
	{
		return r->seek(position+begin);
	}
	virtual bool can_seek() {return r->can_seek();}
};


template<class T>
bool reader_seekback_t<T>::seek(__int64 position)
{
	if (aborting) return false;
	if (position>reader_pos)
	{
		read_pos = reader_pos;
		__int64 skip = (int)(position-reader_pos);
		char temp[256];
		while(skip>0)
		{
			__int64 delta = sizeof(temp);
			if (delta>skip) delta=skip;
			if (read(temp,(int)delta)!=delta) return 0;
			skip-=delta;
		}
		return true;
	}
	else if (reader_pos-position>(__int64)buffered)
	{
		return false;
	}
	else
	{
		read_pos = position;
		return true;
	}
}
template<class T>
unsigned reader_seekback_t<T>::read(void *p_out, unsigned length)
{
	if (aborting) return 0;
	//buffer can be null
	int done = 0;
	char * out = (char*)p_out;
	if (read_pos<reader_pos)
	{
		if (reader_pos-read_pos>(__int64)buffered) return 0;
		unsigned delta = (unsigned)(reader_pos-read_pos);
		if (delta>length) delta = length;
		buffer.read_circular(buffer_pos - (unsigned)(reader_pos-read_pos),out,delta);
		read_pos+=delta;
		length-=delta;
		out+=delta;
		done += delta;
	}
	if (length>0)
	{
		unsigned delta = seekback_read(out,length);
		buffer_pos = buffer.write_circular(buffer_pos,out,delta);
		read_pos+=delta;
		reader_pos+=delta;
		length-=delta;
		out+=delta;
		done+=delta;

		buffered+=delta;
		if (buffered>(int)buffer.get_size()) buffered=buffer.get_size();
	}

	return done;
}



#endif//_READER_HELPER_H_