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

#ifndef DESMUME_QT_DS_INPUT_H
#define DESMUME_QT_DS_INPUT_H

namespace desmume {
namespace qt {
namespace ds {

enum KeysEnum {
	KEY_A = 0,
	KEY_B,
	KEY_SELECT,
	KEY_START,
	KEY_RIGHT,
	KEY_LEFT,
	KEY_UP,
	KEY_DOWN,
	KEY_R,
	KEY_L,
	KEY_X,
	KEY_Y,
	KEY_DEBUG,
	KEY_LID,
	KEY_COUNT
};

class Input {
	bool mKeyStates[KEY_COUNT];
public:
	Input();
	void updateDSInputRaw();
	void updateDSInputActual();
	void keyPress(KeysEnum key);
	void keyRelease(KeysEnum key);
	void touchMove(int x, int y);
	void touchUp();
};

} /* namespace ds */
} /* namespace qt */
} /* namespace desmume */

#endif /* DESMUME_QT_DS_INPUT_H */
