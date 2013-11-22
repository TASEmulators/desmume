/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2009 DeSmuME team

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

#include "CWindow.h"
#include "IORegView.h"
#include <commctrl.h>
#include "debug.h"
#include "resource.h"
#include "../MMU.h"
#include "../armcpu.h"

//this message is only supported in vista so folks sdk might not have it.
//therefore we shall declare it here
#ifndef RB_SETBANDWIDTH
#define RB_SETBANDWIDTH     (WM_USER + 44)   // set width for docked band
#endif

/*--------------------------------------------------------------------------*/

enum EIORegType
{
	ListEnd = 0,
	AllRegs,
	CatBegin,
	MMIOReg,
	CP15Reg
};

typedef struct _IORegBitfield
{
	char name[64];
	int shift;
	int nbits;
} IORegBitfield;

typedef struct _IOReg
{
	EIORegType type;
	char name[32];
	u32 address;
	int size;			// for AllRegs: total number of regs; for CatBegin: number of regs in category
	int numBitfields;
	IORegBitfield bitfields[32];
} IOReg;

IOReg IORegs9[] = {
	{AllRegs, "All registers", 0, 80, 0, {{0}}},

	{CatBegin, "General video registers", 0, 1, 0, {{0}}},
		{MMIOReg, "DISPSTAT", 0x04000004, 2, 8,    {{"VBlank",0,1},{"HBlank",1,1},{"VCount match",2,1},{"Enable VBlank IRQ",3,1},
													{"Enable HBlank IRQ",4,1},{"Enable VCount match IRQ",5,1},{"VCount MSb",7,1},{"VCount LSb's",8,8}}},
		{MMIOReg, "VCOUNT", 0x04000006, 2, 1, {{"VCount",0,9}}},

	{CatBegin, "Video engine A registers", 0, 37, 0, {{0}}},
		{MMIOReg, "[A]DISPCNT", 0x04000000, 4, 23,  {{"BG mode",0,3},{"BG0 -> 3D",3,1},{"Tile OBJ mapping",4,1},{"Bitmap OBJ 2D size",5,1},
													{"Bitmap OBJ mapping",6,1},{"Forced blank",7,1},{"Display BG0",8,1},{"Display BG1",9,1},
													{"Display BG2",10,1},{"Display BG3",11,1},{"Display OBJ",12,1},{"Display window 0",13,1},
													{"Display window 1",14,1},{"Display OBJ window",15,1},{"Display mode",16,2},{"VRAM bank",18,2},
													{"Tile OBJ 1D boundary",20,2},{"Bitmap OBJ 1D boundary",22,1},{"Process OBJs during HBlank",23,1},{"Character base",24,3},
													{"Screen base",27,3},{"Enable BG ext. palettes",30,1},{"Enable OBJ ext. palettes",31,1}}},
		{MMIOReg, "[A]BG0CNT", 0x04000008, 2, 7,   {{"Priority",0,2},{"Character base",2,4},{"Mosaic",6,1},{"Palette mode",7,1},
													{"Screen base",8,5},{"Ext. palette slot",13,1},{"Screen size",14,2}}},
		{MMIOReg, "[A]BG1CNT", 0x0400000A, 2, 7,   {{"Priority",0,2},{"Character base",2,4},{"Mosaic",6,1},{"Palette mode",7,1},
													{"Screen base",8,5},{"Ext. palette slot",13,1},{"Screen size",14,2}}},
		{MMIOReg, "[A]BG2CNT", 0x0400000C, 2, 7,   {{"Priority",0,2},{"Character base",2,4},{"Mosaic",6,1},{"Palette mode",7,1},
													{"Screen base",8,5},{"Display area overflow",13,1},{"Screen size",14,2}}},
		{MMIOReg, "[A]BG3CNT", 0x0400000E, 2, 7,   {{"Priority",0,2},{"Character base",2,4},{"Mosaic",6,1},{"Palette mode",7,1},
													{"Screen base",8,5},{"Display area overflow",13,1},{"Screen size",14,2}}},
		{MMIOReg, "[A]BG0HOFS", 0x04000010, 2, 1, {{"Offset",0,9}}},
		{MMIOReg, "[A]BG0VOFS", 0x04000012, 2, 1, {{"Offset",0,9}}},
		{MMIOReg, "[A]BG1HOFS", 0x04000014, 2, 1, {{"Offset",0,9}}},
		{MMIOReg, "[A]BG1VOFS", 0x04000016, 2, 1, {{"Offset",0,9}}},
		{MMIOReg, "[A]BG2HOFS", 0x04000018, 2, 1, {{"Offset",0,9}}},
		{MMIOReg, "[A]BG2VOFS", 0x0400001A, 2, 1, {{"Offset",0,9}}},
		{MMIOReg, "[A]BG3HOFS", 0x0400001C, 2, 1, {{"Offset",0,9}}},
		{MMIOReg, "[A]BG3VOFS", 0x0400001E, 2, 1, {{"Offset",0,9}}},
		{MMIOReg, "[A]BG2PA", 0x04000020, 2, 3, {{"Fractional part",0,8},{"Integer part",8,7},{"Sign",15,1}}},
		{MMIOReg, "[A]BG2PB", 0x04000022, 2, 3, {{"Fractional part",0,8},{"Integer part",8,7},{"Sign",15,1}}},
		{MMIOReg, "[A]BG2PC", 0x04000024, 2, 3, {{"Fractional part",0,8},{"Integer part",8,7},{"Sign",15,1}}},
		{MMIOReg, "[A]BG2PD", 0x04000026, 2, 3, {{"Fractional part",0,8},{"Integer part",8,7},{"Sign",15,1}}},
		{MMIOReg, "[A]BG2X", 0x04000028, 4, 3, {{"Fractional part",0,8},{"Integer part",8,19},{"Sign",27,1}}},
		{MMIOReg, "[A]BG2Y", 0x0400002C, 4, 3, {{"Fractional part",0,8},{"Integer part",8,19},{"Sign",27,1}}},
		{MMIOReg, "[A]BG3PA", 0x04000030, 2, 3, {{"Fractional part",0,8},{"Integer part",8,7},{"Sign",15,1}}},
		{MMIOReg, "[A]BG3PB", 0x04000032, 2, 3, {{"Fractional part",0,8},{"Integer part",8,7},{"Sign",15,1}}},
		{MMIOReg, "[A]BG3PC", 0x04000034, 2, 3, {{"Fractional part",0,8},{"Integer part",8,7},{"Sign",15,1}}},
		{MMIOReg, "[A]BG3PD", 0x04000036, 2, 3, {{"Fractional part",0,8},{"Integer part",8,7},{"Sign",15,1}}},
		{MMIOReg, "[A]BG3X", 0x04000038, 4, 3, {{"Fractional part",0,8},{"Integer part",8,19},{"Sign",27,1}}},
		{MMIOReg, "[A]BG3Y", 0x0400003C, 4, 3, {{"Fractional part",0,8},{"Integer part",8,19},{"Sign",27,1}}},
		{MMIOReg, "[A]WIN0H", 0x04000040, 2, 2, {{"X2",0,8},{"X1",8,8}}},
		{MMIOReg, "[A]WIN1H", 0x04000042, 2, 2, {{"X2",0,8},{"X1",8,8}}},
		{MMIOReg, "[A]WIN0V", 0x04000044, 2, 2, {{"Y2",0,8},{"Y1",8,8}}},
		{MMIOReg, "[A]WIN1V", 0x04000046, 2, 2, {{"Y2",0,8},{"Y1",8,8}}},
		{MMIOReg, "[A]WININ", 0x04000048, 2, 12,   {{"Enable WIN0 on BG0",0,1},{"Enable WIN0 on BG1",1,1},{"Enable WIN0 on BG2",2,1},{"Enable WIN0 on BG3",3,1},
													{"Enable WIN0 on OBJ",4,1},{"Enable color effect on WIN0",5,1},{"Enable WIN1 on BG0",8,1},{"Enable WIN1 on BG1",9,1},
													{"Enable WIN1 on BG2",10,1},{"Enable WIN1 on BG3",11,1},{"Enable WIN1 on OBJ",12,1},{"Enable color effect on WIN1",13,1}}},
		{MMIOReg, "[A]WINOUT", 0x04000048, 2, 12,  {{"Enable win. out. on BG0",0,1},{"Enable win. out. on BG1",1,1},{"Enable win. out. on BG2",2,1},{"Enable win. out. on BG3",3,1},
													{"Enable win. out. on OBJ",4,1},{"Enable color effect on win. out.",5,1},{"Enable OBJ win. on BG0",8,1},{"Enable OBJ win. on BG1",9,1},
													{"Enable OBJ win. on BG2",10,1},{"Enable OBJ win. on BG3",11,1},{"Enable OBJ win. on OBJ",12,1},{"Enable color effect on OBJ win.",13,1}}},
		{MMIOReg, "[A]MOSAIC", 0x0400004C, 2, 4, {{"BG mosaic width",0,4},{"BG mosaic height",4,4},{"OBJ mosaic width",8,4},{"OBJ mosaic height",12,4}}},
		{MMIOReg, "[A]BLDCNT", 0x04000050, 2, 13,  {{"BG0 -> 1st target",0,1},{"BG1 -> 1st target",1,1},{"BG2 -> 1st target",2,1},{"BG3 -> 1st target",3,1},
													{"OBJ -> 1st target",4,1},{"Backdrop -> 1st target",5,1},{"Color effect",6,2},{"BG0 -> 2nd target",8,1},
													{"BG1 -> 2nd target",9,1},{"BG2 -> 2nd target",10,1},{"BG3 -> 2nd target",11,1},{"OBJ -> 2nd target",12,1},
													{"Backdrop -> 2nd target",13,1}}},
		{MMIOReg, "[A]BLDALPHA", 0x04000052, 2, 2, {{"EVA",0,5},{"EVB",8,5}}},
		{MMIOReg, "[A]BLDY", 0x04000054, 2, 1, {{"EVY",0,5}}},
		{MMIOReg, "DISPCAPCNT", 0x04000064, 4, 10, {{"EVA",0,5},{"EVB",8,5},{"VRAM write bank",16,2},{"VRAM write offset",18,2},
													{"Capture size",20,2},{"Source A",24,1},{"Source B",25,1},{"VRAM read offset",26,2},
													{"Capture source",29,2},{"Capture enable",31,1}}},
		{MMIOReg, "[A]MASTER_BRIGHT", 0x0400006C, 2, 2, {{"Factor",0,5},{"Mode",14,2}}},

		
	{CatBegin, "Video engine B registers", 0, 36, 0, {{0}}},
		{MMIOReg, "[B]DISPCNT", 0x04001000, 4, 18,  {{"BG mode",0,3},{"Tile OBJ mapping",4,1},{"Bitmap OBJ 2D size",5,1},{"Bitmap OBJ mapping",6,1},
													{"Forced blank",7,1},{"Display BG0",8,1},{"Display BG1",9,1},{"Display BG2",10,1},
													{"Display BG3",11,1},{"Display OBJ",12,1},{"Display window 0",13,1},{"Display window 1",14,1},
													{"Display OBJ window",15,1},{"Display mode",16,2},{"Tile OBJ 1D boundary",20,2},{"Process OBJs during HBlank",23,1},
													{"Enable BG ext. palettes",30,1},{"Enable OBJ ext. palettes",31,1}}},
		{MMIOReg, "[B]BG0CNT", 0x04001008, 2, 7,   {{"Priority",0,2},{"Character base",2,4},{"Mosaic",6,1},{"Palette mode",7,1},
													{"Screen base",8,5},{"Ext. palette slot",13,1},{"Screen size",14,2}}},
		{MMIOReg, "[B]BG1CNT", 0x0400100A, 2, 7,   {{"Priority",0,2},{"Character base",2,4},{"Mosaic",6,1},{"Palette mode",7,1},
													{"Screen base",8,5},{"Ext. palette slot",13,1},{"Screen size",14,2}}},
		{MMIOReg, "[B]BG2CNT", 0x0400100C, 2, 7,   {{"Priority",0,2},{"Character base",2,4},{"Mosaic",6,1},{"Palette mode",7,1},
													{"Screen base",8,5},{"Display area overflow",13,1},{"Screen size",14,2}}},
		{MMIOReg, "[B]BG3CNT", 0x0400100E, 2, 7,   {{"Priority",0,2},{"Character base",2,4},{"Mosaic",6,1},{"Palette mode",7,1},
													{"Screen base",8,5},{"Display area overflow",13,1},{"Screen size",14,2}}},
		{MMIOReg, "[B]BG0HOFS", 0x04001010, 2, 1, {{"Offset",0,9}}},
		{MMIOReg, "[B]BG0VOFS", 0x04001012, 2, 1, {{"Offset",0,9}}},
		{MMIOReg, "[B]BG1HOFS", 0x04001014, 2, 1, {{"Offset",0,9}}},
		{MMIOReg, "[B]BG1VOFS", 0x04001016, 2, 1, {{"Offset",0,9}}},
		{MMIOReg, "[B]BG2HOFS", 0x04001018, 2, 1, {{"Offset",0,9}}},
		{MMIOReg, "[B]BG2VOFS", 0x0400101A, 2, 1, {{"Offset",0,9}}},
		{MMIOReg, "[B]BG3HOFS", 0x0400101C, 2, 1, {{"Offset",0,9}}},
		{MMIOReg, "[B]BG3VOFS", 0x0400101E, 2, 1, {{"Offset",0,9}}},
		{MMIOReg, "[B]BG2PA", 0x04001020, 2, 3, {{"Fractional part",0,8},{"Integer part",8,7},{"Sign",15,1}}},
		{MMIOReg, "[B]BG2PB", 0x04001022, 2, 3, {{"Fractional part",0,8},{"Integer part",8,7},{"Sign",15,1}}},
		{MMIOReg, "[B]BG2PC", 0x04001024, 2, 3, {{"Fractional part",0,8},{"Integer part",8,7},{"Sign",15,1}}},
		{MMIOReg, "[B]BG2PD", 0x04001026, 2, 3, {{"Fractional part",0,8},{"Integer part",8,7},{"Sign",15,1}}},
		{MMIOReg, "[B]BG2X", 0x04001028, 4, 3, {{"Fractional part",0,8},{"Integer part",8,19},{"Sign",27,1}}},
		{MMIOReg, "[B]BG2Y", 0x0400102C, 4, 3, {{"Fractional part",0,8},{"Integer part",8,19},{"Sign",27,1}}},
		{MMIOReg, "[B]BG3PA", 0x04001030, 2, 3, {{"Fractional part",0,8},{"Integer part",8,7},{"Sign",15,1}}},
		{MMIOReg, "[B]BG3PB", 0x04001032, 2, 3, {{"Fractional part",0,8},{"Integer part",8,7},{"Sign",15,1}}},
		{MMIOReg, "[B]BG3PC", 0x04001034, 2, 3, {{"Fractional part",0,8},{"Integer part",8,7},{"Sign",15,1}}},
		{MMIOReg, "[B]BG3PD", 0x04001036, 2, 3, {{"Fractional part",0,8},{"Integer part",8,7},{"Sign",15,1}}},
		{MMIOReg, "[B]BG3X", 0x04001038, 4, 3, {{"Fractional part",0,8},{"Integer part",8,19},{"Sign",27,1}}},
		{MMIOReg, "[B]BG3Y", 0x0400103C, 4, 3, {{"Fractional part",0,8},{"Integer part",8,19},{"Sign",27,1}}},
		{MMIOReg, "[B]WIN0H", 0x04001040, 2, 2, {{"X2",0,8},{"X1",8,8}}},
		{MMIOReg, "[B]WIN1H", 0x04001042, 2, 2, {{"X2",0,8},{"X1",8,8}}},
		{MMIOReg, "[B]WIN0V", 0x04001044, 2, 2, {{"Y2",0,8},{"Y1",8,8}}},
		{MMIOReg, "[B]WIN1V", 0x04001046, 2, 2, {{"Y2",0,8},{"Y1",8,8}}},
		{MMIOReg, "[B]WININ", 0x04001048, 2, 12,   {{"Enable WIN0 on BG0",0,1},{"Enable WIN0 on BG1",1,1},{"Enable WIN0 on BG2",2,1},{"Enable WIN0 on BG3",3,1},
													{"Enable WIN0 on OBJ",4,1},{"Enable color effect on WIN0",5,1},{"Enable WIN1 on BG0",8,1},{"Enable WIN1 on BG1",9,1},
													{"Enable WIN1 on BG2",10,1},{"Enable WIN1 on BG3",11,1},{"Enable WIN1 on OBJ",12,1},{"Enable color effect on WIN1",13,1}}},
		{MMIOReg, "[B]WINOUT", 0x04001048, 2, 12,  {{"Enable win. out. on BG0",0,1},{"Enable win. out. on BG1",1,1},{"Enable win. out. on BG2",2,1},{"Enable win. out. on BG3",3,1},
													{"Enable win. out. on OBJ",4,1},{"Enable color effect on win. out.",5,1},{"Enable OBJ win. on BG0",8,1},{"Enable OBJ win. on BG1",9,1},
													{"Enable OBJ win. on BG2",10,1},{"Enable OBJ win. on BG3",11,1},{"Enable OBJ win. on OBJ",12,1},{"Enable color effect on OBJ win.",13,1}}},
		{MMIOReg, "[B]MOSAIC", 0x0400104C, 2, 4, {{"BG mosaic width",0,4},{"BG mosaic height",4,4},{"OBJ mosaic width",8,4},{"OBJ mosaic height",12,4}}},
		{MMIOReg, "[B]BLDCNT", 0x04001050, 2, 13,  {{"BG0 -> 1st target",0,1},{"BG1 -> 1st target",1,1},{"BG2 -> 1st target",2,1},{"BG3 -> 1st target",3,1},
													{"OBJ -> 1st target",4,1},{"Backdrop -> 1st target",5,1},{"Color effect",6,2},{"BG0 -> 2nd target",8,1},
													{"BG1 -> 2nd target",9,1},{"BG2 -> 2nd target",10,1},{"BG3 -> 2nd target",11,1},{"OBJ -> 2nd target",12,1},
													{"Backdrop -> 2nd target",13,1}}},
		{MMIOReg, "[B]BLDALPHA", 0x04001052, 2, 2, {{"EVA",0,5},{"EVB",8,5}}},
		{MMIOReg, "[B]BLDY", 0x04001054, 2, 1, {{"EVY",0,5}}},
		{MMIOReg, "[B]MASTER_BRIGHT", 0x0400106C, 2, 2, {{"Factor",0,5},{"Mode",14,2}}},
		
		
	{CatBegin, "3D video engine registers", 0, 1, 0, {{0}}},
		{MMIOReg, "GXSTAT", 0x04000600, 4, 12, {{"Box/Pos/Vec-test busy",0,1},{"Box-test result",1,1},{"Pos&Vec mtx stack level",8,5},{"Proj mtx stack level",13,1},
												{"Mtx stack busy",14,1},{"Mtx stack over/under-flow",15,1},{"GX FIFO level",16,9},{"GX FIFO full",24,1},
												{"GX FIFO less than half full",25,1},{"GX FIFO empty",26,1},{"GX busy",27,1},{"GX FIFO IRQ condition",30,2}}},

	{CatBegin, "DMA registers", 0, 4, 0, {{0}}},
		{MMIOReg, "DMA0SAD", REG_DMA0SAD, 4, 1, {{"Value",0,28}}},
		{MMIOReg, "DMA0DAD", REG_DMA0DAD, 4, 1, {{"Value",0,28}}},
		{MMIOReg, "DMA0CNT", REG_DMA0CNTL, 4, 8, {{"Word Count",0,21}, {"Dest update method",21,2}, {"Src  update method",23,2}, {"Repeat Flag",25,1}, {"32bit Width Enable",26,1},{"Start Mode",27,3}, {"IRQ Enable",30,1}, {"Enabled",31,1}}},
		{MMIOReg, "DMA1SAD", REG_DMA1SAD, 4, 1, {{"Value",0,28}}},
		{MMIOReg, "DMA1DAD", REG_DMA1DAD, 4, 1, {{"Value",0,28}}},
		{MMIOReg, "DMA1CNT", REG_DMA1CNTL, 4, 8, {{"Word Count",0,21}, {"Dest update method",21,2}, {"Src  update method",23,2}, {"Repeat Flag",25,1}, {"32bit Width Enable",26,1},{"Start Mode",27,3}, {"IRQ Enable",30,1}, {"Enabled",31,1}}},
		{MMIOReg, "DMA2SAD", REG_DMA2SAD, 4, 1, {{"Value",0,28}}},
		{MMIOReg, "DMA2DAD", REG_DMA2DAD, 4, 1, {{"Value",0,28}}},
		{MMIOReg, "DMA2CNT", REG_DMA2CNTL, 4, 8, {{"Word Count",0,21}, {"Dest update method",21,2}, {"Src  update method",23,2}, {"Repeat Flag",25,1}, {"32bit Width Enable",26,1},{"Start Mode",27,3}, {"IRQ Enable",30,1}, {"Enabled",31,1}}},
		{MMIOReg, "DMA3SAD", REG_DMA3SAD, 4, 1, {{"Value",0,28}}},
		{MMIOReg, "DMA3DAD", REG_DMA3DAD, 4, 1, {{"Value",0,28}}},
		{MMIOReg, "DMA3CNT", REG_DMA3CNTL, 4, 8, {{"Word Count",0,21}, {"Dest update method",21,2}, {"Src  update method",23,2}, {"Repeat Flag",25,1}, {"32bit Width Enable",26,1},{"Start Mode",27,3}, {"IRQ Enable",30,1}, {"Enabled",31,1}}},
		
		
		/*{CatBegin, "Video engine B registers", 0, 36, 0, {{0}}},
		{MMIOReg, "[B]DISPCNT", 0x04001000, 4, 18,  {{"BG mode",0,3},{"Tile OBJ mapping",4,1},{"Bitmap OBJ 2D size",5,1},{"Bitmap OBJ mapping",6,1},
													{"Forced blank",7,1},{"Display BG0",8,1},{"Display BG1",9,1},{"Display BG2",10,1},
													{"Display BG3",11,1},{"Display OBJ",12,1},{"Display window 0",13,1},{"Display window 1",14,1},
													{"Display OBJ window",15,1},{"Display mode",16,2},{"Tile OBJ 1D boundary",20,2},{"Process OBJs during HBlank",23,1},
													{"Enable BG ext. palettes",30,1},{"Enable OBJ ext. palettes",31,1}}},
		{MMIOReg, "[B]BG0CNT", 0x04001008, 2, 7,   {{"Priority",0,2},{"Character base",2,4},{"Mosaic",6,1},{"Palette mode",7,1},
		*/
		
	{CatBegin, "IPC registers", 0, 2, 0, {{0}}},
		{MMIOReg, "IPCSYNC", 0x04000180, 2, 3, {{"Data input from remote",0,4},{"Data output to remote",8,4},{"Enable IRQ from remote",14,1}}},
		{MMIOReg, "IPCFIFOCNT", 0x04000184, 2, 8,  {{"Send FIFO empty",0,1},{"Send FIFO full",1,1},{"Enable send FIFO empty IRQ",2,1},{"Recv FIFO empty",8,1},
													{"Recv FIFO full",9,1},{"Enable recv FIFO not empty IRQ",10,1},{"Recv empty/Send full",14,1},{"Enable send/recv FIFO",15,1}}},

		
	{CatBegin, "IRQ control registers", 0, 3, 0, {{0}}},
		{MMIOReg, "IME", 0x04000208, 2, 1, {{"Enable IRQs",0,1}}},
		{MMIOReg, "IE", 0x04000210, 4, 19, {{"VBlank",0,1},{"HBlank",1,1},{"VCount match",2,1},{"Timer 0",3,1},
											{"Timer 1",4,1},{"Timer 2",5,1},{"Timer 3",6,1},{"DMA 0",8,1},
											{"DMA 1",9,1},{"DMA 2",10,1},{"DMA 3",11,1},{"Keypad",12,1},
											{"Game Pak",13,1},{"IPC sync",16,1},{"IPC send FIFO empty",17,1},{"IPC recv FIFO not empty",18,1},
											{"Gamecard transfer",19,1},{"Gamecard IREQ_MC",20,1},{"GX FIFO",21,1}}},
		{MMIOReg, "IF", 0x04000214, 4, 19, {{"VBlank",0,1},{"HBlank",1,1},{"VCount match",2,1},{"Timer 0",3,1},
											{"Timer 1",4,1},{"Timer 2",5,1},{"Timer 3",6,1},{"DMA 0",8,1},
											{"DMA 1",9,1},{"DMA 2",10,1},{"DMA 3",11,1},{"Keypad",12,1},
											{"Game Pak",13,1},{"IPC sync",16,1},{"IPC send FIFO empty",17,1},{"IPC recv FIFO not empty",18,1},
											{"Gamecard transfer",19,1},{"Gamecard IREQ_MC",20,1},{"GX FIFO",21,1}}},

	{ListEnd, "", 0, 0, 0, {{0}}}
};

