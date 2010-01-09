#ifndef _INITQUIT_H_
#define _INITQUIT_H_

#include "service.h"

//init/quit callback, on_init is called after main window has been created, on_quit is called before main window is destroyed
class NOVTABLE initquit : public service_base
{
public:
	virtual void on_init() {}
	virtual void on_quit() {}
	virtual void on_system_shutdown() {}//called instead of on_quit() when system is shutting down

	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}
};

template<class T>
class initquit_factory : public service_factory_single_t<initquit,T> {};

class initquit_autoreg : public service_impl_single_t<initquit>
{
private:
	service_factory_single_ref_t<initquit,initquit_autoreg> * p_factory;
public:
	initquit_autoreg() {p_factory = new service_factory_single_ref_t<initquit,initquit_autoreg>(*this);}
	~initquit_autoreg() {delete p_factory;}
};

class initquit_simple : public initquit_autoreg
{
	void (*func)(bool is_init);
	virtual void on_init() {func(true);}
	virtual void on_quit() {func(false);}
public:
	initquit_simple(void (*p_func)(bool is_init)) {func=p_func;}
};

#endif