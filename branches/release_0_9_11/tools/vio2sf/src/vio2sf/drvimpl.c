#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#include "desmume/MMU.h"
#include "desmume/armcpu.h"
#include "desmume/ndssystem.h"
#include "desmume/spu_exports.h"
#include "desmume/cp15.h"

#include "zlib/zlib.h"
#include "../xsfc/tagget.h"
#include "../xsfc/drvimpl.h"

volatile BOOL execute = FALSE;

static struct
{
	unsigned char *rom;
	unsigned char *state;
	unsigned romsize;
	unsigned statesize;
	unsigned stateptr;
} loaderwork = { 0, 0, 0, 0, 0 };

static void load_term(void)
{
	if (loaderwork.rom)
	{
		free(loaderwork.rom);
		loaderwork.rom = 0;
	}
	loaderwork.romsize = 0;
	if (loaderwork.state)
	{
		free(loaderwork.state);
		loaderwork.state = 0;
	}
	loaderwork.statesize = 0;
}

static int load_map(int issave, unsigned char *udata, unsigned usize)
{	
	unsigned char *iptr;
	unsigned isize;
	unsigned char *xptr;
	unsigned xsize = getdwordle(udata + 4);
	unsigned xofs = getdwordle(udata + 0);
	if (issave)
	{
		iptr = loaderwork.state;
		isize = loaderwork.statesize;
		loaderwork.state = 0;
		loaderwork.statesize = 0;
	}
	else
	{
		iptr = loaderwork.rom;
		isize = loaderwork.romsize;
		loaderwork.rom = 0;
		loaderwork.romsize = 0;
	}
	if (!iptr)
	{
		unsigned rsize = xofs + xsize;
		if (!issave)
		{
			rsize -= 1;
			rsize |= rsize >> 1;
			rsize |= rsize >> 2;
			rsize |= rsize >> 4;
			rsize |= rsize >> 8;
			rsize |= rsize >> 16;
			rsize += 1;
		}
		iptr = malloc(rsize + 10);
		if (!iptr)
			return XSF_FALSE;
		memset(iptr, 0, rsize + 10);
		isize = rsize;
	}
	else if (isize < xofs + xsize)
	{
		unsigned rsize = xofs + xsize;
		if (!issave)
		{
			rsize -= 1;
			rsize |= rsize >> 1;
			rsize |= rsize >> 2;
			rsize |= rsize >> 4;
			rsize |= rsize >> 8;
			rsize |= rsize >> 16;
			rsize += 1;
		}
		xptr = realloc(iptr, xofs + rsize + 10);
		if (!xptr)
		{
			free(iptr);
			return XSF_FALSE;
		}
		iptr = xptr;
		isize = rsize;
	}
	memcpy(iptr + xofs, udata + 8, xsize);
	if (issave)
	{
		loaderwork.state = iptr;
		loaderwork.statesize = isize;
	}
	else
	{
		loaderwork.rom = iptr;
		loaderwork.romsize = isize;
	}
	return XSF_TRUE;
}

static int load_mapz(int issave, unsigned char *zdata, unsigned zsize, unsigned zcrc)
{
	int ret;
	int zerr;
	uLongf usize = 8;
	uLongf rsize = usize;
	unsigned char *udata;
	unsigned char *rdata;

	udata = malloc(usize);
	if (!udata)
		return XSF_FALSE;

	while (Z_OK != (zerr = uncompress(udata, &usize, zdata, zsize)))
	{
		if (Z_MEM_ERROR != zerr && Z_BUF_ERROR != zerr)
		{
			free(udata);
			return XSF_FALSE;
		}
		if (usize >= 8)
		{
			usize = getdwordle(udata + 4) + 8;
			if (usize < rsize)
			{
				rsize += rsize;
				usize = rsize;
			}
			else
				rsize = usize;
		}
		else
		{
			rsize += rsize;
			usize = rsize;
		}
		free(udata);
		udata = malloc(usize);
		if (!udata)
			return XSF_FALSE;
	}

	rdata = realloc(udata, usize);
	if (!rdata)
	{
		free(udata);
		return XSF_FALSE;
	}

	if (0)
	{
		unsigned ccrc = crc32(crc32(0L, Z_NULL, 0), rdata, usize);
		if (ccrc != zcrc)
			return XSF_FALSE;
	}

	ret = load_map(issave, rdata, usize);
	free(rdata);
	return ret;
}

