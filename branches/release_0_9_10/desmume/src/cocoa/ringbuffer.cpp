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

#include "ringbuffer.h"

#include <string.h>
#include <libkern/OSAtomic.h>


RingBuffer::RingBuffer(const size_t numberElements, const size_t newBufferElementSize)
{
	_elementCapacity = numberElements;
	_elementSize = newBufferElementSize;
	
	_buffer = (uint8_t *)calloc(numberElements + 2, newBufferElementSize);
	_bufferSize = (numberElements + 2) * newBufferElementSize;
	
	_readPosition = 0;
	_writePosition = 1;
	_elementFillCount = 0;
}

RingBuffer::~RingBuffer()
{
	free(_buffer);
	_buffer = NULL;
}

void RingBuffer::clear()
{
	this->_readPosition = 0;
	this->_writePosition = 1;
	this->_elementFillCount = 0;
	memset(_buffer, 0, this->_bufferSize);
}

size_t RingBuffer::read(void *__restrict__ destBuffer, size_t requestedNumberElements)
{
	if (destBuffer == NULL)
	{
		return 0;
	}
	
	size_t hiElementsAvailable = 0;
	size_t loElementsAvailable = 0;
	
	const uint8_t *__restrict__ inputData = this->_buffer;
	size_t inputDataReadPos = this->_readPosition;
	const size_t inputDataWritePos = this->_writePosition;
	const size_t inputDataSize = this->_elementCapacity + 2;
	
	// Check buffer availability
	if (inputDataReadPos < inputDataWritePos)
	{
		hiElementsAvailable = inputDataWritePos - inputDataReadPos - 1;
	}
	else if (inputDataReadPos > inputDataWritePos)
	{
		hiElementsAvailable = inputDataSize - inputDataReadPos - 1;
		loElementsAvailable = inputDataWritePos;
	}
	else
	{
		return requestedNumberElements;
	}
	
	// Bounds check for buffer overrun
	if (requestedNumberElements > hiElementsAvailable + loElementsAvailable)
	{
		requestedNumberElements = hiElementsAvailable + loElementsAvailable;
	}
	
	if (requestedNumberElements == 0)
	{
		return requestedNumberElements;
	}
	
	// Copy ring buffer to destination buffer
	if (requestedNumberElements <= hiElementsAvailable)
	{
		memcpy(destBuffer, inputData + ((inputDataReadPos + 1) * this->_elementSize), requestedNumberElements * this->_elementSize);
	}
	else
	{
		memcpy(destBuffer, inputData + ((inputDataReadPos + 1) * this->_elementSize), hiElementsAvailable * this->_elementSize);
		memcpy((uint8_t *)destBuffer + (hiElementsAvailable * this->_elementSize), inputData, (requestedNumberElements - hiElementsAvailable) * this->_elementSize);
	}
	
	// Advance the read position
	inputDataReadPos += requestedNumberElements;
	if (inputDataReadPos >= inputDataSize)
	{
		inputDataReadPos -= inputDataSize;
	}
	
	this->_readPosition = inputDataReadPos;
	
	// Decrease the fill size now that we're done reading.
	OSAtomicAdd32Barrier(-(int32_t)requestedNumberElements, &this->_elementFillCount);
	
	return requestedNumberElements;
}

size_t RingBuffer::write(const void *__restrict__ srcBuffer, size_t requestedNumberElements)
{
	if (srcBuffer == NULL)
	{
		return 0;
	}
	
	size_t hiElementsAvailable = 0;
	size_t loElementsAvailable = 0;
	
	uint8_t *__restrict__ inputData = this->_buffer;
	const size_t inputDataReadPos = this->_readPosition;
	size_t inputDataWritePos = this->_writePosition;
	const size_t inputDataSize = this->_elementCapacity + 2;
	
	// Check buffer availability.
	if (inputDataWritePos > inputDataReadPos)
	{
		hiElementsAvailable = inputDataSize - inputDataWritePos;
		loElementsAvailable = (inputDataReadPos > 0) ? inputDataReadPos - 1 : 0;
	}
	else if (inputDataWritePos < inputDataReadPos)
	{
		hiElementsAvailable = inputDataReadPos - inputDataWritePos - 1;
	}
	else
	{
		return requestedNumberElements;
	}
	
	// Bounds check for buffer overrun
	if (requestedNumberElements > hiElementsAvailable + loElementsAvailable)
	{
		requestedNumberElements = hiElementsAvailable + loElementsAvailable;
	}
	
	if (requestedNumberElements == 0)
	{
		return requestedNumberElements;
	}
	
	// Increase the fill size before writing anything.
	OSAtomicAdd32Barrier((int32_t)requestedNumberElements, &this->_elementFillCount);
	
	// Copy source buffer to ring buffer.
	if (requestedNumberElements <= hiElementsAvailable)
	{
		memcpy(inputData + (inputDataWritePos * this->_elementSize), srcBuffer, requestedNumberElements * this->_elementSize);
	}
	else
	{
		memcpy(inputData + (inputDataWritePos * this->_elementSize), srcBuffer, hiElementsAvailable * this->_elementSize);
		memcpy(inputData, (uint8_t *)srcBuffer + (hiElementsAvailable * this->_elementSize), (requestedNumberElements - hiElementsAvailable) * this->_elementSize);
	}
	
	// Advance the write position.
	inputDataWritePos += requestedNumberElements;
	if (inputDataWritePos > inputDataSize)
	{
		inputDataWritePos -= inputDataSize;
	}
	
	this->_writePosition = inputDataWritePos;
	
	return requestedNumberElements;
}

size_t RingBuffer::drop(size_t requestedNumberElements)
{
	size_t hiElementsAvailable = 0;
	size_t loElementsAvailable = 0;
	
	size_t inputDataReadPos = this->_readPosition;
	const size_t inputDataWritePos = this->_writePosition;
	const size_t inputDataSize = this->_elementCapacity + 2;
	
	// Check buffer availability
	if (inputDataReadPos < inputDataWritePos)
	{
		hiElementsAvailable = inputDataWritePos - inputDataReadPos - 1;
	}
	else if (inputDataReadPos > inputDataWritePos)
	{
		hiElementsAvailable = inputDataSize - inputDataReadPos - 1;
		loElementsAvailable = inputDataWritePos;
	}
	else
	{
		return requestedNumberElements;
	}
	
	// Bounds check for buffer overrun
	if (requestedNumberElements > hiElementsAvailable + loElementsAvailable)
	{
		requestedNumberElements = hiElementsAvailable + loElementsAvailable;
	}
	
	if (requestedNumberElements == 0)
	{
		return requestedNumberElements;
	}
	
	// Advance the read position
	inputDataReadPos += requestedNumberElements;
	if (inputDataReadPos >= inputDataSize)
	{
		inputDataReadPos -= inputDataSize;
	}
	
	this->_readPosition = inputDataReadPos;
	
	// Decrease the fill size now that we're done reading.
	OSAtomicAdd32Barrier(-(int32_t)requestedNumberElements, &this->_elementFillCount);
	
	return requestedNumberElements;
}

size_t RingBuffer::getAvailableElements() const
{
	return (this->_elementCapacity - this->_elementFillCount);
}

size_t RingBuffer::getElementCapacity() const
{
	return this->_elementCapacity;
}

size_t RingBuffer::getElementSize() const
{
	return this->_elementSize;
}

bool RingBuffer::isEmpty() const
{
	return (this->_elementFillCount == 0);
}

bool RingBuffer::isFull() const
{
	return ((size_t)this->_elementFillCount >= this->_elementCapacity);
}
