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

#ifndef DEBUG_H
#define DEBUG_H

#include <vector>
#include <iostream>
#include <cstdarg>

#include "types.h"

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

#ifdef UNTESTEDOPCODELOG
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

#endif
