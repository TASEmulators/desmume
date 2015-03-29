namespace pfc {
	t_size getOptimalWorkerThreadCount();
	t_size getOptimalWorkerThreadCountEx(t_size taskCountLimit);

#ifdef _WINDOWS
	//! IMPORTANT: all classes derived from thread must call waitTillDone() in their destructor, to avoid object destruction during a virtual function call!
	class thread {
	public:
		PFC_DECLARE_EXCEPTION(exception_creation, exception, "Could not create thread");
		thread() : m_thread(INVALID_HANDLE_VALUE) {}
		~thread() {PFC_ASSERT(!isActive()); waitTillDone();}
		void start() {
			close();
			HANDLE thread;
			const int priority = GetThreadPriority(GetCurrentThread());
			const bool overridePriority = (priority != THREAD_PRIORITY_NORMAL);
			thread = CreateThread(NULL,0,g_entry,reinterpret_cast<void*>(this),overridePriority ? CREATE_SUSPENDED : 0,NULL);
			if (thread == NULL) throw exception_creation();
			if (overridePriority) {
				SetThreadPriority(thread, priority);
				ResumeThread(thread);
			}
			m_thread = thread;
		}
		bool isActive() const {
			return m_thread != INVALID_HANDLE_VALUE;
		}
		void waitTillDone() {
			close();
		}
	protected:
		virtual void threadProc() {PFC_ASSERT(!"Stub thread entry - should not get here");}
	private:
		void close() {
			if (isActive()) {
				WaitForSingleObject(m_thread,INFINITE);
				CloseHandle(m_thread); m_thread = INVALID_HANDLE_VALUE;
			}
		}

		static DWORD CALLBACK g_entry(void* p_instance) {
			return reinterpret_cast<thread*>(p_instance)->entry();
		}
		unsigned entry() {
			try {
				threadProc();
			} catch(...) {}
			return 0;
		}
		HANDLE m_thread;

		PFC_CLASS_NOT_COPYABLE_EX(thread)
	};
#else
#error PORTME
#endif
}
