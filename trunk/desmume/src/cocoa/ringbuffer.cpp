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
	_buffer = (uint8_t *)calloc(numberElements + 1, newBufferElementSize);
	_bufferSize = (numberElements + 1) * newBufferElementSize;
	_numElements = numberElements;
	_elementSize = newBufferElementSize;
	
	_readPosition = newBufferElementSize - 1;
	_writePosition = newBufferElementSize;
	_bufferFillSize = 0;
}

RingBuffer::~RingBuffer()
{
	free(_buffer);
	_buffer = NULL;
}

void RingBuffer::clear()
{
	this->_readPosition = this->_elementSize - 1;
	this->_writePosition = this->_elementSize;
	this->_bufferFillSize = 0;
	memset(_buffer, 0, this->_bufferSize);
}

size_t RingBuffer::read(void *__restrict__ destBuffer, size_t requestedNumberBytes)
{
	if (destBuffer == NULL)
	{
		return 0;
	}
	
	size_t hiBufferAvailable = 0;
	size_t loBufferAvailable = 0;
	
	const uint8_t *__restrict__ inputData = this->_buffer;
	size_t inputDataReadPos = this->_readPosition;
	const size_t inputDataWritePos = this->_writePosition;
	const size_t inputDataSize = this->_bufferSize;
	
	// Check buffer availability
	if (inputDataReadPos < inputDataWritePos)
	{
		hiBufferAvailable = inputDataWritePos - inputDataReadPos - 1;
	}
	else if (inputDataReadPos > inputDataWritePos)
	{
		hiBufferAvailable = inputDataSize - inputDataReadPos - 1;
		loBufferAvailable = inputDataWritePos;
	}
	
	// Bounds check for buffer overrun
	if (requestedNumberBytes > hiBufferAvailable + loBufferAvailable)
	{
		requestedNumberBytes = hiBufferAvailable + loBufferAvailable;
		requestedNumberBytes -= requestedNumberBytes % this->_elementSize;
	}
	
	// Copy ring buffer to destination buffer
	if (requestedNumberBytes <= hiBufferAvailable)
	{
		memcpy(destBuffer, inputData + inputDataReadPos + 1, requestedNumberBytes);
	}
	else
	{
		memcpy(destBuffer, inputData + inputDataReadPos + 1, hiBufferAvailable);
		memcpy((uint8_t *)destBuffer + hiBufferAvailable, inputData, requestedNumberBytes - hiBufferAvailable);
	}
	
	// Advance the read position
	inputDataReadPos += requestedNumberBytes;
	if (inputDataReadPos >= inputDataSize)
	{
		inputDataReadPos -= inputDataSize;
	}
	
	this->_readPosition = inputDataReadPos;
	
	// Decrease the fill size now that we're done reading.
	OSAtomicAdd32Barrier(-(int32_t)requestedNumberBytes, &this->_bufferFillSize);
	
	return requestedNumberBytes;
}

size_t RingBuffer::write(const void *__restrict__ srcBuffer, size_t requestedNumberBytes)
{
	if (srcBuffer == NULL)
	{
		return 0;
	}
	
	size_t hiBufferAvailable = 0;
	size_t loBufferAvailable = 0;
	
	uint8_t *__restrict__ inputData = this->_buffer;
	const size_t inputDataReadPos = this->_readPosition;
	size_t inputDataWritePos = this->_writePosition;
	const size_t inputDataSize = this->_bufferSize;
	
	// Check buffer availability.
	if (inputDataWritePos >= inputDataReadPos)
	{
		hiBufferAvailable = inputDataSize - inputDataWritePos;
		loBufferAvailable = inputDataReadPos;
		
		// Subtract one element's worth of bytes
		if (loBufferAvailable > 0)
		{
			loBufferAvailable -= 1;
		}
		else
		{
			hiBufferAvailable -= 1;
		}
	}
	else // (inputDataWritePos < inputDataReadPos)
	{
		hiBufferAvailable = inputDataReadPos - inputDataWritePos - 1;
	}
	
	// Bounds check for buffer overrun
	if (requestedNumberBytes > hiBufferAvailable + loBufferAvailable)
	{
		requestedNumberBytes = hiBufferAvailable + loBufferAvailable;
		requestedNumberBytes -= requestedNumberBytes % this->_elementSize;
	}
	
	// Increase the fill size before writing anything.
	OSAtomicAdd32Barrier((int32_t)requestedNumberBytes, &this->_bufferFillSize);
	
	// Copy source buffer to ring buffer.
	if (requestedNumberBytes <= hiBufferAvailable)
	{
		memcpy(inputData + inputDataWritePos, srcBuffer, requestedNumberBytes);
	}
	else
	{
		memcpy(inputData + inputDataWritePos, srcBuffer, hiBufferAvailable);
		memcpy(inputData, (uint8_t *)srcBuffer + hiBufferAvailable, requestedNumberBytes - hiBufferAvailable);
		
	}
	
	// Advance the write position.
	inputDataWritePos += requestedNumberBytes;
	if (inputDataWritePos >= inputDataSize)
	{
		inputDataWritePos -= inputDataSize;
	}
	
	this->_writePosition = inputDataWritePos;
		
	return requestedNumberBytes;
}

size_t RingBuffer::getBufferFillSize() const
{
	return (size_t)this->_bufferFillSize;
}

size_t RingBuffer::getAvailableElements() const
{
	return ((this->_bufferSize - this->_bufferFillSize) / this->_elementSize) - 1;
}

size_t RingBuffer::getElementSize() const
{
	return this->_elementSize;
}
