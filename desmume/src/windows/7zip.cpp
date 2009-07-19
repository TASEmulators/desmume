#include "7zip.h"

#include "7z/C/Types.h"
#include "7z/CPP/7zip/Archive/IArchive.h"
#include "7z/CPP/Common/InitializeStaticLib.h" // important! (if using a static lib)

#include <string>
#include <vector>
#include <assert.h>

#include "7zipstreams.h" // defines OutStream and InFileStream

#define _NO_CRYPTO

STDAPI GetNumberOfFormats(UINT32 *numFormats);
STDAPI GetHandlerProperty2(UInt32 formatIndex, PROPID propID, PROPVARIANT *value);
STDAPI CreateObject(const GUID *clsid, const GUID *iid, void **outObject);

struct ArchiveFormatInfo
{
	std::string name;
	std::vector<std::string> extensions;
	std::string signature;
	GUID guid;
};

static std::vector<ArchiveFormatInfo> s_formatInfos;

static std::string wstrToStr(const wchar_t* wstr)
{
	char* str = (char*)_alloca((wcslen(wstr)+1));
    sprintf(str, "%S", wstr);
	return std::string(str);
}

static std::vector<std::string> tokenize(const std::string & str, const std::string & delim)
{
	std::vector<std::string> tokens;
	size_t p0 = 0, p1 = std::string::npos;
	while(p0 != std::string::npos)
	{
		p1 = str.find_first_of(delim, p0);
		if(p1 != p0)
		{
			std::string token = str.substr(p0, p1 - p0);
			tokens.push_back(token);
		}
		p0 = str.find_first_not_of(delim, p1);
	}
	return tokens;
}

static std::string s_supportedFormatsFilter;
const char* GetSupportedFormatsFilter()
{
	assert(!s_formatInfos.empty());
	if(s_supportedFormatsFilter.empty())
	{
		s_supportedFormatsFilter = "";
		for(size_t i = 0; i < s_formatInfos.size(); i++)
		{
			for(size_t j = 0; j < s_formatInfos[i].extensions.size(); j++)
			{
				s_supportedFormatsFilter += ";*.";
				s_supportedFormatsFilter += s_formatInfos[i].extensions[j];
			}
		}
	}
	return s_supportedFormatsFilter.c_str();
}

void InitDecoder()
{
	CleanupDecoder();

	UINT32 numFormats = 0;
	GetNumberOfFormats(&numFormats);

	for(unsigned int i = 0; i < numFormats; i++)
	{
		PROPVARIANT var = {VT_EMPTY};
		ArchiveFormatInfo info;

		GetHandlerProperty2(i, NArchive::kName, &var);
		if(var.vt == VT_BSTR)
			info.name = wstrToStr(var.bstrVal);

		GetHandlerProperty2(i, NArchive::kExtension, &var);
		if(var.vt == VT_BSTR)
			info.extensions = tokenize(wstrToStr(var.bstrVal), " ");

		GetHandlerProperty2(i, NArchive::kStartSignature, &var);
		if(var.vt == VT_BSTR)
			info.signature = (const char*)var.bstrVal; // note: there's no 100% correct way of doing this with the existing 7-zip interface, but this is much better than using SysStringLen() in any way (it would return 1 for the signature "BZh", for example)

		GetHandlerProperty2(i,NArchive::kClassID,&var);
		if(var.vt == VT_BSTR)
			memcpy(&info.guid, var.bstrVal, 16);
		else
			memset(&info.guid, 0, 16);

		s_formatInfos.push_back(info);

		VariantClear((VARIANTARG*)&var);
	}
}

void CleanupDecoder()
{
	s_formatInfos.clear();
	s_supportedFormatsFilter.clear();
}

#include "7z/CPP/7zip/Archive/Zip/ZipHandler.h"


ArchiveFile::ArchiveFile(const char* filename)
{
	assert(!s_formatInfos.empty());

	m_typeIndex = -1;
	m_numItems = 0;
	m_items = NULL;
	m_filename = NULL;

	FILE* file = fopen(filename, "rb");
	if(!file)
		return;

	m_filename = new char[strlen(filename)+1];
	strcpy(m_filename, filename);

	// detect archive type using format signature in file
	for(size_t i = 0; i < s_formatInfos.size() && m_typeIndex < 0; i++)
	{
		fseek(file, 0, SEEK_SET);
		
		std::string& formatSig = s_formatInfos[i].signature;
		int len = formatSig.size();

		if(len == 0)
			continue; // because some formats have no signature

		char* fileSig = (char*)_alloca(len);
		fread(fileSig, 1, len, file);

		if(!memcmp(formatSig.c_str(), fileSig, len))
			m_typeIndex = i;
	}

	// if no signature match has been found, detect archive type using filename.
	// this is only for signature-less formats
	const char* fileExt = strrchr(filename, '.');
	if(fileExt++)
	{
		for(size_t i = 0; i < s_formatInfos.size() && m_typeIndex < 0; i++)
		{
			if(s_formatInfos[i].signature.empty())
			{
				std::vector<std::string>& formatExts = s_formatInfos[i].extensions;
				for(size_t j = 0; j < formatExts.size(); j++)
				{
					if(!_stricmp(formatExts[j].c_str(), fileExt))
					{
						m_typeIndex = i;
						break;
					}
				}
			}
		}
	}

	if(m_typeIndex < 0)
	{
		// uncompressed

		m_numItems = 1;
		m_items = new ArchiveItem[m_numItems];

		fseek(file, 0, SEEK_END);
		m_items[0].size = ftell(file);

		m_items[0].name = new char[strlen(filename)+1];
		strcpy(m_items[0].name, filename);
	}
	else
	{
		IInArchive* object = NULL;
		if(SUCCEEDED(CreateObject(&s_formatInfos[m_typeIndex].guid, &IID_IInArchive, (void**)&object)))
		{
			InFileStream* ifs = new InFileStream(filename);
			if(SUCCEEDED(object->Open(ifs,0,0)))
			{
				UInt32 numItems = 0;
				object->GetNumberOfItems(&numItems);
				m_numItems = numItems;
				m_items = new ArchiveItem[m_numItems];

				for(int i = 0; i < m_numItems; i++)
				{
					PROPVARIANT var = {VT_EMPTY};
					ArchiveItem& item = m_items[i];

					object->GetProperty(i, kpidSize, &var);
					item.size = var.uhVal.LowPart;

					object->GetProperty(i, kpidPath, &var);
					std::string& path = wstrToStr(var.bstrVal);
					item.name = new char[path.size()+1];
					strcpy(item.name, path.c_str());

					//object->GetProperty(i, kpidMethod, &var);
					//std::string& method = wstrToStr(var.bstrVal);
					//item.method = new char[method.size()+1];
					//strcpy(item.method, method.c_str());

					object->GetProperty(i, kpidEncrypted, &var);
#ifdef _NO_CRYPTO
					if(var.boolVal)
						item.size = 0; // don't support decompressing it, pretend size zero
#else
					#error password support NYI... see client7z.cpp
					item.encrypted = !!var.boolVal;
#endif

					VariantClear((VARIANTARG*)&var);
				}

				object->Close();
			}

			object->Release();
		}
	}

	fclose(file);
}

