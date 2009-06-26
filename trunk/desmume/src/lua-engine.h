#ifndef _S9XLUA_H
#define _S9XLUA_H


// Just forward function declarations 

//void LUA_LuaWrite(uint32 addr);
void LUA_LuaFrameBoundary();
int LUA_LoadLuaCode(const char *filename);
void LUA_ReloadLuaCode();
void LUA_LuaStop();
int LUA_LuaRunning();

int LUA_LuaUsingJoypad(int);
uint8 LUA_LuaReadJoypad(int);
int LUA_LuaSpeed();
int LUA_LuaFrameskip();
int LUA_LuaRerecordCountSkip();

void LUA_LuaGui(uint8 *XBuf);
void LUA_LuaUpdatePalette();

// And some interesting REVERSE declarations!
char *LUA_GetFreezeFilename(int slot);

// Call this before writing into a buffer passed to LUA_CheatAddRAM().
// (That way, Lua-based memwatch will work as expected for somebody
// used to LUA's memwatch.)
void LUA_LuaWriteInform();


#endif
