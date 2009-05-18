#ifndef _MAIN_H_
#define _MAIN_H_

#include "CWindow.h"
extern WINCLASS	*MainWindow;

extern volatile BOOL execute, paused;
void NDS_Pause();
void NDS_UnPause();
extern unsigned int lastSaveState;
void LoadSaveStateInfo();
void SaveStateMessages(int slotnum, int whichMessage);
void Display();
void Pause();
void FrameAdvance(bool state);
void ResetGame();	//Resets game (for the menu item & hotkey
void AviRecordTo();
void AviEnd();
void WavRecordTo();
void WavEnd();
void SetMessageToDisplay(const char *message);	//For sending output to the main screen

extern bool ShowInputDisplay;
extern bool frameCounterDisplay;
extern bool FpsDisplay;
extern bool ShowInputDisplay;
extern bool ShowLagFrameCounter;

#define GPU3D_NULL 0
#define GPU3D_OPENGL 1
#define GPU3D_SWRAST 2

extern int backupmemorytype;
extern u32 backupmemorysize;


#endif
