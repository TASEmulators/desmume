# Linux 视频录制（FFmpeg 编码器枚举）设计文档

状态：设计已确认，待实现
适用范围：POSIX / GTK3 前端（Linux、BSD 等使用 `frontend/posix` 的平台）
相关平台实现：Windows Media Foundation（`frontend/windows/videoencoders.cpp`、`mfvideowriter.cpp`）、macOS VideoToolbox（`frontend/cocoa/userinterface/MacVideoToolboxCapture.mm`）

---

## 1. 目标

在 Linux 上提供与 Windows / macOS 新版录制功能对齐的视频录制：

1. 通过 FFmpeg（libavcodec）**枚举本机可用的全部视频编码器**，由用户选择。
2. 列表中明确标注每个编码器是 **硬件编码** 还是 **软件编码**。
3. 同时支持视频与音频，复用到一个容器文件（MKV / MP4），不再像旧版那样视频、音频分成两个文件。
4. 构建期由 Meson 请求 FFmpeg 库；找不到库时**整个功能编译期关闭**，不影响旧功能构建。
5. 旧录制功能（外部 `x264` / `flac` 子进程）**原样保留**，移到菜单的 “Old recording” 子菜单。
6. 录制在独立工作线程完成，不阻塞 GTK 主线程/模拟线程。

### 非目标

- 不做 VAAPI 专用路径。VAAPI 只是 FFmpeg 枚举结果中的一种编码器。
- 不做 GPU 零拷贝（DMA-BUF / Vulkan external memory）。当前 GTK 最终画面本来就在 CPU 侧。
- 不做 Windows/macOS 那样的“窗口布局 / 旋转 / CPU 滤镜合成”画面。第一版固定录制原生双屏缓冲。
- 不改 GTK2 前端。GTK2 继续只提供旧录制。
- 不改 Windows / macOS 实现。

---

## 2. 总体方案

### 2.1 平台能力映射

| 能力 | Windows 新版 | macOS 新版 | Linux（本设计） |
|---|---|---|---|
| 编码 | Media Foundation MFT | VideoToolbox | libavcodec（`AVCodec`） |
| 编码器枚举 | `MFTEnumEx` | `VTCopyVideoEncoderList` | `av_codec_iterate()` |
| 硬件标记 | `MFT_ENUM_FLAG_HARDWARE` | `kVTVideoEncoderList_IsHardwareAccelerated` | `AV_CODEC_CAP_HARDWARE` / `AV_CODEC_CAP_HYBRID` |
| 封装 | `IMFSinkWriter` | `AVAssetWriter` | libavformat（`avformat_write_header` 等） |
| 像素格式 | NV12 | BGRA | 由编码器协商（优先 NV12/YUV420P） |
| 音频 | AAC / ALAC | AAC / ALAC / LPCM | libavcodec + libswresample |
| 队列/线程 | `MFVideoSink` / `NDSCaptureObject` | `ClientAVCaptureObject` | 新的工作线程 + 有界队列 |

### 2.2 数据流

```
NDS 帧结束（GTK 主线程）
  Gtk3GPUEventHandler::DidFrameEnd()
      └─ sFrameSerial++
                                                            ↓
  PresentFrame()（Lua GUI 回调之后）
      ├─ 绘制 HUD/Lua（CPU 路径直接画进 src；WGPU 路径画进 HUD 纹理）
      ├─ video->RunFilter()   // src -> dst（RGBA/BGRA、可放大到 2x/3x/4x）
      └─ 录制开启时：
           avout_ffmpeg.updateVideo(video->GetDstBufferPtr(), dstW, dstH, BGRA)
           // 可选：WGPU HUD 纹理读回并按 dst 尺寸双线性合成
                                                            ↓
SPU 每帧音频（GTK 主线程）
  GtkDriver::AVI_SoundUpdate(outbuf, samples)
      └─ avout_ffmpeg.updateAudio(...)                    // 拷贝 s16 stereo
                                                            ↓
                                              +-------------+-------------+
                                              |  有界视频队列 / 音频队列   |
                                              +-------------+-------------+
                                                            ↓
                                              工作线程（std::thread）
                                                ├─ sws_scale BGR555/RGBA/BGRA → NV12/YUV420P
                                                │  （源尺寸与输出尺寸不同时重建缩放器）
                                                ├─ 可选 hwframe 上传（VAAPI/QSV/Vulkan）
                                                ├─ avcodec_send_frame / receive_packet
                                                ├─ swr_convert DESMUME_SAMPLE_RATE → 编码器采样率
                                                └─ av_interleaved_write_frame（libavformat）
                                                            ↓
                                                       .mkv / .mp4
```

若关闭“Include video filter, Lua and OSD”，则仍在 `DidFrameEnd()` 中直接录制
`masterCustomBuffer`（原生双屏、不含滤镜/HUD）。两种模式共用同一个编码队列。

### 2.3 为什么用 FFmpeg 而不是裸 libva

- VAAPI 只负责编码，不负责 MP4/MKV 封装和音频复用；Windows/macOS 的 SinkWriter/AVAssetWriter 是“编码 + 封装”一体的。
- libavformat 提供成熟的 muxer、时间戳/交织、extradata 处理。
- libavcodec 可以统一枚举 `h264_vaapi`、`h264_nvenc`、`h264_qsv`、`h264_v4l2m2m`、`libx264`、`h264_vulkan` 等所有后端，正好满足“枚举所有编码器”的需求。
- 软件编码回退（`libx264` 等）天然可用。

---

## 3. 构建系统设计

### 3.1 Meson 选项

在 `desmume/src/frontend/posix/meson_options.txt` 增加：

```meson
option('ffmpeg-recording',
  type: 'boolean',
  value: true,
  description: 'Build the FFmpeg-based GTK video recorder (requires libavcodec, libavformat, libavutil, libswscale and libswresample)',
)
```

### 3.2 依赖探测

在 `desmume/src/frontend/posix/meson.build` 顶部（target 定义之前）增加：

```meson
dep_libavcodec    = dependency('libavcodec',    version: '>=59.37', required: false)
dep_libavformat   = dependency('libavformat',   required: false)
dep_libavutil     = dependency('libavutil',     version: '>=57.28', required: false)
dep_libswscale    = dependency('libswscale',    required: false)
dep_libswresample = dependency('libswresample', required: false)

ffmpeg_recording = get_option('ffmpeg-recording') \
  and dep_libavcodec.found() \
  and dep_libavformat.found() \
  and dep_libavutil.found() \
  and dep_libswscale.found() \
  and dep_libswresample.found()

if ffmpeg_recording
  add_global_arguments('-DHAVE_FFMPEG_RECORD', language: ['c', 'cpp'])
  message('FFmpeg video recording: enabled')
else
  message('FFmpeg video recording: disabled (missing libav* or -Dffmpeg-recording=false)')
endif
```

版本下限说明：

- `libavcodec >= 59.37`：`AVChannelLayout` / `ch_layout`（FFmpeg 5.1）。
- `libavutil >= 57.28`：`swr_alloc_set_opts2()`（FFmpeg 5.1）。
- `avcodec_get_supported_config()` 仅在新版本存在；源码内用
  `#if LIBAVCODEC_VERSION_MAJOR >= 62` 分支，旧版本回退到 `codec->pix_fmts`。
  对于仍保留旧字段的 `61.x` 等版本，会走旧字段分支，编译不受影响。

本机实测（Fedora/Asahi aarch64，2026-11）：

- `libavcodec 63.1.102`、`libavformat 63.1.102`、`libavutil 61.1.102`、
  `libswscale 10.1.102`、`libswresample 7.1.102`。
- `LIBAVCODEC_VERSION_MAJOR == 63`，`AVCodec` 结构体里已经**没有**
  `pix_fmts` / `sample_fmts` 字段，只能用 `avcodec_get_supported_config()`。
- 头文件安装在 `/usr/include/ffmpeg/` 下，pkg-config 提供 `-I/usr/include/ffmpeg`。
  Meson 的 `dependency('libavcodec')` 会自动带上该路径，源码里不要硬编码
  `/usr/include` 或手工拼 include 路径。

### 3.3 GTK3 目标

在 `desmume/src/frontend/posix/gtk/meson.build`：

```meson
if ffmpeg_recording
  desmume_src += ['../shared/avout_ffmpeg.cpp']
  gtk_dependencies += [
    dep_libavcodec,
    dep_libavformat,
    dep_libavutil,
    dep_libswscale,
    dep_libswresample,
  ]
endif
```

