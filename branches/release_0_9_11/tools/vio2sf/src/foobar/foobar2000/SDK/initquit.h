//! Basic callback startup/shutdown callback, on_init is called after the main window has been created, on_quit is called before the main window is destroyed.
//! To register: static initquit_factory_t<myclass> myclass_factory;
class NOVTABLE initquit : public service_base
{
public:
	virtual void on_init() {}
	virtual void on_quit() {}

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(initquit);
};

template<typename T>
class initquit_factory_t : public service_factory_single_t<T> {};