IOReg IORegs7[] = {
	{AllRegs, "All registers", 0, 5, 0, {{0}}},

	
	{CatBegin, "IPC registers", 0, 2, 0, {{0}}},
		{MMIOReg, "IPCSYNC", 0x04000180, 2, 3, {{"Data input from remote",0,4},{"Data output to remote",8,4},{"Enable IRQ from remote",14,1}}},
		{MMIOReg, "IPCFIFOCNT", 0x04000184, 2, 8,  {{"Send FIFO empty",0,1},{"Send FIFO full",1,1},{"Enable send FIFO empty IRQ",2,1},{"Recv FIFO empty",8,1},
													{"Recv FIFO full",9,1},{"Enable recv FIFO not empty IRQ",10,1},{"Recv empty/Send full",14,1},{"Enable send/recv FIFO",15,1}}},

		
	{CatBegin, "IRQ control registers", 0, 3, 0, {{0}}},
		{MMIOReg, "IME", 0x04000208, 2, 1, {{"Enable IRQs",0,1}}},
		{MMIOReg, "IE", 0x04000210, 4, 22, {{"VBlank",0,1},{"HBlank",1,1},{"VCount match",2,1},{"Timer 0",3,1},
											{"Timer 1",4,1},{"Timer 2",5,1},{"Timer 3",6,1},{"RTC",7,1},
											{"DMA 0",8,1},{"DMA 1",9,1},{"DMA 2",10,1},{"DMA 3",11,1},
											{"Keypad",12,1},{"Game Pak",13,1},{"IPC sync",16,1},{"IPC send FIFO empty",17,1},
											{"IPC recv FIFO not empty",18,1},{"Gamecard transfer",19,1},{"Gamecard IREQ_MC",20,1},{"Lid opened",22,1},
											{"SPI bus",23,1},{"Wifi",24,1}}},
		{MMIOReg, "IF", 0x04000214, 4, 22, {{"VBlank",0,1},{"HBlank",1,1},{"VCount match",2,1},{"Timer 0",3,1},
											{"Timer 1",4,1},{"Timer 2",5,1},{"Timer 3",6,1},{"RTC",7,1},
											{"DMA 0",8,1},{"DMA 1",9,1},{"DMA 2",10,1},{"DMA 3",11,1},
											{"Keypad",12,1},{"Game Pak",13,1},{"IPC sync",16,1},{"IPC send FIFO empty",17,1},
											{"IPC recv FIFO not empty",18,1},{"Gamecard transfer",19,1},{"Gamecard IREQ_MC",20,1},{"Lid opened",22,1},
											{"SPI bus",23,1},{"Wifi",24,1}}},

	{CatBegin, "DMA registers", 0, 4, 0, {{0}}},
		{MMIOReg, "DMA0SAD", REG_DMA0SAD, 4, 1, {{"Value",0,27}}},
		{MMIOReg, "DMA0DAD", REG_DMA0DAD, 4, 1, {{"Value",0,27}}},
		{MMIOReg, "DMA0CNT", REG_DMA0CNTL, 4, 8, {{"Word Count",0,21}, {"Dest update method",21,2}, {"Src  update method",23,2}, {"Repeat Flag",25,1}, {"32bit Width Enable",26,1},{"Start Mode",28,2}, {"IRQ Enable",30,1}, {"Enabled",31,1}}},
		{MMIOReg, "DMA1SAD", REG_DMA1SAD, 4, 1, {{"Value",0,27}}},
		{MMIOReg, "DMA1DAD", REG_DMA1DAD, 4, 1, {{"Value",0,27}}},
		{MMIOReg, "DMA1CNT", REG_DMA1CNTL, 4, 8, {{"Word Count",0,21}, {"Dest update method",21,2}, {"Src  update method",23,2}, {"Repeat Flag",25,1}, {"32bit Width Enable",26,1},{"Start Mode",28,2}, {"IRQ Enable",30,1}, {"Enabled",31,1}}},
		{MMIOReg, "DMA2SAD", REG_DMA2SAD, 4, 1, {{"Value",0,27}}},
		{MMIOReg, "DMA2DAD", REG_DMA2DAD, 4, 1, {{"Value",0,27}}},
		{MMIOReg, "DMA2CNT", REG_DMA2CNTL, 4, 8, {{"Word Count",0,21}, {"Dest update method",21,2}, {"Src  update method",23,2}, {"Repeat Flag",25,1}, {"32bit Width Enable",26,1},{"Start Mode",28,2}, {"IRQ Enable",30,1}, {"Enabled",31,1}}},
		{MMIOReg, "DMA3SAD", REG_DMA3SAD, 4, 1, {{"Value",0,27}}},
		{MMIOReg, "DMA3DAD", REG_DMA3DAD, 4, 1, {{"Value",0,27}}},
		{MMIOReg, "DMA3CNT", REG_DMA3CNTL, 4, 8, {{"Word Count",0,21}, {"Dest update method",21,2}, {"Src  update method",23,2}, {"Repeat Flag",25,1}, {"32bit Width Enable",26,1},{"Start Mode",28,2}, {"IRQ Enable",30,1}, {"Enabled",31,1}}},
		
		
	{ListEnd, "", 0, 0, 0, {{0}}}
};

