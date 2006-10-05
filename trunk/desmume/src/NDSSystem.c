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

#include "NDSSystem.h"
#include <stdlib.h>

NDSSystem nds;

void NDSInit(void) {
     nds.ARM9Cycle = 0;
     nds.ARM7Cycle = 0;
     nds.cycles = 0;
     MMUInit();
     nds.nextHBlank = 3168;
     nds.VCount = 0;
     nds.lignerendu = FALSE;

     ScreenInit();
     
     armcpu_new(&NDS_ARM7,1);
     armcpu_new(&NDS_ARM9,0);
}

void NDSDeInit(void) {
     if(MMU.CART_ROM != MMU.UNUSED_RAM)
     {
		  // free(MMU.CART_ROM); IT'S UP TO THE GUI TO LOAD/UNLOAD AND MALLOC/FREE ROM >(
          MMU_unsetRom();
     }
     nds.nextHBlank = 3168;
     ScreenDeInit();
     MMUDeInit();
}

BOOL NDS_loadROM(u8 * rom, u32 mask)
{
     u32 i;

     if(MMU.CART_ROM != MMU.UNUSED_RAM)
     {
         // free(MMU.CART_ROM); IT'S UP TO THE GUI TO LOAD/UNLOAD AND MALLOC/FREE ROM >(
          MMU_unsetRom();
     }
     MMU_setRom(rom, mask);
     
     NDS_header * header = (NDS_header *)MMU.CART_ROM;
     
     u32 src = header->ARM9src>>2;
     u32 dst = header->ARM9cpy;

     for(i = 0; i < (header->ARM9binSize>>2); ++i)
     {
          MMU_writeWord(0, dst, ((u32 *)rom)[src]);
          dst+=4;
          ++src;
     }

     src = header->ARM7src>>2;
     dst = header->ARM7cpy;
     
     for(i = 0; i < (header->ARM7binSize>>2); ++i)
     {
          MMU_writeWord(1, dst, ((u32 *)rom)[src]);
          dst+=4;
          ++src;
     }
     armcpu_init(&NDS_ARM7, header->ARM7exe);
     armcpu_init(&NDS_ARM9, header->ARM9exe);
     
     nds.ARM9Cycle = 0;
     nds.ARM7Cycle = 0;
     nds.cycles = 0;
     nds.nextHBlank = 3168;
     nds.VCount = 0;
     nds.lignerendu = FALSE;

     MMU_writeHWord(0, 0x04000130, 0x3FF);
     MMU_writeHWord(1, 0x04000130, 0x3FF);
     MMU_writeByte(1, 0x04000136, 0x43);

     MMU_writeByte(0, 0x027FFCDC, 0x20);
     MMU_writeByte(0, 0x027FFCDD, 0x20);
     MMU_writeByte(0, 0x027FFCE2, 0xE0);
     MMU_writeByte(0, 0x027FFCE3, 0x80);
     
     MMU_writeHWord(0, 0x027FFCD8, 0x20<<4);
     MMU_writeHWord(0, 0x027FFCDA, 0x20<<4);
     MMU_writeHWord(0, 0x027FFCDE, 0xE0<<4);
     MMU_writeHWord(0, 0x027FFCE0, 0x80<<4);

     MMU_writeWord(0, 0x027FFE40, 0xE188);
     MMU_writeWord(0, 0x027FFE44, 0x9);
     MMU_writeWord(0, 0x027FFE48, 0xE194);
     MMU_writeWord(0, 0x027FFE4C, 0x0);
//     logcount = 0;
     
     MMU_writeByte(0, 0x023FFC80, 1);
     MMU_writeByte(0, 0x023FFC82, 10);
     MMU_writeByte(0, 0x023FFC83, 7);
     MMU_writeByte(0, 0x023FFC84, 15);
     
     MMU_writeHWord(0, 0x023FFC86, 'y');
     MMU_writeHWord(0, 0x023FFC88, 'o');
     MMU_writeHWord(0, 0x023FFC8A, 'p');
     MMU_writeHWord(0, 0x023FFC8C, 'y');
     MMU_writeHWord(0, 0x023FFC8E, 'o');
     MMU_writeHWord(0, 0x023FFC90, 'p');
     MMU_writeHWord(0, 0x023FFC9A, 6);
     
     MMU_writeHWord(0, 0x023FFC9C, 'H');
     MMU_writeHWord(0, 0x023FFC9E, 'i');
     MMU_writeHWord(0, 0x023FFCA0, ',');
     MMU_writeHWord(0, 0x023FFCA2, 'i');
     MMU_writeHWord(0, 0x023FFCA4, 't');
     MMU_writeHWord(0, 0x023FFCA6, '\'');
     MMU_writeHWord(0, 0x023FFCA8, 's');
     MMU_writeHWord(0, 0x023FFCAA, ' ');
     MMU_writeHWord(0, 0x023FFCAC, 'm');
     MMU_writeHWord(0, 0x023FFCAE, 'e');
     MMU_writeHWord(0, 0x023FFCB0, '!');
     MMU_writeHWord(0, 0x023FFCD0, 11);

     MMU_writeHWord(0, 0x023FFCE4, 2);
          
     MMU_writeWord(0, 0x027FFE40, header->FNameTblOff);
     MMU_writeWord(0, 0x027FFE44, header->FNameTblSize);
     MMU_writeWord(0, 0x027FFE48, header->FATOff);
     MMU_writeWord(0, 0x027FFE4C, header->FATSize);
     
     MMU_writeWord(0, 0x027FFE50, header->ARM9OverlayOff);
     MMU_writeWord(0, 0x027FFE54, header->ARM9OverlaySize);
     MMU_writeWord(0, 0x027FFE58, header->ARM7OverlayOff);
     MMU_writeWord(0, 0x027FFE5C, header->ARM7OverlaySize);
     
     MMU_writeWord(0, 0x027FFE60, header->unknown2a);
     MMU_writeWord(0, 0x027FFE64, header->unknown2b);  //merci EACKiX

     MMU_writeWord(0, 0x027FFE70, header->ARM9unk);
     MMU_writeWord(0, 0x027FFE74, header->ARM7unk);

     MMU_writeWord(0, 0x027FFF9C, 0x027FFF90); // ?????? besoin d'avoir la vrai valeur sur ds
     
     nds.touchX = nds.touchY = 0;
     MainScreen.offset = 192;
     SubScreen.offset = 0;
     
     //MMU_writeWord(0, 0x02007FFC, 0xE92D4030);
     
     return TRUE;
} 

