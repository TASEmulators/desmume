/*
	Copyright (C) 2011-2015 DeSmuME team

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

#include <string>
#include "../types.h"

class EMUFILE;

class ADVANsCEne
{
private:
	std::string database_path;
	time_t			createTime;
	u32				crc32;
	char			serial[6];
	char			version[4];
	u8				versionBase[2];
	u8				saveType;

	bool			loaded;
	bool foundAsCrc, foundAsSerial;

	// XML
	std::string datName;
	std::string datVersion;
	std::string	urlVersion;
	std::string urlDat;
	bool getXMLConfig(const char *in_filename);

public:
	ADVANsCEne()
		: saveType(0xFF),
		crc32(0),
		loaded(false)
	{
		memset(versionBase, 0, sizeof(versionBase));
		memset(version, 0, sizeof(version));
		memset(serial, 0, sizeof(serial));
	}
	void setDatabase(const char *path);
	std::string getDatabase() const { return database_path; }
	u32 convertDB(const char *in_filename, EMUFILE* output);
	u8 checkDB(const char *ROMserial, u32 crc);
	u32 getSaveType() { return saveType; }
	u32 getCRC32() { return crc32; }
	char *getSerial() { return serial; }
	bool isLoaded() { return loaded; }
	const char* getIdMethod() { 
		if(foundAsSerial && foundAsCrc) return "Serial/CRC";
		if(foundAsSerial) return "Serial";
		if(foundAsCrc) return "CRC";
		return "";
	}
	std::string lastImportErrorMessage;
};

extern ADVANsCEne advsc;
