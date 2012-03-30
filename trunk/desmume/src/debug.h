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

#ifndef DEBUG_H
#define DEBUG_H

#include <vector>
#include <iostream>
#include <cstdarg>
#include <bitset>

#include "types.h"
#include "mem.h"
#include "emufile.h"

struct DebugStatistics
{
	DebugStatistics();
	struct InstructionHits {
		InstructionHits();
		u32 thumb[1024];
		u32 arm[4096];
	} instructionHits[2]; //one for each cpu

	s32 sequencerExecutionCounters[32];

	void print();
	void printSequencerExecutionCounters();
};

extern DebugStatistics DEBUG_statistics;

void DEBUG_reset();
void DEBUG_dumpMemory(EMUFILE* fp);

struct armcpu_t;

class Logger {
protected:
	void (*callback)(const Logger& logger, const char * format);
	std::ostream * out;
	unsigned int flags;

	static std::vector<Logger *> channels;

	static void fixSize(unsigned int channel);
public:
	Logger();
	~Logger();

	void vprintf(const char * format, va_list l, const char * filename, unsigned int line);
	void setOutput(std::ostream * o);
	void setCallback(void (*cback)(const Logger& logger, const char * message));
	void setFlag(unsigned int flag);

	std::ostream& getOutput() const;

	static const int LINE = 1;
	static const int FILE = 2;

	static void log(unsigned int channel, const char * file, unsigned int line, const char * format, ...);
	static void log(unsigned int channel, const char * file, unsigned int line, std::ostream& os);
	static void log(unsigned int channel, const char * file, unsigned int line, unsigned int flag);
	static void log(unsigned int channel, const char * file, unsigned int line, void (*callback)(const Logger& logger, const char * message));
};

#if defined(DEBUG) || defined(GPUDEBUG) || defined(DIVDEBUG) || defined(SQRTDEBUG) || defined(DMADEBUG) || defined(DEVELOPER)
#define LOGC(channel, ...) Logger::log(channel, __FILE__, __LINE__, __VA_ARGS__)
#else
#define LOGC(...) {}
#endif

#ifdef DEBUG
#define LOG(...) LOGC(0, __VA_ARGS__)
#else
#define LOG(...) {}
#endif

#ifdef GPUDEBUG
#define GPULOG(...) LOGC(1, __VA_ARGS__)
#else
#define GPULOG(...) {}
#endif

#ifdef DIVDEBUG
#define DIVLOG(...) LOGC(2, __VA_ARGS__)
#else
#define DIVLOG(...) {}
#endif

#ifdef SQRTDEBUG
#define SQRTLOG(...) LOGC(3, __VA_ARGS__)
#else
#define SQRTLOG(...) {}
#endif

#ifdef DMADEBUG
#define DMALOG(...) LOGC(4, __VA_ARGS__)
#else
#define DMALOG(...) {}
#endif

#ifdef CFLASHDEBUG
#define CFLASHLOG(...) LOGC(5, __VA_ARGS__)
#else
#define CFLASHLOG(...) {}
#endif

#ifdef UNTESTEDOPCODEDEBUG
#define UNTESTEDOPCODELOG(...) LOGC(6, __VA_ARGS__)
#else
#define UNTESTEDOPCODELOG(...) {}
#endif


#ifdef DEVELOPER
#define PROGINFO(...) LOGC(7, __VA_ARGS__)
#else
#define PROGINFO(...) {}
#endif


#define INFOC(channel, ...) Logger::log(channel, __FILE__, __LINE__, __VA_ARGS__)
#define INFO(...) INFOC(10, __VA_ARGS__)

void IdeasLog(armcpu_t* cpu);
void NocashMessage(armcpu_t* cpu, int offset);

enum EDEBUG_EVENT
{
	DEBUG_EVENT_READ=1, //read from arm9 or arm7 bus, including cpu prefetch
	DEBUG_EVENT_WRITE=2, //write on arm9 or arm7 bus
	DEBUG_EVENT_EXECUTE=3, //prefetch on arm9 or arm7, triggered after the read event
	DEBUG_EVENT_ACL_EXCEPTION=4, //acl exception on arm9
	DEBUG_EVENT_CACHE_MISS=5, //cache miss on arm9
};

enum EDEBUG_NOTIFY
{
	DEBUG_NOTIFY_READ_BEYOND_END_OF_CART,
	DEBUG_NOTIFY_MAX
};

class DebugNotify
{
public:
	void NextFrame();
	void ReadBeyondEndOfCart(u32 addr, u32 romsize);
private:
	std::bitset<DEBUG_NOTIFY_MAX> pingBits;
	std::bitset<DEBUG_NOTIFY_MAX> enableBits;
	bool ping(EDEBUG_NOTIFY which);
};

extern DebugNotify DEBUG_Notify;
struct armcpu_t;

//information about a debug event will be stuffed into here by the generator
struct TDebugEventData
{
	MMU_ACCESS_TYPE memAccessType;
	u32 procnum, addr, size, val;
	armcpu_t* cpu();
};

extern TDebugEventData DebugEventData;

//bits in here are set according to what debug handlers are installed?
//for now it is just a single bit
extern u32 debugFlag;

FORCEINLINE bool CheckDebugEvent(EDEBUG_EVENT event)
{
	//for now, debug events are only handled in dev+ builds
#ifndef DEVELOPER
	return false;
#endif

	if(!debugFlag) return false;

	return true;
}

void HandleDebugEvent_Read();
void HandleDebugEvent_Write();
void HandleDebugEvent_Execute();
void HandleDebugEvent_ACL_Exception();
void HandleDebugEvent_CacheMiss();

inline void HandleDebugEvent(EDEBUG_EVENT event)
{
	switch(event)
	{
	case DEBUG_EVENT_READ: HandleDebugEvent_Read(); return;
	case DEBUG_EVENT_WRITE: HandleDebugEvent_Write(); return;
	case DEBUG_EVENT_EXECUTE: HandleDebugEvent_Execute(); return;
	case DEBUG_EVENT_ACL_EXCEPTION: HandleDebugEvent_ACL_Exception(); return;
	case DEBUG_EVENT_CACHE_MISS: HandleDebugEvent_CacheMiss(); return;
	}
}


#endif
