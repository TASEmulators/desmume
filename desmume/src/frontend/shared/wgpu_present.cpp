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

#include "wgpu_present.h"

#include <stddef.h>

#if !defined(ENABLE_WGPU)

// wgpu-native ships prebuilt binaries per platform; when the current build does
// not include WebGPU the class still has to link so that the display method can
// be selected and simply fall back.
WGPUPresent::WGPUPresent() : _impl(NULL) {}
WGPUPresent::~WGPUPresent() {}

bool WGPUPresent::begin(WGPUPresentWindow *) { return false; }
void WGPUPresent::kill() {}
void WGPUPresent::setvsync(bool) {}
void WGPUPresent::setUseOutputFilter(bool) {}
bool WGPUPresent::useOutputFilter() const { return false; }
bool WGPUPresent::isReady() const { return false; }

bool WGPUPresent::present(const uint32_t *, int, int, const WGPUPresentRect *,
						  const WGPUPresentRect *, const WGPUPresentRect *, int, uint32_t)
{
	return false;
}

#else

#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <chrono>
#include <thread>
#include <utility>

#include "webgpu.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace
{

struct Vertex
{
	float x, y;
	float u, v;
	float r, g, b, a;
};

// The presentation shader. Positions arrive already in normalized device
// coordinates (computed on the CPU, where the window size is known), so the
// vertex stage only forwards them. The per-vertex color lets the same pipeline
// draw the screen gap as a solid quad through a 1x1 white texture.
static const char *kPresentShader = R"WGSL(
struct VSOut
{
	@builtin(position) pos: vec4f,
	@location(0) uv: vec2f,
	@location(1) color: vec4f,
};

@group(0) @binding(0) var srcTex: texture_2d<f32>;
@group(0) @binding(1) var srcSamp: sampler;

@vertex
fn vsMain(@location(0) pos: vec2f, @location(1) uv: vec2f, @location(2) color: vec4f) -> VSOut
{
	var result: VSOut;
	result.pos = vec4f(pos, 0.0, 1.0);
	result.uv = uv;
	result.color = color;
	return result;
}

@fragment
fn fsMain(input: VSOut) -> @location(0) vec4f
{
	let tex = textureSample(srcTex, srcSamp, input.uv);
	return vec4f(tex.rgb * input.color.rgb, 1.0);
}
)WGSL";

static WGPUStringView Lit(const char *s)
{
	WGPUStringView view;
	view.data = s;
	view.length = WGPU_STRLEN;
	return view;
}

struct AdapterResult
{
	WGPUAdapter adapter;
	WGPURequestAdapterStatus status;
	bool done;
};

static void OnAdapterRequested(WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message, void *userdata1, void *userdata2)
{
	AdapterResult *result = (AdapterResult *)userdata1;
	result->adapter = adapter;
	result->status = status;
	result->done = true;
}

struct DeviceResult
{
	WGPUDevice device;
	WGPURequestDeviceStatus status;
	bool done;
};

static void OnDeviceRequested(WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message, void *userdata1, void *userdata2)
{
	DeviceResult *result = (DeviceResult *)userdata1;
	result->device = device;
	result->status = status;
	result->done = true;
}

static void OnUncapturedError(WGPUDevice const *device, WGPUErrorType type, WGPUStringView message, void *userdata1, void *userdata2)
{
	if (message.data == NULL)
	{
		printf("WGPU present: uncaptured error (type %d)\n", (int)type);
		return;
	}

	const size_t len = (message.length == WGPU_STRLEN) ? strlen(message.data) : (size_t)message.length;
	printf("WGPU present: uncaptured error (type %d): %.*s\n", (int)type, (int)len, message.data);
}

// wgpu-native does not implement wgpuInstanceWaitAny(), so asynchronous
// requests are driven by pumping instance events until the callback fires.
static bool WaitForFlag(WGPUInstance instance, const bool *done)
{
	const std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);

	while (!*done)
	{
		wgpuInstanceProcessEvents(instance);

		if (std::chrono::steady_clock::now() > deadline)
		{
			return false;
		}

		std::this_thread::yield();
	}

	return true;
}

} // namespace

// ---------------------------------------------------------------------------
// Implementation
// ---------------------------------------------------------------------------

struct WGPUPresent::Impl
{
	WGPUPresentWindow *window;

	WGPUInstance instance;
	WGPUAdapter adapter;
	WGPUDevice device;
	WGPUQueue queue;
	WGPUSurface surface;

	WGPUTextureFormat format;
	WGPUPresentMode presentMode;
	WGPUCompositeAlphaMode alphaMode;
	bool supportsImmediate;
	bool vsync;
	uint32_t surfaceWidth;
	uint32_t surfaceHeight;

