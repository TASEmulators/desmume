//! Helper class to load album art from a separate thread (without lagging GUI). In most scenarios you will want to use CAlbumArtLoader instead of deriving from CAlbumArtLoaderBase, unless you want to additionally process the images as they are loaded.
//! All methods meant to be called from the main thread.
//! IMPORTANT: derived classes must call Abort() in their own destructor rather than relying on CAlbumArtLoaderBase destructor, since the thread calls virtual functions!
class CAlbumArtLoaderBase : private CSimpleThread {
public:
	CAlbumArtLoaderBase() : m_api(static_api_ptr_t<album_art_manager>()->instantiate()) {}
	~CAlbumArtLoaderBase() {Abort();}
	
	//! Requests the loader to process specific album art type. See album_art_ids namespace for defined types.
	void AddType(const GUID & p_what) {
		if (!HaveType(p_what)) {
			Abort();
			m_requestIds.add_item(p_what);
			m_api->close();
		}
	}
	bool HaveType(const GUID & p_what) const {
		return m_requestIds.have_item(p_what);
	}

	void ResetTypes() {
		Abort();
		m_requestIds.remove_all();
		m_api->close();
	}

	void Abort() {
		AbortThread();
		m_notify.release();
	}

	//! Completion notify code is 1 when content has changed, 0 when content is the same as before the request (like, advanced to another track with the same album art data).
	void Request(const char * p_path,completion_notify_ptr p_notify = NULL) {
		Abort();
		m_requestPath = p_path;
		m_notify = p_notify;
		StartThread();
	}

	bool IsReady() const {return !IsWorking();}
	bool IsWorking() const {return IsThreadActive();}

protected:
	virtual void OnContent(const GUID & p_what,album_art_data_ptr p_data,abort_callback & p_abort) {}
	virtual void OnContentReset() {}

private:
	unsigned ThreadProc(abort_callback & p_abort) {
		try {
			return ProcessRequest(p_abort);
		} catch(exception_aborted) {
			return 0;
		} catch(std::exception const & e) {
			console::complain("Album Art loading failure", e);
			return 0;
		}
	}

	unsigned ProcessRequest(abort_callback & p_abort) {
		if (m_api->open(m_requestPath,p_abort)) {
			OnContentReset();
			for(pfc::chain_list_v2_t<GUID>::const_iterator walk = m_requestIds.first(); walk.is_valid(); ++walk) {
				album_art_data_ptr data;
				try {
					data = m_api->query(*walk,p_abort);
				} catch(exception_io const & e) {
					console::complain("Requested Album Art entry could not be retrieved", e);
					continue;
				}
				pfc::dynamic_assert( data.is_valid() );
				OnContent(*walk,data,p_abort);
			}
			return 1;
		} else {
			return 0;
		}
	}
	void ThreadDone(unsigned p_code) {
		//release our notify ptr before triggering callbacks, they might fire another query from inside that
		completion_notify_ptr temp; temp << m_notify;
		if (temp.is_valid()) temp->on_completion(p_code);
	}

	pfc::string8 m_requestPath;
	pfc::chain_list_v2_t<GUID> m_requestIds;

	completion_notify_ptr m_notify;

	const album_art_manager_instance_ptr m_api;

	PFC_CLASS_NOT_COPYABLE_EX(CAlbumArtLoaderBase);
};

class CAlbumArtLoader : public CAlbumArtLoaderBase {
public:
	~CAlbumArtLoader() {Abort();}
	bool Query(const GUID & p_what, album_art_data_ptr & p_data) const {
		pfc::dynamic_assert( IsReady() );
		return m_content.query(p_what,p_data);
	}
protected:
	void OnContentReset() {
		m_content.remove_all();
	}
	void OnContent(const GUID & p_what,album_art_data_ptr p_data,abort_callback & p_abort) {
		m_content.set(p_what,p_data);
	}
	pfc::map_t<GUID,album_art_data_ptr> m_content;
};
