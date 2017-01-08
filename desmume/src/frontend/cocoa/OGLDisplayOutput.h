/*
	Copyright (C) 2014-2017 DeSmuME team

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
	#include <OpenGL/OpenGL.h>
#endif

#endif // _OGLDISPLAYOUTPUT_3_2_H_

#include <set>
#include <string>
#include "../../filter/videofilter.h"

#include "ClientDisplayView.h"

#define HUD_VERTEX_ATTRIBUTE_BUFFER_SIZE (sizeof(GLfloat) * HUD_MAX_CHARACTERS * (2 * 4))

class OGLVideoOutput;
struct NDSFrameInfo;

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
	bool _isVAOSupported;
	bool _isPBOSupported;
	bool _isFBOSupported;
	
public:
	OGLInfo();
	virtual ~OGLInfo() {};
		
	bool IsUsingShader150();
	bool IsVBOSupported();
	bool IsVAOSupported();
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
	void GetNormalSize(double &w, double &h) const;
	
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
	bool _needUpdateRotationScale;
	bool _needUpdateVertices;
	
public:
	virtual ~OGLVideoLayer() {};
	
	void SetNeedsUpdateVertices();
	
	virtual bool IsVisible();
	virtual void SetVisibility(const bool visibleState);
	
	virtual void UpdateViewportOGL() {};
	virtual void ProcessOGL() = 0;
	virtual void RenderOGL() = 0;
	virtual void FinishOGL() {};
};

class OGLHUDLayer : public OGLVideoLayer
{
protected:
	OGLShaderProgram *_program;
	GLint _uniformViewSize;
	
	GLuint _vaoMainStatesID;
	GLuint _vboVertexID;
	GLuint _vboTexCoordID;
	GLuint _vboElementID;
	GLuint _texCharMap;
	
	GlyphInfo *_glyphInfo;
	GLfloat _glyphSize;
	GLfloat _glyphTileSize;
	
	void _SetShowInfoItemOGL(bool &infoItemFlag, const bool visibleState);
	void _UpdateVerticesOGL();
	
public:
	OGLHUDLayer(OGLVideoOutput *oglVO);
	virtual ~OGLHUDLayer();
	
	void CopyHUDFont(const FT_Face &fontFace, const size_t glyphSize, const size_t glyphTileSize, GlyphInfo *glyphInfo);
	
	virtual void UpdateViewportOGL();
	virtual void ProcessOGL();
	virtual void RenderOGL();
};

class OGLDisplayLayer : public OGLVideoLayer
{
protected:
	bool _useShader150;
	ShaderSupportTier _shaderSupport;
	GLboolean _useClientStorage;
	
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
	
	GLenum _videoColorFormat;
	const void *_videoSrcBufferHead;
	const void *_videoSrcNativeBuffer[2];
	const void *_videoSrcCustomBuffer[2];
	size_t _videoSrcBufferSize;
	GLsizei _videoSrcCustomBufferWidth[2];
	GLsizei _videoSrcCustomBufferHeight[2];
		
	uint32_t *_vfMasterDstBuffer;
	size_t _vfMasterDstBufferSize;
	VideoFilter *_vf[2];
	GLuint _texCPUFilterDstID[2];
	
	GLuint _texLQ2xLUT;
	GLuint _texHQ2xLUT;
	GLuint _texHQ3xLUT;
	GLuint _texHQ4xLUT;
	
	GLuint _vaoMainStatesID;
	GLuint _vboVertexID;
	GLuint _vboTexCoordID;
	
	GLint _uniformFinalOutputAngleDegrees;
	GLint _uniformFinalOutputScalar;
	GLint _uniformFinalOutputViewSize;
	
	void UploadHQnxLUTs();
	void DetermineTextureStorageHints(GLint &videoSrcTexStorageHint, GLint &cpuFilterTexStorageHint);
	
	void ResizeCPUPixelScalerOGL(const size_t srcWidthMain, const size_t srcHeightMain, const size_t srcWidthTouch, const size_t srcHeightTouch, const size_t scaleMultiply, const size_t scaleDivide);
	
	void _UpdateRotationScaleOGL();
	void _UpdateVerticesOGL();
	
public:
	OGLDisplayLayer() {};
	OGLDisplayLayer(OGLVideoOutput *oglVO);
	virtual ~OGLDisplayLayer();
	
	void SetVideoBuffers(const uint32_t colorFormat,
						 const void *videoBufferHead,
						 const void *nativeBuffer0,
						 const void *nativeBuffer1,
						 const void *customBuffer0, const size_t customWidth0, const size_t customHeight0,
						 const void *customBuffer1, const size_t customWidth1, const size_t customHeight1);
	
	void SetNeedsUpdateRotationScale();
	void SetFiltersPreferGPUOGL();
	
	bool CanUseShaderBasedFilters();
	
	OutputFilterTypeID SetOutputFilterOGL(const OutputFilterTypeID filterID);
	bool SetGPUPixelScalerOGL(const VideoFilterTypeID filterID);
	void SetCPUPixelScalerOGL(const VideoFilterTypeID filterID);
	void LoadFrameOGL(bool isMainSizeNative, bool isTouchSizeNative);
	
	virtual void UpdateViewportOGL();
	virtual void ProcessOGL();
	virtual void RenderOGL();
	virtual void FinishOGL();
};

class OGLVideoOutput : public ClientDisplay3DView
{
protected:
	OGLInfo *_info;
	GLsizei _viewportWidth;
	GLsizei _viewportHeight;
	bool _needUpdateViewport;
	std::vector<OGLVideoLayer *> *_layerList;
	
	void _UpdateViewport();
	
	virtual void _UpdateNormalSize();
	virtual void _UpdateOrder();
	virtual void _UpdateRotation();
	virtual void _UpdateClientSize();
	virtual void _UpdateViewScale();
	
public:
	OGLVideoOutput();
	~OGLVideoOutput();
	
	OGLInfo* GetInfo();
	
	GLsizei GetViewportWidth();
	GLsizei GetViewportHeight();
	OGLDisplayLayer* GetDisplayLayer();
	OGLHUDLayer* GetHUDLayer();
	
	virtual void Init();
	
	virtual void SetOutputFilter(const OutputFilterTypeID filterID);
	virtual void SetPixelScaler(const VideoFilterTypeID filterID);
	
	virtual void CopyHUDFont(const FT_Face &fontFace, const size_t glyphSize, const size_t glyphTileSize, GlyphInfo *glyphInfo);
	virtual void SetHUDVisibility(const bool visibleState);
	
	virtual void SetFiltersPreferGPU(const bool preferGPU);
	
	virtual void SetVideoBuffers(const uint32_t colorFormat,
								 const void *videoBufferHead,
								 const void *nativeBuffer0,
								 const void *nativeBuffer1,
								 const void *customBuffer0, const size_t customWidth0, const size_t customHeight0,
								 const void *customBuffer1, const size_t customWidth1, const size_t customHeight1);
	
	virtual void FrameLoadGPU(bool isMainSizeNative, bool isTouchSizeNative);
	virtual void FrameProcessGPU();
	virtual void FrameProcessHUD();
	virtual void FrameRender();
	virtual void FrameFinish();
};

class MacOGLDisplayView : public OGLVideoOutput
{
protected:
	CGLContextObj _context;
	
	void _FrameRenderAndFlush();
	
public:
	MacOGLDisplayView(CGLContextObj context);
	
	CGLContextObj GetContext() const;
	void SetContext(CGLContextObj context);
	
	virtual void SetVideoBuffers(const uint32_t colorFormat,
								 const void *videoBufferHead,
								 const void *nativeBuffer0,
								 const void *nativeBuffer1,
								 const void *customBuffer0, const size_t customWidth0, const size_t customHeight0,
								 const void *customBuffer1, const size_t customWidth1, const size_t customHeight1);
	
	virtual void SetUseVerticalSync(const bool useVerticalSync);
	virtual void SetScaleFactor(const double scaleFactor);
	
	virtual void SetupViewProperties();
	
	virtual void SetFiltersPreferGPU(const bool preferGPU);
	virtual void SetOutputFilter(const OutputFilterTypeID filterID);
	virtual void SetPixelScaler(const VideoFilterTypeID filterID);
	
	virtual void SetHUDVisibility(const bool visibleState);
	virtual void SetHUDShowVideoFPS(const bool visibleState);
	virtual void SetHUDShowRender3DFPS(const bool visibleState);
	virtual void SetHUDShowFrameIndex(const bool visibleState);
	virtual void SetHUDShowLagFrameCount(const bool visibleState);
	virtual void SetHUDShowCPULoadAverage(const bool visibleState);
	virtual void SetHUDShowRTC(const bool visibleState);
	
	virtual void FrameFinish();
	virtual void HandleGPUFrameEndEvent(const bool isMainSizeNative, const bool isTouchSizeNative);
	virtual void HandleEmulatorFrameEndEvent(const NDSFrameInfo &frameInfo);
	
	virtual void UpdateView();
};

OGLInfo* OGLInfoCreate_Legacy();

extern OGLInfo* (*OGLInfoCreate_Func)();
extern void (*glBindVertexArrayDESMUME)(GLuint id);
extern void (*glDeleteVertexArraysDESMUME)(GLsizei n, const GLuint *ids);
extern void (*glGenVertexArraysDESMUME)(GLsizei n, GLuint *ids);

#endif // _OGLDISPLAYOUTPUT_H_
