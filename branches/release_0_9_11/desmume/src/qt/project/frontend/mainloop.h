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

#ifndef DESMUME_QT_MAINLOOP_H
#define DESMUME_QT_MAINLOOP_H

#include <QObject>
#include <QBasicTimer>
#include <QElapsedTimer>
#include <QSize>

namespace desmume {
namespace qt {

class MainLoop : public QObject {
	Q_OBJECT
	QBasicTimer* mBasicTimer;
	QElapsedTimer* mTime;
	bool mLoopInitialized;
	int mFrameMod3;
	qint64 mNextFrameTimeNs;
	int mFpsCounter;
	int mFps;
	qint64 mNextFpsCountTime;
	void loop();
	void countFps(qint64 thisFrameTime);
protected:
	void timerEvent(QTimerEvent* event);
public:
	explicit MainLoop(QObject* parent = 0);
	void kickStart();
	int fps();

signals:
	void screenRedrawRequested(bool immediate);
	void fpsUpdated(int fps);

private slots:

};

} /* namespace qt */
} /* namespace desmume */

#endif /* DESMUME_QT_MAINLOOP_H */
