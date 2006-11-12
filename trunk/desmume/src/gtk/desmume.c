#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>

#include "../MMU.h"
#include "../armcpu.h"
#include "../NDSSystem.h"
#include "../cflash.h"
#include "../sndsdl.h"

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
	NDS_Init();
        SPU_ChangeSoundCore(SNDCORE_SDL, 735 * 4);
	execute = FALSE;
}

void desmume_free()
{
	execute = FALSE;
	NDS_DeInit();
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
        SPU_Emulate();
}

void desmume_keypad(u16 k)
{
	unsigned short k_ext = (k >> 10) & 0x3;
	unsigned short k_pad = k & 0x3FF;
	((unsigned short *)ARM9Mem.ARM9_REG)[0x130>>1] = ~k_pad;
	((unsigned short *)MMU.ARM7_REG)[0x130>>1] = ~k_ext;
}
 
