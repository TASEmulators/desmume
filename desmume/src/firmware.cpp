/*	
	Copyright (C) 2009-2019 DeSmuME Team

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

#include "firmware.h"

#include "NDSSystem.h"
#include "MMU.h"
#include "path.h"
#include "encrypt.h"
#include "wifi.h"
#include "version.h"

#define DFC_ID_CODE	"DeSmuME Firmware User Settings"
#define DFC_ID_SIZE	sizeof(DFC_ID_CODE)
#define WIFI_SETTINGS_SIZE 0x1D5
#define WIFI_AP_SETTINGS_SIZE 0x300
#define SETTINGS_SIZE (sizeof(FWUserSettings) + WIFI_SETTINGS_SIZE + WIFI_AP_SETTINGS_SIZE)
#define DFC_FILE_SIZE (SETTINGS_SIZE + DFC_ID_SIZE)

static _KEY1	enc(&MMU.ARM7_BIOS[0x0030]);
const char *defaultNickname = DESMUME_NAME;
const char *defaultMessage = DESMUME_NAME " makes you happy!";

u16 CFIRMWARE::_getBootCodeCRC16(const u8 *arm9Data, const u32 arm9Size, const u8 *arm7Data, const u32 arm7Size)
{
	unsigned int i, j;
	u32 crc = 0xFFFF;
	const u16 val[8] = {0xC0C1, 0xC181, 0xC301, 0xC601, 0xCC01, 0xD801, 0xF001, 0xA001};

	for(i = 0; i < arm9Size; i++)
	{
		crc = (crc ^ arm9Data[i]);

		for(j = 0; j < 8; j++) 
		{
			if(crc & 0x0001)
				crc = ((crc >> 1) ^ (val[j] << (7-j)));
			else
				crc =  (crc >> 1);
		}
	}

	for(i = 0; i < arm7Size; i++)
	{
		crc = (crc ^ arm7Data[i]);

		for(j = 0; j < 8; j++) 
		{
			if(crc & 0x0001)
				crc = ((crc >> 1) ^ (val[j] << (7-j)));
			else
				crc =  (crc >> 1);
		}
	}

	return (crc & 0xFFFF);
}

u32 CFIRMWARE::_decrypt(const u8 *in, u8* &out)
{
	u32 curBlock[2] = { 0 };
	u32 blockSize = 0;
	u32 xLen = 0;

	u32 i = 0, j = 0;
	u32 xIn = 4, xOut = 0;
	u32 len = 0;
	u32 offset = 0;
	u32 windowOffset = 0;
	u8 d = 0;
	u16 data = 0;

	memcpy(curBlock, in, 8);
	enc.decrypt(curBlock);
	blockSize = (curBlock[0] >> 8);

	if (blockSize == 0) return (0);

	out = new u8 [blockSize];
	if (!out ) return (0);
	memset(out, 0xFF, blockSize);

	xLen = blockSize;
	while(xLen > 0)
	{
		d = T1ReadByte((u8*)curBlock, (xIn % 8));
		xIn++;
		if((xIn % 8) == 0)
		{
			memcpy(curBlock, in + xIn, 8);
			enc.decrypt(curBlock);
		}

		for(i = 0; i < 8; i++)
		{
			if(d & 0x80)
			{
				data = (T1ReadByte((u8*)curBlock, (xIn % 8)) << 8);
				xIn++;
				if((xIn % 8) == 0)
				{
					memcpy(curBlock, in + xIn, 8);
					enc.decrypt(curBlock);
				}
				data |= T1ReadByte((u8*)curBlock, (xIn % 8));
				xIn++;
				if((xIn % 8) == 0)
				{
					memcpy(curBlock, in + xIn, 8);
					enc.decrypt(curBlock);
				}

				len = (data >> 12) + 3;
				offset = (data & 0xFFF);
				windowOffset = (xOut - offset - 1);

				for(j = 0; j < len; j++)
				{
					T1WriteByte(out, xOut, T1ReadByte(out, windowOffset));
					xOut++;
					windowOffset++;

					xLen--;
					if(xLen == 0) return (blockSize);
				}
			}
			else
			{
				T1WriteByte(out, xOut, T1ReadByte((u8*)curBlock, (xIn % 8)));
				xOut++;
				xIn++;
				if((xIn % 8) == 0)
				{
					memcpy(curBlock, in + xIn, 8);
					enc.decrypt(curBlock);
				}

				xLen--;
				if(xLen == 0) return (blockSize);
			}

			d = ((d << 1) & 0xFF);
		}
	}
	
	return (blockSize);
}

u32 CFIRMWARE::_decompress(const u8 *in, u8* &out)
{
	u32 curBlock[2] = { 0 };
	u32 blockSize = 0;
	u32 xLen = 0;

	u32 i = 0, j = 0;
	u32 xIn = 4, xOut = 0;
	u32 len = 0;
	u32 offset = 0;
	u32 windowOffset = 0;
	u8 d = 0;
	u16 data = 0;

	memcpy(curBlock, in, 8);
	blockSize = (curBlock[0] >> 8);

	if (blockSize == 0) return (0);

	out = new u8 [blockSize];
	if (!out ) return (0);
	memset(out, 0xFF, blockSize);

	xLen = blockSize;
	while(xLen > 0)
	{
		d = T1ReadByte((u8*)curBlock, (xIn % 8));
		xIn++;
		if((xIn % 8) == 0)
		{
			memcpy(curBlock, in + xIn, 8);
		}

		for(i = 0; i < 8; i++)
		{
			if(d & 0x80)
			{
				data = (T1ReadByte((u8*)curBlock, (xIn % 8)) << 8);
				xIn++;
				if((xIn % 8) == 0)
				{
					memcpy(curBlock, in + xIn, 8);
				}
				data |= T1ReadByte((u8*)curBlock, (xIn % 8));
				xIn++;
				if((xIn % 8) == 0)
				{
					memcpy(curBlock, in + xIn, 8);
				}

				len = (data >> 12) + 3;
				offset = (data & 0xFFF);
				windowOffset = (xOut - offset - 1);

				for(j = 0; j < len; j++)
				{
					T1WriteByte(out, xOut, T1ReadByte(out, windowOffset));
					xOut++;
					windowOffset++;

					xLen--;
					if(xLen == 0) return (blockSize);
				}
			}
			else
			{
				T1WriteByte(out, xOut, T1ReadByte((u8*)curBlock, (xIn % 8)));
				xOut++;
				xIn++;
				if((xIn % 8) == 0)
				{
					memcpy(curBlock, in + xIn, 8);
				}

				xLen--;
				if(xLen == 0) return (blockSize);
			}

			d = ((d << 1) & 0xFF);
		}
	}
	
	return (blockSize);
}
//================================================================================
bool CFIRMWARE::load(const char *firmwareFilePath)
{
	size_t fileSize = 0;
	NDSFirmwareData *newFirmwareData = new NDSFirmwareData;
	
	this->_isLoaded = NDS_ReadFirmwareDataFromFile(firmwareFilePath, newFirmwareData, &fileSize, NULL, NULL);
	if (!this->_isLoaded)
	{
		delete newFirmwareData;
		return this->_isLoaded;
	}
	
	this->_fwFilePath = firmwareFilePath;
	this->_header = newFirmwareData->header;

	if (MMU.fw.size != fileSize)	// reallocate
		mc_alloc(&MMU.fw, fileSize);
	
	this->_userDataAddr = LE_TO_LOCAL_16(newFirmwareData->header.userSettingsOffset) * 8;
	
	// fix bad dump of firmware? (wrong DS type)
	// fix mario kart touch screen calibration
	if ( (newFirmwareData->header.key.unused != 0xFFFF) && (newFirmwareData->header.key.consoleType == NDS_CONSOLE_TYPE_IQUE_LITE) )
	{
		newFirmwareData->header.key.consoleType = NDS_CONSOLE_TYPE_FAT;
		newFirmwareData->header.key.unused = 0xFFFF;
	}
	
	memcpy(&MMU.fw.data, newFirmwareData, fileSize);
	delete newFirmwareData;
	
	this->_isLoaded = true;
	return this->_isLoaded;
}

bool CFIRMWARE::unpack()
{
	u32	src = 0;
	u16 shift1 = 0, shift2 = 0, shift3 = 0, shift4 = 0;
	u32 part1addr = 0, part2addr = 0, part3addr = 0, part4addr = 0, part5addr = 0;
	u32 part1ram = 0, part2ram = 0;
	u32 size = MMU.fw.size;

	if (size == 512*1024)
	{
		INFO("ERROR: 32Mbit (512Kb) firmware not supported\n");
		return false;
	}
	
	NDSFirmwareData *workingFirmwareData = new NDSFirmwareData;
	memcpy(workingFirmwareData, &MMU.fw.data, sizeof(NDSFirmwareData));
	
	shift1 = ((this->_header.shift >> 0) & 0x07);
	shift2 = ((this->_header.shift >> 3) & 0x07);
	shift3 = ((this->_header.shift >> 6) & 0x07);
	shift4 = ((this->_header.shift >> 9) & 0x07);

	// todo - add support for 512Kb 
	part1addr = (this->_header.part1_rom_boot9_addr << (2 + shift1));
	part1ram = (0x02800000 - (this->_header.part1_ram_boot9_addr << (2+shift2)));
	part2addr = (this->_header.part2_rom_boot7_addr << (2+shift3));
	part2ram = (0x03810000 - (this->_header.part2_ram_boot7_addr << (2+shift4)));
	part3addr = (this->_header.part3_rom_gui9_addr << 3);
	part4addr = (this->_header.part4_rom_wifi7_addr << 3);
	part5addr = (this->_header.part5_data_gfx_addr << 3);

	u32 ARM9bootAddr = part1ram;
	u32 ARM7bootAddr = part2ram;

	enc.init(this->_header.identifierValue, 1, 0xC);

#if 0
	enc.applyKeycode((u32*)&data[0x18]);
#else
	// fix touch coords
	workingFirmwareData->header.key.timestamp = 0;
	workingFirmwareData->header.key.consoleType = NDS_CONSOLE_TYPE_FAT;
	workingFirmwareData->header.key.unused = 0xFFFF;
#endif

	enc.init(this->_header.identifierValue, 2, 0xC);
	
	u8 *tmp_data9 = NULL;
	u8 *tmp_data7 = NULL;
	u32 arm9Size = 0;
	u32 arm7Size = 0;
	
	arm9Size = this->_decrypt(&workingFirmwareData->_raw[part1addr], tmp_data9);
	if (tmp_data9 == NULL)
	{
		delete workingFirmwareData;
		return false;
	}

	arm7Size = this->_decrypt(&workingFirmwareData->_raw[part2addr], tmp_data7);
	if (tmp_data7 == NULL)
	{
		delete [] tmp_data9;
		delete workingFirmwareData;
		return false;
	}

	u16 crc16_mine = this->_getBootCodeCRC16(tmp_data9, arm9Size, tmp_data7, arm7Size);

	if (crc16_mine != this->_header.part12_boot_crc16)
	{
		INFO("Firmware: ERROR: the boot code CRC16 (0x%04X) doesn't match the value in the firmware header (0x%04X)", crc16_mine, this->_header.part12_boot_crc16);
		delete [] tmp_data9;
		delete [] tmp_data7;
		delete workingFirmwareData;
		return false;
	}

	// Copy firmware boot codes to their respective locations
	src = 0;
	for(u32 i = 0; i < (arm9Size >> 2); i++)
	{
		_MMU_write32<ARMCPU_ARM9>(part1ram, T1ReadLong(tmp_data9, src));
		src += 4; part1ram += 4;
	}

	src = 0;
	for(u32 i = 0; i < (arm7Size >> 2); i++)
	{
		_MMU_write32<ARMCPU_ARM7>(part2ram, T1ReadLong(tmp_data7, src));
		src += 4; part2ram += 4;
	}
	
	delete [] tmp_data7;
	tmp_data7 = NULL;
	
	delete [] tmp_data9;
	tmp_data9 = NULL;
	
	const bool isPatched = (workingFirmwareData->_raw[0x17C] != 0xFF);
	
	INFO("Firmware:\n");
	INFO("- path: %s\n", this->_fwFilePath.c_str());
	INFO("- size: %i bytes (%i Mbit)\n", size, size/1024/8);
	INFO("- CRC : 0x%04X\n", this->_header.part12_boot_crc16);
	INFO("- header: \n");
	INFO("   * size firmware %i\n", ((this->_header.shift >> 12) & 0xF) * 128 * 1024);
	INFO("   * ARM9 boot code address:     0x%08X\n", part1addr);
	INFO("   * ARM9 boot code RAM address: 0x%08X\n", ARM9bootAddr);
	INFO("   * ARM9 unpacked size:         0x%08X (%i) bytes\n", arm9Size, arm9Size);
	INFO("   * ARM9 GUI code address:      0x%08X\n", part3addr);
	INFO("\n");
	INFO("   * ARM7 boot code address:     0x%08X\n", part2addr);
	INFO("   * ARM7 boot code RAM address: 0x%08X\n", ARM7bootAddr);
	INFO("   * ARM7 WiFi code address:     0x%08X\n", part4addr);
	INFO("   * ARM7 unpacked size:         0x%08X (%i) bytes\n", arm7Size, arm7Size);
	INFO("\n");
	INFO("   * Data/GFX address:           0x%08X\n", part5addr);

	if (isPatched)
	{
		u32 patch_offset = 0x3FC80;
		if (workingFirmwareData->_raw[0x17C] > 1)
			patch_offset = 0x3F680;

		memcpy(&this->_header, &workingFirmwareData->_raw[patch_offset], sizeof(this->_header));

		shift1 = ((this->_header.shift >> 0) & 0x07);
		shift2 = ((this->_header.shift >> 3) & 0x07);
		shift3 = ((this->_header.shift >> 6) & 0x07);
		shift4 = ((this->_header.shift >> 9) & 0x07);

		// todo - add support for 512Kb 
		part1addr = (this->_header.part1_rom_boot9_addr << (2 + shift1));
		part1ram = (0x02800000 - (this->_header.part1_ram_boot9_addr << (2+shift2)));
		part2addr = (this->_header.part2_rom_boot7_addr << (2+shift3));
		part2ram = (0x03810000 - (this->_header.part2_ram_boot7_addr << (2+shift4)));

		ARM9bootAddr = part1ram;
		ARM7bootAddr = part2ram;

		arm9Size = this->_decompress(&workingFirmwareData->_raw[part1addr], tmp_data9);
		if (tmp_data9 == NULL)
		{
			delete workingFirmwareData;
			return false;
		}

		arm7Size = this->_decompress(&workingFirmwareData->_raw[part2addr], tmp_data7);
		if (tmp_data7 == NULL)
		{
			delete [] tmp_data9;
			delete workingFirmwareData;
			return false;
		};
		// Copy firmware boot codes to their respective locations
		src = 0;
		for(u32 i = 0; i < (arm9Size >> 2); i++)
		{
			_MMU_write32<ARMCPU_ARM9>(part1ram, T1ReadLong(tmp_data9, src));
			src += 4; part1ram += 4;
		}

		src = 0;
		for(u32 i = 0; i < (arm7Size >> 2); i++)
		{
			_MMU_write32<ARMCPU_ARM7>(part2ram, T1ReadLong(tmp_data7, src));
			src += 4; part2ram += 4;
		}
		delete [] tmp_data7;
		delete [] tmp_data9;

		INFO("\nFlashme:\n");
		INFO("- header: \n");
		INFO("   * ARM9 boot code address:     0x%08X\n", part1addr);
		INFO("   * ARM9 boot code RAM address: 0x%08X\n", ARM9bootAddr);
		INFO("   * ARM9 unpacked size:         0x%08X (%i) bytes\n", arm9Size, arm9Size);
		INFO("\n");
		INFO("   * ARM7 boot code address:     0x%08X\n", part2addr);
		INFO("   * ARM7 boot code RAM address: 0x%08X\n", ARM7bootAddr);
		INFO("   * ARM7 unpacked size:         0x%08X (%i) bytes\n", arm7Size, arm7Size);
	}

	memcpy(&MMU.fw.data, workingFirmwareData, size);
	delete workingFirmwareData;
	
	return true;
}

bool CFIRMWARE::loadSettings(const char *firmwareUserSettingsFilePath)
{
	return NDS_ApplyFirmwareSettingsWithFile(&MMU.fw.data, firmwareUserSettingsFilePath);
}

bool CFIRMWARE::saveSettings(const char *firmwareUserSettingsFilePath)
{
	if ( (firmwareUserSettingsFilePath == NULL) || (strlen(firmwareUserSettingsFilePath) == 0) )
	{
		return false;
	}
	
	FWUserSettings &userSettings0 = MMU.fw.data.userSettings0;
	FWUserSettings &userSettings1 = MMU.fw.data.userSettings1;
	
	if (userSettings1.updateCounter == ((LE_TO_LOCAL_16(userSettings0.updateCounter) + 1) & 0x7F))
	{
		userSettings0 = userSettings1;
	}
	else
	{
		userSettings1 = userSettings0;
	}
	
	printf("Firmware: saving config");
	FILE *fp = fopen(firmwareUserSettingsFilePath, "wb");
	if (fp)
	{
		u8 *usr = new u8[DFC_FILE_SIZE];
		if (usr)
		{
			memcpy(usr, DFC_ID_CODE, DFC_ID_SIZE);
			memcpy(usr + DFC_ID_SIZE, &userSettings0, sizeof(FWUserSettings));
			memcpy(usr + DFC_ID_SIZE + sizeof(FWUserSettings), &MMU.fw.data.wifiInfo, WIFI_SETTINGS_SIZE);
			memcpy(usr + DFC_ID_SIZE + sizeof(FWUserSettings) + WIFI_SETTINGS_SIZE, &MMU.fw.data.wifiAP1, WIFI_AP_SETTINGS_SIZE);
			
			if (fwrite(usr, 1, DFC_FILE_SIZE, fp) == DFC_FILE_SIZE)
				printf(" - done\n");
			else
				printf(" - failed\n");

			delete [] usr;
		}
		fclose(fp);
	}
	else
		printf(" - failed\n");

	return true;
}

std::string CFIRMWARE::GetUserSettingsFilePath(const char *firmwareFilePath)
{
	std::string fwPath = firmwareFilePath;
	std::string fwFileName = Path::GetFileNameFromPathWithoutExt(fwPath);

	std::string configPath = path.getpath(path.BATTERY);
	std::string finalPath = configPath + DIRECTORY_DELIMITER_CHAR + fwFileName + FILE_EXT_DELIMITER_CHAR + FW_CONFIG_FILE_EXT;

	return finalPath;
}

bool CFIRMWARE::loaded()
{
	return this->_isLoaded;
}

void* CFIRMWARE::getTouchCalibrate()
{
	static TSCalInfo cal = {0};

	if (!this->_isLoaded || !CommonSettings.UseExtFirmware)
	{
		cal.adc.x1 = _MMU_read16<ARMCPU_ARM7>(0x027FFC80 + 0x58) & 0x1FFF;
		cal.adc.y1 = _MMU_read16<ARMCPU_ARM7>(0x027FFC80 + 0x5A) & 0x1FFF;
		cal.scr.x1 = _MMU_read08<ARMCPU_ARM7>(0x027FFC80 + 0x5C);
		cal.scr.y1 = _MMU_read08<ARMCPU_ARM7>(0x027FFC80 + 0x5D);
		cal.adc.x2 = _MMU_read16<ARMCPU_ARM7>(0x027FFC80 + 0x5E) & 0x1FFF;
		cal.adc.y2 = _MMU_read16<ARMCPU_ARM7>(0x027FFC80 + 0x60) & 0x1FFF;
		cal.scr.x2 = _MMU_read08<ARMCPU_ARM7>(0x027FFC80 + 0x62);
		cal.scr.y2 = _MMU_read08<ARMCPU_ARM7>(0x027FFC80 + 0x63);
	}
	else
	{
		FWUserSettings &userSettings = (FWUserSettings &)MMU.fw.data._raw[this->_userDataAddr];
		
		cal.adc.x1 = LE_TO_LOCAL_16(userSettings.tscADC_x1) & 0x1FFF;
		cal.adc.y1 = LE_TO_LOCAL_16(userSettings.tscADC_y1) & 0x1FFF;
		cal.scr.x1 = userSettings.tscPixel_x1;
		cal.scr.y1 = userSettings.tscPixel_y1;
		cal.adc.x2 = LE_TO_LOCAL_16(userSettings.tscADC_x2) & 0x1FFF;
		cal.adc.y2 = LE_TO_LOCAL_16(userSettings.tscADC_y2) & 0x1FFF;
		cal.scr.x2 = userSettings.tscPixel_x2;
		cal.scr.y2 = userSettings.tscPixel_y2;
	}

	cal.adc.width	= (cal.adc.x2 - cal.adc.x1);
	cal.adc.height	= (cal.adc.y2 - cal.adc.y1);
	cal.scr.width	= (cal.scr.x2 - cal.scr.x1);
	cal.scr.height	= (cal.scr.y2 - cal.scr.y1);

	return (void*)&cal;
}

//=====================================================================================================
static u32 calc_CRC16(u32 start, const void *inBuffer, int count)
{
	int i,j;
	const u8 *data = (u8 *)inBuffer;
	u32 crc = start & 0xffff;
	const u16 val[8] = { 0xC0C1,0xC181,0xC301,0xC601,0xCC01,0xD801,0xF001,0xA001 };
	for(i = 0; i < count; i++)
	{
		crc = crc ^ data[i];

		for(j = 0; j < 8; j++) {
			int do_bit = 0;

			if ( crc & 0x1)
				do_bit = 1;

			crc = crc >> 1;

			if ( do_bit) {
				crc = crc ^ (val[j] << (7-j));
			}
		}
	}
	return crc;
}

int copy_firmware_user_data( u8 *dest_buffer, const u8 *fw_data)
{
	/*
	* Determine which of the two user settings in the firmware is the current
	* and valid one and then copy this into the destination buffer.
	*
	* The current setting will have a greater count.
	* Settings are only valid if its CRC16 is correct.
	*/
	int user1_valid = 0;
	int user2_valid = 0;
	u32 user_settings_offset;
	u32 fw_crc;
	u32 crc;
	int copy_good = 0;

	user_settings_offset = fw_data[0x20];
	user_settings_offset |= fw_data[0x21] << 8;
	user_settings_offset <<= 3;

	if ( user_settings_offset <= 0x3FE00) {
		s32 copy_settings_offset = -1;

		crc = calc_CRC16( 0xffff, &fw_data[user_settings_offset],
			NDS_FW_USER_SETTINGS_MEM_BYTE_COUNT);
		fw_crc = fw_data[user_settings_offset + 0x72];
		fw_crc |= fw_data[user_settings_offset + 0x73] << 8;
		if ( crc == fw_crc) {
			user1_valid = 1;
		}

		crc = calc_CRC16( 0xffff, &fw_data[user_settings_offset + 0x100],
			NDS_FW_USER_SETTINGS_MEM_BYTE_COUNT);
		fw_crc = fw_data[user_settings_offset + 0x100 + 0x72];
		fw_crc |= fw_data[user_settings_offset + 0x100 + 0x73] << 8;
		if ( crc == fw_crc) {
			user2_valid = 1;
		}

		if ( user1_valid) {
			if ( user2_valid) {
				u16 count1, count2;

				count1 = fw_data[user_settings_offset + 0x70];
				count1 |= fw_data[user_settings_offset + 0x71] << 8;

				count2 = fw_data[user_settings_offset + 0x100 + 0x70];
				count2 |= fw_data[user_settings_offset + 0x100 + 0x71] << 8;

				if ( count2 > count1) {
					copy_settings_offset = user_settings_offset + 0x100;
				}
				else {
					copy_settings_offset = user_settings_offset;
				}
			}
			else {
				copy_settings_offset = user_settings_offset;
			}
		}
		else if ( user2_valid) {
			/* copy the second user settings */
			copy_settings_offset = user_settings_offset + 0x100;
		}

		if ( copy_settings_offset > 0) {
			memcpy( dest_buffer, &fw_data[copy_settings_offset],
				NDS_FW_USER_SETTINGS_MEM_BYTE_COUNT);
			copy_good = 1;
		}
	}

	return copy_good;
}

