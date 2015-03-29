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

#include <glib.h>
#include <glib/gstdio.h>

#include "config.h"

using std::string;
using std::vector;

namespace desmume {
namespace config {

/* class value<bool> */

template<>
void value<bool>::load() {
	GError* err = NULL;
	bool val = g_key_file_get_boolean(this->mKeyFile, this->mSection.c_str(), this->mKey.c_str(), &err);
	if (err != NULL) {
		g_error_free(err);
	} else {
		this->mData = val;
	}
}

template<>
void value<bool>::save() {
	g_key_file_set_boolean(this->mKeyFile, this->mSection.c_str(), this->mKey.c_str(), this->mData ? TRUE : FALSE);
}

/* class value<int> */

template<>
void value<int>::load() {
	GError* err = NULL;
	int val = g_key_file_get_integer(this->mKeyFile, this->mSection.c_str(), this->mKey.c_str(), &err);
	if (err != NULL) {
		g_error_free(err);
	} else {
		this->mData = val;
	}
}

template<>
void value<int>::save() {
	g_key_file_set_integer(this->mKeyFile, this->mSection.c_str(), this->mKey.c_str(), this->mData);
}

/* class value<string> */

template<>
void value<string>::load() {
	char* val = g_key_file_get_string(this->mKeyFile, this->mSection.c_str(), this->mKey.c_str(), NULL);
	if (val != NULL) {
		this->mData = val;
		g_free(val);
	}
}

template<>
void value<string>::save() {
	g_key_file_set_string(this->mKeyFile, this->mSection.c_str(), this->mKey.c_str(), this->mData.c_str());
}

/* class Config */

Config::Config()
		: mKeyFile(g_key_file_new())
#		define OPT(name, type, default, section, key) \
			, name(default, #section, #key)
#		include "config_opts.h"
#		undef OPT
{
#	define OPT(name, type, default, section, key) \
		this->name.mKeyFile = this->mKeyFile;
#	include "config_opts.h"
#	undef OPT
}

Config::~Config() {
	g_key_file_free(this->mKeyFile);
}

void Config::load() {
	char* config_dir = g_build_filename(g_get_user_config_dir(), "desmume", NULL);
	g_mkdir_with_parents(config_dir, 0755);
	char* config_file = g_build_filename(config_dir, "config.cfg", NULL);
	g_key_file_load_from_file(this->mKeyFile, config_file, G_KEY_FILE_KEEP_COMMENTS, NULL);
	g_free(config_file);
	g_free(config_dir);
#	define OPT(name, type, default, section, key) \
		this->name.load();
#	include "config_opts.h"
#	undef OPT
}

void Config::save() {
#	define OPT(name, type, default, section, key) \
		this->name.save();
#	include "config_opts.h"
#	undef OPT
	gsize length;
	char* data = g_key_file_to_data(this->mKeyFile, &length, NULL);
	char* config_dir = g_build_filename(g_get_user_config_dir(), "desmume", NULL);
	g_mkdir_with_parents(config_dir, 0755);
	char* config_file = g_build_filename(config_dir, "config.cfg", NULL);
	g_file_set_contents(config_file, data, length, NULL);
	g_free(config_file);
	g_free(config_dir);
	g_free(data);
}

#if 0
/* class Config::Input */

Config::Input::Input()
		: keys(NB_KEYS)
		, joykeys(NB_KEYS)
{
static const u16 gtk_kb_cfg[NB_KEYS] = {
	GDK_x,         // A
	GDK_z,         // B
	GDK_Shift_R,   // select
	GDK_Return,    // start
	GDK_Right,     // Right
	GDK_Left,      // Left
	GDK_Up,        // Up
	GDK_Down,      // Down       
	GDK_w,         // R
	GDK_q,         // L
	GDK_s,         // X
	GDK_a,         // Y
	GDK_p,         // DEBUG
	GDK_o,         // BOOST
	GDK_BackSpace, // Lid
};
	for (size_t i = 0; i < NB_KEYS; i++) {
		this->keys[i] = new value<int>(gtk_kb_cfg[i], "Keys", ...);
	}
}

void Config::Input::load();

void Config::Input::save();
#endif

} /* namespace config */
} /* namespace desmume */

