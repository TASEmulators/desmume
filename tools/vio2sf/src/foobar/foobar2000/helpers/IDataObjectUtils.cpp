#include "stdafx.h"


HRESULT IDataObjectUtils::DataBlockToSTGMEDIUM(const void * blockPtr, t_size blockSize, STGMEDIUM * medium, DWORD tymed, bool bHere) throw() {
	try {
		if (bHere) {
			switch(tymed) {
			case TYMED_ISTREAM:
				{
					if (medium->pstm == NULL) return E_INVALIDARG;
					ULONG written = 0;
					HRESULT state;
					state = medium->pstm->Write(blockPtr, pfc::downcast_guarded<ULONG>(blockSize),&written);
					if (FAILED(state)) return state;
					if (written != blockSize) return STG_E_MEDIUMFULL;
					return S_OK;
				}
			default:
				return DV_E_TYMED;
			}
		} else {
			if (tymed & TYMED_HGLOBAL) {
				HGLOBAL hMem = HGlobalFromMemblock(blockPtr, blockSize);
				if (hMem == NULL) return E_OUTOFMEMORY;
				medium->tymed = TYMED_HGLOBAL;
				medium->hGlobal = hMem;
				medium->pUnkForRelease = NULL;
				return S_OK;
			}
			if (tymed & TYMED_ISTREAM) {
				HRESULT state;
				HGLOBAL hMem = HGlobalFromMemblock(blockPtr, blockSize);
				if (hMem == NULL) return E_OUTOFMEMORY;
				medium->tymed = TYMED_ISTREAM;
				pfc::com_ptr_t<IStream> stream;
				if (FAILED( state = CreateStreamOnHGlobal(hMem,TRUE,stream.receive_ptr()) ) ) {
					GlobalFree(hMem);
					return state;
				}
				{
					LARGE_INTEGER wtf = {};
					if (FAILED( state = stream->Seek(wtf,STREAM_SEEK_END,NULL) ) ) {
						return state;
					}
				}
				medium->pstm = stream.get_ptr();
				medium->pUnkForRelease = stream.detach();
				return S_OK;
			}
			return DV_E_TYMED;
		}
	} catch(pfc::exception_not_implemented) {
		return E_NOTIMPL;
	} catch(std::bad_alloc) {
		return E_OUTOFMEMORY;
	} catch(...) {
		return E_UNEXPECTED;
	}
}


HGLOBAL IDataObjectUtils::HGlobalFromMemblock(const void * ptr,t_size size) {
	HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE,size);
	if (handle != NULL) {
		void * destptr = GlobalLock(handle);
		if (destptr == NULL) {
			GlobalFree(handle);
			handle = NULL;
		} else {
			memcpy(destptr,ptr,size);
			GlobalUnlock(handle);
		}
	}
	return handle;
}

HRESULT IDataObjectUtils::ExtractDataObjectContent(pfc::com_ptr_t<IDataObject> obj, UINT format, DWORD aspect, LONG index, pfc::array_t<t_uint8> & out) {
	FORMATETC fmt = {};
	fmt.cfFormat = format; fmt.dwAspect = aspect; fmt.lindex = index;
	fmt.tymed = TYMED_HGLOBAL | TYMED_ISTREAM;

	STGMEDIUM med = {};
	HRESULT state;
	if (FAILED( state = obj->GetData(&fmt,&med) ) ) return state;
	ReleaseStgMediumScope relScope(&med);
	return STGMEDIUMToDataBlock(med, out);	
}

HRESULT IDataObjectUtils::STGMEDIUMToDataBlock(const STGMEDIUM & med, pfc::array_t<t_uint8> & out) {
	switch(med.tymed) {
		case TYMED_HGLOBAL:
			{
				CGlobalLockScope lock(med.hGlobal);
				out.set_data_fromptr( (const t_uint8*) lock.GetPtr(), lock.GetSize() );
			}
			return S_OK;
		case TYMED_ISTREAM:
			{
				HRESULT state;
				IStream * stream = med.pstm;
				LARGE_INTEGER offset = {};
				STATSTG stats = {};
				if (FAILED( state = stream->Stat(&stats,STATFLAG_NONAME ) ) ) return state;
				t_size toRead = pfc::downcast_guarded<t_size>(stats.cbSize.QuadPart);
				out.set_size(toRead);
				if (FAILED( state = stream->Seek(offset,STREAM_SEEK_SET,NULL) ) ) return state;
				ULONG cbRead = 0;
				if (FAILED( state = stream->Read(out.get_ptr(), pfc::downcast_guarded<ULONG>(toRead), &cbRead) ) ) return state;
				if (cbRead != toRead) return E_UNEXPECTED;
			}
			return S_OK;
		default:
			return DV_E_TYMED;
	}
}

