/*  Copyright 2007 Jeff Bland

    This file is part of DeSmuME.

    DeSmuME  is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DeSmuME is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DeSmuME; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#define OBJ_C
#include "../SPU.h"
#undef OBJ_C

#ifdef __cplusplus
extern "C"
{
#endif

#define SNDCORE_OSX 58325 //hopefully this is unique number

//This is the sound inerface so the emulator core can send us sound info and whatnot
extern SoundInterface_struct SNDOSX;

//Beyond this point are sound interface extensions specific to the mac port


int SNDOSXReset();
void SNDOSXMuteAudio();
void SNDOSXUnMuteAudio();
void SNDOSXSetVolume(int volume);

//Recording
bool SNDOSXOpenFile(void *fname);  //opens a file for recording (if filename is the currently opened one, it will restart the file), fname is an NSString
void SNDOSXStartRecording();      //begins recording to the currently open file if there is an open file
void SNDOSXStopRecording();       //pauses recording (you can continue recording later)
void SNDOSXCloseFile();           //closes the file, making sure it's saved

#ifdef __cplusplus
}
#endif