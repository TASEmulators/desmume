/*
	Copyright (C) 2006 Normmatt
	Copyright (C) 2006 Theo Berkau
	Copyright (C) 2007 Pascal Giard
	Copyright (C) 2008-2013 DeSmuME team

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

#ifdef HAVE_LIBZ
#include <zlib.h>
#endif
#include <stack>
#include <set>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <fstream>
#include "saves.h"
#include "MMU.h"
#include "NDSSystem.h"
#include "render3D.h"
#include "cp15.h"
#include "GPU_osd.h"
#include "version.h"

#include "readwrite.h"
#include "gfx3d.h"
#include "movie.h"
#include "mic.h"
#include "MMU_timing.h"
#include "slot1.h"
#include "slot2.h"

#include "path.h"

#ifdef HOST_WINDOWS
#include "windows/main.h"
#endif

int lastSaveState = 0;		//Keeps track of last savestate used for quick save/load functions

//void*v is actually a void** which will be indirected before reading
//since this isnt supported right now, it is declared in here to make things compile
#define SS_INDIRECT            0x80000000

u32 _DESMUME_version = EMU_DESMUME_VERSION_NUMERIC();
u32 svn_rev = EMU_DESMUME_SUBVERSION_NUMERIC();
s64 save_time = 0;
NDS_SLOT1_TYPE slot1Type = NDS_SLOT1_RETAIL_AUTO;
NDS_SLOT2_TYPE slot2Type = NDS_SLOT2_AUTO;

savestates_t savestates[NB_STATES];

#define SAVESTATE_VERSION       12
static const char* magic = "DeSmuME SState\0";

//a savestate chunk loader can set this if it wants to permit a silent failure (for compatibility)
static bool SAV_silent_fail_flag;

SFORMAT SF_NDS_INFO[]={
	{ "GINF", 1, sizeof(gameInfo.header), &gameInfo.header},
	{ "GRSZ", 1, 4, &gameInfo.romsize},
	{ "DVMJ", 1, 1, (void*)&DESMUME_VERSION_MAJOR},
	{ "DVMI", 1, 1, (void*)&DESMUME_VERSION_MINOR},
	{ "DSBD", 1, 1, (void*)&DESMUME_VERSION_BUILD},
	{ "GREV", 1, 4, &svn_rev},
	{ "GTIM", 1, 8, &save_time},
	{ 0 }
};

SFORMAT SF_ARM7[]={
	{ "7INS", 4, 1, &NDS_ARM7.instruction },
	{ "7INA", 4, 1, &NDS_ARM7.instruct_adr },
	{ "7INN", 4, 1, &NDS_ARM7.next_instruction },
	{ "7REG", 4,16, NDS_ARM7.R },
	{ "7CPS", 4, 1, &NDS_ARM7.CPSR },
	{ "7SPS", 4, 1, &NDS_ARM7.SPSR },
	{ "7DUS", 4, 1, &NDS_ARM7.R13_usr },
	{ "7EUS", 4, 1, &NDS_ARM7.R14_usr },
	{ "7DSV", 4, 1, &NDS_ARM7.R13_svc },
	{ "7ESV", 4, 1, &NDS_ARM7.R14_svc },
	{ "7DAB", 4, 1, &NDS_ARM7.R13_abt },
	{ "7EAB", 4, 1, &NDS_ARM7.R14_abt },
	{ "7DUN", 4, 1, &NDS_ARM7.R13_und },
	{ "7EUN", 4, 1, &NDS_ARM7.R14_und },
	{ "7DIR", 4, 1, &NDS_ARM7.R13_irq },
	{ "7EIR", 4, 1, &NDS_ARM7.R14_irq },
	{ "78FI", 4, 1, &NDS_ARM7.R8_fiq },
	{ "79FI", 4, 1, &NDS_ARM7.R9_fiq },
	{ "7AFI", 4, 1, &NDS_ARM7.R10_fiq },
	{ "7BFI", 4, 1, &NDS_ARM7.R11_fiq },
	{ "7CFI", 4, 1, &NDS_ARM7.R12_fiq },
	{ "7DFI", 4, 1, &NDS_ARM7.R13_fiq },
	{ "7EFI", 4, 1, &NDS_ARM7.R14_fiq },
	{ "7SVC", 4, 1, &NDS_ARM7.SPSR_svc },
	{ "7ABT", 4, 1, &NDS_ARM7.SPSR_abt },
	{ "7UND", 4, 1, &NDS_ARM7.SPSR_und },
	{ "7IRQ", 4, 1, &NDS_ARM7.SPSR_irq },
	{ "7FIQ", 4, 1, &NDS_ARM7.SPSR_fiq },
	{ "7int", 4, 1, &NDS_ARM7.intVector },
	{ "7LDT", 1, 1, &NDS_ARM7.LDTBit },
	{ "7Wai", 4, 1, &NDS_ARM7.waitIRQ },
	{ "7hef", 4, 1, &NDS_ARM7.halt_IE_and_IF },
	{ "7iws", 1, 1, &NDS_ARM7.intrWaitARM_state },
	{ 0 }
};

SFORMAT SF_ARM9[]={
	{ "9INS", 4, 1, &NDS_ARM9.instruction},
	{ "9INA", 4, 1, &NDS_ARM9.instruct_adr},
	{ "9INN", 4, 1, &NDS_ARM9.next_instruction},
	{ "9REG", 4,16, NDS_ARM9.R},
	{ "9CPS", 4, 1, &NDS_ARM9.CPSR},
	{ "9SPS", 4, 1, &NDS_ARM9.SPSR},
	{ "9DUS", 4, 1, &NDS_ARM9.R13_usr},
	{ "9EUS", 4, 1, &NDS_ARM9.R14_usr},
	{ "9DSV", 4, 1, &NDS_ARM9.R13_svc},
	{ "9ESV", 4, 1, &NDS_ARM9.R14_svc},
	{ "9DAB", 4, 1, &NDS_ARM9.R13_abt},
	{ "9EAB", 4, 1, &NDS_ARM9.R14_abt},
	{ "9DUN", 4, 1, &NDS_ARM9.R13_und},
	{ "9EUN", 4, 1, &NDS_ARM9.R14_und},
	{ "9DIR", 4, 1, &NDS_ARM9.R13_irq},
	{ "9EIR", 4, 1, &NDS_ARM9.R14_irq},
	{ "98FI", 4, 1, &NDS_ARM9.R8_fiq},
	{ "99FI", 4, 1, &NDS_ARM9.R9_fiq},
	{ "9AFI", 4, 1, &NDS_ARM9.R10_fiq},
	{ "9BFI", 4, 1, &NDS_ARM9.R11_fiq},
	{ "9CFI", 4, 1, &NDS_ARM9.R12_fiq},
	{ "9DFI", 4, 1, &NDS_ARM9.R13_fiq},
	{ "9EFI", 4, 1, &NDS_ARM9.R14_fiq},
	{ "9SVC", 4, 1, &NDS_ARM9.SPSR_svc},
	{ "9ABT", 4, 1, &NDS_ARM9.SPSR_abt},
	{ "9UND", 4, 1, &NDS_ARM9.SPSR_und},
	{ "9IRQ", 4, 1, &NDS_ARM9.SPSR_irq},
	{ "9FIQ", 4, 1, &NDS_ARM9.SPSR_fiq},
	{ "9int", 4, 1, &NDS_ARM9.intVector},
	{ "9LDT", 1, 1, &NDS_ARM9.LDTBit},
	{ "9Wai", 4, 1, &NDS_ARM9.waitIRQ},
	{ "9hef", 4, 1, &NDS_ARM9.halt_IE_and_IF },
	{ "9iws", 1, 1, &NDS_ARM9.intrWaitARM_state },
	{ 0 }
};

SFORMAT SF_MEM[]={
	{ "ITCM", 1, sizeof(MMU.ARM9_ITCM),   MMU.ARM9_ITCM},
	{ "DTCM", 1, sizeof(MMU.ARM9_DTCM),   MMU.ARM9_DTCM},

	 //for legacy purposes, WRAX is a separate variable. shouldnt be a problem.
	{ "WRAM", 1, 0x400000, MMU.MAIN_MEM},
	{ "WRAX", 1, 0x400000, MMU.MAIN_MEM+0x400000},

	//NOTE - this is not as large as the allocated memory.
	//the memory is overlarge due to the way our memory map system is setup
	//but there are actually no more registers than this
	{ "9REG", 1, 0x2000,   MMU.ARM9_REG},

	{ "VMEM", 1, sizeof(MMU.ARM9_VMEM),    MMU.ARM9_VMEM},
	{ "OAMS", 1, sizeof(MMU.ARM9_OAM),    MMU.ARM9_OAM},

	//this size is specially chosen to avoid saving the blank space at the end
	{ "LCDM", 1, 0xA4000,		MMU.ARM9_LCD},
	{ 0 }
};

SFORMAT SF_NDS[]={
	{ "_WCY", 4, 1, &nds.wifiCycle},
	{ "_TCY", 8, 8, nds.timerCycle},
	{ "_VCT", 4, 1, &nds.VCount},
	{ "_OLD", 4, 1, &nds.old},
	{ "_TPX", 2, 1, &nds.adc_touchX},
	{ "_TPY", 2, 1, &nds.adc_touchY},
	{ "_TPC", 2, 1, &nds.adc_jitterctr},
	{ "_STX", 2, 1, &nds.scr_touchX},
	{ "_STY", 2, 1, &nds.scr_touchY},
	{ "_TPB", 4, 1, &nds.isTouch},
	{ "_IFB", 4, 1, &nds.isFakeBooted},
	{ "_DBG", 4, 1, &nds._DebugConsole},
	{ "_ENS", 4, 1, &nds.ensataEmulation},
	{ "_TYP", 4, 1, &nds.ConsoleType},
	{ "_ENH", 4, 1, &nds.ensataHandshake},
	{ "_ENI", 4, 1, &nds.ensataIpcSyncCounter},
	{ "_SLP", 4, 1, &nds.sleeping},
	{ "_FBS", 4, 1, &nds.freezeBus},
	{ "_CEJ", 4, 1, &nds.cardEjected},
	{ "_PDL", 2, 1, &nds.paddle},
	{ "_P00", 1, 1, &nds.power1.lcd},
	{ "_P01", 1, 1, &nds.power1.gpuMain},
	{ "_P02", 1, 1, &nds.power1.gfx3d_render},
	{ "_P03", 1, 1, &nds.power1.gfx3d_geometry},
	{ "_P04", 1, 1, &nds.power1.gpuSub},
	{ "_P05", 1, 1, &nds.power1.dispswap},
	{ "_P06", 1, 1, &nds.power2.speakers},
	{ "_P07", 1, 1, &nds.power2.wifi},
	{ 0 }
};

SFORMAT SF_MMU[]={
	{ "M7BI", 1, sizeof(MMU.ARM7_BIOS), MMU.ARM7_BIOS},
	{ "M7ER", 1, sizeof(MMU.ARM7_ERAM), MMU.ARM7_ERAM},
	{ "M7RG", 1, sizeof(MMU.ARM7_REG), MMU.ARM7_REG},
	{ "M7WI", 1, sizeof(MMU.ARM7_WIRAM), MMU.ARM7_WIRAM},
	{ "MSWI", 1, sizeof(MMU.SWIRAM), MMU.SWIRAM},
	{ "M9RW", 1, 1,       &MMU.ARM9_RW_MODE},
	{ "MDTC", 4, 1,       &MMU.DTCMRegion},
	{ "MITC", 4, 1,       &MMU.ITCMRegion},
	{ "MTIM", 2, 8,       MMU.timer},
	{ "MTMO", 4, 8,       MMU.timerMODE},
	{ "MTON", 4, 8,       MMU.timerON},
	{ "MTRN", 4, 8,       MMU.timerRUN},
	{ "MTRL", 2, 8,       MMU.timerReload},
	{ "MIME", 4, 2,       MMU.reg_IME},
	{ "MIE_", 4, 2,       MMU.reg_IE},
	{ "MIF_", 4, 2,       MMU.reg_IF_bits},
	{ "MIFP", 4, 2,       MMU.reg_IF_pending},

	{ "MGXC", 8, 1,       &MMU.gfx3dCycles},
	
	{ "M_SX", 1, 2,       &MMU.SPI_CNT},
	{ "M_SC", 1, 2,       &MMU.SPI_CMD},
	{ "MASX", 1, 2,       &MMU.AUX_SPI_CNT},
	//{ "MASC", 1, 2,       &MMU.AUX_SPI_CMD}, //zero 20-aug-2013 - this seems pointless

	{ "MWRA", 1, 2,       &MMU.WRAMCNT},

	{ "MDV1", 4, 1,       &MMU.divRunning},
	{ "MDV2", 8, 1,       &MMU.divResult},
	{ "MDV3", 8, 1,       &MMU.divMod},
	{ "MDV5", 8, 1,       &MMU.divCycles},

	{ "MSQ1", 4, 1,       &MMU.sqrtRunning},
	{ "MSQ2", 4, 1,       &MMU.sqrtResult},
	{ "MSQ4", 8, 1,       &MMU.sqrtCycles},
	
	//begin memory chips
	{ "BUCO", 1, 1,       &MMU.fw.com},
	{ "BUAD", 4, 1,       &MMU.fw.addr},
	{ "BUAS", 1, 1,       &MMU.fw.addr_shift},
	{ "BUAZ", 1, 1,       &MMU.fw.addr_size},
	{ "BUWE", 4, 1,       &MMU.fw.write_enable},
	{ "BUWR", 4, 1,       &MMU.fw.writeable_buffer},
	//end memory chips

	//TODO:slot-1 plugins
	{ "GC0T", 4, 1,       &MMU.dscard[0].transfer_count},
	{ "GC0M", 4, 1,       &MMU.dscard[0].mode},
	{ "GC1T", 4, 1,       &MMU.dscard[1].transfer_count},
	{ "GC1M", 4, 1,       &MMU.dscard[1].mode},
	//{ "MCHT", 4, 1,       &MMU.CheckTimers},
	//{ "MCHD", 4, 1,       &MMU.CheckDMAs},

	//fifos
	{ "F0TH", 1, 1,       &ipc_fifo[0].head},
	{ "F0TL", 1, 1,       &ipc_fifo[0].tail},
	{ "F0SZ", 1, 1,       &ipc_fifo[0].size},
	{ "F0BF", 4, 16,      ipc_fifo[0].buf},
	{ "F1TH", 1, 1,       &ipc_fifo[1].head},
	{ "F1TL", 1, 1,       &ipc_fifo[1].tail},
	{ "F1SZ", 1, 1,       &ipc_fifo[1].size},
	{ "F1BF", 4, 16,      ipc_fifo[1].buf},

	{ "FDHD", 4, 1,       &disp_fifo.head},
	{ "FDTL", 4, 1,       &disp_fifo.tail},
	{ "FDBF", 4, 0x6000,  disp_fifo.buf},

	{ "PMCN", 1, 1,			&MMU.powerMan_CntReg},
	{ "PMCW", 4, 1,			&MMU.powerMan_CntRegWritten},
	{ "PMCR", 1, 5,			&MMU.powerMan_Reg},

	{ "MR3D", 4, 1,		&MMU.reg_DISP3DCNT_bits},
	
	{ 0 }
};

SFORMAT SF_MOVIE[]={
	{ "FRAC", 4, 1, &currFrameCounter},
	{ "LAGC", 4, 1, &TotalLagFrames},
	{ 0 }
};

// TODO: integrate the new wifi state variables once everything is settled
SFORMAT SF_WIFI[]={
	{ "W000", 4, 1, &wifiMac.powerOn},
	{ "W010", 4, 1, &wifiMac.powerOnPending},

	{ "W020", 2, 1, &wifiMac.rfStatus},
	{ "W030", 2, 1, &wifiMac.rfPins},

	{ "W040", 2, 1, &wifiMac.IE},
	{ "W050", 2, 1, &wifiMac.IF},

	{ "W060", 2, 1, &wifiMac.macMode},
	{ "W070", 2, 1, &wifiMac.wepMode},
	{ "W080", 4, 1, &wifiMac.WEP_enable},

	{ "W100", 2, 1, &wifiMac.TXCnt},
	{ "W120", 2, 1, &wifiMac.TXStat},

	{ "W200", 2, 1, &wifiMac.RXCnt},
	{ "W210", 2, 1, &wifiMac.RXCheckCounter},

	{ "W220", 1, 6, &wifiMac.mac.bytes},
	{ "W230", 1, 6, &wifiMac.bss.bytes},

	{ "W240", 2, 1, &wifiMac.aid},
	{ "W250", 2, 1, &wifiMac.pid},
	{ "W260", 2, 1, &wifiMac.retryLimit},

	{ "W270", 4, 1, &wifiMac.crystalEnabled},
	{ "W280", 8, 1, &wifiMac.usec},
	{ "W290", 4, 1, &wifiMac.usecEnable},
	{ "W300", 8, 1, &wifiMac.ucmp},
	{ "W310", 4, 1, &wifiMac.ucmpEnable},
	{ "W320", 2, 1, &wifiMac.eCount},
	{ "W330", 4, 1, &wifiMac.eCountEnable},

	{ "WR00", 4, 1, &wifiMac.RF.CFG1.val},
	{ "WR01", 4, 1, &wifiMac.RF.IFPLL1.val},
	{ "WR02", 4, 1, &wifiMac.RF.IFPLL2.val},
	{ "WR03", 4, 1, &wifiMac.RF.IFPLL3.val},
	{ "WR04", 4, 1, &wifiMac.RF.RFPLL1.val},
	{ "WR05", 4, 1, &wifiMac.RF.RFPLL2.val},
	{ "WR06", 4, 1, &wifiMac.RF.RFPLL3.val},
	{ "WR07", 4, 1, &wifiMac.RF.RFPLL4.val},
	{ "WR08", 4, 1, &wifiMac.RF.CAL1.val},
	{ "WR09", 4, 1, &wifiMac.RF.TXRX1.val},
	{ "WR10", 4, 1, &wifiMac.RF.PCNT1.val},
	{ "WR11", 4, 1, &wifiMac.RF.PCNT2.val},
	{ "WR12", 4, 1, &wifiMac.RF.VCOT1.val},

	{ "W340", 1, 105, &wifiMac.BB.data[0]},

	{ "W350", 2, 1, &wifiMac.rfIOCnt.val},
	{ "W360", 2, 1, &wifiMac.rfIOStatus.val},
	{ "W370", 4, 1, &wifiMac.rfIOData.val},
	{ "W380", 2, 1, &wifiMac.bbIOCnt.val},

	{ "W400", 2, 0x1000, &wifiMac.RAM[0]},
	{ "W410", 2, 1, &wifiMac.RXRangeBegin},
	{ "W420", 2, 1, &wifiMac.RXRangeEnd},
	{ "W430", 2, 1, &wifiMac.RXWriteCursor},
	{ "W460", 2, 1, &wifiMac.RXReadCursor},
	{ "W470", 2, 1, &wifiMac.RXUnits},
	{ "W480", 2, 1, &wifiMac.RXBufCount},
	{ "W490", 2, 1, &wifiMac.CircBufReadAddress},
	{ "W500", 2, 1, &wifiMac.CircBufWriteAddress},
	{ "W510", 2, 1, &wifiMac.CircBufRdEnd},
	{ "W520", 2, 1, &wifiMac.CircBufRdSkip},
	{ "W530", 2, 1, &wifiMac.CircBufWrEnd},
	{ "W540", 2, 1, &wifiMac.CircBufWrSkip},

	{ "W580", 2, 0x800, &wifiMac.IOPorts[0]},
	{ "W590", 2, 1, &wifiMac.randomSeed},

	{ 0 }
};

extern SFORMAT SF_RTC[];

static u8 reserveVal = 0;
SFORMAT reserveChunks[] = {
	{ "RESV", 1, 1, &reserveVal},
	{ 0 }
};

static bool s_slot1_loadstate(EMUFILE* is, int size)
{
	u32 version = is->read32le();

	//version 0:
	if(version >= 0)
	{
		u8 slotID = is->read32le();
		slot1Type = NDS_SLOT1_RETAIL_AUTO;
		if (version >= 1)
			slot1_getTypeByID(slotID, slot1Type);

		slot1_Change(slot1Type);

		EMUFILE_MEMORY temp;
		is->readMemoryStream(&temp);
		temp.fseek(0,SEEK_SET);
		slot1_Loadstate(&temp);
	}

	return true;
}

static void s_slot1_savestate(EMUFILE* os)
{
	u32 version = 1;
	os->write32le(version);

	u8 slotID = (u8)slot1_List[slot1_GetSelectedType()]->info()->id();
	os->write32le(slotID);

	EMUFILE_MEMORY temp;
	slot1_Savestate(&temp);
	os->writeMemoryStream(&temp);
}

static bool s_slot2_loadstate(EMUFILE* is, int size)
{
	u32 version = is->read32le();

	//version 0:
	if(version >= 0)
	{
		slot2Type = NDS_SLOT2_AUTO;
		u8 slotID = is->read32le();
		if (version == 0)
			slot2_getTypeByID(slotID, slot2Type);
		slot2_Change(slot2Type);

		EMUFILE_MEMORY temp;
		is->readMemoryStream(&temp);
		temp.fseek(0,SEEK_SET);
		slot2_Loadstate(&temp);
	}

	return true;
}

static void s_slot2_savestate(EMUFILE* os)
{
	u32 version = 0;
	os->write32le(version);

	//version 0:
	u8 slotID = (u8)slot2_List[slot2_GetSelectedType()]->info()->id();
	os->write32le(slotID);

	EMUFILE_MEMORY temp;
	slot2_Savestate(&temp);
	os->writeMemoryStream(&temp);
}

static void mmu_savestate(EMUFILE* os)
{
	u32 version = 8;
	write32le(version,os);
	
	//version 2:
	MMU_new.backupDevice.save_state(os);
	
	//version 3:
	MMU_new.gxstat.savestate(os);
	for(int i=0;i<2;i++)
		for(int j=0;j<4;j++)
			MMU_new.dma[i][j].savestate(os);

	MMU_timing.arm9codeFetch.savestate(os, version);
	MMU_timing.arm9dataFetch.savestate(os, version);
	MMU_timing.arm7codeFetch.savestate(os, version);
	MMU_timing.arm7dataFetch.savestate(os, version);
	MMU_timing.arm9codeCache.savestate(os, version);
	MMU_timing.arm9dataCache.savestate(os, version);

	//version 4:
	MMU_new.sqrt.savestate(os);
	MMU_new.div.savestate(os);

	//version 6:
	MMU_new.dsi_tsc.save_state(os);

	//version 8:
	os->write32le(MMU.fw.size);
	os->fwrite(MMU.fw.data,MMU.fw.size);
}

static bool mmu_loadstate(EMUFILE* is, int size)
{
	//read version
	u32 version;
	if(read32le(&version,is) != 1) return false;
	
	if(version == 0 || version == 1)
	{
		u32 bupmem_size;
		u32 addr_size;

		if(version == 0)
		{
			//version 0 was buggy and didnt save the type. 
			//it would silently fail if there was a size mismatch
			SAV_silent_fail_flag = true;
			if(read32le(&bupmem_size,is) != 1) return false;
			//if(bupmem_size != MMU.bupmem.size) return false; //mismatch between current initialized and saved size
			addr_size = BackupDevice::addr_size_for_old_save_size(bupmem_size);
		}
		else if(version == 1)
		{
			//version 1 reinitializes the save system with the type that was saved
			u32 bupmem_type;
			if(read32le(&bupmem_type,is) != 1) return false;
			if(read32le(&bupmem_size,is) != 1) return false;
			addr_size = BackupDevice::addr_size_for_old_save_type(bupmem_type);
			if(addr_size == 0xFFFFFFFF)
				addr_size = BackupDevice::addr_size_for_old_save_size(bupmem_size);
		}

		if(addr_size == 0xFFFFFFFF)
			return false;

		u8* temp = new u8[bupmem_size];
		is->fread((char*)temp,bupmem_size);
		MMU_new.backupDevice.load_old_state(addr_size,temp,bupmem_size);
		delete[] temp;
		if(is->fail()) return false;
	}

	if(version < 2) return true;

	bool ok = MMU_new.backupDevice.load_state(is);

	if(version < 3) return ok;

	ok &= MMU_new.gxstat.loadstate(is);
	
	for(int i=0;i<2;i++)
		for(int j=0;j<4;j++)
			ok &= MMU_new.dma[i][j].loadstate(is);

	ok &= MMU_timing.arm9codeFetch.loadstate(is, version);
	ok &= MMU_timing.arm9dataFetch.loadstate(is, version);
	ok &= MMU_timing.arm7codeFetch.loadstate(is, version);
	ok &= MMU_timing.arm7dataFetch.loadstate(is, version);
	ok &= MMU_timing.arm9codeCache.loadstate(is, version);
	ok &= MMU_timing.arm9dataCache.loadstate(is, version);

	if(version < 4) return ok;

	ok &= MMU_new.sqrt.loadstate(is,version);
	ok &= MMU_new.div.loadstate(is,version);

	//to prevent old savestates from confusing IF bits, mask out ones which had been stored but should have been generated
	MMU.reg_IF_bits[0] &= ~0x00200000;
	MMU.reg_IF_bits[1] &= ~0x00000000;

	MMU_new.gxstat.fifo_low = gxFIFO.size <= 127;
	MMU_new.gxstat.fifo_empty = gxFIFO.size == 0;

	if(version < 5)
		MMU.reg_DISP3DCNT_bits = T1ReadWord(MMU.ARM9_REG,0x60);

	if(version < 6) return ok;

	MMU_new.dsi_tsc.load_state(is);

	//version 6
	if(version < 7)
	{
		//recover WRAMCNT from the stashed WRAMSTAT memory location
		MMU.WRAMCNT = MMU.MMU_MEM[ARMCPU_ARM7][0x40][0x241];
	}

	if(version<8) return ok;

	//version 8:
	delete[] MMU.fw.data;
	MMU.fw.size = is->read32le();
	MMU.fw.data = new u8[size];
	is->fread(MMU.fw.data,MMU.fw.size);

	return ok;
}

static void cp15_savestate(EMUFILE* os)
{
	//version
	write32le(1,os);

	cp15.saveone(os);
	//ARM7 not have coprocessor
	//cp15_saveone((armcp15_t *)NDS_ARM7.coproc[15],os);
}

static bool cp15_loadstate(EMUFILE* is, int size)
{
	//read version
	u32 version;
	if(read32le(&version,is) != 1) return false;
	if(version > 1) return false;

	if(!cp15.loadone(is)) return false;
	
	if(version == 0)
	{
		//ARM7 not have coprocessor
		u8 *tmp_buf = new u8 [sizeof(armcp15_t)];
		if (!tmp_buf) return false;
		if(!cp15.loadone(is)) return false;
		delete [] tmp_buf;
		tmp_buf = NULL;
	}

	return true;
}



/* Format time and convert to string */
static char * format_time(time_t cal_time)
{
  struct tm *time_struct;
  static char str[64];

  time_struct=localtime(&cal_time);
  strftime(str, sizeof str, "%d-%b-%Y %H:%M:%S", time_struct);

  return(str);
}

