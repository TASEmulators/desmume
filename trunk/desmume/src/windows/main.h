#ifndef _MAIN_H_
#define _MAIN_H_

#include "CWindow.h"
extern WINCLASS	*MainWindow;
extern HINSTANCE hAppInst;
extern HMENU mainMenu; //Holds handle to the main DeSmuME menu
extern CToolBar* MainWindowToolbar;

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
void WavRecordTo(int wavmode);
void WavEnd();
void UpdateToolWindows();
bool DemandLua();

extern bool frameCounterDisplay;
extern bool FpsDisplay;
extern bool ShowLagFrameCounter;

#define GPU3D_NULL 0
#define GPU3D_OPENGL 1
#define GPU3D_SWRAST 2

static const int LANGUAGE_ENGLISH = 0;
static const int LANGUAGE_FRENCH = 1;
static const int LANGUAGE_CHINESE = 3;
static const int LANGUAGE_ITALIAN = 4;
static const int LANGUAGE_JAPANESE = 5;
static const int LANGUAGE_SPANISH = 6;
static const int LANGUAGE_KOREAN = 7;

extern void Change3DCoreWithFallbackAndSave(int newCore, int fallbackCore=GPU3D_SWRAST);

extern int backupmemorytype;
extern u32 backupmemorysize;

void WIN_InstallCFlash();

#define IDM_RECENT_RESERVED0                    65500
#define IDM_RECENT_RESERVED1                    65501
#define IDM_RECENT_RESERVED2                    65502
#define IDM_RECENT_RESERVED3                    65503
#define IDM_RECENT_RESERVED4                    65504
#define IDM_RECENT_RESERVED5                    65505
#define IDM_RECENT_RESERVED6                    65506
#define IDM_RECENT_RESERVED7                    65507
#define IDM_RECENT_RESERVED8                    65508
#define IDM_RECENT_RESERVED9                    65509
#define IDM_RECENT_RESERVED10                   65510
#define IDM_RECENT_RESERVED11                   65511
#define IDM_RECENT_RESERVED12                   65512
#define IDM_RECENT_RESERVED13                   65513

#define IDT_VIEW_DISASM7						50001
#define IDT_VIEW_DISASM9                		50002
#define IDT_VIEW_MEM7                   		50003
#define IDT_VIEW_MEM9                   		50004
#define IDT_VIEW_IOREG                  		50005
#define IDT_VIEW_PAL                    		50006
#define IDT_VIEW_TILE                   		50007
#define IDT_VIEW_MAP                    		50008
#define IDT_VIEW_OAM                    		50009
#define IDT_VIEW_MATRIX                 		50010
#define IDT_VIEW_LIGHTS                 		50011
#define IDM_EXEC								50112

#endif
