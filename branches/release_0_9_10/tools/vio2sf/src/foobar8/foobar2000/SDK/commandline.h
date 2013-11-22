#ifndef _FOOBAR2000_SDK_COMMANDLINE_H_
#define _FOOBAR2000_SDK_COMMANDLINE_H_

#include "service.h"

class commandline_handler : public service_base
{
public:
	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}

	enum result
	{
		RESULT_NOT_OURS,//not our command
		RESULT_PROCESSED,//command processed
		RESULT_PROCESSED_EXPECT_FILES,//command processed, we want to takeover file urls after this command
	};
	virtual result on_token(const char * token)=0;
	virtual void on_file(const char * url) {};//optional
	virtual void on_files_done() {};//optional
	virtual bool want_directories() {return false;}
};

class commandline_handler_metadb_handle : public commandline_handler//helper
{
protected:
	virtual void on_file(const char * url);
	virtual bool want_directories() {return true;}
public:
	virtual result on_token(const char * token)=0;	
	virtual void on_files_done() {};
	
	virtual void on_file(metadb_handle * ptr)=0;
};

/*

how commandline_handler is used:

	scenario #1:
		creation => on_token() => deletion
	scenario #2:
		creation => on_token() returning RESULT_PROCESSED_EXPECT_FILES => on_file(), on_file().... => on_files_done() => deletion
*/

template<class T>
class commandline_handler_factory : public service_factory_t<commandline_handler,T> {};



#endif //_FOOBAR2000_SDK_COMMANDLINE_H_