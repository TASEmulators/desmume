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
#include "cflash.h"

#include "ROMReader.h"

NDSSystem nds;
NDSFirmware firmware;

static u32
calc_CRC16( u32 start, u8 *data, int count) {
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

int NDS_Init(void) {
     nds.ARM9Cycle = 0;
     nds.ARM7Cycle = 0;
     nds.cycles = 0;
     MMU_Init();
     nds.nextHBlank = 3168;
     nds.VCount = 0;
     nds.lignerendu = FALSE;

     if (Screen_Init(GFXCORE_DUMMY) != 0)
        return -1;
     
     armcpu_new(&NDS_ARM7,1);
     armcpu_new(&NDS_ARM9,0);

     if (SPU_Init(SNDCORE_DUMMY, 735) != 0)
        return -1;
        
     sprintf(firmware.nickName,"yopyop");
     firmware.nickLen = strlen(firmware.nickName);
     sprintf(firmware.message,"Hi,it/s me!");
     firmware.msgLen = strlen(firmware.message);
     firmware.bDay = 15;
     firmware.bMonth = 7;
     firmware.favColor = 10;
     firmware.language = 2;

#ifdef EXPERIMENTAL_WIFI
	 WIFI_Init(&wifiMac) ;
#endif

     return 0;
}

void NDS_DeInit(void) {
     if(MMU.CART_ROM != MMU.UNUSED_RAM)
        NDS_FreeROM();

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

#define DSGBA_EXTENSTION ".ds.gba"
#define DSGBA_LOADER_SIZE 512
enum
{
	ROM_NDS = 0,
	ROM_DSGBA
};

int NDS_LoadROM(const char *filename, int bmtype, u32 bmsize)
{
   int i;
   int type;
   char * p;
   
   ROMReader_struct * reader;
   FILE * file;
   u32 size, mask;
   u8 *data;
   char * noext;

   if (filename == NULL)
      return -1;

   noext = strdup(filename);
   
   reader = ROMReaderInit(&noext);
   type = ROM_NDS;

   p = noext;
   p += strlen(p);
   p -= strlen(DSGBA_EXTENSTION);

   if(memcmp(p, DSGBA_EXTENSTION, strlen(DSGBA_EXTENSTION)) == 0)
      type = ROM_DSGBA;

   file = reader->Init(filename);

   if (!file)
   {
      free(noext);
      return -1;
   }
   size = reader->Size(file);

   if(type == ROM_DSGBA)
   {
      reader->Seek(file, DSGBA_LOADER_SIZE, SEEK_SET);
      size -= DSGBA_LOADER_SIZE;
   }
	
   mask = size;
   mask |= (mask >>1);
   mask |= (mask >>2);
   mask |= (mask >>4);
   mask |= (mask >>8);
   mask |= (mask >>16);

   // Make sure old ROM is freed first(at least this way we won't be eating
   // up a ton of ram before the old ROM is freed)
   if(MMU.CART_ROM != MMU.UNUSED_RAM)
      NDS_FreeROM();

   if ((data = (u8*)malloc(mask + 1)) == NULL)
   {
      reader->DeInit(file);
      free(noext);
      return -1;
   }

   i = reader->Read(file, data, size);
   reader->DeInit(file);
   MMU_unsetRom();
   NDS_SetROM(data, mask);
   NDS_Reset();

   /* I guess any directory can be used
    * so the current one should be ok */
   strncpy(szRomPath, ".", 512); /* "." is shorter then 512, yet strcpy should be avoided */
   cflash_close();
   cflash_init();

   strncpy(szRomBaseName, filename,512);

   if(type == ROM_DSGBA)
      szRomBaseName[strlen(szRomBaseName)-strlen(DSGBA_EXTENSTION)] = 0x00;
   else
      szRomBaseName[strlen(szRomBaseName)-4] = 0x00;

   // Setup Backup Memory
   if(type == ROM_DSGBA)
   {
	   /* be sure that we dont overwrite anything before stringstart */
	   if (strlen(noext)>= strlen(DSGBA_EXTENSTION))
                   strncpy(noext + strlen(noext) - strlen(DSGBA_EXTENSTION), ".sav",strlen(DSGBA_EXTENSTION)+1);
           else
           {
                   free(noext);
                   return -1;
           }
   }
   else
   {
	   /* be sure that we dont overwrite anything before stringstart */
	   if (strlen(noext)>=4)
                   strncpy(noext + strlen(noext) - 4, ".sav",5);
           else
           {
                   free(noext);
                   return -1;
           }
   }

   mc_realloc(&MMU.bupmem, bmtype, bmsize);
   mc_load_file(&MMU.bupmem, noext);
   free(noext);

   return i;
}

void NDS_FreeROM(void)
{
   if (MMU.CART_ROM != MMU.UNUSED_RAM)
      free(MMU.CART_ROM);
   MMU_unsetRom();
   if (MMU.bupmem.fp)
      fclose(MMU.bupmem.fp);
   MMU.bupmem.fp = NULL;
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
      MMU_writeWord(0, dst, T1ReadLong(MMU.CART_ROM, src));
      dst += 4;
      src += 4;
   }

   src = header->ARM7src;
   dst = header->ARM7cpy;
     
   for(i = 0; i < (header->ARM7binSize>>2); ++i)
   {
      MMU_writeWord(1, dst, T1ReadLong(MMU.CART_ROM, src));
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
   MMU_writeByte(0, 0x023FFC82, firmware.favColor);
   MMU_writeByte(0, 0x023FFC83, firmware.bMonth);
   MMU_writeByte(0, 0x023FFC84, firmware.bDay);
     
   for(i=0; i<firmware.nickLen; i++) 
      MMU_writeHWord(0, 0x023FFC86+(2*i), firmware.nickName[i]);
   MMU_writeHWord(0, 0x023FFC9A, firmware.nickLen);
     
   for(i=0; i<firmware.msgLen; i++) 
      MMU_writeHWord(0, 0x023FFC9C+(2*i), firmware.message[i]);
   MMU_writeHWord(0, 0x023FFCD0, firmware.msgLen);

   MMU_writeHWord(0, 0x023FFCE4, firmware.language); //Language 1=English 2=French
          
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
     
   MainScreen.offset = 192;
   SubScreen.offset = 0;
     
   //MMU_writeWord(0, 0x02007FFC, 0xE92D4030);

     //ARM7 BIOS IRQ HANDLER
     MMU_writeWord(1, 0x00, 0xE25EF002);
     MMU_writeWord(1, 0x04, 0xEAFFFFFE);
     MMU_writeWord(1, 0x18, 0xEA000000);
     MMU_writeWord(1, 0x20, 0xE92D500F);
     MMU_writeWord(1, 0x24, 0xE3A00301);
     MMU_writeWord(1, 0x28, 0xE28FE000);
     MMU_writeWord(1, 0x2C, 0xE510F004);
     MMU_writeWord(1, 0x30, 0xE8BD500F);
     MMU_writeWord(1, 0x34, 0xE25EF004);
    
     //ARM9 BIOS IRQ HANDLER
     MMU_writeWord(0, 0xFFF0018, 0xEA000000);
     MMU_writeWord(0, 0xFFF0020, 0xE92D500F);
     MMU_writeWord(0, 0xFFF0024, 0xEE190F11);
     MMU_writeWord(0, 0xFFF0028, 0xE1A00620);
     MMU_writeWord(0, 0xFFF002C, 0xE1A00600);
     MMU_writeWord(0, 0xFFF0030, 0xE2800C40);
     MMU_writeWord(0, 0xFFF0034, 0xE28FE000);
     MMU_writeWord(0, 0xFFF0038, 0xE510F004);
     MMU_writeWord(0, 0xFFF003C, 0xE8BD500F);
     MMU_writeWord(0, 0xFFF0040, 0xE25EF004);
        
     MMU_writeWord(0, 0x0000004, 0xE3A0010E);
     MMU_writeWord(0, 0x0000008, 0xE3A01020);
//     MMU_writeWord(0, 0x000000C, 0xE1B02110);
     MMU_writeWord(0, 0x000000C, 0xE1B02040);
     MMU_writeWord(0, 0x0000010, 0xE3B02020);
//     MMU_writeWord(0, 0x0000010, 0xE2100202);

   free(header);

   GPU_Reset(MainScreen.gpu, 0);
   GPU_Reset(SubScreen.gpu, 1);
   SPU_Reset();

   NDS_CreateDummyFirmware() ;

   execute = oldexecute;
}

int NDS_ImportSave(const char *filename)
{
   if (strlen(filename) < 4)
      return 0;

   if (memcmp(filename+strlen(filename)-4, ".duc", 4) == 0)
      return mc_load_duc(&MMU.bupmem, filename);

   return 0;
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

#ifdef _MSC_VER
#pragma pack(push, 1)
typedef struct
{
    u16 id;
    u32 size;
    u16 reserved1;
    u16 reserved2;
    u32 imgoffset;
} bmpfileheader_struct;
#pragma pack(pop)
#else
typedef struct
{
    u16 id __PACKED;
    u32 size __PACKED;
    u16 reserved1 __PACKED;
    u16 reserved2 __PACKED;
    u32 imgoffset __PACKED;
} bmpfileheader_struct;
#endif

int NDS_WriteBMP(const char *filename)
{
    bmpfileheader_struct fileheader;
    bmpimgheader_struct imageheader;
    FILE *file;
    int i,j,k;
    u16 * bmp = (u16 *)GPU_screen;

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

    fwrite(&fileheader, 1, sizeof(fileheader), file);
    fwrite(&imageheader, 1, sizeof(imageheader), file);

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
          fwrite(&r, 1, sizeof(u8), file); 
          fwrite(&g, 1, sizeof(u8), file); 
          fwrite(&b, 1, sizeof(u8), file);
       }
    }
    fclose(file);

    return 1;
}

static
void create_user_data( u8 *data, int count) {
  u32 crc;

  memset( data, 0, 0x100);

  /* version */
  data[0x00] = 5;
  data[0x01] = 0;

  /* colour */
  data[0x02] = 7;

  /* birthday month and day */
  data[0x03] = 6;
  data[0x04] = 23;

  /* nickname and length */
  data[0x06] = 'y';
  data[0x08] = 'o';
  data[0x0a] = 'p';
  data[0x0c] = 'y';
  data[0x0e] = 'o';
  data[0x10] = 'p';

  data[0x1a] = 6;

  /* Message */
  data[0x1c] = 'D';
  data[0x1e] = 'e';
  data[0x20] = 'S';
  data[0x22] = 'm';
  data[0x24] = 'u';
  data[0x26] = 'M';
  data[0x28] = 'E';
  data[0x2a] = ' ';
  data[0x2c] = 'm';
  data[0x2e] = 'a';
  data[0x30] = 'k';
  data[0x32] = 'e';
  data[0x34] = 's';
  data[0x36] = ' ';
  data[0x38] = 'y';
  data[0x3a] = 'o';
  data[0x3c] = 'u';
  data[0x3e] = ' ';
  data[0x40] = 'h';
  data[0x42] = 'a';
  data[0x44] = 'p';
  data[0x46] = 'p';
  data[0x48] = 'y';
  data[0x4a] = '!';

  data[0x50] = 24;

  /*
   * touch screen calibration
   */
  /* ADC position 1, x y */
  data[0x58] = 0x00;
  data[0x59] = 0x02;
  data[0x5a] = 0x00;
  data[0x5b] = 0x02;

  /* screen pos 1, x y */
  data[0x5c] = 0x20;
  data[0x5d] = 0x20;

  /* ADC position 2, x y */
  data[0x5e] = 0x00;
  data[0x5f] = 0x0e;
  data[0x60] = 0x00;
  data[0x61] = 0x08;

  /* screen pos 2, x y */
  data[0x62] = 0xe0;
  data[0x63] = 0x80;

  /* language and flags */
  data[0x64] = 0x01;
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
int NDS_CreateDummyFirmware(void)
{
  /*
   * Create the firmware header
   */
  memset( MMU.fw.data, 0, 0x200);

  /* firmware identifier */
  MMU.fw.data[0x8] = 'M';
  MMU.fw.data[0x8 + 1] = 'A';
  MMU.fw.data[0x8 + 2] = 'C';
  MMU.fw.data[0x8 + 3] = 'P';

  /* DS type (phat) */
  MMU.fw.data[0x1d] = 0xff;

  /* User Settings offset 0x3fe00 / 8 */
  MMU.fw.data[0x20] = 0xc0;
  MMU.fw.data[0x21] = 0x7f;


  /*
   * User settings (at 0x3FE00 and 0x3FE00)
   */
  create_user_data( &MMU.fw.data[ 0x3FE00], 0);
  create_user_data( &MMU.fw.data[ 0x3FF00], 1);
  
  
#ifdef EXPERIMENTAL_WIFI
	memcpy(MMU.fw.data+0x36,FW_Mac,sizeof(FW_Mac)) ;
	memcpy(MMU.fw.data+0x44,FW_WIFIInit,sizeof(FW_WIFIInit)) ;
	MMU.fw.data[0x41] = 18 ; /* bits per RF value */
	MMU.fw.data[0x42] = 12 ; /* # of RF values to init */
	memcpy(MMU.fw.data+0x64,FW_BBInit,sizeof(FW_BBInit)) ;
	memcpy(MMU.fw.data+0xCE,FW_RFInit,sizeof(FW_RFInit)) ;
	memcpy(MMU.fw.data+0xF2,FW_RFChannel,sizeof(FW_RFChannel)) ;
	memcpy(MMU.fw.data+0x146,FW_BBChannel,sizeof(FW_BBChannel)) ;

	memcpy(MMU.fw.data+0x03FA40,FW_WFCProfile,sizeof(FW_WFCProfile)) ;
#endif
	return TRUE ;
}

int NDS_LoadFirmware(const char *filename)
{
    int i;
    long size;
    FILE *file;
	
    if ((file = fopen(filename, "rb")) == NULL)
       return -1;
	
    fseek(file, 0, SEEK_END);
    size = ftell(file);
    fseek(file, 0, SEEK_SET);
	
    if(size > MMU.fw.size)
    {
       fclose(file);
       return -1;
    }
	
    i = fread(MMU.fw.data, size, 1, file);
    fclose(file);
	
    return i;
}