IOReg* IORegs[2] = {IORegs9, IORegs7};

static const int kXMargin = 5;
static const int kYMargin = 1;

typedef std::vector<CIORegView*> TIORegViewList;
static TIORegViewList liveIORegViews;
bool anyLiveIORegViews = false;

void RefreshAllIORegViews()
{
	//TODO - this is a placeholder for a more robust system for signalling changes to the IORegView for immediate display.
	//individual windows should subscribe to this service (so it doesnt always waste time)
	for(TIORegViewList::iterator it(liveIORegViews.begin()); it != liveIORegViews.end(); ++it)
	{
		(*it)->Refresh();
		UpdateWindow((*it)->hWnd); //TODO - base class should have this functionality
	}
}

/*--------------------------------------------------------------------------*/

CIORegView::CIORegView()
	: CToolWindow("DeSmuME_IORegView", IORegView_Proc, "I/O registers", 400, 400)
	, CPU(ARMCPU_ARM9)
	, Reg(0)
	, yoff(0)
{
	liveIORegViews.push_back(this);
	anyLiveIORegViews = true;
	PostInitialize();
}

CIORegView::~CIORegView()
{
	DestroyWindow(hWnd);
	UnregWndClass("DeSmuME_IORegView");
	//TODO - is this thread safe? which thread do these calls come from
	liveIORegViews.erase(std::find(liveIORegViews.begin(),liveIORegViews.end(),this));
	if(liveIORegViews.size()==0) anyLiveIORegViews = false;
}

