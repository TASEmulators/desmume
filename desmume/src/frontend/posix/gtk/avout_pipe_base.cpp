/*
	Copyright (C) 2014 DeSmuME team

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

#include <unistd.h>
#include <cerrno>

#include "types.h"
#include "SPU.h"

#include "avout_pipe_base.h"

static inline int writeAll(int fd, const void* buf, size_t count) {
	ssize_t written = 0, writtenTotal = 0;
	do {
		written = write(fd, ((u8*)buf) + writtenTotal, count - writtenTotal);
	} while (written >= 0 && (writtenTotal += written) < count);
	return written;
}

bool AVOutPipeBase::begin(const char* fname) {
	if (this->recording) {
		return false;
	}
	const char* const* args = this->getArgv(fname);
	if (args == NULL) {
		return false;
	}
	int pipefd[2];
	if (pipe(pipefd) < 0) {
		fprintf(stderr, "Fail to open pipe\n");
		return false;
	}
	pid_t pid = fork();
	if (pid == 0) {
		close(pipefd[1]);
		dup2(pipefd[0], STDIN_FILENO);
		execvp(args[0], (char* const*)args);
		fprintf(stderr, "Fail to start %s: %d %s\n", args[0], errno, strerror(errno));
		_exit(1);
	}
	close(pipefd[0]);
	this->pipe_fd = pipefd[1];
	this->recording = true;
	return true;
}

void AVOutPipeBase::end() {
	if (this->recording) {
		close(this->pipe_fd);
		this->recording = false;
	}
}

bool AVOutPipeBase::isRecording() {
	return this->recording;
}

void AVOutPipeBase::updateAudio(void* soundData, int soundLen) {
	if(!this->recording || this->type() != TYPE_AUDIO) {
		return;
	}
	if (writeAll(this->pipe_fd, soundData, soundLen * 2 * 2) == -1) {
		fprintf(stderr, "Error on writing audio: %d %s\n", errno, strerror(errno));
		this->end();
	}
}

void AVOutPipeBase::updateVideo(const u16* buffer) {
	if(!this->recording || this->type() != TYPE_VIDEO) {
		return;
	}
	u8 rgb[256 * 384 * 3];
	u8* cur = rgb;
	for (int i = 0; i < 256 * 384; i++) {
		u16 gpu_pixel = buffer[i];
		*cur = ((gpu_pixel >> 0) & 0x1f) << 3;
		cur++;
		*cur = ((gpu_pixel >> 5) & 0x1f) << 3;
		cur++;
		*cur = ((gpu_pixel >> 10) & 0x1f) << 3;
		cur++;
	}
	if (write(this->pipe_fd, rgb, 256 * 384 * 3) == -1) {
		fprintf(stderr, "Error on writing video: %d %s\n", errno, strerror(errno));
		this->end();
	}
}

