///*  This file is part of DeSmuME, derived from several files in Snes9x 1.51 which are 
//    licensed under the terms supplied at the end of this file (for the terms are very long!)
//    Differences from that baseline version are:
//
//    Copyright (C) 2009 DeSmuME team
//
//    DeSmuME is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    DeSmuME is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with DeSmuME; if not, write to the Free Software
//    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//*/

#include "hotkey.h"
#include "main.h"
#include "NDSSystem.h"
#include "saves.h"
#include "inputdx.h"
#include "render3d.h"
#include "throttle.h"
#include "../mic.h"
#include "../movie.h"
#include "ramwatch.h"		//In order to call UpdateRamWatch (for loadstate functions)
#include "ram_search.h"		//In order to call UpdateRamSearch (for loadstate functions)
#include "replay.h"
#include "aviout.h"
#include "spu.h"
#include "../GPU.h"

extern LRESULT OpenFile();	//adelikat: Made this an extern here instead of main.h  Seemed icky not to limit the scope of this function

SCustomKeys CustomKeys;

bool AutoHoldPressed=false;

bool IsLastCustomKey (const SCustomKey *key)
{
	return (key->key == 0xFFFF && key->modifiers == 0xFFFF);
}

void SetLastCustomKey (SCustomKey *key)
{
	key->key = 0xFFFF;
	key->modifiers = 0xFFFF;
}

void ZeroCustomKeys (SCustomKeys *keys)
{
	UINT i = 0;

	SetLastCustomKey(&keys->LastItem);
	while (!IsLastCustomKey(&keys->key(i))) {
		keys->key(i).key = 0;
		keys->key(i).modifiers = 0;
		i++;
	};
}


void CopyCustomKeys (SCustomKeys *dst, const SCustomKeys *src)
{
	UINT i = 0;

	do {
		dst->key(i) = src->key(i);
	} while (!IsLastCustomKey(&src->key(i++)));
}

//======================================================================================
//=====================================HANDLERS=========================================
//======================================================================================
void HK_OpenROM(int) {OpenFile();}
void HK_PrintScreen(int param)
{
    OPENFILENAME ofn;
	char * ptr;
    char filename[MAX_PATH] = "";
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = MainWindow->getHWnd();
    ofn.lpstrFilter = "png file (*.png)\0*.png\0Bmp file (*.bmp)\0*.bmp\0Any file (*.*)\0*.*\0\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFile =  filename;
	ofn.lpstrTitle = "Print Screen Save As";
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrDefExt = "png";
	ofn.Flags = OFN_OVERWRITEPROMPT;
	GetSaveFileName(&ofn);

	ptr = strrchr(filename,'.');//look for the last . in the filename

	if ( ptr != 0 ) {
		if (( strcmp ( ptr, ".BMP" ) == 0 ) ||
			( strcmp ( ptr, ".bmp" ) == 0 )) 
		{
			NDS_WriteBMP(filename);
		}
		if (( strcmp ( ptr, ".PNG" ) == 0 ) ||
			( strcmp ( ptr, ".png" ) == 0 )) 
		{
			NDS_WritePNG(filename);
		}
	}
}

void HK_StateSaveSlot(int num)
{
	if (romloaded)
	{
		if (!paused)
		{
			NDS_Pause();
			savestate_slot(num);	//Savestate
			NDS_UnPause();
		}
		else
			savestate_slot(num);	//Savestate
		
		lastSaveState = num;		//Set last savestate used
		LoadSaveStateInfo();
	}
}

void HK_StateLoadSlot(int num)
{
	if (romloaded)
	{
		BOOL wasPaused = paused;
		NDS_Pause();
		loadstate_slot(num);		//Loadstate
		lastSaveState = num;		//Set last savestate used

		Update_RAM_Watch();			//adelikat: TODO this should be a single function call in main, that way we can expand as future dialogs need updating
		Update_RAM_Search();		//main.cpp - case IDM_STATE_LOAD: also calls these functions

		if(!wasPaused)
			NDS_UnPause();
		else
			Display();
	}
}