void NDS_GetDefaultFirmwareConfig(FirmwareConfig &outConfig)
{
	memset(&outConfig, 0, sizeof(FirmwareConfig));
	
	outConfig.consoleType = NDS_CONSOLE_TYPE_FAT;
	outConfig.favoriteColor = 7;
	outConfig.birthdayMonth = 6;
	outConfig.birthdayDay = 23;
	outConfig.language = 1; // English
	outConfig.backlightLevel = 3; // Max brightness
	
	// The length of the default strings, ignoring the null-terminating character.
	outConfig.nicknameLength = strlen(defaultNickname);
	if (outConfig.nicknameLength > MAX_FW_NICKNAME_LENGTH)
	{
		outConfig.nicknameLength = MAX_FW_NICKNAME_LENGTH;
	}
	
	outConfig.messageLength = strlen(defaultMessage);
	if (outConfig.messageLength > MAX_FW_MESSAGE_LENGTH)
	{
		outConfig.messageLength = MAX_FW_MESSAGE_LENGTH;
	}
	
	// Copy the default char buffers into the UTF-16 string buffers.
	for (size_t i = 0; i < outConfig.nicknameLength; i++)
	{
		outConfig.nickname[i] = defaultNickname[i];
	}
	
	for (size_t i = 0; i < outConfig.messageLength; i++)
	{
		outConfig.message[i] = defaultMessage[i];
	}
	
	outConfig.MACAddress[0] = 0x00;
	outConfig.MACAddress[1] = 0x09;
	outConfig.MACAddress[2] = 0xBF;
	outConfig.MACAddress[3] = 0x12;
	outConfig.MACAddress[4] = 0x34;
	outConfig.MACAddress[5] = 0x56;
	
	outConfig.WFCUserID[0] = 0x00;
	outConfig.WFCUserID[1] = 0x00;
	outConfig.WFCUserID[2] = 0x00;
	outConfig.WFCUserID[3] = 0x00;
	outConfig.WFCUserID[4] = 0x00;
	outConfig.WFCUserID[5] = 0x00;
	
	// default touchscreen calibration
	
	//ANCIENT DESMUME VALUES
	outConfig.tscADC_x1 = 0x200;
	outConfig.tscADC_y1 = 0x200;
	outConfig.tscPixel_x1 = 0x20 + 1; // calibration screen coords are 1-based,
	outConfig.tscPixel_y1 = 0x20 + 1; // either that or NDS_getADCTouchPosX/Y are wrong.
	//VALUES FROM NOCASH
	//outConfig.tscADC_x1 = 0x02DF;
	//outConfig.tscADC_y1 = 0x032C;
	//outConfig.tscPixel_x1 = 0x20;
	//outConfig.tscPixel_y1 = 0x20;
	
	//ANCIENT DESMUME VALUES
	outConfig.tscADC_x2 = 0xE00;
	outConfig.tscADC_y2 = 0x800;
	outConfig.tscPixel_x2 = 0xE0 + 1;
	outConfig.tscPixel_y2 = 0x80 + 1;
	//VALUES FROM NOCASH
	//outConfig.tscADC_x2 = 0x0D3B;
	//outConfig.tscADC_y2 = 0x0CE7;
	//outConfig.tscPixel_x2 = 0xE0;
	//outConfig.tscPixel_y2 = 0xA0;
	
	outConfig.subnetMask_AP1 = 24;
	outConfig.subnetMask_AP2 = 24;
	outConfig.subnetMask_AP3 = 24;
}

