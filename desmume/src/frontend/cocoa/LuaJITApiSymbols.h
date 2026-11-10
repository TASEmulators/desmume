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

// GENERATED FILE - edit tools/gen_luajit_api.py instead.
//
// The Lua entry points lua-engine.cpp calls, as a table of function pointers
// filled in by dlopen()/dlsym() at runtime.  This is the macOS equivalent of
// the Windows build's lua51.lib + DelayLoadDLLs=lua51.dll: nothing links
// against LuaJIT at build time, and if the dylib is missing the emulator still
// starts and simply reports that scripting is unavailable.
//
// Include this *after* the Lua headers (or just include it, it pulls them in)
// and before any code that calls lua* functions.  Do not include it from a
// translation unit that only needs the loader - use LuaJITApi.h for that.

#ifndef DESMUME_LUAJIT_SYMBOLS_H
#define DESMUME_LUAJIT_SYMBOLS_H

#ifdef DESMUME_LUA_DYNAMIC

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

// One entry per symbol.  Keeping the list in a single macro keeps the struct,
// the resolver and the documentation from drifting apart.
#define DESMUME_LUAJIT_SYMBOLS(_) \
	_(luaL_callmeta) \
	_(luaL_checkinteger) \
	_(luaL_checklstring) \
	_(luaL_checknumber) \
	_(luaL_checkstack) \
	_(luaL_checktype) \
	_(luaL_checkudata) \
	_(luaL_error) \
	_(luaL_loadfile) \
	_(luaL_newmetatable) \
	_(luaL_newstate) \
	_(luaL_openlibs) \
	_(luaL_optinteger) \
	_(luaL_optlstring) \
	_(luaL_ref) \
	_(luaL_register) \
	_(luaL_typerror) \
	_(luaL_unref) \
	_(luaL_where) \
	_(lua_call) \
	_(lua_checkstack) \
	_(lua_close) \
	_(lua_concat) \
	_(lua_createtable) \
	_(lua_gc) \
	_(lua_getfield) \
	_(lua_getinfo) \
	_(lua_getmetatable) \
	_(lua_getstack) \
	_(lua_gettop) \
	_(lua_insert) \
	_(lua_isnumber) \
	_(lua_isstring) \
	_(lua_newuserdata) \
	_(lua_next) \
	_(lua_objlen) \
	_(lua_pcall) \
	_(lua_pushboolean) \
	_(lua_pushcclosure) \
	_(lua_pushinteger) \
	_(lua_pushlstring) \
	_(lua_pushnil) \
	_(lua_pushnumber) \
	_(lua_pushstring) \
	_(lua_pushvalue) \
	_(lua_rawequal) \
	_(lua_rawgeti) \
	_(lua_rawset) \
	_(lua_rawseti) \
	_(lua_remove) \
	_(lua_replace) \
	_(lua_setfield) \
	_(lua_sethook) \
	_(lua_setmetatable) \
	_(lua_settable) \
	_(lua_settop) \
	_(lua_toboolean) \
	_(lua_tocfunction) \
	_(lua_tointeger) \
	_(lua_tolstring) \
	_(lua_tonumber) \
	_(lua_topointer) \
	_(lua_type) \
	_(lua_typename) \

typedef struct LuaJITApi
{
#define DESMUME_LUAJIT_FIELD(name)	__typeof__(&name) L_##name;
	DESMUME_LUAJIT_SYMBOLS(DESMUME_LUAJIT_FIELD)
#undef DESMUME_LUAJIT_FIELD
} LuaJITApi;

// Zeroed until LuaJIT_LoadLibrary() succeeds.  Reading a field before that is
// a null call, so every entry point in lua-engine.cpp is only reachable once
// the engine has been started.
extern LuaJITApi g_luaJITApi;

#ifndef DESMUME_LUAJIT_NO_MACROS

