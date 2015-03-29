class file_info;

//! Contains various I/O related structures and interfaces.

namespace foobar2000_io
{
	//! Type used for file size related variables.
	typedef t_uint64 t_filesize;
	//! Type used for file size related variables when signed value is needed.
	typedef t_int64 t_sfilesize;
	//! Type used for file timestamp related variables. 64-bit value representing the number of 100-nanosecond intervals since January 1, 1601; 0 for invalid/unknown time.
	typedef t_uint64 t_filetimestamp;
	//! Invalid/unknown file timestamp constant. Also see: t_filetimestamp.
	const t_filetimestamp filetimestamp_invalid = 0;
	//! Invalid/unknown file size constant. Also see: t_filesize.
	static const t_filesize filesize_invalid = (t_filesize)(~0);
	
	static const t_filetimestamp filetimestamp_1second_increment = 10000000;

	//! Generic I/O error. Root class for I/O failure exception. See relevant default message for description of each derived exception class.
	PFC_DECLARE_EXCEPTION(exception_io,						pfc::exception,"I/O error");
	//! Object not found.
	PFC_DECLARE_EXCEPTION(exception_io_not_found,			exception_io,"Object not found");
	//! Access denied.
	PFC_DECLARE_EXCEPTION(exception_io_denied,				exception_io,"Access denied");
	//! Unsupported format or corrupted file (unexpected data encountered).
	PFC_DECLARE_EXCEPTION(exception_io_data,				exception_io,"Unsupported format or corrupted file");
	//! Unsupported format or corrupted file (truncation encountered).
	PFC_DECLARE_EXCEPTION(exception_io_data_truncation,		exception_io_data,"Unsupported format or corrupted file");
	//! Unsupported format (a subclass of "unsupported format or corrupted file" exception).
	PFC_DECLARE_EXCEPTION(exception_io_unsupported_format,	exception_io_data,"Unsupported file format");
	//! Object is remote, while specific operation is supported only for local objects.
	PFC_DECLARE_EXCEPTION(exception_io_object_is_remote,	exception_io,"This operation is not supported on remote objects");
	//! Sharing violation.
	PFC_DECLARE_EXCEPTION(exception_io_sharing_violation,	exception_io,"Sharing violation");
	//! Device full.
	PFC_DECLARE_EXCEPTION(exception_io_device_full,			exception_io,"Device full");
	//! Attempt to seek outside valid range.
	PFC_DECLARE_EXCEPTION(exception_io_seek_out_of_range,	exception_io,"Seek offset out of range");
	//! This operation requires a seekable object.
	PFC_DECLARE_EXCEPTION(exception_io_object_not_seekable,	exception_io,"Object is not seekable");
	//! This operation requires an object with known length.
	PFC_DECLARE_EXCEPTION(exception_io_no_length,			exception_io,"Length of object is unknown");
	//! Invalid path.
	PFC_DECLARE_EXCEPTION(exception_io_no_handler_for_path,	exception_io,"Invalid path");
	//! Object already exists.
	PFC_DECLARE_EXCEPTION(exception_io_already_exists,		exception_io,"Object already exists");
	//! Pipe error.
	PFC_DECLARE_EXCEPTION(exception_io_no_data,				exception_io,"The process receiving or sending data has terminated");
	//! Network not reachable.
	PFC_DECLARE_EXCEPTION(exception_io_network_not_reachable,exception_io,"Network not reachable");
	//! Media is write protected.
	PFC_DECLARE_EXCEPTION(exception_io_write_protected,		exception_io_denied,"The media is write protected");
	//! File is corrupted. This indicates filesystem call failure, not actual invalid data being read by the app.
	PFC_DECLARE_EXCEPTION(exception_io_file_corrupted,		exception_io,"The file is corrupted");
	//! The disc required for requested operation is not available.
	PFC_DECLARE_EXCEPTION(exception_io_disk_change,			exception_io,"Disc not available");
	//! The directory is not empty.
	PFC_DECLARE_EXCEPTION(exception_io_directory_not_empty,	exception_io,"Directory not empty");

	//! Stores file stats (size and timestamp).
	struct t_filestats {
		//! Size of the file.
		t_filesize m_size;
		//! Time of last file modification.
		t_filetimestamp m_timestamp;

