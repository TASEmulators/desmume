/*
	Copyright (C) 2013-2015 DeSmuME team

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

#include "slot1comp_mc.h"
#include "slot1comp_rom.h"
#include "slot1comp_protocol.h"

#include <stdio.h>
#include <string>

#include "../slot1.h"
#include "../path.h"
#include "../NDSSystem.h"
#include "../utils/fsnitro.h"

//quick architecture overview:
//MCROM receives GC bus commands from MMU.cpp
//those are passed on to the protocol component for parsing
//protocol calls back into MCROM via ISlot1Comp_Protocol_Client interface for things the protocol doesnt know about (the contents of the rom, chiefly)
//MCROM utilizes the rom component for address logic and delivering data

class Slot1_Retail_DEBUG : public ISlot1Interface, public ISlot1Comp_Protocol_Client
{
private:
	Slot1Comp_Protocol protocol;
	Slot1Comp_Rom rom;
	FILE	*fpROM;
	FS_NITRO *fs;
	u16		curr_file_id;
	string	pathData;

public:

	virtual Slot1Info const* info()
	{
		static Slot1InfoSimple info("Retail DEBUG","Slot1 Retail (standard) card emulation + FS Nitro DEBUG", 0x04);
		return &info;
	}

	virtual void connect()
	{
		protocol.reset(this);
		protocol.chipId = gameInfo.chipID;
		protocol.gameCode = T1ReadLong((u8*)gameInfo.header.gameCode,0);

		curr_file_id = 0xFFFF;
		fpROM = NULL;
		fs = NULL;

		if (!CommonSettings.loadToMemory) 
		{
			printf("NitroFS: change load type to \"Load to RAM\"\n");
			return;
		}
		pathData = path.getpath(path.SLOT1D) + path.GetRomNameWithoutExtension();
		printf("Path to Slot1 data: %s\n", pathData.c_str());
		
		fs = new FS_NITRO(gameInfo.romdata);
		fs->rebuildFAT(pathData);
	}

	virtual u8 auxspi_transaction(int PROCNUM, u8 value)
	{
		return g_Slot1Comp_MC.auxspi_transaction(PROCNUM,value);
	}

	virtual void auxspi_reset(int PROCNUM)
	{
		g_Slot1Comp_MC.auxspi_reset(PROCNUM);
	}

	virtual void write_command(u8 PROCNUM, GC_Command command)
	{
		protocol.write_command(command);
	}
	virtual void write_GCDATAIN(u8 PROCNUM, u32 val)
	{
		protocol.write_GCDATAIN(PROCNUM, val);
	}
	virtual u32 read_GCDATAIN(u8 PROCNUM)
	{
		return protocol.read_GCDATAIN(PROCNUM);
	}

	virtual void post_fakeboot(int PROCNUM)
	{
	    // The BIOS leaves the card in NORMAL mode
	    protocol.mode = eCardMode_NORMAL;
	}

	virtual void savestate(EMUFILE* os)
	{
		protocol.savestate(os);
		rom.savestate(os);
	}

	virtual void loadstate(EMUFILE* is)
	{
		protocol.loadstate(is);
		rom.loadstate(is);
	}

	virtual void slot1client_startOperation(eSlot1Operation operation)
	{
		if (protocol.operation == eSlot1Operation_B7_Read)
		{
			u16 file_id = 0xFFFF; u32 offset = 0;
			bool bFromFile = false;

			if (fs && fs->isFAT(protocol.address))
			{
				fs->rebuildFAT(protocol.address, protocol.length, pathData);
			}
			else
			{
				if (fs && fs->getFileIdByAddr(protocol.address, file_id, offset)) 
				{
					if (file_id != curr_file_id)
					{
						string tmp = fs->getFullPathByFileID(file_id);
						printf("%04X:[%08X, ofs %08X] %s\n", file_id, protocol.address, offset, tmp.c_str());
						
						if (fpROM)
						{
							fclose(fpROM);
							fpROM = NULL;
						}
						tmp = pathData + tmp;

						fpROM = fopen(tmp.c_str(), "rb");
						if (fpROM)
						{
							bFromFile = true;
							printf("\t * found at disk, offset %08X\n", offset);
							if (fseek(fpROM, offset, SEEK_SET) != 0)
							{
								printf("\t\t - ERROR seek file position\n");
							}
						}
					}
					else
					{
						if (fpROM)
						{
							bFromFile = true;
							if (ftell(fpROM) != offset)
							{
								printf("\t * new file seek %08Xh\n", offset);
								fseek(fpROM, offset, SEEK_SET);
							}
						}
					}
				}
			}
			if (!bFromFile && fpROM)
			{
				fclose(fpROM);
				fpROM = NULL;
			}

			curr_file_id = file_id;
		}
		rom.start(operation, protocol.address);
	}

private:

	u32 slot1client_read_GCDATAIN(eSlot1Operation operation)
	{
		if (protocol.operation == eSlot1Operation_B7_Read)
		{
			
			u32 address = rom.getAddress();

			if (fs && fs->isFAT(address))
			{
				u32 res = fs->getFATRecord(address);
				if (res != 0xFFFFFFFF)
				{
					rom.incAddress();
					return res;
				}
			}
			else
				if (fpROM)
				{
					u32 data = 0;
					u32 readed = fread(&data, 1, 4, fpROM);
					if (readed)
					{
						rom.incAddress();
						if (readed < 4)
							data |= ((u32)0xFFFFFFFF << (readed * 8));
						return data;
					}

					fclose(fpROM);
					fpROM = NULL;
				}
		}

		return rom.read();
	}
};

ISlot1Interface* construct_Slot1_Retail_DEBUG() { return new Slot1_Retail_DEBUG(); }