void NDS_GetCurrentWFCUserID(u8 *outMAC, u8 *outUserID)
{
	const NDSFirmwareData &fw = MMU.fw.data;
	
	if (outMAC != NULL)
	{
		outMAC[0] = fw.wifiInfo.MACAddr[0];
		outMAC[1] = fw.wifiInfo.MACAddr[1];
		outMAC[2] = fw.wifiInfo.MACAddr[2];
		outMAC[3] = fw.wifiInfo.MACAddr[3];
		outMAC[4] = fw.wifiInfo.MACAddr[4];
		outMAC[5] = fw.wifiInfo.MACAddr[5];
	}
	
	if (outUserID != NULL)
	{
		outUserID[0] = fw.wifiAP1.wfcUserID[0];
		outUserID[1] = fw.wifiAP1.wfcUserID[1];
		outUserID[2] = fw.wifiAP1.wfcUserID[2];
		outUserID[3] = fw.wifiAP1.wfcUserID[3];
		outUserID[4] = fw.wifiAP1.wfcUserID[4];
		outUserID[5] = fw.wifiAP1.wfcUserID[5];
	}
}

void NDS_ApplyFirmwareSettings(NDSFirmwareData *outFirmware,
							   const FW_HEADER_KEY *headerKey,
							   const FWUserSettings *userSettings0,
							   const FWUserSettings *userSettings1,
							   const FWWifiInfo *wifiInfo,
							   const FWAccessPointSettings *wifiAP1,
							   const FWAccessPointSettings *wifiAP2,
							   const FWAccessPointSettings *wifiAP3)
{
	if (outFirmware == NULL)
	{
		return;
	}
	
	if (headerKey != NULL)
	{
		memcpy(&outFirmware->header.key, headerKey, sizeof(FW_HEADER_KEY));
	}
	
	if (userSettings0 != NULL)
	{
		memcpy(&outFirmware->userSettings0, userSettings0, sizeof(FWUserSettings));
	}
	
	if (userSettings1 != NULL)
	{
		memcpy(&outFirmware->userSettings1, userSettings1, sizeof(FWUserSettings));
	}
	
	if (wifiInfo != NULL)
	{
		memcpy(&outFirmware->wifiInfo, wifiInfo, sizeof(FWWifiInfo));
	}
	
	if (wifiAP1 != NULL)
	{
		memcpy(&outFirmware->wifiAP1, wifiAP1, sizeof(FWAccessPointSettings));
	}
	
	if (wifiAP2 != NULL)
	{
		memcpy(&outFirmware->wifiAP2, wifiAP2, sizeof(FWAccessPointSettings));
	}
	
	if (wifiAP3 != NULL)
	{
		memcpy(&outFirmware->wifiAP3, wifiAP3, sizeof(FWAccessPointSettings));
	}
}

