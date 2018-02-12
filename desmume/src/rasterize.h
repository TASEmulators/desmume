/*
	Copyright (C) 2009-2018 DeSmuME team

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

#ifndef _RASTERIZE_H_
#define _RASTERIZE_H_

#include "render3D.h"
#include "gfx3d.h"

#define SOFTRASTERIZER_MAX_THREADS 32

extern GPU3DInterface gpu3DRasterize;

class Task;
class SoftRasterizerRenderer;
struct edge_fx_fl;

struct SoftRasterizerClearParam
{
	SoftRasterizerRenderer *renderer;
	size_t startPixel;
	size_t endPixel;
};

struct SoftRasterizerPostProcessParams
{
	SoftRasterizerRenderer *renderer;
	size_t startLine;
	size_t endLine;
	bool enableEdgeMarking;
	bool enableFog;
	u32 fogColor;
	bool fogAlphaOnly;
};

class SoftRasterizerTexture : public Render3DTexture
{
private:
	void _clamp(s32 &val, const int size, const s32 sizemask) const;
	void _hclamp(s32 &val) const;
	void _vclamp(s32 &val) const;
	void _repeat(s32 &val, const int size, const s32 sizemask) const;
	void _hrepeat(s32 &val) const;
	void _vrepeat(s32 &val) const;
	void _flip(s32 &val, const int size, const s32 sizemask) const;
	void _hflip(s32 &val) const;
	void _vflip(s32 &val) const;
	
protected:
	u32 *_unpackData;
	u32 *_customBuffer;
	
	u32 *_renderData;
	s32 _renderWidth;
	s32 _renderHeight;
	s32 _renderWidthMask;
	s32 _renderHeightMask;
	u32 _renderWidthShift;
	
public:
	SoftRasterizerTexture(TEXIMAGE_PARAM texAttributes, u32 palAttributes);
	virtual ~SoftRasterizerTexture();
	
	virtual void Load();
	
	u32* GetUnpackData();
	
	u32* GetRenderData();
	s32 GetRenderWidth() const;
	s32 GetRenderHeight() const;
	s32 GetRenderWidthMask() const;
	s32 GetRenderHeightMask() const;
	u32 GetRenderWidthShift() const;
	
	void GetRenderSamplerCoordinates(const u8 wrapMode, s32 &iu, s32 &iv) const;
	
	void SetUseDeposterize(bool willDeposterize);
	void SetScalingFactor(size_t scalingFactor);
};

template <bool RENDERER>
class RasterizerUnit
{
protected:
	bool _debug_thisPoly;
	u32 _SLI_Mask;
	u32 _SLI_Value;
	
	SoftRasterizerRenderer *_softRender;
	SoftRasterizerTexture *_currentTexture;
	VERT *_verts[MAX_CLIPPED_VERTS];
	size_t _polynum;
	u8 _textureWrapMode;
	
	Render3DError _SetupTexture(const POLY &thePoly, size_t polyRenderIndex);
	FORCEINLINE FragmentColor _sample(const float u, const float v);
	FORCEINLINE float _round_s(double val);
	
	template<bool ISSHADOWPOLYGON> FORCEINLINE void _shade(const PolygonMode polygonMode, const FragmentColor src, FragmentColor &dst, const float texCoordU, const float texCoordV);
	template<bool ISSHADOWPOLYGON> FORCEINLINE void _pixel(const POLYGON_ATTR polyAttr, const bool isTranslucent, const size_t fragmentIndex, FragmentColor &dstColor, float r, float g, float b, float invu, float invv, float w, float z);
	template<bool ISSHADOWPOLYGON, bool USELINEHACK> FORCEINLINE void _drawscanline(const POLYGON_ATTR polyAttr, const bool isTranslucent, FragmentColor *dstColor, const size_t framebufferWidth, const size_t framebufferHeight, edge_fx_fl *pLeft, edge_fx_fl *pRight);
	template<bool SLI, bool ISSHADOWPOLYGON, bool USELINEHACK, bool ISHORIZONTAL> void _runscanlines(const POLYGON_ATTR polyAttr, const bool isTranslucent, FragmentColor *dstColor, const size_t framebufferWidth, const size_t framebufferHeight, edge_fx_fl *left, edge_fx_fl *right);
	
	template<int TYPE> FORCEINLINE void _rot_verts();
	template<bool ISBACKWARDS, int TYPE> void _sort_verts();
	template<bool SLI, bool ISBACKWARDS, bool ISSHADOWPOLYGON, bool USELINEHACK> void _shape_engine(const POLYGON_ATTR polyAttr, const bool isTranslucent, FragmentColor *dstColor, const size_t framebufferWidth, const size_t framebufferHeight, int type);
	
public:
	void SetSLI(u32 value, u32 mask, bool debug);
	void SetRenderer(SoftRasterizerRenderer *theRenderer);
	template<bool SLI, bool USELINEHACK> FORCEINLINE void Render();
};

#if defined(ENABLE_SSE2)
class SoftRasterizerRenderer : public Render3D_SSE2
#else
class SoftRasterizerRenderer : public Render3D
#endif
{
protected:
	Task *_task;
	SoftRasterizerClearParam _threadClearParam[SOFTRASTERIZER_MAX_THREADS];
	SoftRasterizerPostProcessParams _threadPostprocessParam[SOFTRASTERIZER_MAX_THREADS];
	
	RasterizerUnit<true> _rasterizerUnit[SOFTRASTERIZER_MAX_THREADS];
	RasterizerUnit<false> _HACK_viewer_rasterizerUnit;
	
	size_t _threadCount;
	size_t _nativeLinesPerThread;
	size_t _nativePixelsPerThread;
	size_t _customLinesPerThread;
	size_t _customPixelsPerThread;
	
	FragmentColor _clearColor6665;
	FragmentAttributes _clearAttributes;
	
	GFX3D_Clipper clipper;
	u8 fogTable[32768];
	FragmentColor edgeMarkTable[8];
	bool edgeMarkDisabled[8];
	
	bool _renderGeometryNeedsFinish;
	
	bool _enableHighPrecisionColorInterpolation;
	bool _enableLineHack;
	
	// SoftRasterizer-specific methods
	virtual Render3DError InitTables();
	
	template<bool USEHIRESINTERPOLATE> size_t performClipping(const VERT *vertList, const POLYLIST *polyList, const INDEXLIST *indexList);
	
	// Base rendering methods
	virtual Render3DError BeginRender(const GFX3D &engine);
	virtual Render3DError RenderGeometry(const GFX3D_State &renderState, const POLYLIST *polyList, const INDEXLIST *indexList);
	virtual Render3DError RenderEdgeMarking(const u16 *colorTable, const bool useAntialias);
	virtual Render3DError RenderFog(const u8 *densityTable, const u32 color, const u32 offset, const u8 shift, const bool alphaOnly);
	virtual Render3DError EndRender(const u64 frameCount);
	
	virtual Render3DError ClearUsingImage(const u16 *__restrict colorBuffer, const u32 *__restrict depthBuffer, const u8 *__restrict fogBuffer, const u8 *__restrict polyIDBuffer);
	virtual Render3DError ClearUsingValues(const FragmentColor &clearColor6665, const FragmentAttributes &clearAttributes);
	
public:
	int _debug_drawClippedUserPoly;
	size_t _clippedPolyCount;
	FragmentColor toonColor32LUT[32];
	GFX3D_Clipper::TClippedPoly *clippedPolys;
	FragmentAttributesBuffer *_framebufferAttributes;
	bool polyVisible[POLYLIST_SIZE];
	bool polyBackfacing[POLYLIST_SIZE];
	GFX3D_State *currentRenderState;
	
	bool _enableFragmentSamplingHack;
	
	SoftRasterizerRenderer();
	virtual ~SoftRasterizerRenderer();
	
	void performViewportTransforms();
	void performBackfaceTests();
	void performCoordAdjustment();
	void GetAndLoadAllTextures();
	Render3DError UpdateEdgeMarkColorTable(const u16 *edgeMarkColorTable);
	Render3DError UpdateFogTable(const u8 *fogDensityTable);
	Render3DError RenderEdgeMarkingAndFog(const SoftRasterizerPostProcessParams &param);
	
	SoftRasterizerTexture* GetLoadedTextureFromPolygon(const POLY &thePoly, bool enableTexturing);
	
	// Base rendering methods
	virtual Render3DError UpdateToonTable(const u16 *toonTableBuffer);
	virtual Render3DError Reset();
	virtual Render3DError ApplyRenderingSettings(const GFX3D_State &renderState);
	virtual Render3DError Render(const GFX3D &engine);
	virtual Render3DError RenderFinish();
	virtual Render3DError RenderFlush(bool willFlushBuffer32, bool willFlushBuffer16);
	virtual void ClearUsingValuesLoop(const size_t startPixel, const size_t endPixel);
	virtual Render3DError SetFramebufferSize(size_t w, size_t h);
};

#ifdef ENABLE_SSE2

class SoftRasterizerRenderer_SSE2 : public SoftRasterizerRenderer
{
protected:
	v128u32 _clearColor_v128u32;
	v128u32 _clearDepth_v128u32;
	v128u8 _clearAttrOpaquePolyID_v128u8;
	v128u8 _clearAttrTranslucentPolyID_v128u8;
	v128u8 _clearAttrStencil_v128u8;
	v128u8 _clearAttrIsFogged_v128u8;
	v128u8 _clearAttrIsTranslucentPoly_v128u8;
	
	virtual Render3DError ClearUsingValues(const FragmentColor &clearColor6665, const FragmentAttributes &clearAttributes);
	
public:
	SoftRasterizerRenderer_SSE2();
	
	virtual void ClearUsingValuesLoop(const size_t startPixel, const size_t endPixel);
	virtual Render3DError SetFramebufferSize(size_t w, size_t h);
};

#endif

#endif // _RASTERIZE_H_
