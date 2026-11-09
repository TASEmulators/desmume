# WGPU OSD 迁移计划

> 状态：设计草案，尚未实施
> 日期：2026-11-10
> 前置工作：路线 1（Windows 原生 GDI+ 后端）已落地，见 `frontend/modules/osd/agg/native_draw.*` 与 `frontend/windows/osd_fonts/`

## 0. 范围

「迁移 WGPU」包含两层含义，本计划按阶段覆盖，阶段 1 是两者的共同前提：

1. **合成层**：把 HUD 从「CPU 叠加进 `video.buffer`」改成「GPU 上传第二张纹理并在同一 render pass 里混合」。HUD 位图仍由 GDI+ 生成。
2. **绘制层**：把 OSD 图元与文字也搬上 GPU（WGSL SDF + 字形图集），彻底不再生成/上传位图。

早期调研已确认：DDraw / WGPU / OGL 都没有文字光栅化和抗锯齿 2D 矢量 API，渲染引擎只能「贴 + 混合」。项目内两个先例是 `vba-rerecording`（GDI 生成位图 → WGPU overlay 管线）和 DeSmuME Cocoa Metal（HUD 每字一个 quad，完全 GPU 绘制）。本计划选择先做前者、把后者拆成可选阶段。

**目标约束**：不修改 `AggDrawTarget` 接口、`agg_osd.cpp` 业务逻辑和 `lua-engine.cpp` 调用点；不改变 256×384 BGRA32 HUD 帧缓冲坐标；不改变 OGL / DDraw 显示路径；posix / cocoa 前端不受影响。

## 1. 现状基线

以下行号均在本轮逐行核对过。

| 事实 | 位置 |
| --- | --- |
| WGPU 共享层不 include `windows.h`、`webgpu.h` 或前端头；posix 也编译同一份 `wgpu_present.cpp` | `frontend/shared/wgpu_present.h:32-33` |
| `CreatePipeline()` 只创建一条管线，`colorTarget.blend = NULL` | `frontend/shared/wgpu_present.cpp:722` |
| 展示用 shader 的 `fsMain` 强制返回 `alpha = 1.0`，**不能复用现有管线做 overlay** | `wgpu_present.cpp:103-107` |
| bind group layout 是 binding 0 = `texture_2d<f32>`、binding 1 = `sampler` | `wgpu_present.cpp:636-648` |
| 顶点缓冲固定 `18 * sizeof(Vertex)`（gap 6 + 两屏各 6） | `wgpu_present.cpp:783-785` |
| 主帧上传要求 `bytesPerRow` 256 对齐；`video.width` 恒为 64 的倍数故满足 | `wgpu_present.cpp:1766-1776` |
| 一个 render pass：`SetPipeline` → gap quad → 两个屏幕 quad → `End` | `wgpu_present.cpp:1954-1985` |
| 屏幕 quad 的 UV 按 rotation 0/90/180/270 做 `ofs` 置换；启用 GPU 滤镜链时改采 0..1 的独立链纹理 | `wgpu_present.cpp:1873-1918` |
| DDraw 与 WGPU 共用 CPU 合成 `NativeDraw_CompositeHUD()`，把 HUD 最近邻缩放写进 `video.buffer` | `frontend/windows/display.cpp:815-831` |
| HUD 合成发生在 `video.filter()` **之前** | `display.cpp:815` → `display.cpp:835` |
| `video.buffer` 是显示专用副本；源码注释明说拷贝是「because we're about to draw the OSD onto it」 | `display.cpp:774` |
| Windows 上 `guiData.data = aggDraw.hud->buf().buf()`，OSD 与 Lua `gui.*` 共用同一张 256×384 缓冲 | `lua-engine.cpp:5394` |
| HUD 目标尺寸恒为 256×384；视频帧为 `256*prescaleHD × 384*prescaleHD` | `aggdraw.cpp` / `frontend/windows/video.h:67-68` |
| 窗口若已被 OGL 用则拒绝建 WGPU surface，并在显示方法切换时 `kill()` 重建 | `display.cpp:716-730, 843, 857` |
| `WGPUDISPLAY` 只是 Win32 适配层，真正渲染在 `WGPUPresent` | `frontend/windows/wgpu_display.h:29-36` |