		inline bool operator==(const t_filestats & param) const {return m_size == param.m_size && m_timestamp == param.m_timestamp;}
		inline bool operator!=(const t_filestats & param) const {return m_size != param.m_size || m_timestamp != param.m_timestamp;}
	};

	//! Invalid/unknown file stats constant. See: t_filestats.
	static const t_filestats filestats_invalid = {filesize_invalid,filetimestamp_invalid};

#ifdef _WIN32
	void exception_io_from_win32(DWORD p_code);
#define WIN32_IO_OP(X) {SetLastError(NO_ERROR); if (!(X)) exception_io_from_win32(GetLastError());}
#endif

	//! Generic interface to read data from a nonseekable stream. Also see: stream_writer, file.	\n
	//! Error handling: all methods may throw exception_io or one of derivatives on failure; exception_aborted when abort_callback is signaled.
	class NOVTABLE stream_reader {
	public:
		//! Attempts to reads specified number of bytes from the stream.
		//! @param p_buffer Receives data being read. Must have at least p_bytes bytes of space allocated.
		//! @param p_bytes Number of bytes to read.
		//! @param p_abort abort_callback object signaling user aborting the operation.
		//! @returns Number of bytes actually read. May be less than requested when EOF was reached.
		virtual t_size read(void * p_buffer,t_size p_bytes,abort_callback & p_abort) = 0;
		//! Reads specified number of bytes from the stream. If requested amount of bytes can't be read (e.g. EOF), throws exception_io_data_truncation.
		//! @param p_buffer Receives data being read. Must have at least p_bytes bytes of space allocated.
		//! @param p_bytes Number of bytes to read.
		//! @param p_abort abort_callback object signaling user aborting the operation.
		virtual void read_object(void * p_buffer,t_size p_bytes,abort_callback & p_abort);
		//! Attempts to skip specified number of bytes in the stream.
		//! @param p_bytes Number of bytes to skip.
		//! @param p_abort abort_callback object signaling user aborting the operation.
		//! @returns Number of bytes actually skipped, May be less than requested when EOF was reached.
		virtual t_filesize skip(t_filesize p_bytes,abort_callback & p_abort);
		//! Skips specified number of bytes in the stream. If requested amount of bytes can't be skipped (e.g. EOF), throws exception_io_data_truncation.
		//! @param p_bytes Number of bytes to skip.
		//! @param p_abort abort_callback object signaling user aborting the operation.
		virtual void skip_object(t_filesize p_bytes,abort_callback & p_abort);

		//! Helper template built around read_object. Reads single raw object from the stream.
		//! @param p_object Receives object read from the stream on success.
		//! @param p_abort abort_callback object signaling user aborting the operation.
		template<typename T> inline void read_object_t(T& p_object,abort_callback & p_abort) {pfc::assert_raw_type<T>(); read_object(&p_object,sizeof(p_object),p_abort);}
		//! Helper template built around read_object. Reads single raw object from the stream; corrects byte order assuming stream uses little endian order.
		//! @param p_object Receives object read from the stream on success.
		//! @param p_abort abort_callback object signaling user aborting the operation.
		template<typename T> inline void read_lendian_t(T& p_object,abort_callback & p_abort) {read_object_t(p_object,p_abort); byte_order::order_le_to_native_t(p_object);}
		//! Helper template built around read_object. Reads single raw object from the stream; corrects byte order assuming stream uses big endian order.
		//! @param p_object Receives object read from the stream on success.
		//! @param p_abort abort_callback object signaling user aborting the operation.
		template<typename T> inline void read_bendian_t(T& p_object,abort_callback & p_abort) {read_object_t(p_object,p_abort); byte_order::order_be_to_native_t(p_object);}

		//! Helper function; reads a string (with a 32-bit header indicating length in bytes followed by UTF-8 encoded data without a null terminator).
		void read_string(pfc::string_base & p_out,abort_callback & p_abort);
		//! Helper function; alternate way of storing strings; assumes string takes space up to end of stream.
		void read_string_raw(pfc::string_base & p_out,abort_callback & p_abort);
		//! Helper function; reads a string (with a 32-bit header indicating length in bytes followed by UTF-8 encoded data without a null terminator).
		pfc::string read_string(abort_callback & p_abort);

