/*
	Copyright (C) 2014 DeSmuME team
	Copyright (C) 2014 Alvin Wong

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

#ifndef DESMUME_QT_DS_H
#define DESMUME_QT_DS_H

#include "video.h"
#include "ds_input.h"

namespace desmume {
namespace qt {
namespace ds {

extern Video *video;
extern Input input;

void init();
void deinit();
bool loadROM(const char* path);
bool isRunning();
void pause();
void unpause();
void execFrame();

} /* namespace ds */
} /* namespace qt */
} /* namespace desmume */

#endif /* DESMUME_QT_DS_H */