新实现只进 GTK3 可执行文件，不塞进 `libdesmume`，避免 CLI / GTK2 被迫链接 FFmpeg。

---

## 4. 新增模块

### 4.1 文件

- 新增：`desmume/src/frontend/posix/shared/avout_ffmpeg.h`
- 新增：`desmume/src/frontend/posix/shared/avout_ffmpeg.cpp`

头文件不包含任何 FFmpeg 头，GTK 代码只依赖下面的稳定接口；FFmpeg 细节全部藏在 `Impl` 中（pimpl）。

### 4.2 公共接口

```cpp
#ifndef _AVOUT_FFMPEG_H_
#define _AVOUT_FFMPEG_H_

#include <string>
#include <vector>
#include "types.h"

// 原生双屏缓冲尺寸（上下两屏拼接）
#define AVOUT_FFMPEG_WIDTH   256
#define AVOUT_FFMPEG_HEIGHT  384

// 输入音频采样率使用 SPU.h 的 DESMUME_SAMPLE_RATE（当前 44100），16-bit，双声道。
// avout_ffmpeg.cpp 直接 #include "SPU.h"，不复制这个常量。

struct FFmpegEncoderInfo
{
    std::string name;       // avcodec 编码器名，例如 "h264_vaapi"
    std::string longName;   // codec->long_name
    bool hardware;          // AV_CODEC_CAP_HARDWARE / AV_CODEC_CAP_HYBRID
};

struct FFmpegRecordConfig
{
    std::string videoEncoder;    // 必填，默认 "libx264"
    std::string audioEncoder;    // 空 = 不录音频
    std::string container;       // "matroska"（默认）或 "mp4"
    int videoBitrateKbps;        // 默认 8000
    int audioBitrateKbps;        // 默认 192
    int width;                   // 0 = 原生 256（滤镜输出时取 dst 宽度）
    int height;                  // 0 = 原生 384（滤镜输出时取 dst 高度）

    FFmpegRecordConfig();
};

// 捕获缓冲的像素布局；头文件不引入 FFmpeg 头
enum FFmpegCaptureFormat
{
    FFmpegCaptureFormat_BGR555LE,   // 16-bit，R 在低 5 位（masterCustomBuffer 的 15-bit 模式）
    FFmpegCaptureFormat_RGBA8888,   // 32-bit，内存字节序 R,G,B,A（masterCustomBuffer 的 18/24-bit 模式）
    FFmpegCaptureFormat_BGRA8888    // 32-bit，内存字节序 B,G,R,A（video filter 的 src/dst）
};

class AVOutFFmpeg
{
public:
    AVOutFFmpeg();
    ~AVOutFFmpeg();

    bool begin(const char* filename, const FFmpegRecordConfig& config);
    void end();
    bool isRecording() const;
    bool hasFailed() const;
    void clearFailure();             // UI 显示完 lastError() 后清除失败状态
    std::string lastError() const;

    // 每帧调用：宽高为源缓冲尺寸。输出尺寸由 config.width/height 决定；
    // 两者不同时由 sws_scale 缩放。数据在返回前已拷贝。
    void updateVideo(const void* buffer, int width, int height,
                     FFmpegCaptureFormat format);

    // 每帧音频：s16 双声道交织，sampleCount 为每声道采样数
    void updateAudio(const void* data, int sampleCount);

private:
    struct Impl;
    Impl* impl;
};

namespace FFmpegRecorder
{
    // 全部已注册的 FFmpeg 视频/音频编码器，按“硬件优先、名称升序”排序
    std::vector<FFmpegEncoderInfo> videoEncoders();
    std::vector<FFmpegEncoderInfo> audioEncoders();

    // "<name> - <long name> [Hardware|Software]"；供 GTK 下拉框直接使用
    std::string encoderLabel(const FFmpegEncoderInfo& info);
}

#endif
```

### 4.3 编码器枚举

实现要点：

```cpp
void* opaque = nullptr;
const AVCodec* codec;
while ((codec = av_codec_iterate(&opaque)) != nullptr)
{
    if (!av_codec_is_encoder(codec) || codec->type != AVMEDIA_TYPE_VIDEO)
        continue;

    const bool hw = (codec->capabilities & (AV_CODEC_CAP_HARDWARE |
                                            AV_CODEC_CAP_HYBRID)) != 0;
    out.push_back({ codec->name, codec->long_name ? codec->long_name : "", hw });
}
```

- **不过滤**具体 codec id；用户能看到 `libx264`、`h264_vaapi`、`h264_nvenc`、`h264_v4l2m2m`、`ffv1`、`rawvideo` 等全部编码器。
- 排序：硬件在前，其次按 `name` 字典序。
- 音频同理，额外在 UI 侧加一行 “None (no audio)”。
- 枚举只读 `AVCodec` 元数据，不初始化设备、不探测硬件是否存在；编码器是否真的可用，在 `begin()` 打开时判定并给出错误。

### 4.4 编码器/像素格式协商

优先级（软件格式优先，硬件格式作为兜底）：

```
NV12 > YUV420P > YUVJ420P > YUV422P > YUV444P > BGR0 > BGRA > RGB24 > GBRP > GRAY8
```

支持的像素格式列表：

- `LIBAVCODEC_VERSION_MAJOR >= 62`：
  `avcodec_get_supported_config(NULL, codec, AV_CODEC_CONFIG_PIX_FORMAT, 0, &cfg, &n)`
- 旧版本：遍历 `codec->pix_fmts`。
- 返回 `NULL` / `n == 0` 表示**不限制**（不是“不支持”）：例如 `h264_v4l2m2m`
  返回 `n=0`，依次尝试 `NV12`、`YUV420P` 打开。

本机实测（libavcodec 63）的部分返回结果：

| 编码器 | `n` | 返回的像素格式 | 选择结果 |
|---|---|---|---|
| `h264_vaapi` | 1 | `vaapi` | 硬件帧路径（VAAPI device + hwframes） |
| `h264_vulkan` | 1 | `vulkan` | 硬件帧路径（Vulkan device + hwframes） |
| `prores_ks_vulkan` | 1 | `vulkan` | 硬件帧路径（Vulkan device + hwframes） |
| `h264_nvenc` | 23 | `yuv420p nv12 … cuda` | 软件路径，优先选 `nv12`；**不要**因为有 `cuda` 就切到 hwframes |
| `h264_v4l2m2m` | 0 | `NULL`（不限制） | 依次尝试 `nv12` → `yuv420p` |
| `libx264` | 15 | `yuv420p … nv12 …` | 软件路径，选 `nv12`，无需先转 YUV420P |
| `libx265` | 20 | `yuv420p … gbrp …` | 软件路径，选 `yuv420p` |
| `ffv1` | 62 | 大量 `yuv*` / `bgr0` / ... | 软件路径，按优先级取第一个 |

选择规则因此必须是：**先在支持列表里找可用的软件格式；一个都没有时，才把
硬件格式当作编码器输入格式并创建 hwframes。** 不能只看编码器是否带
`AV_CODEC_CAP_HARDWARE`，因为 `h264_nvenc` / `h264_v4l2m2m` 这类编码器虽然标为
硬件，但接受软件帧输入，由 libavcodec 内部上传，性能与兼容性都更好。

如果编码器只支持硬件像素格式（如 `AV_PIX_FMT_VAAPI`、`AV_PIX_FMT_QSV`、`AV_PIX_FMT_VULKAN`、`AV_PIX_FMT_CUDA`）：

```cpp
AVHWDeviceType type;
switch (hwFmt) {
    case AV_PIX_FMT_VAAPI:  type = AV_HWDEVICE_TYPE_VAAPI;  break;
    case AV_PIX_FMT_QSV:    type = AV_HWDEVICE_TYPE_QSV;    break;
    case AV_PIX_FMT_VULKAN: type = AV_HWDEVICE_TYPE_VULKAN; break;
    case AV_PIX_FMT_CUDA:   type = AV_HWDEVICE_TYPE_CUDA;   break;
    default: /* 不支持，报错 */;
}

const char* device = nullptr;
if (type == AV_HWDEVICE_TYPE_QSV) device = "auto";
else if (type == AV_HWDEVICE_TYPE_CUDA) device = "0";
// VAAPI 可选：环境变量 DESMUME_VAAPI_DEVICE=/dev/dri/renderD128
else if (type == AV_HWDEVICE_TYPE_VAAPI) device = getenv("DESMUME_VAAPI_DEVICE");

av_hwdevice_ctx_create(&hwDevice, type, device, nullptr, 0);

AVBufferRef* hwFrames = av_hwframe_ctx_alloc(hwDevice);
auto* frames = (AVHWFramesContext*)hwFrames->data;
frames->format = hwFmt;
frames->sw_format = AV_PIX_FMT_NV12;
frames->width = vctx->width;
frames->height = vctx->height;
frames->initial_pool_size = 20;
av_hwframe_ctx_init(hwFrames);
```