**关键结论**：Windows 上 OSD 与 Lua overlay 共用一张缓冲，因此阶段 1 只需要一张 HUD 纹理，不必照 Cocoa 做前后双缓冲。

## 2. 从 GDI+ 迁移继承的约束

1. **保留接缝**。路线 1 之所以能低风险落地，是因为所有调用方只依赖 `AggDrawTarget` 虚接口、输出仍是同一块 BGRA 内存。本轮同样只在「HUD 位图 → 屏幕」这一步换实现。
2. **alpha 契约先行**。AGG 的 `pixfmt_bgra32` 与现行 GDI+ 后端写的是「预乘 RGB + 直通 alpha」（50% 红 = `0x807F0000`），CPU 合成公式为 `src + dst*(1-a)`。GPU blend 必须用 `One / OneMinusSrcAlpha`；直接照抄 `vba-rerecording/src/win32/Wgpu.cpp:1868` 的 `SrcAlpha / OneMinusSrcAlpha` 会二次乘 alpha、整体变暗。若要用 vba 的参数，须先调 `NativeDraw_SetPremultipliedOutput(false)` 切成直通 alpha 输出（该开关已存在）。
3. **编译开关 + 运行时回退**。沿用 `/p:UseNativeOsd=false` 的模式，新增 `UseGpuHudOverlay`（先默认关闭，验证通过后打开）；关闭时应字节级等同现状。
4. **零新依赖**。不引入库；WGSL 用字符串常量或现有 `WGPUPresentShaderSource`（RCDATA）机制；34 份点阵字体已在 exe 的 RCDATA 中，无需新增资源。
5. **独立探针 + 像素级 golden**。照 `.workbuddy/native-draw-preview/preview.exe` / `noagg.exe` 的模式做 `wgpu_overlay_probe`，用 CPU 合成结果作基准，不靠 GUI 截图下结论。

### 2.1 可复现依据（路线 1 的实测数据）

| 项目 | 结果 |
| --- | --- |
| 字体资源 | `34/34 byte-identical to AGG 2.5 embedded fonts` |
| 合成公式 | `AGG=0xFF973163`、预乘公式 `0xFF973163`、直通公式 `0xFF583163` |
| 50% 红 | AGG `0x807F0000`、GDI+ 预乘 `0x7F7F0000`、GDI+ 直通 `0x7FFF0000` |
| HUD 相似度 | AGG vs GDI+ 预乘 `meanAbs=2.134/255`（>8 差像素 19.59%），vs 直通 `15.640` |
| 文字 | 实心像素 `3920 = 3920` 完全一致 |
| 去 libagg | `noagg.exe` 不链接 `agg-2.5*.lib` 运行成功，`fonts=1`、exit 0 |

这些数字说明：alpha 存储方式一旦搞错，误差会从 2.1 跳到 15.6 —— 这正是阶段 1 必须先定 alpha 契约的原因。

## 3. 分阶段计划

### 阶段 1：GPU 纹理混合（建议单独提交，可回退）

**目标**：保留 GDI+ 生成 HUD 位图，去掉 WGPU 分支的 CPU 合成，改成第二张纹理 + 带 blend 的 overlay 管线。

接口草案：

```cpp
struct WGPUPresentOverlay
{
    const uint32_t* pixels;   // BGRA8888, top-down
    int width;
    int height;
    int strideBytes;
    bool premultiplied;
};

// 保留原签名重载；overlay 默认 NULL，posix 调用点无需改动
bool present(const uint32_t* pixels, int width, int height,
             const WGPUPresentRect* srcRects, const WGPUPresentRect* dstRects,
             const WGPUPresentRect* gapRect, int rotation, uint32_t gapColor,
             const WGPUPresentOverlay* overlay = NULL);
```

改动清单：

