#include <shlobj.h>

namespace IDataObjectUtils {

	class ReleaseStgMediumScope {
	public:
		ReleaseStgMediumScope(STGMEDIUM * medium) : m_medium(medium) {}
		~ReleaseStgMediumScope() {if (m_medium != NULL) ReleaseStgMedium(m_medium);}
	private:
		STGMEDIUM * m_medium;

		PFC_CLASS_NOT_COPYABLE_EX(ReleaseStgMediumScope)
	};

	static const DWORD DataBlockToSTGMEDIUM_SupportedTymeds = TYMED_ISTREAM | TYMED_HGLOBAL;
	static const DWORD ExtractDataObjectContent_SupportedTymeds = TYMED_ISTREAM | TYMED_HGLOBAL;

	HRESULT DataBlockToSTGMEDIUM(const void * blockPtr, t_size blockSize, STGMEDIUM * medium, DWORD tymed, bool bHere) throw();

	HGLOBAL HGlobalFromMemblock(const void * ptr,t_size size);

	HRESULT ExtractDataObjectContent(pfc::com_ptr_t<IDataObject> obj, UINT format, DWORD aspect, LONG index, pfc::array_t<t_uint8> & out);
	HRESULT ExtractDataObjectContent(pfc::com_ptr_t<IDataObject> obj, UINT format, pfc::array_t<t_uint8> & out);

	HRESULT ExtractDataObjectContentTest(pfc::com_ptr_t<IDataObject> obj, UINT format, DWORD aspect, LONG index);
	HRESULT ExtractDataObjectContentTest(pfc::com_ptr_t<IDataObject> obj, UINT format);

	HRESULT ExtractDataObjectString(pfc::com_ptr_t<IDataObject> obj, pfc::string_base & out);
	HRESULT SetDataObjectString(pfc::com_ptr_t<IDataObject> obj, const char * str);

	HRESULT SetDataObjectContent(pfc::com_ptr_t<IDataObject> obj, UINT format, DWORD aspect, LONG index, const void * data, t_size dataSize);

	HRESULT STGMEDIUMToDataBlock(const STGMEDIUM & med, pfc::array_t<t_uint8> & out);

	HRESULT ExtractDataObjectDWORD(pfc::com_ptr_t<IDataObject> obj, UINT format, DWORD & val);
	HRESULT SetDataObjectDWORD(pfc::com_ptr_t<IDataObject> obj, UINT format, DWORD val);

	HRESULT PasteSucceeded(pfc::com_ptr_t<IDataObject> obj, DWORD effect);

	class comparator_FORMATETC {
	public:
		static int compare(const FORMATETC & v1, const FORMATETC & v2) {
			int val;
			val = pfc::compare_t(v1.cfFormat,v2.cfFormat); if (val != 0) return val;
			val = pfc::compare_t(v1.dwAspect,v2.dwAspect); if (val != 0) return val;
			val = pfc::compare_t(v1.lindex,  v2.lindex  ); if (val != 0) return val;
			return 0;
		}
	};

	class CDataObjectBase : public IDataObject {
	public:
		COM_QI_BEGIN()
			COM_QI_ENTRY(IUnknown)
			COM_QI_ENTRY(IDataObject)
		COM_QI_END()

		HRESULT STDMETHODCALLTYPE GetData(FORMATETC * formatetc, STGMEDIUM * medium) {
			return GetData_internal(formatetc,medium,false);
		}
		
		HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC * formatetc, STGMEDIUM * medium) {
			return GetData_internal(formatetc,medium,true);
		}

		HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC * formatetc) {
			if (formatetc == NULL) return E_INVALIDARG;
			
			if ((DataBlockToSTGMEDIUM_SupportedTymeds & formatetc->tymed) == 0) return DV_E_TYMED;

			try {
				return RenderDataTest(formatetc->cfFormat,formatetc->dwAspect,formatetc->lindex);
			} FB2K_COM_CATCH;
		}


		HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(FORMATETC * in, FORMATETC * out) {
			//check this again
			if (in == NULL || out == NULL)
				return E_INVALIDARG;
			*out = *in;
			return DATA_S_SAMEFORMATETC;
		}

		HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD dwDirection,IEnumFORMATETC ** ppenumFormatetc) {
			if (dwDirection == DATADIR_GET) {
				if (ppenumFormatetc == NULL) return E_INVALIDARG;
				return CreateIEnumFORMATETC(ppenumFormatetc);
			} else if (dwDirection == DATADIR_SET) {
				return E_NOTIMPL;
			} else {
				return E_INVALIDARG;
			}
		}

		HRESULT STDMETHODCALLTYPE SetData(FORMATETC * pFormatetc, STGMEDIUM * pmedium, BOOL fRelease) {
			try {
				ReleaseStgMediumScope relScope(fRelease ? pmedium : NULL);
				if (pFormatetc == NULL || pmedium == NULL) return E_INVALIDARG;

				/*TCHAR buf[256];
				if (GetClipboardFormatName(pFormatetc->cfFormat,buf,tabsize(buf)) > 0) {
					buf[tabsize(buf)-1] = 0;
					OutputDebugString(TEXT("SetData: ")); OutputDebugString(buf); OutputDebugString(TEXT("\n"));
				} else {
					OutputDebugString(TEXT("SetData: unknown clipboard format.\n"));
				}*/

				pfc::array_t<t_uint8> temp;
				HRESULT state = STGMEDIUMToDataBlock(*pmedium,temp);
				if (FAILED(state)) return state;
				m_entries.set(*pFormatetc,temp);
				return S_OK;
			} FB2K_COM_CATCH;
		}
		HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC * pFormatetc, DWORD advf, IAdviseSink * pAdvSink, DWORD * pdwConnection) {return OLE_E_ADVISENOTSUPPORTED;}
		HRESULT STDMETHODCALLTYPE DUnadvise(DWORD dwConnection) {return OLE_E_ADVISENOTSUPPORTED;}
		HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA ** ppenumAdvise) {return OLE_E_ADVISENOTSUPPORTED;}
	protected:
		virtual HRESULT RenderData(UINT format,DWORD aspect,LONG dataIndex,stream_writer_formatter<> & out) const {
			FORMATETC fmt = {};
			fmt.cfFormat = format; fmt.dwAspect = aspect; fmt.lindex = dataIndex;
			const pfc::array_t<t_uint8> * entry = m_entries.query_ptr(fmt);
			if (entry != NULL) {
				out.write_raw(entry->get_ptr(), entry->get_size());
				return S_OK;
			}
			return DV_E_FORMATETC;
		}
		virtual HRESULT RenderDataTest(UINT format,DWORD aspect, LONG dataIndex) const {
			FORMATETC fmt = {};
			fmt.cfFormat = format; fmt.dwAspect = aspect; fmt.lindex = dataIndex;
			if (m_entries.have_item(fmt)) return S_OK;
			return DV_E_FORMATETC;
		}
		typedef pfc::list_base_t<FORMATETC> TFormatList;

		static void AddFormat(TFormatList & out,UINT code) {
			FORMATETC fmt = {};
			fmt.dwAspect = DVASPECT_CONTENT;
			fmt.lindex = -1;
			fmt.cfFormat = code;
			for(t_size medWalk = 0; medWalk < 32; ++medWalk) {
				const DWORD med = 1 << medWalk;
				if ((DataBlockToSTGMEDIUM_SupportedTymeds & med) != 0) {
					fmt.tymed = med;
					out.add_item(fmt);
				}
			}
		}

		virtual void EnumFormats(TFormatList & out) const {
			pfc::avltree_t<UINT> formats;
			for(t_entries::const_iterator walk = m_entries.first(); walk.is_valid(); ++walk) {
				formats.add_item( walk->m_key.cfFormat );
			}
			for(pfc::const_iterator<UINT> walk = formats.first(); walk.is_valid(); ++walk) {
				AddFormat(out, *walk);
			}
		}
		HRESULT CreateIEnumFORMATETC(IEnumFORMATETC ** outptr) const throw() {
			try {
				pfc::list_t<FORMATETC> out;
				EnumFormats(out);
				return SHCreateStdEnumFmtEtc((UINT)out.get_count(), out.get_ptr(), outptr);
			} FB2K_COM_CATCH;
		}
	private:
		HRESULT GetData_internal(FORMATETC * formatetc, STGMEDIUM * medium,bool bHere) {
			if (formatetc == NULL || medium == NULL) return E_INVALIDARG;
			
			try {
				stream_writer_formatter_simple<> out;
				HRESULT hr = RenderData(formatetc->cfFormat,formatetc->dwAspect,formatetc->lindex,out);
				if (FAILED(hr)) return hr;
				return DataBlockToSTGMEDIUM(out.m_buffer.get_ptr(),out.m_buffer.get_size(),medium,formatetc->tymed,bHere);
			} FB2K_COM_CATCH;
		}

		typedef pfc::map_t<FORMATETC, pfc::array_t<t_uint8>, comparator_FORMATETC> t_entries;
		t_entries m_entries;
	};
}
