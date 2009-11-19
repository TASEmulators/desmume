#include "lua-engine.h"
#include "movie.h"
#include <assert.h>
#include <vector>
#include <map>
#include <string>
#include <algorithm>
#include "zlib.h"
#include "NDSSystem.h"
#include "movie.h"
#include "GPU_osd.h"
#include "saves.h"
#include "emufile.h"
#ifdef WIN32
#include "main.h"
#include "windows.h"
#include "video.h"
#endif


// functions that maybe aren't part of the Lua engine
// but didn't make sense to add to BaseDriver (at least not yet)
static bool IsHardwareAddressValid(u32 address) {
	// maybe TODO? let's say everything is valid.
	return true;
}


// actual lua engine follows
// adapted from gens-rr, nitsuja + upthorn

extern "C" {
	#include "lua.h"
	#include "lauxlib.h"
	#include "lualib.h"
	#include "lstate.h"
};

enum SpeedMode
{
	SPEEDMODE_NORMAL,
	SPEEDMODE_NOTHROTTLE,
	SPEEDMODE_TURBO,
	SPEEDMODE_MAXIMUM,
};

struct LuaGUIData
{
	u32* data;
	int stridePix;
	int xOrigin, yOrigin;
	int xMin, yMin, xMax, yMax;
};

struct LuaContextInfo {
	lua_State* L; // the Lua state
	bool started; // script has been started and hasn't yet been terminated, although it may not be currently running
	bool running; // script is currently running code (either the main call to the script or the callbacks it registered)
	bool returned; // main call to the script has returned (but it may still be active if it registered callbacks)
	bool crashed; // true if script has errored out
	bool restart; // if true, tells the script-running code to restart the script when the script stops
	bool restartLater; // set to true when a still-running script is stopped so that RestartAllLuaScripts can know which scripts to restart
	unsigned int worryCount; // counts up as the script executes, gets reset when the application is able to process messages, triggers a warning prompt if it gets too high
	bool stopWorrying; // set to true if the user says to let the script run forever despite appearing to be frozen
	bool panic; // if set to true, tells the script to terminate as soon as it can do so safely (used because directly calling lua_close() or luaL_error() is unsafe in some contexts)
	bool ranExit; // used to prevent a registered exit callback from ever getting called more than once
	bool guiFuncsNeedDeferring; // true whenever GUI drawing would be cleared by the next emulation update before it would be visible, and thus needs to be deferred until after the next emulation update
	int numDeferredFuncs; // number of deferred function calls accumulated, used to impose an arbitrary limit to avoid running out of memory
	bool ranFrameAdvance; // false if emu.frameadvance() hasn't been called yet
	int transparencyModifier; // values less than 255 will scale down the opacity of whatever the GUI renders, values greater than 255 will increase the opacity of anything transparent the GUI renders
	SpeedMode speedMode; // determines how emu.frameadvance() acts
	char panicMessage [72]; // a message to print if the script terminates due to panic being set
	std::string lastFilename; // path to where the script last ran from so that restart can work (note: storing the script in memory instead would not be useful because we always want the most up-to-date script from file)
	std::string nextFilename; // path to where the script should run from next, mainly used in case the restart flag is true
	unsigned int dataSaveKey; // crc32 of the save data key, used to decide which script should get which data... by default (if no key is specified) it's calculated from the script filename
	unsigned int dataLoadKey; // same as dataSaveKey but set through registerload instead of registersave if the two differ
	bool dataSaveLoadKeySet; // false if the data save keys are unset or set to their default value
	bool rerecordCountingDisabled; // true if this script has disabled rerecord counting for the savestates it loads
	std::vector<std::string> persistVars; // names of the global variables to persist, kept here so their associated values can be output when the script exits
	LuaSaveData newDefaultData; // data about the default state of persisted global variables, which we save on script exit so we can detect when the default value has changed to make it easier to reset persisted variables
	unsigned int numMemHooks; // number of registered memory functions (1 per hooked byte)
	LuaGUIData guiData;
	// callbacks into the lua window... these don't need to exist per context the way I'm using them, but whatever
	void(*print)(int uid, const char* str);
	void(*onstart)(int uid);
	void(*onstop)(int uid, bool statusOK);
};
std::map<int, LuaContextInfo*> luaContextInfo;
std::map<lua_State*, int> luaStateToUIDMap;
int g_numScriptsStarted = 0;
bool g_anyScriptsHighSpeed = false;
bool g_stopAllScriptsEnabled = true;

#define USE_INFO_STACK
#ifdef USE_INFO_STACK
	std::vector<LuaContextInfo*> infoStack;
	#define GetCurrentInfo() (*infoStack.front()) // should be faster but relies on infoStack correctly being updated to always have the current info in the first element
#else
	std::map<lua_State*, LuaContextInfo*> luaStateToContextMap;
	#define GetCurrentInfo() (*luaStateToContextMap[L->l_G->mainthread]) // should always work but might be slower
#endif

//#define ASK_USER_ON_FREEZE // dialog on freeze is disabled now because it seems to be unnecessary, but this can be re-defined to enable it


static std::map<lua_CFunction, const char*> s_cFuncInfoMap;

// using this macro you can define a callable-from-Lua function
// while associating with it some information about its arguments.
// that information will show up if the user tries to print the function
// or otherwise convert it to a string.
// (for example, "writebyte=function(addr,value)" instead of "writebyte=function:0A403490")
// note that the user can always use addressof(func) if they want to retrieve the address.
#define DEFINE_LUA_FUNCTION(name, argstring) \
	static int name(lua_State* L); \
	static const char* name##_args = s_cFuncInfoMap[name] = argstring; \
	static int name(lua_State* L)

#ifdef _MSC_VER
	#define snprintf _snprintf
	#define vscprintf _vscprintf
#else
	#define stricmp strcasecmp
	#define strnicmp strncasecmp
	#define __forceinline __attribute__((always_inline))
#endif


static const char* luaCallIDStrings [] =
{
	"CALL_BEFOREEMULATION",
	"CALL_AFTEREMULATION",
	"CALL_AFTEREMULATIONGUI",
	"CALL_BEFOREEXIT",
	"CALL_BEFORESAVE",
	"CALL_AFTERLOAD",
	"CALL_ONSTART",

	"CALL_HOTKEY_1",
	"CALL_HOTKEY_2",
	"CALL_HOTKEY_3",
	"CALL_HOTKEY_4",
	"CALL_HOTKEY_5",
	"CALL_HOTKEY_6",
	"CALL_HOTKEY_7",
	"CALL_HOTKEY_8",
	"CALL_HOTKEY_9",
	"CALL_HOTKEY_10",
	"CALL_HOTKEY_11",
	"CALL_HOTKEY_12",
	"CALL_HOTKEY_13",
	"CALL_HOTKEY_14",
	"CALL_HOTKEY_15",
	"CALL_HOTKEY_16",
};
static const int _makeSureWeHaveTheRightNumberOfStrings [sizeof(luaCallIDStrings)/sizeof(*luaCallIDStrings) == LUACALL_COUNT ? 1 : 0];

static const char* luaMemHookTypeStrings [] =
{
	"MEMHOOK_WRITE",
	"MEMHOOK_READ",
	"MEMHOOK_EXEC",

	"MEMHOOK_WRITE_SUB",
	"MEMHOOK_READ_SUB",
	"MEMHOOK_EXEC_SUB",
};
static const int _makeSureWeHaveTheRightNumberOfStrings2 [sizeof(luaMemHookTypeStrings)/sizeof(*luaMemHookTypeStrings) == LUAMEMHOOK_COUNT ? 1 : 0];

void StopScriptIfFinished(int uid, bool justReturned = false);
void SetSaveKey(LuaContextInfo& info, const char* key);
void SetLoadKey(LuaContextInfo& info, const char* key);
void RefreshScriptStartedStatus();
void RefreshScriptSpeedStatus();

static char* rawToCString(lua_State* L, int idx=0);
static const char* toCString(lua_State* L, int idx=0);

static void CalculateMemHookRegions(LuaMemHookType hookType);

static int memory_registerHook(lua_State* L, LuaMemHookType hookType, int defaultSize)
{
	// get first argument: address
	unsigned int addr = luaL_checkinteger(L,1);
	//if((addr & ~0xFFFFFF) == ~0xFFFFFF)
	//	addr &= 0xFFFFFF;

	// get optional second argument: size
	int size = defaultSize;
	int funcIdx = 2;
	if(lua_isnumber(L,2))
	{
		size = luaL_checkinteger(L,2);
		if(size < 0)
		{
			size = -size;
			addr -= size;
		}
		funcIdx++;
	}

	// check last argument: callback function
	bool clearing = lua_isnil(L,funcIdx);
	if(!clearing)
		luaL_checktype(L, funcIdx, LUA_TFUNCTION);
	lua_settop(L,funcIdx);

	// get the address-to-callback table for this hook type of the current script
	lua_getfield(L, LUA_REGISTRYINDEX, luaMemHookTypeStrings[hookType]);

	// count how many callback functions we'll be displacing
	int numFuncsAfter = clearing ? 0 : size;
	int numFuncsBefore = 0;
	for(unsigned int i = addr; i != addr+size; i++)
	{
		lua_rawgeti(L, -1, i);
		if(lua_isfunction(L, -1))
			numFuncsBefore++;
		lua_pop(L,1);
	}

	// put the callback function in the address slots
	for(unsigned int i = addr; i != addr+size; i++)
	{
		lua_pushvalue(L, -2);
		lua_rawseti(L, -2, i);
	}

	// adjust the count of active hooks
	LuaContextInfo& info = GetCurrentInfo();
	info.numMemHooks += numFuncsAfter - numFuncsBefore;

	// re-cache regions of hooked memory across all scripts
	CalculateMemHookRegions(hookType);

	StopScriptIfFinished(luaStateToUIDMap[L->l_G->mainthread]);
	return 0;
}

LuaMemHookType MatchHookTypeToCPU(lua_State* L, LuaMemHookType hookType)
{
	int cpuID = 0;

	int cpunameIndex = 0;
	if(lua_type(L,2) == LUA_TSTRING)
		cpunameIndex = 2;
	else if(lua_type(L,3) == LUA_TSTRING)
		cpunameIndex = 3;

	if(cpunameIndex)
	{
		const char* cpuName = lua_tostring(L, cpunameIndex);
//		if(!stricmp(cpuName, "sub") || !stricmp(cpuName, "s68k"))
//			cpuID = 1;
		lua_remove(L, cpunameIndex);
	}

//	switch(cpuID)
//	{
//	case 0: // m68k:
//		return hookType;
//
//	case 1: // s68k:
//		switch(hookType)
//		{
//		case LUAMEMHOOK_WRITE: return LUAMEMHOOK_WRITE_SUB;
//		case LUAMEMHOOK_READ: return LUAMEMHOOK_READ_SUB;
//		case LUAMEMHOOK_EXEC: return LUAMEMHOOK_EXEC_SUB;
//		}
//	}
	return hookType;
}

DEFINE_LUA_FUNCTION(memory_registerwrite, "address,[size=1,][cpuname=\"main\",]func")
{
	return memory_registerHook(L, MatchHookTypeToCPU(L,LUAMEMHOOK_WRITE), 1);
}
DEFINE_LUA_FUNCTION(memory_registerread, "address,[size=1,][cpuname=\"main\",]func")
{
	return memory_registerHook(L, MatchHookTypeToCPU(L,LUAMEMHOOK_READ), 1);
}
DEFINE_LUA_FUNCTION(memory_registerexec, "address,[size=2,][cpuname=\"main\",]func")
{
	return memory_registerHook(L, MatchHookTypeToCPU(L,LUAMEMHOOK_EXEC), 2);
}

DEFINE_LUA_FUNCTION(emu_registerbefore, "func")
{
	if (!lua_isnil(L,1))
		luaL_checktype(L, 1, LUA_TFUNCTION);
	lua_settop(L,1);
	lua_getfield(L, LUA_REGISTRYINDEX, luaCallIDStrings[LUACALL_BEFOREEMULATION]);
	lua_insert(L,1);
	lua_setfield(L, LUA_REGISTRYINDEX, luaCallIDStrings[LUACALL_BEFOREEMULATION]);
	StopScriptIfFinished(luaStateToUIDMap[L->l_G->mainthread]);
	return 1;
}
DEFINE_LUA_FUNCTION(emu_registerafter, "func")
{
	if (!lua_isnil(L,1))
		luaL_checktype(L, 1, LUA_TFUNCTION);
	lua_settop(L,1);
	lua_getfield(L, LUA_REGISTRYINDEX, luaCallIDStrings[LUACALL_AFTEREMULATION]);
	lua_insert(L,1);
	lua_setfield(L, LUA_REGISTRYINDEX, luaCallIDStrings[LUACALL_AFTEREMULATION]);
	StopScriptIfFinished(luaStateToUIDMap[L->l_G->mainthread]);
	return 1;
}
DEFINE_LUA_FUNCTION(emu_registerexit, "func")
{
	if (!lua_isnil(L,1))
		luaL_checktype(L, 1, LUA_TFUNCTION);
	lua_settop(L,1);
	lua_getfield(L, LUA_REGISTRYINDEX, luaCallIDStrings[LUACALL_BEFOREEXIT]);
	lua_insert(L,1);
	lua_setfield(L, LUA_REGISTRYINDEX, luaCallIDStrings[LUACALL_BEFOREEXIT]);
	StopScriptIfFinished(luaStateToUIDMap[L->l_G->mainthread]);
	return 1;
}
DEFINE_LUA_FUNCTION(emu_registerstart, "func") // TODO: use call registered LUACALL_ONSTART functions on reset
{
	if (!lua_isnil(L,1))
		luaL_checktype(L, 1, LUA_TFUNCTION);
	lua_settop(L,1);
	lua_getfield(L, LUA_REGISTRYINDEX, luaCallIDStrings[LUACALL_ONSTART]);
	lua_insert(L,1);
	lua_pushvalue(L,-1); // copy the function so we can also call it
	lua_setfield(L, LUA_REGISTRYINDEX, luaCallIDStrings[LUACALL_ONSTART]);
	if (!lua_isnil(L,-1) && driver->EMU_HasEmulationStarted())
		lua_call(L,0,0); // call the function now since the game has already started and this start function hasn't been called yet
	StopScriptIfFinished(luaStateToUIDMap[L->l_G->mainthread]);
	return 1;
}
DEFINE_LUA_FUNCTION(gui_register, "func")
{
	if (!lua_isnil(L,1))
		luaL_checktype(L, 1, LUA_TFUNCTION);
	lua_settop(L,1);
	lua_getfield(L, LUA_REGISTRYINDEX, luaCallIDStrings[LUACALL_AFTEREMULATIONGUI]);
	lua_insert(L,1);
	lua_setfield(L, LUA_REGISTRYINDEX, luaCallIDStrings[LUACALL_AFTEREMULATIONGUI]);
	StopScriptIfFinished(luaStateToUIDMap[L->l_G->mainthread]);
	return 1;
}
DEFINE_LUA_FUNCTION(state_registersave, "func[,savekey]")
{
	if (!lua_isnil(L,1))
		luaL_checktype(L, 1, LUA_TFUNCTION);
	if (!lua_isnoneornil(L,2))
		SetSaveKey(GetCurrentInfo(), rawToCString(L,2));
	lua_settop(L,1);
	lua_getfield(L, LUA_REGISTRYINDEX, luaCallIDStrings[LUACALL_BEFORESAVE]);
	lua_insert(L,1);
	lua_setfield(L, LUA_REGISTRYINDEX, luaCallIDStrings[LUACALL_BEFORESAVE]);
	StopScriptIfFinished(luaStateToUIDMap[L->l_G->mainthread]);
	return 1;
}
DEFINE_LUA_FUNCTION(state_registerload, "func[,loadkey]")
{
	if (!lua_isnil(L,1))
		luaL_checktype(L, 1, LUA_TFUNCTION);
	if (!lua_isnoneornil(L,2))
		SetLoadKey(GetCurrentInfo(), rawToCString(L,2));
	lua_settop(L,1);
	lua_getfield(L, LUA_REGISTRYINDEX, luaCallIDStrings[LUACALL_AFTERLOAD]);
	lua_insert(L,1);
	lua_setfield(L, LUA_REGISTRYINDEX, luaCallIDStrings[LUACALL_AFTERLOAD]);
	StopScriptIfFinished(luaStateToUIDMap[L->l_G->mainthread]);
	return 1;
}

DEFINE_LUA_FUNCTION(input_registerhotkey, "keynum,func")
{
	int hotkeyNumber = luaL_checkinteger(L,1);
	if(hotkeyNumber < 1 || hotkeyNumber > 16)
	{
		luaL_error(L, "input.registerhotkey(n,func) requires 1 <= n <= 16, but got n = %d.", hotkeyNumber);
		return 0;
	}
	else
	{
		const char* key = luaCallIDStrings[LUACALL_SCRIPT_HOTKEY_1 + hotkeyNumber-1];
		lua_getfield(L, LUA_REGISTRYINDEX, key);
		lua_replace(L,1);
		if (!lua_isnil(L,2))
			luaL_checktype(L, 2, LUA_TFUNCTION);
		lua_settop(L,2);
		lua_setfield(L, LUA_REGISTRYINDEX, key);
		StopScriptIfFinished(luaStateToUIDMap[L->l_G->mainthread]);
		return 1;
	}
}

static int doPopup(lua_State* L, const char* deftype, const char* deficon)
{
	const char* str = toCString(L,1);
	const char* type = lua_type(L,2) == LUA_TSTRING ? lua_tostring(L,2) : deftype;
	const char* icon = lua_type(L,3) == LUA_TSTRING ? lua_tostring(L,3) : deficon;

	int itype = -1, iters = 0;
	while(itype == -1 && iters++ < 2)
	{
		if(!stricmp(type, "ok")) itype = 0;
		else if(!stricmp(type, "yesno")) itype = 1;
		else if(!stricmp(type, "yesnocancel")) itype = 2;
		else if(!stricmp(type, "okcancel")) itype = 3;
		else if(!stricmp(type, "abortretryignore")) itype = 4;
		else type = deftype;
	}
	assert(itype >= 0 && itype <= 4);
	if(!(itype >= 0 && itype <= 4)) itype = 0;

	int iicon = -1; iters = 0;
	while(iicon == -1 && iters++ < 2)
	{
		if(!stricmp(icon, "message") || !stricmp(icon, "notice")) iicon = 0;
		else if(!stricmp(icon, "question")) iicon = 1;
		else if(!stricmp(icon, "warning")) iicon = 2;
		else if(!stricmp(icon, "error")) iicon = 3;
		else icon = deficon;
	}
	assert(iicon >= 0 && iicon <= 3);
	if(!(iicon >= 0 && iicon <= 3)) iicon = 0;

	static const char * const titles [] = {"Notice", "Question", "Warning", "Error"};
	const char* answer = "ok";
#ifdef _WIN32
	static const int etypes [] = {MB_OK, MB_YESNO, MB_YESNOCANCEL, MB_OKCANCEL, MB_ABORTRETRYIGNORE};
	static const int eicons [] = {MB_ICONINFORMATION, MB_ICONQUESTION, MB_ICONWARNING, MB_ICONERROR};
//	DialogsOpen++;
	int uid = luaStateToUIDMap[L->l_G->mainthread];
	EnableWindow(MainWindow->getHWnd(), false);
//	if (Full_Screen)
//	{
//		while (ShowCursor(false) >= 0);
//		while (ShowCursor(true) < 0);
//	}
	int ianswer = MessageBox((HWND)uid, str, titles[iicon], etypes[itype] | eicons[iicon]);
	EnableWindow(MainWindow->getHWnd(), true);
//	DialogsOpen--;
	switch(ianswer)
	{
		case IDOK: answer = "ok"; break;
		case IDCANCEL: answer = "cancel"; break;
		case IDABORT: answer = "abort"; break;
		case IDRETRY: answer = "retry"; break;
		case IDIGNORE: answer = "ignore"; break;
		case IDYES: answer = "yes"; break;
		case IDNO: answer = "no"; break;
	}
#else
	// NYI (assume first answer for now)
	switch(itype)
	{
		case 0: case 3: answer = "ok"; break;
		case 1: case 2: answer = "yes"; break;
		case 4: answer = "abort"; break;
	}
#endif

	lua_pushstring(L, answer);
	return 1;
}

// string gui.popup(string message, string type = "ok", string icon = "message")
// string input.popup(string message, string type = "yesno", string icon = "question")
DEFINE_LUA_FUNCTION(gui_popup, "message[,type=\"ok\"[,icon=\"message\"]]")
{
	return doPopup(L, "ok", "message");
}
DEFINE_LUA_FUNCTION(input_popup, "message[,type=\"yesno\"[,icon=\"question\"]]")
{
	return doPopup(L, "yesno", "question");
}

static const char* FilenameFromPath(const char* path)
{
	const char* slash1 = strrchr(path, '\\');
	const char* slash2 = strrchr(path, '/');
	if(slash1) slash1++;
	if(slash2) slash2++;
	const char* rv = path;
	rv = std::max(rv, slash1);
	rv = std::max(rv, slash2);
	if(!rv) rv = "";
	return rv;
}


static void toCStringConverter(lua_State* L, int i, char*& ptr, int& remaining);

// compare the contents of two items on the Lua stack to determine if they differ
// only works for relatively simple, saveable items (numbers, strings, bools, nil, and possibly-nested tables of those, up to a certain max length)
// not the best implementation, but good enough for what it's currently used for
static bool luaValueContentsDiffer(lua_State* L, int idx1, int idx2)
{
	static const int maxLen = 8192;
	static char str1[maxLen];
	static char str2[maxLen];
	str1[0] = 0;
	str2[0] = 0;
	char* ptr1 = str1;
	char* ptr2 = str2;
	int remaining1 = maxLen;
	int remaining2 = maxLen;
	toCStringConverter(L, idx1, ptr1, remaining1);
	toCStringConverter(L, idx2, ptr2, remaining2);
	return (remaining1 != remaining2) || (strcmp(str1,str2) != 0);
}


// fills output with the path
// also returns a pointer to the first character in the filename (non-directory) part of the path
static char* ConstructScriptSaveDataPath(char* output, int bufferSize, LuaContextInfo& info)
{
//	Get_State_File_Name(output); TODO
	char* slash1 = strrchr(output, '\\');
	char* slash2 = strrchr(output, '/');
	if(slash1) slash1[1] = '\0';
	if(slash2) slash2[1] = '\0';
	char* rv = output + strlen(output);
	strncat(output, "u.", bufferSize-(strlen(output)+1));
	if(!info.dataSaveLoadKeySet)
		strncat(output, FilenameFromPath(info.lastFilename.c_str()), bufferSize-(strlen(output)+1));
	else
		snprintf(output+strlen(output), bufferSize-(strlen(output)+1), "%X", info.dataSaveKey);
	strncat(output, ".luasav", bufferSize-(strlen(output)+1));
	return rv;
}

// emu.persistglobalvariables({
//   variable1 = defaultvalue1,
//   variable2 = defaultvalue2,
//   etc
// })
// takes a table with variable names as the keys and default values as the values,
// and defines each of those variables names as a global variable,
// setting them equal to the values they had the last time the script exited,
// or (if that isn't available) setting them equal to the provided default values.
// as a special case, if you want the default value for a variable to be nil,
// then put the variable name alone in quotes as an entry in the table without saying "= nil".
// this special case is because tables in lua don't store nil valued entries.
// also, if you change the default value that will reset the variable to the new default.
DEFINE_LUA_FUNCTION(emu_persistglobalvariables, "variabletable")
{
	int uid = luaStateToUIDMap[L->l_G->mainthread];
	LuaContextInfo& info = GetCurrentInfo();

	// construct a path we can load the persistent variables from
	char path [1024] = {0};
	char* pathTypeChrPtr = ConstructScriptSaveDataPath(path, 1024, info);

	// load the previously-saved final variable values from file
	LuaSaveData exitData;
	{
		*pathTypeChrPtr = 'e';
		FILE* persistFile = fopen(path, "rb");
		if(persistFile)
		{
			exitData.ImportRecords(persistFile);
			fclose(persistFile);
		}
	}

	// load the previously-saved default variable values from file
	LuaSaveData defaultData;
	{
		*pathTypeChrPtr = 'd';
		FILE* defaultsFile = fopen(path, "rb");
		if(defaultsFile)
		{
			defaultData.ImportRecords(defaultsFile);
			fclose(defaultsFile);
		}
	}

	// loop through the passed-in variables,
	// exposing a global variable to the script for each one
	// while also keeping a record of their names
	// so we can save them (to the persistFile) later when the script exits
	int numTables = lua_gettop(L);
	for(int i = 1; i <= numTables; i++)
	{
		luaL_checktype(L, i, LUA_TTABLE);

		lua_pushnil(L); // before first key
		int keyIndex = lua_gettop(L);
		int valueIndex = keyIndex + 1;
		while(lua_next(L, i))
		{
			int keyType = lua_type(L, keyIndex);
			int valueType = lua_type(L, valueIndex);
			if(keyType == LUA_TSTRING && valueType <= LUA_TTABLE && valueType != LUA_TLIGHTUSERDATA)
			{
				// variablename = defaultvalue,

				// duplicate the key first because lua_next() needs to eat that
				lua_pushvalue(L, keyIndex);
				lua_insert(L, keyIndex);
			}
			else if(keyType == LUA_TNUMBER && valueType == LUA_TSTRING)
			{
				// "variablename",
				// or [index] = "variablename",

				// defaultvalue is assumed to be nil
				lua_pushnil(L);
			}
			else
			{
				luaL_error(L, "'%s' = '%s' entries are not allowed in the table passed to emu.persistglobalvariables()", lua_typename(L,keyType), lua_typename(L,valueType));
			}

			int varNameIndex = valueIndex;
			int defaultIndex = valueIndex+1;

			// keep track of the variable name for later
			const char* varName = lua_tostring(L, varNameIndex);
			info.persistVars.push_back(varName);
			unsigned int varNameCRC = crc32(0, (const unsigned char*)varName, strlen(varName));
			info.newDefaultData.SaveRecordPartial(uid, varNameCRC, defaultIndex);

			// load the previous default value for this variable if it exists.
			// if the new default is different than the old one,
			// assume the user wants to set the value to the new default value
			// instead of the previously-saved exit value.
			bool attemptPersist = true;
			defaultData.LoadRecord(uid, varNameCRC, 1);
			lua_pushnil(L);
			if(luaValueContentsDiffer(L, defaultIndex, defaultIndex+1))
				attemptPersist = false;
			lua_settop(L, defaultIndex);

			if(attemptPersist)
			{
				// load the previous saved value for this variable if it exists
				exitData.LoadRecord(uid, varNameCRC, 1);
				if(lua_gettop(L) > defaultIndex)
					lua_remove(L, defaultIndex); // replace value with loaded record
				lua_settop(L, defaultIndex);
			}

			// set the global variable
			lua_settable(L, LUA_GLOBALSINDEX);

			assert(lua_gettop(L) == keyIndex);
		}
	}

	return 0;
}

