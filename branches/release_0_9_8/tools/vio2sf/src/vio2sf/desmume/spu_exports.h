#ifndef _SPU_EXPORTS_H
#define _SPU_EXPORTS_H


#ifndef _SPU_CPP_

void SPU_WriteLong(u32 addr, u32 val);
void SPU_WriteByte(u32 addr, u8 val);
void SPU_WriteWord(u32 addr, u16 val);
void SPU_EmulateSamples(int numsamples);
int SPU_ChangeSoundCore(int coreid, int buffersize);
void SPU_Reset(void);
void SPU_DeInit(void);

typedef struct
{
   int id;
   const char *Name;
   int (*Init)(int buffersize);
   void (*DeInit)();
   void (*UpdateAudio)(s16 *buffer, u32 num_samples);
   u32 (*GetAudioSpace)();
   void (*MuteAudio)();
   void (*UnMuteAudio)();
   void (*SetVolume)(int volume);
} SoundInterface_struct;

#endif _SPU_CPP_

#endif //_SPU_EXPORTS_H
