/*---------------------------------------------------------------------------------

once upon a time, this was: default ARM7 core

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
#include <nds/arm7/serial.h>

arm7comm_t *arm7comm;

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

//modified from: http://www.bottledlight.com/ds/index.php/Main/Firmware
#define FW_READ_ID     0x9F
#define FW_READ        0x03
#define FW_READ_STATUS 0x05
u32 getFirmwareType()
{
	u32 result;

	// Get ID
	while (REG_SPICNT & SPI_BUSY);
	REG_SPICNT = SPI_ENABLE | SPI_CONTINUOUS | SPI_DEVICE_FIRMWARE;
	REG_SPIDATA = FW_READ_ID;
	while (REG_SPICNT & SPI_BUSY);

	result = 0;
	for (int i = 0; i < 3; i++) {
		REG_SPIDATA = 0;
		while (REG_SPICNT & SPI_BUSY);
		result = (REG_SPIDATA & 0xFF) | (result<<8);
	}

	// Get status
	//zeromus note: this is broken. not only does it put the byte in a different spot than the docs said it does,
	//it is coded otherwise glitchily and just returns bytes of the 3-Byte flash ID 
	//(desmume shows five reads during the ID command; apparently this code fails to reset correctly)
	while (REG_SPICNT & SPI_BUSY);
	REG_SPICNT = SPI_ENABLE | SPI_CONTINUOUS | SPI_DEVICE_FIRMWARE;
	REG_SPIDATA = FW_READ_STATUS;
	while (REG_SPICNT & SPI_BUSY);

	REG_SPIDATA = 0;
	while (REG_SPICNT & SPI_BUSY);
	result = ((REG_SPIDATA & 0xFF) << 24) | result;

	return result;
}



void pokeMessage(const char* msg)
{
	const char* cp = msg;
	int i=0;
	while(*cp)
		arm7comm->message[i++] = *cp++;
	arm7comm->message[i] = 0;
}

void fail(const char* msg, u32 offender=0)
{
	arm7comm->code = 1;
	arm7comm->offender = offender;
	pokeMessage(msg);

	fifoSendValue32(FIFO_USER_01,0);
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

	//find out where the arm9 wants us to stash our info
	while(!fifoCheckAddress(FIFO_USER_01)) swiWaitForVBlank();
	arm7comm = (arm7comm_t*)fifoGetAddress(FIFO_USER_01);

	
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

	arm7comm->firmwareId = getFirmwareType();
	arm7comm->code = 2;
	fifoSendValue32(FIFO_USER_01,0);
	while (1) swiWaitForVBlank();
}


