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

bool WGPUPresent::begin(WGPUPresentWindow *, uint32_t) { return false; }
void WGPUPresent::kill() {}
void WGPUPresent::setvsync(bool) {}
void WGPUPresent::setUseOutputFilter(bool) {}
bool WGPUPresent::useOutputFilter() const { return false; }
void WGPUPresent::setUpscaler(int) {}
int WGPUPresent::upscaler() const { return WGPUPresentUpscaler_Off; }
bool WGPUPresent::upscalerCompiled() const { return false; }
void WGPUPresent::setUpscalerShaderSource(WGPUPresentShaderSource *) {}
int WGPUPresent::upscalerCount() { return 0; }
const char *WGPUPresent::upscalerName(int) { return NULL; }
bool WGPUPresent::isUpscaler(int upscaler) { return upscaler == WGPUPresentUpscaler_Off; }
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
// wgpu-native extensions (WGPUInstanceExtras) used to pin the instance backend.
#include "wgpu.h"

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

// ---------------------------------------------------------------------------
// GPU filters
// ---------------------------------------------------------------------------
//
// A chain is a list of passes over the frame. A pass reads the texture the pass
// before it made -- the frame for the first -- and writes one of its own, and
// the quads that draw the two screens sample the last of them, so the whole
// chain costs one render pass per pass and nothing else.
//
// Nothing here is uploaded per frame: a pass is handed the sizes of its source
// and its target, which change when the frame or the window does and not
// otherwise, so they are written to its uniform buffer then and read from the
// same buffer after.

// The most passes any of the chains has; ScaleFX has five.
#define WGPU_UPSCALER_MAX_PASSES 5

// The largest target a chain may be asked for: the default limit of
// maxTextureDimension2D, which every device wgpu supports is at least.
#define WGPU_UPSCALER_MAX_TARGET 8192

// The four float4s a pass is handed, in the order and the layout of the struct
// the WGSL declares.
struct UpscalerConstants
{
	float sourceSize[4]; // xy: the size the pass reads, zw: one texel
	float outputSize[4]; // xy: the size of its target, zw: one pixel of it
	float scale[4];      // xy: outputSize.xy / sourceSize.xy
	float secondSize[4]; // xy: the size of a second texture, zw: one texel
};

struct UpscalerSource
{
	int value;                    // the WGPUPresentUpscaler it implements
	const char *name;             // for the menu and the log
	const char *const *resources; // per pass: the resource holding its WGSL
	const int *scales;            // per pass: 0 is the target size, n is n times the frame
	const int *seconds;           // per pass: the texture it reads besides the one the
	                              // pass before it made, as an index of the outputs --
	                              // 0 is the frame, n the target of pass n - 1, -1 that
	                              // it reads one texture only
	int passCount;
};

static const char *const g_xbrzResources[]           = { "XBRZ_FREESCALE_WGSL" };
static const char *const g_lanczos3Resources[]       = { "LANCZOS3_WGSL" };
static const char *const g_bicubicResources[]        = { "BICUBIC_WGSL" };
static const char *const g_xbrLv2Resources[]         = { "XBR_LV2_WGSL" };
static const char *const g_xbrLv2NoBlendResources[]  = { "XBR_LV2_NOBLEND_WGSL" };
static const char *const g_xbrLv3Resources[]         = { "XBR_LV3_WGSL" };
static const char *const g_ptSkyWalkerResources[]    = { "PT_SKYWALKER541_WGSL" };

// The flags of every pixel at the frame size, then the blend that reads them
// and the frame together.
static const char *const g_xbr2xResources[]          = { "XBR_2X_PASS0_WGSL", "XBR_2X_PASS1_WGSL" };

// xBR lv3 in the two passes of its preset: the edge data of every pixel, then
// the interpolation that reads it and the frame together.
static const char *const g_xbrLv3MultipassResources[] = { "XBR_LV3_PASS0_WGSL", "XBR_LV3_PASS1_WGSL" };

// super-xBR: the edge data, the picture doubled, then the jinc filter over it.
static const char *const g_superXbrResources[]       = { "SUPER_XBR_PASS0_WGSL", "SUPER_XBR_PASS1_WGSL", "SUPER_XBR_JINC2_WGSL" };

// ScaleFX: the metrics of the edge, the strengths of the corners, the
// junctions, the subpixels, and the pass that takes the colour from the frame
// at each of them.
static const char *const g_scalefxResources[]        = { "SCALEFX_PASS0_WGSL", "SCALEFX_PASS1_WGSL", "SCALEFX_PASS2_WGSL", "SCALEFX_PASS3_WGSL", "SCALEFX_PASS4_WGSL" };

// xBR-mlv4: two passes of edge data, the line each quarter of the pixel
// follows at twice the frame size, and the interpolation along it.
static const char *const g_xbrMlv4Resources[]        = { "XBR_MLV4_PASS1_WGSL", "XBR_MLV4_PASS2_WGSL", "XBR_MLV4_PASS3_WGSL", "XBR_MLV4_PASS4_WGSL" };

static const int g_singleScales[]                    = { 0 };
static const int g_xbr2xScales[]                     = { 1, 0 };
static const int g_xbrLv3MultipassScales[]           = { 1, 0 };
static const int g_superXbrScales[]                  = { 1, 2, 0 };
static const int g_scalefxScales[]                   = { 1, 1, 1, 1, 0 };
static const int g_xbrMlv4Scales[]                   = { 1, 1, 2, 0 };

