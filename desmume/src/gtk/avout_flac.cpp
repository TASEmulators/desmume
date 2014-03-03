/*
	Copyright (C) 2014 DeSmuME team

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

#include <cstdlib>
#include <cstring>

#include "avout_flac.h"
#include "SPU.h"

// Helper macros to convert numerics to strings
#if defined(_MSC_VER)
	//re: http://72.14.203.104/search?q=cache:HG-okth5NGkJ:mail.python.org/pipermail/python-checkins/2002-November/030704.html+_msc_ver+compiler+version+string&hl=en&gl=us&ct=clnk&cd=5
	#define _Py_STRINGIZE(X) _Py_STRINGIZE1((X))
	#define _Py_STRINGIZE1(X) _Py_STRINGIZE2 ## X
	#define _Py_STRINGIZE2(X) #X

	#define TOSTRING(X) _Py_STRINGIZE(X) // Alias _Py_STRINGIZE so that we have a common macro name
#else
	#define STRINGIFY(x) #x
	#define TOSTRING(x) STRINGIFY(x)
#endif

AVOutFlac::AVOutFlac() {
	const char* const args[] = {
		"flac",
		"-f",
		"-o", this->filename,
		"--endian=little",
		"--channels=2",
		"--bps=16",
		"--sample-rate=" TOSTRING(DESMUME_SAMPLE_RATE),
		"--sign=signed",
		"--force-raw-format",
		"-",
		NULL
	};
	memcpy(this->args, args, sizeof(args));
}

const char* const* AVOutFlac::getArgv(const char* fname) {
	if (strlen(fname) >= sizeof(this->filename)) {
		return NULL;
	}
	strncpy(this->filename, fname, sizeof(this->filename));
	return this->args;
}

