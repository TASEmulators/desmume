//! Service providing "unpacker" functionality - processes "packed" file (such as a zip file containing a single media file inside) to allow its contents to be accessed transparently.\n
//! To access existing unpacker implementations, use unpacker::g_open helper function.\n
//! To register your own implementation, use unpacker_factory_t template.
class NOVTABLE unpacker : public service_base {
public:
	//! Attempts to open specified file for unpacking, creates interface to virtual file with uncompressed data on success. When examined file doesn't appear to be one of formats supported by this unpacker implementation, throws exception_io_data.
	//! @param p_out Receives interface to virtual file with uncompressed data on success.
	//! @param p_source Source file to process.
	//! @param p_abort abort_callback object signaling user aborting the operation.
	virtual void open(service_ptr_t<file> & p_out,const service_ptr_t<file> & p_source,abort_callback & p_abort) = 0;

	//! Static helper querying existing unpacker implementations until one that successfully opens specified file is found. Attempts to open specified file for unpacking, creates interface to virtual file with uncompressed data on success. When examined file doesn't appear to be one of formats supported by registered unpacker implementations, throws exception_io_data.
	//! @param p_out Receives interface to virtual file with uncompressed data on success.
	//! @param p_source Source file to process.
	//! @param p_abort abort_callback object signaling user aborting the operation.
	static void g_open(service_ptr_t<file> & p_out,const service_ptr_t<file> & p_source,abort_callback & p_abort);

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(unpacker);
};

template<typename t_myclass>
class unpacker_factory_t : public service_factory_single_t<t_myclass> {};
