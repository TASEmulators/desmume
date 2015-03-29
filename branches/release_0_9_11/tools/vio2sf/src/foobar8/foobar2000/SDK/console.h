#ifndef _CONSOLE_H_
#define _CONSOLE_H_

#include "service.h"

//for sending text (errors etc) to console component
//strings are all utf-8

class NOVTABLE console : public service_base
{
protected:
	virtual void _output(int severity,const char * source,const char * message)=0;
	virtual void _popup()=0;
	virtual void _beep()=0;
public:
	enum {
		SEVERITY_INFO = 1,
		SEVERITY_WARNING = 2,
		SEVERITY_CRITICAL = 3,
	};

	static void output(int severity,const char * message,const char * source);
	static void output(int severity,const char * message);
	static void info(const char * message) {output(SEVERITY_INFO,message);}
	static void warning(const char * message) {output(SEVERITY_WARNING,message);}
	static void error(const char * message) {output(SEVERITY_CRITICAL,message);}
	static bool present();//returns if console component is present
	static void popup();
	static void beep();
	static void info_location(const class playable_location * src);
	static void info_location(class metadb_handle * src);
	static void info_location(const class file_info * src);
	static void info_full(const class file_info * src);


	
	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}

	//usage: console::warning("blah warning !");
};

//all console methods are multi-thread safe, you can call them from any context

#define __crash_to_messagebox(x) __except(uMessageBox(0,string_print_crash(GetExceptionInformation(),x),0,0),1)
#define __crash_to_console __except(console::error(string_print_crash(GetExceptionInformation())),1)
#define __crash_to_console_ex(x) __except(console::error(string_print_crash(GetExceptionInformation(),x)),1)
/*
usage
__try {
	blah();
} __crash_to_console_ex("blah()")
{
	//might want to print additional info about what caused the crash here
}

*/

#endif