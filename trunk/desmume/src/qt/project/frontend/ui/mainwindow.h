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

#ifndef DESMUME_QT_MAINWINDOW_H
#define DESMUME_QT_MAINWINDOW_H

#include <vector>

#include <QMainWindow>
#include <QSize>
#include <QAction>
#include <QActionGroup>

namespace desmume {
namespace qt {

namespace Ui {
class MainWindow;
} /* namespace Ui */

class MainWindow : public QMainWindow
{
	Q_OBJECT
	QActionGroup* videoFilterActionGroup;
	void populateVideoFilterMenu();
protected:
	void keyPressEvent(QKeyEvent *event);
	void keyReleaseEvent(QKeyEvent *event);
public:
	explicit MainWindow(QWidget *parent = 0);
	~MainWindow();

public slots:
	void screenBufferUpdate(unsigned int *buf, const QSize &size, qreal scale);
	void screenRedraw(bool immediate);

private slots:
	void on_actionQuit_triggered();
	void videoFilterActionGroup_triggered(QAction* action);
	void on_actionConfigureControls_triggered();
	void on_actionPause_toggled(bool checked);

	void on_actionOpenROM_triggered();

	void on_actionAboutQt_triggered();

	void on_action_AboutDeSmuME_triggered();

private:
	Ui::MainWindow *ui;
};

} /* namespace qt */
} /* namespace desmume */

#endif /* DESMUME_QT_MAINWINDOW_H */
