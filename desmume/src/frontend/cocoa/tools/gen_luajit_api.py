#!/usr/bin/env python3
"""Regenerate LuaJITApiSymbols.h from the Lua entry points lua-engine.cpp uses.

The list is obtained by compiling lua-engine.cpp with DESMUME_LUAJIT_NO_MACROS
so that *no* name is remapped to the pointer table, and then reading the
undefined lua* symbols out of the object file.  Compiling with the macros
enabled would only ever report the symbols the table is still missing, which is
how three entry points were silently dropped once already.

Note that every remaining undefined lua* symbol is a build failure by design:
this script plus the generated header must cover all of them.

Only macOS needs this.  The Windows build links lua51.lib and lets the loader
resolve lua51.dll through DelayLoadDLLs, which is the same idea done by the
toolchain instead of by hand.

Usage:  python3 gen_luajit_api.py
"""

import os
import re
import subprocess
import sys

SRC_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
OBJ_PATH = '/tmp/lua-engine.nomacros.o'
OUT_PATH = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'LuaJITApiSymbols.h'))

CXXFLAGS = [
    'clang++', '-std=c++11', '-c', '-w',
    '-DHAVE_LUA=1',
    '-DDESMUME_LUA_DYNAMIC=1',
    '-DDESMUME_LUAJIT_NO_MACROS=1',
    '-DDESMUME_COCOA=1',
    '-DDESMUME_COCOA_METAL_OSD=1',
    '-I' + SRC_DIR,
    '-I' + os.path.join(SRC_DIR, 'frontend', 'cocoa'),
    '-I' + os.path.join(SRC_DIR, 'libretro-common', 'include'),
    '-I' + os.path.join(SRC_DIR, 'frontend', 'windows', 'lua', 'include'),
    '-I' + os.path.join(SRC_DIR, 'frontend', 'cocoa', 'freetype', 'include'),
    os.path.join(SRC_DIR, 'lua-engine.cpp'),
    '-o', OBJ_PATH,
]

HEADER = '''/*
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
#define DESMUME_LUAJIT_SYMBOLS(_) \\
'''


def collect_symbols():
    """Compile lua-engine.cpp unremapped and return its undefined lua* symbols."""
    result = subprocess.run(CXXFLAGS, capture_output=True, text=True)
    if result.returncode != 0:
        sys.stderr.write(result.stdout + result.stderr)
        sys.exit('failed to compile lua-engine.cpp')

    nm = subprocess.run(['nm', '-u', OBJ_PATH], capture_output=True, text=True, check=True)
    found = set()
    for line in nm.stdout.splitlines():
        # Undefined entries look like "                 _lua_pushnumber".
        match = re.search(r'\b_((?:lua|luaL|luaJIT)[A-Za-z0-9_]*)$', line.strip())
        if match is None:
            continue
        name = match.group(1)
        # g_luaJITApi is our own table, not something LuaJIT exports.
        if name == 'luaJITApi':
            continue
        found.add(name)

    if not found:
        sys.exit('no lua* symbols found - did the compile flags drift?')
    return sorted(found)


def main():
    syms = collect_symbols()

    out = [HEADER]
    for sym in syms:
        out.append('\t_(%s) \\\n' % sym)
    out.append('\n')
    out.append('''typedef struct LuaJITApi
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
''')
    for sym in syms:
        out.append('#define %s\t(g_luaJITApi.L_%s)\n' % (sym, sym))
    out.append('''
#endif // DESMUME_LUAJIT_NO_MACROS
#endif // DESMUME_LUA_DYNAMIC
#endif // DESMUME_LUAJIT_SYMBOLS_H
''')

    with open(OUT_PATH, 'w') as handle:
        handle.write(''.join(out))
    print('wrote %s (%d symbols)' % (OUT_PATH, len(syms)))


if __name__ == '__main__':
    main()
