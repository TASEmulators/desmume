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
#include "ds_input.h"

#include "NDSSystem.h"

namespace desmume {
namespace qt {
namespace ds {

Input::Input() {
	for (int i = 0; i < KEY_COUNT; i++) {
		mKeyStates[i] = false;
	}
}

void Input::updateDSInputRaw() {
	::NDS_setPad(
		mKeyStates[KEY_RIGHT],
		mKeyStates[KEY_LEFT],
		mKeyStates[KEY_DOWN],
		mKeyStates[KEY_UP],
		mKeyStates[KEY_SELECT],
		mKeyStates[KEY_START],
		mKeyStates[KEY_B],
		mKeyStates[KEY_A],
		mKeyStates[KEY_Y],
		mKeyStates[KEY_X],
		mKeyStates[KEY_L],
		mKeyStates[KEY_R],
		mKeyStates[KEY_DEBUG],
		mKeyStates[KEY_LID]
	);
	//RunAntipodalRestriction(::NDS_getRawUserInput().buttons);
}

void Input::updateDSInputActual() {
	::NDS_beginProcessingInput();
	{
		//UserButtons& input = NDS_getProcessingUserInput().buttons;
		//ApplyAntipodalRestriction(input);
	}
	::NDS_endProcessingInput();
}

void Input::keyPress(KeysEnum key) {
	mKeyStates[key] = true;
	updateDSInputRaw();
}

void Input::keyRelease(KeysEnum key) {
	mKeyStates[key] = false;
	updateDSInputRaw();
}

void Input::touchMove(int x, int y) {
	::NDS_setTouchPos(x, y);
}

void Input::touchUp() {
	::NDS_releaseTouch();
}

} /* namespace ds */
} /* namespace qt */
} /* namespace desmume */
