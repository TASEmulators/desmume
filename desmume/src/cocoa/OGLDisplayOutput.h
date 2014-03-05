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

#ifndef _OGLDISPLAYOUTPUT_H_
#define _OGLDISPLAYOUTPUT_H_

#if defined(__APPLE__)
	#include <OpenGL/gl.h>
	#include <OpenGL/glext.h>
#endif

#include <set>
#include <string>
#include "../filter/videofilter.h"


class OGLVideoOutput
{
protected:
	VideoFilter *_vfSingle;
	VideoFilter *_vfDual;
	VideoFilter *_vf;
	
	bool _isShaderSupported;
	double _normalWidth;
	double _normalHeight;
	int _displayMode;
	int _displayOrder;
	int _displayOrientation;
	
	GLint _displayTexFilter;
	GLsizei _viewportWidth;
	GLsizei _viewportHeight;
	GLfloat _gapScalar;
	GLfloat _rotation;
	
	GLvoid *_glTexBack;
	GLsizei _glTexBackWidth;
	GLsizei _glTexBackHeight;
	
	GLuint _displayTexID;
	GLuint _vboVertexID;
	GLuint _vboTexCoordID;
	GLuint _vboElementID;
	GLuint _vaoMainStatesID;
	GLuint _vertexShaderID;
	GLuint _fragmentShaderID;
	GLuint _shaderProgram;
	
	GLint _uniformAngleDegrees;
	GLint _uniformScalar;
	GLint _uniformViewSize;
	
	GLint vtxBuffer[4 * 8];
	GLfloat texCoordBuffer[2 * 8];
	GLubyte vtxIndexBuffer[12];
	size_t _vtxBufferOffset;
	
	void CalculateDisplayNormalSize(double *w, double *h);
	void UpdateVertices();
	void UpdateTexCoords(GLfloat s, GLfloat t);
	
	void GetExtensionSetOGL(std::set<std::string> *oglExtensionSet);
	bool IsExtensionPresent(const std::set<std::string> &oglExtensionSet, const std::string &extensionName) const;
	bool SetupShadersOGL(const char *vertexProgram, const char *fragmentProgram);
	void SetupShaderIO_OGL();
	
public:
	OGLVideoOutput();
	~OGLVideoOutput();
	
	virtual void InitializeOGL();
	virtual void TerminateOGL();
	
	virtual void UploadVerticesOGL();
	virtual void UploadTexCoordsOGL();
	virtual void UploadDisplayTextureOGL(const GLvoid *textureData, const GLenum texPixelFormat, const GLenum texPixelType, GLsizei texWidth, GLsizei texHeight);
	
	virtual void PrerenderOGL(const GLvoid *textureData, GLsizei texWidth, GLsizei texHeight);
	virtual void RenderOGL();
	virtual void SetViewportSizeOGL(GLsizei w, GLsizei h);
	virtual void UpdateDisplayTransformationOGL();
	virtual void SetVideoFilterOGL(const VideoFilterTypeID videoFilterTypeID);
	
	int GetDisplayMode();
	void SetDisplayMode(int dispMode);
	int GetDisplayOrientation();
	void SetDisplayOrientation(int dispOrientation);
	GLfloat GetGapScalar();
	void SetGapScalar(GLfloat theScalar);
	GLfloat GetRotation();
	void SetRotation(GLfloat theRotation);
	bool GetDisplayBilinear();
	void SetDisplayBilinearOGL(bool useBilinear);
	int GetDisplayOrder();
	void SetDisplayOrder(int dispOrder);
};

#endif // _OGLDISPLAYOUTPUT_H_
