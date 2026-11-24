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
#include <stdlib.h>
#include <string.h>
#include <vector>

#include "types.h"
#include "mic.h"
#include "NDSSystem.h"
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
static std::vector<u8> mic_samples;
static size_t mic_sample_read_position = 0;

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

static BOOL Mic_OpenDevice()
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

BOOL Mic_Init()
{
	Mic_BufPos = 0;
	Mic_PlayBuf = 1;
	Mic_WriteBuf = 0;
	mic_sample_read_position = 0;

	if (CommonSettings.micMode == MicMode_Physical)
		return Mic_OpenDevice();

	return TRUE;
}

BOOL Mic_ApplySettings(const char *sampleFile)
{
	Mic_Inited = FALSE;
	Mic_CloseDevice();
	mic_samples.clear();
	mic_sample_read_position = 0;

	if (CommonSettings.micMode == MicMode_Sample) {
		if (sampleFile == NULL || sampleFile[0] == '\0') {
			fprintf(stderr, "Mic: no microphone sample file selected\n");
			return FALSE;
		}

		SDL_AudioSpec spec;
		Uint8 *data = NULL;
		Uint32 length = 0;
		if (!SDL_LoadWAV(sampleFile, &spec, &data, &length)) {
			Mic_Error("microphone sample load failed");
			return FALSE;
		}

		if (spec.format != SDL_AUDIO_U8 || spec.channels != 1 || length == 0) {
			fprintf(stderr, "Mic: sample must be a non-empty 8-bit mono PCM WAV file\n");
			SDL_free(data);
			return FALSE;
		}

		mic_samples.assign(data, data + length);
		SDL_free(data);
	}

	if (CommonSettings.micMode == MicMode_Physical)
		return Mic_OpenDevice();

	Mic_Reset();
	return TRUE;
}

void Mic_Reset()
{
	mic_sample_read_position = 0;

	if (Mic_Inited && mic_stream != NULL)
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
	Mic_Inited = FALSE;
	Mic_CloseDevice();
	mic_samples.clear();
	mic_sample_read_position = 0;
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

static bool Mic_GetActivate()
{
	return NDS_getFinalUserInput().mic.micButtonPressed;
}

static u8 Mic_ConvertSample(u8 sample)
{
	u8 ret;
	if (Mic_BufPos & 0x1)
		ret = ((sample & 0x1) << 7);
	else
		ret = ((sample & 0xFE) >> 1);

	Mic_BufPos++;
	if (Mic_BufPos >= (MIC_BUFSIZE << 1)) {
		if (CommonSettings.micMode == MicMode_Physical) {
			Mic_Dequeue();
			Mic_BufPos = 0;
			Mic_PlayBuf ^= 1;
			Mic_WriteBuf ^= 1;
			mic_write_offset = 0;
			memset(Mic_Buffer[Mic_WriteBuf], 0, MIC_BUFSIZE);
		} else {
			Mic_BufPos = 0;
		}
	}

	return ret;
}

u8 Mic_ReadSample()
{
	u8 sample = 0x80;
	const MicMode mode = CommonSettings.micMode;

	if (mode == MicMode_Physical) {
		if (!Mic_Inited)
			return 0;

		sample = Mic_Buffer[Mic_PlayBuf][Mic_BufPos >> 1];
		if (!(Mic_BufPos & 0x1))
			Mic_Dequeue();
	} else if (!Mic_GetActivate()) {
		mic_sample_read_position = 0;
	} else if (mode == MicMode_InternalNoise) {
		static const u8 noise[] = {
			0xFC, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xF5, 0xFF, 0xFF, 0xFF, 0xFF, 0x8E, 0xFF,
			0xF4, 0xE1, 0xBF, 0x9A, 0x71, 0x58, 0x5B, 0x5F,
			0x62, 0xC2, 0x25, 0x05, 0x01, 0x01, 0x01, 0x01
		};
		sample = noise[(mic_sample_read_position >> 1) % (sizeof(noise) / sizeof(noise[0]))];
		mic_sample_read_position++;
	} else if (mode == MicMode_Random) {
		sample = (u8)(rand() & 0xFF);
		mic_sample_read_position++;
	} else if (mode == MicMode_Sample) {
		if (mic_sample_read_position < mic_samples.size() * 2) {
			sample = mic_samples[mic_sample_read_position >> 1];
			if (sample == 0)
				sample = 1;
			else if (sample == 255)
				sample = 254;
			mic_sample_read_position++;
		}
	} else {
		mic_sample_read_position = 0;
	}

	return Mic_ConvertSample(sample);
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