- `nvenc` / `amf` / `v4l2m2m` 的软件像素格式路径不建 hwframes；libavcodec 内部完成上传。
- VAAPI 设备默认由 libva 选择；多 GPU 环境可用 `DESMUME_VAAPI_DEVICE` 指定 render node。

### 4.5 视频编码

`AVCodecContext` 关键字段：

```cpp
vctx->width  = (config.width  > 0) ? config.width  : AVOUT_FFMPEG_WIDTH;   // 默认 256
vctx->height = (config.height > 0) ? config.height : AVOUT_FFMPEG_HEIGHT;  // 默认 384
vctx->time_base = { 560190, 33513982 };   // 1 / 59.8261...
vctx->framerate = { 33513982, 560190 };
vctx->bit_rate  = config.videoBitrateKbps * 1000;
vctx->gop_size  = 60;
vctx->max_b_frames = 0;
vctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;  // mp4/mkv 需要 extradata
```

- 输入格式有三种：
  - `masterCustomBuffer`，15-bit 模式：`AV_PIX_FMT_BGR555LE`；
  - `masterCustomBuffer`，18/24-bit 模式：内存为 R,G,B,A，即 `AV_PIX_FMT_RGBA`；
  - 视频滤镜输出 `video->GetDstBufferPtr()`：内存为 B,G,R,A，即 `AV_PIX_FMT_BGRA`。
- 未开启滤镜输出录制时，源缓冲是 `masterCustomBuffer`，尺寸为
  `customWidth × customHeight × 2`，输出固定为 config 指定尺寸（默认 256×384）。
- 开启滤镜输出录制时，源缓冲是滤镜 dst，尺寸为 `GetDstWidth() × GetDstHeight()`，
  输出尺寸在 `begin()` 时按该值固定。若 GPU 缩放或滤镜在录制过程中变化，
  用 `sws_getCachedContext()` 动态重建缩放器，仍写入初始输出尺寸。
- `sws_scale` 的输出格式是编码器协商出的软件格式（优先 NV12/YUV420P）。
- 硬件格式：先 `sws_scale` 到 NV12 的暂存 frame，再
  `av_hwframe_get_buffer()` + `av_hwframe_transfer_data()`。
- PTS：每来一个捕获帧 `frameIndex++`；`frame->pts = frameIndex`。队列满时丢弃**新帧**但 `frameIndex` 仍递增，保持时间轴不塌缩。

### 4.6 音频编码

- 输入：s16、双声道、`DESMUME_SAMPLE_RATE`（当前 44100 Hz）。
- 输出采样率：从 `AV_CODEC_CONFIG_SAMPLE_RATE` 查询，**优先 48000**；列表存在但不含
  48000 时取第一个；返回 `NULL` / 空表示不限制，使用 48000。不能假定所有编码器都支持
  48000，`time_base` 必须跟随最终采样率。
- 选采样格式：优先 `S16 > FLTP > FLT > S32 > S16P > S32P`；支持列表里没有这些时取第一个；
  返回 `NULL` / 空表示不限制，按 `S16`。使用
  `avcodec_get_supported_config(..., AV_CODEC_CONFIG_SAMPLE_FORMAT, ...)`；旧版本遍历
  `codec->sample_fmts`。
- 声道布局：从 `AV_CODEC_CONFIG_CHANNEL_LAYOUT` 查询，优先 stereo；列表存在但没有
  stereo 时取第一个；返回 `NULL` / 空表示不限制，用 `av_channel_layout_default(2)`。
  输入固定 stereo，由 swr 完成上/下混。
- `AVCodecContext`：
  ```cpp
  actx->sample_rate = chosenSampleRate;      // 通常 48000
  av_channel_layout_copy(&actx->ch_layout, &chosenLayout);
  actx->sample_fmt = chosenSampleFmt;
  actx->bit_rate = config.audioBitrateKbps * 1000;
  actx->time_base = { 1, chosenSampleRate };
  ```
- 重采样：`swr_alloc_set_opts2()`，输入 stereo/s16/`DESMUME_SAMPLE_RATE`，输出
  `chosenSampleFmt`/`chosenSampleRate`/`chosenLayout`。
- 使用 `AVAudioFifo` 累积重采样后的样本；凑满 `actx->frame_size` 才编码。
- 采样 PTS 以输出采样数计：`frame->pts = audioSampleCount; audioSampleCount += frame->nb_samples;`
- 结束时先 flush `swr`，再把 FIFO 中不足一帧的样本用静音补齐送编码器，最后发送
  `nullptr` 帧 flush 编码器。

本机实测（libavcodec 63）的音频编码器配置：

| 编码器 | 采样格式 | 采样率 | 声道布局 |
|---|---|---|---|
| `aac` | `fltp`（仅此一种） | 96000 … 48000 … 7350 | 不限制 |
| `flac` | `s16 s32` | 不限制 | 不限制 |
| `libopus` | `s16 flt` | 48000 24000 16000 12000 8000 | 不限制 |
| `libvorbis` | `fltp` | 不限制 | 不限制 |
| `opus`（原生） | `fltp` | 仅 48000 | mono stereo |
| `alac` | `s32p s16p` | 不限制 | mono … 7.1(wide) |
| `libmp3lame` | `s32p fltp s16p` | 44100 48000 32000 … | mono stereo |
| `pcm_s16le` | `s16` | 不限制 | 不限制 |

结论：

- **AAC 只接受 FLTP**，所以“S16 优先”必须能正确回退到 FLTP；不能把 S16 写死。
- 部分编码器只接受平面格式（`s16p` / `s32p`），优先列表必须包含它们。
- `opus` 只支持 48000，说明“优先 48000”的设计在常见编码器上是成立的；同时保留
  按编码器列表回退的能力，避免未来编码器不支持 48000 时直接打开失败。
- 音频编码器全部没有 `AV_CODEC_CAP_HARDWARE`（本机 87 个音频编码器，硬件标记数为 0），
  UI 仍统一显示 `[Software]`，保持与视频列表一致。

### 4.7 封装与时间戳

- `avformat_alloc_output_context2(&fmt, NULL, config.container.c_str(), filename)`。
- 视频流 `time_base = vctx->time_base`，音频流 `time_base = {1, chosenSampleRate}`
  （通常为 48000）。
- `avformat_write_header()` 可能改写流 `time_base`，之后用 `av_packet_rescale_ts()`。
- 所有写包统一走 `av_interleaved_write_frame()`，由 libavformat 处理 A/V 交织。
- 容器默认 **MKV**（异常退出时文件仍可播放）；MP4 作为可选项。
- 容器兼容性分两阶段检查：
  1. `begin()` 里先调用 `avformat_query_codec(fmt->oformat, codec->id, FF_COMPLIANCE_NORMAL)`；
     返回 0 的组合（例如 `rawvideo` + `mp4`）在创建文件前就报错。
  2. 该 API 对少数组合会误报支持（例如 `libvorbis` + `mp4`），只能等到 trailer 阶段失败；
     `av_write_trailer()` 的错误如今会保存到 `lastError()`，停止录制时弹窗提示文件可能不完整。
- `begin()` 失败时会删除已经创建的空文件或只有文件头的半成品。

### 4.8 线程模型与队列

- 一个专用 `std::thread` 负责全部编码与 mux（libavformat 不是线程安全的，不能多写线程）。
- 捕获侧只做拷贝和入队：
  - 视频队列：`std::deque<VideoJob>`，每个 job 保存原始捕获帧（BGR555 或 BGRA）；
    最多 60 帧，且总字节数上限 64 MB（避免高 GPU 缩放时占用过多内存）。
  - 音频队列：`std::deque<std::vector<int16_t>>`，深度按采样数限制（例如 1 秒）。
  - `std::mutex` + `std::condition_variable`。
