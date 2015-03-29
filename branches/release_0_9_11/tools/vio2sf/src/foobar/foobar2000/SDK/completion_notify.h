//! Generic service for receiving notifications about async operation completion. Used by various other services.
class completion_notify : public service_base {
public:
	//! Called when an async operation has been completed. Note that on_completion is always called from main thread. You can use on_completion_async() helper if you need to signal completion while your context is in another thread.\n
	//! IMPLEMENTATION WARNING: If process being completed creates a window taking caller's window as parent, you must not destroy the parent window inside on_completion(). If you need to do so, use PostMessage() or main_thread_callback to delay the deletion.
	//! @param p_code Context-specific status code. Possible values depend on the operation being performed.
	virtual void on_completion(unsigned p_code) = 0;

	//! Helper. Queues a notification, using main_thread_callback.
	void on_completion_async(unsigned p_code);

	//! Helper. Checks for null ptr and calls on_completion_async when the ptr is not null.
	static void g_signal_completion_async(service_ptr_t<completion_notify> p_notify,unsigned p_code);

	FB2K_MAKE_SERVICE_INTERFACE(completion_notify,service_base);
};

//! Implementation helper.
class completion_notify_dummy : public completion_notify {
public:
	void on_completion(unsigned p_code) {}
};

//! Implementation helper.
class completion_notify_orphanable : public completion_notify {
public:
	virtual void orphan() = 0;
};

//! Helper implementation.
//! IMPLEMENTATION WARNING: If process being completed creates a window taking caller's window as parent, you must not destroy the parent window inside on_task_completion(). If you need to do so, use PostMessage() or main_thread_callback to delay the deletion.
template<typename t_receiver>
class completion_notify_impl : public completion_notify_orphanable {
public:
	void on_completion(unsigned p_code) {
		if (m_receiver != NULL) {
			m_receiver->on_task_completion(m_taskid,p_code);
		}
	}
	void setup(t_receiver * p_receiver, unsigned p_task_id) {m_receiver = p_receiver; m_taskid = p_task_id;}
	void orphan() {m_receiver = NULL; m_taskid = 0;}
private:
	t_receiver * m_receiver;
	unsigned m_taskid;
};

template<typename t_receiver>
service_ptr_t<completion_notify_orphanable> completion_notify_create(t_receiver * p_receiver,unsigned p_taskid) {
	service_ptr_t<completion_notify_impl<t_receiver> > instance = new service_impl_t<completion_notify_impl<t_receiver> >();
	instance->setup(p_receiver,p_taskid);
	return instance;
}

typedef service_ptr_t<completion_notify> completion_notify_ptr;
typedef service_ptr_t<completion_notify_orphanable> completion_notify_orphanable_ptr;

//! Helper base class for classes that manage nonblocking tasks and get notified back thru completion_notify interface.
class completion_notify_receiver {
public:
	completion_notify_ptr create_task(unsigned p_id) {
		completion_notify_orphanable_ptr ptr;
		if (m_tasks.query(p_id,ptr)) ptr->orphan();
		ptr = completion_notify_create(this,p_id);
		m_tasks.set(p_id,ptr);
		return ptr;
	}
	bool have_task(unsigned p_id) const {
		return m_tasks.have_item(p_id);
	}
	void orphan_task(unsigned p_id) {
		completion_notify_orphanable_ptr ptr;
		if (m_tasks.query(p_id,ptr)) {
			ptr->orphan();
			m_tasks.remove(p_id);
		}
	}
	~completion_notify_receiver() {
		orphan_all_tasks();
	}
	void orphan_all_tasks() {
		m_tasks.enumerate(orphanfunc);
		m_tasks.remove_all();
	}

	virtual void on_task_completion(unsigned p_id,unsigned p_status) {}
private:
	static void orphanfunc(unsigned,completion_notify_orphanable_ptr p_item) {p_item->orphan();}
	pfc::map_t<unsigned,completion_notify_orphanable_ptr> m_tasks;
};
