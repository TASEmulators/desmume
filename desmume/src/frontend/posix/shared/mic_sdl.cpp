/*
	Copyright (C) 2009-2026 DeSmuME team

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

/*
	Physical microphone input through SDL2's capture API.

	This replaces the ALSA and OpenAL backends.  SDL is already a hard
	dependency of the POSIX frontends (it drives sound output and input),
	so using it for capture removes two optional libraries and gives us
	one backend on Linux, macOS, BSD and Windows alike.

	The sample-pumping logic is deliberately kept identical to mic_alsa:
	two 4096-byte buffers and two dequeue points per 8192-sample cycle.
	Unlike snd_pcm_readi(), SDL_DequeueAudio() is non-blocking and cheap,
	so the extra calls cost nothing when no new samples are available.
*/

#include <SDL.h>
#include <stdio.h>
#include <string.h>

#include "types.h"
#include "mic.h"
#include "readwrite.h"
#include "emufile.h"

#define MIC_BUFSIZE 4096
#define MIC_RATE 16000
#define MIC_CHANNELS 1
#define MIC_FORMAT AUDIO_S8
#define MIC_DEVICE_SAMPLES 1024
#define MIC_SCRATCHSIZE 4096

BOOL Mic_Inited = FALSE;
u8 Mic_Buffer[2][MIC_BUFSIZE];
u16 Mic_BufPos;
u8 Mic_PlayBuf;
u8 Mic_WriteBuf;

int MicButtonPressed;

static SDL_AudioDeviceID mic_device = 0;
/* non-NULL when the driver could not deliver MIC_FORMAT/MIC_RATE/MIC_CHANNELS */
static SDL_AudioStream *mic_stream = NULL;
static u8 mic_scratch[MIC_SCRATCHSIZE];
/* bytes already accumulated in Mic_Buffer[Mic_WriteBuf] for this cycle */
static u32 mic_write_offset = 0;
/* SDL_InitSubSystem(SDL_INIT_AUDIO) is ref-counted; only release our own ref */
static BOOL mic_audio_ref_held = FALSE;

static void Mic_Error(const char *what)
{
	/* One concise line on stderr, like the old g_printerr() calls, without
	   pulling glib into this file (INFO/LOG go to stdout / debug builds). */
	fprintf(stderr, "Mic: %s: %s\n", what, SDL_GetError());
}

static void Mic_CloseDevice()
{
	if (mic_stream != NULL) {
		SDL_FreeAudioStream(mic_stream);
		mic_stream = NULL;
	}
	if (mic_device != 0) {
		SDL_PauseAudioDevice(mic_device, 1);
		SDL_CloseAudioDevice(mic_device);
		mic_device = 0;
	}
	if (mic_audio_ref_held) {
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		mic_audio_ref_held = FALSE;
	}
	mic_write_offset = 0;
}

BOOL Mic_Init()
{
	SDL_AudioSpec want;
	SDL_AudioSpec obtained;

	if (Mic_Inited)
		return TRUE;

	if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
		Mic_Error("SDL audio subsystem init failed");
		return FALSE;
	}
	mic_audio_ref_held = TRUE;

	SDL_zero(want);
	SDL_zero(obtained);
	want.freq = MIC_RATE;
	want.format = MIC_FORMAT;
	want.channels = MIC_CHANNELS;
	want.samples = MIC_DEVICE_SAMPLES;
	want.callback = NULL; /* queue mode: SDL_DequeueAudio() only works with a NULL callback */
	want.userdata = NULL;

	/* First ask for exactly what the emulator consumes.  Some drivers only
	   support 48 kHz / S16 / stereo, so retry with all changes allowed and
	   convert back through an SDL_AudioStream. */
	mic_device = SDL_OpenAudioDevice(NULL, SDL_TRUE, &want, &obtained, 0);
	if (mic_device == 0) {
		SDL_zero(obtained);
		mic_device = SDL_OpenAudioDevice(NULL, SDL_TRUE, &want, &obtained,
		                                SDL_AUDIO_ALLOW_ANY_CHANGE);
	}
	if (mic_device == 0) {
		Mic_Error("SDL capture init failed");
		Mic_CloseDevice();
		return FALSE;
	}

	/* Also tolerate drivers that quietly change the spec despite allowed_changes=0. */
	if (obtained.freq != MIC_RATE || obtained.format != MIC_FORMAT ||
	    obtained.channels != MIC_CHANNELS) {
		mic_stream = SDL_NewAudioStream(obtained.format, obtained.channels, obtained.freq,
		                               MIC_FORMAT, MIC_CHANNELS, MIC_RATE);
		if (mic_stream == NULL) {
			Mic_Error("SDL audio conversion setup failed");
			Mic_CloseDevice();
			return FALSE;
		}
	}

	SDL_ClearQueuedAudio(mic_device);
	SDL_PauseAudioDevice(mic_device, 0); /* start capturing */

	Mic_Inited = TRUE;
	Mic_Reset();

	return TRUE;
}