static const char* deferredGUIIDString = "lazygui";
static const char* deferredJoySetIDString = "lazyjoy";
#define MAX_DEFERRED_COUNT 16384

// store the most recent C function call from Lua (and all its arguments)
// for later evaluation
void DeferFunctionCall(lua_State* L, const char* idstring)
{
	LuaContextInfo& info = GetCurrentInfo();
	if(info.numDeferredFuncs < MAX_DEFERRED_COUNT)
		info.numDeferredFuncs++;
	else
		return; // too many deferred functions on the same frame, silently discard the rest

	// there might be a cleaner way of doing this using lua_pushcclosure and lua_getref

	int num = lua_gettop(L);

	// get the C function pointer
	//lua_CFunction cf = lua_tocfunction(L, -(num+1));
	lua_CFunction cf = (L->ci->func)->value.gc->cl.c.f;
	assert(cf);
	lua_pushcfunction(L,cf);

	// make a list of the function and its arguments (and also pop those arguments from the stack)
	lua_createtable(L, num+1, 0);
	lua_insert(L, 1);
	for(int n = num+1; n > 0; n--)
		lua_rawseti(L, 1, n);

	// put the list into a global array
	lua_getfield(L, LUA_REGISTRYINDEX, idstring);
	lua_insert(L, 1);
	int curSize = lua_objlen(L, 1);
	lua_rawseti(L, 1, curSize+1);

	// clean the stack
	lua_settop(L, 0);
}

static const char* refStashString = "refstash";

void CallDeferredFunctions(lua_State* L, const char* idstring)
{
	lua_getfield(L, LUA_REGISTRYINDEX, idstring);
	int numCalls = lua_objlen(L, -1);
	if(numCalls > 0)
	{
		// save and pop any extra things that were on the stack
		int top = lua_gettop(L);
		int stashRef = -1;
		if(top > 1)
		{
			lua_insert(L, 1);
			lua_getfield(L, LUA_REGISTRYINDEX, refStashString);
			lua_insert(L, 2);
			lua_createtable(L, top-1, 0);
			lua_insert(L, 3);
			for(int remaining = top; remaining-- > 1;)
				lua_rawseti(L, 3, remaining);
			assert(lua_gettop(L) == 3);
			stashRef = luaL_ref(L, 2);
			lua_pop(L, 1);
		}
		
		// loop through all the queued function calls
		for(int i = 1; i <= numCalls; i++)
		{
			lua_rawgeti(L, 1, i);  // get the function+arguments list
			int listSize = lua_objlen(L, 2);

			// push the arguments and the function
			for(int j = 1; j <= listSize; j++)
				lua_rawgeti(L, 2, j);

			// get and pop the function
			lua_CFunction cf = lua_tocfunction(L, -1);
			lua_pop(L, 1);

			// shift first argument to slot 1 and call the function
			lua_remove(L, 2);
			lua_remove(L, 1);
			cf(L);

			// prepare for next iteration
			lua_settop(L, 0);
			lua_getfield(L, LUA_REGISTRYINDEX, idstring);
		}

		// clear the list of deferred functions
		lua_newtable(L);
		lua_setfield(L, LUA_REGISTRYINDEX, idstring);
		LuaContextInfo& info = GetCurrentInfo();
		info.numDeferredFuncs -= numCalls;
		if(info.numDeferredFuncs < 0)
			info.numDeferredFuncs = 0;

		// restore the stack
		lua_settop(L, 0);
		if(top > 1)
		{
			lua_getfield(L, LUA_REGISTRYINDEX, refStashString);
			lua_rawgeti(L, 1, stashRef);
			for(int i = 1; i <= top-1; i++)
				lua_rawgeti(L, 2, i);
			luaL_unref(L, 1, stashRef);
			lua_remove(L, 2);
			lua_remove(L, 1);
		}
		assert(lua_gettop(L) == top - 1);
	}
	else
	{
		lua_pop(L, 1);
	}
}

bool DeferGUIFuncIfNeeded(lua_State* L)
{
	LuaContextInfo& info = GetCurrentInfo();
	if(info.speedMode == SPEEDMODE_MAXIMUM)
	{
		// if the mode is "maximum" then discard all GUI function calls
		// and pretend it was because we deferred them
		return true;
	}
	if(info.guiFuncsNeedDeferring)
	{
		// defer whatever function called this one until later
		DeferFunctionCall(L, deferredGUIIDString);
		return true;
	}

	// ok to run the function right now
	return false;
}

void worry(lua_State* L, int intensity)
{
	LuaContextInfo& info = GetCurrentInfo();
	info.worryCount += intensity;
}

static inline bool isalphaorunderscore(char c)
{
	return isalpha(c) || c == '_';
}

static std::vector<const void*> s_tableAddressStack; // prevents infinite recursion of a table within a table (when cycle is found, print something like table:parent)
static std::vector<const void*> s_metacallStack; // prevents infinite recursion if something's __tostring returns another table that contains that something (when cycle is found, print the inner result without using __tostring)

#define APPENDPRINT { int _n = snprintf(ptr, remaining,
#define END ); if(_n >= 0) { ptr += _n; remaining -= _n; } else { remaining = 0; } }
static void toCStringConverter(lua_State* L, int i, char*& ptr, int& remaining)
{
	if(remaining <= 0)
		return;

	const char* str = ptr; // for debugging

	// if there is a __tostring metamethod then call it
	int usedMeta = luaL_callmeta(L, i, "__tostring");
	if(usedMeta)
	{
		std::vector<const void*>::const_iterator foundCycleIter = std::find(s_metacallStack.begin(), s_metacallStack.end(), lua_topointer(L,i));
		if(foundCycleIter != s_metacallStack.end())
		{
			lua_pop(L, 1);
			usedMeta = false;
		}
		else
		{
			s_metacallStack.push_back(lua_topointer(L,i));
			i = lua_gettop(L);
		}
	}

	switch(lua_type(L, i))
	{
		case LUA_TNONE: break;
		case LUA_TNIL: APPENDPRINT "nil" END break;
		case LUA_TBOOLEAN: APPENDPRINT lua_toboolean(L,i) ? "true" : "false" END break;
		case LUA_TSTRING: APPENDPRINT "%s",lua_tostring(L,i) END break;
		case LUA_TNUMBER: APPENDPRINT "%.12Lg",lua_tonumber(L,i) END break;
		case LUA_TFUNCTION: 
			if((L->base + i-1)->value.gc->cl.c.isC)
			{
				lua_CFunction func = lua_tocfunction(L, i);
				std::map<lua_CFunction, const char*>::iterator iter = s_cFuncInfoMap.find(func);
				if(iter == s_cFuncInfoMap.end())
					goto defcase;
				APPENDPRINT "function(%s)", iter->second END 
			}
			else
			{
				APPENDPRINT "function(" END 
				Proto* p = (L->base + i-1)->value.gc->cl.l.p;
				int numParams = p->numparams + (p->is_vararg?1:0);
				for (int n=0; n<p->numparams; n++)
				{
					APPENDPRINT "%s", getstr(p->locvars[n].varname) END 
					if(n != numParams-1)
						APPENDPRINT "," END
				}
				if(p->is_vararg)
					APPENDPRINT "..." END
				APPENDPRINT ")" END
			}
			break;
defcase:default: APPENDPRINT "%s:%p",luaL_typename(L,i),lua_topointer(L,i) END break;
		case LUA_TTABLE:
		{
			// first make sure there's enough stack space
			if(!lua_checkstack(L, 4))
			{
				// note that even if lua_checkstack never returns false,
				// that doesn't mean we didn't need to call it,
				// because calling it retrieves stack space past LUA_MINSTACK
				goto defcase;
			}

			std::vector<const void*>::const_iterator foundCycleIter = std::find(s_tableAddressStack.begin(), s_tableAddressStack.end(), lua_topointer(L,i));
			if(foundCycleIter != s_tableAddressStack.end())
			{
				int parentNum = s_tableAddressStack.end() - foundCycleIter;
				if(parentNum > 1)
					APPENDPRINT "%s:parent^%d",luaL_typename(L,i),parentNum END
				else
					APPENDPRINT "%s:parent",luaL_typename(L,i) END
			}
			else
			{
				s_tableAddressStack.push_back(lua_topointer(L,i));
				struct Scope { ~Scope(){ s_tableAddressStack.pop_back(); } } scope;

				APPENDPRINT "{" END

				lua_pushnil(L); // first key
				int keyIndex = lua_gettop(L);
				int valueIndex = keyIndex + 1;
				bool first = true;
				bool skipKey = true; // true if we're still in the "array part" of the table
				lua_Number arrayIndex = (lua_Number)0;
				while(lua_next(L, i))
				{
					if(first)
						first = false;
					else
						APPENDPRINT ", " END
					if(skipKey)
					{
						arrayIndex += (lua_Number)1;
						bool keyIsNumber = (lua_type(L, keyIndex) == LUA_TNUMBER);
						skipKey = keyIsNumber && (lua_tonumber(L, keyIndex) == arrayIndex);
					}
					if(!skipKey)
					{
						bool keyIsString = (lua_type(L, keyIndex) == LUA_TSTRING);
						bool invalidLuaIdentifier = (!keyIsString || !isalphaorunderscore(*lua_tostring(L, keyIndex)));
						if(invalidLuaIdentifier)
							if(keyIsString)
								APPENDPRINT "['" END
							else
								APPENDPRINT "[" END

						toCStringConverter(L, keyIndex, ptr, remaining); // key

						if(invalidLuaIdentifier)
							if(keyIsString)
								APPENDPRINT "']=" END
							else
								APPENDPRINT "]=" END
						else
							APPENDPRINT "=" END
					}

					bool valueIsString = (lua_type(L, valueIndex) == LUA_TSTRING);
					if(valueIsString)
						APPENDPRINT "'" END

					toCStringConverter(L, valueIndex, ptr, remaining); // value

					if(valueIsString)
						APPENDPRINT "'" END

					lua_pop(L, 1);

					if(remaining <= 0)
					{
						lua_settop(L, keyIndex-1); // stack might not be clean yet if we're breaking early
						break;
					}
				}
				APPENDPRINT "}" END
			}
		}	break;
	}

	if(usedMeta)
	{
		s_metacallStack.pop_back();
		lua_pop(L, 1);
	}
}

static const int s_tempStrMaxLen = 64 * 1024;
static char s_tempStr [s_tempStrMaxLen];

static char* rawToCString(lua_State* L, int idx)
{
	int a = idx>0 ? idx : 1;
	int n = idx>0 ? idx : lua_gettop(L);

	char* ptr = s_tempStr;
	*ptr = 0;

	int remaining = s_tempStrMaxLen;
	for(int i = a; i <= n; i++)
	{
		toCStringConverter(L, i, ptr, remaining);
		if(i != n)
			APPENDPRINT " " END
	}

	if(remaining < 3)
	{
		while(remaining < 6)
			remaining++, ptr--;
		APPENDPRINT "..." END
	}
	APPENDPRINT "\r\n" END
	// the trailing newline is so print() can avoid having to do wasteful things to print its newline
	// (string copying would be wasteful and calling info.print() twice can be extremely slow)
	// at the cost of functions that don't want the newline needing to trim off the last two characters
	// (which is a very fast operation and thus acceptable in this case)

	return s_tempStr;
}
#undef APPENDPRINT
#undef END


// replacement for luaB_tostring() that is able to show the contents of tables (and formats numbers better, and show function prototypes)
// can be called directly from lua via tostring(), assuming tostring hasn't been reassigned
DEFINE_LUA_FUNCTION(tostring, "...")
{
	char* str = rawToCString(L);
	str[strlen(str)-2] = 0; // hack: trim off the \r\n (which is there to simplify the print function's task)
	lua_pushstring(L, str);
	return 1;
}

// like rawToCString, but will check if the global Lua function tostring()
// has been replaced with a custom function, and call that instead if so
static const char* toCString(lua_State* L, int idx)
{
	int a = idx>0 ? idx : 1;
	int n = idx>0 ? idx : lua_gettop(L);
	lua_getglobal(L, "tostring");
	lua_CFunction cf = lua_tocfunction(L,-1);
	if(cf == tostring) // optimization: if using our own C tostring function, we can bypass the call through Lua and all the string object allocation that would entail
	{
		lua_pop(L,1);
		return rawToCString(L, idx);
	}
	else // if the user overrided the tostring function, we have to actually call it and store the temporarily allocated string it returns
	{
		lua_pushstring(L, "");
		for (int i=a; i<=n; i++) {
			lua_pushvalue(L, -2);  // function to be called
			lua_pushvalue(L, i);   // value to print
			lua_call(L, 1, 1);
			if(lua_tostring(L, -1) == NULL)
				luaL_error(L, LUA_QL("tostring") " must return a string to " LUA_QL("print"));
			lua_pushstring(L, (i<n) ? " " : "\r\n");
			lua_concat(L, 3);
		}
		const char* str = lua_tostring(L, -1);
		strncpy(s_tempStr, str, s_tempStrMaxLen);
		s_tempStr[s_tempStrMaxLen-1] = 0;
		lua_pop(L, 2);
		return s_tempStr;
	}
}

// replacement for luaB_print() that goes to the appropriate textbox instead of stdout
DEFINE_LUA_FUNCTION(print, "...")
{
	const char* str = toCString(L);

	int uid = luaStateToUIDMap[L->l_G->mainthread];
	LuaContextInfo& info = GetCurrentInfo();

	if(info.print)
		info.print(uid, str);
	else
		puts(str);

	worry(L, 100);
	return 0;
}


DEFINE_LUA_FUNCTION(emu_message, "str")
{
	const char* str = toCString(L);
	driver->USR_InfoMessage(str);
	return 0;
}

// provides an easy way to copy a table from Lua
// (simple assignment only makes an alias, but sometimes an independent table is desired)
// currently this function only performs a shallow copy,
// but I think it should be changed to do a deep copy (possibly of configurable depth?)
// that maintains the internal table reference structure
DEFINE_LUA_FUNCTION(copytable, "origtable")
{
	int origIndex = 1; // we only care about the first argument
	int origType = lua_type(L, origIndex);
	if(origType == LUA_TNIL)
	{
		lua_pushnil(L);
		return 1;
	}
	if(origType != LUA_TTABLE)
	{
		luaL_typerror(L, 1, lua_typename(L, LUA_TTABLE));
		lua_pushnil(L);
		return 1;
	}
	
	lua_createtable(L, lua_objlen(L,1), 0);
	int copyIndex = lua_gettop(L);

	lua_pushnil(L); // first key
	int keyIndex = lua_gettop(L);
	int valueIndex = keyIndex + 1;

	while(lua_next(L, origIndex))
	{
		lua_pushvalue(L, keyIndex);
		lua_pushvalue(L, valueIndex);
		lua_rawset(L, copyIndex); // copytable[key] = value
		lua_pop(L, 1);
	}

	// copy the reference to the metatable as well, if any
	if(lua_getmetatable(L, origIndex))
		lua_setmetatable(L, copyIndex);

	return 1; // return the new table
}

// because print traditionally shows the address of tables,
// and the print function I provide instead shows the contents of tables,
// I also provide this function
// (otherwise there would be no way to see a table's address, AFAICT)
DEFINE_LUA_FUNCTION(addressof, "table_or_function")
{
	const void* ptr = lua_topointer(L,-1);
	lua_pushinteger(L, (lua_Integer)ptr);
	return 1;
}

// the following bit operations are ported from LuaBitOp 1.0.1,
// because it can handle the sign bit (bit 31) correctly.

/*
** Lua BitOp -- a bit operations library for Lua 5.1.
** http://bitop.luajit.org/
**
** Copyright (C) 2008-2009 Mike Pall. All rights reserved.
**
** Permission is hereby granted, free of charge, to any person obtaining
** a copy of this software and associated documentation files (the
** "Software"), to deal in the Software without restriction, including
** without limitation the rights to use, copy, modify, merge, publish,
** distribute, sublicense, and/or sell copies of the Software, and to
** permit persons to whom the Software is furnished to do so, subject to
** the following conditions:
**
** The above copyright notice and this permission notice shall be
** included in all copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
** EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
** MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
** IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
** CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
** TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
** SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**
** [ MIT license: http://www.opensource.org/licenses/mit-license.php ]
*/

#ifdef _MSC_VER
/* MSVC is stuck in the last century and doesn't have C99's stdint.h. */
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int64 uint64_t;
#else
#include <stdint.h>
#endif

typedef int32_t SBits;
typedef uint32_t UBits;

typedef union {
  lua_Number n;
#ifdef LUA_NUMBER_DOUBLE
  uint64_t b;
#else
  UBits b;
#endif
} BitNum;

/* Convert argument to bit type. */
static UBits barg(lua_State *L, int idx)
{
  BitNum bn;
  UBits b;
  bn.n = lua_tonumber(L, idx);
#if defined(LUA_NUMBER_DOUBLE)
  bn.n += 6755399441055744.0;  /* 2^52+2^51 */
#ifdef SWAPPED_DOUBLE
  b = (UBits)(bn.b >> 32);
#else
  b = (UBits)bn.b;
#endif
#elif defined(LUA_NUMBER_INT) || defined(LUA_NUMBER_LONG) || \
      defined(LUA_NUMBER_LONGLONG) || defined(LUA_NUMBER_LONG_LONG) || \
      defined(LUA_NUMBER_LLONG)
  if (sizeof(UBits) == sizeof(lua_Number))
    b = bn.b;
  else
    b = (UBits)(SBits)bn.n;
#elif defined(LUA_NUMBER_FLOAT)
#error "A 'float' lua_Number type is incompatible with this library"
#else
#error "Unknown number type, check LUA_NUMBER_* in luaconf.h"
#endif
  if (b == 0 && !lua_isnumber(L, idx))
    luaL_typerror(L, idx, "number");
  return b;
}

/* Return bit type. */
#define BRET(b)  lua_pushnumber(L, (lua_Number)(SBits)(b)); return 1;

DEFINE_LUA_FUNCTION(bit_tobit, "x") { BRET(barg(L, 1)) }
DEFINE_LUA_FUNCTION(bit_bnot, "x") { BRET(~barg(L, 1)) }

#define BIT_OP(func, opr) \
  DEFINE_LUA_FUNCTION(func, "x1 [,x2...]") { int i; UBits b = barg(L, 1); \
    for (i = lua_gettop(L); i > 1; i--) b opr barg(L, i); BRET(b) }
BIT_OP(bit_band, &=)
BIT_OP(bit_bor, |=)
BIT_OP(bit_bxor, ^=)

#define bshl(b, n)  (b << n)
#define bshr(b, n)  (b >> n)
#define bsar(b, n)  ((SBits)b >> n)
#define brol(b, n)  ((b << n) | (b >> (32-n)))
#define bror(b, n)  ((b << (32-n)) | (b >> n))
#define BIT_SH(func, fn) \
  DEFINE_LUA_FUNCTION(func, "x, n") { \
    UBits b = barg(L, 1); UBits n = barg(L, 2) & 31; BRET(fn(b, n)) }
BIT_SH(bit_lshift, bshl)
BIT_SH(bit_rshift, bshr)
BIT_SH(bit_arshift, bsar)
BIT_SH(bit_rol, brol)
BIT_SH(bit_ror, bror)

DEFINE_LUA_FUNCTION(bit_bswap, "x")
{
  UBits b = barg(L, 1);
  b = (b >> 24) | ((b >> 8) & 0xff00) | ((b & 0xff00) << 8) | (b << 24);
  BRET(b)
}

DEFINE_LUA_FUNCTION(bit_tohex, "x [,n]")
{
  UBits b = barg(L, 1);
  SBits n = lua_isnone(L, 2) ? 8 : (SBits)barg(L, 2);
  const char *hexdigits = "0123456789abcdef";
  char buf[8];
  int i;
  if (n < 0) { n = -n; hexdigits = "0123456789ABCDEF"; }
  if (n > 8) n = 8;
  for (i = (int)n; --i >= 0; ) { buf[i] = hexdigits[b & 15]; b >>= 4; }
  lua_pushlstring(L, buf, (size_t)n);
  return 1;
}

static const struct luaL_Reg bit_funcs[] = {
  { "tobit",	bit_tobit },
  { "bnot",	bit_bnot },
  { "band",	bit_band },
  { "bor",	bit_bor },
  { "bxor",	bit_bxor },
  { "lshift",	bit_lshift },
  { "rshift",	bit_rshift },
  { "arshift",	bit_arshift },
  { "rol",	bit_rol },
  { "ror",	bit_ror },
  { "bswap",	bit_bswap },
  { "tohex",	bit_tohex },
  { NULL, NULL }
};

/* Signed right-shifts are implementation-defined per C89/C99.
** But the de facto standard are arithmetic right-shifts on two's
** complement CPUs. This behaviour is required here, so test for it.
*/
#define BAD_SAR		(bsar(-8, 2) != (SBits)-2)

bool luabitop_validate(lua_State *L) // originally named as luaopen_bit
{
  UBits b;
  lua_pushnumber(L, (lua_Number)1437217655L);
  b = barg(L, -1);
  if (b != (UBits)1437217655L || BAD_SAR) {  /* Perform a simple self-test. */
    const char *msg = "compiled with incompatible luaconf.h";
#ifdef LUA_NUMBER_DOUBLE
#ifdef _WIN32
    if (b == (UBits)1610612736L)
      msg = "use D3DCREATE_FPU_PRESERVE with DirectX";
#endif
    if (b == (UBits)1127743488L)
      msg = "not compiled with SWAPPED_DOUBLE";
#endif
    if (BAD_SAR)
      msg = "arithmetic right-shift broken";
    luaL_error(L, "bit library self-test failed (%s)", msg);
    return false;
  }
  return true;
}

// LuaBitOp ends here

DEFINE_LUA_FUNCTION(bitshift, "num,shift")
{
	int shift = luaL_checkinteger(L,2);
	if (shift < 0) {
		lua_pushinteger(L, -shift);
		lua_replace(L, 2);
		return bit_lshift(L);
	}
	else
		return bit_rshift(L);
}

DEFINE_LUA_FUNCTION(bitbit, "whichbit")
{
	int rv = 0;
	int numArgs = lua_gettop(L);
	for(int i = 1; i <= numArgs; i++) {
		int where = luaL_checkinteger(L,i);
		if (where >= 0 && where < 32)
			rv |= (1 << where);
	}
	lua_settop(L,0);
	BRET(rv);
}

int emu_wait(lua_State* L);
int dontworry(LuaContextInfo& info);

void indicateBusy(lua_State* L, bool busy)
{
	// disabled because there have been complaints about this message being useless spam.
	// the script window's title changing should be sufficient, I guess.
/*	if(busy)
	{
		const char* fmt = "script became busy (frozen?)";
		va_list argp;
		va_start(argp, fmt);
		luaL_where(L, 0);
		lua_pushvfstring(L, fmt, argp);
		va_end(argp);
		lua_concat(L, 2);
		LuaContextInfo& info = GetCurrentInfo();
		int uid = luaStateToUIDMap[L->l_G->mainthread];
		if(info.print)
		{
			info.print(uid, lua_tostring(L,-1));
			info.print(uid, "\r\n");
		}
		else
		{
			fprintf(stderr, "%s\n", lua_tostring(L,-1));
		}
		lua_pop(L, 1);
	}
*/
#ifdef _WIN32
	int uid = luaStateToUIDMap[L->l_G->mainthread];
	HWND hDlg = (HWND)uid;
	char str [1024];
	GetWindowText(hDlg, str, 1000);
	char* extra = strchr(str, '<');
	if(busy)
	{
		if(!extra)
			extra = str + strlen(str), *extra++ = ' ';
		strcpy(extra, "<BUSY>");
	}
	else
	{
		if(extra)
			extra[-1] = 0;
	}
	SetWindowText(hDlg, str);
#endif
}