void clear_savestates()
{
  u8 i;
  for( i = 0; i < NB_STATES; i++ )
    savestates[i].exists = FALSE;
}

// Scan for existing savestates and update struct
void scan_savestates()
{
  struct stat sbuf;
  char filename[MAX_PATH+1];

  clear_savestates();

  for(int i = 0; i < NB_STATES; i++ )
    {
     path.getpathnoext(path.STATES, filename);
	  
	  if (strlen(filename) + strlen(".dst") + strlen("-2147483648") /* = biggest string for i */ >MAX_PATH) return ;
      sprintf(filename+strlen(filename), ".ds%d", i);
      if( stat(filename,&sbuf) == -1 ) continue;
      savestates[i].exists = TRUE;
      strncpy(savestates[i].date, format_time(sbuf.st_mtime),40);
	  savestates[i].date[40-1] = '\0';
    }

  return ;
}

void savestate_slot(int num)
{
   struct stat sbuf;
   char filename[MAX_PATH+1];

	lastSaveState = num;		//Set last savestate used

    path.getpathnoext(path.STATES, filename);

   if (strlen(filename) + strlen(".dsx") + strlen("-2147483648") /* = biggest string for num */ >MAX_PATH) return ;
   sprintf(filename+strlen(filename), ".ds%d", num);

   if (savestate_save(filename))
   {
	   osd->setLineColor(255, 255, 255);
	   osd->addLine("Saved to %i slot", num);
   }
   else
   {
	   osd->setLineColor(255, 0, 0);
	   osd->addLine("Error saving %i slot", num);
	   return;
   }

   if (num >= 0 && num < NB_STATES)
   {
	   if (stat(filename,&sbuf) != -1)
	   {
		   savestates[num].exists = TRUE;
		   strncpy(savestates[num].date, format_time(sbuf.st_mtime),40);
		   savestates[num].date[40-1] = '\0';
	   }
   }
}

