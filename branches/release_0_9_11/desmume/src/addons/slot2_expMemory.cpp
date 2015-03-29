/*
	Copyright (C) 2009 CrazyMax
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

#include "../slot2.h"
#include "../emufile.h"
#include "../mem.h"

#if 0
#define EXPINFO(...) INFO(__VA_ARGS__)
#else
#define EXPINFO(...)
#endif 

#define EXPANSION_MEMORY_SIZE (8 * 1024 * 1024)

static u8 header_0x00B0[] = 
{ 0xFF, 0xFF, 0x96, 0x00,  //this 0x96 is strange. it can't be read from the pak when it boots, it must appear later
  0x00, 0x24, 0x24, 0x24, 
  0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0x7F
};

class Slot2_ExpansionPak : public ISlot2Interface
{
private:
	u8		*expMemory;
	bool	ext_ram_lock;
public:
	
	Slot2_ExpansionPak()
	{
		expMemory = NULL;
		ext_ram_lock = true;
	}
	
	virtual Slot2Info const* info()
	{
		static Slot2InfoSimple info("Memory Expansion Pak", "Official RAM expansion for Opera browser", 0x05);
		return &info;
	}

	virtual void connect()
	{
		if (expMemory == NULL)
		{
			expMemory = new u8[EXPANSION_MEMORY_SIZE];
		}
		memset(expMemory, 0xFF, EXPANSION_MEMORY_SIZE);
		ext_ram_lock = true; 
	}

	virtual void disconnect()
	{
		delete[] expMemory;
		expMemory = NULL;
	}

	virtual void writeByte(u8 PROCNUM, u32 addr, u8 val)
	{
		if (ext_ram_lock) return;

		if (addr >= 0x09000000)
		{
			u32 offs = (addr - 0x09000000);
			if (offs >= EXPANSION_MEMORY_SIZE) return;
			T1WriteByte(expMemory, offs, val);
		}
		EXPINFO("ExpMemory: write 08 at 0x%08X = 0x%02X\n", addr, val);
	}
	virtual void writeWord(u8 PROCNUM, u32 addr, u16 val)
	{
		if (addr == 0x08240000)
		{
			if (val == 0)
				ext_ram_lock = true;
			else
				if (val == 1)
					ext_ram_lock = false;
			return;
		}

		if (ext_ram_lock) return;

		if (addr >= 0x09000000)
		{
			u32 offs = (addr - 0x09000000);
			if (offs >= EXPANSION_MEMORY_SIZE) return;
			T1WriteWord(expMemory, offs, val);
		}
		EXPINFO("ExpMemory: write 16 at 0x%08X = 0x%04X\n", addr, val);
	}
	virtual void writeLong(u8 PROCNUM, u32 addr, u32 val)
	{
		if (ext_ram_lock) return;

		if (addr >= 0x09000000)
		{
			u32 offs = (addr - 0x09000000);
			if (offs >= EXPANSION_MEMORY_SIZE) return;
			T1WriteLong(expMemory, offs, val);
		}
		EXPINFO("ExpMemory: write 32 at 0x%08X = 0x%08X\n", addr, val);
	}

	virtual u8	readByte(u8 PROCNUM, u32 addr)
	{
		EXPINFO("ExpMemory: read 08 at 0x%08X\n", addr);

		if ((addr >= 0x080000B0) && (addr < 0x080000C0))
			return T1ReadByte(header_0x00B0, (addr - 0x080000B0));

		if (addr >= 0x09000000)
		{
			u32 offs = (addr - 0x09000000);
			if (offs >= EXPANSION_MEMORY_SIZE) return (0xFF);
			return T1ReadByte(expMemory, offs);
		}
		
		return 0xFF;
	}
	virtual u16	readWord(u8 PROCNUM, u32 addr)
	{
		EXPINFO("ExpMemory: read 16 at 0x%08X\n", addr);

		if ((addr >= 0x080000B0) && (addr < 0x080000C0))
			return T1ReadWord(header_0x00B0, (addr - 0x080000B0));

		if (addr == 0x0801FFFC) return 0x7FFF;
		if (addr == 0x08240002) return 0; //this can't be 0xFFFF. dunno why, we just guessed 0

		if (addr >= 0x09000000)
		{
			u32 offs = (addr - 0x09000000);
			if (offs >= EXPANSION_MEMORY_SIZE) return (0xFFFF);
			return T1ReadWord(expMemory, offs);
		}
		
		return 0xFFFF;
	}
	virtual u32	readLong(u8 PROCNUM, u32 addr)
	{
		EXPINFO("ExpMemory: read 32 at 0x%08X\n", addr);

		if((addr >= 0x080000B0) && (addr < 0x080000C0))
			return T1ReadLong(header_0x00B0, (addr - 0x080000B0));

		if (addr >= 0x09000000)
		{
			u32 offs = (addr - 0x09000000);
			if (offs >= EXPANSION_MEMORY_SIZE) return 0xFFFFFFFF;
			return T1ReadLong(expMemory, offs);
		}
		
		return 0xFFFFFFFF;
	}

	virtual void savestate(EMUFILE* os)
	{
		s32 version = 0;
		EMUFILE_MEMORY *ram = new EMUFILE_MEMORY(expMemory, EXPANSION_MEMORY_SIZE);
		os->write32le(version);
		os->write32le((u32)ext_ram_lock);
		os->writeMemoryStream(ram);
		delete ram;
	}

	virtual void loadstate(EMUFILE* is)
	{
		EMUFILE_MEMORY *ram = new EMUFILE_MEMORY();

		s32 version = is->read32le();

		if (version >= 0)
		{
			is->read32le((u32*)&ext_ram_lock);
			is->readMemoryStream(ram);
			memcpy(expMemory, ram->buf(), std::min(EXPANSION_MEMORY_SIZE, ram->size()));
		}
		delete ram;
	}
};

ISlot2Interface* construct_Slot2_ExpansionPak() { return new Slot2_ExpansionPak(); }
