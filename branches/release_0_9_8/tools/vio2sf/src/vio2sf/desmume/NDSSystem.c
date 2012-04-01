/*  Copyright (C) 2006 yopyop
    yopyop156@ifrance.com
    yopyop156.ifrance.com

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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <string.h>
#include <stdlib.h>

#include "NDSSystem.h"
#include "MMU.h"
//#include "cflash.h"
#include "spu_exports.h"

//#include "ROMReader.h"

/* the count of bytes copied from the firmware into memory */
#define NDS_FW_USER_SETTINGS_MEM_BYTE_COUNT 0x70

NDSSystem nds;

static u32
calc_CRC16( u32 start, const u8 *data, int count) {
  int i,j;
  u32 crc = start & 0xffff;
  static u16 val[] = { 0xC0C1,0xC181,0xC301,0xC601,0xCC01,0xD801,0xF001,0xA001 };
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
     MMU_Init();
     nds.nextHBlank = 3168;
     nds.VCount = 0;
     nds.lignerendu = FALSE;

     if (Screen_Init(GFXCORE_DUMMY) != 0)
        return -1;
     
 #ifdef GDB_STUB
     armcpu_new(&NDS_ARM7,1, arm7_mem_if, arm7_ctrl_iface);
     armcpu_new(&NDS_ARM9,0, arm9_mem_if, arm9_ctrl_iface);
#else
	 armcpu_new(&NDS_ARM7,1);
     armcpu_new(&NDS_ARM9,0);
#endif

     if (SPU_Init(0, 0) != 0)
        return -1;

#ifdef EXPERIMENTAL_WIFI
	 WIFI_Init(&wifiMac) ;
#endif

     return 0;
}

static void armcpu_deinit(armcpu_t *armcpu)
{
	if(armcpu->coproc[15])
	{
		free(armcpu->coproc[15]);
		armcpu->coproc[15] = 0;
	}
}

void NDS_DeInit(void) {
     if(MMU.CART_ROM != MMU.UNUSED_RAM)
        NDS_FreeROM();

	armcpu_deinit(&NDS_ARM7);
	armcpu_deinit(&NDS_ARM9);

     nds.nextHBlank = 3168;
     SPU_DeInit();
     Screen_DeInit();
     MMU_DeInit();
}

BOOL NDS_SetROM(u8 * rom, u32 mask)
{
     MMU_setRom(rom, mask);

     return TRUE;
} 

NDS_header * NDS_getROMHeader(void)
{
	NDS_header * header = malloc(sizeof(NDS_header));

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

     //return (NDS_header *)MMU.CART_ROM;
} 



void NDS_FreeROM(void)
{
   if (MMU.CART_ROM != MMU.UNUSED_RAM)
      free(MMU.CART_ROM);
   MMU_unsetRom();
//   if (MMU.bupmem.fp)
//      fclose(MMU.bupmem.fp);
//   MMU.bupmem.fp = NULL;
}

                         