void loadstate_slot(int num)
{
   char filename[MAX_PATH];

   lastSaveState = num;		//Set last savestate used

    path.getpathnoext(path.STATES, filename);

   if (strlen(filename) + strlen(".dsx") + strlen("-2147483648") /* = biggest string for num */ >MAX_PATH) return ;
   sprintf(filename+strlen(filename), ".ds%d", num);
   if (savestate_load(filename))
   {
	   osd->setLineColor(255, 255, 255);
	   osd->addLine("Loaded from %i slot", num);
   }
   else
   {
	   osd->setLineColor(255, 0, 0);
	   osd->addLine("Error loading %i slot", num);
   }
}


// note: guessSF is so we don't have to do a linear search through the SFORMAT array every time
// in the (most common) case that we already know where the next entry is.
static const SFORMAT *CheckS(const SFORMAT *guessSF, const SFORMAT *firstSF, u32 size, u32 count, char *desc)
{
	const SFORMAT *sf = guessSF ? guessSF : firstSF;
	while(sf->v)
	{
		//NOT SUPPORTED RIGHT NOW
		//if(sf->size==~0)		// Link to another SFORMAT structure.
		//{
		//	SFORMAT *tmp;
		//	if((tmp= CheckS((SFORMAT *)sf->v, tsize, desc) ))
		//		return(tmp);
		//	sf++;
		//	continue;
		//}
		if(!memcmp(desc,sf->desc,4))
		{
			if(sf->size != size || sf->count != count)
				return 0;
			return sf;
		}

		// failed to find it, have to keep iterating
		if(guessSF)
		{
			sf = firstSF;
			guessSF = NULL;
		}
		else
		{
			sf++;
		}
	}
	return 0;
}


