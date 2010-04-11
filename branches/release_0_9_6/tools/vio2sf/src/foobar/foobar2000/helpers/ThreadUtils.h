namespace ThreadUtils {
	static void WaitAbortable(HANDLE ev, abort_callback & abort, DWORD timeout = INFINITE) {
		const HANDLE handles[2] = {ev, abort.get_abort_event()};
		SetLastError(0);
		const DWORD status = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
		switch(status) {
			case WAIT_OBJECT_0:
				break;
			case WAIT_OBJECT_0 + 1:
				throw exception_aborted();
			default:
				throw exception_win32(GetLastError());
		}
	}

	template<typename TWhat>
	class CObjectQueue {
	public:
		CObjectQueue() { m_event.create(true,false); }

		template<typename TSource> void Add(const TSource & source) {
			insync(m_sync);
			m_content.add_item(source);
			if (m_content.get_count() == 1) m_event.set_state(true);
		}
		template<typename TDestination> void Get(TDestination & out, abort_callback & abort) {
			WaitAbortable(m_event.get(), abort);
			insync(m_sync);
			pfc::const_iterator<TWhat> iter = m_content.first();
			pfc::dynamic_assert( iter.is_valid() );
			out = *iter;
			m_content.remove(iter);
			if (m_content.get_count() == 0) m_event.set_state(false);
		}

	private:
		win32_event m_event;
		critical_section m_sync;
		pfc::chain_list_v2_t<TWhat> m_content;
	};


	template<typename TBase>
	class CSingleThreadWrapper : protected pfc::thread {
	private:
		enum status {
			success,
			fail,
			fail_io,
			fail_io_data,
			fail_abort,
		};
	protected:
		class command {
		protected:
			command() : m_status(success), m_abort(), m_completionEvent() {}
			virtual void executeImpl(TBase &) {}
			virtual ~command() {}
		public:
			void execute(TBase & obj) {
				try {
					executeImpl(obj);
					m_status = success;
				} catch(exception_aborted const & e) {
					m_status = fail_abort; m_statusMsg = e.what();
				} catch(exception_io_data const & e) {
					m_status = fail_io_data; m_statusMsg = e.what();
				} catch(exception_io const & e) {
					m_status = fail_io; m_statusMsg = e.what();
				} catch(std::exception const & e) {
					m_status = fail; m_statusMsg = e.what();
				}
				SetEvent(m_completionEvent);
			}
			void rethrow() const {
				switch(m_status) {
					case fail:
						throw pfc::exception(m_statusMsg);
					case fail_io:
						throw exception_io(m_statusMsg);
					case fail_io_data:
						throw exception_io_data(m_statusMsg);
					case fail_abort:
						throw exception_aborted();
					case success:
						break;
					default:
						throw pfc::exception_bug_check_v2();
				}
			}
			status m_status;
			pfc::string8 m_statusMsg;
			HANDLE m_completionEvent;
			abort_callback * m_abort;
		};
		
		typedef pfc::rcptr_t<command> command_ptr;

		CSingleThreadWrapper() {		
			m_completionEvent.create(true,false);
			start();
		}

		~CSingleThreadWrapper() {
			m_threadAbort.abort();
			waitTillDone();
		}

		void invokeCommand(command_ptr cmd, abort_callback & abort) {
			abort.check();
			m_completionEvent.set_state(false);
			pfc::vartoggle_t<abort_callback*> abortToggle(cmd->m_abort, &abort);
			pfc::vartoggle_t<HANDLE> eventToggle(cmd->m_completionEvent, m_completionEvent.get() );
			m_commands.Add(cmd);
			m_completionEvent.wait_for(-1);
			//WaitAbortable(m_completionEvent.get(), abort);
			cmd->rethrow();
		}

	private:
		void threadProc() {
			try {
				TBase instance;
				for(;;) {
					command_ptr cmd;
					m_commands.Get(cmd, m_threadAbort);
					cmd->execute(instance);
				}
			} catch(...) {}
		}
		win32_event m_completionEvent;
		CObjectQueue<command_ptr> m_commands;
		abort_callback_impl m_threadAbort;
	};
}
