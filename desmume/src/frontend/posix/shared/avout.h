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

#ifndef _AVOUT_H_
#define _AVOUT_H_

#include "types.h"

class AVOut {
public:
	virtual bool begin(const char* fname) { return false; }
	virtual void end() {}
	virtual bool isRecording() { return false; }
	virtual void updateAudio(void* soundData, int soundLen) {}
	virtual void updateVideo(const u16* buffer) {}
};

#endif
