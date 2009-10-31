/*	Copyright (C) 2006 yopyop
    yopyop156@ifrance.com
    yopyop156.ifrance.com 

	Copyright (C) 2007 shash
	Copyright (C) 2007-2009 DeSmuME team

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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef MMU_H
#define MMU_H

#include "FIFO.h"
#include "mem.h"
#include "registers.h"
#include "mc.h"
#include "bits.h"
#ifdef HAVE_LUA
#include "lua-engine.h"
#endif

#define ARMCPU_ARM7 1
#define ARMCPU_ARM9 0
#define ARMPROC (PROCNUM ? NDS_ARM7:NDS_ARM9)

typedef const u8 TWaitState;


enum EDMAMode
{
	EDMAMode_Immediate = 0,
	EDMAMode_VBlank = 1,
	EDMAMode_HBlank = 2,
	EDMAMode_HStart = 3,
	EDMAMode_MemDisplay = 4,
	EDMAMode_Card = 5,
	EDMAMode_GBASlot = 6,
	EDMAMode_GXFifo = 7,
	EDMAMode7_Wifi = 8,
	EDMAMode7_GBASlot = 9,
};

enum EDMABitWidth
{
	EDMABitWidth_16 = 0,
	EDMABitWidth_32 = 1
};

enum EDMASourceUpdate
{
	EDMASourceUpdate_Increment = 0,
	EDMASourceUpdate_Decrement = 1,
	EDMASourceUpdate_Fixed = 2,
	EDMASourceUpdate_Invalid = 3,
};

enum EDMADestinationUpdate
{
	EDMADestinationUpdate_Increment = 0,
	EDMADestinationUpdate_Decrement = 1,
	EDMADestinationUpdate_Fixed = 2,
	EDMADestinationUpdate_IncrementReload = 3,
};


class TRegister_32
{
public:
	virtual u32 read32() = 0;
	virtual void write32(const u32 val) = 0;
	void write(const int size, const u32 adr, const u32 val) { 
		if(size==32) write32(val);
		else {
			const u32 offset = adr&3;
			const u32 baseaddr = adr&~offset;
			if(size==8) {
				printf("WARNING! 8BIT DMA ACCESS\n"); 
				u32 mask = 0xFF<<(offset<<3);
				write32((read32()&~mask)|(val<<(offset<<3)));
			}
			else if(size==16) {
				u32 mask = 0xFFFF<<(offset<<3);
				write32((read32()&~mask)|(val<<(offset<<3)));
			}
		}
	}

	u32 read(const int size, const u32 adr)
	{
		if(size==32) return read32();
		else {
			const u32 offset = adr&3;
			const u32 baseaddr = adr&~offset;
			if(size==8) { printf("WARNING! 8BIT DMA ACCESS\n"); return (read32()>>(offset<<3))&0xFF; }
			else return (read32()>>(offset<<3))&0xFFFF;
		}
	}
};

struct TGXSTAT : public TRegister_32
{
	TGXSTAT() {
		gxfifo_irq = se = tr = tb = 0;
	}
	u8 tb; //test busy
	u8 tr; //test result
	u8 se; //stack error
	u8 gxfifo_irq; //irq configuration

	virtual u32 read32();
	virtual void write32(const u32 val);

	void savestate(EMUFILE *f);
	bool loadstate(EMUFILE *f);
};

void triggerDma(EDMAMode mode);

class DmaController
{
public:
	u8 enable, irq, repeatMode, _startmode;
	u8 userEnable;
	u32 wordcount;
	EDMAMode startmode;
	EDMABitWidth bitWidth;
	EDMASourceUpdate sar;
	EDMADestinationUpdate dar;
	u32 saddr, daddr;
	
	//indicates whether the dma needs to be checked for triggering
	BOOL check;

	//indicates whether the dma right now is logically running
	//(though for now we copy all the data when it triggers)
	BOOL running;

	BOOL paused;

	//this flag will sometimes be set when a start condition is triggered
	//other conditions may be automatically triggered based on scanning conditions
	BOOL triggered;

	u64 nextEvent;

	int procnum, chan;

	void savestate(EMUFILE *f);
	bool loadstate(EMUFILE *f);

	void exec();
	void doCopy();
	void doPause();
	void doStop();
	void doSchedule();
	void tryTrigger(EDMAMode mode);

	DmaController() :
		enable(0), irq(0), repeatMode(0), _startmode(0),
		wordcount(0), startmode(EDMAMode_Immediate),
		bitWidth(EDMABitWidth_16),
		sar(EDMASourceUpdate_Increment), dar(EDMADestinationUpdate_Increment),
		//if saddr isnt cleared then rings of fate will trigger copy protection
		//by inspecting dma3 saddr when it boots
		saddr(0), daddr(0),
		check(FALSE),
		running(FALSE),
		paused(FALSE),
		triggered(FALSE),
		nextEvent(0),
		sad(&saddr),
		dad(&daddr)
	{
		sad.controller = this;
		dad.controller = this;
		ctrl.controller = this;
		regs[0] = &sad;
		regs[1] = &dad;
		regs[2] = &ctrl;
	}

	class AddressRegister : public TRegister_32 {
	public:
		//we pass in a pointer to the controller here so we can alert it if anything changes
		DmaController* controller;
		u32 * const ptr;
		AddressRegister(u32* _ptr)
			: ptr(_ptr)
		{}
		virtual u32 read32() { 
			return *ptr;
		}
		virtual void write32(const u32 val) {
			*ptr = val;
		}
	};

	class ControlRegister : public TRegister_32 {
	public:
		//we pass in a pointer to the controller here so we can alert it if anything changes
		DmaController* controller;
		ControlRegister() {}
		virtual u32 read32() { 
			return controller->read32();
		}
		virtual void write32(const u32 val) {
			return controller->write32(val);
		}
	};

	AddressRegister sad, dad;
	ControlRegister ctrl;
	TRegister_32* regs[3];

	void write32(const u32 val);
	u32 read32();

};

enum ECardMode
{
	CardMode_Normal = 0,
	CardMode_KEY1,
	CardMode_KEY2,
};

typedef struct
{
	
	u8 command[8];

	u32 address;
	u32 transfer_count;

	ECardMode mode;

	// NJSD stuff
	int blocklen;
	
} nds_dscard;

struct MMU_struct 
{
	//ARM9 mem
	u8 ARM9_ITCM[0x8000];
    u8 ARM9_DTCM[0x4000];
    u8 MAIN_MEM[0x800000]; //this has been expanded to 8MB to support debug consoles
    u8 ARM9_REG[0x1000000];
    u8 ARM9_BIOS[0x8000];
    u8 ARM9_VMEM[0x800];
	
	#include "PACKED.h"
	struct {
		u8 ARM9_LCD[0xA4000];
		//an extra 128KB for blank memory, directly after arm9_lcd, so that
		//we can easily map things to the end of arm9_lcd to represent 
		//an unmapped state
		u8 blank_memory[0x20000];  
	};
	#include "PACKED_END.h"

    u8 ARM9_OAM[0x800];

	u8* ExtPal[2][4];
	u8* ObjExtPal[2][2];
	
	struct TextureInfo {
		u8* texPalSlot[6];
		u8* textureSlotAddr[4];
	} texInfo;

	//ARM7 mem
	u8 ARM7_BIOS[0x4000];
	u8 ARM7_ERAM[0x10000];
	u8 ARM7_REG[0x10000];
	u8 ARM7_WIRAM[0x10000];

	// VRAM mapping
	u8 VRAM_MAP[4][32];
	u32 LCD_VRAM_ADDR[10];
	u8 LCDCenable[10];

	//Shared ram
	u8 SWIRAM[0x8000];

	//Card rom & ram
	u8 * CART_ROM;
	u32 CART_ROM_MASK;
	u8 CART_RAM[0x10000];

	//Unused ram
	u8 UNUSED_RAM[4];

	//this is here so that we can trap glitchy emulator code
	//which is accessing offsets 5,6,7 of unused ram due to unaligned accesses
	//(also since the emulator doesn't prevent unaligned accesses)
	u8 MORE_UNUSED_RAM[4];

	static u8 * MMU_MEM[2][256];
	static u32 MMU_MASK[2][256];

	u8 ARM9_RW_MODE;

	u32 DTCMRegion;
	u32 ITCMRegion;

	u16 timer[2][4];
	s32 timerMODE[2][4];
	u32 timerON[2][4];
	u32 timerRUN[2][4];
	u16 timerReload[2][4];

	u32 reg_IME[2];
	u32 reg_IE[2];
	u32 reg_IF[2];

	BOOL divRunning;
	s64 divResult;
	s64 divMod;
	u32 divCnt;
	u64 divCycles;

	BOOL sqrtRunning;
	u32 sqrtResult;
	u32 sqrtCnt;
	u64 sqrtCycles;

	u16 SPI_CNT;
	u16 SPI_CMD;
	u16 AUX_SPI_CNT;
	u16 AUX_SPI_CMD;

	u64 gfx3dCycles;

	u8 powerMan_CntReg;
	BOOL powerMan_CntRegWritten;
	u8 powerMan_Reg[4];

	memory_chip_t fw;

	nds_dscard dscard[2];
};

//this contains things which can't be memzeroed because they are smarter classes
struct MMU_struct_new
{
	MMU_struct_new() ;
	BackupDevice backupDevice;
	DmaController dma[2][4];
	TGXSTAT gxstat;

	void write_dma(const int proc, const int size, const u32 adr, const u32 val);
	u32 read_dma(const int proc, const int size, const u32 adr);
	bool is_dma(const u32 adr) { return adr >= _REG_DMA_CONTROL_MIN && adr <= _REG_DMA_CONTROL_MAX; }
};

extern MMU_struct MMU;
extern MMU_struct_new MMU_new;


struct armcpu_memory_iface {
  /** the 32 bit instruction prefetch */
  u32 FASTCALL (*prefetch32)( void *data, u32 adr);

  /** the 16 bit instruction prefetch */
  u16 FASTCALL (*prefetch16)( void *data, u32 adr);

  /** read 8 bit data value */
  u8 FASTCALL (*read8)( void *data, u32 adr);
  /** read 16 bit data value */
  u16 FASTCALL (*read16)( void *data, u32 adr);
  /** read 32 bit data value */
  u32 FASTCALL (*read32)( void *data, u32 adr);

  /** write 8 bit data value */
  void FASTCALL (*write8)( void *data, u32 adr, u8 val);
  /** write 16 bit data value */
  void FASTCALL (*write16)( void *data, u32 adr, u16 val);
  /** write 32 bit data value */
  void FASTCALL (*write32)( void *data, u32 adr, u32 val);

  void *data;
};