static bool ReadStateChunk(EMUFILE* is, const SFORMAT *sf, int size)
{
	const SFORMAT *tmp = NULL;
	const SFORMAT *guessSF = NULL;
	int temp = is->ftell();

	while(is->ftell()<temp+size)
	{
		u32 sz, count;

		char toa[4];
		is->fread(toa,4);
		if(is->fail())
			return false;

		if(!read32le(&sz,is)) return false;
		if(!read32le(&count,is)) return false;

		if((tmp=CheckS(guessSF,sf,sz,count,toa)))
		{
		#ifdef LOCAL_LE
			// no need to ever loop one at a time if not flipping byte order
			is->fread((char *)tmp->v,sz*count);
		#else
			if(sz == 1) {
				//special case: read a huge byte array
				is->fread((char *)tmp->v,count);
			} else {
				for(unsigned int i=0;i<count;i++)
				{
					is->fread((char *)tmp->v + i*sz,sz);
                    FlipByteOrder((u8*)tmp->v + i*sz,sz);
				}
			}
		#endif
			guessSF = tmp + 1;
		}
		else
		{
			is->fseek(sz*count,SEEK_CUR);
			guessSF = NULL;
		}
	} // while(...)
	return true;
}



static int SubWrite(EMUFILE* os, const SFORMAT *sf)
{
	uint32 acc=0;

#ifdef DEBUG
	std::set<std::string> keyset;
#endif

	const SFORMAT* temp = sf;
	while(temp->v) {
		const SFORMAT* seek = sf;
		while(seek->v && seek != temp) {
			if(!strcmp(seek->desc,temp->desc)) {
				printf("ERROR! duplicated chunk name: %s\n", temp->desc);
			}
			seek++;
		}
		temp++;
	}

	while(sf->v)
	{
		//not supported right now
		//if(sf->size==~0)		//Link to another struct
		//{
		//	uint32 tmp;

		//	if(!(tmp=SubWrite(os,(SFORMAT *)sf->v)))
		//		return(0);
		//	acc+=tmp;
		//	sf++;
		//	continue;
		//}

		int count = sf->count;
		int size = sf->size;

        //add size of current node to the accumulator
		acc += 4 + sizeof(sf->size) + sizeof(sf->count);
		acc += count * size;

		if(os)			//Are we writing or calculating the size of this block?
		{
			os->fwrite(sf->desc,4);
			write32le(sf->size,os);
			write32le(sf->count,os);

			#ifdef DEBUG
			//make sure we dont dup any keys
			if(keyset.find(sf->desc) != keyset.end())
			{
				printf("duplicate save key!\n");
				assert(false);
			}
			keyset.insert(sf->desc);
			#endif


		#ifdef LOCAL_LE
			// no need to ever loop one at a time if not flipping byte order
			os->fwrite((char *)sf->v,size*count);
		#else
			if(size == 1) {
				//special case: write a huge byte array
				os->fwrite((char *)sf->v,count);
			} else {
				for(int i=0;i<count;i++) {
					FlipByteOrder((u8*)sf->v + i*size, size);
					os->fwrite((char*)sf->v + i*size,size);
					//Now restore the original byte order.
					FlipByteOrder((u8*)sf->v + i*size, size);
				}
			}
		#endif
		}
		sf++;
	}

	return(acc);
}

