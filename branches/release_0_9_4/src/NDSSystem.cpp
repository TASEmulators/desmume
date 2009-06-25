/*	Copyright (C) 2006 yopyop
    yopyop156@ifrance.com
    yopyop156.ifrance.com 

	Copyright (C) 2008-2009 DeSmuME team

    This file is part of DeSmuME

    DeSmuME is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DeSmuME is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DeSmuME; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <string.h>
#include <stdlib.h>
#include <algorithm>
#include <math.h>
#include <zlib.h>

#include "common.h"
#include "NDSSystem.h"
#include "render3D.h"
#include "MMU.h"
#ifndef EXPERIMENTAL_GBASLOT
#include "cflash.h"
#endif
#include "ROMReader.h"
#include "gfx3d.h"
#include "utils/decrypt/decrypt.h"
#include "utils/decrypt/crc.h"
#include "bios.h"
#include "debug.h"
#include "cheatSystem.h"
#include "movie.h"
#include "Disassembler.h"

#ifdef _WIN32
#include "./windows/disView.h"
#endif

TCommonSettings CommonSettings;
static BaseDriver _stub_driver;
BaseDriver* driver = &_stub_driver;
std::string InputDisplayString;

static BOOL LidClosed = FALSE;
static u8	countLid = 0;
char pathToROM[MAX_PATH];
char pathFilenameToROMwithoutExt[MAX_PATH];

GameInfo gameInfo;

/* the count of bytes copied from the firmware into memory */
#define NDS_FW_USER_SETTINGS_MEM_BYTE_COUNT 0x70

BOOL fw_success = FALSE;

NDSSystem nds;

int lagframecounter;
int LagFrameFlag;
int lastLag;
int TotalLagFrames;

/* ------------------------------------------------------------------------- */
/* FIRMWARE DECRYPTION */

static u32 keyBuf[0x412];
static u32 keyCode[3];

#define DWNUM(i) ((i) >> 2)

static u32 bswap32(u32 val)
{
	return (((val & 0x000000FF) << 24) | ((val & 0x0000FF00) << 8) | ((val & 0x00FF0000) >> 8) | ((val & 0xFF000000) >> 24));
}

static BOOL getKeyBuf()
{
	int dummy;
	FILE *file = fopen(CommonSettings.ARM7BIOS, "rb");

	if(file == NULL)
		return FALSE;

	fseek(file, 0x30, SEEK_SET);
	dummy = fread(keyBuf, 0x412, 4, file);

	fclose(file);
	return TRUE;
}

static void crypt64BitUp(u32 *ptr)
{
	u32 Y = ptr[0];
	u32 X = ptr[1];

	for(u32 i = 0x00; i <= 0x0F; i++)
	{
		u32 Z = (keyBuf[i] ^ X);
		X = keyBuf[DWNUM(0x048 + (((Z >> 24) & 0xFF) << 2))];
		X = (keyBuf[DWNUM(0x448 + (((Z >> 16) & 0xFF) << 2))] + X);
		X = (keyBuf[DWNUM(0x848 + (((Z >> 8) & 0xFF) << 2))] ^ X);
		X = (keyBuf[DWNUM(0xC48 + ((Z & 0xFF) << 2))] + X);
		X = (Y ^ X);
		Y = Z;
	}

	ptr[0] = (X ^ keyBuf[DWNUM(0x40)]);
	ptr[1] = (Y ^ keyBuf[DWNUM(0x44)]);
}

static void crypt64BitDown(u32 *ptr)
{
	u32 Y = ptr[0];
	u32 X = ptr[1];

	for(u32 i = 0x11; i >= 0x02; i--)
	{
		u32 Z = (keyBuf[i] ^ X);
		X = keyBuf[DWNUM(0x048 + (((Z >> 24) & 0xFF) << 2))];
		X = (keyBuf[DWNUM(0x448 + (((Z >> 16) & 0xFF) << 2))] + X);
		X = (keyBuf[DWNUM(0x848 + (((Z >> 8) & 0xFF) << 2))] ^ X);
		X = (keyBuf[DWNUM(0xC48 + ((Z & 0xFF) << 2))] + X);
		X = (Y ^ X);
		Y = Z;
	}

	ptr[0] = (X ^ keyBuf[DWNUM(0x04)]);
	ptr[1] = (Y ^ keyBuf[DWNUM(0x00)]);
}

static void applyKeycode(u32 modulo)
{
	crypt64BitUp(&keyCode[1]);
	crypt64BitUp(&keyCode[0]);

	u32 scratch[2] = {0x00000000, 0x00000000};

	for(u32 i = 0; i <= 0x44; i += 4)
	{
		keyBuf[DWNUM(i)] = (keyBuf[DWNUM(i)] ^ bswap32(keyCode[DWNUM(i % modulo)]));
	}

	for(u32 i = 0; i <= 0x1040; i += 8)
	{
		crypt64BitUp(scratch);
		keyBuf[DWNUM(i)] = scratch[1];
		keyBuf[DWNUM(i+4)] = scratch[0];
	}
}

static BOOL initKeycode(u32 idCode, int level, u32 modulo)
{
	if(getKeyBuf() == FALSE)
		return FALSE;

	keyCode[0] = idCode;
	keyCode[1] = (idCode >> 1);
	keyCode[2] = (idCode << 1);

	if(level >= 1) applyKeycode(modulo);
	if(level >= 2) applyKeycode(modulo);

	keyCode[1] <<= 1;
	keyCode[2] >>= 1;

	if(level >= 3) applyKeycode(modulo);

	return TRUE;
}

