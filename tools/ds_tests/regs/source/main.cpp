/* 	regs IO test

	Copyright 2010 DeSmuME team

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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/
#include <nds.h>

#include <stdio.h>

#define myassert(e,msg)       ((e) ? (void)0 : _myassert(__FILE__, __LINE__, #e, msg))

void _myassert(const char *fileName, int lineNumber, const char* conditionString, const char* message)
{
	iprintf("---\nAssertion failed!\n\x1b[39mFile: \n%s\n\nLine: %d\n\nCondition:\n%s\n\n\x1b[41m%s",fileName, lineNumber, conditionString, message);
	for(;;) swiWaitForVBlank();
}

int main(void) {
	consoleDemoInit();

	for(int i=0;i<32;i++)
	{
		GFX_FOG_TABLE[i] = 0xFF;
		iprintf("%02X\n",GFX_FOG_TABLE[i]);
		myassert(GFX_FOG_TABLE[i] == 0x00,"test whether fog table entries are writeable");
	}
	
	iprintf("all tests OK");
	for(;;) swiWaitForVBlank();
	return 0;
}