NDS_header * NDS_getROMHeader(void)
{
     return (NDS_header *)MMU.CART_ROM;
} 

void NDS_setTouchPos(u16 x, u16 y)
{
     nds.touchX = (x <<4);
     nds.touchY = (y <<4);
     
     MMU.ARM7_REG[0x136] &= 0xBF;
}

void NDS_releasTouch(void)
{ 
     nds.touchX = 0;
     nds.touchY = 0;
     
     MMU.ARM7_REG[0x136] |= 0x40;
}

void debug()
{
     //if(NDS_ARM9.R[15]==0x020520DC) execute = FALSE;
     //DSLinux
          //if(NDS_ARM9.CPSR.bits.mode == 0) execute = FALSE;
          //if((NDS_ARM9.R[15]&0xFFFFF000)==0) execute = FALSE;
          //if((NDS_ARM9.R[15]==0x0201B4F4)/*&&(NDS_ARM9.R[1]==0x0)*/) execute = FALSE;
     //AOE
          //if((NDS_ARM9.R[15]==0x01FFE194)&&(NDS_ARM9.R[0]==0)) execute = FALSE;
          //if((NDS_ARM9.R[15]==0x01FFE134)&&(NDS_ARM9.R[0]==0)) execute = FALSE;

     //BBMAN
          //if(NDS_ARM9.R[15]==0x02098B4C) execute = FALSE;
          //if(NDS_ARM9.R[15]==0x02004924) execute = FALSE;
          //if(NDS_ARM9.R[15]==0x02004890) execute = FALSE;
     
     //if(NDS_ARM9.R[15]==0x0202B800) execute = FALSE;
     //if(NDS_ARM9.R[15]==0x0202B3DC) execute = FALSE;
     //if((NDS_ARM9.R[1]==0x9AC29AC1)&&(!fait)) {execute = FALSE;fait = TRUE;}
     //if(NDS_ARM9.R[1]==0x0400004A) {execute = FALSE;fait = TRUE;}
     /*if(NDS_ARM9.R[4]==0x2E33373C) execute = FALSE;
     if(NDS_ARM9.R[15]==0x02036668) //execute = FALSE;
     {
     nds.logcount++;
     sprintf(logbuf, "%d %08X", nds.logcount, NDS_ARM9.R[13]);
     log::ajouter(logbuf);
     if(nds.logcount==89) execute=FALSE;
     }*/
     //if(NDS_ARM9.instruction==0) execute = FALSE;
     //if((NDS_ARM9.R[15]>>28)) execute = FALSE;
}