	WGPUShaderModule shader;
	WGPUBindGroupLayout bindGroupLayout;
	WGPUPipelineLayout pipelineLayout;
	WGPURenderPipeline pipeline;
	WGPUSampler samplerNearest;
	WGPUSampler samplerLinear;

	WGPUTexture whiteTex;
	WGPUTextureView whiteView;
	WGPUBindGroup whiteBindGroup;

	WGPUTexture videoTex;
	WGPUTextureView videoView;
	WGPUBindGroup videoBindGroup;
	uint32_t videoWidth;
	uint32_t videoHeight;
	bool videoFilter;

	WGPUBuffer vertexBuffer;
	uint64_t vertexBufferSize;

	bool ready;
	bool failed;

	Impl() :
		window(NULL),
		instance(NULL),
		adapter(NULL),
		device(NULL),
		queue(NULL),
		surface(NULL),
		format(WGPUTextureFormat_BGRA8Unorm),
		presentMode(WGPUPresentMode_Fifo),
		alphaMode(WGPUCompositeAlphaMode_Auto),
		supportsImmediate(false),
		vsync(true),
		surfaceWidth(0),
		surfaceHeight(0),
		shader(NULL),
		bindGroupLayout(NULL),
		pipelineLayout(NULL),
		pipeline(NULL),
		samplerNearest(NULL),
		samplerLinear(NULL),
		whiteTex(NULL),
		whiteView(NULL),
		whiteBindGroup(NULL),
		videoTex(NULL),
		videoView(NULL),
		videoBindGroup(NULL),
		videoWidth(0),
		videoHeight(0),
		videoFilter(false),
		vertexBuffer(NULL),
		vertexBufferSize(0),
		ready(false),
		failed(false)
	{
	}

	void Destroy();
	bool CreateDevice();
	bool CreateSurface();
	bool CreatePipeline();
	bool CreateBuffers();
	bool ConfigureSurface(uint32_t w, uint32_t h);
	bool EnsureVideoTexture(uint32_t w, uint32_t h, bool useFilter);
	bool CreateBindGroup(WGPUTextureView view, WGPUSampler sampler, WGPUBindGroup *out);
};

bool WGPUPresent::Impl::CreateDevice()
{
	if (this->instance != NULL)
	{
		return true;
	}

	WGPUInstanceDescriptor instanceDesc = WGPU_INSTANCE_DESCRIPTOR_INIT;
	this->instance = wgpuCreateInstance(&instanceDesc);

	if (this->instance == NULL)
	{
		printf("WGPU present: wgpuCreateInstance() failed\n");
		this->failed = true;
		return false;
	}

	WGPURequestAdapterOptions adapterOptions = WGPU_REQUEST_ADAPTER_OPTIONS_INIT;
	adapterOptions.powerPreference = WGPUPowerPreference_HighPerformance;

	AdapterResult adapterResult;
	adapterResult.adapter = NULL;
	adapterResult.status = WGPURequestAdapterStatus_Error;
	adapterResult.done = false;

	WGPURequestAdapterCallbackInfo adapterCallback = WGPU_REQUEST_ADAPTER_CALLBACK_INFO_INIT;
	adapterCallback.mode = WGPUCallbackMode_AllowProcessEvents;
	adapterCallback.callback = OnAdapterRequested;
	adapterCallback.userdata1 = &adapterResult;

	wgpuInstanceRequestAdapter(this->instance, &adapterOptions, adapterCallback);

	if ( !WaitForFlag(this->instance, &adapterResult.done) ||
		 (adapterResult.status != WGPURequestAdapterStatus_Success) ||
		 (adapterResult.adapter == NULL) )
	{
		printf("WGPU present: no suitable adapter found\n");
		this->failed = true;
		return false;
	}

	this->adapter = adapterResult.adapter;

	WGPUDeviceDescriptor deviceDesc = WGPU_DEVICE_DESCRIPTOR_INIT;
	deviceDesc.label = Lit("DeSmuME WebGPU present device");
	deviceDesc.uncapturedErrorCallbackInfo.callback = OnUncapturedError;

	DeviceResult deviceResult;
	deviceResult.device = NULL;
	deviceResult.status = WGPURequestDeviceStatus_Error;
	deviceResult.done = false;

	WGPURequestDeviceCallbackInfo deviceCallback = WGPU_REQUEST_DEVICE_CALLBACK_INFO_INIT;
	deviceCallback.mode = WGPUCallbackMode_AllowProcessEvents;
	deviceCallback.callback = OnDeviceRequested;
	deviceCallback.userdata1 = &deviceResult;

	wgpuAdapterRequestDevice(this->adapter, &deviceDesc, deviceCallback);

	if ( !WaitForFlag(this->instance, &deviceResult.done) ||
		 (deviceResult.status != WGPURequestDeviceStatus_Success) ||
		 (deviceResult.device == NULL) )
	{
		printf("WGPU present: wgpuAdapterRequestDevice() failed\n");
		this->failed = true;
		return false;
	}

	this->device = deviceResult.device;
	this->queue = wgpuDeviceGetQueue(this->device);

	return true;
}

