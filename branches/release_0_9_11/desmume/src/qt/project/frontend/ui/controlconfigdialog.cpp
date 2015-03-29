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

#include "controlconfigdialog.h"
#include "ui_controlconfigdialog.h"
#include "controlitemconfigdialog.h"

namespace desmume {
namespace qt {

ControlConfigDialog::ControlConfigDialog(QWidget* parent)
	: QDialog(parent)
	, ui(new Ui::ControlConfigDialog)
	, mKeyboard(keyboard)
{
	ui->setupUi(this);
	this->updateButtonText();
}

ControlConfigDialog::~ControlConfigDialog() {
	delete ui;
}

void ControlConfigDialog::updateButtonText() {
	ui->buttonControlA->setText(KeyboardInput::getKeyDisplayText(mKeyboard.getAssignedKey(ds::KEY_A)));
	ui->buttonControlB->setText(KeyboardInput::getKeyDisplayText(mKeyboard.getAssignedKey(ds::KEY_B)));
	ui->buttonControlX->setText(KeyboardInput::getKeyDisplayText(mKeyboard.getAssignedKey(ds::KEY_X)));
	ui->buttonControlY->setText(KeyboardInput::getKeyDisplayText(mKeyboard.getAssignedKey(ds::KEY_Y)));
	ui->buttonControlUp->setText(KeyboardInput::getKeyDisplayText(mKeyboard.getAssignedKey(ds::KEY_UP)));
	ui->buttonControlDown->setText(KeyboardInput::getKeyDisplayText(mKeyboard.getAssignedKey(ds::KEY_DOWN)));
	ui->buttonControlLeft->setText(KeyboardInput::getKeyDisplayText(mKeyboard.getAssignedKey(ds::KEY_LEFT)));
	ui->buttonControlRight->setText(KeyboardInput::getKeyDisplayText(mKeyboard.getAssignedKey(ds::KEY_RIGHT)));
	ui->buttonControlL->setText(KeyboardInput::getKeyDisplayText(mKeyboard.getAssignedKey(ds::KEY_L)));
	ui->buttonControlR->setText(KeyboardInput::getKeyDisplayText(mKeyboard.getAssignedKey(ds::KEY_R)));
	ui->buttonControlStart->setText(KeyboardInput::getKeyDisplayText(mKeyboard.getAssignedKey(ds::KEY_START)));
	ui->buttonControlSelect->setText(KeyboardInput::getKeyDisplayText(mKeyboard.getAssignedKey(ds::KEY_SELECT)));
	ui->buttonControlLid->setText(KeyboardInput::getKeyDisplayText(mKeyboard.getAssignedKey(ds::KEY_LID)));
	ui->buttonControlDebug->setText(KeyboardInput::getKeyDisplayText(mKeyboard.getAssignedKey(ds::KEY_DEBUG)));
}

void ControlConfigDialog::on_buttonControlUp_clicked() {
	ControlItemConfigDialog dlg;
	int res = dlg.exec();
	if (res == QDialog::Accepted) {
		int key = dlg.key();
		mKeyboard.setAssignedKey(ds::KEY_UP, key);
		ui->buttonControlUp->setText(KeyboardInput::getKeyDisplayText(mKeyboard.getAssignedKey(ds::KEY_UP)));
	}
}

void ControlConfigDialog::on_buttonControlDown_clicked() {
	ControlItemConfigDialog dlg;
	int res = dlg.exec();
	if (res == QDialog::Accepted) {
		int key = dlg.key();
		mKeyboard.setAssignedKey(ds::KEY_DOWN, key);
		ui->buttonControlDown->setText(KeyboardInput::getKeyDisplayText(mKeyboard.getAssignedKey(ds::KEY_DOWN)));
	}
}

void ControlConfigDialog::on_buttonControlLeft_clicked() {
	ControlItemConfigDialog dlg;
	int res = dlg.exec();
	if (res == QDialog::Accepted) {
		int key = dlg.key();
		mKeyboard.setAssignedKey(ds::KEY_LEFT, key);
		ui->buttonControlLeft->setText(KeyboardInput::getKeyDisplayText(mKeyboard.getAssignedKey(ds::KEY_LEFT)));
	}
}

void ControlConfigDialog::on_buttonControlRight_clicked() {
	ControlItemConfigDialog dlg;
	int res = dlg.exec();
	if (res == QDialog::Accepted) {
		int key = dlg.key();
		mKeyboard.setAssignedKey(ds::KEY_RIGHT, key);
		ui->buttonControlRight->setText(KeyboardInput::getKeyDisplayText(mKeyboard.getAssignedKey(ds::KEY_RIGHT)));
	}
}

void ControlConfigDialog::on_buttonControlA_clicked() {
	ControlItemConfigDialog dlg;
	int res = dlg.exec();
	if (res == QDialog::Accepted) {
		int key = dlg.key();
		mKeyboard.setAssignedKey(ds::KEY_A, key);
		ui->buttonControlA->setText(KeyboardInput::getKeyDisplayText(mKeyboard.getAssignedKey(ds::KEY_A)));
	}
}

void ControlConfigDialog::on_buttonControlB_clicked() {
	ControlItemConfigDialog dlg;
	int res = dlg.exec();
	if (res == QDialog::Accepted) {
		int key = dlg.key();
		mKeyboard.setAssignedKey(ds::KEY_B, key);
		ui->buttonControlB->setText(KeyboardInput::getKeyDisplayText(mKeyboard.getAssignedKey(ds::KEY_B)));
	}
}

void ControlConfigDialog::on_buttonControlX_clicked() {
	ControlItemConfigDialog dlg;
	int res = dlg.exec();
	if (res == QDialog::Accepted) {
		int key = dlg.key();
		mKeyboard.setAssignedKey(ds::KEY_X, key);
		ui->buttonControlX->setText(KeyboardInput::getKeyDisplayText(mKeyboard.getAssignedKey(ds::KEY_X)));
	}
}

void ControlConfigDialog::on_buttonControlY_clicked() {
	ControlItemConfigDialog dlg;
	int res = dlg.exec();
	if (res == QDialog::Accepted) {
		int key = dlg.key();
		mKeyboard.setAssignedKey(ds::KEY_Y, key);
		ui->buttonControlY->setText(KeyboardInput::getKeyDisplayText(mKeyboard.getAssignedKey(ds::KEY_Y)));
	}
}

void ControlConfigDialog::on_buttonControlL_clicked() {
	ControlItemConfigDialog dlg;
	int res = dlg.exec();
	if (res == QDialog::Accepted) {
		int key = dlg.key();
		mKeyboard.setAssignedKey(ds::KEY_L, key);
		ui->buttonControlL->setText(KeyboardInput::getKeyDisplayText(mKeyboard.getAssignedKey(ds::KEY_L)));
	}
}

void ControlConfigDialog::on_buttonControlR_clicked() {
	ControlItemConfigDialog dlg;
	int res = dlg.exec();
	if (res == QDialog::Accepted) {
		int key = dlg.key();
		mKeyboard.setAssignedKey(ds::KEY_R, key);
		ui->buttonControlR->setText(KeyboardInput::getKeyDisplayText(mKeyboard.getAssignedKey(ds::KEY_R)));
	}
}

void ControlConfigDialog::on_buttonControlStart_clicked() {
	ControlItemConfigDialog dlg;
	int res = dlg.exec();
	if (res == QDialog::Accepted) {
		int key = dlg.key();
		mKeyboard.setAssignedKey(ds::KEY_START, key);
		ui->buttonControlStart->setText(KeyboardInput::getKeyDisplayText(mKeyboard.getAssignedKey(ds::KEY_START)));
	}
}

void ControlConfigDialog::on_buttonControlSelect_clicked() {
	ControlItemConfigDialog dlg;
	int res = dlg.exec();
	if (res == QDialog::Accepted) {
		int key = dlg.key();
		mKeyboard.setAssignedKey(ds::KEY_SELECT, key);
		ui->buttonControlSelect->setText(KeyboardInput::getKeyDisplayText(mKeyboard.getAssignedKey(ds::KEY_SELECT)));
	}
}

void ControlConfigDialog::on_buttonControlLid_clicked() {
	ControlItemConfigDialog dlg;
	int res = dlg.exec();
	if (res == QDialog::Accepted) {
		int key = dlg.key();
		mKeyboard.setAssignedKey(ds::KEY_LID, key);
		ui->buttonControlLid->setText(KeyboardInput::getKeyDisplayText(mKeyboard.getAssignedKey(ds::KEY_LID)));
	}
}

void ControlConfigDialog::on_buttonControlDebug_clicked() {
	ControlItemConfigDialog dlg;
	int res = dlg.exec();
	if (res == QDialog::Accepted) {
		int key = dlg.key();
		mKeyboard.setAssignedKey(ds::KEY_DEBUG, key);
		ui->buttonControlDebug->setText(KeyboardInput::getKeyDisplayText(mKeyboard.getAssignedKey(ds::KEY_DEBUG)));
	}
}

void ControlConfigDialog::on_buttonBox_accepted() {
	keyboard.from(mKeyboard);
	this->accept();
}

void ControlConfigDialog::on_buttonBox_rejected() {
	this->reject();
}

void ControlConfigDialog::on_buttonBox_clicked(QAbstractButton* button) {
	switch (ui->buttonBox->buttonRole(button)) {
	case QDialogButtonBox::DestructiveRole:
		this->reject();
		break;
	case QDialogButtonBox::ResetRole:
		mKeyboard.from(KeyboardInput());
		this->updateButtonText();
		break;
	default:
		break;
	}
}

} /* namespace ui */
} /* namespace desmume */
