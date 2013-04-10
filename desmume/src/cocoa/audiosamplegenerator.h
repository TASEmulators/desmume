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

#ifndef _AUDIO_SAMPLE_GENERATOR_
#define _AUDIO_SAMPLE_GENERATOR_

#include <stdio.h>
#include "types.h"

#define MIC_NULL_SAMPLE_VALUE 64


class AudioGenerator
{
public:
	AudioGenerator() {};
	virtual ~AudioGenerator() {};
	
	virtual size_t generateSampleBlock(size_t sampleCount, u8 *outBuffer)
	{
		if (outBuffer == NULL)
		{
			return 0;
		}
		
		for (u8 *i = outBuffer; i < outBuffer + sampleCount; i++)
		{
			*i = this->generateSample();
		}
		
		return sampleCount;
	}
	
	virtual u8 generateSample()
	{
		return MIC_NULL_SAMPLE_VALUE;
	}
};

class NullGenerator : public AudioGenerator {};

class AudioSampleBlockGenerator : public AudioGenerator
{
protected:
	u8 *_buffer;
	size_t _sampleCount;
	size_t _samplePosition;
	
public:
	AudioSampleBlockGenerator()
		: _buffer(NULL)
		, _sampleCount(0)
		, _samplePosition(0)
	{};
	AudioSampleBlockGenerator(const u8 *audioBuffer, const size_t sampleCount);
	~AudioSampleBlockGenerator();
	
	u8* allocate(const size_t sampleCount);
	u8* getBuffer() const;
	size_t getSampleCount() const;
	size_t getSamplePosition() const;
	void setSamplePosition(size_t thePosition);
	
	virtual u8 generateSample();
};

class InternalNoiseGenerator : public AudioSampleBlockGenerator
{
public:
	InternalNoiseGenerator();
};

class WhiteNoiseGenerator : public AudioGenerator
{
public:
	virtual u8 generateSample();
};

class SineWaveGenerator : public AudioGenerator
{
protected:
	double _frequency;
	double _sampleRate;
	double _cyclePosition;
	
public:
	SineWaveGenerator()
		: _frequency(250.0)
		, _sampleRate(44100.0)
		, _cyclePosition(0.0)
	{};
	SineWaveGenerator(const double freq, const double sampleRate);
	
	double getFrequency() const;
	void setFrequency(double freq);
	double getSampleRate() const;
	void setSampleRate(double sampleRate);
	double getCyclePosition() const;
	void setCyclePosition(double thePosition);
	
	virtual u8 generateSample();
};

#endif // _AUDIO_SAMPLE_GENERATOR_
