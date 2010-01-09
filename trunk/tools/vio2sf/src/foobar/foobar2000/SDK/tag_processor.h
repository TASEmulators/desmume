PFC_DECLARE_EXCEPTION(exception_tag_not_found,exception_io_data,"Tag not found");

//! Callback interface for write-tags-to-temp-file-and-swap scheme, used for ID3v2 tag updates and such where entire file needs to be rewritten. 
//! As a speed optimization, file content can be copied to a temporary file in the same directory as the file being updated, and then source file can be swapped for the newly created file with updated tags.
//! This also gives better security against data loss on crash compared to rewriting the file in place and using memory or generic temporary file APIs to store content being rewritten.
class NOVTABLE tag_write_callback {
public:
	//! Called only once per operation (or not called at all when operation being performed can be done in-place).
	//! Requests a temporary file to be created in the same directory
	virtual bool open_temp_file(service_ptr_t<file> & p_out,abort_callback & p_abort) = 0;	
protected:
	tag_write_callback() {}
	~tag_write_callback() {}
private:
	tag_write_callback(const tag_write_callback &) {throw pfc::exception_not_implemented();}
	const tag_write_callback & operator=(const tag_write_callback &) {throw pfc::exception_not_implemented();}
};

class tag_write_callback_dummy : public tag_write_callback {
public:
	bool open_temp_file(service_ptr_t<file> & p_out,abort_callback & p_abort) {return false;}
};

//! For internal use - call tag_processor namespace methods instead.
class NOVTABLE tag_processor_id3v2 : public service_base
{
public:
	virtual void read(const service_ptr_t<file> & p_file,file_info & p_info,abort_callback & p_abort) = 0;
	virtual void write(const service_ptr_t<file> & p_file,const file_info & p_info,abort_callback & p_abort) = 0;
	virtual void write_ex(tag_write_callback & p_callback,const service_ptr_t<file> & p_file,const file_info & p_info,abort_callback & p_abort) = 0;

	static bool g_get(service_ptr_t<tag_processor_id3v2> & p_out);
	static void g_skip(const service_ptr_t<file> & p_file,t_filesize & p_size_skipped,abort_callback & p_abort);
	static void g_remove(const service_ptr_t<file> & p_file,t_filesize & p_size_removed,abort_callback & p_abort);
	static void g_remove_ex(tag_write_callback & p_callback,const service_ptr_t<file> & p_file,t_filesize & p_size_removed,abort_callback & p_abort);

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(tag_processor_id3v2);
};

//! For internal use - call tag_processor namespace methods instead.
class NOVTABLE tag_processor_trailing : public service_base
{
public:
	enum {
		flag_apev2 = 1,
		flag_id3v1 = 2,
	};

	virtual void read(const service_ptr_t<file> & p_file,file_info & p_info,abort_callback & p_abort) = 0;
	virtual void write(const service_ptr_t<file> & p_file,const file_info & p_info,unsigned p_flags,abort_callback & p_abort) = 0;
	virtual void remove(const service_ptr_t<file> & p_file,abort_callback & p_abort) = 0;
	virtual bool is_id3v1_sufficient(const file_info & p_info) = 0;
	virtual void truncate_to_id3v1(file_info & p_info) = 0;
	virtual void read_ex(const service_ptr_t<file> & p_file,file_info & p_info,t_filesize & p_tagoffset,abort_callback & p_abort) = 0;

	void write_id3v1(const service_ptr_t<file> & p_file,const file_info & p_info,abort_callback & p_abort);
	void write_apev2(const service_ptr_t<file> & p_file,const file_info & p_info,abort_callback & p_abort);
	void write_apev2_id3v1(const service_ptr_t<file> & p_file,const file_info & p_info,abort_callback & p_abort);


	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(tag_processor_trailing);
};

namespace tag_processor {
	//! Strips all recognized tags from the file and writes an ID3v1 tag with specified info.
	void write_id3v1(const service_ptr_t<file> & p_file,const file_info & p_info,abort_callback & p_abort);
	//! Strips all recognized tags from the file and writes an APEv2 tag with specified info.
	void write_apev2(const service_ptr_t<file> & p_file,const file_info & p_info,abort_callback & p_abort);
	//! Strips all recognized tags from the file and writes ID3v1+APEv2 tags with specified info.
	void write_apev2_id3v1(const service_ptr_t<file> & p_file,const file_info & p_info,abort_callback & p_abort);
	//! Strips all recognized tags from the file and writes an ID3v2 tag with specified info.
	void write_id3v2(const service_ptr_t<file> & p_file,const file_info & p_info,abort_callback & p_abort);
	//! Strips all recognized tags from the file and writes ID3v1+ID3v2 tags with specified info.
	void write_id3v2_id3v1(const service_ptr_t<file> & p_file,const file_info & p_info,abort_callback & p_abort);
	//! Strips all recognized tags from the file and writes new tags with specified info according to parameters.
	void write_multi(const service_ptr_t<file> & p_file,const file_info & p_info,abort_callback & p_abort,bool p_write_id3v1,bool p_write_id3v2,bool p_write_apev2);
	//! Strips all recognized tags from the file and writes new tags with specified info according to parameters. Extended to allow write-tags-to-temp-file-and-swap scheme.
	void write_multi_ex(tag_write_callback & p_callback,const service_ptr_t<file> & p_file,const file_info & p_info,abort_callback & p_abort,bool p_write_id3v1,bool p_write_id3v2,bool p_write_apev2);
	//! Removes trailing tags from the file.
	void remove_trailing(const service_ptr_t<file> & p_file,abort_callback & p_abort);
	//! Removes ID3v2 tags from the file. Returns true when a tag was removed, false when the file was not altered.
	bool remove_id3v2(const service_ptr_t<file> & p_file,abort_callback & p_abort);
	//! Removes ID3v2 and trailing tags from specified file (not to be confused with trailing ID3v2 which are not yet supported).
	void remove_id3v2_trailing(const service_ptr_t<file> & p_file,abort_callback & p_abort);
	//! Reads trailing tags from the file.
	void read_trailing(const service_ptr_t<file> & p_file,file_info & p_info,abort_callback & p_abort);
	//! Reads trailing tags from the file. Extended version, returns offset at which parsed tags start.
	void read_trailing_ex(const service_ptr_t<file> & p_file,file_info & p_info,t_filesize & p_tagoffset,abort_callback & p_abort);
	//! Reads ID3v2 tags from specified file.
	void read_id3v2(const service_ptr_t<file> & p_file,file_info & p_info,abort_callback & p_abort);
	//! Reads ID3v2 and trailing tags from specified file (not to be confused with trailing ID3v2 which are not yet supported).
	void read_id3v2_trailing(const service_ptr_t<file> & p_file,file_info & p_info,abort_callback & p_abort);

	void skip_id3v2(const service_ptr_t<file> & p_file,t_filesize & p_size_skipped,abort_callback & p_abort);

	bool is_id3v1_sufficient(const file_info & p_info);
	void truncate_to_id3v1(file_info & p_info);

};
