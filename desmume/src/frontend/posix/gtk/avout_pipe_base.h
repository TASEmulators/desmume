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

#ifndef _AVOUT_PIPE_BASE_H_
#define _AVOUT_PIPE_BASE_H_

#include "avout.h"

class AVOutPipeBase : public AVOut {
public:
	bool begin(const char* fname);
	void end();
	bool isRecording();
	void updateAudio(void* soundData, int soundLen);
	void updateVideo(const u16* buffer);
protected:
	enum Type { TYPE_AUDIO, TYPE_VIDEO };
	virtual Type type() = 0;
	virtual const char* const* getArgv(const char* fname) = 0;
private:
	bool recording;
	int pipe_fd;
};

#endif