		//! Helper function; reads a string of specified length from the stream.
		void read_string_ex(pfc::string_base & p_out,t_size p_bytes,abort_callback & p_abort);
		//! Helper function; reads a string of specified length from the stream.
		pfc::string read_string_ex(t_size p_len,abort_callback & p_abort);
	protected:
		stream_reader() {}
		~stream_reader() {}
	};


	//! Generic interface to write data to a nonseekable stream. Also see: stream_reader, file.	\n
	//! Error handling: all methods may throw exception_io or one of derivatives on failure; exception_aborted when abort_callback is signaled.
	class NOVTABLE stream_writer {
	public:
		//! Writes specified number of bytes from specified buffer to the stream.
		//! @param p_buffer Buffer with data to write. Must contain at least p_bytes bytes.
		//! @param p_bytes Number of bytes to write.
		//! @param p_abort abort_callback object signaling user aborting the operation.
		virtual void write(const void * p_buffer,t_size p_bytes,abort_callback & p_abort) = 0;
		
		//! Helper. Same as write(), provided for consistency.
		inline void write_object(const void * p_buffer,t_size p_bytes,abort_callback & p_abort) {write(p_buffer,p_bytes,p_abort);}

		//! Helper template. Writes single raw object to the stream.
		//! @param p_object Object to write.
		//! @param p_abort abort_callback object signaling user aborting the operation.
		template<typename T> inline void write_object_t(const T & p_object, abort_callback & p_abort) {pfc::assert_raw_type<T>(); write_object(&p_object,sizeof(p_object),p_abort);}
		//! Helper template. Writes single raw object to the stream; corrects byte order assuming stream uses little endian order.
		//! @param p_object Object to write.
		//! @param p_abort abort_callback object signaling user aborting the operation.
		template<typename T> inline void write_lendian_t(const T & p_object, abort_callback & p_abort) {T temp = p_object; byte_order::order_native_to_le_t(temp); write_object_t(temp,p_abort);}
		//! Helper template. Writes single raw object to the stream; corrects byte order assuming stream uses big endian order.
		//! @param p_object Object to write.
		//! @param p_abort abort_callback object signaling user aborting the operation.
		template<typename T> inline void write_bendian_t(const T & p_object, abort_callback & p_abort) {T temp = p_object; byte_order::order_native_to_be_t(temp); write_object_t(temp,p_abort);}

		//! Helper function; writes string (with 32-bit header indicating length in bytes followed by UTF-8 encoded data without null terminator).
		void write_string(const char * p_string,abort_callback & p_abort);
		void write_string(const char * p_string,t_size p_len,abort_callback & p_abort);

		template<typename T>
		void write_string(const T& val,abort_callback & p_abort) {write_string(pfc::stringToPtr(val),p_abort);}

		//! Helper function; writes raw string to the stream, with no length info or null terminators.
		void write_string_raw(const char * p_string,abort_callback & p_abort);
	protected:
		stream_writer() {}
		~stream_writer() {}
	};

	//! A class providing abstraction for an open file object, with reading/writing/seeking methods. See also: stream_reader, stream_writer (which it inherits read/write methods from). \n
	//! Error handling: all methods may throw exception_io or one of derivatives on failure; exception_aborted when abort_callback is signaled.
	class NOVTABLE file : public service_base, public stream_reader, public stream_writer {
	public:

		//! Seeking mode constants. Note: these are purposedly defined to same values as standard C SEEK_* constants
		enum t_seek_mode {
			//! Seek relative to beginning of file (same as seeking to absolute offset).
			seek_from_beginning = 0,
			//! Seek relative to current position.
			seek_from_current = 1,
			//! Seek relative to end of file.
			seek_from_eof = 2,
		};

		//! Retrieves size of the file.
		//! @param p_abort abort_callback object signaling user aborting the operation.
		//! @returns File size on success; filesize_invalid if unknown (nonseekable stream etc).
		virtual t_filesize get_size(abort_callback & p_abort) = 0;


		//! Retrieves read/write cursor position in the file. In case of non-seekable stream, this should return number of bytes read so far since open/reopen call.
		//! @param p_abort abort_callback object signaling user aborting the operation.
		//! @returns Read/write cursor position
		virtual t_filesize get_position(abort_callback & p_abort) = 0;

