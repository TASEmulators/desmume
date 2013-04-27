#ifndef _READER_HELPER_MEM_
#define _READER_HELPER_MEM_

template<class T>
class reader_mem_base_t : public T
{
private:
	char * data;
	unsigned size,pos;
protected:
	bool free_on_exit;
	void mem_init(void * p_data,int p_size)
	{
		if (data && free_on_exit) free(data);
		data = (char*)p_data;
		size = p_size;
		pos=0;
	}
public:
	reader_mem_base_t() {data=0;size=0;pos=0;free_on_exit=true;}
	~reader_mem_base_t() {if (free_on_exit && data) free(data);}

	virtual unsigned write(const void*, unsigned) {return 0;}

	virtual unsigned read(void *buffer, unsigned length)
	{
		if (pos + length > size) length = size - pos;
		memcpy(buffer,data+pos,length);
		pos+=length;
		return length;
	}

	virtual __int64 get_length()
	{
		return (__int64)size;
	}
	virtual __int64 get_position()
	{
		return (__int64)pos;
	}

	virtual bool seek(__int64 position)
	{
		if (position < 0) position=0;
		else if (position>(__int64)size) position=size;
		pos = (int)position;
		return true;
	}
	virtual bool is_in_memory() {return 1;}
};

#define reader_mem_base reader_mem_base_t< service_impl_t<reader> >

class reader_mem_temp : public reader_mem_base_t< service_impl_t<reader_filetime> >
{
	__int64 m_time;
	virtual bool open(const char * filename,MODE mode) {return 0;}//dummy
	virtual __int64 get_modification_time() {return m_time;}
public:
	reader_mem_temp(void * p_data,int p_size,__int64 p_time = 0) : m_time(p_time)
	{
		free_on_exit = false;
		mem_init(p_data,p_size);
	}
};


class reader_mem : public reader_mem_base
{
	virtual bool open(const char * filename,MODE mode) {return 0;}//dummy
public:
	reader_mem(void * p_data,int p_size)
	{
		free_on_exit = true;
		mem_init(p_data,p_size);
	}
};

#endif //_READER_HELPER_MEM_