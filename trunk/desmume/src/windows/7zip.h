/*
	Copyright (C) 2009 DeSmuME team

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

#ifndef _7ZIP_DEC_HEADER
#define _7ZIP_DEC_HEADER

#include "NDSSystem.h"

// 7zip file extraction
// NOTE: if you want to add support for opening files within archives to some part of DeSmuME,
// consider using the higher-level interface provided by OpenArchive.h instead

void InitDecoder();
void CleanupDecoder();
const char* GetSupportedFormatsFilter();

// simplest way of extracting a file after calling InitDecoder():
// int size = ArchiveFile(filename).ExtractItem(0, buf, sizeof(buf));

struct ArchiveFile
{
	ArchiveFile(const char* filename);
	virtual ~ArchiveFile();

	int GetNumItems();
	int GetItemSize(int item);
	const char* GetItemName(int item);
	int ExtractItem(int item, unsigned char* outBuffer, int bufSize) const; // returns size, or 0 if failed
	int ExtractItem(int item, const char* outFilename) const;

	bool IsCompressed();
	const char* GetArchiveTypeName();

protected:
	struct ArchiveItem
	{
		int size;
		char* name;
	};
	ArchiveItem* m_items;
	int m_numItems;
	int m_typeIndex;
	char* m_filename;
};

#endif
