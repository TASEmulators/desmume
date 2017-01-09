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

#include "OGLDisplayOutput_3_2.h"

enum OGLVertexAttributeID
{
	OGLVertexAttributeID_Position = 0,
	OGLVertexAttributeID_TexCoord0 = 8
};

void glBindVertexArray_3_2(GLuint vaoID)
{
	glBindVertexArray(vaoID);
}

void glDeleteVertexArrays_3_2(GLsizei n, const GLuint *vaoIDs)
{
	glDeleteVertexArrays(n, vaoIDs);
}

void glGenVertexArrays_3_2(GLsizei n, GLuint *vaoIDs)
{
	glGenVertexArrays(n, vaoIDs);
}

OGLContextInfo_3_2::OGLContextInfo_3_2()
{
	_useShader150 = true;
	_isVBOSupported = true;
	_isVAOSupported = true;
	_isPBOSupported = true;
	_isFBOSupported = true;
	
	glBindVertexArrayDESMUME = &glBindVertexArray_3_2;
	glDeleteVertexArraysDESMUME = &glDeleteVertexArrays_3_2;
	glGenVertexArraysDESMUME = &glGenVertexArrays_3_2;
	
	_shaderSupport = ShaderSupport_MidTier;
	
	if (_versionMajor == 4)
	{
		if (_versionMinor <= 1)
		{
			_shaderSupport = ShaderSupport_HighTier;
		}
		else
		{
			_shaderSupport = ShaderSupport_TopTier;
		}
	}
}

void OGLContextInfo_3_2::GetExtensionSetOGL(std::set<std::string> *oglExtensionSet)
{
	GLint extensionCount = 0;
	
	glGetIntegerv(GL_NUM_EXTENSIONS, &extensionCount);
	for (unsigned int i = 0; i < extensionCount; i++)
	{
		std::string extensionName = std::string((const char *)glGetStringi(GL_EXTENSIONS, i));
		oglExtensionSet->insert(extensionName);
	}
}

bool OGLContextInfo_3_2::IsExtensionPresent(const std::set<std::string> &oglExtensionSet, const std::string &extensionName) const
{
	if (oglExtensionSet.size() == 0)
	{
		return false;
	}
	
	return (oglExtensionSet.find(extensionName) != oglExtensionSet.end());
}
