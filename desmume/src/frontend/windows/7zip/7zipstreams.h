// COM streams for the 7-Zip in-process API.
// Based on vba-rr-modernized / FCEUX (GPL-2.0 or later), adapted for DeSmuME:
//   * wide file paths (DeSmuME passes UTF-8 archive paths on Windows)
//   * correct reference counting and QueryInterface
//   * extraction size is reported back so truncated writes are detected

#ifndef _7ZIPSTREAMS_HEADER
#define _7ZIPSTREAMS_HEADER

#include "7z/CPP/Common/MyCom.h"
#include <cstdio>
#include <cstring>

#define SEVENZ_REFCOUNT \
	public: \
	ULONG refCount = 0; \
	ULONG STDMETHODCALLTYPE AddRef() noexcept { return ++refCount; } \
	ULONG STDMETHODCALLTYPE Release() noexcept \
	{ ULONG remaining = --refCount; if (remaining == 0) delete this; return remaining; }

class SeqOutStreamBase : public ISequentialOutStream
{
public:
	virtual UINT32 Size() const = 0;
};

class SeqMemoryOutStream : public SeqOutStreamBase
{
	Byte* const output;
	UINT32 pos;
	const UINT32 size;
	SEVENZ_REFCOUNT

public:
	SeqMemoryOutStream(void* data, UINT32 bytes) : output((Byte*)data), pos(0), size(bytes) {}

	HRESULT STDMETHODCALLTYPE QueryInterface(REFGUID iid, void** out) noexcept
	{
		if (!out) return E_INVALIDARG;
		if (iid == IID_IUnknown || iid == IID_ISequentialOutStream)
		{
			*out = static_cast<ISequentialOutStream*>(this);
			AddRef();
			return S_OK;
		}
		*out = NULL;
		return E_NOINTERFACE;
	}

	HRESULT STDMETHODCALLTYPE Write(const void* data, UInt32 length, UInt32* bytesWritten) noexcept
	{
		if (bytesWritten) *bytesWritten = 0;
		if (!data && length) return E_INVALIDARG;
		if (length > size - pos) return E_FAIL;

		if (length)
			memcpy(output + pos, data, length);
		pos += length;
		if (bytesWritten) *bytesWritten = length;
		return S_OK;
	}

	UINT32 Size() const { return pos; }
};

class SeqFileOutStream : public SeqOutStreamBase
{
	FILE* file;
	UINT32 pos;
	SEVENZ_REFCOUNT

public:
	explicit SeqFileOutStream(const wchar_t* outFilename) : file(NULL), pos(0)
	{
		file = _wfopen(outFilename, L"wb");
	}
	~SeqFileOutStream()
	{
		if (file) fclose(file);
	}

	HRESULT STDMETHODCALLTYPE QueryInterface(REFGUID iid, void** out) noexcept
	{
		if (!out) return E_INVALIDARG;
		if (iid == IID_IUnknown || iid == IID_ISequentialOutStream)
		{
			*out = static_cast<ISequentialOutStream*>(this);
			AddRef();
			return S_OK;
		}
		*out = NULL;
		return E_NOINTERFACE;
	}

	HRESULT STDMETHODCALLTYPE Write(const void* data, UInt32 length, UInt32* bytesWritten) noexcept
	{
		if (bytesWritten) *bytesWritten = 0;
		if (!file) return E_FAIL;
		if (!data && length) return E_INVALIDARG;

		size_t written = length ? fwrite(data, 1, length, file) : 0;
		pos += (UINT32)written;
		if (bytesWritten) *bytesWritten = (UInt32)written;
		return S_OK;
	}

	UINT32 Size() const { return pos; }
};

class OutStream : public IArchiveExtractCallback
{
	SeqOutStreamBase* seqStream;
	const UINT32 index;
	UINT32* writtenOut;
	SEVENZ_REFCOUNT

public:
	OutStream(UINT32 itemIndex, void* data, UINT32 size, UINT32* written = NULL)
		: seqStream(NULL), index(itemIndex), writtenOut(written)
	{
		seqStream = new SeqMemoryOutStream(data, size);
		seqStream->AddRef();
		AddRef(); // balanced by the caller after IInArchive::Extract
	}
	OutStream(UINT32 itemIndex, const wchar_t* outFilename, UINT32* written = NULL)
		: seqStream(NULL), index(itemIndex), writtenOut(written)
	{
		seqStream = new SeqFileOutStream(outFilename);
		seqStream->AddRef();
		AddRef(); // balanced by the caller after IInArchive::Extract
	}
	~OutStream()
	{
		seqStream->Release();
	}

