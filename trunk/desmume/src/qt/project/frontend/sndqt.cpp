/*
	Copyright (C) 2014 DeSmuME team
	Copyright (C) 2014 Alvin Wong

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

#include "SPU.h"
#include "sndqt.h"

#include <QAudioOutput>
#include <QAudioFormat>
#include <QAudioDeviceInfo>
#include <QIODevice>
#include <QDebug>

int SNDQTInit(int buffersize);
void SNDQTDeInit();
void SNDQTUpdateAudio(s16 *buffer, u32 num_samples);
u32 SNDQTGetAudioSpace();
void SNDQTMuteAudio();
void SNDQTUnMuteAudio();
void SNDQTSetVolume(int volume);

SoundInterface_struct SNDQt = {
SNDCORE_QT,
"Qt QAudioOutput Sound Interface",
SNDQTInit,
SNDQTDeInit,
SNDQTUpdateAudio,
SNDQTGetAudioSpace,
SNDQTMuteAudio,
SNDQTUnMuteAudio,
SNDQTSetVolume
};

namespace {

QAudioOutput *audio = NULL;
QIODevice *aio = NULL;

} /* namespace */

int SNDQTInit(int buffersize) {
	QAudioFormat fmt;
	fmt.setSampleRate(DESMUME_SAMPLE_RATE);
	fmt.setSampleType(QAudioFormat::SignedInt);
	fmt.setSampleSize(16);
	fmt.setChannelCount(2);
	fmt.setCodec("audio/pcm");
	fmt.setByteOrder(QAudioFormat::LittleEndian);

	QAudioDeviceInfo info(QAudioDeviceInfo::defaultOutputDevice());
	if (!info.isFormatSupported(fmt)) {
		qWarning() << "Raw audio format not supported by backend, cannot play audio.";
		return -1;
	}

	audio = new QAudioOutput(fmt);

	audio->setBufferSize(buffersize * 2 * 2);

	aio = audio->start();
	return 0;
}

void SNDQTDeInit() {
	audio->stop();
	aio = NULL;
	delete audio;
	audio = NULL;
}

void SNDQTUpdateAudio(s16 *buffer, u32 num_samples) {
	if (aio == NULL) {
		qDebug("%s is called at the wrong time!\n", __FUNCTION__);
		return;
	}
	qint64 written = 0, writtenTotal = 0;
	const u32 count = num_samples * 2 * 2;
	do {
		written = aio->write((char *) buffer, count - writtenTotal);
	} while (written >= 0 && (writtenTotal += written) < count);
}

u32 SNDQTGetAudioSpace() {
	if (aio == NULL) {
		qDebug("%s is called at the wrong time!\n", __FUNCTION__);
		return 0;
	}
	return audio->bytesFree() / 2 / 2;
}

void SNDQTMuteAudio() {
	audio->suspend();
	//audio->setVolume(0.0);
}

void SNDQTUnMuteAudio() {
	audio->resume();
	//audio->setVolume(1.0);
}

void SNDQTSetVolume(int volume) {
	audio->setVolume(volume / 1.0);
}
