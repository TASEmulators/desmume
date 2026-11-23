// 7z.dll-backed archive access for DeSmuME.
// Adapted from vba-rr-modernized (GPL-2.0); interface originally shared with
// DeSmuME's old File_Extractor frontend. No archive decoder is linked into the
// executable: all handlers come from the architecture-matching 7z.dll.

#include "7zip.h"

#include "7z/C/7zTypes.h"
#include "7z/CPP/7zip/Archive/IArchive.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "7zipstreams.h"
#include "7zapi.h"
#include "../../../utils/ziparchive.h"

struct ArchiveFormatInfo
{
	std::string name;
	std::vector<std::string> extensions;
	std::string signature;
	GUID guid;
};

static std::vector<ArchiveFormatInfo> s_formatInfos;
static std::string s_supportedFormatsFilter;

static std::wstring Utf8ToWide(const char* text)
{
	if (!text || !*text) return std::wstring();
	int length = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
	if (length <= 0) return std::wstring();
	std::wstring result((size_t)length, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, text, -1, &result[0], length);
	result.resize((size_t)length - 1);
	return result;
}

static std::string WideToUtf8(const wchar_t* text)
{
	if (!text || !*text) return std::string();
	int length = WideCharToMultiByte(CP_UTF8, 0, text, -1, NULL, 0, NULL, NULL);
	if (length <= 0) return std::string();
	std::string result((size_t)length, '\0');
	WideCharToMultiByte(CP_UTF8, 0, text, -1, &result[0], length, NULL, NULL);
	result.resize((size_t)length - 1);
	return result;
}

static std::vector<std::string> tokenize(const std::string& str, const char delim)
{
	std::vector<std::string> tokens;
	size_t start = 0;
	while (start <= str.size())
	{
		size_t end = str.find(delim, start);
		if (end == std::string::npos)
		{
			if (end != start) tokens.push_back(str.substr(start));
			break;
		}
		if (end != start) tokens.push_back(str.substr(start, end - start));
		start = end + 1;
	}
	return tokens;
}

const char* GetSupportedFormatsFilter()
{
	if (s_supportedFormatsFilter.empty())
	{
		for (size_t i = 0; i < s_formatInfos.size(); i++)
			for (size_t j = 0; j < s_formatInfos[i].extensions.size(); j++)
				s_supportedFormatsFilter += ";*." + s_formatInfos[i].extensions[j];
	}
	return s_supportedFormatsFilter.c_str();
}

void InitDecoder()
{
	CleanupDecoder();
	if (!SevenZipLoad())
		return;

	UINT32 numFormats = 0;
	if (FAILED(GetNumberOfFormats(&numFormats)))
		return;

	for (UINT32 i = 0; i < numFormats; i++)
	{
		PROPVARIANT var = { VT_EMPTY };
		ArchiveFormatInfo info;

		if (SUCCEEDED(GetHandlerProperty2(i, NArchive::NHandlerPropID::kName, &var)) && var.vt == VT_BSTR)
			info.name = WideToUtf8(var.bstrVal);
		VariantClear((VARIANTARG*)&var);
		var.vt = VT_EMPTY;

		if (SUCCEEDED(GetHandlerProperty2(i, NArchive::NHandlerPropID::kExtension, &var)) && var.vt == VT_BSTR)
		{
			std::vector<std::string> exts = tokenize(WideToUtf8(var.bstrVal), ' ');
			info.extensions.insert(info.extensions.end(), exts.begin(), exts.end());
		}
		VariantClear((VARIANTARG*)&var);
		var.vt = VT_EMPTY;

		if (SUCCEEDED(GetHandlerProperty2(i, NArchive::NHandlerPropID::kSignature, &var)) && var.vt == VT_BSTR)
		{
			// 7-Zip signatures are byte sequences in a BSTR. Do not use
			// SysStringLen() here: embedded NUL bytes are significant.
			info.signature.assign((const char*)var.bstrVal, strlen((const char*)var.bstrVal));
		}
		VariantClear((VARIANTARG*)&var);
		var.vt = VT_EMPTY;

		if (SUCCEEDED(GetHandlerProperty2(i, NArchive::NHandlerPropID::kClassID, &var)) && var.vt == VT_BSTR)
			memcpy(&info.guid, var.bstrVal, sizeof(GUID));
		else
			memset(&info.guid, 0, sizeof(GUID));
		VariantClear((VARIANTARG*)&var);

		s_formatInfos.push_back(info);
	}
}

