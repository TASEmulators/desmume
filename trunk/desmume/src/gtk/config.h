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

#ifndef DESMUME_CONFIG_CLASS_H_
#define DESMUME_CONFIG_CLASS_H_

#include <string>
#include <vector>

#include <glib.h>

namespace desmume {
namespace config {

template<class T>
class value {
	friend class Config;

	std::string mSection;
	std::string mKey;
	GKeyFile* mKeyFile;

	T mDefaultVal;
	T mData;

	value(const value&); // DO NOT IMPLEMENT
	value operator=(const value&); // DO NOT IMPLEMENT
	void load();
	void save();
public:
	value(const T& defaultVal, const std::string& section, const std::string& key)
		: mSection(section)
		, mKey(key)
		, mDefaultVal(defaultVal)
		, mData(defaultVal) {}
	inline T defaultVal() {
		return this->mDefaultVal;
	}
	inline T get() {
		return this->mData;
	}
	inline operator T() {
		return this->mData;
	}
	inline void set(const T& value) {
		this->mData = value;
		//this->save();
	}
	inline T operator=(const T& value) {
		this->mData = value;
		return this->mData;
	}
}; /* class value<T> */

#ifdef OPT
#	undef OPT
#endif

class Config {
protected:
	GKeyFile* mKeyFile;

	Config(const Config&); // DO NOT IMPLEMENT
	Config operator=(const Config&); // DO NOT IMPLEMENT
public:
	// member fields
#	define OPT(name, type, default, section, key) \
		value<type> name;
#	include "config_opts.h"
#	undef OPT
#if 0
	// Special case: input config
	class Input {
		friend class Config;
		std::vector<value<int>*> keys;
		std::vector<value<int>*> joykeys;
		Input();
		void load();
		void save();
	} inputs;
#endif

	// constructor
	Config();

	// methods
	virtual ~Config();
	void load();
	void save();
}; /* class Config */

#undef DESMUME_CONFIG_CONFIG

} /* namespace config */
} /* namespace desmume */

#endif /* DESMUME_CONFIG_CLASS_H_ */