#define HOOKCOUNT 4096
#define MAX_WORRY_COUNT 6000
void LuaRescueHook(lua_State* L, lua_Debug *dbg)
{
	LuaContextInfo& info = GetCurrentInfo();

	info.worryCount++;

	if(info.stopWorrying && !info.panic)
	{
		if(info.worryCount > (MAX_WORRY_COUNT >> 2))
		{
			// the user already said they're OK with the script being frozen,
			// but we don't trust their judgement completely,
			// so periodically update the main loop so they have a chance to manually stop it
			info.worryCount = 0;
			emu_wait(L);
			info.stopWorrying = true;
		}
		return;
	}

	if(info.worryCount > MAX_WORRY_COUNT || info.panic)
	{
		info.worryCount = 0;
		info.stopWorrying = false;

		bool stoprunning = true;
		bool stopworrying = true;
		if(!info.panic)
		{
			SPU_ClearOutputBuffer();
#if defined(ASK_USER_ON_FREEZE) && defined(_WIN32)
			DialogsOpen++;
			int answer = MessageBox(HWnd, "A Lua script has been running for quite a while. Maybe it is in an infinite loop.\n\nWould you like to stop the script?\n\n(Yes to stop it now,\n No to keep running and not ask again,\n Cancel to keep running but ask again later)", "Lua Alert", MB_YESNOCANCEL | MB_DEFBUTTON3 | MB_ICONASTERISK);
			DialogsOpen--;
			if(answer == IDNO)
				stoprunning = false;
			if(answer == IDCANCEL)
				stopworrying = false;
#else
			stoprunning = false;
#endif
		}

		if(!stoprunning && stopworrying)
		{
			info.stopWorrying = true; // don't remove the hook because we need it still running for RequestAbortLuaScript to work
			indicateBusy(info.L, true);
		}

		if(stoprunning)
		{
			//lua_sethook(L, NULL, 0, 0);
			assert(L->errfunc || L->errorJmp);
			luaL_error(L, info.panic ? info.panicMessage : "terminated by user");
		}

		info.panic = false;
	}
}

void printfToOutput(const char* fmt, ...)
{
	va_list list;
	va_start(list, fmt);
	int len = vscprintf(fmt, list);
	char* str = new char[len+1];
	vsprintf(str, fmt, list);
	va_end(list);
	LuaContextInfo& info = GetCurrentInfo();
	if(info.print)
	{
		lua_State* L = info.L;
		int uid = luaStateToUIDMap[L->l_G->mainthread];
		info.print(uid, str);
		info.print(uid, "\r\n");
		worry(L,300);
	}
	else
	{
		fprintf(stdout, "%s\n", str);
	}
	delete[] str;
}

bool FailVerifyAtFrameBoundary(lua_State* L, const char* funcName, int unstartedSeverity=2, int inframeSeverity=2)
{
	if (!driver->EMU_HasEmulationStarted())
	{
		static const char* msg = "cannot call %s() when emulation has not started.";
		switch(unstartedSeverity)
		{
		case 0: break;
		case 1: printfToOutput(msg, funcName); break;
		default: case 2: luaL_error(L, msg, funcName); break;
		}
		return true;
	}
	if(!driver->EMU_IsAtFrameBoundary())
	{
		static const char* msg = "cannot call %s() inside an emulation frame.";
		switch(inframeSeverity)
		{
		case 0: break;
		case 1: printfToOutput(msg, funcName); break;
		default: case 2: luaL_error(L, msg, funcName); break;
		}
		return true;
	}
	return false;
}


// wrapper for EMU_StepMainLoop that provides a default implementation if ESTEP_NOT_IMPLEMENTED is returned.
// which only works if called from a function whose return value will get returned to Lua directly.
// TODO: actually implement the default case by making the main thread we use into a coroutine and resuming it periodically
static bool stepped_emulation = false; // <-- this is the result of running the macro
#define StepEmulationOnce(allowSleep, allowPause, frameSkip, disableUser, disableCore) \
	switch(driver->EMU_StepMainLoop(allowSleep, allowPause, frameSkip, disableUser, disableCore)) \
	{	default: \
		case BaseDriver::ESTEP_NOT_IMPLEMENTED: /*return lua_yield(L, 0);*/ luaL_error(L, "Lua frame advance functions are not yet implemented for this platform, and neither is the fallback implementation."); break;/*TODO*/ \
		case BaseDriver::ESTEP_CALL_AGAIN: stepped_emulation = !driver->EMU_HasEmulationStarted(); break; \
		case BaseDriver::ESTEP_DONE: stepped_emulation = true; break; \
	}

// same as StepEmulationOnce, except calls EMU_StepMainLoop multiple times if it returns ESTEP_CALL_AGAIN
#define StepEmulation(allowSleep, allowPause, frameSkip, disableUser, disableCore) \
	do { \
		StepEmulationOnce(allowSleep, allowPause, frameSkip, disableUser, disableCore); \
		if(stepped_emulation || (info).panic) break; \
	} while(true)


// note: caller must return the value this returns to Lua (at least if nonzero)
int StepEmulationAtSpeed(lua_State* L, SpeedMode speedMode, bool allowPause)
{
	int postponeTime;
	bool drawNextFrame;
	int worryIntensity;
	bool allowSleep;
	int frameSkip;
	bool disableUserFeedback;

	LuaContextInfo& info = GetCurrentInfo();
	switch(speedMode)
	{
		default:
		case SPEEDMODE_NORMAL:
			postponeTime = 0, drawNextFrame = true, worryIntensity = 300;
			allowSleep = true;
			frameSkip = -1;
			disableUserFeedback = false;
			break;
		case SPEEDMODE_NOTHROTTLE:
			postponeTime = 250, drawNextFrame = true, worryIntensity = 200;
			allowSleep = driver->EMU_IsEmulationPaused();
			frameSkip = driver->EMU_IsFastForwarding() ? -1 : 0;
			disableUserFeedback = false;
			break;
		case SPEEDMODE_TURBO:
			postponeTime = 500, drawNextFrame = true, worryIntensity = 150;
			allowSleep = driver->EMU_IsEmulationPaused();
			frameSkip = 16;
			disableUserFeedback = false;
			break;
		case SPEEDMODE_MAXIMUM:
			postponeTime = 1000, drawNextFrame = false, worryIntensity = 100;
			allowSleep = driver->EMU_IsEmulationPaused();
			frameSkip = 65535;
			disableUserFeedback = true;
			break;
	}

	driver->USR_SetDisplayPostpone(postponeTime, drawNextFrame);
	allowPause ? dontworry(info) : worry(L, worryIntensity);

	if(!allowPause && driver->EMU_IsEmulationPaused())
		driver->EMU_PauseEmulation(false);

	StepEmulation(allowSleep, allowPause, frameSkip, disableUserFeedback, false);
	return 0;
}

// acts similar to normal emulation update
DEFINE_LUA_FUNCTION(emu_emulateframe, "")
{
	if(FailVerifyAtFrameBoundary(L, "emu.emulateframe", 0,1))
		return 0;

	return StepEmulationAtSpeed(L, SPEEDMODE_NORMAL, false);
}

// acts as a fast-forward emulation update that still renders every frame
DEFINE_LUA_FUNCTION(emu_emulateframefastnoskipping, "")
{
	if(FailVerifyAtFrameBoundary(L, "emu.emulateframefastnoskipping", 0,1))
		return 0;

	return StepEmulationAtSpeed(L, SPEEDMODE_NOTHROTTLE, false);
}

// acts as a fast-forward emulation update
DEFINE_LUA_FUNCTION(emu_emulateframefast, "")
{
	if(FailVerifyAtFrameBoundary(L, "emu.emulateframefast", 0,1))
		return 0;

	return StepEmulationAtSpeed(L, SPEEDMODE_TURBO, false);
}

// acts as an extremely-fast-forward emulation update
// that also doesn't render any graphics or generate any sounds
DEFINE_LUA_FUNCTION(emu_emulateframeinvisible, "")
{
	if(FailVerifyAtFrameBoundary(L, "emu.emulateframeinvisible", 0,1))
		return 0;

	return StepEmulationAtSpeed(L, SPEEDMODE_MAXIMUM, false);
}

DEFINE_LUA_FUNCTION(emu_speedmode, "mode")
{
	SpeedMode newSpeedMode = SPEEDMODE_NORMAL;
	if(lua_isnumber(L,1))
		newSpeedMode = (SpeedMode)luaL_checkinteger(L,1);
	else
	{
		const char* str = luaL_checkstring(L,1);
		if(!stricmp(str, "normal"))
			newSpeedMode = SPEEDMODE_NORMAL;
		else if(!stricmp(str, "nothrottle"))
			newSpeedMode = SPEEDMODE_NOTHROTTLE;
		else if(!stricmp(str, "turbo"))
			newSpeedMode = SPEEDMODE_TURBO;
		else if(!stricmp(str, "maximum"))
			newSpeedMode = SPEEDMODE_MAXIMUM;
	}

	LuaContextInfo& info = GetCurrentInfo();
	info.speedMode = newSpeedMode;
	RefreshScriptSpeedStatus();
	return 0;
}


// tells the emulation to wait while the script is doing calculations
// can call this periodically instead of emu.frameadvance
// note that the user can use hotkeys at this time
// (e.g. a savestate could possibly get loaded before emu.wait() returns)
DEFINE_LUA_FUNCTION(emu_wait, "")
{
	LuaContextInfo& info = GetCurrentInfo();
	StepEmulationOnce(false, false, -1, true, true);
	dontworry(info);
	return 0;
}





DEFINE_LUA_FUNCTION(emu_frameadvance, "")
{
	if(FailVerifyAtFrameBoundary(L, "emu.frameadvance", 0,1))
		return emu_wait(L);

	int uid = luaStateToUIDMap[L->l_G->mainthread];
	LuaContextInfo& info = GetCurrentInfo();

	if(!info.ranFrameAdvance)
	{
		// otherwise we'll never see the first frame of GUI drawing
		if(info.speedMode != SPEEDMODE_MAXIMUM)
			driver->USR_RefreshScreen();
		info.ranFrameAdvance = true;
	}

	return StepEmulationAtSpeed(L, info.speedMode, true);
}

DEFINE_LUA_FUNCTION(emu_pause, "")
{
	driver->EMU_PauseEmulation(true);

	LuaContextInfo& info = GetCurrentInfo();
	StepEmulation(true, true, 0, true, true);

	// allow the user to not have to manually unpause
	// after restarting a script that used emu.pause()
	if(info.panic)
		driver->EMU_PauseEmulation(false);

	return 0;
}

DEFINE_LUA_FUNCTION(emu_unpause, "")
{
	LuaContextInfo& info = GetCurrentInfo();
	driver->EMU_PauseEmulation(false);
	return 0;
}

DEFINE_LUA_FUNCTION(emu_redraw, "")
{
	driver->USR_RefreshScreen();
	worry(L,250);
	return 0;
}



DEFINE_LUA_FUNCTION(memory_readbyte, "address")
{
	int address = luaL_checkinteger(L,1);
	unsigned char value = (unsigned char)(_MMU_read08<ARMCPU_ARM9>(address) & 0xFF);
	lua_settop(L,0);
	lua_pushinteger(L, value);
	return 1; // we return the number of return values
}
DEFINE_LUA_FUNCTION(memory_readbytesigned, "address")
{
	int address = luaL_checkinteger(L,1);
	signed char value = (signed char)(_MMU_read08<ARMCPU_ARM9>(address) & 0xFF);
	lua_settop(L,0);
	lua_pushinteger(L, value);
	return 1;
}
DEFINE_LUA_FUNCTION(memory_readword, "address")
{
	int address = luaL_checkinteger(L,1);
	unsigned short value = (unsigned short)(_MMU_read16<ARMCPU_ARM9>(address) & 0xFFFF);
	lua_settop(L,0);
	lua_pushinteger(L, value);
	return 1;
}
DEFINE_LUA_FUNCTION(memory_readwordsigned, "address")
{
	int address = luaL_checkinteger(L,1);
	signed short value = (signed short)(_MMU_read16<ARMCPU_ARM9>(address) & 0xFFFF);
	lua_settop(L,0);
	lua_pushinteger(L, value);
	return 1;
}
DEFINE_LUA_FUNCTION(memory_readdword, "address")
{
	int address = luaL_checkinteger(L,1);
	unsigned long value = (unsigned long)(_MMU_read32<ARMCPU_ARM9>(address));
	lua_settop(L,0);
	lua_pushinteger(L, value);
	return 1;
}
DEFINE_LUA_FUNCTION(memory_readdwordsigned, "address")
{
	int address = luaL_checkinteger(L,1);
	signed long value = (signed long)(_MMU_read32<ARMCPU_ARM9>(address));
	lua_settop(L,0);
	lua_pushinteger(L, value);
	return 1;
}

DEFINE_LUA_FUNCTION(memory_writebyte, "address,value")
{
	int address = luaL_checkinteger(L,1);
	unsigned char value = (unsigned char)(luaL_checkinteger(L,2) & 0xFF);
	_MMU_write08<ARMCPU_ARM9>(address, value);
	return 0;
}
DEFINE_LUA_FUNCTION(memory_writeword, "address,value")
{
	int address = luaL_checkinteger(L,1);
	unsigned short value = (unsigned short)(luaL_checkinteger(L,2) & 0xFFFF);
	_MMU_write16<ARMCPU_ARM9>(address, value);
	return 0;
}
DEFINE_LUA_FUNCTION(memory_writedword, "address,value")
{
	int address = luaL_checkinteger(L,1);
	unsigned long value = (unsigned long)(luaL_checkinteger(L,2));
	_MMU_write32<ARMCPU_ARM9>(address, value);
	return 0;
}

DEFINE_LUA_FUNCTION(memory_readbyterange, "address,length")
{
	int address = luaL_checkinteger(L,1);
	int length = luaL_checkinteger(L,2);

	if(length < 0)
	{
		address += length;
		length = -length;
	}

	// push the array
	lua_createtable(L, abs(length), 0);

	// put all the values into the (1-based) array
	for(int a = address, n = 1; n <= length; a++, n++)
	{
		if(IsHardwareAddressValid(a))
		{
			unsigned char value = (unsigned char)(_MMU_read08<ARMCPU_ARM9>(address) & 0xFF);
			lua_pushinteger(L, value);
			lua_rawseti(L, -2, n);
		}
		// else leave the value nil
	}

	return 1;
}

DEFINE_LUA_FUNCTION(memory_isvalid, "address")
{
	int address = luaL_checkinteger(L,1);
	lua_settop(L,0);
	lua_pushboolean(L, IsHardwareAddressValid(address));
	return 1;
}

struct registerPointerMap
{
	const char* registerName;
	unsigned int* pointer;
	int dataSize;
};

#define RPM_ENTRY(name,var) {name, (unsigned int*)&var, sizeof(var)},

registerPointerMap arm9PointerMap [] = {
	RPM_ENTRY("r0", NDS_ARM9.R[0])
	RPM_ENTRY("r1", NDS_ARM9.R[1])
	RPM_ENTRY("r2", NDS_ARM9.R[2])
	RPM_ENTRY("r3", NDS_ARM9.R[3])
	RPM_ENTRY("r4", NDS_ARM9.R[4])
	RPM_ENTRY("r5", NDS_ARM9.R[5])
	RPM_ENTRY("r6", NDS_ARM9.R[6])
	RPM_ENTRY("r7", NDS_ARM9.R[7])
	RPM_ENTRY("r8", NDS_ARM9.R[8])
	RPM_ENTRY("r9", NDS_ARM9.R[9])
	RPM_ENTRY("r10", NDS_ARM9.R[10])
	RPM_ENTRY("r11", NDS_ARM9.R[11])
	RPM_ENTRY("r12", NDS_ARM9.R[12])
	RPM_ENTRY("r13", NDS_ARM9.R[13])
	RPM_ENTRY("r14", NDS_ARM9.R[14])
	RPM_ENTRY("r15", NDS_ARM9.R[15])
	RPM_ENTRY("cpsr", NDS_ARM9.CPSR.val)
	RPM_ENTRY("spsr", NDS_ARM9.SPSR.val)
	{}
};
registerPointerMap arm7PointerMap [] = {
	RPM_ENTRY("r0", NDS_ARM7.R[0])
	RPM_ENTRY("r1", NDS_ARM7.R[1])
	RPM_ENTRY("r2", NDS_ARM7.R[2])
	RPM_ENTRY("r3", NDS_ARM7.R[3])
	RPM_ENTRY("r4", NDS_ARM7.R[4])
	RPM_ENTRY("r5", NDS_ARM7.R[5])
	RPM_ENTRY("r6", NDS_ARM7.R[6])
	RPM_ENTRY("r7", NDS_ARM7.R[7])
	RPM_ENTRY("r8", NDS_ARM7.R[8])
	RPM_ENTRY("r9", NDS_ARM7.R[9])
	RPM_ENTRY("r10", NDS_ARM7.R[10])
	RPM_ENTRY("r11", NDS_ARM7.R[11])
	RPM_ENTRY("r12", NDS_ARM7.R[12])
	RPM_ENTRY("r13", NDS_ARM7.R[13])
	RPM_ENTRY("r14", NDS_ARM7.R[14])
	RPM_ENTRY("r15", NDS_ARM7.R[15])
	RPM_ENTRY("cpsr", NDS_ARM7.CPSR.val)
	RPM_ENTRY("spsr", NDS_ARM7.SPSR.val)
	{}
};

struct cpuToRegisterMap
{
	const char* cpuName;
	registerPointerMap* rpmap;
}
cpuToRegisterMaps [] =
{
	{"arm9.", arm9PointerMap},
	{"main.", arm9PointerMap},
	{"arm7.", arm7PointerMap},
	{"sub.",  arm7PointerMap},
	{"", arm9PointerMap},
};


DEFINE_LUA_FUNCTION(memory_getregister, "cpu_dot_registername_string")
{
	const char* qualifiedRegisterName = luaL_checkstring(L,1);
	lua_settop(L,0);
	for(int cpu = 0; cpu < sizeof(cpuToRegisterMaps)/sizeof(*cpuToRegisterMaps); cpu++)
	{
		cpuToRegisterMap ctrm = cpuToRegisterMaps[cpu];
		int cpuNameLen = strlen(ctrm.cpuName);
		if(!strnicmp(qualifiedRegisterName, ctrm.cpuName, cpuNameLen))
		{
			qualifiedRegisterName += cpuNameLen;
			for(int reg = 0; ctrm.rpmap[reg].dataSize; reg++)
			{
				registerPointerMap rpm = ctrm.rpmap[reg];
				if(!stricmp(qualifiedRegisterName, rpm.registerName))
				{
					switch(rpm.dataSize)
					{ default:
					case 1: lua_pushinteger(L, *(unsigned char*)rpm.pointer); break;
					case 2: lua_pushinteger(L, *(unsigned short*)rpm.pointer); break;
					case 4: lua_pushinteger(L, *(unsigned long*)rpm.pointer); break;
					}
					return 1;
				}
			}
			lua_pushnil(L);
			return 1;
		}
	}
	lua_pushnil(L);
	return 1;
}
DEFINE_LUA_FUNCTION(memory_setregister, "cpu_dot_registername_string,value")
{
	const char* qualifiedRegisterName = luaL_checkstring(L,1);
	unsigned long value = (unsigned long)(luaL_checkinteger(L,2));
	lua_settop(L,0);
	for(int cpu = 0; cpu < sizeof(cpuToRegisterMaps)/sizeof(*cpuToRegisterMaps); cpu++)
	{
		cpuToRegisterMap ctrm = cpuToRegisterMaps[cpu];
		int cpuNameLen = strlen(ctrm.cpuName);
		if(!strnicmp(qualifiedRegisterName, ctrm.cpuName, cpuNameLen))
		{
			qualifiedRegisterName += cpuNameLen;
			for(int reg = 0; ctrm.rpmap[reg].dataSize; reg++)
			{
				registerPointerMap rpm = ctrm.rpmap[reg];
				if(!stricmp(qualifiedRegisterName, rpm.registerName))
				{
					switch(rpm.dataSize)
					{ default:
					case 1: *(unsigned char*)rpm.pointer = (unsigned char)(value & 0xFF); break;
					case 2: *(unsigned short*)rpm.pointer = (unsigned short)(value & 0xFFFF); break;
					case 4: *(unsigned long*)rpm.pointer = value; break;
					}
					return 0;
				}
			}
			return 0;
		}
	}
	return 0;
}

DEFINE_LUA_FUNCTION(state_create, "[location]")
{
	if(lua_isnumber(L,1))
	{
		// simply return the integer that got passed in
		// (that's as good a savestate object as any for a numbered savestate slot)
		lua_settop(L,1);
		return 1;
	}

	// allocate a pointer to an in-memory/anonymous savestate
	EMUFILE_MEMORY** ppEmuFile = (EMUFILE_MEMORY**)lua_newuserdata(L, sizeof(EMUFILE_MEMORY*));
	*ppEmuFile = new EMUFILE_MEMORY();
	luaL_getmetatable(L, "EMUFILE_MEMORY*");
	lua_setmetatable(L, -2);

	return 1;
}

// savestate.save(location [, option])
// saves the current emulation state to the given location
// you can pass in either a savestate file number (an integer),
// OR you can pass in a savestate object that was returned by savestate.create()
// if option is "quiet" then any warning messages will be suppressed
// if option is "scriptdataonly" then the state will not actually be saved, but any save callbacks will still get called and their results will be saved (see savestate.registerload()/savestate.registersave())
DEFINE_LUA_FUNCTION(state_save, "location[,option]")
{
	//const char* option = (lua_type(L,2) == LUA_TSTRING) ? lua_tostring(L,2) : NULL;
	//if(option)
	//{
	//	if(!stricmp(option, "quiet")) // I'm not sure if saving can generate warning messages, but we might as well support suppressing them should they turn out to exist
	//		g_disableStatestateWarnings = true;
	//	else if(!stricmp(option, "scriptdataonly")) // TODO
	//		g_onlyCallSavestateCallbacks = true;
	//}
	//struct Scope { ~Scope(){ g_disableStatestateWarnings = false; g_onlyCallSavestateCallbacks = false; } } scope; // needs to run even if the following code throws an exception... maybe I should have put this in a "finally" block instead, but this project seems to have something against using the "try" statement

	if(/*!g_onlyCallSavestateCallbacks &&*/ FailVerifyAtFrameBoundary(L, "savestate.save", 2,2))
		return 0;

	int type = lua_type(L,1);
	switch(type)
	{
		case LUA_TNUMBER: // numbered save file
		default:
		{
			int stateNumber = luaL_checkinteger(L,1);
			savestate_slot(stateNumber);
		}	return 0;
		case LUA_TUSERDATA: // in-memory save slot
		{
			EMUFILE_MEMORY** ppEmuFile = (EMUFILE_MEMORY**)luaL_checkudata(L, 1, "EMUFILE_MEMORY*");
			(*ppEmuFile)->fseek(0, SEEK_SET);

			if((*ppEmuFile)->fail())
				luaL_error(L, "failed to save, savestate object was dead.");

			savestate_save(*ppEmuFile, 0);

			if((*ppEmuFile)->fail())
				luaL_error(L, "failed to save savestate!");
			if((*ppEmuFile)->size() == 0)
				luaL_error(L, "failed to save, savestate became empty somehow.");
		}	return 0;
	}
}

// savestate.load(location [, option])
// loads the current emulation state from the given location
// you can pass in either a savestate file number (an integer),
// OR you can pass in a savestate object that was returned by savestate.create() and has already saved to with savestate.save()
// if option is "quiet" then any warning messages will be suppressed
// if option is "scriptdataonly" then the state will not actually be loaded, but load callbacks will still get called and supplied with the data saved by save callbacks (see savestate.registerload()/savestate.registersave())
DEFINE_LUA_FUNCTION(state_load, "location[,option]")
{
	//const char* option = (lua_type(L,2) == LUA_TSTRING) ? lua_tostring(L,2) : NULL;
	//if(option)
	//{
	//	if(!stricmp(option, "quiet"))
	//		g_disableStatestateWarnings = true;
	//	else if(!stricmp(option, "scriptdataonly")) // TODO
	//		g_onlyCallSavestateCallbacks = true;
	//}
	//struct Scope { ~Scope(){ g_disableStatestateWarnings = false; g_onlyCallSavestateCallbacks = false; } } scope; // needs to run even if the following code throws an exception... maybe I should have put this in a "finally" block instead, but this project seems to have something against using the "try" statement

	if(/*!g_onlyCallSavestateCallbacks &&*/ FailVerifyAtFrameBoundary(L, "savestate.load", 2,2))
		return 0;

//	g_disableStatestateWarnings = lua_toboolean(L,2) != 0;

	int type = lua_type(L,1);
	switch(type)
	{
		case LUA_TNUMBER: // numbered save file
		default:
		{
			//LuaContextInfo& info = GetCurrentInfo();
			//if(info.rerecordCountingDisabled)
			//	SkipNextRerecordIncrement = true;
			int stateNumber = luaL_checkinteger(L,1);
			loadstate_slot(stateNumber);
		}	return 0;
		case LUA_TUSERDATA: // in-memory save slot
		{
			EMUFILE_MEMORY** ppEmuFile = (EMUFILE_MEMORY**)luaL_checkudata(L, 1, "EMUFILE_MEMORY*");
			(*ppEmuFile)->fseek(0, SEEK_SET);

			if((*ppEmuFile)->fail())
				luaL_error(L, "failed to load, savestate object was dead.");
			if((*ppEmuFile)->size() == 0)
				luaL_error(L, "failed to load, savestate wasn't saved first.");

			savestate_load(*ppEmuFile);

			if((*ppEmuFile)->fail())
				luaL_error(L, "failed to load savestate!");
		}	return 0;
	}
}

