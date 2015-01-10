/*	
	Copyright (C) 2009-2015 DeSmuME Team

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

#define DFC_ID_CODE	"DeSmuME Firmware User Settings"
#define DFC_ID_SIZE	sizeof(DFC_ID_CODE)
#define USER_SETTINGS_SIZE 0x100
#define WIFI_SETTINGS_SIZE 0x1D5
#define WIFI_AP_SETTINGS_SIZE 0x300
#define SETTINGS_SIZE (USER_SETTINGS_SIZE + WIFI_SETTINGS_SIZE + WIFI_AP_SETTINGS_SIZE)
#define DFC_FILE_SIZE (SETTINGS_SIZE + DFC_ID_SIZE)
#define WIFI_SETTINGS_OFF 0x0000002A
#define WIFI_AP_SETTINGS_OFF 0x0003FA00

static _KEY1	enc(&MMU.ARM7_BIOS[0x0030]);

u16 CFIRMWARE::getBootCodeCRC16()
{
	unsigned int i, j;
	u32 crc = 0xFFFF;
	const u16 val[8] = {0xC0C1, 0xC181, 0xC301, 0xC601, 0xCC01, 0xD801, 0xF001, 0xA001};

	for(i = 0; i < size9; i++)
	{
		crc = (crc ^ tmp_data9[i]);

		for(j = 0; j < 8; j++) 
		{
			if(crc & 0x0001)
				crc = ((crc >> 1) ^ (val[j] << (7-j)));
			else
				crc =  (crc >> 1);
		}
	}

	for(i = 0; i < size7; i++)
	{
		crc = (crc ^ tmp_data7[i]);

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

u32 CFIRMWARE::decrypt(const u8 *in, u8* &out)
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

u32 CFIRMWARE::decompress(const u8 *in, u8* &out)
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
bool CFIRMWARE::load()
{
	u32 size = 0;
	u8	*data = NULL;
	
	if (CommonSettings.UseExtFirmware == false)
		return false;
	if (strlen(CommonSettings.Firmware) == 0)
		return false;
	
	FILE	*fp = fopen(CommonSettings.Firmware, "rb");
	if (!fp)
		return false;
	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	if( (size != NDS_FW_SIZE_V1) && (size != NDS_FW_SIZE_V2) )
	{
		fclose(fp);
		return false;
	}

	data = new u8 [size];
	if (!data)
	{
		fclose(fp);
		return false;
	}

	if (fread(data, 1, size, fp) != size)
	{
		delete [] data;
		data = NULL;
		fclose(fp);
		return false; 
	}

	memcpy(&header, data, sizeof(header));
	if ((header.fw_identifier & 0x00FFFFFF) != 0x0043414D)
	{
		delete [] data;
		data = NULL;
		fclose(fp);
		return false;
	}
	fclose(fp);

	if (MMU.fw.size != size)	// reallocate
		mc_alloc(&MMU.fw, size);

	userDataAddr = T1ReadWord(data, 0x20) * 8;

	// fix bad dump of firmware? (wrong DS type)
	// fix mario kart touch screen calibration
	if ((T1ReadWord(data, 0x1E) != 0xFFFF) && data[0x1D] == 0x63)
	{
		data[0x1D] = NDS_CONSOLE_TYPE_FAT;
		data[0x1E] = 0xFF;
		data[0x1F] = 0xFF;
	}

	memcpy(MMU.fw.data, data, size);

	delete [] data;
	data = NULL;

	// Generate the path for the external firmware config file.
	std::string extFilePath = CFIRMWARE::GetExternalFilePath();
	strncpy(MMU.fw.userfile, extFilePath.c_str(), MAX_PATH);

	successLoad = true;
	return true;
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

	u8	*data = new u8 [size];
	if (!data)
		return false;

	memcpy(data, MMU.fw.data, size);

	shift1 = ((header.shift_amounts >> 0) & 0x07);
	shift2 = ((header.shift_amounts >> 3) & 0x07);
	shift3 = ((header.shift_amounts >> 6) & 0x07);
	shift4 = ((header.shift_amounts >> 9) & 0x07);

	// todo - add support for 512Kb 
	part1addr = (header.part1_rom_boot9_addr << (2 + shift1));
	part1ram = (0x02800000 - (header.part1_ram_boot9_addr << (2+shift2)));
	part2addr = (header.part2_rom_boot7_addr << (2+shift3));
	part2ram = (0x03810000 - (header.part2_ram_boot7_addr << (2+shift4)));
	part3addr = (header.part3_rom_gui9_addr << 3);
	part4addr = (header.part4_rom_wifi7_addr << 3);
	part5addr = (header.part5_data_gfx_addr << 3);

	ARM9bootAddr = part1ram;
	ARM7bootAddr = part2ram;

	enc.init(header.fw_identifier, 1, 0xC);

#if 0
	enc.applyKeycode((u32*)&data[0x18]);
#else
	// fix touch coords
	data[0x18] = 0x00;
	data[0x19] = 0x00;
	data[0x1A] = 0x00;
	data[0x1B] = 0x00;

	data[0x1C] = 0x00;
	data[0x1D] = 0xFF;
	data[0x1E] = 0x00;
	data[0x1F] = 0x00;
#endif

	enc.init(header.fw_identifier, 2, 0xC);

	size9 = decrypt(data + part1addr, tmp_data9);
	if (!tmp_data9)
	{
		delete [] data; data = NULL;
		return false;
	}

	size7 = decrypt(data + part2addr, tmp_data7);
	if (!tmp_data7)
	{
		delete [] tmp_data9;
		delete [] data; data = NULL;
		return false;
	}

	u16 crc16_mine = getBootCodeCRC16();

	if (crc16_mine != header.part12_boot_crc16)
	{
		INFO("Firmware: ERROR: the boot code CRC16 (0x%04X) doesn't match the value in the firmware header (0x%04X)", crc16_mine, header.part12_boot_crc16);
		delete [] tmp_data9;
		delete [] tmp_data7;
		delete [] data; data = NULL;
		return false;
	}

	// Copy firmware boot codes to their respective locations
	src = 0;
	for(u32 i = 0; i < (size9 >> 2); i++)
	{
		_MMU_write32<ARMCPU_ARM9>(part1ram, T1ReadLong(tmp_data9, src));
		src += 4; part1ram += 4;
	}

	src = 0;
	for(u32 i = 0; i < (size7 >> 2); i++)
	{
		_MMU_write32<ARMCPU_ARM7>(part2ram, T1ReadLong(tmp_data7, src));
		src += 4; part2ram += 4;
	}
	delete [] tmp_data7;
	delete [] tmp_data9;

	patched = false;
	if (data[0x17C] != 0xFF)
		patched = true;

	INFO("Firmware:\n");
	INFO("- path: %s\n", CommonSettings.Firmware);
	INFO("- size: %i bytes (%i Mbit)\n", size, size/1024/8);
	INFO("- CRC : 0x%04X\n", header.part12_boot_crc16);
	INFO("- header: \n");
	INFO("   * size firmware %i\n", ((header.shift_amounts >> 12) & 0xF) * 128 * 1024);
	INFO("   * ARM9 boot code address:     0x%08X\n", part1addr);
	INFO("   * ARM9 boot code RAM address: 0x%08X\n", ARM9bootAddr);
	INFO("   * ARM9 unpacked size:         0x%08X (%i) bytes\n", size9, size9);
	INFO("   * ARM9 GUI code address:      0x%08X\n", part3addr);
	INFO("\n");
	INFO("   * ARM7 boot code address:     0x%08X\n", part2addr);
	INFO("   * ARM7 boot code RAM address: 0x%08X\n", ARM7bootAddr);
	INFO("   * ARM7 WiFi code address:     0x%08X\n", part4addr);
	INFO("   * ARM7 unpacked size:         0x%08X (%i) bytes\n", size7, size7);
	INFO("\n");
	INFO("   * Data/GFX address:           0x%08X\n", part5addr);

	if (patched)
	{
		u32 patch_offset = 0x3FC80;
		if (data[0x17C] > 1)
			patch_offset = 0x3F680;

		memcpy(&header, data + patch_offset, sizeof(header));

		shift1 = ((header.shift_amounts >> 0) & 0x07);
		shift2 = ((header.shift_amounts >> 3) & 0x07);
		shift3 = ((header.shift_amounts >> 6) & 0x07);
		shift4 = ((header.shift_amounts >> 9) & 0x07);

		// todo - add support for 512Kb 
		part1addr = (header.part1_rom_boot9_addr << (2 + shift1));
		part1ram = (0x02800000 - (header.part1_ram_boot9_addr << (2+shift2)));
		part2addr = (header.part2_rom_boot7_addr << (2+shift3));
		part2ram = (0x03810000 - (header.part2_ram_boot7_addr << (2+shift4)));

		ARM9bootAddr = part1ram;
		ARM7bootAddr = part2ram;

		size9 = decompress(data + part1addr, tmp_data9);
		if (!tmp_data9) 
		{
			delete [] data;
			return false;
		}

		size7 = decompress(data + part2addr, tmp_data7);
		if (!tmp_data7)
		{
			delete [] tmp_data9;
			delete [] data;
			return false;
		};
		// Copy firmware boot codes to their respective locations
		src = 0;
		for(u32 i = 0; i < (size9 >> 2); i++)
		{
			_MMU_write32<ARMCPU_ARM9>(part1ram, T1ReadLong(tmp_data9, src));
			src += 4; part1ram += 4;
		}

		src = 0;
		for(u32 i = 0; i < (size7 >> 2); i++)
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
		INFO("   * ARM9 unpacked size:         0x%08X (%i) bytes\n", size9, size9);
		INFO("\n");
		INFO("   * ARM7 boot code address:     0x%08X\n", part2addr);
		INFO("   * ARM7 boot code RAM address: 0x%08X\n", ARM7bootAddr);
		INFO("   * ARM7 unpacked size:         0x%08X (%i) bytes\n", size7, size7);
	}

	memcpy(MMU.fw.data, data, size);
	MMU.fw.fp = NULL;

	delete [] data; data = NULL;
	return true;
}

bool CFIRMWARE::loadSettings()
{
	if (!CommonSettings.UseExtFirmware) return false;
	if (!CommonSettings.UseExtFirmwareSettings) return false;

	FILE *fp = fopen(MMU.fw.userfile, "rb");
	if (fp)
	{
		fseek(fp, 0, SEEK_END);
		if (ftell(fp) == DFC_FILE_SIZE)
		{
			fseek(fp, 0, SEEK_SET);
			u8 *usr = new u8[SETTINGS_SIZE];
			if (usr)
			{
				if (fread(usr, 1, DFC_ID_SIZE, fp) == DFC_ID_SIZE)
				{
					if (memcmp(usr, DFC_ID_CODE, DFC_ID_SIZE) == 0)
					{
						if (fread(usr, 1, SETTINGS_SIZE, fp) == SETTINGS_SIZE)
						{
							memcpy(&MMU.fw.data[userDataAddr], usr, USER_SETTINGS_SIZE);
							memcpy(&MMU.fw.data[userDataAddr + 0x100], usr, USER_SETTINGS_SIZE);
							memcpy(&MMU.fw.data[WIFI_SETTINGS_OFF], usr + USER_SETTINGS_SIZE, WIFI_SETTINGS_SIZE);
							memcpy(&MMU.fw.data[WIFI_AP_SETTINGS_OFF], usr + USER_SETTINGS_SIZE + WIFI_SETTINGS_SIZE, WIFI_AP_SETTINGS_SIZE);
							printf("Loaded user settings from %s\n", MMU.fw.userfile);
						}
					}
				}
				delete [] usr;
				usr = NULL;
			}
		}
		else
			printf("Failed loading firmware config from %s (wrong file size)\n", MMU.fw.userfile);

		fclose(fp);
	}

	return false;
}

bool CFIRMWARE::saveSettings()
{
	if (!CommonSettings.UseExtFirmware) return false;
	if (!CommonSettings.UseExtFirmwareSettings) return false;

	u8 *data = &MMU.fw.data[userDataAddr];
	u8 counter0 = data[0x070];
	u8 counter1 = data[0x170];

	if (counter1 == ((counter0 + 1) & 0x7F))
	{
		// copy User Settings 1 to User Settings 0 area
		memcpy(data, data + 0x100, 0x100);
	}
	else
	{
		// copy User Settings 0 to User Settings 1 area
		memcpy(data + 0x100, data, 0x100);
	}
	
	printf("Firmware: saving config");
	FILE *fp = fopen(MMU.fw.userfile, "wb");
	if (fp)
	{
		u8 *usr = new u8[DFC_FILE_SIZE];
		if (usr)
		{
			memcpy(usr, DFC_ID_CODE, DFC_ID_SIZE);
			memcpy(usr + DFC_ID_SIZE, data, USER_SETTINGS_SIZE);
			memcpy(usr + DFC_ID_SIZE + USER_SETTINGS_SIZE, &MMU.fw.data[WIFI_SETTINGS_OFF], WIFI_SETTINGS_SIZE);
			memcpy(usr + DFC_ID_SIZE + USER_SETTINGS_SIZE + WIFI_SETTINGS_SIZE, &MMU.fw.data[WIFI_AP_SETTINGS_OFF], WIFI_AP_SETTINGS_SIZE);
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

std::string CFIRMWARE::GetExternalFilePath()
{
	std::string fwPath = CommonSettings.Firmware;
	std::string fwFileName = Path::GetFileNameFromPathWithoutExt(fwPath);
	char configPath[MAX_PATH] = {0};
	path.getpath(path.BATTERY, configPath);
	if (configPath[strlen(configPath)-1] == DIRECTORY_DELIMITER_CHAR)
			configPath[strlen(configPath)-1] = 0;
	std::string finalPath = std::string(configPath) + DIRECTORY_DELIMITER_CHAR + fwFileName + FILE_EXT_DELIMITER_CHAR + FW_CONFIG_FILE_EXT;

	return finalPath;
}

void *CFIRMWARE::getTouchCalibrate()
{
	static TSCalInfo cal = {0};

	if (!successLoad || !CommonSettings.UseExtFirmware || !successLoad)
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
		cal.adc.x1 = T1ReadWord(MMU.fw.data, userDataAddr + 0x58) & 0x1FFF;
		cal.adc.y1 = T1ReadWord(MMU.fw.data, userDataAddr + 0x5A) & 0x1FFF;
		cal.scr.x1 = T1ReadByte(MMU.fw.data, userDataAddr + 0x5C);
		cal.scr.y1 = T1ReadByte(MMU.fw.data, userDataAddr + 0x5D);
		cal.adc.x2 = T1ReadWord(MMU.fw.data, userDataAddr + 0x5E) & 0x1FFF;
		cal.adc.y2 = T1ReadWord(MMU.fw.data, userDataAddr + 0x60) & 0x1FFF;
		cal.scr.x2 = T1ReadByte(MMU.fw.data, userDataAddr + 0x62);
		cal.scr.y2 = T1ReadByte(MMU.fw.data, userDataAddr + 0x63);
	}

	cal.adc.width	= (cal.adc.x2 - cal.adc.x1);
	cal.adc.height	= (cal.adc.y2 - cal.adc.y1);
	cal.scr.width	= (cal.scr.x2 - cal.scr.x1);
	cal.scr.height	= (cal.scr.y2 - cal.scr.y1);

	return (void*)&cal;
}

//=====================================================================================================
static u32
calc_CRC16( u32 start, const u8 *data, int count) {
	int i,j;
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

static void fill_user_data_area( struct NDS_fw_config_data *user_settings,u8 *data, int count)
{
	u32 crc;
	int i;
	u8 *ts_cal_data_area;

	memset( data, 0, 0x100);

	// version
	data[0x00] = 5;
	data[0x01] = 0;

	// colour
	data[0x02] = user_settings->fav_colour;

	// birthday month and day
	data[0x03] = user_settings->birth_month;
	data[0x04] = user_settings->birth_day;

	//nickname and length
	for ( i = 0; i < MAX_FW_NICKNAME_LENGTH; i++) {
		data[0x06 + (i * 2)] = user_settings->nickname[i] & 0xff;
		data[0x06 + (i * 2) + 1] = (user_settings->nickname[i] >> 8) & 0xff;
	}

	data[0x1a] = user_settings->nickname_len;

	//Message
	for ( i = 0; i < MAX_FW_MESSAGE_LENGTH; i++) {
		data[0x1c + (i * 2)] = user_settings->message[i] & 0xff;
		data[0x1c + (i * 2) + 1] = (user_settings->message[i] >> 8) & 0xff;
	}

	data[0x50] = user_settings->message_len;

	//touch screen calibration
	ts_cal_data_area = &data[0x58];
	for ( i = 0; i < 2; i++) {
		// ADC x y
		*ts_cal_data_area++ = user_settings->touch_cal[i].adc_x & 0xff;
		*ts_cal_data_area++ = (user_settings->touch_cal[i].adc_x >> 8) & 0xff;
		*ts_cal_data_area++ = user_settings->touch_cal[i].adc_y & 0xff;
		*ts_cal_data_area++ = (user_settings->touch_cal[i].adc_y >> 8) & 0xff;

		//screen x y
		*ts_cal_data_area++ = user_settings->touch_cal[i].screen_x;
		*ts_cal_data_area++ = user_settings->touch_cal[i].screen_y;
	}

	//language and flags
	data[0x64] = user_settings->language;
	data[0x65] = 0xfc;

	//update count and crc
	data[0x70] = count & 0xff;
	data[0x71] = (count >> 8) & 0xff;

	crc = calc_CRC16( 0xffff, data, 0x70);
	data[0x72] = crc & 0xff;
	data[0x73] = (crc >> 8) & 0xff;

	memset( &data[0x74], 0xff, 0x100 - 0x74);
}

// creates an firmware flash image, which contains all needed info to initiate a wifi connection
int NDS_CreateDummyFirmware(NDS_fw_config_data *user_settings)
{
	//Create the firmware header

	memset( MMU.fw.data, 0, 0x40000);

	//firmware identifier
	MMU.fw.data[0x8] = 'M';
	MMU.fw.data[0x8 + 1] = 'A';
	MMU.fw.data[0x8 + 2] = 'C';
	MMU.fw.data[0x8 + 3] = 'P';

	// DS type
	if ( user_settings->ds_type == NDS_CONSOLE_TYPE_DSI)
		MMU.fw.data[0x1d] = 0xFF;
	else
		MMU.fw.data[0x1d] = user_settings->ds_type;

	//User Settings offset 0x3fe00 / 8
	MMU.fw.data[0x20] = 0xc0;
	MMU.fw.data[0x21] = 0x7f;


	//User settings (at 0x3FE00 and 0x3FF00)

	fill_user_data_area( user_settings, &MMU.fw.data[ 0x3FE00], 0);
	fill_user_data_area( user_settings, &MMU.fw.data[ 0x3FF00], 1);

	// Wifi config length
	MMU.fw.data[0x2C] = 0x38;
	MMU.fw.data[0x2D] = 0x01;

	MMU.fw.data[0x2E] = 0x00;

	//Wifi version
	MMU.fw.data[0x2F] = 0x00;

	//MAC address
	memcpy((MMU.fw.data + 0x36), FW_Mac, sizeof(FW_Mac));

	//Enabled channels
	MMU.fw.data[0x3C] = 0xFE;
	MMU.fw.data[0x3D] = 0x3F;

	MMU.fw.data[0x3E] = 0xFF;
	MMU.fw.data[0x3F] = 0xFF;

	//RF related
	MMU.fw.data[0x40] = 0x02;
	MMU.fw.data[0x41] = 0x18;
	MMU.fw.data[0x42] = 0x0C;

	MMU.fw.data[0x43] = 0x01;

	//Wifi I/O init values
	memcpy((MMU.fw.data + 0x44), FW_WIFIInit, sizeof(FW_WIFIInit));

	//Wifi BB init values
	memcpy((MMU.fw.data + 0x64), FW_BBInit, sizeof(FW_BBInit));

	//Wifi RF init values
	memcpy((MMU.fw.data + 0xCE), FW_RFInit, sizeof(FW_RFInit));

	//Wifi channel-related init values
	memcpy((MMU.fw.data + 0xF2), FW_RFChannel, sizeof(FW_RFChannel));
	memcpy((MMU.fw.data + 0x146), FW_BBChannel, sizeof(FW_BBChannel));
	memset((MMU.fw.data + 0x154), 0x10, 0xE);

	//WFC profiles
	memcpy((MMU.fw.data + 0x3FA40), &FW_WFCProfile1, sizeof(FW_WFCProfile));
	memcpy((MMU.fw.data + 0x3FB40), &FW_WFCProfile2, sizeof(FW_WFCProfile));
	memcpy((MMU.fw.data + 0x3FC40), &FW_WFCProfile3, sizeof(FW_WFCProfile));
	(*(u16*)(MMU.fw.data + 0x3FAFE)) = (u16)calc_CRC16(0, (MMU.fw.data + 0x3FA00), 0xFE);
	(*(u16*)(MMU.fw.data + 0x3FBFE)) = (u16)calc_CRC16(0, (MMU.fw.data + 0x3FB00), 0xFE);
	(*(u16*)(MMU.fw.data + 0x3FCFE)) = (u16)calc_CRC16(0, (MMU.fw.data + 0x3FC00), 0xFE);


	MMU.fw.data[0x162] = 0x19;
	memset((MMU.fw.data + 0x163), 0xFF, 0x9D);

	//Wifi settings CRC16
	(*(u16*)(MMU.fw.data + 0x2A)) = calc_CRC16(0, (MMU.fw.data + 0x2C), 0x138);

	if (&CommonSettings.fw_config != user_settings)
		memcpy(&CommonSettings.fw_config, user_settings, sizeof(NDS_fw_config_data));

	return TRUE ;
}

void NDS_FillDefaultFirmwareConfigData(NDS_fw_config_data *fw_config) {
	const char *default_nickname = "DeSmuME";
	const char *default_message = "DeSmuME makes you happy!";
	int i;
	int str_length;

	memset( fw_config, 0, sizeof(NDS_fw_config_data));
	fw_config->ds_type = NDS_CONSOLE_TYPE_FAT;

	fw_config->fav_colour = 7;

	fw_config->birth_day = 23;
	fw_config->birth_month = 6;

	str_length = strlen( default_nickname);
	for ( i = 0; i < str_length; i++) {
		fw_config->nickname[i] = default_nickname[i];
	}
	fw_config->nickname_len = str_length;

	str_length = strlen( default_message);
	for ( i = 0; i < str_length; i++) {
		fw_config->message[i] = default_message[i];
	}
	fw_config->message_len = str_length;

	//default to English
	fw_config->language = 1;

	// default touchscreen calibration

	//ANCIENT DESMUME VALUES
	fw_config->touch_cal[0].adc_x = 0x200;
	fw_config->touch_cal[0].adc_y = 0x200;
	fw_config->touch_cal[0].screen_x = 0x20 + 1; // calibration screen coords are 1-based,
	fw_config->touch_cal[0].screen_y = 0x20 + 1; // either that or NDS_getADCTouchPosX/Y are wrong.
	//VALUES FROM NOCASH
	//fw_config->touch_cal[0].adc_x = 0x02DF;
	//fw_config->touch_cal[0].adc_y = 0x032C;
	//fw_config->touch_cal[0].screen_x = 0x20;
	//fw_config->touch_cal[0].screen_y = 0x20;

	//ANCIENT DESMUME VALUES
	fw_config->touch_cal[1].adc_x = 0xe00;
	fw_config->touch_cal[1].adc_y = 0x800;
	fw_config->touch_cal[1].screen_x = 0xe0 + 1;
	fw_config->touch_cal[1].screen_y = 0x80 + 1;
	//VALUES FROM NOCASH
	//fw_config->touch_cal[1].adc_x = 0x0D3B;
	//fw_config->touch_cal[1].adc_y = 0x0CE7;
	//fw_config->touch_cal[1].screen_x = 0xE0;
	//fw_config->touch_cal[1].screen_y = 0xA0;
}

void NDS_PatchFirmwareMAC()
{
	memcpy((MMU.fw.data + 0x36), FW_Mac, sizeof(FW_Mac));
	(*(u16*)(MMU.fw.data + 0x2A)) = calc_CRC16(0, (MMU.fw.data + 0x2C), 0x138);
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
		if (mc->fp)
		{
			fseek(mc->fp, 0, SEEK_SET);
			fwrite(mc->data, mc->size, 1, mc->fp);
		}

		if (mc->isFirmware && CommonSettings.UseExtFirmware && CommonSettings.UseExtFirmwareSettings && firmware)
		{
			firmware->saveSettings();
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
						data = mc->data[mc->addr];       /* return byte */
						mc->addr++;      /* then increment address */
					}
					break;
					
				case FW_CMD_PAGEWRITE:
					if(mc->addr < mc->size)
					{
						mc->data[mc->addr] = data;       /* write byte */
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
	mc->data = NULL;
	mc->size = 0;
	mc->write_enable = FALSE;
	mc->writeable_buffer = FALSE;
	mc->type = type;

	switch(mc->type)
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
	default: break;
	}
}

u8 *mc_alloc(fw_memory_chip *mc, u32 size)
{
	u8 *buffer = NULL;
	buffer = new u8[size];
	if(!buffer) { return NULL; }
	memset(buffer,0,size);

	if (mc->data) delete [] mc->data;
	mc->data = buffer;
	mc->size = size;
	mc->writeable_buffer = TRUE;

	return buffer;
}

void mc_free(fw_memory_chip *mc)
{
    if(mc->data) delete[] mc->data;
    mc_init(mc, 0);
}