static u16 getBootCodeCRC16()
{
	unsigned int i, j;
	u32 crc = 0xFFFF;
	const u16 val[8] = {0xC0C1, 0xC181, 0xC301, 0xC601, 0xCC01, 0xD801, 0xF001, 0xA001};

	for(i = 0; i < nds.FW_ARM9BootCodeSize; i++)
	{
		crc = (crc ^ nds.FW_ARM9BootCode[i]);

		for(j = 0; j < 8; j++) 
		{
			if(crc & 0x0001)
				crc = ((crc >> 1) ^ (val[j] << (7-j)));
			else
				crc =  (crc >> 1);
		}
	}

	for(i = 0; i < nds.FW_ARM7BootCodeSize; i++)
	{
		crc = (crc ^ nds.FW_ARM7BootCode[i]);

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

static u32 * decryptFirmwareBlock(const char *blockName, u32 *src, u32 &size)
{
	u32 curBlock[2];
	u32 *dst;
	u32 i, j;
	u32 xIn = 4, xOut = 0;
	u32 len;
	u32 offset;
	u32 windowOffset;
	u32 xLen;
	u8 d;
	u16 data;

	memcpy(curBlock, src, 8);
	crypt64BitDown(curBlock);

	u32 blockSize = (curBlock[0] >> 8);
	size = blockSize;

	INFO("Firmware: %s final size: %i bytes\n", blockName, blockSize);

	dst = (u32*)new u8[blockSize];

	xLen = blockSize;

	while(xLen > 0)
	{
		d = T1ReadByte((u8*)curBlock, (xIn % 8));
		xIn++;
		if((xIn % 8) == 0)
		{
			memcpy(curBlock, (((u8*)src) + xIn), 8);
			crypt64BitDown(curBlock);
		}

		for(i = 0; i < 8; i++)
		{
			if(d & 0x80)
			{
				data = (T1ReadByte((u8*)curBlock, (xIn % 8)) << 8);
				xIn++;
				if((xIn % 8) == 0)
				{
					memcpy(curBlock, (((u8*)src) + xIn), 8);
					crypt64BitDown(curBlock);
				}
				data |= T1ReadByte((u8*)curBlock, (xIn % 8));
				xIn++;
				if((xIn % 8) == 0)
				{
					memcpy(curBlock, (((u8*)src) + xIn), 8);
					crypt64BitDown(curBlock);
				}

				len = (data >> 12) + 3;
				offset = (data & 0xFFF);
				windowOffset = (xOut - offset - 1);

				for(j = 0; j < len; j++)
				{
					T1WriteByte((u8*)dst, xOut, T1ReadByte((u8*)dst, windowOffset));
					xOut++;
					windowOffset++;

					xLen--;
					if(xLen == 0)
						goto lz77End;
				}
			}
			else
			{
				T1WriteByte((u8*)dst, xOut, T1ReadByte((u8*)curBlock, (xIn % 8)));
				xOut++;
				xIn++;
				if((xIn % 8) == 0)
				{
					memcpy(curBlock, (((u8*)src) + xIn), 8);
					crypt64BitDown(curBlock);
				}

				xLen--;
				if(xLen == 0)
					goto lz77End;
			}

			d = ((d << 1) & 0xFF);
		}
	}

lz77End:

	return dst;
}

static BOOL decryptFirmware(u8 *data)
{
	u16 shifts;
	u16 shift1, shift2, shift3, shift4;
	u32 part1addr, part2addr, part3addr, part4addr, part5addr;
	u32 part1ram, part2ram;

	shifts = T1ReadWord(data, 0x14);
	shift1 = (shifts & 0x7);
	shift2 = ((shifts >> 3) & 0x7);
	shift3 = ((shifts >> 6) & 0x7);
	shift4 = ((shifts >> 9) & 0x7);

	part1addr = T1ReadWord(data, 0x0C);
	part1addr = (part1addr << (2+shift1));

	INFO("Firmware: ARM9 boot code address: %05X\n", part1addr);

	part1ram = T1ReadWord(data, 0x0E);
	part1ram = (0x02800000 - (part1ram << (2+shift2)));

	INFO("Firmware: ARM9 boot code RAM address: %08X\n", part1ram);

	part2addr = T1ReadWord(data, 0x10);
	part2addr = (part2addr << (2+shift3));

	INFO("Firmware: ARM7 boot code address: %05X\n", part2addr);

	part2ram = T1ReadWord(data, 0x12);
	part2ram = (0x03810000 - (part2ram << (2+shift4)));

	INFO("Firmware: ARM7 boot code RAM address: %08X\n", part2ram);

	part3addr = T1ReadWord(data, 0x00);
	part3addr = (part3addr << 3);

	INFO("Firmware: ARM9 GUI code address: %05X\n", part3addr);

	part4addr = T1ReadWord(data, 0x02);
	part4addr = (part4addr << 3);

	INFO("Firmware: ARM7 GUI code address: %05X\n", part4addr);

	part5addr = T1ReadWord(data, 0x16);
	part5addr = (part5addr << 3);

	INFO("Firmware: data/gfx address: %05X\n", part5addr);

	if(initKeycode(T1ReadLong(data, 0x08), 1, 0xC) == FALSE) return FALSE;
	crypt64BitDown((u32*)&data[0x18]);
	if(initKeycode(T1ReadLong(data, 0x08), 2, 0xC) == FALSE) return FALSE;

	nds.FW_ARM9BootCode = (u8*)decryptFirmwareBlock("ARM9 boot code", (u32*)&data[part1addr], nds.FW_ARM9BootCodeSize);
	nds.FW_ARM7BootCode = (u8*)decryptFirmwareBlock("ARM7 boot code", (u32*)&data[part2addr], nds.FW_ARM7BootCodeSize);

	nds.FW_ARM9BootCodeAddr = part1ram;
	nds.FW_ARM7BootCodeAddr = part2ram;

	u16 crc16_header = T1ReadWord(data, 0x06);
	u16 crc16_mine = getBootCodeCRC16();
	if(crc16_header != crc16_mine)
	{
		INFO("Firmware: error: the boot code CRC16 (%04X) doesn't match the value in the firmware header (%04X).\n", 
			crc16_mine, crc16_header);
		return FALSE;
	}

	return TRUE;
}

/* ------------------------------------------------------------------------- */

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

static int
copy_firmware_user_data( u8 *dest_buffer, const u8 *fw_data) {
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


#ifdef GDB_STUB
int NDS_Init( struct armcpu_memory_iface *arm9_mem_if,
struct armcpu_ctrl_iface **arm9_ctrl_iface,
struct armcpu_memory_iface *arm7_mem_if,
struct armcpu_ctrl_iface **arm7_ctrl_iface) {
#else
int NDS_Init( void) {
#endif
	nds.ARM9Cycle = 0;
	nds.ARM7Cycle = 0;
	nds.cycles = 0;
	nds.idleFrameCounter = 0;
	memset(nds.runCycleCollector,0,sizeof(nds.runCycleCollector));
	MMU_Init();
	nds.nextHBlank = 3168;
	nds.VCount = 0;
	nds.lignerendu = FALSE;

	nds.sleeping = FALSE;

	if (Screen_Init(GFXCORE_DUMMY) != 0)
		return -1;

	gfx3d_init();

#ifdef GDB_STUB
	armcpu_new(&NDS_ARM7,1, arm7_mem_if, arm7_ctrl_iface);
	armcpu_new(&NDS_ARM9,0, arm9_mem_if, arm9_ctrl_iface);
#else
	armcpu_new(&NDS_ARM7,1);
	armcpu_new(&NDS_ARM9,0);
#endif

	if (SPU_Init(SNDCORE_DUMMY, 740) != 0)
		return -1;

#ifdef EXPERIMENTAL_WIFI
	WIFI_Init(&wifiMac) ;
	WIFI_SoftAP_Init(&wifiMac);
#endif

	nds.FW_ARM9BootCode = NULL;
	nds.FW_ARM7BootCode = NULL;

	return 0;
}

void NDS_DeInit(void) {
	if(MMU.CART_ROM != MMU.UNUSED_RAM)
		NDS_FreeROM();

	nds.nextHBlank = 3168;
	SPU_DeInit();
	Screen_DeInit();
	MMU_DeInit();
	gpu3D->NDS_3D_Close();

#ifdef EXPERIMENTAL_WIFI
	WIFI_SoftAP_Shutdown(&wifiMac);
#endif
	cheatsSearchClose();
}

BOOL NDS_SetROM(u8 * rom, u32 mask)
{
	MMU_setRom(rom, mask);

	return TRUE;
} 

NDS_header * NDS_getROMHeader(void)
{
	NDS_header * header = new NDS_header;

	memcpy(header->gameTile, MMU.CART_ROM, 12);
	memcpy(header->gameCode, MMU.CART_ROM + 12, 4);
	header->makerCode = T1ReadWord(MMU.CART_ROM, 16);
	header->unitCode = MMU.CART_ROM[18];
	header->deviceCode = MMU.CART_ROM[19];
	header->cardSize = MMU.CART_ROM[20];
	memcpy(header->cardInfo, MMU.CART_ROM + 21, 8);
	header->flags = MMU.CART_ROM[29];
	header->ARM9src = T1ReadLong(MMU.CART_ROM, 32);
	header->ARM9exe = T1ReadLong(MMU.CART_ROM, 36);
	header->ARM9cpy = T1ReadLong(MMU.CART_ROM, 40);
	header->ARM9binSize = T1ReadLong(MMU.CART_ROM, 44);
	header->ARM7src = T1ReadLong(MMU.CART_ROM, 48);
	header->ARM7exe = T1ReadLong(MMU.CART_ROM, 52);
	header->ARM7cpy = T1ReadLong(MMU.CART_ROM, 56);
	header->ARM7binSize = T1ReadLong(MMU.CART_ROM, 60);
	header->FNameTblOff = T1ReadLong(MMU.CART_ROM, 64);
	header->FNameTblSize = T1ReadLong(MMU.CART_ROM, 68);
	header->FATOff = T1ReadLong(MMU.CART_ROM, 72);
	header->FATSize = T1ReadLong(MMU.CART_ROM, 76);
	header->ARM9OverlayOff = T1ReadLong(MMU.CART_ROM, 80);
	header->ARM9OverlaySize = T1ReadLong(MMU.CART_ROM, 84);
	header->ARM7OverlayOff = T1ReadLong(MMU.CART_ROM, 88);
	header->ARM7OverlaySize = T1ReadLong(MMU.CART_ROM, 92);
	header->unknown2a = T1ReadLong(MMU.CART_ROM, 96);
	header->unknown2b = T1ReadLong(MMU.CART_ROM, 100);
	header->IconOff = T1ReadLong(MMU.CART_ROM, 104);
	header->CRC16 = T1ReadWord(MMU.CART_ROM, 108);
	header->ROMtimeout = T1ReadWord(MMU.CART_ROM, 110);
	header->ARM9unk = T1ReadLong(MMU.CART_ROM, 112);
	header->ARM7unk = T1ReadLong(MMU.CART_ROM, 116);
	memcpy(header->unknown3c, MMU.CART_ROM + 120, 8);
	header->ROMSize = T1ReadLong(MMU.CART_ROM, 128);
	header->HeaderSize = T1ReadLong(MMU.CART_ROM, 132);
	memcpy(header->unknown5, MMU.CART_ROM + 136, 56);
	memcpy(header->logo, MMU.CART_ROM + 192, 156);
	header->logoCRC16 = T1ReadWord(MMU.CART_ROM, 348);
	header->headerCRC16 = T1ReadWord(MMU.CART_ROM, 350);
	memcpy(header->reserved, MMU.CART_ROM + 352, 160);

	return header;
} 

void NDS_setTouchPos(u16 x, u16 y)
{
	nds.touchX = (x <<4);
	nds.touchY = (y <<4);
	nds.isTouch = 1;

	MMU.ARM7_REG[0x136] &= 0xBF;
}

void NDS_releaseTouch(void)
{ 
	nds.touchX = 0;
	nds.touchY = 0;
	nds.isTouch = 0;

	MMU.ARM7_REG[0x136] |= 0x40;
}

void debug()
{
	//if(NDS_ARM9.R[15]==0x020520DC) emu_halt();
	//DSLinux
	//if(NDS_ARM9.CPSR.bits.mode == 0) emu_halt();
	//if((NDS_ARM9.R[15]&0xFFFFF000)==0) emu_halt();
	//if((NDS_ARM9.R[15]==0x0201B4F4)/*&&(NDS_ARM9.R[1]==0x0)*/) emu_halt();
	//AOE
	//if((NDS_ARM9.R[15]==0x01FFE194)&&(NDS_ARM9.R[0]==0)) emu_halt();
	//if((NDS_ARM9.R[15]==0x01FFE134)&&(NDS_ARM9.R[0]==0)) emu_halt();

	//BBMAN
	//if(NDS_ARM9.R[15]==0x02098B4C) emu_halt();
	//if(NDS_ARM9.R[15]==0x02004924) emu_halt();
	//if(NDS_ARM9.R[15]==0x02004890) emu_halt();

	//if(NDS_ARM9.R[15]==0x0202B800) emu_halt();
	//if(NDS_ARM9.R[15]==0x0202B3DC) emu_halt();
	//if((NDS_ARM9.R[1]==0x9AC29AC1)&&(!fait)) {emu_halt();fait = TRUE;}
	//if(NDS_ARM9.R[1]==0x0400004A) {emu_halt();fait = TRUE;}
	/*if(NDS_ARM9.R[4]==0x2E33373C) emu_halt();
	if(NDS_ARM9.R[15]==0x02036668) //emu_halt();
	{
	nds.logcount++;
	sprintf(logbuf, "%d %08X", nds.logcount, NDS_ARM9.R[13]);
	log::ajouter(logbuf);
	if(nds.logcount==89) execute=FALSE;
	}*/
	//if(NDS_ARM9.instruction==0) emu_halt();
	//if((NDS_ARM9.R[15]>>28)) emu_halt();
}

#define DSGBA_LOADER_SIZE 512
enum
{
	ROM_NDS = 0,
	ROM_DSGBA
};

#if 0 /* not used */
//http://www.aggregate.org/MAGIC/#Population%20Count%20(Ones%20Count)
static u32 ones32(u32 x)
{
	/* 32-bit recursive reduction using SWAR...
	but first step is mapping 2-bit values
	into sum of 2 1-bit values in sneaky way
	*/
	x -= ((x >> 1) & 0x55555555);
	x = (((x >> 2) & 0x33333333) + (x & 0x33333333));
	x = (((x >> 4) + x) & 0x0f0f0f0f);
	x += (x >> 8);
	x += (x >> 16);
	return(x & 0x0000003f);
}
#endif

void GameInfo::populate()
{
	NDS_header * _header = NDS_getROMHeader();
	header = *_header;
	delete _header;

	if (
		// ??? in all Homebrews game title have is 2E0000EA
		//(
		//(header->gameTile[0] == 0x2E) && 
		//(header->gameTile[1] == 0x00) && 
		//(header->gameTile[2] == 0x00) && 
		//(header->gameTile[3] == 0xEA)
		//) &&
		(
			((header.gameCode[0] == 0x23) && 
			(header.gameCode[1] == 0x23) && 
			(header.gameCode[2] == 0x23) && 
			(header.gameCode[3] == 0x23)
			) || 
			(header.gameCode[0] == 0x00) 
		)
		&&
			header.makerCode == 0x0
		)
	{
		memset(ROMserial, 0, sizeof(ROMserial));
		strcpy(ROMserial, "Homebrew");
	}
	else
	{
		memset(ROMserial, '_', sizeof(ROMserial));
		memcpy(ROMserial, header.gameTile, strlen(header.gameTile) < 12 ? strlen(header.gameTile) : 12);
		memcpy(ROMserial+12+1, header.gameCode, 4);
		memcpy(ROMserial+12+1+4, &header.makerCode, 2);
		memset(ROMserial+19, '\0', 1);
	}
}

#ifdef EXPERIMENTAL_GBASLOT
int NDS_LoadROM( const char *filename)
#else
int NDS_LoadROM( const char *filename,
				const char *cflash_disk_image_file)
#endif
{
	int					ret;
	int					type;
	ROMReader_struct	*reader;
	void				*file;
	u32					size, mask;
	u8					*data;
	char				*noext;
	char				buf[MAX_PATH];
	char				extROM[MAX_PATH];
	char				extROM2[5];

	if (filename == NULL)
		return -1;

	memset(pathToROM, 0, MAX_PATH);
	memset(pathFilenameToROMwithoutExt, 0, MAX_PATH);
	memset(extROM, 0, MAX_PATH);
	memset(extROM2, 0, 5);

	noext = strdup(filename);
	reader = ROMReaderInit(&noext);
	
	for (int t = strlen(filename); t>0; t--)
		if ( (filename[t] == '\\') || (filename[t] == '/') )
		{
			strncpy(pathToROM, filename, t+1);
			break;
		}
	
	for (int t = strlen(filename); t>0; t--)
		if ( (filename[t] == '\\') || (filename[t] == '/') || (filename[t] == '.') )
		{
			if (filename[t] != '.') return -1;
			strncpy(pathFilenameToROMwithoutExt, filename, t);
			strncpy(extROM, filename+t, strlen(filename) - t);
			if (t>4)
				strncpy(extROM2, filename+(t-3), 3);
			break;
		}

	if(!strcasecmp(extROM, ".zip"))
		type = ROM_NDS;
	else if ( !strcasecmp(extROM, ".nds"))
		type = ROM_NDS;
	else if ( !strcasecmp(extROM, ".gba") && !strcasecmp(extROM2, ".ds")) 
		type = ROM_DSGBA;
	else
		type = ROM_NDS;

	file = reader->Init(filename);
	if (!file)
	{
		reader->DeInit(file);
		free(noext);
		return -1;
	}

	size = reader->Size(file);

	if(type == ROM_DSGBA)
	{
		reader->Seek(file, DSGBA_LOADER_SIZE, SEEK_SET);
		size -= DSGBA_LOADER_SIZE;
	}

	//check that size is at least the size of the header
	if (size < 352+160) {
		reader->DeInit(file);
		free(noext);
		return -1;
	}

	//zero 25-dec-08 - this used to yield a mask which was 2x large
	//mask = size; 
	mask = size-1; 
	mask |= (mask >>1);
	mask |= (mask >>2);
	mask |= (mask >>4);
	mask |= (mask >>8);
	mask |= (mask >>16);

	// Make sure old ROM is freed first(at least this way we won't be eating
	// up a ton of ram before the old ROM is freed)
	if(MMU.CART_ROM != MMU.UNUSED_RAM)
		NDS_FreeROM();

	data = new u8[mask + 1];
	if (!data)
	{
		reader->DeInit(file);
		free(noext);
		return -1;
	}

	ret = reader->Read(file, data, size);
	reader->DeInit(file);

	//decrypt if necessary..
	//but this is untested and suspected to fail on big endian, so lets not support this on big endian
#ifndef WORDS_BIGENDIAN
	bool okRom = DecryptSecureArea(data,size);

	if(!okRom) {
		printf("Specified file is not a valid rom\n");
		return -1;
	}
#endif


	cheatsSearchClose();
	MMU_unsetRom();
	NDS_SetROM(data, mask);
	NDS_Reset();

#ifndef EXPERIMENTAL_GBASLOT
	cflash_close();
	cflash_init(cflash_disk_image_file);
#endif
	free(noext);

	memset(buf, 0, MAX_PATH);
	strcpy(buf, pathFilenameToROMwithoutExt);
	strcat(buf, ".dsv");							// DeSmuME memory card	:)

	MMU_new.backupDevice.load_rom(buf);

	memset(buf, 0, MAX_PATH);
	strcpy(buf, pathFilenameToROMwithoutExt);
	strcat(buf, ".dct");							// DeSmuME cheat		:)
	cheatsInit(buf);

	gameInfo.populate();
	gameInfo.crc = crc32(0,data,size);
	INFO("\nROM crc: %08X\n\n", gameInfo.crc);
	INFO("\nROM serial: %s\n", gameInfo.ROMserial);

	return ret;
}

void NDS_FreeROM(void)
{
	if (MMU.CART_ROM != MMU.UNUSED_RAM)
		delete [] MMU.CART_ROM;
	MMU_unsetRom();
	if (MMU.bupmem.fp)
		fclose(MMU.bupmem.fp);
	MMU.bupmem.fp = NULL;
}

bool _HACK_DONT_STOPMOVIE = false;
void NDS_Reset()
{
	unsigned int i;
	u32 src;
	u32 dst;
	FILE* inf = 0;
	NDS_header * header = NDS_getROMHeader();

	if (!header) return ;

	if(movieMode != MOVIEMODE_INACTIVE && !_HACK_DONT_STOPMOVIE)
		movie_reset_command = true;

	MMU_clearMem();
	MMU_new.backupDevice.reset();

	//ARM7 BIOS IRQ HANDLER
	if(CommonSettings.UseExtBIOS == true)
		inf = fopen(CommonSettings.ARM7BIOS,"rb");
	else
		inf = NULL;

	if(inf) {
		fread(MMU.ARM7_BIOS,1,16384,inf);
		fclose(inf);
		if(CommonSettings.SWIFromBIOS == true) NDS_ARM7.swi_tab = 0;
		else NDS_ARM7.swi_tab = ARM7_swi_tab;
		INFO("ARM7 BIOS is loaded.\n");
	} else {
		NDS_ARM7.swi_tab = ARM7_swi_tab;
		_MMU_write32<ARMCPU_ARM7>(0x00, 0xE25EF002);
		_MMU_write32<ARMCPU_ARM7>(0x04, 0xEAFFFFFE);
		_MMU_write32<ARMCPU_ARM7>(0x18, 0xEA000000);
		_MMU_write32<ARMCPU_ARM7>(0x20, 0xE92D500F);
		_MMU_write32<ARMCPU_ARM7>(0x24, 0xE3A00301);
		_MMU_write32<ARMCPU_ARM7>(0x28, 0xE28FE000);
		_MMU_write32<ARMCPU_ARM7>(0x2C, 0xE510F004);
		_MMU_write32<ARMCPU_ARM7>(0x30, 0xE8BD500F);
		_MMU_write32<ARMCPU_ARM7>(0x34, 0xE25EF004);
	}

	//ARM9 BIOS IRQ HANDLER
	if(CommonSettings.UseExtBIOS == true)
		inf = fopen(CommonSettings.ARM9BIOS,"rb");
	else
		inf = NULL;
	//memcpy(ARM9Mem.ARM9_BIOS + 0x20, gba_header_data_0x04, 156);

	if(inf) {
		fread(ARM9Mem.ARM9_BIOS,1,4096,inf);
		fclose(inf);
		if(CommonSettings.SWIFromBIOS == true) NDS_ARM9.swi_tab = 0;
		else NDS_ARM9.swi_tab = ARM9_swi_tab;
		INFO("ARM9 BIOS is loaded.\n");
	} else {
		NDS_ARM9.swi_tab = ARM9_swi_tab;
		_MMU_write32<ARMCPU_ARM9>(0xFFFF0018, 0xEA000000);
		_MMU_write32<ARMCPU_ARM9>(0xFFFF0020, 0xE92D500F);
		_MMU_write32<ARMCPU_ARM9>(0xFFFF0024, 0xEE190F11);
		_MMU_write32<ARMCPU_ARM9>(0xFFFF0028, 0xE1A00620);
		_MMU_write32<ARMCPU_ARM9>(0xFFFF002C, 0xE1A00600);
		_MMU_write32<ARMCPU_ARM9>(0xFFFF0030, 0xE2800C40);
		_MMU_write32<ARMCPU_ARM9>(0xFFFF0034, 0xE28FE000);
		_MMU_write32<ARMCPU_ARM9>(0xFFFF0038, 0xE510F004);
		_MMU_write32<ARMCPU_ARM9>(0xFFFF003C, 0xE8BD500F);
		_MMU_write32<ARMCPU_ARM9>(0xFFFF0040, 0xE25EF004);
	}

	/* Is it really needed ??? */
	_MMU_write32<ARMCPU_ARM9>(0x0000004, 0xE3A0010E);
	_MMU_write32<ARMCPU_ARM9>(0x0000008, 0xE3A01020);
	// _MMU_write32<ARMCPU_ARM9>(0x000000C, 0xE1B02110);
	_MMU_write32<ARMCPU_ARM9>(0x000000C, 0xE1B02040);
	_MMU_write32<ARMCPU_ARM9>(0x0000010, 0xE3B02020);
	// _MMU_write32<ARMCPU_ARM9>(0x0000010, 0xE2100202);

	if(CommonSettings.UseExtFirmware == true)
		NDS_LoadFirmware(CommonSettings.Firmware);

	if((CommonSettings.UseExtBIOS == true) && (CommonSettings.UseExtFirmware == true) && (CommonSettings.BootFromFirmware == true) && (fw_success == TRUE))
	{
		for(i = 0; i < nds.FW_ARM9BootCodeSize; i += 4)
		{
			_MMU_write32<ARMCPU_ARM9>((nds.FW_ARM9BootCodeAddr + i), T1ReadLong(nds.FW_ARM9BootCode, i));
		}

		for(i = 0; i < nds.FW_ARM7BootCodeSize; i += 4)
		{
			_MMU_write32<ARMCPU_ARM7>((nds.FW_ARM7BootCodeAddr + i), T1ReadLong(nds.FW_ARM7BootCode, i));
		}

		armcpu_init(&NDS_ARM9, nds.FW_ARM9BootCodeAddr);
		armcpu_init(&NDS_ARM7, nds.FW_ARM7BootCodeAddr);
		//armcpu_init(&NDS_ARM9, 0xFFFF0000);
		//armcpu_init(&NDS_ARM7, 0x00000000);
	}
	else
	{
		src = header->ARM9src;
		dst = header->ARM9cpy;

		for(i = 0; i < (header->ARM9binSize>>2); ++i)
		{
			_MMU_write32<ARMCPU_ARM9>(dst, T1ReadLong(MMU.CART_ROM, src));
			dst += 4;
			src += 4;
		}

		src = header->ARM7src;
		dst = header->ARM7cpy;

		for(i = 0; i < (header->ARM7binSize>>2); ++i)
		{
			_MMU_write32<ARMCPU_ARM7>(dst, T1ReadLong(MMU.CART_ROM, src));
			dst += 4;
			src += 4;
		}

		armcpu_init(&NDS_ARM7, header->ARM7exe);
		armcpu_init(&NDS_ARM9, header->ARM9exe);
	}
	
	nds.ARM9Cycle = 0;
	nds.ARM7Cycle = 0;
	nds.wifiCycle = 0;
	nds.cycles = 0;
	memset(nds.timerCycle, 0, sizeof(s32) * 2 * 4);
	memset(nds.timerOver, 0, sizeof(BOOL) * 2 * 4);
	nds.nextHBlank = 3168;
	nds.VCount = 0;
	nds.old = 0;
	nds.diff = 0;
	nds.lignerendu = FALSE;
	nds.touchX = nds.touchY = 0;
	nds.isTouch = 0;
	nds.debugConsole = CommonSettings.DebugConsole;
	SetupMMU(nds.debugConsole);

	_MMU_write16<ARMCPU_ARM9>(0x04000130, 0x3FF);
	_MMU_write16<ARMCPU_ARM7>(0x04000130, 0x3FF);
	_MMU_write08<ARMCPU_ARM7>(0x04000136, 0x43);

	LidClosed = FALSE;
	countLid = 0;

	/*
	* Setup a copy of the firmware user settings in memory.
	* (this is what the DS firmware would do).
	*/
	{
		u8 temp_buffer[NDS_FW_USER_SETTINGS_MEM_BYTE_COUNT];
		int fw_index;

		if ( copy_firmware_user_data( temp_buffer, MMU.fw.data)) {
			for ( fw_index = 0; fw_index < NDS_FW_USER_SETTINGS_MEM_BYTE_COUNT; fw_index++)
				_MMU_write08<ARMCPU_ARM9>(0x027FFC80 + fw_index, temp_buffer[fw_index]);
		}
	}

	// Copy the whole header to Main RAM 0x27FFE00 on startup.
	//  Reference: http://nocash.emubase.de/gbatek.htm#dscartridgeheader
	for (i = 0; i < ((0x170+0x90)/4); i++) {
		_MMU_write32<ARMCPU_ARM9>(0x027FFE00+i*4, LE_TO_LOCAL_32(((u32*)MMU.CART_ROM)[i]));
	}

	// Write the header checksum to memory (the firmware needs it to see the cart)
	_MMU_write16<ARMCPU_ARM9>(0x027FF808, T1ReadWord(MMU.CART_ROM, 0x15E));

	MainScreen.offset = 0;
	SubScreen.offset = 192;

	//_MMU_write32[ARMCPU_ARM9](0x02007FFC, 0xE92D4030);

	delete header;

	Screen_Reset();
	gfx3d_reset();
	gpu3D->NDS_3D_Reset();
	SPU_Reset();

#ifdef EXPERIMENTAL_WIFI
	WIFI_Init(&wifiMac);

	WIFI_SoftAP_Shutdown(&wifiMac);
	WIFI_SoftAP_Init(&wifiMac);
#endif

	memcpy(FW_Mac, (MMU.fw.data + 0x36), 6);
}

int NDS_ImportSave(const char *filename)
{
	if (strlen(filename) < 4)
		return 0;

	if (memcmp(filename+strlen(filename)-4, ".duc", 4) == 0)
		return MMU_new.backupDevice.load_duc(filename);
	else
		return MMU_new.backupDevice.load_raw(filename);

	return 0;
}

bool NDS_ExportSave(const char *filename)
{
	if (strlen(filename) < 4)
		return false;

	if (memcmp(filename+strlen(filename)-4, ".sav", 4) == 0)
		return MMU_new.backupDevice.save_raw(filename);

	return false;
}

static int WritePNGChunk(FILE *fp, uint32 size, const char *type, const uint8 *data)
{
	uint32 crc;

	uint8 tempo[4];

	tempo[0]=size>>24;
	tempo[1]=size>>16;
	tempo[2]=size>>8;
	tempo[3]=size;

	if(fwrite(tempo,4,1,fp)!=1)
		return 0;
	if(fwrite(type,4,1,fp)!=1)
		return 0;

	if(size)
		if(fwrite(data,1,size,fp)!=size)
			return 0;

	crc = crc32(0,(uint8 *)type,4);
	if(size)
		crc = crc32(crc,data,size);

	tempo[0]=crc>>24;
	tempo[1]=crc>>16;
	tempo[2]=crc>>8;
	tempo[3]=crc;

	if(fwrite(tempo,4,1,fp)!=1)
		return 0;
	return 1;
}
int NDS_WritePNG(const char *fname)
{
	int x, y;
	int width=256;
	int height=192*2;
	u16 * bmp = (u16 *)GPU_tempScreen;
	FILE *pp=NULL;
	uint8 *compmem = NULL;
	uLongf compmemsize = (uLongf)( (height * (width + 1) * 3 * 1.001 + 1) + 12 );

	if(!(compmem=(uint8 *)malloc(compmemsize)))
		return 0;

	if(!(pp=fopen(fname, "wb")))
	{
		return 0;
	}
	{
		static uint8 header[8]={137,80,78,71,13,10,26,10};
		if(fwrite(header,8,1,pp)!=1)
			goto PNGerr;
	}

	{
		uint8 chunko[13];

		chunko[0] = width >> 24;		// Width
		chunko[1] = width >> 16;
		chunko[2] = width >> 8;
		chunko[3] = width;

		chunko[4] = height >> 24;		// Height
		chunko[5] = height >> 16;
		chunko[6] = height >> 8;
		chunko[7] = height;

		chunko[8]=8;				// 8 bits per sample(24 bits per pixel)
		chunko[9]=2;				// Color type; RGB triplet
		chunko[10]=0;				// compression: deflate
		chunko[11]=0;				// Basic adapative filter set(though none are used).
		chunko[12]=0;				// No interlace.

		if(!WritePNGChunk(pp,13,"IHDR",chunko))
			goto PNGerr;
	}

	{
		uint8 *tmp_buffer;
		uint8 *tmp_inc;
		tmp_inc = tmp_buffer = (uint8 *)malloc((width * 3 + 1) * height);

		for(y=0;y<height;y++)
		{
			*tmp_inc = 0;
			tmp_inc++;
			for(x=0;x<width;x++)
			{
				int r,g,b;
				u16 pixel = bmp[y*256+x];
				r = pixel>>10;
				pixel-=r<<10;
				g = pixel>>5;
				pixel-=g<<5;
				b = pixel;
				r*=255/31;
				g*=255/31;
				b*=255/31;
				tmp_inc[0] = b;
				tmp_inc[1] = g;
				tmp_inc[2] = r;
				tmp_inc += 3;
			}
		}

		if(compress(compmem, &compmemsize, tmp_buffer, height * (width * 3 + 1))!=Z_OK)
		{
			if(tmp_buffer) free(tmp_buffer);
			goto PNGerr;
		}
		if(tmp_buffer) free(tmp_buffer);
		if(!WritePNGChunk(pp,compmemsize,"IDAT",compmem))
			goto PNGerr;
	}
	if(!WritePNGChunk(pp,0,"IEND",0))
		goto PNGerr;

	free(compmem);
	fclose(pp);

	return 1;

PNGerr:
	if(compmem)
		free(compmem);
	if(pp)
		fclose(pp);
	return(0);
}

typedef struct
{
	u32 size;
	s32 width;
	s32 height;
	u16 planes;
	u16 bpp;
	u32 cmptype;
	u32 imgsize;
	s32 hppm;
	s32 vppm;
	u32 numcol;
	u32 numimpcol;
} bmpimgheader_struct;

#include "PACKED.h"
typedef struct
{
	u16 id __PACKED;
	u32 size __PACKED;
	u16 reserved1 __PACKED;
	u16 reserved2 __PACKED;
	u32 imgoffset __PACKED;
} bmpfileheader_struct;
#include "PACKED_END.h"

int NDS_WriteBMP(const char *filename)
{
	bmpfileheader_struct fileheader;
	bmpimgheader_struct imageheader;
	FILE *file;
	int i,j;
	u16 * bmp = (u16 *)GPU_tempScreen;
	size_t elems_written = 0;

	memset(&fileheader, 0, sizeof(fileheader));
	fileheader.size = sizeof(fileheader);
	fileheader.id = 'B' | ('M' << 8);
	fileheader.imgoffset = sizeof(fileheader)+sizeof(imageheader);

	memset(&imageheader, 0, sizeof(imageheader));
	imageheader.size = sizeof(imageheader);
	imageheader.width = 256;
	imageheader.height = 192*2;
	imageheader.planes = 1;
	imageheader.bpp = 24;
	imageheader.cmptype = 0; // None
	imageheader.imgsize = imageheader.width * imageheader.height * 3;

	if ((file = fopen(filename,"wb")) == NULL)
		return 0;

	elems_written += fwrite(&fileheader, 1, sizeof(fileheader), file);
	elems_written += fwrite(&imageheader, 1, sizeof(imageheader), file);

	for(j=0;j<192*2;j++)
	{
		for(i=0;i<256;i++)
		{
			u8 r,g,b;
			u16 pixel = bmp[(192*2-j-1)*256+i];
			r = pixel>>10;
			pixel-=r<<10;
			g = pixel>>5;
			pixel-=g<<5;
			b = pixel;
			r*=255/31;
			g*=255/31;
			b*=255/31;
			elems_written += fwrite(&r, 1, sizeof(u8), file); 
			elems_written += fwrite(&g, 1, sizeof(u8), file); 
			elems_written += fwrite(&b, 1, sizeof(u8), file);
		}
	}
	fclose(file);

	return 1;
}

int NDS_WriteBMP_32bppBuffer(int width, int height, const void* buf, const char *filename)
{
	bmpfileheader_struct fileheader;
	bmpimgheader_struct imageheader;
	FILE *file;
	size_t elems_written = 0;
	memset(&fileheader, 0, sizeof(fileheader));
	fileheader.size = sizeof(fileheader);
	fileheader.id = 'B' | ('M' << 8);
	fileheader.imgoffset = sizeof(fileheader)+sizeof(imageheader);

	memset(&imageheader, 0, sizeof(imageheader));
	imageheader.size = sizeof(imageheader);
	imageheader.width = width;
	imageheader.height = height;
	imageheader.planes = 1;
	imageheader.bpp = 32;
	imageheader.cmptype = 0; // None
	imageheader.imgsize = imageheader.width * imageheader.height * 4;

	if ((file = fopen(filename,"wb")) == NULL)
		return 0;

	elems_written += fwrite(&fileheader, 1, sizeof(fileheader), file);
	elems_written += fwrite(&imageheader, 1, sizeof(imageheader), file);

	for(int i=0;i<height;i++)
		for(int x=0;x<width;x++)
		{
			u8* pixel = (u8*)buf + (height-i-1)*width*4;
			pixel += (x*4);
			elems_written += fwrite(pixel+2,1,1,file);
			elems_written += fwrite(pixel+1,1,1,file);
			elems_written += fwrite(pixel+0,1,1,file);
			elems_written += fwrite(pixel+3,1,1,file);
		}
	fclose(file);

	return 1;
}


static void
fill_user_data_area( struct NDS_fw_config_data *user_settings,
					u8 *data, int count) {
						u32 crc;
						int i;
						u8 *ts_cal_data_area;

						memset( data, 0, 0x100);

						/* version */
						data[0x00] = 5;
						data[0x01] = 0;

						/* colour */
						data[0x02] = user_settings->fav_colour;

						/* birthday month and day */
						data[0x03] = user_settings->birth_month;
						data[0x04] = user_settings->birth_day;

						/* nickname and length */
						for ( i = 0; i < MAX_FW_NICKNAME_LENGTH; i++) {
							data[0x06 + (i * 2)] = user_settings->nickname[i] & 0xff;
							data[0x06 + (i * 2) + 1] = (user_settings->nickname[i] >> 8) & 0xff;
						}

						data[0x1a] = user_settings->nickname_len;

						/* Message */
						for ( i = 0; i < MAX_FW_MESSAGE_LENGTH; i++) {
							data[0x1c + (i * 2)] = user_settings->message[i] & 0xff;
							data[0x1c + (i * 2) + 1] = (user_settings->message[i] >> 8) & 0xff;
						}

						data[0x50] = user_settings->message_len;

						/*
						* touch screen calibration
						*/
						ts_cal_data_area = &data[0x58];
						for ( i = 0; i < 2; i++) {
							/* ADC x y */
							*ts_cal_data_area++ = user_settings->touch_cal[i].adc_x & 0xff;
							*ts_cal_data_area++ = (user_settings->touch_cal[i].adc_x >> 8) & 0xff;
							*ts_cal_data_area++ = user_settings->touch_cal[i].adc_y & 0xff;
							*ts_cal_data_area++ = (user_settings->touch_cal[i].adc_y >> 8) & 0xff;

							/* screen x y */
							*ts_cal_data_area++ = user_settings->touch_cal[i].screen_x;
							*ts_cal_data_area++ = user_settings->touch_cal[i].screen_y;
						}

						/* language and flags */
						data[0x64] = user_settings->language;
						data[0x65] = 0xfc;

						/* update count and crc */
						data[0x70] = count & 0xff;
						data[0x71] = (count >> 8) & 0xff;

						crc = calc_CRC16( 0xffff, data, 0x70);
						data[0x72] = crc & 0xff;
						data[0x73] = (crc >> 8) & 0xff;

						memset( &data[0x74], 0xff, 0x100 - 0x74);
}

/* creates an firmware flash image, which contains all needed info to initiate a wifi connection */
int NDS_CreateDummyFirmware( struct NDS_fw_config_data *user_settings)
{
	/*
	* Create the firmware header
	*/
	memset( MMU.fw.data, 0, 0x40000);

	/* firmware identifier */
	MMU.fw.data[0x8] = 'M';
	MMU.fw.data[0x8 + 1] = 'A';
	MMU.fw.data[0x8 + 2] = 'C';
	MMU.fw.data[0x8 + 3] = 'P';

	/* DS type */
	if ( user_settings->ds_type == NDS_FW_DS_TYPE_LITE)
		MMU.fw.data[0x1d] = 0x20;
	else
		MMU.fw.data[0x1d] = 0xff;

	/* User Settings offset 0x3fe00 / 8 */
	MMU.fw.data[0x20] = 0xc0;
	MMU.fw.data[0x21] = 0x7f;


	/*
	* User settings (at 0x3FE00 and 0x3FF00)
	*/
	fill_user_data_area( user_settings, &MMU.fw.data[ 0x3FE00], 0);
	fill_user_data_area( user_settings, &MMU.fw.data[ 0x3FF00], 1);

	/* Wifi config length */
	MMU.fw.data[0x2C] = 0x38;
	MMU.fw.data[0x2D] = 0x01;

	MMU.fw.data[0x2E] = 0x00;

	/* Wifi version */
	MMU.fw.data[0x2F] = 0x00;

	/* MAC address */
	memcpy((MMU.fw.data + 0x36), FW_Mac, sizeof(FW_Mac));

	/* Enabled channels */
	MMU.fw.data[0x3C] = 0xFE;
	MMU.fw.data[0x3D] = 0x3F;

	MMU.fw.data[0x3E] = 0xFF;
	MMU.fw.data[0x3F] = 0xFF;

	/* RF related */
	MMU.fw.data[0x40] = 0x02;
	MMU.fw.data[0x41] = 0x18;
	MMU.fw.data[0x42] = 0x0C;

	MMU.fw.data[0x43] = 0x01;

	/* Wifi I/O init values */
	memcpy((MMU.fw.data + 0x44), FW_WIFIInit, sizeof(FW_WIFIInit));

	/* Wifi BB init values */
	memcpy((MMU.fw.data + 0x64), FW_BBInit, sizeof(FW_BBInit));

	/* Wifi RF init values */
	memcpy((MMU.fw.data + 0xCE), FW_RFInit, sizeof(FW_RFInit));

	/* Wifi channel-related init values */
	memcpy((MMU.fw.data + 0xF2), FW_RFChannel, sizeof(FW_RFChannel));
	memcpy((MMU.fw.data + 0x146), FW_BBChannel, sizeof(FW_BBChannel));
	memset((MMU.fw.data + 0x154), 0x10, 0xE);

	/* WFC profiles */
	memcpy((MMU.fw.data + 0x3FA40), &FW_WFCProfile1, sizeof(FW_WFCProfile));
	memcpy((MMU.fw.data + 0x3FB40), &FW_WFCProfile2, sizeof(FW_WFCProfile));
	memcpy((MMU.fw.data + 0x3FC40), &FW_WFCProfile3, sizeof(FW_WFCProfile));
	(*(u16*)(MMU.fw.data + 0x3FAFE)) = (u16)calc_CRC16(0, (MMU.fw.data + 0x3FA00), 0xFE);
	(*(u16*)(MMU.fw.data + 0x3FBFE)) = (u16)calc_CRC16(0, (MMU.fw.data + 0x3FB00), 0xFE);
	(*(u16*)(MMU.fw.data + 0x3FCFE)) = (u16)calc_CRC16(0, (MMU.fw.data + 0x3FC00), 0xFE);


	MMU.fw.data[0x162] = 0x19;
	memset((MMU.fw.data + 0x163), 0xFF, 0x9D);

	/* Wifi settings CRC16 */
	(*(u16*)(MMU.fw.data + 0x2A)) = calc_CRC16(0, (MMU.fw.data + 0x2C), 0x138);

	return TRUE ;
}

void
NDS_FillDefaultFirmwareConfigData( struct NDS_fw_config_data *fw_config) {
	const char *default_nickname = "yopyop";
	const char *default_message = "DeSmuME makes you happy!";
	int i;
	int str_length;

	memset( fw_config, 0, sizeof( struct NDS_fw_config_data));
	fw_config->ds_type = NDS_FW_DS_TYPE_FAT;

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

	/* default to English */
	fw_config->language = 1;

	/* default touchscreen calibration */
	fw_config->touch_cal[0].adc_x = 0x200;
	fw_config->touch_cal[0].adc_y = 0x200;
	fw_config->touch_cal[0].screen_x = 0x20;
	fw_config->touch_cal[0].screen_y = 0x20;

	fw_config->touch_cal[1].adc_x = 0xe00;
	fw_config->touch_cal[1].adc_y = 0x800;
	fw_config->touch_cal[1].screen_x = 0xe0;
	fw_config->touch_cal[1].screen_y = 0x80;
}

int NDS_LoadFirmware(const char *filename)
{
	int i;
	unsigned long size;
	//FILE *file;

	if ((MMU.fw.fp = fopen(filename, "rb+")) == NULL)
		return -1;

	fseek(MMU.fw.fp, 0, SEEK_END);
	size = ftell(MMU.fw.fp);
	fseek(MMU.fw.fp, 0, SEEK_SET);

	if(size > MMU.fw.size)
	{
		fclose(MMU.fw.fp);
		fw_success = FALSE;
		return -1;
	}

	i = fread(MMU.fw.data, size, 1, MMU.fw.fp);
	//fclose(file);

	INFO("Firmware: decrypting NDS firmware %s...\n", filename);

	if(decryptFirmware(MMU.fw.data) == FALSE)
	{
		INFO("Firmware: decryption failed.\n");
		fw_success = FALSE;
	}
	else
	{
		INFO("Firmware: decryption successful.\n");
		fw_success = TRUE;
	}

	return i;
}

void NDS_Sleep() { nds.sleeping = TRUE; }

bool SkipCur2DFrame = false, SkipNext2DFrame = false;
bool SkipCur3DFrame = false;

void NDS_SkipNextFrame() { SkipNext2DFrame = true; SkipCur3DFrame = true; }

#define INDEX(i) ((((i)>>16)&0xFF0)|(((i)>>4)&0xF))

//#define LOG_ARM9
//#define LOG_ARM7
//static bool dolog = false;


template<bool FORCE>
void NDS_exec(s32 nb)
{
	int i, j;

	LagFrameFlag=1;

	if((currFrameCounter&63) == 0)
		MMU_new.backupDevice.lazy_flush();

	//increase this to execute more instructions in each batch (reducing overhead)
	//the value of 4 seems to optimize speed.. do lower values increase precision? 
	//answer: YES, MAYBE. and after 0.9.3 we need to study it (spider-man)
	const int INSTRUCTIONS_PER_BATCH = 4;

	//decreasing this should increase precision at the cost of speed
	//the traditional value was somewhere between 100 and 400 depending on circumstances
	const int CYCLES_TO_WAIT_FOR_IRQ = 400;

	for(; (nb >= nds.cycles) && ((FORCE)||(execute)); )
	{
		for (j = 0; j < INSTRUCTIONS_PER_BATCH && (!FORCE) && (execute); j++)
		{
			if(nds.ARM9Cycle<=nds.cycles)
			{
				for (i = 0; i < INSTRUCTIONS_PER_BATCH && (!FORCE) && (execute); i++)
				{
					if(nds.sleeping) {
						break;
					} else
					if(NDS_ARM9.waitIRQ) {
						nds.ARM9Cycle += CYCLES_TO_WAIT_FOR_IRQ;
						nds.idleCycles += CYCLES_TO_WAIT_FOR_IRQ;
						break; //it is rather pointless to do this more than once
					} else {
						#ifdef LOG_ARM9
						if(dolog)
						{
							char dasmbuf[1024];
							char logbuf[4096];
							if(NDS_ARM9.CPSR.bits.T)
								des_thumb_instructions_set[((NDS_ARM9.instruction)>>6)&1023](NDS_ARM9.instruct_adr, NDS_ARM9.instruction, dasmbuf);
							else
								des_arm_instructions_set[INDEX(NDS_ARM9.instruction)](NDS_ARM9.instruct_adr, NDS_ARM9.instruction, dasmbuf);
		
							printf("%05d %08d 9:%08X %08X %-30s %08X R00:%08X R01:%08X R02:%08X R03:%08X R04:%08X R05:%08X R06:%08X R07:%08X R08:%08X R09:%08X R10:%08X R11:%08X R12:%08X R13:%08X R14:%08X R15:%08X\n",
								currFrameCounter, nds.cycles, 
								NDS_ARM9.instruct_adr,NDS_ARM9.instruction, dasmbuf, 
								NDS_ARM9.R[0],  NDS_ARM9.R[1],  NDS_ARM9.R[2],  NDS_ARM9.R[3],  NDS_ARM9.R[4],  NDS_ARM9.R[5],  NDS_ARM9.R[6],  NDS_ARM9.R[7], 
								NDS_ARM9.R[8],  NDS_ARM9.R[9],  NDS_ARM9.R[10],  NDS_ARM9.R[11],  NDS_ARM9.R[12],  NDS_ARM9.R[13],  NDS_ARM9.R[14],  NDS_ARM9.R[15]);  
						}
						#endif

						bool r7zero = NDS_ARM9.R[7] == 0;
						nds.ARM9Cycle += armcpu_exec<ARMCPU_ARM9>();
						bool r7one = NDS_ARM9.R[7] == 1;
					}
				}
#ifdef _WIN32
#ifdef DEVELOPER
				DisassemblerTools_Refresh<ARMCPU_ARM9>();
#endif
#endif
			}

#ifdef EXPERIMENTAL_WIFI
#if 0

			if((nds.ARM7Cycle % 0x3F03) == 0)
			{
				/* 3F03 arm7 cycles = ~1usec */
				// WRONG!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
				// 1 usec = ~33 ARM7 cycles because ARM7 runs at 33.51 mhz
				// 33.51 mhz = 33.51 million cycles per second = 33.51 cycles per usec
				WIFI_usTrigger(&wifiMac) ;
			}
#endif

			int nds7old = nds.ARM7Cycle;
#endif

			if(nds.ARM7Cycle<=nds.cycles)
			{
				for (i = 0; i < INSTRUCTIONS_PER_BATCH && (!FORCE) && (execute); i++)
				{
					if(nds.sleeping) {
						break;
					} else
					if(NDS_ARM7.waitIRQ)
					{
						nds.ARM7Cycle += CYCLES_TO_WAIT_FOR_IRQ;
						break; //it is rather pointless to do this more than once
					}
					else
					{
						#ifdef LOG_ARM7
						if(dolog)
						{
							char dasmbuf[1024];
							if(NDS_ARM7.CPSR.bits.T)
								des_thumb_instructions_set[((NDS_ARM7.instruction)>>6)&1023](NDS_ARM7.instruct_adr, NDS_ARM7.instruction, dasmbuf);
							else
								des_arm_instructions_set[INDEX(NDS_ARM7.instruction)](NDS_ARM7.instruct_adr, NDS_ARM7.instruction, dasmbuf);
							printf("%05d %08d 7:%08X %08X %-30s %08X R00:%08X R01:%08X R02:%08X R03:%08X R04:%08X R05:%08X R06:%08X R07:%08X R08:%08X R09:%08X R10:%08X R11:%08X R12:%08X R13:%08X R14:%08X R15:%08X\n",
								currFrameCounter, nds.cycles, 
								NDS_ARM7.instruct_adr,NDS_ARM7.instruction, dasmbuf, 
								NDS_ARM7.R[0],  NDS_ARM7.R[1],  NDS_ARM7.R[2],  NDS_ARM7.R[3],  NDS_ARM7.R[4],  NDS_ARM7.R[5],  NDS_ARM7.R[6],  NDS_ARM7.R[7], 
								NDS_ARM7.R[8],  NDS_ARM7.R[9],  NDS_ARM7.R[10],  NDS_ARM7.R[11],  NDS_ARM7.R[12],  NDS_ARM7.R[13],  NDS_ARM7.R[14],  NDS_ARM7.R[15],
								NDS_ARM7.CPSR, NDS_ARM7.SPSR);  
						}
						#endif
						nds.ARM7Cycle += (armcpu_exec<ARMCPU_ARM7>()<<1);
					}
				}
#ifdef _WIN32
#ifdef DEVELOPER
				DisassemblerTools_Refresh<ARMCPU_ARM7>();
#endif
#endif
			}

//this doesnt work anyway. why take a speed hit for public releases?
// Luigi__: I don't agree. We should start include wifi emulation in public releases
// because it already allows games to not hang during wifi operations
// and thus games that perform wifi checks upon boot shouldn't hang with wifi
// emulation turned on.
#ifndef PUBLIC_RELEASE
#ifdef EXPERIMENTAL_WIFI
			// FIXME: the wifi stuff isn't actually clocked by the ARM7 clock, 
			// but by a 22 mhz oscillator.
			// But do we really care? while we're triggering wifi core every microsecond,
			// everything is fine, isn't it?

			//zeromus way: i did this in a rush and it is not perfect, but it is more like what it needs to be
			//nds.wifiCycle += (nds.ARM7Cycle-nds7old)<<16;
			//while(nds.wifiCycle > 0)
			//{
			//	nds.wifiCycle -= 946453; //22*22000000*65536/ARM7_CLOCK;
			//	WIFI_usTrigger(&wifiMac);
			//	WIFI_SoftAP_usTrigger(&wifiMac);
			//}
			//
			//luigi's way>
			// new one, more like zeromus way
			// but I can't understand why it uses that value (22*22000000*65536/ARM7_CLOCK)
			nds.wifiCycle += (nds.ARM7Cycle - nds7old) << 16;
			while (nds.wifiCycle > 0)
			{
				// 2196372 ~= (ARM7_CLOCK << 16) / 1000000
				// This value makes more sense to me, because:
				// ARM7_CLOCK   = 33.51 mhz
				//				= 33513982 cycles per second
				// 				= 33.513982 cycles per microsecond
				// The value is left shifted by 16 for more precision.
				nds.wifiCycle -= (2196372 << 1);

				WIFI_usTrigger(&wifiMac);
				WIFI_SoftAP_usTrigger(&wifiMac);
			}
#endif
#endif
		} // for (j = 0; j < INSTRUCTIONS_PER_BATCH && (!FORCE) && (execute); j++)

		if(!nds.sleeping)
		{

		//	for(i = 0; i < INSTRUCTIONS_PER_BATCH && (!FORCE) && (execute); i++)
		//	{

		nds.cycles = std::min(nds.ARM9Cycle,nds.ARM7Cycle);

		//debug();

		if(nds.cycles>=nds.nextHBlank)
		{
			if(!nds.lignerendu)
			{
				T1WriteWord(ARM9Mem.ARM9_REG, 4, T1ReadWord(ARM9Mem.ARM9_REG, 4) | 2);
				T1WriteWord(MMU.ARM7_REG, 4, T1ReadWord(MMU.ARM7_REG, 4) | 2);
				NDS_ARM9HBlankInt();
				NDS_ARM7HBlankInt();
				SPU_Emulate_core();
				driver->AVI_SoundUpdate(SPU_core->outbuf,spu_core_samples);
				WAV_WavSoundUpdate(SPU_core->outbuf,spu_core_samples);
				if(nds.VCount<192)
				{
					if(!SkipCur2DFrame)
					{
						GPU_ligne(&MainScreen, nds.VCount);
						GPU_ligne(&SubScreen, nds.VCount);
					}

					if(MMU.DMAStartTime[0][0] == 2)
						MMU_doDMA<ARMCPU_ARM9>(0);
					if(MMU.DMAStartTime[0][1] == 2)
						MMU_doDMA<ARMCPU_ARM9>(1);
					if(MMU.DMAStartTime[0][2] == 2)
						MMU_doDMA<ARMCPU_ARM9>(2);
					if(MMU.DMAStartTime[0][3] == 2)
						MMU_doDMA<ARMCPU_ARM9>(3);
				}
				nds.lignerendu = TRUE;
			}
			if(nds.cycles>=nds.nextHBlank+1092)
			{
				u32 vmatch;

				++nds.VCount;
				nds.nextHBlank += 4260;
				T1WriteWord(ARM9Mem.ARM9_REG, 4, T1ReadWord(ARM9Mem.ARM9_REG, 4) & 0xFFFD);
				T1WriteWord(MMU.ARM7_REG, 4, T1ReadWord(MMU.ARM7_REG, 4) & 0xFFFD);

				if(MMU.DMAStartTime[0][0] == 3)
					MMU_doDMA<ARMCPU_ARM9>(0);
				if(MMU.DMAStartTime[0][1] == 3)
					MMU_doDMA<ARMCPU_ARM9>(1);
				if(MMU.DMAStartTime[0][2] == 3)
					MMU_doDMA<ARMCPU_ARM9>(2);
				if(MMU.DMAStartTime[0][3] == 3)
					MMU_doDMA<ARMCPU_ARM9>(3);

				// Main memory display
				if(MMU.DMAStartTime[0][0] == 4)
				{
					MMU_doDMA<ARMCPU_ARM9>(0);
					MMU.DMAStartTime[0][0] = 0;
				}
				if(MMU.DMAStartTime[0][1] == 4)
				{
					MMU_doDMA<ARMCPU_ARM9>(1);
					MMU.DMAStartTime[0][1] = 0;
				}
				if(MMU.DMAStartTime[0][2] == 4)
				{
					MMU_doDMA<ARMCPU_ARM9>(2);
					MMU.DMAStartTime[0][2] = 0;
				}
				if(MMU.DMAStartTime[0][3] == 4)
				{
					MMU_doDMA<ARMCPU_ARM9>(3);
					MMU.DMAStartTime[0][3] = 0;
				}

				if(MMU.DMAStartTime[1][0] == 4)
				{
					MMU_doDMA<ARMCPU_ARM7>(0);
					MMU.DMAStartTime[1][0] = 0;
				}
				if(MMU.DMAStartTime[1][1] == 4)
				{
					MMU_doDMA<ARMCPU_ARM7>(1);
					MMU.DMAStartTime[0][1] = 0;
				}
				if(MMU.DMAStartTime[1][2] == 4)
				{
					MMU_doDMA<ARMCPU_ARM7>(2);
					MMU.DMAStartTime[1][2] = 0;
				}
				if(MMU.DMAStartTime[1][3] == 4)
				{
					MMU_doDMA<ARMCPU_ARM7>(3);
					MMU.DMAStartTime[1][3] = 0;
				}

				nds.lignerendu = FALSE;
				if(nds.VCount==192)
				{
					//osdA->update();
					gfx3d_VBlankSignal();

					if(SkipNext2DFrame)
					{
						SkipCur2DFrame = true;
						SkipNext2DFrame = false;
					}
					else
						SkipCur2DFrame = false;

					T1WriteWord(ARM9Mem.ARM9_REG, 4, T1ReadWord(ARM9Mem.ARM9_REG, 4) | 1);
					T1WriteWord(MMU.ARM7_REG, 4, T1ReadWord(MMU.ARM7_REG, 4) | 1);
					NDS_ARM9VBlankInt();
					NDS_ARM7VBlankInt();
					cheatsProcess();

					nds.runCycleCollector[nds.idleFrameCounter] = 1120380-nds.idleCycles;
					nds.idleFrameCounter++;
					nds.idleFrameCounter &= 15;
					nds.idleCycles = 0;


					if(MMU.DMAStartTime[0][0] == 1)
						MMU_doDMA<ARMCPU_ARM9>(0);
					if(MMU.DMAStartTime[0][1] == 1)
						MMU_doDMA<ARMCPU_ARM9>(1);
					if(MMU.DMAStartTime[0][2] == 1)
						MMU_doDMA<ARMCPU_ARM9>(2);
					if(MMU.DMAStartTime[0][3] == 1)
						MMU_doDMA<ARMCPU_ARM9>(3);

					if(MMU.DMAStartTime[1][0] == 1)
						MMU_doDMA<ARMCPU_ARM7>(0);
					if(MMU.DMAStartTime[1][1] == 1)
						MMU_doDMA<ARMCPU_ARM7>(1);
					if(MMU.DMAStartTime[1][2] == 1)
						MMU_doDMA<ARMCPU_ARM7>(2);
					if(MMU.DMAStartTime[1][3] == 1)
						MMU_doDMA<ARMCPU_ARM7>(3);
				}
				else if(nds.VCount==214)
				{
					gfx3d_VBlankEndSignal(SkipCur3DFrame);
					SkipCur3DFrame = false;
				}
				else if(nds.VCount==263)
				{
					//osd->update();
					//osdB->update();
					gpu_UpdateRender();

					nds.nextHBlank = 3168;
					nds.VCount = 0;
					T1WriteWord(ARM9Mem.ARM9_REG, 4, T1ReadWord(ARM9Mem.ARM9_REG, 4) & 0xFFFE);
					T1WriteWord(MMU.ARM7_REG, 4, T1ReadWord(MMU.ARM7_REG, 4) & 0xFFFE);

					nds.cycles -= (560190<<1);
					nds.ARM9Cycle -= (560190<<1);
					nds.ARM7Cycle -= (560190<<1);
					nb -= (560190<<1);

					if(MMU.divRunning)
					{
						MMU.divCycles -= (560190 << 1);
					}

					if(MMU.sqrtRunning)
					{
						MMU.sqrtCycles -= (560190 << 1);
					}

					if (MMU.CheckTimers)
					{
						if(MMU.timerON[0][0])	nds.timerCycle[0][0] -= (560190<<1);
						if(MMU.timerON[0][1])	nds.timerCycle[0][1] -= (560190<<1);
						if(MMU.timerON[0][2])	nds.timerCycle[0][2] -= (560190<<1);
						if(MMU.timerON[0][3])	nds.timerCycle[0][3] -= (560190<<1);

						if(MMU.timerON[1][0])	nds.timerCycle[1][0] -= (560190<<1);
						if(MMU.timerON[1][1])	nds.timerCycle[1][1] -= (560190<<1);
						if(MMU.timerON[1][2])	nds.timerCycle[1][2] -= (560190<<1);
						if(MMU.timerON[1][3])	nds.timerCycle[1][3] -= (560190<<1);
					}

					if (MMU.CheckDMAs)
					{
						if(MMU.DMAing[0][0])	MMU.DMACycle[0][0] -= (560190<<1);
						if(MMU.DMAing[0][1])	MMU.DMACycle[0][1] -= (560190<<1);
						if(MMU.DMAing[0][2])	MMU.DMACycle[0][2] -= (560190<<1);
						if(MMU.DMAing[0][3])	MMU.DMACycle[0][3] -= (560190<<1);

						if(MMU.DMAing[1][0])	MMU.DMACycle[1][0] -= (560190<<1);
						if(MMU.DMAing[1][1])	MMU.DMACycle[1][1] -= (560190<<1);
						if(MMU.DMAing[1][2])	MMU.DMACycle[1][2] -= (560190<<1);
						if(MMU.DMAing[1][3])	MMU.DMACycle[1][3] -= (560190<<1);
					}
				}

				T1WriteWord(ARM9Mem.ARM9_REG, 6, nds.VCount);
				T1WriteWord(ARM9Mem.ARM9_REG, 0x1006, nds.VCount);
				T1WriteWord(MMU.ARM7_REG, 6, nds.VCount);
				T1WriteWord(MMU.ARM7_REG, 0x1006, nds.VCount);

				vmatch = T1ReadWord(ARM9Mem.ARM9_REG, 4);
				if(nds.VCount==((vmatch>>8)|((vmatch<<1)&(1<<8))))
				{
					T1WriteWord(ARM9Mem.ARM9_REG, 4, T1ReadWord(ARM9Mem.ARM9_REG, 4) | 4);
					if(T1ReadWord(ARM9Mem.ARM9_REG, 4) & 32)
						NDS_makeARM9Int(2);
				}
				else
					T1WriteWord(ARM9Mem.ARM9_REG, 4, T1ReadWord(ARM9Mem.ARM9_REG, 4) & 0xFFFB);

				vmatch = T1ReadWord(MMU.ARM7_REG, 4);
				if(nds.VCount==((vmatch>>8)|((vmatch<<1)&(1<<8))))
				{
					T1WriteWord(MMU.ARM7_REG, 4, T1ReadWord(MMU.ARM7_REG, 4) | 4);
					if(T1ReadWord(MMU.ARM7_REG, 4) & 32)
						NDS_makeARM7Int(2);
				}
				else
					T1WriteWord(MMU.ARM7_REG, 4, T1ReadWord(MMU.ARM7_REG, 4) & 0xFFFB);
			}
		} // if(nds.cycles >= nds.nextHBlank)

	/*	} // for(i = 0; i < INSTRUCTIONS_PER_BATCH && (!FORCE) && (execute); i++)

		} // if(!nds.sleeping)

		} // for (j = 0; j < INSTRUCTIONS_PER_BATCH && (!FORCE) && (execute); j++)

		if(!nds.sleeping)
		{*/

		if(MMU.divRunning)
		{
			if(nds.cycles > MMU.divCycles)
			{
				T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x2A0, (u32)MMU.divResult);
				T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x2A4, (u32)(MMU.divResult >> 32));
				T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x2A8, (u32)MMU.divMod);
				T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x2AC, (u32)(MMU.divMod >> 32));
				T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x280, MMU.divCnt);

				MMU.divRunning = FALSE;
			}
		}

		if(MMU.sqrtRunning)
		{
			if(nds.cycles > MMU.sqrtCycles)
			{
				T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x2B4, MMU.sqrtResult);
				T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x2B0, MMU.sqrtCnt);

				MMU.sqrtRunning = FALSE;
			}
		}

		if (MMU.CheckTimers)
		{
			/* assume the timers have not expired */
			nds.timerOver[0][0] = 0;
			nds.timerOver[0][1] = 0;
			nds.timerOver[0][2] = 0;
			nds.timerOver[0][3] = 0;
			nds.timerOver[1][0] = 0;
			nds.timerOver[1][1] = 0;
			nds.timerOver[1][2] = 0;
			nds.timerOver[1][3] = 0;
			if(MMU.timerON[0][0])
			{
				if(MMU.timerRUN[0][0])
				{
					switch(MMU.timerMODE[0][0])
					{
					case 0xFFFF :
						break;
					default :
						{
							nds.diff = (nds.cycles - nds.timerCycle[0][0])>>MMU.timerMODE[0][0];
							nds.old = MMU.timer[0][0];
							MMU.timer[0][0] += nds.diff;
							nds.timerCycle[0][0] += (nds.diff << MMU.timerMODE[0][0]);
							nds.timerOver[0][0] = nds.old>MMU.timer[0][0];
							if(nds.timerOver[0][0])
							{
								if(T1ReadWord(ARM9Mem.ARM9_REG, 0x102) & 0x40)
									NDS_makeARM9Int(3);
								MMU.timer[0][0] = MMU.timerReload[0][0];
							}
						}
						break;
					}
				}
				else
				{
					MMU.timerRUN[0][0] = TRUE;
					nds.timerCycle[0][0] = nds.cycles;
				}
			}
			if(MMU.timerON[0][1])
			{
				if(MMU.timerRUN[0][1])
				{
					switch(MMU.timerMODE[0][1])
					{
					case 0xFFFF :
						if(nds.timerOver[0][0])
						{
							++(MMU.timer[0][1]);
							nds.timerOver[0][1] = !MMU.timer[0][1];
							if (nds.timerOver[0][1])
							{
								if(T1ReadWord(ARM9Mem.ARM9_REG, 0x106) & 0x40)
									NDS_makeARM9Int(4);
								MMU.timer[0][1] = MMU.timerReload[0][1];
							}
						}
						break;
					default :
						{
							nds.diff = (nds.cycles - nds.timerCycle[0][1])>>MMU.timerMODE[0][1];
							nds.old = MMU.timer[0][1];
							MMU.timer[0][1] += nds.diff;
							nds.timerCycle[0][1] += nds.diff << MMU.timerMODE[0][1];
							nds.timerOver[0][1] = nds.old>MMU.timer[0][1];
							if(nds.timerOver[0][1])
							{
								if(T1ReadWord(ARM9Mem.ARM9_REG, 0x106) & 0x40)
									NDS_makeARM9Int(4);
								MMU.timer[0][1] = MMU.timerReload[0][1];
							}
						}
						break;

					}
				}
				else
				{
					MMU.timerRUN[0][1] = TRUE;
					nds.timerCycle[0][1] = nds.cycles;
				}
			}
			if(MMU.timerON[0][2])
			{
				if(MMU.timerRUN[0][2])
				{
					switch(MMU.timerMODE[0][2])
					{
					case 0xFFFF :
						if(nds.timerOver[0][1])
						{
							++(MMU.timer[0][2]);
							nds.timerOver[0][2] = !MMU.timer[0][2];
							if (nds.timerOver[0][2])
							{
								if(T1ReadWord(ARM9Mem.ARM9_REG, 0x10A) & 0x40)
									NDS_makeARM9Int(5);
								MMU.timer[0][2] = MMU.timerReload[0][2];
							}
						}
						break;
					default :
						{
							nds.diff = (nds.cycles - nds.timerCycle[0][2])>>MMU.timerMODE[0][2];
							nds.old = MMU.timer[0][2];
							MMU.timer[0][2] += nds.diff;
							nds.timerCycle[0][2] += nds.diff << MMU.timerMODE[0][2];
							nds.timerOver[0][2] = nds.old>MMU.timer[0][2];
							if(nds.timerOver[0][2])
							{
								if(T1ReadWord(ARM9Mem.ARM9_REG, 0x10A) & 0x40)
									NDS_makeARM9Int(5);
								MMU.timer[0][2] = MMU.timerReload[0][2];
							}
						}
						break;
					}
				}
				else
				{
					MMU.timerRUN[0][2] = TRUE;
					nds.timerCycle[0][2] = nds.cycles;
				}
			}
			if(MMU.timerON[0][3])
			{
				if(MMU.timerRUN[0][3])
				{
					switch(MMU.timerMODE[0][3])
					{
					case 0xFFFF :
						if(nds.timerOver[0][2])
						{
							++(MMU.timer[0][3]);
							nds.timerOver[0][3] = !MMU.timer[0][3];
							if (nds.timerOver[0][3])
							{
								if(T1ReadWord(ARM9Mem.ARM9_REG, 0x10E) & 0x40)
									NDS_makeARM9Int(6);
								MMU.timer[0][3] = MMU.timerReload[0][3];
							}
						}
						break;
					default :
						{
							nds.diff = (nds.cycles - nds.timerCycle[0][3])>>MMU.timerMODE[0][3];
							nds.old = MMU.timer[0][3];
							MMU.timer[0][3] += nds.diff;
							nds.timerCycle[0][3] += nds.diff << MMU.timerMODE[0][3];
							nds.timerOver[0][3] = nds.old>MMU.timer[0][3];
							if(nds.timerOver[0][3])
							{
								if(T1ReadWord(ARM9Mem.ARM9_REG, 0x10E) & 0x40)
									NDS_makeARM9Int(6);
								MMU.timer[0][3] = MMU.timerReload[0][3];
							}
						}
						break;
					}
				}
				else
				{
					MMU.timerRUN[0][3] = TRUE;
					nds.timerCycle[0][3] = nds.cycles;
				}
			}

			if(MMU.timerON[1][0])
			{
				if(MMU.timerRUN[1][0])
				{
					switch(MMU.timerMODE[1][0])
					{
					case 0xFFFF :
						break;
					default :
						{
							nds.diff = (nds.cycles - nds.timerCycle[1][0])>>MMU.timerMODE[1][0];
							nds.old = MMU.timer[1][0];
							MMU.timer[1][0] += nds.diff;
							nds.timerCycle[1][0] += nds.diff << MMU.timerMODE[1][0];
							nds.timerOver[1][0] = nds.old>MMU.timer[1][0];
							if(nds.timerOver[1][0])
							{
								if(T1ReadWord(MMU.ARM7_REG, 0x102) & 0x40)
									NDS_makeARM7Int(3);
								MMU.timer[1][0] = MMU.timerReload[1][0];
							}
						}
						break;
					}
				}
				else
				{
					MMU.timerRUN[1][0] = TRUE;
					nds.timerCycle[1][0] = nds.cycles;
				}
			}
			if(MMU.timerON[1][1])
			{
				if(MMU.timerRUN[1][1])
				{
					switch(MMU.timerMODE[1][1])
					{
					case 0xFFFF :
						if(nds.timerOver[1][0])
						{
							++(MMU.timer[1][1]);
							nds.timerOver[1][1] = !MMU.timer[1][1];
							if (nds.timerOver[1][1])
							{
								if(T1ReadWord(MMU.ARM7_REG, 0x106) & 0x40)
									NDS_makeARM7Int(4);
								MMU.timer[1][1] = MMU.timerReload[1][1];
							}
						}
						break;
					default :
						{
							nds.diff = (nds.cycles - nds.timerCycle[1][1])>>MMU.timerMODE[1][1];
							nds.old = MMU.timer[1][1];
							MMU.timer[1][1] += nds.diff;
							nds.timerCycle[1][1] += nds.diff << MMU.timerMODE[1][1];
							nds.timerOver[1][1] = nds.old>MMU.timer[1][1];
							if(nds.timerOver[1][1])
							{
								if(T1ReadWord(MMU.ARM7_REG, 0x106) & 0x40)
									NDS_makeARM7Int(4);
								MMU.timer[1][1] = MMU.timerReload[1][1];
							}
						}
						break;
					}
				}
				else
				{
					MMU.timerRUN[1][1] = TRUE;
					nds.timerCycle[1][1] = nds.cycles;
				}
			}
			if(MMU.timerON[1][2])
			{
				if(MMU.timerRUN[1][2])
				{
					switch(MMU.timerMODE[1][2])
					{
					case 0xFFFF :
						if(nds.timerOver[1][1])
						{
							++(MMU.timer[1][2]);
							nds.timerOver[1][2] = !MMU.timer[1][2];
							if (nds.timerOver[1][2])
							{
								if(T1ReadWord(MMU.ARM7_REG, 0x10A) & 0x40)
									NDS_makeARM7Int(5);
								MMU.timer[1][2] = MMU.timerReload[1][2];
							}
						}
						break;
					default :
						{
							nds.diff = (nds.cycles - nds.timerCycle[1][2])>>MMU.timerMODE[1][2];
							nds.old = MMU.timer[1][2];
							MMU.timer[1][2] += nds.diff;
							nds.timerCycle[1][2] += nds.diff << MMU.timerMODE[1][2];
							nds.timerOver[1][2] = nds.old>MMU.timer[1][2];
							if(nds.timerOver[1][2])
							{
								if(T1ReadWord(MMU.ARM7_REG, 0x10A) & 0x40)
									NDS_makeARM7Int(5);
								MMU.timer[1][2] = MMU.timerReload[1][2];
							}
						}
						break;
					}
				}
				else
				{
					MMU.timerRUN[1][2] = TRUE;
					nds.timerCycle[1][2] = nds.cycles;
				}
			}
			if(MMU.timerON[1][3])
			{
				if(MMU.timerRUN[1][3])
				{
					switch(MMU.timerMODE[1][3])
					{
					case 0xFFFF :
						if(nds.timerOver[1][2])
						{
							++(MMU.timer[1][3]);
							nds.timerOver[1][3] = !MMU.timer[1][3];
							if (nds.timerOver[1][3])
							{
								if(T1ReadWord(MMU.ARM7_REG, 0x10E) & 0x40)
									NDS_makeARM7Int(6);
								MMU.timer[1][3] += MMU.timerReload[1][3];
							}
						}
						break;
					default :
						{
							nds.diff = (nds.cycles - nds.timerCycle[1][3])>>MMU.timerMODE[1][3];
							nds.old = MMU.timer[1][3];
							MMU.timer[1][3] += nds.diff;
							nds.timerCycle[1][3] += nds.diff << MMU.timerMODE[1][3];
							nds.timerOver[1][3] = nds.old>MMU.timer[1][3];
							if(nds.timerOver[1][3])
							{
								if(T1ReadWord(MMU.ARM7_REG, 0x10E) & 0x40)
									NDS_makeARM7Int(6);
								MMU.timer[1][3] += MMU.timerReload[1][3];
							}
						}
						break;
					}
				}
				else
				{
					MMU.timerRUN[1][3] = TRUE;
					nds.timerCycle[1][3] = nds.cycles;
				}
			}
		}

		if (MMU.CheckDMAs)
		{

			if((MMU.DMAing[0][0])&&(MMU.DMACycle[0][0]<=nds.cycles))
			{
				T1WriteLong(ARM9Mem.ARM9_REG, 0xB8 + (0xC*0), T1ReadLong(ARM9Mem.ARM9_REG, 0xB8 + (0xC*0)) & 0x7FFFFFFF);
				if((MMU.DMACrt[0][0])&(1<<30)) NDS_makeARM9Int(8);
				MMU.DMAing[0][0] = FALSE;
				MMU.CheckDMAs &= ~(1<<(0+(0<<2)));
			}

			if((MMU.DMAing[0][1])&&(MMU.DMACycle[0][1]<=nds.cycles))
			{
				T1WriteLong(ARM9Mem.ARM9_REG, 0xB8 + (0xC*1), T1ReadLong(ARM9Mem.ARM9_REG, 0xB8 + (0xC*1)) & 0x7FFFFFFF);
				if((MMU.DMACrt[0][1])&(1<<30)) NDS_makeARM9Int(9);
				MMU.DMAing[0][1] = FALSE;
				MMU.CheckDMAs &= ~(1<<(1+(0<<2)));
			}

			if((MMU.DMAing[0][2])&&(MMU.DMACycle[0][2]<=nds.cycles))
			{
				T1WriteLong(ARM9Mem.ARM9_REG, 0xB8 + (0xC*2), T1ReadLong(ARM9Mem.ARM9_REG, 0xB8 + (0xC*2)) & 0x7FFFFFFF);
				if((MMU.DMACrt[0][2])&(1<<30)) NDS_makeARM9Int(10);
				MMU.DMAing[0][2] = FALSE;
				MMU.CheckDMAs &= ~(1<<(2+(0<<2)));
			}

			if((MMU.DMAing[0][3])&&(MMU.DMACycle[0][3]<=nds.cycles))
			{
				T1WriteLong(ARM9Mem.ARM9_REG, 0xB8 + (0xC*3), T1ReadLong(ARM9Mem.ARM9_REG, 0xB8 + (0xC*3)) & 0x7FFFFFFF);
				if((MMU.DMACrt[0][3])&(1<<30)) NDS_makeARM9Int(11);
				MMU.DMAing[0][3] = FALSE;
				MMU.CheckDMAs &= ~(1<<(3+(0<<2)));
			}

			if((MMU.DMAing[1][0])&&(MMU.DMACycle[1][0]<=nds.cycles))
			{
				T1WriteLong(MMU.ARM7_REG, 0xB8 + (0xC*0), T1ReadLong(MMU.ARM7_REG, 0xB8 + (0xC*0)) & 0x7FFFFFFF);
				if((MMU.DMACrt[1][0])&(1<<30)) NDS_makeARM7Int(8);
				MMU.DMAing[1][0] = FALSE;
				MMU.CheckDMAs &= ~(1<<(0+(1<<2)));
			}

			if((MMU.DMAing[1][1])&&(MMU.DMACycle[1][1]<=nds.cycles))
			{
				T1WriteLong(MMU.ARM7_REG, 0xB8 + (0xC*1), T1ReadLong(MMU.ARM7_REG, 0xB8 + (0xC*1)) & 0x7FFFFFFF);
				if((MMU.DMACrt[1][1])&(1<<30)) NDS_makeARM7Int(9);
				MMU.DMAing[1][1] = FALSE;
				MMU.CheckDMAs &= ~(1<<(1+(1<<2)));
			}

			if((MMU.DMAing[1][2])&&(MMU.DMACycle[1][2]<=nds.cycles))
			{
				T1WriteLong(MMU.ARM7_REG, 0xB8 + (0xC*2), T1ReadLong(MMU.ARM7_REG, 0xB8 + (0xC*2)) & 0x7FFFFFFF);
				if((MMU.DMACrt[1][2])&(1<<30)) NDS_makeARM7Int(10);
				MMU.DMAing[1][2] = FALSE;
				MMU.CheckDMAs &= ~(1<<(2+(1<<2)));
			}

			if((MMU.DMAing[1][3])&&(MMU.DMACycle[1][3]<=nds.cycles))
			{
				T1WriteLong(MMU.ARM7_REG, 0xB8 + (0xC*3), T1ReadLong(MMU.ARM7_REG, 0xB8 + (0xC*3)) & 0x7FFFFFFF);
				if((MMU.DMACrt[1][3])&(1<<30)) NDS_makeARM7Int(11);
				MMU.DMAing[1][3] = FALSE;
				MMU.CheckDMAs &= ~(1<<(3+(1<<2)));
			}
		}

		if(MMU.reg_IE[ARMCPU_ARM9]&(1<<21))
			NDS_makeARM9Int(21);		// GX geometry

		if((MMU.reg_IF[0]&MMU.reg_IE[0]) && (MMU.reg_IME[0]))
		{
#ifdef GDB_STUB
			if ( armcpu_flagIrq( &NDS_ARM9)) 
#else
			if ( armcpu_irqException(&NDS_ARM9))
#endif
			{
				nds.ARM9Cycle = nds.cycles;
			}
		}

		if((MMU.reg_IF[1]&MMU.reg_IE[1]) && (MMU.reg_IME[1]))
		{
#ifdef GDB_STUB
			if ( armcpu_flagIrq( &NDS_ARM7)) 
#else
			if ( armcpu_irqException(&NDS_ARM7))
#endif
			{
				nds.ARM7Cycle = nds.cycles;
			}
		}

		} // if(!nds.sleeping)
		else
		{
			gpu_UpdateRender();
			if((MMU.reg_IE[1] & MMU.reg_IF[1]) & (1<<22))
			{
				nds.sleeping = FALSE;
			}
			break;
		}
	}

	if(LagFrameFlag)
	{
		lagframecounter++;
		TotalLagFrames++;
	}
	else 
	{
		lastLag = lagframecounter;
		lagframecounter = 0;
	}

	currFrameCounter++;
}