void CleanupDecoder()
{
	s_formatInfos.clear();
	s_supportedFormatsFilter.clear();
	SevenZipUnload();
}

bool IsArchiveSupportAvailable()
{
	return SevenZipIsLoaded();
}

// Extensions that must not reach the ROM loaders as raw data when 7z.dll is
// missing. With the DLL loaded, detection uses 7-Zip format signatures.
static bool LooksLikeArchive(const char* filename)
{
	static const char* extensions[] = {
		"7z", "rar", "zip", "r00", "lzma", "bz2", "bzip2", "tar", "gz", "z",
		"xz", "cab", "iso", "arj", "lzh", "lha", "cpio", "rpm", "deb", "zst"
	};

	const char* ext = strrchr(filename, '.');
	if (!ext++) return false;
	for (size_t i = 0; i < sizeof(extensions) / sizeof(*extensions); i++)
	{
		if (!_stricmp(ext, extensions[i])) return true;
	}
	return false;
}

ArchiveFile::ArchiveFile(const char* filename, const char* displayFilename)
{
	m_typeIndex = -1;
	m_numItems = 0;
	m_items = NULL;
	m_filename = NULL;
	m_displayFilename = NULL;
	m_userMadeSelection = false;
	m_unsupportedArchive = false;
	m_zip = NULL;

	if (!SevenZipIsLoaded())
	{
		const char* ext = strrchr(filename, '.');
		if (ext && !_stricmp(ext, ".zip"))
		{
			// zlib fallback: the same shared reader used by ROMReader on all
			// platforms. Keeps ZIP usable when 7z.dll is missing or replaced.
			m_zip = new ZipArchive(filename);
			if (!m_zip->IsValid())
			{
				delete m_zip;
				m_zip = NULL;
				m_unsupportedArchive = true;
				return;
			}

			m_numItems = m_zip->GetEntryCount();
			m_items = new ArchiveItem[m_numItems];
			for (int i = 0; i < m_numItems; i++)
			{
				ZipEntryInfo info;
				m_zip->GetEntryInfo(i, &info);
				m_items[i].size = info.uncompressedSize > 0x7FFFFFFFu ? 0 : (int)info.uncompressedSize;
				m_items[i].wname = Utf8ToWide(info.name);
				std::string narrow = WideToUtf8(m_items[i].wname.c_str());
				m_items[i].name = new char[narrow.size() + 1];
				memcpy(m_items[i].name, narrow.c_str(), narrow.size() + 1);
			}

			m_filename = new char[strlen(filename) + 1];
			strcpy(m_filename, filename);
			if (displayFilename)
			{
				m_displayFilename = new char[strlen(displayFilename) + 1];
				strcpy(m_displayFilename, displayFilename);
			}
			return;
		}

		if (LooksLikeArchive(filename))
			m_unsupportedArchive = true;
		return;
	}

	const std::wstring wideFilename = Utf8ToWide(filename);
	FILE* file = _wfopen(wideFilename.c_str(), L"rb");
	if (!file)
		return;

	m_filename = new char[strlen(filename) + 1];
	strcpy(m_filename, filename);
	if (displayFilename)
	{
		m_displayFilename = new char[strlen(displayFilename) + 1];
		strcpy(m_displayFilename, displayFilename);
	}

	for (size_t i = 0; i < s_formatInfos.size() && m_typeIndex < 0; i++)
	{
		const std::string& formatSig = s_formatInfos[i].signature;
		if (formatSig.empty()) continue;
		fseek(file, 0, SEEK_SET);

		std::string fileSig(formatSig.size(), '\0');
		size_t got = fread(&fileSig[0], 1, fileSig.size(), file);
		if (got == fileSig.size() && fileSig == formatSig)
			m_typeIndex = (int)i;
	}

	// Signature-less formats fall back to the file extension.
	const char* fileExt = strrchr(filename, '.');
	if (fileExt++)
	{
		for (size_t i = 0; i < s_formatInfos.size() && m_typeIndex < 0; i++)
		{
			if (!s_formatInfos[i].signature.empty()) continue;
			for (size_t j = 0; j < s_formatInfos[i].extensions.size(); j++)
			{
				if (!_stricmp(fileExt, s_formatInfos[i].extensions[j].c_str()))
				{
					m_typeIndex = (int)i;
					break;
				}
			}
		}
	}

	if (m_typeIndex < 0)
	{
		m_numItems = 1;
		m_items = new ArchiveItem[1];
		fseek(file, 0, SEEK_END);
		m_items[0].size = (int)ftell(file);
		m_items[0].name = new char[strlen(filename) + 1];
		strcpy(m_items[0].name, filename);
		m_items[0].wname = wideFilename;
	}
	else
	{
		IInArchive* archive = NULL;
		if (SUCCEEDED(CreateObject(&s_formatInfos[m_typeIndex].guid, &IID_IInArchive, (void**)&archive)))
		{
			InFileStream* stream = new InFileStream(wideFilename.c_str());
			if (SUCCEEDED(archive->Open(stream, 0, 0)))
			{
				UInt32 count = 0;
				archive->GetNumberOfItems(&count);
				m_numItems = (int)count;
				m_items = new ArchiveItem[m_numItems];

				for (UInt32 i = 0; i < count; i++)
				{
					PROPVARIANT var = { VT_EMPTY };
					ArchiveItem& item = m_items[i];

					if (SUCCEEDED(archive->GetProperty(i, kpidSize, &var)))
					{
						UInt64 size = 0;
						if (var.vt == VT_UI8) size = var.uhVal.QuadPart;
						else if (var.vt == VT_UI4) size = var.ulVal;
						item.size = size > 0x7FFFFFFFu ? 0 : (int)size;
					}
					else item.size = 0;
					VariantClear((VARIANTARG*)&var);
					var.vt = VT_EMPTY;

					item.wname.clear();
					if (SUCCEEDED(archive->GetProperty(i, kpidPath, &var)) && var.vt == VT_BSTR && var.bstrVal)
						item.wname.assign(var.bstrVal, wcslen(var.bstrVal));
					else
						item.wname = L"";
					VariantClear((VARIANTARG*)&var);
					var.vt = VT_EMPTY;

					std::string narrow = WideToUtf8(item.wname.c_str());
					item.name = new char[narrow.size() + 1];
					memcpy(item.name, narrow.c_str(), narrow.size() + 1);

					// Encrypted entries cannot be extracted by this frontend.
					if (SUCCEEDED(archive->GetProperty(i, kpidEncrypted, &var)) && var.boolVal)
						item.size = 0;
					VariantClear((VARIANTARG*)&var);
				}
				archive->Close();
			}
			stream->Release();
			archive->Release();
		}
	}

	fclose(file);
}

