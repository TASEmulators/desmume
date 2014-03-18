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

#include "screenwidget.h"
#include <QPainter>
#include <QPen>
#include <QImage>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QTransform>
#include <QDebug>

namespace desmume {
namespace qt {

ScreenWidget::ScreenWidget(QWidget* parent)
	: QWidget(parent)
	, mScreenBuf(NULL)
{
}

void ScreenWidget::calcTransform() {
	if (mScreenBuf != NULL) {
		QTransform transform;
		int daW = this->width(), daH = this->height();
		int imgW, imgH;
		switch (/* layout */ 0) {
		case /* vertical */ 0:
			imgW = mScreenSize.width();
			imgH = mScreenSize.height() /* + gapSize * mScale */;
			break;
		case /* horizontal */ 1:
			imgW = mScreenSize.width() * 2;
			imgH = mScreenSize.height() / 2;
			break;
		case /* singleScreen */ 2:
			imgW = mScreenSize.width();
			imgH = mScreenSize.height() / 2;
			break;
		}
		if (/* rotateHorizontal */ false) {
			qSwap(imgW, imgH);
		}
		qreal hRatio = (qreal)daW / (qreal)imgW;
		qreal vRatio = (qreal)daH / (qreal)imgH;
		if (/* keepAspectRatio */ true) {
			hRatio = qMin(hRatio, vRatio);
			vRatio = hRatio;
		}

		// Scale to fit drawing area
		transform.translate(daW / 2, daH / 2);
		transform.scale(hRatio, vRatio);
		transform.rotate(/* rotationAngle */ 0.0);
		transform.translate(-mScreenSize.width() / 2, -mScreenSize.height() / 2);

		// Transform each screen
		if (/* hasMainScreen */ true) {
			mMainScreenTransform = transform;
			if (/* swapped */ false) {
				switch (/* layout */ 0) {
				case /* vertical */ 0:
					mMainScreenTransform.translate(0, mScreenSize.height() / 2 /* + gapSize * mScale */);
					break;
				case /* horizontal */ 1:
					mMainScreenTransform.translate(mScreenSize.width(), 0);
					break;
				case /* singleScreen */ 2:
					break;
				}
			}
			mMainScreenTransformInv = mMainScreenTransform;
			mMainScreenTransformInv.scale(mScale, mScale);
			mMainScreenTransformInv = mMainScreenTransformInv.inverted();
		}
		if (/* hasTouchScreen */ true) {
			mTouchScreenTransform = transform;
			if (/* !swapped */ true) {
				switch (/* layout */ 0) {
				case /* vertical */ 0:
					mTouchScreenTransform.translate(0, mScreenSize.height() / 2 /* + gapSize * mScale */);
					break;
				case /* horizontal */ 1:
					mTouchScreenTransform.translate(mScreenSize.width(), 0);
					break;
				case /* singleScreen */ 2:
					break;
				}
			}
			mTouchScreenTransformInv = mTouchScreenTransform;
			mTouchScreenTransformInv.scale(mScale, mScale);
			mTouchScreenTransformInv = mTouchScreenTransformInv.inverted();
		}
	}
}

void ScreenWidget::resizeEvent(QResizeEvent* event) {
	calcTransform();
}

void ScreenWidget::mousePressEvent(QMouseEvent* event) {
	this->setFocus();
	mAcceptTouchDS = false;
	if (ds::isRunning() && event->button() == Qt::LeftButton) {
		const QPoint touchScreenPt = mTouchScreenTransformInv.map(event->pos());
		const QRect r(0, 0, 255, 191);
		if (r.contains(touchScreenPt)) {
			mAcceptTouchDS = true;
			ds::input.touchMove(touchScreenPt.x(), touchScreenPt.y());
		}
	}
}

void ScreenWidget::mouseMoveEvent(QMouseEvent* event) {
	if (ds::isRunning() && event->buttons() == Qt::LeftButton && mAcceptTouchDS) {
		const QPoint touchScreenPt = mTouchScreenTransformInv.map(event->pos());
		ds::input.touchMove(touchScreenPt.x(), touchScreenPt.y());
	}
}

void ScreenWidget::mouseReleaseEvent(QMouseEvent* event) {
	if (mAcceptTouchDS) {
		ds::input.touchUp();
	}
	mAcceptTouchDS = false;
}

void ScreenWidget::setScreenBuffer(unsigned int* buf, const QSize& size, qreal scale) {
	this->mScreenBuf = buf;
	this->mScreenSize = size;
	this->mScale = scale;
	calcTransform();
}

void ScreenWidget::paintEvent(QPaintEvent* event) {
	QPainter painter(this);
	// Fill background with magenta for debugging purpose
	painter.fillRect(0, 0, this->width(), this->height(), QColor::fromRgb(255, 0, 255));
	if (this->mScreenBuf != NULL) {
		QImage img(
			(unsigned char*)this->mScreenBuf,
			this->mScreenSize.width(),
			this->mScreenSize.height(),
			QImage::Format_RGBX8888
		);
		painter.setTransform(mMainScreenTransform);
		painter.drawImage(0, 0, img, 0, 0, -1, mScreenSize.height() / 2);
		painter.setTransform(mTouchScreenTransform);
		painter.drawImage(0, 0, img, 0, mScreenSize.height() / 2);
	}
}

} /* namespace qt */
} /* namespace desmume */