static int load_psf_one(unsigned char *pfile, unsigned bytes)
{
	unsigned char *ptr = pfile;
	unsigned code_size;
	unsigned resv_size;
	unsigned code_crc;
	if (bytes < 16 || getdwordle(ptr) != 0x24465350)
		return XSF_FALSE;

	resv_size = getdwordle(ptr + 4);
	code_size = getdwordle(ptr + 8);
	code_crc = getdwordle(ptr + 12);

	if (resv_size)
	{
		unsigned resv_pos = 0;
		ptr = pfile + 16;
		if (16+ resv_size > bytes)
			return XSF_FALSE;
		while (resv_pos + 12 < resv_size)
		{
			unsigned save_size = getdwordle(ptr + resv_pos + 4);
			unsigned save_crc = getdwordle(ptr + resv_pos + 8);
			if (getdwordle(ptr + resv_pos + 0) == 0x45564153)
			{
				if (resv_pos + 12 + save_size > resv_size)
					return XSF_FALSE;
				if (!load_mapz(1, ptr + resv_pos + 12, save_size, save_crc))
					return XSF_FALSE;
			}
			resv_pos += 12 + save_size;
		}
	}

	if (code_size)
	{
		ptr = pfile + 16 + resv_size;
		if (16 + resv_size + code_size > bytes)
			return XSF_FALSE;
		if (!load_mapz(0, ptr, code_size, code_crc))
			return XSF_FALSE;
	}

	return XSF_TRUE;
}

typedef struct
{
	const char *tag;
	int taglen;
	int level;
	int found;
} loadlibwork_t;

static int load_psf_and_libs(int level, void *pfile, unsigned bytes);

static int load_psfcb(void *pWork, const char *pNameTop, const char *pNameEnd, const char *pValueTop, const char *pValueEnd)
{
	loadlibwork_t *pwork = (loadlibwork_t *)pWork;
	int ret = xsf_tagenum_callback_returnvaluecontinue;
	if (pNameEnd - pNameTop == pwork->taglen && !_strnicmp(pNameTop, pwork->tag , pwork->taglen))
	{
		unsigned l = pValueEnd - pValueTop;
		char *lib = malloc(l + 1);
		if (!lib)
		{
			ret = xsf_tagenum_callback_returnvaluebreak;
		}
		else
		{
			void *libbuf;
			unsigned libsize;
			memcpy(lib, pValueTop, l);
			lib[l] = '\0';
			if (!xsf_get_lib(lib, &libbuf, &libsize))
			{
				ret = xsf_tagenum_callback_returnvaluebreak;
			}
			else
			{
				if (!load_psf_and_libs(pwork->level + 1, libbuf, libsize))
					ret = xsf_tagenum_callback_returnvaluebreak;
				else
					pwork->found++;
				free(libbuf);
			}
			free(lib);
		}
	}
	return ret;
}

static int load_psf_and_libs(int level, void *pfile, unsigned bytes)
{
	int haslib = 0;
	loadlibwork_t work;

	work.level = level;
	work.tag = "_lib";
	work.taglen = strlen(work.tag);
	work.found = 0;

	if (level <= 10 && xsf_tagenum(load_psfcb, &work, pfile, bytes) < 0)
		return XSF_FALSE;

	haslib = work.found;

	if (!load_psf_one(pfile, bytes))
		return XSF_FALSE;

/*	if (haslib) */
	{
		int n = 2;
		do
		{
			char tbuf[16];
#ifdef HAVE_SPRINTF_S
			sprintf_s(tbuf, sizeof(tbuf), "_lib%d", n++);
#else
			sprintf(tbuf, "_lib%d", n++);
#endif
			work.tag = tbuf;
			work.taglen = strlen(work.tag);
			work.found = 0;
			if (xsf_tagenum(load_psfcb, &work, pfile, bytes) < 0)
				return XSF_FALSE;
		}
		while (work.found);
	}
	return XSF_TRUE;
}