- 队列满时：视频丢新帧并计数；音频丢块并计数（正常情况不会发生）。
- 工作线程循环：
  1. 等待 “stop 或 有任务”。
  2. 取一帧视频或一块音频处理。
  3. `send_frame` → `receive_packet` → 写包。
  4. stop 且队列为空 → 退出循环。
- `end()`：置 `stop`，`notify_all()`，`join()`，然后 `av_write_trailer()`、`avio_closep()`、释放资源。
- 运行时错误（编码/写包失败）设置 `failed = true`、保存 `lastError()`，清空队列；UI 通过 1 秒轮询恢复按钮状态。

### 4.9 失败与错误信息

- `begin()` 同步完成打开编码器、创建设备、写文件头；任何失败都返回 `false`，`lastError()` 提供给 UI。
- 使用 `av_strerror()` 把 FFmpeg 错误码转成可读文本。
- 典型错误：
  - 编码器不存在或被移除。
  - 硬件编码器初始化失败（无 GPU、无驱动、无 `/dev/dri/renderD*` 权限）。
  - 编码器/容器组合不支持。
  - 输出路径不可写。

---

## 5. GTK3 集成

### 5.1 菜单（`frontend/posix/gtk/menu.ui`）

把现有 “Record video/audio” 子菜单改成：

```
Record _video/audio
├── Record _video (FFmpeg)…        app.record_video        ← 新增
├── _Stop recording                app.record_stop
└── _Old recorders                 (submenu)
    ├── Record lossless H._264 (video only)…   app.record_x264
    └── Record _FLAC (audio only)…              app.record_flac
```

同时修正现有 bug：`app.stop_recording` 改为已注册的 `app.record_stop`。

未编译 FFmpeg 录制时：

- 不注册 `record_video` action。
- 菜单项仍由 `menu.ui` 生成，但因 action 不存在而自动显示为不可用；
- 旧 `x264` / `flac` 项可正常使用。

### 5.2 动作

在 `main.cpp` 的 `app_entries[]` 中：

```cpp
{ "record_video", RecordAV_Video },   // 仅 HAVE_FFMPEG_RECORD 时注册
{ "record_stop",  RecordAV_stop  },
```

`RecordAV_x264` / `RecordAV_flac` 保留，行为不变。

### 5.3 录制设置对话框

`RecordAV_Video()` 流程：

1. 若有任意录制在运行（旧 x264 / 旧 flac / 新 FFmpeg），提示并返回。
2. 暂停模拟（与旧录制一致）。
3. 弹出 `GtkDialog`：
   - **Video encoder**：`GtkComboBoxText`，内容来自 `FFmpegRecorder::videoEncoders()`；
     文本格式：`"<name> — <longName> [Hardware]"` / `[Software]`。
     默认值依次尝试：配置中保存的编码器 → `libx264` → 第一个软件编码器。
   - **Audio encoder**：第一项 “None (no audio)”，其后为全部音频编码器（同样标 Hardware/Software）。
     默认：配置值 → `aac` → None。
   - **Container**：`Matroska (.mkv)` / `MP4 (.mp4)`，默认 MKV。
   - **Video bitrate (kbps)**：`GtkSpinButton`，范围 100–100000，默认 8000。
   - **Audio bitrate (kbps)**：范围 32–512，默认 192；Audio=None 时禁用。
   - **Include video filter, Lua and OSD**：复选框，默认开启；开启时录制滤镜输出
     （尺寸跟随滤镜，例如 2x = 512×768），关闭时录制原生 `masterCustomBuffer`。
   - 说明文字：`Captures the shown frame at 59.83 fps.`
4. 用户确认后，弹出 `GtkFileChooserNative` 保存文件：
   - 过滤器跟随容器（`*.mkv` / `*.mp4`）。
   - 文件名缺扩展名时自动补 `.mkv` / `.mp4`。
5. 调 `avout_ffmpeg.begin(path, cfg)`：
   - 成功：禁用 `record_video`、`record_x264`、`record_flac`，启用 `record_stop`，
     状态栏显示 `Recording to <path>`。
   - 失败：弹错误对话框，内容为 `lastError()`。
6. 保存本次选择到配置文件（键见 5.6）。

### 5.4 停止

`RecordAV_stop()`：

```cpp
avout_x264.end();
avout_flac.end();
#ifdef HAVE_FFMPEG_RECORD
avout_ffmpeg.end();
#endif
```

随后重新启用开始类 action，状态栏显示 `Recording stopped`。

### 5.5 帧/音频钩子

视频，`Gtk3GPUEventHandler::DidFrameEnd()`：

```cpp
// 旧 x264 管道录制器仍然只认原生 16-bit 缓冲。
avout_x264.updateVideo(latestDisplayInfo.masterNativeBuffer16);

#ifdef HAVE_FFMPEG_RECORD
sFrameSerial++;   // 真实模拟帧；RedrawScreen() 不会递增

if (!sRecordFilteredOutput)
{
    // 18/24-bit masterCustomBuffer 的内存顺序是 R,G,B,A。
    const FFmpegCaptureFormat captureFormat =
        (latestDisplayInfo.colorFormat == NDSColorFormat_BGR555_Rev)
            ? FFmpegCaptureFormat_BGR555LE
            : FFmpegCaptureFormat_RGBA8888;
    avout_ffmpeg.updateVideo(latestDisplayInfo.masterCustomBuffer,
                             (int)latestDisplayInfo.customWidth,
                             (int)(latestDisplayInfo.customHeight * 2),
                             captureFormat);
}
#endif
```

滤镜输出模式在 `PresentFrame()` 中、`video->RunFilter()` 之后采集：

```cpp
video->RunFilter();

if (sRecordFilteredOutput && avout_ffmpeg.isRecording() &&
    sFrameSerial != sLastRecordedFrameSerial)
{
    sLastRecordedFrameSerial = sFrameSerial;

    const int w = (int)video->GetDstWidth();
    const int h = (int)video->GetDstHeight();
    const u32 *pixels = video->GetDstBufferPtr();

    sRecordFrameBuffer.resize((size_t)w * h * 4);
    memcpy(sRecordFrameBuffer.data(), pixels, sRecordFrameBuffer.size());

    // WGPU HUD 是单独的纹理；读回并双线性缩放到滤镜尺寸后合成。
    WGPU_HudCompositeFrame(sRecordFrameBuffer.data(), w, h);
    avout_ffmpeg.updateVideo(sRecordFrameBuffer.data(), w, h,
                             FFmpegCaptureFormat_BGRA8888);
}
```

`PresentFrame()` 也会被 `RedrawScreen()`（菜单、缩放、暂停）直接调用，所以用
`sFrameSerial` 而不是 `sFramePresentPending` 来判断“这是不是一个新模拟帧”。

> 早期版本读的是 `masterNativeBuffer16`。GTK3 在 18/24-bit 色彩模式下把 3D 屏幕直接渲染到
> `masterCustomBuffer`，原生 16-bit 缓冲里没有这一层，于是录像的 3D 屏幕全黑。改用
> `masterCustomBuffer` 后两个屏幕（含 3D）都来自实际显示内容。

音频，`GtkDriver::AVI_SoundUpdate()`：

```cpp
avout_flac.updateAudio(soundData, soundLen);
#ifdef HAVE_FFMPEG_RECORD
avout_ffmpeg.updateAudio(soundData, soundLen);
#endif
```

录制状态：

```cpp
bool AVI_IsRecording()
{
    bool recording = avout_x264.isRecording() || avout_flac.isRecording();
#ifdef HAVE_FFMPEG_RECORD
    recording = recording || avout_ffmpeg.isRecording();
#endif
    return recording;
}
```

`AVI_IsRecording()` 必须包含新录制，否则 SPU 在 DualSynch/Asynch 模式下会跳过混音，导致录到静音。

### 5.6 配置持久化

在 `frontend/posix/gtk/config_opts.h` 增加：

```cpp
/* Recording */
OPT(record_video_encoder,   std::string, "libx264",      Recording, VideoEncoder)
OPT(record_audio_encoder,   std::string, "aac",          Recording, AudioEncoder)
OPT(record_container,       std::string, "matroska",     Recording, Container)
OPT(record_video_bitrate,   int,         8000,           Recording, VideoBitrate)
OPT(record_audio_bitrate,   int,         192,            Recording, AudioBitrate)
OPT(record_filtered,        bool,        true,           Recording, FilteredOutput)
```

- 编码器按 `name` 保存，便于跨版本复用。
- 若保存的编码器在当前机器不存在，对话框回退到默认值。