void NDS_Reset( void)
{
   BOOL oldexecute=execute;
   int i;
   u32 src;
   u32 dst;
   NDS_header * header = NDS_getROMHeader();

	if (!header) return ;

   execute = FALSE;

   MMU_clearMem();

   src = header->ARM9src;
   dst = header->ARM9cpy;

   for(i = 0; i < (header->ARM9binSize>>2); ++i)
   {
      MMU_write32(0, dst, T1ReadLong(MMU.CART_ROM, src));
      dst += 4;
      src += 4;
   }

   src = header->ARM7src;
   dst = header->ARM7cpy;
     
   for(i = 0; i < (header->ARM7binSize>>2); ++i)
   {
      MMU_write32(1, dst, T1ReadLong(MMU.CART_ROM, src));
      dst += 4;
      src += 4;
   }

   armcpu_init(&NDS_ARM7, header->ARM7exe);
   armcpu_init(&NDS_ARM9, header->ARM9exe);
     
   nds.ARM9Cycle = 0;
   nds.ARM7Cycle = 0;
   nds.cycles = 0;
   memset(nds.timerCycle, 0, sizeof(s32) * 2 * 4);
   memset(nds.timerOver, 0, sizeof(BOOL) * 2 * 4);
   nds.nextHBlank = 3168;
   nds.VCount = 0;
   nds.old = 0;
   nds.diff = 0;
   nds.lignerendu = FALSE;
   nds.touchX = nds.touchY = 0;

   MMU_write16(0, 0x04000130, 0x3FF);
   MMU_write16(1, 0x04000130, 0x3FF);
   MMU_write8(1, 0x04000136, 0x43);

   /*
    * Setup a copy of the firmware user settings in memory.
    * (this is what the DS firmware would do).
    */
   {
     u8 temp_buffer[NDS_FW_USER_SETTINGS_MEM_BYTE_COUNT];
     int fw_index;

     if ( copy_firmware_user_data( temp_buffer, MMU.fw.data)) {
       for ( fw_index = 0; fw_index < NDS_FW_USER_SETTINGS_MEM_BYTE_COUNT; fw_index++) {
         MMU_write8( 0, 0x027FFC80 + fw_index, temp_buffer[fw_index]);
       }
     }
   }

	// Copy the whole header to Main RAM 0x27FFE00 on startup.
	//  Reference: http://nocash.emubase.de/gbatek.htm#dscartridgeheader
	for (i = 0; i < ((0x170+0x90)/4); i++)
	{
		MMU_write32 (0, 0x027FFE00+i*4, LE_TO_LOCAL_32(((u32*)MMU.CART_ROM)[i]));
	}
     
   MainScreen.offset = 0;
   SubScreen.offset = 192;
     
   //MMU_write32(0, 0x02007FFC, 0xE92D4030);

     //ARM7 BIOS IRQ HANDLER
     MMU_write32(1, 0x00, 0xE25EF002);
     MMU_write32(1, 0x04, 0xEAFFFFFE);
     MMU_write32(1, 0x18, 0xEA000000);
     MMU_write32(1, 0x20, 0xE92D500F);
     MMU_write32(1, 0x24, 0xE3A00301);
     MMU_write32(1, 0x28, 0xE28FE000);
     MMU_write32(1, 0x2C, 0xE510F004);
     MMU_write32(1, 0x30, 0xE8BD500F);
     MMU_write32(1, 0x34, 0xE25EF004);
    
     //ARM9 BIOS IRQ HANDLER
     MMU_write32(0, 0xFFFF0018, 0xEA000000);
     MMU_write32(0, 0xFFFF0020, 0xE92D500F);
     MMU_write32(0, 0xFFFF0024, 0xEE190F11);
     MMU_write32(0, 0xFFFF0028, 0xE1A00620);
     MMU_write32(0, 0xFFFF002C, 0xE1A00600);
     MMU_write32(0, 0xFFFF0030, 0xE2800C40);
     MMU_write32(0, 0xFFFF0034, 0xE28FE000);
     MMU_write32(0, 0xFFFF0038, 0xE510F004);
     MMU_write32(0, 0xFFFF003C, 0xE8BD500F);
     MMU_write32(0, 0xFFFF0040, 0xE25EF004);
        
     MMU_write32(0, 0x0000004, 0xE3A0010E);
     MMU_write32(0, 0x0000008, 0xE3A01020);
//     MMU_write32(0, 0x000000C, 0xE1B02110);
     MMU_write32(0, 0x000000C, 0xE1B02040);
     MMU_write32(0, 0x0000010, 0xE3B02020);
//     MMU_write32(0, 0x0000010, 0xE2100202);

   free(header);

   GPU_Reset(MainScreen.gpu, 0);
   GPU_Reset(SubScreen.gpu, 1);
   SPU_Reset();

   execute = oldexecute;
}

