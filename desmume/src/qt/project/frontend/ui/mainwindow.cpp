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
#include "keyboardinput.h"
#include "version.h"

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "controlconfigdialog.h"
#include <QImage>
#include <QPainter>
#include <QKeyEvent>
#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>

namespace desmume {
namespace qt {

MainWindow::MainWindow(QWidget *parent)
		: QMainWindow(parent)
		, ui(new Ui::MainWindow)
{
	ui->setupUi(this);
	this->populateVideoFilterMenu();
	this->setWindowTitle(EMU_DESMUME_NAME_AND_VERSION());
	ui->statusBar->showMessage(EMU_DESMUME_NAME_AND_VERSION());
	this->ui->screen->setFocus();
}

MainWindow::~MainWindow() {
	delete ui;
}

void MainWindow::populateVideoFilterMenu() {
	videoFilterActionGroup = new QActionGroup(this);
	for (int i = 0; i < VideoFilterTypeIDCount; i++) {
		const VideoFilterAttributes& filter = VideoFilterAttributesList[i];
		QAction *act = new QAction(this);
		act->setObjectName(QStringLiteral("actionVideoFilter") + filter.typeString);
		act->setCheckable(true);
		act->setData(i);
		if (i == VideoFilterTypeID_None) {
			act->setText("&None");
			act->setChecked(true);
		} else {
			act->setText(filter.typeString);
		}
		videoFilterActionGroup->addAction(act);
		ui->menuVideoFilter->addAction(act);
	}
	connect(videoFilterActionGroup, SIGNAL(triggered(QAction*)), this, SLOT(videoFilterActionGroup_triggered(QAction*)));
}

void MainWindow::videoFilterActionGroup_triggered(QAction *action) {
	VideoFilterTypeID filterID = (VideoFilterTypeID)action->data().toInt();
	ds::video->setFilter(filterID);
}

void MainWindow::screenBufferUpdate(unsigned int *buf, const QSize &size, qreal scale) {
	ui->screen->setScreenBuffer(buf, size, scale);
}

void MainWindow::screenRedraw(bool immediate) {
	if (immediate) {
		ui->screen->repaint();
	} else {
		ui->screen->update();
	}
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
	if (!keyboard.keyPress(event->key())) {
		QMainWindow::keyPressEvent(event);
	}
}

void MainWindow::keyReleaseEvent(QKeyEvent *event) {
	if (!keyboard.keyRelease(event->key())) {
		QMainWindow::keyPressEvent(event);
	}
}

void MainWindow::on_actionOpenROM_triggered() {
	QString fileName = QFileDialog::getOpenFileName(this, "Open NDS ROM", "", "NDS ROM Files (*.nds)");
	if (fileName.length() > 0) {
		if (ds::loadROM(fileName.toLocal8Bit())) {
			ui->statusBar->showMessage(QStringLiteral("Loaded ROM ") + fileName);
			ui->actionPause->setEnabled(true);
		} else {
			ui->actionPause->setEnabled(false);
			QMessageBox::warning(this, QStringLiteral("DeSmuME"), QStringLiteral("Cannot open ROM ") + fileName);
		}
	}
}

void MainWindow::on_actionPause_toggled(bool checked) {
	if (checked) {
		ds::pause();
	} else {
		ds::unpause();
	}
}

void MainWindow::on_actionConfigureControls_triggered() {
	ControlConfigDialog dlg(this);
	dlg.exec();
}

void MainWindow::on_actionAboutQt_triggered() {
	QMessageBox::aboutQt(this);
}

void desmume::qt::MainWindow::on_action_AboutDeSmuME_triggered() {
	// TODO: Make a better About dialog
	QMessageBox::about(
		this,
		"About DeSmuME",
		QStringLiteral("Version: \n\t") +
			EMU_DESMUME_NAME_AND_VERSION() + "\n"
			"\n"
			"This file is free software: you can redistribute it and/or modify\n"
			"it under the terms of the GNU General Public License as published by\n"
			"the Free Software Foundation, either version 2 of the License, or\n"
			"(at your option) any later version.\n"
			"\n"
			"This file is distributed in the hope that it will be useful,\n"
			"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
			"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
			"GNU General Public License for more details.\n"
			"\n"
			"You should have received a copy of the GNU General Public License\n"
			"along with the this software.  If not, see <http://www.gnu.org/licenses/>."
	);
}

void MainWindow::on_actionQuit_triggered() {
	this->close();
}

} /* namespace qt */
} /* namespace desmume */
