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

#ifndef _OGLDISPLAYOUTPUT_3_2_H_

#if defined(__APPLE__)
	#include <OpenGL/gl.h>
	#include <OpenGL/glext.h>
#endif

#endif // _OGLDISPLAYOUTPUT_3_2_H_

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

enum ShaderSupportTier
{
	ShaderSupport_Unsupported	= 0,
	ShaderSupport_BottomTier	= 1,
	ShaderSupport_LowTier		= 2,
	ShaderSupport_MidTier		= 3,
	ShaderSupport_HighTier		= 4,
	ShaderSupport_TopTier		= 5,
	ShaderSupport_FutureTier	= 6,
};

class OGLInfo
{
protected:
	GLint _versionMajor;
	GLint _versionMinor;
	GLint _versionRevision;
	ShaderSupportTier _shaderSupport;
	bool _useShader150;
	
	bool _isVBOSupported;
	bool _isPBOSupported;
	bool _isFBOSupported;
	
public:
	OGLInfo();
	virtual ~OGLInfo() {};
		
	bool IsUsingShader150();
	bool IsVBOSupported();
	bool IsPBOSupported();
	bool IsShaderSupported();
	bool IsFBOSupported();
	ShaderSupportTier GetShaderSupport();
	
	virtual void GetExtensionSetOGL(std::set<std::string> *oglExtensionSet) = 0;
	virtual bool IsExtensionPresent(const std::set<std::string> &oglExtensionSet, const std::string &extensionName) const = 0;
};

class OGLInfo_Legacy : public OGLInfo
{
public:
	OGLInfo_Legacy();
	
	virtual void GetExtensionSetOGL(std::set<std::string> *oglExtensionSet);
	virtual bool IsExtensionPresent(const std::set<std::string> &oglExtensionSet, const std::string &extensionName) const;
};

class OGLShaderProgram
{
protected:
	GLuint _vertexID;
	GLuint _fragmentID;
	GLuint _programID;
	ShaderSupportTier _shaderSupport;
	
	virtual GLuint LoadShaderOGL(GLenum shaderType, const char *shaderProgram, bool useShader150);
	virtual bool LinkOGL();
	
public:
	OGLShaderProgram();
	virtual ~OGLShaderProgram();
	
	ShaderSupportTier GetShaderSupport();
	void SetShaderSupport(const ShaderSupportTier theTier);
	GLuint GetVertexShaderID();
	void SetVertexShaderOGL(const char *shaderProgram, bool useShader150);
	GLuint GetFragmentShaderID();
	void SetFragmentShaderOGL(const char *shaderProgram, bool useShader150);
	void SetVertexAndFragmentShaderOGL(const char *vertShaderProgram, const char *fragShaderProgram, bool useShader150);
	GLuint GetProgramID();
};

class OGLFilter
{
protected:
	OGLShaderProgram *_program;
	bool _isVAOPresent;
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
	
	virtual void OGLFilterInit(GLsizei srcWidth, GLsizei srcHeight, GLfloat scale);
	
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
	virtual GLuint RunFilterOGL(GLuint srcTexID);
	void DownloadDstBufferOGL(uint32_t *dstBuffer, size_t lineOffset, size_t readLineCount);
};

class OGLFilterDeposterize : public OGLFilter
{
protected:
	GLuint _texIntermediateID;
	
public:
	OGLFilterDeposterize(GLsizei srcWidth, GLsizei srcHeight, ShaderSupportTier theTier, bool useShader150);
	~OGLFilterDeposterize();
	
	virtual GLuint RunFilterOGL(GLuint srcTexID);
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
	
	virtual void ProcessOGL() {};
	virtual void RenderOGL() {};
};

class OGLImage
{
protected:
	bool _isVAOPresent;
	bool _canUseShaderBasedFilters;
	bool _canUseShaderOutput;
	bool _useShader150;
	ShaderSupportTier _shaderSupport;
	
	bool _needUploadVertices;
	bool _useDeposterize;
	bool _useShaderBasedPixelScaler;
	bool _filtersPreferGPU;
	int _outputFilter;
	VideoFilterTypeID _pixelScaler;
	
	OGLFilter *_filterDeposterize;
	OGLFilter *_shaderFilter;
	OGLShaderProgram *_finalOutputProgram;
	
	VideoFilter *_vf;
	uint32_t *_vfMasterDstBuffer;
	
	double _normalWidth;
	double _normalHeight;
	GLsizei _viewportWidth;
	GLsizei _viewportHeight;
	
	GLubyte *_vtxElementPointer;
	
	GLint _displayTexFilter;
	GLuint _texCPUFilterDstID;
	
	GLuint _texLQ2xLUT;
	GLuint _texHQ2xLUT;
	GLuint _texHQ4xLUT;
	
	GLint _vtxBuffer[8];
	GLfloat _texCoordBuffer[8];
	
