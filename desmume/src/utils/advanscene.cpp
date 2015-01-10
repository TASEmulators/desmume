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

#include <stdio.h>
#include <time.h>

#define TIXML_USE_STL
#include "tinyxml/tinyxml.h"

#include "advanscene.h"
#include "../common.h"
#include "../mc.h"
#include "../emufile.h"

ADVANsCEne advsc;

#define _ADVANsCEne_BASE_ID "DeSmuME database (ADVANsCEne)\0x1A"
#define _ADVANsCEne_BASE_VERSION_MAJOR 1
#define _ADVANsCEne_BASE_VERSION_MINOR 0
#define _ADVANsCEne_BASE_NAME "ADVANsCEne Nintendo DS Collection"

u8 ADVANsCEne::checkDB(const char *ROMserial, u32 crc)
{
	loaded = false;
	FILE *fp = fopen(database_path.c_str(), "rb");
	if (fp)
	{
		char buf[64];
		memset(buf, 0, sizeof(buf));
		if (fread(buf, 1, strlen(_ADVANsCEne_BASE_ID), fp) == strlen(_ADVANsCEne_BASE_ID))
		{
			//printf("ID: %s\n", buf);
			if (strcmp(buf, _ADVANsCEne_BASE_ID) == 0)
			{
				if (fread(&versionBase[0], 1, 2, fp) == 2)
				{
					//printf("Version base: %i.%i\n", versionBase[0], versionBase[1]);
					if (fread(&version[0], 1, 4, fp) == 4)
					{
						//printf("Version: %c%c%c%c\n", version[3], version[2], version[1], version[0]);
						if (fread(&createTime, 1, sizeof(time_t), fp) == sizeof(time_t))
						{
							memset(buf, 0,sizeof(buf));
							// serial(8) + crc32(4) + save_type(1) = 13 + reserved(8) = 21
							while (true)
							{
								if (fread(buf, 1, 21, fp) != 21) break;

								bool serialFound = (memcmp(&buf[4], ROMserial, 4) == 0);
								u32 dbcrc = LE_TO_LOCAL_32(*(u32*)(buf+8));
								bool crcFound = (crc == dbcrc);

								if(serialFound || crcFound)
								{
									foundAsCrc = crcFound;
									foundAsSerial = serialFound;
									memcpy(&crc32, &buf[8], 4);
									memcpy(&serial[0], &buf[4], 4);
									//printf("%s founded: crc32=%04X, save type %02X\n", ROMserial, crc32, buf[12]);
									saveType = buf[12];
									fclose(fp);
									loaded = true;
									return true;
								}
							}
						}
					}
				}
			}
		}
		fclose(fp);
	}
	return false;
}

 
void ADVANsCEne::setDatabase(const char *path)
{
	database_path = path;
	
	//i guess this means it needs (re)loading on account of the path having changed
	loaded = false;
}

bool ADVANsCEne::getXMLConfig(const char *in_filename)
{
	TiXmlDocument	*xml = NULL;
	TiXmlElement	*el = NULL;
	TiXmlElement	*el_configuration = NULL;
	TiXmlElement	*el_newDat = NULL;
	
	xml = new TiXmlDocument();
	if (!xml) return false;
	if (!xml->LoadFile(in_filename)) return false;
	el = xml->FirstChildElement("dat");
	if (!el) return false;
	el_configuration = el->FirstChildElement("configuration");
	if (!el_configuration) return false;
	el = el_configuration->FirstChildElement("datName"); if (el) { datName = el->GetText() ? el->GetText() : ""; }
	el = el_configuration->FirstChildElement("datVersion"); if (el) { datVersion = el->GetText() ? el->GetText() : ""; }
	
	el_newDat = el_configuration->FirstChildElement("newDat");
	if (!el_newDat) return false;
	el = el_newDat->FirstChildElement("datVersionURL"); if (el) { urlVersion = el->GetText() ? el->GetText() : ""; }
	el = el_newDat->FirstChildElement("datURL"); if (el) { urlDat = el->GetText() ? el->GetText() : ""; }

	delete xml;
	return true;
}

