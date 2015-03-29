#ifndef _FOOBAR2000_SDK_REPLAYGAIN_H_
#define _FOOBAR2000_SDK_REPLAYGAIN_H_

#include "service.h"

#include "file_info.h"
#include "metadb_handle.h"
#include "metadb.h"


class NOVTABLE replaygain : public service_base
{
public:
	enum mode_t
	{
		MODE_DEFAULT,//uses settings from core config
		MODE_DISABLED,
		MODE_TRACK,
		MODE_ALBUM,
	};

	virtual double query_scale(const file_info * info,mode_t mode = MODE_DEFAULT)=0;
	virtual mode_t get_user_settings()=0;
	
	static replaygain * get()//helper
	{
		return service_enum_create_t(replaygain,0);
	}

	static mode_t g_get_user_settings()//returns whatever MODE_DEFAULT currently corresponds to
	{
		mode_t rv = MODE_DISABLED;
		replaygain * ptr = get();
		if (ptr)
		{
			rv = ptr->get_user_settings();
			ptr->service_release();
		}
		return rv;
	}

	static double g_query_scale(const file_info* info,mode_t mode = MODE_DEFAULT)
	{
		double rv = 1;
		replaygain * ptr = get();
		if (ptr)
		{
			rv = ptr->query_scale(info,mode);
			ptr->service_release();
		}
		return rv;
	}

	static double g_query_scale(const playable_location* entry,mode_t mode = MODE_DEFAULT)
	{
		double rv = 1;
		metadb * p_metadb = metadb::get();
		if (p_metadb)
		{
			metadb_handle * handle = p_metadb->handle_create(entry);
			if (handle)
			{
				rv = g_query_scale(handle,mode);
				handle->handle_release();
			}
		}
		return rv;
	}

	static double g_query_scale(metadb_handle * handle,mode_t mode = MODE_DEFAULT)
	{
		double rv = 1;
		handle->handle_lock();
		const file_info * info = handle->handle_query_locked();
		if (info)
			rv = g_query_scale(info,mode);
		handle->handle_unlock();
		return rv;
	}

	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}
};

#endif //_FOOBAR2000_SDK_REPLAYGAIN_H_