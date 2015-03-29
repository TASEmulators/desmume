/*
	Copyright (C) 2009-2015 DeSmuME team

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

#ifndef LUA_SCRIPT_H
#define LUA_SCRIPT_H

#include "types.h"

void OpenLuaContext(int uid, void(*print)(int uid, const char* str) = 0, void(*onstart)(int uid) = 0, void(*onstop)(int uid, bool statusOK) = 0);
void RunLuaScriptFile(int uid, const char* filename);
void StopLuaScript(int uid);
void RequestAbortLuaScript(int uid, const char* message = 0);
void CloseLuaContext(int uid);
bool AnyLuaActive();

enum LuaCallID
{
	LUACALL_BEFOREEMULATION,
	LUACALL_AFTEREMULATION,
	LUACALL_AFTEREMULATIONGUI,
	LUACALL_BEFOREEXIT,
	LUACALL_BEFORESAVE,
	LUACALL_AFTERLOAD,
	LUACALL_ONSTART,
	LUACALL_ONINITMENU,

	LUACALL_SCRIPT_HOTKEY_1,
	LUACALL_SCRIPT_HOTKEY_2,
	LUACALL_SCRIPT_HOTKEY_3,
	LUACALL_SCRIPT_HOTKEY_4,
	LUACALL_SCRIPT_HOTKEY_5,
	LUACALL_SCRIPT_HOTKEY_6,
	LUACALL_SCRIPT_HOTKEY_7,
	LUACALL_SCRIPT_HOTKEY_8,
	LUACALL_SCRIPT_HOTKEY_9,
	LUACALL_SCRIPT_HOTKEY_10,
	LUACALL_SCRIPT_HOTKEY_11,
	LUACALL_SCRIPT_HOTKEY_12,
	LUACALL_SCRIPT_HOTKEY_13,
	LUACALL_SCRIPT_HOTKEY_14,
	LUACALL_SCRIPT_HOTKEY_15,
	LUACALL_SCRIPT_HOTKEY_16,

	LUACALL_COUNT
};
void CallRegisteredLuaFunctions(LuaCallID calltype);

enum LuaMemHookType
{
	LUAMEMHOOK_WRITE,
	LUAMEMHOOK_READ,
	LUAMEMHOOK_EXEC,
	LUAMEMHOOK_WRITE_SUB,
	LUAMEMHOOK_READ_SUB,
	LUAMEMHOOK_EXEC_SUB,

	LUAMEMHOOK_COUNT
};
void CallRegisteredLuaMemHook(unsigned int address, int size, unsigned int value, LuaMemHookType hookType);

struct LuaSaveData
{
	LuaSaveData() { recordList = 0; }
	~LuaSaveData() { ClearRecords(); }

	struct Record
	{
		unsigned int key; // crc32
		unsigned int size; // size of data
		unsigned char* data;
		Record* next;
	};

	Record* recordList;

	void SaveRecord(int uid, unsigned int key); // saves Lua stack into a record and pops it
	void LoadRecord(int uid, unsigned int key, unsigned int itemsToLoad) const; // pushes a record's data onto the Lua stack
	void SaveRecordPartial(int uid, unsigned int key, int idx); // saves part of the Lua stack (at the given index) into a record and does NOT pop anything

	void ExportRecords(void* file) const; // writes all records to an already-open file
	void ImportRecords(void* file); // reads records from an already-open file
	void ClearRecords(); // deletes all record data

private:
	// disallowed, it's dangerous to call this
	// (because the memory the destructor deletes isn't refcounted and shouldn't need to be copied)
	// so pass LuaSaveDatas by reference and this should never get called
	LuaSaveData(const LuaSaveData& copy) {}
};
void CallRegisteredLuaSaveFunctions(int savestateNumber, LuaSaveData& saveData);
void CallRegisteredLuaLoadFunctions(int savestateNumber, const LuaSaveData& saveData);

typedef size_t PlatformMenuItem;
void CallRegisteredLuaMenuHandlers(PlatformMenuItem menuItem);

void StopAllLuaScripts();
void RestartAllLuaScripts();
void EnableStopAllLuaScripts(bool enable);
void DontWorryLua();



#include <vector>
#include <algorithm>

// the purpose of this structure is to provide a way of
// QUICKLY determining whether a memory address range has a hook associated with it,
// with a bias toward fast rejection because the majority of addresses will not be hooked.
// (it must not use any part of Lua or perform any per-script operations,
//  otherwise it would definitely be too slow.)
// calculating the regions when a hook is added/removed may be slow,
// but this is an intentional tradeoff to obtain a high speed of checking during later execution
struct TieredRegion
{
	template<unsigned int maxGap>
	struct Region
	{
		struct Island
		{
			unsigned int start;
			unsigned int end;
			FORCEINLINE bool Contains(unsigned int address, int size) const { return address < end && address+size > start; }
		};
		std::vector<Island> islands;

		void Calculate(const std::vector<unsigned int>& bytes)
		{
			islands.clear();

			unsigned int lastEnd = ~0;

			std::vector<unsigned int>::const_iterator iter = bytes.begin();
			std::vector<unsigned int>::const_iterator end = bytes.end();
			for(; iter != end; ++iter)
			{
				unsigned int addr = *iter;
				if(addr < lastEnd || addr > lastEnd + (long long)maxGap)
				{
					islands.push_back(Island());
					islands.back().start = addr;
				}
				islands.back().end = addr+1;
				lastEnd = addr+1;
			}
		}

		bool Contains(unsigned int address, int size) const
		{
			typename std::vector<Island>::const_iterator iter = islands.begin();
			typename std::vector<Island>::const_iterator end = islands.end();
			for(; iter != end; ++iter)
				if(iter->Contains(address, size))
					return true;
			return false;
		}
	};

	Region<0xFFFFFFFF> broad;
	Region<0x1000> mid;
	Region<0> narrow;

	void Calculate(std::vector<unsigned int>& bytes)
	{
		std::sort(bytes.begin(), bytes.end());

		broad.Calculate(bytes);
		mid.Calculate(bytes);
		narrow.Calculate(bytes);
	}

	TieredRegion()
	{
		std::vector<unsigned int> somevector;
		Calculate(somevector);
	}

	FORCEINLINE int NotEmpty()
	{
		return broad.islands.size();
	}

	// note: it is illegal to call this if NotEmpty() returns 0
	FORCEINLINE bool Contains(unsigned int address, int size)
	{
		return broad.islands[0].Contains(address,size) &&
		       mid.Contains(address,size) &&
			   narrow.Contains(address,size);
	}
};
extern TieredRegion hookedRegions [LUAMEMHOOK_COUNT];

void CallRegisteredLuaMemHook_LuaMatch(unsigned int address, int size, unsigned int value, LuaMemHookType hookType);

FORCEINLINE void CallRegisteredLuaMemHook(unsigned int address, int size, unsigned int value, LuaMemHookType hookType)
{
	// performance critical! (called VERY frequently)
	// I suggest timing a large number of calls to this function in Release if you change anything in here,
	// before and after, because even the most innocent change can make it become 30% to 400% slower.
	// a good amount to test is: 100000000 calls with no hook set, and another 100000000 with a hook set.
	// (on my system that consistently took 200 ms total in the former case and 350 ms total in the latter case)
	if(hookedRegions[hookType].NotEmpty())
	{
		//if((hookType <= LUAMEMHOOK_EXEC) && (address >= 0xE00000))
		//	address |= 0xFF0000; // gens: account for mirroring of RAM
		if(hookedRegions[hookType].Contains(address, size))
			CallRegisteredLuaMemHook_LuaMatch(address, size, value, hookType); // something has hooked this specific address
	}
}


#endif

