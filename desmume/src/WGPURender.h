/*
	Copyright (C) 2026 DeSmuME team

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

#ifndef WGPURENDER_H
#define WGPURENDER_H

#include "types.h"
#include "render3D.h"

// The WebGPU 3D renderer. It lives behind the ENABLE_WGPU compile-time switch so
// that ports which don't ship wgpu-native keep building without it. When the
// switch is off, gpu3DWGPU still exists but reports itself as unavailable, which
// lets frontends list it unconditionally and fall back gracefully at runtime.
extern GPU3DInterface gpu3DWGPU;

#if defined(ENABLE_WGPU)

// Enumerates the adapters wgpu-native can use and builds one renderer entry per
// distinct hardware backend (D3D12, Vulkan, ...), so a frontend can list
// "WebGPU - D3D12" and "WebGPU - Vulkan" as separate choices. CPU/software
// adapters are skipped on purpose. Cheap: it only queries adapters, never
// creates a device. Call once before building the renderer list.
void WGPURender_InitBackendList();

// Number of entries produced by WGPURender_InitBackendList(). Zero means that
// nothing usable was found, in which case the frontend should fall back to
// gpu3DWGPU, which lets wgpu-native pick whatever it wants.
size_t WGPURender_GetBackendCount();

// The renderer entry for backend <index>. Never NULL for index < count.
GPU3DInterface *WGPURender_GetBackendInterface(const size_t index);

// The instance backend mask (WGPUInstanceBackend bits) behind entry <index>.
// Frontends that offer the same backend choice outside the 3D renderer -- the
// WebGPU display method, for instance -- pin their instance with it. Zero for an
// out-of-range index, which is also the "let wgpu-native pick" value.
uint32_t WGPURender_GetBackendMask(const size_t index);

// Short backend name ("D3D12", "Vulkan", ...) behind entry <index>, without the
// "WebGPU - " prefix that the renderer names carry. NULL for an out-of-range
// index. Used where the surrounding label already says WebGPU, such as a
// "WebGPU" submenu in the display method menu.
const char *WGPURender_GetBackendLabel(const size_t index);

#include <map>
#include <vector>

#include "webgpu.h"
#include "utils/colorspacehandler/colorspacehandler.h"

class WGPURender;

// Render target that reads the renderer's color texture back to the CPU. The
// emulator's compositor works on CPU memory, so every flushed frame has to make
// the round trip through a staging buffer.
class WGPUColorOut : public Render3DColorOut
{
protected:
	Color4u8 *_bufferCPU32[2];

public:
	WGPUColorOut(size_t w, size_t h);
	virtual ~WGPUColorOut();

	virtual size_t BindRead32();
	virtual const Color4u8* GetFramebuffer32() const;

	virtual Render3DError SetSize(size_t w, size_t h);
	virtual Render3DError FillZero();
	virtual Render3DError FillColor32(const Color4u8 *src, const bool isSrcNativeSize);
};

// Per-texture GPU resources. Keyed by the emulator's texture cache key, so an
// evicted cache entry can never leave us holding a dangling pointer.
struct WGPUTextureEntry
{
	WGPUTexture texture;
	WGPUTextureView view;
	WGPUBindGroup bindGroup;
};

// Textures go through this subclass because the emulator's texture upscaling
// (xBRZ 2x/4x) and deposterize options need CPU-side scratch buffers that the
// renderer owns and shares between textures. Load() builds the native-size
// image; GetMipLevelData() hands out the image for each mip level, generating
// the xBRZ upscale on demand.
class WGPURenderTexture : public Render3DTexture
{
protected:
	u32 *_unpackBuffer;
	u32 *_upscaleBuffer;
	const u32 *_baseData;

public:
	WGPURenderTexture(TEXIMAGE_PARAM texAttributes, u32 palAttributes);

	void SetUnpackBuffer(u32 *unpackBuffer);
	void SetUpscaleBuffer(u32 *upscaleBuffer);
	void SetDeposterizeBuffer(void *dstBuffer, void *workingBuffer);

	void Load();

	// Mip level 0 is the largest image; level (mipLevelCount - 1) is the native
	// one. The returned pointer stays valid until the next call on the same
	// texture.
	const u32 *GetMipLevelData(const size_t level, const size_t mipLevelCount,
							   size_t &outWidth, size_t &outHeight);
};

// One contiguous run of vertices that can be submitted with a single draw call.
struct WGPUDrawRun
{
	uint32_t firstVertex;
	uint32_t vertexCount;
	Render3DTexture *tex;
	uint8_t topology;		// 0 = triangle list, 1 = line list
	uint8_t depthEqual;		// polygon requests the NDS "equal" depth test
	uint8_t depthWrite;
	uint8_t blend;
};

class WGPURender : public Render3D
{
protected:
	// Instance backends this renderer is limited to, as WGPUInstanceBackend
	// bits. Zero lets wgpu-native pick any backend it was built with.
	uint32_t _instanceBackends;

	WGPUInstance _instance;
	WGPUAdapter _adapter;
	WGPUDevice _device;
	WGPUQueue _queue;

	WGPUCommandEncoder _encoder;

	bool _isDeviceReady;
	bool _hasFailed;

	// Offscreen render targets, sized to the emulator framebuffer.
	WGPUTexture _colorTex;
	WGPUTextureView _colorView;
	WGPUTexture _polyIDTex;
	WGPUTextureView _polyIDView;
	WGPUTexture _fogTex;
	WGPUTextureView _fogView;
	WGPUTexture _depthTex;
	WGPUTextureView _depthView;

	// Final conversion target. In 24-bit mode a fullscreen pass writes the
	// framebuffer as 6665 stored in the 8888 channels here, so the flush's
	// readback skips the CPU-side colorspace conversion.
	WGPUTexture _outputTex;
	WGPUTextureView _outputView;

	// Multisampled variants of the targets above. They are only created when
	// MSAA is enabled; the single-sampled targets then become the resolve
	// targets (color) or the postprocess inputs (depth, via a resolve pass,
	// because WebGPU cannot resolve depth attachments itself).
	uint32_t _sampleCount;
	WGPUTexture _colorTexMS;
	WGPUTextureView _colorViewMS;
	WGPUTexture _polyIDTexMS;
	WGPUTextureView _polyIDViewMS;
	WGPUTexture _fogTexMS;
	WGPUTextureView _fogViewMS;
	WGPUTexture _depthTexMS;
	WGPUTextureView _depthViewMS;

	// Clear-image sources, always native size.
	WGPUTexture _clearColorTex;
	WGPUTextureView _clearColorView;
	WGPUTexture _clearDepthTex;
	WGPUTextureView _clearDepthView;
	WGPUTexture _clearFogTex;
	WGPUTextureView _clearFogView;

	// One buffer carries the frame uniform, the lookup tables, the per-polygon
	// states and the vertex data. wgpu-native allocates a staging buffer for
	// every wgpuQueueWriteBuffer() call, so keeping the four regions together
	// turns four allocations per frame into one.
	WGPUBuffer _frameUploadBuffer;
	std::vector<u8> _frameUploadStaging;
	size_t _frameUploadVertexOffset;
	size_t _frameUploadVertexBytes;
	WGPUBuffer _readbackBuffer[2];
	size_t _readbackBufferSize;

	WGPUShaderModule _shaderModuleGeometry;
	WGPUShaderModule _shaderModulePost;
	WGPUShaderModule _shaderModuleClear;
	WGPUShaderModule _shaderModuleDepthResolve;
	WGPUShaderModule _shaderModuleOutput;

	WGPUBindGroupLayout _geomBGL;
	WGPUBindGroupLayout _postBGL;
	WGPUBindGroupLayout _clearBGL;
	WGPUBindGroupLayout _depthResolveBGL;
	WGPUBindGroupLayout _outputBGL;
	WGPUPipelineLayout _geomLayout;
	WGPUPipelineLayout _postLayout;
	WGPUPipelineLayout _clearLayout;
	WGPUPipelineLayout _depthResolveLayout;
	WGPUPipelineLayout _outputLayout;

	WGPUSampler _samplerNearest;
	WGPUSampler _samplerLinear;

	WGPUBindGroup _postBindGroup;
	WGPUBindGroup _clearBindGroup;
	WGPUBindGroup _depthResolveBindGroup;
	WGPUBindGroup _outputBindGroup;

	// 1x1 white texture and the bind group built around it. Drawn in place of a
	// texture for polygons that sample nothing, so index 0 is always bound.
	WGPUTexture _whiteTex;
	WGPUTextureView _whiteView;
	WGPUBindGroup _whiteBindGroup;

	// pipeline cache: [topology][depthEqual][depthWrite][blend]
	WGPURenderPipeline _geomPipelines[2][2][2][2];
	WGPURenderPipeline _edgePipeline;
	WGPURenderPipeline _fogPipeline;
	WGPURenderPipeline _clearPipeline;
	WGPURenderPipeline _depthResolvePipeline;
	WGPURenderPipeline _outputPipeline;

	std::map<u64, WGPUTextureEntry> _textureMap;

	std::vector<WGPUDrawRun> _drawRuns;

	u8 *_texUnpackBuffer;
	u8 _frameUniformData[832];

	const GFX3D_State *_currentState;
	bool _useClearImage;

	WGPUColorOut *_wgpuColorOut;

	void _SetPipelineEnableFlags(const GFX3D_State &renderState);
	void _WriteFrameUniforms(const GFX3D_State &renderState);
	void _BuildGeometry(const GFX3D_State &renderState, const GFX3D_GeometryList &renderGList);
	void _EncodeRenderPasses(const GFX3D_State &renderState);
	Render3DTexture* _GetLoadedTexture(const POLY &thePoly);
	WGPURenderPipeline _GetGeomPipeline(const WGPUDrawRun &run);
	WGPUTextureEntry* _GetTextureEntry(Render3DTexture *tex);
	void _FlushTextureMap();

	Render3DError _SetupDevice();
	void _TeardownDevice();
	Render3DError _CreateRenderTargets(size_t w, size_t h);
	void _DestroyRenderTargets();
	Render3DError _CreatePipelines();
	void _DestroyPipelines();
	void _CreateBindGroups();

	virtual Render3DError BeginRender(const GFX3D_State &renderState, const GFX3D_GeometryList &renderGList);
	virtual Render3DError RenderGeometry();
	virtual Render3DError PostprocessFramebuffer();
	virtual Render3DError EndRender();

	WGPURender(const uint32_t instanceBackends);

public:
	WGPURender();
	virtual ~WGPURender();

	static Render3D* Create();

	// Creates a renderer that restricts wgpu-native to the given instance
	// backends bitmask (WGPUInstanceBackend values). Zero means "let wgpu pick".
	static Render3D* CreateWithBackend(const uint32_t instanceBackends);

	static void Destroy();

	virtual Render3DError ApplyRenderingSettings(const GFX3D_State &renderState);

	virtual Render3DError Reset();
	virtual Render3DError RenderPowerOff();
	virtual Render3DError Render(const GFX3D_State &renderState, const GFX3D_GeometryList &renderGList);
	virtual Render3DError RenderFinish();
	virtual Render3DError RenderFlush(bool willFlushBuffer32, bool willFlushBuffer16);
	virtual Render3DError VramReconfigureSignal();

	virtual Render3DError SetFramebufferSize(size_t w, size_t h);

	virtual Render3DError ClearUsingImage(const u16 *colorBuffer, const u32 *depthBuffer, const u8 *fogBuffer, const u8 opaquePolyID);
	virtual Render3DError ClearUsingValues(const Color4u8 &clearColor6665, const FragmentAttributes &clearAttributes);

	// Called by WGPUColorOut when the emulator flushes the 3D layer.
	void ReadbackFramebuffer(const size_t idx, u8 *dstRaw, u8 *dstConverted);

	const GFX3D_State* GetCurrentState() const { return this->_currentState; }
};

// Renders a reference scene through the whole WebGPU path and writes it as a
// binary PPM. Used by the development self-test; returns 0 on success.
int WGPURender_SelfTest(const char *outPathPPM);

#endif // ENABLE_WGPU

#endif // WGPURENDER_H