HRESULT IDataObjectUtils::ExtractDataObjectContent(pfc::com_ptr_t<IDataObject> obj, UINT format, pfc::array_t<t_uint8> & out) {
	return ExtractDataObjectContent(obj, format, DVASPECT_CONTENT, -1, out);
}

HRESULT IDataObjectUtils::ExtractDataObjectContentTest(pfc::com_ptr_t<IDataObject> obj, UINT format, DWORD aspect, LONG index) {
	FORMATETC fmt = {};
	fmt.cfFormat = format; fmt.dwAspect = aspect; fmt.lindex = index;
	for(t_uint32 walk = 0; walk < 32; ++walk) {
		const DWORD tymed = 1 << walk;
		if ((ExtractDataObjectContent_SupportedTymeds & tymed) != 0) {
			fmt.tymed = tymed;
			HRESULT state = obj->QueryGetData(&fmt);
			if (SUCCEEDED(state)) return S_OK;
			if (state != DV_E_TYMED) return state;
		}
	}
	return E_FAIL;
}

HRESULT IDataObjectUtils::ExtractDataObjectContentTest(pfc::com_ptr_t<IDataObject> obj, UINT format) {
	return ExtractDataObjectContentTest(obj,format,DVASPECT_CONTENT,-1);
}

HRESULT IDataObjectUtils::ExtractDataObjectString(pfc::com_ptr_t<IDataObject> obj, pfc::string_base & out) {
	pfc::array_t<t_uint8> data;
	HRESULT state;
	state = ExtractDataObjectContent(obj,CF_UNICODETEXT,data);
	if (SUCCEEDED(state)) {
		out = pfc::stringcvt::string_utf8_from_os_ex( (const wchar_t*) data.get_ptr(), data.get_size() / sizeof(wchar_t) );
		return S_OK;
	}
	state = ExtractDataObjectContent(obj,CF_TEXT,data);
	if (SUCCEEDED(state)) {
		out = pfc::stringcvt::string_utf8_from_os_ex( (const char*) data.get_ptr(), data.get_size() / sizeof(char) );
		return S_OK;
	}
	return E_FAIL;
}

HRESULT IDataObjectUtils::SetDataObjectContent(pfc::com_ptr_t<IDataObject> obj, UINT format, DWORD aspect, LONG index, const void * data, t_size dataSize) {
	STGMEDIUM med = {};
	FORMATETC fmt = {};
	fmt.cfFormat = format; fmt.dwAspect = aspect; fmt.lindex = index; fmt.tymed = TYMED_HGLOBAL;
	HRESULT state;
	if (FAILED(state = DataBlockToSTGMEDIUM(data,dataSize,&med,TYMED_HGLOBAL,false))) return state;
	return obj->SetData(&fmt,&med,TRUE);
}

HRESULT IDataObjectUtils::SetDataObjectString(pfc::com_ptr_t<IDataObject> obj, const char * str) {
	pfc::stringcvt::string_wide_from_utf8 s(str);
	return SetDataObjectContent(obj,CF_UNICODETEXT,DVASPECT_CONTENT,-1,s.get_ptr(), (s.length()+1) * sizeof(s[0]));
}

HRESULT IDataObjectUtils::ExtractDataObjectDWORD(pfc::com_ptr_t<IDataObject> obj, UINT format, DWORD & val) {
	HRESULT state;
	pfc::array_t<t_uint8> buffer;
	if (FAILED( state = ExtractDataObjectContent(obj, format, DVASPECT_CONTENT, -1, buffer) ) ) return state;
	if (buffer.get_size() < sizeof(val)) return E_UNEXPECTED;
	val = *(DWORD*) buffer.get_ptr();
	return S_OK;
}
HRESULT IDataObjectUtils::SetDataObjectDWORD(pfc::com_ptr_t<IDataObject> obj, UINT format, DWORD val) {
	return SetDataObjectContent(obj,format,DVASPECT_CONTENT,-1,&val,sizeof(val));
}
HRESULT IDataObjectUtils::PasteSucceeded(pfc::com_ptr_t<IDataObject> obj, DWORD effect) {
	return SetDataObjectDWORD(obj, RegisterClipboardFormat(CFSTR_PASTESUCCEEDED), effect);
}