### 5.7 运行时状态恢复

增加一个 1 秒 `g_timeout_add_seconds()` 看门狗：

- 若 `avout_ffmpeg.hasFailed()` 且之前正在录制：
  - `avout_ffmpeg.end()` 清理；
  - 重新启用开始类 action；
  - 状态栏显示错误摘要。
- 否则只同步 action 的敏感状态。

目的：工作线程里发生的编码/写盘错误不会让菜单永久停留在“录制中”。

---

## 6. 关键实现细节

### 6.1 为什么不录制 masterNativeBuffer16

`masterNativeBuffer16` 的布局：

```
bit  0..4  = R
bit  5..9  = G
bit 10..14 = B
bit 15     = 未使用（或 alpha）
```

这**不是** FFmpeg 的 `AV_PIX_FMT_RGB555LE`：`RGB555LE` 把 R 放在 bit 10..14，
直接使用会让红蓝互换，画面整体偏红。正确的格式是 `AV_PIX_FMT_BGR555LE`。

更重要的是，GTK3 默认 `NDSColorFormat_BGR666_Rev`，GPU 会把需要 3D 渲染的屏幕
直接画到 `masterCustomBuffer`，`masterNativeBuffer16` 中对应区域保持黑色。
因此新录制读取 `masterCustomBuffer`：

```
colorFormat == BGR555_Rev  ->  16-bit BGR555  ->  AV_PIX_FMT_BGR555LE
colorFormat == BGR666/888  ->  32-bit R,G,B,A ->  AV_PIX_FMT_RGBA
```

> 注意：`masterCustomBuffer` 的 18/24-bit 内存顺序是 **R,G,B,A**（由
> `ColorspaceConvertBuffer555xTo6665Opaque<false,...>` / `...To8888Opaque<false,...>`
> 写入）。显示/滤镜缓冲则是由它经过 `SWAP_RB` 转换得到的 **B,G,R,A**。
> 早期代码把两者都当成 BGRA，结果 18-bit 录像仍然红蓝互换。

两个屏幕在 custom buffer 中上下连续排列，步长为 `customWidth`。
未开启滤镜输出时输出为 config 指定尺寸（默认 256×384）；GPU 缩放大于 1x 时由
`sws_scale` 缩放。

### 6.1.1 滤镜输出录制

`PresentFrame()` 的执行顺序是：

1. `PrepareHudTarget()` 把 AGG/Lua 的绘制目标设到 `video->GetSrcBufferPtr()`；
2. Lua GUI 回调与内置 HUD 绘制；
3. `WGPU_HudRenderFrame()` 把 HUD 渲染成独立纹理（WGPU 路径）；
4. `video->RunFilter()` 把 src 滤镜到 `video->GetDstBufferPtr()`；
5. 显示层读取 dst。

因此滤镜输出录制的采集点是第 4 步之后：

- 源格式固定为 `AV_PIX_FMT_BGRA`（滤镜 src/dst 的内存顺序是 B,G,R,A）；
- 宽高取 `GetDstWidth()/GetDstHeight()`，因此 2x/3x/4x 滤镜会得到 512×768、
  768×1152、1024×1536 等输出；
- CPU HUD 路径下 HUD/Lua 已经画在 src 里，会一起经过滤镜；
- WGPU HUD 路径下 HUD 不在 dst 里，需要 `readPixels()` 读回 HUD 纹理，
  再以 `dst` 尺寸双线性合成（`WGPU_HudCompositeFrame()`），与呈现器的采样方式一致；
- `sFrameSerial` 只在 `DidFrameEnd()` 递增，`RedrawScreen()` 的重复呈现不会产生
  重复视频帧。

这里**不包含**窗口布局、屏幕间距、旋转和窗口缩放，因为这些是在呈现阶段由
Cairo/WGPU 直接对 dst 做的合成，并不是一张 CPU 图像。这与 Windows/macOS 的非
composed 录制范围一致。

### 6.2 音频采样率

`SPU.h` 中 `DESMUME_SAMPLE_RATE = 44100`（当前配置）。`avout_ffmpeg.cpp` 直接包含
`SPU.h` 并使用该宏作为重采样输入采样率，不在录制模块里另写一份数字，避免两处漂移。
若未来 `DESMUME_SAMPLE_RATE` 改动，录制路径自动跟随。

### 6.3 帧率常量

```
33513982 / 560190 = 59.8261... Hz
```

不能用 60/1，否则长时间录制会与音频漂移。使用 `AVRational{560190, 33513982}` 作为
`time_base`，`{33513982, 560190}` 作为 `framerate`。

### 6.4 硬件编码器的两种输入模式

1. **软件帧输入**（`h264_nvenc`、`h264_amf`、`h264_v4l2m2m`）：
   - 编码器像素格式是 `NV12`/`YUV420P`；
   - 直接 `sws_scale` 后 `send_frame`；
   - 由编码器内部上传到 GPU。
2. **硬件帧输入**（`h264_vaapi`、`h264_qsv`、`h264_vulkan`、`*_cuda`）：
   - 编码器像素格式是 `AV_PIX_FMT_VAAPI` 等；
   - 需要 `hw_frames_ctx`；
   - 先 `sws_scale` 到 `NV12` 软件帧，再 `av_hwframe_transfer_data()`。

### 6.5 编码器可能“枚举得到但打不开”

这是正常现象，不应在枚举阶段过滤：

- `h264_vaapi`：没有 `/dev/dri/renderD*` 或驱动无编码入口；
- `h264_nvenc`：没有 NVIDIA GPU / 驱动；
- `h264_qsv`：没有 Intel iHD 驱动；
- `h264_v4l2m2m`：没有对应 SoC 编码节点；
- `h264_vulkan`：驱动未实现 Vulkan Video encode。

用户选中后，`begin()` 给出明确错误即可。

### 6.6 不阻塞模拟线程

旧实现的 `AVOutPipeBase::updateVideo()` 直接 `write()` 到管道，编码器慢时会阻塞模拟。
新实现只做 `memcpy` + 入队；编码在工作线程完成。

---

## 7. 测试计划

### 7.1 构建

```bash
meson setup build -Dffmpeg-recording=true
meson compile -C build
```

检查：

- `HAVE_FFMPEG_RECORD` 已定义；
- `desmume` 可执行文件链接了 `libavcodec/libavformat/libavutil/libswscale/libswresample`；
- `-Dffmpeg-recording=false` 或缺少依赖时仍可编译，且菜单中新项不可用、旧录制可用。

### 7.2 独立编码冒烟测试

用一个不依赖 GTK 的小程序直接驱动 `AVOutFFmpeg`：

- 生成 256×384 的 RGB555 渐变帧 + 1 kHz 正弦波音频；
- 录 5 秒；
- 用 `ffprobe` 检查：
  - 视频流编码器、分辨率、帧率 ≈ 59.826；
  - 音频流编码器、采样率 48000、双声道；
  - 时长 ≈ 5 秒，音画起点一致。

本机（aarch64/Asahi）测试注意：

- `libx264` / `libx265` 编码器由发行版 FFmpeg 编译进来，**即使系统没有安装
  `x264` / `x265` 命令行工具**也能使用。旧录制路径依赖外部 `x264` 进程，在本机
  会启动失败；新路径不受影响，这也是新录制的默认编码器选 `libx264` 的依据。
- 本机 `vainfo` 失败、没有 NVIDIA/AMD 设备，但 `av_codec_iterate()` 仍会枚举出
  23 个带硬件能力标记的视频编码器（VAAPI/NVENC/AMF/Vulkan/V4L2-M2M），因为
  “编译进来”不等于“本机可用”。因此 `h264_vaapi` 行只能验证 `begin()` 的报错路径，
  不能验证真实硬件编码。
- 本机可枚举 132 个视频编码器（23 硬件 + 109 软件）、87 个音频编码器（全部软件）。
  列表会很长，但这是“枚举全部编码器”的预期结果。

建议覆盖组合：

