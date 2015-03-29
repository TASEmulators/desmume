//new in 0.9.5

//! Common class for handling picture data. \n
//! Type of contained picture data is unknown and to be determined according to memory block contents by code parsing/rendering the picture. Commonly encountered types are: BMP, PNG, JPEG and GIF. \n
//! Implementation: use album_art_data_impl.
class NOVTABLE album_art_data : public service_base {
public:
	//! Retrieves a pointer to a memory block containing the picture.
	virtual const void * get_ptr() const = 0;
	//! Retrieves size of the memory block containing the picture.
	virtual t_size get_size() const = 0;

	//! Determine whether two album_art_data objects store the same picture data.
	static bool equals(album_art_data const & v1, album_art_data const & v2) {
		const t_size s = v1.get_size();
		if (s != v2.get_size()) return false;
		return memcmp(v1.get_ptr(), v2.get_ptr(),s) == 0;
	}
	bool operator==(const album_art_data & other) const {return equals(*this,other);}
	bool operator!=(const album_art_data & other) const {return !equals(*this,other);}

	FB2K_MAKE_SERVICE_INTERFACE(album_art_data,service_base);
};

typedef service_ptr_t<album_art_data> album_art_data_ptr;

//! Implements album_art_data.
class album_art_data_impl : public album_art_data {
public:
	const void * get_ptr() const {return m_content.get_ptr();}
	t_size get_size() const {return m_content.get_size();}

	void * get_ptr() {return m_content.get_ptr();}
	void set_size(t_size p_size) {m_content.set_size(p_size);}

	//! Reads picture data from the specified stream object.
	void from_stream(stream_reader * p_stream,t_size p_bytes,abort_callback & p_abort) {
		set_size(p_bytes); p_stream->read_object(get_ptr(),p_bytes,p_abort);
	}

	//! Creates an album_art_data object from picture data contained in a memory buffer.
	static album_art_data_ptr g_create(const void * p_buffer,t_size p_bytes) {
		service_ptr_t<album_art_data_impl> instance = new service_impl_t<album_art_data_impl>();
		instance->set_size(p_bytes);
		memcpy(instance->get_ptr(),p_buffer,p_bytes);
		return instance;
	}
	//! Creates an album_art_data object from picture data contained in a stream.
	static album_art_data_ptr g_create(stream_reader * p_stream,t_size p_bytes,abort_callback & p_abort) {
		service_ptr_t<album_art_data_impl> instance = new service_impl_t<album_art_data_impl>();
		instance->from_stream(p_stream,p_bytes,p_abort);
		return instance;
	}

private:
	pfc::array_t<t_uint8> m_content;
};

//! Namespace containing identifiers of album art types.
namespace album_art_ids {
	//! Front cover.
	static const GUID cover_front = { 0xf1e66f4e, 0xfe09, 0x4b94, { 0x91, 0xa3, 0x67, 0xc2, 0x3e, 0xd1, 0x44, 0x5e } };
	//! Back cover.
	static const GUID cover_back = { 0xcb552d19, 0x86d5, 0x434c, { 0xac, 0x77, 0xbb, 0x24, 0xed, 0x56, 0x7e, 0xe4 } };
	//! Picture of a disc or other storage media.
	static const GUID disc = { 0x3dba9f36, 0xf928, 0x4fa4, { 0x87, 0x9c, 0xd3, 0x40, 0x47, 0x59, 0x58, 0x7e } };
	//! Album-specific icon (NOT a file type icon).
	static const GUID icon = { 0x74cdf5b4, 0x7053, 0x4b3d, { 0x9a, 0x3c, 0x54, 0x69, 0xf5, 0x82, 0x6e, 0xec } };
};

PFC_DECLARE_EXCEPTION(exception_album_art_not_found,exception_io_not_found,"Album Art Not Found");
PFC_DECLARE_EXCEPTION(exception_album_art_unsupported_entry,exception_io_data,"Unsupported Album Art Entry");

