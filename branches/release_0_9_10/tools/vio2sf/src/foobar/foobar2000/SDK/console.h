//! Namespace with functions for sending text to console. All functions are fully multi-thread safe, though they must not be called during dll initialization or deinitialization (e.g. static object constructors or destructors) when service system is not available.
namespace console
{
	void info(const char * p_message);
	void error(const char * p_message);
	void warning(const char * p_message);
	void info_location(const playable_location & src);
	void info_location(const metadb_handle_ptr & src);
	void print_location(const playable_location & src);
	void print_location(const metadb_handle_ptr & src);

	void print(const char*);
    void printf(const char*,...);
	void printfv(const char*,va_list p_arglist);

	//! Usage: console::formatter() << "blah " << somenumber << " asdf" << somestring;
	class formatter : public pfc::string_formatter {
	public:
		~formatter() {if (!is_empty()) console::print(get_ptr());}
	};
	void complain(const char * what, const char * msg);
	void complain(const char * what, std::exception const & e);
};

//! Interface receiving console output. Do not call directly; use console namespace functions instead.
class NOVTABLE console_receiver : public service_base {
public:
	virtual void print(const char * p_message,t_size p_message_length) = 0;

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(console_receiver);
};