ArchiveFile::~ArchiveFile()
{
	for (int i = 0; i < m_numItems; i++)
		delete[] m_items[i].name;
	delete[] m_items;
	delete m_zip;
	delete[] m_filename;
	delete[] m_displayFilename;
}

const char* ArchiveFile::GetArchiveTypeName()
{
	if (m_zip) return "Zip";
	if ((size_t)m_typeIndex >= s_formatInfos.size()) return "";
	return s_formatInfos[m_typeIndex].name.c_str();
}

int ArchiveFile::GetNumItems() { return m_numItems; }

int ArchiveFile::GetItemSize(int item)
{
	if (!(item >= 0 && item < m_numItems)) return 0;
	return m_items[item].size;
}

const char* ArchiveFile::GetItemName(int item)
{
	if (!(item >= 0 && item < m_numItems)) return "";
	return m_items[item].name;
}

const wchar_t* ArchiveFile::GetItemNameW(int item) const
{
	if (!(item >= 0 && item < m_numItems)) return L"";
	return m_items[item].wname.c_str();
}

bool ArchiveFile::IsCompressed()
{
	return m_typeIndex >= 0 || m_zip != NULL;
}

int ArchiveFile::ExtractItem(int index, unsigned char* outBuffer, int bufSize) const
{
	if (!(index >= 0 && index < m_numItems)) return 0;
	const ArchiveItem& item = m_items[index];
	if (bufSize < item.size || item.size <= 0) return 0;

	if (m_zip)
		return m_zip->ExtractEntry(index, outBuffer, (unsigned int)item.size);

	if (m_typeIndex < 0)
	{
		FILE* file = _wfopen(Utf8ToWide(m_filename).c_str(), L"rb");
		if (!file) return 0;
		size_t got = fread(outBuffer, 1, item.size, file);
		fclose(file);
		return got == (size_t)item.size ? item.size : 0;
	}

	IInArchive* archive = NULL;
	UINT32 written = 0;
	HRESULT hr = E_FAIL;
	if (SUCCEEDED(CreateObject(&s_formatInfos[m_typeIndex].guid, &IID_IInArchive, (void**)&archive)))
	{
		InFileStream* stream = new InFileStream(Utf8ToWide(m_filename).c_str());
		if (SUCCEEDED(archive->Open(stream, 0, 0)))
		{
			OutStream* out = new OutStream((UInt32)index, outBuffer, (UInt32)item.size, &written);
			const UInt32 indices[1] = { (UInt32)index };
			hr = archive->Extract(indices, 1, 0, out);
			out->Release();
			archive->Close();
		}
		stream->Release();
		archive->Release();
	}
	return SUCCEEDED(hr) && written == (UInt32)item.size ? item.size : 0;
}

