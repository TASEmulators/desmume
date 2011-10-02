/* mic.cpp - this file is part of DeSmuME
 *
 * Copyright (C) 2009-2011 DeSmuME Team
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef WIN32

#include <stdlib.h>
#include "mic.h"
#include "NDSSystem.h"
#include "readwrite.h"

#define MIC_NULL_SAMPLE_VALUE 0
#define MIC_MAX_BUFFER_SAMPLES 320
#define MIC_BUFFER_SIZE (sizeof(u8) * MIC_MAX_BUFFER_SAMPLES)
#define NUM_INTERNAL_NOISE_SAMPLES 32

static u8 *micSampleBuffer = NULL; // Pointer to the internal sample buffer.
static u8 *micReadPosition = NULL; // Pointer to the read position of the internal sample buffer.
static u8 *micWritePosition = NULL; // Pointer to the write position of the internal sample buffer.
static unsigned int micBufferFillCount; // The number of readable samples in the internal sample buffer.

static void Mic_BufferClear(void)
{
	if (micSampleBuffer == NULL) {
		return;
	}

	memset(micSampleBuffer, MIC_NULL_SAMPLE_VALUE, MIC_BUFFER_SIZE);
	micReadPosition = micSampleBuffer;
	micWritePosition = micSampleBuffer;
	micBufferFillCount = 0;
}

BOOL Mic_Init(void)
{
	BOOL result = FALSE;

	u8 *newBuffer = (u8 *)malloc(MIC_BUFFER_SIZE);
	if (newBuffer == NULL) {
		return result;
	}

	micSampleBuffer = newBuffer;
	Mic_BufferClear();
	result = TRUE;

	return result;
}

void Mic_DeInit(void)
{
	free(micSampleBuffer);
	micSampleBuffer = NULL;
}

void Mic_Reset(void)
{
	*micReadPosition = MIC_NULL_SAMPLE_VALUE;
	micWritePosition = micReadPosition;
	micBufferFillCount = 0;
}

static bool Mic_GetActivate(void)
{
	return NDS_getFinalUserInput().mic.micButtonPressed;
}

static bool Mic_IsBufferFull(void)
{
	return (micBufferFillCount >= MIC_MAX_BUFFER_SAMPLES);
}

static bool Mic_IsBufferEmpty(void)
{
	return (micBufferFillCount == 0);
}

static u8 Mic_DefaultBufferRead(void)
{
	u8 theSample = MIC_NULL_SAMPLE_VALUE;

	if (micSampleBuffer == NULL) {
		return theSample;
	}

	theSample = *micReadPosition;

	if (Mic_IsBufferEmpty()) {
		return theSample;
	}

	micReadPosition++;
	micBufferFillCount--;

	// Move the pointer back to start if we reach the end of the memory block.
	if (micReadPosition >= (micSampleBuffer + MIC_BUFFER_SIZE)) {
		micReadPosition = micSampleBuffer;
	}

	return theSample;
}

u8 Mic_ReadSample(void)
{
	// All mic modes other than Physical must have the mic hotkey pressed in order
	// to work.
	if (CommonSettings.micMode != TCommonSettings::Physical && !Mic_GetActivate()) {
		return MIC_NULL_SAMPLE_VALUE;
	}

	return Mic_DefaultBufferRead();
}

static void Mic_DefaultBufferWrite(u8 theSample)
{
	if (micSampleBuffer == NULL || Mic_IsBufferFull()) {
		return;
	}

	*micWritePosition = theSample;
	micWritePosition++;
	micBufferFillCount++;

	// Move the pointer back to start if we reach the end of the memory block.
	if (micWritePosition >= (micSampleBuffer + MIC_BUFFER_SIZE)) {
		micWritePosition = micSampleBuffer;
	}
}

static u8 Mic_GenerateInternalNoiseSample(void)
{
	const u8 noiseSample[NUM_INTERNAL_NOISE_SAMPLES] =
	{
		0xFC, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF5, 0xFF, 0xFF, 0xFF, 0xFF, 0x8E, 0xFF, 
		0xF4, 0xE1, 0xBF, 0x9A, 0x71, 0x58, 0x5B, 0x5F, 0x62, 0xC2, 0x25, 0x05, 0x01, 0x01, 0x01, 0x01
	};
	static unsigned int i = 0;

	if (++i >= NUM_INTERNAL_NOISE_SAMPLES) {
		i = 0;
	}

	return noiseSample[i];
}

static u8 Mic_GenerateWhiteNoiseSample(void)
{
	return (u8)(rand() & 0xFF);
}

static u8 Mic_GenerateNullSample(void)
{
	return MIC_NULL_SAMPLE_VALUE;
}

void Mic_DoNoise(BOOL noise)
{
	u8 (*generator) (void) = NULL;

	if (micSampleBuffer == NULL) {
		return;
	}

	if (!noise) {
		generator = &Mic_GenerateNullSample;
	} else if (CommonSettings.micMode == TCommonSettings::InternalNoise) {
		generator = &Mic_GenerateInternalNoiseSample;
	} else if (CommonSettings.micMode == TCommonSettings::Random) {
		generator = &Mic_GenerateWhiteNoiseSample;
	}

	if (generator == NULL) {
		return;
	}

	while (micBufferFillCount < MIC_MAX_BUFFER_SAMPLES) {
		Mic_DefaultBufferWrite(generator());
	}
}

void mic_savestate(EMUFILE* os)
{
	write32le(-1,os);
}

bool mic_loadstate(EMUFILE* is, int size)
{
	is->fseek(size, SEEK_CUR);
	return TRUE;
}

#endif
