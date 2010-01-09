#ifndef _PFC_CFG_VAR_H_
#define _PFC_CFG_VAR_H_

class cfg_var_notify;

//IMPORTANT:
//cfg_var objects are intended ONLY to be created statically !!!!


class NOVTABLE cfg_var
{
private:
	string_simple var_name;
	static cfg_var * list;
	cfg_var * next;
	cfg_var_notify * notify_list;

public:
	class NOVTABLE write_config_callback
	{
	public:
		inline void write_byte(BYTE param) {write(&param,1);}
		void write_dword_le(DWORD param);
		void write_dword_be(DWORD param);
		void write_guid_le(const GUID& param);
		void write_string(const char * param);
		virtual void write(const void * ptr,unsigned bytes)=0;
	};

	class write_config_callback_i : public write_config_callback
	{
	public:
		mem_block data;
		write_config_callback_i() {data.set_mem_logic(mem_block::ALLOC_FAST_DONTGODOWN);}
		virtual void write(const void * ptr,unsigned bytes) {data.append(ptr,bytes);}
	};

	class write_config_callback_i_ref : public write_config_callback
	{
	public:
		mem_block & data;
		write_config_callback_i_ref(mem_block & out) : data(out) {}
		virtual void write(const void * ptr,unsigned bytes) {data.append(ptr,bytes);}
	};
	

	class read_config_helper
	{
		const unsigned char * ptr;
		unsigned remaining;
	public:
		read_config_helper(const void * p_ptr,unsigned bytes) : ptr((const unsigned char*)p_ptr), remaining(bytes) {}
		bool read(void * out,unsigned bytes);
		bool read_dword_le(DWORD & val);
		bool read_dword_be(DWORD & val);
		inline bool read_byte(BYTE & val) {return read(&val,1)==1;}
		bool read_guid_le(GUID & val);
		bool read_string(string_base & out);
		inline unsigned get_remaining() const {return remaining;}
		inline const void * get_ptr() const {return ptr;}
		inline void advance(unsigned delta) {assert(delta<=remaining);ptr+=delta;remaining-=delta;}
	};

protected:
	cfg_var(const char * name) : var_name(name) {next=list;list=this;notify_list=0;};
	
	const char * var_get_name() const {return var_name;}

	//override me
	virtual bool get_raw_data(write_config_callback * out)=0;//return false if values are default and dont need to be stored
	virtual void set_raw_data(const void * data,int size)=0;

	void on_change();//call this whenever your contents change

public:

	const char * get_name() const {return var_name;}

	void refresh() {on_change();}
	void add_notify(cfg_var_notify *);
	static void config_read_file(const void * src,unsigned size);
	static void config_write_file(write_config_callback * out);
	static void config_on_app_init();//call this first from main()
};

class cfg_int : public cfg_var
{
private:
	long val;
	virtual bool get_raw_data(write_config_callback * out);
	virtual void set_raw_data(const void * data,int size);
public:
	explicit inline cfg_int(const char * name,long v) : cfg_var(name) {val=v;}
	inline long operator=(const cfg_int & v) {val=v;on_change();return val;}
	inline operator int() const {return val;}
	inline long operator=(int v) {val=v;on_change();return val;}
};

class cfg_string : public cfg_var
{
private:
	string_simple val;
	
	virtual bool get_raw_data(write_config_callback * out)
	{
		out->write((const char*)val,strlen(val) * sizeof(char));
		return true;
	}

	virtual void set_raw_data(const void * data,int size)
	{
		val.set_string_n((const char*)data,size/sizeof(char));
		on_change();
	}

public:
	explicit inline cfg_string(const char * name,const char * v) : cfg_var(name), val(v) {}
	void operator=(const cfg_string & v) {val=v.get_val();on_change();}
	inline operator const char * () const {return val;}
	inline const char * operator=(const char* v) {val=v;on_change();return val;}
	inline const char * get_val() const {return val;}
	inline bool is_empty() {return !val[0];}
};

class cfg_guid : public cfg_var
{
	GUID val;

	virtual bool get_raw_data(write_config_callback * out)
	{
		out->write_guid_le(val);
		return true;
	}

	virtual void set_raw_data(const void * data,int size)
	{
		read_config_helper r(data,size);
		r.read_guid_le(val);
	}
public:
	explicit inline cfg_guid(const char * name,GUID v) : cfg_var(name) {val=v;}
	explicit inline cfg_guid(const char * name) : cfg_var(name) {memset(&val,0,sizeof(val));}
	inline GUID operator=(GUID v) {val = v; on_change(); return v;}
	inline GUID get_val() const {return val;}
	inline operator GUID() const {return val;}
};

template<class T>
class cfg_struct_t : public cfg_var
{
private:
	T val;


	virtual bool get_raw_data(write_config_callback * out)
	{
		out->write(&val,sizeof(val));
		return true;
	}

	virtual void set_raw_data(const void * data,int size)
	{
		if (size==sizeof(T))
		{
			memcpy(&val,data,sizeof(T));
			on_change();
		}
	}
public:
	explicit inline cfg_struct_t(const char * name,T v) : cfg_var(name) {val=v;}
	explicit inline cfg_struct_t(const char * name,int filler) : cfg_var(name) {memset(&val,filler,sizeof(T));}
	inline const T& operator=(const cfg_struct_t<T> & v) {val = v.get_val();on_change();return val;}
	inline T& get_val() {return val;}
	inline const T& get_val() const {return val;}
	inline operator T() const {return val;}
	inline const T& operator=(const T& v) {val=v;on_change();return val;}
};

class cfg_var_notify
{
	friend cfg_var;
private:
	cfg_var * my_var;
	cfg_var_notify * next;
	cfg_var_notify * var_next;
	static cfg_var_notify * list;
public:
	cfg_var_notify(cfg_var * var) {my_var=var;next=list;list=this;var_next=0;}
	//override me
	virtual void on_var_change(const char * name,cfg_var * var) {}

	static void on_app_init();//called by cfg_var::config_on_app_init()
};

class cfg_var_notify_func : public cfg_var_notify
{
private:
	void (*func)();
public:
	cfg_var_notify_func(cfg_var * var,void (*p_func)() ) : cfg_var_notify(var), func(p_func) {}
	virtual void on_var_change(const char * name,cfg_var * var) {func();}
};


#endif