void MMU_Init(void);
void MMU_DeInit(void);

void MMU_Reset( void);

void MMU_setRom(u8 * rom, u32 mask);
void MMU_unsetRom( void);

void print_memory_profiling( void);

// Memory reading/writing (old)
u8 FASTCALL MMU_read8(u32 proc, u32 adr);
u16 FASTCALL MMU_read16(u32 proc, u32 adr);
u32 FASTCALL MMU_read32(u32 proc, u32 adr);
void FASTCALL MMU_write8(u32 proc, u32 adr, u8 val);
void FASTCALL MMU_write16(u32 proc, u32 adr, u16 val);
void FASTCALL MMU_write32(u32 proc, u32 adr, u32 val);
 
//template<int PROCNUM> void FASTCALL MMU_doDMA(u32 num);

//The base ARM memory interfaces
extern struct armcpu_memory_iface arm9_base_memory_iface;
extern struct armcpu_memory_iface arm7_base_memory_iface;
extern struct armcpu_memory_iface arm9_direct_memory_iface;	

#define VRAM_BANKS 9
#define VRAM_BANK_A 0
#define VRAM_BANK_B 1
#define VRAM_BANK_C 2
#define VRAM_BANK_D 3
#define VRAM_BANK_E 4
#define VRAM_BANK_F 5
#define VRAM_BANK_G 6
#define VRAM_BANK_H 7
#define VRAM_BANK_I 8

