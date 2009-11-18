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

// this file is split from MMU.h for the purpose of avoiding ridiculous recompile times
// when changing it, because practically everything includes MMU.h.
#ifndef MMUTIMING_H
#define MMUTIMING_H

#include <algorithm>
#include "MMU.h"
#include "cp15.h"
#include "readwrite.h"

////////////////////////////////////////////////////////////////
// MEMORY TIMING ACCURACY CONFIGURATION
//
// the more of these are enabled,
// the more accurate memory access timing _should_ become.
// they should be listed roughly in order of most to least important.
// it's reasonable to disable some of these as a speed hack.
// obviously, these defines don't cover all the variables or features needed,
// and in particular, DMA or code+data access bus contention is still missing.

	// makes non-sequential accesses slower than sequential ones.
#define ACCOUNT_FOR_NON_SEQUENTIAL_ACCESS
	//(SOMETIMES THIS IS A BIG SPEED HIT!)

	// enables emulation of code fetch waits.
#define ACCOUNT_FOR_CODE_FETCH_CYCLES

	// makes access to DTCM (arm9 only) fast.
#define ACCOUNT_FOR_DATA_TCM_SPEED

	// enables simulation of cache hits and cache misses.
#define ENABLE_CACHE_CONTROLLER_EMULATION

//
////////////////////////////////////////////////////////////////


enum MMU_ACCESS_DIRECTION
{
	MMU_AD_READ, MMU_AD_WRITE
};


// note that we don't actually emulate the cache contents here,
// only enough to guess what would be a cache hit or a cache miss.
// this doesn't really get used unless ENABLE_CACHE_CONTROLLER_EMULATION is defined.
template<int SIZESHIFT, int ASSOCIATIVESHIFT, int BLOCKSIZESHIFT>
class CacheController
{
public:
	template<MMU_ACCESS_DIRECTION DIR>
	FORCEINLINE bool Cached(u32 addr)
	{
		u32 blockMasked = addr & BLOCKMASK;
		if(blockMasked == m_cacheCache)
			return true;
		else
			return this->CachedInternal<DIR>(addr, blockMasked);
	}
	
	void Reset()
	{
		for(int blockIndex = 0; blockIndex < NUMBLOCKS; blockIndex++)
			m_blocks[blockIndex].Reset();
		m_cacheCache = ~0;
	}
	CacheController()
	{
		Reset();
	}
	
	void savestate(EMUFILE* os, int version)
	{
		write32le(m_cacheCache, os);
		for(int i = 0; i < NUMBLOCKS; i++)
		{
			for(int j = 0; j < ASSOCIATIVITY; j++)
				write32le(m_blocks[i].tag[j],os);
			write32le(m_blocks[i].nextWay,os);
		}
	}
	bool loadstate(EMUFILE* is, int version)
	{
		read32le(&m_cacheCache, is);
		for(int i = 0; i < NUMBLOCKS; i++)
		{
			for(int j = 0; j < ASSOCIATIVITY; j++)
				read32le(&m_blocks[i].tag[j],is);
			read32le(&m_blocks[i].nextWay,is);
		}
		return true;
	}

private:
	template<MMU_ACCESS_DIRECTION DIR>
	bool CachedInternal(u32 addr, u32 blockMasked)
	{
		u32 blockIndex = blockMasked >> BLOCKSIZESHIFT;
		CacheBlock& block = m_blocks[blockIndex];
		addr &= TAGMASK;

		for(int way = 0; way < ASSOCIATIVITY; way++)
			if(addr == block.tag[way])
			{
				// found it, already allocated
				m_cacheCache = blockMasked;
				return true;
			}
		if(DIR == MMU_AD_READ)
		{
			// TODO: support other allocation orders?
			block.tag[block.nextWay++] = addr;
			block.nextWay %= ASSOCIATIVITY;
			m_cacheCache = blockMasked;
		}
		return false;
	}