static void dma_check(void)
{
	if((MMU.DMAing[0][0])&&(MMU.DMACycle[0][0]<=nds.cycles))
	{
		T1WriteLong(ARM9Mem.ARM9_REG, 0xB8 + (0xC*0), T1ReadLong(ARM9Mem.ARM9_REG, 0xB8 + (0xC*0)) & 0x7FFFFFFF);
		if((MMU.DMACrt[0][0])&(1<<30)) NDS_makeARM9Int(8);
		MMU.DMAing[0][0] = FALSE;
	}

	if((MMU.DMAing[0][1])&&(MMU.DMACycle[0][1]<=nds.cycles))
	{
		T1WriteLong(ARM9Mem.ARM9_REG, 0xB8 + (0xC*1), T1ReadLong(ARM9Mem.ARM9_REG, 0xB8 + (0xC*1)) & 0x7FFFFFFF);
		if((MMU.DMACrt[0][1])&(1<<30)) NDS_makeARM9Int(9);
		MMU.DMAing[0][1] = FALSE;
	}

	if((MMU.DMAing[0][2])&&(MMU.DMACycle[0][2]<=nds.cycles))
	{
		T1WriteLong(ARM9Mem.ARM9_REG, 0xB8 + (0xC*2), T1ReadLong(ARM9Mem.ARM9_REG, 0xB8 + (0xC*2)) & 0x7FFFFFFF);
		if((MMU.DMACrt[0][2])&(1<<30)) NDS_makeARM9Int(10);
		MMU.DMAing[0][2] = FALSE;
	}

	if((MMU.DMAing[0][3])&&(MMU.DMACycle[0][3]<=nds.cycles))
	{
		T1WriteLong(ARM9Mem.ARM9_REG, 0xB8 + (0xC*3), T1ReadLong(ARM9Mem.ARM9_REG, 0xB8 + (0xC*3)) & 0x7FFFFFFF);
		if((MMU.DMACrt[0][3])&(1<<30)) NDS_makeARM9Int(11);
		MMU.DMAing[0][3] = FALSE;
	}

	if((MMU.DMAing[1][0])&&(MMU.DMACycle[1][0]<=nds.cycles))
	{
		T1WriteLong(MMU.ARM7_REG, 0xB8 + (0xC*0), T1ReadLong(MMU.ARM7_REG, 0xB8 + (0xC*0)) & 0x7FFFFFFF);
		if((MMU.DMACrt[1][0])&(1<<30)) NDS_makeARM7Int(8);
		MMU.DMAing[1][0] = FALSE;
	}

	if((MMU.DMAing[1][1])&&(MMU.DMACycle[1][1]<=nds.cycles))
	{
		T1WriteLong(MMU.ARM7_REG, 0xB8 + (0xC*1), T1ReadLong(MMU.ARM7_REG, 0xB8 + (0xC*1)) & 0x7FFFFFFF);
		if((MMU.DMACrt[1][1])&(1<<30)) NDS_makeARM7Int(9);
		MMU.DMAing[1][1] = FALSE;
	}

	if((MMU.DMAing[1][2])&&(MMU.DMACycle[1][2]<=nds.cycles))
	{
		T1WriteLong(MMU.ARM7_REG, 0xB8 + (0xC*2), T1ReadLong(MMU.ARM7_REG, 0xB8 + (0xC*2)) & 0x7FFFFFFF);
		if((MMU.DMACrt[1][2])&(1<<30)) NDS_makeARM7Int(10);
		MMU.DMAing[1][2] = FALSE;
	}

	if((MMU.DMAing[1][3])&&(MMU.DMACycle[1][3]<=nds.cycles))
	{
		T1WriteLong(MMU.ARM7_REG, 0xB8 + (0xC*3), T1ReadLong(MMU.ARM7_REG, 0xB8 + (0xC*3)) & 0x7FFFFFFF);
		if((MMU.DMACrt[1][3])&(1<<30)) NDS_makeARM7Int(11);
		MMU.DMAing[1][3] = FALSE;
	}

	if((MMU.reg_IF[0]&MMU.reg_IE[0]) && (MMU.reg_IME[0]))
	{
#ifdef GDB_STUB
		if ( armcpu_flagIrq( &NDS_ARM9)) 
#else
		if ( armcpu_irqExeption(&NDS_ARM9))
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
		if ( armcpu_irqExeption(&NDS_ARM7))
#endif
		{
			nds.ARM7Cycle = nds.cycles;
		}
	}

}

