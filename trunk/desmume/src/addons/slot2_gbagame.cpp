/*
	Copyright (C) 2009 CrazyMax
	Copyright (C) 2009-2013 DeSmuME team

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

#include <string.h>
#include "../mem.h"
#include "../MMU.h"
#include "../NDSSystem.h"
#include "../path.h"
#include "../slot2.h"

#define EEPROM		0x52504545
#define SRAM_		0x4D415253
#define FLASH		0x53414C46
#define FLASH1M_	0x5F4D3148
#define SIIRTC_V	0x52494953

static const char *saveTypes[] = {
	"EEPROM",
	"SRAM",
	"FLASH",
	"FLASH1M",
	"SIIRTC_V",
};

class Slot2_GbaCart : public ISlot2Interface
{
private:
	FILE	*fROM;
	FILE	*fSRAM;
	u32		romSize;
	u32		currentROMPos;
	u32		sramSize;
	u32		currentSRAMPos;
	u32		saveType;

	struct 
	{
		u32	size;
		u8	state;
		u8	cmd;
		u8	idDevice;
		u8	idManufacturer;
		u8	bank;
	} gbaFlash;

	u32	readRom(const u32 pos, const u8 size) 
	{
		if (fROM)
		{
			if (currentROMPos != pos)
				fseek(fROM, pos, SEEK_SET);

			u32 data = 0xFFFFFFFF;
			u32 readed = fread(&data, 1, size, fROM);
			currentROMPos = (pos + readed);
			return data;
		}
		return 0xFFFFFFFF;
	}

	u32 readSRAM(const u32 pos, const u8 size) 
	{
		if (fSRAM)
		{
			if (currentSRAMPos != pos)
				fseek(fSRAM, pos, SEEK_SET);

			u32 data = 0xFFFFFFFF;
			u32 readed = fread(&data, 1, size, fSRAM);
			currentSRAMPos = (pos + readed);
			return data;
		}
		return 0xFFFFFFFF;
	}

	void writeSRAM(const u32 pos, const u8 *data, u32 size) 
	{
		if (fSRAM)
		{
			if (currentSRAMPos != pos)
				fseek(fSRAM, pos, SEEK_SET);

			u32 writed = fwrite(&data, 1, size, fSRAM);
			currentSRAMPos = (pos + writed);
		}
	}


	u32 getSaveTypeGBA()
	{
		if (!fROM) return 0xFF;

		u32 saveROMPos = currentROMPos;
		u32 tmp = 0;

		fseek(fROM, 0, SEEK_SET);

		while (!feof(fROM))
		{
			u32 readed = fread(&tmp, 1, 4, fROM);
			if (readed < 4) break;

			switch (tmp)
			{
				case EEPROM:	fseek(fROM, saveROMPos, SEEK_SET); return 1;
				case SRAM_:		fseek(fROM, saveROMPos, SEEK_SET); return 2;
				case FLASH:
				{
					fread(&tmp, 1, 4, fROM);
					fseek(fROM, saveROMPos, SEEK_SET); 
					return ((tmp == FLASH1M_)?3:5);
				}
				case SIIRTC_V:	 fseek(fROM, saveROMPos, SEEK_SET); return 4;
			}
		}

		return 0xFF;
	}

	void gbaWriteFlash(u32 adr, u8 val)
	{
		if (!fSRAM) return;
		switch (gbaFlash.state)
		{
			case 0:
				if (adr == 0x0A005555)
				{
					if (val == 0xF0)
					{
						//INFO("GBAgame: Flash: reset\n");
						gbaFlash.state = 0;
						gbaFlash.cmd = 0;
						return;
					}
					if (val == 0xAA)
					{
						gbaFlash.state = 1;
						return;
					}
				}

				if (adr == 0x0A000000)
				{
					if (gbaFlash.cmd == 0xB0)
					{
						gbaFlash.bank = val;
						gbaFlash.cmd = 0;
						//INFO("GBAgame: Flash: change bank %i\n", val);
						return;
					}
				}
				break;
			case 1:
				if ( (adr == 0x0A002AAA) && (val == 0x55) )
				{
					gbaFlash.state = 2;
					return;
				}
				gbaFlash.state = 0;
				break;
			case 2:
				if (adr == 0x0A005555)
				{
					//INFO("GBAgame: Flash: send command flash 0x%02X\n", val);
					switch (val)
					{
						case 0x80:		// Erase
							gbaFlash.state = 0x80;
						break;

						case 0x90:		// Chip Identification
							gbaFlash.state = 0x90;
						break;

						case 0xA0:		// Write
							gbaFlash.state = 0;
						break;

						default:
							gbaFlash.state = 0;
						break;
					}
					gbaFlash.cmd = val;
					return;
				}
				gbaFlash.state = 0;
			break;

			// erase
			case 0x80:
				if ( (adr == 0x0A005555) && (val == 0xAA) )
				{
					gbaFlash.state = 0x81;
					return;
				}
				gbaFlash.state = 0;
			break;

			case 0x81:
				if ( (adr == 0x0A002AAA) && (val == 0x55) )
				{
					gbaFlash.state = 0x82;
					return;
				}
				gbaFlash.state = 0;
			break;

			case 0x82:
				if (val == 0x30)
				{
					u32 ofs = (adr & 0x0000F000);
					//INFO("GBAgame: Flash: erase from 0x%08X to 0x%08X\n", ofs + 0x0A000000, ofs + 0x0A001000);
					u8 *tmp = new u8[0x1000];
					memset(tmp, 0xFF, 0x1000);
					writeSRAM(ofs, tmp, 0x1000);
					delete [] tmp;
				}
				gbaFlash.state = 0;
				gbaFlash.cmd = 0;
			return;

			// Chip Identification
			case 0x90:
				if ( (adr == 0x0A005555) && (val == 0xAA) )
				{
					gbaFlash.state = 0x91;
					return;
				}
				gbaFlash.state = 0;
			break;

			case 0x91:
				if ( (adr == 0x0A002AAA) && (val == 0x55) )
				{
					gbaFlash.state = 0x92;
					return;
				}
				gbaFlash.state = 0;
			break;

			case 0x92:
				gbaFlash.state = 0;
				gbaFlash.cmd = 0;
			return;
		}

		if (gbaFlash.cmd == 0xA0)	// write
		{
			writeSRAM((adr & 0x1FFFF) + (0x10000 * gbaFlash.bank), &val, 1);
			gbaFlash.state = 0;
			gbaFlash.cmd = 0;
			return;
		}
		INFO("GBAgame: Flash: write unknown atn 0x%08X = 0x%02X\n", adr, val);
	}

	u8 gbaReadFlash(u32 adr)
	{
		if (!fSRAM) return 0xFF;
		if (gbaFlash.cmd == 0)
			return readSRAM((adr & 0x1FFFF) + (0x10000 * gbaFlash.bank), 1);

		//INFO("GBAgame: flash read at 0x%08X\n", adr);

		switch (gbaFlash.cmd)
		{
			case 0x90:			// Chip Identification
				if (adr == 0x0A000000) return gbaFlash.idManufacturer;
				if (adr == 0x0A000001) return gbaFlash.idDevice;
			break;

			case 0xF0:			//
				gbaFlash.state = 0;
				gbaFlash.cmd = 0;
			break;

			case 0xB0:			// Bank switching
			break;

			default:
				INFO("GBAgame: Flash: read - unknown command at 0x%08X = 0x%02X\n", adr, gbaFlash.cmd);
			break;
		}

		return 0xFF;
	}

public:
	virtual Slot2Info const* info()
	{
		static Slot2InfoSimple info("GBA cartridge", "GBA cartridge in slot", 0x03);
		return &info;
	}

	virtual void connect()
	{
		if (fROM) fclose(fROM);
		if (fSRAM) fclose(fSRAM);
		fROM = NULL; fSRAM = NULL;
		romSize = 0;
		currentROMPos = 0;
		sramSize = 0;
		currentSRAMPos = 0;

		if (gameInfo.romsize == 0) return;

		if (!strcasecmp(GBAgameName, "self"))
			strcpy(GBAgameName, path.path.c_str());

		if (!strlen(GBAgameName)) return;
		
		printf("GBASlot opening ROM %s", GBAgameName);

		fROM = fopen(GBAgameName, "rb");
		if (fROM)
		{
			fseek(fROM, 0, SEEK_END);
			romSize = ftell(fROM);
			fseek(fROM, 0, SEEK_SET);
			printf(" - Success (%u bytes)\n", romSize);

			//try loading the sram
			char *dot = strrchr(GBAgameName, '.');
			if(!dot) return;

			std::string sram_fname = GBAgameName;
			sram_fname.resize(dot-GBAgameName);
			sram_fname += ".sav";
			fSRAM = fopen(sram_fname.c_str(), "rb+");
			if(!fSRAM) return;
			fseek(fSRAM, 0, SEEK_END);
			sramSize = ftell(fSRAM);
			fseek(fSRAM, 0, SEEK_SET);
			saveType = getSaveTypeGBA();
			printf("GBASlot found SRAM %s (%s - %u bytes)\n", sram_fname.c_str(), (saveType == 0xFF)?"Unknown":saveTypes[saveType], sramSize);
			gbaFlash.size = sramSize;
			if (gbaFlash.size <= (64 * 1024))
			{
				gbaFlash.idDevice = 0x1B;
				gbaFlash.idManufacturer = 0x32;
			}
			else
			{
				gbaFlash.idDevice = 0x09;
				gbaFlash.idManufacturer = 0xC2;
			}
		}
		else
			printf(" - Failed\n");
	}

	virtual void disconnect()
	{
		if (fROM) fclose(fROM);
		if (fSRAM) fclose(fSRAM);
		fROM = NULL; fSRAM = NULL;
		romSize = 0;
		currentROMPos = 0;
		sramSize = 0;
		currentSRAMPos = 0;
	}

	virtual void writeByte(u8 PROCNUM, u32 addr, u8 val)
	{
		if ((addr >= 0x0A000000) && (addr < 0x0A010000))
		{
			switch (saveType)
			{
				case 3:			// Flash
				case 5:
					gbaWriteFlash(addr, val);
				break;

				default: break;
			}
		}
	}

	virtual u8	readByte(u8 PROCNUM, u32 addr)
	{
		if (addr < 0x0A000000)
			return (u8)readRom(addr - 0x08000000, 1);

		if (addr < 0x0A010000)
		{
			if ((saveType == 3) || (saveType == 5))
				return gbaReadFlash(addr);

			return (u8)readSRAM(addr - 0x0A000000, 1);
		}

		return 0xFF;
	}
	virtual u16	readWord(u8 PROCNUM, u32 addr)
	{
		if (addr < 0x0A000000)
			return (u16)readRom(addr - 0x08000000, 2);

		if (addr < 0x0A010000)
			return (u16)readSRAM(addr - 0x0A000000, 2);

		return 0xFFFF;
	}
	virtual u32	readLong(u8 PROCNUM, u32 addr)
	{
		if (addr < 0x0A000000)
			return (u32)readRom(addr - 0x08000000, 4);

		if (addr < 0x0A010000)
			return (u32)readSRAM(addr - 0x0A000000, 4);

		return 0xFFFFFFFF;
	}
};

ISlot2Interface* construct_Slot2_GbaCart() { return new Slot2_GbaCart(); }
