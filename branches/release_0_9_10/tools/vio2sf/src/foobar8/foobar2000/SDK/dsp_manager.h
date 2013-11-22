#ifndef _DSP_MANAGER_H_
#define _DSP_MANAGER_H_

#include "dsp.h"

//use this to run user-configured DSPs on your data stream
//one implementation in main exe, dont override
class NOVTABLE dsp_manager : public service_base
{
public:
	virtual double run(dsp_chunk_list * list,class metadb_handle * cur_file,int eof)=0;
	virtual void flush();

	virtual void set_chain(int b_use_custom,unsigned int count,const GUID * list)=0;
	//user settings from core config will be used when b_use_custom is zero, custom DSP list specified by count/list params will be used when non-zero
	//user settings are used by default if you don't call set_chain()

	class NOVTABLE get_default_chain_callback
	{
	public:
		virtual void on_list(unsigned int num,const GUID * ptr)=0;
	};

	class get_default_chain_callback_i : public get_default_chain_callback
	{
		mem_block_t<GUID> & data;
	public:
		virtual void on_list(unsigned int num,const GUID * ptr)
		{
			data.set_size(num);
			data.copy(ptr,num);
		}
		get_default_chain_callback_i(mem_block_t<GUID> & param) : data(param) {}
	};

	//use this to get user's DSP config from core preferences
	virtual void get_default_chain(get_default_chain_callback * cb)=0;

	void get_default_chain(mem_block_t<GUID> & dest)//helper
	{
		get_default_chain(&get_default_chain_callback_i(dest));
	}

	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}

	static dsp_manager* create() {return service_enum_create_t(dsp_manager,0);}
};



#endif