// savestate.loadscriptdata(location)
// returns the user data associated with the given savestate
// without actually loading the rest of that savestate or calling any callbacks.
// you can pass in either a savestate file number (an integer),
// OR you can pass in a savestate object that was returned by savestate.create()
// but note that currently only non-anonymous savestates can have associated scriptdata
//
// also note that this returns the same values
// that would be passed into a registered load function.
// the main reason this exists also is so you can register a load function that
// chooses whether or not to load the scriptdata instead of always loading it,
// and also to provide a nicer interface for loading scriptdata
// without needing to trigger savestate loading first
DEFINE_LUA_FUNCTION(state_loadscriptdata, "location")
{
	int type = lua_type(L,1);
	switch(type)
	{
		case LUA_TNUMBER: // numbered save file
		default:
		{
			// TODO
			//int stateNumber = luaL_checkinteger(L,1);
			//Set_Current_State(stateNumber, false,false);
			//char Name [1024] = {0};
			//Get_State_File_Name(Name);
			//{
			//	LuaSaveData saveData;

			//	char luaSaveFilename [512];
			//	strncpy(luaSaveFilename, Name, 512);
			//	luaSaveFilename[512-(1+7/*strlen(".luasav")*/)] = '\0';
			//	strcat(luaSaveFilename, ".luasav");
			//	FILE* luaSaveFile = fopen(luaSaveFilename, "rb");
			//	if(luaSaveFile)
			//	{
			//		saveData.ImportRecords(luaSaveFile);
			//		fclose(luaSaveFile);

			//		int uid = luaStateToUIDMap[L->l_G->mainthread];
			//		LuaContextInfo& info = GetCurrentInfo();

			//		lua_settop(L, 0);
			//		saveData.LoadRecord(uid, info.dataLoadKey, (unsigned int)-1);
			//		return lua_gettop(L);
			//	}
			//}
		}	return 0;
		case LUA_TUSERDATA: // in-memory save slot
		{	// there can be no user data associated with those, at least not yet
		}	return 0;
	}
}

// savestate.savescriptdata(location)
// same as savestate.save(location, "scriptdataonly")
// only provided for consistency with savestate.loadscriptdata(location)
DEFINE_LUA_FUNCTION(state_savescriptdata, "location")
{
	lua_settop(L, 1);
	lua_pushstring(L, "scriptdataonly");
	return state_save(L);
}

#ifndef PUBLIC_RELEASE
#include "gfx3d.h"
class EMUFILE_MEMORY_VERIFIER : public EMUFILE_MEMORY { 
public:
	EMUFILE_MEMORY_VERIFIER(EMUFILE_MEMORY* underlying) : EMUFILE_MEMORY(underlying->get_vec()) { }

	std::vector<std::string> differences;

	virtual void fwrite(const void *ptr, size_t bytes)
	{
		if(!failbit)
		{
			u8* dst = buf()+pos;
			const u8* src = (const u8*)ptr;
			int differencesAddedThisCall = 0;
			for(int i = pos; i < (int)bytes+pos; i++)
			{
				if(*src != *dst)
				{
					if(differences.size() == 100)
						failbit = true;
					else
					{
						char temp [256];
						sprintf(temp, " " /*"mismatch at "*/ "byte %d(0x%X at 0x%X): %d(0x%X) != %d(0x%X)\n", i, i, dst, *src,*src, *dst,*dst);

						if(ptr == GPU_screen || ptr == gfx3d_convertedScreen) // ignore screen-only differences since frame skipping can cause them and it's probably ok
							break;

						differences.push_back(temp); // <-- probably the best place for a breakpoint

						if(++differencesAddedThisCall == 4)
							break;
					}
				}
				src++;
				dst++;
			}
		}

		pos += bytes;
	}
};

// like savestate.save() except instead of actually saving
// it compares against what's already in the savestate
// and throws an error if any differences are found.
// only useful for development (catching desyncs).
DEFINE_LUA_FUNCTION(state_verify, "location[,option]")
{
	int type = lua_type(L,1);
	switch(type)
	{
		case LUA_TNUMBER: // numbered save file
		default:
		{
			luaL_error(L, "savestate.verify only works for in-memory saves.");
		}	return 0;
		case LUA_TUSERDATA: // in-memory save slot
		{
			EMUFILE_MEMORY** ppEmuFile = (EMUFILE_MEMORY**)luaL_checkudata(L, 1, "EMUFILE_MEMORY*");

			if((*ppEmuFile)->fail())
				luaL_error(L, "failed to verify, savestate object was dead.");

			EMUFILE_MEMORY_VERIFIER verifier (*ppEmuFile);
			savestate_save(&verifier, 0);
			if(verifier.differences.size())
			{
				fputs("\n", stdout);
				for(unsigned int i = 0; i < verifier.differences.size(); i++)
					fputs(verifier.differences[i].c_str(), stdout);
				luaL_error(L, "failed to verify savestate! %s", verifier.differences[0].c_str());
			}
		}	return 0;
	}
}

#endif






//joypad lib

static const char *button_mappings[] = {
//   G   E   W   X   Y   A   B   S       T        U    D      L      R       F
"debug","R","L","X","Y","A","B","start","select","up","down","left","right","lid"
};

static int joy_getArgControllerNum(lua_State* L, int& index)
{
	// well, I think there's only one controller,
	// but this should probably stay here for cross-emulator consistency
	int type = lua_type(L,index);
	if(type == LUA_TSTRING || type == LUA_TNUMBER)
		index++;
	return 1;
}

// joypad.set(table buttons)
//
//  Sets the joypad state (takes effect at the next frame boundary)
//    true -> pressed
//    false -> unpressed
//    nil -> no change
DEFINE_LUA_FUNCTION(joy_set, "buttonTable")
{
	if(movieMode == MOVIEMODE_PLAY) // don't allow tampering with a playing movie's input
		return 0; // (although it might be useful sometimes...)

	if(!NDS_isProcessingUserInput())
	{
		// defer this function until when we are processing input
		DeferFunctionCall(L, deferredJoySetIDString);
		return 0;
	}

	int index = 1;
	(void)joy_getArgControllerNum(L, index);
	luaL_checktype(L, index, LUA_TTABLE);

	UserButtons& buttons = NDS_getProcessingUserInput().buttons;

	for(int i = 0; i < sizeof(button_mappings)/sizeof(*button_mappings); i++)
	{
		const char* name = button_mappings[i];
		lua_getfield(L, index, name);
		if (!lua_isnil(L,-1))
		{
			bool pressed = lua_toboolean(L,-1) != 0;
			buttons.array[i] = pressed;
		}
		lua_pop(L,1);
	}

	return 0;
}

// table joypad.read()
//
//  Reads the joypad state (what the game sees)
int joy_get_internal(lua_State* L, bool reportUp, bool reportDown)
{
	int index = 1;
	(void)joy_getArgControllerNum(L, index);

	lua_newtable(L);

	const UserButtons& buttons = NDS_getFinalUserInput().buttons;

	for(int i = 0; i < sizeof(button_mappings)/sizeof(*button_mappings); i++)
	{
		const char* name = button_mappings[i];
		bool pressed = buttons.array[i];
		if((pressed && reportDown) || (!pressed && reportUp))
		{
			lua_pushboolean(L, pressed);
			lua_setfield(L, -2, name);
		}
	}

	return 1;
}
// joypad.get()
// returns a table of every game button,
// true meaning currently-held and false meaning not-currently-held
// (as of last frame boundary)
// this WILL read input from a currently-playing movie
DEFINE_LUA_FUNCTION(joy_get, "")
{
	return joy_get_internal(L, true, true);
}
// joypad.getdown()
// returns a table of every game button that is currently held
DEFINE_LUA_FUNCTION(joy_getdown, "")
{
	return joy_get_internal(L, false, true);
}
// joypad.getup()
// returns a table of every game button that is not currently held
DEFINE_LUA_FUNCTION(joy_getup, "")
{
	return joy_get_internal(L, true, false);
}

// table joypad.peek()
//
//  Reads the joypad state (what the user is currently pressing/requesting)
int joy_peek_internal(lua_State* L, bool reportUp, bool reportDown)
{
	int index = 1;
	(void)joy_getArgControllerNum(L, index);

	lua_newtable(L);

	const UserButtons& buttons = NDS_getRawUserInput().buttons;

	for(int i = 0; i < sizeof(button_mappings)/sizeof(*button_mappings); i++)
	{
		const char* name = button_mappings[i];
		bool pressed = buttons.array[i];
		if((pressed && reportDown) || (!pressed && reportUp))
		{
			lua_pushboolean(L, pressed);
			lua_setfield(L, -2, name);
		}
	}

	return 1;
}

// joypad.peek()
// returns a table of every game button,
// true meaning currently-held and false meaning not-currently-held
// peek checks which joypad buttons are physically pressed,
// so it will NOT read input from a playing movie,
// it CAN read mid-frame input,
// and it will NOT pay attention to stuff like disabled L+R/U+D
DEFINE_LUA_FUNCTION(joy_peek, "")
{
	return joy_peek_internal(L, true, true);
}
// joypad.peekdown()
// returns a table of every game button that is currently held (according to what joypad.peek() would return)
DEFINE_LUA_FUNCTION(joy_peekdown, "")
{
	return joy_peek_internal(L, false, true);
}
// joypad.peekup()
// returns a table of every game button that is not currently held (according to what joypad.peek() would return)
DEFINE_LUA_FUNCTION(joy_peekup, "")
{
	return joy_peek_internal(L, true, false);
}


static const struct ColorMapping
{
	const char* name;
	int value;
}
s_colorMapping [] =
{
	{"white",     0xFFFFFFFF},
	{"black",     0x000000FF},
	{"clear",     0x00000000},
	{"gray",      0x7F7F7FFF},
	{"grey",      0x7F7F7FFF},
	{"red",       0xFF0000FF},
	{"orange",    0xFF7F00FF},
	{"yellow",    0xFFFF00FF},
	{"chartreuse",0x7FFF00FF},
	{"green",     0x00FF00FF},
	{"teal",      0x00FF7FFF},
	{"cyan" ,     0x00FFFFFF},
	{"blue",      0x0000FFFF},
	{"purple",    0x7F00FFFF},
	{"magenta",   0xFF00FFFF},
};

inline int getcolor_unmodified(lua_State *L, int idx, int defaultColor)
{
	int type = lua_type(L,idx);
	switch(type)
	{
		case LUA_TNUMBER:
		{
			return lua_tointeger(L,idx);
		}	break;
		case LUA_TSTRING:
		{
			const char* str = lua_tostring(L,idx);
			if(*str == '#')
			{
				int color;
				sscanf(str+1, "%X", &color);
				int len = strlen(str+1);
				int missing = std::max(0, 8-len);
				color <<= missing << 2;
				if(missing >= 2) color |= 0xFF;
				return color;
			}
			else for(int i = 0; i<sizeof(s_colorMapping)/sizeof(*s_colorMapping); i++)
			{
				if(!stricmp(str,s_colorMapping[i].name))
					return s_colorMapping[i].value;
			}
			if(!strnicmp(str, "rand", 4))
				return ((rand()*255/RAND_MAX) << 8) | ((rand()*255/RAND_MAX) << 16) | ((rand()*255/RAND_MAX) << 24) | 0xFF;
		}	break;
		case LUA_TTABLE:
		{
			int color = 0xFF;
			lua_pushnil(L); // first key
			int keyIndex = lua_gettop(L);
			int valueIndex = keyIndex + 1;
			bool first = true;
			while(lua_next(L, idx))
			{
				bool keyIsString = (lua_type(L, keyIndex) == LUA_TSTRING);
				bool keyIsNumber = (lua_type(L, keyIndex) == LUA_TNUMBER);
				int key = keyIsString ? tolower(*lua_tostring(L, keyIndex)) : (keyIsNumber ? lua_tointeger(L, keyIndex) : 0);
				int value = lua_tointeger(L, valueIndex);
				if(value < 0) value = 0;
				if(value > 255) value = 255;
				switch(key)
				{
				case 1: case 'r': color |= value << 24; break;
				case 2: case 'g': color |= value << 16; break;
				case 3: case 'b': color |= value << 8; break;
				case 4: case 'a': color = (color & ~0xFF) | value; break;
				}
				lua_pop(L, 1);
			}
			return color;
		}	break;
		case LUA_TFUNCTION:
			return 0;
	}
	return defaultColor;
}
int getcolor(lua_State *L, int idx, int defaultColor)
{
	int color = getcolor_unmodified(L, idx, defaultColor);
	LuaContextInfo& info = GetCurrentInfo();
	if(info.transparencyModifier != 255)
	{
		int alpha = (((color & 0xFF) * info.transparencyModifier) / 255);
		if(alpha > 255) alpha = 255;
		color = (color & ~0xFF) | alpha;
	}
	return color;
}

// r,g,b,a = gui.parsecolor(color)
// examples:
// local r,g,b = gui.parsecolor("green")
// local r,g,b,a = gui.parsecolor(0x7F3FFF7F)
DEFINE_LUA_FUNCTION(gui_parsecolor, "color")
{
	int color = getcolor_unmodified(L, 1, 0);
	int r = (color & 0xFF000000) >> 24;
	int g = (color & 0x00FF0000) >> 16;
	int b = (color & 0x0000FF00) >> 8;
	int a = (color & 0x000000FF);
	lua_pushinteger(L, r);
	lua_pushinteger(L, g);
	lua_pushinteger(L, b);
	lua_pushinteger(L, a);
	return 4;
}



static inline void blend32(u32 *dstPixel, u32 color)
{
	u8 *dst = (u8*) dstPixel;
	int r = (color & 0xFF000000) >> 24;
	int g = (color & 0x00FF0000) >> 16;
	int b = (color & 0x0000FF00) >> 8;
	int a = color & 0x000000FF;

	if (a == 255) {
		// direct copy
		dst[0] = b;
		dst[1] = g;
		dst[2] = r;
		dst[3] = a;
	}
	else if (a == 0) {
		// do not copy
	}
	else {
		// alpha-blending
		u8 bo = dst[0];
		u8 go = dst[1];
		u8 ro = dst[2];
		u8 ao = dst[3];
        dst[0] = (((b - bo) * a + (bo << 8)) >> 8);
        dst[1] = (((g - go) * a + (go << 8)) >> 8);
        dst[2] = (((r - ro) * a + (ro << 8)) >> 8);
        dst[3] =  ((a + ao) - ((a * ao + 0xFF) >> 8));
	}
}

static LuaGUIData curGuiData;

static void prepare_drawing()
{
	curGuiData = GetCurrentInfo().guiData;
}
static void prepare_reading()
{
	curGuiData = GetCurrentInfo().guiData;
	u32* buf = (u32*)aggDraw.screen->buf().buf();
	if(buf)
	{
		curGuiData.data = buf;
		curGuiData.stridePix = aggDraw.screen->buf().stride_abs() / 4;
	}
	else
	{
#ifdef WIN32
		extern VideoInfo video;
		curGuiData.data = video.buffer;
		curGuiData.stridePix = 256;
#endif
	}
}

// note: prepare_drawing or prepare_reading must be called,
// before any of the following bunch of gui functions will work properly.


// negative -> top
// positive -> bottom
// 0 -> both
static void restrict_to_screen(int ySelectScreen)
{
	if(ySelectScreen > 0)
		curGuiData.yMin = (curGuiData.yMin + curGuiData.yMax) >> 1;
	else if(ySelectScreen < 0)
		curGuiData.yMax = (curGuiData.yMin + curGuiData.yMax) >> 1;
}

// check if a pixel is in the lua canvas
static FORCEINLINE bool gui_checkboundary(int x, int y) {
	return !(x < curGuiData.xMin || x >= curGuiData.xMax || y < curGuiData.yMin || y >= curGuiData.yMax);
}
static FORCEINLINE void gui_adjust_coord(int& x, int& y) {
	x += curGuiData.xOrigin;
	y += curGuiData.yOrigin;
}
static FORCEINLINE bool gui_checkbox(int x1, int y1, int x2, int y2) {
	if((x1 <  curGuiData.xMin && x2 <  curGuiData.xMin)
	|| (x1 >= curGuiData.xMax && x2 >= curGuiData.xMax)
	|| (y1 <  curGuiData.yMin && y2 <  curGuiData.yMin)
	|| (y1 >= curGuiData.yMax && y2 >= curGuiData.yMax))
		return false;
	return true;
}

// write a pixel (do not check boundaries for speedup)
static FORCEINLINE void gui_drawpixel_unchecked(int x, int y, u32 color) {
	blend32((u32*) &curGuiData.data[y*curGuiData.stridePix+x], color);
}

// write a pixel (check boundaries)
static FORCEINLINE void gui_drawpixel_checked(int x, int y, u32 color) {
	if (gui_checkboundary(x, y))
		gui_drawpixel_unchecked(x, y, color);
}

static FORCEINLINE u32 gui_getpixel_unchecked(int x, int y) {
	return curGuiData.data[y*curGuiData.stridePix+x];
}
static FORCEINLINE u32 gui_adjust_coord_and_getpixel(int x, int y) {
	x += curGuiData.xOrigin;
	y += curGuiData.yOrigin;
	x = min(max(x, curGuiData.xMin), curGuiData.xMax-1);
	y = min(max(y, curGuiData.yMin), curGuiData.yMax-1);
	return gui_getpixel_unchecked(x, y);
}

// draw a line (checks boundaries)
static void gui_drawline_internal(int x1, int y1, int x2, int y2, bool lastPixel, u32 color)
{
	// Note: New version of Bresenham's Line Algorithm
	// http://groups.google.co.jp/group/rec.games.roguelike.development/browse_thread/thread/345f4c42c3b25858/29e07a3af3a450e6?show_docid=29e07a3af3a450e6

	int swappedx = 0;
	int swappedy = 0;

	int xtemp = x1-x2;
	int ytemp = y1-y2;
	if (xtemp == 0 && ytemp == 0) {
		gui_drawpixel_checked(x1, y1, color);
		return;
	}
	if (xtemp < 0) {
		xtemp = -xtemp;
		swappedx = 1;
	}
	if (ytemp < 0) {
		ytemp = -ytemp;
		swappedy = 1;
	}

	int delta_x = xtemp << 1;
	int delta_y = ytemp << 1;

	signed char ix = x1 > x2?1:-1;
	signed char iy = y1 > y2?1:-1;

	if (lastPixel)
		gui_drawpixel_checked(x2, y2, color);

	if (delta_x >= delta_y) {
		int error = delta_y - (delta_x >> 1);

		while (x2 != x1) {
			if (error == 0 && !swappedx)
				gui_drawpixel_checked(x2+ix, y2, color);
			if (error >= 0) {
				if (error || (ix > 0)) {
					y2 += iy;
					error -= delta_x;
				}
			}
			x2 += ix;
			gui_drawpixel_checked(x2, y2, color);
			if (error == 0 && swappedx)
				gui_drawpixel_checked(x2, y2+iy, color);
			error += delta_y;
		}
	}
	else {
		int error = delta_x - (delta_y >> 1);

		while (y2 != y1) {
			if (error == 0 && !swappedy)
				gui_drawpixel_checked(x2, y2+iy, color);
			if (error >= 0) {
				if (error || (iy > 0)) {
					x2 += ix;
					error -= delta_y;
				}
			}
			y2 += iy;
			gui_drawpixel_checked(x2, y2, color);
			if (error == 0 && swappedy)
				gui_drawpixel_checked(x2+ix, y2, color);
			error += delta_x;
		}
	}
}

static const u8 Small_Font_Data[] =
{
#define I +0,
#define a +1
#define b +2
#define c +4
#define d +8
#define e +16
//  !"#$%&'
           I     c     I   b   d   I           I      d    I a b       I     c     I     c     I
           I     c     I   b   d   I   b   d   I    c d e  I a b     e I   b   d   I     c     I
           I     c     I           I a b c d e I  b        I       d   I   b c     I           I
           I     c     I           I   b   d   I  b c      I     c     I   b       I           I
           I     c     I           I a b c d e I      d e  I   b       I a   c   e I           I
           I           I           I   b   d   I        e  I a     d e I a     d   I           I
           I     c     I           I           I  b c d    I       d e I   b c   e I           I
           I           I           I           I    c      I           I           I           I
// ()*+,-./
        e  I   b       I           I           I           I           I           I        e  I
      d    I     c     I  b     e  I     c     I           I           I           I        e  I
      d    I     c     I    c d    I     c     I           I           I           I      d    I
      d    I     c     I  b c d e  I a b c d e I           I  b c d e  I           I      d    I
      d    I     c     I    c d    I     c     I           I           I           I    c      I
      d    I     c     I  b     e  I     c     I      d    I           I    c d    I    c      I
      d    I     c     I           I           I      d    I           I    c d    I  b        I
        e  I   b       I           I           I    c      I           I           I  b        I
// 01234567
    c d    I      d    I    c d    I    c d    I  b     e  I  b c d e  I    c d    I  b c d e  I
  b     e  I    c d    I  b     e  I  b     e  I  b     e  I  b        I  b     e  I        e  I
  b     e  I      d    I        e  I        e  I  b     e  I  b        I  b        I      d    I
  b     e  I      d    I      d    I    c d    I  b c d e  I  b c d    I  b c d    I      d    I
  b     e  I      d    I    c      I        e  I        e  I        e  I  b     e  I    c      I
  b     e  I      d    I  b        I  b     e  I        e  I        e  I  b     e  I    c      I
    c d    I    c d e  I  b c d e  I    c d    I        e  I  b c d    I    c d    I  b        I
           I           I           I           I           I           I           I           I
// 89:;<=>?
    c d    I    c d    I           I           I        e  I           I  b        I    c d    I
  b     e  I  b     e  I           I           I      d    I           I    c      I  b     e  I
  b     e  I  b     e  I    c d    I      d    I    c      I  b c d e  I      d    I        e  I
    c d    I    c d e  I    c d    I      d    I  b        I           I        e  I      d    I
  b     e  I        e  I           I           I    c      I  b c d e  I      d    I    c      I
  b     e  I  b     e  I    c d    I      d    I      d    I           I    c      I           I
    c d    I    c d    I    c d    I      d    I        e  I           I  b        I    c      I
           I           I           I    c      I           I           I           I           I
// @ABCDEFG
   b c d   I    c d    I  b c d    I    c d    I  b c d    I  b c d e  I  b c d e  I    c d    I
 a       e I  b     e  I  b     e  I  b     e  I  b     e  I  b        I  b        I  b     e  I
 a     d e I  b     e  I  b     e  I  b        I  b     e  I  b        I  b        I  b        I
 a   c   e I  b c d e  I  b c d    I  b        I  b     e  I  b c d    I  b c d    I  b        I
 a     d e I  b     e  I  b     e  I  b        I  b     e  I  b        I  b        I  b   d e  I
 a         I  b     e  I  b     e  I  b     e  I  b     e  I  b        I  b        I  b     e  I
   b c d e I  b     e  I  b c d    I    c d    I  b c d    I  b c d e  I  b        I    c d e  I
           I           I           I           I           I           I           I           I
// HIJKlMNO
  b     e  I   b c d   I        e  I  b     e  I  b        I a       e I  b     e  I  b c d e  I
  b     e  I     c     I        e  I  b     e  I  b        I a b   d e I  b c   e  I  b     e  I
  b     e  I     c     I        e  I  b   d    I  b        I a   c   e I  b   d e  I  b     e  I
  b c d e  I     c     I        e  I  b c      I  b        I a   c   e I  b     e  I  b     e  I
  b     e  I     c     I        e  I  b   d    I  b        I a       e I  b     e  I  b     e  I
  b     e  I     c     I  b     e  I  b     e  I  b        I a       e I  b     e  I  b     e  I
  b     e  I   b c d   I    c d    I  b     e  I  b c d e  I a       e I  b     e  I  b c d e  I
           I           I           I           I           I           I           I           I
// PQRSTUVW
  b c d    I    c d    I  b c d    I    c d e  I a b c d e I  b     e  I a       e I a       e I
  b     e  I  b     e  I  b     e  I  b        I     c     I  b     e  I a       e I a       e I
  b     e  I  b     e  I  b     e  I  b        I     c     I  b     e  I a       e I a       e I
  b c d    I  b     e  I  b c d    I    c d    I     c     I  b     e  I a       e I a       e I
  b        I  b     e  I  b     e  I        e  I     c     I  b     e  I   b   d   I a   c   e I
  b        I  b     e  I  b     e  I        e  I     c     I  b     e  I   b   d   I a b   d e I
  b        I    c d    I  b     e  I  b c d    I     c     I    c d    I     c     I a       e I
           I      d e  I           I           I           I           I           I           I
// XYZ[\]^_
 a       e I a       e I a b c d e I      d e  I  b        I   b c     I     c     I           I
 a       e I a       e I         e I      d    I  b        I     c     I   b   d   I           I
   b   d   I a       e I       d   I      d    I    c      I     c     I           I           I
     c     I   b   d   I     c     I      d    I    c      I     c     I           I           I
   b   d   I     c     I   b       I      d    I      d    I     c     I           I           I
 a       e I     c     I a         I      d    I      d    I     c     I           I           I
 a       e I     c     I a b c d e I      d    I        e  I     c     I           I           I
           I           I           I      d e  I        e  I   b c     I           I a b c d e I
// `abcdefg
  b        I           I  b        I           I        e  I           I      d e  I           I
    c      I           I  b        I           I        e  I           I    c      I           I
           I    c d    I  b        I    c d    I        e  I    c d    I    c      I    c d e  I
           I        e  I  b c d    I  b     e  I    c d e  I  b     e  I  b c d    I  b     e  I
           I    c d e  I  b     e  I  b        I  b     e  I  b c d e  I    c      I  b     e  I
           I  b     e  I  b     e  I  b        I  b     e  I  b        I    c      I    c d e  I
           I    c d e  I  b c d    I    c d e  I    c d e  I    c d e  I    c      I        e  I
           I           I           I           I           I           I           I    c d    I
// hijklmno
  b        I           I           I  b        I     c     I           I           I           I
  b        I     c     I     c     I  b        I     c     I           I           I           I
  b        I           I           I  b        I     c     I a b c d   I  b c d    I    c d    I
  b c d    I     c     I     c     I  b   d e  I     c     I a   c   e I  b     e  I  b     e  I
  b     e  I     c     I     c     I  b c      I     c     I a   c   e I  b     e  I  b     e  I
  b     e  I     c     I     c     I  b   d    I     c     I a   c   e I  b     e  I  b     e  I
  b     e  I     c     I     c     I  b     e  I     c     I a       e I  b     e  I    c d    I
           I           I   b       I           I           I           I           I           I
// pqrstuvw
           I           I           I           I     c     I           I           I           I
           I           I           I           I     c     I           I           I           I
  b c d    I    c d e  I  b c d    I    c d e  I   b c d   I  b     e  I  b     e  I a       e I
  b     e  I  b     e  I  b     e  I  b        I     c     I  b     e  I  b     e  I a       e I
  b     e  I  b     e  I  b        I    c d    I     c     I  b     e  I  b     e  I a   c   e I
  b c d    I    c d e  I  b        I        e  I     c     I  b     e  I    c d    I a b   d e I
  b        I        e  I  b        I  b c d    I       d   I    c d e  I    c d    I a       e I
  b        I        e  I           I           I           I           I           I           I
// xyz{|}~
           I           I           I      d e  I     c     I   b c     I           I           I
           I           I           I      d    I     c     I     c     I           I   b   d   I
  b     e  I  b     e  I  b c d e  I      d    I     c     I     c     I    c   e  I a   c   e I
    c d    I  b     e  I        e  I    c d    I     c     I     c d   I  b   d    I a       e I
    c d    I  b     e  I    c d    I    c d    I     c     I     c d   I           I   b   d   I
  b     e  I    c d e  I  b        I      d    I     c     I     c     I           I     c     I
  b     e  I        e  I  b c d e  I      d    I     c     I     c     I           I           I
           I  b c d    I           I      d e  I     c     I   b c     I           I           I
#undef I
#undef a
#undef b
#undef c
#undef d
#undef e
};

