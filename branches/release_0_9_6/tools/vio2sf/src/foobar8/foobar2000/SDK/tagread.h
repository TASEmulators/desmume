#ifndef _TAGREAD_H_
#define _TAGREAD_H_

#include "service.h"
#include "reader.h"
#include "file_info.h"

//helper class to let others use "standard" tag readers/writers in foo_input_std, do not reimplement

class NOVTABLE tag_reader : public service_base
{
public:
	virtual const char * get_name()=0;
	virtual int run(reader * r,file_info * info)=0;//return 1 on success, 0 on failure / unknown format
		
	static int g_run(reader * r,file_info * info,const char * name);//will seek back to original file offset
	static int g_run_multi(reader * r,file_info * info,const char * list);//will seek back to original file offset
	static bool g_have_type(const char * name);

	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}
};

class NOVTABLE tag_writer : public service_base
{
public:
	virtual const char * get_name()=0;
	virtual int run(reader * r,const file_info * info)=0;//return 1 on success, 0 on failure / unknown format

	static int g_run(reader * r,const file_info * info,const char * name);//will seek back to original file offset
	static bool g_have_type(const char * name);

	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}
};

class NOVTABLE tag_remover : public service_base
{
public:
	virtual void run(reader * r)=0;
	
	static void g_run(reader * r);

	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}
};

#endif