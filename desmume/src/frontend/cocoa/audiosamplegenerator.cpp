/*
	Copyright (C) 2013-2017 DeSmuME team

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
#include <string.h>
#include "ClientInputHandler.h"

#define NUM_INTERNAL_NOISE_SAMPLES 32

static const uint8_t noiseSample[NUM_INTERNAL_NOISE_SAMPLES] =
{
	0xFC, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF5, 0xFF, 0xFF, 0xFF, 0xFF, 0x8E, 0xFF,
	0xF4, 0xE1, 0xBF, 0x9A, 0x71, 0x58, 0x5B, 0x5F, 0x62, 0xC2, 0x25, 0x05, 0x01, 0x01, 0x01, 0x01
};

size_t AudioGenerator::resetSamples()
{
	// Do nothing. This is implementation-dependent.
	//
	// The return value represents the number of samples that were dropped.
	// By default, return 0 to signify that no samples are dropped. In other
	// words, all samples will continue to exist after the reset.
	return 0;
}

uint8_t AudioGenerator::generateSample()
{
	return MIC_NULL_SAMPLE_VALUE;
}

size_t AudioGenerator::generateSampleBlock(size_t sampleCount, uint8_t *outBuffer)
{
	if (outBuffer == NULL)
	{
		return 0;
	}
	
	for (uint8_t *i = outBuffer; i < outBuffer + sampleCount; i++)
	{
		*i = this->generateSample();
	}
	
	return sampleCount;
}

AudioSampleBlockGenerator::AudioSampleBlockGenerator(const uint8_t *audioBuffer, const size_t sampleCount)
{
	_buffer = (uint8_t *)malloc(sampleCount * sizeof(uint8_t));
	_sampleCount = sampleCount;
	_samplePosition = 0;
	memcpy(_buffer, audioBuffer, _sampleCount * sizeof(uint8_t));
}

AudioSampleBlockGenerator::~AudioSampleBlockGenerator()
{
	free(this->_buffer);
	this->_buffer = NULL;
}

uint8_t* AudioSampleBlockGenerator::allocate(const size_t sampleCount)
{
	if (this->_buffer != NULL)
	{
		free(this->_buffer);
	}
	
	this->_buffer = (uint8_t *)malloc(sampleCount * sizeof(uint8_t));
	this->_sampleCount = sampleCount;
	this->_samplePosition = 0;
	memset(this->_buffer, MIC_NULL_SAMPLE_VALUE, this->_sampleCount * sizeof(uint8_t));
	
	return this->_buffer;
}

uint8_t AudioSampleBlockGenerator::generateSample()
{
	if (this->_samplePosition >= this->_sampleCount)
	{
		this->_samplePosition = 0;
	}
	
	return this->_buffer[_samplePosition++];
}

uint8_t* AudioSampleBlockGenerator::getBuffer() const
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
	_buffer = (uint8_t *)malloc(NUM_INTERNAL_NOISE_SAMPLES * sizeof(uint8_t));
	_sampleCount = NUM_INTERNAL_NOISE_SAMPLES;
	_samplePosition = 0;
	memcpy(_buffer, noiseSample, _sampleCount * sizeof(uint8_t));
	
	for (size_t i = 0; i < NUM_INTERNAL_NOISE_SAMPLES; i++)
	{
		_buffer[i] >>= 1;
	}
}

uint8_t WhiteNoiseGenerator::generateSample()
{
#ifdef __APPLE__
	#ifdef MAC_OS_X_VERSION_10_7
	if (IsOSXVersionSupported(10, 7, 0))
	{
		return (uint8_t)(arc4random_uniform(0x00000080) & 0x7F);
	}
	return (uint8_t)((arc4random() % 0x00000080) & 0x7F);
	#else
	return (uint8_t)((arc4random() % 0x00000080) & 0x7F);
	#endif
#else
	return (uint8_t)(rand() & 0x7F);
#endif
}

SineWaveGenerator::SineWaveGenerator()
{
	_frequency = 250.0;
	_sampleRate = MIC_SAMPLE_RATE;
	_cyclePosition = 0.0;
};

SineWaveGenerator::SineWaveGenerator(const double freq, const double sampleRate)
{
	_frequency = freq;
	_sampleRate = sampleRate;
	_cyclePosition = 0.0;
}

uint8_t SineWaveGenerator::generateSample()
{
	const uint8_t sampleValue = (uint8_t)(63.0 * sin(2.0 * M_PI * this->_cyclePosition)) + MIC_NULL_SAMPLE_VALUE;
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