bool NDS_ApplyFirmwareSettingsWithFile(NDSFirmwareData *outFirmware, const char *inFileName)
{
	bool didReadExtFirmwareData = false;
	u8 *extFirmwareData = NULL;
	size_t fileSize = 0;
	
	if ( (outFirmware == NULL) || (inFileName == NULL) || (strlen(inFileName) == 0) )
	{
		return didReadExtFirmwareData;
	}
	
	FILE *fp = fopen(inFileName, "rb");
	if (fp == NULL)
	{
		printf("Ext. Firmware: Failed loading config from %s\n               Could not open file.\n", inFileName);
		return didReadExtFirmwareData;
	}
	
	fseek(fp, 0, SEEK_END);
	fileSize = (size_t)ftell(fp);
	if (fileSize != DFC_FILE_SIZE)
	{
		printf("Ext. Firmware: Failed loading config from %s\n               Actual file size was %zu bytes, expected %zu bytes.\n", inFileName, fileSize, DFC_FILE_SIZE);
		fclose(fp);
		return didReadExtFirmwareData;
	}
	fseek(fp, 0, SEEK_SET);
	
	extFirmwareData = (u8 *)malloc(SETTINGS_SIZE);
	if (extFirmwareData == NULL)
	{
		return didReadExtFirmwareData;
	}
	
	if (fread(extFirmwareData, 1, DFC_ID_SIZE, fp) == DFC_ID_SIZE)
	{
		if (memcmp(extFirmwareData, DFC_ID_CODE, DFC_ID_SIZE) == 0)
		{
			const size_t readSettingsSize = fread(extFirmwareData, 1, SETTINGS_SIZE, fp);
			didReadExtFirmwareData = (readSettingsSize == SETTINGS_SIZE);
		}
	}
	
	fclose(fp);
	
	if (didReadExtFirmwareData)
	{
		// Need a temp struct here because WIFI_SETTINGS_SIZE is smaller than sizeof(FWWifiInfo).
		FWWifiInfo wifiInfo = outFirmware->wifiInfo;
		memcpy(&wifiInfo, extFirmwareData + sizeof(FWUserSettings), WIFI_SETTINGS_SIZE);
		
		NDS_ApplyFirmwareSettings(outFirmware,
								  NULL,
								  (FWUserSettings *)extFirmwareData,
								  (FWUserSettings *)extFirmwareData,
								  &wifiInfo,
								  (FWAccessPointSettings *)(extFirmwareData + sizeof(FWUserSettings) + WIFI_SETTINGS_SIZE),
								  NULL,
								  NULL);
		
		printf("Ext. Firmware: Successfully loaded config from %s\n", inFileName);
	}
	
	free(extFirmwareData);
	
	return didReadExtFirmwareData;
}