template<int dxdx, int dydy, int dxdy, int dydx>
static void PutTextInternal (const char *str, int len, short x, short y, int color, int backcolor)
{
	int Opac = color & 0xFF;
	int backOpac = backcolor & 0xFF;
	int origX = x;
	int origY = y;

	if(!Opac && !backOpac)
		return;

	while(*str && len)
	{
		if(dydy > 0 && y >= curGuiData.yMax) break;
		if(dydy < 0 && y < curGuiData.yMin) break;
		if(dxdy > 0 && x >= curGuiData.xMax) break;
		if(dxdy < 0 && x < curGuiData.xMin) break;

		int c = *str++;
		if(dxdx > 0 && x >= curGuiData.xMax
		|| dxdx < 0 && x < curGuiData.xMin
		|| dydx > 0 && y >= curGuiData.yMax
		|| dydx < 0 && y < curGuiData.yMin)
		{
			while (c != '\n') {
				c = *str;
				if (c == '\0')
					break;
				str++;
			}
		}

		if(c == '\n')
		{
			if(dydy)
			{
				x = origX;
				y += 10 * dydy;
			}
			else
			{
				y = origY;
				x += 10 * dxdy;
			}
			continue;
		}
		else if(c == '\t') // just in case
		{
			const int tabSpace = 8;
			x += (tabSpace-(((x-origX)/5)%tabSpace))*5*dxdx;
			y += (tabSpace-(((y-origY)/5)%tabSpace))*5*dydx;
			continue;
		}
		c -= 32;
		if((unsigned int)c >= 96)
			continue;

		if(c)
		{
			const u8* Cur_Glyph = (const unsigned char*)&Small_Font_Data + (c%8)+((c/8)*64);
			for(int y2 = -1; y2 < 10; y2++)
			{
				for(int x2 = -1; x2 < 6; x2++)
				{
					bool on = y2 >= 0 && y2 < 8 && (Cur_Glyph[y2*8] & (1 << x2));
					if(on)
					{
						gui_drawpixel_checked(x+x2*dxdx+y2*dxdy, y+y2*dydy+x2*dydx, color);
					}
					else if(backOpac)
					{
						for(int y3 = max(0,y2-1); y3 <= min(7,y2+1); y3++)
						{
							for(int x3 = max(0,x2-1); x3 <= min(4,x2+1); x3++)
							{
								on |= y3 >= 0 && y3 < 8 && (Cur_Glyph[y3*8] & (1 << x3));
								if (on)
									goto draw_outline; // speedup?
							}
						}
						if(on)
						{
draw_outline:
							gui_drawpixel_checked(x+x2*dxdx+y2*dxdy, y+y2*dydy+x2*dydx, backcolor);
						}
					}
				}
			}
		}

		x += 6*dxdx;
		y += 6*dydx;
		len--;
	}
}

static int strlinelen(const char* string)
{
	const char* s = string;
	while(*s && *s != '\n')
		s++;
	if(*s)
		s++;
	return s - string;
}

static void LuaDisplayString (const char *str, int x, int y, u32 color, u32 outlineColor)
{
	if(!str)
		return;

#if 1
	//if(rotate == 0)
		PutTextInternal<1,1,0,0>(str, strlen(str), x, y, color, outlineColor);
	//else if(rotate == 90)
	//	PutTextInternal<0,0,1,-1>(str, strlen(str), x, y, color, outlineColor);
	//else if
#else
	const char* ptr = str;
	while(*ptr && y < curGuiData.yMax)
	{
		int len = strlinelen(ptr);
		int skip = 0;
		if(len < 1) len = 1;

		// break up the line if it's too long to display otherwise
		if(len > 63)
		{
			len = 63;
			const char* ptr2 = ptr + len-1;
			for(int j = len-1; j; j--, ptr2--)
			{
				if(*ptr2 == ' ' || *ptr2 == '\t')
				{
					len = j;
					skip = 1;
					break;
				}
			}
		}

		int xl = 0;
		int yl = curGuiData.yMin;
		int xh = (curGuiData.xMax - 1 - 1) - 4*len;
		int yh = curGuiData.yMax - 1;
		int x2 = min(max(x,xl),xh);
		int y2 = min(max(y,yl),yh);

		PutTextInternal<1,1,0,0>(ptr,len,x2,y2,color,outlineColor);

		ptr += len + skip;
		y += 8;
	}
#endif
}




DEFINE_LUA_FUNCTION(gui_text, "x,y,str[,color=\"white\"[,outline=\"black\"]]")
{
	int x = luaL_checkinteger(L,1); // have to check for errors before deferring
	int y = luaL_checkinteger(L,2);

	if(DeferGUIFuncIfNeeded(L))
		return 0; // we have to wait until later to call this function because we haven't emulated the next frame yet
		          // (the only way to avoid this deferring is to be in a gui.register or emu.registerafter callback)

	const char* str = toCString(L,3); // better than using luaL_checkstring here (more permissive)
	
	if(str && *str)
	{
		int foreColor = getcolor(L,4,0xFFFFFFFF);
		int backColor = getcolor(L,5,0x000000FF);

		prepare_drawing();
		gui_adjust_coord(x,y);

		LuaDisplayString(str, x, y, foreColor, backColor);
	}

	return 0;
}

DEFINE_LUA_FUNCTION(gui_box, "x1,y1,x2,y2[,fill[,outline]]")
{
	int x1 = luaL_checkinteger(L,1); // have to check for errors before deferring
	int y1 = luaL_checkinteger(L,2);
	int x2 = luaL_checkinteger(L,3);
	int y2 = luaL_checkinteger(L,4);

	if(DeferGUIFuncIfNeeded(L))
		return 0;

	int fillcolor = getcolor(L,5,0xFFFFFF3F);
	int outlinecolor = getcolor(L,6,fillcolor|0xFF);

	prepare_drawing();
	restrict_to_screen(y1);
	gui_adjust_coord(x1,y1);
	gui_adjust_coord(x2,y2);

	if(!gui_checkbox(x1,y1,x2,y2))
		return 0;

	// require x1,y1 <= x2,y2
	if (x1 > x2)
		std::swap(x1,x2);
	if (y1 > y2)
		std::swap(y1,y2);

	// avoid trying to draw lots of offscreen pixels
	// (this is intentionally 1 out from the edge here)
	x1 = min(max(x1, curGuiData.xMin-1), curGuiData.xMax);
	x2 = min(max(x2, curGuiData.xMin-1), curGuiData.xMax);
	y1 = min(max(y1, curGuiData.yMin-1), curGuiData.yMax);
	y2 = min(max(y2, curGuiData.yMin-1), curGuiData.yMax);

	if(outlinecolor & 0xFF)
	{
		if(y1 >= curGuiData.yMin)
			for (short x = x1+1; x < x2; x++)
				gui_drawpixel_unchecked(x,y1,outlinecolor);

		if(x1 >= curGuiData.xMin && x1 < curGuiData.xMax)
		{
			if(y1 >= curGuiData.yMin)
				gui_drawpixel_unchecked(x1,y1,outlinecolor);
			for (short y = y1+1; y < y2; y++)
				gui_drawpixel_unchecked(x1,y,outlinecolor);
			if(y2 < curGuiData.yMax)
				gui_drawpixel_unchecked(x1,y2,outlinecolor);
		}

		if(y1 != y2 && y2 < curGuiData.yMax)
			for (short x = x1+1; x < x2; x++)
				gui_drawpixel_unchecked(x,y2,outlinecolor);
		if(x1 != x2 && x2 >= curGuiData.xMin && x2 < curGuiData.xMax)
		{
			if(y1 >= curGuiData.yMin)
				gui_drawpixel_unchecked(x2,y1,outlinecolor);
			for (short y = y1+1; y < y2; y++)
				gui_drawpixel_unchecked(x2,y,outlinecolor);
			if(y2 < curGuiData.yMax)
				gui_drawpixel_unchecked(x2,y2,outlinecolor);
		}
	}

	if(fillcolor & 0xFF)
	{
		for(short y = y1+1; y <= y2-1; y++)
			for(short x = x1+1; x <= x2-1; x++)
				gui_drawpixel_unchecked(x,y,fillcolor);
	}

	return 0;
}
// gui.setpixel(x,y,color)
// color can be a RGB web color like '#ff7030', or with alpha RGBA like '#ff703060'
//   or it can be an RGBA hex number like 0xFF703060
//   or it can be a preset color like 'red', 'orange', 'blue', 'white', etc.
DEFINE_LUA_FUNCTION(gui_pixel, "x,y[,color=\"white\"]")
{
	int x = luaL_checkinteger(L,1); // have to check for errors before deferring
	int y = luaL_checkinteger(L,2);

	if(DeferGUIFuncIfNeeded(L))
		return 0;

	int color = getcolor(L,3,0xFFFFFFFF);
	if(color & 0xFF)
	{
		prepare_drawing();
		gui_adjust_coord(x,y);
		gui_drawpixel_checked(x, y, color);
	}

	return 0;
}
// r,g,b = gui.getpixel(x,y)
DEFINE_LUA_FUNCTION(gui_getpixel, "x,y")
{
	prepare_reading();

	int x = luaL_checkinteger(L,1);
	int y = luaL_checkinteger(L,2);

	u32 color = gui_adjust_coord_and_getpixel(x,y);

	int b = (color & 0x000000FF);
	int g = (color & 0x0000FF00) >> 8;
	int r = (color & 0x00FF0000) >> 16;

	lua_pushinteger(L, r);
	lua_pushinteger(L, g);
	lua_pushinteger(L, b);

	return 3;
}
DEFINE_LUA_FUNCTION(gui_line, "x1,y1,x2,y2[,color=\"white\"[,skipfirst=false]]")
{
	int x1 = luaL_checkinteger(L,1); // have to check for errors before deferring
	int y1 = luaL_checkinteger(L,2);
	int x2 = luaL_checkinteger(L,3);
	int y2 = luaL_checkinteger(L,4);

	if(DeferGUIFuncIfNeeded(L))
		return 0;

	int color = getcolor(L,5,0xFFFFFFFF);
	int skipFirst = lua_toboolean(L,6);

	if(!(color & 0xFF))
		return 0;

	prepare_drawing();
	restrict_to_screen(y1);
	gui_adjust_coord(x1,y1);
	gui_adjust_coord(x2,y2);

	if(!gui_checkbox(x1,y1,x2,y2))
		return 0;

	gui_drawline_internal(x2, y2, x1, y1, !skipFirst, color);

	return 0;
}

// gui.opacity(number alphaValue)
// sets the transparency of subsequent draw calls
// 0.0 is completely transparent, 1.0 is completely opaque
// non-integer values are supported and meaningful, as are values greater than 1.0
// it is not necessary to use this function to get transparency (or the less-recommended gui.transparency() either),
// because you can provide an alpha value in the color argument of each draw call.
// however, it can be convenient to be able to globally modify the drawing transparency
DEFINE_LUA_FUNCTION(gui_setopacity, "alpha_0_to_1")
{
	lua_Number opacF = luaL_checknumber(L,1);
	opacF *= 255.0;
	if(opacF < 0) opacF = 0;
	int opac;
	lua_number2int(opac, opacF);
	LuaContextInfo& info = GetCurrentInfo();
	info.transparencyModifier = opac;
	return 0;
}

// gui.transparency(number transparencyValue)
// sets the transparency of subsequent draw calls
// 0.0 is completely opaque, 4.0 is completely transparent
// non-integer values are supported and meaningful, as are values less than 0.0
// this is a legacy function, and the range is from 0 to 4 solely for this reason
// it does the exact same thing as gui.opacity() but with a different argument range
DEFINE_LUA_FUNCTION(gui_settransparency, "transparency_4_to_0")
{
	lua_Number transp = luaL_checknumber(L,1);
	lua_Number opacF = 4 - transp;
	opacF *= 255.0 / 4.0;
	if(opacF < 0) opacF = 0;
	int opac;
	lua_number2int(opac, opacF);
	LuaContextInfo& info = GetCurrentInfo();
	info.transparencyModifier = opac;
	return 0;
}

// takes a screenshot and returns it in gdstr format
// example: gd.createFromGdStr(gui.gdscreenshot()):png("outputimage.png")
DEFINE_LUA_FUNCTION(gui_gdscreenshot, "[whichScreen='both']")
{
	prepare_reading();

	int selectedScreen = 0;
	if(lua_isboolean(L, 1))
		selectedScreen = lua_toboolean(L, 1) ? 1 : -1;
	else if(lua_isnumber(L, 1))
		selectedScreen = lua_tointeger(L, 1);
	else if(lua_isstring(L, 1))
	{
		const char* str = lua_tostring(L, 1);
		if(!stricmp(str,"top"))
			selectedScreen = -1;
		if(!stricmp(str,"bottom"))
			selectedScreen = 1;
	}
	restrict_to_screen(selectedScreen);

	int width = curGuiData.xMax - curGuiData.xMin;
	int height = curGuiData.yMax - curGuiData.yMin;
	int size = 11 + width * height * 4;

	char* str = new char[size+1]; // TODO: use _alloca instead (but I don't know which compilers support it)
	str[size] = 0;
	unsigned char* ptr = (unsigned char*)str;

	// GD format header for truecolor image (11 bytes)
	*ptr++ = (65534 >> 8) & 0xFF;
	*ptr++ = (65534     ) & 0xFF;
	*ptr++ = (width >> 8) & 0xFF;
	*ptr++ = (width     ) & 0xFF;
	*ptr++ = (height >> 8) & 0xFF;
	*ptr++ = (height     ) & 0xFF;
	*ptr++ = 1;
	*ptr++ = 255;
	*ptr++ = 255;
	*ptr++ = 255;
	*ptr++ = 255;

	u8* Src = (u8*)curGuiData.data + (curGuiData.stridePix*4) * curGuiData.yMin;

	for(int y = curGuiData.yMin; y < curGuiData.yMax; y++, Src += curGuiData.stridePix*4)
	{
		for(int x = curGuiData.xMin; x < curGuiData.xMax; x++)
		{
			*ptr++ = 255 - Src[4*x+3];
			*ptr++ = Src[4*x+2];
			*ptr++ = Src[4*x+1];
			*ptr++ = Src[4*x+0];
		}
	}

	lua_pushlstring(L, str, size);
	delete[] str;
	return 1;
}

// draws a gd image that's in gdstr format to the screen
// example: gui.gdoverlay(gd.createFromPng("myimage.png"):gdStr())
DEFINE_LUA_FUNCTION(gui_gdoverlay, "[x=0,y=0,]gdimage[,alphamul]")
{
	int xStart = 0;
	int yStart = 0;

	int index = 1;
	if(lua_type(L,index) == LUA_TNUMBER)
	{
		xStart = lua_tointeger(L,index++);
		if(lua_type(L,index) == LUA_TNUMBER)
			yStart = lua_tointeger(L,index++);
	}

	luaL_checktype(L,index,LUA_TSTRING); // have to check for errors before deferring

	if(DeferGUIFuncIfNeeded(L))
		return 0;

	const unsigned char* ptr = (const unsigned char*)lua_tostring(L,index++);

	// GD format header for truecolor image (11 bytes)
	ptr++;
	ptr++;
	int width = *ptr++ << 8;
	width |= *ptr++;
	int height = *ptr++ << 8;
	height |= *ptr++;
	ptr += 5;

	u8* Dst = (u8*)curGuiData.data;

	LuaContextInfo& info = GetCurrentInfo();
	int alphaMul = info.transparencyModifier;
	if(lua_isnumber(L, index))
		alphaMul = (int)(alphaMul * lua_tonumber(L, index++));
	if(alphaMul <= 0)
		return 0;

	prepare_drawing();
	gui_adjust_coord(xStart,yStart);

	int xMin = curGuiData.xMin;
	int yMin = curGuiData.yMin;
	int xMax = curGuiData.xMax - 1;
	int yMax = curGuiData.yMax - 1;
	int strideBytes = curGuiData.stridePix * 4;

	// since there aren't that many possible opacity levels,
	// do the opacity modification calculations beforehand instead of per pixel
	int opacMap[256];
	for(int i = 0; i < 256; i++)
	{
		int opac = 255 - (i << 1); // not sure why, but gdstr seems to divide each alpha value by 2
		opac = (opac * alphaMul) / 255;
		if(opac < 0) opac = 0;
		if(opac > 255) opac = 255;
		opacMap[i] = 255 - opac;
	}

	Dst += yStart * strideBytes;
	for(int y = yStart; y < height+yStart && y < yMax; y++, Dst += strideBytes)
	{
		if(y < yMin)
			ptr += width * 4;
		else
		{
			int xA = (xStart < xMin ? xMin : xStart);
			int xB = (xStart+width > xMax ? xMax : xStart+width);
			ptr += (xA - xStart) * 4;
			for(int x = xA; x < xB; x++)
			{
				int opac = opacMap[ptr[0]];
				u32 pix = (opac|(ptr[3]<<8)|(ptr[2]<<16)|(ptr[1]<<24));
				blend32((u32*)(Dst+x*4), pix);
				ptr += 4;
			}
			ptr += (xStart+width - xB) * 4;
		}
	}

	return 0;
}

static void GetCurrentScriptDir(char* buffer, int bufLen)
{
	LuaContextInfo& info = GetCurrentInfo();
	strncpy(buffer, info.lastFilename.c_str(), bufLen);
	buffer[bufLen-1] = 0;
	char* slash = std::max(strrchr(buffer, '/'), strrchr(buffer, '\\'));
	if(slash)
		slash[1] = 0;
}

DEFINE_LUA_FUNCTION(emu_openscript, "filename")
{
#ifdef WIN32
	char curScriptDir[1024]; GetCurrentScriptDir(curScriptDir, 1024); // make sure we can always find scripts that are in the same directory as the current script
	const char* filename = lua_isstring(L,1) ? lua_tostring(L,1) : NULL;
	extern const char* OpenLuaScript(const char* filename, const char* extraDirToCheck, bool makeSubservient);
	const char* errorMsg = OpenLuaScript(filename, curScriptDir, true);
	if(errorMsg)
		luaL_error(L, errorMsg);
#endif
    return 0;
}

DEFINE_LUA_FUNCTION(emu_reset, "")
{
	extern bool _HACK_DONT_STOPMOVIE;
	_HACK_DONT_STOPMOVIE = true;
	NDS_Reset();
	_HACK_DONT_STOPMOVIE = false;
	return 0;
}

// TODO
/*
DEFINE_LUA_FUNCTION(emu_loadrom, "filename")
{
	struct Temp { Temp() {EnableStopAllLuaScripts(false);} ~Temp() {EnableStopAllLuaScripts(true);}} dontStopScriptsHere;
	const char* filename = lua_isstring(L,1) ? lua_tostring(L,1) : NULL;
	char curScriptDir[1024]; GetCurrentScriptDir(curScriptDir, 1024);
	filename = MakeRomPathAbsolute(filename, curScriptDir);
	int result = GensLoadRom(filename);
	if(result <= 0)
		luaL_error(L, "Failed to load ROM \"%s\": %s", filename, result ? "invalid or unsupported" : "cancelled or not found");
	CallRegisteredLuaFunctions(LUACALL_ONSTART);
    return 0;
}
*/
DEFINE_LUA_FUNCTION(emu_getframecount, "")
{
	lua_pushinteger(L, currFrameCounter);
	return 1;
}
DEFINE_LUA_FUNCTION(emu_getlagcount, "")
{
	lua_pushinteger(L, TotalLagFrames);
	return 1;
}
DEFINE_LUA_FUNCTION(emu_lagged, "")
{
	lua_pushboolean(L, LagFrameFlag);
	return 1;
}
DEFINE_LUA_FUNCTION(emu_emulating, "")
{
	lua_pushboolean(L, driver->EMU_HasEmulationStarted());
	return 1;
}
DEFINE_LUA_FUNCTION(emu_atframeboundary, "")
{
	lua_pushboolean(L, driver->EMU_IsAtFrameBoundary());
	return 1;
}
DEFINE_LUA_FUNCTION(movie_getlength, "")
{
	lua_pushinteger(L, currMovieData.records.size());
	return 1;
}
DEFINE_LUA_FUNCTION(movie_isactive, "")
{
	lua_pushboolean(L, movieMode != MOVIEMODE_INACTIVE);
	return 1;
}
DEFINE_LUA_FUNCTION(movie_rerecordcount, "")
{
	lua_pushinteger(L, currMovieData.rerecordCount);
	return 1;
}
DEFINE_LUA_FUNCTION(movie_setrerecordcount, "")
{
	currMovieData.rerecordCount = luaL_checkinteger(L, 1);
	return 0;
}
DEFINE_LUA_FUNCTION(emu_rerecordcounting, "[enabled]")
{
	LuaContextInfo& info = GetCurrentInfo();
	if(lua_gettop(L) == 0)
	{
		// if no arguments given, return the current value
		lua_pushboolean(L, !info.rerecordCountingDisabled);
		return 1;
	}
	else
	{
		// set rerecord disabling
		info.rerecordCountingDisabled = !lua_toboolean(L,1);
		return 0;
	}
}
DEFINE_LUA_FUNCTION(movie_getreadonly, "")
{
	lua_pushboolean(L, movie_readonly);
	return 1;
}
DEFINE_LUA_FUNCTION(movie_setreadonly, "readonly")
{
	movie_readonly = lua_toboolean(L,1) != 0;

//	else if(!movie_readonly)
//		luaL_error(L, "movie.setreadonly failed: write permission denied");

	return 0;
}
DEFINE_LUA_FUNCTION(movie_isrecording, "")
{
	lua_pushboolean(L, movieMode == MOVIEMODE_RECORD);
	return 1;
}
DEFINE_LUA_FUNCTION(movie_isplaying, "")
{
	lua_pushboolean(L, movieMode == MOVIEMODE_PLAY);
	return 1;
}
DEFINE_LUA_FUNCTION(movie_getmode, "")
{
	switch(movieMode)
	{
	case MOVIEMODE_PLAY:
		lua_pushstring(L, "playback");
		break;
	case MOVIEMODE_RECORD:
		lua_pushstring(L, "record");
		break;
	case MOVIEMODE_INACTIVE:
		lua_pushstring(L, "inactive");
		break;
	case MOVIEMODE_FINISHED:
		lua_pushstring(L, "finished");
		break;
	default:
		lua_pushnil(L);
		break;
	}
	return 1;
}
DEFINE_LUA_FUNCTION(movie_getname, "")
{
	extern char curMovieFilename[512];
	lua_pushstring(L, curMovieFilename);
	return 1;
}
// movie.play() -- plays a movie of the user's choice
// movie.play(filename) -- starts playing a particular movie
// throws an error (with a description) if for whatever reason the movie couldn't be played
DEFINE_LUA_FUNCTION(movie_play, "[filename]")
{
	const char* filename = lua_isstring(L,1) ? lua_tostring(L,1) : NULL;
	const char* errorMsg = FCEUI_LoadMovie(filename, true, false, 0);
	if(errorMsg)
		luaL_error(L, errorMsg);
    return 0;
}
DEFINE_LUA_FUNCTION(movie_replay, "")
{
	if(movieMode == MOVIEMODE_INACTIVE)
		return 0;
	lua_settop(L, 0);
	extern char curMovieFilename[512];
	lua_pushstring(L, curMovieFilename);
	return movie_play(L);
}
DEFINE_LUA_FUNCTION(movie_close, "")
{
	FCEUI_StopMovie();
	return 0;
}

DEFINE_LUA_FUNCTION(sound_clear, "")
{
	SPU_ClearOutputBuffer();
	return 0;
}

