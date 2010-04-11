#include "foobar2000.h"

void abort_callback::check() const {
	if (is_aborting()) throw exception_aborted();
}

void abort_callback::sleep(double p_timeout_seconds) const {
	if (!sleep_ex(p_timeout_seconds)) throw exception_aborted();
}

bool abort_callback::sleep_ex(double p_timeout_seconds) const {
#ifdef _WIN32
	return !win32_event::g_wait_for(get_abort_event(),p_timeout_seconds);
#else
#error PORTME
#endif
}