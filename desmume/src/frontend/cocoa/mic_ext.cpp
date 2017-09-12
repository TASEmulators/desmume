/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2012-2017 DeSmuME team

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

#include "ClientInputHandler.h"
#include "mic_ext.h"
#include "../../emufile.h"
#include "../../readwrite.h"


MicResetCallback _micResetCallback = &Mic_DefaultResetCallback;
void *_resetCallbackParam1 = NULL;
void *_resetCallbackParam2 = NULL;

MicSampleReadCallback _micSampleReadCallback = &Mic_DefaultSampleReadCallback;
void *_sampleReadCallbackParam1 = NULL;
void *_sampleReadCallbackParam2 = NULL;

BOOL Mic_Init()
{
	return TRUE;
}

void Mic_DeInit()
{
	
}

void Mic_Reset()
{
	_micResetCallback(_resetCallbackParam1, _resetCallbackParam2);
}

// The NDS reads audio samples in the following format:
//    Format: Unsigned Linear PCM
//    Channels: 1 (mono)
//    Sample Rate: 16000 Hz
//    Sample Resolution: 7-bit
u8 Mic_ReadSample()
{
	return _micSampleReadCallback(_sampleReadCallbackParam1, _sampleReadCallbackParam2);
}

void Mic_SetResetCallback(MicResetCallback callbackFunc, void *inParam1, void *inParam2)
{
	_micResetCallback = callbackFunc;
	_resetCallbackParam1 = inParam1;
	_resetCallbackParam2 = inParam2;
}

void Mic_SetSampleReadCallback(MicSampleReadCallback callbackFunc, void *inParam1, void *inParam2)
{
	_micSampleReadCallback = callbackFunc;
	_sampleReadCallbackParam1 = inParam1;
	_sampleReadCallbackParam2 = inParam2;
}

void mic_savestate(EMUFILE &os)
{
	os.write_32LE(-1);
}

bool mic_loadstate(EMUFILE &is, int size)
{
	is.fseek(size, SEEK_CUR);
	return true;
}

void Mic_DefaultResetCallback(void *inParam1, void *inParam2)
{
	// Do nothing.
}

uint8_t Mic_DefaultSampleReadCallback(void *inParam1, void *inParam2)
{
	return MIC_NULL_SAMPLE_VALUE;
}
