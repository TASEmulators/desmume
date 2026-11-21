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
#include <stdint.h>
#include <stddef.h>

// The "uid" (a.k.a. script context id) is an opaque value chosen by the frontend.
// It must be wide enough to hold a native handle (e.g. an HWND on Windows/x64),
// so it is declared as intptr_t instead of int.
void OpenLuaContext(intptr_t uid, void(*print)(intptr_t uid, const char* str) = 0, void(*onstart)(intptr_t uid) = 0, void(*onstop)(intptr_t uid, bool statusOK) = 0);
void RunLuaScriptFile(intptr_t uid, const char* filename);
void StopLuaScript(intptr_t uid);
void RequestAbortLuaScript(intptr_t uid, const char* message = 0);
void CloseLuaContext(intptr_t uid);
bool AnyLuaActive();

// Runtime description of the Lua engine the scripts actually run on.
// This is queried from a fresh temporary lua_State rather than from the
// version macros in the headers this was compiled against: on Windows
// lua51.dll is delay-loaded, so either stock Lua 5.1 or LuaJIT may answer.
// Modelled on vba-rerecording's VBALuaEngineVersion().
struct LuaEngineInfo
{
	char version[128];   // "LuaJIT 2.1.1792588164 (Lua 5.1)" / "Lua 5.1 (no LuaJIT)"
	char platform[64];   // "x64 / Windows" (LuaJIT only, else empty)
	char jit[96];        // trace-compiler policy, or "n/a (not LuaJIT)"
	char headers[64];    // "Lua 5.1.4 (LuaJIT 2.1.1792588164)" - LUA_RELEASE, plus
	                     // LUAJIT_VERSION when luajit.h was included
	bool  isLuaJIT;
};

// Fills *out. Returns false if a temporary lua_State could not be created.
// The caller must make sure the Lua DLL is loadable first on delay-load builds.
bool GetLuaEngineInfo(LuaEngineInfo* out);

// Fills `out` with the shared library/DLL the Lua entry points were resolved
// from.  Empty when the engine is linked at build time or is not loaded.  The
// dynamic frontends use this for their "About Lua Scripting" dialog.
void GetLuaEngineLibraryPath(char* out, size_t outSize);

// Optional frontend hooks for Lua APIs that need real UI.  They are installed
// at startup and stay valid for the process lifetime.  When a hook is NULL the
// engine keeps the old platform-neutral fallback (a fixed popup answer, or a
// no-op emu.openscript), which is what the CLI and the retired GTK2 frontend
// use.
//
// Popup: show `message` with the requested button set and return the answer
// string exactly as the Windows MessageBox mapping produces it ("ok", "yes",
// "no", "cancel", "abort", "retry", "ignore").  The returned pointer must stay
// valid until the next call.
typedef const char* (*LuaPopupHandler)(const char* message, const char* type, const char* icon);
void SetLuaPopupHandler(LuaPopupHandler handler);

// OpenScript: start `filename` in a new script context.  Relative paths may be
// resolved against `extraDirToCheck` (the calling script's directory) first.
// Return NULL on success, or a human-readable error message.
typedef const char* (*LuaOpenScriptHandler)(const char* filename, const char* extraDirToCheck);
void SetLuaOpenScriptHandler(LuaOpenScriptHandler handler);

// ---------------------------------------------------------------------------
// input.get() / controller.get() frontend hooks.
//
// These mirror the macOS MacLuaInputStatus / MacLuaControllerStatus structures
// and the Windows GetKeyboardState() / joyGetPosEx() behaviour, but use plain C
// types so the shared engine needs no GTK, SDL or platform headers.  Frontends
// that install the handlers get real input; the others keep returning empty
// tables, exactly as before.
// ---------------------------------------------------------------------------
enum
{
	LUA_INPUT_MAX_KEYS = 64,
	LUA_INPUT_KEY_NAME_SIZE = 16,
	LUA_CONTROLLER_MAX_BUTTONS = 33,
	LUA_CONTROLLER_MAX_AXES = 6
};