| 视频 | 音频 | 容器 | 结果 |
|---|---|---|---|
| `libx264` | `aac` | mkv | 必须成功 |
| `libx264` | `flac` | mkv | 必须成功 |
| `libx265`（或 `libvpx-vp9`） | `aac` | mp4 | 成功 |
| `ffv1` | `flac` | mkv | 成功（注意 `ffv1` 帧内编码，文件较大） |
| `ffv1` | `aac` | mp4 | 在本机 FFmpeg 9 上实际成功（旧假设“容器不支持”不成立） |
| `rawvideo` | 无 | mp4 | `begin()` 预检失败：`avformat_query_codec()` 返回 0 |
| `libx264` | `libvorbis` | mp4 | header 成功、trailer 失败，停止时弹窗提示文件不完整 |
| `h264_vaapi` | `aac` | mkv | 本机无设备时给出清晰错误 |
| `h264_v4l2m2m` | `aac` | mkv | 本机无 SoC 编码节点时给出清晰错误 |

### 7.3 GTK 手工测试

1. 菜单：新项 + “Old recorders” 子菜单显示正确；旧项仍可录制。
2. 编码器列表包含全部视频编码器，并正确标注 Hardware/Software。
3. 选中 `h264_vaapi`（本机可枚举、但没有可用 VAAPI 编码设备）时弹窗错误，
   不留下半截文件。
4. 录制 30 秒：画面流畅、音频不超前/滞后、停止后文件可播放。
5. 录制期间菜单项禁用；停止后恢复；旧录制与新录制不能同时启动。
6. 录制中关闭窗口 / 退出：文件能正常收尾（或至少 MKV 可播放）。
7. 录音期间切换静音/音量不影响录制数据。
8. 状态栏显示录制路径与停止提示。

### 7.4 异常注入

- 输出目录只读；
- `/dev/dri/renderD128` 无权限；
- 录制中拔掉/禁用 GPU（可选）；
- 工作线程写盘失败（例如磁盘满）→ 1 秒内菜单恢复、状态栏报错。

---

## 8. 代码改动清单

| 文件 | 改动 |
|---|---|
| `desmume/src/frontend/posix/meson_options.txt` | 新增 `ffmpeg-recording` 选项 |
| `desmume/src/frontend/posix/meson.build` | 探测 libav*、定义 `HAVE_FFMPEG_RECORD` |
| `desmume/src/frontend/posix/gtk/meson.build` | 加入新源文件与 FFmpeg 依赖 |
| `desmume/src/frontend/posix/shared/avout_ffmpeg.h` | 新增公共接口 |
| `desmume/src/frontend/posix/shared/avout_ffmpeg.cpp` | 新增 FFmpeg 录制实现 |
| `desmume/src/frontend/posix/gtk/main.cpp` | 动作、对话框、钩子、看门狗、状态栏 |
| `desmume/src/frontend/posix/gtk/menu.ui` | 新菜单结构、旧录制移入子菜单、修正 stop action |
| `desmume/src/frontend/posix/gtk/config_opts.h` | 新增 Recording 配置项 |

不改：

- `avout_x264.*`、`avout_flac.*`、`avout_pipe_base.*`（旧功能保留）
- `frontend/posix/gtk2/*`
- Windows / macOS 前端

---

## 9. 风险与限制

1. **编码器数量巨大**：本机枚举结果为 132 个视频编码器（23 硬件 + 109 软件）和
   87 个音频编码器。列表会包含 `rawvideo`、`ffv1`、`wrapped_avframe`、`pcm_*`、
   `adpcm_*`、`anull` 等不常用项。这是“枚举所有编码器”的直接结果；后续可增加
   “常用/全部”过滤。
2. **枚举 ≠ 可用**：硬件编码器是否可用取决于驱动、GPU、权限，只能在打开时判定。
   本机没有任何可用的 VAAPI/NVENC/AMF 设备，但发行版 FFmpeg 仍枚举出 23 个硬件
   编码器。UI 不应在枚举阶段隐藏它们。
3. **容器兼容性**：不是所有编码器都能进 MP4；失败信息必须清晰。
4. **VAAPI 覆盖有限**：Intel/AMD 较稳；NVIDIA 通常需 `h264_nvenc`；ARM/Asahi 可能完全没有
   可用的硬件编码器（本机就是这种情况，只剩 Vulkan 编码器可枚举，但多数驱动未实现
   Vulkan Video encode）。
5. **音频重采样**：优先统一到 48000 Hz；对 AAC/Opus 是必要的，对 FLAC/PCM 会产生一次重采样。
   若编码器不支持 48000，则退回其支持列表中的第一个采样率（`time_base` 随之变化）。
   （若需要“音频原样无损”，后续可让 FLAC/PCM 直接使用 `DESMUME_SAMPLE_RATE`）。
6. **画面内容**：
   - 默认（滤镜输出开启）：读取 `video->GetDstBufferPtr()`，包含 3D、CPU 视频滤镜、
     Lua 和 HUD；输出尺寸跟随滤镜（如 2x = 512×768）。WGPU HUD 纹理通过
     `readPixels()` 合成，因此会多一次 GPU 回读。
   - 关闭滤镜输出：读取 `masterCustomBuffer`，输出固定 256×384 双屏，不含滤镜/HUD。
   - 两种模式都不包含窗口布局、屏幕间距、旋转和窗口缩放，和 Windows/macOS 的
     composed 模式仍有差距。
7. **异常退出**：MP4 在未写 `moov` 时不可播放；默认 MKV 降低风险。
8. **C++14 线程**：使用 `std::thread`/`std::mutex`/`std::condition_variable`；与项目现有
   `Task`/`pthread` 风格混用，需注意只在录制模块内部使用。
9. **音频编码器列表同样很长**：87 个条目里包含 `pcm_*`、`adpcm_*`、`anull` 等，
   其中许多不能进 MKV/MP4 或没有实际用途。第一版按“全部枚举”显示，靠 `begin()`
   的容器/编码器打开错误兜底；后续可增加常用列表或过滤。

---

## 10. 后续可扩展项

- 录制 composed 画面：复用 `frontend/windows/record_compositor.cpp` 的 CPU 合成逻辑
  （其算法本身跨平台），支持布局、间距、旋转、CPU 滤镜、最多 4x 放大。
- 常用编码器过滤 / 收藏列表。
- 硬件编码器可用性预探测（打开一次空编码器并立即关闭）以在列表中灰显不可用项。
- 录制暂停/恢复、帧步进（`frameStep`）。
- 音频无损直通（FLAC/PCM 保持 `DESMUME_SAMPLE_RATE`）。
- 零拷贝：WebGPU/EGL 输出 → DMA-BUF → VAAPI/Vulkan surface。
- GTK2 前端接入新录制（复用同一 `AVOutFFmpeg`）。

---

## 11. 实现期间确认的事实

本节记录实现前在目标机器和 FFmpeg 头文件中实际核对过的内容，避免后续维护时
重复踩坑。测试环境为 Fedora/Asahi aarch64，日期 2026-11。

### 11.1 本机 FFmpeg 版本与安装布局

```text
libavcodec      63.1.102
libavformat     63.1.102
libavutil       61.1.102
libswscale      10.1.102
libswresample    7.1.102
LIBAVCODEC_VERSION_MAJOR = 63
```

- 头文件在 `/usr/include/ffmpeg/libavcodec/...` 等目录下。
- `pkg-config --cflags libavcodec` 输出 `-I/usr/include/ffmpeg`。
- Meson 的 `dependency('libavcodec', ...)` 通过 pkg-config 自动获得该 include 路径，
  不需要在 `meson.build` 或源码中写死 `/usr/include/ffmpeg`。
- `ffmpeg` CLI、`ffprobe` 可用；`vainfo` 失败。

### 11.2 libavcodec 63 的 API 事实

1. **`AVCodec` 已移除 `pix_fmts` / `sample_fmts` 字段**。当前版本只能用：

   ```c
   int avcodec_get_supported_config(const AVCodecContext *avctx,
                                    const AVCodec *codec,
                                    enum AVCodecConfig config,
                                    unsigned flags,
                                    const void **out_configs,
                                    int *out_num_configs);
   ```

   传 `avctx = NULL`、`codec = 目标编码器`、`flags = 0` 即可查询编码器声明支持的值。

2. `enum AVCodecConfig` 中与本功能相关的项及列表终止符：

   | `config` | 元素类型 | 终止符 |
   |---|---|---|
   | `AV_CODEC_CONFIG_PIX_FORMAT` | `enum AVPixelFormat` | `AV_PIX_FMT_NONE` |
   | `AV_CODEC_CONFIG_SAMPLE_FORMAT` | `enum AVSampleFormat` | `AV_SAMPLE_FMT_NONE` |
   | `AV_CODEC_CONFIG_SAMPLE_RATE` | `int` | `0` |
   | `AV_CODEC_CONFIG_CHANNEL_LAYOUT` | `AVChannelLayout` | `{0}`（`nb_channels == 0`） |

   `out_num_configs` 不包含终止符。`out_configs == NULL` 表示**不限制**，并不是
   “不支持”；此时按设计默认值处理（视频 `NV12`，音频 `S16` / 48000 / stereo）。