void NDS_ApplyFirmwareSettingsWithConfig(NDSFirmwareData *outFirmware, const FirmwareConfig &inConfig)
{
	if (outFirmware == NULL)
	{
		return;
	}
	
	FW_HEADER_KEY headerKey = outFirmware->header.key;
	FWUserSettings userSettings0 = outFirmware->userSettings0;
	FWUserSettings userSettings1 = outFirmware->userSettings1;
	FWWifiInfo wifiInfo = outFirmware->wifiInfo;
	FWAccessPointSettings wifiAP1 = outFirmware->wifiAP1;
	FWAccessPointSettings wifiAP2 = outFirmware->wifiAP2;
	FWAccessPointSettings wifiAP3 = outFirmware->wifiAP3;
	
	// NDS type
	headerKey.consoleType = inConfig.consoleType;
	
	// User settings (at 0x3FE00 and 0x3FF00)
	userSettings0.favoriteColor = inConfig.favoriteColor;
	userSettings0.birthdayMonth = inConfig.birthdayMonth;
	userSettings0.birthdayDay = inConfig.birthdayDay;
	
	// Copy the default char buffers into the UTF-16 string buffers.
	u16 nicknameLength = inConfig.nicknameLength;
	if (nicknameLength > MAX_FW_NICKNAME_LENGTH)
	{
		nicknameLength = MAX_FW_NICKNAME_LENGTH;
	}
	
	u16 messageLength = inConfig.messageLength;
	if (messageLength > MAX_FW_MESSAGE_LENGTH)
	{
		messageLength = MAX_FW_MESSAGE_LENGTH;
	}
	
	userSettings0.nicknameLength = LOCAL_TO_LE_16(nicknameLength);
	userSettings0.messageLength = LOCAL_TO_LE_16(messageLength);
	
	memset(userSettings0.nickname, 0, MAX_FW_NICKNAME_LENGTH * sizeof(u16));
	for (size_t i = 0; i < nicknameLength; i++)
	{
		userSettings0.nickname[i] = LOCAL_TO_LE_16(inConfig.nickname[i]);
	}
	
	memset(userSettings0.message, 0, MAX_FW_MESSAGE_LENGTH * sizeof(u16));
	for (size_t i = 0; i < messageLength; i++)
	{
		userSettings0.message[i] = LOCAL_TO_LE_16(inConfig.message[i]);
	}
	
	// Default touch-screen calibration settings.
	userSettings0.tscADC_x1 = LOCAL_TO_LE_16(inConfig.tscADC_x1);
	userSettings0.tscADC_y1 = LOCAL_TO_LE_16(inConfig.tscADC_y1);
	userSettings0.tscPixel_x1 = inConfig.tscPixel_x1;
	userSettings0.tscPixel_y1 = inConfig.tscPixel_y1;
	userSettings0.tscADC_x2 = LOCAL_TO_LE_16(inConfig.tscADC_x2);
	userSettings0.tscADC_y2 = LOCAL_TO_LE_16(inConfig.tscADC_y2);
	userSettings0.tscPixel_x2 = inConfig.tscPixel_x2;
	userSettings0.tscPixel_y2 = inConfig.tscPixel_y2;
	
	// Default language and flags.
	userSettings0.languageFlags.language = inConfig.language;
	userSettings0.languageFlags.backlightLevel = inConfig.backlightLevel;
	
	// Copy the default config for userSettings0 into userSettings1.
	userSettings1 = userSettings0;
	
	userSettings0.updateCounter = 0x0000;
	userSettings1.updateCounter = 0x0001;
	userSettings0.crc16 = LOCAL_TO_LE_16( (u16)calc_CRC16(0xFFFF, &userSettings0, 0x70) );
	userSettings1.crc16 = LOCAL_TO_LE_16( (u16)calc_CRC16(0xFFFF, &userSettings1, 0x70) );
	
	// MAC address
	wifiInfo.MACAddr[0] = inConfig.MACAddress[0];
	wifiInfo.MACAddr[1] = inConfig.MACAddress[1];
	wifiInfo.MACAddr[2] = inConfig.MACAddress[2];
	wifiInfo.MACAddr[3] = inConfig.MACAddress[3];
	wifiInfo.MACAddr[4] = inConfig.MACAddress[4];
	wifiInfo.MACAddr[5] = inConfig.MACAddress[5];
	
	// Wifi settings CRC16
	wifiInfo.crc16 = LOCAL_TO_LE_16( (u16)calc_CRC16(0, &wifiInfo.length, wifiInfo.length) );
	
	// WFC User ID, uniquely located on the first WiFi profile and no other
	wifiAP1.wfcUserID[0] = inConfig.WFCUserID[0];
	wifiAP1.wfcUserID[1] = inConfig.WFCUserID[1];
	wifiAP1.wfcUserID[2] = inConfig.WFCUserID[2];
	wifiAP1.wfcUserID[3] = inConfig.WFCUserID[3];
	wifiAP1.wfcUserID[4] = inConfig.WFCUserID[4];
	wifiAP1.wfcUserID[5] = inConfig.WFCUserID[5];
	
	// WiFi profiles
	if ( ((*(u32 *)inConfig.ipv4Address_AP1 != 0) && (*(u32 *)inConfig.ipv4Gateway_AP1 != 0) && (inConfig.subnetMask_AP1 != 0)) ||
		  (*(u32 *)inConfig.ipv4PrimaryDNS_AP1 != 0) ||
		  (*(u32 *)inConfig.ipv4SecondaryDNS_AP1 != 0) )
	{
		wifiAP1.SSID[0] = 'S';
		wifiAP1.SSID[1] = 'o';
		wifiAP1.SSID[2] = 'f';
		wifiAP1.SSID[3] = 't';
		wifiAP1.SSID[4] = 'A';
		wifiAP1.SSID[5] = 'P';
		wifiAP1.ipv4Address[0] = inConfig.ipv4Address_AP1[0];
		wifiAP1.ipv4Address[1] = inConfig.ipv4Address_AP1[1];
		wifiAP1.ipv4Address[2] = inConfig.ipv4Address_AP1[2];
		wifiAP1.ipv4Address[3] = inConfig.ipv4Address_AP1[3];
		wifiAP1.ipv4Gateway[0] = inConfig.ipv4Gateway_AP1[0];
		wifiAP1.ipv4Gateway[1] = inConfig.ipv4Gateway_AP1[1];
		wifiAP1.ipv4Gateway[2] = inConfig.ipv4Gateway_AP1[2];
		wifiAP1.ipv4Gateway[3] = inConfig.ipv4Gateway_AP1[3];
		wifiAP1.ipv4PrimaryDNS[0] = inConfig.ipv4PrimaryDNS_AP1[0];
		wifiAP1.ipv4PrimaryDNS[1] = inConfig.ipv4PrimaryDNS_AP1[1];
		wifiAP1.ipv4PrimaryDNS[2] = inConfig.ipv4PrimaryDNS_AP1[2];
		wifiAP1.ipv4PrimaryDNS[3] = inConfig.ipv4PrimaryDNS_AP1[3];
		wifiAP1.ipv4SecondaryDNS[0] = inConfig.ipv4SecondaryDNS_AP1[0];
		wifiAP1.ipv4SecondaryDNS[1] = inConfig.ipv4SecondaryDNS_AP1[1];
		wifiAP1.ipv4SecondaryDNS[2] = inConfig.ipv4SecondaryDNS_AP1[2];
		wifiAP1.ipv4SecondaryDNS[3] = inConfig.ipv4SecondaryDNS_AP1[3];
		wifiAP1.subnetMask = inConfig.subnetMask_AP1;
		wifiAP1.configureMode = 0;
	}
	else
	{
		wifiAP1.configureMode = 0xFF;
	}
	
	if ( ((*(u32 *)inConfig.ipv4Address_AP2 != 0) && (*(u32 *)inConfig.ipv4Gateway_AP2 != 0) && (inConfig.subnetMask_AP2 != 0)) ||
		  (*(u32 *)inConfig.ipv4PrimaryDNS_AP2 != 0) ||
		  (*(u32 *)inConfig.ipv4SecondaryDNS_AP2 != 0) )
	{
		wifiAP2.SSID[0] = 'S';
		wifiAP2.SSID[1] = 'o';
		wifiAP2.SSID[2] = 'f';
		wifiAP2.SSID[3] = 't';
		wifiAP2.SSID[4] = 'A';
		wifiAP2.SSID[5] = 'P';
		wifiAP2.ipv4Address[0] = inConfig.ipv4Address_AP2[0];
		wifiAP2.ipv4Address[1] = inConfig.ipv4Address_AP2[1];
		wifiAP2.ipv4Address[2] = inConfig.ipv4Address_AP2[2];
		wifiAP2.ipv4Address[3] = inConfig.ipv4Address_AP2[3];
		wifiAP2.ipv4Gateway[0] = inConfig.ipv4Gateway_AP2[0];
		wifiAP2.ipv4Gateway[1] = inConfig.ipv4Gateway_AP2[1];
		wifiAP2.ipv4Gateway[2] = inConfig.ipv4Gateway_AP2[2];
		wifiAP2.ipv4Gateway[3] = inConfig.ipv4Gateway_AP2[3];
		wifiAP2.ipv4PrimaryDNS[0] = inConfig.ipv4PrimaryDNS_AP2[0];
		wifiAP2.ipv4PrimaryDNS[1] = inConfig.ipv4PrimaryDNS_AP2[1];
		wifiAP2.ipv4PrimaryDNS[2] = inConfig.ipv4PrimaryDNS_AP2[2];
		wifiAP2.ipv4PrimaryDNS[3] = inConfig.ipv4PrimaryDNS_AP2[3];
		wifiAP2.ipv4SecondaryDNS[0] = inConfig.ipv4SecondaryDNS_AP2[0];
		wifiAP2.ipv4SecondaryDNS[1] = inConfig.ipv4SecondaryDNS_AP2[1];
		wifiAP2.ipv4SecondaryDNS[2] = inConfig.ipv4SecondaryDNS_AP2[2];
		wifiAP2.ipv4SecondaryDNS[3] = inConfig.ipv4SecondaryDNS_AP2[3];
		wifiAP2.subnetMask = inConfig.subnetMask_AP2;
		wifiAP2.configureMode = 0;
	}
	else
	{
		wifiAP2.configureMode = 0xFF;
	}
	
	if ( ((*(u32 *)inConfig.ipv4Address_AP3 != 0) && (*(u32 *)inConfig.ipv4Gateway_AP3 != 0) && (inConfig.subnetMask_AP3 != 0)) ||
		  (*(u32 *)inConfig.ipv4PrimaryDNS_AP3 != 0) ||
		  (*(u32 *)inConfig.ipv4SecondaryDNS_AP3 != 0) )
	{
		wifiAP3.SSID[0] = 'S';
		wifiAP3.SSID[1] = 'o';
		wifiAP3.SSID[2] = 'f';
		wifiAP3.SSID[3] = 't';
		wifiAP3.SSID[4] = 'A';
		wifiAP3.SSID[5] = 'P';
		wifiAP3.ipv4Address[0] = inConfig.ipv4Address_AP3[0];
		wifiAP3.ipv4Address[1] = inConfig.ipv4Address_AP3[1];
		wifiAP3.ipv4Address[2] = inConfig.ipv4Address_AP3[2];
		wifiAP3.ipv4Address[3] = inConfig.ipv4Address_AP3[3];
		wifiAP3.ipv4Gateway[0] = inConfig.ipv4Gateway_AP3[0];
		wifiAP3.ipv4Gateway[1] = inConfig.ipv4Gateway_AP3[1];
		wifiAP3.ipv4Gateway[2] = inConfig.ipv4Gateway_AP3[2];
		wifiAP3.ipv4Gateway[3] = inConfig.ipv4Gateway_AP3[3];
		wifiAP3.ipv4PrimaryDNS[0] = inConfig.ipv4PrimaryDNS_AP3[0];
		wifiAP3.ipv4PrimaryDNS[1] = inConfig.ipv4PrimaryDNS_AP3[1];
		wifiAP3.ipv4PrimaryDNS[2] = inConfig.ipv4PrimaryDNS_AP3[2];
		wifiAP3.ipv4PrimaryDNS[3] = inConfig.ipv4PrimaryDNS_AP3[3];
		wifiAP3.ipv4SecondaryDNS[0] = inConfig.ipv4SecondaryDNS_AP3[0];
		wifiAP3.ipv4SecondaryDNS[1] = inConfig.ipv4SecondaryDNS_AP3[1];
		wifiAP3.ipv4SecondaryDNS[2] = inConfig.ipv4SecondaryDNS_AP3[2];
		wifiAP3.ipv4SecondaryDNS[3] = inConfig.ipv4SecondaryDNS_AP3[3];
		wifiAP3.subnetMask = inConfig.subnetMask_AP3;
		wifiAP3.configureMode = 0;
	}
	else
	{
		wifiAP3.configureMode = 0xFF;
	}
	
	wifiAP1.crc16 = LOCAL_TO_LE_16( (u16)calc_CRC16(0, &wifiAP1, 254) );
	wifiAP2.crc16 = LOCAL_TO_LE_16( (u16)calc_CRC16(0, &wifiAP2, 254) );
	wifiAP3.crc16 = LOCAL_TO_LE_16( (u16)calc_CRC16(0, &wifiAP3, 254) );
	
	NDS_ApplyFirmwareSettings(outFirmware,
							  &headerKey,
							  &userSettings0,
							  &userSettings1,
							  &wifiInfo,
							  &wifiAP1,
							  &wifiAP2,
							  &wifiAP3);
}

