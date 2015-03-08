/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2012-2015 DeSmuME team

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

#ifndef _MIC_EXTENSION_
#define _MIC_EXTENSION_

#include <stdint.h>


typedef void (*MicResetCallback)(void *inParam1, void *inParam2);
typedef uint8_t (*MicSampleReadCallback)(void *inParam1, void *inParam2);

void Mic_SetResetCallback(MicResetCallback callbackFunc, void *inParam1, void *inParam2);
void Mic_SetSampleReadCallback(MicSampleReadCallback callbackFunc, void *inParam1, void *inParam2);
void Mic_DefaultResetCallback(void *inParam1, void *inParam2);
uint8_t Mic_DefaultSampleReadCallback(void *inParam1, void *inParam2);

#endif // _MIC_EXTENSION_