void HK_StateSetSlot(int num)
{
	if (romloaded)
	{
		lastSaveState = num;
		osd->addLine("State %i selected", num);
	}
}

void HK_StateQuickSaveSlot(int)
{
	HK_StateSaveSlot(lastSaveState);
}

void HK_StateQuickLoadSlot(int)
{
	HK_StateLoadSlot(lastSaveState);
}

void HK_MicrophoneKeyDown(int) {MicButtonPressed =1;}
void HK_MicrophoneKeyUp(int) {MicButtonPressed =0;}

void HK_AutoHoldClearKeyDown(int) {
	
	ClearAutoHold();
}

void HK_Reset(int) {ResetGame();}

void HK_RecordAVI(int) { if (AVI_IsRecording()) AviEnd(); else AviRecordTo(); }
void HK_RecordWAV(int) { if (WAV_IsRecording()) WavEnd(); else WavRecordTo(); }

void HK_ToggleFrame(int) {frameCounterDisplay ^= true;}
void HK_ToggleFPS(int) {FpsDisplay ^= true;}
void HK_ToggleInput(int) {ShowInputDisplay ^= true;}
void HK_ToggleLag(int) {ShowLagFrameCounter ^= true;}
void HK_ToggleReadOnly(int) {
	movie_readonly ^= true; 
	if(movie_readonly)
		osd->addLine("Read Only");
	else
		osd->addLine("Read+Write");
}

void HK_PlayMovie(int) 
{
	if (romloaded)
	{
		//NDS_Pause();
		//Replay_LoadMovie();
		//NDS_UnPause();
	}
}

void HK_RecordMovie(int) 
{
	if (romloaded)
	{
		//NDS_Pause();
		//MovieRecordTo();
		//NDS_UnPause();
	}
}

void HK_StopMovie(int) 
{
	FCEUI_StopMovie();
}

void HK_AutoHoldKeyDown(int) {AutoHoldPressed = true;}
void HK_AutoHoldKeyUp(int) {AutoHoldPressed = false;}

void HK_TurboRightKeyDown(int) { Turbo.Right = true; }
void HK_TurboRightKeyUp(int) { Turbo.Right = false; }

void HK_TurboLeftKeyDown(int) { Turbo.Left = true; }
void HK_TurboLeftKeyUp(int) { Turbo.Left = false; }

void HK_TurboRKeyDown(int) { Turbo.R = true; }
void HK_TurboRKeyUp(int) { Turbo.R = false; }

void HK_TurboLKeyDown(int) { Turbo.L = true; }
void HK_TurboLKeyUp(int) { Turbo.L = false; }

void HK_TurboDownKeyDown(int) { Turbo.Down = true; }
void HK_TurboDownKeyUp(int) { Turbo.Down = false; }

void HK_TurboUpKeyDown(int) { Turbo.Up = true; }
void HK_TurboUpKeyUp(int) { Turbo.Up = false; }

void HK_TurboBKeyDown(int) { Turbo.B = true; }
void HK_TurboBKeyUp(int) { Turbo.B = false; }

void HK_TurboAKeyDown(int) { Turbo.A = true; }
void HK_TurboAKeyUp(int) { Turbo.A = false; }

void HK_TurboXKeyDown(int) { Turbo.X = true; }
void HK_TurboXKeyUp(int) { Turbo.X = false; }

void HK_TurboYKeyDown(int) { Turbo.Y = true; }
void HK_TurboYKeyUp(int) { Turbo.Y = false; }

void HK_TurboStartKeyDown(int) { Turbo.Start = true; }
void HK_TurboStartKeyUp(int) { Turbo.Start = false; }

void HK_TurboSelectKeyDown(int) { Turbo.Select = true; }
void HK_TurboSelectKeyUp(int) { Turbo.Select = false; }

