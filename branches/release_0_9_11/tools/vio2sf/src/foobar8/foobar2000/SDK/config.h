#ifndef _CONFIG_H_
#define _CONFIG_H_

#include "service.h"

class NOVTABLE config : public service_base
{
public:	//NOTE your config class instance will get deleted BEFORE returned window is destroyed or even used
	virtual HWND create(HWND parent)=0;
	virtual const char * get_name()=0;
	virtual const char * get_parent_name() {return 0;}//optional, retuns name of parent config item

	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}
};

template<class T>
class config_factory : public service_factory_single_t<config,T> {};

#endif