#define VRAM_PAGE_ABG 0
#define VRAM_PAGE_BBG 128
#define VRAM_PAGE_AOBJ 256
#define VRAM_PAGE_BOBJ 384


struct VramConfiguration {

	enum Purpose {
		OFF, INVALID, ABG, BBG, AOBJ, BOBJ, LCDC, ARM7, TEX, TEXPAL, ABGEXTPAL, BBGEXTPAL, AOBJEXTPAL, BOBJEXTPAL
	};

	struct BankInfo {
		Purpose purpose;
		int ofs;
	} banks[VRAM_BANKS];
	
	inline void clear() {
		for(int i=0;i<VRAM_BANKS;i++) {
			banks[i].ofs = 0;
			banks[i].purpose = OFF;
		}
	}

	std::string describePurpose(Purpose p);
	std::string describe();
};

extern VramConfiguration vramConfiguration;

#define VRAM_ARM9_PAGES 512
extern u8 vram_arm9_map[VRAM_ARM9_PAGES];
FORCEINLINE void* MMU_gpu_map(u32 vram_addr)
{
	//this is supposed to map a single gpu vram address to emulator host memory
	//but it returns a pointer to some zero memory in case of accesses to unmapped memory.
	//this correctly handles the case with tile accesses to unmapped memory.
	//it could also potentially go through a different LUT than vram_arm9_map in case we discover
	//that it needs to be set up with different or no mirroring
	//(I think it is a reasonable possibility that only the cpu has the nutty mirroring rules)
	//
	//if this system isn't used, Fantasy Aquarium displays garbage in the first ingame screen
	//due to it storing 0x0F0F or somesuch in screen memory which points to a ridiculously big tile
	//which should contain all 0 pixels

	u32 vram_page = (vram_addr>>14)&(VRAM_ARM9_PAGES-1);
	u32 ofs = vram_addr & 0x3FFF;
	vram_page = vram_arm9_map[vram_page];
	//blank pages are handled by the extra 16KB of blank memory at the end of ARM9_LCD
	//and the fact that blank pages are mapped to appear at that location
	return MMU.ARM9_LCD + (vram_page<<14) + ofs;
}