		//! Resizes file to the specified size in bytes.
		//! @param p_abort abort_callback object signaling user aborting the operation.
		virtual void resize(t_filesize p_size,abort_callback & p_abort) = 0;

		//! Sets read/write cursor position to the specified offset.
		//! @param p_position position to seek to.
		//! @param p_abort abort_callback object signaling user aborting the operation.
		virtual void seek(t_filesize p_position,abort_callback & p_abort) = 0;

		
		//! Sets read/write cursor position to the specified offset; extended form allowing seeking relative to current position or to end of file.
		//! @param p_position Position to seek to; interpretation of this value depends on p_mode parameter.
		//! @param p_mode Seeking mode; see t_seek_mode enum values for further description.
		//! @param p_abort abort_callback object signaling user aborting the operation.
		virtual void seek_ex(t_sfilesize p_position,t_seek_mode p_mode,abort_callback & p_abort);

		//! Returns whether the file is seekable or not. If can_seek() returns false, all seek() or seek_ex() calls will fail; reopen() is still usable on nonseekable streams.
		virtual bool can_seek() = 0;

		//! Retrieves mime type of the file.
		//! @param p_out Receives content type string on success.
		virtual bool get_content_type(pfc::string_base & p_out) = 0;

		//! Hint, returns whether the file is already fully buffered into memory.
		virtual bool is_in_memory() {return false;}

		//! Optional, called by owner thread before sleeping.
		//! @param p_abort abort_callback object signaling user aborting the operation.
		virtual void on_idle(abort_callback & p_abort) {}

		//! Retrieves last modification time of the file.
		//! @param p_abort abort_callback object signaling user aborting the operation.
		//! @returns Last modification time o fthe file; filetimestamp_invalid if N/A.
		virtual t_filetimestamp get_timestamp(abort_callback & p_abort) {return filetimestamp_invalid;}

		//! Resets non-seekable stream, or seeks to zero on seekable file.
		//! @param p_abort abort_callback object signaling user aborting the operation.
		virtual void reopen(abort_callback & p_abort) = 0;
		
		//! Indicates whether the file is a remote resource and non-sequential access may be slowed down by lag. This is typically returns to true on non-seekable sources but may also return true on seekable sources indicating that seeking is supported but will be relatively slow.
		virtual bool is_remote() = 0;
		
		//! Retrieves file stats structure. Usese get_size() and get_timestamp().
		t_filestats get_stats(abort_callback & p_abort);

		//! Returns whether read/write cursor position is at the end of file.
		bool is_eof(abort_callback & p_abort);

		//! Truncates file to specified size (while preserving read/write cursor position if possible); uses set_eof().
		void truncate(t_filesize p_position,abort_callback & p_abort);

		//! Truncates the file at current read/write cursor position.
		void set_eof(abort_callback & p_abort) {resize(get_position(p_abort),p_abort);}


		//! Helper; retrieves size of the file. If size is not available (get_size() returns filesize_invalid), throws exception_io_no_length.
		t_filesize get_size_ex(abort_callback & p_abort);

		//! Helper; retrieves amount of bytes between read/write cursor position and end of file. Fails when length can't be determined.
		t_filesize get_remaining(abort_callback & p_abort);

		//! Helper; throws exception_io_object_not_seekable if file is not seekable.
		void ensure_seekable();

		//! Helper; throws exception_io_object_is_remote if the file is remote.
		void ensure_local();

		//! Helper; transfers specified number of bytes between streams.
		//! @returns number of bytes actually transferred. May be less than requested if e.g. EOF is reached.
		static t_filesize g_transfer(stream_reader * src,stream_writer * dst,t_filesize bytes,abort_callback & p_abort);
		//! Helper; transfers specified number of bytes between streams. Throws exception if requested number of bytes could not be read (EOF).
		static void g_transfer_object(stream_reader * src,stream_writer * dst,t_filesize bytes,abort_callback & p_abort);
		//! Helper; transfers entire file content from one file to another, erasing previous content.
		static void g_transfer_file(const service_ptr_t<file> & p_from,const service_ptr_t<file> & p_to,abort_callback & p_abort);

