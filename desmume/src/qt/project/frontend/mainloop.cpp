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

#include "ds.h"

#include "mainloop.h"
#include <QBasicTimer>
#include <QElapsedTimer>
#include <QDebug>

namespace desmume {
namespace qt {

MainLoop::MainLoop(QObject *parent)
	: QObject(parent)
	, mBasicTimer(new QBasicTimer)
	, mTime(new QElapsedTimer)
	, mLoopInitialized(false)
	, mFrameMod3(0)
	, mNextFrameTimeNs(0)
	, mFpsCounter(0)
	, mFps(0)
	, mNextFpsCountTime(0)
{
}

void MainLoop::kickStart() {
	mBasicTimer->start(100, this);
}

int MainLoop::fps() {
	return mFps;
}

void MainLoop::timerEvent(QTimerEvent* event) {
	loop();
}

void MainLoop::loop() {
	if (mLoopInitialized) {
		// I wanted to put some code here but...
	} else {
		mLoopInitialized = true;
		mTime->start();
		mNextFrameTimeNs = 0;
		mFrameMod3 = 0;
		mFpsCounter = 0;
		mFps = 0;
		mNextFpsCountTime = 1000;
	}

	// ---- Time keeping ----
	// TODO: Add frameskip option

	mFrameMod3 = (mFrameMod3 + 1) % 3;
	int timerInterval = 16;

	// Adjust the time drift
	qint64 timeDriftNs = mTime->nsecsElapsed() - mNextFrameTimeNs;
	if (timeDriftNs > 0) {
		//qDebug("Drift! %lld", timeDriftNs);
		if (timeDriftNs < (10 * 1000 * 1000)) {
			// If the drift is tolerable, try to catch up the next frame
			// It's fine if the drift is less than 1ms
			// Hopefully the compiler will generate mul + shr
			timerInterval -= timeDriftNs / (1000 * 1000);
		} else {
			// Otherwise don't, delay the next frame instead.
			mNextFrameTimeNs += timeDriftNs;
		}
	} else {
		while (mTime->nsecsElapsed() < mNextFrameTimeNs);
	}

	// Schedule next frame
	if (/* fpsLimiterEnabled */ true) {
		mBasicTimer->start(timerInterval, Qt::PreciseTimer, this);
		const int frameIntervalNs = (mFrameMod3 == 0) ? 16666666 : 16666667;
		mNextFrameTimeNs += frameIntervalNs;
	} else {
		mBasicTimer->start(0, this);
	}

	// Count the fps
	countFps(mTime->elapsed());

	//qDebug("Accumulative: %lld", mTime->elapsed());

	// ---- Real action ----

	// Redraw last frame
	this->screenRedrawRequested(true);

	// Update input and emulate!
	ds::input.updateDSInputActual();
	ds::execFrame();

	// Run the video filter
	ds::video->runFilter();

	// Don't redraw yet...
	//this->screenRedrawRequested(false);
}

void MainLoop::countFps(qint64 thisFrameTime) {
	mFpsCounter += 1;
	if (thisFrameTime >= mNextFpsCountTime) {
		mNextFpsCountTime = thisFrameTime + 1000;
		mFps = mFpsCounter;
		mFpsCounter = 0;
		this->fpsUpdated(mFps);
	}
}

} /* namespace qt */
} /* namespace desmume */