static int savestate_WriteChunk(EMUFILE* os, int type, const SFORMAT *sf)
{
	write32le(type,os);
	if(!sf) return 4;
	int bsize = SubWrite((EMUFILE*)0,sf);
	write32le(bsize,os);

	if(!SubWrite(os,sf))
	{
		return 8;
	}
	return (bsize+8);
}

static void savestate_WriteChunk(EMUFILE* os, int type, void (*saveproc)(EMUFILE* os))
{
	u32 pos1 = os->ftell();

	//write the type, size(placeholder), and data
	write32le(type,os);
	os->fseek(4, SEEK_CUR); // skip the size, we write that later
	saveproc(os);

	//get the size
	u32 pos2 = os->ftell();
	assert(pos2 != (u32)-1); // if this assert fails, saveproc did something bad
	u32 size = (pos2 - pos1) - (2 * sizeof(u32));

	//fill in the actual size
	os->fseek(pos1 + sizeof(u32),SEEK_SET);
	write32le(size,os);
	os->fseek(pos2,SEEK_SET);

/*
// old version of this function,
// for reference in case the new one above starts misbehaving somehow:

	// - this is retarded. why not write placeholders for size and then write directly to the stream
	//and then go back and fill them in

	//get the size
	memorystream mstemp;
	saveproc(&mstemp);
	mstemp.flush();
	u32 size = mstemp.size();

	//write the type, size, and data
	write32le(type,os);
	write32le(size,os);
	os->write(mstemp.buf(),size);
*/
}

