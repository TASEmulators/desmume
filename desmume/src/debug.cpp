/*  Copyright (C) 2008 Guillaume Duhamel
	Copyright (C) 2009 DeSmuME team

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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "debug.h"

#include <algorithm>
#include <stdarg.h>
#include <stdio.h>
#include "MMU.h"
#include "armcpu.h"
#include "arm_instructions.h"
#include "thumb_instructions.h"

std::vector<Logger *> Logger::channels;

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
	DEBUG_statistics = DebugStatistics();
	printf("DEBUG_reset: %08X\n",&DebugStatistics::print); //force a reference to this function
}

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
	printf("EMULOG%c: ",cpu->proc_ID==0?'9':'7');
	for(;;) {
		u8 c = MMU_read8(cpu->proc_ID,adr);
		adr++;
		if(!c) break;
		printf("%c",c);
	}
	printf("\n");
}