/*--------------------------------------------------------------------------*/

void CIORegView::ChangeCPU(int cpu)
{
	CPU = cpu;

	SendMessage(hRegCombo, CB_RESETCONTENT, 0, 0);
	for (int i = 0; ; i++)
	{
		IOReg reg = IORegs[CPU][i];
		char str[128];

		if (reg.type == ListEnd)
			break;
		else
		switch (reg.type)
		{
		case AllRegs:
			sprintf(str, "* %s", reg.name);
			break;
		case CatBegin:
			sprintf(str, "** %s", reg.name);
			break;
		case MMIOReg:
			sprintf(str, "*** 0x%08X - %s", reg.address, reg.name);
			break;
		}

		SendMessage(hRegCombo, CB_ADDSTRING, 0, (LPARAM)str);
	}
	ChangeReg(0);
}

void CIORegView::ChangeReg(int reg)
{
	Reg = reg;
	IOReg _reg = IORegs[CPU][Reg];

	if ((_reg.type == AllRegs) || (_reg.type == CatBegin))
		numlines = 2 + _reg.size;
	else
		numlines = 3 + _reg.numBitfields;

	UpdateScrollbar();
	SendMessage(hRegCombo, CB_SETCURSEL, Reg, 0);
}

/*--------------------------------------------------------------------------*/