static void writechunks(EMUFILE* os);

bool savestate_save(EMUFILE* outstream, int compressionLevel)
{
#ifdef HAVE_JIT 
	arm_jit_sync();
#endif
	#ifndef HAVE_LIBZ
	compressionLevel = Z_NO_COMPRESSION;
	#endif

	EMUFILE_MEMORY ms;
	EMUFILE* os;
	
	if(compressionLevel != Z_NO_COMPRESSION)
	{
		//generate the savestate in memory first
		os = (EMUFILE*)&ms;
		writechunks(os);
	}
	else
	{
		os = outstream;
		os->fseek(32,SEEK_SET); //skip the header
		writechunks(os);
	}

	//save the length of the file
	u32 len = os->ftell();

	u32 comprlen = 0xFFFFFFFF;
	u8* cbuf;

	//compress the data
	int error = Z_OK;
	if(compressionLevel != Z_NO_COMPRESSION)
	{
		cbuf = ms.buf();
		uLongf comprlen2;
		//worst case compression.
		//zlib says "0.1% larger than sourceLen plus 12 bytes"
		comprlen = (len>>9)+12 + len;
		cbuf = new u8[comprlen];
		// Workaround to make it compile under linux 64bit
		comprlen2 = comprlen;
		error = compress2(cbuf,&comprlen2,ms.buf(),len,compressionLevel);
		comprlen = (u32)comprlen2;
	}

	//dump the header
	outstream->fseek(0,SEEK_SET);
	outstream->fwrite(magic,16);
	write32le(SAVESTATE_VERSION,outstream);
	write32le(EMU_DESMUME_VERSION_NUMERIC(),outstream); //desmume version
	write32le(len,outstream); //uncompressed length
	write32le(comprlen,outstream); //compressed length (-1 if it is not compressed)

	if(compressionLevel != Z_NO_COMPRESSION)
	{
		outstream->fwrite((char*)cbuf,comprlen==(u32)-1?len:comprlen);
		delete[] cbuf;
	}

	return error == Z_OK;
}

bool savestate_save (const char *file_name)
{
	EMUFILE_MEMORY ms;
	size_t elems_written;
#ifdef HAVE_LIBZ
	if(!savestate_save(&ms, Z_DEFAULT_COMPRESSION))
#else
	if(!savestate_save(&ms, 0))
#endif
		return false;
	FILE* file = fopen(file_name,"wb");
	if(file)
	{
		elems_written = fwrite(ms.buf(),1,ms.size(),file);
		fclose(file);
		return (elems_written == ms.size());
	} else return false;
}

