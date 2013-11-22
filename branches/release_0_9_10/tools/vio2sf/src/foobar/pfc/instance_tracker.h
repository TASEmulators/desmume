namespace pfc {
	template<typename t_object>
	class instance_tracker_server_t {
	public:
		void add(t_object * p_object) {
			m_list.add_item(p_object);
		}
		void remove(t_object * p_object) {
			m_list.remove_item(p_object);
		}

		t_size get_count() const {return m_list.get_count();}
		t_object * get_item(t_size p_index) {return m_list[p_index];}
		t_object * operator[](t_size p_index) {return m_list[p_index];}

	private:
		ptr_list_hybrid_t<t_object,4> m_list;
	};


	template<typename t_object,instance_tracker_server_t<t_object> & p_server>
	class instance_tracker_client_t {
	public:
		instance_tracker_client_t(t_object* p_ptr) : m_ptr(NULL), m_added(false) {initialize(p_ptr);}
		instance_tracker_client_t() : m_ptr(NULL), m_added(false) {}

		void initialize(t_object * p_ptr) {
			uninitialize();
			p_server.add(p_ptr);
			m_ptr = p_ptr;
			m_added = true;
		}

		void uninitialize() {
			if (m_added) {
				p_server.remove(m_ptr);
				m_ptr = NULL;
				m_added = false;
			}
		}

		~instance_tracker_client_t() {
			uninitialize();
		}
	private:
		bool m_added;
		t_object * m_ptr;
	};
}