/*
	Copyright (C) 2014 DeSmuME team

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

#include "OGLDisplayOutput.h"
#include "cocoa_globals.h"
#include "utilities.h"
#include "../filter/videofilter.h"


// VERTEX SHADER FOR DISPLAY OUTPUT
static const char *vertexProgram_100 = {"\
	attribute vec2 inPosition; \n\
	attribute vec2 inTexCoord0; \n\
	\n\
	uniform vec2 viewSize; \n\
	uniform float scalar; \n\
	uniform float angleDegrees; \n\
	\n\
	varying vec2 vtxTexCoord; \n\
	\n\
	void main() \n\
	{ \n\
		float angleRadians = radians(angleDegrees); \n\
		\n\
		mat2 projection	= mat2(	vec2(2.0/viewSize.x,            0.0), \n\
								vec2(           0.0, 2.0/viewSize.y)); \n\
		\n\
		mat2 rotation	= mat2(	vec2(cos(angleRadians), -sin(angleRadians)), \n\
								vec2(sin(angleRadians),  cos(angleRadians))); \n\
		\n\
		mat2 scale		= mat2(	vec2(scalar,    0.0), \n\
								vec2(   0.0, scalar)); \n\
		\n\
		vtxTexCoord = inTexCoord0; \n\
		gl_Position = vec4(projection * rotation * scale * inPosition, 1.0, 1.0); \n\
	} \n\
"};

// FRAGMENT SHADER FOR DISPLAY OUTPUT
static const char *fragmentProgram_100 = {"\
	varying vec2 vtxTexCoord; \n\
	uniform sampler2D tex; \n\
	\n\
	void main() \n\
	{ \n\
		gl_FragColor = texture2D(tex, vtxTexCoord); \n\
	} \n\
"};

enum OGLVertexAttributeID
{
	OGLVertexAttributeID_Position = 0,
	OGLVertexAttributeID_TexCoord0 = 8
};


OGLVideoOutput::OGLVideoOutput()
{
	_gapScalar = 0.0f;
	_normalWidth = GPU_DISPLAY_WIDTH;
	_normalHeight = GPU_DISPLAY_HEIGHT*2.0 + (DS_DISPLAY_GAP*_gapScalar);
	_displayMode = DS_DISPLAY_TYPE_DUAL;
	_displayOrder = DS_DISPLAY_ORDER_MAIN_FIRST;
	_displayOrientation = DS_DISPLAY_ORIENTATION_VERTICAL;
	_rotation = 0.0f;
	
	_displayTexFilter = GL_NEAREST;
	_glTexPixelFormat = GL_UNSIGNED_SHORT_1_5_5_5_REV;
	_glTexBackWidth = GetNearestPositivePOT((uint32_t)_normalWidth);
	_glTexBackHeight = GetNearestPositivePOT((uint32_t)_normalHeight);
	_glTexBack = (GLvoid *)calloc(_glTexBackWidth * _glTexBackHeight, sizeof(uint16_t));
	
	_vfSingle = new VideoFilter(GPU_DISPLAY_WIDTH, GPU_DISPLAY_HEIGHT, VideoFilterTypeID_None, 0);
	_vfDual = new VideoFilter(GPU_DISPLAY_WIDTH, GPU_DISPLAY_HEIGHT*2, VideoFilterTypeID_None, 2);
	_vf = _vfDual;
	
	UpdateVertices();
	UpdateTexCoords(1.0f, 2.0f);
	_vtxBufferOffset = 0;
	
	// Set up initial vertex elements
	vtxIndexBuffer[0]	= 0;	vtxIndexBuffer[1]	= 1;	vtxIndexBuffer[2]	= 2;
	vtxIndexBuffer[3]	= 2;	vtxIndexBuffer[4]	= 3;	vtxIndexBuffer[5]	= 0;
	
	vtxIndexBuffer[6]	= 4;	vtxIndexBuffer[7]	= 5;	vtxIndexBuffer[8]	= 6;
	vtxIndexBuffer[9]	= 6;	vtxIndexBuffer[10]	= 7;	vtxIndexBuffer[11]	= 4;
}

OGLVideoOutput::~OGLVideoOutput()
{
	free(_glTexBack);
	delete _vfSingle;
	delete _vfDual;
	_vf = NULL;
}

void OGLVideoOutput::InitializeOGL()
{
	// Check the OpenGL capabilities for this renderer
	std::set<std::string> oglExtensionSet;
	GetExtensionSetOGL(&oglExtensionSet);
	
	// Set up textures
	glGenTextures(1, &this->_displayTexID);
	glBindTexture(GL_TEXTURE_2D, this->_displayTexID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);
	
	// Set up shaders (but disable on PowerPC, since it doesn't seem to work there)
#if defined(__i386__) || defined(__x86_64__)
	_isShaderSupported	= IsExtensionPresent(oglExtensionSet, "GL_ARB_shader_objects") &&
						  IsExtensionPresent(oglExtensionSet, "GL_ARB_vertex_shader") &&
						  IsExtensionPresent(oglExtensionSet, "GL_ARB_fragment_shader") &&
						  IsExtensionPresent(oglExtensionSet, "GL_ARB_vertex_program");
#else
	this->_isShaderSupported	= false;
#endif
	if (this->_isShaderSupported)
	{
		bool isShaderSetUp = SetupShadersOGL(vertexProgram_100, fragmentProgram_100);
		if (isShaderSetUp)
		{
			glUseProgram(this->_shaderProgram);
			
			this->_uniformAngleDegrees = glGetUniformLocation(this->_shaderProgram, "angleDegrees");
			this->_uniformScalar = glGetUniformLocation(this->_shaderProgram, "scalar");
			this->_uniformViewSize = glGetUniformLocation(this->_shaderProgram, "viewSize");
			
			glUniform1f(this->_uniformAngleDegrees, 0.0f);
			glUniform1f(this->_uniformScalar, 1.0f);
			glUniform2f(this->_uniformViewSize, GPU_DISPLAY_WIDTH, GPU_DISPLAY_HEIGHT*2.0 + (DS_DISPLAY_GAP*this->_gapScalar));
		}
		else
		{
			this->_isShaderSupported = false;
		}
	}
	
	// Set up VBOs
	glGenBuffersARB(1, &this->_vboVertexID);
	glGenBuffersARB(1, &this->_vboTexCoordID);
	glGenBuffersARB(1, &this->_vboElementID);
	
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, this->_vboVertexID);
	glBufferDataARB(GL_ARRAY_BUFFER_ARB, sizeof(GLint) * (2 * 8), this->vtxBuffer, GL_STATIC_DRAW_ARB);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, this->_vboTexCoordID);
	glBufferDataARB(GL_ARRAY_BUFFER_ARB, sizeof(GLfloat) * (2 * 8), this->texCoordBuffer, GL_STATIC_DRAW_ARB);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, this->_vboElementID);
	glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, sizeof(GLubyte) * 12, this->vtxIndexBuffer, GL_STATIC_DRAW_ARB);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
	
	// Set up VAO
	glGenVertexArraysAPPLE(1, &this->_vaoMainStatesID);
	glBindVertexArrayAPPLE(this->_vaoMainStatesID);
	
	if (this->_isShaderSupported)
	{
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, this->_vboVertexID);
		glVertexAttribPointer(OGLVertexAttributeID_Position, 2, GL_INT, GL_FALSE, 0, 0);
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, this->_vboTexCoordID);
		glVertexAttribPointer(OGLVertexAttributeID_TexCoord0, 2, GL_FLOAT, GL_FALSE, 0, 0);
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, this->_vboElementID);
		
		glEnableVertexAttribArray(OGLVertexAttributeID_Position);
		glEnableVertexAttribArray(OGLVertexAttributeID_TexCoord0);
	}
	else
	{
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, this->_vboVertexID);
		glVertexPointer(2, GL_INT, 0, 0);
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, this->_vboTexCoordID);
		glTexCoordPointer(2, GL_FLOAT, 0, 0);
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, this->_vboElementID);
		
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	
	glBindVertexArrayAPPLE(0);
	
	// Render State Setup (common to both shaders and fixed-function pipeline)
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_DITHER);
	glDisable(GL_STENCIL_TEST);
	
	// Set up fixed-function pipeline render states.
	if (!_isShaderSupported)
	{
		glDisable(GL_ALPHA_TEST);
		glDisable(GL_LIGHTING);
		glDisable(GL_FOG);
		glEnable(GL_TEXTURE_2D);
	}
	
	// Set up clear attributes
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
}

void OGLVideoOutput::TerminateOGL()
{
	glDeleteTextures(1, &this->_displayTexID);
	
	glDeleteVertexArraysAPPLE(1, &this->_vaoMainStatesID);
	glDeleteBuffersARB(1, &this->_vboVertexID);
	glDeleteBuffersARB(1, &this->_vboTexCoordID);
	glDeleteBuffersARB(1, &this->_vboElementID);
	
	if (this->_isShaderSupported)
	{
		glUseProgram(0);
		
		glDetachShader(this->_shaderProgram, this->_vertexShaderID);
		glDetachShader(this->_shaderProgram, this->_fragmentShaderID);
		
		glDeleteProgram(this->_shaderProgram);
		glDeleteShader(this->_vertexShaderID);
		glDeleteShader(this->_fragmentShaderID);
	}
}

int OGLVideoOutput::GetDisplayMode()
{
	return this->_displayMode;
}

void OGLVideoOutput::SetDisplayMode(int dispMode)
{
	this->_displayMode = dispMode;
	this->_vf = (dispMode == DS_DISPLAY_TYPE_DUAL) ? this->_vfDual : this->_vfSingle;
	this->CalculateDisplayNormalSize(&this->_normalWidth, &this->_normalHeight);
	this->UpdateVertices();
}

int OGLVideoOutput::GetDisplayOrientation()
{
	return this->_displayOrientation;
}

void OGLVideoOutput::SetDisplayOrientation(int dispOrientation)
{
	this->_displayOrientation = dispOrientation;
	this->CalculateDisplayNormalSize(&this->_normalWidth, &this->_normalHeight);
	this->UpdateVertices();
}

GLfloat OGLVideoOutput::GetGapScalar()
{
	return this->_gapScalar;
}

void OGLVideoOutput::SetGapScalar(GLfloat theScalar)
{
	this->_gapScalar = theScalar;
	this->CalculateDisplayNormalSize(&this->_normalWidth, &this->_normalHeight);
	this->UpdateVertices();
}

GLfloat OGLVideoOutput::GetRotation()
{
	return this->_rotation;
}

void OGLVideoOutput::SetRotation(GLfloat theRotation)
{
	this->_rotation = theRotation;
}

bool OGLVideoOutput::GetDisplayBilinear()
{
	return (this->_displayTexFilter == GL_LINEAR);
}

void OGLVideoOutput::SetDisplayBilinearOGL(bool useBilinear)
{
	this->_displayTexFilter = (useBilinear) ? GL_LINEAR : GL_NEAREST;
	
	glBindTexture(GL_TEXTURE_2D, this->_displayTexID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, this->_displayTexFilter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, this->_displayTexFilter);
	glBindTexture(GL_TEXTURE_2D, 0);
}

int OGLVideoOutput::GetDisplayOrder()
{
	return this->_displayOrder;
}

void OGLVideoOutput::SetDisplayOrder(int dispOrder)
{
	this->_displayOrder = dispOrder;
	
	if (this->_displayOrder == DS_DISPLAY_ORDER_MAIN_FIRST)
	{
		this->_vtxBufferOffset = 0;
	}
	else // dispOrder == DS_DISPLAY_ORDER_TOUCH_FIRST
	{
		this->_vtxBufferOffset = (2 * 8);
	}
}

void OGLVideoOutput::SetViewportSizeOGL(GLsizei w, GLsizei h)
{
	this->_viewportWidth = w;
	this->_viewportHeight = h;
	const CGSize checkSize = GetTransformedBounds(this->_normalWidth, this->_normalHeight, 1.0, this->_rotation);
	const GLdouble s = GetMaxScalarInBounds(checkSize.width, checkSize.height, w, h);
	
	glViewport(0, 0, w, h);
	
	if (this->_isShaderSupported)
	{
		glUniform2f(this->_uniformViewSize, w, h);
		glUniform1f(this->_uniformScalar, s);
	}
	else
	{
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(-w/2, -w/2 + w, -h/2, -h/2 + h, -1.0, 1.0);
		glRotatef(CLOCKWISE_DEGREES(this->_rotation), 0.0f, 0.0f, 1.0f);
		glScalef(s, s, 1.0f);
	}
}

void OGLVideoOutput::UpdateDisplayTransformationOGL()
{
	const CGSize checkSize = GetTransformedBounds(this->_normalWidth, this->_normalHeight, 1.0, this->_rotation);
	const GLdouble s = GetMaxScalarInBounds(checkSize.width, checkSize.height, this->_viewportWidth, this->_viewportHeight);
	
	if (this->_isShaderSupported)
	{
		glUniform1f(this->_uniformAngleDegrees, this->_rotation);
		glUniform1f(this->_uniformScalar, s);
	}
	else
	{
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glRotatef(CLOCKWISE_DEGREES(this->_rotation), 0.0f, 0.0f, 1.0f);
		glScalef(s, s, 1.0f);
	}
}

void OGLVideoOutput::RespondToVideoFilterChangeOGL(const VideoFilterTypeID videoFilterTypeID)
{
	this->_vfSingle->ChangeFilterByID(videoFilterTypeID);
	this->_vfDual->ChangeFilterByID(videoFilterTypeID);
	
	const GLsizei vfDstWidth = this->_vf->GetDstWidth();
	const GLsizei vfDstHeight = (this->_displayMode == DS_DISPLAY_TYPE_DUAL) ? this->_vf->GetDstHeight() : this->_vf->GetDstHeight() * 2;
	
	size_t colorDepth = sizeof(uint32_t);
	this->_glTexPixelFormat = GL_UNSIGNED_INT_8_8_8_8_REV;
	
	if (videoFilterTypeID == VideoFilterTypeID_None)
	{
		colorDepth = sizeof(uint16_t);
		this->_glTexPixelFormat = GL_UNSIGNED_SHORT_1_5_5_5_REV;
	}
	
	// Convert textures to Power-of-Two to support older GPUs
	// Example: Radeon X1600M on the 2006 MacBook Pro
	const GLsizei potW = GetNearestPositivePOT((uint32_t)vfDstWidth);
	const GLsizei potH = GetNearestPositivePOT((uint32_t)vfDstHeight);
	
	if (this->_glTexBackWidth != potW || this->_glTexBackHeight != potH)
	{
		this->_glTexBackWidth = potW;
		this->_glTexBackHeight = potH;
		
		free(this->_glTexBack);
		this->_glTexBack = (GLvoid *)calloc((size_t)potW * (size_t)potH, colorDepth);
		if (this->_glTexBack == NULL)
		{
			return;
		}
	}
	
	const GLfloat s = (GLfloat)vfDstWidth / (GLfloat)potW;
	const GLfloat t = (GLfloat)vfDstHeight / (GLfloat)potH;
	this->UpdateTexCoords(s, t);
	
	glBindTexture(GL_TEXTURE_2D, this->_displayTexID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)potW, (GLsizei)potH, 0, GL_BGRA, this->_glTexPixelFormat, this->_glTexBack);
	glBindTexture(GL_TEXTURE_2D, 0);
	this->UploadTexCoordsOGL();
}

void OGLVideoOutput::CalculateDisplayNormalSize(double *w, double *h)
{
	if (w == NULL || h == NULL)
	{
		return;
	}
	
	if (this->_displayMode != DS_DISPLAY_TYPE_DUAL)
	{
		*w = GPU_DISPLAY_WIDTH;
		*h = GPU_DISPLAY_HEIGHT;
		return;
	}
	
	if (this->_displayOrientation == DS_DISPLAY_ORIENTATION_VERTICAL)
	{
		*w = GPU_DISPLAY_WIDTH;
		*h = GPU_DISPLAY_HEIGHT * 2.0 + (DS_DISPLAY_GAP * this->_gapScalar);
	}
	else
	{
		*w = GPU_DISPLAY_WIDTH * 2.0 + (DS_DISPLAY_GAP * this->_gapScalar);
		*h = GPU_DISPLAY_HEIGHT;
	}
}

void OGLVideoOutput::GetExtensionSetOGL(std::set<std::string> *oglExtensionSet)
{
	std::string oglExtensionString = std::string((const char *)glGetString(GL_EXTENSIONS));
	
	size_t extStringStartLoc = 0;
	size_t delimiterLoc = oglExtensionString.find_first_of(' ', extStringStartLoc);
	while (delimiterLoc != std::string::npos)
	{
		std::string extensionName = oglExtensionString.substr(extStringStartLoc, delimiterLoc - extStringStartLoc);
		oglExtensionSet->insert(extensionName);
		
		extStringStartLoc = delimiterLoc + 1;
		delimiterLoc = oglExtensionString.find_first_of(' ', extStringStartLoc);
	}
	
	if (extStringStartLoc - oglExtensionString.length() > 0)
	{
		std::string extensionName = oglExtensionString.substr(extStringStartLoc, oglExtensionString.length() - extStringStartLoc);
		oglExtensionSet->insert(extensionName);
	}
}

bool OGLVideoOutput::IsExtensionPresent(const std::set<std::string> &oglExtensionSet, const std::string &extensionName) const
{
	if (oglExtensionSet.size() == 0)
	{
		return false;
	}
	
	return (oglExtensionSet.find(extensionName) != oglExtensionSet.end());
}

bool OGLVideoOutput::SetupShadersOGL(const char *vertexProgram, const char *fragmentProgram)
{
	bool result = false;
	GLint shaderStatus = GL_TRUE;
	
	this->_vertexShaderID = glCreateShader(GL_VERTEX_SHADER);
	if (this->_vertexShaderID == 0)
	{
		printf("OpenGL Error - Failed to create vertex shader.");
		return result;
	}
	
	glShaderSource(this->_vertexShaderID, 1, (const GLchar **)&vertexProgram, NULL);
	glCompileShader(this->_vertexShaderID);
	glGetShaderiv(this->_vertexShaderID, GL_COMPILE_STATUS, &shaderStatus);
	if (shaderStatus == GL_FALSE)
	{
		glDeleteShader(this->_vertexShaderID);
		printf("OpenGL Error - Failed to compile vertex shader.");
		return result;
	}
	
	this->_fragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);
	if (this->_fragmentShaderID == 0)
	{
		glDeleteShader(this->_vertexShaderID);
		printf("OpenGL Error - Failed to create fragment shader.");
		return result;
	}
	
	glShaderSource(this->_fragmentShaderID, 1, (const GLchar **)&fragmentProgram, NULL);
	glCompileShader(this->_fragmentShaderID);
	glGetShaderiv(this->_fragmentShaderID, GL_COMPILE_STATUS, &shaderStatus);
	if (shaderStatus == GL_FALSE)
	{
		glDeleteShader(this->_vertexShaderID);
		glDeleteShader(this->_fragmentShaderID);
		printf("OpenGL Error - Failed to compile fragment shader.");
		return result;
	}
	
	this->_shaderProgram = glCreateProgram();
	if (this->_shaderProgram == 0)
	{
		glDeleteShader(this->_vertexShaderID);
		glDeleteShader(this->_fragmentShaderID);
		printf("OpenGL Error - Failed to create shader program.");
		return result;
	}
	
	glAttachShader(this->_shaderProgram, this->_vertexShaderID);
	glAttachShader(this->_shaderProgram, this->_fragmentShaderID);
	
	this->SetupShaderIO_OGL();
	
	glLinkProgram(this->_shaderProgram);
	glGetProgramiv(this->_shaderProgram, GL_LINK_STATUS, &shaderStatus);
	if (shaderStatus == GL_FALSE)
	{
		glDeleteProgram(this->_shaderProgram);
		glDeleteShader(this->_vertexShaderID);
		glDeleteShader(this->_fragmentShaderID);
		printf("OpenGL Error - Failed to link shader program.");
		return result;
	}
	
	glValidateProgram(this->_shaderProgram);
	
	result = true;
	return result;
}

void OGLVideoOutput::SetupShaderIO_OGL()
{
	glBindAttribLocation(this->_shaderProgram, OGLVertexAttributeID_Position, "inPosition");
	glBindAttribLocation(this->_shaderProgram, OGLVertexAttributeID_TexCoord0, "inTexCoord0");
}

void OGLVideoOutput::UploadVerticesOGL()
{
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, this->_vboVertexID);
	glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, 0, sizeof(GLint) * (2 * 8), this->vtxBuffer + this->_vtxBufferOffset);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
}

void OGLVideoOutput::UploadTexCoordsOGL()
{
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, this->_vboTexCoordID);
	glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, 0, sizeof(GLfloat) * (2 * 8), this->texCoordBuffer);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
}

void OGLVideoOutput::UploadDisplayTextureOGL(const GLvoid *textureData, GLsizei texWidth, GLsizei texHeight)
{
	if (textureData == NULL)
	{
		return;
	}
	
	const GLint lineOffset = (this->_displayMode == DS_DISPLAY_TYPE_TOUCH) ? texHeight : 0;
	
	glBindTexture(GL_TEXTURE_2D, this->_displayTexID);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, lineOffset, texWidth, texHeight, GL_RGBA, this->_glTexPixelFormat, textureData);
	glBindTexture(GL_TEXTURE_2D, 0);
}

void OGLVideoOutput::UpdateVertices()
{
	const GLfloat w = GPU_DISPLAY_WIDTH;
	const GLfloat h = GPU_DISPLAY_HEIGHT;
	const GLfloat gap = DS_DISPLAY_GAP * this->_gapScalar / 2.0;
	
	if (this->_displayMode == DS_DISPLAY_TYPE_DUAL)
	{
		// displayOrder == DS_DISPLAY_ORDER_MAIN_FIRST
		if (this->_displayOrientation == DS_DISPLAY_ORIENTATION_VERTICAL)
		{
			vtxBuffer[0]	= -w/2;			vtxBuffer[1]		= h+gap;	// Top display, top left
			vtxBuffer[2]	= w/2;			vtxBuffer[3]		= h+gap;	// Top display, top right
			vtxBuffer[4]	= w/2;			vtxBuffer[5]		= gap;		// Top display, bottom right
			vtxBuffer[6]	= -w/2;			vtxBuffer[7]		= gap;		// Top display, bottom left
			
			vtxBuffer[8]	= -w/2;			vtxBuffer[9]		= -gap;		// Bottom display, top left
			vtxBuffer[10]	= w/2;			vtxBuffer[11]		= -gap;		// Bottom display, top right
			vtxBuffer[12]	= w/2;			vtxBuffer[13]		= -(h+gap);	// Bottom display, bottom right
			vtxBuffer[14]	= -w/2;			vtxBuffer[15]		= -(h+gap);	// Bottom display, bottom left
		}
		else // displayOrientationID == DS_DISPLAY_ORIENTATION_HORIZONTAL
		{
			vtxBuffer[0]	= -(w+gap);		vtxBuffer[1]		= h/2;		// Left display, top left
			vtxBuffer[2]	= -gap;			vtxBuffer[3]		= h/2;		// Left display, top right
			vtxBuffer[4]	= -gap;			vtxBuffer[5]		= -h/2;		// Left display, bottom right
			vtxBuffer[6]	= -(w+gap);		vtxBuffer[7]		= -h/2;		// Left display, bottom left
			
			vtxBuffer[8]	= gap;			vtxBuffer[9]		= h/2;		// Right display, top left
			vtxBuffer[10]	= w+gap;		vtxBuffer[11]		= h/2;		// Right display, top right
			vtxBuffer[12]	= w+gap;		vtxBuffer[13]		= -h/2;		// Right display, bottom right
			vtxBuffer[14]	= gap;			vtxBuffer[15]		= -h/2;		// Right display, bottom left
		}
		
		// displayOrder == DS_DISPLAY_ORDER_TOUCH_FIRST
		memcpy(vtxBuffer + (2 * 8), vtxBuffer + (1 * 8), sizeof(GLint) * (1 * 8));
		memcpy(vtxBuffer + (3 * 8), vtxBuffer + (0 * 8), sizeof(GLint) * (1 * 8));
	}
	else // displayModeID == DS_DISPLAY_TYPE_MAIN || displayModeID == DS_DISPLAY_TYPE_TOUCH
	{
		vtxBuffer[0]	= -w/2;		vtxBuffer[1]		= h/2;		// First display, top left
		vtxBuffer[2]	= w/2;		vtxBuffer[3]		= h/2;		// First display, top right
		vtxBuffer[4]	= w/2;		vtxBuffer[5]		= -h/2;		// First display, bottom right
		vtxBuffer[6]	= -w/2;		vtxBuffer[7]		= -h/2;		// First display, bottom left
		
		memcpy(vtxBuffer + (1 * 8), vtxBuffer + (0 * 8), sizeof(GLint) * (1 * 8));	// Second display
		memcpy(vtxBuffer + (2 * 8), vtxBuffer + (0 * 8), sizeof(GLint) * (2 * 8));	// Second display
	}
}

void OGLVideoOutput::UpdateTexCoords(GLfloat s, GLfloat t)
{
	texCoordBuffer[0]	= 0.0f;		texCoordBuffer[1]	=   0.0f;
	texCoordBuffer[2]	=    s;		texCoordBuffer[3]	=   0.0f;
	texCoordBuffer[4]	=    s;		texCoordBuffer[5]	= t/2.0f;
	texCoordBuffer[6]	= 0.0f;		texCoordBuffer[7]	= t/2.0f;
	
	texCoordBuffer[8]	= 0.0f;		texCoordBuffer[9]	= t/2.0f;
	texCoordBuffer[10]	=    s;		texCoordBuffer[11]	= t/2.0f;
	texCoordBuffer[12]	=    s;		texCoordBuffer[13]	=      t;
	texCoordBuffer[14]	= 0.0f;		texCoordBuffer[15]	=      t;
}

void OGLVideoOutput::PrerenderOGL(const GLvoid *textureData, GLsizei texWidth, GLsizei texHeight)
{
	uint32_t *vfTextureData = (uint32_t *)textureData;
	
	if (this->_vf->GetTypeID() != VideoFilterTypeID_None)
	{
		RGB555ToRGBA8888Buffer((const uint16_t *)textureData, (uint32_t *)this->_vf->GetSrcBufferPtr(), texWidth * texHeight);
		texWidth = this->_vf->GetDstWidth();
		texHeight = this->_vf->GetDstHeight();
		vfTextureData = this->_vf->RunFilter();
	}
	
	this->UploadDisplayTextureOGL(vfTextureData, texWidth, texHeight);
}

void OGLVideoOutput::RenderOGL()
{
	// Enable vertex attributes
	glBindVertexArrayAPPLE(this->_vaoMainStatesID);
	
	const GLsizei vtxElementCount = (this->_displayMode == DS_DISPLAY_TYPE_DUAL) ? 12 : 6;
	const GLubyte *elementPointer = !(this->_displayMode == DS_DISPLAY_TYPE_TOUCH) ? 0 : (GLubyte *)(vtxElementCount * sizeof(GLubyte));
	
	glClear(GL_COLOR_BUFFER_BIT);
	glBindTexture(GL_TEXTURE_2D, this->_displayTexID);
	glDrawElements(GL_TRIANGLES, vtxElementCount, GL_UNSIGNED_BYTE, elementPointer);
	glBindTexture(GL_TEXTURE_2D, 0);
	
	// Disable vertex attributes
	glBindVertexArrayAPPLE(0);
}
