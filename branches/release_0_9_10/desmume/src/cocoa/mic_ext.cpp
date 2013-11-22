/*
	Copyright (C) 2011 Roger Manuel
	Copyright (C) 2012-2013 DeSmuME team

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

#import "cocoa_globals.h"
#include "mic_ext.h"
#include "readwrite.h"

RingBuffer micInputBuffer(MIC_MAX_BUFFER_SAMPLES * 2, sizeof(u8));
NullGenerator nullSampleGenerator;
InternalNoiseGenerator internalNoiseGenerator;
WhiteNoiseGenerator whiteNoiseGenerator;
SineWaveGenerator sineWaveGenerator(250.0, MIC_SAMPLE_RATE);

static bool _micUseBufferedSource = true;
static AudioGenerator *_selectedDirectSampleGenerator = NULL;


BOOL Mic_Init()
{
	// Do nothing.
	return TRUE;
}

void Mic_Reset()
{
	micInputBuffer.clear();
}

void Mic_DeInit()
{
	// Do nothing.
}

u8 Mic_ReadSample()
{
	u8 theSample = MIC_NULL_SAMPLE_VALUE;
	
	if (_micUseBufferedSource)
	{
		if (micInputBuffer.isEmpty())
		{
			return theSample;
		}
		
		micInputBuffer.read(&theSample, 1);
	}
	else
	{
		if (_selectedDirectSampleGenerator != NULL)
		{
			theSample = _selectedDirectSampleGenerator->generateSample();
		}
	}
	
	return theSample;
}

bool Mic_GetUseBufferedSource()
{
	return _micUseBufferedSource;
}

void Mic_SetUseBufferedSource(bool theState)
{
	_micUseBufferedSource = theState;
}

AudioGenerator* Mic_GetSelectedDirectSampleGenerator()
{
	return _selectedDirectSampleGenerator;
}

void Mic_SetSelectedDirectSampleGenerator(AudioGenerator *theGenerator)
{
	_selectedDirectSampleGenerator = theGenerator;
}

void mic_savestate(EMUFILE* os)
{
	write32le(-1, os);
}

bool mic_loadstate(EMUFILE* is, int size)
{
	is->fseek(size, SEEK_CUR);
	return true;
}