//! Class encapsulating access to album art stored in a media file. Use album_art_extractor class obtain album_art_extractor_instance referring to specified media file.
class NOVTABLE album_art_extractor_instance : public service_base {
public:
	//! Throws exception_album_art_not_found when the requested album art entry could not be found in the referenced media file.
	virtual album_art_data_ptr query(const GUID & p_what,abort_callback & p_abort) = 0;

	FB2K_MAKE_SERVICE_INTERFACE(album_art_extractor_instance,service_base);
};

//! Class encapsulating access to album art stored in a media file. Use album_art_editor class to obtain album_art_editor_instance referring to specified media file.
class NOVTABLE album_art_editor_instance : public album_art_extractor_instance {
public:
	//! Throws exception_album_art_unsupported_entry when the file format we're dealing with does not support specific entry.
	virtual void set(const GUID & p_what,album_art_data_ptr p_data,abort_callback & p_abort) = 0;

	//! Removes the requested entry. Fails silently when the entry doesn't exist.
	virtual void remove(const GUID & p_what) = 0;

	//! Finalizes file tag update operation.
	virtual void commit(abort_callback & p_abort) = 0;
	

	FB2K_MAKE_SERVICE_INTERFACE(album_art_editor_instance,album_art_extractor_instance);
};

typedef service_ptr_t<album_art_extractor_instance> album_art_extractor_instance_ptr;
typedef service_ptr_t<album_art_editor_instance> album_art_editor_instance_ptr;

//! Entrypoint class for accessing album art extraction functionality. Register your own implementation to allow album art extraction from your media file format. \n
//! If you want to extract album art from a media file, it's recommended that you use album_art_manager API instead of calling album_art_extractor directly.
class NOVTABLE album_art_extractor : public service_base {
public:
	//! Returns whether the specified file is one of formats supported by our album_art_extractor implementation.
	//! @param p_path Path to file being queried.
	//! @param p_extension Extension of file being queried (also present in p_path parameter) - provided as a separate parameter for performance reasons.
	virtual bool is_our_path(const char * p_path,const char * p_extension) = 0;
	
	//! Instantiates album_art_extractor_instance providing access to album art stored in a specified media file. \n
	//! Throws one of I/O exceptions on failure; exception_album_art_not_found when the file has no album art record at all.
	//! @param p_filehint Optional; specifies a file interface to use for accessing the specified file; can be null - in that case, the implementation will open and close the file internally.
	virtual album_art_extractor_instance_ptr open(file_ptr p_filehint,const char * p_path,abort_callback & p_abort) = 0;

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(album_art_extractor);
};

//! Entrypoint class for accessing album art editing functionality. Register your own implementation to allow album art editing on your media file format.
class NOVTABLE album_art_editor : public service_base {
public:
	//! Returns whether the specified file is one of formats supported by our album_art_editor implementation.
	//! @param p_path Path to file being queried.
	//! @param p_extension Extension of file being queried (also present in p_path parameter) - provided as a separate parameter for performance reasons.
	virtual bool is_our_path(const char * p_path,const char * p_extension) = 0;

	//! Instantiates album_art_editor_instance providing access to album art stored in a specified media file. \n
	//! @param p_filehint Optional; specifies a file interface to use for accessing the specified file; can be null - in that case, the implementation will open and close the file internally.
	virtual album_art_editor_instance_ptr open(file_ptr p_filehint,const char * p_path,abort_callback & p_abort) = 0;

	//! Helper; attempts to retrieve an album_art_editor service pointer that supports the specified file.
	//! @returns True on success, false on failure (no registered album_art_editor supports this file type).
	static bool g_get_interface(service_ptr_t<album_art_editor> & out,const char * path);
	//! Helper; returns whether one of registered album_art_editor implementations is capable of opening the specified file.
	static bool g_is_supported_path(const char * path);

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(album_art_editor);
};

