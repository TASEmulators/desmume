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
#include "render3D.h"
#include "MMU.h"
#include "cflash.h"

#include "MMU.h"
#include "cflash.h"
#include "ROMReader.h"
#include "gfx3d.h"

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
	 nds.idleFrameCounter = 0;
	 memset(nds.runCycleCollector,0,sizeof(nds.runCycleCollector));
     MMU_Init();
     nds.nextHBlank = 3168;
     nds.VCount = 0;
     nds.lignerendu = FALSE;

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

     if (SPU_Init(SNDCORE_DUMMY, 735) != 0)
        return -1;

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
	 gpu3D->NDS_3D_Close();
}

BOOL NDS_SetROM(u8 * rom, u32 mask)
{
     MMU_setRom(rom, mask);

     return TRUE;
} 

NDS_header * NDS_getROMHeader(void)
{
	NDS_header * header = new NDS_header();

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

int NDS_LoadROM( const char *filename, int bmtype, u32 bmsize,
                 const char *cflash_disk_image_file)
{
   int i;
   int type;
   char * p;
   
   ROMReader_struct * reader;
   void* file;
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
   cflash_init( cflash_disk_image_file);

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

   delete header;

   GPU_Reset(MainScreen.gpu, 0);
   GPU_Reset(SubScreen.gpu, 1);
   gfx3d_reset();
   gpu3D->NDS_3D_Reset();
   SPU_Reset();

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

int NDS_WriteBMP_32bppBuffer(int width, int height, const void* buf, const char *filename)
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
    imageheader.width = width;
    imageheader.height = height;
    imageheader.planes = 1;
    imageheader.bpp = 32;
    imageheader.cmptype = 0; // None
    imageheader.imgsize = imageheader.width * imageheader.height * 4;
    
    if ((file = fopen(filename,"wb")) == NULL)
       return 0;

    fwrite(&fileheader, 1, sizeof(fileheader), file);
    fwrite(&imageheader, 1, sizeof(imageheader), file);

	fwrite(buf,1,imageheader.imgsize,file);
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
  memset( MMU.fw.data, 0, 0x200);

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
   * User settings (at 0x3FE00 and 0x3FE00)
   */
  fill_user_data_area( user_settings, &MMU.fw.data[ 0x3FE00], 0);
  fill_user_data_area( user_settings, &MMU.fw.data[ 0x3FF00], 1);
  
  
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

#define INDEX(i) ((((i)>>16)&0xFF0)|(((i)>>4)&0xF))
u32
NDS_exec(s32 nb, BOOL force)
{
	int i, j;

  nb += nds.cycles;//(nds.cycles>>26)<<26;
            
  for(; (nb >= nds.cycles) && ((force)||(execute)); )
    {
		for (j = 0; j < 4 && (!force) && (execute); j++)
		{
			if(nds.ARM9Cycle<=nds.cycles)
			{
#ifdef LOG_ARM9
				if(logcount==3){
					if(NDS_ARM9.CPSR.bits.T)
						des_thumb_instructions_set[(NDS_ARM9.instruction)>>6](NDS_ARM9.instruct_adr, NDS_ARM9.instruction, logbuf);
					else
						des_arm_instructions_set[INDEX(NDS_ARM9.instruction)](NDS_ARM9.instruct_adr, NDS_ARM9.instruction, logbuf);
					sprintf(logbuf, "%s\t%08X\n\t R00: %08X, R01: %08X, R02: %08X, R03: %08X, R04: %08X, R05: %08X, R06: %08X, R07: %08X,\n\t R08: %08X, R09: %08X, R10: %08X, R11: %08X, R12: %08X, R13: %08X, R14: %08X, R15: %08X,\n\t CPSR: %08X , SPSR: %08X",
						logbuf, NDS_ARM9.instruction, NDS_ARM9.R[0],  NDS_ARM9.R[1],  NDS_ARM9.R[2],  NDS_ARM9.R[3],  NDS_ARM9.R[4],  NDS_ARM9.R[5],  NDS_ARM9.R[6],  NDS_ARM9.R[7], 
						NDS_ARM9.R[8],  NDS_ARM9.R[9],  NDS_ARM9.R[10],  NDS_ARM9.R[11],  NDS_ARM9.R[12],  NDS_ARM9.R[13],  NDS_ARM9.R[14],  NDS_ARM9.R[15],
						NDS_ARM9.CPSR, NDS_ARM9.SPSR);  
					LOG(logbuf);
				}
#endif
				for (i = 0; i < 4 && (!force) && (execute); i++)
				{
					if(NDS_ARM9.waitIRQ) {
						nds.ARM9Cycle += 100;
						nds.idleCycles += 100;
					} else
						//nds.ARM9Cycle += NDS_ARM9.exec();
						//nds.ARM9Cycle += armcpu_exec(&NDS_ARM9);
						nds.ARM9Cycle += armcpu_exec<0>();
				}
			}

#ifdef EXPERIMENTAL_WIFI

			if((nds.ARM7Cycle % 0x3F03) == 0)
			{
				/* 3F03 arm7 cyles = ~1usec */
				WIFI_usTrigger(&wifiMac) ;
			}
#endif
			if(nds.ARM7Cycle<=nds.cycles)
			{
#ifdef LOG_ARM7
				if(logcount==1){
					if(NDS_ARM7.CPSR.bits.T)
						des_thumb_instructions_set[(NDS_ARM7.instruction)>>6](NDS_ARM7.instruct_adr, NDS_ARM7.instruction, logbuf);
					else
						des_arm_instructions_set[INDEX(NDS_ARM7.instruction)](NDS_ARM7.instruct_adr, NDS_ARM7.instruction, logbuf);
					sprintf(logbuf, "%s\n\t R00: %08X, R01: %08X, R02: %08X, R03: %08X, R04: %08X, R05: %08X, R06: %08X, R07: %08X,\n\t R08: %08X, R09: %08X, R10: %08X, R11: %08X, R12: %08X, R13: %08X, R14: %08X, R15: %08X,\n\t CPSR: %08X , SPSR: %08X",
						logbuf, NDS_ARM7.R[0],  NDS_ARM7.R[1],  NDS_ARM7.R[2],  NDS_ARM7.R[3],  NDS_ARM7.R[4],  NDS_ARM7.R[5],  NDS_ARM7.R[6],  NDS_ARM7.R[7], 
						NDS_ARM7.R[8],  NDS_ARM7.R[9],  NDS_ARM7.R[10],  NDS_ARM7.R[11],  NDS_ARM7.R[12],  NDS_ARM7.R[13],  NDS_ARM7.R[14],  NDS_ARM7.R[15],
						NDS_ARM7.CPSR, NDS_ARM7.SPSR);  
					LOG(logbuf);
				}
#endif
				for (i = 0; i < 4 && (!force) && (execute); i++)
				{
					if(NDS_ARM7.waitIRQ)
						nds.ARM7Cycle += 100;
					else
						//nds.ARM7Cycle += (NDS_ARM7.exec()<<1);
						//nds.ARM7Cycle += (armcpu_exec(&NDS_ARM7)<<1);
						nds.ARM7Cycle += (armcpu_exec<1>()<<1);
				}
			}
		}

		nds.cycles = (nds.ARM9Cycle<nds.ARM7Cycle)?nds.ARM9Cycle : nds.ARM7Cycle;
                 
      //debug();
                 
      if(nds.cycles>=nds.nextHBlank)
        {
          if(!nds.lignerendu)
            {
              T1WriteWord(ARM9Mem.ARM9_REG, 4, T1ReadWord(ARM9Mem.ARM9_REG, 4) | 2);
              T1WriteWord(MMU.ARM7_REG, 4, T1ReadWord(MMU.ARM7_REG, 4) | 2);
              NDS_ARM9HBlankInt();
              NDS_ARM7HBlankInt();

              if(nds.VCount<192)
                {
                  GPU_ligne(&MainScreen, nds.VCount);
                  GPU_ligne(&SubScreen, nds.VCount);
				
                  if(MMU.DMAStartTime[0][0] == 2)
                    MMU_doDMA(0, 0);
                  if(MMU.DMAStartTime[0][1] == 2)
                    MMU_doDMA(0, 1);
                  if(MMU.DMAStartTime[0][2] == 2)
                    MMU_doDMA(0, 2);
                  if(MMU.DMAStartTime[0][3] == 2)
                    MMU_doDMA(0, 3);
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
                                
              nds.lignerendu = FALSE;
              if(nds.VCount==192)
                {
					gfx3d_VBlankSignal();

                  T1WriteWord(ARM9Mem.ARM9_REG, 4, T1ReadWord(ARM9Mem.ARM9_REG, 4) | 1);
                  T1WriteWord(MMU.ARM7_REG, 4, T1ReadWord(MMU.ARM7_REG, 4) | 1);
                  NDS_ARM9VBlankInt();
                  NDS_ARM7VBlankInt();
				  nds.runCycleCollector[nds.idleFrameCounter] = 1120380-nds.idleCycles;
				  nds.idleFrameCounter++;
				  nds.idleFrameCounter &= 15;
				  nds.idleCycles = 0;

                                
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
              else                         
                if(nds.VCount==263)
                  {
                    nds.nextHBlank = 3168;
                    nds.VCount = 0;
                    T1WriteWord(ARM9Mem.ARM9_REG, 4, T1ReadWord(ARM9Mem.ARM9_REG, 4) & 0xFFFE);
                    T1WriteWord(MMU.ARM7_REG, 4, T1ReadWord(MMU.ARM7_REG, 4) & 0xFFFE);

                    nds.cycles -= (560190<<1);
                    nds.ARM9Cycle -= (560190<<1);
                    nds.ARM7Cycle -= (560190<<1);
                    nb -= (560190<<1);

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

		if(MMU.reg_IE[0]&(1<<21))
		{
			if(MMU.gfxfifo.irq==0) return nds.cycles;
			if(MMU.gfxfifo.irq==3) return nds.cycles;
			if(MMU.gfxfifo.irq==1 && MMU.gfxfifo.half) NDS_makeARM9Int(21);
			if(MMU.gfxfifo.irq==2 && MMU.gfxfifo.empty) NDS_makeARM9Int(21);
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
  
	return nds.cycles;
}