static std::string MakeInputDisplayString(u16 pad, const std::string* Buttons, int count) {
    std::string s;
    for (int x = 0; x < count; x++) {
        if (pad & (1 << x))
            s.append(Buttons[x].size(), ' '); 
        else
            s += Buttons[x];
    }
    return s;
}

static std::string MakeInputDisplayString(u16 pad, u16 padExt) {
    const std::string Buttons[] = {"A", "B", "Sl", "St", "R", "L", "U", "D", "Rs", "Ls"};
    const std::string Ext[] = {"X", "Y"};

    std::string s = MakeInputDisplayString(pad, Ext, ARRAY_SIZE(Ext));
    s += MakeInputDisplayString(padExt, Buttons, ARRAY_SIZE(Buttons));

    return s;
}

void ClearAutoHold(void) {
	
	for (int i=0; i < 12; i++) {
		AutoHold.hold(i)=false;
	}
}

void NDS_setPadFromMovie(u16 pad)
{
#define FIX(b,n) (((pad>>n)&1)!=0)
	NDS_setPad(
		FIX(pad,12), //R
		FIX(pad,11), //L
		FIX(pad,10), //D
		FIX(pad,9), //U 
		FIX(pad,7), //Select
		FIX(pad,8), //Start
		FIX(pad,6), //B
		FIX(pad,5), //A
		FIX(pad,4), //Y
		FIX(pad,3), //X
		FIX(pad,2),
		FIX(pad,1),
		FIX(pad,0),
		movie_lid
		);
#undef FIX

}