#ifdef _WIN32
const char* s_keyToName[256] =
{
	NULL,
	"leftclick",
	"rightclick",
	NULL,
	"middleclick",
	NULL,
	NULL,
	NULL,
	"backspace",
	"tab",
	NULL,
	NULL,
	NULL,
	"enter",
	NULL,
	NULL,
	"shift", // 0x10
	"control",
	"alt",
	"pause",
	"capslock",
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	"escape",
	NULL,
	NULL,
	NULL,
	NULL,
	"space", // 0x20
	"pageup",
	"pagedown",
	"end",
	"home",
	"left",
	"up",
	"right",
	"down",
	NULL,
	NULL,
	NULL,
	NULL,
	"insert",
	"delete",
	NULL,
	"0","1","2","3","4","5","6","7","8","9",
	NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	"A","B","C","D","E","F","G","H","I","J",
	"K","L","M","N","O","P","Q","R","S","T",
	"U","V","W","X","Y","Z",
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	"numpad0","numpad1","numpad2","numpad3","numpad4","numpad5","numpad6","numpad7","numpad8","numpad9",
	"numpad*","numpad+",
	NULL,
	"numpad-","numpad.","numpad/",
	"F1","F2","F3","F4","F5","F6","F7","F8","F9","F10","F11","F12",
	"F13","F14","F15","F16","F17","F18","F19","F20","F21","F22","F23","F24",
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	"numlock",
	"scrolllock",
	NULL, // 0x92
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, // 0xB9
	"semicolon",
	"plus",
	"comma",
	"minus",
	"period",
	"slash",
	"tilde",
	NULL, // 0xC1
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, // 0xDA
	"leftbracket",
	"backslash",
	"rightbracket",
	"quote",
};
#endif


// input.get()
// takes no input, returns a lua table of entries representing the current input state,
// independent of the joypad buttons the emulated game thinks are pressed
// for example:
//   if the user is holding the W key and the left mouse button
//   and has the mouse at the bottom-right corner of the game screen,
//   then this would return {W=true, leftclick=true, xmouse=319, ymouse=223}
DEFINE_LUA_FUNCTION(input_getcurrentinputstatus, "")
{
	lua_newtable(L);

#ifdef _WIN32
	// keyboard and mouse button status
	{
		int BackgroundInput = 0;//TODO

		unsigned char keys [256];
		if(!BackgroundInput)
		{
			if(GetKeyboardState(keys))
			{
				for(int i = 1; i < 255; i++)
				{
					int mask = (i == VK_CAPITAL || i == VK_NUMLOCK || i == VK_SCROLL) ? 0x01 : 0x80;
					if(keys[i] & mask)
					{
						const char* name = s_keyToName[i];
						if(name)
						{
							lua_pushboolean(L, true);
							lua_setfield(L, -2, name);
						}
					}
				}
			}
		}
		else // use a slightly different method that will detect background input:
		{
			for(int i = 1; i < 255; i++)
			{
				const char* name = s_keyToName[i];
				if(name)
				{
					int active;
					if(i == VK_CAPITAL || i == VK_NUMLOCK || i == VK_SCROLL)
						active = GetKeyState(i) & 0x01;
					else
						active = GetAsyncKeyState(i) & 0x8000;
					if(active)
					{
						lua_pushboolean(L, true);
						lua_setfield(L, -2, name);
					}
				}
			}
		}
	}
	// mouse position in game screen pixel coordinates
	{
		void UnscaleScreenCoords(s32& x, s32& y);
		void ToDSScreenRelativeCoords(s32& x, s32& y, int bottomScreen);

		POINT point;
		GetCursorPos(&point);
		ScreenToClient(MainWindow->getHWnd(), &point);
		s32 x (point.x);
		s32 y (point.y);

		UnscaleScreenCoords(x,y);
		ToDSScreenRelativeCoords(x,y,1);

		lua_pushinteger(L, x);
		lua_setfield(L, -2, "xmouse");
		lua_pushinteger(L, y);
		lua_setfield(L, -2, "ymouse");
	}

	worry(L,10);
#else
	// NYI (well, return an empty table)
#endif

	return 1;
}


// resets our "worry" counter of the Lua state
int dontworry(LuaContextInfo& info)
{
	if(info.stopWorrying)
	{
		info.stopWorrying = false;
		if(info.worryCount)
			indicateBusy(info.L, false);
	}
	info.worryCount = 0;
	return 0;
}

//agg basic shapes
//TODO polygon and polyline, maybe the overloads for roundedRect and curve

#include "aggdraw.h"

static int line(lua_State *L) {

	double x1,y1,x2,y2;
	x1 = luaL_checknumber(L,1) + 0.5;
	y1 = luaL_checknumber(L,2) + 0.5;
	x2 = luaL_checknumber(L,3) + 0.5;
	y2 = luaL_checknumber(L,4) + 0.5;

	aggDraw.hud->line(x1, y1, x2, y2);

	return 0;
}

static int triangle(lua_State *L) {

	double x1,y1,x2,y2,x3,y3;
	x1 = luaL_checknumber(L,1) + 0.5;
	y1 = luaL_checknumber(L,2) + 0.5;
	x2 = luaL_checknumber(L,3) + 0.5;
	y2 = luaL_checknumber(L,4) + 0.5;
	x3 = luaL_checknumber(L,5) + 0.5;
	y3 = luaL_checknumber(L,6) + 0.5;

	aggDraw.hud->triangle(x1, y1, x2, y2, x3, y3);

	return 0;
}

static int rectangle(lua_State *L) {

	double x1,y1,x2,y2;
	x1 = luaL_checknumber(L,1) + 0.5;
	y1 = luaL_checknumber(L,2) + 0.5;
	x2 = luaL_checknumber(L,3) + 0.5;
	y2 = luaL_checknumber(L,4) + 0.5;

	aggDraw.hud->rectangle(x1, y1, x2, y2);

	return 0;
}

static int roundedRect(lua_State *L) {

	double x1,y1,x2,y2,r;
	x1 = luaL_checknumber(L,1) + 0.5;
	y1 = luaL_checknumber(L,2) + 0.5;
	x2 = luaL_checknumber(L,3) + 0.5;
	y2 = luaL_checknumber(L,4) + 0.5;
	r  = luaL_checknumber(L,5);

	aggDraw.hud->roundedRect(x1, y1, x2, y2, r);

	return 0;
}

static int ellipse(lua_State *L) {

	double cx,cy,rx,ry;
	cx = luaL_checknumber(L,1) + 0.5;
	cy = luaL_checknumber(L,2) + 0.5;
	rx = luaL_checknumber(L,3);
	ry = luaL_checknumber(L,4);

	aggDraw.hud->ellipse(cx, cy, rx, ry);

	return 0;
}

static int arc(lua_State *L) {

	double cx,cy,rx,ry,start,sweep;
	cx = luaL_checknumber(L,1) + 0.5;
	cy = luaL_checknumber(L,2) + 0.5;
	rx = luaL_checknumber(L,3);
	ry = luaL_checknumber(L,4);
	start = luaL_checknumber(L,5);
	sweep = luaL_checknumber(L,6);

	aggDraw.hud->arc(cx, cy,rx, ry, start, sweep);

	return 0;
}

static int star(lua_State *L) {

	double cx,cy,r1,r2,startAngle;
	int numRays;
	cx = luaL_checknumber(L,1) + 0.5;
	cy = luaL_checknumber(L,2) + 0.5;
	r1 = luaL_checknumber(L,3);
	r2 = luaL_checknumber(L,4);
	startAngle = luaL_checknumber(L,5);
	numRays = luaL_checkinteger(L,6);

	aggDraw.hud->star(cx, cy, r1, r2, startAngle, numRays);

	return 0;
}

static int curve(lua_State *L) {

	double x1,y1,x2,y2,x3,y3;
	x1 = luaL_checknumber(L,1) + 0.5;
	y1 = luaL_checknumber(L,2) + 0.5;
	x2 = luaL_checknumber(L,3) + 0.5;
	y2 = luaL_checknumber(L,4) + 0.5;
	x3 = luaL_checknumber(L,5) + 0.5;
	y3 = luaL_checknumber(L,6) + 0.5;

	aggDraw.hud->curve(x1, y1, x2, y2, x3, y3);

	return 0;
}

static const struct luaL_reg aggbasicshapes [] =
{
	{"line", line},
	{"triangle", triangle},
	{"rectangle", rectangle},
	{"roundedRect", roundedRect},
	{"ellipse", ellipse},
	{"arc", arc},
	{"star", star},
	{"curve", curve},
//	{"polygon", polygon},
//	{"polyline", polyline},
	{NULL, NULL}
};

//agg general attributes
//TODO missing functions, maybe the missing overloads 

static void getColorForAgg(lua_State *L, int&r,int&g,int&b,int&a)
{
	if(lua_gettop(L) == 1)
	{
		int color = getcolor(L, 1, 0xFF);
		r = (color & 0xFF000000) >> 24;
		g = (color & 0x00FF0000) >> 16;
		b = (color & 0x0000FF00) >> 8;
		a = (color & 0x000000FF);
	}
	else
	{
		r = luaL_optinteger(L,1,255);
		g = luaL_optinteger(L,2,255);
		b = luaL_optinteger(L,3,255);
		a = luaL_optinteger(L,4,255);
	}
}

static int fillColor(lua_State *L) {

	int r,g,b,a;
	getColorForAgg(L, r,g,b,a);

	aggDraw.hud->fillColor(r, g, b, a);

	return 0;
}

static int noFill(lua_State *L) {

	aggDraw.hud->noFill();
	return 0;
}

static int lineColor(lua_State *L) {

	int r,g,b,a;
	getColorForAgg(L, r,g,b,a);

	aggDraw.hud->lineColor(r, g, b, a);

	return 0;
}

static int noLine(lua_State *L) {

	aggDraw.hud->noLine();
	return 0;
}

static int lineWidth(lua_State *L) {

	double w;
	w = luaL_checknumber(L,1);

	aggDraw.hud->lineWidth(w);

	return 0;
}

static const struct luaL_reg agggeneralattributes [] =
{
//	{"blendMode", blendMode},
//	{"imageBlendMode", imageBlendMode},
//	{"imageBlendColor", imageBlendColor},
//	{"masterAlpha", masterAlpha},
//	{"antiAliasGamma", antiAliasGamma},
//	{"font", font},
	{"fillColor", fillColor},
	{"noFill", noFill},
	{"lineColor", lineColor},
	{"noLine", noLine},
//	{"fillLinearGradient", fillLinearGradient},
//	{"lineLinearGradient", lineLinearGradient},
//	{"fillRadialGradient", fillRadialGradient},
//	{"lineRadialGradient", lineRadialGradient},
	{"lineWidth", lineWidth},
//	{"lineCap", lineCap},
//	{"lineJoin", lineJoin},
//	{"fillEvenOdd", fillEvenOdd},
	{NULL, NULL}
};

static int setFont(lua_State *L) {

	const char *choice;
	choice = luaL_checkstring(L,1);

	aggDraw.hud->setFont(choice);
	return 0;
}

static int text(lua_State *L) {
	int x, y;
	const char *choice;

	x = luaL_checkinteger(L, 1);
	y = luaL_checkinteger(L, 2);
	choice = luaL_checkstring(L,3);

	aggDraw.hud->renderTextDropshadowed(x,y,choice);
	return 0;
}

static const struct luaL_reg aggcustom [] =
{
	{"setFont", setFont},
	{"text", text},
	{NULL, NULL}
};


// gui.osdtext(int x, int y, string msg)
//
//  Displays the given text on the screen, using the same font and techniques as the
//  main HUD.
//
static int gui_osdtext(lua_State *L)
{
	int x = luaL_checkinteger(L,1); // have to check for errors before deferring
	int y = luaL_checkinteger(L,2);

	if(DeferGUIFuncIfNeeded(L))
		return 0; // we have to wait until later to call this function because we haven't emulated the next frame yet
		          // (the only way to avoid this deferring is to be in a gui.register or emu.registerafter callback)

	const char* msg = toCString(L,3);

	osd->addFixed(x, y, "%s", msg);

	return 0;
}

static int stylus_read(lua_State *L){
	
	lua_newtable(L);

	lua_pushinteger(L, nds.touchX >> 4);
	lua_setfield(L, -2, "x");
	lua_pushinteger(L, nds.touchY >> 4);
	lua_setfield(L, -2, "y");
	lua_pushboolean(L, nds.isTouch);
	lua_setfield(L, -2, "touch");	

	return 1;
}
static int stylus_peek(lua_State *L){
	
	lua_newtable(L);

	lua_pushinteger(L, NDS_getRawUserInput().touch.touchX >> 4);
	lua_setfield(L, -2, "x");
	lua_pushinteger(L, NDS_getRawUserInput().touch.touchY >> 4);
	lua_setfield(L, -2, "y");
	lua_pushboolean(L, NDS_getRawUserInput().touch.isTouch?1:0);
	lua_setfield(L, -2, "touch");	

	return 1;
}
static int toTouchValue(int pixCoord, int maximum)
{
	pixCoord = std::min(std::max(pixCoord, 0), maximum-1);
	return (pixCoord << 4) & 0x0FF0;
}
static int stylus_write(lua_State *L)
{
	if(movieMode == MOVIEMODE_PLAY) // don't allow tampering with a playing movie's input
		return 0; // (although it might be useful sometimes...)

	if(!NDS_isProcessingUserInput())
	{
		// defer this function until when we are processing input
		DeferFunctionCall(L, deferredJoySetIDString);
		return 0;
	}

	int index = 1;
	luaL_checktype(L, index, LUA_TTABLE);

	UserTouch& touch = NDS_getProcessingUserInput().touch;

	lua_getfield(L, index, "x");
	if (!lua_isnil(L,-1))
		touch.touchX = toTouchValue(lua_tointeger(L, -1), 256);
	lua_pop(L, 1);

	lua_getfield(L, index, "y");
	if (!lua_isnil(L,-1))
		touch.touchY = toTouchValue(lua_tointeger(L, -1), 192);
	lua_pop(L, 1);

	lua_getfield(L, index, "touch");
	if (!lua_isnil(L,-1))
		touch.isTouch = lua_toboolean(L, -1) ? true : false;
	lua_pop(L, 1);

	return 0;
}

static int gcEMUFILE_MEMORY(lua_State *L)
{
	EMUFILE_MEMORY** ppEmuFile = (EMUFILE_MEMORY**)luaL_checkudata(L, 1, "EMUFILE_MEMORY*");
	delete (*ppEmuFile);
	*ppEmuFile = 0;
	return 0;
}


static const struct luaL_reg styluslib [] =
{
	{"get", stylus_read},
	{"peek", stylus_peek},
	{"set", stylus_write},
	// alternative names
	{"read", stylus_read},
	{"write", stylus_write},
	{NULL, NULL}
};

static const struct luaL_reg emulib [] =
{
	{"frameadvance", emu_frameadvance},
	{"speedmode", emu_speedmode},
	{"wait", emu_wait},
	{"pause", emu_pause},
	{"unpause", emu_unpause},
	{"emulateframe", emu_emulateframe},
	{"emulateframefastnoskipping", emu_emulateframefastnoskipping},
	{"emulateframefast", emu_emulateframefast},
	{"emulateframeinvisible", emu_emulateframeinvisible},
	{"redraw", emu_redraw},
	{"framecount", emu_getframecount},
	{"lagcount", emu_getlagcount},
	{"lagged", emu_lagged},
	{"emulating", emu_emulating},
	{"atframeboundary", emu_atframeboundary},
	{"registerbefore", emu_registerbefore},
	{"registerafter", emu_registerafter},
	{"registerstart", emu_registerstart},
	{"registerexit", emu_registerexit},
	{"persistglobalvariables", emu_persistglobalvariables},
	{"message", emu_message},
	{"print", print}, // sure, why not
	{"openscript", emu_openscript},
//	{"loadrom", emu_loadrom},
	{"reset", emu_reset},
	// alternative names
//	{"openrom", emu_loadrom},
	{NULL, NULL}
};
static const struct luaL_reg guilib [] =
{
	{"register", gui_register},
	{"text", gui_text},
	{"box", gui_box},
	{"line", gui_line},
	{"pixel", gui_pixel},
	{"getpixel", gui_getpixel},
	{"opacity", gui_setopacity},
	{"transparency", gui_settransparency},
	{"popup", gui_popup},
	{"parsecolor", gui_parsecolor},
	{"gdscreenshot", gui_gdscreenshot},
	{"gdoverlay", gui_gdoverlay},
	{"redraw", emu_redraw}, // some people might think of this as more of a GUI function
	{"osdtext", gui_osdtext},
	// alternative names
	{"drawtext", gui_text},
	{"drawbox", gui_box},
	{"drawline", gui_line},
	{"drawpixel", gui_pixel},
	{"setpixel", gui_pixel},
	{"writepixel", gui_pixel},
	{"readpixel", gui_getpixel},
	{"rect", gui_box},
	{"drawrect", gui_box},
	{"drawimage", gui_gdoverlay},
	{"image", gui_gdoverlay},
	{NULL, NULL}
};
static const struct luaL_reg statelib [] =
{
	{"create", state_create},
	{"save", state_save},
	{"load", state_load},
#ifndef PUBLIC_RELEASE
	{"verify", state_verify}, // for desync catching
#endif
	// TODO
	//{"loadscriptdata", state_loadscriptdata},
	//{"savescriptdata", state_savescriptdata},
	//{"registersave", state_registersave},
	//{"registerload", state_registerload},
	{NULL, NULL}
};
static const struct luaL_reg memorylib [] =
{
	{"readbyte", memory_readbyte},
	{"readbytesigned", memory_readbytesigned},
	{"readword", memory_readword},
	{"readwordsigned", memory_readwordsigned},
	{"readdword", memory_readdword},
	{"readdwordsigned", memory_readdwordsigned},
	{"readbyterange", memory_readbyterange},
	{"writebyte", memory_writebyte},
	{"writeword", memory_writeword},
	{"writedword", memory_writedword},
	{"isvalid", memory_isvalid},
	{"getregister", memory_getregister},
	{"setregister", memory_setregister},
	// alternate naming scheme for word and double-word and unsigned
	{"readbyteunsigned", memory_readbyte},
	{"readwordunsigned", memory_readword},
	{"readdwordunsigned", memory_readdword},
	{"readshort", memory_readword},
	{"readshortunsigned", memory_readword},
	{"readshortsigned", memory_readwordsigned},
	{"readlong", memory_readdword},
	{"readlongunsigned", memory_readdword},
	{"readlongsigned", memory_readdwordsigned},
	{"writeshort", memory_writeword},
	{"writelong", memory_writedword},

	// memory hooks
	{"registerwrite", memory_registerwrite},
	{"registerread", memory_registerread},
	{"registerexec", memory_registerexec},
	// alternate names
	{"register", memory_registerwrite},
	{"registerrun", memory_registerexec},
	{"registerexecute", memory_registerexec},

	{NULL, NULL}
};
static const struct luaL_reg joylib [] =
{
	{"get", joy_get},
	{"getdown", joy_getdown},
	{"getup", joy_getup},
	{"peek", joy_peek},
	{"peekdown", joy_peekdown},
	{"peekup", joy_peekup},
	{"set", joy_set},
	// alternative names
	{"read", joy_get},
	{"write", joy_set},
	{"readdown", joy_getdown},
	{"readup", joy_getup},
	{NULL, NULL}
};
static const struct luaL_reg inputlib [] =
{
	{"get", input_getcurrentinputstatus},
	{"registerhotkey", input_registerhotkey},
	{"popup", input_popup},
	// alternative names
	{"read", input_getcurrentinputstatus},
	{NULL, NULL}
};
static const struct luaL_reg movielib [] =
{
	{"active", movie_isactive},
	{"recording", movie_isrecording},
	{"playing", movie_isplaying},
	{"mode", movie_getmode},

	{"length", movie_getlength},
	{"name", movie_getname},
	{"rerecordcount", movie_rerecordcount},
	{"setrerecordcount", movie_setrerecordcount},

	{"rerecordcounting", emu_rerecordcounting},
	{"readonly", movie_getreadonly},
	{"setreadonly", movie_setreadonly},
	{"framecount", emu_getframecount}, // for those familiar with other emulators that have movie.framecount() instead of emulatorname.framecount()

	{"play", movie_play},
	{"replay", movie_replay},
	{"stop", movie_close},

	// alternative names
	{"open", movie_play},
	{"close", movie_close},
	{"getname", movie_getname},
	{"playback", movie_play},
	{"getreadonly", movie_getreadonly},
	{NULL, NULL}
};
static const struct luaL_reg soundlib [] =
{
	{"clear", sound_clear},
	{NULL, NULL}
};

static const struct CFuncInfo
{
	const char* library;
	const char* name;
	const char* args;
	bool registry;
}
cFuncInfo [] = // this info is stored here to avoid having to change all of Lua's libraries to use something like DEFINE_LUA_FUNCTION
{
	{LUA_STRLIBNAME, "byte", "str[,start[,end]]"},
	{LUA_STRLIBNAME, "char", "...[bytes]"},
	{LUA_STRLIBNAME, "dump", "func"},
	{LUA_STRLIBNAME, "find", "str,pattern[,init[,plain]]"},
	{LUA_STRLIBNAME, "format", "formatstring,..."},
	{LUA_STRLIBNAME, "gfind", "!deprecated!"},
	{LUA_STRLIBNAME, "gmatch", "str,pattern"},
	{LUA_STRLIBNAME, "gsub", "str,pattern,repl[,n]"},
	{LUA_STRLIBNAME, "len", "str"},
	{LUA_STRLIBNAME, "lower", "str"},
	{LUA_STRLIBNAME, "match", "str,pattern[,init]"},
	{LUA_STRLIBNAME, "rep", "str,n"},
	{LUA_STRLIBNAME, "reverse", "str"},
	{LUA_STRLIBNAME, "sub", "str,start[,end]"},
	{LUA_STRLIBNAME, "upper", "str"},
	{NULL, "module", "name[,...]"},
	{NULL, "require", "modname"},
	{LUA_LOADLIBNAME, "loadlib", "libname,funcname"},
	{LUA_LOADLIBNAME, "seeall", "module"},
	{LUA_COLIBNAME, "create", "func"},
	{LUA_COLIBNAME, "resume", "co[,val1,...]"},
	{LUA_COLIBNAME, "running", ""},
	{LUA_COLIBNAME, "status", "co"},
	{LUA_COLIBNAME, "wrap", "func"},
	{LUA_COLIBNAME, "yield", "..."},
	{NULL, "assert", "cond[,message]"},
	{NULL, "collectgarbage", "opt[,arg]"},
	{NULL, "gcinfo", ""},
	{NULL, "dofile", "filename"},
	{NULL, "error", "message[,level]"},
	{NULL, "getfenv", "[level_or_func]"},
	{NULL, "getmetatable", "object"},
	{NULL, "ipairs", "arraytable"},
	{NULL, "load", "func[,chunkname]"},
	{NULL, "loadfile", "[filename]"},
	{NULL, "loadstring", "str[,chunkname]"},
	{NULL, "next", "table[,index]"},
	{NULL, "pairs", "table"},
	{NULL, "pcall", "func,arg1,..."},
	{NULL, "rawequal", "v1,v2"},
	{NULL, "rawget", "table,index"},
	{NULL, "rawset", "table,index,value"},
	{NULL, "select", "index,..."},
	{NULL, "setfenv", "level_or_func,envtable"},
	{NULL, "setmetatable", "table,metatable"},
	{NULL, "tonumber", "str_or_num[,base]"},
	{NULL, "type", "obj"},
	{NULL, "unpack", "list[,i=1[,j=#list]]"},
	{NULL, "xpcall", "func,errhandler"},
	{NULL, "newproxy", "hasmeta"},
	{LUA_MATHLIBNAME, "abs", "x"},
	{LUA_MATHLIBNAME, "acos", "x"},
	{LUA_MATHLIBNAME, "asin", "x"},
	{LUA_MATHLIBNAME, "atan", "x"},
	{LUA_MATHLIBNAME, "atan2", "y,x"},
	{LUA_MATHLIBNAME, "ceil", "x"},
	{LUA_MATHLIBNAME, "cos", "rads"},
	{LUA_MATHLIBNAME, "cosh", "x"},
	{LUA_MATHLIBNAME, "deg", "rads"},
	{LUA_MATHLIBNAME, "exp", "x"},
	{LUA_MATHLIBNAME, "floor", "x"},
	{LUA_MATHLIBNAME, "fmod", "x,y"},
	{LUA_MATHLIBNAME, "frexp", "x"},
	{LUA_MATHLIBNAME, "ldexp", "m,e"},
	{LUA_MATHLIBNAME, "log", "x"},
	{LUA_MATHLIBNAME, "log10", "x"},
	{LUA_MATHLIBNAME, "max", "x,..."},
	{LUA_MATHLIBNAME, "min", "x,..."},
	{LUA_MATHLIBNAME, "modf", "x"},
	{LUA_MATHLIBNAME, "pow", "x,y"},
	{LUA_MATHLIBNAME, "rad", "degs"},
	{LUA_MATHLIBNAME, "random", "[m[,n]]"},
	{LUA_MATHLIBNAME, "randomseed", "x"},
	{LUA_MATHLIBNAME, "sin", "rads"},
	{LUA_MATHLIBNAME, "sinh", "x"},
	{LUA_MATHLIBNAME, "sqrt", "x"},
	{LUA_MATHLIBNAME, "tan", "rads"},
	{LUA_MATHLIBNAME, "tanh", "x"},
	{LUA_IOLIBNAME, "close", "[file]"},
	{LUA_IOLIBNAME, "flush", ""},
	{LUA_IOLIBNAME, "input", "[file]"},
	{LUA_IOLIBNAME, "lines", "[filename]"},
	{LUA_IOLIBNAME, "open", "filename[,mode=\"r\"]"},
	{LUA_IOLIBNAME, "output", "[file]"},
	{LUA_IOLIBNAME, "popen", "prog,[model]"},
	{LUA_IOLIBNAME, "read", "..."},
	{LUA_IOLIBNAME, "tmpfile", ""},
	{LUA_IOLIBNAME, "type", "obj"},
	{LUA_IOLIBNAME, "write", "..."},
	{LUA_OSLIBNAME, "clock", ""},
	{LUA_OSLIBNAME, "date", "[format[,time]]"},
	{LUA_OSLIBNAME, "difftime", "t2,t1"},
	{LUA_OSLIBNAME, "execute", "[command]"},
	{LUA_OSLIBNAME, "exit", "[code]"},
	{LUA_OSLIBNAME, "getenv", "varname"},
	{LUA_OSLIBNAME, "remove", "filename"},
	{LUA_OSLIBNAME, "rename", "oldname,newname"},
	{LUA_OSLIBNAME, "setlocale", "locale[,category]"},
	{LUA_OSLIBNAME, "time", "[timetable]"},
	{LUA_OSLIBNAME, "tmpname", ""},
	{LUA_DBLIBNAME, "debug", ""},
	{LUA_DBLIBNAME, "getfenv", "o"},
	{LUA_DBLIBNAME, "gethook", "[thread]"},
	{LUA_DBLIBNAME, "getinfo", "[thread,]function[,what]"},
	{LUA_DBLIBNAME, "getlocal", "[thread,]level,local"},
	{LUA_DBLIBNAME, "getmetatable", "[object]"},
	{LUA_DBLIBNAME, "getregistry", ""},
	{LUA_DBLIBNAME, "getupvalue", "func,up"},
	{LUA_DBLIBNAME, "setfenv", "object,table"},
	{LUA_DBLIBNAME, "sethook", "[thread,]hook,mask[,count]"},
	{LUA_DBLIBNAME, "setlocal", "[thread,]level,local,value"},
	{LUA_DBLIBNAME, "setmetatable", "object,table"},
	{LUA_DBLIBNAME, "setupvalue", "func,up,value"},
	{LUA_DBLIBNAME, "traceback", "[thread,][message][,level]"},
	{LUA_TABLIBNAME, "concat", "table[,sep[,i[,j]]]"},
	{LUA_TABLIBNAME, "insert", "table,[pos,]value"},
	{LUA_TABLIBNAME, "maxn", "table"},
	{LUA_TABLIBNAME, "remove", "table[,pos]"},
	{LUA_TABLIBNAME, "sort", "table[,comp]"},
	{LUA_TABLIBNAME, "foreach", "table,func"},
	{LUA_TABLIBNAME, "foreachi", "table,func"},
	{LUA_TABLIBNAME, "getn", "table"},
	{LUA_TABLIBNAME, "maxn", "table"},
	{LUA_TABLIBNAME, "setn", "table,value"}, // I know some of these are obsolete but they should still have argument info if they're exposed to the user
	{LUA_FILEHANDLE, "setvbuf", "mode[,size]", true},
	{LUA_FILEHANDLE, "lines", "", true},
	{LUA_FILEHANDLE, "read", "...", true},
	{LUA_FILEHANDLE, "flush", "", true},
	{LUA_FILEHANDLE, "seek", "[whence][,offset]", true},
	{LUA_FILEHANDLE, "write", "...", true},
	{LUA_FILEHANDLE, "__tostring", "obj", true},
	{LUA_FILEHANDLE, "__gc", "", true},
	{"_LOADLIB", "__gc", "", true},
};