static int load_psf(void *pfile, unsigned bytes)
{
	load_term();

	return load_psf_and_libs(1, pfile, bytes);
}

static void load_getstateinit(unsigned ptr)
{
	loaderwork.stateptr = ptr;
}

static u16 getwordle(const unsigned char *pData)
{
	return pData[0] | (((u16)pData[1]) << 8);
}

static void load_getsta(Status_Reg *ptr, unsigned l)
{
	unsigned s = l << 2;
	unsigned i;
	if ((loaderwork.stateptr > loaderwork.statesize) || ((loaderwork.stateptr + s) > loaderwork.statesize))
		return;
	for (i = 0; i < l; i++)
		{
			u32 st = getdwordle(loaderwork.state + loaderwork.stateptr + (i << 2));
			ptr[i].bits.N = (st >> 31) & 1;
			ptr[i].bits.Z = (st >> 30) & 1;
			ptr[i].bits.C = (st >> 29) & 1;
			ptr[i].bits.V = (st >> 28) & 1;
			ptr[i].bits.Q = (st >> 27) & 1;
			ptr[i].bits.RAZ = (st >> 8) & ((1 << 19) - 1);
			ptr[i].bits.I = (st >> 7) & 1;
			ptr[i].bits.F = (st >> 6) & 1;
			ptr[i].bits.T = (st >> 5) & 1;
			ptr[i].bits.mode = (st >> 0) & 0x1f;
		}
	loaderwork.stateptr += s;
}

static void load_getbool(BOOL *ptr, unsigned l)
{
	unsigned s = l << 2;
	unsigned i;
	if ((loaderwork.stateptr > loaderwork.statesize) || ((loaderwork.stateptr + s) > loaderwork.statesize))
		return;
	for (i = 0; i < l; i++)
		ptr[i] = (BOOL)getdwordle(loaderwork.state + loaderwork.stateptr + (i << 2));
	loaderwork.stateptr += s;
}

#if defined(SIGNED_IS_NOT_2S_COMPLEMENT)
/* 2's complement */
#define u32tos32(v) ((s32)((((s64)(v)) ^ 0x80000000) - 0x80000000))
#else
/* 2's complement */
#define u32tos32(v) ((s32)v)
#endif

static void load_gets32(s32 *ptr, unsigned l)
{
	unsigned s = l << 2;
	unsigned i;
	if ((loaderwork.stateptr > loaderwork.statesize) || ((loaderwork.stateptr + s) > loaderwork.statesize))
		return;
	for (i = 0; i < l; i++)
		ptr[i] = u32tos32(getdwordle(loaderwork.state + loaderwork.stateptr + (i << 2)));
	loaderwork.stateptr += s;
}

static void load_getu32(u32 *ptr, unsigned l)
{
	unsigned s = l << 2;
	unsigned i;
	if ((loaderwork.stateptr > loaderwork.statesize) || ((loaderwork.stateptr + s) > loaderwork.statesize))
		return;
	for (i = 0; i < l; i++)
		ptr[i] = getdwordle(loaderwork.state + loaderwork.stateptr + (i << 2));
	loaderwork.stateptr += s;
}

static void load_getu16(u16 *ptr, unsigned l)
{
	unsigned s = l << 1;
	unsigned i;
	if ((loaderwork.stateptr > loaderwork.statesize) || ((loaderwork.stateptr + s) > loaderwork.statesize))
		return;
	for (i = 0; i < l; i++)
		ptr[i] = getwordle(loaderwork.state + loaderwork.stateptr + (i << 1));
	loaderwork.stateptr += s;
}

static void load_getu8(u8 *ptr, unsigned l)
{
	unsigned s = l;
	unsigned i;
	if ((loaderwork.stateptr > loaderwork.statesize) || ((loaderwork.stateptr + s) > loaderwork.statesize))
		return;
	for (i = 0; i < l; i++)
		ptr[i] = loaderwork.state[loaderwork.stateptr + i];
	loaderwork.stateptr += s;
}

