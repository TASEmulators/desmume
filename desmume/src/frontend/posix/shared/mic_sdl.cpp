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
	Physical microphone input through SDL3's recording device stream.

	SDL is already a hard dependency of the frontends (it drives sound output
	and input), so using it for capture keeps one backend on Linux, macOS,
	BSD and Windows alike.  SDL3 opens a stream whose app-side format is
	exactly what the emulator consumes and resamples/converts the hardware
	side internally, so the old "retry with allowed changes + manual
	SDL_AudioStream conversion" fallback is no longer needed.

	The sample-pumping logic is deliberately kept identical to mic_alsa:
	two 4096-byte buffers and two dequeue points per 8192-sample cycle.
	SDL_GetAudioStreamData() is non-blocking and cheap, so the extra calls
	cost nothing when no new samples are available.
*/

#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#include "types.h"
#include "mic.h"
#include "readwrite.h"
#include "emufile.h"

#define MIC_BUFSIZE 4096
#define MIC_RATE 16000
#define MIC_CHANNELS 1
#define MIC_FORMAT SDL_AUDIO_S8

BOOL Mic_Inited = FALSE;
u8 Mic_Buffer[2][MIC_BUFSIZE];
u16 Mic_BufPos;
u8 Mic_PlayBuf;
u8 Mic_WriteBuf;

int MicButtonPressed;

/* Stream bound to the default recording device.  Its app side is
   MIC_FORMAT / MIC_RATE / MIC_CHANNELS; destroying it closes the device. */
static SDL_AudioStream *mic_stream = NULL;
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
		SDL_PauseAudioStreamDevice(mic_stream);
		SDL_DestroyAudioStream(mic_stream);
		mic_stream = NULL;
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

	if (Mic_Inited)
		return TRUE;

	if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
		Mic_Error("SDL audio subsystem init failed");
		return FALSE;
	}
	mic_audio_ref_held = TRUE;

	SDL_zero(want);
	want.freq = MIC_RATE;
	want.format = MIC_FORMAT;
	want.channels = MIC_CHANNELS;

	/* NULL callback means we pull data ourselves with SDL_GetAudioStreamData().
	   The device starts paused, so resume it after Mic_Reset(). */
	mic_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_RECORDING,
	                                      &want, NULL, NULL);
	if (mic_stream == NULL) {
		Mic_Error("SDL capture init failed");
		Mic_CloseDevice();
		return FALSE;
	}

	Mic_Inited = TRUE;
	Mic_Reset();
	SDL_ResumeAudioStreamDevice(mic_stream);

	return TRUE;
}

void Mic_Reset()
{
	if (!Mic_Inited)
		return;

	if (mic_stream != NULL)
		SDL_ClearAudioStream(mic_stream);

	/* SDL_AUDIO_S8 is signed, so 0 really is silence here.  (The old OpenAL
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
   Never blocks: SDL_GetAudioStreamData() returns 0 when nothing is available. */
static void Mic_Dequeue()
{
	u8 *dest;
	u32 room;
	int got;

	if (mic_stream == NULL || mic_write_offset >= MIC_BUFSIZE)
		return;

	dest = Mic_Buffer[Mic_WriteBuf] + mic_write_offset;
	room = MIC_BUFSIZE - mic_write_offset;

	got = SDL_GetAudioStreamData(mic_stream, dest, (int)room);
	if (got > 0)
		mic_write_offset += (u32)got;
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
