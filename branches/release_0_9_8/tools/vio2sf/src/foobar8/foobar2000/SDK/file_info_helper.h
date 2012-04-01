#ifndef _FILE_INFO_HELPER_H_
#define _FILE_INFO_HELPER_H_

#include "file_info.h"
#include "metadb_handle.h"

class file_info_i : public file_info
{
private:
	playable_location_i location;
	double length;

	struct entry
	{
		string_simple name,value;
		entry(const char * p_name,const char * p_value) : name(p_name), value(p_value) {}
	};
	ptr_list_t<entry> meta,info;
public:
	file_info_i(const playable_location & src) : location(src), length(0) {}
	file_info_i(const playable_location * src) : location(*src), length(0) {}

	file_info_i(const char * fn,int num) : location(fn,num), length(0) {}

	file_info_i() : length(0) {}

	file_info_i(metadb_handle * src)
		: location(*src->handle_get_location()), length(0)
	{
		src->handle_query(this);
	}

	file_info_i(const file_info & src) : length(0) {copy(&src);}


	~file_info_i() {meta.delete_all();info.delete_all();}

	//multiple fields of the same name ARE allowed.
	virtual int meta_get_count() const {return meta.get_count();}
	virtual const char * meta_enum_name(int n) const {return meta[n]->name;}
	virtual const char * meta_enum_value(int n) const {return meta[n]->value;}

	virtual void meta_modify_value(int n,const char * new_value)
	{
		assert(is_valid_utf8(new_value));
		meta[n]->value=new_value;
	}
	
	virtual void meta_insert(int index,const char * name,const char * value)
	{
		assert(is_valid_utf8(name));
		assert(is_valid_utf8(value));
		meta.insert_item(new entry(name,value),index);
	}

	virtual void meta_add(const char * name,const char * value)
	{
		assert(is_valid_utf8(name));
		assert(is_valid_utf8(value));
		meta.add_item(new entry(name,value));
	}

	virtual void meta_remove(int n) {meta.delete_by_idx(n);}
	virtual void meta_remove_all() {meta.delete_all();}
	
	//tech infos (bitrate, replaygain, etc), not user-editable
	//multiple fields of the same are NOT allowed.
	virtual void info_set(const char * name,const char * value)
	{
		assert(is_valid_utf8(name));
		assert(is_valid_utf8(value));
		info_remove_field(name);
		info.add_item(new entry(name,value));
	}

	virtual int info_get_count() const {return info.get_count();}
	virtual const char * info_enum_value(int n) const {return info[n]->value;}
	virtual const char * info_enum_name(int n) const {return info[n]->name;}
	virtual void info_remove(int n) {info.delete_by_idx(n);}
	virtual void info_remove_all() {info.delete_all();}


	virtual const playable_location * get_location() const {return &location;}
	virtual void set_location(const playable_location * z) {location.copy(z);}

	virtual void set_length(double v) {length = v;}
	virtual double get_length() const {return length;}

};

#define file_info_i_full file_info_i

#endif //_FILE_INFO_HELPER_H_