bool WGPUPresent::Impl::CreateSurface()
{
	if (this->surface != NULL)
	{
		return true;
	}

	if (this->window == NULL)
	{
		return false;
	}

	// The window adapter is the only piece that knows which window system this
	// is; everything below stays platform independent.
	this->surface = (WGPUSurface)this->window->createSurface(this->instance);

	if (this->surface == NULL)
	{
		printf("WGPU present: wgpuInstanceCreateSurface() failed\n");
		this->failed = true;
		return false;
	}

	WGPUSurfaceCapabilities caps = WGPU_SURFACE_CAPABILITIES_INIT;

	if (wgpuSurfaceGetCapabilities(this->surface, this->adapter, &caps) == WGPUStatus_Success)
	{
		this->format = (caps.formatCount > 0) ? caps.formats[0] : WGPUTextureFormat_BGRA8Unorm;

		for (size_t i = 0; i < caps.formatCount; i++)
		{
			if (caps.formats[i] == WGPUTextureFormat_BGRA8Unorm)
			{
				this->format = WGPUTextureFormat_BGRA8Unorm;
				break;
			}
		}

		for (size_t i = 0; i < caps.presentModeCount; i++)
		{
			if (caps.presentModes[i] == WGPUPresentMode_Immediate)
			{
				this->supportsImmediate = true;
				break;
			}
		}

		// WGPUCompositeAlphaMode_Auto is always valid and resolves to whatever
		// the surface prefers, so there is nothing to pick here.

		wgpuSurfaceCapabilitiesFreeMembers(caps);
	}

	this->presentMode = this->vsync
		? WGPUPresentMode_Fifo
		: (this->supportsImmediate ? WGPUPresentMode_Immediate : WGPUPresentMode_Fifo);

	return true;
}