enum MMU_ACCESS_TYPE
{
	MMU_AT_CODE, MMU_AT_DATA, MMU_AT_GPU, MMU_AT_DMA
};

template<int PROCNUM, MMU_ACCESS_TYPE AT> u8 _MMU_read08(u32 addr);
template<int PROCNUM, MMU_ACCESS_TYPE AT> u16 _MMU_read16(u32 addr);
template<int PROCNUM, MMU_ACCESS_TYPE AT> u32 _MMU_read32(u32 addr);
template<int PROCNUM, MMU_ACCESS_TYPE AT> void _MMU_write08(u32 addr, u8 val);
template<int PROCNUM, MMU_ACCESS_TYPE AT> void _MMU_write16(u32 addr, u16 val);
template<int PROCNUM, MMU_ACCESS_TYPE AT> void _MMU_write32(u32 addr, u32 val);

template<int PROCNUM> FORCEINLINE u8 _MMU_read08(u32 addr) { return _MMU_read08<PROCNUM, MMU_AT_DATA>(addr); }
template<int PROCNUM> FORCEINLINE u16 _MMU_read16(u32 addr) { return _MMU_read16<PROCNUM, MMU_AT_DATA>(addr); }
template<int PROCNUM> FORCEINLINE u32 _MMU_read32(u32 addr) { return _MMU_read32<PROCNUM, MMU_AT_DATA>(addr); }
template<int PROCNUM> FORCEINLINE void _MMU_write08(u32 addr, u8 val) { _MMU_write08<PROCNUM, MMU_AT_DATA>(addr,val); }
template<int PROCNUM> FORCEINLINE void _MMU_write16(u32 addr, u16 val) { _MMU_write16<PROCNUM, MMU_AT_DATA>(addr,val); }
template<int PROCNUM> FORCEINLINE void _MMU_write32(u32 addr, u32 val) { _MMU_write32<PROCNUM, MMU_AT_DATA>(addr,val); }