| 文件 | 改动 |
| --- | --- |
| `frontend/shared/wgpu_present.h` | 新增 `WGPUPresentOverlay` 与 `present()` 重载/默认参数 |
| `frontend/shared/wgpu_present.cpp` | `Impl` 增加 `hudTex / hudView / hudBindGroup / overlayPipeline`；`CreatePipeline()` 建第二条带 `WGPUBlendState` 的管线；`Destroy()` 释放；`kill()` 后重建时清零 |
| 同上 | 新增 `fsOverlay`，直接返回采样 RGBA（现有 `fsMain` 把 alpha 写死为 1.0，不可复用） |
| 同上 | 每帧 `wgpuQueueWriteTexture` 上传 HUD；HUD 宽 256 → `bytesPerRow = 1024` 正好 256 对齐，**无需 vba 的 staging padding**；加内容哈希脏标记省掉无变化帧的 384 KB 上传（HUD 每帧 clear 重画但内容常不变） |
| 同上 | 顶点缓冲 18 → 30（或另建 12 顶点 overlay buffer）：每屏一个 HUD quad，复用对应屏幕的 `dstRect` 与 rotation `ofs` 置换，但 UV 取 HUD 纹理子矩形（按 `prefilterWidth/Height` 归一化，与 `chain[i]` 解耦）；在 `wgpuRenderPassEncoderEnd()` 前、屏幕 quad 之后 draw |
| `frontend/windows/wgpu_display.h/.cpp` | 透传 overlay 参数；无 `ENABLE_WGPU` 的 stub 同步加参数 |
| `frontend/windows/display.cpp` | `:815` 条件收窄为仅 `isRunningDirectDraw`；WGPU 分支把 `aggDraw.hud->buf()` 经 `WGPU_DoDisplay()` 传给 `present()`；`needsHUDDrawing == false` 时传空 |
| `frontend/windows/desmume.props` | 新增 `UseGpuHudOverlay` 开关 |

blend 状态（预乘契约）：

```cpp
WGPUBlendComponent overlayColor = WGPU_BLEND_COMPONENT_INIT;
overlayColor.operation = WGPUBlendOperation_Add;
overlayColor.srcFactor = WGPUBlendFactor_One;
overlayColor.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
WGPUBlendComponent overlayAlpha = WGPU_BLEND_COMPONENT_INIT;
overlayAlpha.operation = WGPUBlendOperation_Add;
overlayAlpha.srcFactor = WGPUBlendFactor_One;
overlayAlpha.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
```

```wgsl
@fragment
fn fsOverlay(input: VSOut) -> @location(0) vec4f
{
    return textureSample(srcTex, srcSamp, input.uv);
}
```

纹理格式沿用 `EnsureVideoTexture()` 里 `videoTex` 的格式，usage 需含 `TextureBinding | CopyDst`。采样器用 `samplerNearest`（CPU 合成是整数最近邻，linear 会让点阵字发虚）。

**验收门槛**：

- 数学级 golden：`out.rgb = src.rgb + dst.rgb * (1 - src.a)` 对 50% 红、渐变末端 `alpha=0`、文字 AA 边缘逐像素比对；预乘 / 直通两种契约各跑一轮。
- 可选离屏校验：`copyTextureToBuffer` + `mapAsync` 读回 GPU 结果与 CPU 结果比对。
- 几何矩阵：`rotation {0,90,180,270} × video.swap {0,1,2,3} × GPU filter {Off, Xbrz} × prescaleHD {1,2,3}`，用带十字标记的测试 HUD 检查 1px 偏移与错位。
- 回退：关掉开关时输出与现状逐字节一致。
- 共享层在 Windows 与 posix 两种配置下都能编译（不得出现 Win32 / GDI+ 类型）。

### 阶段 2：GPU 图元

新增独立编译单元 `frontend/modules/osd/wgpu/wgpu_draw.h/.cpp` + `hud_shapes.wgsl`，开关 `DESMUME_WGPU_OSD`。把 `AggDrawTarget` 的图元调用录制成 command list（quad + shapeType + 渐变端点 + 颜色 + 变换矩阵），由 WGSL SDF 管线绘制：