static const int g_noSecond[]                        = { -1 };
static const int g_xbr2xSeconds[]                    = { -1, 0 };
static const int g_xbrLv3MultipassSeconds[]          = { -1, 0 };
static const int g_superXbrSeconds[]                 = { -1, 0, -1 };
static const int g_scalefxSeconds[]                  = { -1, -1, 1, -1, 0 };
static const int g_xbrMlv4Seconds[]                  = { -1, 0, 0, 0 };

// Rows are in menu order, and the menu is drawn straight off this table, so a
// chain sits where it reads best: the two resampling filters first, then the
// edge blenders.
static const UpscalerSource g_upscalers[] =
{
	{ WGPUPresentUpscaler_Xbrz,             "xBRZ",              g_xbrzResources,             g_singleScales,          g_noSecond,                 1 },
	{ WGPUPresentUpscaler_Lanczos3,         "Lanczos3",          g_lanczos3Resources,         g_singleScales,          g_noSecond,                 1 },
	{ WGPUPresentUpscaler_Bicubic,          "Bicubic",           g_bicubicResources,          g_singleScales,          g_noSecond,                 1 },
	{ WGPUPresentUpscaler_XbrLv2,           "xBR lv2",           g_xbrLv2Resources,           g_singleScales,          g_noSecond,                 1 },
	{ WGPUPresentUpscaler_XbrLv2NoBlend,    "xBR lv2 no-blend",  g_xbrLv2NoBlendResources,    g_singleScales,          g_noSecond,                 1 },
	{ WGPUPresentUpscaler_XbrLv3,           "xBR lv3",           g_xbrLv3Resources,           g_singleScales,          g_noSecond,                 1 },
	{ WGPUPresentUpscaler_XbrLv3Multipass,  "xBR lv3 multipass", g_xbrLv3MultipassResources,  g_xbrLv3MultipassScales, g_xbrLv3MultipassSeconds,   2 },
	{ WGPUPresentUpscaler_SuperXbr,         "super-xBR",         g_superXbrResources,         g_superXbrScales,        g_superXbrSeconds,          3 },
	{ WGPUPresentUpscaler_ScaleFx,          "ScaleFX",           g_scalefxResources,          g_scalefxScales,         g_scalefxSeconds,           5 },
	{ WGPUPresentUpscaler_Xbr2x,            "2xBR",              g_xbr2xResources,            g_xbr2xScales,           g_xbr2xSeconds,             2 },
	{ WGPUPresentUpscaler_XbrMlv4,          "xBR-mlv4",          g_xbrMlv4Resources,          g_xbrMlv4Scales,         g_xbrMlv4Seconds,           4 },
	{ WGPUPresentUpscaler_PtSkyWalker541,   "PT SkyWalker541",   g_ptSkyWalkerResources,      g_singleScales,          g_noSecond,                 1 },
};

#define WGPU_UPSCALER_COUNT ((int)(sizeof(g_upscalers) / sizeof(g_upscalers[0])))

// The rows are the menu, and the values are the menu positions, so a new chain
// needs both an enum entry and a row here; adding only one leaves a numbered
// item with no name, or a chain that never appears.
static_assert(WGPU_UPSCALER_COUNT == (WGPUPresentUpscalerCount - 1),
			  "one chain table row per WGPUPresentUpscaler value, Off excepted");

// The row of the table a WGPUPresentUpscaler value is, or -1.
static int UpscalerIndex(int value)
{
	for (int i = 0; i < WGPU_UPSCALER_COUNT; i++)
	{
		if (g_upscalers[i].value == value)
		{
			return i;
		}
	}

	return -1;
}

} // namespace

// ---------------------------------------------------------------------------
// Implementation
// ---------------------------------------------------------------------------

struct WGPUPresent::Impl
{
	WGPUPresentWindow *window;

	// Instance backends this presentation path is pinned to, as
	// WGPUInstanceBackend bits. Zero lets wgpu-native pick any backend.
	uint32_t instanceBackends;

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

	// The GPU filter the frame goes through. The two screens are chained
	// separately: each one has its own copy of its half of the frame, its own
	// targets and its own bind groups, so a filter that reads the pixels around
	// one never reads the other screen's across the seam. The pipelines are
	// shared, because they depend on the filter and not on the screen.
	// `upscalerBuilt` is the value those pipelines were built for, and
	// `upscalerFailed` says the WGSL of the selected filter did not compile, in
	// which case the plain blit is presented instead.
	WGPUPresentShaderSource *shaderSource;
	int upscaler;
	int upscalerBuilt;
	bool upscalerFailed;
	WGPUBindGroupLayout upscalerBindGroupLayout;
	WGPUPipelineLayout upscalerPipelineLayout;

	struct UpscalerPass
	{
		WGPUBindGroup group;         // what it reads, and its constants
		WGPUBuffer constants;
		WGPUTexture texture;         // the target it draws into
		WGPUTextureView view;
	};

	struct UpscalerScreen
	{
		UpscalerPass passes[WGPU_UPSCALER_MAX_PASSES];

		// The screen's own copy of its half of the frame, and where its chain
		// starts. A texture view cannot be a sub-rectangle, so the rectangle
		// the screen occupies is copied out of the frame texture before the
		// first pass reads it; the two formats match, so the copy is straight.
		WGPUTexture sourceTex;
		WGPUTextureView sourceView;

