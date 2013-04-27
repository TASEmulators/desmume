/*
	Copyright (C) 2013 DeSmuME team

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

#ifndef _RINGBUFFER_
#define _RINGBUFFER_

#include <stdlib.h>
#include <stdint.h>


class RingBuffer
{
private:
	uint8_t *_buffer;
	size_t _bufferSize;
	
	size_t _elementCapacity;
	size_t _elementSize;
	int32_t _elementFillCount; // need to use int32_t for OSAtomicAdd32Barrier()
	size_t _readPosition;
	size_t _writePosition;
	
public:
	RingBuffer(const size_t numberElements, const size_t newBufferElementSize);
	~RingBuffer();
	
	void clear();
	size_t read(void *__restrict__ destBuffer, size_t requestedNumberElements);
	size_t write(const void *__restrict__ srcBuffer, size_t requestedNumberElements);
	size_t drop(size_t requestedNumberElements);
	size_t getAvailableElements() const;
	size_t getElementCapacity() const;
	size_t getElementSize() const;
	bool isEmpty() const;
	bool isFull() const;
};

#endif
