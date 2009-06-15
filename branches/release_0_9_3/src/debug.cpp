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

#include "debug.h"

#include <stdarg.h>
#include <stdio.h>

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
