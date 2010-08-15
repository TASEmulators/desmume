/*---------------------------------------------------------------------------------

default ARM7 core

Copyright (C) 2005
Michael Noland (joat)
Jason Rogers (dovoto)
Dave Murphy (WinterMute)

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1.	The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.
2.	Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.
3.	This notice may not be removed or altered from any source
distribution.

---------------------------------------------------------------------------------*/
#include <nds.h>
#include <dswifi7.h>
#include <maxmod7.h>
#include "../../regstest.h"

//---------------------------------------------------------------------------------
void VcountHandler() {
	//---------------------------------------------------------------------------------
	inputGetAndSend();
}

//---------------------------------------------------------------------------------
void VblankHandler(void) {
//---------------------------------------------------------------------------------
	Wifi_Update();
}

void pokeMessage(char* msg)
{
	char* cp = msg;
	int i=0;
	while(*cp)
		arm7comm->message[i++] = *cp++;
	arm7comm->message[i] = 0;
}

void fail(char* msg, u32 offender=0)
{
	arm7comm->code = 1;
	arm7comm->offender = offender;
	pokeMessage(msg);

	while (1) swiWaitForVBlank();
}

//---------------------------------------------------------------------------------
int main() {
//---------------------------------------------------------------------------------
	irqInit();
	fifoInit();

	// read User Settings from firmware
	readUserSettings();

	// Start the RTC tracking IRQ
	initClockIRQ();

	SetYtrigger(80);

	installWifiFIFO();
	installSoundFIFO();

	mmInstall(FIFO_MAXMOD);

	installSystemFIFO();
	
	irqSet(IRQ_VCOUNT, VcountHandler);
	irqSet(IRQ_VBLANK, VblankHandler);

	irqEnable( IRQ_VBLANK | IRQ_VCOUNT | IRQ_NETWORK);   

	
	while (arm7comm->code != 0xDEADBEEF) 
		swiWaitForVBlank();

	//spu source reg should only be 27bit
	//but it is not readable so what does it matter
	for(int i=0;i<16;i++)
	{
		vu32* reg = (vu32*)(0x04000404 + (i<<4));
		*reg = 0xFFFFFFFF;
		//if(*reg != 0x07FFFFFF) fail("spu source reg is only 27bit.\nshould be 0x07FFFFFF",*reg);
		if(*reg != 0x00000000) fail("spu source reg is not readable!",*reg);
	}

	//spu length reg should only be 22bit
	for(int i=0;i<16;i++)
	{
		vu32* reg = (vu32*)(0x0400040C + (i<<4));
		*reg = 0xFFFFFFFF;
		//if(*reg != 0x003FFFFF) fail("spu length reg is only 22bit.\nshould be 0x003FFFFF",*reg);
		if(*reg != 0x00000000) fail("spu length reg is not readable!",*reg);
	}

	
	arm7comm->code = 2;
	while (1) swiWaitForVBlank();
}


