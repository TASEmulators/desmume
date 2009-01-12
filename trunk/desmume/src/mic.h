#ifndef MIC_H
#define MIC_H

#ifdef WIN32

BOOL Mic_Init();
void Mic_Reset();
void Mic_DeInit();
u8 Mic_ReadSample();

#else
/* TODO : mic support for other platforms */

BOOL Mic_Init() { return TRUE; }
void Mic_Reset() { }
void Mic_DeInit() { }
u8 Mic_ReadSample() { return 0; }

#endif

#endif
