namespace CF {
	template<typename TWhat> class CallForwarder {
	public:
		CallForwarder(TWhat * ptr) { m_ptr.new_t(ptr); }

		void Orphan() {*m_ptr = NULL;}
		bool IsValid() const { return *m_ptr != NULL; }
		bool IsEmpty() const { return !IsValid(); }

		TWhat * operator->() const {
			PFC_ASSERT( IsValid() );
			return *m_ptr;
		}

		TWhat & operator*() const {
			PFC_ASSERT( IsValid() );
			return **m_ptr;
		}

	private:
		pfc::rcptr_t<TWhat*> m_ptr;
	};

	template<typename TWhat> class CallForwarderMaster : public CallForwarder<TWhat> {
	public:
		CallForwarderMaster(TWhat * ptr) : CallForwarder<TWhat>(ptr) {}
		~CallForwarderMaster() { Orphan(); }

		PFC_CLASS_NOT_COPYABLE(CallForwarderMaster, CallForwarderMaster<TWhat>);
	};

}