#ifndef MIC_H
#define MIC_H

extern int MicButtonPressed;

#ifdef WIN32
static char MicSampleName[256];
char* LoadSample(const char *name);
extern int MicDisplay;
#else
void Mic_DoNoise(BOOL);
#endif

BOOL Mic_Init();
void Mic_Reset();
void Mic_DeInit();
u8 Mic_ReadSample();

#endif