void HK_NextSaveSlot(int) { 
	lastSaveState++; 
	if(lastSaveState>9) 
		lastSaveState=0; 
	osd->addLine("State %i selected", lastSaveState);
}

void HK_PreviousSaveSlot(int) { 

	if(lastSaveState==0) 
		lastSaveState=9; 
	else
		lastSaveState--;
	osd->addLine("State %i selected", lastSaveState);
}

void HK_Pause(int) { Pause(); }
void HK_FastForwardToggle(int) { FastForward ^=1; }
void HK_FastForwardKeyDown(int) { FastForward = 1; }
void HK_FastForwardKeyUp(int) { FastForward = 0; }
void HK_IncreaseSpeed(int) { IncreaseSpeed(); }
void HK_DecreaseSpeed(int) { DecreaseSpeed(); }
void HK_FrameAdvanceKeyDown(int) { FrameAdvance(true); }
void HK_FrameAdvanceKeyUp(int) { FrameAdvance(false); }

void HK_ToggleRasterizer(int) { 
	if(cur3DCore == GPU3D_OPENGL)
		cur3DCore = GPU3D_SWRAST;
	else cur3DCore = GPU3D_OPENGL;

	NDS_3D_ChangeCore(cur3DCore);
	WritePrivateProfileInt("3D", "Renderer", cur3DCore, IniName);
}

//======================================================================================
//=====================================DEFINITIONS======================================
//======================================================================================