void gdb_stub_fix(armcpu_t *armcpu)
{
	/* armcpu->R[15] = armcpu->instruct_adr; */
	armcpu->next_instruction = armcpu->instruct_adr;
	if(armcpu->CPSR.bits.T == 0)
	{
		armcpu->instruction = MMU_read32_acl(armcpu->proc_ID, armcpu->next_instruction,CP15_ACCESS_EXECUTE);
		armcpu->instruct_adr = armcpu->next_instruction;
		armcpu->next_instruction += 4;
		armcpu->R[15] = armcpu->next_instruction + 4;
	}
	else
	{
		armcpu->instruction = MMU_read16_acl(armcpu->proc_ID, armcpu->next_instruction,CP15_ACCESS_EXECUTE);
		armcpu->instruct_adr = armcpu->next_instruction;
		armcpu->next_instruction += 2;
		armcpu->R[15] = armcpu->next_instruction + 2;
	}
}

static void load_setstate(void)
{
	if (!loaderwork.statesize)
		return;

    /* Skip over "Desmume Save File" crap */
	load_getstateinit(0x17);

	/* Read ARM7 cpu registers */
	load_getu32(&NDS_ARM7.proc_ID, 1);
	load_getu32(&NDS_ARM7.instruction, 1);
	load_getu32(&NDS_ARM7.instruct_adr, 1);
	load_getu32(&NDS_ARM7.next_instruction, 1);
	load_getu32(NDS_ARM7.R, 16);
	load_getsta(&NDS_ARM7.CPSR, 1);
	load_getsta(&NDS_ARM7.SPSR, 1);
	load_getu32(&NDS_ARM7.R13_usr, 1);
	load_getu32(&NDS_ARM7.R14_usr, 1);
	load_getu32(&NDS_ARM7.R13_svc, 1);
	load_getu32(&NDS_ARM7.R14_svc, 1);
	load_getu32(&NDS_ARM7.R13_abt, 1);
	load_getu32(&NDS_ARM7.R14_abt, 1);
	load_getu32(&NDS_ARM7.R13_und, 1);
	load_getu32(&NDS_ARM7.R14_und, 1);
	load_getu32(&NDS_ARM7.R13_irq, 1);
	load_getu32(&NDS_ARM7.R14_irq, 1);
	load_getu32(&NDS_ARM7.R8_fiq, 1);
	load_getu32(&NDS_ARM7.R9_fiq, 1);
	load_getu32(&NDS_ARM7.R10_fiq, 1);
	load_getu32(&NDS_ARM7.R11_fiq, 1);
	load_getu32(&NDS_ARM7.R12_fiq, 1);
	load_getu32(&NDS_ARM7.R13_fiq, 1);
	load_getu32(&NDS_ARM7.R14_fiq, 1);
	load_getsta(&NDS_ARM7.SPSR_svc, 1);
	load_getsta(&NDS_ARM7.SPSR_abt, 1);
	load_getsta(&NDS_ARM7.SPSR_und, 1);
	load_getsta(&NDS_ARM7.SPSR_irq, 1);
	load_getsta(&NDS_ARM7.SPSR_fiq, 1);
	load_getu32(&NDS_ARM7.intVector, 1);
	load_getu8(&NDS_ARM7.LDTBit, 1);
	load_getbool(&NDS_ARM7.waitIRQ, 1);
	load_getbool(&NDS_ARM7.wIRQ, 1);
	load_getbool(&NDS_ARM7.wirq, 1);

	/* Read ARM9 cpu registers */
	load_getu32(&NDS_ARM9.proc_ID, 1);
	load_getu32(&NDS_ARM9.instruction, 1);
	load_getu32(&NDS_ARM9.instruct_adr, 1);
	load_getu32(&NDS_ARM9.next_instruction, 1);
	load_getu32(NDS_ARM9.R, 16);
	load_getsta(&NDS_ARM9.CPSR, 1);
	load_getsta(&NDS_ARM9.SPSR, 1);
	load_getu32(&NDS_ARM9.R13_usr, 1);
	load_getu32(&NDS_ARM9.R14_usr, 1);
	load_getu32(&NDS_ARM9.R13_svc, 1);
	load_getu32(&NDS_ARM9.R14_svc, 1);
	load_getu32(&NDS_ARM9.R13_abt, 1);
	load_getu32(&NDS_ARM9.R14_abt, 1);
	load_getu32(&NDS_ARM9.R13_und, 1);
	load_getu32(&NDS_ARM9.R14_und, 1);
	load_getu32(&NDS_ARM9.R13_irq, 1);
	load_getu32(&NDS_ARM9.R14_irq, 1);
	load_getu32(&NDS_ARM9.R8_fiq, 1);
	load_getu32(&NDS_ARM9.R9_fiq, 1);
	load_getu32(&NDS_ARM9.R10_fiq, 1);
	load_getu32(&NDS_ARM9.R11_fiq, 1);
	load_getu32(&NDS_ARM9.R12_fiq, 1);
	load_getu32(&NDS_ARM9.R13_fiq, 1);
	load_getu32(&NDS_ARM9.R14_fiq, 1);
	load_getsta(&NDS_ARM9.SPSR_svc, 1);
	load_getsta(&NDS_ARM9.SPSR_abt, 1);
	load_getsta(&NDS_ARM9.SPSR_und, 1);
	load_getsta(&NDS_ARM9.SPSR_irq, 1);
	load_getsta(&NDS_ARM9.SPSR_fiq, 1);
	load_getu32(&NDS_ARM9.intVector, 1);
	load_getu8(&NDS_ARM9.LDTBit, 1);
	load_getbool(&NDS_ARM9.waitIRQ, 1);
	load_getbool(&NDS_ARM9.wIRQ, 1);
	load_getbool(&NDS_ARM9.wirq, 1);

	/* Read in other internal variables that are important */
	load_gets32(&nds.ARM9Cycle, 1);
	load_gets32(&nds.ARM7Cycle, 1);
	load_gets32(&nds.cycles, 1);
	load_gets32(nds.timerCycle[0], 4);
	load_gets32(nds.timerCycle[1], 4);
	load_getbool(nds.timerOver[0], 4);
	load_getbool(nds.timerOver[1], 4);
	load_gets32(&nds.nextHBlank, 1);
	load_getu32(&nds.VCount, 1);
	load_getu32(&nds.old, 1);
	load_gets32(&nds.diff, 1);
	load_getbool(&nds.lignerendu, 1);
	load_getu16(&nds.touchX, 1);
	load_getu16(&nds.touchY, 1);

	/* Read in memory/registers specific to the ARM9 */
	load_getu8 (ARM9Mem.ARM9_ITCM, 0x8000);
	load_getu8 (ARM9Mem.ARM9_DTCM, 0x4000);
	load_getu8 (ARM9Mem.ARM9_WRAM, 0x1000000);
	load_getu8 (ARM9Mem.MAIN_MEM, 0x400000);
	load_getu8 (ARM9Mem.ARM9_REG, 0x10000);
	load_getu8 (ARM9Mem.ARM9_VMEM, 0x800);
	load_getu8 (ARM9Mem.ARM9_OAM, 0x800);    
	load_getu8 (ARM9Mem.ARM9_ABG, 0x80000);
	load_getu8 (ARM9Mem.ARM9_BBG, 0x20000);
	load_getu8 (ARM9Mem.ARM9_AOBJ, 0x40000);
	load_getu8 (ARM9Mem.ARM9_BOBJ, 0x20000);
	load_getu8 (ARM9Mem.ARM9_LCD, 0xA4000);

	/* Read in memory/registers specific to the ARM7 */
	load_getu8 (MMU.ARM7_ERAM, 0x10000);
	load_getu8 (MMU.ARM7_REG, 0x10000);
	load_getu8 (MMU.ARM7_WIRAM, 0x10000);

	/* Read in shared memory */
	load_getu8 (MMU.SWIRAM, 0x8000);

#ifdef GDB_STUB
#else
	gdb_stub_fix(&NDS_ARM9);
	gdb_stub_fix(&NDS_ARM7);
#endif
}