bool WGPUPresent::Impl::CreatePipeline()
{
	WGPUShaderSourceWGSL wgsl = WGPU_SHADER_SOURCE_WGSL_INIT;
	wgsl.code = Lit(kPresentShader);

	WGPUShaderModuleDescriptor shaderDesc = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
	shaderDesc.nextInChain = &wgsl.chain;
	this->shader = wgpuDeviceCreateShaderModule(this->device, &shaderDesc);

	if (this->shader == NULL)
	{
		return false;
	}

	WGPUBindGroupLayoutEntry layoutEntries[2] = {
		WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT,
		WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT
	};

	layoutEntries[0].binding = 0;
	layoutEntries[0].visibility = WGPUShaderStage_Fragment;
	layoutEntries[0].texture.sampleType = WGPUTextureSampleType_Float;
	layoutEntries[0].texture.viewDimension = WGPUTextureViewDimension_2D;

	layoutEntries[1].binding = 1;
	layoutEntries[1].visibility = WGPUShaderStage_Fragment;
	layoutEntries[1].sampler.type = WGPUSamplerBindingType_Filtering;

	WGPUBindGroupLayoutDescriptor bindGroupLayoutDesc = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
	bindGroupLayoutDesc.entryCount = 2;
	bindGroupLayoutDesc.entries = layoutEntries;

	this->bindGroupLayout = wgpuDeviceCreateBindGroupLayout(this->device, &bindGroupLayoutDesc);

	if (this->bindGroupLayout == NULL)
	{
		return false;
	}

	WGPUPipelineLayoutDescriptor pipelineLayoutDesc = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
	pipelineLayoutDesc.bindGroupLayoutCount = 1;
	pipelineLayoutDesc.bindGroupLayouts = &this->bindGroupLayout;

	this->pipelineLayout = wgpuDeviceCreatePipelineLayout(this->device, &pipelineLayoutDesc);

	if (this->pipelineLayout == NULL)
	{
		return false;
	}

	WGPUSamplerDescriptor samplerDesc = WGPU_SAMPLER_DESCRIPTOR_INIT;
	samplerDesc.addressModeU = WGPUAddressMode_ClampToEdge;
	samplerDesc.addressModeV = WGPUAddressMode_ClampToEdge;
	samplerDesc.addressModeW = WGPUAddressMode_ClampToEdge;
	samplerDesc.magFilter = WGPUFilterMode_Nearest;
	samplerDesc.minFilter = WGPUFilterMode_Nearest;
	samplerDesc.mipmapFilter = WGPUMipmapFilterMode_Nearest;
	this->samplerNearest = wgpuDeviceCreateSampler(this->device, &samplerDesc);

	samplerDesc.magFilter = WGPUFilterMode_Linear;
	samplerDesc.minFilter = WGPUFilterMode_Linear;
	this->samplerLinear = wgpuDeviceCreateSampler(this->device, &samplerDesc);

	if ( (this->samplerNearest == NULL) || (this->samplerLinear == NULL) )
	{
		return false;
	}

	WGPUVertexAttribute attributes[3] = {
		WGPU_VERTEX_ATTRIBUTE_INIT,
		WGPU_VERTEX_ATTRIBUTE_INIT,
		WGPU_VERTEX_ATTRIBUTE_INIT
	};

	attributes[0].format = WGPUVertexFormat_Float32x2;
	attributes[0].offset = offsetof(Vertex, x);
	attributes[0].shaderLocation = 0;

	attributes[1].format = WGPUVertexFormat_Float32x2;
	attributes[1].offset = offsetof(Vertex, u);
	attributes[1].shaderLocation = 1;

	attributes[2].format = WGPUVertexFormat_Float32x4;
	attributes[2].offset = offsetof(Vertex, r);
	attributes[2].shaderLocation = 2;

	WGPUVertexBufferLayout vertexBufferLayout = WGPU_VERTEX_BUFFER_LAYOUT_INIT;
	vertexBufferLayout.stepMode = WGPUVertexStepMode_Vertex;
	vertexBufferLayout.arrayStride = sizeof(Vertex);
	vertexBufferLayout.attributeCount = 3;
	vertexBufferLayout.attributes = attributes;

	WGPUVertexState vertexState = WGPU_VERTEX_STATE_INIT;
	vertexState.module = this->shader;
	vertexState.entryPoint = Lit("vsMain");
	vertexState.bufferCount = 1;
	vertexState.buffers = &vertexBufferLayout;

	WGPUColorTargetState colorTarget = WGPU_COLOR_TARGET_STATE_INIT;
	colorTarget.format = this->format;
	colorTarget.blend = NULL;
	colorTarget.writeMask = WGPUColorWriteMask_All;

	WGPUFragmentState fragmentState = WGPU_FRAGMENT_STATE_INIT;
	fragmentState.module = this->shader;
	fragmentState.entryPoint = Lit("fsMain");
	fragmentState.targetCount = 1;
	fragmentState.targets = &colorTarget;

	WGPUPrimitiveState primitiveState = WGPU_PRIMITIVE_STATE_INIT;
	primitiveState.topology = WGPUPrimitiveTopology_TriangleList;
	primitiveState.cullMode = WGPUCullMode_None;

	WGPUMultisampleState multisampleState = WGPU_MULTISAMPLE_STATE_INIT;
	multisampleState.count = 1;

	WGPURenderPipelineDescriptor pipelineDesc = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
	pipelineDesc.layout = this->pipelineLayout;
	pipelineDesc.vertex = vertexState;
	pipelineDesc.primitive = primitiveState;
	pipelineDesc.multisample = multisampleState;
	pipelineDesc.fragment = &fragmentState;

	this->pipeline = wgpuDeviceCreateRenderPipeline(this->device, &pipelineDesc);

	return (this->pipeline != NULL);
}

bool WGPUPresent::Impl::CreateBindGroup(WGPUTextureView view, WGPUSampler sampler, WGPUBindGroup *out)
{
	WGPUBindGroupEntry entries[2] = {
		WGPU_BIND_GROUP_ENTRY_INIT,
		WGPU_BIND_GROUP_ENTRY_INIT
	};

	entries[0].binding = 0;
	entries[0].textureView = view;

	entries[1].binding = 1;
	entries[1].sampler = sampler;

	WGPUBindGroupDescriptor bindGroupDesc = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
	bindGroupDesc.layout = this->bindGroupLayout;
	bindGroupDesc.entryCount = 2;
	bindGroupDesc.entries = entries;

	*out = wgpuDeviceCreateBindGroup(this->device, &bindGroupDesc);
	return (*out != NULL);
}