void CIORegView::UpdateScrollbar()
{
	if (maxlines < numlines)
	{
		RECT rc;
		BOOL oldenable = IsWindowEnabled(hScrollbar);
		int range;

		GetClientRect(hWnd, &rc);
		range = (numlines * lineheight) - (rc.bottom - rebarHeight);

		if (!oldenable)
		{
			EnableWindow(hScrollbar, TRUE);
			SendMessage(hScrollbar, SBM_SETRANGE, 0, range);
			SendMessage(hScrollbar, SBM_SETPOS, 0, TRUE);
			yoff = 0;
		}
		else
		{
			int pos = min(range, (int)SendMessage(hScrollbar, SBM_GETPOS, 0, 0));
			SendMessage(hScrollbar, SBM_SETRANGE, 0, range);
			SendMessage(hScrollbar, SBM_SETPOS, pos, TRUE);
			yoff = -pos;
		}
	}
	else
	{
		EnableWindow(hScrollbar, FALSE);
		yoff = 0;
	}
}

/*--------------------------------------------------------------------------*/

void IORegView_Paint(CIORegView* wnd, HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	HDC 			hDC; 
	PAINTSTRUCT 	ps;
	HDC 			hMemDC;
	HBITMAP			hMemBitmap;
	RECT			rc;
	HPEN 			pen;
	int				x, y, w, h;
	int				nameColWidth;
	int				curx, cury;
	SIZE			fontsize;
	IOReg			reg;
	char 			txt[80];

	GetClientRect(hWnd, &rc);
	x = 0;
	y = wnd->rebarHeight;
	w = rc.right - wnd->vsbWidth;
	h = rc.bottom - wnd->rebarHeight;
	curx = kXMargin; cury = wnd->yoff + kYMargin;

	hDC = BeginPaint(hWnd, &ps);

	hMemDC = CreateCompatibleDC(hDC);
	hMemBitmap = CreateCompatibleBitmap(hDC, w, h);
	SelectObject(hMemDC, hMemBitmap);

	pen = CreatePen(PS_SOLID, 1, RGB(210, 230, 255));
	SelectObject(hMemDC, pen);

	SelectObject(hMemDC, wnd->hFont);
	GetTextExtentPoint32(hMemDC, " ", 1, &fontsize);

	FillRect(hMemDC, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));

	reg = IORegs[wnd->CPU][wnd->Reg];
	if ((reg.type == AllRegs) || (reg.type == CatBegin))
	{
		nameColWidth = w - (kXMargin + (fontsize.cx*8) + kXMargin + 1 + kXMargin + kXMargin + 1 + kXMargin + (fontsize.cx*8) + kXMargin);

		DrawText(hMemDC, reg.name, curx, cury, w, fontsize.cy, DT_LEFT | DT_END_ELLIPSIS);
		cury += fontsize.cy + kYMargin;
		MoveToEx(hMemDC, 0, cury, NULL);
		LineTo(hMemDC, w, cury);
		cury ++;

		curx = kXMargin;
		DrawText(hMemDC, "Address", curx, cury+kYMargin, fontsize.cx*8, fontsize.cy, DT_LEFT);
		curx += (fontsize.cx*8) + kXMargin;
		MoveToEx(hMemDC, curx, cury, NULL);
		LineTo(hMemDC, curx, h);
		curx += 1 + kXMargin;
		DrawText(hMemDC, "Name", curx, cury+kYMargin, nameColWidth, fontsize.cy, DT_LEFT | DT_END_ELLIPSIS);
		curx += nameColWidth + kXMargin;
		MoveToEx(hMemDC, curx, cury, NULL);
		LineTo(hMemDC, curx, h);
		curx += 1 + kXMargin;
		DrawText(hMemDC, "Value", curx, cury+kYMargin, fontsize.cx*8, fontsize.cy, DT_RIGHT);

		cury += kYMargin + fontsize.cy + kYMargin;
		MoveToEx(hMemDC, 0, cury, NULL);
		LineTo(hMemDC, w, cury);
		cury += 1 + kYMargin;

		for (int i = wnd->Reg+1; ; i++)
		{
			IOReg curReg = IORegs[wnd->CPU][i];
			curx = kXMargin;

			if (curReg.type == CatBegin)
			{
				if (reg.type == AllRegs) continue;
				else break;
			}
			else if (curReg.type == ListEnd)
				break;
			else if (curReg.type == MMIOReg)
			{
				u32 val;

				sprintf(txt, "%08X", curReg.address);
				DrawText(hMemDC, txt, curx, cury, fontsize.cx*8, fontsize.cy, DT_LEFT);
				curx += (fontsize.cx*8) + kXMargin + 1 + kXMargin;

				DrawText(hMemDC, curReg.name, curx, cury, nameColWidth, fontsize.cy, DT_LEFT | DT_END_ELLIPSIS | DT_NOPREFIX);
				curx += nameColWidth + kXMargin + 1 + kXMargin;

				switch (curReg.size)
				{
				case 1:
					val = (u32)MMU_read8(wnd->CPU, curReg.address);
					sprintf(txt, "%02X", val);
					break;
				case 2:
					val = (u32)MMU_read16(wnd->CPU, curReg.address);
					sprintf(txt, "%04X", val);
					break;
				case 4:
					val = MMU_read32(wnd->CPU, curReg.address);
					sprintf(txt, "%08X", val);
					break;
				}
				DrawText(hMemDC, txt, curx, cury, fontsize.cx*8, fontsize.cy, DT_RIGHT);
			}

			cury += fontsize.cy + kYMargin;
			if (cury >= h) break;
			MoveToEx(hMemDC, 0, cury, NULL);
			LineTo(hMemDC, w, cury);
			cury += 1 + kYMargin;
		}
	}
	else
	{
		u32 val;
		nameColWidth = w - (kXMargin + (fontsize.cx*8) + kXMargin + 1 + kXMargin + kXMargin + 1 + kXMargin + (fontsize.cx*8) + kXMargin);

		sprintf(txt, "%08X - %s", reg.address, reg.name);
		DrawText(hMemDC, txt, curx, cury, w, fontsize.cy, DT_LEFT | DT_END_ELLIPSIS | DT_NOPREFIX);
		cury += fontsize.cy + kYMargin;
		MoveToEx(hMemDC, 0, cury, NULL);
		LineTo(hMemDC, w, cury);
		cury += 1 + kYMargin;

		switch (reg.size)
		{
		case 1:
			val = (u32)MMU_read8(wnd->CPU, reg.address);
			sprintf(txt, "Value:       %02X", val);
			break;
		case 2:
			val = (u32)MMU_read16(wnd->CPU, reg.address);
			sprintf(txt, "Value:     %04X", val);
			break;
		case 4:
			val = MMU_read32(wnd->CPU, reg.address);
			sprintf(txt, "Value: %08X", val);
			break;
		}
		DrawText(hMemDC, txt, curx, cury, w, fontsize.cy, DT_LEFT);
		cury += fontsize.cy + kYMargin;
		MoveToEx(hMemDC, 0, cury, NULL);
		LineTo(hMemDC, w, cury);
		cury ++;

		curx = kXMargin;
		DrawText(hMemDC, "Bits", curx, cury+kYMargin, fontsize.cx*8, fontsize.cy, DT_LEFT);
		curx += (fontsize.cx*8) + kXMargin;
		MoveToEx(hMemDC, curx, cury, NULL);
		LineTo(hMemDC, curx, h);
		curx += 1 + kXMargin;
		DrawText(hMemDC, "Description", curx, cury+kYMargin, nameColWidth, fontsize.cy, DT_LEFT | DT_END_ELLIPSIS);
		curx += nameColWidth + kXMargin;
		MoveToEx(hMemDC, curx, cury, NULL);
		LineTo(hMemDC, curx, h);
		curx += 1 + kXMargin;
		DrawText(hMemDC, "Value", curx, cury+kYMargin, fontsize.cx*8, fontsize.cy, DT_RIGHT);

		cury += kYMargin + fontsize.cy + kYMargin;
		MoveToEx(hMemDC, 0, cury, NULL);
		LineTo(hMemDC, w, cury);
		cury += 1 + kYMargin;

		for (int i = 0; i < reg.numBitfields; i++)
		{
			IORegBitfield bitfield = reg.bitfields[i];
			curx = kXMargin;

			if (bitfield.nbits > 1)
				sprintf(txt, "Bit%i-%i", bitfield.shift, bitfield.shift + bitfield.nbits - 1);
			else
				sprintf(txt, "Bit%i", bitfield.shift);
			DrawText(hMemDC, txt, curx, cury, fontsize.cx*8, fontsize.cy, DT_LEFT);
			curx += (fontsize.cx*8) + kXMargin + 1 + kXMargin;

			DrawText(hMemDC, bitfield.name, curx, cury, nameColWidth, fontsize.cy, DT_LEFT | DT_END_ELLIPSIS | DT_NOPREFIX);
			curx += nameColWidth + kXMargin + 1 + kXMargin;

			char bfpattern[8];
			sprintf(bfpattern, "%%0%iX", ((bitfield.nbits+3)&~3) >> 2);
			sprintf(txt, bfpattern, (val >> bitfield.shift) & ((1<<bitfield.nbits)-1));
			DrawText(hMemDC, txt, curx, cury, fontsize.cx*8, fontsize.cy, DT_RIGHT);

			cury += fontsize.cy + kYMargin;
			if (cury >= h) break;
			MoveToEx(hMemDC, 0, cury, NULL);
			LineTo(hMemDC, w, cury);
			cury += 1 + kYMargin;
		}
	}

	BitBlt(hDC, x, y, w, h, hMemDC, 0, 0, SRCCOPY);

	DeleteDC(hMemDC);
	DeleteObject(hMemBitmap);

	DeleteObject(pen);

	EndPaint(hWnd, &ps);
}

