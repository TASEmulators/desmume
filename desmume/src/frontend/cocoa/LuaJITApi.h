/*
	Copyright (C) 2025 DeSmuME team

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

#ifndef DESMUME_LUAJIT_API_H
#define DESMUME_LUAJIT_API_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Runtime loader for LuaJIT, the macOS counterpart of the Windows build's
// lua51.lib + DelayLoadDLLs=lua51.dll.
//
// Nothing here mentions lua_State or any other Lua type on purpose: this
// header is safe to include from frontend code that does not want the Lua
// entry points macro-substituted (see LuaJITApiSymbols.h for those).  The
// dylib is dlopen()ed on demand and every symbol lua-engine.cpp needs is
// resolved by name; if any of that fails the emulator keeps running with
// scripting disabled instead of failing to launch.
//
// Search order:
//   1. $DESMUME_LUAJIT_DYLIB, if set (a full path, or a bare library name)
//   2. <bundle>/Contents/Frameworks/libluajit-5.1.dylib  (bundled copy)
//   3. the usual package manager locations (Homebrew, /usr/local, MacPorts)
//   4. a bare "libluajit-5.1.dylib", letting dyld's search paths decide
//
// LoadLibrary() is idempotent and thread safe.

// Loads and resolves everything.  Returns true if the library is ready to be
// called into; on failure LuaJIT_LoadError() explains what went wrong.
bool LuaJIT_LoadLibrary(void);

// True once LuaJIT_LoadLibrary() has succeeded.
bool LuaJIT_IsLoaded(void);

// Path the library was actually loaded from, or "" when it is not loaded.
const char *LuaJIT_LibraryPath(void);

// Human readable reason the last load attempt failed, or "" if none has.
const char *LuaJIT_LoadError(void);

// Releases the library.  Only for tests and shutdown; calling into Lua
// afterwards crashes, and the resolved function pointers are left dangling
// unless the process is about to exit.
void LuaJIT_UnloadLibrary(void);

#ifdef __cplusplus
}
#endif

#endif // DESMUME_LUAJIT_API_H