// Object-like macros, so lua_call(x) becomes (g_luaJITApi.L_lua_call)(x) and
// &lua_call still works.  They must not be defined while lua.h is being
// parsed, which is why this header is included after it.
#define luaL_callmeta	(g_luaJITApi.L_luaL_callmeta)
#define luaL_checkinteger	(g_luaJITApi.L_luaL_checkinteger)
#define luaL_checklstring	(g_luaJITApi.L_luaL_checklstring)
#define luaL_checknumber	(g_luaJITApi.L_luaL_checknumber)
#define luaL_checkstack	(g_luaJITApi.L_luaL_checkstack)
#define luaL_checktype	(g_luaJITApi.L_luaL_checktype)
#define luaL_checkudata	(g_luaJITApi.L_luaL_checkudata)
#define luaL_error	(g_luaJITApi.L_luaL_error)
#define luaL_loadfile	(g_luaJITApi.L_luaL_loadfile)
#define luaL_newmetatable	(g_luaJITApi.L_luaL_newmetatable)
#define luaL_newstate	(g_luaJITApi.L_luaL_newstate)
#define luaL_openlibs	(g_luaJITApi.L_luaL_openlibs)
#define luaL_optinteger	(g_luaJITApi.L_luaL_optinteger)
#define luaL_optlstring	(g_luaJITApi.L_luaL_optlstring)
#define luaL_ref	(g_luaJITApi.L_luaL_ref)
#define luaL_register	(g_luaJITApi.L_luaL_register)
#define luaL_typerror	(g_luaJITApi.L_luaL_typerror)
#define luaL_unref	(g_luaJITApi.L_luaL_unref)
#define luaL_where	(g_luaJITApi.L_luaL_where)
#define lua_call	(g_luaJITApi.L_lua_call)
#define lua_checkstack	(g_luaJITApi.L_lua_checkstack)
#define lua_close	(g_luaJITApi.L_lua_close)
#define lua_concat	(g_luaJITApi.L_lua_concat)
#define lua_createtable	(g_luaJITApi.L_lua_createtable)
#define lua_gc	(g_luaJITApi.L_lua_gc)
#define lua_getfield	(g_luaJITApi.L_lua_getfield)
#define lua_getinfo	(g_luaJITApi.L_lua_getinfo)
#define lua_getmetatable	(g_luaJITApi.L_lua_getmetatable)
#define lua_getstack	(g_luaJITApi.L_lua_getstack)
#define lua_gettop	(g_luaJITApi.L_lua_gettop)
#define lua_insert	(g_luaJITApi.L_lua_insert)
#define lua_isnumber	(g_luaJITApi.L_lua_isnumber)
#define lua_isstring	(g_luaJITApi.L_lua_isstring)
#define lua_newuserdata	(g_luaJITApi.L_lua_newuserdata)
#define lua_next	(g_luaJITApi.L_lua_next)
#define lua_objlen	(g_luaJITApi.L_lua_objlen)
#define lua_pcall	(g_luaJITApi.L_lua_pcall)
#define lua_pushboolean	(g_luaJITApi.L_lua_pushboolean)
#define lua_pushcclosure	(g_luaJITApi.L_lua_pushcclosure)
#define lua_pushinteger	(g_luaJITApi.L_lua_pushinteger)
#define lua_pushlstring	(g_luaJITApi.L_lua_pushlstring)
#define lua_pushnil	(g_luaJITApi.L_lua_pushnil)
#define lua_pushnumber	(g_luaJITApi.L_lua_pushnumber)
#define lua_pushstring	(g_luaJITApi.L_lua_pushstring)
#define lua_pushvalue	(g_luaJITApi.L_lua_pushvalue)
#define lua_rawequal	(g_luaJITApi.L_lua_rawequal)
#define lua_rawgeti	(g_luaJITApi.L_lua_rawgeti)
#define lua_rawset	(g_luaJITApi.L_lua_rawset)
#define lua_rawseti	(g_luaJITApi.L_lua_rawseti)
#define lua_remove	(g_luaJITApi.L_lua_remove)
#define lua_replace	(g_luaJITApi.L_lua_replace)
#define lua_setfield	(g_luaJITApi.L_lua_setfield)
#define lua_sethook	(g_luaJITApi.L_lua_sethook)
#define lua_setmetatable	(g_luaJITApi.L_lua_setmetatable)
#define lua_settable	(g_luaJITApi.L_lua_settable)
#define lua_settop	(g_luaJITApi.L_lua_settop)
#define lua_toboolean	(g_luaJITApi.L_lua_toboolean)
#define lua_tocfunction	(g_luaJITApi.L_lua_tocfunction)
#define lua_tointeger	(g_luaJITApi.L_lua_tointeger)
#define lua_tolstring	(g_luaJITApi.L_lua_tolstring)
#define lua_tonumber	(g_luaJITApi.L_lua_tonumber)
#define lua_topointer	(g_luaJITApi.L_lua_topointer)
#define lua_type	(g_luaJITApi.L_lua_type)
#define lua_typename	(g_luaJITApi.L_lua_typename)

#endif // DESMUME_LUAJIT_NO_MACROS
#endif // DESMUME_LUA_DYNAMIC
#endif // DESMUME_LUAJIT_SYMBOLS_H