		//! Helper; improved performance over g_transfer on streams (avoids disk fragmentation when transferring large blocks).
		static t_filesize g_transfer(service_ptr_t<file> p_src,service_ptr_t<file> p_dst,t_filesize p_bytes,abort_callback & p_abort);
		//! Helper; improved performance over g_transfer_file on streams (avoids disk fragmentation when transferring large blocks).
		static void g_transfer_object(service_ptr_t<file> p_src,service_ptr_t<file> p_dst,t_filesize p_bytes,abort_callback & p_abort);


		t_filesize skip(t_filesize p_bytes,abort_callback & p_abort);

		FB2K_MAKE_SERVICE_INTERFACE(file,service_base);
	};

	typedef service_ptr_t<file> file_ptr;

	//! Special hack for shoutcast metadata nonsense handling. Documentme.
	class file_dynamicinfo : public file {
	public:
		//! Retrieves "static" info that doesn't change in the middle of stream, such as station names etc. Returns true on success; false when static info is not available.
		virtual bool get_static_info(class file_info & p_out) = 0;
		//! Returns whether dynamic info is available on this stream or not.
		virtual bool is_dynamic_info_enabled()=0;
		//! Retrieves dynamic stream info (e.g. online stream track titles). Returns true on success, false when info has not changed since last call.
		virtual bool get_dynamic_info(class file_info & p_out) = 0;

		FB2K_MAKE_SERVICE_INTERFACE(file_dynamicinfo,file);
	};

	//! Implementation helper - contains dummy implementations of methods that modify the file
	template<typename t_base> class file_readonly_t : public t_base {
	public:
		void resize(t_filesize p_size,abort_callback & p_abort) {throw exception_io_denied();}
		void write(const void * p_buffer,t_size p_bytes,abort_callback & p_abort) {throw exception_io_denied();}
	};
	typedef file_readonly_t<file> file_readonly;

	class filesystem;

	class NOVTABLE directory_callback {
	public:
		//! @returns true to continue enumeration, false to abort.
		virtual bool on_entry(filesystem * p_owner,abort_callback & p_abort,const char * p_url,bool p_is_subdirectory,const t_filestats & p_stats)=0;
	};


	//! Entrypoint service for all filesystem operations.\n
	//! Implementation: standard implementations for local filesystem etc are provided by core.\n
	//! Instantiation: use static helper functions rather than calling filesystem interface methods directly, e.g. filesystem::g_open() to open a file.
	class NOVTABLE filesystem : public service_base {
	public:
		//! Enumeration specifying how to open a file. See: filesystem::open(), filesystem::g_open().
		enum t_open_mode {
			//! Opens an existing file for reading; if the file does not exist, the operation will fail.
			open_mode_read,
			//! Opens an existing file for writing; if the file does not exist, the operation will fail.
			open_mode_write_existing,
			//! Opens a new file for writing; if the file exists, its contents will be wiped.
			open_mode_write_new,
		};

		virtual bool get_canonical_path(const char * p_path,pfc::string_base & p_out)=0;
		virtual bool is_our_path(const char * p_path)=0;
		virtual bool get_display_path(const char * p_path,pfc::string_base & p_out)=0;

		virtual void open(service_ptr_t<file> & p_out,const char * p_path, t_open_mode p_mode,abort_callback & p_abort)=0;
		virtual void remove(const char * p_path,abort_callback & p_abort)=0;
		virtual void move(const char * p_src,const char * p_dst,abort_callback & p_abort)=0;
		//! Queries whether a file at specified path belonging to this filesystem is a remove object or not.
		virtual bool is_remote(const char * p_src) = 0;

		//! Retrieves stats of a file at specified path.
		virtual void get_stats(const char * p_path,t_filestats & p_stats,bool & p_is_writeable,abort_callback & p_abort) = 0;
		
		virtual bool relative_path_create(const char * file_path,const char * playlist_path,pfc::string_base & out) {return false;}
		virtual bool relative_path_parse(const char * relative_path,const char * playlist_path,pfc::string_base & out) {return false;}

		//! Creates a directory.
		virtual void create_directory(const char * p_path,abort_callback & p_abort) = 0;

		virtual void list_directory(const char * p_path,directory_callback & p_out,abort_callback & p_abort)=0;

		//! Hint; returns whether this filesystem supports mime types. \n 
		//! When this returns false, all file::get_content_type() calls on files opened thru this filesystem implementation will return false; otherwise, file::get_content_type() calls may return true depending on the file.
		virtual bool supports_content_types() = 0;

