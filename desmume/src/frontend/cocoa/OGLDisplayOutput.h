/*
	Copyright (C) 2014-2018 DeSmuME team

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
#include <pthread.h>
#include "../../filter/videofilter.h"

#include "ClientDisplayView.h"

#define OPENGL_FETCH_BUFFER_COUNT	2

class OGLVideoOutput;

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

struct OGLProcessedFrameInfo
{
	uint8_t bufferIndex;
	GLuint texID[2];
	bool isMainDisplayProcessed;
	bool isTouchDisplayProcessed;
};
typedef struct OGLProcessedFrameInfo OGLProcessedFrameInfo;

class OGLContextInfo
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
	OGLContextInfo();
	virtual ~OGLContextInfo() {};
		
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

class OGLContextInfo_Legacy : public OGLContextInfo
{
public:
	OGLContextInfo_Legacy();
	
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
	void SetVertexShaderOGL(const char *shaderProgram, bool useVtxColors, bool useShader150);
	GLuint GetFragmentShaderID();
	void SetFragmentShaderOGL(const char *shaderProgram, bool useShader150);
	void SetVertexAndFragmentShaderOGL(const char *vertShaderProgram, const char *fragShaderProgram, bool useVtxColors, bool useShader150);
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
	virtual void SetSrcSizeOGL(GLsizei w, GLsizei h);
	GLfloat GetScale();
	void SetScaleOGL(GLfloat scale, void *buffer);
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
	
	GLint _uniformAngleDegrees;
	GLint _uniformScalar;
	GLint _uniformViewSize;
	GLint _uniformRenderFlipped;
	GLint _uniformBacklightIntensity;
	
	void UploadVerticesOGL();
	void UploadTexCoordsOGL();
	void UploadTransformationOGL();
	
	void UpdateVertices();
	void UpdateTexCoords(GLfloat s, GLfloat t);
	
public:
	OGLImage() {};
	OGLImage(OGLContextInfo *oglInfo, GLsizei imageWidth, GLsizei imageHeight, GLsizei viewportWidth, GLsizei viewportHeight);
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
	bool _needUpdateViewport;
	bool _needUpdateRotationScale;
	bool _needUpdateVertices;
	
	GLint _uniformAngleDegrees;
	GLint _uniformScalar;
	GLint _uniformViewSize;
	GLint _uniformRenderFlipped;
	GLint _uniformBacklightIntensity;
	
public:
	virtual ~OGLVideoLayer() {};
	
	void SetNeedsUpdateViewport();
	void SetNeedsUpdateRotationScale();
	void SetNeedsUpdateVertices();
	
	virtual bool IsVisible();
	virtual void SetVisibility(const bool visibleState);
	
	virtual void RenderOGL(bool isRenderingFlipped) = 0;
};

class OGLHUDLayer : public OGLVideoLayer
{
protected:
	OGLShaderProgram *_program;
	
	GLuint _vaoMainStatesID;
	GLuint _vboPositionVertexID;
	GLuint _vboColorVertexID;
	GLuint _vboTexCoordID;
	GLuint _vboElementID;
	GLuint _texCharMap;
	
	void _UpdateVerticesOGL();
	
public:
	OGLHUDLayer(OGLVideoOutput *oglVO);
	virtual ~OGLHUDLayer();
	
	void CopyHUDFont(const FT_Face &fontFace, const size_t glyphSize, const size_t glyphTileSize, GlyphInfo *glyphInfo);
	
	virtual void RenderOGL(bool isRenderingFlipped);
};

class OGLDisplayLayer : public OGLVideoLayer
{
protected:
	OGLShaderProgram *_finalOutputProgram;
	OGLFilter *_filterDeposterize[2];
	OGLFilter *_shaderFilter[2];
	GLint _displayTexFilter[2];
	pthread_rwlock_t _cpuFilterRWLock[2][2];
	
	GLuint _vaoMainStatesID;
	GLuint _vboVertexID;
	GLuint _vboTexCoordID;
	
	void _UpdateRotationScaleOGL();
	void _UpdateVerticesOGL();
	
public:
	OGLDisplayLayer() {};
	OGLDisplayLayer(OGLVideoOutput *oglVO);
	virtual ~OGLDisplayLayer();
	
	OutputFilterTypeID SetOutputFilterOGL(const OutputFilterTypeID filterID);
	bool SetGPUPixelScalerOGL(const VideoFilterTypeID filterID);
	
	void LoadNativeDisplayByID_OGL(const NDSDisplayID displayID);
	void ProcessOGL();
	
	virtual void RenderOGL(bool isRenderingFlipped);
};

class OGLClientFetchObject : public GPUClientFetchObject
{
protected:
	OGLContextInfo *_contextInfo;
	GLenum _fetchColorFormatOGL;
	GLuint _texDisplayFetchNative[2][OPENGL_FETCH_BUFFER_COUNT];
	GLuint _texDisplayFetchCustom[2][OPENGL_FETCH_BUFFER_COUNT];
	
	GLuint _texLQ2xLUT;
	GLuint _texHQ2xLUT;
	GLuint _texHQ3xLUT;
	GLuint _texHQ4xLUT;
	
	GLuint _texFetch[2];
	
	bool _useDirectToCPUFilterPipeline;
	uint32_t *_srcNativeCloneMaster;
	uint32_t *_srcNativeClone[2][OPENGL_FETCH_BUFFER_COUNT];
	pthread_rwlock_t _srcCloneRWLock[2][OPENGL_FETCH_BUFFER_COUNT];
	pthread_rwlock_t _texFetchRWLock[2];
	bool _srcCloneNeedsUpdate[2][OPENGL_FETCH_BUFFER_COUNT];
	
	virtual void _FetchNativeDisplayByID(const NDSDisplayID displayID, const u8 bufferIndex);
	virtual void _FetchCustomDisplayByID(const NDSDisplayID displayID, const u8 bufferIndex);
	
public:
	OGLClientFetchObject();
	virtual ~OGLClientFetchObject();
	
	OGLContextInfo* GetContextInfo() const;
	uint32_t* GetSrcClone(const NDSDisplayID displayID, const u8 bufferIndex) const;
	GLuint GetTexNative(const NDSDisplayID displayID, const u8 bufferIndex) const;
	GLuint GetTexCustom(const NDSDisplayID displayID, const u8 bufferIndex) const;
	
	// For lack of a better place, we're putting the HQnx LUTs in the fetch object because
	// we know that it will be shared for all display views.
	GLuint GetTexLQ2xLUT() const;
	GLuint GetTexHQ2xLUT() const;
	GLuint GetTexHQ3xLUT() const;
	GLuint GetTexHQ4xLUT() const;
	
	void CopyFromSrcClone(uint32_t *dstBufferPtr, const NDSDisplayID displayID, const u8 bufferIndex);
	void FetchNativeDisplayToSrcClone(const NDSDisplayID displayID, const u8 bufferIndex, bool needsLock);
	void FetchTextureWriteLock(const NDSDisplayID displayID);
	void FetchTextureReadLock(const NDSDisplayID displayID);
	void FetchTextureUnlock(const NDSDisplayID displayID);
	
	virtual void Init();
	virtual void SetFetchBuffers(const NDSDisplayInfo &currentDisplayInfo);
	virtual void FetchFromBufferIndex(const u8 index);
	
	virtual GLuint GetFetchTexture(const NDSDisplayID displayID);
	virtual void SetFetchTexture(const NDSDisplayID displayID, GLuint texID);
};

class OGLVideoOutput : public ClientDisplay3DPresenter
{
protected:
	OGLContextInfo *_contextInfo;
	GLsizei _viewportWidth;
	GLsizei _viewportHeight;
	bool _needUpdateViewport;
	bool _hasOGLPixelScaler;
	
	GLuint _texCPUFilterDstID[2];
	GLuint _fboFrameCopyID;
	
	OGLProcessedFrameInfo _processedFrameInfo;
	
	std::vector<OGLVideoLayer *> *_layerList;
	
	void _UpdateViewport();
	
	virtual void _UpdateNormalSize();
	virtual void _UpdateOrder();
	virtual void _UpdateRotation();
	virtual void _UpdateClientSize();
	virtual void _UpdateViewScale();
	
	virtual void _LoadNativeDisplayByID(const NDSDisplayID displayID);
	virtual void _ResizeCPUPixelScaler(const VideoFilterTypeID filterID);
	
public:
	OGLVideoOutput();
	~OGLVideoOutput();
	
	OGLContextInfo* GetContextInfo();
	
	GLsizei GetViewportWidth();
	GLsizei GetViewportHeight();
	OGLDisplayLayer* GetDisplayLayer();
	OGLHUDLayer* GetHUDLayer();
	
	virtual void Init();
	
	// NDS screen filters
	virtual void SetOutputFilter(const OutputFilterTypeID filterID);
	virtual void SetPixelScaler(const VideoFilterTypeID filterID);
	
	virtual void CopyHUDFont(const FT_Face &fontFace, const size_t glyphSize, const size_t glyphTileSize, GlyphInfo *glyphInfo);
	virtual void SetHUDVisibility(const bool visibleState);
	
	virtual void SetFiltersPreferGPU(const bool preferGPU);
	
	GLuint GetTexCPUFilterDstID(const NDSDisplayID displayID) const;
	
	// Client view interface
	virtual void ProcessDisplays();
	virtual void CopyFrameToBuffer(uint32_t *dstBuffer);
	virtual void RenderFrameOGL(bool isRenderingFlipped);
	
	virtual const OGLProcessedFrameInfo& GetProcessedFrameInfo();
	virtual void SetProcessedFrameInfo(const OGLProcessedFrameInfo &processedInfo);
	
	virtual void WriteLockEmuFramebuffer(const uint8_t bufferIndex);
	virtual void ReadLockEmuFramebuffer(const uint8_t bufferIndex);
	virtual void UnlockEmuFramebuffer(const uint8_t bufferIndex);
};

extern void (*glBindVertexArrayDESMUME)(GLuint id);
extern void (*glDeleteVertexArraysDESMUME)(GLsizei n, const GLuint *ids);
extern void (*glGenVertexArraysDESMUME)(GLsizei n, GLuint *ids);

#endif // _OGLDISPLAYOUTPUT_H_