static struct
{
	unsigned char *pcmbufalloc;
	unsigned char *pcmbuftop;
	unsigned filled;
	unsigned used;
	u32 bufferbytes;
	u32 cycles;
	int xfs_load;
	int sync_type;
	int arm7_clockdown_level;
	int arm9_clockdown_level;
} sndifwork = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static void SNDIFDeInit(void)
{
	if (sndifwork.pcmbufalloc)
	{
		free(sndifwork.pcmbufalloc);
		sndifwork.pcmbufalloc = 0;
		sndifwork.pcmbuftop = 0;
		sndifwork.bufferbytes = 0;
	}
}
static int SNDIFInit(int buffersize)
{
	u32 bufferbytes = buffersize * sizeof(s16);
	SNDIFDeInit();
	sndifwork.pcmbufalloc = malloc(bufferbytes + 3);
	if (!sndifwork.pcmbufalloc)
		return -1;
	sndifwork.pcmbuftop = sndifwork.pcmbufalloc + ((4 - (((int)sndifwork.pcmbufalloc) & 3)) & 3);
	sndifwork.bufferbytes = bufferbytes;
	sndifwork.filled = 0;
	sndifwork.used = 0;
	sndifwork.cycles = 0;
	return 0;
}
static void SNDIFMuteAudio(void)
{
}
static void SNDIFUnMuteAudio(void)
{
}
static void SNDIFSetVolume(int volume)
{
}
static int SNDIFGetAudioSpace(void)
{
	return sndifwork.bufferbytes >> 2; // bytes to samples
}
static void SNDIFUpdateAudio(s16 * buffer, u32 num_samples)
{
	u32 num_bytes = num_samples << 2;
	if (num_bytes > sndifwork.bufferbytes) num_bytes = sndifwork.bufferbytes;
	memcpy(sndifwork.pcmbuftop, buffer, num_bytes);
	sndifwork.filled = num_bytes;
	sndifwork.used = 0;
}
#define VIO2SFSNDIFID 0
static SoundInterface_struct VIO2SFSNDIF =
{
	VIO2SFSNDIFID,
	"vio2sf Sound Interface",
	SNDIFInit,
	SNDIFDeInit,
	SNDIFUpdateAudio,
	SNDIFGetAudioSpace,
	SNDIFMuteAudio,
	SNDIFUnMuteAudio,
	SNDIFSetVolume
};