		static void g_get_canonical_path(const char * path,pfc::string_base & out);
		static void g_get_display_path(const char * path,pfc::string_base & out);

		static bool g_get_interface(service_ptr_t<filesystem> & p_out,const char * path);//path is AFTER get_canonical_path
		static bool g_is_remote(const char * p_path);//path is AFTER get_canonical_path
		static bool g_is_recognized_and_remote(const char * p_path);//path is AFTER get_canonical_path
		static bool g_is_remote_safe(const char * p_path) {return g_is_recognized_and_remote(p_path);}
		static bool g_is_remote_or_unrecognized(const char * p_path);
		static bool g_is_recognized_path(const char * p_path);
		
		//! Opens file at specified path, with specified access privileges.
		static void g_open(service_ptr_t<file> & p_out,const char * p_path,t_open_mode p_mode,abort_callback & p_abort);
		//! Attempts to open file at specified path; if the operation fails with sharing violation error, keeps retrying (with short sleep period between retries) for specified amount of time.
		static void g_open_timeout(service_ptr_t<file> & p_out,const char * p_path,t_open_mode p_mode,double p_timeout,abort_callback & p_abort);
		static void g_open_write_new(service_ptr_t<file> & p_out,const char * p_path,abort_callback & p_abort);
		static void g_open_read(service_ptr_t<file> & p_out,const char * path,abort_callback & p_abort) {return g_open(p_out,path,open_mode_read,p_abort);}
		static void g_open_precache(service_ptr_t<file> & p_out,const char * path,abort_callback & p_abort);//open only for precaching data (eg. will fail on http etc)
		static bool g_exists(const char * p_path,abort_callback & p_abort);
		static bool g_exists_writeable(const char * p_path,abort_callback & p_abort);
		//! Removes file at specified path.
		static void g_remove(const char * p_path,abort_callback & p_abort);
		//! Attempts to remove file at specified path; if the operation fails with a sharing violation error, keeps retrying (with short sleep period between retries) for specified amount of time.
		static void g_remove_timeout(const char * p_path,double p_timeout,abort_callback & p_abort);
		//! Moves file from one path to another.
		static void g_move(const char * p_src,const char * p_dst,abort_callback & p_abort);
		//! Attempts to move file from one path to another; if the operation fails with a sharing violation error, keeps retrying (with short sleep period between retries) for specified amount of time.
		static void g_move_timeout(const char * p_src,const char * p_dst,double p_timeout,abort_callback & p_abort);

		static void g_copy(const char * p_src,const char * p_dst,abort_callback & p_abort);//needs canonical path
		static void g_copy_timeout(const char * p_src,const char * p_dst,double p_timeout,abort_callback & p_abort);//needs canonical path
		static void g_copy_directory(const char * p_src,const char * p_dst,abort_callback & p_abort);//needs canonical path
		static void g_get_stats(const char * p_path,t_filestats & p_stats,bool & p_is_writeable,abort_callback & p_abort);
		static bool g_relative_path_create(const char * p_file_path,const char * p_playlist_path,pfc::string_base & out);
		static bool g_relative_path_parse(const char * p_relative_path,const char * p_playlist_path,pfc::string_base & out);
		
		static void g_create_directory(const char * p_path,abort_callback & p_abort);

		//! If for some bloody reason you ever need stream io compatibility, use this, INSTEAD of calling fopen() on the path string you've got; will only work with file:// (and not with http://, unpack:// or whatever)
		static FILE * streamio_open(const char * p_path,const char * p_flags); 

		static void g_open_temp(service_ptr_t<file> & p_out,abort_callback & p_abort);
		static void g_open_tempmem(service_ptr_t<file> & p_out,abort_callback & p_abort);

		static void g_list_directory(const char * p_path,directory_callback & p_out,abort_callback & p_abort);// path must be canonical

		static bool g_is_valid_directory(const char * path,abort_callback & p_abort);
		static bool g_is_empty_directory(const char * path,abort_callback & p_abort);

		FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(filesystem);
	};

	class directory_callback_impl : public directory_callback
	{
		struct t_entry
		{
			pfc::string_simple m_path;
			t_filestats m_stats;
			t_entry(const char * p_path, const t_filestats & p_stats) : m_path(p_path), m_stats(p_stats) {}
		};