bool WGPUPresent::Impl::CreateBuffers()
{
	this->vertexBufferSize = 18 * sizeof(Vertex);

	WGPUBufferDescriptor bufferDesc = WGPU_BUFFER_DESCRIPTOR_INIT;
	bufferDesc.usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;
	bufferDesc.size = this->vertexBufferSize;
	this->vertexBuffer = wgpuDeviceCreateBuffer(this->device, &bufferDesc);

	if (this->vertexBuffer == NULL)
	{
		return false;
	}

	// 1x1 white texture for the gap quad.
	WGPUTextureDescriptor textureDesc = WGPU_TEXTURE_DESCRIPTOR_INIT;
	textureDesc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
	textureDesc.dimension = WGPUTextureDimension_2D;
	textureDesc.size.width = 1;
	textureDesc.size.height = 1;
	textureDesc.size.depthOrArrayLayers = 1;
	textureDesc.format = WGPUTextureFormat_BGRA8Unorm;
	textureDesc.mipLevelCount = 1;
	textureDesc.sampleCount = 1;
	this->whiteTex = wgpuDeviceCreateTexture(this->device, &textureDesc);

	if (this->whiteTex == NULL)
	{
		return false;
	}

	this->whiteView = wgpuTextureCreateView(this->whiteTex, NULL);

	if (this->whiteView == NULL)
	{
		return false;
	}

	// writeTexture wants 256-byte aligned rows, so pad the single pixel out to a
	// full row instead of passing a bare 4-byte row.
	uint8_t whiteRow[256];
	memset(whiteRow, 0xFF, sizeof(whiteRow));

	WGPUTexelCopyTextureInfo copyDest = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
	copyDest.texture = this->whiteTex;

	WGPUTexelCopyBufferLayout copyLayout = WGPU_TEXEL_COPY_BUFFER_LAYOUT_INIT;
	copyLayout.bytesPerRow = sizeof(whiteRow);
	copyLayout.rowsPerImage = 1;

	WGPUExtent3D copySize = WGPU_EXTENT_3D_INIT;
	copySize.width = 1;
	copySize.height = 1;
	copySize.depthOrArrayLayers = 1;

	wgpuQueueWriteTexture(this->queue, &copyDest, whiteRow, sizeof(whiteRow), &copyLayout, &copySize);

	return this->CreateBindGroup(this->whiteView, this->samplerNearest, &this->whiteBindGroup);
}

bool WGPUPresent::Impl::ConfigureSurface(uint32_t w, uint32_t h)
{
	if ( (this->surface == NULL) || (this->device == NULL) || (w == 0) || (h == 0) )
	{
		return false;
	}

	if ( (w == this->surfaceWidth) && (h == this->surfaceHeight) )
	{
		return true;
	}

	WGPUSurfaceConfiguration config = WGPU_SURFACE_CONFIGURATION_INIT;
	config.device = this->device;
	config.format = this->format;
	config.usage = WGPUTextureUsage_RenderAttachment;
	config.width = w;
	config.height = h;
	config.alphaMode = this->alphaMode;
	config.presentMode = this->presentMode;

	wgpuSurfaceConfigure(this->surface, &config);

	this->surfaceWidth = w;
	this->surfaceHeight = h;

	return true;
}

bool WGPUPresent::Impl::EnsureVideoTexture(uint32_t w, uint32_t h, bool useFilter)
{
	if ( (w == 0) || (h == 0) )
	{
		return false;
	}

	if ( (this->videoTex != NULL) && (this->videoWidth == w) && (this->videoHeight == h) &&
		 (this->videoFilter == useFilter) )
	{
		return true;
	}

	this->videoFilter = useFilter;

	if (this->videoBindGroup != NULL)
	{
		wgpuBindGroupRelease(this->videoBindGroup);
		this->videoBindGroup = NULL;
	}

	if (this->videoView != NULL)
	{
		wgpuTextureViewRelease(this->videoView);
		this->videoView = NULL;
	}

	if (this->videoTex != NULL)
	{
		wgpuTextureRelease(this->videoTex);
		this->videoTex = NULL;
	}

	WGPUTextureDescriptor textureDesc = WGPU_TEXTURE_DESCRIPTOR_INIT;
	textureDesc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
	textureDesc.dimension = WGPUTextureDimension_2D;
	textureDesc.size.width = w;
	textureDesc.size.height = h;
	textureDesc.size.depthOrArrayLayers = 1;
	textureDesc.format = WGPUTextureFormat_BGRA8Unorm;
	textureDesc.mipLevelCount = 1;
	textureDesc.sampleCount = 1;

	this->videoTex = wgpuDeviceCreateTexture(this->device, &textureDesc);

	if (this->videoTex == NULL)
	{
		return false;
	}

	this->videoView = wgpuTextureCreateView(this->videoTex, NULL);

	if (this->videoView == NULL)
	{
		return false;
	}

	this->videoWidth = w;
	this->videoHeight = h;

	return this->CreateBindGroup(this->videoView, this->videoFilter ? this->samplerLinear : this->samplerNearest, &this->videoBindGroup);
}