static void timer_check(void)
{
	int p, t;
	for (p = 0; p < 2; p++)
	{
		for (t = 0; t < 4; t++)
		{
			nds.timerOver[p][t] = 0;
			if(MMU.timerON[p][t])
			{
				if(MMU.timerRUN[p][t])
				{
					switch(MMU.timerMODE[p][t])
					{
					case 0xFFFF :
						if(t > 0 && nds.timerOver[p][t - 1])
						{
							++(MMU.timer[p][t]);
							nds.timerOver[p][t] = !MMU.timer[p][t];
							if (nds.timerOver[p][t])
							{
								if (p == 0)
								{
									if(T1ReadWord(ARM9Mem.ARM9_REG, 0x102 + (t << 2)) & 0x40)
										NDS_makeARM9Int(3 + t);
								}
								else
								{
									if(T1ReadWord(MMU.ARM7_REG, 0x102 + (t << 2)) & 0x40)
										NDS_makeARM7Int(3 + t);
								}
								MMU.timer[p][t] = MMU.timerReload[p][t];
							}
						}
						break;
					default :
						{
							nds.diff = (nds.cycles >> MMU.timerMODE[p][t]) - (nds.timerCycle[p][t] >> MMU.timerMODE[p][t]);
							nds.old = MMU.timer[p][t];
							MMU.timer[p][t] += nds.diff;
							nds.timerCycle[p][t] += nds.diff << MMU.timerMODE[p][t];
							nds.timerOver[p][t] = nds.old >= MMU.timer[p][t];
							if(nds.timerOver[p][t])
							{
								if (p == 0)
								{
									if(T1ReadWord(ARM9Mem.ARM9_REG, 0x102 + (t << 2)) & 0x40)
										NDS_makeARM9Int(3 + t);
								}
								else
								{
									if(T1ReadWord(MMU.ARM7_REG, 0x102 + (t << 2)) & 0x40)
										NDS_makeARM7Int(3 + t);
								}
								MMU.timer[p][t] = MMU.timerReload[p][t] + MMU.timer[p][t] - nds.old;
							}
						}
						break;
					}
				}
				else
				{
					MMU.timerRUN[p][t] = TRUE;
					nds.timerCycle[p][t] = nds.cycles;
				}
			}
		}
	}
}

