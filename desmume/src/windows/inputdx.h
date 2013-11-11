/*
	This file is part of DeSmuME, derived from several files in Snes9x 1.51 which are 
	licensed under the terms supplied at the end of this file (for the terms are very long!)
	Differences from that baseline version are:

	Copyright (C) 2008-2010 DeSmuME team

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


#ifndef INPUTDX_INCLUDED
#define INPUTDX_INCLUDED

#include <mmsystem.h>
#define DIRECTINPUT_VERSION 0x0800
#include "directx/dinput.h"

typedef struct
{
    COLORREF crForeGnd;    // Foreground text colour
    COLORREF crBackGnd;    // Background text colour
    HFONT    hFont;        // The font
    HWND     hwnd;         // The control's window handle
} InputCust;
COLORREF CheckButtonKey( WORD Key);
COLORREF CheckHotKey( WORD Key, int modifiers);
InputCust * GetInputCustom(HWND hwnd);

#define CUSTKEY_ALT_MASK   0x01
#define CUSTKEY_CTRL_MASK  0x02
#define CUSTKEY_SHIFT_MASK 0x04
#define CUSTKEY_NONE_MASK  0x08

struct SJoypad {
    BOOL Enabled;
    WORD Left;
    WORD Right;
    WORD Up;
    WORD Down;
    WORD Left_Up;
    WORD Left_Down;
    WORD Right_Up;
    WORD Right_Down;
    WORD Start;
    WORD Select;
	WORD Lid;
    WORD Debug;
    WORD A;
    WORD B;
    WORD X;
    WORD Y;
    WORD L;
    WORD R;
};

#define LEFT_MASK 0x0001
#define RIGHT_MASK 0x0002
#define UP_MASK 0x0004
#define DOWN_MASK 0x0008
#define START_MASK 0x0010
#define SELECT_MASK 0x0020
#define LID_MASK 0x0040
#define DEBUG_MASK 0x0080
#define A_MASK 0x0100
#define B_MASK 0x0200
#define X_MASK 0x0400
#define Y_MASK 0x0800
#define L_MASK 0x1000
#define R_MASK 0x2000


struct SJoyState{
	LPDIRECTINPUTDEVICE8 Device;
    bool Attached;
    JOYCAPS Caps;
    int Threshold;
    bool Left;
    bool Right;
    bool Up;
    bool Down;
    
	bool XRotMax;
    bool XRotMin;
    bool YRotMax;
    bool YRotMin;
    bool ZRotMax;
    bool ZRotMin;

    bool PovLeft;
    bool PovRight;
    bool PovUp;
    bool PovDown;
    bool PovDnLeft;
    bool PovDnRight;
    bool PovUpLeft;
    bool PovUpRight;
    bool RUp;
    bool RDown;
    bool UUp;
    bool UDown;
    bool VUp;
    bool VDown;
    bool ZUp;
    bool ZDown;
    bool Button[128];
	bool FeedBack;
	LPDIRECTINPUTEFFECT     pEffect;
};

extern SJoypad Joypad[16];
extern SJoypad ToggleJoypadStorage[8];
//extern SCustomKeys CustomKeys;
extern SJoypad TurboToggleJoypadStorage[8];

void RunInputConfig();
void RunHotkeyConfig();
void input_acquire();
void input_process();
void LoadHotkeyConfig();
void input_init();
void input_deinit();

struct SGuitar {
    BOOL Enabled;
    WORD GREEN;
    WORD RED;
    WORD YELLOW;
    WORD BLUE;
};

struct SPiano {
    BOOL Enabled;
    WORD C,CS,D,DS,E,F,FS,G,GS,A,AS,B,HIC;
};

struct SPaddle {
    BOOL Enabled;
    WORD DEC;
    WORD INC;
};

extern SGuitar Guitar;
extern SPiano Piano;
extern SPaddle Paddle;

#endif


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