static void writechunks(EMUFILE* os) {

	DateTime tm = DateTime::get_Now();
	svn_rev = EMU_DESMUME_SUBVERSION_NUMERIC();

	save_time = tm.get_Ticks();

	savestate_WriteChunk(os,1,SF_ARM9);
	savestate_WriteChunk(os,2,SF_ARM7);
	savestate_WriteChunk(os,3,cp15_savestate);
	savestate_WriteChunk(os,4,SF_MEM);
	savestate_WriteChunk(os,5,SF_NDS);
	savestate_WriteChunk(os,51,nds_savestate);
	savestate_WriteChunk(os,60,SF_MMU);
	savestate_WriteChunk(os,61,mmu_savestate);
	savestate_WriteChunk(os,7,gpu_savestate);
	savestate_WriteChunk(os,8,spu_savestate);
	savestate_WriteChunk(os,81,mic_savestate);
	savestate_WriteChunk(os,90,SF_GFX3D);
	savestate_WriteChunk(os,91,gfx3d_savestate);
	savestate_WriteChunk(os,100,SF_MOVIE);
	savestate_WriteChunk(os,101,mov_savestate);
	savestate_WriteChunk(os,110,SF_WIFI);
	savestate_WriteChunk(os,120,SF_RTC);
	savestate_WriteChunk(os,130,SF_NDS_INFO);
	savestate_WriteChunk(os,140,s_slot1_savestate);
	savestate_WriteChunk(os,150,s_slot2_savestate);
	// reserved for future versions
	savestate_WriteChunk(os,160,reserveChunks);
	savestate_WriteChunk(os,170,reserveChunks);
	savestate_WriteChunk(os,180,reserveChunks);
	// ============================
	savestate_WriteChunk(os,0xFFFFFFFF,(SFORMAT*)0);
}