		pfc::list_t<pfc::rcptr_t<t_entry> > m_data;
		bool m_recur;

		static int sortfunc(const pfc::rcptr_t<const t_entry> & p1, const pfc::rcptr_t<const t_entry> & p2) {return stricmp_utf8(p1->m_path,p2->m_path);}
	public:
		bool on_entry(filesystem * owner,abort_callback & p_abort,const char * url,bool is_subdirectory,const t_filestats & p_stats);

		directory_callback_impl(bool p_recur) : m_recur(p_recur) {}
		t_size get_count() {return m_data.get_count();}
		const char * operator[](t_size n) const {return m_data[n]->m_path;}
		const char * get_item(t_size n) const {return m_data[n]->m_path;}
		const t_filestats & get_item_stats(t_size n) const {return m_data[n]->m_stats;}
		void sort() {m_data.sort_t(sortfunc);}
	};

	class archive;

	class NOVTABLE archive_callback : public abort_callback {
	public:
		virtual bool on_entry(archive * owner,const char * url,const t_filestats & p_stats,const service_ptr_t<file> & p_reader) = 0;
	};

	//! Interface for archive reader services. When implementing, derive from archive_impl rather than from deriving from archive directly.
	class NOVTABLE archive : public filesystem {
	public:
		virtual void archive_list(const char * p_path,const service_ptr_t<file> & p_reader,archive_callback & p_callback,bool p_want_readers) = 0;
		
		FB2K_MAKE_SERVICE_INTERFACE(archive,filesystem);
	};

	//! Root class for archive implementations. Derive from this instead of from archive directly.
	class NOVTABLE archive_impl : public archive {
	private:
		//do not override these
		bool get_canonical_path(const char * path,pfc::string_base & out);
		bool is_our_path(const char * path);
		bool get_display_path(const char * path,pfc::string_base & out);
		void remove(const char * path,abort_callback & p_abort);
		void move(const char * src,const char * dst,abort_callback & p_abort);
		bool is_remote(const char * src);
		bool relative_path_create(const char * file_path,const char * playlist_path,pfc::string_base & out);
		bool relative_path_parse(const char * relative_path,const char * playlist_path,pfc::string_base & out);
		void open(service_ptr_t<file> & p_out,const char * path, t_open_mode mode,abort_callback & p_abort);
		void create_directory(const char * path,abort_callback &);
		void list_directory(const char * p_path,directory_callback & p_out,abort_callback & p_abort);
		void get_stats(const char * p_path,t_filestats & p_stats,bool & p_is_writeable,abort_callback & p_abort);
	protected:
		//override these
		virtual const char * get_archive_type()=0;//eg. "zip", must be lowercase
		virtual t_filestats get_stats_in_archive(const char * p_archive,const char * p_file,abort_callback & p_abort) = 0;
		virtual void open_archive(service_ptr_t<file> & p_out,const char * archive,const char * file, abort_callback & p_abort)=0;//opens for reading
	public:
		//override these
		
		virtual void archive_list(const char * path,const service_ptr_t<file> & p_reader,archive_callback & p_out,bool p_want_readers)=0;

		static bool g_parse_unpack_path(const char * path,pfc::string8 & archive,pfc::string8 & file);
		static void g_make_unpack_path(pfc::string_base & path,const char * archive,const char * file,const char * name);
		void make_unpack_path(pfc::string_base & path,const char * archive,const char * file);

		
	};

	template<typename T>
	class archive_factory_t : public service_factory_single_t<T> {};


	t_filetimestamp filetimestamp_from_system_timer();

#ifdef _WIN32
	inline t_filetimestamp import_filetimestamp(FILETIME ft) {
		return *reinterpret_cast<t_filetimestamp*>(&ft);
	}
#endif

	void generate_temp_location_for_file(pfc::string_base & p_out, const char * p_origpath,const char * p_extension,const char * p_magic);


	static file_ptr fileOpen(const char * p_path,filesystem::t_open_mode p_mode,abort_callback & p_abort,double p_timeout) {
		file_ptr temp; filesystem::g_open_timeout(temp,p_path,p_mode,p_timeout,p_abort); PFC_ASSERT(temp.is_valid()); return temp;
	}