void FASTCALL _MMU_ARM9_write08(u32 adr, u8 val);
void FASTCALL _MMU_ARM9_write16(u32 adr, u16 val);
void FASTCALL _MMU_ARM9_write32(u32 adr, u32 val);
u8  FASTCALL _MMU_ARM9_read08(u32 adr);
u16 FASTCALL _MMU_ARM9_read16(u32 adr);
u32 FASTCALL _MMU_ARM9_read32(u32 adr);

void FASTCALL _MMU_ARM7_write08(u32 adr, u8 val);
void FASTCALL _MMU_ARM7_write16(u32 adr, u16 val);
void FASTCALL _MMU_ARM7_write32(u32 adr, u32 val);
u8  FASTCALL _MMU_ARM7_read08(u32 adr);
u16 FASTCALL _MMU_ARM7_read16(u32 adr);
u32 FASTCALL _MMU_ARM7_read32(u32 adr);

extern u32 partie;

extern u32 _MMU_MAIN_MEM_MASK;
extern u32 _MMU_MAIN_MEM_MASK16;
extern u32 _MMU_MAIN_MEM_MASK32;
inline void SetupMMU(BOOL debugConsole) {
	if(debugConsole) _MMU_MAIN_MEM_MASK = 0x7FFFFF;
	else _MMU_MAIN_MEM_MASK = 0x3FFFFF;
	_MMU_MAIN_MEM_MASK16 = _MMU_MAIN_MEM_MASK & ~1;
	_MMU_MAIN_MEM_MASK32 = _MMU_MAIN_MEM_MASK & ~3;
}

//TODO: at one point some of the early access code included this. consider re-adding it
  //ARM7 private memory
  //if ( (adr & 0x0f800000) == 0x03800000) {
    //T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM7][(adr >> 20) & 0xFF],
      //         adr & MMU.MMU_MASK[ARMCPU_ARM7][(adr >> 20) & 0xFF]); 

FORCEINLINE u8 _MMU_read08(const int PROCNUM, const MMU_ACCESS_TYPE AT, const u32 addr)
{
	//special handling for DMA: read 0 from TCM
	if(PROCNUM==ARMCPU_ARM9 && AT == MMU_AT_DMA)
	{
		if(addr<0x02000000) return 0; //itcm
		if((addr&(~0x3FFF)) == MMU.DTCMRegion) return 0; //dtcm
	}

#ifdef HAVE_LUA
	CallRegisteredLuaMemHook(addr, 1, /*FIXME*/ 0, LUAMEMHOOK_READ);
#endif

	if(PROCNUM==ARMCPU_ARM9)
		if((addr&(~0x3FFF)) == MMU.DTCMRegion)
		{
			//Returns data from DTCM (ARM9 only)
			return T1ReadByte(MMU.ARM9_DTCM, addr & 0x3FFF);
		}

	if ( (addr & 0x0F000000) == 0x02000000)
		return T1ReadByte( MMU.MAIN_MEM, addr & _MMU_MAIN_MEM_MASK);

	if(PROCNUM==ARMCPU_ARM9) return _MMU_ARM9_read08(addr);
	else return _MMU_ARM7_read08(addr);
}

