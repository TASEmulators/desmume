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

AVOutFlac::AVOutFlac() {
	const char* const args[] = {
		"flac",
		"-f",
		"-o", this->filename,
		"--endian=little",
		"--channels=2",
		"--bps=16",
		"--sample-rate=44100",
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