	static file_ptr fileOpenReadExisting(const char * p_path,abort_callback & p_abort,double p_timeout = 0) {
		return fileOpen(p_path,filesystem::open_mode_read,p_abort,p_timeout);
	}
	static file_ptr fileOpenWriteExisting(const char * p_path,abort_callback & p_abort,double p_timeout = 0) {
		return fileOpen(p_path,filesystem::open_mode_write_existing,p_abort,p_timeout);
	}
	static file_ptr fileOpenWriteNew(const char * p_path,abort_callback & p_abort,double p_timeout = 0) {
		return fileOpen(p_path,filesystem::open_mode_write_new,p_abort,p_timeout);
	}
	
	template<typename t_list>
	class directory_callback_retrieveList : public directory_callback {
	public:
		directory_callback_retrieveList(t_list & p_list,bool p_getFiles,bool p_getSubDirectories) : m_list(p_list), m_getFiles(p_getFiles), m_getSubDirectories(p_getSubDirectories) {}
		bool on_entry(filesystem * p_owner,abort_callback & p_abort,const char * p_url,bool p_is_subdirectory,const t_filestats & p_stats) {
			p_abort.check();
			if (p_is_subdirectory ? m_getSubDirectories : m_getFiles) {
				m_list.add_item(p_url);
			}
			return true;
		}
	private:
		const bool m_getSubDirectories;
		const bool m_getFiles;
		t_list & m_list;
	};
	template<typename t_list>
	class directory_callback_retrieveListEx : public directory_callback {
	public:
		directory_callback_retrieveListEx(t_list & p_files, t_list & p_directories) : m_files(p_files), m_directories(p_directories) {}
		bool on_entry(filesystem * p_owner,abort_callback & p_abort,const char * p_url,bool p_is_subdirectory,const t_filestats & p_stats) {
			p_abort.check();
			if (p_is_subdirectory) m_directories += p_url;
			else m_files += p_url;
			return true;
		}
	private:
		t_list & m_files;
		t_list & m_directories;
	};
	template<typename t_list> class directory_callback_retrieveListRecur : public directory_callback {
	public:
		directory_callback_retrieveListRecur(t_list & p_list) : m_list(p_list) {}
		bool on_entry(filesystem * owner,abort_callback & p_abort,const char * path, bool isSubdir, const t_filestats&) {
			if (isSubdir) {
				try { owner->list_directory(path,*this,p_abort); } catch(exception_io) {}
			} else {
				m_list.add_item(path);
			}
			return true;
		}
	private:
		t_list & m_list;
	};

	template<typename t_list>
	static void listFiles(const char * p_path,t_list & p_out,abort_callback & p_abort) {
		directory_callback_retrieveList<t_list> callback(p_out,true,false);
		filesystem::g_list_directory(p_path,callback,p_abort);
	}
	template<typename t_list>
	static void listDirectories(const char * p_path,t_list & p_out,abort_callback & p_abort) {
		directory_callback_retrieveList<t_list> callback(p_out,false,true);
		filesystem::g_list_directory(p_path,callback,p_abort);
	}
	template<typename t_list>
	static void listFilesAndDirectories(const char * p_path,t_list & p_files,t_list & p_directories,abort_callback & p_abort) {
		directory_callback_retrieveListEx<t_list> callback(p_files,p_directories);
		filesystem::g_list_directory(p_path,callback,p_abort);
	}
	template<typename t_list>
	static void listFilesRecur(const char * p_path,t_list & p_out,abort_callback & p_abort) {
		directory_callback_retrieveListRecur<t_list> callback(p_out);
		filesystem::g_list_directory(p_path,callback,p_abort);
	}

	bool extract_native_path(const char * p_fspath,pfc::string_base & p_native);
	bool _extract_native_path_ptr(const char * & p_fspath);
	bool extract_native_path_ex(const char * p_fspath, pfc::string_base & p_native);//prepends \\?\ where needed

	template<typename T>
	pfc::string getPathDisplay(const T& source) {
		pfc::string_formatter temp;
		filesystem::g_get_display_path(pfc::stringToPtr(source),temp);
		return temp.toString();
	}
	template<typename T>
	pfc::string getPathCanonical(const T& source) {
		pfc::string_formatter temp;
		filesystem::g_get_canonical_path(pfc::stringToPtr(source),temp);
		return temp.toString();
	}
}

using namespace foobar2000_io;

#include "filesystem_helper.h"
