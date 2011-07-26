/*
	Copyright 2009-2011 DeSmuME team

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

#include "types.h"

#include "path.h"
#include <stdio.h>


//-----------------------------------
//This is taken from mono Path.cs
static const char InvalidPathChars[] = {
	'\x22', '\x3C', '\x3E', '\x7C', '\x00', '\x01', '\x02', '\x03', '\x04', '\x05', '\x06', '\x07',
	'\x08', '\x09', '\x0A', '\x0B', '\x0C', '\x0D', '\x0E', '\x0F', '\x10', '\x11', '\x12', 
	'\x13', '\x14', '\x15', '\x16', '\x17', '\x18', '\x19', '\x1A', '\x1B', '\x1C', '\x1D', 
	'\x1E', '\x1F'
};

//but it is sort of windows-specific. Does it work in linux? Maybe we'll have to make it smarter
static const char VolumeSeparatorChar = ':';
static const char AltDirectorySeparatorChar = '/';
static bool dirEqualsVolume = (DIRECTORY_DELIMITER_CHAR == VolumeSeparatorChar);


bool Path::IsPathRooted (const std::string &path)
{
	if (path.empty()) {
		return false;
	}
	
	if (path.find_first_of(InvalidPathChars) != std::string::npos) {
		return false;
	}
	
	char c = path[0];
	return (c == DIRECTORY_DELIMITER_CHAR 	||
			c == AltDirectorySeparatorChar 	||
			(!dirEqualsVolume && path.size() > 1 && path[1] == VolumeSeparatorChar));
}

std::string Path::GetFileDirectoryPath(std::string filePath)
{
	if (filePath.empty()) {
		return "";
	}
	
	size_t i = filePath.find_last_of(DIRECTORY_DELIMITER_CHAR);
	if (i == std::string::npos) {
		return filePath;
	}
	
	return filePath.substr(0, i);
}

std::string Path::GetFileNameFromPath(std::string filePath)
{
	if (filePath.empty()) {
		return "";
	}
	
	size_t i = filePath.find_last_of(DIRECTORY_DELIMITER_CHAR);
	if (i == std::string::npos) {
		return filePath;
	}
	
	return filePath.substr(i + 1);
}

std::string Path::GetFileNameWithoutExt(std::string fileName)
{
	if (fileName.empty()) {
		return "";
	}
	
	size_t i = fileName.find_last_of(FILE_EXT_DELIMITER_CHAR);
	if (i == std::string::npos) {
		return fileName;
	}
	
	return fileName.substr(0, i);
}

std::string Path::GetFileNameFromPathWithoutExt(std::string filePath)
{
	if (filePath.empty()) {
		return "";
	}
	
	std::string fileName = GetFileNameFromPath(filePath);
	
	return GetFileNameWithoutExt(fileName);
}

std::string Path::GetFileExt(std::string fileName)
{
	if (fileName.empty()) {
		return "";
	}
	
	size_t i = fileName.find_last_of(FILE_EXT_DELIMITER_CHAR);
	if (i == std::string::npos) {
		return fileName;
	}
	
	return fileName.substr(i + 1);
}

//-----------------------------------
#ifdef _WINDOWS
void FCEUD_MakePathDirs(const char *fname)
{
	char path[MAX_PATH];
	const char* div = fname;

	do
	{
		const char* fptr = strchr(div, '\\');

		if(!fptr)
		{
			fptr = strchr(div, '/');
		}

		if(!fptr)
		{
			break;
		}

		int off = fptr - fname;
		strncpy(path, fname, off);
		path[off] = '\0';
		mkdir(path);

		div = fptr + 1;
		
		while(div[0] == '\\' || div[0] == '/')
		{
			div++;
		}

	} while(1);
}
#endif
//------------------------------