void registerLibs(lua_State* L)
{
	luaL_openlibs(L);

	luaL_register(L, "emu", emulib);
	luaL_register(L, "gui", guilib);
	luaL_register(L, "stylus", styluslib);
	luaL_register(L, "savestate", statelib);
	luaL_register(L, "memory", memorylib);
	luaL_register(L, "joypad", joylib); // for game input
	luaL_register(L, "input", inputlib); // for user input
	luaL_register(L, "movie", movielib);
	luaL_register(L, "sound", soundlib);
	luaL_register(L, "bit", bit_funcs); // LuaBitOp library

	luaL_register(L, "agg", aggbasicshapes);
	luaL_register(L, "agg", agggeneralattributes);
	luaL_register(L, "agg", aggcustom);
	
	lua_settop(L, 0); // clean the stack, because each call to luaL_register leaves a table on top
	
	// register a few utility functions outside of libraries (in the global namespace)
	lua_register(L, "print", print);
	lua_register(L, "tostring", tostring);
	lua_register(L, "addressof", addressof);
	lua_register(L, "copytable", copytable);

	// old bit operation functions
	lua_register(L, "AND", bit_band);
	lua_register(L, "OR", bit_bor);
	lua_register(L, "XOR", bit_bxor);
	lua_register(L, "SHIFT", bitshift);
	lua_register(L, "BIT", bitbit);

	luabitop_validate(L);

	// populate s_cFuncInfoMap the first time
	static bool once = true;
	if(once)
	{
		once = false;

		for(int i = 0; i < sizeof(cFuncInfo)/sizeof(*cFuncInfo); i++)
		{
			const CFuncInfo& cfi = cFuncInfo[i];
			if(cfi.registry)
			{
				lua_getregistry(L);
				lua_getfield(L, -1, cfi.library);
				lua_remove(L, -2);
				lua_getfield(L, -1, cfi.name);
				lua_remove(L, -2);
			}
			else if(cfi.library)
			{
				lua_getfield(L, LUA_GLOBALSINDEX, cfi.library);
				lua_getfield(L, -1, cfi.name);
				lua_remove(L, -2);
			}
			else
			{
				lua_getfield(L, LUA_GLOBALSINDEX, cfi.name);
			}

			lua_CFunction func = lua_tocfunction(L, -1);
			s_cFuncInfoMap[func] = cfi.args;
			lua_pop(L, 1);
		}

		// deal with some stragglers
		lua_getfield(L, LUA_GLOBALSINDEX, "package");
		lua_getfield(L, -1, "loaders");
		lua_remove(L, -2);
		if(lua_istable(L, -1))
		{
			for(int i=1;;i++)
			{
				lua_rawgeti(L, -1, i);
				lua_CFunction func = lua_tocfunction(L, -1);
				lua_pop(L,1);
				if(!func)
					break;
				s_cFuncInfoMap[func] = "name";
			}
		}
		lua_pop(L,1);
	}

	// push arrays for storing hook functions in
	for(int i = 0; i < LUAMEMHOOK_COUNT; i++)
	{
		lua_newtable(L);
		lua_setfield(L, LUA_REGISTRYINDEX, luaMemHookTypeStrings[i]);
	}

	// register type
	luaL_newmetatable(L, "EMUFILE_MEMORY*");
	lua_pushcfunction(L, gcEMUFILE_MEMORY);
	lua_setfield(L, -2, "__gc");
	lua_pop(L, 1);
}

void ResetInfo(LuaContextInfo& info)
{
	info.L = NULL;
	info.started = false;
	info.running = false;
	info.returned = false;
	info.crashed = false;
	info.restart = false;
	info.restartLater = false;
	info.worryCount = 0;
	info.stopWorrying = false;
	info.panic = false;
	info.ranExit = false;
	info.numDeferredFuncs = 0;
	info.ranFrameAdvance = false;
	info.transparencyModifier = 255;
	info.speedMode = SPEEDMODE_NORMAL;
	info.guiFuncsNeedDeferring = false;
	info.dataSaveKey = 0;
	info.dataLoadKey = 0;
	info.dataSaveLoadKeySet = false;
	info.rerecordCountingDisabled = false;
	info.numMemHooks = 0;
	info.persistVars.clear();
	info.newDefaultData.ClearRecords();
	info.guiData.data = (u32*)aggDraw.hud->buf().buf();
	info.guiData.stridePix = aggDraw.hud->buf().stride_abs() / 4;
	info.guiData.xMin = 0;
	info.guiData.xMax = 256;
	info.guiData.yMin = 0;
	info.guiData.yMax = 192 * 2;
	info.guiData.xOrigin = 0;
	info.guiData.yOrigin = 192;
}

void OpenLuaContext(int uid, void(*print)(int uid, const char* str), void(*onstart)(int uid), void(*onstop)(int uid, bool statusOK))
{
	LuaContextInfo* newInfo = new LuaContextInfo();
	ResetInfo(*newInfo);
	newInfo->print = print;
	newInfo->onstart = onstart;
	newInfo->onstop = onstop;
	luaContextInfo[uid] = newInfo;
}

void RunLuaScriptFile(int uid, const char* filenameCStr)
{
	if(luaContextInfo.find(uid) == luaContextInfo.end())
		return;
	StopLuaScript(uid);

	LuaContextInfo& info = *luaContextInfo[uid];

#ifdef USE_INFO_STACK
	infoStack.insert(infoStack.begin(), &info);
	struct Scope { ~Scope(){ infoStack.erase(infoStack.begin()); } } scope; // doing it like this makes sure that the info stack gets cleaned up even if an exception is thrown
#endif

	info.nextFilename = filenameCStr;

	if(info.running)
	{
		// it's a little complicated, but... the call to luaL_dofile below
		// could call a C function that calls this very function again
		// additionally, if that happened then the above call to StopLuaScript
		// probably couldn't stop the script yet, so instead of continuing,
		// we'll set a flag that tells the first call of this function to loop again
		// when the script is able to stop safely
		info.restart = true;
		return;
	}

	do
	{
		std::string filename = info.nextFilename;

		lua_State* L = lua_open();
#ifndef USE_INFO_STACK
		luaStateToContextMap[L] = &info;
#endif
		luaStateToUIDMap[L] = uid;
		ResetInfo(info);
		info.L = L;
		info.guiFuncsNeedDeferring = true;
		info.lastFilename = filename;

		SetSaveKey(info, FilenameFromPath(filename.c_str()));
		info.dataSaveLoadKeySet = false;

		registerLibs(L);

		// register a function to periodically check for inactivity
		lua_sethook(L, LuaRescueHook, LUA_MASKCOUNT, HOOKCOUNT);

		// deferred evaluation table
		lua_newtable(L);
		lua_setfield(L, LUA_REGISTRYINDEX, deferredGUIIDString);
		lua_newtable(L);
		lua_setfield(L, LUA_REGISTRYINDEX, deferredJoySetIDString);
		lua_newtable(L);
		lua_setfield(L, LUA_REGISTRYINDEX, refStashString);

		info.started = true;
		RefreshScriptStartedStatus();
		if(info.onstart)
			info.onstart(uid);
		info.running = true;
		RefreshScriptSpeedStatus();
		info.returned = false;
		int errorcode = luaL_dofile(L,filename.c_str());
		info.running = false;
		RefreshScriptSpeedStatus();
		info.returned = true;

		if (errorcode)
		{
			info.crashed = true;
			if(info.print)
			{
				info.print(uid, lua_tostring(L,-1));
				info.print(uid, "\r\n");
			}
			else
			{
				fprintf(stderr, "%s\n", lua_tostring(L,-1));
			}
			StopLuaScript(uid);
		}
		else
		{
			dontworry(info);
			driver->USR_RefreshScreen();
			StopScriptIfFinished(uid, true);
		}
	} while(info.restart);
}

void StopScriptIfFinished(int uid, bool justReturned)
{
	LuaContextInfo& info = *luaContextInfo[uid];
	if(!info.returned)
		return;

	// the script has returned, but it is not necessarily done running
	// because it may have registered a function that it expects to keep getting called
	// so check if it has any registered functions and stop the script only if it doesn't

	bool keepAlive = (info.numMemHooks != 0);
	for(int calltype = 0; calltype < LUACALL_COUNT && !keepAlive; calltype++)
	{
		lua_State* L = info.L;
		if(L)
		{
			const char* idstring = luaCallIDStrings[calltype];
			lua_getfield(L, LUA_REGISTRYINDEX, idstring);
			bool isFunction = lua_isfunction(L, -1);
			lua_pop(L, 1);

			if(isFunction)
				keepAlive = true;
		}
	}

	if(keepAlive)
	{
		if(justReturned)
		{
			if(info.print)
				info.print(uid, "script returned but is still running registered functions\r\n");
			else
				fprintf(stdout, "%s\n", "script returned but is still running registered functions");
		}
	}
	else
	{
		if(info.print)
			info.print(uid, "script finished running\r\n");
		else
			fprintf(stdout, "%s\n", "script finished running");

		StopLuaScript(uid);
	}
}

void RequestAbortLuaScript(int uid, const char* message)
{
	if(luaContextInfo.find(uid) == luaContextInfo.end())
		return;
	LuaContextInfo& info = *luaContextInfo[uid];
	lua_State* L = info.L;
	if(L)
	{
		// this probably isn't the right way to do it
		// but calling luaL_error here is positively unsafe
		// (it seemingly works fine but sometimes corrupts the emulation state in colorful ways)
		// and this works pretty well and is definitely safe, so screw it
		info.L->hookcount = 1; // run hook function as soon as possible
		info.panic = true; // and call luaL_error once we're inside the hook function
		if(message)
		{
			strncpy(info.panicMessage, message, sizeof(info.panicMessage));
			info.panicMessage[sizeof(info.panicMessage)-1] = 0;
		}
		else
		{
			// attach file/line info because this is the case where it's most necessary to see that,
			// and often it won't be possible for the later luaL_error call to retrieve it otherwise.
			// this means sometimes printing multiple file/line numbers if luaL_error does find something,
			// but that's fine since more information is probably better anyway.
			luaL_where(L,0); // should be 0 and not 1 here to get useful (on force stop) messages
			const char* whereString = lua_tostring(L,-1);
			snprintf(info.panicMessage, sizeof(info.panicMessage), "%sscript terminated", whereString);
			lua_pop(L,1);
		}
	}
}

void SetSaveKey(LuaContextInfo& info, const char* key)
{
	info.dataSaveKey = crc32(0, (const unsigned char*)key, strlen(key));

	if(!info.dataSaveLoadKeySet)
	{
		info.dataLoadKey = info.dataSaveKey;
		info.dataSaveLoadKeySet = true;
	}
}
void SetLoadKey(LuaContextInfo& info, const char* key)
{
	info.dataLoadKey = crc32(0, (const unsigned char*)key, strlen(key));

	if(!info.dataSaveLoadKeySet)
	{
		info.dataSaveKey = info.dataLoadKey;
		info.dataSaveLoadKeySet = true;
	}
}

void HandleCallbackError(lua_State* L, LuaContextInfo& info, int uid, bool stopScript)
{
	info.crashed = true;
	if(L->errfunc || L->errorJmp)
		luaL_error(L, lua_tostring(L,-1));
	else
	{
		if(info.print)
		{
			info.print(uid, lua_tostring(L,-1));
			info.print(uid, "\r\n");
		}
		else
		{
			fprintf(stderr, "%s\n", lua_tostring(L,-1));
		}
		if(stopScript)
			StopLuaScript(uid);
	}
}

void CallExitFunction(int uid)
{
	LuaContextInfo& info = *luaContextInfo[uid];
	lua_State* L = info.L;

	if(!L)
		return;

	dontworry(info);

	// first call the registered exit function if there is one
	if(!info.ranExit)
	{
		info.ranExit = true;

#ifdef USE_INFO_STACK
		infoStack.insert(infoStack.begin(), &info);
		struct Scope { ~Scope(){ infoStack.erase(infoStack.begin()); } } scope;
#endif

		//lua_settop(L, 0);
		lua_getfield(L, LUA_REGISTRYINDEX, luaCallIDStrings[LUACALL_BEFOREEXIT]);
		
		int errorcode = 0;
		if (lua_isfunction(L, -1))
		{
			bool wasRunning = info.running;
			info.running = true;
			RefreshScriptSpeedStatus();

			bool wasPanic = info.panic;
			info.panic = false; // otherwise we could barely do anything in the exit function

			errorcode = lua_pcall(L, 0, 0, 0);

			info.panic |= wasPanic; // restore panic

			info.running = wasRunning;
			RefreshScriptSpeedStatus();
		}

		// save persisted variable info after the exit function runs (even if it crashed)
		{
			// gather the final value of the variables we're supposed to persist
			LuaSaveData newExitData;
			{
				int numPersistVars = info.persistVars.size();
				for(int i = 0; i < numPersistVars; i++)
				{
					const char* varName = info.persistVars[i].c_str();
					lua_getfield(L, LUA_GLOBALSINDEX, varName);
					int type = lua_type(L,-1);
					unsigned int varNameCRC = crc32(0, (const unsigned char*)varName, strlen(varName));
					newExitData.SaveRecordPartial(uid, varNameCRC, -1);
					lua_pop(L,1);
				}
			}

			char path [1024] = {0};
			char* pathTypeChrPtr = ConstructScriptSaveDataPath(path, 1024, info);

			*pathTypeChrPtr = 'd';
			if(info.newDefaultData.recordList)
			{
				FILE* defaultsFile = fopen(path, "wb");
				if(defaultsFile)
				{
					info.newDefaultData.ExportRecords(defaultsFile);
					fclose(defaultsFile);
				}
			}
			else unlink(path);

			*pathTypeChrPtr = 'e';
			if(newExitData.recordList)
			{
				FILE* persistFile = fopen(path, "wb");
				if(persistFile)
				{
					newExitData.ExportRecords(persistFile);
					fclose(persistFile);
				}
			}
			else unlink(path);
		}

		if (errorcode)
			HandleCallbackError(L,info,uid,false);

	}
}

void StopLuaScript(int uid)
{
	LuaContextInfo* infoPtr = luaContextInfo[uid];
	if(!infoPtr)
		return;

	LuaContextInfo& info = *infoPtr;

	if(info.running)
	{
		// if it's currently running then we can't stop it now without crashing
		// so the best we can do is politely request for it to go kill itself
		RequestAbortLuaScript(uid);
		return;
	}

	lua_State* L = info.L;
	if(L)
	{
		CallExitFunction(uid);

		if(info.onstop)
		{
			info.stopWorrying = true, info.worryCount++, dontworry(info); // clear "busy" status
			info.onstop(uid, !info.crashed); // must happen before closing L and after the exit function, otherwise the final GUI state of the script won't be shown properly or at all
		}

		if(info.started) // this check is necessary
		{
			lua_close(L);
#ifndef USE_INFO_STACK
			luaStateToContextMap.erase(L);
#endif
			luaStateToUIDMap.erase(L);
			info.L = NULL;
			info.started = false;
			
			info.numMemHooks = 0;
			for(int i = 0; i < LUAMEMHOOK_COUNT; i++)
				CalculateMemHookRegions((LuaMemHookType)i);
		}
		RefreshScriptStartedStatus();
	}
}

void CloseLuaContext(int uid)
{
	StopLuaScript(uid);
	delete luaContextInfo[uid];
	luaContextInfo.erase(uid);
}


TieredRegion hookedRegions [LUAMEMHOOK_COUNT];


// currently disabled for desmume,
// and the performance hit might not be accepable
// unless we can switch the emulation into a whole separate path
// that has this callback enabled.
static void CalculateMemHookRegions(LuaMemHookType hookType)
{
	std::vector<unsigned int> hookedBytes;
	std::map<int, LuaContextInfo*>::iterator iter = luaContextInfo.begin();
	std::map<int, LuaContextInfo*>::iterator end = luaContextInfo.end();
	while(iter != end)
	{
		LuaContextInfo& info = *iter->second;
		if(info.numMemHooks)
		{
			lua_State* L = info.L;
			if(L)
			{
				int top = lua_gettop(L);
				lua_getfield(L, LUA_REGISTRYINDEX, luaMemHookTypeStrings[hookType]);
				lua_pushnil(L);
				while(lua_next(L, -2))
				{
					if(lua_isfunction(L, -1))
					{
						unsigned int addr = lua_tointeger(L, -2);
						hookedBytes.push_back(addr);
					}
					lua_pop(L, 1);
				}
				if(!info.crashed)
					lua_settop(L, top);
			}
		}
		++iter;
	}
	hookedRegions[hookType].Calculate(hookedBytes);
}





void CallRegisteredLuaMemHook_LuaMatch(unsigned int address, int size, unsigned int value, LuaMemHookType hookType)
{
	std::map<int, LuaContextInfo*>::iterator iter = luaContextInfo.begin();
	std::map<int, LuaContextInfo*>::iterator end = luaContextInfo.end();
	while(iter != end)
	{
		LuaContextInfo& info = *iter->second;
		if(info.numMemHooks)
		{
			lua_State* L = info.L;
			if(L && !info.panic)
			{
#ifdef USE_INFO_STACK
				infoStack.insert(infoStack.begin(), &info);
				struct Scope { ~Scope(){ infoStack.erase(infoStack.begin()); } } scope;
#endif
				int top = lua_gettop(L);
				lua_getfield(L, LUA_REGISTRYINDEX, luaMemHookTypeStrings[hookType]);
				for(int i = address; i != address+size; i++)
				{
					lua_rawgeti(L, -1, i);
					if (lua_isfunction(L, -1))
					{
						bool wasRunning = info.running;
						info.running = true;
						RefreshScriptSpeedStatus();
						lua_pushinteger(L, address);
						lua_pushinteger(L, size);
						int errorcode = lua_pcall(L, 2, 0, 0);
						info.running = wasRunning;
						RefreshScriptSpeedStatus();
						if (errorcode)
						{
							int uid = iter->first;
							HandleCallbackError(L,info,uid,true);
						}
						break;
					}
					else
					{
						lua_pop(L,1);
					}
				}
				if(!info.crashed)
					lua_settop(L, top);
			}
		}
		++iter;
	}
}


bool AnyLuaActive()
{
	std::map<int, LuaContextInfo*>::iterator iter = luaContextInfo.begin();
	std::map<int, LuaContextInfo*>::iterator end = luaContextInfo.end();
	while(iter != end)
	{
		LuaContextInfo& info = *iter->second;
		if(info.started)
			return true;
		++iter;
	}
	return false;
}

void CallRegisteredLuaFunctions(LuaCallID calltype)
{
	assert((unsigned int)calltype < (unsigned int)LUACALL_COUNT);
	const char* idstring = luaCallIDStrings[calltype];

	std::map<int, LuaContextInfo*>::iterator iter = luaContextInfo.begin();
	std::map<int, LuaContextInfo*>::iterator end = luaContextInfo.end();
	while(iter != end)
	{
		int uid = iter->first;
		LuaContextInfo& info = *iter->second;
		lua_State* L = info.L;
		if(L && (!info.panic || calltype == LUACALL_BEFOREEXIT))
		{
#ifdef USE_INFO_STACK
			infoStack.insert(infoStack.begin(), &info);
			struct Scope { ~Scope(){ infoStack.erase(infoStack.begin()); } } scope;
#endif
			// handle deferred GUI function calls and disabling deferring when unnecessary
			if(calltype == LUACALL_AFTEREMULATIONGUI || calltype == LUACALL_AFTEREMULATION)
				info.guiFuncsNeedDeferring = false;
			if(calltype == LUACALL_AFTEREMULATIONGUI)
				CallDeferredFunctions(L, deferredGUIIDString);
			if(calltype == LUACALL_BEFOREEMULATION)
			{
				assert(NDS_isProcessingUserInput());
				CallDeferredFunctions(L, deferredJoySetIDString);
			}

			int top = lua_gettop(L);
			lua_getfield(L, LUA_REGISTRYINDEX, idstring);
			
			if (lua_isfunction(L, -1))
			{
				bool wasRunning = info.running;
				info.running = true;
				RefreshScriptSpeedStatus();
				int errorcode = lua_pcall(L, 0, 0, 0);
				info.running = wasRunning;
				RefreshScriptSpeedStatus();
				if (errorcode)
					HandleCallbackError(L,info,uid,true);
			}
			else
			{
				lua_pop(L, 1);
			}

			info.guiFuncsNeedDeferring = true;
			if(!info.crashed)
			{
				lua_settop(L, top);
				if(!info.panic)
					dontworry(info);
			}
		}

		++iter;
	}
}

void CallRegisteredLuaSaveFunctions(int savestateNumber, LuaSaveData& saveData)
{
	const char* idstring = luaCallIDStrings[LUACALL_BEFORESAVE];

	std::map<int, LuaContextInfo*>::iterator iter = luaContextInfo.begin();
	std::map<int, LuaContextInfo*>::iterator end = luaContextInfo.end();
	while(iter != end)
	{
		int uid = iter->first;
		LuaContextInfo& info = *iter->second;
		lua_State* L = info.L;
		if(L)
		{
#ifdef USE_INFO_STACK
			infoStack.insert(infoStack.begin(), &info);
			struct Scope { ~Scope(){ infoStack.erase(infoStack.begin()); } } scope;
#endif

			int top = lua_gettop(L);
			lua_getfield(L, LUA_REGISTRYINDEX, idstring);
			
			if (lua_isfunction(L, -1))
			{
				bool wasRunning = info.running;
				info.running = true;
				RefreshScriptSpeedStatus();
				lua_pushinteger(L, savestateNumber);
				int errorcode = lua_pcall(L, 1, LUA_MULTRET, 0);
				info.running = wasRunning;
				RefreshScriptSpeedStatus();
				if (errorcode)
					HandleCallbackError(L,info,uid,true);
				saveData.SaveRecord(uid, info.dataSaveKey);
			}
			else
			{
				lua_pop(L, 1);
			}
			if(!info.crashed)
				lua_settop(L, top);
		}

		++iter;
	}
}