3. 硬件能力标记：

   ```c
   #define AV_CODEC_CAP_HARDWARE (1 << 18)
   #define AV_CODEC_CAP_HYBRID   (1 << 19)
   ```

   `AV_CODEC_CAP_HYBRID` 表示编码器可能部分使用硬件，UI 同样标为 Hardware。

4. `AVCodecContext` 已有 `AVChannelLayout ch_layout`；可用
   `av_channel_layout_default()` / `av_channel_layout_copy()` 设置。
5. `av_samples_alloc()` 返回的是**一整块**分配（平面格式的各 plane 指针都指向同一块），
   释放时只需 `av_freep(&audio_data[0])`。
6. `AVAudioFifo` 可直接用 `frame->extended_data` 作为读写指针数组，打包/平面格式都适用。

### 11.3 本机编码器枚举结果

通过 `av_codec_iterate()` + `av_codec_is_encoder()` 实测：

| 类型 | 总数 | 硬件标记 | 软件 |
|---|---:|---:|---:|
| 视频 | 132 | 23 | 109 |
| 音频 | 87 | 0 | 87 |

23 个带硬件标记的视频编码器（按 wrapper 分组）：

- VAAPI：`av1_vaapi`、`h264_vaapi`、`hevc_vaapi`、`mjpeg_vaapi`、`mpeg2_vaapi`、
  `vp8_vaapi`、`vp9_vaapi`
- NVENC：`av1_nvenc`、`h264_nvenc`、`hevc_nvenc`
- AMF：`av1_amf`、`h264_amf`、`hevc_amf`
- Vulkan：`av1_vulkan`、`ffv1_vulkan`、`h264_vulkan`、`hevc_vulkan`、
  `prores_ks_vulkan`
- V4L2 M2M：`h263_v4l2m2m`、`h264_v4l2m2m`、`hevc_v4l2m2m`、`mpeg4_v4l2m2m`、
  `vp8_v4l2m2m`

这些条目只是“发行版 FFmpeg 编译进来了”，**不代表本机有对应硬件**。本机没有可用的
VAAPI/NVENC/AMF 设备，Vulkan 多数驱动也未实现 Video encode，所以实际打开都会失败。
这验证了设计决定：枚举阶段不过滤，`begin()` 打开失败时再给出明确错误。

软件编码器中的关键项：

- 视频：`libx264`、`libx264rgb`、`libx265`、`libvpx`、`libvpx-vp9`、`ffv1`、
  `mpeg4`、`mjpeg`、`rawvideo` 等。
- 音频：`aac`、`ac3`、`flac`、`alac`、`opus`、`libopus`、`libvorbis`、`libmp3lame`、
  `pcm_*`、`adpcm_*`、`anull` 等。

特别注意：**本机没有安装 `x264` 命令行工具，但 `libx264` 编码器在 libavcodec 中可用。**
旧录制依赖外部 `x264` 进程，会失败；新录制直接调用 `libx264`，不受影响。因此新录制的
默认视频编码器仍选 `libx264`。

### 11.4 视频像素格式协商的实测细节

见 §4.4 的表格。要点复述：

- `h264_vaapi` / `h264_vulkan` / `prores_ks_vulkan`：只返回硬件像素格式，必须走
  hwdevice + hwframes 路径。
- `h264_nvenc`：返回 22 个软件格式再加 `cuda`，软件路径优先，选 `nv12` 即可。
- `h264_v4l2m2m`：返回 `n=0`（不限制），需要自行尝试 `nv12` / `yuv420p`。
- `libx264`：支持 `nv12`，可直接 sws 到 NV12，不必先转 `yuv420p`。
- `ffv1`：支持 62 种像素格式，按优先级取第一个即可。

### 11.5 音频编码配置的实测细节

见 §4.6 的表格。最容易出错的点：

- `aac` **只支持 `fltp`**，如果代码把 `S16` 写死，AAC 无法打开。
- `flac` 支持 `s16` / `s32`，`libopus` 支持 `s16` / `flt`，但不是所有编码器都支持 S16。
- `opus` 只支持 48000 Hz；`aac`、`libopus` 都支持 48000 Hz，所以默认 48000 合理。
- 部分编码器只支持平面格式（`s16p` / `s32p`），采样格式优先列表必须包含它们。
- 本机所有音频编码器的硬件能力标记为 0，UI 仍统一显示 `[Software]`。

### 11.6 DeSmuME 侧已确认的源码事实

- `SPU.h`：`DESMUME_SAMPLE_RATE` 当前为 **44100**，不是 32768。
- `NDSSystem.cpp`：每帧调用
  `driver->AVI_SoundUpdate(SPU_core->outbuf, spu_core_samples)`，
  `spu_core_samples` 是**每声道**采样数，缓冲区为交织 s16 stereo。
- `SPU.cpp` 在 DualSynch/Asynch 模式下通过 `driver->AVI_IsRecording()` 决定是否继续混音；
  新录制必须计入该判断，否则会录到静音。
- `masterNativeBuffer16`：256×384 的 `u16` 缓冲，主屏在上、副屏在下，位布局为
  R[0..4] / G[5..9] / B[10..14]，对应 `AV_PIX_FMT_BGR555LE`（不是 RGB555LE）。
  仅当 GPU 以原生 15-bit 路径渲染时它才包含完整画面。
- `masterCustomBuffer`：前端实际显示的连续缓冲。15-bit 模式下是 `u16` BGR555；
  18/24-bit 模式下是 32-bit，内存字节序 R,G,B,A（`AV_PIX_FMT_RGBA`）。
  GTK3 默认 18-bit，3D 屏幕只出现在这个缓冲里。
- 显示/滤镜缓冲（`VideoFilter` 的 src/dst）：由 `masterCustomBuffer` 经 `SWAP_RB`
  转换得到，内存字节序 B,G,R,A（`AV_PIX_FMT_BGRA`）。
- 帧率常量为 `33513982 / 560190 ≈ 59.8261 fps`，不能简化为 60 fps。
- GTK3 `menu.ui` 原先使用不存在的 `app.stop_recording`，而 `main.cpp` 注册的是
  `record_stop`；本次一并修正。
- GTK3 配置类 `Config::value<T>` 提供 `operator T()`、`.get()`、`.set()` 和
  `operator=`；但持久化只在程序正常退出时调用 `config.save()` 写入
  `$XDG_CONFIG_HOME/desmume/config.cfg`。异常退出会丢失本次录制选项。
- GTK3 已有 `UpdateStatusBar()`、`g_timeout_add_seconds()` 和
  `g_action_map_lookup_action()`，看门狗与 action 恢复直接复用这些设施即可。

### 11.7 对测试计划的影响

- `h264_vaapi` / `h264_nvenc` / `h264_v4l2m2m` 在本机只能验证**失败路径**和错误信息，
  不能验证真实硬件编码。
- 新录制必须至少验证 `libx264 + aac + mkv` 与 `libx264 + flac + mkv`；两者不依赖
  外部命令行工具。
- 旧录制在本机因缺少 `x264` 命令而失败，这是预期现象，不应归因于新实现。
- 若要用 `ffprobe` 校验，重点检查音频采样率 48000、双声道、视频帧率约 59.826，
  以及 MKV 在强制结束进程后仍可播放。

### 11.8 实现与冒烟测试结果

构建实测：

- `meson setup --reconfigure ..` 找到全部 5 个 libav 依赖，输出
  `FFmpeg video recording: enabled`。
- `ninja` 成功；`ldd gtk/desmume` 确认链接了
  `libavcodec.so.63`、`libavformat.so.63`、`libavutil.so.61`、
  `libswscale.so.10`、`libswresample.so.7`。
- 另建 `-Dffmpeg-recording=false` 的构建目录（同时 `-Dwgpu=false`）可完整编译通过，
  说明 `#ifdef HAVE_FFMPEG_RECORD` 隔离有效。
- `gtk-builder-tool validate menu.ui` 通过；GTK 程序启动时没有出现
  “unknown action” 或菜单解析警告。
- 加入滤镜输出录制后，主构建与 `-Dffmpeg-recording=false` 构建都重新编译通过。

