enum {
	input_flag_no_seeking					= 1 << 0,
	input_flag_no_looping					= 1 << 1,
	input_flag_playback						= 1 << 2,
	input_flag_testing_integrity			= 1 << 3,
	input_flag_allow_inaccurate_seeking		= 1 << 4,

	input_flag_simpledecode = input_flag_no_seeking|input_flag_no_looping,
};

//! Class providing interface for retrieval of information (metadata, duration, replaygain, other tech infos) from files. Also see: file_info. \n
//! Instantiating: see input_entry.\n
//! Implementing: see input_impl.

class NOVTABLE input_info_reader : public service_base
{
public:
	//! Retrieves count of subsongs in the file. 1 for non-multisubsong-enabled inputs.
	//! Note: multi-subsong handling is disabled for remote files (see: filesystem::is_remote) for performance reasons. Remote files are always assumed to be single-subsong, with null index.
	virtual t_uint32 get_subsong_count() = 0;
	
	//! Retrieves identifier of specified subsong; this identifier is meant to be used in playable_location as well as a parameter for input_info_reader::get_info().
	//! @param p_index Index of subsong to query. Must be >=0 and < get_subsong_count().
	virtual t_uint32 get_subsong(t_uint32 p_index) = 0;
	
	//! Retrieves information about specified subsong.
	//! @param p_subsong Identifier of the subsong to query. See: input_info_reader::get_subsong(), playable_location.
	//! @param p_info file_info object to fill. Must be empty on entry.
	//! @param p_abort abort_callback object signaling user aborting the operation.
	virtual void get_info(t_uint32 p_subsong,file_info & p_info,abort_callback & p_abort) = 0;

	//! Retrieves file stats. Equivalent to calling get_stats() on file object.
	virtual t_filestats get_file_stats(abort_callback & p_abort) = 0;

	FB2K_MAKE_SERVICE_INTERFACE(input_info_reader,service_base);
};

//! Class providing interface for retrieval of PCM audio data from files.\n
//! Instantiating: see input_entry.\n
//! Implementing: see input_impl.

class NOVTABLE input_decoder : public input_info_reader
{
public:
	//! Prepares to decode specified subsong; resets playback position to the beginning of specified subsong. This must be called first, before any other input_decoder methods (other than those inherited from input_info_reader). \n
	//! It is legal to set initialize() more than once, with same or different subsong, to play either the same subsong again or another subsong from same file without full reopen.\n
	//! Warning: this interface inherits from input_info_reader, it is legal to call any input_info_reader methods even during decoding! Call order is not defined, other than initialize() requirement before calling other input_decoder methods.\n
	//! @param p_subsong Subsong to decode. Should always be 0 for non-multi-subsong-enabled inputs.
	//!	@param p_flags Specifies additional hints for decoding process. It can be null, or a combination of one or more following constants: \n
	//!		input_flag_no_seeking - Indicates that seek() will never be called. Can be used to avoid building potentially expensive seektables when only sequential reading is needed.\n
	//!		input_flag_no_looping - Certain input implementations can be configured to utilize looping info from file formats they process and keep playing single file forever, or keep repeating it specified number of times. This flag indicates that such features should be disabled, for e.g. ReplayGain scan or conversion.\n
	//!		input_flag_playback	- Indicates that decoding process will be used for realtime playback rather than conversion. This can be used to reconfigure features that are relevant only for conversion and take a lot of resources, such as very slow secure CDDA reading. \n
	//!		input_flag_testing_integrity - Indicates that we're testing integrity of the file. Any recoverable problems where decoding would normally continue should cause decoder to fail with exception_io_data.
	//! @param p_abort abort_callback object signaling user aborting the operation.
	virtual void initialize(t_uint32 p_subsong,unsigned p_flags,abort_callback & p_abort) = 0;

	//! Reads/decodes one chunk of audio data. Use false return value to signal end of file (no more data to return). Before calling run(), decoding must be initialized by initialize() call.
	//! @param p_chunk audio_chunk object receiving decoded data. Contents are valid only the method returns true.
	//! @param p_abort abort_callback object signaling user aborting the operation.
	//! @returns true on success (new data decoded), false on EOF.
	virtual bool run(audio_chunk & p_chunk,abort_callback & p_abort) = 0;

	//! Seeks to specified time offset. Before seeking or other decoding calls, decoding must be initialized with initialize() call.
	//! @param p_seconds Time to seek to, in seconds. If p_seconds exceeds length of the object being decoded, succeed, and then return false from next run() call.
	//! @param p_abort abort_callback object signaling user aborting the operation.
	virtual void seek(double p_seconds,abort_callback & p_abort) = 0;
	
