/*
	Copyright (C) 2009-2023 DeSmuME team

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
#include "emufile.h"

#ifndef _MSC_VER 
#include <stdint.h>
#endif

static const char hexValid[23] = {"0123456789ABCDEFabcdef"};

CHEATS *cheats = NULL;
CHEATSEARCH *cheatSearch = NULL;

static bool cheatsResetJit = false;

void CHEATS::clear()
{
	this->_list.resize(0);
	this->_currentGet = 0;
}

void CHEATS::init(const char *thePath)
{
	this->clear();
	this->setFilePath(thePath);
	this->load();
}

const char* CHEATS::getFilePath() const
{
	return (const char *)this->_filename;
}

void CHEATS::setFilePath(const char *thePath)
{
	memset(this->_filename, '\0', sizeof(this->_filename));
	
	if (thePath != NULL)
	{
		strncpy((char *)this->_filename, thePath, sizeof(this->_filename));
		this->_filename[ sizeof(this->_filename) - 1 ] = '\0';
	}
}

size_t CHEATS::addItem(const CHEATS_LIST &srcCheat)
{
	this->_list.push_back(srcCheat);
	return this->_list.size() - 1;
}

bool CHEATS::add(u8 size, u32 address, u32 val, char *description, bool enabled)
{
	bool didValidateItem = false;
	size_t num = this->_list.size();
	this->_list.push_back(CHEATS_LIST());
	
	didValidateItem = this->update(size, address, val, description, enabled, num);
	return didValidateItem;
}

bool CHEATS::add(u8 size, u32 address, u32 val, char *description, u8 enabled)
{
	return this->add(size, address, val, description, (enabled != 0));
}

bool CHEATS::updateItemAtIndex(const CHEATS_LIST &srcCheat, const size_t pos)
{
	bool didValidateItem = true; // No error check for memcpy() here!
	CHEATS_LIST *theItem = &(this->_list[pos]);
	
	memcpy(theItem, &srcCheat, sizeof(CHEATS_LIST));
	
	return didValidateItem;
}

bool CHEATS::update(u8 size, u32 address, u32 val, char *description, bool enabled, const size_t pos)
{
	bool didValidateItem = false;
	if (pos >= this->_list.size())
	{
		return didValidateItem;
	}

	this->_list[pos].code[0][0] = address & 0x0FFFFFFF;
	this->_list[pos].code[0][1] = val;
	this->_list[pos].num = 1;
	this->_list[pos].type = CHEAT_TYPE_INTERNAL;
	this->_list[pos].size = size;
	this->setDescription(description, pos);
	this->_list[pos].enabled = (enabled) ? 1 : 0;

	didValidateItem = true;
	return didValidateItem;
}

bool CHEATS::update(u8 size, u32 address, u32 val, char *description, u8 enabled, const size_t pos)
{
	return this->update(size, address, val, description, (enabled != 0), pos);
}

bool CHEATS::move(size_t srcPos, size_t dstPos)
{
	bool didValidateItem = false;
	if (srcPos >= this->_list.size() || dstPos > this->_list.size())
	{
		return didValidateItem;
	}

	// get the item to move
	CHEATS_LIST srcCheat = this->_list[srcPos];
	// insert item in the new position
	this->_list.insert(this->_list.begin() + dstPos, srcCheat);
	// remove the original item
	if (dstPos < srcPos) srcPos++;
	this->_list.erase(this->_list.begin() + srcPos);

	didValidateItem = true;
	return didValidateItem;
}

#define CHEATLOG(...) 
//#define CHEATLOG(...) printf(__VA_ARGS__)

template <size_t LENGTH>
bool CHEATS::DirectWrite(const int targetProc, const u32 targetAddress, u32 newValue)
{
	return MMU_WriteFromExternal<u32, LENGTH>(targetProc, targetAddress, newValue);
}

template bool CHEATS::DirectWrite<1>(const int targetProc, const u32 targetAddress, u32 newValue);
template bool CHEATS::DirectWrite<2>(const int targetProc, const u32 targetAddress, u32 newValue);
template bool CHEATS::DirectWrite<3>(const int targetProc, const u32 targetAddress, u32 newValue);
template bool CHEATS::DirectWrite<4>(const int targetProc, const u32 targetAddress, u32 newValue);

bool CHEATS::DirectWrite(const size_t newValueLength, const int targetProc, const u32 targetAddress, u32 newValue)
{
	switch (newValueLength)
	{
		case 1: return CHEATS::DirectWrite<1>(targetProc, targetAddress, newValue);
		case 2: return CHEATS::DirectWrite<2>(targetProc, targetAddress, newValue);
		case 3: return CHEATS::DirectWrite<3>(targetProc, targetAddress, newValue);
		case 4: return CHEATS::DirectWrite<4>(targetProc, targetAddress, newValue);
			
		default:
			break;
	}
	
	return false;
}

bool CHEATS::ARparser(const CHEATS_LIST &theList)
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
	bool needsJitReset = false;

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

	for (u32 i = 0; i < theList.num; i++)
	{
		bool shouldResetJit = false;
		const u32 hi = theList.code[i][0];
		const u32 lo = theList.code[i][1];

		CHEATLOG("executing [%02d] %08X %08X (ofs=%08X)\n",i, hi,lo, st.offset);

		//parse codes into types by kodewerx standards
		u32 type = theList.code[i][0] >> 28;
		//these two are broken down into subtypes
		if(type == 0x0C || type == 0x0D)
			type = theList.code[i][0] >> 24;

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
			shouldResetJit = CHEATS::DirectWrite<4>(st.proc, addr, y);
			break;
		
		case 0x01:
			//16-bit (Constant RAM Writes)
			//1XXXXXXX 0000YYYY
			//Writes halfword YYYY to [XXXXXXX+offset].
			x = hi & 0x0FFFFFFF;
			y = lo & 0xFFFF;
			addr = x + st.offset;
			shouldResetJit = CHEATS::DirectWrite<2>(st.proc, addr, y);
			break;

		case 0x02:
			//8-bit (Constant RAM Writes)
			//2XXXXXXX 000000YY
			//Writes byte YY to [XXXXXXX+offset].
			x = hi & 0x0FFFFFFF;
			y = lo & 0xFF;
			addr = x + st.offset;
			shouldResetJit = CHEATS::DirectWrite<1>(st.proc, addr, y);
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
			shouldResetJit = CHEATS::DirectWrite<4>(st.proc, x, st.offset);
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
			shouldResetJit = CHEATS::DirectWrite<4>(st.proc, addr, st.data);
			st.offset += 4;
			break;

		case 0xD7:
			//16-Bit Incrementive Write (Data Register Codes)
			//D7000000 XXXXXXXX 
			//Writes the 'Dx data' halfword to [XXXXXXXX+offset], and increments the offset by 2.
			//<gbatek> half[XXXXXXXX+offset]=datareg, offset=offset+2
			x = lo;
			addr = x + st.offset;
			shouldResetJit = CHEATS::DirectWrite<2>(st.proc, addr, st.data);
			st.offset += 2;
			break;

		case 0xD8:
			//8-Bit Incrementive Write (Data Register Codes)
			//D8000000 XXXXXXXX 
			//Writes the 'Dx data' byte to [XXXXXXXX+offset], and increments the offset by 1.
			//<gbatek> byte[XXXXXXXX+offset]=datareg, offset=offset+1
			x = lo;
			addr = x + st.offset;
			shouldResetJit = CHEATS::DirectWrite<1>(st.proc, addr, st.data);
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
				u32 t = 0;
				u32 b = 0;
				
				if(y>0) i++; //skip over the current code
				while(y>=4)
				{
					if (i == theList.num) break; //if we erroneously went off the end, bail
					u32 tmp = theList.code[i][t];
					if (t == 1) i++;
					t ^= 1;
					shouldResetJit = CHEATS::DirectWrite<4>(st.proc, addr, tmp);
					addr += 4;
					y -= 4;
				}
				while(y>0)
				{
					if (i == theList.num) break; //if we erroneously went off the end, bail
					u32 tmp = theList.code[i][t]>>b;
					shouldResetJit = CHEATS::DirectWrite<1>(st.proc, addr, tmp);
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
				if (i == theList.num) break; //if we erroneously went off the end, bail
				u32 tmp = _MMU_read32(st.proc,MMU_AT_DEBUG,addr);
				shouldResetJit = CHEATS::DirectWrite<4>(st.proc, operand, tmp);
				addr += 4;
				operand += 4;
				y -= 4;
			}
			while(y>0)
			{
				if (i == theList.num) break; //if we erroneously went off the end, bail
				u8 tmp = _MMU_read08(st.proc,MMU_AT_DEBUG,addr);
				shouldResetJit = CHEATS::DirectWrite<1>(st.proc, operand, tmp);
				addr += 1;
				operand += 1;
				y -= 1;
			}
			break;

		default:
			printf("AR: ERROR unknown command %08X %08X\n", hi, lo);
			break;
		}
		
		if (shouldResetJit)
		{
			needsJitReset = true;
		}
	}
	
	return needsJitReset;
}

size_t CHEATS::add_AR_Direct(const CHEATS_LIST &srcCheat)
{
	const size_t itemIndex = this->addItem(srcCheat);
	this->_list[itemIndex].type = CHEAT_TYPE_AR;
	
	return itemIndex;
}

bool CHEATS::add_AR(char *code, char *description, bool enabled)
{
	bool didValidateItem = false;
	size_t num = this->_list.size();

	CHEATS_LIST temp;

	didValidateItem = CHEATS::XXCodeFromString(code, temp);
	if (!didValidateItem)
	{
		return didValidateItem;
	}

	this->_list.push_back(temp);
	this->_list[num].type = CHEAT_TYPE_AR;
	
	this->setDescription(description, num);
	this->_list[num].enabled = (enabled) ? 1 : 0;

	didValidateItem = true;
	return didValidateItem;
}

bool CHEATS::add_AR(char *code, char *description, u8 enabled)
{
	return this->add_AR(code, description, (enabled != 0));
}

bool CHEATS::update_AR(char *code, char *description, bool enabled, const size_t pos)
{
	bool didValidateItem = false;
	if (pos >= this->_list.size())
	{
		return didValidateItem;
	}

	if (code != NULL)
	{
		CHEATS_LIST *cheatItem = this->getItemPtrAtIndex(pos);
		if (cheatItem == NULL)
		{
			return didValidateItem;
		}

		didValidateItem = CHEATS::XXCodeFromString(code, *cheatItem);
		if (!didValidateItem)
		{
			return didValidateItem;
		}

		this->setDescription(description, pos);
		this->_list[pos].type = CHEAT_TYPE_AR;
	}
	
	this->_list[pos].enabled = (enabled) ? 1 : 0;

	didValidateItem = true;
	return didValidateItem;
}

bool CHEATS::update_AR(char *code, char *description, u8 enabled, const size_t pos)
{
	return this->update_AR(code, description, (enabled != 0), pos);
}

bool CHEATS::add_CB(char *code, char *description, bool enabled)
{
	bool didValidateItem = false;
	size_t num = this->_list.size();

	CHEATS_LIST *cheatItem = this->getItemPtrAtIndex(num);
	if (cheatItem == NULL)
	{
		return didValidateItem;
	}

	didValidateItem = CHEATS::XXCodeFromString(code, *cheatItem);
	if (!didValidateItem)
	{
		return didValidateItem;
	}
	
	this->_list[num].type = CHEAT_TYPE_CODEBREAKER;
	
	this->setDescription(description, num);
	this->_list[num].enabled = (enabled) ? 1 : 0;

	didValidateItem = true;
	return didValidateItem;
}

bool CHEATS::add_CB(char *code, char *description, u8 enabled)
{
	return this->add_CB(code, description, (enabled != 0));
}

bool CHEATS::update_CB(char *code, char *description, bool enabled, const size_t pos)
{
	bool didValidateItem = false;
	if (pos >= this->_list.size())
	{
		return didValidateItem;
	}

	if (code != NULL)
	{
		CHEATS_LIST *cheatItem = this->getItemPtrAtIndex(pos);
		if (cheatItem == NULL)
		{
			return didValidateItem;
		}

		didValidateItem = CHEATS::XXCodeFromString(code, *cheatItem);
		if (!didValidateItem)
		{
			return didValidateItem;
		}

		this->_list[pos].type = CHEAT_TYPE_CODEBREAKER;
		this->setDescription(description, pos);
	}
	this->_list[pos].enabled = (enabled) ? 1 : 0;

	didValidateItem = true;
	return didValidateItem;
}

bool CHEATS::update_CB(char *code, char *description, u8 enabled, const size_t pos)
{
	return this->update_CB(code, description, (enabled != 0), pos);
}

bool CHEATS::remove(const size_t pos)
{
	bool didRemoveItem = false;

	if (pos >= this->_list.size())
	{
		return didRemoveItem;
	}

	if (this->_list.size() == 0)
	{
		return didRemoveItem;
	}

	this->_list.erase(this->_list.begin()+pos);

	didRemoveItem = true;
	return didRemoveItem;
}

void CHEATS::getListReset()
{
	this->_currentGet = 0;
	return;
}

bool CHEATS::getList(CHEATS_LIST *cheat)
{
	bool result = false;
	
	if (this->_currentGet >= this->_list.size()) 
	{
		this->getListReset();
		return result;
	}
	
	result = this->copyItemFromIndex(this->_currentGet++, *cheat);
	
	return result;
}

CHEATS_LIST* CHEATS::getListPtr()
{
	return &this->_list[0];
}

bool CHEATS::copyItemFromIndex(const size_t pos, CHEATS_LIST &outCheatItem)
{
	const CHEATS_LIST *itemPtr = this->getItemPtrAtIndex(pos);
	if (itemPtr == NULL)
	{
		return false;
	}
	
	outCheatItem = *itemPtr;
	
	return true;
}

CHEATS_LIST* CHEATS::getItemPtrAtIndex(const size_t pos) const
{
	if (pos >= this->getListSize())
	{
		return NULL;
	}
	
	const CHEATS_LIST *itemPtr = &this->_list[pos];
	return (CHEATS_LIST *)itemPtr;
}

size_t CHEATS::getListSize() const
{
	return this->_list.size();
}

size_t CHEATS::getActiveCount() const
{
	size_t activeCheatCount = 0;
	const size_t cheatListCount = this->getListSize();
	
	for (size_t i = 0; i < cheatListCount; i++)
	{
		if (this->_list[i].enabled != 0)
		{
			activeCheatCount++;
		}
	}
	
	return activeCheatCount;
}

void CHEATS::setDescription(const char *description, const size_t pos)
{
	strncpy(this->_list[pos].description, description, sizeof(this->_list[pos].description));
	this->_list[pos].description[sizeof(this->_list[pos].description) - 1] = '\0';
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


bool CHEATS::save()
{
	bool didSave = false;
	const char	*types[] = {"DS", "AR", "CB"};
	std::string	cheatLineStr = "";

	EMUFILE_FILE flist((char *)this->_filename, "w");
	if (flist.fail())
	{
		return didSave;
	}

	flist.fprintf("; DeSmuME cheats file. VERSION %i.%03i\n", CHEAT_VERSION_MAJOR, CHEAT_VERSION_MINOR);
	flist.fprintf("Name=%s\n", gameInfo.ROMname);
	flist.fprintf("Serial=%s\n", gameInfo.ROMserial);
	flist.fprintf("\n; cheats list\n");
	for (size_t i = 0;  i < this->_list.size(); i++)
	{
		if (this->_list[i].num == 0) continue;
			
		char buf1[8] = {0};
		snprintf(buf1, sizeof(buf1), "%s %c ", types[this->_list[i].type], this->_list[i].enabled ? '1' : '0');
		cheatLineStr = buf1;
			
		for (u32 t = 0; t < this->_list[i].num; t++)
		{
			char buf2[10] = { 0 };

			u32 adr = this->_list[i].code[t][0];
			if (this->_list[i].type == CHEAT_TYPE_INTERNAL)
			{
				//size of the cheat is written out as adr highest nybble
				adr &= 0x0FFFFFFF;
				adr |= (this->_list[i].size << 28);
			}
			snprintf(buf2, sizeof(buf2), "%08X", adr);
			cheatLineStr += buf2;
				
			snprintf(buf2, sizeof(buf2), "%08X", this->_list[i].code[t][1]);
			cheatLineStr += buf2;
			if (t < (this->_list[i].num - 1))
				cheatLineStr += ",";
		}
			
		cheatLineStr += " ;";
		cheatLineStr += trim(this->_list[i].description);
		flist.fprintf("%s\n", cheatLineStr.c_str());
	}
	flist.fprintf("\n");

	didSave = true;
	return didSave;
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

bool CHEATS::load()
{
	bool didLoadAllItems = false;
	int valueReadResult = 0;
	
	if (strlen((const char *)this->_filename) == 0)
	{
		return didLoadAllItems;
	}
	
	EMUFILE_FILE flist((char *)this->_filename, "r");
	if (flist.fail())
	{
		return didLoadAllItems;
	}

	size_t readSize = (MAX_XX_CODE * 17) + sizeof(this->_list[0].description) + 7;
	if (readSize < CHEAT_FILE_MIN_FGETS_BUFFER)
	{
		readSize = CHEAT_FILE_MIN_FGETS_BUFFER;
	}
	
	char *buf = (char *)malloc(readSize);
	
	readSize *= sizeof(*buf);
	
	std::string		codeStr = "";
	u32				last = 0;
	u32				line = 0;
	
	INFO("Load cheats: %s\n", this->_filename);
	clear();
	last = 0; line = 0;
	while (!flist.eof())
	{
		CHEATS_LIST		tmp_cht;
		line++;				// only for debug
		memset(buf, 0, readSize);
		if (flist.fgets(buf, (int)readSize) == NULL) {
			//INFO("Cheats: Failed to read from flist at line %i\n", line);
			continue;
		}
		trim(buf);
		if ((buf[0] == 0) || (buf[0] == ';')) continue;
		if(!strncasecmp(buf,"name=",5)) continue;
		if(!strncasecmp(buf,"serial=",7)) continue;

		memset(&tmp_cht, 0, sizeof(tmp_cht));
		if ((buf[0] == 'D') && (buf[1] == 'S'))		// internal
			tmp_cht.type = CHEAT_TYPE_INTERNAL;
		else
			if ((buf[0] == 'A') && (buf[1] == 'R'))	// Action Replay
				tmp_cht.type = CHEAT_TYPE_AR;
			else
				if ((buf[0] == 'B') && (buf[1] == 'S'))	// Codebreaker
					tmp_cht.type = CHEAT_TYPE_CODEBREAKER;
				else
					continue;
		// TODO: CB not supported
		if (tmp_cht.type == CHEAT_TYPE_CODEBREAKER)
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

		tmp_cht.enabled = (buf[3] == '0') ? 0 : 1;
		u32 descr_pos = (u32)std::max<s32>((s32)(strchr((char*)buf, ';') - buf), 0);
		if (descr_pos != 0)
		{
			strncpy(tmp_cht.description, (buf + descr_pos + 1), sizeof(tmp_cht.description));
			tmp_cht.description[sizeof(tmp_cht.description) - 1] = '\0';
		}

		tmp_cht.num = (u32)codeStr.length() / 16;
		if ((tmp_cht.type == CHEAT_TYPE_INTERNAL) && (tmp_cht.num > 1))
		{
			INFO("Cheats: Too many values for internal cheat\n", line);
			continue;
		}

		for (u32 i = 0; i < tmp_cht.num; i++)
		{
			char tmp_buf[9] = {0};

			strncpy(tmp_buf, &codeStr[i * 16], 8);
			valueReadResult = sscanf(tmp_buf, "%x", &tmp_cht.code[i][0]);
			if (valueReadResult == 0)
			{
				INFO("Cheats: Could not read first value at line %i\n", line);
				continue;
			}

			if (tmp_cht.type == CHEAT_TYPE_INTERNAL)
			{
				tmp_cht.size = std::min<u32>(3, ((tmp_cht.code[i][0] & 0xF0000000) >> 28));
				tmp_cht.code[i][0] &= 0x0FFFFFFF;
			}
			
			strncpy(tmp_buf, &codeStr[(i * 16) + 8], 8);
			valueReadResult = sscanf(tmp_buf, "%x", &tmp_cht.code[i][1]);
			if (valueReadResult == 0)
			{
				INFO("Cheats: Could not read second value at line %i\n", line);
				continue;
			}
		}

		this->_list.push_back(tmp_cht);
		last++;
	}
	
	free(buf);
	buf = NULL;

	INFO("Added %i cheat codes\n", this->_list.size());
	
	didLoadAllItems = true;
	return didLoadAllItems;
}

bool CHEATS::process(int targetType) const
{
	bool needsJitReset = false;
	
	if (CommonSettings.cheatsDisable || (this->_list.size() == 0))
		return needsJitReset;
	
	size_t num = this->_list.size();
	for (size_t i = 0; i < num; i++)
	{
		bool shouldResetJit = false;
		
		if (this->_list[i].enabled == 0)
			continue;

		int type = this->_list[i].type;
		
		if (type != targetType)
			continue;

		switch (type)
		{
			case CHEAT_TYPE_INTERNAL:
				//INFO("list at 0x0|%07X value %i (size %i)\n",list[i].code[0], list[i].lo[0], list[i].size);
				shouldResetJit = CHEATS::DirectWrite(this->_list[i].size + 1, ARMCPU_ARM9, this->_list[i].code[0][0], this->_list[i].code[0][1]);
				break;
				
			case CHEAT_TYPE_AR:
				shouldResetJit = CHEATS::ARparser(this->_list[i]);
				break;
				
			case CHEAT_TYPE_CODEBREAKER:
				break;
				
			default:
				continue;
		}
		
		if (shouldResetJit)
		{
			needsJitReset = true;
		}
	}
	
	if (needsJitReset)
	{
		CHEATS::JitNeedsReset();
	}
	
	return needsJitReset;
}

void CHEATS::JitNeedsReset()
{
	cheatsResetJit = true;
}

bool CHEATS::ResetJitIfNeeded()
{
	bool didJitReset = false;
	
#ifdef HAVE_JIT
	if (cheatsResetJit)
	{
		if (CommonSettings.use_jit)
		{
			printf("Cheat code operation potentially not compatible with JIT operations. Resetting JIT...\n");
			arm_jit_reset(true, true);
		}
		
		cheatsResetJit = false;
		didJitReset = true;
	}
#endif
	
	return didJitReset;
}

void CHEATS::StringFromXXCode(const CHEATS_LIST &srcCheatItem, char *outCStringBuffer)
{
	char buf[50] = { 0 };

	for (u32 i = 0; i < srcCheatItem.num; i++)
	{
		snprintf(buf, 19, "%08X %08X\n", srcCheatItem.code[i][0], srcCheatItem.code[i][1]);
		strcat(outCStringBuffer, buf);
	}
}

bool CHEATS::XXCodeFromString(const std::string codeString, CHEATS_LIST &outCheatItem)
{
	return CHEATS::XXCodeFromString(codeString.c_str(), outCheatItem);
}

bool CHEATS::XXCodeFromString(const char *codeString, CHEATS_LIST &outCheatItem)
{
	bool didValidateCode = true;
	int valueReadResult = 0;
	
	if (codeString == NULL)
	{
		didValidateCode = false;
		return didValidateCode;
	}
	
	size_t	count = 0;
	u16		t = 0;
	
	const size_t tmpBufferSize = sizeof(outCheatItem.code) * 2 + 1;
	char *tmp_buf = (char *)malloc(tmpBufferSize);
	if (tmp_buf == NULL)
	{
		didValidateCode = false;
		return didValidateCode;
	}
	memset(tmp_buf, 0, tmpBufferSize);
	
	size_t code_len = strlen(codeString);
	// remove wrong chars
	for (size_t i = 0; i < code_len; i++)
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
	if ((len % 16) != 0)
	{
		free(tmp_buf);
		didValidateCode = false;
		return didValidateCode;
	}

	// TODO: syntax check
	count = (len / 16);
	for (size_t i = 0; i < count; i++)
	{
		char buf[9] = {0};
		memcpy(buf, tmp_buf+(i*16), 8);
		valueReadResult = sscanf(buf, "%x", &outCheatItem.code[i][0]);
		if (valueReadResult == 0)
		{
			INFO("Cheats: Could not read first value at line %i\n", i);
			didValidateCode = false;
			break;
		}

		memcpy(buf, tmp_buf+(i*16) + 8, 8);
		valueReadResult = sscanf(buf, "%x", &outCheatItem.code[i][1]);
		if (valueReadResult == 0)
		{
			INFO("Cheats: Could not read second value at line %i\n", i);
			didValidateCode = false;
			break;
		}
	}
	
	outCheatItem.num = (u32)count;
	outCheatItem.size = 0;

	free(tmp_buf);
	
	return didValidateCode;
}

// ========================================== search
bool CHEATSEARCH::start(u8 type, u8 size, u8 sign)
{
	bool didStartSearch = false;

	if ( (this->_statMem != NULL) || (this->_mem != NULL) )
	{
		// If these buffers exist, then that implies that a search is already
		// in progress. However, the existing code design is such that a
		// CHEATSEARCH object may only have one search in progress at a time.
		return didStartSearch;
	}

	this->_statMem = new u8 [ ( 4 * 1024 * 1024 ) / 8 ];
	memset(this->_statMem, 0xFF, ( 4 * 1024 * 1024 ) / 8);

	// comparative search type (need 8Mb RAM !!! (4+4))
	this->_mem = new u8 [ ( 4 * 1024 * 1024 ) ];
	memcpy(this->_mem, MMU.MMU_MEM[0][0x20], ( 4 * 1024 * 1024 ) );

	this->_type = type;
	this->_size = size;
	this->_sign = sign;
	this->_amount = 0;
	this->_lastRecord = 0;
	
	//INFO("Cheat search system is inited (type %s)\n", type?"comparative":"exact");
	didStartSearch = true;
	return didStartSearch;
}

void CHEATSEARCH::close()
{
	if (this->_statMem != NULL)
	{
		delete [] this->_statMem;
		this->_statMem = NULL;
	}

	if (this->_mem != NULL)
	{
		delete [] this->_mem;
		this->_mem = NULL;
	}

	this->_amount = 0;
	this->_lastRecord = 0;
	//INFO("Cheat search system is closed\n");
}

u32 CHEATSEARCH::search(u32 val)
{
	this->_amount = 0;

	switch (this->_size)
	{
		case 0:		// 1 byte
			for (u32 i = 0; i < (4 * 1024 * 1024); i++)
			{
				u32	addr = (i >> 3);
				u32	offs = (i % 8);
				if (this->_statMem[addr] & (1<<offs))
				{
					if ( T1ReadByte(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) == val )
					{
						this->_statMem[addr] |= (1<<offs);
						this->_amount++;
						continue;
					}
					this->_statMem[addr] &= ~(1<<offs);
				}
			}
		break;

		case 1:		// 2 bytes
			for (u32 i = 0; i < (4 * 1024 * 1024); i+=2)
			{
				u32	addr = (i >> 3);
				u32	offs = (i % 8);
				if (this->_statMem[addr] & (3<<offs))
				{
					if ( T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) == val )
					{
						this->_statMem[addr] |= (3<<offs);
						this->_amount++;
						continue;
					}
					this->_statMem[addr] &= ~(3<<offs);
				}
			}
		break;

		case 2:		// 3 bytes
			for (u32 i = 0; i < (4 * 1024 * 1024); i+=3)
			{
				u32	addr = (i >> 3);
				u32	offs = (i % 8);
				if (this->_statMem[addr] & (0x7<<offs))
				{
					if ( (T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) & 0x00FFFFFF) == val )
					{
						this->_statMem[addr] |= (0x7<<offs);
						this->_amount++;
						continue;
					}
					this->_statMem[addr] &= ~(0x7<<offs);
				}
			}
		break;

		case 3:		// 4 bytes
			for (u32 i = 0; i < (4 * 1024 * 1024); i+=4)
			{
				u32	addr = (i >> 3);
				u32	offs = (i % 8);
				if (this->_statMem[addr] & (0xF<<offs))
				{
					if ( T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) == val )
					{
						this->_statMem[addr] |= (0xF<<offs);
						this->_amount++;
						continue;
					}
					this->_statMem[addr] &= ~(0xF<<offs);
				}
			}
		break;
	}

	return this->_amount;
}

u32 CHEATSEARCH::search(u8 comp)
{
	bool didComparePass = false;

	this->_amount = 0;
	
	switch (this->_size)
	{
		case 0:		// 1 byte
			for (u32 i = 0; i < (4 * 1024 * 1024); i++)
			{
				u32	addr = (i >> 3);
				u32	offs = (i % 8);
				if (this->_statMem[addr] & (1<<offs))
				{
					switch (comp)
					{
						case 0: didComparePass = (T1ReadByte(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) > T1ReadByte(this->_mem, i)); break;
						case 1: didComparePass = (T1ReadByte(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) < T1ReadByte(this->_mem, i)); break;
						case 2: didComparePass = (T1ReadByte(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) == T1ReadByte(this->_mem, i)); break;
						case 3: didComparePass = (T1ReadByte(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) != T1ReadByte(this->_mem, i)); break;
						default: didComparePass = false; break;
					}
					if (didComparePass)
					{
						this->_statMem[addr] |= (1<<offs);
						this->_amount++;
						continue;
					}
					this->_statMem[addr] &= ~(1<<offs);
				}
			}
		break;

		case 1:		// 2 bytes
			for (u32 i = 0; i < (4 * 1024 * 1024); i+=2)
			{
				u32	addr = (i >> 3);
				u32	offs = (i % 8);
				if (this->_statMem[addr] & (3<<offs))
				{
					switch (comp)
					{
						case 0: didComparePass = (T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) > T1ReadWord(this->_mem, i)); break;
						case 1: didComparePass = (T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) < T1ReadWord(this->_mem, i)); break;
						case 2: didComparePass = (T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) == T1ReadWord(this->_mem, i)); break;
						case 3: didComparePass = (T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) != T1ReadWord(this->_mem, i)); break;
						default: didComparePass = false; break;
					}
					if (didComparePass)
					{
						this->_statMem[addr] |= (3<<offs);
						this->_amount++;
						continue;
					}
					this->_statMem[addr] &= ~(3<<offs);
				}
			}
		break;

		case 2:		// 3 bytes
			for (u32 i = 0; i < (4 * 1024 * 1024); i+=3)
			{
				u32	addr = (i >> 3);
				u32	offs = (i % 8);
				if (this->_statMem[addr] & (7<<offs))
				{
					switch (comp)
					{
						case 0: didComparePass = ((T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) & 0x00FFFFFF) > (T1ReadLong(this->_mem, i) & 0x00FFFFFF) ); break;
						case 1: didComparePass = ((T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) & 0x00FFFFFF) < (T1ReadLong(this->_mem, i) & 0x00FFFFFF) ); break;
						case 2: didComparePass = ((T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) & 0x00FFFFFF) == (T1ReadLong(this->_mem, i) & 0x00FFFFFF) ); break;
						case 3: didComparePass = ((T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) & 0x00FFFFFF) != (T1ReadLong(this->_mem, i) & 0x00FFFFFF) ); break;
						default: didComparePass = false; break;
					}
					if (didComparePass)
					{
						this->_statMem[addr] |= (7<<offs);
						this->_amount++;
						continue;
					}
					this->_statMem[addr] &= ~(7<<offs);
				}
			}
		break;

		case 3:		// 4 bytes
			for (u32 i = 0; i < (4 * 1024 * 1024); i+=4)
			{
				u32	addr = (i >> 3);
				u32	offs = (i % 8);
				if (this->_statMem[addr] & (0xF<<offs))
				{
					switch (comp)
					{
						case 0: didComparePass = (T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) > T1ReadLong(this->_mem, i)); break;
						case 1: didComparePass = (T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) < T1ReadLong(this->_mem, i)); break;
						case 2: didComparePass = (T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) == T1ReadLong(this->_mem, i)); break;
						case 3: didComparePass = (T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) != T1ReadLong(this->_mem, i)); break;
						default: didComparePass = false; break;
					}
					if (didComparePass)
					{
						this->_statMem[addr] |= (0xF<<offs);
						this->_amount++;
						continue;
					}
					this->_statMem[addr] &= ~(0xF<<offs);
				}
			}
		break;
	}

	memcpy(this->_mem, MMU.MMU_MEM[0][0x20], ( 4 * 1024 * 1024 ) );

	return this->_amount;
}

u32 CHEATSEARCH::getAmount()
{
	return this->_amount;
}

bool CHEATSEARCH::getList(u32 *address, u32 *curVal)
{
	bool didGetValue = false;
	u8 step = this->_size + 1;
	u8 stepMem = 1;

	switch (this->_size)
	{
		case 1: stepMem = 0x3; break;
		case 2: stepMem = 0x7; break;
		case 3: stepMem = 0xF; break;
	}

	for (u32 i = this->_lastRecord; i < (4 * 1024 * 1024); i+=step)
	{
		u32	addr = (i >> 3);
		u32	offs = (i % 8);
		if (this->_statMem[addr] & (stepMem<<offs))
		{
			*address = i;
			this->_lastRecord = i+step;
			
			switch (this->_size)
			{
				case 0: *curVal = (u32)T1ReadByte(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i); break;
				case 1: *curVal = (u32)T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i); break;
				case 2: *curVal = (u32)T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i) & 0x00FFFFFF; break;
				case 3: *curVal = (u32)T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x20], i); break;
				default: break;
			}

			didGetValue = true;
			return didGetValue;
		}
	}
	this->_lastRecord = 0;
	return didGetValue;
}

void CHEATSEARCH::getListReset()
{
	this->_lastRecord = 0;
}

// ========================================================================= Export

CheatDBGame* GetCheatDBGameEntryFromList(const CheatDBGameList &gameList, const char *gameCode, const u32 gameDatabaseCRC)
{
	CheatDBGame *outGameEntry = NULL;
	
	for (size_t i = 0; i < gameList.size(); i++)
	{
		const CheatDBGame &gameEntry = gameList[i];
		
		if ( (gameDatabaseCRC == gameEntry.GetCRC()) && !memcmp(gameCode, gameEntry.GetSerial(), 4) )
		{
			outGameEntry = (CheatDBGame *)&gameEntry;
			break;
		}
	}
	
	return outGameEntry;
}

void CheatItemGenerateDescriptionHierarchical(const char *itemName, const char *itemNote, CHEATS_LIST &outCheatItem)
{
	if (itemName != NULL)
	{
		strncpy(outCheatItem.descriptionMajor, itemName, sizeof(outCheatItem.descriptionMajor));
		outCheatItem.descriptionMajor[sizeof(outCheatItem.descriptionMajor) - 1] = '\0';
	}
	
	if (itemNote != NULL)
	{
		strncpy(outCheatItem.descriptionMinor, itemNote, sizeof(outCheatItem.descriptionMinor));
		outCheatItem.descriptionMinor[sizeof(outCheatItem.descriptionMinor) - 1] = '\0';
	}
}

void CheatItemGenerateDescriptionFlat(const char *folderName, const char *folderNote, const char *itemName, const char *itemNote, CHEATS_LIST &outCheatItem)
{
	std::string descriptionStr = "";
	
	if ( (folderName != NULL) && (*folderName != '\0') )
	{
		descriptionStr += folderName;
	}
	
	if ( (folderNote != NULL) && (*folderNote != '\0') )
	{
		if ( (folderName != NULL) && (*folderName != '\0') )
		{
			descriptionStr += " ";
		}
		
		descriptionStr += "[";
		descriptionStr += folderNote;
		descriptionStr += "]";
	}
	
	if ( ((folderName != NULL) && (*folderName != '\0')) ||
		 ((folderNote != NULL) && (*folderNote != '\0')) )
	{
		descriptionStr += ": ";
	}
	
	if ( (itemName != NULL) && (*itemName != '\0') )
	{
		descriptionStr += itemName;
	}
	
	if ( (itemNote != NULL) && (*itemNote != '\0') )
	{
		if ( (itemName != NULL) && (*itemName != '\0') )
		{
			descriptionStr += " | ";
		}
		
		descriptionStr += itemNote;
	}
	
	strncpy(outCheatItem.description, descriptionStr.c_str(), sizeof(outCheatItem.description));
	outCheatItem.description[sizeof(outCheatItem.description) - 1] = '\0';
}

CheatDBGame::CheatDBGame()
{
	_baseOffset = 0;
	_firstEntryOffset = 0;
	_encryptOffset = 0;
	
	_rawDataSize = 0;
	_workingDataSize = 0;
	_crc = 0;
	_entryCount = 0;
	
	_title = "";
	memset(_serial, 0, sizeof(_serial));
	
	_entryDataRawPtr = NULL;
	_entryData = NULL;
	
	_entryRoot.base = NULL;
	_entryRoot.name = NULL;
	_entryRoot.note = NULL;
	_entryRoot.codeLength = NULL;
	_entryRoot.codeData = NULL;
	_entryRoot.child.resize(0);
	_entryRoot.parent = NULL;
	
	_cheatItemCount = 0;
}

CheatDBGame::CheatDBGame(const u32 encryptOffset, const FAT_R4 &fat, const u32 rawDataSize)
{
	CheatDBGame::SetInitialProperties(rawDataSize, encryptOffset, fat);
	
	_firstEntryOffset = 0;
	_entryCount = 0;
	_title = "";
	
	_entryDataRawPtr = NULL;
	_entryData = NULL;
	
	_entryRoot.base = NULL;
	_entryRoot.name = NULL;
	_entryRoot.note = NULL;
	_entryRoot.codeLength = NULL;
	_entryRoot.codeData = NULL;
	_entryRoot.child.resize(0);
	_entryRoot.parent = NULL;
	
	_cheatItemCount = 0;
}

CheatDBGame::CheatDBGame(FILE *fp, const bool isEncrypted, const u32 encryptOffset, const FAT_R4 &fat, const u32 rawDataSize, u8 (&workingBuffer)[1024])
{
	CheatDBGame::SetInitialProperties(rawDataSize, encryptOffset, fat);
	CheatDBGame::LoadPropertiesFromFile(fp, isEncrypted, workingBuffer);
	
	_entryDataRawPtr = NULL;
	_entryData = NULL;
	
	_entryRoot.base = NULL;
	_entryRoot.name = NULL;
	_entryRoot.note = NULL;
	_entryRoot.codeLength = NULL;
	_entryRoot.codeData = NULL;
	_entryRoot.child.resize(0);
	_entryRoot.parent = NULL;
	
	_cheatItemCount = 0;
}

CheatDBGame::~CheatDBGame()
{
	if (this->_entryDataRawPtr != NULL)
	{
		free(this->_entryDataRawPtr);
		this->_entryDataRawPtr = NULL;
		this->_entryData = NULL;
	}
}

void CheatDBGame::SetInitialProperties(const u32 rawDataSize, const u32 encryptOffset, const FAT_R4 &fat)
{
	_rawDataSize = rawDataSize;
	_workingDataSize = rawDataSize + encryptOffset;
	_encryptOffset = encryptOffset;
	_baseOffset = (u32)fat.addr;
	_crc = fat.CRC;
	_serial[0] = fat.serial[0];
	_serial[1] = fat.serial[1];
	_serial[2] = fat.serial[2];
	_serial[3] = fat.serial[3];
	_serial[4] = '\0';
}

void CheatDBGame::LoadPropertiesFromFile(FILE *fp, const bool isEncrypted, u8 (&workingBuffer)[1024])
{
	const size_t gameDataBufferSize = (_workingDataSize < sizeof(workingBuffer)) ? _workingDataSize : sizeof(workingBuffer);
	CheatDBFile::ReadToBuffer(fp, _baseOffset, isEncrypted, _encryptOffset, gameDataBufferSize, workingBuffer);
	
	const u8 *gameDataBuffer = workingBuffer + _encryptOffset;
	const char *gameTitlePtrInBuffer = (const char *)gameDataBuffer;
	_title = gameTitlePtrInBuffer;
	
	const u32 offsetMask = ~(u32)0x00000003;
	const u32 entryCountOffset = (_baseOffset + (u32)strlen(gameTitlePtrInBuffer) + 4) & offsetMask;
	_entryCount = *(u32 *)(gameDataBuffer + entryCountOffset - _baseOffset);
	_firstEntryOffset = entryCountOffset + 36;
}

u32 CheatDBGame::GetBaseOffset() const
{
	return this->_baseOffset;
}

u32 CheatDBGame::GetFirstEntryOffset() const
{
	return this->_firstEntryOffset;
}

u32 CheatDBGame::GetEncryptOffset() const
{
	return this->_encryptOffset;
}

u32 CheatDBGame::GetRawDataSize() const
{
	return this->_rawDataSize;
}

u32 CheatDBGame::GetWorkingDataSize() const
{
	return this->_workingDataSize;
}

u32 CheatDBGame::GetCRC() const
{
	return this->_crc;
}

u32 CheatDBGame::GetEntryCount() const
{
	return this->_entryCount;
}

u32 CheatDBGame::GetCheatItemCount() const
{
	return this->_cheatItemCount;
}

const char* CheatDBGame::GetTitle() const
{
	return this->_title.c_str();
}

const char* CheatDBGame::GetSerial() const
{
	return this->_serial;
}

const CheatDBEntry& CheatDBGame::GetEntryRoot() const
{
	return this->_entryRoot;
}

const u8* CheatDBGame::GetEntryRawData() const
{
	return this->_entryData;
}

bool CheatDBGame::IsEntryDataLoaded() const
{
	return (this->_entryData != NULL);
}

void CheatDBGame::_UpdateDirectoryParents(CheatDBEntry &directory)
{
	const size_t directoryItemCount = directory.child.size();
	
	for (size_t i = 0; i < directoryItemCount; i++)
	{
		CheatDBEntry &entry = directory.child[i];
		entry.parent = &directory;
		
		const u32 entryValue = *(u32 *)entry.base;
		const bool isFolder = ((entryValue & 0xF0000000) == 0x10000000);
		
		if (isFolder)
		{
			this->_UpdateDirectoryParents(entry);
		}
	}
}

u8* CheatDBGame::LoadEntryData(FILE *fp, const bool isEncrypted)
{
	this->_cheatItemCount = 0;
	
	this->_entryRoot.base = NULL;
	this->_entryRoot.name = NULL;
	this->_entryRoot.note = NULL;
	this->_entryRoot.codeLength = NULL;
	this->_entryRoot.codeData = NULL;
	this->_entryRoot.child.resize(0);
	this->_entryRoot.parent = NULL;
	
	if (this->_entryDataRawPtr != NULL)
	{
		free(this->_entryDataRawPtr);
		this->_entryDataRawPtr = NULL;
		this->_entryData = NULL;
	}
	
	this->_entryDataRawPtr = (u8 *)malloc(this->_workingDataSize + 8);
	memset(this->_entryDataRawPtr, 0, this->_workingDataSize + 8);
	
	bool didReadSuccessfully = CheatDBFile::ReadToBuffer(fp, this->_baseOffset, isEncrypted, this->_encryptOffset, this->_workingDataSize, this->_entryDataRawPtr);
	if (!didReadSuccessfully)
	{
		free(this->_entryDataRawPtr);
		this->_entryDataRawPtr = NULL;
		this->_entryData = NULL;
		
		return this->_entryData;
	}
	
	this->_entryData = this->_entryDataRawPtr + this->_encryptOffset;
	this->_entryRoot.base = this->_entryData;
	this->_entryRoot.name = (char *)this->_entryRoot.base;
	
	CheatDBEntry *currentDirectory = &this->_entryRoot;
	u32 currentDirectoryItemCount = 0;
	std::vector<u32> directoryItemCountList;
	
	CheatDBEntry tempEntry;
	tempEntry.name = NULL;
	tempEntry.note = NULL;
	tempEntry.base = NULL;
	tempEntry.codeLength = NULL;
	tempEntry.codeData = NULL;
	tempEntry.parent = NULL;
	tempEntry.child.resize(0);
	
	const uintptr_t ptrMask = ~(uintptr_t)0x00000003;
	u32 *cmd = (u32 *)(this->_entryData + this->_firstEntryOffset - this->_baseOffset);
	
	for (size_t i = 0; i < this->_entryCount; i++)
	{
		const u32 entryValue = *cmd;
		const bool isFolder = ((entryValue & 0xF0000000) == 0x10000000);
		
		currentDirectory->child.push_back(tempEntry);
		CheatDBEntry &newEntry = currentDirectory->child.back();
		newEntry.parent = currentDirectory;
		
		const u32 baseOffset = (u32)((uintptr_t)cmd - (uintptr_t)this->_entryData);
		newEntry.base = this->_entryData + baseOffset;
		
		const u32 nameOffset = baseOffset + 4;
		newEntry.name = (char *)(this->_entryData + nameOffset);
		
		const u32 noteOffset = nameOffset + (u32)strlen(newEntry.name) + 1;
		newEntry.note = (char *)(this->_entryData + noteOffset);
		
		if (isFolder)
		{
			newEntry.codeLength = NULL;
			newEntry.codeData = NULL;
			cmd = (u32 *)( ((uintptr_t)newEntry.note + strlen(newEntry.note) + 1 + 3) & ptrMask );
			
			const u32 entryCount = entryValue & 0x00FFFFFF;
			
			if (entryCount > 0)
			{
				// Reserve the memory now to avoid std::vector from doing any memory
				// reallocations that would mess up the parent pointers.
				newEntry.child.reserve(entryCount);
				
				if (currentDirectoryItemCount > 1)
				{
					currentDirectoryItemCount--;
					directoryItemCountList.push_back(currentDirectoryItemCount);
				}
			}
			
			currentDirectoryItemCount = entryCount;
			currentDirectory = &newEntry;
		}
		else
		{
			const u32 codeLengthOffset = (noteOffset + (u32)strlen(newEntry.note) + 1 + 3) & 0xFFFFFFFC;
			newEntry.codeLength = (u32 *)(this->_entryData + codeLengthOffset);
			
			const u32 codeDataOffset = codeLengthOffset + 4;
			newEntry.codeData = (u32 *)(this->_entryData + codeDataOffset);
			
			const u32 entrySize = (entryValue & 0x00FFFFFF) + 1; // Note that this does not represent bytes, but the number of 32-bit chunks.
			cmd += entrySize;
			
			if (currentDirectory == &this->_entryRoot)
			{
				// If a cheat item exists at the base level of the tree, then we need to
				// add each cheat item individually.
				currentDirectoryItemCount = 1;
			}
			
			currentDirectoryItemCount--;
			this->_cheatItemCount++;
		}
		
		if (currentDirectoryItemCount == 0)
		{
			currentDirectory = currentDirectory->parent;
			if (currentDirectory == NULL)
			{
				currentDirectory = &this->_entryRoot;
			}
			
			if (directoryItemCountList.size() > 0)
			{
				currentDirectoryItemCount = directoryItemCountList.back();
				directoryItemCountList.pop_back();
			}
		}
	}
	
	// Child items of directories are implemented as std::vectors, which can move their
	// elements around in memory as items are added to the vector. This means that parent
	// pointers are undoubtedly broken at this point. Therefore, we need to update the
	// parent pointers now so that all relationships are properly set.
	//
	// Note that any additions or removals to elements in this->_entryRoot will result in
	// broken parent pointers, and so we must assume that this->_entryRoot is immutable
	// at this point. Considering that the purpose of this->_entryRoot is supposed to be
	// a glorified wrapper to this->_entryData, which itself is immutable, the
	// immutability of this->_entryRoot is deemed acceptable.
	this->_UpdateDirectoryParents(this->_entryRoot);
	
	return this->_entryData;
}

bool CheatDBGame::_CreateCheatItemFromCheatEntry(const CheatDBEntry &inEntry, const bool isHierarchical, CHEATS_LIST &outCheatItem)
{
	bool didParseCheatEntry = false;
	
	if (inEntry.codeLength == NULL)
	{
		return didParseCheatEntry;
	}
	
	u32 codeCount = *inEntry.codeLength / 2;
	if (codeCount > MAX_XX_CODE)
	{
		return didParseCheatEntry;
	}
	
	if (isHierarchical)
	{
		CheatItemGenerateDescriptionHierarchical(inEntry.name, inEntry.note, outCheatItem);
	}
	else
	{
		const char *folderName = ( (inEntry.parent != NULL) && (inEntry.parent != &this->_entryRoot) ) ? inEntry.parent->name : NULL;
		const char *folderNote = ( (inEntry.parent != NULL) && (inEntry.parent != &this->_entryRoot) ) ? inEntry.parent->note : NULL;
		
		CheatItemGenerateDescriptionFlat(folderName, folderNote, inEntry.name, inEntry.note, outCheatItem);
	}
	
	outCheatItem.num = codeCount;
	outCheatItem.type = CHEAT_TYPE_AR;
	
	for (size_t j = 0, t = 0; j < codeCount; j++, t+=2)
	{
		outCheatItem.code[j][0] = *(inEntry.codeData + t + 0);
		outCheatItem.code[j][1] = *(inEntry.codeData + t + 1);
	}
	
	didParseCheatEntry = true;
	return didParseCheatEntry;
}

size_t CheatDBGame::_DirectoryAddCheatsFlat(const CheatDBEntry &directory, const bool isHierarchical, size_t cheatIndex, CHEATS_LIST *outCheatsList)
{
	size_t cheatCount = 0;
	if (outCheatsList == NULL)
	{
		return cheatCount;
	}
	
	const size_t directoryItemCount = directory.child.size();
	for (size_t i = 0; i < directoryItemCount; i++)
	{
		const CheatDBEntry &entry = directory.child[i];
		const u32 entryValue = *(u32 *)entry.base;
		const bool isFolder = ((entryValue & 0xF0000000) == 0x10000000);
		
		if (isFolder)
		{
			const size_t addedCount = this->_DirectoryAddCheatsFlat(entry, isHierarchical, cheatIndex, outCheatsList);
			cheatCount += addedCount;
			cheatIndex += addedCount;
		}
		else
		{
			CHEATS_LIST &outCheat = outCheatsList[cheatIndex];
			
			const bool didParseCheatEntry = this->_CreateCheatItemFromCheatEntry(entry, isHierarchical, outCheat);
			if (!didParseCheatEntry)
			{
				continue;
			}
			
			cheatIndex++;
			cheatCount++;
		}
	}
	
	return cheatCount;
}

size_t CheatDBGame::ParseEntriesToCheatsListFlat(CHEATS_LIST *outCheatsList)
{
	return this->_DirectoryAddCheatsFlat(this->_entryRoot, false, 0, outCheatsList);
}

CheatDBFile::CheatDBFile()
{
	_path = "";
	_description = "";
	
	_type = CHEATS_DB_R4;
	_isEncrypted = false;
	_size = 0;
	_fp = NULL;
}

CheatDBFile::~CheatDBFile()
{
	this->CloseFile();
}

FILE* CheatDBFile::GetFilePtr() const
{
	return this->_fp;
}

bool CheatDBFile::IsEncrypted() const
{
	return this->_isEncrypted;
}

const char* CheatDBFile::GetDescription() const
{
	return this->_description.c_str();
}

int CheatDBFile::OpenFile(const char *filePath)
{
	int error = 0;
	
	this->_fp = fopen(filePath, "rb");
	if (this->_fp == NULL)
	{
		printf("ERROR: Failed to open the cheat database.\n");
		error = 1;
		return error;
	}
	
	// Determine the file's total size.
	fseek(this->_fp, 0, SEEK_END);
	this->_size = ftell(this->_fp);
	
	// Validate the file header before doing anything else. At this time,
	// we will also be determining the file's encryption status.
	const char *headerID = "R4 CheatCode";
	if (this->_size < strlen(headerID))
	{
		printf("ERROR: Failed to validate the file header.\n");
		error = 2;
		return error;
	}
	
	if (this->_size < CHEATDB_FILEOFFSET_FIRST_FAT_ENTRY)
	{
		printf("ERROR: No FAT entries found.\n");
		error = 2;
		return error;
	}
	
	// Read the file header.
	u8 workingBuffer[512] = {0};
	fseek(this->_fp, 0, SEEK_SET);
	const size_t headerReadSize = (this->_size < 512) ? this->_size : 512;
	fread(workingBuffer, 1, headerReadSize, this->_fp);
	
	if (strncmp((char *)workingBuffer, headerID, strlen(headerID)) != 0)
	{
		// Try checking the file header again, assuming that the file is
		// encrypted this time.
		CheatDBFile::R4Decrypt(workingBuffer, headerReadSize, 0);
		if (strncmp((char *)workingBuffer, headerID, strlen(headerID)) != 0)
		{
			// Whether the file is encrypted or unencrypted, the file
			// header failed validation. Therefore, the file will be
			// closed and we will bail out now.
			fclose(this->_fp);
			this->_fp = NULL;
			
			printf("ERROR: Failed to validate the file header.\n");
			error = 2;
			return error;
		}
		
		// File header validation passed, but did so only because we
		// decrypted the header first, so mark the file as encrypted
		// for future operations.
		this->_isEncrypted = true;
	}
	
	this->_description = (const char *)(workingBuffer + CHEATDB_OFFSET_FILE_DESCRIPTION);
	this->_path = filePath;
	
	return error;
}

void CheatDBFile::CloseFile()
{
	if (this->_fp != NULL)
	{
		fclose(this->_fp);
		this->_fp = NULL;
	}
}

void CheatDBFile::R4Decrypt(u8 *buf, const size_t len, u64 n)
{
	size_t r = 0;
	while (r < len)
	{
		u16 key = n ^ 0x484A;
		for (size_t i = 0; (i < 512) && (i < (len - r)); i++)
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

			u32 k = (u32)(((u16)buf[i] << 8) ^ key) << 16;
			u32 x = k;
			for (size_t j = 1; j < 32; j++)
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

bool CheatDBFile::ReadToBuffer(FILE *fp, const size_t fileOffset,
                               const bool isEncrypted, const size_t encryptOffset,
                               const size_t requestedSize, u8 *outBuffer)
{
	bool didReadSuccessfully = false;
	
	if ( (fp == NULL) || (outBuffer == NULL) )
	{
		return didReadSuccessfully;
	}
	
	fseek(fp, fileOffset - encryptOffset, SEEK_SET);
	
	const size_t readSize = fread(outBuffer, 1, requestedSize, fp);
	if (readSize != requestedSize)
	{
		return didReadSuccessfully;
	}

	if (isEncrypted)
	{
		CheatDBFile::R4Decrypt(outBuffer, requestedSize, fileOffset >> 9);
	}
	
	didReadSuccessfully = true;
	return didReadSuccessfully;
}

CheatDBGame CheatDBFile::_ReadGame(const u32 encryptOffset, const FAT_R4 &fat, const u32 gameDataSize, u8 (&workingBuffer)[1024])
{
	return CheatDBGame(this->_fp, this->_isEncrypted, encryptOffset, fat, gameDataSize, workingBuffer);
}

u32 CheatDBFile::LoadGameList(const char *gameCode, const u32 gameDatabaseCRC, CheatDBGameList &outList)
{
	u32 entryCount = 0;
	if (this->_fp == NULL)
	{
		return entryCount;
	}
	
	// If gameCode is NULL, then we're going to assume that the entire
	// game list will be rebuilt from scratch.
	if (gameCode == NULL)
	{
		outList.resize(0);
	}
	
	u8 fatBuffer[512] = {0};
	u8 gameEntryBuffer[1024] = {0};
	u32 pos = CHEATDB_FILEOFFSET_FIRST_FAT_ENTRY;
	u64 t = 0;
	FAT_R4 fatEntryCurrent = {0};
	FAT_R4 fatEntryNext = {0};
	
	if (this->_isEncrypted)
	{
		fseek(this->_fp, 0, SEEK_SET);
		fread(fatBuffer, 1, 512, this->_fp);
		CheatDBFile::R4Decrypt(fatBuffer, 512, 0);
	}
	else
	{
		fseek(this->_fp, pos, SEEK_SET);
		fread(&fatEntryNext, sizeof(fatEntryNext), 1, this->_fp);
	}
	
	do
	{
		if (this->_isEncrypted)
		{
			memcpy(&fatEntryCurrent, &fatBuffer[pos % 512], sizeof(fatEntryCurrent));
			pos += sizeof(fatEntryCurrent);
			if ((pos >> 9) > t)
			{
				t++;
				if ( (t << 9) > this->_size)
				{
					break;
				}
				
				fseek(this->_fp, t << 9, SEEK_SET);
				fread(fatBuffer, 1, 512, this->_fp);
				CheatDBFile::R4Decrypt(fatBuffer, 512, t);
			}
			memcpy(&fatEntryNext, &fatBuffer[pos % 512], sizeof(fatEntryNext));
		}
		else
		{
			memcpy(&fatEntryCurrent, &fatEntryNext, sizeof(fatEntryNext));
			pos += sizeof(fatEntryNext);
			fseek(this->_fp, pos, SEEK_SET);
			fread(&fatEntryNext, sizeof(fatEntryNext), 1, this->_fp);
		}
		
		u32 encryptOffset = 0;
		u32 dataSize = 0;
		
		if (fatEntryNext.addr > 0)
		{
			dataSize = (u32)(fatEntryNext.addr - fatEntryCurrent.addr);
		}
		else
		{
			// For the last game entry, use whatever remaining data there is in the
			// file for the data size. However, we don't know how much extraneous
			// data may exist, so limit the data size to 1 MB, which should be more
			// than enough to account for all of the game entry data.
			dataSize = (u32)(this->_size - fatEntryCurrent.addr);
			if (dataSize > (1024 * 1024))
			{
				dataSize = 1024 * 1024;
			}
		}
		
		if (this->_isEncrypted)
		{
			encryptOffset = (u32)(fatEntryCurrent.addr % 512);
		}
		
		if (dataSize == 0)
		{
			continue;
		}
		
		// If gameCode is provided, then this method will search the file for the first matching
		// serial and CRC, and then stop searching, adding only a single entry to the game list.
		// If all you need is a limited number of games (or just one game), then using this
		// method is a CPU and memory efficient means to get an entry from game list.
		//
		// If gameCode is NULL, then this method will add all game entries from the database file
		// to the game list. This will consume more CPU cycles and memory, but having the entire
		// game list in memory can be useful for clients that allow for database browsing.
		if (gameCode != NULL)
		{
			if ( (gameDatabaseCRC == fatEntryCurrent.CRC) && !memcmp(gameCode, fatEntryCurrent.serial, 4) )
			{
				CheatDBGame *foundGameEntry = GetCheatDBGameEntryFromList(outList, gameCode, gameDatabaseCRC);
				if (foundGameEntry == NULL)
				{
					// Only add to the game list if the entry has not been added yet.
					outList.push_back( this->_ReadGame(encryptOffset, fatEntryCurrent, dataSize, gameEntryBuffer) );
				}
				
				entryCount = 1;
				return entryCount;
			}
		}
		else
		{
			outList.push_back( this->_ReadGame(encryptOffset, fatEntryCurrent, dataSize, gameEntryBuffer) );
			entryCount++;
		}
		
	} while (fatEntryNext.addr > 0);
	
	return entryCount;
}

CHEATSEXPORT::CHEATSEXPORT()
{
	_selectedDbGame = NULL;
	_cheats = NULL;
	_error = 0;
}

CHEATSEXPORT::~CHEATSEXPORT()
{
	this->close();
}

bool CHEATSEXPORT::load(const char *path)
{
	bool didLoadSucceed = false;
	
	this->_error = this->_dbFile.OpenFile(path);
	if (this->_error != 0)
	{
		return didLoadSucceed;
	}
	
	this->_dbFile.LoadGameList(gameInfo.header.gameCode, gameInfo.crcForCheatsDb, this->_tempGameList);
	CheatDBGame *dbGamePtr = GetCheatDBGameEntryFromList(this->_tempGameList, gameInfo.header.gameCode, gameInfo.crcForCheatsDb);
	
	if (dbGamePtr == NULL)
	{
		const char gameCodeString[5] = {
			gameInfo.header.gameCode[0],
			gameInfo.header.gameCode[1],
			gameInfo.header.gameCode[2],
			gameInfo.header.gameCode[3],
			'\0'
		};
		
		printf("ERROR: Cheat for game code '%s' not found in database.\n", gameCodeString);
		this->_error = 3;
		return didLoadSucceed;
	}
	
	u8 *dbGameBuffer = dbGamePtr->LoadEntryData(this->_dbFile.GetFilePtr(), this->_dbFile.IsEncrypted());
	if (dbGameBuffer == NULL)
	{
		printf("ERROR: Failed to read game entries from file.\n");
		this->_error = 1;
		return didLoadSucceed;
	}
	
	if (this->_cheats != NULL)
	{
		free(this->_cheats);
	}
	
	this->_cheats = (CHEATS_LIST *)malloc( sizeof(CHEATS_LIST) * dbGamePtr->GetEntryCount() );
	memset(this->_cheats, 0, sizeof(CHEATS_LIST) * dbGamePtr->GetEntryCount());
	
	const size_t parsedCheatCount = dbGamePtr->ParseEntriesToCheatsListFlat(this->_cheats);
	if (parsedCheatCount == 0)
	{
		printf("ERROR: export cheats failed\n");
		this->_error = 4;
		return didLoadSucceed;
	}
	
	this->_selectedDbGame = dbGamePtr;
	
	didLoadSucceed = true;
	return didLoadSucceed;
}

void CHEATSEXPORT::close()
{
	this->_dbFile.CloseFile();
	
	if (this->_cheats != NULL)
	{
		free(this->_cheats);
		this->_cheats = NULL;
	}
}

CHEATS_LIST *CHEATSEXPORT::getCheats() const
{
	return this->_cheats;
}

size_t CHEATSEXPORT::getCheatsNum() const
{
	if (this->_selectedDbGame == NULL)
	{
		return 0;
	}
	
	return (size_t)this->_selectedDbGame->GetCheatItemCount();
}

const char* CHEATSEXPORT::getGameTitle() const
{
	if (this->_selectedDbGame == NULL)
	{
		return "";
	}
	
	return this->_selectedDbGame->GetTitle();
}

const char* CHEATSEXPORT::getDescription() const
{
	return (const char *)this->_dbFile.GetDescription();
}

u8 CHEATSEXPORT::getErrorCode() const
{
	return this->_error;
}
