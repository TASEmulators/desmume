#ifndef _foobar2000_sdk_link_resolver_h_
#define _foobar2000_sdk_link_resolver_h_

//! Interface for resolving different sorts of link files.
//! Utilized by mechanisms that convert filesystem path into list of playable locations.
//! For security reasons, link may only point to playable object path, not to a playlist or another link.

class NOVTABLE link_resolver : public service_base
{
public:

	//! Tests whether specified file is supported by this link_resolver service.
	//! @param p_path Path of file being queried.
	//! @param p_extension Extension of file being queried. This is provided for performance reasons, path already includes it.
	virtual bool is_our_path(const char * p_path,const char * p_extension) = 0;

	//! Resolves a link file. Before this is called, path must be accepted by is_our_path().
	//! @param p_filehint Optional file interface to use. If null/empty, implementation should open file by itself.
	//! @param p_path Path of link file to resolve.
	//! @param p_out Receives path the link is pointing to.
	//! @param p_abort abort_callback object signaling user aborting the operation.
	virtual void resolve(service_ptr_t<file> p_filehint,const char * p_path,pfc::string_base & p_out,abort_callback & p_abort) = 0;

	//! Helper function; finds link_resolver interface that supports specified link file.
	//! @param p_out Receives link_resolver interface on success.
	//! @param p_path Path of file to query.
	//! @returns True on success, false on failure (no interface that supports specified path could be found).
	static bool g_find(service_ptr_t<link_resolver> & p_out,const char * p_path);

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(link_resolver);
};

#endif //_foobar2000_sdk_link_resolver_h_
