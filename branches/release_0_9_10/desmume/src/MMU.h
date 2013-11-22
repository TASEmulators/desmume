/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2007 shash
	Copyright (C) 2007-2012 DeSmuME team

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

#ifndef MMU_H
#define MMU_H

#include "FIFO.h"
#include "mem.h"
#include "registers.h"
#include "mc.h"
#include "bits.h"
#include "readwrite.h"
#include "debug.h"
#include "firmware.h"

#ifdef HAVE_LUA
#include "lua-engine.h"
#endif

#ifdef HAVE_JIT
#include "arm_jit.h"
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

//TODO
//n.b. this may be a bad idea, for complex registers like the dma control register.
//we need to know exactly what part was written to, instead of assuming all 32bits were written.
class TRegister_32
{
public:
	virtual u32 read32() = 0;
	virtual void write32(const u32 val) = 0;
	void write(const int size, const u32 adr, const u32 val) { 
		if(size==32) write32(val);
		else {
			const u32 offset = adr&3;
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
			if(size==8) { printf("WARNING! 8BIT DMA ACCESS\n"); return (read32()>>(offset<<3))&0xFF; }
			else return (read32()>>(offset<<3))&0xFFFF;
		}
	}
};

struct TGXSTAT : public TRegister_32
{
	TGXSTAT() {
		gxfifo_irq = se = tr = tb = sb = 0;
		fifo_empty = true;
		fifo_low = false;
	}
	u8 tb; //test busy
	u8 tr; //test result
	u8 se; //stack error
	u8 sb; //stack busy
	u8 gxfifo_irq; //irq configuration

	bool fifo_empty, fifo_low;

	virtual u32 read32();
	virtual void write32(const u32 val);

	void savestate(EMUFILE *f);
	bool loadstate(EMUFILE *f);
};

void triggerDma(EDMAMode mode);

class DivController
{
public:
	DivController()
		: mode(0), busy(0)
	{}
	void exec();
	u8 mode, busy, div0;
	u16 read16() { return mode|(busy<<15)|(div0<<14); }
	void write16(u16 val) { 
		mode = val&3;
		//todo - do we clear the div0 flag here or is that strictly done by the divider unit?
	}
	void savestate(EMUFILE* os)
	{
		write8le(&mode,os);
		write8le(&busy,os);
		write8le(&div0,os);
	}
	bool loadstate(EMUFILE* is, int version)
	{
		int ret = 1;
		ret &= read8le(&mode,is);
		ret &= read8le(&busy,is);
		ret &= read8le(&div0,is);
		return ret==1;
	}
};

class SqrtController
{
public:
	SqrtController()
		: mode(0), busy(0)
	{}
	void exec();
	u8 mode, busy;
	u16 read16() { return mode|(busy<<15); }
	void write16(u16 val) { mode = val&1; }
	void savestate(EMUFILE* os)
	{
		write8le(&mode,os);
		write8le(&busy,os);
	}
	bool loadstate(EMUFILE* is, int version)
	{
		int ret=1;
		ret &= read8le(&mode,is);
		ret &= read8le(&busy,is);
		return ret==1;
	}
};


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
	u32 saddr_user, daddr_user;
	
	//indicates whether the dma needs to be checked for triggering
	BOOL dmaCheck;

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
	template<int PROCNUM> void doCopy();
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
		saddr_user(0), daddr_user(0),
		dmaCheck(FALSE),
		running(FALSE),
		paused(FALSE),
		triggered(FALSE),
		nextEvent(0),
		sad(&saddr_user),
		dad(&daddr_user)
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

enum eCardMode
{
	//when the GC system is first booted up, the protocol is in raw mode
	eCardMode_RAW = 0,
	
	//an intermediate stage during the protocol bootup process. commands are KEY1 encrypted.
	eCardMode_KEY1,

	//an intermediate stage during the protocol bootup process. commands are KEY1 encrypted, while replies are KEY2 encrypted
	eCardMode_KEY2,

	//the final stage of the protocol bootup process. "main data load" mode. commands are KEY2 encrypted, and replies are KEY2 encrypted.
	//optionally, according to some flag we havent designed yet but should probably go in GCBUS_Controller, the whole KEY2 part can be bypassed
	//(this is typical when skipping the firmware boot process)
	eCardMode_NORMAL,

	//help fix to 32bit
	eCardMode_Pad = 0xFFFFFFFF
};

//#define GCLOG(...) printf(__VA_ARGS__);
#define GCLOG(...)

void MMU_GC_endTransfer(u32 PROCNUM);

