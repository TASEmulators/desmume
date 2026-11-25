/*
	path_settings_core.h - category table shared by the Path Settings dialogs

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

#ifndef DESMUME_PATH_SETTINGS_CORE_H_
#define DESMUME_PATH_SETTINGS_CORE_H_

#include <cstdlib>
#include <string>

#include "../../path.h"

namespace desmume {
namespace pathsettings {

/* One editable row of the Path Settings dialog.
 *
 * The order is the one the Windows dialog uses.  iniKey is the key that
 * WritePathSettings() writes into [PathSettings], so GTK uses the same names
 * and a desmume.ini keeps its meaning if it is copied between platforms.
 * The Windows frontend still owns its own dialog and does not read this
 * table; it exists for the ports that had no path settings at all. */
struct CategoryInfo
{
	PathInfo::KnownPath category;
	const char *iniKey;
	const char *label;
	const char *description;
};

inline const CategoryInfo *Categories()
{
	static const CategoryInfo table[] =
	{
		{ PathInfo::ROMS,               "Roms",           "ROM files",         "Folder the Open ROM dialog starts in." },
		{ PathInfo::BATTERY,            "Battery",        "Battery saves",      "Folder for .dsv battery save data." },
		{ PathInfo::SRAM_IMPORT_EXPORT, "SramImportExport", "SRAM import/export", "Folder for .sav, .duc and .dss backup files." },
		{ PathInfo::STATES,             "States",         "Save states",        "Folder for Save State To / Load State From." },
		{ PathInfo::STATE_SLOTS,        "StateSlots",     "Save state slots",   "Folder for the ten quick save state slots." },
		{ PathInfo::SCREENSHOTS,        "Screenshots",    "Screenshots",        "Folder for screenshots." },
		{ PathInfo::AVI_FILES,          "AviFiles",       "Video recordings",   "Folder for recorded video." },
		{ PathInfo::MOVIES,             "Movies",         "Movies",             "Folder for .dsm playback and recording." },
		{ PathInfo::CHEATS,             "Cheats",         "Cheat files",        "Folder for .dct cheat files." },
		{ PathInfo::SOUNDS,             "SoundSamples",   "Sound samples",      "Folder for microphone sample files." },
		{ PathInfo::FIRMWARE,           "Firmware",       "Firmware",           "Folder for firmware.bin." },
		{ PathInfo::LUA,                "Lua",            "Lua scripts",        "Folder the Lua script picker starts in." },
		{ PathInfo::SLOT1D,             "Slot1D",         "Slot 1 debug",       "Folder for Slot 1 debug files." },
	};

	return table;
}

inline int CategoryCount()
{
	// Keep in sync with the table in Categories().
	return 13;
}

inline const CategoryInfo *FindCategory(PathInfo::KnownPath category)
{
	const CategoryInfo *table = Categories();

	for (int i = 0; i < CategoryCount(); i++)
	{
		if (table[i].category == category)
		{
			return &table[i];
		}
	}

	return NULL;
}

/* Trims surrounding whitespace, expands a leading "~" and drops trailing
 * directory separators.  An empty (or whitespace-only) input stays empty,
 * which both hosts read as "use the built-in default". */
inline std::string NormalizeDirectory(const std::string &input)
{
	static const char *kWhitespace = " \t\r\n";

	const size_t first = input.find_first_not_of(kWhitespace);
	if (first == std::string::npos)
	{
		return std::string();
	}

	const size_t last = input.find_last_not_of(kWhitespace);
	std::string value = input.substr(first, last - first + 1);

	if ((value == "~") || (value.compare(0, 2, "~/") == 0))
	{
#if defined(HOST_WINDOWS)
		const char *home = getenv("USERPROFILE");
#else
		const char *home = getenv("HOME");
#endif
		if (home != NULL)
		{
			value = std::string(home) + value.substr(1);
		}
	}

	while ((value.size() > 1) &&
	       ((value[value.size() - 1] == '/') || (value[value.size() - 1] == '\\')))
	{
		value.erase(value.size() - 1);
	}

	return value;
}

} /* namespace pathsettings */
} /* namespace desmume */

#endif /* DESMUME_PATH_SETTINGS_CORE_H_ */
