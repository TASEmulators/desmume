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

#ifndef DESMUME_QT_KEYBOARDINPUT_H
#define DESMUME_QT_KEYBOARDINPUT_H

#include "ds_input.h"

namespace desmume {
namespace qt {

class KeyboardInput {
	int mKeyAssignment[ds::KEY_COUNT];
public:
	KeyboardInput();
	bool keyPress(int key);
	bool keyRelease(int key);
};

extern KeyboardInput keyboard;

} /* namespace qt */
} /* namespace desmume */

#endif /* DESMUME_QT_KEYBOARDINPUT_H */
