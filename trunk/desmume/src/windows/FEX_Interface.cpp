/*
	Copyright (C) 2009-2015 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
*/

//TODO - consider making this use UTF-8 everywhere instead of wstrings. sort of cleaner. but im tired of it now. it works presently.

#include "FEX_Interface.h"
#include "File_Extractor/fex/fex.h"

#include <windows.h>
#include <string>
#include <vector>
#include <assert.h>

STDAPI GetNumberOfFormats(UINT32 *numFormats);

struct ArchiveFormatInfo
{
	std::string name;
	std::vector<std::string> extensions;
	std::vector<std::string> signatures;
	fex_type_t type;
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

	fex_init();

	const fex_type_t * type_list = fex_type_list();

	for(unsigned int i = 0; type_list[i]; i++)
	{
		ArchiveFormatInfo info;

		const char * extension = fex_type_extension(type_list[i]);
		if (strlen(extension) == 0) continue;

		info.name = fex_type_name(type_list[i]);

		info.extensions.push_back(extension + 1); // all extensions have a period on them

		info.type = type_list[i];

		const char ** signatures = fex_type_signatures(type_list[i]);

		for (unsigned int j = 0; signatures[j]; j++)
		{
			info.signatures.push_back(signatures[j]);
		}

		s_formatInfos.push_back(info);
	}
}

void CleanupDecoder()
{
	s_formatInfos.clear();
	s_supportedFormatsFilter.clear();
}

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

	//TODO - maybe windows only check here if we ever drop fex into portable code. unfortunately this is probably too unwieldy to ever happen

	//convert the filename to unicode
	wchar_t temp_wchar[MAX_PATH*2];
	char filename_utf8[MAX_PATH*4];
	MultiByteToWideChar(CP_THREAD_ACP,0,filename,-1,temp_wchar,ARRAY_SIZE(temp_wchar));
	//now convert it back to utf-8. is there a way to do this in one step?
	WideCharToMultiByte(CP_UTF8,0,temp_wchar,-1,filename_utf8,ARRAY_SIZE(filename_utf8), NULL, NULL);


	m_filename = new char[strlen(filename_utf8)+1];
	strcpy(m_filename, filename_utf8);

	// detect archive type using format signature in file
	for(size_t i = 0; i < s_formatInfos.size() && m_typeIndex < 0; i++)
	{
		for (size_t j = 0; j < s_formatInfos[i].signatures.size(); j++)
		{
			fseek(file, 0, SEEK_SET);

			std::string& formatSig = s_formatInfos[i].signatures[j];
			int len = formatSig.size();

			if(len == 0)
				continue; // because some formats have no signature

			char* fileSig = (char*)_alloca(len);
			fread(fileSig, 1, len, file);

			if(!memcmp(formatSig.c_str(), fileSig, len))
				m_typeIndex = i;
		}
	}

	// if no signature match has been found, detect archive type using filename.
	// this is only for signature-less formats
	const char* fileExt = strrchr(filename, '.');
	if(fileExt++)
	{
		for(size_t i = 0; i < s_formatInfos.size() && m_typeIndex < 0; i++)
		{
			if(s_formatInfos[i].signatures.empty())
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

		m_items[0].wname = _wcsdup(temp_wchar);
	}
	else
	{
		fex_t * object;
		fex_err_t err = fex_open_type( &object, m_filename, s_formatInfos[m_typeIndex].type );
		if ( !err )
		{
			int numItems = 0;
			while ( !fex_done(object) )
			{
				numItems++;
				err = fex_next(object);
				if ( err ) break;
			}
			err = fex_rewind(object);
			if ( err )
			{
				fex_close(object);
				return;
			}

			m_numItems = numItems;
			m_items = new ArchiveItem[m_numItems];

			for (int i = 0; i < m_numItems; i++)
			{
				ArchiveItem& item = m_items[i];

				const char * name = fex_name(object);
				item.name = new char[strlen(name)+1];
				strcpy(item.name, name);

				const wchar_t* wname = fex_wname(object);
				if(wname)
				{
					item.wname = _wcsdup(fex_wname(object));
				}
				else
				{
					const char* name = fex_name(object);
					wchar_t temp_wchar[MAX_PATH];
					//what code page to use??? who knows. 
					MultiByteToWideChar(CP_ACP,0,name,-1,temp_wchar,ARRAY_SIZE(temp_wchar));
					item.wname = _wcsdup(temp_wchar);
				}


				err = fex_stat(object);
				if (err) break;

				item.size = fex_size(object);
				item.offset = fex_tell_arc(object);

				fex_next(object);
			}
			
			fex_close(object);
		}
	}

	fclose(file);
}

ArchiveFile::~ArchiveFile()
{
	for(int i = 0; i < m_numItems; i++)
	{
		delete[] m_items[i].name;
		free(m_items[i].wname);
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

const wchar_t* ArchiveFile::GetItemNameW(int item)
{
	//assert(item >= 0 && item < m_numItems);
	if(!(item >= 0 && item < m_numItems)) return L"";
	return m_items[item].wname;
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
		fex_t * object;
		fex_err_t err = fex_open_type( &object, m_filename, s_formatInfos[m_typeIndex].type );
		if ( !err )
		{
			if ( index != 0 ) err = fex_seek_arc( object, item.offset );
			if ( !err )
			{
				err = fex_read( object, outBuffer, item.size );
				if ( !err )
				{
					fex_close( object );
					return item.size;
				}
			}
			fex_close( object );
		}

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
		fex_t * object;
		fex_err_t err = fex_open_type( &object, m_filename, s_formatInfos[m_typeIndex].type );
		if ( !err )
		{
			if ( index != 0 ) err = fex_seek_arc( object, item.offset );
			if ( !err )
			{
				unsigned char * buffer = new unsigned char[item.size];
				err = fex_read( object, buffer, item.size );
				if ( !err )
				{
					FILE * f = fopen(outFilename, "wb");
					if (f)
					{
						fwrite( buffer, 1, item.size, f );
						fclose( f );
					}
					else rv = 0;
				}
				else rv = 0;
			}
			else rv = 0;
			fex_close( object );
		}
		else rv = 0;
	}

	if(outAttributes & FILE_ATTRIBUTE_READONLY)
		SetFileAttributes(outFilename, outAttributes); // restore read-only attribute

	return rv;
}
