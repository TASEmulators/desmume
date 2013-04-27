#ifndef _COREVERSION_H_
#define _COREVERSION_H_

#include "service.h"

class core_version_info : public service_base
{
private:
	virtual const char * _get_version_string()=0;
public:
	static const char * get_version_string()
	{
		const char * ret = "";
		core_version_info * ptr = service_enum_create_t(core_version_info,0);
		if (ptr)
		{
			ret = ptr->_get_version_string();
			ptr->service_release();
		}
		return ret;
	}

	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}

};

#endif //_COREVERSION_H_