void NDS_InitDefaultFirmware(NDSFirmwareData *outFirmware)
{
	if (outFirmware == NULL)
	{
		return;
	}
	
	// First, clear the firmware data with all zeroes
	memset(outFirmware, 0, sizeof(NDSFirmwareData));
	
	FirmwareConfig defaultConfig;
	NDS_GetDefaultFirmwareConfig(defaultConfig);
	
	// Firmware identifier
	outFirmware->header.identifier[0] = 'M';
	outFirmware->header.identifier[1] = 'A';
	outFirmware->header.identifier[2] = 'C';
	outFirmware->header.identifier[3] = 'P';
	
	// User Settings offset 0x3fe00 / 8
	outFirmware->header.userSettingsOffset = LOCAL_TO_LE_16( (u16)(offsetof(NDSFirmwareData, userSettings0) / 8) );
	
	// User settings (at 0x3FE00 and 0x3FF00)
	outFirmware->userSettings0.version = 5;
	
	// Default language and flags.
	outFirmware->userSettings0.languageFlags.value = 0xFC00;
	outFirmware->userSettings0.languageFlags.gbaModeScreenSelection = 0;
	outFirmware->userSettings0.languageFlags.bootmenuDisable = 0;
	
	// Since we don't support DSi at the moment, we just fill this area with the default
	// NDS data of 0xFF.
	memset(outFirmware->userSettings0.nds.unused5, 0xFF, sizeof(outFirmware->userSettings0.nds.unused5));
	
	// Copy the default config for userSettings0 into userSettings1.
	outFirmware->userSettings1 = outFirmware->userSettings0;
	
	// Begin setting up the WiFi info.
	outFirmware->wifiInfo.length = 0x0138;
	outFirmware->wifiInfo.version = 0;
	outFirmware->wifiInfo.channels = 0x3FFE;
	outFirmware->wifiInfo.flags = 0xFFFF;
	
	// RF related
	outFirmware->wifiInfo.rfType = 2;
	outFirmware->wifiInfo.rfBits = 0x18;
	outFirmware->wifiInfo.rfEntries = 0x0C;
	outFirmware->wifiInfo.UNKNOWN1 = 0x01;
	
	// Wifi I/O init values
	outFirmware->wifiInfo.wifiConfig146 = 0x0002;			// 0x0044
	outFirmware->wifiInfo.wifiConfig148 = 0x0017;			// 0x0046
	outFirmware->wifiInfo.wifiConfig14A = 0x0026;			// 0x0048
	outFirmware->wifiInfo.wifiConfig14C = 0x1818;			// 0x004A
	outFirmware->wifiInfo.wifiConfig120 = 0x0048;			// 0x004C
	outFirmware->wifiInfo.wifiConfig122 = 0x4840;			// 0x004E
	outFirmware->wifiInfo.wifiConfig154 = 0x0058;			// 0x0050
	outFirmware->wifiInfo.wifiConfig144 = 0x0042;			// 0x0052
	outFirmware->wifiInfo.wifiConfig130 = 0x0140;			// 0x0054
	outFirmware->wifiInfo.wifiConfig132 = 0x8064;			// 0x0056
	outFirmware->wifiInfo.wifiConfig140 = 0xE0E0;			// 0x0058
	outFirmware->wifiInfo.wifiConfig142 = 0x2443;			// 0x005A
	outFirmware->wifiInfo.wifiConfigPowerTX = 0x000E;		// 0x005C
	outFirmware->wifiInfo.wifiConfig124 = 0x0032;			// 0x005E
	outFirmware->wifiInfo.wifiConfig128 = 0x01F4;			// 0x0060
	outFirmware->wifiInfo.wifiConfig150 = 0x0101;			// 0x0062
	
	// Wifi BB init values
	memcpy(outFirmware->wifiInfo.bbData, FW_BBInit, sizeof(FW_BBInit));
	
	// Wifi RF init values
	memcpy(outFirmware->wifiInfo.Type2.rfValue, FW_RFInit, sizeof(FW_RFInit));
	
	// Wifi channel-related init values
	memcpy(outFirmware->wifiInfo.Type2.rfChannelValue24, FW_RFChannel, sizeof(FW_RFChannel));
	memcpy(outFirmware->wifiInfo.Type2.bbChannelValue8, FW_BBChannel, sizeof(FW_BBChannel));
	memset(outFirmware->wifiInfo.Type2.rfChannelValue8, 0x10, 14);
	
	outFirmware->wifiInfo.UNKNOWN2 = 0x19;
	outFirmware->wifiInfo.unused4 = 0xFF;
	memset(outFirmware->wifiInfo.unused5, 0xFF, sizeof(outFirmware->wifiInfo.unused5));
	
	NDS_ApplyFirmwareSettingsWithConfig(outFirmware, defaultConfig);
}

