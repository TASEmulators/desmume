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

#ifndef DESMUME_QT_VIDEO_H
#define DESMUME_QT_VIDEO_H

#include "filter/videofilter.h"
#include <QObject>
#include <QSize>

namespace desmume {
namespace qt {

class Video : public QObject {
	Q_OBJECT
	VideoFilter mFilter;
public:
	Video(int numThreads, QObject* parent = 0);
	bool setFilter(VideoFilterTypeID filterID);
	unsigned int* runFilter();
	unsigned int* getDstBufferPtr();
	int getDstWidth();
	int getDstHeight();
	QSize getDstSize();
	qreal getDstScale();
signals:
	void screenBufferUpdated(unsigned int *buf, const QSize &size, qreal scale);
};

} /* namespace qt */
} /* namespace desmume */

#endif /* DESMUME_QT_VIDEO_H */