u32 ADVANsCEne::convertDB(const char *in_filename, EMUFILE* output)
{
	//these strings are contained in the xml file, verbatim, so they function as enum values
	//we leave the strings here rather than pooled elsewhere to remind us that theyre part of the advanscene format.
	const char *saveTypeNames[] = {
		"Eeprom - 4 kbit",		// EEPROM 4kbit
		"Eeprom - 64 kbit",		// EEPROM 64kbit
		"Eeprom - 512 kbit",	// EEPROM 512kbit
		"Fram - 256 kbit",		// FRAM 256kbit !
		"Flash - 2 mbit",		// FLASH 2Mbit
		"Flash - 4 mbit",		// FLASH 4Mbit
		"Flash - 8 mbit",		// FLASH 8Mbit
		"Flash - 16 mbit",		// FLASH 16Mbit !
		"Flash - 32 mbit",		// FLASH 32Mbit !
		"Flash - 64 mbit",		// FLASH 64Mbit
		"Flash - 128 mbit",		// FLASH 128Mbit !
		"Flash - 256 mbit",		// FLASH 256Mbit !
		"Flash - 512 mbit"		// FLASH 512Mbit !
	};

	TiXmlDocument	*xml = NULL;
	TiXmlElement	*el = NULL;
	TiXmlElement	*el_serial = NULL;
	TiXmlElement	*el_games = NULL;
	TiXmlElement	*el_crc32 = NULL;
	TiXmlElement	*el_saveType = NULL;
	u32				crc32 = 0;
	u32				reserved = 0;

	lastImportErrorMessage = "";

	printf("Converting DB...\n");
	if (getXMLConfig(in_filename))
	{
		if (datName.size()==0) return 0;
		if (datName != _ADVANsCEne_BASE_NAME) return 0;
	}

	// Header
	output->fwrite(_ADVANsCEne_BASE_ID, strlen(_ADVANsCEne_BASE_ID));
	output->fputc(_ADVANsCEne_BASE_VERSION_MAJOR);
	output->fputc(_ADVANsCEne_BASE_VERSION_MINOR);
	if (datVersion.size()) 
		output->fwrite(&datVersion[0], datVersion.size());
	else
		output->fputc(0);
	time_t __time = time(NULL);
	output->fwrite(&__time, sizeof(time_t));

	xml = new TiXmlDocument();
	if (!xml) return 0;
	if (!xml->LoadFile(in_filename)) return 0;
	el = xml->FirstChildElement("dat");
	if (!el) return 0;
	el_games = el->FirstChildElement("games");
	if (!el_games) return 0;
	el = el_games->FirstChildElement("game");
	if (!el) return 0;
	u32 count = 0;
	while (el)
	{
		TiXmlElement* title = el->FirstChildElement("title");
		if(title)
		{
			//just a little diagnostic
			//printf("Importing %s\n",title->GetText());
		}
		else return 0;

		el_serial = el->FirstChildElement("serial");
		
		if(!el_serial)
		{
			lastImportErrorMessage = "Missing <serial> element. Did you use the right xml file? We need the RtoolDS one.";
			return 0;
		}
		output->fwrite(el_serial->GetText(), 8);

		// CRC32
		el_crc32 = el->FirstChildElement("files"); 
		sscanf_s(el_crc32->FirstChildElement("romCRC")->GetText(), "%x", &crc32);
		output->fwrite(&crc32, sizeof(u32));
		
		// Save type
		el_saveType = el->FirstChildElement("saveType"); 
		u8 selectedSaveType = 0xFF; //unknown
		if (el_saveType)
		{
			const char *tmp = el_saveType->GetText();
			if (tmp)
			{
				if (strcmp(tmp, "None")  == 0)
					selectedSaveType = 0xFE;
				else
				{
					for (u8 i = 0; i < MAX_SAVE_TYPES; i++)
					{
						if (strcmp(saveTypeNames[i], "") == 0) continue;
						if (strcasecmp(tmp, saveTypeNames[i]) == 0)
						{
							selectedSaveType = i;
							break;
						}
					}
				}
			}
		}
		output->fputc(selectedSaveType);
		output->fwrite(&reserved, sizeof(u32));
		output->fwrite(&reserved, sizeof(u32));
		count++;
		el = el->NextSiblingElement("game");
	}
	printf("\n");
	delete xml;
	if (count > 0) 
		printf("done\n");
	else
		printf("error\n");
	printf("ADVANsCEne converter: %i found\n", count);
	return count;
}