FORCEINLINE u16 _MMU_read16(const int PROCNUM, const MMU_ACCESS_TYPE AT, const u32 addr) 
{
	//special handling for DMA: read 0 from TCM
	if(PROCNUM==ARMCPU_ARM9 && AT == MMU_AT_DMA)
	{
		if(addr<0x02000000) return 0; //itcm
		if((addr&(~0x3FFF)) == MMU.DTCMRegion) return 0; //dtcm
	}

#ifdef HAVE_LUA
	CallRegisteredLuaMemHook(addr, 2, /*FIXME*/ 0, LUAMEMHOOK_READ);
#endif

	//special handling for execution from arm9, since we spend so much time in there
	if(PROCNUM==ARMCPU_ARM9 && AT == MMU_AT_CODE)
	{
		if ((addr & 0x0F000000) == 0x02000000)
			return T1ReadWord_guaranteedAligned( MMU.MAIN_MEM, addr & _MMU_MAIN_MEM_MASK16);

		if(addr<0x02000000) 
			return T1ReadWord_guaranteedAligned(MMU.ARM9_ITCM, addr&0x7FFE);

		goto dunno;
	}

	if(PROCNUM==ARMCPU_ARM9)
		if((addr&(~0x3FFF)) == MMU.DTCMRegion)
		{
			//Returns data from DTCM (ARM9 only)
			return T1ReadWord_guaranteedAligned(MMU.ARM9_DTCM, addr & 0x3FFE);
		}

	if ( (addr & 0x0F000000) == 0x02000000)
		return T1ReadWord_guaranteedAligned( MMU.MAIN_MEM, addr & _MMU_MAIN_MEM_MASK16);

dunno:
	if(PROCNUM==ARMCPU_ARM9) return _MMU_ARM9_read16(addr);
	else return _MMU_ARM7_read16(addr);
}

FORCEINLINE u32 _MMU_read32(const int PROCNUM, const MMU_ACCESS_TYPE AT, const u32 addr)
{
	//special handling for DMA: read 0 from TCM
	if(PROCNUM==ARMCPU_ARM9 && AT == MMU_AT_DMA)
	{
		if(addr<0x02000000) return 0; //itcm
		if((addr&(~0x3FFF)) == MMU.DTCMRegion) return 0; //dtcm
	}

#ifdef HAVE_LUA
	CallRegisteredLuaMemHook(addr, 4, /*FIXME*/ 0, LUAMEMHOOK_READ);
#endif

	//special handling for execution from arm9, since we spend so much time in there
	if(PROCNUM==ARMCPU_ARM9 && AT == MMU_AT_CODE)
	{
		if ( (addr & 0x0F000000) == 0x02000000)
			return T1ReadLong_guaranteedAligned( MMU.MAIN_MEM, addr & _MMU_MAIN_MEM_MASK32);

		if(addr<0x02000000) 
			return T1ReadLong_guaranteedAligned(MMU.ARM9_ITCM, addr&0x7FFC);

		goto dunno;
	}

	//special handling for execution from arm7. try reading from main memory first
	if(PROCNUM==ARMCPU_ARM7)
	{
		if ( (addr & 0x0F000000) == 0x02000000)
			return T1ReadLong_guaranteedAligned( MMU.MAIN_MEM, addr & _MMU_MAIN_MEM_MASK32);
		else if((addr & 0xFF800000) == 0x03800000)
			return T1ReadLong_guaranteedAligned(MMU.ARM7_ERAM, addr&0xFFFC);
		else if((addr & 0xFF800000) == 0x03000000)
			return T1ReadLong_guaranteedAligned(MMU.SWIRAM, addr&0x7FFC);
	}


	//for other arm9 cases, we have to check from dtcm first because it is patched on top of the main memory range
	if(PROCNUM==ARMCPU_ARM9)
	{
		if((addr&(~0x3FFF)) == MMU.DTCMRegion)
		{
			//Returns data from DTCM (ARM9 only)
			return T1ReadLong_guaranteedAligned(MMU.ARM9_DTCM, addr & 0x3FFC);
		}
	
		if ( (addr & 0x0F000000) == 0x02000000)
			return T1ReadLong_guaranteedAligned( MMU.MAIN_MEM, addr & _MMU_MAIN_MEM_MASK32);
	}

dunno:
	if(PROCNUM==ARMCPU_ARM9) return _MMU_ARM9_read32(addr);
	else return _MMU_ARM7_read32(addr);
}

