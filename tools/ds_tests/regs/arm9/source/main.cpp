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
#include "../../regstest.h"

#define myassert(e,msg)       ((e) ? (void)0 : _myassert(__FILE__, __LINE__, #e, msg))

void _myassert(const char *fileName, int lineNumber, const char* conditionString, const char* message)
{
	iprintf("---\nAssertion failed!\n\x1b[39mFile: \n%s\n\nLine: %d\n\nCondition:\n%s\n\n\x1b[41m%s",fileName, lineNumber, conditionString, message);
	for(;;) swiWaitForVBlank();
}

arm7comm_t arm7comm;

int main(void) {
	consoleDemoInit();

	//tell the arm7 where to store its stuff
	//(the arm7 needs to know a location in main memory (shared) to store its results)
	//(it could store its results in shared arm7 memory but we chose to do it this way since)
	//(I couldn't figure out how to specify storage there for a global in arm7.cpp. any suggestions?)
	fifoSendAddress(FIFO_USER_01,&arm7comm);

	//fog table entries should not be readable. they should return 0
	for(int i=0;i<32;i++)
	{
		GFX_FOG_TABLE[i] = 0xFF;
		//iprintf("%02X\n",GFX_FOG_TABLE[i]);
		myassert(GFX_FOG_TABLE[i] == 0x00,"test whether fog table entries are non-readable");
	}

	//8bit vram reads should work. 8bit vram writes should fail
	vu8* bgmem8 = (vu8*)0x06000000;
	vu16* bgmem16 = (vu16*)0x06000000;
	vu32* bgmem32 = (vu32*)0x06000000;
	bgmem16[0] = 0x1234;
	bgmem16[1] = 0x5678;
	myassert(bgmem8[0] == 0x34 && bgmem8[1] == 0x12 && bgmem8[2] == 0x78 && bgmem8[3] == 0x56,"test 8bit vram reads");
	bgmem8[0] = 0;
	myassert(bgmem8[0] == 0x34, "test 8bit vram writes");

	//test OAM also
	vu8* oam8 = (vu8*)0x07000000;
	vu16* oam16 = (vu16*)0x07000000;
	vu32* oam32 = (vu32*)0x07000000;
	oam16[0] = 0x1234;
	oam16[1] = 0x5678;
	myassert(oam8[0] == 0x34 && oam8[1] == 0x12 && oam8[2] == 0x78 && oam8[3] == 0x56,"test 8bit oam reads");
	oam8[0] = 0;
	myassert(oam8[0] == 0x34, "test 8bit oam writes");

	//test pal also
	vu8* pal8 = (vu8*)0x05000000;
	vu16* pal16 = (vu16*)0x05000000;
	vu32* pal32 = (vu32*)0x05000000;
	pal16[0] = 0x1234;
	pal16[1] = 0x5678;
	myassert(pal8[0] == 0x34 && pal8[1] == 0x12 && pal8[2] == 0x78 && pal8[3] == 0x56,"test 8bit pal reads");
	pal8[0] = 0;
	myassert(pal8[0] == 0x34, "test 8bit pal writes");


	//-------------------
	iprintf("waiting for arm7 test to finish!\n");

	for(;;)
	{
		if(fifoCheckValue32(FIFO_USER_01))
			break;
		swiWaitForVBlank();
	}

	iprintf("arm7 finish code: %d\n",arm7comm.code);

	if(arm7comm.code == 1)
	{
		iprintf("arm7 test failed!\n");
		iprintf("%s\n",arm7comm.message);
		iprintf("offending val: 0x%08X\n",arm7comm.offender);
	}
	else
	{
		iprintf("all tests OK");
	}
	
	for(;;) swiWaitForVBlank();
	return 0;
}