turbo Turbo;
turbotime TurboTime;

static void SetTurbo(bool (&pad) [12]) {

	bool turbo[4] = {true, false, true, false};
	bool currentbutton;

	for (int i=0; i < 12; i++) {
		currentbutton=Turbo.button(i);

		if(currentbutton) {
			pad[i]=turbo[TurboTime.time(i)-1];
			
			if(TurboTime.time(i) >= (int)ARRAY_SIZE(turbo))
				TurboTime.time(i)=0;
		}
		else
			TurboTime.time(i)=0; //reset timer if the button isn't pressed
	}
	for (int i=0; i<12; i++)
		TurboTime.time(i)++;
}

autohold AutoHold;

void NDS_setPad(bool R,bool L,bool D,bool U,bool T,bool S,bool B,bool A,bool Y,bool X,bool W,bool E,bool G, bool F)
{

	bool padarray[12] = {R, L, D, U, T, S, B, A, Y, X, W, E};

	SetTurbo(padarray);

	R=padarray[0];
	L=padarray[1];
	D=padarray[2];
	U=padarray[3];
	T=padarray[4];
	S=padarray[5];
	B=padarray[6];
	A=padarray[7];
	Y=padarray[8];
	X=padarray[9];
	W=padarray[10];
	E=padarray[11];

	if(AutoHold.Right) R=!padarray[0];
	if(AutoHold.Left)  L=!padarray[1];
	if(AutoHold.Down)  D=!padarray[2];
	if(AutoHold.Up)    U=!padarray[3];
	if(AutoHold.Select)T=!padarray[4];
	if(AutoHold.Start) S=!padarray[5];
	if(AutoHold.B)     B=!padarray[6];
	if(AutoHold.A)     A=!padarray[7];
	if(AutoHold.Y)     Y=!padarray[8];
	if(AutoHold.X)     X=!padarray[9];
	if(AutoHold.L)     W=!padarray[10];
	if(AutoHold.R)     E=!padarray[11];

	//this macro is the opposite of what you would expect
#define FIX(b) (b?0:0x80)

	int r = FIX(R);
	int l = FIX(L);
	int d = FIX(D);
	int u = FIX(U);
	int t = FIX(T);
	int s = FIX(S);
	int b = FIX(B);
	int a = FIX(A);
	int y = FIX(Y);
	int x = FIX(X);
	int w = FIX(W);
	int e = FIX(E);
	int g = FIX(G);
	int f = FIX(F);

	u16	pad	= (0 |
		((a) >> 7) |
		((b) >> 6) |
		((s) >> 5) |
		((t) >> 4) |
		((r) >> 3) |
		((l) >> 2) |
		((u) >> 1) |
		((d))	   |
		((e) << 1) |
		((w) << 2)) ;

	((u16 *)ARM9Mem.ARM9_REG)[0x130>>1] = (u16)pad;
	((u16 *)MMU.ARM7_REG)[0x130>>1] = (u16)pad;

	if (!f && !countLid) 
	{
		LidClosed = (!LidClosed) & 0x01;
		if (!LidClosed)
		{
		//	SPU_Pause(FALSE);
			NDS_makeARM7Int(22);

		}
		//else
			//SPU_Pause(TRUE);

		countLid = 30;
	}
	else 
	{
		if (countLid > 0)
			countLid--;
	}

	u16 padExt = (((u16 *)MMU.ARM7_REG)[0x136>>1] & 0x0070) |
		((x) >> 7) |
		((y) >> 6) |
		((g) >> 4) |
		((LidClosed) << 7) |
		0x0034;

	((u16 *)MMU.ARM7_REG)[0x136>>1] = (u16)padExt;

	InputDisplayString=MakeInputDisplayString(padExt, pad);

	//put into the format we want for the movie system
	//RLDUTSBAYXWEGF
#undef FIX
#define FIX(b) (b?1:0)

	r = FIX(R);
	l = FIX(L);
	d = FIX(D);
	u = FIX(U);
	t = FIX(T);
	s = FIX(S);
	b = FIX(B);
	a = FIX(A);
	y = FIX(Y);
	x = FIX(X);
	w = FIX(W);
	e = FIX(E);
	g = FIX(G);
	f = FIX(F);

	if(f) movie_lid=true;
	else movie_lid=false;

	nds.pad =
		(FIX(r)<<12)|
		(FIX(l)<<11)|
		(FIX(d)<<10)|
		(FIX(u)<<9)|
		(FIX(s)<<8)|
		(FIX(t)<<7)|
		(FIX(b)<<6)|
		(FIX(a)<<5)|
		(FIX(y)<<4)|
		(FIX(x)<<3)|
		(FIX(w)<<2)|
		(FIX(e)<<1);

	// TODO: low power IRQ
}


void emu_halt() {
	execute = false;
}

//these templates needed to be instantiated manually
template void NDS_exec<FALSE>(s32 nb);
template void NDS_exec<TRUE>(s32 nb);
