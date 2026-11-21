/*
	Copyright (C) 2026 DeSmuME team

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

/*
	This file used to pull the JIT implementation in through #includes, once
	per architecture, because Universal and OpenEmu targets had no direct JIT
	source registration. That shim is retired: both targets now register the
	asmjit and backend sources directly, and each backend TU guards itself by
	host architecture. Kept as an empty translation unit so the existing build
	file entries stay valid.
*/