FORCEINLINE void _MMU_write08(const int PROCNUM, const MMU_ACCESS_TYPE AT, const u32 addr, u8 val)
{
	//special handling for DMA: discard writes to TCM
	if(PROCNUM==ARMCPU_ARM9 && AT == MMU_AT_DMA)
	{
		if(addr<0x02000000) return; //itcm
		if((addr&(~0x3FFF)) == MMU.DTCMRegion) return; //dtcm
	}

	if(PROCNUM==ARMCPU_ARM9)
		if((addr&(~0x3FFF)) == MMU.DTCMRegion)
		{
			T1WriteByte(MMU.ARM9_DTCM, addr & 0x3FFF, val);
#ifdef HAVE_LUA
			CallRegisteredLuaMemHook(addr, 1, val, LUAMEMHOOK_WRITE);
#endif
			return;
		}

	if ( (addr & 0x0F000000) == 0x02000000) {
		T1WriteByte( MMU.MAIN_MEM, addr & _MMU_MAIN_MEM_MASK, val);
#ifdef HAVE_LUA
		CallRegisteredLuaMemHook(addr, 1, val, LUAMEMHOOK_WRITE);
#endif
		return;
	}

	if(PROCNUM==ARMCPU_ARM9) _MMU_ARM9_write08(addr,val);
	else _MMU_ARM7_write08(addr,val);
#ifdef HAVE_LUA
	CallRegisteredLuaMemHook(addr, 1, val, LUAMEMHOOK_WRITE);
#endif
}

FORCEINLINE void _MMU_write16(const int PROCNUM, const MMU_ACCESS_TYPE AT, const u32 addr, u16 val)
{
	//special handling for DMA: discard writes to TCM
	if(PROCNUM==ARMCPU_ARM9 && AT == MMU_AT_DMA)
	{
		if(addr<0x02000000) return; //itcm
		if((addr&(~0x3FFF)) == MMU.DTCMRegion) return; //dtcm
	}

	if(PROCNUM==ARMCPU_ARM9)
		if((addr&(~0x3FFF)) == MMU.DTCMRegion)
		{
			T1WriteWord(MMU.ARM9_DTCM, addr & 0x3FFE, val);
#ifdef HAVE_LUA
			CallRegisteredLuaMemHook(addr, 2, val, LUAMEMHOOK_WRITE);
#endif
			return;
		}

	if ( (addr & 0x0F000000) == 0x02000000) {
		T1WriteWord( MMU.MAIN_MEM, addr & _MMU_MAIN_MEM_MASK16, val);
#ifdef HAVE_LUA
		CallRegisteredLuaMemHook(addr, 2, val, LUAMEMHOOK_WRITE);
#endif
		return;
	}

	if(PROCNUM==ARMCPU_ARM9) _MMU_ARM9_write16(addr,val);
	else _MMU_ARM7_write16(addr,val);
#ifdef HAVE_LUA
	CallRegisteredLuaMemHook(addr, 2, val, LUAMEMHOOK_WRITE);
#endif
}

FORCEINLINE void _MMU_write32(const int PROCNUM, const MMU_ACCESS_TYPE AT, const u32 addr, u32 val)
{
	//special handling for DMA: discard writes to TCM
	if(PROCNUM==ARMCPU_ARM9 && AT == MMU_AT_DMA)
	{
		if(addr<0x02000000) return; //itcm
		if((addr&(~0x3FFF)) == MMU.DTCMRegion) return; //dtcm
	}

	if(PROCNUM==ARMCPU_ARM9)
		if((addr&(~0x3FFF)) == MMU.DTCMRegion)
		{
			T1WriteLong(MMU.ARM9_DTCM, addr & 0x3FFC, val);
#ifdef HAVE_LUA
			CallRegisteredLuaMemHook(addr, 4, val, LUAMEMHOOK_WRITE);
#endif
			return;
		}

	if ( (addr & 0x0F000000) == 0x02000000) {
		T1WriteLong( MMU.MAIN_MEM, addr & _MMU_MAIN_MEM_MASK32, val);
#ifdef HAVE_LUA
		CallRegisteredLuaMemHook(addr, 4, val, LUAMEMHOOK_WRITE);
#endif
		return;
	}

	if(PROCNUM==ARMCPU_ARM9) _MMU_ARM9_write32(addr,val);
	else _MMU_ARM7_write32(addr,val);
#ifdef HAVE_LUA
	CallRegisteredLuaMemHook(addr, 4, val, LUAMEMHOOK_WRITE);
#endif
}