typedef struct LuaInputStatus
{
	// Names using the Windows s_keyToName[] vocabulary ("A".."Z", "leftclick",
	// "space", ...).  Fixed storage keeps the pointers valid for the whole
	// engine call without the frontend having to manage lifetimes.
	char keyNames[LUA_INPUT_MAX_KEYS][LUA_INPUT_KEY_NAME_SIZE];
	int keyCount;

	// Pointer position in touch-screen-relative game pixels: the bottom
	// screen is 0..191 and the upper screen is -192..-1, matching Windows'
	// ToDSScreenRelativeCoords(..., 1).  mouseValid is false when the pointer
	// is not over either screen.
	bool mouseValid;
	int mouseX;
	int mouseY;
} LuaInputStatus;

// Fill *out with the current keyboard/mouse state.  Return false when the
// frontend cannot report it; input.get() then returns an empty table.
typedef bool (*LuaGetInputStatusHandler)(LuaInputStatus* out);

typedef struct LuaControllerStatus
{
	bool dpad[4]; // up, right, down, left
	int buttonCount;
	bool buttons[LUA_CONTROLLER_MAX_BUTTONS];
	int axisCount;
	double axes[LUA_CONTROLLER_MAX_AXES]; // x, y, z, r, u, v scaled to -1..1
} LuaControllerStatus;

// Read pad `joystickID`.  Return false when that pad is not present, which the
// engine reports as an empty table (same as Windows for a missing pad).
typedef bool (*LuaGetControllerStatusHandler)(unsigned int joystickID, bool returnDiagonals, LuaControllerStatus* out);

void SetLuaInputStatusHandler(LuaGetInputStatusHandler handler);
void SetLuaGetControllerStatusHandler(LuaGetControllerStatusHandler handler);

// ---------------------------------------------------------------------------
// emu.addmenu() / emu.setmenuiteminfo() / emu.registermenustart() hooks.
//
// A menu handle is opaque to the engine; the frontend decides what it points
// at.  Item ids are allocated by the frontend and must be freed by it when the
// owning script stops.  The GTK3 frontend installs these hooks; the CLI and the
// retired GTK2 frontend leave them unset, so the menu APIs stay no-ops there.
//
// Selecting an item is reported by the frontend with
// CallRegisteredLuaMenuHandlers() using the id it was created with.
// ---------------------------------------------------------------------------
typedef void* LuaMenuHandle;

typedef struct LuaMenuHooks
{
	int  (*allocItemID)(void);
	void (*freeItemID)(int itemID);
	bool (*isOwnedItemID)(int itemID);

	LuaMenuHandle (*rootMenu)(void);
	LuaMenuHandle (*findSubmenu)(LuaMenuHandle menu, const char* title, int* outItemID);
	LuaMenuHandle (*addSubmenu)(LuaMenuHandle parentMenu, const char* title, int itemID);
	bool (*addItem)(LuaMenuHandle menu, int itemID, const char* title);
	bool (*addSeparator)(LuaMenuHandle menu, int itemID);
	void (*removeItem)(LuaMenuHandle menu, int itemID);
	bool (*isMenuEmpty)(LuaMenuHandle menu);
	void (*setEnabled)(LuaMenuHandle menu, int itemID, bool enabled);
	void (*setChecked)(LuaMenuHandle menu, int itemID, bool checked);
	void (*setTitle)(LuaMenuHandle menu, int itemID, const char* title);

	// Ask the frontend to run the LUACALL_ONINITMENU handlers before the Lua
	// menus are next shown, the GMenu counterpart of WM_INITMENUPOPUP.
	void (*queueMenuInit)(void);
} LuaMenuHooks;

void SetLuaMenuHooks(const LuaMenuHooks* hooks);

// True while any script is executing Lua code.  Frontends use this to avoid
// re-entering a lua_State from a GTK timeout or menu callback.
bool AnyLuaRunning();

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
	LUACALL_REGISTER3DEVENT,

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
void CallRegistered3dEvent(int which, void* arg);

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

	void SaveRecord(intptr_t uid, unsigned int key); // saves Lua stack into a record and pops it
	void LoadRecord(intptr_t uid, unsigned int key, unsigned int itemsToLoad) const; // pushes a record's data onto the Lua stack
	void SaveRecordPartial(intptr_t uid, unsigned int key, int idx); // saves part of the Lua stack (at the given index) into a record and does NOT pop anything

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
		return (int)broad.islands.size();
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