	enum { SIZE = 1 << SIZESHIFT };
	enum { ASSOCIATIVITY = 1 << ASSOCIATIVESHIFT };
	enum { BLOCKSIZE = 1 << BLOCKSIZESHIFT };
	enum { TAGSHIFT = SIZESHIFT - ASSOCIATIVESHIFT };
	enum { TAGMASK = (u32)(~0 << TAGSHIFT) };
	enum { BLOCKMASK = ((u32)~0 >> (32 - TAGSHIFT)) & (u32)(~0 << BLOCKSIZESHIFT) };
	enum { WORDSIZE = sizeof(u32) };
	enum { WORDSPERBLOCK = (1 << BLOCKSIZESHIFT) / WORDSIZE };
	enum { DATAPERWORD = WORDSIZE * ASSOCIATIVITY };
	enum { DATAPERBLOCK = DATAPERWORD * WORDSPERBLOCK };
	enum { NUMBLOCKS = SIZE / DATAPERBLOCK };

	struct CacheBlock
	{
		u32 tag [ASSOCIATIVITY];
		u32 nextWay;

		void Reset()
		{
			nextWay = 0;
			for(int way = 0; way < ASSOCIATIVITY; way++)
				tag[way] = 0;
		}
	};

	u32 m_cacheCache; // optimization

	CacheBlock m_blocks [NUMBLOCKS];
};


template<int PROCNUM, MMU_ACCESS_TYPE AT, int READSIZE, MMU_ACCESS_DIRECTION DIRECTION>
FORCEINLINE u32 _MMU_accesstime(u32 addr, bool sequential);


template<int PROCNUM, MMU_ACCESS_TYPE AT>
class FetchAccessUnit
{
public:
	template<int READSIZE, MMU_ACCESS_DIRECTION DIRECTION>
	FORCEINLINE u32 Fetch(u32 address)
	{
#ifndef ACCOUNT_FOR_CODE_FETCH_CYCLES
		if(AT == MMU_AT_CODE)
			return 1;
#endif

		u32 time = _MMU_accesstime<PROCNUM, AT, READSIZE, DIRECTION>(address,
#ifdef ACCOUNT_FOR_NON_SEQUENTIAL_ACCESS
			address == (m_lastAddress + (READSIZE>>3))
#else
			true
#endif
		);

#ifdef ACCOUNT_FOR_NON_SEQUENTIAL_ACCESS
		m_lastAddress = address;
#endif

		return time;
	}

	void Reset()
	{
		m_lastAddress = ~0;
	}
	FetchAccessUnit() { this->Reset(); }

	void savestate(EMUFILE* os, int version)
	{
		write32le(m_lastAddress,os);
	}
	bool loadstate(EMUFILE* is, int version)
	{
		read32le(&m_lastAddress,is);
		return true;
	}

private:
	u32 m_lastAddress;
};





struct MMU_struct_timing
{
	// technically part of the cp15, but I didn't want the dereferencing penalty.
	// these template values correspond with the value of armcp15->cacheType.
	CacheController<13,2,5> arm9codeCache; // 8192 bytes, 4-way associative, 32-byte blocks
	CacheController<12,2,5> arm9dataCache; // 4096 bytes, 4-way associative, 32-byte blocks

	// technically part of armcpu_t, but that struct isn't templated on PROCNUM
	FetchAccessUnit<0,MMU_AT_CODE> arm9codeFetch;
	FetchAccessUnit<0,MMU_AT_DATA> arm9dataFetch;
	FetchAccessUnit<1,MMU_AT_CODE> arm7codeFetch;
	FetchAccessUnit<1,MMU_AT_DATA> arm7dataFetch;