void WGPUPresent::Impl::Destroy()
{
	this->ready = false;

	if (this->videoBindGroup != NULL) { wgpuBindGroupRelease(this->videoBindGroup); this->videoBindGroup = NULL; }
	if (this->videoView != NULL) { wgpuTextureViewRelease(this->videoView); this->videoView = NULL; }
	if (this->videoTex != NULL) { wgpuTextureRelease(this->videoTex); this->videoTex = NULL; }
	this->videoWidth = 0;
	this->videoHeight = 0;

	if (this->whiteBindGroup != NULL) { wgpuBindGroupRelease(this->whiteBindGroup); this->whiteBindGroup = NULL; }
	if (this->whiteView != NULL) { wgpuTextureViewRelease(this->whiteView); this->whiteView = NULL; }
	if (this->whiteTex != NULL) { wgpuTextureRelease(this->whiteTex); this->whiteTex = NULL; }

	if (this->vertexBuffer != NULL) { wgpuBufferRelease(this->vertexBuffer); this->vertexBuffer = NULL; }
	this->vertexBufferSize = 0;

	if (this->pipeline != NULL) { wgpuRenderPipelineRelease(this->pipeline); this->pipeline = NULL; }
	if (this->pipelineLayout != NULL) { wgpuPipelineLayoutRelease(this->pipelineLayout); this->pipelineLayout = NULL; }
	if (this->bindGroupLayout != NULL) { wgpuBindGroupLayoutRelease(this->bindGroupLayout); this->bindGroupLayout = NULL; }
	if (this->samplerNearest != NULL) { wgpuSamplerRelease(this->samplerNearest); this->samplerNearest = NULL; }
	if (this->samplerLinear != NULL) { wgpuSamplerRelease(this->samplerLinear); this->samplerLinear = NULL; }
	if (this->shader != NULL) { wgpuShaderModuleRelease(this->shader); this->shader = NULL; }

	if (this->surface != NULL) { wgpuSurfaceRelease(this->surface); this->surface = NULL; }
	this->surfaceWidth = 0;
	this->surfaceHeight = 0;

	if (this->queue != NULL) { wgpuQueueRelease(this->queue); this->queue = NULL; }
	if (this->device != NULL) { wgpuDeviceRelease(this->device); this->device = NULL; }
	if (this->adapter != NULL) { wgpuAdapterRelease(this->adapter); this->adapter = NULL; }
	if (this->instance != NULL) { wgpuInstanceRelease(this->instance); this->instance = NULL; }

	this->window = NULL;
}

// ---------------------------------------------------------------------------
// WGPUPresent
// ---------------------------------------------------------------------------

WGPUPresent::WGPUPresent()
{
	_impl = new Impl();
}

WGPUPresent::~WGPUPresent()
{
	if (_impl != NULL)
	{
		_impl->Destroy();
		delete _impl;
		_impl = NULL;
	}
}

bool WGPUPresent::begin(WGPUPresentWindow *window)
{
	if (_impl == NULL)
	{
		return false;
	}

	if (_impl->ready && (_impl->window == window))
	{
		return true;
	}

	if (_impl->failed)
	{
		return false;
	}

	// Start from a clean slate so a partially initialized attempt cannot leak or
	// keep a surface bound to a stale window.
	_impl->Destroy();

	_impl->window = window;

	if (!_impl->CreateDevice())
	{
		return false;
	}

	if (!_impl->CreateSurface())
	{
		return false;
	}

	if (!_impl->CreatePipeline())
	{
		return false;
	}

	if (!_impl->CreateBuffers())
	{
		return false;
	}

	_impl->ready = true;

	return true;
}

void WGPUPresent::kill()
{
	if (_impl != NULL)
	{
		_impl->Destroy();
	}
}

void WGPUPresent::setvsync(bool vsync)
{
	if (_impl == NULL)
	{
		return;
	}

	if (_impl->vsync == vsync)
	{
		return;
	}

	_impl->vsync = vsync;
	_impl->presentMode = vsync
		? WGPUPresentMode_Fifo
		: (_impl->supportsImmediate ? WGPUPresentMode_Immediate : WGPUPresentMode_Fifo);

	// Force a reconfigure on the next frame.
	_impl->surfaceWidth = 0;
	_impl->surfaceHeight = 0;
}

void WGPUPresent::setUseOutputFilter(bool theState)
{
	if (_impl == NULL)
	{
		return;
	}

	_impl->videoFilter = theState;
}

bool WGPUPresent::useOutputFilter() const
{
	return (_impl != NULL) && _impl->videoFilter;
}

bool WGPUPresent::isReady() const
{
	return (_impl != NULL) && _impl->ready;
}

