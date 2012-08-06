/*
	Copyright (C) 2012 DeSmuME team

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

#import <OpenEmuBase/OERingBuffer.h>
#include "../../SPU.h"

#define SNDCORE_OPENEMU 58326

// Sound interface to the SPU
extern SoundInterface_struct SNDOpenEmu;

// Ring buffer
extern OERingBuffer *openEmuSoundInterfaceBuffer;

#ifdef __cplusplus
extern "C"
{
#endif

// OpenEmu functions for the sound interface
int		SNDOpenEmuInit(int buffer_size);
void	SNDOpenEmuDeInit();
int		SNDOpenEmuReset();
void	SNDOpenEmuUpdateAudio(s16 *buffer, u32 num_samples);
u32		SNDOpenEmuGetAudioSpace();
void	SNDOpenEmuMuteAudio();
void	SNDOpenEmuUnMuteAudio();
void	SNDOpenEmuSetVolume(int volume);
void	SNDOpenEmuClearBuffer();
void	SNDOpenEmuFetchSamples(s16 *sampleBuffer, size_t sampleCount, ESynchMode synchMode, ISynchronizingAudioBuffer *theSynchronizer);
size_t	SNDOpenEmuPostProcessSamples(s16 *postProcessBuffer, size_t requestedSampleCount, ESynchMode synchMode, ISynchronizingAudioBuffer *theSynchronizer);

#ifdef __cplusplus
}
#endif
