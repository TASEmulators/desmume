#ifndef MIC_H
#define MIC_H

#ifdef WIN32
extern int MicButtonPressed;
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
