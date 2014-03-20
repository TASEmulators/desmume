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

#include <cstring>

#include <Qt>
#include <QKeySequence>

namespace desmume {
namespace qt {

KeyboardInput keyboard;

KeyboardInput::KeyboardInput() {
	mKeyAssignment[ds::KEY_A] = Qt::Key_X;
	mKeyAssignment[ds::KEY_B] = Qt::Key_Z;
	mKeyAssignment[ds::KEY_SELECT] = Qt::Key_Shift;
	mKeyAssignment[ds::KEY_START] = Qt::Key_Enter;
	mKeyAssignment[ds::KEY_RIGHT] = Qt::Key_Right;
	mKeyAssignment[ds::KEY_LEFT] = Qt::Key_Left;
	mKeyAssignment[ds::KEY_UP] = Qt::Key_Up;
	mKeyAssignment[ds::KEY_DOWN] = Qt::Key_Down;
	mKeyAssignment[ds::KEY_R] = Qt::Key_W;
	mKeyAssignment[ds::KEY_L] = Qt::Key_Q;
	mKeyAssignment[ds::KEY_X] = Qt::Key_S;
	mKeyAssignment[ds::KEY_Y] = Qt::Key_A;
	mKeyAssignment[ds::KEY_DEBUG] = 0;
	mKeyAssignment[ds::KEY_LID] = Qt::Key_Backspace;
}

int KeyboardInput::getAssignedKey(ds::KeysEnum key) const {
	if (key >= 0 && key < ds::KEY_COUNT) {
		return mKeyAssignment[key];
	} else {
		return 0;
	}
}

bool KeyboardInput::isKeyAssigned(int keyboardKey) const {
	for (int i = 0; i < ds::KEY_COUNT; i++) {
		if (mKeyAssignment[i] == keyboardKey) {
			return true;
		}
	}
	return false;
}

void KeyboardInput::setAssignedKey(ds::KeysEnum key, int keyboardKey) {
	if (key >= 0 && key < ds::KEY_COUNT) {
		mKeyAssignment[key] = keyboardKey;
	}
}

bool KeyboardInput::keyPress(int keyboardKey) {
	for (int i = 0; i < ds::KEY_COUNT; i++) {
		if (mKeyAssignment[i] == keyboardKey) {
			ds::input.keyPress((ds::KeysEnum)i);
			return true;
		}
	}
	return false;
}

bool KeyboardInput::keyRelease(int keyboardKey) {
	for (int i = 0; i < ds::KEY_COUNT; i++) {
		if (mKeyAssignment[i] == keyboardKey) {
			ds::input.keyRelease((ds::KeysEnum)i);
			return true;
		}
	}
	return false;
}

void KeyboardInput::from(const KeyboardInput& other){
	::memcpy(mKeyAssignment, other.mKeyAssignment, ds::KEY_COUNT * sizeof(mKeyAssignment[0]));
}

QString KeyboardInput::getKeyDisplayText(int key) {
	Qt::Key k = (Qt::Key)key;
	QString str;
	switch (k) {
	case Qt::Key_Shift:
		str = QKeySequence(Qt::ShiftModifier).toString();
		if (str.endsWith('+')) {
			str.chop(1);
		}
		break;
	case Qt::Key_Control:
		str = QKeySequence(Qt::ControlModifier).toString();
		if (str.endsWith('+')) {
			str.chop(1);
		}
		break;
	case Qt::Key_Meta:
		str = QKeySequence(Qt::MetaModifier).toString();
		if (str.endsWith('+')) {
			str.chop(1);
		}
		break;
	case Qt::Key_Alt:
		str = QKeySequence(Qt::AltModifier).toString();
		if (str.endsWith('+')) {
			str.chop(1);
		}
		break;
	default:
		str = QKeySequence(k).toString();
	}
	return str;
}

} /* namespace qt */
} /* namespace desmume */