SoundInterface_struct *SNDCoreList[] = {
	&VIO2SFSNDIF,
	NULL
};

static struct armcpu_ctrl_iface *arm9_ctrl_iface = 0;
static struct armcpu_ctrl_iface *arm7_ctrl_iface = 0;

int xsf_start(void *pfile, unsigned bytes)
{
	int frames = xsf_tagget_int("_frames", pfile, bytes, -1);
	int clockdown = xsf_tagget_int("_clockdown", pfile, bytes, 0);
	sndifwork.sync_type = xsf_tagget_int("_vio2sf_sync_type", pfile, bytes, 0);
	sndifwork.arm9_clockdown_level = xsf_tagget_int("_vio2sf_arm9_clockdown_level", pfile, bytes, clockdown);
	sndifwork.arm7_clockdown_level = xsf_tagget_int("_vio2sf_arm7_clockdown_level", pfile, bytes, clockdown);

	sndifwork.xfs_load = 0;
	if (!load_psf(pfile, bytes))
		return XSF_FALSE;

#ifdef GDB_STUB
	if (NDS_Init(&arm9_base_memory_iface, &arm9_ctrl_iface, &arm7_base_memory_iface, &arm7_ctrl_iface))
#else
	if (NDS_Init())
#endif
		return XSF_FALSE;

	SPU_ChangeSoundCore(0, 737);

	execute = FALSE;

	MMU_unsetRom();
	if (loaderwork.rom)
	{
		NDS_SetROM(loaderwork.rom, loaderwork.romsize - 1);
	}

	NDS_Reset();

	execute = TRUE;

	if (loaderwork.state)
	{
		armcp15_t *c9 = (armcp15_t *)NDS_ARM9.coproc[15];
		int proc;
		if (frames == -1)
		{

			/* set initial ARM9 coprocessor state */

			armcp15_moveARM2CP(c9, 0x00000000, 0x01, 0x00, 0, 0);
			armcp15_moveARM2CP(c9, 0x00000000, 0x07, 0x05, 0, 0);
			armcp15_moveARM2CP(c9, 0x00000000, 0x07, 0x06, 0, 0);
			armcp15_moveARM2CP(c9, 0x00000000, 0x07, 0x0a, 0, 4);
			armcp15_moveARM2CP(c9, 0x04000033, 0x06, 0x00, 0, 4);
			armcp15_moveARM2CP(c9, 0x0200002d, 0x06, 0x01, 0, 0);
			armcp15_moveARM2CP(c9, 0x027e0021, 0x06, 0x02, 0, 0);
			armcp15_moveARM2CP(c9, 0x08000035, 0x06, 0x03, 0, 0);
			armcp15_moveARM2CP(c9, 0x027e001b, 0x06, 0x04, 0, 0);
			armcp15_moveARM2CP(c9, 0x0100002f, 0x06, 0x05, 0, 0);
			armcp15_moveARM2CP(c9, 0xffff001d, 0x06, 0x06, 0, 0);
			armcp15_moveARM2CP(c9, 0x027ff017, 0x06, 0x07, 0, 0);
			armcp15_moveARM2CP(c9, 0x00000020, 0x09, 0x01, 0, 1);

			armcp15_moveARM2CP(c9, 0x027e000a, 0x09, 0x01, 0, 0);

			armcp15_moveARM2CP(c9, 0x00000042, 0x02, 0x00, 0, 1);
			armcp15_moveARM2CP(c9, 0x00000042, 0x02, 0x00, 0, 0);
			armcp15_moveARM2CP(c9, 0x00000002, 0x03, 0x00, 0, 0);
			armcp15_moveARM2CP(c9, 0x05100011, 0x05, 0x00, 0, 3);
			armcp15_moveARM2CP(c9, 0x15111011, 0x05, 0x00, 0, 2);
			armcp15_moveARM2CP(c9, 0x07dd1e10, 0x01, 0x00, 0, 0);
			armcp15_moveARM2CP(c9, 0x0005707d, 0x01, 0x00, 0, 0);

			armcp15_moveARM2CP(c9, 0x00000000, 0x07, 0x0a, 0, 4);
			armcp15_moveARM2CP(c9, 0x02004000, 0x07, 0x05, 0, 1);
			armcp15_moveARM2CP(c9, 0x02004000, 0x07, 0x0e, 0, 1);

			/* set initial timer state */

			MMU_write16(0, REG_TM0CNTL, 0x0000);
			MMU_write16(0, REG_TM0CNTH, 0x00C1);
			MMU_write16(1, REG_TM0CNTL, 0x0000);
			MMU_write16(1, REG_TM0CNTH, 0x00C1);
			MMU_write16(1, REG_TM1CNTL, 0xf7e7);
			MMU_write16(1, REG_TM1CNTH, 0x00C1);

			/* set initial interrupt state */

			MMU.reg_IME[0] = 0x00000001;
			MMU.reg_IE[0]  = 0x00042001;
			MMU.reg_IME[1] = 0x00000001;
			MMU.reg_IE[1]  = 0x0104009d;
		}
		else if (frames > 0)
		{
			/* execute boot code */
			int i;
			for (i=0; i<frames; i++)
				NDS_exec_frame(0, 0);
			i = 0;
		}

		/* load state */

		load_setstate();
		free(loaderwork.state);
		loaderwork.state = 0;

		if (frames == -1)
		{
			armcp15_moveARM2CP(c9, (NDS_ARM9.R13_irq & 0x0fff0000) | 0x0a, 0x09, 0x01, 0, 0);
		}

		/* restore timer state */

		for (proc = 0; proc < 2; proc++)
		{
			MMU_write16(proc, REG_TM0CNTH, T1ReadWord(MMU.MMU_MEM[proc][0x40], REG_TM0CNTH & 0xFFF));
			MMU_write16(proc, REG_TM1CNTH, T1ReadWord(MMU.MMU_MEM[proc][0x40], REG_TM1CNTH & 0xFFF));
			MMU_write16(proc, REG_TM2CNTH, T1ReadWord(MMU.MMU_MEM[proc][0x40], REG_TM2CNTH & 0xFFF));
			MMU_write16(proc, REG_TM3CNTH, T1ReadWord(MMU.MMU_MEM[proc][0x40], REG_TM3CNTH & 0xFFF));
		}
	}
	else if (frames > 0)
	{
		/* skip 1 sec */
		int i;
		for (i=0; i<frames; i++)
			NDS_exec_frame(0, 0);
	}
	execute = TRUE;
	sndifwork.xfs_load = 1;
	return XSF_TRUE;
}