bool WGPUPresent::present(const uint32_t *pixels, int width, int height,
						  const WGPUPresentRect *srcRects,
						  const WGPUPresentRect *dstRects,
						  const WGPUPresentRect *gapRect,
						  int rotation, uint32_t gapColor)
{
	if ( (_impl == NULL) || !_impl->ready || (pixels == NULL) ||
		 (width <= 0) || (height <= 0) ||
		 (srcRects == NULL) || (dstRects == NULL) )
	{
		return false;
	}

	Impl &d = *_impl;

	if ( (d.window == NULL) || !d.window->isAlive() )
	{
		return false;
	}

	int windowWidth  = 0;
	int windowHeight = 0;

	if ( !d.window->getClientSize(&windowWidth, &windowHeight) ||
		 (windowWidth <= 0) || (windowHeight <= 0) )
	{
		return false;
	}

	if (!d.ConfigureSurface((uint32_t)windowWidth, (uint32_t)windowHeight))
	{
		return false;
	}

	WGPUSurfaceTexture surfaceTexture = WGPU_SURFACE_TEXTURE_INIT;
	wgpuSurfaceGetCurrentTexture(d.surface, &surfaceTexture);

	const bool gotTexture =
		(surfaceTexture.status == WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal) ||
		(surfaceTexture.status == WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal);

	if (!gotTexture)
	{
		if (surfaceTexture.texture != NULL)
		{
			wgpuTextureRelease(surfaceTexture.texture);
		}

		// Force a reconfigure and retry on the next frame.
		d.surfaceWidth = 0;
		d.surfaceHeight = 0;
		return false;
	}

	WGPUTextureView targetView = wgpuTextureCreateView(surfaceTexture.texture, NULL);

	if (targetView == NULL)
	{
		wgpuTextureRelease(surfaceTexture.texture);
		return false;
	}

	if (!d.EnsureVideoTexture((uint32_t)width, (uint32_t)height, d.videoFilter))
	{
		wgpuTextureViewRelease(targetView);
		wgpuTextureRelease(surfaceTexture.texture);
		return false;
	}

	// Upload the composited frame.
	WGPUTexelCopyTextureInfo uploadDest = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
	uploadDest.texture = d.videoTex;

	WGPUTexelCopyBufferLayout uploadLayout = WGPU_TEXEL_COPY_BUFFER_LAYOUT_INIT;
	// video.width is always a multiple of 64 (256*n or prefilterWidth*{1,3/2,2,3,4,5}),
	// so width * 4 is always 256-byte aligned, as writeTexture requires.
	uploadLayout.bytesPerRow = (uint32_t)width * 4;
	uploadLayout.rowsPerImage = (uint32_t)height;

	WGPUExtent3D uploadSize = WGPU_EXTENT_3D_INIT;
	uploadSize.width = (uint32_t)width;
	uploadSize.height = (uint32_t)height;
	uploadSize.depthOrArrayLayers = 1;

	wgpuQueueWriteTexture(d.queue, &uploadDest, pixels, (size_t)width * (size_t)height * 4, &uploadLayout, &uploadSize);

	// Build the quads. Positions are converted straight to NDC here because the
	// window size is known; UVs are permuted exactly like the OpenGL path so
	// that screen rotation matches.
	Vertex vertices[18];
	int vertexCount = 0;

	const float halfW = (float)windowWidth;
	const float halfH = (float)windowHeight;

	const auto ndcX = [halfW](int x) -> float { return ((float)x / halfW) * 2.0f - 1.0f; };
	const auto ndcY = [halfH](int y) -> float { return 1.0f - ((float)y / halfH) * 2.0f; };

	const auto pushVertex = [&](float x, float y, float u, float v, float r, float g, float b)
	{
		vertices[vertexCount].x = x;
		vertices[vertexCount].y = y;
		vertices[vertexCount].u = u;
		vertices[vertexCount].v = v;
		vertices[vertexCount].r = r;
		vertices[vertexCount].g = g;
		vertices[vertexCount].b = b;
		vertices[vertexCount].a = 1.0f;
		vertexCount++;
	};

	const auto pushQuad = [&](const float xs[4], const float ys[4], const float us[4], const float vs[4], float r, float g, float b)
	{
		static const int order[6] = { 0, 1, 2, 0, 2, 3 };
		for (int i = 0; i < 6; i++)
		{
			const int corner = order[i];
			pushVertex(xs[corner], ys[corner], us[corner], vs[corner], r, g, b);
		}
	};

	// The gap is drawn first, exactly like the OpenGL path fills it before the
	// two screens, so that a partially overlapping gap never hides game pixels.
	int gapVertexCount = 0;

	if ( (gapRect != NULL) && (gapRect->right > gapRect->left) && (gapRect->bottom > gapRect->top) )
	{
		const float gapR = (float)((gapColor >> 0) & 0xFF) / 255.0f;
		const float gapG = (float)((gapColor >> 8) & 0xFF) / 255.0f;
		const float gapB = (float)((gapColor >> 16) & 0xFF) / 255.0f;

		const float xs[4] = { ndcX(gapRect->left), ndcX(gapRect->right), ndcX(gapRect->right), ndcX(gapRect->left) };
		const float ys[4] = { ndcY(gapRect->top), ndcY(gapRect->top), ndcY(gapRect->bottom), ndcY(gapRect->bottom) };
		const float us[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		const float vs[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

		pushQuad(xs, ys, us, vs, gapR, gapG, gapB);
		gapVertexCount = 6;
	}

	WGPUPresentRect src[2] = { srcRects[0], srcRects[1] };

	for (int i = 0; i < 2; i++)
	{
		int idx = i;
		int ofs = 0;

		switch (rotation)
		{
			case 0:
				break;

			case 90:
				ofs = 3;
				idx = 1 - i;
				std::swap(src[idx].right, src[idx].bottom);
				std::swap(src[idx].left, src[idx].top);
				break;

			case 180:
				idx = 1 - i;
				ofs = 2;
				break;

			case 270:
				std::swap(src[idx].right, src[idx].bottom);
				std::swap(src[idx].left, src[idx].top);
				ofs = 1;
				break;

			default:
				break;
		}

		const WGPUPresentRect &s = src[idx];
		const int dstLeft   = dstRects[i].left;
		const int dstTop    = dstRects[i].top;
		const int dstRight  = dstRects[i].right;
		const int dstBottom = dstRects[i].bottom;

		const float u1 = (float)s.left   / (float)width;
		const float u2 = (float)s.right  / (float)width;
		const float v1 = (float)s.top    / (float)height;
		const float v2 = (float)s.bottom / (float)height;

		const float u[4] = { u1, u2, u2, u1 };
		const float v[4] = { v1, v1, v2, v2 };

		const float us[4] = { u[(ofs + 0) % 4], u[(ofs + 1) % 4], u[(ofs + 2) % 4], u[(ofs + 3) % 4] };
		const float vs[4] = { v[(ofs + 0) % 4], v[(ofs + 1) % 4], v[(ofs + 2) % 4], v[(ofs + 3) % 4] };

		const float xs[4] = { ndcX(dstLeft), ndcX(dstRight), ndcX(dstRight), ndcX(dstLeft) };
		const float ys[4] = { ndcY(dstTop), ndcY(dstTop), ndcY(dstBottom), ndcY(dstBottom) };

		pushQuad(xs, ys, us, vs, 1.0f, 1.0f, 1.0f);
	}

	wgpuQueueWriteBuffer(d.queue, d.vertexBuffer, 0, vertices, (size_t)vertexCount * sizeof(Vertex));

	WGPUCommandEncoderDescriptor encoderDesc = WGPU_COMMAND_ENCODER_DESCRIPTOR_INIT;
	WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(d.device, &encoderDesc);

	WGPURenderPassColorAttachment colorAttachment = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
	colorAttachment.view = targetView;
	colorAttachment.loadOp = WGPULoadOp_Clear;
	colorAttachment.storeOp = WGPUStoreOp_Store;
	colorAttachment.clearValue.r = 0.0;
	colorAttachment.clearValue.g = 0.0;
	colorAttachment.clearValue.b = 0.0;
	colorAttachment.clearValue.a = 1.0;

	WGPURenderPassDescriptor renderPassDesc = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
	renderPassDesc.colorAttachmentCount = 1;
	renderPassDesc.colorAttachments = &colorAttachment;

	WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);

	wgpuRenderPassEncoderSetPipeline(pass, d.pipeline);
	wgpuRenderPassEncoderSetVertexBuffer(pass, 0, d.vertexBuffer, 0, d.vertexBufferSize);

	if (gapVertexCount > 0)
	{
		wgpuRenderPassEncoderSetBindGroup(pass, 0, d.whiteBindGroup, 0, NULL);
		wgpuRenderPassEncoderDraw(pass, (uint32_t)gapVertexCount, 1, 0, 0);
	}

	wgpuRenderPassEncoderSetBindGroup(pass, 0, d.videoBindGroup, 0, NULL);
	wgpuRenderPassEncoderDraw(pass, 12, 1, (uint32_t)gapVertexCount, 0);

	wgpuRenderPassEncoderEnd(pass);

	WGPUCommandBufferDescriptor commandBufferDesc = WGPU_COMMAND_BUFFER_DESCRIPTOR_INIT;
	WGPUCommandBuffer commandBuffer = wgpuCommandEncoderFinish(encoder, &commandBufferDesc);

	wgpuQueueSubmit(d.queue, 1, &commandBuffer);

	wgpuCommandBufferRelease(commandBuffer);
	wgpuRenderPassEncoderRelease(pass);
	wgpuCommandEncoderRelease(encoder);
	wgpuTextureViewRelease(targetView);
	wgpuTextureRelease(surfaceTexture.texture);

	if (wgpuSurfacePresent(d.surface) != WGPUStatus_Success)
	{
		d.surfaceWidth = 0;
		d.surfaceHeight = 0;
		return false;
	}

	return true;
}

#endif // ENABLE_WGPU