	GLuint _texVideoInputDataID;
	GLuint _texVideoSourceID;
	GLuint _texVideoPixelScalerID;
	GLuint _texVideoOutputID;
	GLuint _vaoMainStatesID;
	GLuint _vboVertexID;
	GLuint _vboTexCoordID;
	GLuint _vboElementID;
	
	GLint _uniformFinalOutputAngleDegrees;
	GLint _uniformFinalOutputScalar;
	GLint _uniformFinalOutputViewSize;
	
	void UploadHQnxLUTs();
	
	virtual void UploadVerticesOGL();
	virtual void UploadTexCoordsOGL();
	virtual void UploadTransformationOGL();
	
	void UpdateVertices();
	void UpdateTexCoords(GLfloat s, GLfloat t);
	
public:
	OGLImage() {};
	OGLImage(OGLInfo *oglInfo, GLsizei imageWidth, GLsizei imageHeight, GLsizei viewportWidth, GLsizei viewportHeight);
	virtual ~OGLImage();
	
	bool GetFiltersPreferGPU();
	void SetFiltersPreferGPUOGL(bool preferGPU);
	
	bool GetSourceDeposterize();
	void SetSourceDeposterize(bool useDeposterize);
	
	bool CanUseShaderBasedFilters();
	void GetNormalSize(double *w, double *h);
	
	int GetOutputFilter();
	virtual void SetOutputFilterOGL(const int filterID);
	int GetPixelScaler();
	virtual void SetPixelScalerOGL(const int filterID);
	virtual bool SetGPUPixelScalerOGL(const VideoFilterTypeID filterID);
	virtual void SetCPUPixelScalerOGL(const VideoFilterTypeID filterID);
	virtual void LoadFrameOGL(const uint32_t *frameData, GLint x, GLint y, GLsizei w, GLsizei h);
	virtual void ProcessOGL();
	virtual void RenderOGL();
};

class OGLDisplayLayer : public OGLVideoLayer
{
protected:
	bool _isVAOPresent;
	bool _canUseShaderBasedFilters;
	bool _canUseShaderOutput;
	bool _useShader150;
	ShaderSupportTier _shaderSupport;
	
	bool _needUploadVertices;
	bool _useDeposterize;
	bool _useShaderBasedPixelScaler;
	bool _filtersPreferGPU;
	int _outputFilter;
	VideoFilterTypeID _pixelScaler;
	
	OGLFilter *_filterDeposterize;
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
	
	GLsizei _vtxElementCount;
	GLubyte *_vtxElementPointer;
	
	GLint _displayTexFilter;
	GLuint _texCPUFilterDstID;
	
	GLuint _texLQ2xLUT;
	GLuint _texHQ2xLUT;
	GLuint _texHQ4xLUT;
	
	GLint vtxBuffer[4 * 8];
	GLfloat texCoordBuffer[2 * 8];
	size_t _vtxBufferOffset;
	
	GLuint _texVideoInputDataID;
	GLuint _texVideoSourceID;
	GLuint _texVideoPixelScalerID;
	GLuint _texVideoOutputID;
	GLuint _vaoMainStatesID;
	GLuint _vboVertexID;
	GLuint _vboTexCoordID;
	GLuint _vboElementID;
	
	GLint _uniformFinalOutputAngleDegrees;
	GLint _uniformFinalOutputScalar;
	GLint _uniformFinalOutputViewSize;
	
	void UploadHQnxLUTs();
	
	virtual void UploadVerticesOGL();
	virtual void UploadTexCoordsOGL();
	virtual void UploadTransformationOGL();
	
	void UpdateVertices();
	void UpdateTexCoords(GLfloat s, GLfloat t);
	
public:
	OGLDisplayLayer() {};
	OGLDisplayLayer(OGLVideoOutput *oglVO);
	virtual ~OGLDisplayLayer();
	
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
	bool GetSourceDeposterize();
	void SetSourceDeposterize(bool useDeposterize);
	
	bool CanUseShaderBasedFilters();
	void GetNormalSize(double *w, double *h);
	
	int GetOutputFilter();
	virtual void SetOutputFilterOGL(const int filterID);
	int GetPixelScaler();
	virtual void SetPixelScalerOGL(const int filterID);
	virtual bool SetGPUPixelScalerOGL(const VideoFilterTypeID filterID);
	virtual void SetCPUPixelScalerOGL(const VideoFilterTypeID filterID);
	virtual void LoadFrameOGL(const uint16_t *frameData, GLint x, GLint y, GLsizei w, GLsizei h);
	virtual void ProcessOGL();
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
		
	virtual void ProcessOGL();
	virtual void RenderOGL();
	virtual void SetViewportSizeOGL(GLsizei w, GLsizei h);
};

OGLInfo* OGLInfoCreate_Legacy();

extern OGLInfo* (*OGLInfoCreate_Func)();
extern void (*glBindVertexArrayDESMUME)(GLuint id);
extern void (*glDeleteVertexArraysDESMUME)(GLsizei n, const GLuint *ids);
extern void (*glGenVertexArraysDESMUME)(GLsizei n, GLuint *ids);

#endif // _OGLDISPLAYOUTPUT_H_