static bool ReadStateChunks(EMUFILE* is, s32 totalsize)
{
	bool ret = true;
	bool haveInfo = false;
	
	s64 save_time = 0;
	u32 romsize = 0;
	u8 version_major = 0;
	u8 version_minor = 0;
	u8 version_build = 0;
	
	NDS_header header;
	SFORMAT SF_INFO[]={
		{ "GINF", 1, sizeof(header), &header},
		{ "GRSZ", 1, 4, &romsize},
		{ "DVMJ", 1, 1, &version_major},
		{ "DVMI", 1, 1, &version_minor},
		{ "DSBD", 1, 1, &version_build},
		{ "GREV", 1, 4, &svn_rev},
		{ "GTIM", 1, 8, &save_time},
		{ 0 }
	};
	memset(&header, 0, sizeof(header));

	while(totalsize > 0)
	{
		u32 size = 0;
		u32 t = 0;
		if(!read32le(&t,is))  { ret=false; break; }
		if(t == 0xFFFFFFFF) break;
		if(!read32le(&size,is))  { ret=false; break; }
		switch(t)
		{
			case 1: if(!ReadStateChunk(is,SF_ARM9,size)) ret=false; break;
			case 2: if(!ReadStateChunk(is,SF_ARM7,size)) ret=false; break;
			case 3: if(!cp15_loadstate(is,size)) ret=false; break;
			case 4: if(!ReadStateChunk(is,SF_MEM,size)) ret=false; break;
			case 5: if(!ReadStateChunk(is,SF_NDS,size)) ret=false; break;
			case 51: if(!nds_loadstate(is,size)) ret=false; break;
			case 60: if(!ReadStateChunk(is,SF_MMU,size)) ret=false; break;
			case 61: if(!mmu_loadstate(is,size)) ret=false; break;
			case 7: if(!gpu_loadstate(is,size)) ret=false; break;
			case 8: if(!spu_loadstate(is,size)) ret=false; break;
			case 81: if(!mic_loadstate(is,size)) ret=false; break;
			case 90: if(!ReadStateChunk(is,SF_GFX3D,size)) ret=false; break;
			case 91: if(!gfx3d_loadstate(is,size)) ret=false; break;
			case 100: if(!ReadStateChunk(is,SF_MOVIE, size)) ret=false; break;
			case 101: if(!mov_loadstate(is, size)) ret=false; break;
			case 110: if(!ReadStateChunk(is,SF_WIFI,size)) ret=false; break;
			case 120: if(!ReadStateChunk(is,SF_RTC,size)) ret=false; break;
			case 130: if(!ReadStateChunk(is,SF_INFO,size)) ret=false; else haveInfo=true; break;
			case 140: if(!s_slot1_loadstate(is, size)) ret=false; break;
			case 150: if(!s_slot2_loadstate(is, size)) ret=false; break;
			// reserved for future versions
			case 160:
			case 170:
			case 180:
				if(!ReadStateChunk(is,reserveChunks,size)) ret=false;
			break;
			// ============================
				
			default:
				return false;
		}
		if(!ret)
			return false;
	}

	if (haveInfo)
	{
		char buf[14] = {0};
		memset(&buf[0], 0, sizeof(buf));
		memcpy(buf, header.gameTile, sizeof(header.gameTile));
		printf("Savestate info:\n");
		if (version_major | version_minor | version_build)
		{
			char buf[32] = {0};
			if (svn_rev != 0xFFFFFFFF)
				sprintf(buf, " svn %u", svn_rev);
			printf("\tDeSmuME version: %u.%u.%u%s\n", version_major, version_minor, version_build, buf);
		}

		if (save_time)
		{
			static const char *wday[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
			DateTime tm = save_time;
			printf("\tSave created: %04d-%03s-%02d %s %02d:%02d:%02d\n", tm.get_Year(), DateTime::GetNameOfMonth(tm.get_Month()), tm.get_Day(), wday[tm.get_DayOfWeek()%7], tm.get_Hour(), tm.get_Minute(), tm.get_Second());
		}
		printf("\tGame title: %s\n", buf);
		printf("\tGame code: %c%c%c%c\n", header.gameCode[3], header.gameCode[2], header.gameCode[1], header.gameCode[0]);
		printf("\tMaker code: %c%c (0x%04X) - %s\n", header.makerCode & 0xFF, header.makerCode >> 8, header.makerCode, getDeveloperNameByID(header.makerCode).c_str());
		printf("\tDevice capacity: %dMb (real size %dMb)\n", ((128 * 1024) << header.cardSize) / (1024 * 1024), romsize / (1024 * 1024));
		printf("\tCRC16: %04Xh\n", header.CRC16);
		printf("\tHeader CRC16: %04Xh\n", header.headerCRC16);
		printf("\tSlot1: %s\n", slot1_List[slot1Type]->info()->name());
		printf("\tSlot2: %s\n", slot2_List[slot2Type]->info()->name());

		if (gameInfo.romsize != romsize || memcmp(&gameInfo.header, &header, sizeof(header)) != 0)
			msgbox->warn("The savestate you are loading does not match the ROM you are running.\nYou should find the correct ROM");
	}

	return ret;
}

static void loadstate()
{
    // This should regenerate the vram banks
    for (int i = 0; i < 0xA; i++)
       _MMU_write08<ARMCPU_ARM9>(0x04000240+i, _MMU_read08<ARMCPU_ARM9>(0x04000240+i));

    // This should regenerate the graphics power control register
    _MMU_write16<ARMCPU_ARM9>(0x04000304, _MMU_read16<ARMCPU_ARM9>(0x04000304));

	// This should regenerate the graphics configuration
	//zero 27-jul-09 : was formerly up to 7F but that wrote to dispfifo which is dumb (one of nitsuja's desynch bugs [that he found, not caused])
	//so then i brought it down to 66 but this resulted in a conceptual bug with affine start registers, which shouldnt get regenerated
	//so then i just made this exhaustive list
 //   for (int i = REG_BASE_DISPA; i<=REG_BASE_DISPA + 0x66; i+=2)
	//_MMU_write16<ARMCPU_ARM9>(i, _MMU_read16<ARMCPU_ARM9>(i));
 //   for (int i = REG_BASE_DISPB; i<=REG_BASE_DISPB + 0x7F; i+=2)
	//_MMU_write16<ARMCPU_ARM9>(i, _MMU_read16<ARMCPU_ARM9>(i));
	static const u8 mainRegenAddr[] = {0x00,0x02,0x08,0x0a,0x0c,0x0e,0x40,0x42,0x44,0x46,0x48,0x4a,0x4c,0x50,0x52,0x54,0x64,0x66,0x6c};
	static const u8 subRegenAddr[] =  {0x00,0x02,0x08,0x0a,0x0c,0x0e,0x40,0x42,0x44,0x46,0x48,0x4a,0x4c,0x50,0x52,0x54,0x6c};
	for(u32 i=0;i<ARRAY_SIZE(mainRegenAddr);i++)
		_MMU_write16<ARMCPU_ARM9>(REG_BASE_DISPA+mainRegenAddr[i], _MMU_read16<ARMCPU_ARM9>(REG_BASE_DISPA+mainRegenAddr[i]));
	for(u32 i=0;i<ARRAY_SIZE(subRegenAddr);i++)
		_MMU_write16<ARMCPU_ARM9>(REG_BASE_DISPB+subRegenAddr[i], _MMU_read16<ARMCPU_ARM9>(REG_BASE_DISPB+subRegenAddr[i]));
	// no need to restore 0x60 since control and MMU.ARM9_REG are both in the savestates, and restoring it could mess up the ack bits anyway

	SetupMMU(nds.Is_DebugConsole(),nds.Is_DSI());

	execute = !driver->EMU_IsEmulationPaused();
}

bool savestate_load(EMUFILE* is)
{
	SAV_silent_fail_flag = false;
	char header[16];
	is->fread(header,16);
	if(is->fail() || memcmp(header,magic,16))
		return false;

	u32 ssversion,len,comprlen;
	if(!read32le(&ssversion,is)) return false;
	if(!read32le(&_DESMUME_version,is)) return false;
	if(!read32le(&len,is)) return false;
	if(!read32le(&comprlen,is)) return false;

	if(ssversion != SAVESTATE_VERSION) return false;

	std::vector<u8> buf(len);

	if(comprlen != 0xFFFFFFFF) {
#ifndef HAVE_LIBZ
		//without libz, we can't decompress this savestate
		return false;
#endif
		std::vector<char> cbuf(comprlen);
		is->fread(&cbuf[0],comprlen);
		if(is->fail()) return false;

#ifdef HAVE_LIBZ
		uLongf uncomprlen = len;
		int error = uncompress((uint8*)&buf[0],&uncomprlen,(uint8*)&cbuf[0],comprlen);
		if(error != Z_OK || uncomprlen != len)
			return false;
#endif
	} else {
		is->fread((char*)&buf[0],len-32);
	}

	//GO!! READ THE SAVESTATE
	//THERE IS NO GOING BACK NOW
	//reset the emulator first to clean out the host's state

	//while the series of resets below should work,
	//we are testing the robustness of the savestate system with this full reset.
	//the full reset wipes more things, so we can make sure that they are being restored correctly
	extern bool _HACK_DONT_STOPMOVIE;
	_HACK_DONT_STOPMOVIE = true;
	NDS_Reset();
	_HACK_DONT_STOPMOVIE = false;

	//reset some options to their old defaults which werent saved
	nds._DebugConsole = FALSE;

	//GPU_Reset(MainScreen.gpu, 0);
	//GPU_Reset(SubScreen.gpu, 1);
	//gfx3d_reset();
	//gpu3D->NDS_3D_Reset();
	//SPU_Reset();

	EMUFILE_MEMORY mstemp(&buf);
	bool x = ReadStateChunks(&mstemp,(s32)len);

	if(!x && !SAV_silent_fail_flag)
	{
		msgbox->error("Error loading savestate. It failed halfway through;\nSince there is no savestate backup system, your current game session is wrecked");
		return false;
	}

	loadstate();

	if(nds.ConsoleType != CommonSettings.ConsoleType) {
		printf("WARNING: forcing console type to: ConsoleType=%d\n",nds.ConsoleType);
	}

	if((nds._DebugConsole!=0) != CommonSettings.DebugConsole) {
			printf("WARNING: forcing console debug mode to: debugmode=%s\n",nds._DebugConsole?"TRUE":"FALSE");
	}


	return true;
}

bool savestate_load(const char *file_name)
{
	EMUFILE_FILE f(file_name,"rb");
	if(f.fail()) return false;

	return savestate_load(&f);
}

static std::stack<EMUFILE_MEMORY*> rewindFreeList;
static std::vector<EMUFILE_MEMORY*> rewindbuffer;

int rewindstates = 16;
int rewindinterval = 4;

void rewindsave () {

	if(currFrameCounter % rewindinterval)
		return;

	//printf("rewindsave"); printf("%d%s", currFrameCounter, "\n");

	
	EMUFILE_MEMORY *ms;
	if(!rewindFreeList.empty()) {
		ms = rewindFreeList.top();
		rewindFreeList.pop();
	} else {
		ms = new EMUFILE_MEMORY(1024*1024*12);
	}

	if(!savestate_save(ms, Z_NO_COMPRESSION))
		return;

	rewindbuffer.push_back(ms);
	
	if((int)rewindbuffer.size() > rewindstates) {
		delete *rewindbuffer.begin();
		rewindbuffer.erase(rewindbuffer.begin());
	}
}

void dorewind()
{
	if(currFrameCounter % rewindinterval)
		return;

	//printf("rewind\n");

	int size = rewindbuffer.size();

	if(size < 1) {
		printf("rewind buffer empty\n");
		return;
	}

	printf("%d", size);

	EMUFILE_MEMORY* loadms = rewindbuffer[size-1];
	loadms->fseek(32, SEEK_SET);

	ReadStateChunks(loadms,loadms->size()-32);
	loadstate();

	if(rewindbuffer.size()>1)
	{
		rewindFreeList.push(loadms);
		rewindbuffer.pop_back();
	}

}