	template<int PROCNUM> FORCEINLINE FetchAccessUnit<PROCNUM,MMU_AT_CODE>& armCodeFetch();
	template<int PROCNUM> FORCEINLINE FetchAccessUnit<PROCNUM,MMU_AT_DATA>& armDataFetch();
};
template<> FORCEINLINE FetchAccessUnit<0,MMU_AT_CODE>& MMU_struct_timing::armCodeFetch<0>() { return this->arm9codeFetch; }
template<> FORCEINLINE FetchAccessUnit<1,MMU_AT_CODE>& MMU_struct_timing::armCodeFetch<1>() { return this->arm7codeFetch; }
template<> FORCEINLINE FetchAccessUnit<0,MMU_AT_DATA>& MMU_struct_timing::armDataFetch<0>() { return this->arm9dataFetch; }
template<> FORCEINLINE FetchAccessUnit<1,MMU_AT_DATA>& MMU_struct_timing::armDataFetch<1>() { return this->arm7dataFetch; }


extern MMU_struct_timing MMU_timing;



// calculates the time a single memory access takes,
// in units of cycles of the current processor.
// this function replaces what used to be MMU_WAIT16 and MMU_WAIT32.
// this may have side effects, so don't call it more than necessary.
template<int PROCNUM, MMU_ACCESS_TYPE AT, int READSIZE, MMU_ACCESS_DIRECTION DIRECTION>
FORCEINLINE u32 _MMU_accesstime(u32 addr, bool sequential)
{
	static const int MC = 1; // cached or tcm memory speed
	static const int M32 = (PROCNUM==ARMCPU_ARM9) ? 2 : 1; // access through 32-bit bus
	static const int M16 = M32 * ((READSIZE>16) ? 2 : 1); // access through 16-bit bus
	static const int MSLW = M16 * 8; // this needs tuning

	if(PROCNUM==ARMCPU_ARM9 && AT == MMU_AT_CODE && addr < 0x02000000)
		return MC; // ITCM

#ifdef ACCOUNT_FOR_DATA_TCM_SPEED
	if(PROCNUM==ARMCPU_ARM9 && AT==MMU_AT_DATA && (addr&(~0x3FFF)) == MMU.DTCMRegion)
		return MC; // DTCM
#endif

	// for now, assume the cache is always enabled for all of main memory
	if(PROCNUM==ARMCPU_ARM9 && (addr & 0x0F000000) == 0x02000000)
	{
#ifdef ENABLE_CACHE_CONTROLLER_EMULATION
		bool cached = false;
		if(AT==MMU_AT_CODE)
			cached = MMU_timing.arm9codeCache.Cached<DIRECTION>(addr);
		if(AT==MMU_AT_DATA)
			cached = MMU_timing.arm9dataCache.Cached<DIRECTION>(addr);
		if(cached)
			return MC;
		u32 c;
		if(sequential && AT==MMU_AT_DATA)
			c = M16; // bonus for sequential data access
		else if(DIRECTION == MMU_AD_READ)
			c = M16 * 5;
		else
			c = M16 * 2; // should be 4, but write buffer isn't emulated yet.
		if(DIRECTION == MMU_AD_READ)
		{
			// cache miss while reading means it has to fill a whole cache line
			// by reading 32 bytes...
			c += 8 * M32*2;
		}
		return c;
#elif defined(ACCOUNT_FOR_NON_SEQUENTIAL_ACCESS)
		// this is the closest approximation I could find
		// to the with-cache-controller timing
		// that doesn't do any actual caching logic.
		return sequential ? MC : M16;
#endif
	}

	static const TWaitState MMU_WAIT[16*16] = {
        // ITCM, ITCM, MAIN, SWI, REG, VMEM, LCD, OAM,  ROM,  ROM,  RAM,   U,  U,  U,  U, BIOS
#define X    MC,   MC,  M16, M32, M32,  M16, M16, M32, MSLW, MSLW, MSLW, M32,M32,M32,M32,  M32,
		// duplicate it 16 times (this was somehow faster than using a mask of 0xF)
		X X X X  X X X X  X X X X  X X X X
#undef X
	};

	u32 c = MMU_WAIT[(addr >> 24)];

#ifdef ACCOUNT_FOR_NON_SEQUENTIAL_ACCESS
	if(!sequential)
	{
		//if(c != MC || PROCNUM==ARMCPU_ARM7) // check not needed anymore because ITCM/DTCM return earlier
		{
			c += (PROCNUM==ARMCPU_ARM9) ? 3*2 : 1;
		}
	}
#endif

	return c;
}