LRESULT CALLBACK IORegView_Proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	CIORegView* wnd = (CIORegView*)GetWindowLongPtr(hWnd, DWLP_USER);
	if ((wnd == NULL) && (uMsg != WM_CREATE))
		return DefWindowProc(hWnd, uMsg, wParam, lParam);

	switch (uMsg)
	{
	case WM_CREATE:
		{
			RECT rc;
			SIZE fontsize;

			// Retrieve the CIORegView instance passed upon window creation
			// and match it to the window
			wnd = (CIORegView*)((CREATESTRUCT*)lParam)->lpCreateParams;
			SetWindowLongPtr(hWnd, DWLP_USER, (LONG)wnd);

			// Create the fixed-pitch font
			wnd->hFont = CreateFont(16, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 
				OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, GetFontQuality(), FIXED_PITCH, "Courier New");

			// Create the vertical scrollbar
			// The sizing and positioning of the scrollbar is done in WM_SIZE messages
			wnd->vsbWidth = GetSystemMetrics(SM_CXVSCROLL);
			wnd->hScrollbar = CreateWindow("Scrollbar", "",
				WS_CHILD | WS_VISIBLE | WS_DISABLED | SBS_VERT,
				0, 0, 0, 0, hWnd, NULL, hAppInst, NULL);

			// Create the rebar that will hold all the controls
			wnd->hRebar = CreateWindowEx(WS_EX_TOOLWINDOW, REBARCLASSNAME, NULL, 
				WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | CCS_NODIVIDER | RBS_VARHEIGHT | RBS_BANDBORDERS, 
				0, 0, 0, 0, hWnd, NULL, hAppInst, NULL);

			// Create the CPU combobox and fill it
			wnd->hCPUCombo = CreateWindow("ComboBox", "",
				WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST, 
				0, 0, 0, 50, wnd->hRebar, (HMENU)IDC_CPU, hAppInst, NULL);

			SendMessage(wnd->hCPUCombo, WM_SETFONT, (WPARAM)wnd->hFont, TRUE);
			SendMessage(wnd->hCPUCombo, CB_ADDSTRING, 0, (LPARAM)"ARM9");
			SendMessage(wnd->hCPUCombo, CB_ADDSTRING, 0, (LPARAM)"ARM7");
			SendMessage(wnd->hCPUCombo, CB_SETCURSEL, 0, 0);

			// Create the reg combobox and fill it
			wnd->hRegCombo = CreateWindow("ComboBox", "",
				WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST, 
				0, 0, 0, 400, wnd->hRebar, (HMENU)IDC_IOREG, hAppInst, NULL);

			SendMessage(wnd->hRegCombo, WM_SETFONT, (WPARAM)wnd->hFont, TRUE);
			SendMessage(wnd->hRegCombo, CB_SETDROPPEDWIDTH, 300, 0);
			wnd->ChangeCPU(ARMCPU_ARM9);
			SendMessage(wnd->hRegCombo, CB_SETCURSEL, 0, 0);

			// Add all those nice controls to the rebar
			REBARBANDINFO rbBand = { 80 };

			rbBand.fMask = RBBIM_STYLE | RBBIM_TEXT | RBBIM_CHILD | RBBIM_CHILDSIZE | RBBIM_SIZE;
			rbBand.fStyle = RBBS_CHILDEDGE | RBBS_NOGRIPPER;

			GetWindowRect(wnd->hCPUCombo, &rc);
			rbBand.lpText = "CPU: ";
			rbBand.hwndChild = wnd->hCPUCombo;
			rbBand.cxMinChild = 0;
			rbBand.cyMinChild = rc.bottom - rc.top;
			rbBand.cx = 100;
			SendMessage(wnd->hRebar, RB_INSERTBAND, (WPARAM)-1, (LPARAM)&rbBand);

			GetWindowRect(wnd->hRegCombo, &rc);
			rbBand.lpText = "Registers: ";
			rbBand.hwndChild = wnd->hRegCombo;
			rbBand.cxMinChild = 0;
			rbBand.cyMinChild = rc.bottom - rc.top;
			rbBand.cx = 0;
			SendMessage(wnd->hRebar, RB_INSERTBAND, (WPARAM)-1, (LPARAM)&rbBand);

			GetWindowRect(wnd->hRebar, &rc);
			wnd->rebarHeight = rc.bottom - rc.top;

			GetFontSize(hWnd, wnd->hFont, &fontsize);
			wnd->lineheight = kYMargin + fontsize.cy + kYMargin + 1;
		}
		return 0;

	case WM_CLOSE:
		CloseToolWindow(wnd);
		return 0;

	case WM_SIZE:
		{
			RECT rc;

			// Resize and reposition the controls
			SetWindowPos(wnd->hRebar, NULL, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, SWP_NOZORDER | SWP_NOMOVE);

			GetClientRect(hWnd, &rc);
			SetWindowPos(wnd->hScrollbar, NULL, rc.right-wnd->vsbWidth, wnd->rebarHeight, wnd->vsbWidth, rc.bottom-wnd->rebarHeight, SWP_NOZORDER);

			// Keep the first rebar band width to a reasonable value
			SendMessage(wnd->hRebar, RB_SETBANDWIDTH, 0, 100);

			GetClientRect(hWnd, &rc);
			wnd->maxlines = (rc.bottom - wnd->rebarHeight) / wnd->lineheight;

			wnd->UpdateScrollbar();
			wnd->Refresh();
		}
		return 0;

	case WM_PAINT:
		IORegView_Paint(wnd, hWnd, wParam, lParam);
		return 0;

	case WM_VSCROLL:
		{
			int pos = SendMessage(wnd->hScrollbar, SBM_GETPOS, 0, 0);
			int minpos, maxpos;

			SendMessage(wnd->hScrollbar, SBM_GETRANGE, (WPARAM)&minpos, (LPARAM)&maxpos);

			switch(LOWORD(wParam))
			{
			case SB_LINEUP:
				pos = max(minpos, pos - 1);
				break;
			case SB_LINEDOWN:
				pos = min(maxpos, pos + 1);
				break;
			case SB_PAGEUP:
				pos = max(minpos, pos - wnd->lineheight);
				break;
			case SB_PAGEDOWN:
				pos = min(maxpos, pos + wnd->lineheight);
				break;
			case SB_THUMBTRACK:
			case SB_THUMBPOSITION:
				{
					SCROLLINFO si;
					
					ZeroMemory(&si, sizeof(si));
					si.cbSize = sizeof(si);
					si.fMask = SIF_TRACKPOS;

					SendMessage(wnd->hScrollbar, SBM_GETSCROLLINFO, 0, (LPARAM)&si);
					pos = si.nTrackPos;
				}
				break;
			}

			SendMessage(wnd->hScrollbar, SBM_SETPOS, pos, TRUE);
			wnd->yoff = -pos;
			wnd->Refresh();
		}
		return 0;

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_CPU:
			switch (HIWORD(wParam))
			{
			case CBN_SELCHANGE:
			case CBN_CLOSEUP:
				{
					int cpu = SendMessage(wnd->hCPUCombo, CB_GETCURSEL, 0, 0);
					if (cpu != wnd->CPU) 
					{
						wnd->ChangeCPU(cpu);
						wnd->Refresh();
					}
				}
				break;
			}
			break;

		case IDC_IOREG:
			switch (HIWORD(wParam))
			{
			case CBN_SELCHANGE:
			case CBN_CLOSEUP:
				{
					int reg = SendMessage(wnd->hRegCombo, CB_GETCURSEL, 0, 0);
					if (reg != wnd->Reg) 
					{
						wnd->ChangeReg(reg);
						wnd->Refresh();
					}
				}
				break;
			}
			break;
		}
		return 0;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

/*--------------------------------------------------------------------------*/