	HRESULT STDMETHODCALLTYPE QueryInterface(REFGUID iid, void** out) noexcept
	{
		if (!out) return E_INVALIDARG;
		if (iid == IID_IUnknown || iid == IID_IArchiveExtractCallback)
		{
			*out = static_cast<IArchiveExtractCallback*>(this);
			AddRef();
			return S_OK;
		}
		*out = NULL;
		return E_NOINTERFACE;
	}

	HRESULT STDMETHODCALLTYPE SetTotal(UInt64) noexcept { return S_OK; }
	HRESULT STDMETHODCALLTYPE SetCompleted(const UInt64*) noexcept { return S_OK; }
	HRESULT STDMETHODCALLTYPE PrepareOperation(Int32) noexcept { return S_OK; }

	HRESULT STDMETHODCALLTYPE GetStream(UInt32 id, ISequentialOutStream** out, Int32 mode) noexcept
	{
		if (!out) return E_INVALIDARG;
		*out = NULL;
		if (mode == NArchive::NExtract::NAskMode::kExtract ||
		    mode == NArchive::NExtract::NAskMode::kTest)
		{
			if (id != index) return S_FALSE;
			*out = seqStream;
			seqStream->AddRef();
		}
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE SetOperationResult(Int32 result) noexcept
	{
		if (writtenOut &&
		    result == NArchive::NExtract::NOperationResult::kOK)
			*writtenOut = seqStream->Size();
		return result == NArchive::NExtract::NOperationResult::kOK ? S_OK : E_FAIL;
	}
};

class InStream : public IInStream, public IStreamGetSize
{
	SEVENZ_REFCOUNT

public:
	InStream() {}
	virtual ~InStream() {}

	HRESULT STDMETHODCALLTYPE QueryInterface(REFGUID iid, void** out) noexcept
	{
		if (!out) return E_INVALIDARG;
		if (iid == IID_IUnknown || iid == IID_IInStream)
			*out = static_cast<IInStream*>(this);
		else if (iid == IID_IStreamGetSize)
			*out = static_cast<IStreamGetSize*>(this);
		else
		{
			*out = NULL;
			return E_NOINTERFACE;
		}
		AddRef();
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE GetSize(UInt64* outSize) noexcept
	{
		if (!outSize) return E_INVALIDARG;
		*outSize = Size();
		return S_OK;
	}

	virtual UInt64 Size() const = 0;
};

class InFileStream : public InStream
{
	FILE* file;
	UInt64 size;

public:
	explicit InFileStream(const wchar_t* filename) : file(NULL), size(0)
	{
		file = _wfopen(filename, L"rb");
		if (file)
		{
			fseek(file, 0, SEEK_END);
			size = (UInt64)ftell(file);
			fseek(file, 0, SEEK_SET);
		}
		AddRef(); // balanced by the caller after Open/Extract
	}
	~InFileStream()
	{
		if (file) fclose(file);
	}

	UInt64 Size() const override { return size; }

	HRESULT STDMETHODCALLTYPE Read(void* data, UInt32 length, UInt32* bytesRead) noexcept override
	{
		if (bytesRead) *bytesRead = 0;
		if (!file) return E_FAIL;
		if (!data && length) return E_INVALIDARG;
		if (!length) return S_OK;

		size_t got = fread(data, 1, length, file);
		*bytesRead = (UInt32)got;
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE Seek(Int64 offset, UInt32 origin, UInt64* newPos) noexcept override
	{
		if (!file) return E_FAIL;
		if (origin > 2) return E_INVALIDARG;
		if (fseek(file, (long)offset, (int)origin) != 0) return E_FAIL;

		long pos = ftell(file);
		if (pos < 0) return E_FAIL;
		if (newPos) *newPos = (UInt64)pos;
		return S_OK;
	}
};

#endif