// calculates the cycle time of a single memory access in the MEM stage.
// to be used to calculate the memCycles argument for MMU_aluMemCycles.
// this may have side effects, so don't call it more than necessary.
template<int PROCNUM, int READSIZE, MMU_ACCESS_DIRECTION DIRECTION>
FORCEINLINE u32 MMU_memAccessCycles(u32 addr)
{
	return MMU_timing.armDataFetch<PROCNUM>().template Fetch<READSIZE,DIRECTION>((addr)&(~((READSIZE>>3)-1)));
}

// calculates the cycle time of a single code fetch in the FETCH stage
// to be used to calculate the fetchCycles argument for MMU_fetchExecuteCycles.
// this may have side effects, so don't call it more than necessary.
template<int PROCNUM, int READSIZE>
FORCEINLINE u32 MMU_codeFetchCycles(u32 addr)
{
	return MMU_timing.armCodeFetch<PROCNUM>().template Fetch<READSIZE,MMU_AD_READ>((addr)&(~((READSIZE>>3)-1)));
}

// calculates the cycle contribution of ALU + MEM stages (= EXECUTE)
// given ALU cycle time and the summation of multiple memory access cycle times.
// this function might belong more in armcpu, but I don't think it matters.
template<int PROCNUM>
FORCEINLINE u32 MMU_aluMemCycles(u32 aluCycles, u32 memCycles)
{
	if(PROCNUM==ARMCPU_ARM9)
	{
		// ALU and MEM are different stages of the 5-stage pipeline.
		// we approximate the pipeline throughput using max,
		// since simply adding the cycles of each instruction together
		// fails to take into account the parallelism of the arm pipeline
		// and would make the emulated system unnaturally slow.
		return std::max(aluCycles, memCycles);
	}
	else
	{
		// ALU and MEM are part of the same stage of the 3-stage pipeline,
		// thus they occur in sequence and we can simply add the counts together.
		return aluCycles + memCycles;
	}
}

// calculates the cycle contribution of ALU + MEM stages (= EXECUTE)
// given ALU cycle time and the description of a single memory access.
// this may have side effects, so don't call it more than necessary.
template<int PROCNUM, int READSIZE, MMU_ACCESS_DIRECTION DIRECTION>
FORCEINLINE u32 MMU_aluMemAccessCycles(u32 aluCycles, u32 addr)
{
	u32 memCycles = MMU_memAccessCycles<PROCNUM,READSIZE,DIRECTION>(addr);
	return MMU_aluMemCycles<PROCNUM>(aluCycles, memCycles);
}

// calculates the cycle contribution of FETCH + EXECUTE stages
// given executeCycles = the combined ALU+MEM cycles
//     and fetchCycles = the cycle time of the FETCH stage
// this function might belong more in armcpu, but I don't think it matters.
template<int PROCNUM>
FORCEINLINE u32 MMU_fetchExecuteCycles(u32 executeCycles, u32 fetchCycles)
{
#ifdef ACCOUNT_FOR_CODE_FETCH_CYCLES
	// execute and fetch are different stages of the pipeline for both arm7 and arm9.
	// again, we approximate the pipeline throughput using max.
	return std::max(executeCycles, fetchCycles);
	// TODO: add an option to support conflict between MEM and FETCH cycles
	//  if they're both using the same data bus.
	//  in the case of a conflict this should be:
	//  return std::max(aluCycles, memCycles + fetchCycles);
#else
	return executeCycles;
#endif
}


#endif