#ifdef MMU_ENABLE_ACL
	void FASTCALL MMU_write8_acl(u32 proc, u32 adr, u8 val);
	void FASTCALL MMU_write16_acl(u32 proc, u32 adr, u16 val);
	void FASTCALL MMU_write32_acl(u32 proc, u32 adr, u32 val);
	u8 FASTCALL MMU_read8_acl(u32 proc, u32 adr, u32 access);
	u16 FASTCALL MMU_read16_acl(u32 proc, u32 adr, u32 access);
	u32 FASTCALL MMU_read32_acl(u32 proc, u32 adr, u32 access);
#else
	#define MMU_write8_acl(proc, adr, val)  _MMU_write08<proc>(adr, val)
	#define MMU_write16_acl(proc, adr, val) _MMU_write16<proc>(adr, val)
	#define MMU_write32_acl(proc, adr, val) _MMU_write32<proc>(adr, val)
	#define MMU_read8_acl(proc,adr,access)  _MMU_read08<proc>(adr)
	#define MMU_read16_acl(proc,adr,access) ((access==CP15_ACCESS_EXECUTE)?_MMU_read16<proc,MMU_AT_CODE>(adr):_MMU_read16<proc,MMU_AT_DATA>(adr))
	#define MMU_read32_acl(proc,adr,access) ((access==CP15_ACCESS_EXECUTE)?_MMU_read32<proc,MMU_AT_CODE>(adr):_MMU_read32<proc,MMU_AT_DATA>(adr))
#endif

// Use this macros for reading/writing, so the GDB stub isn't broken
#ifdef GDB_STUB
	#define READ32(a,b)		cpu->mem_if->read32(a,(b) & 0xFFFFFFFC)
	#define WRITE32(a,b,c)	cpu->mem_if->write32(a,(b) & 0xFFFFFFFC,c)
	#define READ16(a,b)		cpu->mem_if->read16(a,(b) & 0xFFFFFFFE)
	#define WRITE16(a,b,c)	cpu->mem_if->write16(a,(b) & 0xFFFFFFFE,c)
	#define READ8(a,b)		cpu->mem_if->read8(a,b)
	#define WRITE8(a,b,c)	cpu->mem_if->write8(a,b,c)
#else
	#define READ32(a,b)		_MMU_read32<PROCNUM>((b) & 0xFFFFFFFC)
	#define WRITE32(a,b,c)	_MMU_write32<PROCNUM>((b) & 0xFFFFFFFC,c)
	#define READ16(a,b)		_MMU_read16<PROCNUM>((b) & 0xFFFFFFFE)
	#define WRITE16(a,b,c)	_MMU_write16<PROCNUM>((b) & 0xFFFFFFFE,c)
	#define READ8(a,b)		_MMU_read08<PROCNUM>(b)
	#define WRITE8(a,b,c)	_MMU_write08<PROCNUM>(b, c)
#endif

template<int PROCNUM, MMU_ACCESS_TYPE AT>
FORCEINLINE u8 _MMU_read08(u32 addr) { return _MMU_read08(PROCNUM, AT, addr); }

template<int PROCNUM, MMU_ACCESS_TYPE AT>
FORCEINLINE u16 _MMU_read16(u32 addr) { return _MMU_read16(PROCNUM, AT, addr); }

template<int PROCNUM, MMU_ACCESS_TYPE AT>
FORCEINLINE u32 _MMU_read32(u32 addr) { return _MMU_read32(PROCNUM, AT, addr); }

template<int PROCNUM, MMU_ACCESS_TYPE AT>
FORCEINLINE void _MMU_write08(u32 addr, u8 val) { _MMU_write08(PROCNUM, AT, addr, val); }

template<int PROCNUM, MMU_ACCESS_TYPE AT>
FORCEINLINE void _MMU_write16(u32 addr, u16 val) { _MMU_write16(PROCNUM, AT, addr, val); }

template<int PROCNUM, MMU_ACCESS_TYPE AT>
FORCEINLINE void _MMU_write32(u32 addr, u32 val) { _MMU_write32(PROCNUM, AT, addr, val); }

void FASTCALL MMU_DumpMemBlock(u8 proc, u32 address, u32 size, u8 *buffer);

#endif
