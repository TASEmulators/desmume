#ifndef WIN32

#include "types.h"
#include "mic.h"

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
	return 0;
}

#endif
