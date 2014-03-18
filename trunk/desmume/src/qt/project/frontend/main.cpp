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

#include "mainloop.h"
#include "ds.h"

#include "ui/mainwindow.h"
#include <QApplication>
#include <QTimer>

int main(int argc, char *argv[]) {
	QApplication a(argc, argv);
	QIcon icon;
	icon.addFile(QStringLiteral(":/images/DeSmuME-icon-16"));
	icon.addFile(QStringLiteral(":/images/DeSmuME-icon-32"));
	icon.addFile(QStringLiteral(":/images/DeSmuME-icon"));
	a.setWindowIcon(icon);

	desmume::qt::MainWindow w;
	w.show();

	desmume::qt::ds::init();

	desmume::qt::MainLoop mainLoop;
	mainLoop.kickStart();

	QObject::connect(desmume::qt::ds::video, &desmume::qt::Video::screenBufferUpdated, &w, &desmume::qt::MainWindow::screenBufferUpdate);
	QObject::connect(&mainLoop, &desmume::qt::MainLoop::screenRedrawRequested, &w, &desmume::qt::MainWindow::screenRedraw);

	return a.exec();
}
