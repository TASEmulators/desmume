#include "foobar2000.h"

void commandline_handler_metadb_handle::on_file(const char * url)
{

	abort_callback_dummy blah;

	{
		playlist_loader_callback_impl callback(blah);

		bool fail = false;
		try {
			playlist_loader::g_process_path_ex(url,callback);
		} catch(std::exception const & e) {
			console::complain("Unhandled exception in playlist loader", e);
			fail = true;
		}

		if (!fail) {
			t_size n,m=callback.get_count();
			for(n=0;n<m;n++) on_file(callback.get_item(n));
		}
	}
}