bool NDS_ReadFirmwareDataFromFile(const char *fileName, NDSFirmwareData *outFirmware, size_t *outFileSize, int *outConsoleType, u8 *outMACAddr)
{
	bool result = false;
	
	if ( (fileName == NULL) || (strlen(fileName) == 0) )
	{
		return result;
	}
	
	// Open the file.
	FILE *fp = fopen(fileName, "rb");
	if (fp == NULL)
	{
		return result;
	}
	
	// Do some basic validation of the firmware file.
	fseek(fp, 0, SEEK_END);
	const size_t fileSize = ftell(fp);
	if (outFileSize != NULL)
	{
		*outFileSize = fileSize;
	}
	
	if ( (fileSize != NDS_FW_SIZE_V1) && (fileSize != NDS_FW_SIZE_V2) )
	{
		fclose(fp);
		return result;
	}
	
	size_t readBytes = 0;
	u32 identifierValue = 0;
	
	fseek(fp, offsetof(FWHeader, identifierValue), SEEK_SET);
	readBytes = fread(&identifierValue, 1, sizeof(u32), fp);
	
	if ( (readBytes != sizeof(u32)) || ((identifierValue & 0x00FFFFFF) != 0x0043414D) )
	{
		fclose(fp);
		return result;
	}
	
	// Now that the firmware file has been validated, let's start reading the individual
	// pieces of data that the caller has requested.
	result = true;
	
	if (outFirmware != NULL)
	{
		fseek(fp, 0, SEEK_SET);
		readBytes = fread(outFirmware, 1, sizeof(NDSFirmwareData), fp);
		
		if ( readBytes == sizeof(NDSFirmwareData) )
		{
			if (outConsoleType != NULL)
			{
				*outConsoleType = (int)outFirmware->header.key.consoleType;
			}
			
			if (outMACAddr != NULL)
			{
				outMACAddr[0] = outFirmware->wifiInfo.MACAddr[0];
				outMACAddr[1] = outFirmware->wifiInfo.MACAddr[1];
				outMACAddr[2] = outFirmware->wifiInfo.MACAddr[2];
				outMACAddr[3] = outFirmware->wifiInfo.MACAddr[3];
				outMACAddr[4] = outFirmware->wifiInfo.MACAddr[4];
				outMACAddr[5] = outFirmware->wifiInfo.MACAddr[5];
			}
		}
		else
		{
			printf("Ext. Firmware: Failed to read the firmware data. (%zu out of %zu bytes read.)\n", readBytes, sizeof(NDSFirmwareData));
			result = false;
		}
	}
	else
	{
		if (outConsoleType != NULL)
		{
			FW_HEADER_KEY fpKey;
			fpKey.consoleType = NDS_CONSOLE_TYPE_FAT;
			
			fseek(fp, offsetof(FWHeader, key), SEEK_SET);
			readBytes = fread(&fpKey, 1, sizeof(FW_HEADER_KEY), fp);
			
			if (readBytes == sizeof(FW_HEADER_KEY))
			{
				*outConsoleType = (int)fpKey.consoleType;
			}
			else
			{
				printf("Ext. Firmware: Failed to read the console type. (%zu out of %zu bytes read.)\n", readBytes, sizeof(FW_HEADER_KEY));
				result = false;
			}
		}
		
		if (outMACAddr != NULL)
		{
			fseek(fp, sizeof(FWHeader) + offsetof(FWWifiInfo, MACAddr), SEEK_SET);
			readBytes = fread(outMACAddr, 1, 6, fp);
			
			if (readBytes != 6)
			{
				printf("Ext. Firmware: Failed to read the MAC address. (%zu out of %zu bytes read.)\n", readBytes, sizeof(u8) * 6);
				result = false;
			}
		}
	}
	
	// Close the file.
	fclose(fp);
	
	return result;
}

