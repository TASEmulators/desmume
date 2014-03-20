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

#ifndef DESMUME_QT_CONTROLCONFIGDIALOG_H
#define DESMUME_QT_CONTROLCONFIGDIALOG_H

#include "keyboardinput.h"

#include <QDialog>
#include <QAbstractButton>

namespace desmume {
namespace qt {

namespace Ui {
class ControlConfigDialog;
} /* namespace Ui */

class ControlConfigDialog : public QDialog {
	Q_OBJECT
	KeyboardInput mKeyboard;
	void updateButtonText();
public:
	explicit ControlConfigDialog(QWidget* parent = 0);
	~ControlConfigDialog();

private slots:
	void on_buttonControlUp_clicked();
	void on_buttonControlDown_clicked();
	void on_buttonControlLeft_clicked();
	void on_buttonControlRight_clicked();
	void on_buttonControlA_clicked();
	void on_buttonControlB_clicked();
	void on_buttonControlX_clicked();
	void on_buttonControlY_clicked();
	void on_buttonControlL_clicked();
	void on_buttonControlR_clicked();
	void on_buttonControlStart_clicked();
	void on_buttonControlSelect_clicked();
	void on_buttonControlLid_clicked();
	void on_buttonControlDebug_clicked();
	void on_buttonBox_accepted();
	void on_buttonBox_rejected();
	void on_buttonBox_clicked(QAbstractButton* button);

private:
	Ui::ControlConfigDialog* ui;
};


} /* namespace qt */
} /* namespace desmume */
#endif /* DESMUME_QT_CONTROLCONFIGDIALOG_H */
