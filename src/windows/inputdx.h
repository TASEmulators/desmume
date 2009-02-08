/*  Copyright (C) 2006 yopyop
    yopyop156@ifrance.com
    yopyop156.ifrance.com

    Copyright (C) 2006-2008 DeSmuME team

    This file is part of DeSmuME

    DeSmuME is free software; you can redistribute it and/or modify
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

#ifndef _INPUT_DX_
#define _INPUT_DX_

#define DIRECTINPUT_VERSION 0x0800
#include "../common.h"
#include "../types.h"
#include "directx/dinput.h"

#define MAXKEYPAD 15

typedef void (*INPUTPROC)(BOOL, LPSTR);

class DI_CLASS
{
	friend BOOL CALLBACK EnumCallback(LPCDIDEVICEINSTANCE lpddi, LPVOID pvRef);
	friend BOOL CALLBACK EnumObjects(const DIDEVICEOBJECTINSTANCE* pdidoi,VOID* pContext);
public:
	char	JoystickName[255];
	BOOL	Feedback;
	LPDIRECTINPUTDEVICE8 EnumDevices(LPDIRECTINPUT8 pDI);
protected:
	BOOL CALLBACK EnumCallback(LPCDIDEVICEINSTANCE lpddi, LPVOID pvRef);
	BOOL CALLBACK EnumObjects(const DIDEVICEOBJECTINSTANCE* pdidoi,VOID* pContext);
};

class INPUTCLASS : private DI_CLASS
{
private:
	HWND					hParentWnd;
	BOOL					paused;
	
	char					cDIBuf[512];
	LPDIRECTINPUT8			pDI;
	LPDIRECTINPUTDEVICE8	pKeyboard;
	LPDIRECTINPUTDEVICE8	pJoystick;
	DIDEVCAPS				DIJoycap;
	LPDIRECTINPUTEFFECT     pEffect;

	INPUTPROC				inputProc;

public:
	INPUTCLASS();
	~INPUTCLASS();
	BOOL	Init(HWND hParentWnd, INPUTPROC inputProc);
	BOOL	JoystickEnabled();
	void	JoystickFeedback(BOOL on);
	void	process();
};

// ========== emu input
extern	void InputConfig(HWND hwnd);
extern	void NDS_inputInit();
extern	void NDS_inputPost(BOOL paused, LPSTR buf);
extern	u16		keyPad[MAXKEYPAD];
#endif