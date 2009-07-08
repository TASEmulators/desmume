#ifndef WIN32

#include "types.h"
#include "mic.h"

static BOOL silence = TRUE;
int MicButtonPressed;

BOOL Mic_Init()
{
	return TRUE;
}

void Mic_Reset()
{
}

void Mic_DeInit()
{
}

u8 Mic_ReadSample()
{
	if (silence)
		return 0;

	return 150;
}

void Mic_DoNoise(BOOL noise)
{
	silence = !noise;
}

#endif