void NDS_exec_hframe(int cpu_clockdown_level_arm9, int cpu_clockdown_level_arm7)
{
	int h;
	for (h = 0; h < 2; h++)
	{
		s32 nb = nds.cycles + (h ? (99 * 12) : (256 * 12));

		while (nb > nds.ARM9Cycle && !NDS_ARM9.waitIRQ)
			nds.ARM9Cycle += armcpu_exec(&NDS_ARM9) << (cpu_clockdown_level_arm9);
		if (NDS_ARM9.waitIRQ) nds.ARM9Cycle = nb;
		while (nb > nds.ARM7Cycle && !NDS_ARM7.waitIRQ)
			nds.ARM7Cycle += armcpu_exec(&NDS_ARM7) << (1 + (cpu_clockdown_level_arm7));
		if (NDS_ARM7.waitIRQ) nds.ARM7Cycle = nb;
		nds.cycles = (nds.ARM9Cycle<nds.ARM7Cycle)?nds.ARM9Cycle : nds.ARM7Cycle;

		/* HBLANK */
		if (h)
		{
			T1WriteWord(ARM9Mem.ARM9_REG, 4, T1ReadWord(ARM9Mem.ARM9_REG, 4) | 2);
			T1WriteWord(MMU.ARM7_REG, 4, T1ReadWord(MMU.ARM7_REG, 4) | 2);
			NDS_ARM9HBlankInt();
			NDS_ARM7HBlankInt();

			if(nds.VCount<192)
			{
				if(MMU.DMAStartTime[0][0] == 2)
					MMU_doDMA(0, 0);
				if(MMU.DMAStartTime[0][1] == 2)
					MMU_doDMA(0, 1);
				if(MMU.DMAStartTime[0][2] == 2)
					MMU_doDMA(0, 2);
				if(MMU.DMAStartTime[0][3] == 2)
					MMU_doDMA(0, 3);
			}
		}
		else
		{
			/* HDISP */
			u32 vmatch;

			nds.nextHBlank += 4260;
			++nds.VCount;
			T1WriteWord(ARM9Mem.ARM9_REG, 4, T1ReadWord(ARM9Mem.ARM9_REG, 4) & 0xFFFD);
			T1WriteWord(MMU.ARM7_REG, 4, T1ReadWord(MMU.ARM7_REG, 4) & 0xFFFD);

			if(MMU.DMAStartTime[0][0] == 3)
				MMU_doDMA(0, 0);
			if(MMU.DMAStartTime[0][1] == 3)
				MMU_doDMA(0, 1);
			if(MMU.DMAStartTime[0][2] == 3)
				MMU_doDMA(0, 2);
			if(MMU.DMAStartTime[0][3] == 3)
				MMU_doDMA(0, 3);

			// Main memory display
			if(MMU.DMAStartTime[0][0] == 4)
			{
				MMU_doDMA(0, 0);
				MMU.DMAStartTime[0][0] = 0;
			}
			if(MMU.DMAStartTime[0][1] == 4)
			{
				MMU_doDMA(0, 1);
				MMU.DMAStartTime[0][1] = 0;
			}
			if(MMU.DMAStartTime[0][2] == 4)
			{
				MMU_doDMA(0, 2);
				MMU.DMAStartTime[0][2] = 0;
			}
			if(MMU.DMAStartTime[0][3] == 4)
			{
				MMU_doDMA(0, 3);
				MMU.DMAStartTime[0][3] = 0;
			}

			if(MMU.DMAStartTime[1][0] == 4)
			{
				MMU_doDMA(1, 0);
				MMU.DMAStartTime[1][0] = 0;
			}
			if(MMU.DMAStartTime[1][1] == 4)
			{
				MMU_doDMA(1, 1);
				MMU.DMAStartTime[0][1] = 0;
			}
			if(MMU.DMAStartTime[1][2] == 4)
			{
				MMU_doDMA(1, 2);
				MMU.DMAStartTime[1][2] = 0;
			}
			if(MMU.DMAStartTime[1][3] == 4)
			{
				MMU_doDMA(1, 3);
				MMU.DMAStartTime[1][3] = 0;
			}
                            
			if(nds.VCount == 192)
			{
				/* VBLANK */
				T1WriteWord(ARM9Mem.ARM9_REG, 4, T1ReadWord(ARM9Mem.ARM9_REG, 4) | 1);
				T1WriteWord(MMU.ARM7_REG, 4, T1ReadWord(MMU.ARM7_REG, 4) | 1);
				NDS_ARM9VBlankInt();
				NDS_ARM7VBlankInt();

				if(MMU.DMAStartTime[0][0] == 1)
					MMU_doDMA(0, 0);
				if(MMU.DMAStartTime[0][1] == 1)
					MMU_doDMA(0, 1);
				if(MMU.DMAStartTime[0][2] == 1)
					MMU_doDMA(0, 2);
				if(MMU.DMAStartTime[0][3] == 1)
					MMU_doDMA(0, 3);

				if(MMU.DMAStartTime[1][0] == 1)
					MMU_doDMA(1, 0);
				if(MMU.DMAStartTime[1][1] == 1)
					MMU_doDMA(1, 1);
				if(MMU.DMAStartTime[1][2] == 1)
					MMU_doDMA(1, 2);
				if(MMU.DMAStartTime[1][3] == 1)
					MMU_doDMA(1, 3);
			}
			else if(nds.VCount == 263)
			{
				const int cycles_per_frame = (263 * (99 * 12 + 256 * 12));
				/* VDISP */
				nds.nextHBlank = 3168;
				nds.VCount = 0;
				T1WriteWord(ARM9Mem.ARM9_REG, 4, T1ReadWord(ARM9Mem.ARM9_REG, 4) & 0xFFFE);
				T1WriteWord(MMU.ARM7_REG, 4, T1ReadWord(MMU.ARM7_REG, 4) & 0xFFFE);

				nds.cycles -= cycles_per_frame;
				nds.ARM9Cycle -= cycles_per_frame;
				nds.ARM7Cycle -= cycles_per_frame;
				nb -= cycles_per_frame;
				if(MMU.timerON[0][0])
					nds.timerCycle[0][0] -= cycles_per_frame;
				if(MMU.timerON[0][1])
					nds.timerCycle[0][1] -= cycles_per_frame;
				if(MMU.timerON[0][2])
					nds.timerCycle[0][2] -= cycles_per_frame;
				if(MMU.timerON[0][3])
					nds.timerCycle[0][3] -= cycles_per_frame;

				if(MMU.timerON[1][0])
					nds.timerCycle[1][0] -= cycles_per_frame;
				if(MMU.timerON[1][1])
					nds.timerCycle[1][1] -= cycles_per_frame;
				if(MMU.timerON[1][2])
					nds.timerCycle[1][2] -= cycles_per_frame;
				if(MMU.timerON[1][3])
					nds.timerCycle[1][3] -= cycles_per_frame;
				if(MMU.DMAing[0][0])
					MMU.DMACycle[0][0] -= cycles_per_frame;
				if(MMU.DMAing[0][1])
					MMU.DMACycle[0][1] -= cycles_per_frame;
				if(MMU.DMAing[0][2])
					MMU.DMACycle[0][2] -= cycles_per_frame;
				if(MMU.DMAing[0][3])
					MMU.DMACycle[0][3] -= cycles_per_frame;
				if(MMU.DMAing[1][0])
					MMU.DMACycle[1][0] -= cycles_per_frame;
				if(MMU.DMAing[1][1])
					MMU.DMACycle[1][1] -= cycles_per_frame;
				if(MMU.DMAing[1][2])
					MMU.DMACycle[1][2] -= cycles_per_frame;
				if(MMU.DMAing[1][3])
					MMU.DMACycle[1][3] -= cycles_per_frame;

			}

			T1WriteWord(ARM9Mem.ARM9_REG, 6, nds.VCount);
			T1WriteWord(MMU.ARM7_REG, 6, nds.VCount);

			vmatch = T1ReadWord(ARM9Mem.ARM9_REG, 4);
			if((nds.VCount==(vmatch>>8)|((vmatch<<1)&(1<<8))))
			{
				T1WriteWord(ARM9Mem.ARM9_REG, 4, T1ReadWord(ARM9Mem.ARM9_REG, 4) | 4);
				if(T1ReadWord(ARM9Mem.ARM9_REG, 4) & 32)
					NDS_makeARM9Int(2);
			}
			else
				T1WriteWord(ARM9Mem.ARM9_REG, 4, T1ReadWord(ARM9Mem.ARM9_REG, 4) & 0xFFFB);

			vmatch = T1ReadWord(MMU.ARM7_REG, 4);
			if((nds.VCount==(vmatch>>8)|((vmatch<<1)&(1<<8))))
			{
				T1WriteWord(MMU.ARM7_REG, 4, T1ReadWord(MMU.ARM7_REG, 4) | 4);
				if(T1ReadWord(MMU.ARM7_REG, 4) & 32)
					NDS_makeARM7Int(2);
			}
			else
				T1WriteWord(MMU.ARM7_REG, 4, T1ReadWord(MMU.ARM7_REG, 4) & 0xFFFB);

			timer_check();
			dma_check();
		}
	}
}

void NDS_exec_frame(int cpu_clockdown_level_arm9, int cpu_clockdown_level_arm7)
{
	int v;
	for (v = 0; v < 263; v++)
	{
		NDS_exec_hframe(cpu_clockdown_level_arm9, cpu_clockdown_level_arm7);
	}
}