//! Primary API for interfacing with foobar2000 core's album art extraction functionality. \n
//! Use static_api_ptr_t<album_art_manager>()->instantiate() to obtain a pointer to album_art_manager_instance. \n
//! The main difference between using album_art_manager_instance and calling album_art_extractor methods directly is that 
//! album_art_manager_instance will fall back to returning pictures found in the folder containing the specified media file
//! in case requested album art entries can't be extracted from the media file itself.
class NOVTABLE album_art_manager_instance : public service_base {
public:
	//! @returns True when the newly requested file has different album art than the old one, false when album art we're referencing is the same as before.
	virtual bool open(const char * p_file,abort_callback & p_abort) = 0;
	//! Resets internal data.
	virtual void close() = 0;

	//! Queries album art data for currently open media file. Throws exception_album_art_not_found when the requested album art entry isn't present.
	virtual album_art_data_ptr query(const GUID & p_what,abort_callback & p_abort) = 0;

	//! Queries for stub image to display when there's no album art to show. \n
	//! May fail with exception_album_art_not_found as well when we have no stub image configured.
	virtual album_art_data_ptr query_stub_image(abort_callback & p_abort) = 0;

	FB2K_MAKE_SERVICE_INTERFACE(album_art_manager_instance,service_base);
};

typedef service_ptr_t<album_art_manager_instance> album_art_manager_instance_ptr;

//! Entrypoint API for accessing album art loading functionality provided by foobar2000 core. Usage: static_api_ptr_t<album_art_manager>. \n
//! This API was introduced in 0.9.5.
class NOVTABLE album_art_manager : public service_base {
public:
	virtual album_art_manager_instance_ptr instantiate() = 0;
	
	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(album_art_manager);
};


//! Helper - simple implementation of album_art_extractor_instance.
class album_art_extractor_instance_simple : public album_art_extractor_instance {
public:
	void set(const GUID & p_what,album_art_data_ptr p_content) {m_content.set(p_what,p_content);}
	bool have_item(const GUID & p_what) {return m_content.have_item(p_what);}
	album_art_data_ptr query(const GUID & p_what,abort_callback & p_abort) {
		album_art_data_ptr temp;
		if (!m_content.query(p_what,temp)) throw exception_album_art_not_found();
		return temp;
	}
	bool is_empty() const {return m_content.get_count() == 0;}
private:
	pfc::map_t<GUID,album_art_data_ptr> m_content;
};

//! Helper API for extracting album art from APEv2 tags - introduced in 0.9.5.
class NOVTABLE tag_processor_album_art_utils : public service_base {
public:

	//! Throws one of I/O exceptions on failure; exception_album_art_not_found when the file has no album art record at all.
	virtual album_art_extractor_instance_ptr open(file_ptr p_file,abort_callback & p_abort) = 0;

	//! Currently not implemented. Reserved for future use.
	virtual album_art_editor_instance_ptr edit(file_ptr p_file,abort_callback & p_abort) = 0;

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(tag_processor_album_art_utils)
};

//! Helper implementation of album_art_extractor - reads album art from arbitrary file formats that comply with APEv2 tagging specification.
class album_art_extractor_impl_stdtags : public album_art_extractor {
public:
	//! @param exts Semicolon-separated list of file format extensions to support.
	album_art_extractor_impl_stdtags(const char * exts) {
		pfc::splitStringSimple_toList(m_extensions,";",exts);
	}

	bool is_our_path(const char * p_path,const char * p_extension) {
		return m_extensions.have_item(p_extension);
	}

	album_art_extractor_instance_ptr open(file_ptr p_filehint,const char * p_path,abort_callback & p_abort) {
		PFC_ASSERT( is_our_path(p_path, pfc::string_extension(p_path) ) );
		file_ptr l_file ( p_filehint );
		if (l_file.is_empty()) filesystem::g_open_read(l_file, p_path, p_abort);
		return static_api_ptr_t<tag_processor_album_art_utils>()->open( l_file, p_abort );
	}
private:
	pfc::avltree_t<pfc::string,pfc::string::comparatorCaseInsensitiveASCII> m_extensions;
};
