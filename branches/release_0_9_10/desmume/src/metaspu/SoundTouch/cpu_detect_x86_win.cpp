////////////////////////////////////////////////////////////////////////////////
///
/// Win32 version of the x86 CPU detect routine.
///
/// This file is to be compiled in Windows platform with Microsoft Visual C++ 
/// Compiler. Please see 'cpu_detect_x86_gcc.cpp' for the gcc compiler version 
/// for all GNU platforms.
///
/// Author        : Copyright (c) Olli Parviainen
/// Author e-mail : oparviai 'at' iki.fi
/// SoundTouch WWW: http://www.surina.net/soundtouch
///
////////////////////////////////////////////////////////////////////////////////
//
// Last changed  : $Date: 2006/02/05 16:44:06 $
// File revision : $Revision: 1.10 $
//
// $Id: cpu_detect_x86_win.cpp,v 1.10 2006/02/05 16:44:06 Olli Exp $
//
////////////////////////////////////////////////////////////////////////////////
//
// License :
//
//  SoundTouch audio processing library
//  Copyright (c) Olli Parviainen
//
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2.1 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public
//  License along with this library; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
////////////////////////////////////////////////////////////////////////////////

#include "cpu_detect.h"
#ifdef _MSC_VER
#include <intrin.h>
#endif

#ifndef _WIN32
#error wrong platform - this source code file is exclusively for Win32 platform
#endif

//////////////////////////////////////////////////////////////////////////////
//
// processor instructions extension detection routines
//
//////////////////////////////////////////////////////////////////////////////

// Flag variable indicating whick ISA extensions are disabled (for debugging)
static uint _dwDisabledISA = 0x00;      // 0xffffffff; //<- use this to disable all extensions


// Disables given set of instruction extensions. See SUPPORT_... defines.
void disableExtensions(uint dwDisableMask)
{
    _dwDisabledISA = dwDisableMask;
}



/// Checks which instruction set extensions are supported by the CPU.
uint detectCPUextensions(void)
{
    uint res = 0;

    if (_dwDisabledISA == 0xffffffff) return 0;

#ifdef _MSC_VER
	int CPUInfo[4] = {-1};
	__cpuid(CPUInfo, 1);
	res |= (CPUInfo[3] & (1 << 23))?SUPPORT_MMX:0;
	res |= (CPUInfo[3] & (1 << 25))?SUPPORT_SSE:0;

	// Test 3Dnow
	__cpuid(CPUInfo, 0x80000000);
	if (CPUInfo[0] == 0x80000000)
	{
		__cpuid(CPUInfo, 0x80000001);
		res |= (CPUInfo[3] & (1 << 31))?SUPPORT_3DNOW:0;
	}
#endif

    return res & ~_dwDisabledISA;
}