void CallRegisteredLuaLoadFunctions(int savestateNumber, const LuaSaveData& saveData)
{
	const char* idstring = luaCallIDStrings[LUACALL_AFTERLOAD];

	std::map<int, LuaContextInfo*>::iterator iter = luaContextInfo.begin();
	std::map<int, LuaContextInfo*>::iterator end = luaContextInfo.end();
	while(iter != end)
	{
		int uid = iter->first;
		LuaContextInfo& info = *iter->second;
		lua_State* L = info.L;
		if(L)
		{
#ifdef USE_INFO_STACK
			infoStack.insert(infoStack.begin(), &info);
			struct Scope { ~Scope(){ infoStack.erase(infoStack.begin()); } } scope;
#endif

			int top = lua_gettop(L);
			lua_getfield(L, LUA_REGISTRYINDEX, idstring);
			
			if (lua_isfunction(L, -1))
			{
				bool wasRunning = info.running;
				info.running = true;
				RefreshScriptSpeedStatus();

				// since the scriptdata can be very expensive to load
				// (e.g. the registered save function returned some huge tables)
				// check the number of parameters the registered load function expects
				// and don't bother loading the parameters it wouldn't receive anyway
				int numParamsExpected = (L->top - 1)->value.gc->cl.l.p->numparams;
				if(numParamsExpected) numParamsExpected--; // minus one for the savestate number we always pass in

				int prevGarbage = lua_gc(L, LUA_GCCOUNT, 0);

				lua_pushinteger(L, savestateNumber);
				saveData.LoadRecord(uid, info.dataLoadKey, numParamsExpected);
				int n = lua_gettop(L) - 1;

				int errorcode = lua_pcall(L, n, 0, 0);
				info.running = wasRunning;
				RefreshScriptSpeedStatus();
				if (errorcode)
					HandleCallbackError(L,info,uid,true);
				else
				{
					int newGarbage = lua_gc(L, LUA_GCCOUNT, 0);
					if(newGarbage - prevGarbage > 50)
					{
						// now seems to be a very good time to run the garbage collector
						// it might take a while now but that's better than taking 10 whiles 9 loads from now
						lua_gc(L, LUA_GCCOLLECT, 0);
					}
				}
			}
			else
			{
				lua_pop(L, 1);
			}
			if(!info.crashed)
				lua_settop(L, top);
		}

		++iter;
	}
}

static const unsigned char* s_dbg_dataStart = NULL;
static int s_dbg_dataSize = 0;


// can't remember what the best way of doing this is...
#if defined(i386) || defined(__i386) || defined(__i386__) || defined(M_I86) || defined(_M_IX86) || defined(_WIN32)
	#define IS_LITTLE_ENDIAN
#endif

// push a value's bytes onto the output stack
template<typename T>
void PushBinaryItem(T item, std::vector<unsigned char>& output)
{
	unsigned char* buf = (unsigned char*)&item;
#ifdef IS_LITTLE_ENDIAN
	for(int i = sizeof(T); i; i--)
		output.push_back(*buf++);
#else
	int vecsize = output.size();
	for(int i = sizeof(T); i; i--)
		output.insert(output.begin() + vecsize, *buf++);
#endif
}
// read a value from the byte stream and advance the stream by its size
template<typename T>
T AdvanceByteStream(const unsigned char*& data, unsigned int& remaining)
{
#ifdef IS_LITTLE_ENDIAN
	T rv = *(T*)data;
	data += sizeof(T);
#else
	T rv; unsigned char* rvptr = (unsigned char*)&rv;
	for(int i = sizeof(T)-1; i>=0; i--)
		rvptr[i] = *data++;
#endif
	remaining -= sizeof(T);
	return rv;
}
// advance the byte stream by a certain size without reading a value
void AdvanceByteStream(const unsigned char*& data, unsigned int& remaining, int amount)
{
	data += amount;
	remaining -= amount;
}

#define LUAEXT_TLONG		30 // 0x1E // 4-byte signed integer
#define LUAEXT_TUSHORT		31 // 0x1F // 2-byte unsigned integer
#define LUAEXT_TSHORT		32 // 0x20 // 2-byte signed integer
#define LUAEXT_TBYTE		33 // 0x21 // 1-byte unsigned integer
#define LUAEXT_TNILS		34 // 0x22 // multiple nils represented by a 4-byte integer (warning: becomes multiple stack entities)
#define LUAEXT_TTABLE		0x40 // 0x40 through 0x4F // tables of different sizes:
#define LUAEXT_BITS_1A		0x01 // size of array part fits in a 1-byte unsigned integer
#define LUAEXT_BITS_2A		0x02 // size of array part fits in a 2-byte unsigned integer
#define LUAEXT_BITS_4A		0x03 // size of array part fits in a 4-byte unsigned integer
#define LUAEXT_BITS_1H		0x04 // size of hash part fits in a 1-byte unsigned integer
#define LUAEXT_BITS_2H		0x08 // size of hash part fits in a 2-byte unsigned integer
#define LUAEXT_BITS_4H		0x0C // size of hash part fits in a 4-byte unsigned integer
#define BITMATCH(x,y) (((x) & (y)) == (y))

static void PushNils(std::vector<unsigned char>& output, int& nilcount)
{
	int count = nilcount;
	nilcount = 0;

	static const int minNilsWorthEncoding = 6; // because a LUAEXT_TNILS entry is 5 bytes

	if(count < minNilsWorthEncoding)
	{
		for(int i = 0; i < count; i++)
			output.push_back(LUA_TNIL);
	}
	else
	{
		output.push_back(LUAEXT_TNILS);
		PushBinaryItem<u32>(count, output);
	}
}


static void LuaStackToBinaryConverter(lua_State* L, int i, std::vector<unsigned char>& output)
{
	int type = lua_type(L, i);

	// the first byte of every serialized item says what Lua type it is
	output.push_back(type & 0xFF);

	switch(type)
	{
		default:
			{
				//printf("wrote unknown type %d (0x%x)\n", type, type);	
				//assert(0);

				LuaContextInfo& info = GetCurrentInfo();
				if(info.print)
				{
					char errmsg [1024];
					sprintf(errmsg, "values of type \"%s\" are not allowed to be returned from registered save functions.\r\n", luaL_typename(L,i));
					info.print(luaStateToUIDMap[L->l_G->mainthread], errmsg);
				}
				else
				{
					fprintf(stderr, "values of type \"%s\" are not allowed to be returned from registered save functions.\n", luaL_typename(L,i));
				}
			}
			break;
		case LUA_TNIL:
			// no information necessary beyond the type
			break;
		case LUA_TBOOLEAN:
			// serialize as 0 or 1
			output.push_back(lua_toboolean(L,i));
			break;
		case LUA_TSTRING:
			// serialize as a 0-terminated string of characters
			{
				const char* str = lua_tostring(L,i);
				while(*str)
					output.push_back(*str++);
				output.push_back('\0');
			}
			break;
		case LUA_TNUMBER:
			{
				double num = (double)lua_tonumber(L,i);
				s32 inum = (s32)lua_tointeger(L,i);
				if(num != inum)
				{
					PushBinaryItem(num, output);
				}
				else
				{
					if((inum & ~0xFF) == 0)
						type = LUAEXT_TBYTE;
					else if((u16)(inum & 0xFFFF) == inum)
						type = LUAEXT_TUSHORT;
					else if((s16)(inum & 0xFFFF) == inum)
						type = LUAEXT_TSHORT;
					else
						type = LUAEXT_TLONG;
					output.back() = type;
					switch(type)
					{
					case LUAEXT_TLONG:
						PushBinaryItem<s32>(inum, output);
						break;
					case LUAEXT_TUSHORT:
						PushBinaryItem<u16>(inum, output);
						break;
					case LUAEXT_TSHORT:
						PushBinaryItem<s16>(inum, output);
						break;
					case LUAEXT_TBYTE:
						output.push_back(inum);
						break;
					}
				}
			}
			break;
		case LUA_TTABLE:
			// serialize as a type that describes how many bytes are used for storing the counts,
			// followed by the number of array entries if any, then the number of hash entries if any,
			// then a Lua value per array entry, then a (key,value) pair of Lua values per hashed entry
			// note that the structure of table references are not faithfully serialized (yet)
		{
			int outputTypeIndex = output.size() - 1;
			int arraySize = 0;
			int hashSize = 0;

			if(lua_checkstack(L, 4) && std::find(s_tableAddressStack.begin(), s_tableAddressStack.end(), lua_topointer(L,i)) == s_tableAddressStack.end())
			{
				s_tableAddressStack.push_back(lua_topointer(L,i));
				struct Scope { ~Scope(){ s_tableAddressStack.pop_back(); } } scope;

				bool wasnil = false;
				int nilcount = 0;
				arraySize = lua_objlen(L, i);
				int arrayValIndex = lua_gettop(L) + 1;
				for(int j = 1; j <= arraySize; j++)
				{
			        lua_rawgeti(L, i, j);
					bool isnil = lua_isnil(L, arrayValIndex);
					if(isnil)
						nilcount++;
					else
					{
						if(wasnil)
							PushNils(output, nilcount);
						LuaStackToBinaryConverter(L, arrayValIndex, output);
					}
					lua_pop(L, 1);
					wasnil = isnil;
				}
				if(wasnil)
					PushNils(output, nilcount);

				if(arraySize)
					lua_pushinteger(L, arraySize); // before first key
				else
					lua_pushnil(L); // before first key

				int keyIndex = lua_gettop(L);
				int valueIndex = keyIndex + 1;
				while(lua_next(L, i))
				{
					assert(lua_type(L, keyIndex) && "nil key in Lua table, impossible");
					assert(lua_type(L, valueIndex) && "nil value in Lua table, impossible");
					LuaStackToBinaryConverter(L, keyIndex, output);
					LuaStackToBinaryConverter(L, valueIndex, output);
					lua_pop(L, 1);
					hashSize++;
				}
			}

			int outputType = LUAEXT_TTABLE;
			if(arraySize & 0xFFFF0000)
				outputType |= LUAEXT_BITS_4A;
			else if(arraySize & 0xFF00)
				outputType |= LUAEXT_BITS_2A;
			else if(arraySize & 0xFF)
				outputType |= LUAEXT_BITS_1A;
			if(hashSize & 0xFFFF0000)
				outputType |= LUAEXT_BITS_4H;
			else if(hashSize & 0xFF00)
				outputType |= LUAEXT_BITS_2H;
			else if(hashSize & 0xFF)
				outputType |= LUAEXT_BITS_1H;
			output[outputTypeIndex] = outputType;

			int insertIndex = outputTypeIndex;
			if(BITMATCH(outputType,LUAEXT_BITS_4A) || BITMATCH(outputType,LUAEXT_BITS_2A) || BITMATCH(outputType,LUAEXT_BITS_1A))
				output.insert(output.begin() + (++insertIndex), arraySize & 0xFF);
			if(BITMATCH(outputType,LUAEXT_BITS_4A) || BITMATCH(outputType,LUAEXT_BITS_2A))
				output.insert(output.begin() + (++insertIndex), (arraySize & 0xFF00) >> 8);
			if(BITMATCH(outputType,LUAEXT_BITS_4A))
				output.insert(output.begin() + (++insertIndex), (arraySize & 0x00FF0000) >> 16),
				output.insert(output.begin() + (++insertIndex), (arraySize & 0xFF000000) >> 24);
			if(BITMATCH(outputType,LUAEXT_BITS_4H) || BITMATCH(outputType,LUAEXT_BITS_2H) || BITMATCH(outputType,LUAEXT_BITS_1H))
				output.insert(output.begin() + (++insertIndex), hashSize & 0xFF);
			if(BITMATCH(outputType,LUAEXT_BITS_4H) || BITMATCH(outputType,LUAEXT_BITS_2H))
				output.insert(output.begin() + (++insertIndex), (hashSize & 0xFF00) >> 8);
			if(BITMATCH(outputType,LUAEXT_BITS_4H))
				output.insert(output.begin() + (++insertIndex), (hashSize & 0x00FF0000) >> 16),
				output.insert(output.begin() + (++insertIndex), (hashSize & 0xFF000000) >> 24);

		}	break;
	}
}


// complements LuaStackToBinaryConverter
void BinaryToLuaStackConverter(lua_State* L, const unsigned char*& data, unsigned int& remaining)
{
	assert(s_dbg_dataSize - (data - s_dbg_dataStart) == remaining);

	unsigned char type = AdvanceByteStream<unsigned char>(data, remaining);

	switch(type)
	{
		default:
			{
				//printf("read unknown type %d (0x%x)\n", type, type);
				//assert(0);

				LuaContextInfo& info = GetCurrentInfo();
				if(info.print)
				{
					char errmsg [1024];
					if(type <= 10 && type != LUA_TTABLE)
						sprintf(errmsg, "values of type \"%s\" are not allowed to be loaded into registered load functions. The save state's Lua save data file might be corrupted.\r\n", lua_typename(L,type));
					else
						sprintf(errmsg, "The save state's Lua save data file seems to be corrupted.\r\n");
					info.print(luaStateToUIDMap[L->l_G->mainthread], errmsg);
				}
				else
				{
					if(type <= 10 && type != LUA_TTABLE)
						fprintf(stderr, "values of type \"%s\" are not allowed to be loaded into registered load functions. The save state's Lua save data file might be corrupted.\n", lua_typename(L,type));
					else
						fprintf(stderr, "The save state's Lua save data file seems to be corrupted.\n");
				}
			}
			break;
		case LUA_TNIL:
			lua_pushnil(L);
			break;
		case LUA_TBOOLEAN:
			lua_pushboolean(L, AdvanceByteStream<u8>(data, remaining));
			break;
		case LUA_TSTRING:
			lua_pushstring(L, (const char*)data);
			AdvanceByteStream(data, remaining, strlen((const char*)data) + 1);
			break;
		case LUA_TNUMBER:
			lua_pushnumber(L, AdvanceByteStream<double>(data, remaining));
			break;
		case LUAEXT_TLONG:
			lua_pushinteger(L, AdvanceByteStream<s32>(data, remaining));
			break;
		case LUAEXT_TUSHORT:
			lua_pushinteger(L, AdvanceByteStream<u16>(data, remaining));
			break;
		case LUAEXT_TSHORT:
			lua_pushinteger(L, AdvanceByteStream<s16>(data, remaining));
			break;
		case LUAEXT_TBYTE:
			lua_pushinteger(L, AdvanceByteStream<u8>(data, remaining));
			break;
		case LUAEXT_TTABLE:
		case LUAEXT_TTABLE | LUAEXT_BITS_1A:
		case LUAEXT_TTABLE | LUAEXT_BITS_2A:
		case LUAEXT_TTABLE | LUAEXT_BITS_4A:
		case LUAEXT_TTABLE | LUAEXT_BITS_1H:
		case LUAEXT_TTABLE | LUAEXT_BITS_2H:
		case LUAEXT_TTABLE | LUAEXT_BITS_4H:
		case LUAEXT_TTABLE | LUAEXT_BITS_1A | LUAEXT_BITS_1H:
		case LUAEXT_TTABLE | LUAEXT_BITS_2A | LUAEXT_BITS_1H:
		case LUAEXT_TTABLE | LUAEXT_BITS_4A | LUAEXT_BITS_1H:
		case LUAEXT_TTABLE | LUAEXT_BITS_1A | LUAEXT_BITS_2H:
		case LUAEXT_TTABLE | LUAEXT_BITS_2A | LUAEXT_BITS_2H:
		case LUAEXT_TTABLE | LUAEXT_BITS_4A | LUAEXT_BITS_2H:
		case LUAEXT_TTABLE | LUAEXT_BITS_1A | LUAEXT_BITS_4H:
		case LUAEXT_TTABLE | LUAEXT_BITS_2A | LUAEXT_BITS_4H:
		case LUAEXT_TTABLE | LUAEXT_BITS_4A | LUAEXT_BITS_4H:
			{
				unsigned int arraySize = 0;
				if(BITMATCH(type,LUAEXT_BITS_4A) || BITMATCH(type,LUAEXT_BITS_2A) || BITMATCH(type,LUAEXT_BITS_1A))
					arraySize |= AdvanceByteStream<u8>(data, remaining);
				if(BITMATCH(type,LUAEXT_BITS_4A) || BITMATCH(type,LUAEXT_BITS_2A))
					arraySize |= ((u16)AdvanceByteStream<u8>(data, remaining)) << 8;
				if(BITMATCH(type,LUAEXT_BITS_4A))
					arraySize |= ((u32)AdvanceByteStream<u8>(data, remaining)) << 16,
					arraySize |= ((u32)AdvanceByteStream<u8>(data, remaining)) << 24;

				unsigned int hashSize = 0;
				if(BITMATCH(type,LUAEXT_BITS_4H) || BITMATCH(type,LUAEXT_BITS_2H) || BITMATCH(type,LUAEXT_BITS_1H))
					hashSize |= AdvanceByteStream<u8>(data, remaining);
				if(BITMATCH(type,LUAEXT_BITS_4H) || BITMATCH(type,LUAEXT_BITS_2H))
					hashSize |= ((u16)AdvanceByteStream<u8>(data, remaining)) << 8;
				if(BITMATCH(type,LUAEXT_BITS_4H))
					hashSize |= ((u32)AdvanceByteStream<u8>(data, remaining)) << 16,
					hashSize |= ((u32)AdvanceByteStream<u8>(data, remaining)) << 24;

				lua_createtable(L, arraySize, hashSize);

				unsigned int n = 1;
				while(n <= arraySize)
				{
					if(*data == LUAEXT_TNILS)
					{
						AdvanceByteStream(data, remaining, 1);
						n += AdvanceByteStream<u32>(data, remaining);
					}
					else
					{
						BinaryToLuaStackConverter(L, data, remaining); // push value
						lua_rawseti(L, -2, n); // table[n] = value
						n++;
					}
				}

				for(unsigned int h = 1; h <= hashSize; h++)
				{
					BinaryToLuaStackConverter(L, data, remaining); // push key
					BinaryToLuaStackConverter(L, data, remaining); // push value
					lua_rawset(L, -3); // table[key] = value
				}
			}
			break;
	}
}

static const unsigned char luaBinaryMajorVersion = 9;
static const unsigned char luaBinaryMinorVersion = 1;

unsigned char* LuaStackToBinary(lua_State* L, unsigned int& size)
{
	int n = lua_gettop(L);
	if(n == 0)
		return NULL;

	std::vector<unsigned char> output;
	output.push_back(luaBinaryMajorVersion);
	output.push_back(luaBinaryMinorVersion);

	for(int i = 1; i <= n; i++)
		LuaStackToBinaryConverter(L, i, output);

	unsigned char* rv = new unsigned char [output.size()];
	memcpy(rv, &output.front(), output.size());
	size = output.size();
	return rv;
}

void BinaryToLuaStack(lua_State* L, const unsigned char* data, unsigned int size, unsigned int itemsToLoad)
{
	unsigned char major = *data++;
	unsigned char minor = *data++;
	size -= 2;
	if(luaBinaryMajorVersion != major || luaBinaryMinorVersion != minor)
		return;

	while(size > 0 && itemsToLoad > 0)
	{
		BinaryToLuaStackConverter(L, data, size);
		itemsToLoad--;
	}
}

// saves Lua stack into a record and pops it
void LuaSaveData::SaveRecord(int uid, unsigned int key)
{
	LuaContextInfo& info = *luaContextInfo[uid];
	lua_State* L = info.L;
	if(!L)
		return;

	Record* cur = new Record();
	cur->key = key;
	cur->data = LuaStackToBinary(L, cur->size);
	cur->next = NULL;

	lua_settop(L,0);

	if(cur->size <= 0)
	{
		delete cur;
		return;
	}

	Record* last = recordList;
	while(last && last->next)
		last = last->next;
	if(last)
		last->next = cur;
	else
		recordList = cur;
}

// pushes a record's data onto the Lua stack
void LuaSaveData::LoadRecord(int uid, unsigned int key, unsigned int itemsToLoad) const
{
	LuaContextInfo& info = *luaContextInfo[uid];
	lua_State* L = info.L;
	if(!L)
		return;

	Record* cur = recordList;
	while(cur)
	{
		if(cur->key == key)
		{
			s_dbg_dataStart = cur->data;
			s_dbg_dataSize = cur->size;
			BinaryToLuaStack(L, cur->data, cur->size, itemsToLoad);
			return;
		}
		cur = cur->next;
	}
}

// saves part of the Lua stack (at the given index) into a record and does NOT pop anything
void LuaSaveData::SaveRecordPartial(int uid, unsigned int key, int idx)
{
	LuaContextInfo& info = *luaContextInfo[uid];
	lua_State* L = info.L;
	if(!L)
		return;

	if(idx < 0)
		idx += lua_gettop(L)+1;

	Record* cur = new Record();
	cur->key = key;
	cur->next = NULL;

	if(idx <= lua_gettop(L))
	{
		std::vector<unsigned char> output;
		output.push_back(luaBinaryMajorVersion);
		output.push_back(luaBinaryMinorVersion);

		LuaStackToBinaryConverter(L, idx, output);

		unsigned char* rv = new unsigned char [output.size()];
		memcpy(rv, &output.front(), output.size());
		cur->size = output.size();
		cur->data = rv;
	}

	if(cur->size <= 0)
	{
		delete cur;
		return;
	}

	Record* last = recordList;
	while(last && last->next)
		last = last->next;
	if(last)
		last->next = cur;
	else
		recordList = cur;
}

void fwriteint(unsigned int value, FILE* file)
{
	for(int i=0;i<4;i++)
	{
		int w = value & 0xFF;
		fwrite(&w, 1, 1, file);
		value >>= 8;
	}
}
void freadint(unsigned int& value, FILE* file)
{
	int rv = 0;
	for(int i=0;i<4;i++)
	{
		int r = 0;
		fread(&r, 1, 1, file);
		rv |= r << (i*8);
	}
	value = rv;
}

// writes all records to an already-open file
void LuaSaveData::ExportRecords(void* fileV) const
{
	FILE* file = (FILE*)fileV;
	if(!file)
		return;

	Record* cur = recordList;
	while(cur)
	{
		fwriteint(cur->key, file);
		fwriteint(cur->size, file);
		fwrite(cur->data, cur->size, 1, file);
		cur = cur->next;
	}
}

// reads records from an already-open file
void LuaSaveData::ImportRecords(void* fileV)
{
	FILE* file = (FILE*)fileV;
	if(!file)
		return;

	ClearRecords();

	Record rec;
	Record* cur = &rec;
	Record* last = NULL;
	while(1)
	{
		freadint(cur->key, file);
		freadint(cur->size, file);

		if(feof(file) || ferror(file))
			break;

		cur->data = new unsigned char [cur->size];
		fread(cur->data, cur->size, 1, file);

		Record* next = new Record();
		memcpy(next, cur, sizeof(Record));
		next->next = NULL;

		if(last)
			last->next = next;
		else
			recordList = next;
		last = next;
	}
}

void LuaSaveData::ClearRecords()
{
	Record* cur = recordList;
	while(cur)
	{
		Record* del = cur;
		cur = cur->next;

		delete[] del->data;
		delete del;
	}

	recordList = NULL;
}



void DontWorryLua() // everything's going to be OK
{
	std::map<int, LuaContextInfo*>::const_iterator iter = luaContextInfo.begin();
	std::map<int, LuaContextInfo*>::const_iterator end = luaContextInfo.end();
	while(iter != end)
	{
		dontworry(*iter->second);
		++iter;
	}
}

void EnableStopAllLuaScripts(bool enable)
{
	g_stopAllScriptsEnabled = enable;
}

void StopAllLuaScripts()
{
	if(!g_stopAllScriptsEnabled)
		return;

	std::map<int, LuaContextInfo*>::const_iterator iter = luaContextInfo.begin();
	std::map<int, LuaContextInfo*>::const_iterator end = luaContextInfo.end();
	while(iter != end)
	{
		int uid = iter->first;
		LuaContextInfo& info = *iter->second;
		bool wasStarted = info.started;
		StopLuaScript(uid);
		info.restartLater = wasStarted;
		++iter;
	}
}

void RestartAllLuaScripts()
{
	if(!g_stopAllScriptsEnabled)
		return;

	std::map<int, LuaContextInfo*>::const_iterator iter = luaContextInfo.begin();
	std::map<int, LuaContextInfo*>::const_iterator end = luaContextInfo.end();
	while(iter != end)
	{
		int uid = iter->first;
		LuaContextInfo& info = *iter->second;
		if(info.restartLater || info.started)
		{
			info.restartLater = false;
			RunLuaScriptFile(uid, info.lastFilename.c_str());
		}
		++iter;
	}
}

// sets anything that needs to depend on the total number of scripts running
void RefreshScriptStartedStatus()
{
	int numScriptsStarted = 0;

	std::map<int, LuaContextInfo*>::const_iterator iter = luaContextInfo.begin();
	std::map<int, LuaContextInfo*>::const_iterator end = luaContextInfo.end();
	while(iter != end)
	{
		LuaContextInfo& info = *iter->second;
		if(info.started)
			numScriptsStarted++;
		++iter;
	}

//	frameadvSkipLagForceDisable = (numScriptsStarted != 0); // disable while scripts are running because currently lag skipping makes lua callbacks get called twice per frame advance
	g_numScriptsStarted = numScriptsStarted;
}

// sets anything that needs to depend on speed mode or running status of scripts
void RefreshScriptSpeedStatus()
{
	g_anyScriptsHighSpeed = false;

	std::map<int, LuaContextInfo*>::const_iterator iter = luaContextInfo.begin();
	std::map<int, LuaContextInfo*>::const_iterator end = luaContextInfo.end();
	while(iter != end)
	{
		LuaContextInfo& info = *iter->second;
		if(info.running)
			if(info.speedMode == SPEEDMODE_TURBO || info.speedMode == SPEEDMODE_MAXIMUM)
				g_anyScriptsHighSpeed = true;
		++iter;
	}
}

