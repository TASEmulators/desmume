#ifndef _FILE_OPERATION_CALLBACK_H_
#define _FILE_OPERATION_CALLBACK_H_

//! Interface to notify component system about files being deleted or moved. Operates in app's main thread only.

class NOVTABLE file_operation_callback : public service_base {
public:
	typedef const pfc::list_base_const_t<const char *> & t_pathlist;
	//! p_items is a metadb::path_compare sorted list of files that have been deleted.
	virtual void on_files_deleted_sorted(t_pathlist p_items) = 0;
	//! p_from is a metadb::path_compare sorted list of files that have been moved, p_to is a list of corresponding target locations.
	virtual void on_files_moved_sorted(t_pathlist p_from,t_pathlist p_to) = 0;
	//! p_from is a metadb::path_compare sorted list of files that have been copied, p_to is a list of corresponding target locations.
	virtual void on_files_copied_sorted(t_pathlist p_from,t_pathlist p_to) = 0;

	static void g_on_files_deleted(const pfc::list_base_const_t<const char *> & p_items);
	static void g_on_files_moved(const pfc::list_base_const_t<const char *> & p_from,const pfc::list_base_const_t<const char *> & p_to);
	static void g_on_files_copied(const pfc::list_base_const_t<const char *> & p_from,const pfc::list_base_const_t<const char *> & p_to);

	static bool g_search_sorted_list(const pfc::list_base_const_t<const char*> & p_list,const char * p_string,t_size & p_index);
	static bool g_update_list_on_moved(metadb_handle_list_ref p_list,t_pathlist p_from,t_pathlist p_to);

	static bool g_update_list_on_moved_ex(metadb_handle_list_ref p_list,t_pathlist p_from,t_pathlist p_to, metadb_handle_list_ref itemsAdded, metadb_handle_list_ref itemsRemoved);

	static bool g_mark_dead_entries(metadb_handle_list_cref items, bit_array_var & mask, t_pathlist deadPaths);


	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(file_operation_callback);
};



//! New in 0.9.5.
class NOVTABLE file_operation_callback_dynamic {
public:
	//! p_items is a metadb::path_compare sorted list of files that have been deleted.
	virtual void on_files_deleted_sorted(const pfc::list_base_const_t<const char *> & p_items) = 0;
	//! p_from is a metadb::path_compare sorted list of files that have been moved, p_to is a list of corresponding target locations.
	virtual void on_files_moved_sorted(const pfc::list_base_const_t<const char *> & p_from,const pfc::list_base_const_t<const char *> & p_to) = 0;
	//! p_from is a metadb::path_compare sorted list of files that have been copied, p_to is a list of corresponding target locations.
	virtual void on_files_copied_sorted(const pfc::list_base_const_t<const char *> & p_from,const pfc::list_base_const_t<const char *> & p_to) = 0;
};

//! New in 0.9.5.
class NOVTABLE file_operation_callback_dynamic_manager : public service_base {
public:
	virtual void register_callback(file_operation_callback_dynamic * p_callback) = 0;
	virtual void unregister_callback(file_operation_callback_dynamic * p_callback) = 0;

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(file_operation_callback_dynamic_manager);
};

//! New in 0.9.5.
class file_operation_callback_dynamic_impl_base : public file_operation_callback_dynamic {
public:
	file_operation_callback_dynamic_impl_base() {static_api_ptr_t<file_operation_callback_dynamic_manager>()->register_callback(this);}
	~file_operation_callback_dynamic_impl_base() {static_api_ptr_t<file_operation_callback_dynamic_manager>()->unregister_callback(this);}

	void on_files_deleted_sorted(const pfc::list_base_const_t<const char *> & p_items) {}
	void on_files_moved_sorted(const pfc::list_base_const_t<const char *> & p_from,const pfc::list_base_const_t<const char *> & p_to) {}
	void on_files_copied_sorted(const pfc::list_base_const_t<const char *> & p_from,const pfc::list_base_const_t<const char *> & p_to) {}

	PFC_CLASS_NOT_COPYABLE_EX(file_operation_callback_dynamic_impl_base);
};

#endif //_FILE_OPERATION_CALLBACK_H_
