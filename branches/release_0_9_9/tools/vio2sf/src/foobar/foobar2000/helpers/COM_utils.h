#define FB2K_COM_CATCH catch(exception_com const & e) {return e.get_code();} catch(std::bad_alloc) {return E_OUTOFMEMORY;} catch(pfc::exception_invalid_params) {return E_INVALIDARG;} catch(...) {return E_UNEXPECTED;}

#define COM_QI_BEGIN() HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void ** ppvObject) { if (ppvObject == NULL) return E_INVALIDARG;
#define COM_QI_ENTRY(IWhat) { if (iid == IID_##IWhat) {IWhat * temp = this; temp->AddRef(); * ppvObject = temp; return S_OK;} }
#define COM_QI_END() return E_NOINTERFACE; }

#define COM_QI_CHAIN(Parent) { HRESULT status = Parent::QueryInterface(iid, ppvObject); if (SUCCEEDED(status)) return status; }