void InitCustomKeys (SCustomKeys *keys)
{
	UINT i = 0;

	SetLastCustomKey(&keys->LastItem);
	while (!IsLastCustomKey(&keys->key(i))) {
		SCustomKey &key = keys->key(i);
		key.key = 0;
		key.modifiers = 0;
		key.handleKeyDown = NULL;
		key.handleKeyUp = NULL;
		key.page = NUM_HOTKEY_PAGE;
		key.param = 0;

		//keys->key[i].timing = PROCESS_NOW;
		i++;
	};

	//Main Page---------------------------------------
	keys->OpenROM.handleKeyDown = HK_OpenROM;
	keys->OpenROM.code = "OpenROM";
	keys->OpenROM.name = L"Open ROM";
	keys->OpenROM.page = HOTKEY_PAGE_MAIN;
	keys->OpenROM.key = 'O';
	keys->OpenROM.modifiers = CUSTKEY_CTRL_MASK;

	keys->Reset.handleKeyDown = HK_Reset;
	keys->Reset.code = "Reset";
	keys->Reset.name = L"Reset";
	keys->Reset.page = HOTKEY_PAGE_MAIN;
	keys->Reset.key = 'R';
	keys->Reset.modifiers = CUSTKEY_CTRL_MASK;

	keys->Pause.handleKeyDown = HK_Pause;
	keys->Pause.code = "Pause";
	keys->Pause.name = L"Pause";
	keys->Pause.page = HOTKEY_PAGE_MAIN;
	keys->Pause.key = VK_PAUSE;

	keys->FrameAdvance.handleKeyDown = HK_FrameAdvanceKeyDown;
	keys->FrameAdvance.handleKeyUp = HK_FrameAdvanceKeyUp;
	keys->FrameAdvance.code = "FrameAdvance";
	keys->FrameAdvance.name = L"Frame Advance";
	keys->FrameAdvance.page = HOTKEY_PAGE_MAIN;
	keys->FrameAdvance.key = 'N';

	keys->FastForward.handleKeyDown = HK_FastForwardKeyDown;
	keys->FastForward.handleKeyUp = HK_FastForwardKeyUp;
	keys->FastForward.code = "FastForward";
	keys->FastForward.name = L"Fast Forward";
	keys->FastForward.page = HOTKEY_PAGE_MAIN;
	keys->FastForward.key = VK_TAB;

	keys->FastForwardToggle.handleKeyDown = HK_FastForwardToggle;
	keys->FastForwardToggle.code = "FastForwardToggle";
	keys->FastForwardToggle.name = L"Fast Forward Toggle";
	keys->FastForwardToggle.page = HOTKEY_PAGE_MAIN;
	keys->FastForwardToggle.key = NULL;

	keys->IncreaseSpeed.handleKeyDown = HK_IncreaseSpeed;
	keys->IncreaseSpeed.code = "IncreaseSpeed";
	keys->IncreaseSpeed.name = L"Increase Speed";
	keys->IncreaseSpeed.page = HOTKEY_PAGE_MAIN;
	keys->IncreaseSpeed.key = VK_OEM_PLUS;

	keys->DecreaseSpeed.handleKeyDown = HK_DecreaseSpeed;
	keys->DecreaseSpeed.code = "DecreaseSpeed";
	keys->DecreaseSpeed.name = L"Decrease Speed";
	keys->DecreaseSpeed.page = HOTKEY_PAGE_MAIN;
	keys->DecreaseSpeed.key = VK_OEM_MINUS;
	
	keys->Microphone.handleKeyDown = HK_MicrophoneKeyDown;
	keys->Microphone.handleKeyUp = HK_MicrophoneKeyUp;
	keys->Microphone.code = "Microphone";
	keys->Microphone.name = L"Microphone";
	keys->Microphone.page = HOTKEY_PAGE_MAIN;
	keys->Microphone.key = NULL;

	keys->AutoHold.handleKeyDown = HK_AutoHoldKeyDown;
	keys->AutoHold.handleKeyUp = HK_AutoHoldKeyUp;
	keys->AutoHold.code = "AutoHold";
	keys->AutoHold.name = L"Auto-Hold";
	keys->AutoHold.page = HOTKEY_PAGE_MAIN;
	keys->AutoHold.key = NULL;

	keys->AutoHoldClear.handleKeyDown = HK_AutoHoldClearKeyDown;
	keys->AutoHoldClear.code = "AutoHoldClear";
	keys->AutoHoldClear.name = L"Auto-Hold Clear";
	keys->AutoHoldClear.page = HOTKEY_PAGE_MAIN;
	keys->AutoHoldClear.key = NULL;

	keys->ToggleRasterizer.handleKeyDown = HK_ToggleRasterizer;
	keys->ToggleRasterizer.code = "ToggleRasterizer";
	keys->ToggleRasterizer.name = L"Toggle Rasterizer";
	keys->ToggleRasterizer.page = HOTKEY_PAGE_MAIN;
	keys->ToggleRasterizer.key = VK_SUBTRACT;

	keys->PrintScreen.handleKeyDown = HK_PrintScreen;
	keys->PrintScreen.code = "SaveScreenshotas";
	keys->PrintScreen.name = L"Save Screenshot as";
	keys->PrintScreen.page = HOTKEY_PAGE_MAIN;
	keys->PrintScreen.key = VK_F12;

	keys->ToggleFrameCounter.handleKeyDown = HK_ToggleFrame;
	keys->ToggleFrameCounter.code = "ToggleFrameDisplay";
	keys->ToggleFrameCounter.name = L"Toggle Frame Display";
	keys->ToggleFrameCounter.page = HOTKEY_PAGE_MAIN;
	keys->ToggleFrameCounter.key = VK_OEM_PERIOD;

	keys->ToggleFPS.handleKeyDown = HK_ToggleFPS;
	keys->ToggleFPS.code = "ToggleFPSDisplay";
	keys->ToggleFPS.name = L"Toggle FPS Display";
	keys->ToggleFPS.page = HOTKEY_PAGE_MAIN;
	keys->ToggleFPS.key = NULL;

	keys->ToggleInput.handleKeyDown = HK_ToggleInput;
	keys->ToggleInput.code = "ToggleInputDisplay";
	keys->ToggleInput.name = L"Toggle Input Display";
	keys->ToggleInput.page = HOTKEY_PAGE_MAIN;
	keys->ToggleInput.key = VK_OEM_COMMA;

	keys->ToggleLag.handleKeyDown = HK_ToggleLag;
	keys->ToggleLag.code = "ToggleLagDisplay";
	keys->ToggleLag.name = L"Toggle Lag Display";
	keys->ToggleLag.page = HOTKEY_PAGE_MAIN;
	keys->ToggleLag.key = NULL;

	keys->ToggleReadOnly.handleKeyDown = HK_ToggleReadOnly;
	keys->ToggleReadOnly.code = "ToggleReadOnly";
	keys->ToggleReadOnly.name = L"Toggle Read Only";
	keys->ToggleReadOnly.page = HOTKEY_PAGE_MOVIE;
	keys->ToggleReadOnly.key = NULL;

	keys->PlayMovie.handleKeyDown = HK_PlayMovie;
	keys->PlayMovie.code = "PlayMovie";
	keys->PlayMovie.name = L"Play Movie";
	keys->PlayMovie.page = HOTKEY_PAGE_MOVIE;
	keys->PlayMovie.key = NULL;

	keys->RecordMovie.handleKeyDown = HK_RecordMovie;
	keys->RecordMovie.code = "RecordMovie";
	keys->RecordMovie.name = L"Record Movie";
	keys->RecordMovie.page = HOTKEY_PAGE_MOVIE;
	keys->RecordMovie.key = NULL;

	keys->StopMovie.handleKeyDown = HK_StopMovie;
	keys->StopMovie.code = "StopMovie";
	keys->StopMovie.name = L"Stop Movie";
	keys->StopMovie.page = HOTKEY_PAGE_MOVIE;
	keys->StopMovie.key = NULL;

	keys->RecordWAV.handleKeyDown = HK_RecordWAV;
	keys->RecordWAV.code = "RecordWAV";
	keys->RecordWAV.name = L"Record WAV";
	keys->RecordWAV.page = HOTKEY_PAGE_MAIN;
	keys->RecordWAV.key = NULL;

	keys->RecordAVI.handleKeyDown = HK_RecordAVI;
	keys->RecordAVI.code = "RecordAVI";
	keys->RecordAVI.name = L"Record AVI";
	keys->RecordAVI.page = HOTKEY_PAGE_MAIN;
	keys->RecordAVI.key = NULL;

	//Turbo Page---------------------------------------
	keys->TurboRight.handleKeyDown = HK_TurboRightKeyDown;
	keys->TurboRight.handleKeyUp = HK_TurboRightKeyUp;
	keys->TurboRight.code = "TurboRight";
	keys->TurboRight.name = L"Turbo Right";
	keys->TurboRight.page = HOTKEY_PAGE_TURBO;
	keys->TurboRight.key = NULL;

	keys->TurboLeft.handleKeyDown = HK_TurboLeftKeyDown;
	keys->TurboLeft.handleKeyUp = HK_TurboLeftKeyUp;
	keys->TurboLeft.code = "TurboLeft";
	keys->TurboLeft.name = L"Turbo Left";
	keys->TurboLeft.page = HOTKEY_PAGE_TURBO;
	keys->TurboLeft.key = NULL;

	keys->TurboR.handleKeyDown = HK_TurboRKeyDown;
	keys->TurboR.handleKeyUp = HK_TurboRKeyUp;
	keys->TurboR.code = "TurboR";
	keys->TurboR.name = L"Turbo R";
	keys->TurboR.page = HOTKEY_PAGE_TURBO;
	keys->TurboR.key = NULL;

	keys->TurboL.handleKeyDown = HK_TurboLKeyDown;
	keys->TurboL.handleKeyUp = HK_TurboLKeyUp;
	keys->TurboL.code = "TurboL";
	keys->TurboL.name = L"Turbo L";
	keys->TurboL.page = HOTKEY_PAGE_TURBO;
	keys->TurboL.key = NULL;

	keys->TurboDown.handleKeyDown = HK_TurboDownKeyDown;
	keys->TurboDown.handleKeyUp = HK_TurboDownKeyUp;
	keys->TurboDown.code = "TurboDown";
	keys->TurboDown.name = L"Turbo Down";
	keys->TurboDown.page = HOTKEY_PAGE_TURBO;
	keys->TurboDown.key = NULL;

	keys->TurboUp.handleKeyDown = HK_TurboUpKeyDown;
	keys->TurboUp.handleKeyUp = HK_TurboUpKeyUp;
	keys->TurboUp.code = "TurboUp";
	keys->TurboUp.name = L"Turbo Up";
	keys->TurboUp.page = HOTKEY_PAGE_TURBO;
	keys->TurboUp.key = NULL;

	keys->TurboB.handleKeyDown = HK_TurboBKeyDown;
	keys->TurboB.handleKeyUp = HK_TurboBKeyUp;
	keys->TurboB.code = "TurboB";
	keys->TurboB.name = L"Turbo B";
	keys->TurboB.page = HOTKEY_PAGE_TURBO;
	keys->TurboB.key = NULL;

	keys->TurboA.handleKeyDown = HK_TurboAKeyDown;
	keys->TurboA.handleKeyUp = HK_TurboAKeyUp;
	keys->TurboA.code = "TurboA";
	keys->TurboA.name = L"Turbo A";
	keys->TurboA.page = HOTKEY_PAGE_TURBO;
	keys->TurboA.key = NULL;

	keys->TurboX.handleKeyDown = HK_TurboXKeyDown;
	keys->TurboX.handleKeyUp = HK_TurboXKeyUp;
	keys->TurboX.code = "TurboX";
	keys->TurboX.name = L"Turbo X";
	keys->TurboX.page = HOTKEY_PAGE_TURBO;
	keys->TurboX.key = NULL;

	keys->TurboY.handleKeyDown = HK_TurboYKeyDown;
	keys->TurboY.handleKeyUp = HK_TurboYKeyUp;
	keys->TurboY.code = "TurboY";
	keys->TurboY.name = L"Turbo Y";
	keys->TurboY.page = HOTKEY_PAGE_TURBO;
	keys->TurboY.key = NULL;

	keys->TurboSelect.handleKeyDown = HK_TurboSelectKeyDown;
	keys->TurboSelect.handleKeyUp = HK_TurboSelectKeyUp;
	keys->TurboSelect.code = "TurboSelect";
	keys->TurboSelect.name = L"Turbo Select";
	keys->TurboSelect.page = HOTKEY_PAGE_TURBO;
	keys->TurboSelect.key = NULL;

	keys->TurboStart.handleKeyDown = HK_TurboStartKeyDown;
	keys->TurboStart.handleKeyUp = HK_TurboStartKeyUp;
	keys->TurboStart.code = "TurboStart";
	keys->TurboStart.name = L"Turbo Start";
	keys->TurboStart.page = HOTKEY_PAGE_TURBO;
	keys->TurboStart.key = NULL;

	keys->NextSaveSlot.handleKeyDown = HK_NextSaveSlot;
	keys->NextSaveSlot.code = "NextSaveSlot";
	keys->NextSaveSlot.name = L"Next Save Slot";
	keys->NextSaveSlot.page = HOTKEY_PAGE_STATE_SLOTS;
	keys->NextSaveSlot.key = NULL;

	keys->PreviousSaveSlot.handleKeyDown = HK_PreviousSaveSlot;
	keys->PreviousSaveSlot.code = "PreviousSaveSlot";
	keys->PreviousSaveSlot.name = L"Previous Save Slot";
	keys->PreviousSaveSlot.page = HOTKEY_PAGE_STATE_SLOTS;
	keys->PreviousSaveSlot.key = NULL;
	
	keys->QuickSave.handleKeyDown = HK_StateQuickSaveSlot;
	keys->QuickSave.code = "QuickSave";
	keys->QuickSave.name = L"Quick Save";
	keys->QuickSave.page = HOTKEY_PAGE_STATE_SLOTS;
	keys->QuickSave.key = 'I';

	keys->QuickLoad.handleKeyDown = HK_StateQuickLoadSlot;
	keys->QuickLoad.code = "QuickLoad";
	keys->QuickLoad.name = L"Quick Load";
	keys->QuickLoad.page = HOTKEY_PAGE_STATE_SLOTS;
	keys->QuickLoad.key = 'P';

	for(int i=0;i<10;i++) {
		static const char* saveNames[] = {"SaveToSlot0","SaveToSlot1","SaveToSlot2","SaveToSlot3","SaveToSlot4","SaveToSlot5","SaveToSlot6","SaveToSlot7","SaveToSlot8","SaveToSlot9"};
		static const char* loadNames[] = {"LoadFromSlot0","LoadFromSlot1","LoadFromSlot2","LoadFromSlot3","LoadFromSlot4","LoadFromSlot5","LoadFromSlot6","LoadFromSlot7","LoadFromSlot8","LoadFromSlot9"};
		static const char* slotNames[] = {"SelectSlot0","SelectSlot1","SelectSlot2","SelectSlot3","SelectSlot4","SelectSlot5","SelectSlot6","SelectSlot7","SelectSlot8","SelectSlot9"};

		WORD key = VK_F1 + i - 1;
		if(i==0) key = VK_F10;

		SCustomKey & save = keys->Save[i];
		save.handleKeyDown = HK_StateSaveSlot;
		save.param = i;
		save.page = HOTKEY_PAGE_STATE;
		wchar_t tmp[16];
		_itow(i,tmp,10);
		save.name = (std::wstring)L"Save To Slot " + (std::wstring)tmp;
		save.code = saveNames[i];
		save.key = key;
		save.modifiers = CUSTKEY_SHIFT_MASK;

		SCustomKey & load = keys->Load[i];
		load.handleKeyDown = HK_StateLoadSlot;
		load.param = i;
		load.page = HOTKEY_PAGE_STATE;
		_itow(i,tmp,10);
		load.name = (std::wstring)L"Load from Slot " + (std::wstring)tmp;
		load.code = loadNames[i];
		load.key = key;

		key = '0' + i;

		SCustomKey & slot = keys->Slot[i];
		slot.handleKeyDown = HK_StateSetSlot;
		slot.param = i;
		slot.page = HOTKEY_PAGE_STATE_SLOTS;
		_itow(i,tmp,10);
		slot.name = (std::wstring)L"Select Save Slot " + (std::wstring)tmp;
		slot.code = slotNames[i];
		slot.key = key;
	}
}


