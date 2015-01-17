/*
	Copyright (C) 2014-2015 DeSmuME team

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

class OGLVideoOutput;

enum
{
	OutputFilterTypeID_NearestNeighbor	= 0,
	OutputFilterTypeID_Bilinear			= 1,
	OutputFilterTypeID_BicubicBSpline	= 2,
	OutputFilterTypeID_BicubicMitchell	= 3,
	OutputFilterTypeID_Lanczos2			= 4,
	OutputFilterTypeID_Lanczos3			= 5
};

class OGLInfo
{
protected:
	unsigned int _versionMajor;
	unsigned int _versionMinor;
	unsigned int _versionRevision;
	
	bool _isVBOSupported;
	bool _isPBOSupported;
	bool _isShaderSupported;
	bool _isFBOSupported;
	
public:
	OGLInfo();
	virtual ~OGLInfo() {};
	
	static OGLInfo* GetVersionedObjectOGL();
	
	bool IsVBOSupported();
	bool IsPBOSupported();
	bool IsShaderSupported();
	bool IsFBOSupported();
	
	virtual void GetExtensionSetOGL(std::set<std::string> *oglExtensionSet) = 0;
	virtual bool IsExtensionPresent(const std::set<std::string> &oglExtensionSet, const std::string &extensionName) const = 0;
};

class OGLInfo_1_2 : public OGLInfo
{
public:
	OGLInfo_1_2();
	
	virtual void GetExtensionSetOGL(std::set<std::string> *oglExtensionSet);
	virtual bool IsExtensionPresent(const std::set<std::string> &oglExtensionSet, const std::string &extensionName) const;
};

class OGLInfo_2_0 : public OGLInfo_1_2
{
public:
	OGLInfo_2_0();
};

class OGLInfo_2_1 : public OGLInfo_2_0
{
public:
	OGLInfo_2_1();
};

class OGLInfo_3_2 : public OGLInfo_2_0
{
public:
	OGLInfo_3_2();
	
	virtual void GetExtensionSetOGL(std::set<std::string> *oglExtensionSet);
};

class OGLShaderProgram
{
protected:
	GLuint _vertexID;
	GLuint _fragmentID;
	GLuint _programID;
	
	virtual GLuint LoadShaderOGL(GLenum shaderType, const char *shaderProgram);
	virtual bool LinkOGL();
	
public:
	OGLShaderProgram();
	virtual ~OGLShaderProgram();
	
	GLuint GetVertexShaderID();
	void SetVertexShaderOGL(const char *shaderProgram);
	GLuint GetFragmentShaderID();
	void SetFragmentShaderOGL(const char *shaderProgram);
	void SetVertexAndFragmentShaderOGL(const char *vertShaderProgram, const char *fragShaderProgram);
	GLuint GetProgramID();
};

class OGLFilter
{
private:
	void OGLFilterInit(GLsizei srcWidth, GLsizei srcHeight, GLfloat scale);
	
protected:
	OGLShaderProgram *_program;
	GLuint _texDstID;
	GLint _texCoordBuffer[8];
	
	GLuint _fboID;
	GLuint _vaoID;
	GLuint _vboVtxID;
	GLuint _vboTexCoordID;
	GLuint _vboElementID;
	
	GLfloat _scale;
	GLsizei _srcWidth;
	GLsizei _srcHeight;
	GLsizei _dstWidth;
	GLsizei _dstHeight;
	
public:
	OGLFilter();
	OGLFilter(GLsizei srcWidth, GLsizei srcHeight, GLfloat scale);
	virtual ~OGLFilter();
	static void GetSupport(int vfTypeID, bool *outSupportCPU, bool *outSupportShader);
	
	OGLShaderProgram* GetProgram();
	GLuint GetDstTexID();
	GLsizei GetDstWidth();
	GLsizei GetDstHeight();
	void SetSrcSizeOGL(GLsizei w, GLsizei h);
	GLfloat GetScale();
	void SetScaleOGL(GLfloat scale);
	virtual GLuint RunFilterOGL(GLuint srcTexID, GLsizei viewportWidth, GLsizei viewportHeight);
	void DownloadDstBufferOGL(uint32_t *dstBuffer, size_t lineOffset, size_t readLineCount);
};

class OGLFilterDeposterize : public OGLFilter
{
protected:
	GLuint _texIntermediateID;
	
public:
	OGLFilterDeposterize(GLsizei srcWidth, GLsizei srcHeight);
	~OGLFilterDeposterize();
	
	virtual GLuint RunFilterOGL(GLuint srcTexID, GLsizei viewportWidth, GLsizei viewportHeight);
};

class OGLVideoLayer
{
protected:
	OGLVideoOutput *_output;
	GLsizei _viewportWidth;
	GLsizei _viewportHeight;
	
public:
	OGLVideoLayer() {};
	~OGLVideoLayer() {};
	
	void SetViewportSizeOGL(GLsizei w, GLsizei h)
	{
		this->_viewportWidth = w;
		this->_viewportHeight = h;
	};
	
	virtual void ProcessOGL(const uint16_t *videoData, GLsizei w, GLsizei h) {};
	virtual void RenderOGL() {};
};

class OGLDisplayLayer : public OGLVideoLayer
{
protected:
	bool _canUseShaderBasedFilters;
	bool _canUseShaderOutput;
	
	bool _needUploadVertices;
	bool _useDeposterize;
	bool _useShaderBasedPixelScaler;
	bool _filtersPreferGPU;
	int _outputFilter;
	int _pixelScaler;
	
	OGLFilterDeposterize *_filterDeposterize;
	OGLFilter *_shaderFilter;
	OGLShaderProgram *_finalOutputProgram;
	
	VideoFilter *_vfSingle;
	VideoFilter *_vfDual;
	VideoFilter *_vf;
	uint32_t *_vfMasterDstBuffer;
	
	int _displayMode;
	int _displayOrder;
	int _displayOrientation;
	double _normalWidth;
	double _normalHeight;
	GLfloat _gapScalar;
	GLfloat _rotation;
	
	GLint _displayTexFilter;
	GLuint _texCPUFilterDstID;
	
	GLint vtxBuffer[4 * 8];
	GLfloat texCoordBuffer[2 * 8];
	size_t _vtxBufferOffset;
	
	GLuint _texInputVideoDataID;
	GLuint _texOutputVideoDataID;
	GLuint _texPrevOutputVideoDataID;
	GLuint _vaoMainStatesID;
	GLuint _vboVertexID;
	GLuint _vboTexCoordID;
	GLuint _vboElementID;
	
	GLint _uniformFinalOutputAngleDegrees;
	GLint _uniformFinalOutputScalar;
	GLint _uniformFinalOutputViewSize;
	
	virtual void UploadVerticesOGL();
	virtual void UploadTexCoordsOGL();
	virtual void UploadTransformationOGL();
	
	void UpdateVertices();
	void UpdateTexCoords(GLfloat s, GLfloat t);
	
public:
	OGLDisplayLayer(OGLVideoOutput *oglVO);
	~OGLDisplayLayer();
	
	bool GetFiltersPreferGPU();
	void SetFiltersPreferGPUOGL(bool preferGPU);
	
	int GetMode();
	void SetMode(int dispMode);
	int GetOrientation();
	void SetOrientation(int dispOrientation);
	int GetOrder();
	void SetOrder(int dispOrder);
	GLfloat GetGapScalar();
	void SetGapScalar(GLfloat theScalar);
	GLfloat GetRotation();
	void SetRotation(GLfloat theRotation);
	bool GetBilinear();
	void SetBilinear(bool useBilinear);
	bool GetSourceDeposterize();
	void SetSourceDeposterize(bool useDeposterize);
	
	bool CanUseShaderBasedFilters();
	void GetNormalSize(double *w, double *h);
	
	int GetOutputFilter();
	virtual void SetOutputFilterOGL(const int filterID);
	int GetPixelScaler();
	virtual void SetPixelScalerOGL(const int filterID);
	virtual void SetCPUFilterOGL(const VideoFilterTypeID videoFilterTypeID);
	virtual void ProcessOGL(const uint16_t *videoData, GLsizei w, GLsizei h);
	virtual void RenderOGL();
};

class OGLVideoOutput
{
protected:
	OGLInfo *_info;
	GLsizei _viewportWidth;
	GLsizei _viewportHeight;
	std::vector<OGLVideoLayer *> *_layerList;
	
public:
	OGLVideoOutput();
	~OGLVideoOutput();
	
	virtual void InitLayers();
	virtual OGLInfo* GetInfo();
	virtual GLsizei GetViewportWidth();
	virtual GLsizei GetViewportHeight();
	OGLDisplayLayer* GetDisplayLayer();
		
	virtual void ProcessOGL(const uint16_t *videoData, GLsizei w, GLsizei h);
	virtual void RenderOGL();
	virtual void SetViewportSizeOGL(GLsizei w, GLsizei h);
};

#endif // _OGLDISPLAYOUTPUT_H_