struct GC_Command
{
	u8 bytes[8];
	void print()
	{
		GCLOG("%02X%02X%02X%02X%02X%02X%02X%02X\n",bytes[0],bytes[1],bytes[2],bytes[3],bytes[4],bytes[5],bytes[6],bytes[7]);
	}
	void toCryptoBuffer(u32 buf[2])
	{
		u8 temp[8] = { bytes[7], bytes[6], bytes[5], bytes[4], bytes[3], bytes[2], bytes[1], bytes[0] };
		buf[0] = T1ReadLong(temp,0);
		buf[1] = T1ReadLong(temp,4);
	}
	void fromCryptoBuffer(u32 buf[2])
	{
		bytes[7] = (buf[0]>>0)&0xFF;
		bytes[6] = (buf[0]>>8)&0xFF;
		bytes[5] = (buf[0]>>16)&0xFF;
		bytes[4] = (buf[0]>>24)&0xFF;
		bytes[3] = (buf[1]>>0)&0xFF;
		bytes[2] = (buf[1]>>8)&0xFF;
		bytes[1] = (buf[1]>>16)&0xFF;
		bytes[0] = (buf[1]>>24)&0xFF;
	}
};

//should rather be known as GCBUS controller, or somesuch
struct GCBUS_Controller
{
	int transfer_count;
	eCardMode mode; //probably only one of these
};

#define DUP2(x)  x, x
#define DUP4(x)  x, x, x, x
#define DUP8(x)  x, x, x, x,  x, x, x, x
#define DUP16(x) x, x, x, x,  x, x, x, x,  x, x, x, x,  x, x, x, x

struct MMU_struct 
{
	//ARM9 mem
	u8 ARM9_ITCM[0x8000];
	u8 ARM9_DTCM[0x4000];

	//u8 MAIN_MEM[4*1024*1024]; //expanded from 4MB to 8MB to support debug consoles
	//u8 MAIN_MEM[8*1024*1024]; //expanded from 8MB to 16MB to support dsi
	u8 MAIN_MEM[16*1024*1024]; //expanded from 8MB to 16MB to support dsi
	u8 ARM9_REG[0x1000000]; //this variable is evil and should be removed by correctly emulating all registers.
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
	u8 ARM7_ERAM[0x10000]; //64KB of exclusive WRAM
	u8 ARM7_REG[0x10000];
	u8 ARM7_WIRAM[0x10000]; //WIFI ram

	// VRAM mapping
	u8 VRAM_MAP[4][32];
	u32 LCD_VRAM_ADDR[10];
	u8 LCDCenable[10];

	//32KB of shared WRAM - can be switched between ARM7 & ARM9 in two blocks
	u8 SWIRAM[0x8000];

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
	
	//these are the user-controlled IF bits. some IF bits are generated as necessary from hardware conditions
	u32 reg_IF_bits[2];
	//these flags are set occasionally to indicate that an irq should have entered the pipeline, and processing will be deferred a tiny bit to help emulate things
	u32 reg_IF_pending[2];

	u32 reg_DISP3DCNT_bits;

	template<int PROCNUM> u32 gen_IF();

	BOOL divRunning;
	s64 divResult;
	s64 divMod;
	u64 divCycles;

	BOOL sqrtRunning;
	u32 sqrtResult;
	u64 sqrtCycles;

	u16 SPI_CNT;
	u16 SPI_CMD;
	u16 AUX_SPI_CNT;
	//u16 AUX_SPI_CMD; //zero 20-aug-2013 - this seems pointless

	u8 WRAMCNT;

	u64 gfx3dCycles;

	u8 powerMan_CntReg;
	BOOL powerMan_CntRegWritten;
	u8 powerMan_Reg[5];

	fw_memory_chip fw;

	GCBUS_Controller dscard[2];
};


//everything in here is derived from libnds behaviours. no hardware tests yet
class DSI_TSC
{
public:
	DSI_TSC();
	void reset_command();
	u16 write16(u16 val);
	bool save_state(EMUFILE* os);
	bool load_state(EMUFILE* is);

private:
	u16 read16();
	u8 reg_selection;
	u8 read_flag;
	s32 state;
	s32 readcount;

	//registers[0] contains the current page.
	//we are going to go ahead and save these out in case we want to change the way this is emulated in the future..
	//we may want to poke registers in here at more convenient times and have the TSC dumbly pluck them out,
	//rather than generate the values on the fly
	u8 registers[0x80];
};