		// The last target, bound for the pipeline that draws this screen's quad.
		WGPUBindGroup outputGroup;

		int passCount;
		uint32_t width;
		uint32_t height;
		uint32_t targetWidth;
		uint32_t targetHeight;
	};

	WGPURenderPipeline upscalerPipelines[WGPU_UPSCALER_MAX_PASSES];
	int upscalerPipelineCount;
	UpscalerScreen upscalerScreens[2];

	bool ready;
	bool failed;

	Impl() :
		window(NULL),
		instanceBackends(0),
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
		shaderSource(NULL),
		upscaler(WGPUPresentUpscaler_Off),
		upscalerBuilt(WGPUPresentUpscaler_Off),
		upscalerFailed(false),
		upscalerBindGroupLayout(NULL),
		upscalerPipelineLayout(NULL),
		upscalerPipelineCount(0),
		ready(false),
		failed(false)
	{
		memset(this->upscalerPipelines, 0, sizeof(this->upscalerPipelines));
		memset(this->upscalerScreens, 0, sizeof(this->upscalerScreens));
	}

	void Destroy();
	bool CreateDevice(uint32_t instanceBackends);
	bool CreateSurface();
	bool CreatePipeline();
	bool CreateBuffers();
	bool ConfigureSurface(uint32_t w, uint32_t h);
	bool EnsureVideoTexture(uint32_t w, uint32_t h, bool useFilter);
	bool CreateBindGroup(WGPUTextureView view, WGPUSampler sampler, WGPUBindGroup *out);

	bool CreateUpscalerResources();
	bool CreateUpscalerPipeline(WGPUShaderModule module, int index);
	bool BuildUpscalerChain(int value);
	void ReleaseUpscalerChain();
	void ReleaseUpscalerScreen(UpscalerScreen &screen);
	bool PrepareUpscaler(int screen, uint32_t width, uint32_t height, uint32_t targetWidth, uint32_t targetHeight);
	void DrawUpscalerChain(WGPUCommandEncoder encoder, int screen, const WGPUPresentRect &source);
};