/**********************************************************************************
  Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.

  (c) Copyright 1996 - 2002  Gary Henderson (gary.henderson@ntlworld.com),
                             Jerremy Koot (jkoot@snes9x.com)

  (c) Copyright 2002 - 2004  Matthew Kendora

  (c) Copyright 2002 - 2005  Peter Bortas (peter@bortas.org)

  (c) Copyright 2004 - 2005  Joel Yliluoma (http://iki.fi/bisqwit/)

  (c) Copyright 2001 - 2006  John Weidman (jweidman@slip.net)

  (c) Copyright 2002 - 2006  funkyass (funkyass@spam.shaw.ca),
                             Kris Bleakley (codeviolation@hotmail.com)

  (c) Copyright 2002 - 2007  Brad Jorsch (anomie@users.sourceforge.net),
                             Nach (n-a-c-h@users.sourceforge.net),
                             zones (kasumitokoduck@yahoo.com)

  (c) Copyright 2006 - 2007  nitsuja


  BS-X C emulator code
  (c) Copyright 2005 - 2006  Dreamer Nom,
                             zones

  C4 x86 assembler and some C emulation code
  (c) Copyright 2000 - 2003  _Demo_ (_demo_@zsnes.com),
                             Nach,
                             zsKnight (zsknight@zsnes.com)

  C4 C++ code
  (c) Copyright 2003 - 2006  Brad Jorsch,
                             Nach

  DSP-1 emulator code
  (c) Copyright 1998 - 2006  _Demo_,
                             Andreas Naive (andreasnaive@gmail.com)
                             Gary Henderson,
                             Ivar (ivar@snes9x.com),
                             John Weidman,
                             Kris Bleakley,
                             Matthew Kendora,
                             Nach,
                             neviksti (neviksti@hotmail.com)

  DSP-2 emulator code
  (c) Copyright 2003         John Weidman,
                             Kris Bleakley,
                             Lord Nightmare (lord_nightmare@users.sourceforge.net),
                             Matthew Kendora,
                             neviksti


  DSP-3 emulator code
  (c) Copyright 2003 - 2006  John Weidman,
                             Kris Bleakley,
                             Lancer,
                             z80 gaiden

  DSP-4 emulator code
  (c) Copyright 2004 - 2006  Dreamer Nom,
                             John Weidman,
                             Kris Bleakley,
                             Nach,
                             z80 gaiden

  OBC1 emulator code
  (c) Copyright 2001 - 2004  zsKnight,
                             pagefault (pagefault@zsnes.com),
                             Kris Bleakley,
                             Ported from x86 assembler to C by sanmaiwashi

  SPC7110 and RTC C++ emulator code
  (c) Copyright 2002         Matthew Kendora with research by
                             zsKnight,
                             John Weidman,
                             Dark Force

  S-DD1 C emulator code
  (c) Copyright 2003         Brad Jorsch with research by
                             Andreas Naive,
                             John Weidman

  S-RTC C emulator code
  (c) Copyright 2001-2006    byuu,
                             John Weidman

  ST010 C++ emulator code
  (c) Copyright 2003         Feather,
                             John Weidman,
                             Kris Bleakley,
                             Matthew Kendora

  Super FX x86 assembler emulator code
  (c) Copyright 1998 - 2003  _Demo_,
                             pagefault,
                             zsKnight,

  Super FX C emulator code
  (c) Copyright 1997 - 1999  Ivar,
                             Gary Henderson,
                             John Weidman

  Sound DSP emulator code is derived from SNEeSe and OpenSPC:
  (c) Copyright 1998 - 2003  Brad Martin
  (c) Copyright 1998 - 2006  Charles Bilyue'

  SH assembler code partly based on x86 assembler code
  (c) Copyright 2002 - 2004  Marcus Comstedt (marcus@mc.pp.se)

  2xSaI filter
  (c) Copyright 1999 - 2001  Derek Liauw Kie Fa

  HQ2x, HQ3x, HQ4x filters
  (c) Copyright 2003         Maxim Stepin (maxim@hiend3d.com)

  Win32 GUI code
  (c) Copyright 2003 - 2006  blip,
                             funkyass,
                             Matthew Kendora,
                             Nach,
                             nitsuja

  Mac OS GUI code
  (c) Copyright 1998 - 2001  John Stiles
  (c) Copyright 2001 - 2007  zones


  Specific ports contains the works of other authors. See headers in
  individual files.


  Snes9x homepage: http://www.snes9x.com

  Permission to use, copy, modify and/or distribute Snes9x in both binary
  and source form, for non-commercial purposes, is hereby granted without
  fee, providing that this license information and copyright notice appear
  with all copies and any derived work.

  This software is provided 'as-is', without any express or implied
  warranty. In no event shall the authors be held liable for any damages
  arising from the use of this software or it's derivatives.

  Snes9x is freeware for PERSONAL USE only. Commercial users should
  seek permission of the copyright holders first. Commercial use includes,
  but is not limited to, charging money for Snes9x or software derived from
  Snes9x, including Snes9x or derivatives in commercial game bundles, and/or
  using Snes9x as a promotion for your commercial product.

  The copyright holders request that bug fixes and improvements to the code
  should be forwarded to them so everyone can benefit from the modifications
  in future versions.

  Super NES and Super Nintendo Entertainment System are trademarks of
  Nintendo Co., Limited and its subsidiary companies.
**********************************************************************************/

