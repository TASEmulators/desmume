// This file is (modified) from
// FCEUX (2009)
// FCE Ultra - NES/Famicom Emulator
// Copyright (C) 2003 Xodnizel
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#ifndef _7ZIPSTREAMS_HEADER
#define _7ZIPSTREAMS_HEADER

#include "7z/CPP/Common/MyCom.h"

class ICountedSequentialOutStream : public ISequentialOutStream
{
public:
	virtual UINT32 Size() const = 0;
};

class SeqMemoryOutStream : public ICountedSequentialOutStream, private CMyUnknownImp
{
	UINT8* const output;
	UINT32 pos;
	const UINT32 size;
	ULONG refCount;

	HRESULT STDMETHODCALLTYPE QueryInterface(REFGUID, void**)
	{
		return E_NOINTERFACE;
	}

	HRESULT STDMETHODCALLTYPE Write(const void* data, UInt32 length, UInt32* bytesWritten)
	{
		if (data != NULL || size == 0)
		{
			//assert(length <= size - pos);

			if (length > size - pos)
				length = size - pos;

			if(data)
				memcpy(output + pos, data, length);
			pos += length;

			if (bytesWritten)
				*bytesWritten = length;

			return S_OK;
		}
		else
		{
			return E_INVALIDARG;
		}
	}

	MY_ADDREF_RELEASE

public:

	SeqMemoryOutStream(void* d, UINT32 s) : output((UINT8*)d), pos(0), size(s), refCount(0) {}

	virtual ~SeqMemoryOutStream()
	{
		int a = 0;
	}

	UINT32 Size() const
	{
		return pos;
	}
};

class SeqFileOutStream : public ICountedSequentialOutStream, private CMyUnknownImp
{
	FILE* file;
	UINT32 pos;
	ULONG refCount;

	HRESULT STDMETHODCALLTYPE QueryInterface(REFGUID, void**)
	{
		return E_NOINTERFACE;
	}

	HRESULT STDMETHODCALLTYPE Write(const void* data, UInt32 length, UInt32* bytesWritten)
	{
		if(!file)
			return E_FAIL;

		if (data != NULL)
		{
			int written = 0;
			if(data)
				written = fwrite(data, 1, length, file);

			pos += written;
			if (bytesWritten)
				*bytesWritten = written;

			return S_OK;
		}
		else
		{
			return E_INVALIDARG;
		}
	}

	MY_ADDREF_RELEASE

public:

	SeqFileOutStream(const char* outFilename) : pos(0), refCount(0)
	{
		file = fopen(outFilename, "wb");
	}
	virtual ~SeqFileOutStream()
	{
		if(file)
			fclose(file);
	}

	UINT32 Size() const
	{
		return pos;
	}
};


class OutStream : public IArchiveExtractCallback, private CMyUnknownImp
{
	ICountedSequentialOutStream* seqStream;
	const UINT32 index;
	ULONG refCount;

	HRESULT STDMETHODCALLTYPE QueryInterface(REFGUID, void**)
	{
		return E_NOINTERFACE;
	}

	HRESULT STDMETHODCALLTYPE PrepareOperation(Int32)
	{
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE SetTotal(UInt64)
	{
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE SetCompleted(const UInt64*)
	{
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE SetOperationResult(Int32)
	{
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE GetStream(UInt32 id, ISequentialOutStream** ptr, Int32 mode)
	{
		switch (mode)
		{
			case NArchive::NExtract::NAskMode::kExtract:
			case NArchive::NExtract::NAskMode::kTest:

				if (id != index || ptr == NULL)
					return S_FALSE;
				else
					*ptr = seqStream;
			// fall through
			case NArchive::NExtract::NAskMode::kSkip:
				return S_OK;

			default:
				return E_INVALIDARG;
		}
	}

	MY_ADDREF_RELEASE

public:

	OutStream(UINT32 index, void* data, UINT32 size) : index(index), refCount(0)
	{
		seqStream = new SeqMemoryOutStream(data, size);
		seqStream->AddRef();
	}
	OutStream(UINT32 index, const char* outFilename) : index(index), refCount(0)
	{
		seqStream = new SeqFileOutStream(outFilename);
		seqStream->AddRef();
	}
	virtual ~OutStream()
	{
		//seqStream->Release(); // commented out because apparently IInArchive::Extract() calls Release one more time than it calls AddRef
	}
	UINT32 Size() const
	{
		return seqStream->Size();
	}
};

class InStream : public IInStream, private IStreamGetSize, private CMyUnknownImp
{
	ULONG refCount;

	HRESULT STDMETHODCALLTYPE QueryInterface(REFGUID, void**)
	{
		return E_NOINTERFACE;
	}

	HRESULT STDMETHODCALLTYPE GetSize(UInt64* outSize)
	{
		if (outSize)
		{
			*outSize = size;
			return S_OK;
		}
		else
		{
			return E_INVALIDARG;
		}
	}

	MY_ADDREF_RELEASE

protected:

	UINT32 size;

public:

	explicit InStream() : refCount(0) {}
	virtual ~InStream() {}
};


class InFileStream : public InStream
{
public:

	virtual ~InFileStream()
	{
		if(file)
			fclose(file);
	}

	FILE* file;

	InFileStream(const char* fname) : file(NULL)
	{
		file = fopen(fname, "rb");
		if(file)
		{
			fseek(file, 0, SEEK_END);
			size = ftell(file);
			fseek(file, 0, SEEK_SET);
		}
	}

	HRESULT STDMETHODCALLTYPE Read(void* data, UInt32 length, UInt32* bytesRead)
	{
		if(!file)
			return E_FAIL;

		if (data != NULL || length == 0)
		{
			int read = fread(data, 1, length, file);
			
			if (bytesRead)
				*bytesRead = read;

			return S_OK;
		}
		else
		{
			return E_INVALIDARG;
		}
	}

	HRESULT STDMETHODCALLTYPE Seek(Int64 offset, UInt32 origin, UInt64* pos)
	{
		if(!file)
			return E_FAIL;

		if (origin < 3)
		{
			fseek(file, (long)offset, origin);
			origin = ftell(file);

			if (pos)
				*pos = origin;

			return S_OK;
		}
		else
		{
			return E_INVALIDARG;
		}
	
	}
};

#endif