bool WGPUPresent::Impl::CreateDevice(uint32_t instanceBackends)
{
	// Remembered first: the early-out below must not leave a stale mask behind.
	this->instanceBackends = instanceBackends;

	if (this->instance != NULL)
	{
		return true;
	}

	WGPUInstanceExtras extras;
	memset(&extras, 0, sizeof(extras));

	WGPUInstanceDescriptor instanceDesc = WGPU_INSTANCE_DESCRIPTOR_INIT;

	if (instanceBackends != 0)
	{
		// Pin the instance to the requested backend so the frontend can offer one
		// display method per backend, the same way it lists the 3D renderers.
		extras.chain.sType = (WGPUSType)WGPUSType_InstanceExtras;
		extras.backends = (WGPUInstanceBackend)instanceBackends;
		instanceDesc.nextInChain = &extras.chain;
	}

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

	if (this->pipeline == NULL)
	{
		return false;
	}

	// The GPU filter chains need layouts of their own. A device that cannot
	// make them leaves the filters unavailable: PrepareUpscaler() gives up on
	// a NULL layout long before it asks for a shader, and the plain blit is
	// what the frame gets.
	this->CreateUpscalerResources();

	return true;
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
	// CopySrc as well: a GPU filter copies each screen's rectangle out of this
	// texture before running that screen's chain over it.
	textureDesc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst | WGPUTextureUsage_CopySrc;
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

// ---------------------------------------------------------------------------
// GPU filters
// ---------------------------------------------------------------------------

// The one bind group layout every pass of every chain uses: the texture it
// reads, a sampler for the shaders that fetch through one, its constants, and
// the second texture of a longer chain with a sampler of its own. A pass of a
// single texture leaves the last two unread, and a layout entry a shader does
// not use costs nothing.
bool WGPUPresent::Impl::CreateUpscalerResources()
{
	WGPUBindGroupLayoutEntry entries[5] = {
		WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT,
		WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT,
		WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT,
		WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT,
		WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT
	};

	entries[0].binding = 0;
	entries[0].visibility = WGPUShaderStage_Fragment;
	entries[0].texture.sampleType = WGPUTextureSampleType_Float;
	entries[0].texture.viewDimension = WGPUTextureViewDimension_2D;

	entries[1].binding = 1;
	entries[1].visibility = WGPUShaderStage_Fragment;
	entries[1].sampler.type = WGPUSamplerBindingType_Filtering;

	entries[2].binding = 2;
	entries[2].visibility = WGPUShaderStage_Fragment;
	entries[2].buffer.type = WGPUBufferBindingType_Uniform;
	entries[2].buffer.hasDynamicOffset = false;
	entries[2].buffer.minBindingSize = sizeof(UpscalerConstants);

	entries[3].binding = 3;
	entries[3].visibility = WGPUShaderStage_Fragment;
	entries[3].texture.sampleType = WGPUTextureSampleType_Float;
	entries[3].texture.viewDimension = WGPUTextureViewDimension_2D;

	entries[4].binding = 4;
	entries[4].visibility = WGPUShaderStage_Fragment;
	entries[4].sampler.type = WGPUSamplerBindingType_Filtering;

	WGPUBindGroupLayoutDescriptor layoutDesc = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
	layoutDesc.entryCount = 5;
	layoutDesc.entries = entries;

	this->upscalerBindGroupLayout = wgpuDeviceCreateBindGroupLayout(this->device, &layoutDesc);

	if (this->upscalerBindGroupLayout == NULL)
	{
		return false;
	}

	WGPUPipelineLayoutDescriptor pipelineLayoutDesc = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
	pipelineLayoutDesc.bindGroupLayoutCount = 1;
	pipelineLayoutDesc.bindGroupLayouts = &this->upscalerBindGroupLayout;

	this->upscalerPipelineLayout = wgpuDeviceCreatePipelineLayout(this->device, &pipelineLayoutDesc);

	return (this->upscalerPipelineLayout != NULL);
}

// A pass covers its target and takes its taps from the texture, so it needs
// neither a vertex buffer nor an attribute: the vertex stage makes the quad out
// of the vertex index, and the draw is the four vertices of a strip.
bool WGPUPresent::Impl::CreateUpscalerPipeline(WGPUShaderModule module, int index)
{
	WGPUVertexState vertexState = WGPU_VERTEX_STATE_INIT;
	vertexState.module = module;
	vertexState.entryPoint = Lit("vsMain");
	vertexState.bufferCount = 0;
	vertexState.buffers = NULL;

	WGPUColorTargetState colorTarget = WGPU_COLOR_TARGET_STATE_INIT;
	colorTarget.format = WGPUTextureFormat_RGBA8Unorm;
	colorTarget.blend = NULL;
	colorTarget.writeMask = WGPUColorWriteMask_All;

	WGPUFragmentState fragmentState = WGPU_FRAGMENT_STATE_INIT;
	fragmentState.module = module;
	fragmentState.entryPoint = Lit("fsMain");
	fragmentState.targetCount = 1;
	fragmentState.targets = &colorTarget;

	WGPUPrimitiveState primitiveState = WGPU_PRIMITIVE_STATE_INIT;
	primitiveState.topology = WGPUPrimitiveTopology_TriangleStrip;
	primitiveState.cullMode = WGPUCullMode_None;

	WGPUMultisampleState multisampleState = WGPU_MULTISAMPLE_STATE_INIT;
	multisampleState.count = 1;

	WGPURenderPipelineDescriptor pipelineDesc = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
	pipelineDesc.layout = this->upscalerPipelineLayout;
	pipelineDesc.vertex = vertexState;
	pipelineDesc.primitive = primitiveState;
	pipelineDesc.multisample = multisampleState;
	pipelineDesc.fragment = &fragmentState;

	this->upscalerPipelines[index] = wgpuDeviceCreateRenderPipeline(this->device, &pipelineDesc);

	return (this->upscalerPipelines[index] != NULL);
}

void WGPUPresent::Impl::ReleaseUpscalerScreen(UpscalerScreen &screen)
{
	for (int i = 0; i < WGPU_UPSCALER_MAX_PASSES; i++)
	{
		UpscalerPass &state = screen.passes[i];

		if (state.group != NULL) { wgpuBindGroupRelease(state.group); state.group = NULL; }
		if (state.constants != NULL) { wgpuBufferRelease(state.constants); state.constants = NULL; }
		if (state.view != NULL) { wgpuTextureViewRelease(state.view); state.view = NULL; }
		if (state.texture != NULL) { wgpuTextureRelease(state.texture); state.texture = NULL; }
	}

	if (screen.outputGroup != NULL)
	{
		wgpuBindGroupRelease(screen.outputGroup);
		screen.outputGroup = NULL;
	}

	if (screen.sourceView != NULL) { wgpuTextureViewRelease(screen.sourceView); screen.sourceView = NULL; }
	if (screen.sourceTex != NULL) { wgpuTextureRelease(screen.sourceTex); screen.sourceTex = NULL; }

	screen.passCount = 0;
	screen.width = 0;
	screen.height = 0;
	screen.targetWidth = 0;
	screen.targetHeight = 0;
}

void WGPUPresent::Impl::ReleaseUpscalerChain()
{
	this->ReleaseUpscalerScreen(this->upscalerScreens[0]);
	this->ReleaseUpscalerScreen(this->upscalerScreens[1]);

	for (int i = 0; i < WGPU_UPSCALER_MAX_PASSES; i++)
	{
		if (this->upscalerPipelines[i] != NULL)
		{
			wgpuRenderPipelineRelease(this->upscalerPipelines[i]);
			this->upscalerPipelines[i] = NULL;
		}
	}

	this->upscalerPipelineCount = 0;
	this->upscalerBuilt = WGPUPresentUpscaler_Off;
}

// Compiles one module and one pipeline per pass. The pipelines stay for as long
// as the chain does; only the targets below them are rebuilt when a size
// changes, because a pass pipeline is independent of the size it draws at.
bool WGPUPresent::Impl::BuildUpscalerChain(int value)
{
	const int index = UpscalerIndex(value);

	if ( (index < 0) || (this->device == NULL) ||
		 (this->upscalerBindGroupLayout == NULL) || (this->upscalerPipelineLayout == NULL) )
	{
		return false;
	}

	if ( (value == this->upscalerBuilt) && (this->upscalerPipelineCount > 0) )
	{
		return true;
	}

	// A chain whose WGSL the device rejected is not compiled again: the menu
	// keeps it selectable, and the frames it is selected for are presented with
	// the plain blit rather than with a compile attempt per frame.
	if ( (value == this->upscalerBuilt) && this->upscalerFailed )
	{
		return false;
	}

	this->ReleaseUpscalerChain();

	const UpscalerSource &source = g_upscalers[index];

	bool built = true;

	for (int pass = 0; (pass < source.passCount) && built; pass++)
	{
		const char *code = NULL;
		size_t length = 0;

		if ( (this->shaderSource == NULL) ||
			 !this->shaderSource->getShader(source.resources[pass], &code, &length) ||
			 (code == NULL) || (length == 0) )
		{
			printf("WGPU present: the %s shader %s is missing\n", source.name, source.resources[pass]);
			built = false;
			break;
		}

		// The sources are resources rather than string literals, so they carry
		// their length instead of a terminator.
		WGPUShaderSourceWGSL wgsl = WGPU_SHADER_SOURCE_WGSL_INIT;
		wgsl.code.data = code;
		wgsl.code.length = length;

		WGPUShaderModuleDescriptor shaderDesc = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
		shaderDesc.nextInChain = &wgsl.chain;

		WGPUShaderModule module = wgpuDeviceCreateShaderModule(this->device, &shaderDesc);

		if (module == NULL)
		{
			built = false;
			break;
		}

		built = this->CreateUpscalerPipeline(module, pass);

		wgpuShaderModuleRelease(module);
	}

	this->upscalerBuilt = value;
	this->upscalerFailed = !built;

	if (!built)
	{
		this->ReleaseUpscalerChain();
		this->upscalerBuilt = value;
		printf("WGPU present: the %s filter is not available on this device\n", source.name);
		return false;
	}

	this->upscalerPipelineCount = source.passCount;

	return true;
}

// Builds one screen's source texture, the targets of its chain and the bind
// group of every pass, one time per size. `width`/`height` is the rectangle the
// screen occupies in the frame, `targetWidth`/`targetHeight` the size it is
// drawn at on screen, which is also the size of its last target.
bool WGPUPresent::Impl::PrepareUpscaler(int screenIndex, uint32_t width, uint32_t height,
										uint32_t targetWidth, uint32_t targetHeight)
{
	if ( (this->upscaler == WGPUPresentUpscaler_Off) || (this->shaderSource == NULL) )
	{
		return false;
	}

	if (!this->BuildUpscalerChain(this->upscaler))
	{
		return false;
	}

	const int index = UpscalerIndex(this->upscaler);

	if ( (index < 0) || (this->upscalerPipelineCount != g_upscalers[index].passCount) )
	{
		return false;
	}

	const UpscalerSource &source = g_upscalers[index];
	UpscalerScreen &screen = this->upscalerScreens[screenIndex];

	if ( (screen.outputGroup != NULL) &&
		 (screen.width == width) &&
		 (screen.height == height) &&
		 (screen.targetWidth == targetWidth) &&
		 (screen.targetHeight == targetHeight) )
	{
		return true;
	}

	this->ReleaseUpscalerScreen(screen);

	screen.width = width;
	screen.height = height;
	screen.targetWidth = targetWidth;
	screen.targetHeight = targetHeight;

	// The screen's own copy of its half of the frame: the chain has to read
	// nothing but this screen, or a filter that looks at its neighbours would
	// reach across the seam into the other one.
	{
		WGPUExtent3D sourceSize = WGPU_EXTENT_3D_INIT;
		sourceSize.width = width;
		sourceSize.height = height;
		sourceSize.depthOrArrayLayers = 1;

		WGPUTextureDescriptor sourceDesc = WGPU_TEXTURE_DESCRIPTOR_INIT;
		sourceDesc.size = sourceSize;
		sourceDesc.mipLevelCount = 1;
		sourceDesc.sampleCount = 1;
		sourceDesc.dimension = WGPUTextureDimension_2D;
		// The same format as the frame, so the copy out of it is a straight one.
		sourceDesc.format = WGPUTextureFormat_BGRA8Unorm;
		sourceDesc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;

		screen.sourceTex = wgpuDeviceCreateTexture(this->device, &sourceDesc);
		screen.sourceView = (screen.sourceTex != NULL) ? wgpuTextureCreateView(screen.sourceTex, NULL) : NULL;

		if ( (screen.sourceTex == NULL) || (screen.sourceView == NULL) )
		{
			printf("WGPU present: the %s filter cannot hold the frame of screen %d\n", source.name, screenIndex + 1);
			this->ReleaseUpscalerScreen(screen);
			return false;
		}
	}

	WGPUTextureView input = screen.sourceView;
	uint32_t inputWidth = width;
	uint32_t inputHeight = height;

	// What each pass made, so that a later one can be handed it as its second
	// texture: 2xBR's blend reads the flags of the pass in front of it and the
	// frame together.
	WGPUTextureView passViews[WGPU_UPSCALER_MAX_PASSES] = { NULL };
	uint32_t passWidths[WGPU_UPSCALER_MAX_PASSES] = { 0 };
	uint32_t passHeights[WGPU_UPSCALER_MAX_PASSES] = { 0 };

	for (int pass = 0; pass < source.passCount; pass++)
	{
		UpscalerPass &state = screen.passes[pass];

		const int second = source.seconds[pass];
		WGPUTextureView secondView = screen.sourceView;
		uint32_t secondWidth = width;
		uint32_t secondHeight = height;

		if (second > 0)
		{
			secondView = passViews[second - 1];
			secondWidth = passWidths[second - 1];
			secondHeight = passHeights[second - 1];
		}

		// A fixed scale is a multiple of the frame rather than of the pass
		// before it, which is what the presets ask for.
		uint32_t passWidth = (source.scales[pass] > 0) ? (width * (uint32_t)source.scales[pass]) : targetWidth;
		uint32_t passHeight = (source.scales[pass] > 0) ? (height * (uint32_t)source.scales[pass]) : targetHeight;

		if (passWidth == 0) { passWidth = 1; }
		if (passHeight == 0) { passHeight = 1; }

		WGPUExtent3D size = WGPU_EXTENT_3D_INIT;
		size.width = passWidth;
		size.height = passHeight;
		size.depthOrArrayLayers = 1;

		WGPUTextureDescriptor textureDesc = WGPU_TEXTURE_DESCRIPTOR_INIT;
		textureDesc.size = size;
		textureDesc.mipLevelCount = 1;
		textureDesc.sampleCount = 1;
		textureDesc.dimension = WGPUTextureDimension_2D;
		textureDesc.format = WGPUTextureFormat_RGBA8Unorm;
		textureDesc.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding;

		state.texture = wgpuDeviceCreateTexture(this->device, &textureDesc);
		state.view = (state.texture != NULL) ? wgpuTextureCreateView(state.texture, NULL) : NULL;

		WGPUBufferDescriptor bufferDesc = WGPU_BUFFER_DESCRIPTOR_INIT;
		bufferDesc.size = sizeof(UpscalerConstants);
		bufferDesc.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
		bufferDesc.mappedAtCreation = false;

		state.constants = wgpuDeviceCreateBuffer(this->device, &bufferDesc);

		if ( (state.view == NULL) || (state.constants == NULL) )
		{
			printf("WGPU present: the %s filter cannot make pass %d of screen %d\n", source.name, pass + 1, screenIndex + 1);
			this->ReleaseUpscalerScreen(screen);
			return false;
		}

		// The sizes of this pass, written once: they change when the window or
		// the frame does, and not per frame.
		UpscalerConstants constants;
		memset(&constants, 0, sizeof(constants));

		constants.sourceSize[0] = (float)inputWidth;
		constants.sourceSize[1] = (float)inputHeight;
		constants.sourceSize[2] = 1.0f / (float)inputWidth;
		constants.sourceSize[3] = 1.0f / (float)inputHeight;
		constants.outputSize[0] = (float)passWidth;
		constants.outputSize[1] = (float)passHeight;
		constants.outputSize[2] = 1.0f / (float)passWidth;
		constants.outputSize[3] = 1.0f / (float)passHeight;
		constants.scale[0] = (float)passWidth / (float)inputWidth;
		constants.scale[1] = (float)passHeight / (float)inputHeight;
		constants.secondSize[0] = (float)secondWidth;
		constants.secondSize[1] = (float)secondHeight;
		constants.secondSize[2] = 1.0f / (float)secondWidth;
		constants.secondSize[3] = 1.0f / (float)secondHeight;

		wgpuQueueWriteBuffer(this->queue, state.constants, 0, &constants, sizeof(constants));

		WGPUBindGroupEntry entries[5] = {
			WGPU_BIND_GROUP_ENTRY_INIT,
			WGPU_BIND_GROUP_ENTRY_INIT,
			WGPU_BIND_GROUP_ENTRY_INIT,
			WGPU_BIND_GROUP_ENTRY_INIT,
			WGPU_BIND_GROUP_ENTRY_INIT
		};

		entries[0].binding = 0;
		entries[0].textureView = input;

		entries[1].binding = 1;
		entries[1].sampler = this->samplerNearest;

		entries[2].binding = 2;
		entries[2].buffer = state.constants;
		entries[2].offset = 0;
		entries[2].size = sizeof(UpscalerConstants);

		entries[3].binding = 3;
		entries[3].textureView = secondView;

		entries[4].binding = 4;
		entries[4].sampler = this->samplerNearest;

		WGPUBindGroupDescriptor groupDesc = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
		groupDesc.layout = this->upscalerBindGroupLayout;
		groupDesc.entryCount = 5;
		groupDesc.entries = entries;

		state.group = wgpuDeviceCreateBindGroup(this->device, &groupDesc);

		if (state.group == NULL)
		{
			printf("WGPU present: the %s filter cannot bind pass %d\n", source.name, pass + 1);
			this->ReleaseUpscalerScreen(screen);
			return false;
		}

		input = state.view;
		inputWidth = passWidth;
		inputHeight = passHeight;

		passViews[pass] = state.view;
		passWidths[pass] = passWidth;
		passHeights[pass] = passHeight;
	}

	// The last target is what this screen's quad samples, through the same bind
	// group layout as the frame itself.
	if (!this->CreateBindGroup(passViews[source.passCount - 1], this->samplerNearest, &screen.outputGroup))
	{
		this->ReleaseUpscalerScreen(screen);
		return false;
	}

	screen.passCount = source.passCount;

	printf("WGPU present: %s, screen %d %ux%u over %ux%u, %d pass(es)\n",
		   source.name, screenIndex + 1, width, height, targetWidth, targetHeight, source.passCount);

	return true;
}

// Copies this screen's rectangle out of the frame into the screen's own texture
// and runs the chain over it. The copy is what keeps the two screens apart: the
// first pass of a chain only ever sees the pixels of the screen it belongs to,
// so an edge filter cannot blend one screen into the other across the seam
// between them.
void WGPUPresent::Impl::DrawUpscalerChain(WGPUCommandEncoder encoder, int screenIndex, const WGPUPresentRect &sourceRect)
{
	UpscalerScreen &screen = this->upscalerScreens[screenIndex];

	if ( (screen.passCount == 0) || (screen.sourceTex == NULL) || (this->videoTex == NULL) )
	{
		return;
	}

	WGPUTexelCopyTextureInfo copySource = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
	copySource.texture = this->videoTex;
	copySource.origin.x = (uint32_t)sourceRect.left;
	copySource.origin.y = (uint32_t)sourceRect.top;

	WGPUTexelCopyTextureInfo copyDestination = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
	copyDestination.texture = screen.sourceTex;

	WGPUExtent3D copySize = WGPU_EXTENT_3D_INIT;
	copySize.width = screen.width;
	copySize.height = screen.height;
	copySize.depthOrArrayLayers = 1;

	wgpuCommandEncoderCopyTextureToTexture(encoder, &copySource, &copyDestination, &copySize);

	for (int i = 0; i < screen.passCount; i++)
	{
		WGPURenderPassColorAttachment colorAttachment = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
		colorAttachment.view = screen.passes[i].view;
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

		if (pass == NULL)
		{
			return;
		}

		wgpuRenderPassEncoderSetPipeline(pass, this->upscalerPipelines[i]);
		wgpuRenderPassEncoderSetBindGroup(pass, 0, screen.passes[i].group, 0, NULL);
		wgpuRenderPassEncoderDraw(pass, 4, 1, 0, 0);

		wgpuRenderPassEncoderEnd(pass);
		wgpuRenderPassEncoderRelease(pass);
	}
}

void WGPUPresent::Impl::Destroy()
{
	this->ready = false;

	this->ReleaseUpscalerChain();

	if (this->upscalerPipelineLayout != NULL) { wgpuPipelineLayoutRelease(this->upscalerPipelineLayout); this->upscalerPipelineLayout = NULL; }
	if (this->upscalerBindGroupLayout != NULL) { wgpuBindGroupLayoutRelease(this->upscalerBindGroupLayout); this->upscalerBindGroupLayout = NULL; }
	this->upscalerFailed = false;

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
	this->instanceBackends = 0;
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

bool WGPUPresent::begin(WGPUPresentWindow *window, uint32_t instanceBackends)
{
	if (_impl == NULL)
	{
		return false;
	}

	// Same window and same backend: keep what is already running.
	const bool isSameRequest = (_impl->window == window) && (_impl->instanceBackends == instanceBackends);

	if (_impl->ready && isSameRequest)
	{
		return true;
	}

	// A failure is only sticky for the request that produced it: picking another
	// backend (or window) deserves a fresh attempt.
	if (_impl->failed && isSameRequest)
	{
		return false;
	}

	// Start from a clean slate so a partially initialized attempt cannot leak or
	// keep a surface bound to a stale window.
	_impl->failed = false;
	_impl->Destroy();

	_impl->window = window;

	// CreateDevice() takes the backend mask and records it on the way in.
	if (!_impl->CreateDevice(instanceBackends))
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

void WGPUPresent::setUpscaler(int upscaler)
{
	if (_impl == NULL)
	{
		return;
	}

	if ( (upscaler < WGPUPresentUpscaler_Off) || (upscaler >= WGPUPresentUpscalerCount) )
	{
		upscaler = WGPUPresentUpscaler_Off;
	}

	if (_impl->upscaler == upscaler)
	{
		return;
	}

	_impl->upscaler = upscaler;

	// Turning the filters off gives the targets back at once, but keeps the
	// pipelines, so turning them on again costs nothing. Switching from one
	// filter to another releases the old chain when the next frame builds the
	// new one, in BuildUpscalerChain().
	if (upscaler == WGPUPresentUpscaler_Off)
	{
		_impl->ReleaseUpscalerScreen(_impl->upscalerScreens[0]);
		_impl->ReleaseUpscalerScreen(_impl->upscalerScreens[1]);
	}
}

int WGPUPresent::upscaler() const
{
	return (_impl != NULL) ? _impl->upscaler : WGPUPresentUpscaler_Off;
}

bool WGPUPresent::upscalerCompiled() const
{
	return (_impl != NULL) && (_impl->upscaler != WGPUPresentUpscaler_Off) && !_impl->upscalerFailed;
}

void WGPUPresent::setUpscalerShaderSource(WGPUPresentShaderSource *source)
{
	if (_impl == NULL)
	{
		return;
	}

	_impl->shaderSource = source;

	// Chains compiled from the previous source are stale.
	_impl->ReleaseUpscalerChain();
	_impl->upscalerFailed = false;
}

int WGPUPresent::upscalerCount()
{
	return WGPU_UPSCALER_COUNT;
}

const char *WGPUPresent::upscalerName(int upscaler)
{
	const int index = UpscalerIndex(upscaler);

	return (index < 0) ? NULL : g_upscalers[index].name;
}

bool WGPUPresent::isUpscaler(int upscaler)
{
	return (upscaler == WGPUPresentUpscaler_Off) || (UpscalerIndex(upscaler) >= 0);
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

	// The GPU filter, when one is selected, is set up before the quads are
	// built: which texture a screen's quad samples decides the uv its vertices
	// carry. Each screen has a chain of its own over its own copy of its half of
	// the frame, so a filter never reads across the seam between the two.
	// Rotation is left out: the filters want upright pixels, and a rotated
	// frame would feed them sideways taps.
	bool chain[2] = { false, false };

	if ( (d.upscaler != WGPUPresentUpscaler_Off) && (rotation == 0) )
	{
		for (int i = 0; i < 2; i++)
		{
			const int screenWidth  = srcRects[i].right - srcRects[i].left;
			const int screenHeight = srcRects[i].bottom - srcRects[i].top;
			const int targetWidth  = dstRects[i].right - dstRects[i].left;
			const int targetHeight = dstRects[i].bottom - dstRects[i].top;

			if ( (screenWidth <= 0) || (screenHeight <= 0) || (targetWidth <= 0) || (targetHeight <= 0) )
			{
				continue;
			}

			// A screen is drawn at the size of its destination rectangle, which
			// is also how large its last target is. The cap keeps a window
			// larger than the device limit from asking for a texture it cannot
			// have; the sampler then does the last stretch.
			uint32_t chainWidth  = (uint32_t)((targetWidth  > WGPU_UPSCALER_MAX_TARGET) ? WGPU_UPSCALER_MAX_TARGET : targetWidth);
			uint32_t chainHeight = (uint32_t)((targetHeight > WGPU_UPSCALER_MAX_TARGET) ? WGPU_UPSCALER_MAX_TARGET : targetHeight);

			chain[i] = d.PrepareUpscaler(i, (uint32_t)screenWidth, (uint32_t)screenHeight, chainWidth, chainHeight);
		}
	}

	// Build the quads. Positions are converted straight to NDC here because the
	// window size is known; UVs are permuted exactly like the OpenGL path so
	// that screen rotation matches.
	Vertex vertices[18];
	int vertexCount = 0;
	int screenVertexStart[2] = { 0, 0 };

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

		// A chained screen samples a texture that holds that screen and nothing
		// else, so its uv runs over the whole of it rather than over a sub-
		// rectangle of the frame.
		const float u1 = chain[i] ? 0.0f : (float)s.left   / (float)width;
		const float u2 = chain[i] ? 1.0f : (float)s.right  / (float)width;
		const float v1 = chain[i] ? 0.0f : (float)s.top    / (float)height;
		const float v2 = chain[i] ? 1.0f : (float)s.bottom / (float)height;

		const float u[4] = { u1, u2, u2, u1 };
		const float v[4] = { v1, v1, v2, v2 };

		const float us[4] = { u[(ofs + 0) % 4], u[(ofs + 1) % 4], u[(ofs + 2) % 4], u[(ofs + 3) % 4] };
		const float vs[4] = { v[(ofs + 0) % 4], v[(ofs + 1) % 4], v[(ofs + 2) % 4], v[(ofs + 3) % 4] };

		const float xs[4] = { ndcX(dstLeft), ndcX(dstRight), ndcX(dstRight), ndcX(dstLeft) };
		const float ys[4] = { ndcY(dstTop), ndcY(dstTop), ndcY(dstBottom), ndcY(dstBottom) };

		screenVertexStart[i] = vertexCount;

		pushQuad(xs, ys, us, vs, 1.0f, 1.0f, 1.0f);
	}

	wgpuQueueWriteBuffer(d.queue, d.vertexBuffer, 0, vertices, (size_t)vertexCount * sizeof(Vertex));

	WGPUCommandEncoderDescriptor encoderDesc = WGPU_COMMAND_ENCODER_DESCRIPTOR_INIT;
	WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(d.device, &encoderDesc);

	for (int i = 0; i < 2; i++)
	{
		if (chain[i])
		{
			d.DrawUpscalerChain(encoder, i, srcRects[i]);
		}
	}

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

	// Each screen is its own draw, because a chained screen samples a texture of
	// its own: the two quads no longer share one bind group.
	for (int i = 0; i < 2; i++)
	{
		WGPUBindGroup group = d.videoBindGroup;

		if (chain[i] && (d.upscalerScreens[i].outputGroup != NULL))
		{
			group = d.upscalerScreens[i].outputGroup;
		}

		if (group == NULL)
		{
			continue;
		}

		wgpuRenderPassEncoderSetBindGroup(pass, 0, group, 0, NULL);
		wgpuRenderPassEncoderDraw(pass, 6, 1, (uint32_t)screenVertexStart[i], 0);
	}

	wgpuRenderPassEncoderEnd(pass);

	WGPUCommandBufferDescriptor commandBufferDesc = WGPU_COMMAND_BUFFER_DESCRIPTOR_INIT;
	WGPUCommandBuffer commandBuffer = wgpuCommandEncoderFinish(encoder, &commandBufferDesc);

	wgpuQueueSubmit(d.queue, 1, &commandBuffer);

	wgpuCommandBufferRelease(commandBuffer);
	wgpuRenderPassEncoderRelease(pass);
	wgpuCommandEncoderRelease(encoder);

	// Present before dropping the surface texture: wgpu-native needs the
	// acquired image to still be alive while it hands it to the compositor.
	const WGPUStatus presentStatus = wgpuSurfacePresent(d.surface);

	wgpuTextureViewRelease(targetView);
	wgpuTextureRelease(surfaceTexture.texture);

	if (presentStatus != WGPUStatus_Success)
	{
		d.surfaceWidth = 0;
		d.surfaceHeight = 0;
		return false;
	}

	return true;
}

#endif // ENABLE_WGPU
