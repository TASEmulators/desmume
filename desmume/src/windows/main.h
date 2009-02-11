#ifndef _MAIN_H_
#define _MAIN_H_

#include "CWindow.h"
extern WINCLASS	*MainWindow;

extern volatile BOOL execute, paused;
void NDS_Pause();
void NDS_UnPause();
extern unsigned int lastSaveState;
void SaveStateMessages(int slotnum, int whichMessage);
void Display();
void Pause();
void FrameAdvance();

#endif
