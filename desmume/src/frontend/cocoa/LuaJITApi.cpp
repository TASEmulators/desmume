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

#include "LuaJITApi.h"

#ifdef DESMUME_LUA_DYNAMIC

#include <dlfcn.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mach-o/dyld.h>

#include <string>
#include <vector>

// The struct and the symbol list, but not the lua* macros: this file talks
// about lua_call as a name to resolve, not as something to call.
#define DESMUME_LUAJIT_NO_MACROS 1
#include "LuaJITApiSymbols.h"

LuaJITApi g_luaJITApi;

static void *sLuaJITHandle = NULL;
static std::string sLoadedPath;
static std::string sLastError;
static pthread_once_t sLoadOnce = PTHREAD_ONCE_INIT;

// Candidate dylib file names.  Homebrew ships libluajit-5.1.dylib, some builds
// only provide the unversioned libluajit.dylib, and LuaJIT's own Makefile
// installs libluajit-5.1.2.dylib.
static const char * const kLibraryNames[] =
{
	"libluajit-5.1.dylib",
	"libluajit.dylib",
	"libluajit-5.1.2.dylib",
};

static std::string ExecutableDirectory(void)
{
	uint32_t size = 0;
	_NSGetExecutablePath(NULL, &size);
	if (size == 0)
		return std::string();

	std::vector<char> buffer(size + 1, '\0');
	if (_NSGetExecutablePath(&buffer[0], &size) != 0)
		return std::string();

	char resolved[PATH_MAX] = "";
	if (realpath(&buffer[0], resolved) == NULL)
		return std::string();

	std::string path(resolved);
	const size_t slash = path.rfind('/');
	if (slash == std::string::npos)
		return std::string();
	return path.substr(0, slash);
}

static std::string JoinPath(const std::string &dir, const char *name)
{
	if (dir.empty())
		return std::string(name);
	std::string out(dir);
	if (out[out.size() - 1] != '/')
		out += '/';
	out += name;
	return out;
}

// Directories to try, in order.  Empty entries mean "no directory", i.e. pass
// the bare library name to dlopen() and let dyld search.
static std::vector<std::string> CandidateDirectories(void)
{
	std::vector<std::string> dirs;

	const std::string exeDir = ExecutableDirectory();
	if (!exeDir.empty())
		dirs.push_back(exeDir + "/../Frameworks");

	dirs.push_back("/opt/homebrew/opt/luajit/lib");
	dirs.push_back("/opt/homebrew/lib");
	dirs.push_back("/usr/local/opt/luajit/lib");
	dirs.push_back("/usr/local/lib");
	dirs.push_back("/opt/local/lib");

	dirs.push_back("");
	return dirs;
}

static bool ResolveSymbols(void)
{
	const char *missing = NULL;

#define DESMUME_LUAJIT_RESOLVE(name) \
	do \
	{ \
		g_luaJITApi.L_##name = (__typeof__(g_luaJITApi.L_##name))dlsym(sLuaJITHandle, #name); \
		if (g_luaJITApi.L_##name == NULL && missing == NULL) \
			missing = #name; \
	} while (0);
	DESMUME_LUAJIT_SYMBOLS(DESMUME_LUAJIT_RESOLVE)
#undef DESMUME_LUAJIT_RESOLVE

	if (missing != NULL)
	{
		// A truncated or unrelated library: report the first symbol we could
		// not find, it is almost always enough to tell what went wrong.
		char message[256];
		snprintf(message, sizeof(message), "%s does not export %s", sLoadedPath.c_str(), missing);
		sLastError = message;
		return false;
	}

	return true;
}

static bool TryLoad(const std::string &path)
{
	void *handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
	if (handle == NULL)
	{
		const char *why = dlerror();
		sLastError = path + ": " + (why ? why : "dlopen failed");
		return false;
	}

	sLuaJITHandle = handle;
	sLoadedPath = path;
	sLastError.clear();

	if (!ResolveSymbols())
	{
		dlclose(handle);
		sLuaJITHandle = NULL;
		sLoadedPath.clear();
		return false;
	}

	return true;
}

static void LoadOnce(void)
{
	const char *overridePath = getenv("DESMUME_LUAJIT_DYLIB");
	if (overridePath != NULL && overridePath[0] != '\0')
	{
		// A forced path is exactly that - do not silently fall back to some
		// other copy, people set this to test a freshly built LuaJIT.
		if (!TryLoad(overridePath))
		{
			sLastError = std::string("DESMUME_LUAJIT_DYLIB=") + overridePath + ": " + sLastError;
		}
		return;
	}

	std::string firstError;
	const std::vector<std::string> dirs = CandidateDirectories();
	for (size_t i = 0; i < dirs.size(); i++)
	{
		for (size_t n = 0; n < sizeof(kLibraryNames) / sizeof(kLibraryNames[0]); n++)
		{
			if (TryLoad(JoinPath(dirs[i], kLibraryNames[n])))
				return;
			if (firstError.empty())
				firstError = sLastError;
		}
	}

	// Every candidate failed.  Keep the failure of the first (most likely)
	// candidate, that is the one that names the file people expect to exist.
	sLastError = firstError.empty() ? "libluajit-5.1.dylib not found" : firstError;
}

bool LuaJIT_LoadLibrary(void)
{
	pthread_once(&sLoadOnce, LoadOnce);
	return sLuaJITHandle != NULL;
}

bool LuaJIT_IsLoaded(void)
{
	return sLuaJITHandle != NULL;
}

const char *LuaJIT_LibraryPath(void)
{
	return sLoadedPath.c_str();
}

const char *LuaJIT_LoadError(void)
{
	return sLastError.c_str();
}

void LuaJIT_UnloadLibrary(void)
{
	if (sLuaJITHandle == NULL)
		return;

	dlclose(sLuaJITHandle);
	sLuaJITHandle = NULL;
	sLoadedPath.clear();
	memset(&g_luaJITApi, 0, sizeof(g_luaJITApi));
}
#endif // DESMUME_LUA_DYNAMIC