独立冒烟测试（不依赖 GTK，直接驱动 `AVOutFFmpeg`）：

- 输入：120 帧 256×384 BGR555 渐变帧；每帧 735 个每声道 s16 采样；
  以 20 ms/帧的节奏送入，避免测试程序比编码器快而导致队列丢帧。
- 结果：

| 组合 | 结果 | `ffprobe` 关键值 |
|---|---|---|
| `libx264` + `aac` + mkv | 成功 | 2.026 s，h264 256×384，1376/23≈59.826 fps，aac 48000 Hz stereo |
| `libx264` + `flac` + mkv | 成功 | 2.016 s，flac 48000 Hz stereo |
| `libx264` + 无音频 + mkv | 成功 | 2.006 s，仅视频流 |
| `libx265` + `aac` + mp4 | 成功 | 2.0058 s，hevc 256×384，16756991/280095≈59.826 fps，aac 48000 Hz stereo |
| `h264_vaapi` + `aac` + mkv | 打开失败（预期） | `Unable to create the hardware device: Input/output error` |

补充事实：

- Matroska 与 MP4 对同一帧率会显示不同的有理数（`1376/23` 与
  `16756991/280095`），数值都约等于 59.826，属于正常的时间基化简差异。
- 第一版冒烟测试不节流时，120 帧在几毫秒内全部入队，工作线程处理不过来，
  有界队列丢弃了新帧，文件时长缩短到 1.4–1.97 秒。这验证了“队列满丢新帧”
  的行为，也说明**测试/使用时应按真实帧率送入**；真实模拟器路径不会这样突发。
- 硬件编码器不可用时的错误来自 `av_hwdevice_ctx_create()`，目前信息较底层
  （`Input/output error`）。后续可在 `begin()` 前检查 `/dev/dri/renderD*`
  或环境变量，给出更友好的提示。
- GTK 配置类 `value<std::string>` 的 `operator T()` 不参与
  `operator==(const basic_string&, const char*)` 的模板推导，因此代码里必须写
  `config.record_container.get() == "mp4"`、`config.record_audio_encoder.get().empty()`。
- 音频 “None” 保存为空字符串。再次打开对话框时，空字符串必须被当作
  用户显式选择 None，而不能回退到 `aac`，否则“无音频”选项无法持久化。
- 无 ROM 的短时启动/退出测试中出现一条与本功能无关的
  `GLib-CRITICAL ... g_key_file_free: assertion 'key_file != NULL' failed`；
  新录制路径没有产生 GTK critical。

容器兼容性测试（独立程序）：

| 场景 | 行为 |
|---|---|
| `rawvideo` + mp4 | `begin()` 返回 false，错误为 “The MP4 container does not support the rawvideo video encoder…”，且不留下文件 |
| `ffv1` + aac + mp4 | 成功（本机 FFmpeg 9 的 mp4 muxer 接受 ffv1） |
| `libx264` + `libvorbis` + mp4 | header 成功；`end()` 后 `hasFailed()` 为 true，错误为 `Unable to finish the output file: Invalid data found when processing input` |
| `libx264` + `aac` + mkv | 成功，`end()` 后无失败状态 |

颜色验证（`ffv1` 无损 + `ffprobe`/解码回 RGB）：

| 输入 | 解码结果 |
|---|---|
| BGR555 纯蓝（bit 10..14） | `(0, 0, 247)` |
| BGR555 纯红（bit 0..4） | `(248, 0, 0)` |
| RGBA 纯蓝（custom buffer 18-bit） | `(0, 0, 255)` |
| RGBA 纯红（custom buffer 18-bit） | `(254, 0, 0)` |
| BGRA 纯蓝（滤镜 dst） | `(0, 0, 255)` |
| BGRA 纯红（滤镜 dst） | `(254, 0, 0)` |
| BGRA 512×768 纯蓝（2x 滤镜） | `(0, 0, 255)`，尺寸 512×768 |
| BGRA 512×768 纯红（2x 滤镜） | `(254, 0, 0)`，尺寸 512×768 |

> 这些测试只验证“给定内存布局 → 编码器 → 解码颜色”的映射，不验证
> `masterCustomBuffer` 本身的内存布局。后者由 `colorspacehandler` 的模板参数
> 决定，已在 §6.1 中直接核对。

### 11.9 实测画面问题：3D 图层缺失与偏红

用户用 GTK3 实际游戏录制 `test.mp4` 后发现：

- 上屏（2D 菜单）可见，但整体偏红；
- 下屏（3D 街道）完全黑色；
- 视频为 256×384、h264 + aac，时长和帧率正常。

根因有两处，均与 GTK3 的缓冲选择有关：

1. **偏红**：`masterNativeBuffer16` 的位布局是 R[0..4]/G[5..9]/B[10..14]，
   对应 `AV_PIX_FMT_BGR555LE`。早期代码按 `AV_PIX_FMT_RGB555LE` 解释，
   红蓝通道互换，蓝色菜单变成红色。
2. **3D 图层缺失**：GTK3 默认 18-bit 色彩格式，GPU 会把 3D 屏幕直接渲染到
   `masterCustomBuffer`；`masterNativeBuffer16` 里只有原生渲染的 2D 屏幕，
   所以下屏全黑。

修复：

- 录制源改为 `latestDisplayInfo.masterCustomBuffer`；
- 按 `latestDisplayInfo.colorFormat` 选择 `FFmpegCaptureFormat_BGR555LE`
  （15-bit）或 `FFmpegCaptureFormat_RGBA8888`（18/24-bit）；
- 每帧携带源宽高，`sws_getCachedContext()` 在 GPU 缩放变化时重建缩放器；
- 视频队列增加 64 MB 字节上限，避免高缩放时占用过多内存。

**后续修正（同一问题）**：第一次修复把 18/24-bit 的 `masterCustomBuffer`
按 `AV_PIX_FMT_BGRA` 解释，但 `ColorspaceConvertBuffer555xTo6665Opaque<false,...>`
写出的内存顺序实际是 R,G,B,A。颜色测试当时只覆盖了编码器 API 的 BGRA 路径，
没有覆盖真实 custom buffer，因此没有暴露这个问题。现已改为 `AV_PIX_FMT_RGBA`。
显示/滤镜缓冲是 B,G,R,A，滤镜输出录制仍使用 `AV_PIX_FMT_BGRA`。

### 11.10 滤镜输出录制

新增选项 **Include video filter, Lua and OSD**（默认开启，持久化键
`record_filtered`）。开启时：

- 捕获点从 `DidFrameEnd()` 移到 `PresentFrame()` 的 `video->RunFilter()` 之后；
- 捕获 `video->GetDstBufferPtr()`，格式 `BGRA8888`，尺寸为滤镜 dst 尺寸；
- `begin()` 用该尺寸作为编码输出尺寸，2x 滤镜得到 512×768，3x 得到 768×1152；
- CPU HUD 路径下 OSD/Lua 已在 src 中，会随滤镜一起录制；
- WGPU HUD 路径下 HUD 是独立纹理：`readPixels()` 读回后由
  `WGPU_HudCompositeFrame()` 双线性缩放到 dst 尺寸并合成；
- `sFrameSerial` 只由 `DidFrameEnd()` 递增，`RedrawScreen()` 的重复呈现不会
  生成重复视频帧；
- 关闭该选项时退回 `masterCustomBuffer` 原生 256×384 录制。

测试（独立驱动 `AVOutFFmpeg`，`ffv1` 无损）：

- RGBA 256×384 纯红/纯蓝 → 解码 `(254,0,0)` / `(0,0,255)`；
- BGRA 512×768 纯红/纯蓝 → 尺寸 512×768，颜色正确；
- BGR555 256×384 纯蓝 → `(0,0,247)`；
- 原有 `libx264 + aac/flac + mkv`、`libx265 + aac + mp4`、MP4 兼容性测试全部保持通过；
- `-Dffmpeg-recording=false` 构建仍可编译，`#ifdef HAVE_FFMPEG_RECORD` 隔离有效。

已知限制：

- 录制里不含窗口布局、屏幕间距、旋转、窗口缩放；
- 录制中切换滤镜时，编码输出尺寸仍是开始录制时的尺寸，后续帧会被缩放；
- WGPU HUD 读回需要同步 GPU，开启滤镜输出 + HUD 时会有一点额外开销；
- Cairo 路径没有独立 HUD 纹理，HUD 已在 src 中，不需要读回。
