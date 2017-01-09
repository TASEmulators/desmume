/*
	Copyright (C) 2015 DeSmuME team

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

#ifndef _OGLDISPLAYOUTPUT_3_2_H_
#define _OGLDISPLAYOUTPUT_3_2_H_

#if defined(__APPLE__)
	#include <OpenGL/gl3.h>
	#include <OpenGL/gl3ext.h>
	#include <OpenGL/OpenGL.h>
#endif

#include "OGLDisplayOutput.h"

class OGLContextInfo_3_2 : public OGLContextInfo
{
public:
	OGLContextInfo_3_2();
	
	virtual void GetExtensionSetOGL(std::set<std::string> *oglExtensionSet);
	virtual bool IsExtensionPresent(const std::set<std::string> &oglExtensionSet, const std::string &extensionName) const;
};

#endif
