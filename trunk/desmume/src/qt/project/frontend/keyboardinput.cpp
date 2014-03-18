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

#include <Qt>

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

bool KeyboardInput::keyPress(int key) {
	for (int i = 0; i < ds::KEY_COUNT; i++) {
		if (mKeyAssignment[i] == key) {
			ds::input.keyPress((ds::KeysEnum)i);
			return true;
		}
	}
	return false;
}

bool KeyboardInput::keyRelease(int key) {
	for (int i = 0; i < ds::KEY_COUNT; i++) {
		if (mKeyAssignment[i] == key) {
			ds::input.keyRelease((ds::KeysEnum)i);
			return true;
		}
	}
	return false;
}

} /* namespace qt */
} /* namespace desmume */
