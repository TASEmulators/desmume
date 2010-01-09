#ifndef _foobar2000_sdk_abort_callback_h_
#define _foobar2000_sdk_abort_callback_h_

namespace foobar2000_io {

PFC_DECLARE_EXCEPTION(exception_aborted,pfc::exception,"User abort");

#ifdef _WIN32
typedef HANDLE abort_callback_event;
#else
#error PORTME
#endif

//! This class is used to signal underlying worker code whether user has decided to abort a potentially time-consuming operation. It is commonly required by all file related operations. Code that receives an abort_callback object should periodically check it and abort any operations being performed if it is signaled, typically throwing exception_aborted. \n
//! See abort_callback_impl for an implementation.
class NOVTABLE abort_callback
{
public:
	//! Returns whether user has requested the operation to be aborted.
	virtual bool is_aborting() const = 0;

	//! Retrieves event object that can be used with some OS calls. The even object becomes signaled when abort is triggered. On win32, this is equivalent to win32 event handle (see: CreateEvent). \n
	//! You must not close this handle or call any methods that change this handle's state (SetEvent() or ResetEvent()), you can only wait for it.
	virtual abort_callback_event get_abort_event() const = 0;
	
	//! Checks if user has requested the operation to be aborted, and throws exception_aborted if so.
	void check() const;

	//! For compatibility with old code. Do not call.
	inline void check_e() const {check();}

	
	//! Sleeps p_timeout_seconds or less when aborted, throws exception_aborted on abort.
	void sleep(double p_timeout_seconds) const;
	//! Sleeps p_timeout_seconds or less when aborted, returns true when execution should continue, false when not.
	bool sleep_ex(double p_timeout_seconds) const;
protected:
	abort_callback() {}
	~abort_callback() {}
};



//! Implementation of abort_callback interface.
class abort_callback_impl : public abort_callback {
public:
	abort_callback_impl() : m_aborting(false) {
		m_event.create(true,false);
	}
	inline void abort() {set_state(true);}
	inline void reset() {set_state(false);}

	void set_state(bool p_state) {m_aborting = p_state; m_event.set_state(p_state);}

	bool is_aborting() const {return m_aborting;}

	abort_callback_event get_abort_event() const {return m_event.get();}

private:
	abort_callback_impl(const abort_callback_impl &) {throw pfc::exception_not_implemented();}
	const abort_callback_impl & operator=(const abort_callback_impl&) {throw pfc::exception_not_implemented();}
	
	volatile bool m_aborting;
#ifdef WIN32
	win32_event m_event;
#endif
};

//! Dummy abort_callback that never gets aborted. To be possibly optimized in the future.
typedef abort_callback_impl abort_callback_dummy;

}

using namespace foobar2000_io;

#endif //_foobar2000_sdk_abort_callback_h_
