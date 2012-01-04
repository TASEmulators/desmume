/*
	Copyright (C) 2007 Jeff Bland
	Copyright (C) 2007-2011 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "../SPU.h"

#define SNDCORE_OSX 58325 //hopefully this is unique number


// This is the sound interface so the emulator core can send us sound info and whatnot
extern SoundInterface_struct SNDOSX;

// Sound interface extensions for CoreAudio
void SNDOSXStartup();
void SNDOSXShutdown();
int SNDOSXInit(int buffer_size);
void SNDOSXDeInit();
int SNDOSXReset();
void SNDOSXUpdateAudio(s16 *buffer, u32 num_samples);
u32 SNDOSXGetAudioSpace();
void SNDOSXMuteAudio();
void SNDOSXUnMuteAudio();
void SNDOSXSetVolume(int volume);
void SNDOSXClearBuffer();

// Recording
// Not supported as of 2011/12/28 - rogerman
bool SNDOSXOpenFile(void *fname);  //opens a file for recording (if filename is the currently opened one, it will restart the file), fname is an NSString
void SNDOSXStartRecording();      //begins recording to the currently open file if there is an open file
void SNDOSXStopRecording();       //pauses recording (you can continue recording later)
void SNDOSXCloseFile();           //closes the file, making sure it's saved