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

	//test what happens when you do 8bit reads and writes to OAM
	vu16* const oam16 = (vu16*)0x07000000;
	vu8* const oam8 = (vu8*)0x07000000;
	vu8* const oam8b = (vu8*)0x07000001;
	*oam16 = 0;
	myassert(*oam16 == 0x00,"oam access A");
	myassert(*oam8 == 0x00,"oam access B");
	myassert(*oam8b == 0x00,"oam access C");
	*oam16 = 0x1234;
	myassert(*oam16 == 0x1234,"oam access D");
	myassert(*oam8 == 0x34,"oam access E");
	myassert(*oam8b == 0x12,"oam access F");
	*oam8 = 0xFF;
	*oam8b = 0xEE;
	myassert(*oam16 == 0x1234,"oam access G");
	myassert(*oam8 == 0x34,"oam access H");
	myassert(*oam8b == 0x12,"oam access I");
	
	iprintf("all tests OK");
	for(;;) swiWaitForVBlank();
	return 0;
}
