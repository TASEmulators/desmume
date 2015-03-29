/* 	Matrix Stack test

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

GL_MATRIX_MODE_ENUM modes[4] = {GL_PROJECTION, GL_POSITION, GL_MODELVIEW, GL_TEXTURE};
char	*modesTXT[4] = {"PROJECTION", "POSITION", "MODELVIEW", "TEXTURE"};

PrintConsole *scr = consoleDemoInit();

s32 POPstep = 0;
u32 mode = 0;
u32 POPs = 0, PUSHes = 0;
//---------------------------------------------------------------------------------
int main(void) {
//---------------------------------------------------------------------------------
	int keys;
	videoSetModeSub(MODE_0_2D);
	videoSetMode(MODE_0_3D);
	
	vramSetBankA(VRAM_A_MAIN_BG);
	vramSetBankC(VRAM_C_SUB_BG);

	consoleInit(scr, 3, BgType_Text4bpp, BgSize_T_256x256, 31, 0, false, true);
	
	glInit();
	glMatrixMode(GL_PROJECTION);
	glFlush(0);
	
	POPstep=0;
	mode = 0;
	POPs = 0;
	PUSHes = 0;

	while(1) {
		scanKeys();
		keys = keysDown();
		if(keys & KEY_A) 
		{
			glPushMatrix();
			PUSHes++;
		}
		if(keys & KEY_B) 
		{
			glPopMatrix(POPstep);
			POPs++;
		}
		if(keys & KEY_X)
		{
			mode++;
			if (mode > 3) mode = 0;
			glMatrixMode(modes[mode]);
		}
		
		if(keys & KEY_UP)
		{
			if (POPstep < 31) POPstep += 1;
		}
		if(keys & KEY_DOWN)
		{
			if (POPstep > -30) POPstep -= 1;
		}
			
		consoleClear();
		iprintf("Matrix mode = %i:%s\n", mode, modesTXT[mode]);
		iprintf("\n");
		iprintf("GFX STATUS  = 0x%08X\n", GFX_STATUS);
		iprintf("\t - Pos&Vec level  = %i\n", (GFX_STATUS>>8)&0x1F);
		iprintf("\t - Proj.   level  = %i\n", (GFX_STATUS>>13)&0x01);
		iprintf("\t - Error %s\n", (GFX_STATUS>>15)&0x01?"YES":"no");
		iprintf("\n");
		iprintf("POP step    = %i\n", POPstep);
		iprintf("POP count    = %i\n", POPs);
		iprintf("PUSH count   = %i\n", PUSHes);
		
		iprintf("\n");
		iprintf("A\t - PUSH\n");
		iprintf("B\t - POP\n");
		iprintf("UP\t- POP increment step\n");
		iprintf("DOWN - POP decrement step\n");
		iprintf("X\t - Change mode\n");
		swiWaitForVBlank();
	}

	return 0;
}
