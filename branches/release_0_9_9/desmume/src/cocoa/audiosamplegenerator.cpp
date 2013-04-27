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

#ifdef __APPLE__
	#include <Availability.h>
	#include "utilities.h"
#endif // __APPLE__

#include "audiosamplegenerator.h"
#include <math.h>

#define NUM_INTERNAL_NOISE_SAMPLES 32

static const u8 noiseSample[NUM_INTERNAL_NOISE_SAMPLES] =
{
	0xFC, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF5, 0xFF, 0xFF, 0xFF, 0xFF, 0x8E, 0xFF,
	0xF4, 0xE1, 0xBF, 0x9A, 0x71, 0x58, 0x5B, 0x5F, 0x62, 0xC2, 0x25, 0x05, 0x01, 0x01, 0x01, 0x01
};

AudioSampleBlockGenerator::AudioSampleBlockGenerator(const u8 *audioBuffer, const size_t sampleCount)
{
	_buffer = (u8 *)malloc(sampleCount * sizeof(u8));
	_sampleCount = sampleCount;
	_samplePosition = 0;
	memcpy(_buffer, audioBuffer, _sampleCount * sizeof(u8));
}

AudioSampleBlockGenerator::~AudioSampleBlockGenerator()
{
	free(this->_buffer);
	this->_buffer = NULL;
}

u8* AudioSampleBlockGenerator::allocate(const size_t sampleCount)
{
	if (this->_buffer != NULL)
	{
		free(this->_buffer);
	}
	
	this->_buffer = (u8 *)malloc(sampleCount * sizeof(u8));
	this->_sampleCount = sampleCount;
	this->_samplePosition = 0;
	memset(this->_buffer, MIC_NULL_SAMPLE_VALUE, this->_sampleCount * sizeof(u8));
	
	return this->_buffer;
}

u8 AudioSampleBlockGenerator::generateSample()
{
	if (this->_samplePosition >= this->_sampleCount)
	{
		this->_samplePosition = 0;
	}
	
	return this->_buffer[_samplePosition++];
}

u8* AudioSampleBlockGenerator::getBuffer() const
{
	return this->_buffer;
}

size_t AudioSampleBlockGenerator::getSampleCount() const
{
	return this->_sampleCount;
}

size_t AudioSampleBlockGenerator::getSamplePosition() const
{
	return this->_samplePosition;
}

void AudioSampleBlockGenerator::setSamplePosition(size_t thePosition)
{
	this->_samplePosition = thePosition % this->_sampleCount;
}

InternalNoiseGenerator::InternalNoiseGenerator()
{
	_buffer = (u8 *)malloc(NUM_INTERNAL_NOISE_SAMPLES * sizeof(u8));
	_sampleCount = NUM_INTERNAL_NOISE_SAMPLES;
	_samplePosition = 0;
	memcpy(_buffer, noiseSample, _sampleCount * sizeof(u8));
}

u8 WhiteNoiseGenerator::generateSample()
{
#ifdef __APPLE__
	#ifdef MAC_OS_X_VERSION_10_7
	if (IsOSXVersionSupported(10, 7, 0))
	{
		return (u8)(arc4random_uniform(0x00000080) & 0x7F);
	}
	return (u8)((arc4random() % 0x00000080) & 0x7F);
	#else
	return (u8)((arc4random() % 0x00000080) & 0x7F);
	#endif
#else
	return (u8)(rand() & 0x7F);
#endif
}

SineWaveGenerator::SineWaveGenerator(const double freq, const double sampleRate)
{
	_frequency = freq;
	_sampleRate = sampleRate;
	_cyclePosition = 0.0;
}

u8 SineWaveGenerator::generateSample()
{
	const u8 sampleValue = (u8)(63.0 * sin(2.0 * M_PI * this->_cyclePosition)) + MIC_NULL_SAMPLE_VALUE;
	this->_cyclePosition += (this->_frequency / this->_sampleRate);
	
	return sampleValue;
}

double SineWaveGenerator::getFrequency() const
{
	return this->_frequency;
}

void SineWaveGenerator::setFrequency(double freq)
{
	this->_frequency = freq;
}

double SineWaveGenerator::getSampleRate() const
{
	return this->_sampleRate;
}

void SineWaveGenerator::setSampleRate(double sampleRate)
{
	this->_sampleRate = sampleRate;
}

double SineWaveGenerator::getCyclePosition() const
{
	return this->_cyclePosition;
}

void SineWaveGenerator::setCyclePosition(double thePosition)
{
	this->_cyclePosition = thePosition;
}