//this contains things which can't be memzeroed because they are smarter classes
struct MMU_struct_new
{
	MMU_struct_new() ;
	BackupDevice backupDevice;
	DmaController dma[2][4];
	TGXSTAT gxstat;
	SqrtController sqrt;
	DivController div;
	DSI_TSC dsi_tsc;

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
void SetupMMU(bool debugConsole, bool dsi);

FORCEINLINE void CheckMemoryDebugEvent(EDEBUG_EVENT event, const MMU_ACCESS_TYPE type, const u32 procnum, const u32 addr, const u32 size, const u32 val)
{
	//TODO - ugh work out a better prefetch event system
	if(type == MMU_AT_CODE && event == DEBUG_EVENT_READ)
		event = DEBUG_EVENT_EXECUTE;
	if(CheckDebugEvent(event))
	{
		DebugEventData.memAccessType = type;
		DebugEventData.procnum = procnum;
		DebugEventData.addr = addr;
		DebugEventData.size = size;
		DebugEventData.val = val;
		HandleDebugEvent(event);
	}
}


//ALERT!!!!!!!!!!!!!!
//the following inline functions dont do the 0x0FFFFFFF mask.
//this may result in some unexpected behavior

FORCEINLINE u8 _MMU_read08(const int PROCNUM, const MMU_ACCESS_TYPE AT, const u32 addr)
{
	CheckMemoryDebugEvent(DEBUG_EVENT_READ,AT,PROCNUM,addr,8,0);

	//special handling to un-protect the ARM7 bios during debug reading
	if(PROCNUM == ARMCPU_ARM7 && AT == MMU_AT_DEBUG && addr<0x00004000)
	{
		return T1ReadByte(MMU.ARM7_BIOS, addr);
	}

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
	CheckMemoryDebugEvent(DEBUG_EVENT_READ,AT,PROCNUM,addr,16,0);

	//special handling to un-protect the ARM7 bios during debug reading
	if(PROCNUM == ARMCPU_ARM7 && AT == MMU_AT_DEBUG && addr<0x00004000)
	{
		return T1ReadWord_guaranteedAligned(MMU.ARM7_BIOS, addr);
	}

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
	CheckMemoryDebugEvent(DEBUG_EVENT_READ,AT,PROCNUM,addr,32,0);

	//special handling to un-protect the ARM7 bios during debug reading
	if(PROCNUM == ARMCPU_ARM7 && AT == MMU_AT_DEBUG && addr<0x00004000)
	{
		return T1ReadLong_guaranteedAligned(MMU.ARM7_BIOS, addr);
	}

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

		//what happens when we execute from DTCM? nocash makes it look like we get 0xFFFFFFFF but i can't seem to verify it
		//historically, desmume would fall through to its old memory map struct
		//which would return unused memory (0)
		//it seems the hardware returns 0 or something benign because in actuality 0xFFFFFFFF is an undefined opcode
		//and we know our handling for that is solid

		goto dunno;
	}

	//special handling for execution from arm7. try reading from main memory first
	if(PROCNUM==ARMCPU_ARM7)
	{
		if ( (addr & 0x0F000000) == 0x02000000)
			return T1ReadLong_guaranteedAligned( MMU.MAIN_MEM, addr & _MMU_MAIN_MEM_MASK32);
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
	CheckMemoryDebugEvent(DEBUG_EVENT_WRITE,AT,PROCNUM,addr,8,val);

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
#ifdef HAVE_JIT
		JIT_COMPILED_FUNC_KNOWNBANK(addr, MAIN_MEM, _MMU_MAIN_MEM_MASK, 0) = 0;
#endif
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
	CheckMemoryDebugEvent(DEBUG_EVENT_WRITE,AT,PROCNUM,addr,16,val);

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
#ifdef HAVE_JIT
		JIT_COMPILED_FUNC_KNOWNBANK(addr, MAIN_MEM, _MMU_MAIN_MEM_MASK16, 0) = 0;
#endif
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
	CheckMemoryDebugEvent(DEBUG_EVENT_WRITE,AT,PROCNUM,addr,32,val);

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
#ifdef HAVE_JIT
		JIT_COMPILED_FUNC_KNOWNBANK(addr, MAIN_MEM, _MMU_MAIN_MEM_MASK32, 0) = 0;
		JIT_COMPILED_FUNC_KNOWNBANK(addr, MAIN_MEM, _MMU_MAIN_MEM_MASK32, 1) = 0;
#endif
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


//#ifdef MMU_ENABLE_ACL
//	void FASTCALL MMU_write8_acl(u32 proc, u32 adr, u8 val);
//	void FASTCALL MMU_write16_acl(u32 proc, u32 adr, u16 val);
//	void FASTCALL MMU_write32_acl(u32 proc, u32 adr, u32 val);
//	u8 FASTCALL MMU_read8_acl(u32 proc, u32 adr, u32 access);
//	u16 FASTCALL MMU_read16_acl(u32 proc, u32 adr, u32 access);
//	u32 FASTCALL MMU_read32_acl(u32 proc, u32 adr, u32 access);
//#else
//	#define MMU_write8_acl(proc, adr, val)  _MMU_write08<proc>(adr, val)
//	#define MMU_write16_acl(proc, adr, val) _MMU_write16<proc>(adr, val)
//	#define MMU_write32_acl(proc, adr, val) _MMU_write32<proc>(adr, val)
//	#define MMU_read8_acl(proc,adr,access)  _MMU_read08<proc>(adr)
//	#define MMU_read16_acl(proc,adr,access) ((access==CP15_ACCESS_EXECUTE)?_MMU_read16<proc,MMU_AT_CODE>(adr):_MMU_read16<proc,MMU_AT_DATA>(adr))
//	#define MMU_read32_acl(proc,adr,access) ((access==CP15_ACCESS_EXECUTE)?_MMU_read32<proc,MMU_AT_CODE>(adr):_MMU_read32<proc,MMU_AT_DATA>(adr))
//#endif

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