int xsf_gen(void *pbuffer, unsigned samples)
{
	unsigned char *ptr = pbuffer;
	unsigned bytes = samples <<= 2;
	if (!sndifwork.xfs_load) return 0;
	while (bytes)
	{
		unsigned remainbytes = sndifwork.filled - sndifwork.used;
		if (remainbytes > 0)
		{
			if (remainbytes > bytes)
			{
				memcpy(ptr, sndifwork.pcmbuftop + sndifwork.used, bytes);
				sndifwork.used += bytes;
				ptr += bytes;
				remainbytes -= bytes; /**/
				bytes = 0;  /**/
				break;
			}
			else
			{
				memcpy(ptr, sndifwork.pcmbuftop + sndifwork.used, remainbytes);
				sndifwork.used += remainbytes;
				ptr += remainbytes;
				bytes -= remainbytes;
				remainbytes = 0;
			}
		}
		if (remainbytes == 0)
		{

/*
#define HBASE_CYCLES (16756000*2)
#define HBASE_CYCLES (33512000*1)
#define HBASE_CYCLES (33509300.322234)
*/
#define HBASE_CYCLES (33509300.322234)
#define HLINE_CYCLES (6 * (99 + 256))
#define HSAMPLES ((u32)((44100.0 * HLINE_CYCLES) / HBASE_CYCLES))
#define VDIVISION 100
#define VLINES 263
#define VBASE_CYCLES (((double)HBASE_CYCLES) / VDIVISION)
#define VSAMPLES ((u32)((44100.0 * HLINE_CYCLES * VLINES) / HBASE_CYCLES))

			int numsamples;
			if (sndifwork.sync_type == 1)
			{
				/* vsync */
				sndifwork.cycles += ((44100 / VDIVISION) * HLINE_CYCLES * VLINES);
				if (sndifwork.cycles >= (u32)(VBASE_CYCLES * (VSAMPLES + 1)))
				{
					numsamples = (VSAMPLES + 1);
					sndifwork.cycles -= (u32)(VBASE_CYCLES * (VSAMPLES + 1));
				}
				else
				{
					numsamples = (VSAMPLES + 0);
					sndifwork.cycles -= (u32)(VBASE_CYCLES * (VSAMPLES + 0));
				}
				NDS_exec_frame(sndifwork.arm9_clockdown_level, sndifwork.arm7_clockdown_level);
			}
			else
			{
				/* hsync */
				sndifwork.cycles += (44100 * HLINE_CYCLES);
				if (sndifwork.cycles >= (u32)(HBASE_CYCLES * (HSAMPLES + 1)))
				{
					numsamples = (HSAMPLES + 1);
					sndifwork.cycles -= (u32)(HBASE_CYCLES * (HSAMPLES + 1));
				}
				else
				{
					numsamples = (HSAMPLES + 0);
					sndifwork.cycles -= (u32)(HBASE_CYCLES * (HSAMPLES + 0));
				}
				NDS_exec_hframe(sndifwork.arm9_clockdown_level, sndifwork.arm7_clockdown_level);
			}
			SPU_EmulateSamples(numsamples);
		}
	}
	return ptr - (unsigned char *)pbuffer;
}

void xsf_term(void)
{
	MMU_unsetRom();
	NDS_DeInit();
	load_term();
}