void Mic_Reset()
{
	if (!Mic_Inited)
		return;

	if (mic_device != 0)
		SDL_ClearQueuedAudio(mic_device);
	if (mic_stream != NULL)
		SDL_AudioStreamClear(mic_stream);

	/* AUDIO_S8 is signed, so 0 really is silence here.  (The old OpenAL
	   backend used unsigned 8-bit samples and therefore filled with 128.) */
	memset(Mic_Buffer, 0, sizeof(Mic_Buffer));
	mic_write_offset = 0;
	Mic_BufPos = 0;
	Mic_PlayBuf = 1;
	Mic_WriteBuf = 0;
}

void Mic_DeInit()
{
	if (!Mic_Inited)
		return;

	Mic_Inited = FALSE;
	Mic_CloseDevice();
}

/* Pull whatever the driver has captured so far into the current write buffer.
   Never blocks: SDL_DequeueAudio() returns 0 when the capture queue is empty. */
static void Mic_Dequeue()
{
	u8 *dest;
	u32 room;

	if (mic_device == 0 || mic_write_offset >= MIC_BUFSIZE)
		return;

	dest = Mic_Buffer[Mic_WriteBuf] + mic_write_offset;
	room = MIC_BUFSIZE - mic_write_offset;

	if (mic_stream == NULL) {
		mic_write_offset += SDL_DequeueAudio(mic_device, dest, room);
	} else {
		Uint32 raw = SDL_DequeueAudio(mic_device, mic_scratch, sizeof(mic_scratch));
		int available;
		int wanted;
		int got;

		if (raw > 0)
			SDL_AudioStreamPut(mic_stream, mic_scratch, (int)raw);

		available = SDL_AudioStreamAvailable(mic_stream);
		if (available <= 0)
			return;

		wanted = (available < (int)room) ? available : (int)room;
		got = SDL_AudioStreamGet(mic_stream, dest, wanted);
		if (got > 0)
			mic_write_offset += (u32)got;
	}
}

u8 Mic_ReadSample()
{
	u8 tmp;
	u8 ret;

	if (!Mic_Inited)
		return 0;

	tmp = Mic_Buffer[Mic_PlayBuf][Mic_BufPos >> 1];

	if (Mic_BufPos & 0x1) {
		ret = ((tmp & 0x1) << 7);
	} else {
		ret = ((tmp & 0xFE) >> 1);
		Mic_Dequeue(); /* same prefetch point as snd_pcm_read() in mic_alsa */
	}

	Mic_BufPos++;
	if (Mic_BufPos >= (MIC_BUFSIZE << 1)) {
		Mic_Dequeue(); /* top up the buffer that is about to become the play buffer */
		Mic_BufPos = 0;
		Mic_PlayBuf ^= 1;
		Mic_WriteBuf ^= 1;
		mic_write_offset = 0;
		/* Unfilled tail of the new write buffer would otherwise replay the
		   previous cycle; make it silence instead. */
		memset(Mic_Buffer[Mic_WriteBuf], 0, MIC_BUFSIZE);
	}

	return ret;
}

/* FIXME: stub! */
void mic_savestate(EMUFILE &os)
{
	os.write_32LE(-1);
}

bool mic_loadstate(EMUFILE &is, int size)
{
	is.fseek(size, SEEK_CUR);
	return TRUE;
}
