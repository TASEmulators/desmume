/*  Copyright (C) 2008 Guillaume Duhamel

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

#ifndef LOGGER_H
#define LOGGER_H

#include <vector>
#include <iostream>

class Logger {
protected:
	void (*callback)(const Logger& logger, const char * format);
	std::ostream * out;
	unsigned int flags;

	static std::vector<Logger *> channels;

	static void fixSize(unsigned int channel);
public:
	Logger();

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

#ifdef DEBUG

#define LOGC(channel, ...) Logger::log(channel, __FILE__, __LINE__, __VA_ARGS__)
#define LOG(...) LOGC(0, __VA_ARGS__)

#define GPULOG(...) LOGC(1, __VA_ARGS__)
#define DIVLOG(...) LOGC(2, __VA_ARGS__)
#define SQRTLOG(...) LOGC(3, __VA_ARGS__)
#define DMALOG(...) LOGC(3, __VA_ARGS__)

#else

#define LOGC(...)
#define LOG(...)

#define GPULOG(...)
#define DIVLOG(...)
#define SQRTLOG(...)
#define DMALOG(...)

#endif

#define INFOC(channel, ...) Logger::log(channel, __FILE__, __LINE__, __VA_ARGS__)
#define INFO(...) INFOC(10, __VA_ARGS__)

#endif