ArchiveFile::~ArchiveFile()
{
	for(int i = 0; i < m_numItems; i++)
	{
		delete[] m_items[i].name;
	}
	delete[] m_items;
	delete[] m_filename;
}

const char* ArchiveFile::GetArchiveTypeName()
{
	assert(!s_formatInfos.empty());

	if((size_t)m_typeIndex >= s_formatInfos.size())
		return "";

	return s_formatInfos[m_typeIndex].name.c_str();
}

int ArchiveFile::GetNumItems()
{
	return m_numItems;
}

int ArchiveFile::GetItemSize(int item)
{
	assert(item >= 0 && item < m_numItems);
	if(!(item >= 0 && item < m_numItems)) return 0;
	return m_items[item].size;
}

const char* ArchiveFile::GetItemName(int item)
{
	//assert(item >= 0 && item < m_numItems);
	if(!(item >= 0 && item < m_numItems)) return "";
	return m_items[item].name;
}

bool ArchiveFile::IsCompressed()
{
	return (m_typeIndex >= 0);
}

int ArchiveFile::ExtractItem(int index, unsigned char* outBuffer, int bufSize) const
{
	assert(!s_formatInfos.empty());
	//assert(index >= 0 && index < m_numItems);
	if(!(index >= 0 && index < m_numItems)) return 0;

	ArchiveItem& item = m_items[index];

	if(bufSize < item.size)
		return 0;

	if(m_typeIndex < 0)
	{
		// uncompressed
		FILE* file = fopen(m_filename, "rb");
		fread(outBuffer, 1, item.size, file);
		fclose(file);
	}
	else
	{
		IInArchive* object = NULL;
		HRESULT hr = E_FAIL;
		if(SUCCEEDED(CreateObject(&s_formatInfos[m_typeIndex].guid, &IID_IInArchive, (void**)&object)))
		{
			InFileStream* ifs = new InFileStream(m_filename);
			if(SUCCEEDED(object->Open(ifs,0,0)))
			{
				OutStream* os = new OutStream(index, outBuffer, item.size);
				const UInt32 indices [1] = {index};
				hr = object->Extract(indices, 1, 0, os);
				object->Close();
			}
			object->Release();
		}
		if(FAILED(hr))
			return 0;
	}

	return item.size;
}



int ArchiveFile::ExtractItem(int index, const char* outFilename) const
{
	assert(!s_formatInfos.empty());
	//assert(index >= 0 && index < m_numItems);
	if(!(index >= 0 && index < m_numItems)) return 0;

	ArchiveItem& item = m_items[index];
	int rv = item.size;

	DWORD outAttributes = GetFileAttributes(outFilename);
	if(outAttributes & FILE_ATTRIBUTE_READONLY)
		SetFileAttributes(outFilename, outAttributes & ~FILE_ATTRIBUTE_READONLY); // temporarily remove read-only attribute so we can decompress to there

	if(m_typeIndex < 0)
	{
		// uncompressed
		if(!CopyFile(m_filename, outFilename, false))
			rv = 0;
	}
	else
	{
		IInArchive* object = NULL;
		HRESULT hr = E_FAIL;
		if(SUCCEEDED(CreateObject(&s_formatInfos[m_typeIndex].guid, &IID_IInArchive, (void**)&object)))
		{
			InFileStream* ifs = new InFileStream(m_filename);
			if(SUCCEEDED(object->Open(ifs,0,0)))
			{
				if(gameInfo.romdata != NULL)
					delete[] gameInfo.romdata;
				gameInfo.romdata = new char[rv];
				gameInfo.romsize = rv;
				OutStream* os = new OutStream(index, gameInfo.romdata, rv);
				const UInt32 indices [1] = {index};
				hr = object->Extract(indices, 1, 0, os);
				object->Close();
			}
			object->Release();
		}
		if(FAILED(hr))
			rv = 0;
	}

	if(outAttributes & FILE_ATTRIBUTE_READONLY)
		SetFileAttributes(outFilename, outAttributes); // restore read-only attribute

	return rv;
}
