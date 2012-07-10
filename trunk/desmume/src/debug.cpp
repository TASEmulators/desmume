/*
	Copyright (C) 2006 Guillaume Duhamel
	Copyright (C) 2006-2011 DeSmuME team

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

#include "debug.h"

#include <algorithm>
#include <stdarg.h>
#include <stdio.h>
#include "MMU.h"
#include "armcpu.h"
#include "instructions.h"
#include "cp15.h"
#include "NDSSystem.h"
#include "utils/xstring.h"
#include "movie.h"

#ifdef HAVE_LUA
#include "lua-engine.h"
#endif

armcpu_t* TDebugEventData::cpu() { return procnum==0?&NDS_ARM9:&NDS_ARM7; }

TDebugEventData DebugEventData;
u32 debugFlag;

//DEBUG CONFIGURATION
const bool debug_acl = false;
const bool debug_cacheMiss = false;

static bool acl_check_access(u32 adr, u32 access) {

	//non-user modes get separate access handling, so check that here
	if(NDS_ARM9.CPSR.bits.mode != USR)
		access |= 1;
	
	if (cp15.isAccessAllowed(adr,access)==FALSE) {
		HandleDebugEvent(DEBUG_EVENT_ACL_EXCEPTION);
	}
	return true;
}

void HandleDebugEvent_ACL_Exception()
{
	printf("ACL EXCEPTION!\n");
	if(DebugEventData.memAccessType == MMU_AT_CODE)
		armcpu_exception(DebugEventData.cpu(),EXCEPTION_PREFETCH_ABORT);
	else if(DebugEventData.memAccessType == MMU_AT_DATA)
		armcpu_exception(DebugEventData.cpu(),EXCEPTION_DATA_ABORT);
}


static bool CheckRange(u32 adr, u32 min, u32 len)
{
	return (adr>=min && adr<min+len);
}

void HandleDebugEvent_Read()
{
	if(!debug_acl) return;
	if(DebugEventData.procnum != ARMCPU_ARM9) return; //acl only valid on arm9
	acl_check_access(DebugEventData.addr,CP15_ACCESS_READ);
}

void HandleDebugEvent_Write()
{
	if(!debug_acl) return;
	if(DebugEventData.procnum != ARMCPU_ARM9) return; //acl only valid on arm9
	acl_check_access(DebugEventData.addr,CP15_ACCESS_WRITE);
}

void HandleDebugEvent_Execute()
{
	//HACKY BREAKPOINTS!
	//extern bool nds_debug_continuing[2];
	//if(!nds_debug_continuing[DebugEventData.procnum]) //dont keep hitting the same breakpoint
	//{
	//	if((DebugEventData.addr & 0xFFFFFFF0) == 0x02000000)
	//	{
	//		void NDS_debug_break();
	//		NDS_debug_break();
	//	}
	//}
	if(!debug_acl) return;
	if(DebugEventData.procnum != ARMCPU_ARM9) return; //acl only valid on arm9
	acl_check_access(DebugEventData.addr,CP15_ACCESS_EXECUTE);
}

void HandleDebugEvent_CacheMiss()
{
	if(!debug_cacheMiss) return;
	extern int currFrameCounter;
	if(currFrameCounter<200) return;
	static FILE* outf = NULL;
	if(!outf) outf = fopen("c:\\miss.txt","wb");
	fprintf(outf,"%05d,%08X,%d\n",currFrameCounter,DebugEventData.addr,DebugEventData.size);
}

//------------------------------------------------
DebugStatistics DEBUG_statistics;

DebugStatistics::DebugStatistics()
{
}

DebugStatistics::InstructionHits::InstructionHits()
{
	memset(&arm,0,sizeof(arm));
	memset(&thumb,0,sizeof(thumb));
}


static DebugStatistics::InstructionHits combinedHits[2];

template<int proc, int which>
static bool debugStatsSort(int num1, int num2) {
	if(which==0) {
		if(combinedHits[proc].arm[num2] == combinedHits[proc].arm[num1]) return false;
		if(combinedHits[proc].arm[num1] == 0xFFFFFFFF) return false;
		if(combinedHits[proc].arm[num2] == 0xFFFFFFFF) return true;
		return combinedHits[proc].arm[num2] < combinedHits[proc].arm[num1];
	}
	else {
		if(combinedHits[proc].thumb[num2] == combinedHits[proc].thumb[num1]) return false;
		if(combinedHits[proc].thumb[num1] == 0xFFFFFFFF) return false;
		if(combinedHits[proc].thumb[num2] == 0xFFFFFFFF) return true;
		return combinedHits[proc].thumb[num2] < combinedHits[proc].thumb[num1];
	}
}

void DebugStatistics::print()
{
	//consolidate opcodes with the same names
	for(int i=0;i<2;i++) { 
		combinedHits[i] = DEBUG_statistics.instructionHits[i];

		for(int j=0;j<4096;j++) {
			if(combinedHits[i].arm[j] == 0xFFFFFFFF) 
				continue;
			std::string name = arm_instruction_names[j];
			for(int k=j+1;k<4096;k++) {
				if(combinedHits[i].arm[k] == 0xFFFFFFFF) 
					continue;
				if(name == arm_instruction_names[k]) {
					//printf("combining %s with %d and %d\n",name.c_str(),combinedHits[i].arm[j],combinedHits[i].arm[k]);
					combinedHits[i].arm[j] += combinedHits[i].arm[k];
					combinedHits[i].arm[k] = 0xFFFFFFFF;
				}
				
			}
		}

		for(int j=0;j<1024;j++) {
			if(combinedHits[i].thumb[j] == 0xFFFFFFFF) 
				continue;
			std::string name = thumb_instruction_names[j];
			for(int k=j+1;k<1024;k++) {
				if(combinedHits[i].thumb[k] == 0xFFFFFFFF) 
					continue;
				if(name == thumb_instruction_names[k]) {
					//printf("combining %s with %d and %d\n",name.c_str(),combinedHits[i].arm[j],combinedHits[i].arm[k]);
					combinedHits[i].thumb[j] += combinedHits[i].thumb[k];
					combinedHits[i].thumb[k] = 0xFFFFFFFF;
				}
				
			}
		}
	}

	InstructionHits sorts[2];
	for(int i=0;i<2;i++) {
		for(int j=0;j<4096;j++) sorts[i].arm[j] = j;
		for(int j=0;j<1024;j++) sorts[i].thumb[j] = j;
	}
	std::sort(sorts[0].arm, sorts[0].arm+4096, debugStatsSort<0,0>);
	std::sort(sorts[0].thumb, sorts[0].thumb+1024, debugStatsSort<0,1>);
	std::sort(sorts[1].arm, sorts[1].arm+4096, debugStatsSort<1,0>);
	std::sort(sorts[1].thumb, sorts[1].thumb+1024, debugStatsSort<1,1>);

	for(int i=0;i<2;i++) {
		printf("Top arm instructions for ARM%d:\n",7+i*2);
		for(int j=0;j<10;j++) {
			int val = sorts[i].arm[j];
			printf("%08d: %s\n", combinedHits[i].arm[val], arm_instruction_names[val]);
		}
		printf("Top thumb instructions for ARM%d:\n",7+i*2);
		for(int j=0;j<10;j++) {
			int val = sorts[i].thumb[j];
			printf("%08d: %s\n", combinedHits[i].thumb[val], thumb_instruction_names[val]);
		}
	}
}

void DebugStatistics::printSequencerExecutionCounters()
{
	for(int i=0;i<21;i++) printf("%06d ",sequencerExecutionCounters[i]);
	printf("\n");
}

void DEBUG_reset()
{
	//for now, just enable all debugging in developer builds
#ifdef DEVELOPER
	debugFlag = 1;
#endif

	DEBUG_Notify = DebugNotify();
	DEBUG_statistics = DebugStatistics();
	printf("DEBUG_reset: %08X\n",&DebugStatistics::print); //force a reference to this function
}

static void DEBUG_dumpMemory_fill(EMUFILE *fp, u32 size)
{
	static std::vector<u8> buf;
	buf.resize(size);
	memset(&buf[0],0,size);
	fp->fwrite(&buf[0],size);
}

void DEBUG_dumpMemory(EMUFILE* fp)
{
	fp->fseek(0x000000,SEEK_SET); fp->fwrite(MMU.MAIN_MEM,0x800000); //arm9 main mem (8192K)
	fp->fseek(0x900000,SEEK_SET); fp->fwrite(MMU.ARM9_DTCM,0x4000); //arm9 DTCM (16K)
	fp->fseek(0xA00000,SEEK_SET); fp->fwrite(MMU.ARM9_ITCM,0x8000); //arm9 ITCM (32K)
	fp->fseek(0xB00000,SEEK_SET); fp->fwrite(MMU.ARM9_LCD,0xA4000); //LCD mem 656K
	fp->fseek(0xC00000,SEEK_SET); fp->fwrite(MMU.ARM9_VMEM,0x800); //OAM
	fp->fseek(0xD00000,SEEK_SET); fp->fwrite(MMU.ARM7_ERAM,0x10000); //arm7 WRAM (64K)
	fp->fseek(0xE00000,SEEK_SET); fp->fwrite(MMU.ARM7_WIRAM,0x10000); //arm7 wifi RAM ?
	fp->fseek(0xF00000,SEEK_SET); fp->fwrite(MMU.SWIRAM,0x8000); //arm9/arm7 shared WRAM (32KB)
}

//----------------------------------------------------

std::vector<Logger *> Logger::channels;


static void defaultCallback(const Logger& logger, const char * message) {
	logger.getOutput() << message;
}

Logger::Logger() {
	out = &std::cout;
	callback = defaultCallback;
	flags = 0;
}

Logger::~Logger() {
	for(int i=0;i<(int)channels.size();i++)
		delete channels[i];
}

void Logger::vprintf(const char * format, va_list l, const char * file, unsigned int line) {
	char buffer[1024];
	char * cur = buffer;

	if (flags & Logger::FILE) cur += sprintf(cur, "%s:", file);
	if (flags & Logger::LINE) cur += sprintf(cur, "%d:", line);
	if (flags) cur += sprintf(cur, " ");

	::vsnprintf(cur, 1024, format, l);
	callback(*this, buffer);
}

void Logger::setOutput(std::ostream * o) {
	out = o;
}

void Logger::setCallback(void (*cback)(const Logger& logger, const char * message)) {
	callback = cback;
}

void Logger::setFlag(unsigned int flag) {
	this->flags = flag;
}

void Logger::fixSize(unsigned int channel) {
	while(channel >= channels.size()) {
		channels.push_back(new Logger());
	}
}

std::ostream& Logger::getOutput() const {
	return *out;
}

void Logger::log(unsigned int channel, const char * file, unsigned int line, const char * format, ...) {
	fixSize(channel);

	va_list l;
	va_start(l, format);
	channels[channel]->vprintf(format, l, file, line);
	va_end(l);
}

void Logger::log(unsigned int channel, const char * file, unsigned int line, std::ostream& os) {
	fixSize(channel);

	channels[channel]->setOutput(&os);
}

void Logger::log(unsigned int channel, const char * file, unsigned int line, unsigned int flag) {
	fixSize(channel);

	channels[channel]->setFlag(flag);
}

void Logger::log(unsigned int channel, const char * file, unsigned int line, void (*callback)(const Logger& logger, const char * message)) {
	fixSize(channel);

	channels[channel]->setCallback(callback);
}

void IdeasLog(armcpu_t* cpu)
{
	u32 adr = cpu->R[0];
	for(;;) {
		u8 c = _MMU_read08(cpu->proc_ID, MMU_AT_DEBUG, adr);
		adr++;
		if(!c) break;
		printf("%c",c);
	}
	//don't emit a newline. that is a pain in the butt.
}

void NocashMessage(armcpu_t* cpu, int offset)
{
	u32 adr = cpu->instruct_adr + offset;

	std::string todo;
	for(;;) {
		u8 c = _MMU_read08(cpu->proc_ID, MMU_AT_DEBUG, adr);
		adr++;
		if(!c) break;
		todo.push_back(c);
	}

	//r0,r1,r2,...,r15  show register content (displayed as 32bit Hex number)
	//sp,lr,pc          alias for r13,r14,r15
	//scanline          show current scanline number
	//frame             show total number of frames since coldboot
	//totalclks         show total number of clock cycles since coldboot
	//lastclks          show number of cycles since previous lastclks (or zeroclks)
	//zeroclks          resets the 'lastclks' counter

	//this is very inefficiently coded!
	char tmp[100];
	todo = mass_replace(todo,"%sp%","%r13%");
	todo = mass_replace(todo,"%lr%","%r14%");
	todo = mass_replace(todo,"%pc%","%r15%");
	sprintf(tmp,"%08X",cpu->R[0]); todo = mass_replace(todo,"%r0%",tmp);
	sprintf(tmp,"%08X",cpu->R[1]); todo = mass_replace(todo,"%r1%",tmp);
	sprintf(tmp,"%08X",cpu->R[2]); todo = mass_replace(todo,"%r2%",tmp);
	sprintf(tmp,"%08X",cpu->R[3]); todo = mass_replace(todo,"%r3%",tmp);
	sprintf(tmp,"%08X",cpu->R[4]); todo = mass_replace(todo,"%r4%",tmp);
	sprintf(tmp,"%08X",cpu->R[5]); todo = mass_replace(todo,"%r5%",tmp);
	sprintf(tmp,"%08X",cpu->R[6]); todo = mass_replace(todo,"%r6%",tmp);
	sprintf(tmp,"%08X",cpu->R[7]); todo = mass_replace(todo,"%r7%",tmp);
	sprintf(tmp,"%08X",cpu->R[8]); todo = mass_replace(todo,"%r8%",tmp);
	sprintf(tmp,"%08X",cpu->R[9]); todo = mass_replace(todo,"%r9%",tmp);
	sprintf(tmp,"%08X",cpu->R[10]); todo = mass_replace(todo,"%r10%",tmp);
	sprintf(tmp,"%08X",cpu->R[11]); todo = mass_replace(todo,"%r11%",tmp);
	sprintf(tmp,"%08X",cpu->R[12]); todo = mass_replace(todo,"%r12%",tmp);
	sprintf(tmp,"%08X",cpu->R[13]); todo = mass_replace(todo,"%r13%",tmp);
	sprintf(tmp,"%08X",cpu->R[14]); todo = mass_replace(todo,"%r14%",tmp);
	sprintf(tmp,"%08X",cpu->R[15]); todo = mass_replace(todo,"%r15%",tmp);
	sprintf(tmp,"%d",nds.VCount); todo = mass_replace(todo,"%scanline%",tmp);
	sprintf(tmp,"%d",currFrameCounter); todo = mass_replace(todo,"%frame%",tmp);
	sprintf(tmp,"%lld",nds_timer); todo = mass_replace(todo,"%totalclks%",tmp);

	printf("%s",todo.c_str());
}

//-------
DebugNotify DEBUG_Notify;

//enable bits arent being used right now.
//if you want exhaustive logging, move the print before the early return (or comment the early return)

//the intent of this system is to provide a compact dialog box showing which debug notifies have been
//triggered in this frame (with a glowing LED!) and which debug notifies have been triggered EVER
//which can be cleared, like a clip indicator in an audio tool.
//obviously all this isnt implemented yet.

void DebugNotify::NextFrame()
{
#ifdef DEVELOPER
	pingBits.reset();
#endif
}

void DebugNotify::ReadBeyondEndOfCart(u32 addr, u32 romsize)
{
#ifdef DEVELOPER
	if(!ping(DEBUG_NOTIFY_READ_BEYOND_END_OF_CART)) return;
	INFO("Reading beyond end of cart! ... %08X >= %08X\n",addr,romsize);
#endif
}

bool DebugNotify::ping(EDEBUG_NOTIFY which)
{
	bool wasPinged = pingBits[(int)which];
	pingBits[(int)which] = true;
	return !wasPinged;
}