| AGG / agg_osd.cpp 语义 | WGSL 实现 |
| --- | --- |
| `roundedRect(..., 1)`，半径恒为 1px（`agg_osd.cpp:308/330/337`） | `sdRoundedBox(r=1)` 或直接矩形 |
| `fillLinearGradient()`：256 级 LUT + 沿方向向量投影（`agg2d.inl:634-663`） | `t = clamp(dot(p-p1,p2-p1)/dot(p2-p1,p2-p1),0,1); mix(c0,c1,t)`，alpha 同步插值 |
| `ellipse()` / `polygon()` / `line()` | `sdEllipse` / 多边形 SDF / `abs(d) - w/2` 描边 |
| `FillAndStroke` 状态机与 `noLine()`（`agg_osd.cpp:373`） | 每图元带 fill/stroke 标志 |
| `floor(x)+0.5` 半像素对齐（`agg_osd.cpp:320-322`） | 保持坐标不变，勿改 |

**范围控制**：只覆盖 `agg_osd.cpp` 实际用到的图元。Lua `agg.*` 的 `arcTo` / Bezier / `viewport` / `parallelogram` / 25 种 `BlendMode` 继续走位图路径，形成「混合后端」——这正是路线 1 中「不碰业务层」的做法。同时保留 `BlendAlpha` 默认值假设，非默认 blend 的 GPU 实现另立任务。

### 阶段 3：GPU 文字

- 把 34 份 RCDATA 点阵字体（名称与顺序保持与 `fonts_list[]` 一致，兼容旧 ini 与 Lua 脚本）一次性上传成字形图集纹理，每字符一个 quad；采样用 Nearest 才是原本的像素观感。
- 可改用 Cocoa Metal 的 FreeType TTF 图集方案（`ClientDisplayView.cpp:500-547`、`MacMetalDisplayViewShaders.metal:166-203` 的 `hud_vertex/hud_fragment`），但会改变字体外观，需另做兼容决策。
- `renderTextDropshadowed()` 是两次 blit，需保留；文字颜色相乘、按缩放选 mip 的逻辑可参照 Metal。

### 阶段 4：收尾

- 移除 WGPU 分支的 CPU 合成调用；DDraw 保留 CPU 合成，OGL 现有 `srcHUD` 第二纹理路径不动。
- 完整 `msbuild` 全链接 + GUI 实机验证（由用户执行）。
- 提交时精确暂存，排除 `.workbuddy/`（探针、预览程序与 PNG 不应入仓）。

## 4. 必须先拍板的决策点

1. **滤镜语义（唯一真正的可见行为变化）**。当前 HUD 在 `display.cpp:815` 合成、`:835 video.filter()` 之后才呈现，所以 HUD 会随视频滤镜一起被放大/处理。GPU overlay 在 pass 最后贴会绕过滤镜、更清晰。二选一：接受新观感（与 Cocoa 一致），或把 HUD blend 到 upscaler 链的输入纹理以保原样。
2. **alpha 契约**。选 `One / OneMinusSrcAlpha`（保持预乘输出）还是切直通后照抄 vba。它决定 shader、CPU golden 和 `NativeDraw_SetPremultipliedOutput()` 的取值。
3. **HUD quad 的几何覆盖**。现状 CPU 合成覆盖整帧（含两屏与 gap）。建议两屏各一个 quad 并单独处理 gap 区域，避免 gap 上出现 HUD 内容；同时必须复用屏幕 quad 的 `dstRect` 与 rotation 置换。

## 5. 已澄清的风险与实施细节

**录像 / 截图无风险（纠正早期推断）**：`DRV_AviVideoUpdate()` 使用 `dispInfo.masterCustomBuffer`（`aviout.cpp:1235`），`MakeScreenshotBGR24()` 同源（`hotkey.cpp:225/237`）；HUD 只写在 `video.buffer` 这个显示专用副本上。因此改成 GPU overlay **不会改变录像或截图内容**。早期把 vba DDraw 的 "do not draw Lua HUD to a video dump" 注释套到本项目、推断「GPU overlay 会让 HUD 不进录像」是不成立的。

其余细节：

