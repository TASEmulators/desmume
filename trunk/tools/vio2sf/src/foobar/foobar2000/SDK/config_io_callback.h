//! Implementing this interface lets you maintain your own configuration files rather than depending on the cfg_var system. \n
//! Note that you must not make assumptions about what happens first: config_io_callback::on_read(), initialization of cfg_var values or config_io_callback::on_read() in other components. Order of these things is undefined and will change with each run. \n
//! Use service_factory_single_t<myclass> to register your implementations. Do not call other people's implementations, core is responsible for doing that when appropriate.
class NOVTABLE config_io_callback : public service_base {
public:
	//! Called on startup. You can read your configuration file from here. \n
	//! Hint: use core_api::get_profile_path() to retrieve the path of the folder where foobar2000 configuration files are stored.
	virtual void on_read() = 0;
	//! Called on shutdown. You can write your configuration file from here.
	//! Hint: use core_api::get_profile_path() to retrieve the path of the folder where foobar2000 configuration files are stored.
	//! @param reset If set to true, our configuration is being reset, so you should wipe your files rather than rewrite them with current configuration.
	virtual void on_write(bool reset) = 0;

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(config_io_callback);
};
