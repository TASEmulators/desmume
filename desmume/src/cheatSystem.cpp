/*
	Copyright (C) 2009-2019 DeSmuME team

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

#include "cheatSystem.h"
#include "utils/bits.h"

#include "NDSSystem.h"
#include "mem.h"
#include "MMU.h"
#include "debug.h"

#ifndef _MSC_VER 
#include <stdint.h>
#endif

static const char hexValid[23] = {"0123456789ABCDEFabcdef"};

CHEATS *cheats = NULL;
CHEATSEARCH *cheatSearch = NULL;

static bool cheatsResetJit;

void CHEATS::clear()
{
	list.resize(0);
	currentGet = 0;
}

void CHEATS::init(char *path)
{
	clear();
	strcpy((char *)filename, path);

	load();
}

BOOL CHEATS::add(u8 size, u32 address, u32 val, char *description, BOOL enabled)
{
	size_t num = list.size();
	list.push_back(CHEATS_LIST());
	list[num].code[0][0] = address & 0x0FFFFFFF;
	list[num].code[0][1] = val;
	list[num].num = 1;
	list[num].type = 0;
	list[num].size = size;
	this->setDescription(description, num);
	list[num].enabled = enabled;
	return TRUE;
}

BOOL CHEATS::update(u8 size, u32 address, u32 val, char *description, BOOL enabled, u32 pos)
{
	if (pos >= list.size()) return FALSE;
	list[pos].code[0][0] = address & 0x0FFFFFFF;
	list[pos].code[0][1] = val;
	list[pos].num = 1;
	list[pos].type = 0;
	list[pos].size = size;
	this->setDescription(description, pos);
	list[pos].enabled = enabled;
	return TRUE;
}

BOOL CHEATS::move(u32 srcPos, u32 dstPos)
{
	if (srcPos >= list.size() || dstPos > list.size()) return false;
	if (srcPos < 0 || dstPos < 0) return false;

	// get the item to move
	CHEATS_LIST srcCheat = list[srcPos];
	// insert item in the new position
	list.insert(list.begin() + dstPos, srcCheat);
	// remove the original item
	if (dstPos < srcPos) srcPos++;
	list.erase(list.begin() + srcPos);

	return true;
}

#define CHEATLOG(...) 
//#define CHEATLOG(...) printf(__VA_ARGS__)

static void CheatWrite(int size, int proc, u32 addr, u32 val)
{
	bool dirty = true;

	bool isDangerous = false;
	if(addr >= 0x02000000 && addr < 0x02400000)
		isDangerous = true;

	if(isDangerous)
	{
		//test dirtiness
		if(size == 8) dirty = _MMU_read08(proc, MMU_AT_DEBUG, addr) != val;
		if(size == 16) dirty = _MMU_read16(proc, MMU_AT_DEBUG, addr) != val;
		if(size == 32) dirty = _MMU_read32(proc, MMU_AT_DEBUG, addr) != val;
	}

	if(!dirty) return;

	if(size == 8) _MMU_write08(proc, MMU_AT_DEBUG, addr, val);
	if(size == 16) _MMU_write16(proc, MMU_AT_DEBUG, addr, val);
	if(size == 32) _MMU_write32(proc, MMU_AT_DEBUG, addr, val);

	if(isDangerous)
		cheatsResetJit = true;
}


void CHEATS::ARparser(CHEATS_LIST& list)
{
	//primary organizational source (seems to be referenced by cheaters the most) - http://doc.kodewerx.org/hacking_nds.html
	//secondary clarification and details (for programmers) - http://problemkaputt.de/gbatek.htm#dscartcheatactionreplayds
	//more here: http://www.kodewerx.org/forum/viewtopic.php?t=98

	//general note: gbatek is more precise with the maths; it does not indicate any special wraparound/masking behaviour so it's assumed to be standard 32bit masking
	//general note: <gbatek>  For all word/halfword accesses, the address should be aligned accordingly -- OR ELSE WHAT? (probably the default behaviour of our MMU interface)

	//TODO: "Code Hacks" from kodewerx (seems frail, but we should at least detect them and print some diagnostics
	//TODO: v154 special stuff

	bool v154 = true; //on advice of power users, v154 is so old, we can assume all cheats use it
	bool vEmulator = true;

	struct {
		//LSB is 
		u32 status;

		struct {
			//backup copy of status managed by loop commands
			u32 status;

			//loop counter index
			u32 idx;

			//loop iterations goal (runs rpt+1 times generally)
			u32 iterations;
		
			//target of loop
			u32 top;
		} loop;

		//registers
		u32 offset, data;

		//nu fake registers
		int proc;

	} st;

	memset(&st,0,sizeof(st));
	st.proc = ARMCPU_ARM7;

	CHEATLOG("-----------------------------------\n");

	for (u32 i=0; i < list.num; i++)
	{
		const u32 hi = list.code[i][0];
		const u32 lo = list.code[i][1];

		CHEATLOG("executing [%02d] %08X %08X (ofs=%08X)\n",i, hi,lo, st.offset);

		//parse codes into types by kodewerx standards
		u32 type = list.code[i][0] >> 28;
		//these two are broken down into subtypes
		if(type == 0x0C || type == 0x0D)
			type = list.code[i][0] >> 24;

		//process current execution status:
		u32 statusSkip = st.status & 1;
		//<gbatek> The condition register is checked, for all code types but the D0, D1 and D2 code type
		//<gbatek> and for the C5 code type it's checked AFTER the counter has been incremented (so the counter is always incremented)
		//but first: we must run this for IFs regardless of the condition flag to track the nest level
		if(type >= 0x03 && type <= 0x0A)
		{
			//pretty wild guess as to how this is implemented (I could be off by one)
			//we begin by assuming the current status is disabled, since we wont have a chance to correct if this IF is not supposed to be evaluated
			st.status = (st.status<<1) | 1;
		}
		if(type == 0xD0 || type == 0xD1 || type == 0xD2) {}
		else if(type == 0xC5) {}
		else if(type == 0x0E)
		{
			if (statusSkip) {
				CHEATLOG(" (skip multiple lines!)\n");
				i += (lo + 7) / 8;
				continue;
			}
		}
		else
		{
			if(statusSkip) {
				CHEATLOG(" (skip!)\n");
				continue;
			}
		}

		u32 operand,x,y,z,addr;

		switch(type)
		{
		case 0x00:
			//(hi == 0x00000000) implies: "manual hook code" -- sometimes
			//maybe a type "M" code -- impossible to enter into desmume? not supported right now.
			//otherwise:

			//32-bit (Constant RAM Writes)
			//0XXXXXXX YYYYYYYY
			//Writes word YYYYYYYY to [XXXXXXX+offset].
			x = hi & 0x0FFFFFFF;
			y = lo;
			addr = x + st.offset;
			CheatWrite(32,st.proc,addr, y);
			break;
		
		case 0x01:
			//16-bit (Constant RAM Writes)
			//1XXXXXXX 0000YYYY
			//Writes halfword YYYY to [XXXXXXX+offset].
			x = hi & 0x0FFFFFFF;
			y = lo & 0xFFFF;
			addr = x + st.offset;
			CheatWrite(16,st.proc,addr, y);
			break;

		case 0x02:
			//8-bit (Constant RAM Writes)
			//2XXXXXXX 000000YY
			//Writes byte YY to [XXXXXXX+offset].
			x = hi & 0x0FFFFFFF;
			y = lo & 0xFF;
			addr = x + st.offset;
			CheatWrite(8,st.proc,addr, y);
			break;

		case 0x03:
			//Greater Than (Conditional 32-Bit Code Types)
			//3XXXXXXX YYYYYYYY
			//Checks if YYYYYYYY > (word at [XXXXXXX])
			//If not, the code(s) following this one are not executed (ie. execution status is set to false) until a code type D0 or D2 is encountered, or until the end of the code list is reached.
			x = hi & 0x0FFFFFFF;
			y = lo;
			if(v154) if(x == 0) x = st.offset;
			operand = _MMU_read32(st.proc,MMU_AT_DEBUG,x);
			if(y > operand) st.status &= ~1;
			break;

		case 0x04:
			//Less Than (Conditional 32-Bit Code Types)
			//4XXXXXXX YYYYYYYY
			//Checks if YYYYYYYY < (word at [XXXXXXX])
			//If not, the code(s) following this one are not executed (ie. execution status is set to false) until a code type D0 or D2 is encountered, or until the end of the code list is reached.
			x = hi & 0x0FFFFFFF;
			y = lo;
			if(v154) if(x == 0) x = st.offset;
			operand = _MMU_read32(st.proc,MMU_AT_DEBUG,x);
			if(y < operand) st.status &= ~1;
			break;

		case 0x05:
			//Equal To (Conditional 32-Bit Code Types)
			//5XXXXXXX YYYYYYYY
			//Checks if YYYYYYYY == (word at [XXXXXXX])
			//If not, the code(s) following this one are not executed (ie. execution status is set to false) until a code type D0 or D2 is encountered, or until the end of the code list is reached.
			x = hi & 0x0FFFFFFF;
			y = lo;
			if(v154) if(x == 0) x = st.offset;
			operand = _MMU_read32(st.proc,MMU_AT_DEBUG,x);
			if(y == operand) st.status &= ~1;
			break;

		case 0x06:
			//Not Equal To (Conditional 32-Bit Code Types)
			//6XXXXXXX YYYYYYYY
			//Checks if YYYYYYYY != (word at [XXXXXXX])
			//If not, the code(s) following this one are not executed (ie. execution status is set to false) until a code type D0 or D2 is encountered, or until the end of the code list is reached.
			x = hi & 0x0FFFFFFF;
			y = lo;
			if(v154) if(x == 0) x = st.offset;
			operand = _MMU_read32(st.proc,MMU_AT_DEBUG,x);
			if(y != operand) st.status &= ~1;
			break;

		case 0x07:
			//Greater Than (Conditional 16-Bit + Masking RAM "Writes") (but WRITING has nothing to do with this)
			//7XXXXXXX ZZZZYYYY
			//Checks if (YYYY) > (not (ZZZZ) & halfword at [XXXX]).
			//If not, the code(s) following this one are not executed (ie. execution status is set to false) until a code type D0 or D2 is encountered, or until the end of the code list is reached.
			x = hi & 0x0FFFFFFF;
			y = lo&0xFFFF;
			z = lo>>16;
			if(v154) if(x == 0) x = st.offset;
			operand = _MMU_read16(st.proc,MMU_AT_DEBUG,x);
			if(y > (u16)( (~z) & operand) ) st.status &= ~1;
			break;

		case 0x08:
			//Less Than (Conditional 16-Bit + Masking RAM "Writes") (but WRITING has nothing to do with this)
			//8XXXXXXX ZZZZYYYY
			//Checks if (YYYY) < (not (ZZZZ) & halfword at [XXXX]).
			//If not, the code(s) following this one are not executed (ie. execution status is set to false) until a code type D0 or D2 is encountered, or until the end of the code list is reached.
			x = hi & 0x0FFFFFFF;
			y = lo&0xFFFF;
			z = lo>>16;
			if(v154) if(x == 0) x = st.offset;
			operand = _MMU_read16(st.proc,MMU_AT_DEBUG,x);
			if(y < (u16)( (~z) & operand) ) st.status &= ~1;
			break;

		case 0x09:
			//Equal To (Conditional 16-Bit + Masking RAM "Writes") (but WRITING has nothing to do with this)
			//9XXXXXXX ZZZZYYYY
			//Checks if (YYYY) == (not (ZZZZ) & halfword at [XXXX]).
			//If not, the code(s) following this one are not executed (ie. execution status is set to false) until a code type D0 or D2 is encountered, or until the end of the code list is reached.
			x = hi & 0x0FFFFFFF;
			y = lo&0xFFFF;
			z = lo>>16;
			if(v154) if(x == 0) x = st.offset;
			operand = _MMU_read16(st.proc,MMU_AT_DEBUG,x);
			if(y == (u16)( (~z) & operand) ) st.status &= ~1;
			break;

		case 0x0A:
			//Not Equal To (Conditional 16-Bit + Masking RAM "Writes") (but WRITING has nothing to do with this)
			//AXXXXXXX ZZZZYYYY
			//Checks if (YYYY) != (not (ZZZZ) & halfword at [XXXX]).
			//If not, the code(s) following this one are not executed (ie. execution status is set to false) until a code type D0 or D2 is encountered, or until the end of the code list is reached.
			x = hi & 0x0FFFFFFF;
			y = lo&0xFFFF;
			z = lo>>16;
			if(v154) if(x == 0) x = st.offset;
			operand = _MMU_read16(st.proc,MMU_AT_DEBUG,x);
			if(y != (u16)( (~z) & operand) ) st.status &= ~1;
			break;

		case 0x0B:
			//Load offset (Offset Codes)
			//BXXXXXXX 00000000
			//Loads the 32-bit value into the 'offset'.
			//Offset = word at [0XXXXXXX + offset].
			x = hi & 0x0FFFFFFF;
			addr = x + st.offset;
			st.offset = _MMU_read32(st.proc,MMU_AT_DEBUG,addr);
			break;

		case 0xC0:
			//(Loop Code)
			//C0000000 YYYYYYYY 
			//This sets the 'Dx repeat value' to YYYYYYYY and saves the 'Dx nextcode to be executed' and the 'Dx execution status'. Repeat will be executed when a D1/D2 code is encountered.
			//When repeat is executed, the AR reloads the 'next code to be executed' and the 'execution status' from the Dx registers.
			//<gbatek> FOR loopcount=0 to YYYYYYYY  ;execute Y+1 times
			y = lo;
			st.loop.idx = 0; //<gbatek> any FOR statement does forcefully terminate any prior loop
			st.loop.iterations = y;
			st.loop.top = i; //current instruction is saved as top for branching back up
			//<gbatek> FOR does backup the current IF condidition flags, and NEXT does restore these flags
			st.loop.status = st.status;
			break;

		case 0xC4:
			//Rewrite Code (v1.54 only) (trainer toolkit codes)
			//<gbatek> offset = address of the C4000000 code
			//it seems this lets us rewrite the code at runtime. /his will be very difficult to emulate. 
			//But it would be possible: we could copy whenever it's activated and allow that to be rewritten
			//we would try to select a special sentinel pointer which couldn't be confused for a useful address
			if(!v154) break;
			CHEATLOG("Unsupported C4 code");
			break;

		case 0xC5:
			//If Counter  (trainer toolkit codes)
			//counter=counter+1, IF (counter AND YYYY) = XXXX ;v1.54
			//http://www.kodewerx.org/forum/viewtopic.php?t=98 has more details, but this can't be executed readily without adding something to the cheat engine
			if(!v154) break;
			CHEATLOG("Unsupported C5 code");
			break;

		case 0xC6:
			//Store Offset  (trainer toolkit codes)
			//<gbatek> C6000000 XXXXXXXX   [XXXXXXXX]=offset  
			if(!v154) break;
			x = lo;
			CheatWrite(32,st.proc,x, st.offset);
			break;

		case 0xD0:
			//Terminator (Special Codes)
			//D0000000 00000000
			//Loads the previous execution status. If none exists, the execution status stays at 'execute codes'
			
			//wild guess as to fine details of how this is implemented
			st.status >>= 1;
			//"If none exists, the execution status stays at 'execute codes'." 
			//0 will be shifted in, so execution will always proceed
			//in other words, a stack underflow results in the original state of execution

			break;

		case 0xD1:
			//Loop execute variant (Special Codes)
			//D1000000 00000000 
			//Executes the next block of codes 'n' times (specified by the 0x0C codetype), but doesn't clear the Dx register upon completion.
			//<gbatek> FOR does backup the current IF condidition flags, and NEXT does restore these flags
			st.status = st.loop.status;
			if(st.loop.idx < st.loop.iterations)
			{
				st.loop.idx++;
				i = st.loop.top;
			}
			break;

		case 0xD2:
			//Loop Execute Variant/ Full Terminator (Special Codes)
			//D2000000 00000000 
			//Executes the next block of codes 'n' times (specified by the 0x0C codetype), and clears all temporary data. (i.e. execution status, offsets, code C settings, etc.)
			//This code can also be used as a full terminator, giving the same effects to any block of code. 
			//<gbatek> FOR does backup the current IF condidition flags, and NEXT does restore these flags
			st.status = st.loop.status;
			if(st.loop.idx < st.loop.iterations)
			{
				st.loop.idx++;
				i = st.loop.top;
			}
			else
			{
				//<gbatek> The NEXT+FLUSH command does (after finishing the loop) reset offset=0, datareg=0, and does clear all condition flags, so further ENDIF(s) aren't required after the loop.
				memset(&st,0,sizeof(st));
				st.proc = ARMCPU_ARM7;
			}
			break;

		case 0xD3: 
			//Set offset (Offset Codes)
			//D3000000 XXXXXXXX
			//Sets the offset value to XXXXXXXX.
			x = lo;
			st.offset = x;
			break;

		case 0xD4:
			//Add Value (Data Register Codes)
			//D4000000 XXXXXXXX 
			//Adds 'XXXXXXXX' to the data register used by codetypes 0xD6 - 0xDB.
			//<gbatek> datareg = datareg + XXXXXXXX
			x = lo;
			st.data += x;
			break;

		case 0xD5:
			//Set Value (Data Register Codes)
			//D5000000 XXXXXXXX 
			//Set 'XXXXXXXX' to the data register used by code types 0xD6 - 0xD8.
			x = lo;
			st.data = x;
			break;

		case 0xD6:
			//32-Bit Incrementive Write (Data Register Codes)
			//D6000000 XXXXXXXX 
			//Writes the 'Dx data' word to [XXXXXXXX+offset], and increments the offset by 4.
			//<gbatek> word[XXXXXXXX+offset]=datareg, offset=offset+4
			x = lo;
			addr = x + st.offset;
			CheatWrite(32,st.proc,addr, st.data);
			st.offset += 4;
			break;

		case 0xD7:
			//16-Bit Incrementive Write (Data Register Codes)
			//D7000000 XXXXXXXX 
			//Writes the 'Dx data' halfword to [XXXXXXXX+offset], and increments the offset by 2.
			//<gbatek> half[XXXXXXXX+offset]=datareg, offset=offset+2
			x = lo;
			addr = x + st.offset;
			CheatWrite(16,st.proc,addr, st.data);
			st.offset += 2;
			break;

		case 0xD8:
			//8-Bit Incrementive Write (Data Register Codes)
			//D8000000 XXXXXXXX 
			//Writes the 'Dx data' byte to [XXXXXXXX+offset], and increments the offset by 1.
			//<gbatek> byte[XXXXXXXX+offset]=datareg, offset=offset+1
			x = lo;
			addr = x + st.offset;
			CheatWrite(8,st.proc,addr, st.data);
			st.offset += 1;
			break;

		case 0xD9:
			//32-Bit Load (Data Register Codes)
			//D9000000 XXXXXXXX 
			//Loads the word at [XXXXXXXX+offset] and stores it in the'Dx data register'.
			x = lo;
			addr = x + st.offset;
			st.data = _MMU_read32(st.proc,MMU_AT_DEBUG,addr);
			break;

		case 0xDA:
			//16-Bit Load (Data Register Codes)
			//DA000000 XXXXXXXX 
			//Loads the halfword at [XXXXXXXX+offset] and stores it in the'Dx data register'.
			x = lo;
			addr = x + st.offset;
			st.data = _MMU_read16(st.proc,MMU_AT_DEBUG,addr);
			break;

		case 0xDB:
			//8-Bit Load (Data Register Codes)
			//DB000000 XXXXXXXX 
			//Loads the byte at [XXXXXXXX+offset] and stores it in the'Dx data register'.
			//This is a bugged code type. Check 'AR Hack #0' for the fix. 
			x = lo;
			addr = x + st.offset;
			st.data = _MMU_read08(st.proc,MMU_AT_DEBUG,addr);
			//<gbatek> Before v1.54, the DB000000 code did accidently set offset=offset+XXXXXXX after execution of the code
			if(!v154)
				st.offset = addr;
			break;

		case 0xDC: 
			//Set offset (Offset Codes)
			//DC000000 XXXXXXXX
			//Adds an offset to the current offset. (Dual Offset)
			x = lo;
			st.offset += x;
			break;

		case 0xDF:
			if(vEmulator)
			{
				if(hi == 0xDFFFFFFF) {
					if(lo == 0x99999999)
						st.proc = ARMCPU_ARM9;
					else if(lo == 0x77777777)
						st.proc = ARMCPU_ARM7;
				}
			}
			break;

		case 0x0E:
			//Patch Code (Miscellaneous Memory Manipulation Codes)
			//EXXXXXXX YYYYYYYY 
			//Copies YYYYYYYY bytes from (current code location + 8) to [XXXXXXXX + offset].
			//<gbatek> Copy YYYYYYYY parameter bytes to [XXXXXXXX+offset...]
			//<gbatek> For the COPY commands, addresses should be aligned by four (all data is copied with ldr/str, except, on odd lengths, the last 1..3 bytes do use ldrb/strb).
			//attempting to emulate logic the way they may have implemented it, just in case
			x = hi & 0x0FFFFFFF;
			y = lo;
			addr = x + st.offset;

			{
				u32 j=0,t=0,b=0;
				if(y>0) i++; //skip over the current code
				while(y>=4)
				{
					if(i==list.num) break; //if we erroneously went off the end, bail
					u32 tmp = list.code[i][t];
					if(t==1) i++;
					t ^= 1;
					CheatWrite(32,st.proc,addr,tmp);
					addr += 4;
					y -= 4;
				}
				while(y>0)
				{
					if(i==list.num) break; //if we erroneously went off the end, bail
					u32 tmp = list.code[i][t]>>b;
					CheatWrite(8,st.proc,addr,tmp);
					addr += 1;
					y -= 1;
					b += 4;
				}

				//the main loop will increment to the next cheat, but the loop above may have gone one too far
				if(t==0)
					i--;
			}
			break;

		case 0x0F:
			//Memory Copy Code (Miscellaneous Memory Manipulation Codes)
			//FXXXXXXX YYYYYYYY 
			//<gbatek> Copy YYYYYYYY bytes from [offset..] to [XXXXXXX...]
			//<gbatek> For the COPY commands, addresses should be aligned by four (all data is copied with ldr/str, except, on odd lengths, the last 1..3 bytes do use ldrb/strb).
			//attempting to emulate logic the way they may have implemented it, just in case
			x = hi & 0x0FFFFFFF;
			y = lo;
			addr = st.offset;
			operand = x; //mis-use of this variable to store dst
			while(y>=4)
			{
				if(i==list.num) break; //if we erroneously went off the end, bail
				u32 tmp = _MMU_read32(st.proc,MMU_AT_DEBUG,addr);
				CheatWrite(32, st.proc,operand,tmp);
				addr += 4;
				operand += 4;
				y -= 4;
			}
			while(y>0)
			{
				if(i==list.num) break; //if we erroneously went off the end, bail
				u8 tmp = _MMU_read08(st.proc,MMU_AT_DEBUG,addr);
				CheatWrite(8,st.proc,operand,tmp);
				addr += 1;
				operand += 1;
				y -= 1;
			}
			break;

		default:
			printf("AR: ERROR unknown command %08X %08X\n", hi, lo);
			break;
		}
	}

}

BOOL CHEATS::add_AR_Direct(CHEATS_LIST cheat)
{
	size_t num = list.size();
	list.push_back(cheat);
	list[num].type = 1;
	return TRUE;
}

BOOL CHEATS::add_AR(char *code, char *description, BOOL enabled)
{
	//if (num == MAX_CHEAT_LIST) return FALSE;
	size_t num = list.size();

	CHEATS_LIST temp;
	if (!CHEATS::XXCodeFromString(&temp, code)) return FALSE;

	list.push_back(temp);
	
	list[num].type = 1;
	
	this->setDescription(description, num);
	list[num].enabled = enabled;
	return TRUE;
}

BOOL CHEATS::update_AR(char *code, char *description, BOOL enabled, u32 pos)
{
	if (pos >= list.size()) return FALSE;

	if (code != NULL)
	{
		if (!CHEATS::XXCodeFromString(this->getItemByIndex(pos), code)) return FALSE;
		this->setDescription(description, pos);
		list[pos].type = 1;
	}
	
	list[pos].enabled = enabled;
	return TRUE;
}

BOOL CHEATS::add_CB(char *code, char *description, BOOL enabled)
{
	//if (num == MAX_CHEAT_LIST) return FALSE;
	size_t num = list.size();

	if (!CHEATS::XXCodeFromString(this->getItemByIndex(num), code)) return FALSE;
	
	list[num].type = 2;
	
	this->setDescription(description, num);
	list[num].enabled = enabled;
	return TRUE;
}

BOOL CHEATS::update_CB(char *code, char *description, BOOL enabled, u32 pos)
{
	if (pos >= list.size()) return FALSE;

	if (code != NULL)
	{
		if (!CHEATS::XXCodeFromString(this->getItemByIndex(pos), code)) return FALSE;
		list[pos].type = 2;
		this->setDescription(description, pos);
	}
	list[pos].enabled = enabled;
	return TRUE;
}

BOOL CHEATS::remove(u32 pos)
{
	if (pos >= list.size()) return FALSE;
	if (list.size() == 0) return FALSE;

	list.erase(list.begin()+pos);

	return TRUE;
}

void CHEATS::getListReset()
{
	currentGet = 0;
	return;
}

BOOL CHEATS::getList(CHEATS_LIST *cheat)
{
	BOOL result = FALSE;
	
	if (currentGet >= this->list.size()) 
	{
		this->getListReset();
		return result;
	}
	
	result = this->get(cheat, currentGet++);
	
	return result;
}

CHEATS_LIST* CHEATS::getListPtr()
{
	return &this->list[0];
}

BOOL CHEATS::get(CHEATS_LIST *cheat, u32 pos)
{
	CHEATS_LIST *item = this->getItemByIndex(pos);
	if (item == NULL)
	{
		return FALSE;
	}
	
	*cheat = *item;
	
	return TRUE;
}

CHEATS_LIST* CHEATS::getItemByIndex(const u32 pos)
{
	if (pos >= this->getSize())
	{
		return NULL;
	}
	
	return &this->list[pos];
}

u32	CHEATS::getSize()
{
	return list.size();
}

size_t CHEATS::getActiveCount()
{
	size_t activeCheatCount = 0;
	const size_t cheatListCount = this->getSize();
	
	for (size_t i = 0; i < cheatListCount; i++)
	{
		if (list[i].enabled)
		{
			activeCheatCount++;
		}
	}
	
	return activeCheatCount;
}

void CHEATS::setDescription(const char *description, u32 pos)
{
	strncpy(list[pos].description, description, sizeof(list[pos].description));
	list[pos].description[sizeof(list[pos].description) - 1] = '\0';
}


static char *trim(char *s, int len = -1)
{
	char *ptr = NULL;
	if (!s) return NULL;
	if (!*s) return s;
	
	if(len==-1)
		ptr = s + strlen(s) - 1;
	else ptr = s+len - 1;
	for (; (ptr >= s) && (!*ptr || isspace((u8)*ptr)) ; ptr--);
	ptr[1] = '\0';
	return s;
}


BOOL CHEATS::save()
{
	const char	*types[] = {"DS", "AR", "CB"};
	std::string	cheatLineStr = "";

	EMUFILE_FILE flist((char *)filename, "w");
	if(flist.fail())
		return FALSE;

	flist.fprintf("; DeSmuME cheats file. VERSION %i.%03i\n", CHEAT_VERSION_MAJOR, CHEAT_VERSION_MINOR);
	flist.fprintf("Name=%s\n", gameInfo.ROMname);
	flist.fprintf("Serial=%s\n", gameInfo.ROMserial);
	flist.fprintf("\n; cheats list\n");
	for (size_t i = 0;  i < list.size(); i++)
	{
		if (list[i].num == 0) continue;
			
		char buf1[8] = {0};
		sprintf(buf1, "%s %c ", types[list[i].type], list[i].enabled?'1':'0');
		cheatLineStr = buf1;
			
		for (int t = 0; t < list[i].num; t++)
		{
			char buf2[10] = { 0 };

			u32 adr = list[i].code[t][0];
			if (list[i].type == 0)
			{
				//size of the cheat is written out as adr highest nybble
				adr &= 0x0FFFFFFF;
				adr |= (list[i].size << 28);
			}
			sprintf(buf2, "%08X", adr);
			cheatLineStr += buf2;
				
			sprintf(buf2, "%08X", list[i].code[t][1]);
			cheatLineStr += buf2;
			if (t < (list[i].num - 1))
				cheatLineStr += ",";
		}
			
		cheatLineStr += " ;";
		cheatLineStr += trim(list[i].description);
		flist.fprintf("%s\n", cheatLineStr.c_str());
	}
	flist.fprintf("\n");
	return TRUE;
}

char *CHEATS::clearCode(char *s)
{
	char	*buf = s;
	if (!s) return NULL;
	if (!*s) return s;

	for (u32 i = 0; i < strlen(s); i++)
	{
		if (s[i] == ';') break;
		if (strchr(hexValid, s[i]))
		{
			*buf = s[i];
			buf++;
		}
	}
	*buf = 0;
	return s;
}

BOOL CHEATS::load()
{
	EMUFILE_FILE flist((char *)filename, "r");
	if(flist.fail())
		return FALSE;
	
	size_t readSize = (MAX_XX_CODE * 17) + sizeof(list[0].description) + 7;
	if (readSize < CHEAT_FILE_MIN_FGETS_BUFFER)
	{
		readSize = CHEAT_FILE_MIN_FGETS_BUFFER;
	}
	
	char *buf = (char *)malloc(readSize);
	
	readSize *= sizeof(*buf);
	
	std::string		codeStr = "";
	u32				last = 0;
	u32				line = 0;
	
	INFO("Load cheats: %s\n", filename);
	clear();
	last = 0; line = 0;
	while (!flist.eof())
	{
		CHEATS_LIST		tmp_cht;
		line++;				// only for debug
		memset(buf, 0, readSize);
		if (flist.fgets(buf, readSize) == NULL) {
			//INFO("Cheats: Failed to read from flist at line %i\n", line);
			continue;
		}
		trim(buf);
		if ((buf[0] == 0) || (buf[0] == ';')) continue;
		if(!strncasecmp(buf,"name=",5)) continue;
		if(!strncasecmp(buf,"serial=",7)) continue;

		memset(&tmp_cht, 0, sizeof(tmp_cht));
		if ((buf[0] == 'D') && (buf[1] == 'S'))		// internal
			tmp_cht.type = 0;
		else
			if ((buf[0] == 'A') && (buf[1] == 'R'))	// Action Replay
				tmp_cht.type = 1;
			else
				if ((buf[0] == 'B') && (buf[1] == 'S'))	// Codebreaker
					tmp_cht.type = 2;
				else
					continue;
		// TODO: CB not supported
		if (tmp_cht.type == 3)
		{
			INFO("Cheats: Codebreaker code no supported at line %i\n", line);
			continue;
		}
		
		codeStr = (char *)(buf + 5);
		codeStr = clearCode((char *)codeStr.c_str());
		
		if (codeStr.empty() || (codeStr.length() % 16 != 0))
		{
			INFO("Cheats: Syntax error at line %i\n", line);
			continue;
		}

		tmp_cht.enabled = (buf[3] == '0')?FALSE:TRUE;
		u32 descr_pos = (u32)(std::max<s32>(strchr((char*)buf, ';') - buf, 0));
		if (descr_pos != 0)
		{
			strncpy(tmp_cht.description, (buf + descr_pos + 1), sizeof(tmp_cht.description));
			tmp_cht.description[sizeof(tmp_cht.description) - 1] = '\0';
		}

		tmp_cht.num = codeStr.length() / 16;
		if ((tmp_cht.type == 0) && (tmp_cht.num > 1))
		{
			INFO("Cheats: Too many values for internal cheat\n", line);
			continue;
		}
		for (int i = 0; i < tmp_cht.num; i++)
		{
			char tmp_buf[9] = {0};

			strncpy(tmp_buf, &codeStr[i * 16], 8);
			sscanf(tmp_buf, "%x", &tmp_cht.code[i][0]);

			if (tmp_cht.type == 0)
			{
				tmp_cht.size = std::min<u32>(3, ((tmp_cht.code[i][0] & 0xF0000000) >> 28));
				tmp_cht.code[i][0] &= 0x0FFFFFFF;
			}
			
			strncpy(tmp_buf, &codeStr[(i * 16) + 8], 8);
			sscanf(tmp_buf, "%x", &tmp_cht.code[i][1]);
		}

		list.push_back(tmp_cht);
		last++;
	}
	
	free(buf);
	buf = NULL;

	INFO("Added %i cheat codes\n", list.size());
	
	return TRUE;
}

void CHEATS::process(int targetType)
{
	if (CommonSettings.cheatsDisable) return;

	if (list.size() == 0) return;

	cheatsResetJit = false;

	size_t num = list.size();
	for (size_t i = 0; i < num; i++)
	{
		if (!list[i].enabled) continue;

		int type = list[i].type;
		
		if(type != targetType)
		continue;

		switch(type)
		{
			case 0:		// internal cheat system
			{
				//INFO("list at 0x0|%07X value %i (size %i)\n",list[i].code[0], list[i].lo[0], list[i].size);
				u32 addr = list[i].code[0][0];
				u32 val = list[i].code[0][1];
				switch (list[i].size)
				{
				case 0: 
					_MMU_write08<ARMCPU_ARM9,MMU_AT_DEBUG>(addr,val);
					break;
				case 1: 
					_MMU_write16<ARMCPU_ARM9,MMU_AT_DEBUG>(addr,val);
					break;
				case 2:
					{
						u32 tmp = _MMU_read32<ARMCPU_ARM9,MMU_AT_DEBUG>(addr);
						tmp &= 0xFF000000;
						tmp |= (val & 0x00FFFFFF);
						_MMU_write32<ARMCPU_ARM9,MMU_AT_DEBUG>(addr,tmp);
						break;
					}
				case 3: 
					_MMU_write32<ARMCPU_ARM9,MMU_AT_DEBUG>(addr,val);
					break;
				}
				break;
			} //end case 0 internal cheat system

			case 1:		// Action Replay
				ARparser(list[i]);
				break;
			case 2:		// Codebreaker
				break;
			default: continue;
		}
	}
	
#ifdef HAVE_JIT
	if(cheatsResetJit)
	{
		if(CommonSettings.use_jit)
		{
			printf("Cheat code operation potentially not compatible with JIT operations. Resetting JIT...\n");
			arm_jit_reset(true, true);
		}
	}
#endif
}

void CHEATS::getXXcodeString(CHEATS_LIST list, char *res_buf)
{
	char	buf[50] = { 0 };

	for (int i=0; i < list.num; i++)
	{
		sprintf(buf, "%08X %08X\n", list.code[i][0], list.code[i][1]);
		strcat(res_buf, buf);
	}
}

BOOL CHEATS::XXCodeFromString(CHEATS_LIST *cheatItem, const std::string codeString)
{
	return CHEATS::XXCodeFromString(cheatItem, codeString.c_str());
}

BOOL CHEATS::XXCodeFromString(CHEATS_LIST *cheatItem, const char *codeString)
{
	BOOL result = FALSE;
	
	if (cheatItem == NULL || codeString == NULL)
	{
		return result;
	}
	
	int		count = 0;
	u16		t = 0;
	char	tmp_buf[sizeof(cheatItem->code) * 2 + 1];
	memset(tmp_buf, 0, sizeof(tmp_buf));
	
	size_t code_len = strlen(codeString);
	// remove wrong chars
	for (size_t i=0; i < code_len; i++)
	{
		char c = codeString[i];
		//apparently 100% of pokemon codes were typed with the letter O in place of zero in some places
		//so let's try to adjust for that here
		static const char *AR_Valid = "Oo0123456789ABCDEFabcdef";
		if (strchr(AR_Valid, c))
		{
			if(c=='o' || c=='O') c='0';
			tmp_buf[t++] = c;
		}
	}
	
	size_t len = strlen(tmp_buf);
	if ((len % 16) != 0) return result;			// error
	
	// TODO: syntax check
	count = (len / 16);
	for (int i=0; i < count; i++)
	{
		char buf[9] = {0};
		memcpy(buf, tmp_buf+(i*16), 8);
		sscanf(buf, "%x", &cheatItem->code[i][0]);
		memcpy(buf, tmp_buf+(i*16) + 8, 8);
		sscanf(buf, "%x", &cheatItem->code[i][1]);
	}
	
	cheatItem->num = count;
	cheatItem->size = 0;
	
	result = TRUE;
	
	return result;
}

// ========================================== search
BOOL CHEATSEARCH::start(u8 type, u8 size, u8 sign)
{
	if (statMem) return FALSE;
	if (mem) return FALSE;

	statMem = new u8 [ ( 4 * 1024 * 1024 ) / 8 ];
	memset(statMem, 0xFF, ( 4 * 1024 * 1024 ) / 8);

	// comparative search type (need 8Mb RAM !!! (4+4))
	mem = new u8 [ ( 4 * 1024 * 1024 ) ];
	memcpy(mem, MMU.MMU_MEM[0][0x20], ( 4 * 1024 * 1024 ) );

	_type = type;
	_size = size;
	_sign = sign;
	amount = 0;
	lastRecord = 0;
	
	//INFO("Cheat search system is inited (type %s)\n", type?"comparative":"exact");
	return TRUE;
}

BOOL CHEATSEARCH::close()
{
	if (statMem)
	{
		delete [] statMem;
		statMem = NULL;
	}

	if (mem)
	{
		delete [] mem;
		mem = NULL;
	}
	amount = 0;
	lastRecord = 0;
	//INFO("Cheat search system is closed\n");
	return FALSE;
}

u32 CHEATSEARCH::search(u32 val)
{
	amount = 0;

	switch (_size)
	{
		case 0:		// 1 byte
			for (u32 i = 0; i < (4 * 1024 * 1024); i++)
			{
				u32	addr = (i >> 3);
				u32	offs = (i % 8);
				if (statMem[addr] & (1<<offs))
				{
					if ( T1ReadByte(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) == val )
					{
						statMem[addr] |= (1<<offs);
						amount++;
						continue;
					}
					statMem[addr] &= ~(1<<offs);
				}
			}
		break;

		case 1:		// 2 bytes
			for (u32 i = 0; i < (4 * 1024 * 1024); i+=2)
			{
				u32	addr = (i >> 3);
				u32	offs = (i % 8);
				if (statMem[addr] & (3<<offs))
				{
					if ( T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) == val )
					{
						statMem[addr] |= (3<<offs);
						amount++;
						continue;
					}
					statMem[addr] &= ~(3<<offs);
				}
			}
		break;

		case 2:		// 3 bytes
			for (u32 i = 0; i < (4 * 1024 * 1024); i+=3)
			{
				u32	addr = (i >> 3);
				u32	offs = (i % 8);
				if (statMem[addr] & (0x7<<offs))
				{
					if ( (T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) & 0x00FFFFFF) == val )
					{
						statMem[addr] |= (0x7<<offs);
						amount++;
						continue;
					}
					statMem[addr] &= ~(0x7<<offs);
				}
			}
		break;

		case 3:		// 4 bytes
			for (u32 i = 0; i < (4 * 1024 * 1024); i+=4)
			{
				u32	addr = (i >> 3);
				u32	offs = (i % 8);
				if (statMem[addr] & (0xF<<offs))
				{
					if ( T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) == val )
					{
						statMem[addr] |= (0xF<<offs);
						amount++;
						continue;
					}
					statMem[addr] &= ~(0xF<<offs);
				}
			}
		break;
	}

	return (amount);
}

u32 CHEATSEARCH::search(u8 comp)
{
	BOOL	res = FALSE;

	amount = 0;
	
	switch (_size)
	{
		case 0:		// 1 byte
			for (u32 i = 0; i < (4 * 1024 * 1024); i++)
			{
				u32	addr = (i >> 3);
				u32	offs = (i % 8);
				if (statMem[addr] & (1<<offs))
				{
					switch (comp)
					{
						case 0: res=(T1ReadByte(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) > T1ReadByte(mem, i)); break;
						case 1: res=(T1ReadByte(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) < T1ReadByte(mem, i)); break;
						case 2: res=(T1ReadByte(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) == T1ReadByte(mem, i)); break;
						case 3: res=(T1ReadByte(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) != T1ReadByte(mem, i)); break;
						default: res = FALSE; break;
					}
					if ( res )
					{
						statMem[addr] |= (1<<offs);
						amount++;
						continue;
					}
					statMem[addr] &= ~(1<<offs);
				}
			}
		break;

		case 1:		// 2 bytes
			for (u32 i = 0; i < (4 * 1024 * 1024); i+=2)
			{
				u32	addr = (i >> 3);
				u32	offs = (i % 8);
				if (statMem[addr] & (3<<offs))
				{
					switch (comp)
					{
						case 0: res=(T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) > T1ReadWord(mem, i)); break;
						case 1: res=(T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) < T1ReadWord(mem, i)); break;
						case 2: res=(T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) == T1ReadWord(mem, i)); break;
						case 3: res=(T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) != T1ReadWord(mem, i)); break;
						default: res = FALSE; break;
					}
					if ( res )
					{
						statMem[addr] |= (3<<offs);
						amount++;
						continue;
					}
					statMem[addr] &= ~(3<<offs);
				}
			}
		break;

		case 2:		// 3 bytes
			for (u32 i = 0; i < (4 * 1024 * 1024); i+=3)
			{
				u32	addr = (i >> 3);
				u32	offs = (i % 8);
				if (statMem[addr] & (7<<offs))
				{
					switch (comp)
					{
						case 0: res=((T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) & 0x00FFFFFF) > (T1ReadLong(mem, i) & 0x00FFFFFF) ); break;
						case 1: res=((T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) & 0x00FFFFFF) < (T1ReadLong(mem, i) & 0x00FFFFFF) ); break;
						case 2: res=((T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) & 0x00FFFFFF) == (T1ReadLong(mem, i) & 0x00FFFFFF) ); break;
						case 3: res=((T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) & 0x00FFFFFF) != (T1ReadLong(mem, i) & 0x00FFFFFF) ); break;
						default: res = FALSE; break;
					}
					if ( res )
					{
						statMem[addr] |= (7<<offs);
						amount++;
						continue;
					}
					statMem[addr] &= ~(7<<offs);
				}
			}
		break;

		case 3:		// 4 bytes
			for (u32 i = 0; i < (4 * 1024 * 1024); i+=4)
			{
				u32	addr = (i >> 3);
				u32	offs = (i % 8);
				if (statMem[addr] & (0xF<<offs))
				{
					switch (comp)
					{
						case 0: res=(T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) > T1ReadLong(mem, i)); break;
						case 1: res=(T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) < T1ReadLong(mem, i)); break;
						case 2: res=(T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) == T1ReadLong(mem, i)); break;
						case 3: res=(T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) != T1ReadLong(mem, i)); break;
						default: res = FALSE; break;
					}
					if ( res )
					{
						statMem[addr] |= (0xF<<offs);
						amount++;
						continue;
					}
					statMem[addr] &= ~(0xF<<offs);
				}
			}
		break;
	}

	memcpy(mem, MMU.MMU_MEM[0][0x20], ( 4 * 1024 * 1024 ) );

	return (amount);
}

u32 CHEATSEARCH::getAmount()
{
	return (amount);
}

BOOL CHEATSEARCH::getList(u32 *address, u32 *curVal)
{
	u8	step = (_size+1);
	u8	stepMem = 1;
	switch (_size)
	{
		case 1: stepMem = 0x3; break;
		case 2: stepMem = 0x7; break;
		case 3: stepMem = 0xF; break;
	}

	for (u32 i = lastRecord; i < (4 * 1024 * 1024); i+=step)
	{
		u32	addr = (i >> 3);
		u32	offs = (i % 8);
		if (statMem[addr] & (stepMem<<offs))
		{
			*address = i;
			lastRecord = i+step;
			
			switch (_size)
			{
				case 0: *curVal=(u32)T1ReadByte(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i); return TRUE;
				case 1: *curVal=(u32)T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i); return TRUE;
				case 2: *curVal=(u32)T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) & 0x00FFFFFF; return TRUE;
				case 3: *curVal=(u32)T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i); return TRUE;
				default: return TRUE;
			}
		}
	}
	lastRecord = 0;
	return FALSE;
}

void CHEATSEARCH::getListReset()
{
	lastRecord = 0;
}

// ========================================================================= Export
void CHEATSEXPORT::R4decrypt(u8 *buf, u32 len, u32 n)
{
	size_t r = 0;
	while (r < len)
	{
		size_t i ;
		u16 key = n ^ 0x484A;
		for (i = 0 ; i < 512 && i < len - r ; i ++)
		{
			u8 _xor = 0;
			if (key & 0x4000) _xor |= 0x80;
			if (key & 0x1000) _xor |= 0x40;
			if (key & 0x0800) _xor |= 0x20;
			if (key & 0x0200) _xor |= 0x10;
			if (key & 0x0080) _xor |= 0x08;
			if (key & 0x0040) _xor |= 0x04;
			if (key & 0x0002) _xor |= 0x02;
			if (key & 0x0001) _xor |= 0x01;

			u32 k = ((buf[i] << 8) ^ key) << 16;
			u32 x = k;
			for (u8 j = 1; j < 32; j ++)
				x ^= k >> j;
			key = 0x0000;
			if (BIT_N(x, 23)) key |= 0x8000;
			if (BIT_N(k, 22)) key |= 0x4000;
			if (BIT_N(k, 21)) key |= 0x2000;
			if (BIT_N(k, 20)) key |= 0x1000;
			if (BIT_N(k, 19)) key |= 0x0800;
			if (BIT_N(k, 18)) key |= 0x0400;
			if (BIT_N(k, 17) != BIT_N(x, 31)) key |= 0x0200;
			if (BIT_N(k, 16) != BIT_N(x, 30)) key |= 0x0100;
			if (BIT_N(k, 30) != BIT_N(k, 29)) key |= 0x0080;
			if (BIT_N(k, 29) != BIT_N(k, 28)) key |= 0x0040;
			if (BIT_N(k, 28) != BIT_N(k, 27)) key |= 0x0020;
			if (BIT_N(k, 27) != BIT_N(k, 26)) key |= 0x0010;
			if (BIT_N(k, 26) != BIT_N(k, 25)) key |= 0x0008;
			if (BIT_N(k, 25) != BIT_N(k, 24)) key |= 0x0004;
			if (BIT_N(k, 25) != BIT_N(x, 26)) key |= 0x0002;
			if (BIT_N(k, 24) != BIT_N(x, 25)) key |= 0x0001;
			buf[i] ^= _xor;
		}

		buf+= 512;
		r  += 512;
		n  += 1;
	}
}

bool CHEATSEXPORT::load(char *path)
{
	error = 0;

	fp = fopen(path, "rb");
	if (!fp)
	{
		printf("Error open database\n");
		error = 1;
		return false;
	}

	const char *headerID = "R4 CheatCode";
	char buf[255] = {0};
	fread(buf, 1, strlen(headerID), fp);
	if (strncmp(buf, headerID, strlen(headerID)) != 0)
	{
		// check encrypted
		R4decrypt((u8 *)buf, strlen(headerID), 0);
		if (strcmp(buf, headerID) != 0)
		{
			error = 2;
			return false;
		}
		encrypted = true;
	}

	fseek(fp, 0, SEEK_END);
	fsize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	if (!search())
	{
		printf("ERROR: cheat in database not found\n");
		error = 3;
		return false;
	}
	
	if (!getCodes())
	{
		printf("ERROR: export cheats failed\n");
		error = 4;
		return false;
	}

	return true;
}
void CHEATSEXPORT::close()
{
	if (fp)
		fclose(fp);
	if (cheats)
	{
		delete [] cheats;
		cheats = NULL;
	}
}

bool CHEATSEXPORT::search()
{
	if (!fp) return false;

	u32		pos = 0x0100;
	FAT_R4	fat_tmp = {0};
	u8		buf[512] = {0};

	CRC = 0;
	encOffset = 0;
	u32 t = 0;
	memset(date, 0, sizeof(date));
	if (encrypted)
	{
		fseek(fp, 0, SEEK_SET);
		fread(&buf[0], 1, 512, fp);
		R4decrypt((u8 *)&buf[0], 512, 0);
		memcpy(&date[0], &buf[0x10], 16);
	}
	else
	{
		fseek(fp, 0x10, SEEK_SET);
		fread(&date, 16, 1, fp);
		fseek(fp, pos, SEEK_SET);
		fread(&fat_tmp, sizeof(fat), 1, fp);
	}

	while (1)
	{
		if (encrypted)
		{
			memcpy(&fat, &buf[pos % 512], sizeof(fat));
			pos += sizeof(fat);
			if ((pos>>9) > t)
			{
				t++;
				fread(&buf[0], 1, 512, fp);
				R4decrypt((u8 *)&buf[0], 512, t);
			}
			memcpy(&fat_tmp, &buf[pos % 512], sizeof(fat_tmp));	// next
		}
		else
		{
			memcpy(&fat, &fat_tmp, sizeof(fat));
			fread(&fat_tmp, sizeof(fat_tmp), 1, fp);
			
		}
		//printf("serial: %s, offset %08X\n", fat.serial, fat.addr);
		if (gameInfo.crcForCheatsDb == fat.CRC
			&& !memcmp(gameInfo.header.gameCode, &fat.serial[0], 4))
		{
			dataSize = fat_tmp.addr?(fat_tmp.addr - fat.addr):0;
			if (encrypted)
			{
				encOffset = fat.addr % 512;
				dataSize += encOffset;
			}
			if (!dataSize) return false;
			CRC = fat.CRC;
			char buf[5] = {0};
			memcpy(&buf, &fat.serial[0], 4);
			printf("Cheats: found %s CRC %08X at 0x%08llX, size %i byte(s)\n", buf, fat.CRC, fat.addr, dataSize - encOffset);
			return true;
		}

		if (fat.addr == 0) break;
	}

	memset(&fat, 0, sizeof(FAT_R4));
	return false;
}

bool CHEATSEXPORT::getCodes()
{
	if (!fp) return false;

	u32	pos = 0;
	u32	pos_cht = 0;

	u8 *data = new u8 [dataSize+8];
	if (!data) return false;
	memset(data, 0, dataSize+8);
	
	fseek(fp, fat.addr - encOffset, SEEK_SET);

	if (fread(data, 1, dataSize, fp) != dataSize)
	{
		delete [] data;
		data = NULL;
		return false;
	}

	if (encrypted)
		R4decrypt(data, dataSize, fat.addr >> 9);
	
	intptr_t ptrMask = (~0 << 2);
	u8 *gameTitlePtr = (u8 *)data + encOffset;
	
	memset(gametitle, 0, CHEAT_DB_GAME_TITLE_SIZE);
	memcpy(gametitle, gameTitlePtr, strlen((const char *)gameTitlePtr));
	
	u32 *cmd = (u32 *)(((intptr_t)gameTitlePtr + strlen((const char *)gameTitlePtr) + 4) & ptrMask);
	numCheats = cmd[0] & 0x0FFFFFFF;
	cmd += 9;
	cheats = new CHEATS_LIST[numCheats];
	memset(cheats, 0, sizeof(CHEATS_LIST) * numCheats);

	while (pos < numCheats)
	{
		u32 folderNum = 1;
		u8	*folderName = NULL;
		u8	*folderNote = NULL;
		if ((*cmd & 0xF0000000) == 0x10000000)	// Folder
		{
			folderNum = (*cmd  & 0x00FFFFFF);
			folderName = (u8*)((intptr_t)cmd + 4);
			folderNote = (u8*)((intptr_t)folderName + strlen((char*)folderName) + 1);
			pos++;
			cmd = (u32 *)(((intptr_t)folderName + strlen((char*)folderName) + 1 + strlen((char*)folderNote) + 1 + 3) & ptrMask);
		}

		for (u32 i = 0; i < folderNum; i++)		// in folder
		{
			u8 *cheatName = (u8 *)((intptr_t)cmd + 4);
			u8 *cheatNote = (u8 *)((intptr_t)cheatName + strlen((char*)cheatName) + 1);
			u32 *cheatData = (u32 *)(((intptr_t)cheatNote + strlen((char*)cheatNote) + 1 + 3) & ptrMask);
			u32 cheatDataLen = *cheatData++;
			u32 numberCodes = cheatDataLen / 2;

			if (numberCodes <= MAX_XX_CODE)
			{
				std::string descriptionStr = "";
				
				if ( folderName && *folderName )
				{
					descriptionStr += (char *)folderName;
					descriptionStr += ": ";
				}
				
				descriptionStr += (char *)cheatName;
				
				if ( cheatNote && *cheatNote )
				{
					descriptionStr += " | ";
					descriptionStr += (char *)cheatNote;
				}
				
				strncpy(cheats[pos_cht].description, descriptionStr.c_str(), sizeof(cheats[pos_cht].description));
				cheats[pos_cht].description[sizeof(cheats[pos_cht].description) - 1] = '\0';
				
				cheats[pos_cht].num = numberCodes;
				cheats[pos_cht].type = 1;

				for(u32 j = 0, t = 0; j < numberCodes; j++, t+=2 )
				{
					cheats[pos_cht].code[j][0] = (u32)*(cheatData+t);
					//printf("%i: %08X ", j, cheats[pos_cht].code[j][0]);
					cheats[pos_cht].code[j][1] = (u32)*(cheatData+t+1);
					//printf("%08X\n", cheats[pos_cht].code[j][1]);
					
				}
				pos_cht++;
			}

			pos++;
			cmd = (u32 *)((intptr_t)cmd + ((*cmd + 1)*4));
		}
		
	};

	delete [] data;

	numCheats = pos_cht;
	//for (int i = 0; i < numCheats; i++)
	//	printf("%i: %s\n", i, cheats[i].description);
	
	return true;
}

CHEATS_LIST *CHEATSEXPORT::getCheats()
{
	return cheats;
}
u32 CHEATSEXPORT::getCheatsNum()
{
	return numCheats;
}