- **窗口 / 后端切换**：`display.cpp:720` 的 `windowRejectsWGPU` 与 `:843/857` 的 `kill()` 会在 WGPU ↔ OGL/DDraw 切换时重建全部资源，overlay 纹理、bind group、管线不能假设常驻。
- **跨平台**：`wgpu_present.cpp` 不 include `windows.h`，posix meson 也会编译；新增代码只能用 `const uint32_t*` 传入 HUD 像素，不能出现 Win32 / GDI+ 类型。
- **缩放与对齐**：CPU 合成是整数最近邻把 256×384 拉到 `prefilterWidth × prefilterHeight`；GPU 采样必须 Nearest 且注意 UV 半像素，否则文字偏移 1px。
- **性能**：每帧 384 KB 上传可接受；脏标记（参考 vba `TextOverlay.cpp` 的 FNV-1a 签名）值得做，尤其在暂停/无 HUD 变化时。
- **回退链**：三层开关互相独立 —— `UseNativeOsd=false` 回退到 AGG 后端，`UseGpuHudOverlay=false` 回退到 CPU 合成，`DESMUME_WGPU_OSD` 控制阶段 2/3。

## 6. 验证策略汇总

| 层次 | 手段 | 通过标准 |
| --- | --- | --- |
| 编译 | MSVC 单 TU 编译 `wgpu_present.cpp` / `wgpu_display.cpp` / `display.cpp`；posix 配置再编一遍 | 0 error；共享层无 Win32 依赖 |
| 链接 | 完整 `msbuild /p:Configuration=Release /p:Platform=x64 /m` | 链接成功 |
| 像素 | `wgpu_overlay_probe` 与 CPU golden 比对 | 预乘契约下逐像素一致或差值在量化误差内 |
| 几何 | rotation × swap × filter × prescaleHD 矩阵，十字标记 HUD | 无错位、无 1px 偏移 |
| 视觉 | `hud_features.png` 式的对照图 + GUI 实机 | 文字/渐变/手柄/存档槽齐全 |
| 回退 | 三个开关分别关闭 | 与改动前输出一致 |

## 7. 建议顺序

阶段 1 单独提交并拿到像素 golden 之后，再评估是否继续阶段 2/3。阶段 2 的收益是省掉每帧位图生成与上传，但会引入一套新的图元语义实现，工作量与路线 1 中补齐 `arcTo` / `parallelogram` / `viewport` / 25 种 blend 相当，不宜与阶段 1 捆绑。

## 附：涉及文件清单

| 文件 | 阶段 | 动作 |
| --- | --- | --- |
| `frontend/shared/wgpu_present.h` | 1 | 改 |
| `frontend/shared/wgpu_present.cpp` | 1 | 改 |
| `frontend/windows/wgpu_display.h` | 1 | 改 |
| `frontend/windows/wgpu_display.cpp` | 1 | 改 |
| `frontend/windows/display.cpp` | 1 | 改 |
| `frontend/windows/desmume.props` | 1 | 改 |
| `frontend/modules/osd/wgpu/wgpu_draw.h/.cpp` | 2 | 新增 |
| `frontend/modules/osd/wgpu/hud_shapes.wgsl` | 2 | 新增 |
| `frontend/modules/osd/agg/agg_osd.cpp`、`lua-engine.cpp` | 全部 | **不改** |

## 8. 平台适用性（Windows / Linux / macOS）

### 8.1 已提交的 GDI+ 后端是 Windows 专属

- `native_draw.h:42` 用 `#if defined(DESMUME_NATIVE_DRAW) && defined(WIN32)` 包住整个类；依赖 GDI+（`gdiplus.lib`）、Win32 资源 API（`FindResource` / `LoadResource` / `LockResource`、`RT_RCDATA`）与 MSVC 的 `ResourceCompile`（`osd_fonts.rc` + vcxproj 条件项）。
- `desmume.props` 的 `UseNativeOsd` 开关只存在于 Windows 工程；`display.cpp` 的 CPU 合成替换点也在 Windows 前端。
- posix / cocoa 不定义 `DESMUME_NATIVE_DRAW`，因此**没有破坏**这两个平台，但也**没有给它们带来任何去 AGG 的收益**。
- 结论：这是「Windows 后端替换」，不是可移植方案。

