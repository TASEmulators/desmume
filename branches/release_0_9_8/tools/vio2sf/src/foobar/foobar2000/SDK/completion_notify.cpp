#include "foobar2000.h"

namespace {
	class main_thread_callback_myimpl : public main_thread_callback {
	public:
		void callback_run() {
			m_notify->on_completion(m_code);
		}

		main_thread_callback_myimpl(completion_notify_ptr p_notify,unsigned p_code) : m_notify(p_notify), m_code(p_code) {}
	private:
		completion_notify_ptr m_notify;
		unsigned m_code;
	};
}

void completion_notify::g_signal_completion_async(completion_notify_ptr p_notify,unsigned p_code) {
	if (p_notify.is_valid()) {
		static_api_ptr_t<main_thread_callback_manager>()->add_callback(new service_impl_t<main_thread_callback_myimpl>(p_notify,p_code));
	}
}

void completion_notify::on_completion_async(unsigned p_code) {
	static_api_ptr_t<main_thread_callback_manager>()->add_callback(new service_impl_t<main_thread_callback_myimpl>(this,p_code));
}