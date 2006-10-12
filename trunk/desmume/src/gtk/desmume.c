#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>

#include "../MMU.h"
#include "../armcpu.h"
#include "../NDSSystem.h"
#include "../cflash.h"

#include "desmume.h"

BOOL execute = FALSE;
BOOL click = FALSE;
BOOL fini = FALSE;
unsigned long glock = 0;

void desmume_mem_init();

u8 *desmume_rom_data = NULL;
u32 desmume_last_cycle;

void desmume_init()
{
	NDSInit();
	execute = FALSE;
}

void desmume_free()
{
	execute = FALSE;
	desmume_free_rom();
	NDSDeInit();
}

void desmume_mem_init()
{
	//ARM7 EXCEPTION VECTORS
	MMU_writeWord(1, 0x00, 0xE25EF002);
	MMU_writeWord(1, 0x04, 0xEAFFFFFE);
	MMU_writeWord(1, 0x18, 0xEA000000);
	MMU_writeWord(1, 0x20, 0xE92D500F);
	MMU_writeWord(1, 0x24, 0xE3A00301);
	MMU_writeWord(1, 0x28, 0xE28FE000);
	MMU_writeWord(1, 0x2C, 0xE510F004);
	MMU_writeWord(1, 0x30, 0xE8BD500F);
	MMU_writeWord(1, 0x34, 0xE25EF004);
	
	//ARM9 EXCEPTION VECTORS
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
}

#define DSGBA_EXTENSTION ".ds.gba"
#define DSGBA_LOADER_SIZE 512
enum
{
	ROM_NDS = 0,
	ROM_DSGBA
};
int desmume_load_rom(const char *filename)
{
	int i;
	uint type;
	const char *p = filename;
	FILE *file;
	u32 size, mask;
	u8 *data;
	
	type = ROM_NDS;
	
	p += strlen(p);
	p -= strlen(DSGBA_EXTENSTION);
	if(memcmp(p, DSGBA_EXTENSTION, strlen(DSGBA_EXTENSTION)) == 0)
	{
		type = ROM_DSGBA;
	}
	
	file = fopen(filename, "rb");
	if(!file) { return -1; }
	
	fseek(file, 0, SEEK_END);
	size = ftell(file);
	fseek(file, 0, SEEK_SET);
	
	if(type == ROM_DSGBA)
	{
		fseek(file, DSGBA_LOADER_SIZE, SEEK_SET);
		size -= DSGBA_LOADER_SIZE;
	}
	
	mask = size;
	mask |= (mask >>1);
	mask |= (mask >>2);
	mask |= (mask >>4);
	mask |= (mask >>8);
	mask |= (mask >>16);
	
	data = (u8*)malloc(mask + 1);
	if(!data) { fclose(file); return -1; }
	
	i = fread(data, 1, size, file);
	
	fclose(file);
	
	MMU_unsetRom();
	NDS_loadROM(data, mask);
	desmume_rom_data = data;
	
	strcpy(szRomPath, dirname((char *) filename));
	cflash_close();
	cflash_init();
	
	return i;
}

void desmume_free_rom()
{
	if(desmume_rom_data)
	{
		free(desmume_rom_data);
		desmume_rom_data = NULL;
	}
}

void desmume_reset()
{
	execute = FALSE;
	desmume_mem_init();
	desmume_last_cycle = 0;
}

void desmume_pause()
{
	execute = FALSE;
}
void desmume_resume()
{
	execute = TRUE;
}
void desmume_toggle()
{
	execute = (execute) ? FALSE : TRUE;
}
BOOL desmume_running()
{
	return execute;
}

void desmume_cycle()
{
	desmume_last_cycle = NDS_exec((560190 << 1) - desmume_last_cycle, FALSE);
}

void desmume_touch(s16 x, s16 y)
{
	NDS_setTouchPos(x, y);
}
void desmume_touch_release()
{
	NDS_releasTouch();
}

void desmume_keypad(u16 k)
{
	unsigned short k_ext = (k >> 10) & 0x3;
	unsigned short k_pad = k & 0x3FF;
	((unsigned short *)ARM9Mem.ARM9_REG)[0x130>>1] = ~k_pad;
	((unsigned short *)MMU.ARM7_REG)[0x130>>1] = ~k_ext;
}
 