	//! Queries whether resource being read/decoded is seekable. If p_value is set to false, all seek() calls will fail. Before calling can_seek() or other decoding calls, decoding must be initialized with initialize() call.
	virtual bool can_seek() = 0;

	//! This function is used to signal dynamic VBR bitrate, etc. Called after each run() (or not called at all if caller doesn't care about dynamic info).
	//! @param p_out Initially contains currently displayed info (either last get_dynamic_info result or current cached info), use this object to return changed info.
	//! @param p_timestamp_delta Indicates when returned info should be displayed (in seconds, relative to first sample of last decoded chunk), initially set to 0.
	//! @returns false to keep old info, or true to indicate that changes have been made to p_info and those should be displayed.
	virtual bool get_dynamic_info(file_info & p_out, double & p_timestamp_delta) = 0;

	//! This function is used to signal dynamic live stream song titles etc. Called after each run() (or not called at all if caller doesn't care about dynamic info). The difference between this and get_dynamic_info() is frequency and relevance of dynamic info changes - get_dynamic_info_track() returns new info only on track change in the stream, returning new titles etc.
	//! @param p_out Initially contains currently displayed info (either last get_dynamic_info_track result or current cached info), use this object to return changed info.
	//! @param p_timestamp_delta Indicates when returned info should be displayed (in seconds, relative to first sample of last decoded chunk), initially set to 0.
	//! @returns false to keep old info, or true to indicate that changes have been made to p_info and those should be displayed.
	virtual bool get_dynamic_info_track(file_info & p_out, double & p_timestamp_delta) = 0;

	//! Called from playback thread before sleeping.
	//! @param p_abort abort_callback object signaling user aborting the operation.
	virtual void on_idle(abort_callback & p_abort) = 0;


	FB2K_MAKE_SERVICE_INTERFACE(input_decoder,input_info_reader);
};


class NOVTABLE input_decoder_v2 : public input_decoder {
	FB2K_MAKE_SERVICE_INTERFACE(input_decoder_v2, input_decoder)
public:

	//! OPTIONAL, throws pfc::exception_not_implemented() when not supported by this implementation.
	//! Special version of run(). Returns an audio_chunk object as well as a raw data block containing original PCM stream. This is mainly used for MD5 checks on lossless formats. \n
	//! If you set a "MD5" tech info entry in get_info(), you should make sure that run_raw() returns data stream that can be used to verify it. \n
	//! Returned raw data should be possible to cut into individual samples; size in bytes should be divisible by audio_chunk's sample count for splitting in case partial output is needed (with cuesheets etc).
	virtual bool run_raw(audio_chunk & out, mem_block_container & outRaw, abort_callback & abort) = 0;

	//! OPTIONAL, the call is ignored if this implementation doesn't support status logging. \n
	//! Mainly used to generate logs when ripping CDs etc.
	virtual void set_logger(event_logger::ptr ptr) = 0;
};

//! Class providing interface for writing metadata and replaygain info to files. Also see: file_info. \n
//! Instantiating: see input_entry.\n
//! Implementing: see input_impl.

class NOVTABLE input_info_writer : public input_info_reader
{
public:
	//! Tells the service to update file tags with new info for specified subsong.
	//! @param p_subsong Subsong to update. Should be always 0 for non-multisubsong-enabled inputs.
	//! @param p_info New info to write. Sometimes not all contents of p_info can be written. Caller will typically read info back after successful write, so e.g. tech infos that change with retag are properly maintained.
	//! @param p_abort abort_callback object signaling user aborting the operation. WARNING: abort_callback object is provided for consistency; if writing tags actually gets aborted, user will be likely left with corrupted file. Anything calling this should make sure that aborting is either impossible, or gives appropriate warning to the user first.
	virtual void set_info(t_uint32 p_subsong,const file_info & p_info,abort_callback & p_abort) = 0;
	
	//! Commits pending updates. In case of multisubsong inputs, set_info should queue the update and perform actual file access in commit(). Otherwise, actual writing can be done in set_info() and then Commit() can just do nothing and always succeed.
	//! @param p_abort abort_callback object signaling user aborting the operation. WARNING: abort_callback object is provided for consistency; if writing tags actually gets aborted, user will be likely left with corrupted file. Anything calling this should make sure that aborting is either impossible, or gives appropriate warning to the user first.
	virtual void commit(abort_callback & p_abort) = 0;

	FB2K_MAKE_SERVICE_INTERFACE(input_info_writer,input_info_reader);
};

class NOVTABLE input_entry : public service_base
{
public:
	//! Determines whether specified content type can be handled by this input.
	//! @param p_type Content type string to test.
	virtual bool is_our_content_type(const char * p_type)=0;

