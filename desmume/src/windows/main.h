#ifndef _MAIN_H_
#define _MAIN_H_

#include "CWindow.h"
extern WINCLASS	*MainWindow;

extern volatile bool execute, paused;
void NDS_Pause(bool showMsg = true);
void NDS_UnPause(bool showMsg = true);
void LoadSaveStateInfo();
void Display();
void Pause();
void Unpause();
void TogglePause();
void FrameAdvance(bool state);
void ResetGame();	//Resets game (for the menu item & hotkey
void AviRecordTo();
void AviEnd();
void WavRecordTo();
void WavEnd();

extern bool frameCounterDisplay;
extern bool FpsDisplay;
extern bool ShowLagFrameCounter;

#define GPU3D_NULL 0
#define GPU3D_OPENGL 1
#define GPU3D_SWRAST 2

extern int backupmemorytype;
extern u32 backupmemorysize;


#endif
