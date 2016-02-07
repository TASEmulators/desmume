/*
	Copyright (C) 2014-2016 DeSmuME team

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

#include <ft2build.h>
#include FT_FREETYPE_H

class OGLVideoOutput;
struct NDSFrameInfo;

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
	GLsizei GetSrcWidth();
	GLsizei GetSrcHeight();
	GLsizei GetDstWidth();
	GLsizei GetDstHeight();
	virtual void SetSrcSizeOGL(GLsizei w, GLsizei h);
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
	
	virtual void SetSrcSizeOGL(GLsizei w, GLsizei h);
	virtual GLuint RunFilterOGL(GLuint srcTexID);
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
	GLuint _texHQ3xLUT;
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
	
	void UploadVerticesOGL();
	void UploadTexCoordsOGL();
	void UploadTransformationOGL();
	
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
	void GetNormalSize(double &w, double &h);
	
	int GetOutputFilter();
	virtual void SetOutputFilterOGL(const int filterID);
	int GetPixelScaler();
	void SetPixelScalerOGL(const int filterID);
	bool SetGPUPixelScalerOGL(const VideoFilterTypeID filterID);
	void SetCPUPixelScalerOGL(const VideoFilterTypeID filterID);
	void LoadFrameOGL(const uint32_t *frameData, GLint x, GLint y, GLsizei w, GLsizei h);
	void ProcessOGL();
	void RenderOGL();
};

class OGLVideoLayer
{
protected:
	OGLVideoOutput *_output;
	bool _isVisible;
	GLsizei _viewportWidth;
	GLsizei _viewportHeight;
	
public:
	virtual ~OGLVideoLayer() {};
	
	virtual bool IsVisible();
	virtual void SetVisibility(const bool visibleState);
	virtual void SetViewportSizeOGL(GLsizei w, GLsizei h);
	
	virtual void ProcessOGL() = 0;
	virtual void RenderOGL() = 0;
	virtual void FinishOGL() {};
};

typedef struct
{
	uint32_t width;
	GLfloat texCoord[8];
} GlyphInfo;

class OGLHUDLayer : public OGLVideoLayer
{
protected:
	FT_Library _ftLibrary;
	GLuint _texCharMap;	
	GlyphInfo _glyphInfo[256];
	
	std::string _statusString;
	size_t _glyphSize;
	size_t _glyphTileSize;
	
	OGLShaderProgram *_program;
	
	GLint _vtxBuffer[4096 * (2 * 4)];
	GLfloat _texCoordBuffer[4096 * (2 * 4)];
	GLshort _idxBuffer[4096 * 6];
	
	bool _isVAOPresent;
	bool _canUseShaderOutput;
	
	GLint _uniformViewSize;
	
	GLuint _vaoMainStatesID;
	GLuint _vboVertexID;
	GLuint _vboTexCoordID;
	GLuint _vboElementID;
	
	bool _showVideoFPS;
	bool _showRender3DFPS;
	bool _showFrameIndex;
	bool _showLagFrameCount;
	bool _showCPULoadAverage;
	bool _showRTC;
	
	uint32_t _lastVideoFPS;
	uint32_t _lastRender3DFPS;
	uint32_t _lastFrameIndex;
	uint32_t _lastLagFrameCount;
	uint32_t _lastCpuLoadAvgARM9;
	uint32_t _lastCpuLoadAvgARM7;
	char _lastRTCString[25];
	
	GLint _textBoxLines;
	GLint _textBoxWidth;
	
	void _SetShowInfoItemOGL(bool &infoItemFlag, const bool visibleState);
	void _ProcessVerticesOGL();
	
public:
	OGLHUDLayer(OGLVideoOutput *oglVO);
	virtual ~OGLHUDLayer();
	
	void SetFontUsingPath(const char *filePath);
	
	void SetInfo(const uint32_t videoFPS, const uint32_t render3DFPS, const uint32_t frameIndex, const uint32_t lagFrameCount, const char *rtcString, const uint32_t cpuLoadAvgARM9, const uint32_t cpuLoadAvgARM7);
	void RefreshInfo();
	
	void SetShowVideoFPS(const bool visibleState);
	bool GetShowVideoFPS() const;
	void SetShowRender3DFPS(const bool visibleState);
	bool GetShowRender3DFPS() const;
	void SetShowFrameIndex(const bool visibleState);
	bool GetShowFrameIndex() const;
	void SetShowLagFrameCount(const bool visibleState);
	bool GetShowLagFrameCount() const;
	void SetShowCPULoadAverage(const bool visibleState);
	bool GetShowCPULoadAverage() const;
	void SetShowRTC(const bool visibleState);
	bool GetShowRTC() const;
	
	virtual void SetViewportSizeOGL(GLsizei w, GLsizei h);
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
	
	OGLShaderProgram *_finalOutputProgram;
	OGLFilter *_filterDeposterize[2];
	OGLFilter *_shaderFilter[2];
	GLint _displayTexFilter[2];
	
	bool _isTexVideoInputDataNative[2];
	GLuint _texVideoInputDataNativeID[2];
	GLuint _texVideoInputDataCustomID[2];
	GLuint _texVideoOutputID[2];
	GLfloat _texLoadedWidth[2];
	GLfloat _texLoadedHeight[2];
	
	uint16_t *_videoSrcBufferHead;
	size_t _videoSrcBufferSize;
	uint16_t *_videoSrcNativeBuffer[2];
	uint16_t *_videoSrcCustomBuffer[2];
	GLsizei _videoSrcCustomBufferWidth[2];
	GLsizei _videoSrcCustomBufferHeight[2];
	
	GLuint _fenceTexUploadNativeID[2];
	GLuint _fenceTexUploadCustomID[2];
	
	uint32_t *_vfMasterDstBuffer;
	size_t _vfMasterDstBufferSize;
	VideoFilter *_vf[2];
	GLuint _texCPUFilterDstID[2];
	
	uint16_t _displayWidth;
	uint16_t _displayHeight;
	int _displayMode;
	int _displayOrder;
	int _displayOrientation;
	double _normalWidth;
	double _normalHeight;
	GLfloat _gapScalar;
	GLfloat _rotation;
	
	GLuint _texLQ2xLUT;
	GLuint _texHQ2xLUT;
	GLuint _texHQ3xLUT;
	GLuint _texHQ4xLUT;
	
	GLint vtxBuffer[4 * 8];
	GLfloat texCoordBuffer[2 * 8];
	size_t _vtxBufferOffset;
	
	GLuint _vaoMainStatesID;
	GLuint _vboVertexID;
	GLuint _vboTexCoordID;
	GLuint _vboElementID;
	
	GLint _uniformFinalOutputAngleDegrees;
	GLint _uniformFinalOutputScalar;
	GLint _uniformFinalOutputViewSize;
	
	void UploadHQnxLUTs();
	void DetermineTextureStorageHints(GLint &videoSrcTexStorageHint, GLint &cpuFilterTexStorageHint);
	
	void ResizeCPUPixelScalerOGL(const size_t srcWidthMain, const size_t srcHeightMain, const size_t srcWidthTouch, const size_t srcHeightTouch, const size_t scaleMultiply, const size_t scaleDivide);
	void UploadVerticesOGL();
	void UploadTexCoordsOGL();
	void UploadTransformationOGL();
	
	void UpdateVertices();
	void UpdateTexCoords(GLfloat s0, GLfloat t0, GLfloat s1, GLfloat t1);
	
public:
	OGLDisplayLayer() {};
	OGLDisplayLayer(OGLVideoOutput *oglVO);
	virtual ~OGLDisplayLayer();
	
	void SetVideoBuffers(const void *videoBufferHead,
						 const void *nativeBuffer0,
						 const void *nativeBuffer1,
						 const void *customBuffer0, const size_t customWidth0, const size_t customHeight0,
						 const void *customBuffer1, const size_t customWidth1, const size_t customHeight1);
	
	bool GetFiltersPreferGPU();
	void SetFiltersPreferGPUOGL(bool preferGPU);
	
	uint16_t GetDisplayWidth();
	uint16_t GetDisplayHeight();
	void SetDisplaySize(uint16_t w, uint16_t h);
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
	void GetNormalSize(double &w, double &h);
	
	int GetOutputFilter();
	virtual void SetOutputFilterOGL(const int filterID);
	int GetPixelScaler();
	void SetPixelScalerOGL(const int filterID);
	bool SetGPUPixelScalerOGL(const VideoFilterTypeID filterID);
	void SetCPUPixelScalerOGL(const VideoFilterTypeID filterID);
	void LoadFrameOGL(bool isMainSizeNative, bool isTouchSizeNative);
	
	virtual void ProcessOGL();
	virtual void RenderOGL();
	virtual void FinishOGL();
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
	
	void InitLayers();
	OGLInfo* GetInfo();
	GLsizei GetViewportWidth();
	GLsizei GetViewportHeight();
	OGLDisplayLayer* GetDisplayLayer();
	OGLHUDLayer* GetHUDLayer();
		
	void ProcessOGL();
	void RenderOGL();
	void SetViewportSizeOGL(GLsizei w, GLsizei h);
	void FinishOGL();
};

OGLInfo* OGLInfoCreate_Legacy();

extern OGLInfo* (*OGLInfoCreate_Func)();
extern void (*glBindVertexArrayDESMUME)(GLuint id);
extern void (*glDeleteVertexArraysDESMUME)(GLsizei n, const GLuint *ids);
extern void (*glGenVertexArraysDESMUME)(GLsizei n, GLuint *ids);

#endif // _OGLDISPLAYOUTPUT_H_