	//! Determines whether specified file type can be handled by this input. This must not use any kind of file access; the result should be only based on file path / extension.
	//! @param p_full_path Full URL of file being tested.
	//! @param p_extension Extension of file being tested, provided by caller for performance reasons.
	virtual bool is_our_path(const char * p_full_path,const char * p_extension)=0;
	
	//! Opens specified resource for decoding.
	//! @param p_instance Receives new input_decoder instance if successful.
	//! @param p_filehint Optional; passes file object to use for the operation; if set to null, the service will handle opening file by itself. Note that not all inputs operate on physical files that can be reached through filesystem API, some of them require this parameter to be set to null (tone and silence generators for an example).
	//! @param p_path URL of resource being opened.
	//! @param p_abort abort_callback object signaling user aborting the operation.
	virtual void open_for_decoding(service_ptr_t<input_decoder> & p_instance,service_ptr_t<file> p_filehint,const char * p_path,abort_callback & p_abort) = 0;

	//! Opens specified file for reading info.
	//! @param p_instance Receives new input_info_reader instance if successful.
	//! @param p_filehint Optional; passes file object to use for the operation; if set to null, the service will handle opening file by itself. Note that not all inputs operate on physical files that can be reached through filesystem API, some of them require this parameter to be set to null (tone and silence generators for an example).
	//! @param p_path URL of resource being opened.
	//! @param p_abort abort_callback object signaling user aborting the operation.
	virtual void open_for_info_read(service_ptr_t<input_info_reader> & p_instance,service_ptr_t<file> p_filehint,const char * p_path,abort_callback & p_abort) = 0;

	//! Opens specified file for writing info.
	//! @param p_instance Receives new input_info_writer instance if successful.
	//! @param p_filehint Optional; passes file object to use for the operation; if set to null, the service will handle opening file by itself. Note that not all inputs operate on physical files that can be reached through filesystem API, some of them require this parameter to be set to null (tone and silence generators for an example).
	//! @param p_path URL of resource being opened.
	//! @param p_abort abort_callback object signaling user aborting the operation.
	virtual void open_for_info_write(service_ptr_t<input_info_writer> & p_instance,service_ptr_t<file> p_filehint,const char * p_path,abort_callback & p_abort) = 0;

	//! Reserved for future use. Do nothing and return until specifications are finalized.
	virtual void get_extended_data(service_ptr_t<file> p_filehint,const playable_location & p_location,const GUID & p_guid,mem_block_container & p_out,abort_callback & p_abort) = 0;

	enum {
		//! Indicates that this service implements some kind of redirector that opens another input for decoding, used to avoid circular call possibility.
		flag_redirect = 1,
		//! Indicates that multi-CPU optimizations should not be used.
		flag_parallel_reads_slow = 2,
	};
	//! See flag_* enums.
	virtual unsigned get_flags() = 0;

	inline bool is_redirect() {return (get_flags() & flag_redirect) != 0;}
	inline bool are_parallel_reads_slow() {return (get_flags() & flag_parallel_reads_slow) != 0;}

	static bool g_find_service_by_path(service_ptr_t<input_entry> & p_out,const char * p_path);
	static bool g_find_service_by_content_type(service_ptr_t<input_entry> & p_out,const char * p_content_type);
	static void g_open_for_decoding(service_ptr_t<input_decoder> & p_instance,service_ptr_t<file> p_filehint,const char * p_path,abort_callback & p_abort,bool p_from_redirect = false);
	static void g_open_for_info_read(service_ptr_t<input_info_reader> & p_instance,service_ptr_t<file> p_filehint,const char * p_path,abort_callback & p_abort,bool p_from_redirect = false);
	static void g_open_for_info_write(service_ptr_t<input_info_writer> & p_instance,service_ptr_t<file> p_filehint,const char * p_path,abort_callback & p_abort,bool p_from_redirect = false);
	static void g_open_for_info_write_timeout(service_ptr_t<input_info_writer> & p_instance,service_ptr_t<file> p_filehint,const char * p_path,abort_callback & p_abort,double p_timeout,bool p_from_redirect = false);
	static bool g_is_supported_path(const char * p_path);


	void open(service_ptr_t<input_decoder> & p_instance,service_ptr_t<file> const & p_filehint,const char * p_path,abort_callback & p_abort) {open_for_decoding(p_instance,p_filehint,p_path,p_abort);}
	void open(service_ptr_t<input_info_reader> & p_instance,service_ptr_t<file> const & p_filehint,const char * p_path,abort_callback & p_abort) {open_for_info_read(p_instance,p_filehint,p_path,p_abort);}
	void open(service_ptr_t<input_info_writer> & p_instance,service_ptr_t<file> const & p_filehint,const char * p_path,abort_callback & p_abort) {open_for_info_write(p_instance,p_filehint,p_path,p_abort);}

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(input_entry);
};
