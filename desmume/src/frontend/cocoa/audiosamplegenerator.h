/*
	Copyright (C) 2013-2015 DeSmuME team

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

#include <stdint.h>
#include <stdlib.h>


class AudioGenerator
{
public:
	AudioGenerator() {};
	virtual ~AudioGenerator() {};
	
	virtual size_t resetSamples();
	virtual uint8_t generateSample();
	virtual size_t generateSampleBlock(size_t sampleCount, uint8_t *outBuffer);
};

class NullGenerator : public AudioGenerator {};

class AudioSampleBlockGenerator : public AudioGenerator
{
protected:
	uint8_t *_buffer;
	size_t _sampleCount;
	size_t _samplePosition;
	
public:
	AudioSampleBlockGenerator()
		: _buffer(NULL)
		, _sampleCount(0)
		, _samplePosition(0)
	{};
	AudioSampleBlockGenerator(const uint8_t *audioBuffer, const size_t sampleCount);
	~AudioSampleBlockGenerator();
	
	uint8_t* allocate(const size_t sampleCount);
	uint8_t* getBuffer() const;
	size_t getSampleCount() const;
	size_t getSamplePosition() const;
	void setSamplePosition(size_t thePosition);
	
	virtual uint8_t generateSample();
};

class InternalNoiseGenerator : public AudioSampleBlockGenerator
{
public:
	InternalNoiseGenerator();
};

class WhiteNoiseGenerator : public AudioGenerator
{
public:
	virtual uint8_t generateSample();
};

class SineWaveGenerator : public AudioGenerator
{
protected:
	double _frequency;
	double _sampleRate;
	double _cyclePosition;
	
public:
	SineWaveGenerator();
	SineWaveGenerator(const double freq, const double sampleRate);
	
	double getFrequency() const;
	void setFrequency(double freq);
	double getSampleRate() const;
	void setSampleRate(double sampleRate);
	double getCyclePosition() const;
	void setCyclePosition(double thePosition);
	
	virtual uint8_t generateSample();
};

#endif // _AUDIO_SAMPLE_GENERATOR_