int ArchiveFile::ExtractItem(int index, const char* outFilename) const
{
	if (!(index >= 0 && index < m_numItems)) return 0;
	const ArchiveItem& item = m_items[index];
	if (item.size <= 0) return 0;

	DWORD attributes = GetFileAttributesA(outFilename);
	bool restoreReadOnly = attributes != INVALID_FILE_ATTRIBUTES &&
	                       (attributes & FILE_ATTRIBUTE_READONLY) != 0;
	if (restoreReadOnly)
		SetFileAttributesA(outFilename, attributes & ~FILE_ATTRIBUTE_READONLY);

	int result = 0;
	const std::wstring wideOut = Utf8ToWide(outFilename);

	if (m_zip)
	{
		unsigned char* buffer = new unsigned char[item.size];
		const int got = m_zip->ExtractEntry(index, buffer, (unsigned int)item.size);
		if (got == item.size)
		{
			FILE* out = _wfopen(wideOut.c_str(), L"wb");
			if (out)
			{
				result = (int)fwrite(buffer, 1, item.size, out) == item.size ? item.size : 0;
				fclose(out);
			}
		}
		delete[] buffer;
	}
	else if (m_typeIndex < 0)
	{
		if (CopyFileW(Utf8ToWide(m_filename).c_str(), wideOut.c_str(), FALSE))
			result = item.size;
	}
	else
	{
		IInArchive* archive = NULL;
		UINT32 written = 0;
		HRESULT hr = E_FAIL;
		if (SUCCEEDED(CreateObject(&s_formatInfos[m_typeIndex].guid, &IID_IInArchive, (void**)&archive)))
		{
			InFileStream* stream = new InFileStream(Utf8ToWide(m_filename).c_str());
			if (SUCCEEDED(archive->Open(stream, 0, 0)))
			{
				OutStream* out = new OutStream((UInt32)index, wideOut.c_str(), &written);
				const UInt32 indices[1] = { (UInt32)index };
				hr = archive->Extract(indices, 1, 0, out);
				out->Release();
				archive->Close();
			}
			stream->Release();
			archive->Release();
		}
		if (SUCCEEDED(hr) && written == (UInt32)item.size)
			result = item.size;
	}

	if (restoreReadOnly)
		SetFileAttributesA(outFilename, attributes);
	return result;
}