//=========================
//- firmware SPI chip interface -
//the original intention was for this code to have similarity to the AUXSPI backup memory devices.
//however, that got overgrown, and this stuff stayed pretty simple.
//perhaps it can be re-defined in terms of a simpler AUXSPI device after the AUXSPI devices are re-designed.

#define FW_CMD_READ             0x03
#define FW_CMD_WRITEDISABLE     0x04
#define FW_CMD_READSTATUS       0x05
#define FW_CMD_WRITEENABLE      0x06
#define FW_CMD_PAGEWRITE        0x0A
#define FW_CMD_READ_ID			0x9F

void fw_reset_com(fw_memory_chip *mc)
{
	if(mc->com == FW_CMD_PAGEWRITE)
	{
		if (mc->isFirmware && CommonSettings.UseExtFirmware && CommonSettings.UseExtFirmwareSettings && (extFirmwareObj != NULL))
		{
			extFirmwareObj->saveSettings(CommonSettings.ExtFirmwareUserSettingsPath);
		}
		mc->write_enable = FALSE;
	}

	mc->com = 0;
}

u8 fw_transfer(fw_memory_chip *mc, u8 data)
{
	if(mc->com == FW_CMD_READ || mc->com == FW_CMD_PAGEWRITE) /* check if we are in a command that needs 3 bytes address */
	{
		if(mc->addr_shift > 0)   /* if we got a complete address */
		{
			mc->addr_shift--;
			mc->addr |= data << (mc->addr_shift * 8); /* argument is a byte of address */
		}
		else    /* if we have received 3 bytes of address, proceed command */
		{
			switch(mc->com)
			{
				case FW_CMD_READ:
					if(mc->addr < mc->size)  /* check if we can read */
					{
						data = mc->data._raw[mc->addr];       /* return byte */
						mc->addr++;      /* then increment address */
					}
					break;
					
				case FW_CMD_PAGEWRITE:
					if(mc->addr < mc->size)
					{
						if ( (mc->addr >= offsetof(NDSFirmwareData, wifiAP1.wfcUserID[0])) && (mc->addr <= offsetof(NDSFirmwareData, wifiAP1.wfcUserID[5])) )
						{
							CommonSettings.fwConfig.WFCUserID[mc->addr - offsetof(NDSFirmwareData, wifiAP1.wfcUserID[0])] = data;
						}
						
						mc->data._raw[mc->addr] = data;       /* write byte */
						mc->addr++;
					}
					break;
			}
			
		}
	}
	else if(mc->com == FW_CMD_READ_ID)
	{
		switch(mc->addr)
		{
		//here is an ID string measured from an old ds fat: 62 16 00 (0x62=sanyo)
		//but we chose to use an ST from martin's ds fat string so programs might have a clue as to the firmware size:
		//20 40 12
		case 0: 
			data = 0x20;
			mc->addr=1; 
			break;
		case 1: 
			data = 0x40; //according to gbatek this is the device ID for the flash on someone's ds fat
			mc->addr=2; 
			break;
		case 2: 
			data = 0x12;
			mc->addr = 0; 
			break;
		}
	}
	else if(mc->com == FW_CMD_READSTATUS)
	{
		return (mc->write_enable ? 0x02 : 0x00);
	}
	else	//finally, check if it's a new command
	{
		switch(data)
		{
			case 0: break;	//nothing

			case FW_CMD_READ_ID:
				mc->addr = 0;
				mc->com = FW_CMD_READ_ID;
				break;
			
			case FW_CMD_READ:    //read command
				mc->addr = 0;
				mc->addr_shift = 3;
				mc->com = FW_CMD_READ;
				break;
				
			case FW_CMD_WRITEENABLE:     //enable writing
				if(mc->writeable_buffer) { mc->write_enable = TRUE; }
				break;
				
			case FW_CMD_WRITEDISABLE:    //disable writing
				mc->write_enable = FALSE;
				break;
				
			case FW_CMD_PAGEWRITE:       //write command
				if(mc->write_enable)
				{
					mc->addr = 0;
					mc->addr_shift = 3;
					mc->com = FW_CMD_PAGEWRITE;
				}
				else { data = 0; }
				break;
			
			case FW_CMD_READSTATUS:  //status register command
				mc->com = FW_CMD_READSTATUS;
				break;
				
			default:
				printf("Unhandled FW command: %02X\n", data);
				break;
		}
	}
	
	return data;
}	


void mc_init(fw_memory_chip *mc, int type)
{
	mc->com = 0;
	mc->addr = 0;
	mc->addr_shift = 0;
	mc->size = 0;
	mc->write_enable = FALSE;
	mc->writeable_buffer = FALSE;
	mc->type = type;

	switch (mc->type)
	{
		case MC_TYPE_EEPROM1:
			mc->addr_size = 1;
			break;
			
		case MC_TYPE_EEPROM2:
		case MC_TYPE_FRAM:
			mc->addr_size = 2;
			break;
			
		case MC_TYPE_FLASH:
			mc->addr_size = 3;
			break;
			
		default:
			break;
	}
}

u8 *mc_alloc(fw_memory_chip *mc, u32 size)
{
	memset(mc->data._raw, 0, sizeof(NDSFirmwareData));
	mc->size = size;
	mc->writeable_buffer = TRUE;

	return mc->data._raw;
}

void mc_free(fw_memory_chip *mc)
{
    mc_init(mc, MC_TYPE_AUTODETECT);
}