### 8.2 WGPU 阶段 1：渲染层平台无关，数据层不是

- `frontend/shared/wgpu_present.cpp` 不 include `windows.h` / `webgpu.h` / 前端头（`wgpu_present.h:32-33`）；posix meson 已经编译它（`posix/meson.build:352`），并配有 Linux 适配器 `wgpu_present_linux.cpp` 与 `wgpu_display_linux.cpp:194-203`。阶段 1 的第二纹理、overlay 管线与顶点改动**可被 Linux 直接复用**，前提是接口只用 `const uint32_t*` 传 HUD 像素。
- 但 HUD 数据的来源语义不同：Windows 有独立的 256×384 HUD 帧缓冲；posix GTK 在 `gtk/main.cpp:4084` 直接 `aggDraw.hud->setDrawTargetDims(video->GetSrcBufferPtr(), ...)`，把 HUD 画进**视频源缓冲本身**，没有独立位图。把阶段 1 移植到 Linux 必须先给 posix 分配独立 HUD 缓冲并改这一处（或让 overlay 接口支持任意尺寸 / stride）。
- Linux 仍编译 `aggdraw.cpp` / `agg_osd.cpp`（`posix/meson.build:301-302`，受 `dep_agg` 控制），`HAVE_LIBAGG` 可关（`meson.build:294`）；关闭后 OSDCLASS 退化为壳、Lua `agg.*` 变 no-op。因此 Linux 要去 AGG，还需要一个非 AGG 的 HUD 生成层 —— GDI+ 不可用，可选：复用 `lua-engine.cpp:2980` 的 `PutTextInternal` + `Small_Font_Data` 软件 blit，或引入平台无关的软件光栅库。
- macOS 不走 WebGPU：cocoa 用 Metal 渲染（`MacMetalDisplayView.mm`），且不编译 `osd/agg`（cocoa 目录内无 `aggdraw.cpp` / `HAVE_LIBAGG` 引用）。阶段 1 对 macOS **不适用**。

### 8.3 阶段 2 / 3：算法可移植，代码不可直接复用

- WGSL 是 WebGPU 标准，Metal 用 MSL，shader 源码不能共用；但 SDF 与 `dot + mix` 渐变的算法、几何与状态机设计可以照搬。
- cocoa Metal 已有等价且更完整的实现：FreeType 字形图集 + 每字一个 quad（`ClientDisplayView.cpp:500-547`、`MacMetalDisplayViewShaders.metal:166-203`）。要达到阶段 2/3 的效果，在 `hud_fragment` 里补 SDF 分支比给 cocoa 引入 WebGPU 更合理。

### 8.4 全平台去 AGG 的三个前置条件

1. **接口去 AGG 化**：`aggdraw.h` 无条件 include `agg2d.h` / `agg_color_rgba.h` / `agg_rendering_buffer.h`，`AggDrawTarget` 使用 `agg::rgba8`、`agg::rendering_buffer` 和 `Agg2DBase` 枚举。需换成自定义 POD 类型，否则任何平台都仍必须安装 AGG 头。
2. **字体资源平台无关化**：34 份字体目前是 Windows RCDATA。需改成编译期数组、独立数据文件或各平台自有资源机制；名称与顺序必须保持以兼容旧 ini 和 Lua 脚本。
3. **HUD 帧缓冲语义统一**：把 posix 的「直接画进视频缓冲」改成与 Windows 一致的独立 256×384 缓冲，overlay 接口才有统一输入。

### 8.5 结论

| 平台 | GDI+ 后端 | WGPU 阶段 1 | 阶段 2 / 3 |
| --- | --- | --- | --- |
| Windows | 已实现 | 直接适用 | 直接适用 |
| Linux | 不适用 | 渲染层可复用；需先分离独立 HUD 缓冲并提供非 AGG 生成层 | 算法可复用；需接口去 AGG 化 |
| macOS (cocoa) | 不适用 | 不适用（走 Metal） | 建议在现有 Metal HUD 管线里补 SDF 分支 |
