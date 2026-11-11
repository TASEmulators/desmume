# WGPU OSD 迁移计划

> 状态：设计草案，已完成实施前代码复核；WGPU/Metal/Lua 相关落地记录见 §11–§21
> 日期：2026-11-10（§21 更新于 2026-11-11）
> 前置工作：路线 1（Windows 原生 GDI+ 后端）已落地，见 `frontend/modules/osd/agg/native_draw.*` 与 `frontend/windows/osd_fonts/`
> 复核：2026-11-10 逐行核对了本文涉及的文件；与代码冲突之处及新增约束集中在 §9，阅读时以 §9 为准

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
| DDraw 与 WGPU 共用 CPU 合成 `NativeDraw_CompositeHUD()`，把 HUD 最近邻缩放写进 `video.buffer` | `frontend/windows/display.cpp:821-831` |
| HUD 合成发生在 `video.filter()` **之前** | `display.cpp:821` → `display.cpp:837` |
| `video.buffer` 是显示专用副本；源码注释明说拷贝是「because we're about to draw the OSD onto it」 | `display.cpp:774` |
| Windows 上 `guiData.data = aggDraw.hud->buf().buf()`，OSD 与 Lua `gui.*` 共用同一张 256×384 缓冲 | `lua-engine.cpp:5394` |
| HUD 目标尺寸恒为 256×384；视频 prefilter 帧为 `256*prescaleHD × 384*prescaleHD`，`video.width/height` 在 CPU 滤镜开启时可为它的 1.5/2/3/4/5 倍 | `aggdraw.cpp` / `frontend/windows/video.h:67-68, 159-194` |
| 窗口若已被 OGL 用则拒绝建 WGPU surface，并在显示方法切换时 `kill()` 重建 | `display.cpp:716-730, 843, 857` |
| `WGPUDISPLAY` 只是 Win32 适配层，真正渲染在 `WGPUPresent` | `frontend/windows/wgpu_display.h:29-36` |

**关键结论**：Windows 多核（默认）下 OSD 与 Lua overlay 共用一张缓冲，因此阶段 1 只需要一张 HUD 纹理，不必照 Cocoa 做前后双缓冲。单核模式下 `aggDraw.hud` 实际指向 `aggDraw.screen` 且该缓冲为 NULL，详见 §9.2 第 1 条。

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
| `frontend/shared/wgpu_present.cpp` | `Impl` 增加 `hudTex / hudView / hudBindGroup / overlayPipeline`；`CreatePipeline()` 建 overlay 管线（预乘 / 直通各一条，见 §9.3）；`Destroy()` 释放；`kill()` 后重建时清零 |
| 同上 | 新增 `fsOverlay`，直接返回采样 RGBA（现有 `fsMain` 把 alpha 写死为 1.0，不可复用） |
| 同上 | 每帧 `wgpuQueueWriteTexture` 上传 HUD；HUD 宽 256 → `bytesPerRow = 1024` 正好 256 对齐，无需 vba 的 staging padding；但通用 stride 仍需 staging（§9.2 第 7 条）。加内容哈希脏标记省掉无变化帧的 384 KB 上传（HUD 每帧 clear 重画但内容常不变） |
| 同上 | 顶点缓冲 18 → 30（或另建 12 顶点 overlay buffer）：每屏一个 HUD quad，复用对应屏幕的 `dstRect` 与 rotation `ofs` 置换；UV 用 `present()` 的 `width/height` 归一化（不是 `prefilterWidth/Height`，见 §9.2 第 3 条），与 `chain[i]` 解耦；在 `wgpuRenderPassEncoderEnd()` 前、屏幕 quad 之后 draw |
| `frontend/windows/wgpu_display.h/.cpp` | 透传 overlay 参数；无 `ENABLE_WGPU` 的 stub 同步加参数；`wgpu_present.cpp` 的 `!ENABLE_WGPU` stub 也要同步（§9.2 第 10 条） |
| `frontend/windows/display.cpp` | `:821` 条件收窄为仅 `isRunningDirectDraw`；WGPU 分支把 `aggDraw.hud->buf()` 经 `WGPU_DoDisplay()` 传给 `present()`；`needsHUDDrawing == false` 或 HUD 指针为空时传空 |
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
- `renderTextDropshadowed()` 实际是 **九次** `renderText()` 调用（`aggdraw.h:417-435`），不是两次 blit；基类的调用序列保持不变，GPU 目标逐次录制成 text run。
- 实现与验证记录见 §13。核心选择：R8Unorm 覆盖率图集 + `font` 指针按 `fonts_list[].font` 注册，文字颜色取当前 `lineColor`，不乘 `masterAlpha`（与 AGG 一致）。

### 阶段 4：收尾

- 移除 WGPU 分支的 CPU 合成调用；DDraw 保留 CPU 合成，OGL 现有 `srcHUD` 第二纹理路径不动。
- 完整 `msbuild` 全链接 + GUI 实机验证（由用户执行）。
- 提交时精确暂存，排除 `.workbuddy/`（探针、预览程序与 PNG 不应入仓）。

## 4. 必须先拍板的决策点

1. **滤镜语义（唯一真正的可见行为变化）**。当前 HUD 在 `display.cpp:821` 合成、`:837 video.filter()` 之后才呈现，所以 HUD 会随 CPU 滤镜和 GPU 滤镜链一起被放大/处理。GPU overlay 在 pass 最后贴会绕过两者、更清晰。二选一：接受新观感（与 Cocoa 一致），或把 HUD blend 到 upscaler 链的输入纹理以保原样（CPU 滤镜仍需另做处理）。
2. **alpha 契约**。选 `One / OneMinusSrcAlpha`（保持预乘输出）还是切直通后照抄 vba。它决定 shader、CPU golden 和 `NativeDraw_SetPremultipliedOutput()` 的取值。
3. **HUD quad 的几何覆盖**。现状 CPU 合成覆盖整帧（含两屏与 gap），但 gap quad 在屏幕之后绘制、总会覆盖该区域的 HUD，所以“两屏各一个 quad”已经与现状等价，不需要单独处理 gap。必须复用屏幕 quad 的 `dstRect` 与 rotation 置换；UV 用 `present()` 的 `width/height` 归一化（见 §9.2 第 3 条）。

## 5. 已澄清的风险与实施细节

**录像 / 截图无风险（纠正早期推断）**：`DRV_AviVideoUpdate()` 使用 `dispInfo.masterCustomBuffer`（`aviout.cpp:1235`），`MakeScreenshotBGR24()` 同源（`hotkey.cpp:225/237`）；HUD 只写在 `video.buffer` 这个显示专用副本上。因此改成 GPU overlay **不会改变录像或截图内容**。早期把 vba DDraw 的 "do not draw Lua HUD to a video dump" 注释套到本项目、推断「GPU overlay 会让 HUD 不进录像」是不成立的。

其余细节：

- **窗口 / 后端切换**：`display.cpp:720` 的 `windowRejectsWGPU` 与 `:843/857` 的 `kill()` 会在 WGPU ↔ OGL/DDraw 切换时重建全部资源，overlay 纹理、bind group、管线不能假设常驻。
- **跨平台**：`wgpu_present.cpp` 不 include `windows.h`，posix meson 也会编译；新增代码只能用 `const uint32_t*` 传入 HUD 像素，不能出现 Win32 / GDI+ 类型。
- **缩放与对齐**：CPU 合成是整数最近邻把 256×384 拉到 `prefilterWidth × prefilterHeight`；GPU 采样必须 Nearest，且 HUD UV 必须按 `present()` 的 `width/height`（CPU 滤镜后的 `video.width/height`）归一化，否则有 CPU 滤镜时会错位。UV 半像素仍需验证，避免文字偏移 1px。
- **性能**：每帧 384 KB 上传可接受；脏标记（参考 vba `TextOverlay.cpp` 的 FNV-1a 签名）值得做，尤其在暂停/无 HUD 变化时。
- **回退链**：三层开关互相独立 —— `UseNativeOsd=false` 回退到 AGG 后端，`UseGpuHudOverlay=false` 回退到 CPU 合成，`DESMUME_WGPU_OSD` 控制阶段 2/3。

## 6. 验证策略汇总

| 层次 | 手段 | 通过标准 |
| --- | --- | --- |
| 编译 | MSVC 单 TU 编译 `wgpu_present.cpp` / `wgpu_display.cpp` / `display.cpp`；posix 配置再编一遍 | 0 error；共享层无 Win32 依赖 |
| 链接 | 完整 `msbuild /p:Configuration=Release /p:Platform=x64 /m` | 链接成功 |
| 像素 | `wgpu_overlay_probe` 与 CPU golden 比对 | 预乘契约下每通道差值 ≤ 1（浮点/整数舍入），少数边界像素 ≤ 2；详见 §9.4 |
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
- 但 HUD 数据的来源语义不同：Windows **多核**（默认）有独立的 256×384 HUD 帧缓冲（单核会把 `aggDraw.hud` 指向 `aggDraw.screen`，其后端缓冲实际为 NULL，详见 §9.2 第 1 条）；posix GTK 在 `gtk/main.cpp:4084` 直接 `aggDraw.hud->setDrawTargetDims(video->GetSrcBufferPtr(), ...)`，把 HUD 画进**视频源缓冲本身**，没有独立位图。把阶段 1 移植到 Linux 必须先给 posix 分配独立 HUD 缓冲并改这一处（或让 overlay 接口支持任意尺寸 / stride）。
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

## 9. 实施前复核结果（2026-11-10）

> 本节是逐行核对代码后的修正与新增事实。前文保留原始设计，**冲突时以本节为准**。

### 9.1 复核方法与已确认的基线

- 共享层可以在 Linux 上独立做语法/编译检查，不需要 Windows：
  `g++ -std=c++11 -DENABLE_WGPU -I3rdparty/wgpu -Ifrontend/shared -c frontend/shared/wgpu_present.cpp` 通过。
- 前文表格中的代码事实基本成立；复核后更精确的位置如下：
  - `fsMain` 把 alpha 写死为 `1.0`：`wgpu_present.cpp:103-106`。
  - `CreatePipeline()` 从 `wgpu_present.cpp:622` 开始，`colorTarget.blend = NULL` 在 `:722`；`fsMain` 入口在 `:727`。
  - bind group layout 仍是 binding 0 = `texture_2d<f32>`、binding 1 = `sampler`：`:636-648`。sampler 的 layout 类型是 `Filtering`，但现有 nearest sampler 已经成功绑在这个 layout 上（白纹理路径），overlay 可复用。
  - 顶点缓冲固定 `18 * sizeof(Vertex)`：`:785`。
  - `present()` 从 `:1692` 开始；主帧上传与 `bytesPerRow = width * 4` 在 `:1761-1776`。
  - rotation 的 `ofs` 置换与 UV 计算在 `:1873-1918`；render pass 在 `:1950-1985`。
  - `display.cpp` 的 CPU 合成在 `:821-831`，`video.filter()` 在 `:837`（不是前文的 815 / 835）。
  - `video.buffer` 的显示专用拷贝在 `:773-775`；HUD 每帧 `clear()` 在 `:792`。
  - `lua-engine.cpp` 的 `guiData` 仍直接指向 `aggDraw.hud`：`:5394-5395`。
  - WGPU 窗口冲突判断在 `display.cpp:720-723`，后端切换时 `wgpudisplay.kill()` 在 `:857`。
  - 表面格式固定为 `BGRA8Unorm`（非 sRGB）：`wgpu_present.cpp:589-595`。因此 GPU 混合值与 CPU 8 位合成值在同一个编码空间，不需要做 sRGB 线性化。
  - `video.width` 确实恒为 64 的倍数：`video.h:161-193` 中所有分支都是 `prefilterWidth * {1, 3/2, 2, 3, 4, 5}`，而 `prefilterWidth = 256 * prescaleHD`；故 `width * 4` 恒为 256 字节对齐。
  - `NativeDraw_CompositeHUD()` 的 `srcPremultiplied` 默认值是 `true`（`native_draw.h:287-291`），且全仓没有任何调用点调用 `NativeDraw_SetPremultipliedOutput(false)`（只有声明和定义）。所以当前 GDI+ 与 AGG 两条后端实际都走预乘契约。

### 9.2 需要修正的前文结论

1. **“Windows 有独立的 256×384 HUD 帧缓冲”并非在所有配置下成立。**
   `aggdraw.cpp:143` 与 `:153` 的 screen target 都以 `buf = 0` 构造；`aggdraw.cpp:204-207` 在 `single_core()` 时执行 `aggDraw.hud = aggDraw.screen`，而 Windows 前端没有任何地方重新 `setDrawTargetDims()` 给 screen target。结果是单核模式下 `aggDraw.hud->buf().buf() == NULL`，现有 `NativeDraw_CompositeHUD()` 会直接 `return`，即**单核 Windows 的 OSD 本来就不显示**（这是既有状态，不是阶段 1 引入的）。
   阶段 1 必须把 `overlay->pixels == NULL` 当作“没有 overlay”，不能断言 Windows 永远有 HUD 缓冲。多核（默认）下才有独立 `hudBuffer`。
2. **gap 不需要单独处理。** CPU 合成确实覆盖整帧（含 gap），但 WGPU 先画 gap quad、再画两块屏幕，gap 颜色始终覆盖 HUD 内容。因此“两屏各一个 HUD quad”已经与现状一致；额外处理 gap 反而可能把 HUD 画到 gap 上。
3. **HUD 的 UV 不能用 `prefilterWidth/Height` 归一化。** `present()` 收到的 `width/height` 是 `video.width/height`；开启 CPU 滤镜后它们是 prefilter 的 1.5/2/3/4/5 倍。`srcRects` 也在这个坐标系里，所以 HUD quad 的 UV 必须用 `s.left / width`、`s.top / height`，其中 `s` 是已经过 rotation 置换的源矩形。只要缩放是均匀的，这与“HUD 相对整帧的位置”一致；用 prefilter 尺寸会在有 CPU 滤镜时错位。
4. **被绕过的滤镜不只是 GPU 链，还包括 CPU 滤镜。** 现状是 HUD 合成进 `video.buffer` 后，`video.filter()` 和 GPU 链都可能处理 HUD。overlay 在 render pass 最后绘制，两者都绕过。验收时应把 CPU 滤镜（尤其 1.5× / 1.25× 这类非整数倍）也纳入“预期观感变化”，不能用逐像素 golden 要求一致。
5. **“预乘契约下逐像素一致”做不到严格成立。** `NativeDraw_CompositeHUD()` 用整数 `/255` 截断；GPU 的 UNORM blend 用浮点计算后按 UNORM 规则舍入。文字 AA 边缘可能有 ±1（个别通道 ±2）的量化差。像素 golden 的判定应改为“每个通道差值 ≤ 1（或明确记录少数 ≤ 2 的边界像素）”，而不是逐字节相等。
6. **`premultiplied` 字段不能只靠一条 blend 管线实现。** 预乘需要 `One / OneMinusSrcAlpha`；直通需要 `SrcAlpha / OneMinusSrcAlpha`。若保留运行时 `premultiplied` 参数，应创建两条 overlay pipeline，或在 shader 里按标志先乘 alpha。注意直通模式下 **alpha 分量仍应使用 `One / OneMinusSrcAlpha`**，只有颜色分量的源因子改为 `SrcAlpha`；把 alpha 也设成 `SrcAlpha` 会把 alpha 平方。
7. **HUD 上传不能假设 `strideBytes == width * 4` 且始终 256 对齐。** 当前 256×384 的 HUD 恰好是 1024 字节/行，可以直接 `wgpuQueueWriteTexture`；但接口里的 `strideBytes` 是通用的。实现时应：行距已对齐则直接上传，否则先 copy 到按 256 对齐的 staging 行；负 stride（bottom-up）当前没有来源，可直接拒绝或翻转。脏标记应按实际 `width * 4` 像素行计算，不能把行尾 padding 算进去。
8. **脏标记需要显式失效。** `kill()` / texture 重建 / 尺寸变化后必须把 hash 置为无效，否则新纹理可能永远不收到第一次上传。
9. **前文引用的 `.workbuddy/native-draw-preview/preview.exe` 与 `noagg.exe` 不在当前工作树中。** `.workbuddy/` 目录不存在；这些探针需要重新建立或从旧工作目录取回，不能把“照现有探针做”当成现成步骤。
10. **`WGPUDISPLAY` 的 stub 不是唯一需要同步的 stub。** `wgpu_present.cpp` 在 `#if !defined(ENABLE_WGPU)` 分支里也手写了 `WGPUPresent::present()`；新增 overlay 参数时两处 stub 都必须改，否则非 WGPU 构建会链接失败。Linux 的 `WGPULinuxDisplay::present()` 继续用默认参数即可，不必改签名。
11. **`WGPU_DoDisplay()` 是 `display.cpp` 内的 static 函数，`display.h` 没有声明它。** 因此给它加 overlay 参数是局部改动，不会影响其他翻译单元；`WGPUDISPLAY::present()` 才需要改头文件。
12. **posix 如果将来传 overlay 会双重绘制。** Linux 的 HUD 直接画进 `video->GetSrcBufferPtr()`（`gtk/main.cpp:4084`），而 WGPU 呈现的是 `video->GetDstBufferPtr()`（`gtk/main.cpp:1859`），HUD 已经在图像里。共享层新参数默认 `NULL` 是正确且必须的；Linux 适配器不要顺手把 HUD 传进去。
13. **共享层新增代码只能使用 `const uint32_t*` 与基本类型。** 不能在 `wgpu_present.h/.cpp` 中出现 `HDC`、GDI+、`RECT` 或 `HWND`；Windows 适配层负责把 `RECT` 转成 `WGPUPresentRect`，HUD 直接以裸指针 + stride 传入。

### 9.3 阶段 1 实现时应采用的最终形态

```cpp
struct WGPUPresentOverlay
{
    const uint32_t *pixels;   // BGRA8888, top-down
    int width;
    int height;
    int strideBytes;          // >= width * 4
    bool premultiplied;       // true: RGB already multiplied by alpha
};

bool present(const uint32_t *pixels, int width, int height,
             const WGPUPresentRect *srcRects, const WGPUPresentRect *dstRects,
             const WGPUPresentRect *gapRect, int rotation, uint32_t gapColor,
             const WGPUPresentOverlay *overlay = NULL);
```

- `Impl` 增加 `hudTex / hudView / hudBindGroup / hudWidth / hudHeight / hudHash / hudHashValid`，以及 `overlayPipelinePremultiplied / overlayPipelineStraight`。复用现有 `bindGroupLayout` 与 `samplerNearest`。
- overlay 纹理惰性创建（首次收到非空 HUD 时），格式 `BGRA8Unorm`，usage `TextureBinding | CopyDst`，`Destroy()` 中释放。
- 顶点数 18 → 30；屏幕 quad 之后为每块屏幕追加一个 HUD quad，复用同一 `dstRects[i]` 与同一 rotation `ofs`，UV 用 `s.left/width` 等（见 9.2 第 3 条），并跳过零面积目标。
- 绘制顺序保持：gap → 两块屏幕 → HUD（两个 HUD quad 可在同一 bind group 下连续 draw）。
- 预乘管线：颜色 `One / OneMinusSrcAlpha`，alpha `One / OneMinusSrcAlpha`。
- 直通管线：颜色 `SrcAlpha / OneMinusSrcAlpha`，alpha `One / OneMinusSrcAlpha`。
- `fsOverlay` 直接返回采样到的 RGBA；不复用把 alpha 写死为 1 的 `fsMain`。
- `Display()` 中仅在 `isRunningDirectDraw` 时保留 CPU 合成；WGPU 分支把 `(needsHUDDrawing && aggDraw.hud->buf().buf() != NULL)` 的 HUD 作为 overlay 传入。`needsHUDDrawing == false` 时传 `NULL`。
- 若 overlay 管线或纹理创建失败，不要因此让整个 `present()` 失败；跳过 overlay 并在调试输出中报告。真正失败时 CPU 合成已被跳过，因此这条路径应在 `begin()` 阶段通过日志尽早暴露。

### 9.4 复核后的验收门槛（替换 §6 对应行）

| 层次 | 修正后的标准 |
| --- | --- |
| 编译 | 除 MSVC 单 TU 外，先用 `g++ -std=c++11 -DENABLE_WGPU` 编共享层；再确认 `!ENABLE_WGPU` stub 也能过 |
| 像素 | 预乘契约下允许每通道 ±1 量化差；直通管线单独跑一轮。不要承诺逐字节一致 |
| 几何 | rotation × swap × **CPU filter** × GPU filter × prescaleHD；CPU 滤镜下的 HUD 变化按“预期观感差异”记录，不作为错位 |
| 单核 | `aggDraw.hud->buf().buf() == NULL` 时必须安全跳过，不能崩溃、不能画垃圾 |
| 回退 | `UseGpuHudOverlay=false`（或未定义宏）时与改动前逐字节一致 |
| Linux | 不传 overlay，呈现结果与现状一致；不得出现 HUD 双重绘制 |
| 实机 | Windows GUI 由用户验证；本环境只能做到共享层编译与静态审查 |

### 9.5 对路线图的结论

阶段 1 仍然可行，且不需要动 `AggDrawTarget`、`agg_osd.cpp` 或 `lua-engine.cpp`。需要先按 9.2 / 9.3 修正实现细节，尤其是单核 NULL HUD、UV 归一化、两条 blend 管线和 stride staging。阶段 2/3 本次没有逐行复核；按原计划应等阶段 1 的像素与实机结果出来后再决定，不应与阶段 1 捆绑提交。

## 10. GDI+ 后端审计（2026-11-10）

结论：**当前 OSD/Lua 实际调用路径上没有发现会造成可见错误的 bug，因此不在阶段 1 前改动 `native_draw.cpp`。** 以下差异是真实的，但都需要特定调用条件才会触发，现阶段修复收益低于回归风险，记为后续加固项。

| 位置 | 差异 | 是否影响当前 OSD |
| --- | --- | --- |
| `ellipse()` / `roundedRect()` | 只变换中心点/包围盒角点，半径和圆角形状不随旋转、斜切、非均匀缩放变化；AGG 是对整条路径做仿射 | 否。全仓没有对 `aggDraw.hud` 调用 `rotate/scale/skew/matrix/viewport/parallelogram`，变换恒为单位矩阵 |
| `clearAll()` / `clearClipBox()` / `BlendBGRA()` | 预乘用 `/255`，AGG 用 `(v*a+255)>>8`（约 `/256`） | 仅 ±1 量化差；文字与清除色可能差 1，不构成功能错误 |
| `BlitText()` | 直接写 `m_pixels`，不检查 `m_clip`；负 stride 的行基址处理不完整 | 否。`agg_osd.cpp` 从不调用 `clipBox()`；所有 HUD 缓冲都是正的紧密 stride |
| `transformImage()` | 不做目标裁剪，负坐标或越界目标会写到缓冲外 | 否。唯一调用点是 `display.cpp` 的 `(0,0,prefilterWidth,prefilterHeight)`，恰好等于目标尺寸 |
| `setFont()` | 未知字体名静默忽略，保留旧字体 | 否。字体名来自固定表/ini，和 AGG 的查找行为在实际配置下一致 |

单核 Windows 的 `aggDraw.hud == aggDraw.screen` 且 screen 缓冲为 `NULL` 是既有行为，属于数据层问题，不是 GDI+ 后端 bug；阶段 1 用 `overlay->pixels == NULL` 安全跳过，而不是尝试修复单核 OSD。

## 11. 阶段 1 实现记录（2026-11-10）

已按 9.2 / 9.3 落地，未改 `AggDrawTarget`、`agg_osd.cpp`、`lua-engine.cpp`，也未改 Linux 适配器：

- `frontend/shared/wgpu_present.h`：新增 `WGPUPresentOverlay`；`present()` 末参 `overlay` 默认 `NULL`，原 8 参调用点无需改动。
- `frontend/shared/wgpu_present.cpp`：新增 `fsOverlay`；预乘与直通两条 overlay 管线；HUD 纹理/bind group 惰性创建；按像素行做 FNV 哈希，内容不变时跳过上传；stride 非 256 对齐时复制到 staging；顶点缓冲 18→30；每块屏幕一个 HUD quad，使用旋转后的源矩形归一化 UV；`Destroy()`/`kill()` 释放全部 overlay 资源并使哈希失效。overlay 管线创建失败只打印日志，不影响主呈现。
- `frontend/windows/wgpu_display.h/.cpp`：`present()` 透传 overlay，`ENABLE_WGPU` 与 stub 两个版本同步。
- `frontend/windows/display.cpp`：`DESMUME_GPU_HUD_OVERLAY` 打开且 HUD 缓冲有效时，WGPU 分支改为上传 overlay 并跳过 CPU 合成；HUD 缓冲为空、宏未定义或 DirectDraw 时仍走原 CPU 合成。`WGPU_DoDisplay()` 增加 overlay 参数。
- `frontend/windows/desmume.props`：新增 `UseGpuHudOverlay`，默认关闭（与第 2 节第 3 条一致），`/p:UseGpuHudOverlay=true` 打开，仅 x64 定义 `DESMUME_GPU_HUD_OVERLAY`。默认构建仍走原 CPU 合成，输出不变。

验证状态：共享层在 `-DENABLE_WGPU` 与非 WGPU 两种配置下都能用 `g++ -std=c++11` 编译；Windows GUI 与像素 golden 仍需在 Windows 上由用户执行。

## 12. 阶段 2 实现记录（2026-11-10）

阶段 2 已落地为独立编译单元，**尚未接入前端**，因此默认构建和阶段 1 的输出都不变。

### 12.1 新增文件

- `frontend/modules/osd/wgpu/wgpu_draw.h/.cpp`：`WGPUOSDAggTarget`（继承 `T_AGG_RGBA`）+ `WGPUOSDRenderer`。
- `frontend/modules/osd/wgpu/hud_shapes.wgsl`：形状管线的规范 WGSL。`wgpu_draw.cpp` 内嵌一份同样的源码（Windows 构建没有资源打包步骤），`WGPUOSDShapeShaderSource()` 暴露内嵌副本，自测会与 `.wgsl` 文件逐字节比对，防止漂移。

### 12.2 录制与绘制

`WGPUOSDAggTarget` 覆盖 `agg_osd.cpp` 用到的接口，把图元录进 `std::vector<WGPUOSDShape>` / `WGPUOSDPoint>`，不调用 AGG 的 CPU 光栅器；`render()` 时一次性上传实例缓冲，用 `6` 个顶点 × N 个实例的 SDF 管线绘制到 BGRA8Unorm 目标纹理。

| 调用 | 录制内容 | WGSL |
| --- | --- | --- |
| `rectangle()` | 中心 + 半尺寸 | `sdBox` |
| `roundedRect(...,1)` | 中心 + 半尺寸 + 半径 | `sdRoundBox` |
| `ellipse()` | 中心 + 半径 | `sdEllipse`（小半径方向缩放） |
| `polygon()`（≤64 点） | 点表 + 包围盒 | 奇偶规则 `sdPolygon`，支持凹多边形（D-pad） |
| `triangle()` | 3 点闭合多边形 | `sdPolygon`（阶段 4 加入） |
| `star()` | `2 × numRays` 点闭合多边形 | `sdPolygon`（阶段 4 加入） |
| `arc()` | 按弧长细分的开放折线 | `sdPolyline`（不闭合、无弦线，阶段 4 加入） |
| `curve()` | 二次贝塞尔 16 段开放折线 | `sdPolyline`（阶段 4 加入） |
| `line()` | 两端点 + 线宽 | `sdSegment`（圆头） |
| `fillColor` / `lineColor` / `noFill` / `noLine` / `lineWidth` | 绘制状态 | fill/stroke 分别算覆盖率，一次片元合成 |
| `fillLinearGradient` / `lineLinearGradient` | 两个颜色 + 端点 | `t = clamp(dot(p-p1,d)/dot(d,d),0,1)`，颜色/alpha 同步插值 |

目标纹理是预乘 BGRA8，blend 固定 `One / OneMinusSrcAlpha`，与阶段 1 的预乘契约一致。文字层仍是 AGG 写出的 CPU 缓冲（预乘 BGRA8），形状画完后用最近邻采样整屏叠加。

### 12.3 回退与边界

以下情况直接调用基类 AGG，输出仍然正确，但会进入文字层并在所有 GPU 形状之后合成：

- 非单位仿射、非 `BlendAlpha`；
- `roundedRect` 的 `rx != ry` 或四角不同半径变体；
- `polygon` 超过 64 点、点表为空、`line()` 时 `noLine()` 状态；
- `arc()` / `curve()` 在阶段 4 之前（现在已由 `WGPUOSDShape_Polyline` 录制）；
- 未来 Lua `gui.*` 使用的变换/裁剪/混合等（阶段 4 的 `gui.*` 仍然直接写 CPU 回退层）。

`clipBox` 目前不作用于录制图元；`agg_osd.cpp` 不调用它。线性渐变只支持默认 `profile`（`agg_osd.cpp` 也只用默认值）。

### 12.4 构建接入

- POSIX Meson：阶段 2 时依赖系统 `libagg`（`dep_agg.found()`）；阶段 4 起改为**无条件**从仓库内 `frontend/windows/agg` 源码静态编译 AGG，并把 `wgpu_draw.cpp` 加入 `libdesmume_src`、定义 `DESMUME_WGPU_OSD`（详见 §16）。
- `DeSmuME.vcxproj(.filters)`：加入 `wgpu_draw.cpp/.h` 与 `hud_shapes.wgsl`（作为 `None` 项展示）。
- `desmume.props`：新增 `UseWgpuOsd`（默认关闭），`/p:UseWgpuOsd=true` 时仅 x64 定义 `DESMUME_WGPU_OSD`。
- 该宏目前只作为**前端接线开关**，不改变编译内容：`wgpu_draw.cpp` 始终单独成 TU 编译（默认构建的运行时行为不变，也保证该文件持续被编译检查），前端接线落地时再在 WGPU 显示路径上按此宏选择 `WGPUOSDAggTarget`。暂不接线的原因：本机无法编译/运行 Windows 前端，且文字仍走 AGG 位图层，阶段 2 单独接线的收益要等阶段 3 才能完全体现。

### 12.5 验证结果

本机 aarch64 + wgpu-native 离屏自测（`/tmp/opencode/wgpu_selftest/osd_probe.cpp`，链接真实 `wgpu_draw.cpp` 与 AGG 源码）全部通过，共 55 项检查、0 失败、0 条 wgpu validation error：

- 实心矩形、圆角矩形、椭圆、凹多边形、粗线、fill+stroke 的覆盖率与颜色；
- 线性渐变左/中/右三个采样点；
- 两次 50% 叠加得到 `A=0.75, R=0.25, B=0.5` 的预乘结果；
- 文字层不透明/半透明叠加，以及未触碰区域；
- 按 `agg_osd.cpp::drawPad` 顺序复刻的整段输入显示（`scale=4`，body/screen/A 键/D-pad）；
- 内嵌 WGSL 与 `hud_shapes.wgsl` 逐字节一致。

**与 AGG CPU 的像素对拍**：同一组调用分别作用于 `WGPUOSDAggTarget` 和真实的 `T_AGG_RGBA`，读回预乘 BGRA 后逐点比较。内部像素要求 `±3`，抗锯齿边缘放宽到 `±24`。覆盖矩形 fill+stroke、`roundedRect(...,1)`、椭圆（含非圆 `rx≠ry`）、线性渐变、12 点凹多边形、5px 粗线、两次半透明叠加。全部通过；量化差通常为 1，最差边缘差在放宽范围内。

对拍暴露并修掉的问题：

- 非圆椭圆的边缘覆盖率偏低（扁平侧的径向近似把距离算大）。`sdEllipse` 改为按隐函数梯度归一化：`d = (|q| - 1) * |q| / |q / r|`，圆时退化为精确的 `(|q| - 1) * r`；修复后边缘 alpha 从 190 回到 252（AGG 251）。
- 自测过程中的两个 WGSL 问题：`type` / `in` / `out` 是保留字；多边形奇偶测试必须同时处理 `all(c)` 和 `all(!c)`，否则凹多边形会被填满。

`wgpu_draw.cpp` 在 `-DENABLE_WGPU` 与不定义 `ENABLE_WGPU` 两种配置下都能以 `-Wall -Wextra` 干净编译。Windows GUI 与真实 HUD 像素仍需在 Windows 上验证；接入前端时应把 `WGPUOSDAggTarget::textureView()` 交给阶段 1 的 overlay 路径，而不是继续上传 CPU 缓冲。


## 13. 阶段 3 实现记录（2026-11-10）

阶段 3 已把点阵文字接入 GPU，**尚未接入前端**：前端没有调用 `WGPUOSDAggTarget::setFonts()` 之前，`renderText()` 会继续走基类 AGG，默认构建和现有 HUD 输出完全不变。

### 13.1 新增文件与 API

- `frontend/modules/osd/wgpu/hud_text.wgsl`：文字管线的规范 WGSL。`wgpu_draw.cpp` 内嵌同样的 `kGlyphShader`，由 `WGPUOSDTextShaderSource()` 暴露，自测与 `.wgsl` 逐字节比对。
- `WGPUOSDFont { name, data, size }`：注册一份 AGG 点阵字体；`data` 就是 `fonts_list[].font`，`size` 可为 0。
- `WGPUOSDTextRun { font, x, y, color, charOffset, charCount }`：一次 `renderText()` 的录制结果。字符串字节放在 `m_textBytes`，`render()` 时统一展开成字形实例。
- `WGPUOSDRenderer::setFonts()` / `hasFont()`，以及 `WGPUOSDAggTarget::setFonts()` 转发。字体可以在 `beginGPU()` 前注册（随设备资源一起建图集），也可以在之后注册（重建图集）。
- `WGPUOSDRenderer::render()` 新增 `runs/runCount/text/textLength` 三个参数，绘制顺序固定为 **形状 → 字形 → CPU 回退层**。

### 13.2 字形图集与几何

- 图集格式：`R8Unorm`，固定宽 1024，高按需取 2 的幂，每个字形右边/下边留 1px；采样器复用已有的 `Nearest` + `ClampToEdge`。
- 34 份字体实测共 292,121 个字形像素，最大字形 19×18；本机打包高度 372，取 512，图集约 0.5 MB。
- 解析格式与 `agg::glyph_raster_bin` 完全一致：`height, base, firstChar, count`（`count==0` 按 256）、小端 uint16 偏移表、每字形 `width` + `((width+7)>>3)*height` 字节的 MSB-first 1bpp 行。
- 每个字形展开成一个 `GpuGlyph`（48 字节：`posSize` / `uv` / `color`），`vsGlyph` 展开 6 顶点四边形，`fsGlyph` 返回预乘颜色，blend 为 `One / OneMinusSrcAlpha`。
- 几何逐项对齐 AGG：`x` 从 `int(dstX)` 开始、按字形宽度累加；首行 `y = int(dstY) - base`；UV 取纹素边界，保证 Nearest 采样命中精确纹素。
- 文字颜色取当前 `lineColor`（直通 RGBA），不乘 `masterAlpha`，与 AGG 的 `m_lineColor` 一致。`renderTextDropshadowed()` 无需重写：基类的九次 `renderText()` 会自然录成九个 text run。

### 13.3 边界与一处刻意偏差

- 未注册字体、非单位仿射、非 `BlendAlpha`、空字符串：继续调用基类 AGG，进入 CPU 回退层。
- 矢量字体（`AGG2D_USE_VECTORFONTS`）仍走 AGG；本构建未开启。
- AGG 的 `blend_solid_hspan` 在半透明文字上写入的是**非预乘**颜色（例如白色 `a=128` 得到 `255,255,255,129`），并在字形矩形内留下 `a≈1/255` 的雾。GPU 路径输出正确的预乘值，且覆盖率 0 的像素保持全透明。OSD 的文字颜色实际总是 `a=255`（`lineText_color` / `InputDisplay` 均如此），因此没有可见变化；这属于对 AGG 潜在缺陷的有意修正，已在文档和自测中标注。
- CPU 回退层（预乘 BGRA8）仍会整屏上传，即使这一帧没有回退图元/字体；移除它是阶段 4 的收尾项。

### 13.4 构建接入

- `DeSmuME.vcxproj(.filters)`：新增 `hud_text.wgsl` 的 `None` 项，仍归入 `frontend\modules\osd\wgpu` 过滤器。
- POSIX Meson 不需要新增文件：文字实现仍在 `wgpu_draw.cpp` 内；阶段 2 的 `dep_agg` 条件在阶段 4 被自带 AGG 取代（见 §16）。
- `wgpu_draw.cpp` 在 `-DENABLE_WGPU` 与不定义 `ENABLE_WGPU` 两种配置下都能以 `-Wall -Wextra` 干净编译。

### 13.5 验证结果

本机 aarch64 + wgpu-native 离屏自测（`/tmp/opencode/wgpu_selftest/osd_probe.cpp`）扩展到 **66 项检查、0 失败、0 条 wgpu validation error**。阶段 3 新增项：

- 5 种字体（`gse4x6`、`mcs6x10_mono`、`verdana14`、`verdana17_bold`、`verdana18_bold`）的整段文字与真实 `T_AGG_RGBA` 逐像素对拍；
- `renderTextDropshadowed()` 的深色/浅色两条阴影颜色分支，各 9 次绘制；
- 文字覆盖在矩形形状之上，验证「形状先、文字后」的层序；
- 半透明白字在 GPU 目标中保持预乘（峰值 `128,128,128,128`）；
- `clear()` 会同时丢弃录制的字形；
- `hud_text.wgsl` 与内嵌 `kGlyphShader` 逐字节一致。

对拍时跳过两边 alpha 都小于 16 的像素，以排除上述 AGG 雾状残留；其余字形覆盖像素（全透明或全不透明覆盖率）完全一致。真实 Windows GUI 与 HUD 像素仍需在 Windows 上验证。


## 14. 方案 B 实现记录：自定义 ELF 段 + `__start_/__stop_`（2026-11-10）

结论：**已直接在仓库中实现，Linux/ELF 可用，且不需要 libelf**。34 份字体（59,276 字节）打包进一个 60,072 字节的只读段；`--gc-sections`、GCC LTO、`strip --strip-all`、PIE/非 PIE、静态库归档和 Clang 构建均通过，段内容与 `frontend/windows/osd_fonts/fonts/*.bin` 逐字节一致。阶段 3 的离屏 WGPU 对拍也用该段字体跑通（66 项、0 失败），说明它可以作为 POSIX 上 `fonts_list[].font` 的来源。

实现范围：**仅 POSIX/ELF 且 `dep_agg` 存在时启用**。Windows 继续走 RCDATA，macOS/Mach-O 不启用（原因见 14.5）。没有改动 `fonts_list[]`、`font_Nums`、`lookupFont()` 的名称/顺序/行为。

### 14.1 容器格式

段名必须是 C 标识符，不能带前导点，最终使用 `desmume_osd_fonts`。容器显式按**小端**编码，不依赖目标机字节序：

```
偏移 0   u8[4]  "DSF1"
偏移 4   u16    version = 1
偏移 6   u16    headerSize = 16
偏移 8   u32    totalSize
偏移 12  u16    fontCount
偏移 14  u16    reserved = 0

每个字体项：
  u16  nameLen
  u16  reserved = 0
  u32  dataLen
  u8   name[nameLen]         // 不含 NUL，只允许 [A-Za-z0-9_.-]
  pad 到 4 字节
  u8   data[dataLen]         // AGG 点阵字体原始 blob
  pad 到 4 字节
```

段尾正好落在 `totalSize`，不允许多余字节。生成器先计算 `totalSize`，再写头部；条目数据 4 字节对齐。当前 34 项总开销为 `60,072 - 59,276 = 796` 字节，段对齐 16 字节。

生成器 `desmume/src/frontend/windows/osd_fonts/generate_elf_font_section.py` 用 `.incbin` 嵌入每个 `.bin`，并用 `.byte` 显式写小端字段（不用 `.short`/`.long`，避免大端目标下变成目标字节序）。名称与顺序取自 `fonts_list[]`，但运行时兼容契约仍是**按名称查找**；顺序只用于诊断和确定性。生成器接受 `OUTPUT.S [--font-dir DIR]`，默认字体目录是同目录下的 `fonts/`。

### 14.2 链接与保留

汇编在段首定义 `desmume_osd_fonts_anchor`，provider 强引用它：

- 对 `--gc-sections`：provider 可达时，锚点引用让整个段保留；
- 对静态库：`__start_/__stop_` 不是普通未定义符号，不能把段对象从 `.a` 里拉出来；锚点是普通未定义符号，可以。实测 provider 和段在同一个 `libfonts_all.a` 里、对象顺序相反也能链接；
- provider 对 `__start_desmume_osd_fonts` / `__stop_desmume_osd_fonts` 使用 weak 声明；非 ELF 平台完全不引用锚点，`Init()` 返回 "ELF-only"。ELF 构建如果完全没链接段对象，锚点的强引用会直接导致链接失败——这是有意的构建期错误，避免把空字体表带到运行期。

段 flags 为 `A`（alloc，只读），类型 `PROGBITS`。`strip --strip-all` 后段仍在；`__start/__stop` 符号从符号表消失也不影响运行，因为重定位在链接时已经完成。

### 14.3 仓库文件与 Meson 接入

新增/修改的仓库文件：

- `desmume/src/frontend/windows/osd_fonts/generate_elf_font_section.py`：从 34 个 `.bin` 生成 `osd_fonts_section.S`；
- `desmume/src/frontend/modules/osd/agg/osd_fonts_section.h`：provider API（`OSDEmbeddedFonts_Init/Count/At/Find/LastError`，测试时另有 `ParseSection`）；
- `desmume/src/frontend/modules/osd/agg/osd_fonts_section.cpp`：ELF 段解析、容器校验、`__start_/__stop_` 与锚点处理；
- `desmume/src/frontend/modules/osd/agg/aggdraw.cpp`：`Agg_init()` 先尝试段字体，按 `fonts_list[i].name` 填充指针；
- `desmume/src/frontend/posix/meson.build`：`custom_target('osd-fonts-section')`、provider 源文件、`-DDESMUME_ELF_FONTS`；
- `desmume/src/frontend/posix/meson_options.txt`：`osd-elf-fonts`（boolean，默认 `true`）。

启用条件（三者同时满足）：`dep_agg.found()`、`get_option('osd-elf-fonts')`、`host_machine.system()` 属于 ELF 主机列表（`android, dragonfly, freebsd, gnu, linux, netbsd, openbsd, solaris`）。`custom_target` 的 `depend_files` 列出全部 34 个 `.bin`，字体更新会触发重新生成；生成的 `.S` 与 provider 一起加入 `libdesmume`。

运行期语义：

```
Agg_init()
  -> OSDEmbeddedFonts_Init()          // ELF 段有效时
       -> 对每个 fonts_list[i]，按名称查段内字体并覆盖 .font
  -> Agg_init_fonts()                 // 原有字体表/查找逻辑不变
```

`AGG_FONT(x)` 仍然展开为 `agg::x`，所以段不可用（非 ELF、未启用选项、解析失败）时自动回退到 libagg 的静态数组，不改变 `lookupFont()` 与旧 ini/Lua 字体名。锚点 `desmume_osd_fonts_anchor` 是强引用：一旦开启 `DESMUME_ELF_FONTS` 却漏链段对象，会在链接期失败，而不是悄悄得到空字体表。

### 14.4 验证矩阵

独立 provider 验证全部为 `PASS: 507 check(s), 0 failure(s)`：

| 变体 | 结果 |
| --- | --- |
| GCC 16，默认 PIE / `-no-pie` | 通过 |
| `-ffunction-sections -fdata-sections -Wl,--gc-sections` | 通过，段仍在 |
| GCC `-flto` + `--gc-sections` | 通过，段仍在 |
| Clang 23（普通 / `--gc-sections`） | 通过；本机无 lld，Clang LTO 未测 |
| GNU ld / gold / mold（含 `--gc-sections`） | 通过，段仍在 |
| 段对象单独归档、provider 单独归档、两者同档 | 通过（含相反对象顺序） |
| `strip --strip-all` / `objcopy --strip-all` | 通过，段仍在 |
| `objcopy --dump-section` 后 Python 独立解析 | 34 项、60,072 字节、字节一致、顺序一致 |
| `ldd` | 无 libelf 依赖 |

507 项检查覆盖：段起止/头部字段/大小、独立遍历与 provider 遍历一致、34 个名称与顺序、每个 blob 与参考 `.bin` 逐字节比较、AGG 字形偏移表边界与派生长度、重复/空/超长/非法名称、截断/尾随/保留字段、错误高度等畸形容器拒绝。畸形输入全部返回明确错误且不越界读取。

阶段 3 联调：在既有 `osd_probe.cpp` 上加 `-DUSE_ELF_SECTION_FONTS`，链接 section + provider，把 `fonts_list[].font` 全部指向段内数据，再跑原有的 GPU/AGG 文字对拍。结果：

```
ELF section fonts: 34 entries, 60072 bytes
PASS: 0 failure(s)
```

66 项检查、0 条 wgpu validation error；5 种字体、两种阴影分支、文字覆盖形状、半透明预乘、`clear()` 全部通过。GCC `--gc-sections` 和 `-flto` 版本也各跑了一遍，结果相同。

**Meson 端到端验证**：本机没有发行版 libagg，阶段 4 已改为直接静态编译仓库内 `frontend/windows/agg` 的源码（见 §16），不再需要假 `libagg`。用该配置构建完整 POSIX 工程，`-Dopengl=true -Degl=true`。`libdesmume`、`cli/desmume-cli`、`gtk/desmume` 全部编译链接成功。链接产物中：

```
readelf -SW cli/desmume-cli | grep desmume_osd_fonts
  [15] desmume_osd_fonts PROGBITS ... 00eaa8  A  0 0 16

nm cli/desmume-cli | grep desmume_osd_fonts
  0000000000ac21b0 R desmume_osd_fonts_anchor
  0000000000ac21b0 R __start_desmume_osd_fonts
  0000000000ad0c58 R __stop_desmume_osd_fonts
```

段大小 `0xEAA8` = 60,072，锚点与 `__start` 地址相同，`__stop - __start` 正好等于段大小，说明 `custom_target` 生成的汇编被正确编入并保留。`gtk/desmume` 也包含同名同大小的段。另用同一组仓库文件搭了最小 Meson 工程（`custom_target` + `static_library` + `probe`），`meson test` 通过 1/1。

在真实链接产物上调用 `Agg_init()` 后检查：`fonts_list[0].font == __start_desmume_osd_fonts + 32`（第一个条目的数据偏移），且 **34/34** 个 `fonts_list[i].font` 都落在 `[__start, __stop)` 内，证明 `Agg_init()` 确实优先采用了段内字体而不是 libagg 数组。另以 `-Dosd-elf-fonts=false` 重新配置：`build.ninja` 中不再有 `osd-fonts-section` 目标，`aggdraw.cpp` 仍可正常编译，回退路径成立。

### 14.5 Mach-O / macOS 说明

`__start_<section>` / `__stop_<section>` 是 **ELF 链接器**合成的符号，Mach-O 没有对等机制，因此本方案不能原样用于 macOS：

- Mach-O 的等价写法是 `section$start$__SEGMENT$__section` / `section$end$__SEGMENT$__section`（例如 `section$start$__DATA$__osdfonts`），或者改用方案 A（生成 C 数组表）。
- 本实现因此只引用 `__start_/__stop_` 与锚点，并把主机限制在 ELF 列表；provider 在 `#if defined(__ELF__)` 之外直接返回 "custom-section font embedding is ELF-only"，不会引入无法链接的符号。
- macOS cocoa 前端目前**不编译** `osd/agg`（`cocoa/` 下不引用 `aggdraw.cpp` / `agg_osd.cpp` / `Agg_init()`），所以该限制当前不影响任何 macOS 构建；它仍然使用自己的 `ClientDisplayView` 渲染路径。将来若 cocoa 接入这套 POSIX OSD，需要补一个 Mach-O provider 或方案 A。

### 14.6 未验证项与已知限制

- 32 位 ELF（本机无 multilib）和大端 ELF 运行（无交叉工具链/模拟器）；容器字段已按字节写死小端，理论上不依赖目标字节序。
- 完整 POSIX 构建在本机使用仓库内 AGG 源码静态编译（阶段 4 起，见 §16），不是发行版 libagg/fontconfig 包；真实发行版的 fontconfig 组合仍建议在实际打包环境跑一遍。
- Windows 构建与真实 GUI/HUD 像素（RCDATA 路径未改）。
- macOS/Mach-O 构建（原因见 14.5）。
- `custom_target` 生成的 `.S` 位于构建目录，`.incbin` 使用字体目录的绝对路径，这是 Meson 生成文件，不会写进仓库。


## 15. 阶段 4 实现记录：前端接线与 GPU HUD 目标（2026-11-10）

阶段 4 把阶段 2/3 的 `WGPUOSDAggTarget` 真正接到 Windows 的 WGPU 显示路径上。目标是在 WGPU 分支**不再把 HUD 合成到 CPU 帧缓冲**，而是把 256×384 的 HUD 目标作为纹理直接交给阶段 1 的 overlay 管线。所有改动仍未提交，Windows 本机无法编译/运行，实际 GUI 与像素验证由用户在 Windows 侧完成。

### 15.1 共享呈现层

`frontend/shared/wgpu_present.h/.cpp`：

- `WGPUPresentOverlay` 新增 `const void *textureView`。非空时优先使用外部 `WGPUTextureView`，忽略同一结构里的 `pixels/width/height/strideBytes`；这样 WGPU OSD 的渲染纹理可以**零拷贝**进入 overlay，不必再读回 CPU。
- 外部视图的 bind group 按每次 `present()` 创建，提交后立即释放；所有权仍属于调用方（OSD 渲染器），呈现层不销毁它。
- `WGPUPresent` 新增 `instance()` / `device()` / `queue()`（均为 `void*`），供前端取得 WGPU 对象来创建 OSD 渲染器；生命周期由呈现层自己持有。
- 原有像素路径完全保留：外部视图为空时仍上传 CPU 缓冲，作为 GPU HUD 失败时的回退。

overlay 的 UV 仍按实际呈现帧尺寸归一化，所以 256×384 的 HUD 在任意滤镜/缩放比例下都映射到正确位置。

### 15.2 Windows 前端

`frontend/windows/wgpu_display.h/.cpp` 透传新的 overlay 字段与访问器，`ENABLE_WGPU` 与 stub 两个版本保持同步。

`frontend/windows/display.cpp`：

- `WGPU_HudTarget()`：惰性创建 `WGPUOSDAggTarget`，复用 `WGPUPresent` 的 instance/device/queue。
- `WGPU_HudBegin()`：注册 `fonts_list[]`（名称/顺序不变），并从 INI 的 `[Display] HUD Font` 同步当前字体，保证旧配置与 Lua 字体选择一致。
- `WGPU_HudRender()`：把录制的形状/文字渲染到 HUD 纹理，返回 `textureView()`。
- 绘制 HUD 前用 `Agg_setHudTarget(hudTarget)` 临时替换 `aggDraw.hud`，绘制后恢复；因此 `agg_osd.cpp`、Lua `agg.*` 都无需知道 WGPU 的存在。
- 只有 `needsHUDDrawing && isRunningWGPU && !CommonSettings.single_core()` 时才启用 GPU HUD；`single_core()` 仍走原路径，避免与单核渲染线程的 HUD 缓冲竞争。
- GPU HUD 创建/渲染失败，或 overlay 不支持外部视图时，自动回退到原来的 CPU HUD 像素上传。
- 宏开关：`desmume.props` 新增 `DESMUME_WGPU_HUD_OVERLAY`（默认关闭，`/p:UseWgpuHudOverlay=true` 打开）。

`frontend/modules/osd/agg/aggdraw.h/.cpp`：

- `Agg_setHudTarget(AggDrawTarget *)`：安装/恢复 HUD 目标；`NULL` 恢复 `Agg_init()` 记录的 `defaultHudTarget`。
- `Agg_setCustomSize()` 改为调整 `targets[1]`（AGG HUD 目标），而不是 `aggDraw.hud`，以免在前端临时替换目标时改错对象。

### 15.3 边界

- 本阶段只在 Windows 前端接线。GTK 仍保持原样：其截图与 Cairo 回退路径依赖 CPU HUD 缓冲，贸然切换会影响截图与无 WGPU 环境，留待单独评估。
- Windows RCDATA 字体、OGL `srcHUD`、DirectDraw 路径均未改动。
- 已知层序限制：Lua `gui.*` 写入 CPU 回退层，整体在 GPU 形状/字形之后合成；混合使用 `agg.*` 与 `gui.*` 时无法严格保持调用顺序（见 §17）。


## 16. POSIX 自带 AGG 静态编译（2026-11-10）

POSIX 之前依赖系统 `libagg`，但多数发行版不提供该包，导致 WGPU OSD、软件 HUD 和 Lua `agg.*` 在 Linux 上默认不可用。现在改为**无条件**从仓库内 `frontend/windows/agg` 源码静态编译，不再需要外部 libagg。

`frontend/posix/meson.build`：

- `agg_sources` 列出 25 个 `frontend/windows/agg/src/*.cpp`（arc、bezier、curves、embedded fonts、image filters、rounded rect、stroke、trans、vcgen 等），直接并入 `libdesmume`。
- POSIX 上无条件定义 `HAVE_LIBAGG`；`osd/agg` 的软件 HUD 路径与 WGPU OSD 因此始终参与编译。
- 只有同时找到 `freetype2` 与 `fontconfig` 时才定义 `AGG2D_USE_VECTORFONTS`；`agg_font_freetype.cpp` 仅在 `dep_freetype.found()` 时编译，避免无字体环境下的硬依赖。
- `cli` / `gtk` / `gtk2` 的 Meson 文件补上 AGG include 目录。
- `frontend/windows/agg/font_freetype/agg_font_freetype.cpp` 做了一处可移植性修正：`tags` 改为 `const char*` 并显式转换，使其在没有 `-fpermissive` 的 GCC/Clang 下也能编译。

验证：在没有任何系统 libagg 的干净构建目录重新配置并编译，`libdesmume.a`、`desmume-cli`、`desmume` 全部成功；编译命令中可见 `-DHAVE_LIBAGG`、`-DAGG2D_USE_VECTORFONTS`、`-DDESMUME_WGPU_OSD`、`-DDESMUME_ELF_FONTS`，链接产物不依赖 libagg。

macOS 仍不使用这套 AGG：cocoa 前端不编译 `osd/agg`，并且 `WGPUOSDAggTarget` 继承 `T_AGG_RGBA`。因此 macOS 的 Lua 图形支持不能靠 AGG 回退，必须由 WGPU 录制器覆盖 Lua 的全部绘图操作，见 §17。


## 17. Lua 图形/文字支持（WGPU 原生路径，2026-11-10）

Lua 是 HUD 的关键消费者：在 macOS 上 AGG 不可用，脚本通过 `agg.*` / `gui.*` 画的所有图形和文字都必须走原生 WGPU 后端。因此阶段 4 不再把 Lua 排除在 GPU HUD 之外，而是让录制器覆盖 Lua 的操作集。

### 17.1 `agg.*`：录制为 GPU 图元

- `triangle()`、`star()`：录成闭合多边形，走 `sdPolygon` 奇偶填充。
- `arc()`、`curve()`：录成新的 `WGPUOSDShape_Polyline = 5`。多边形点数上限从 16 提高到 64；`polyline` 只计算线段距离、**不闭合**、**不填充**，与 AGG 的 `StrokeOnly` 一致（不会多出一条弦线）。
- `polygon()`：上限同步提高到 64 点。
- `agg.text()`：仍走阶段 3 的位图字形录制；`lineColor` 决定文字颜色。

### 17.2 `gui.*`：CPU 回退层

`gui.pixel/box/line/text` 直接写 BGRA 缓冲，不经过绘制调用录制。`lua-engine.cpp::prepare_drawing()` 在 `HAVE_LIBAGG` 下每帧从 `aggDraw.hud` 刷新 `curGuiData.data/stridePix`，因此当前端安装 WGPU HUD 目标时，这些像素会写进该目标的 CPU 层，并由 WGPU 渲染器作为回退层上传、在 GPU 形状/字形之后合成。

这也是 §15.3 的层序限制来源：`gui.*` 与 `agg.*` 混用时，前者总在最上层。对现有 HUD 与多数脚本无影响，但需要严格 z-order 的脚本应统一使用 `agg.*`。

### 17.3 自测扩展

离屏自测 `/tmp/opencode/wgpu_selftest/osd_probe.cpp` 扩展到 **89 项检查、0 失败、0 条 wgpu validation error**。新增：

- `triangle`、`arc`、`star`、`curve`、24 点 `polygon` 与真实 `T_AGG_RGBA` 的逐点对拍；其中 `arc` 专门检查弦线位置保持透明，验证 `sdPolyline` 的开放路径语义；
- `gui.box` 区域与 `gui.pixel` 半透明像素经 CPU 层上传后的合成结果；
- 原有矩形/圆角矩形/椭圆/渐变/12 点凹多边形/粗线/半透明叠加/5 种字体/阴影/文字覆盖形状等检查全部保留；
- `hud_shapes.wgsl`（6,065 字节）与 `hud_text.wgsl`（1,941 字节）仍与内嵌副本逐字节一致。

### 17.4 未完成项

- `WGPUOSDAggTarget` 继承 `T_AGG_RGBA`，所以 macOS 当前仍无法编译它。要在无 AGG 的 cocoa 前端启用，需要提供一个不依赖 AGG 的 `AggDrawTarget` 兼容目标；录制器本身已经覆盖 Lua 的绘图操作集（矩形/圆角矩形/椭圆/多边形/线/弧/曲线/星形/文字）。**该缺口已由 §18 的 Metal 后端补齐**（不复用 `AggDrawTarget` 类型，而是提供方法名相同的 AGG-free 录制目标）。
- Windows 前端与真实 GUI/HUD 像素仍需用户在 Windows 上验证。


## 18. Cocoa Metal 后端实现记录（2026-11-10）

把阶段 2/3 的 WGPU OSD 后端转写为 Metal，并接入 cocoa 前端。§8.5 里「macOS 不适用」的结论针对的是 WebGPU 阶段 1；本节是独立的 Metal 实现，走 cocoa 自己的 `MacMetalDisplayView`。

### 18.1 新增文件（`frontend/cocoa/userinterface/`）

| 文件 | 内容 |
| --- | --- |
| `MacMetalOSDDraw.h/.cpp` | AGG-free 录制目标 `MacMetalOSDDrawTarget`（方法名与 `AggDrawTarget` 子集一致）+ 双缓冲帧交换。纯 C++，`lua-engine.cpp` 可直接 include |
| `MacMetalOSDFontData.cpp` | 34 份点阵字体（59,276 字节）编译期 C 数组，名称/顺序与 `fonts_list[]` 一致。Mach-O 没有 §14 的 `__start_/__stop_` 段，故用方案 A |
| `MacMetalOSDRenderer.h/.mm` | `MacMetalOSDRenderer`：三条离屏管线 + 字形图集 + 256×384 预乘 BGRA8 HUD 纹理 + 合成管线 |
| `MacMetalOSDShaders.metal` | MSL 版 `hud_shapes.wgsl`（SDF）、`hud_text.wgsl`（字形）、`kTextShader`（CPU 回退层），外加 `osd_overlay_*` 合成着色器 |

MSL 结构体 `OSDShapeInstance`（672 字节）与 C++ `MetalOSDInstance` 用 `static_assert` 锁定；`MetalOSDGlyph` 48 字节、3×`float4` @0/16/32、per-instance。所有离屏管线沿用 WGPU 的 `One / OneMinusSrcAlpha` 预乘契约，采样 Nearest + ClampToEdge。

### 18.2 与 WGPU 版的对应关系

| WGPU | Metal |
| --- | --- |
| `WGPUOSDAggTarget : T_AGG_RGBA` | `MacMetalOSDDrawTarget`（无基类、无 AGG 头） |
| `WGPUOSDRenderer` | `MacMetalOSDRenderer`（ObjC++，持有 MTL 对象） |
| WGSL `vsShape/fsShape` | `osd_shape_vertex/osd_shape_fragment`（`[[instance_id]]` + device 实例缓冲，SDF 逐行照搬） |
| WGSL `vsGlyph/fsGlyph` | `osd_glyph_vertex/osd_glyph_fragment`（`MTLVertexDescriptor` per-instance） |
| 内嵌 `kTextShader` | `osd_blit_vertex/osd_blit_fragment` |
| `WGPUPresentOverlay::textureView` 合成 | `osd_overlay_*`，在 cocoa 的同一个 `rce` 里、Lua overlay 之后 / FPS HUD 之前贴两张屏幕 quad |

有意的偏差：

- 没有 AGG 回退层，所有 `agg.*` 调用都录制；`roundedRect(rx≠ry)` / 四角不同半径用较小半径近似（WGPU 会退回 AGG）。
- 字体数据改为编译期数组（§14.5 的 Mach-O 限制）。
- 目标坐标仍是 256×384 像素、左上原点；合成时把上半张贴主屏、下半张贴触摸屏，UV 用归一化值，位置变换与 `display_output_vertex` 完全一致（cocoa 的顶点缓冲是**中心原点**像素坐标，投影矩阵没有 -1 偏移）。
- `gui.*` 保持原样：它写的 `blend32()` 是**直通** alpha，而 CPU 回退层是预乘的，两者不能共用一条混合管线。新后端的 CPU 层通过 `pixelBuffer()` 提供，仅在使用时才上传（按 ring slot 分配纹理，避免 in-flight 写冲突）。

### 18.3 接线点

- `DeSmuME_Prefix.pch`：`ENABLE_APPLE_METAL` 时定义 `DESMUME_COCOA_METAL_OSD`。
- `cocoa_core.mm` / `macOS_driver.cpp`：帧首 `MacMetalOSD_ClearBack()`、帧尾 `MacMetalOSD_PresentBack()`，与 `lua_script_*_graphics_buffer()` 并列。
- `lua-engine.cpp`：三个 `#ifdef HAVE_LIBAGG` 的 `agg.*` 实现放宽为 `HAVE_LIBAGG || DESMUME_COCOA_METAL_OSD`，用 `OSD_HUD_TARGET` 宏选择 `aggDraw.hud` 或 `MacMetalOSD_BackTarget()`；`gui.osdtext()` 在无 AGG 时转 `MacMetalOSD_AddFixedText()`。
- `MacMetalDisplayView.h/.mm`：`setup` 创建 renderer、`dealloc` 释放；`renderForCommandBuffer:` 开头先编码离屏 OSD pass（同一 command buffer，覆盖 drawable / 截图 / 录像三条路径），再按显示模式把 HUD 纹理贴到屏幕 quad 上；合成 PSO 按当前 colorAttachment 格式选 BGRA/RGBA 两套。
- `DeSmuME (Latest).xcodeproj/project.pbxproj`：6 个文件引用 + 44 条 `PBXBuildFile`；纯 C++ 文件进全部 12 个 Sources phase，`.mm`/`.metal` 进 10 个 Metal phase。

### 18.4 验证

- `xcodebuild -target "DeSmuME (macOS App; AppleSilicon -- Latest Xcode)" -configuration Debug`：**BUILD SUCCEEDED**，0 error；`default.metallib` 含 8 个 `osd_*` 函数。
- 独立 GPU 冒烟测试（`/tmp/metal_osd_probe.mm`，Apple M1 Pro）：8 类图元 + 9 次阴影文字 + CPU 回退层全部录制；MSL 运行时编译通过；两条离屏 pass 与合成 pass 的 command buffer 均 `Completed` 且无 error；空帧正确跳过。
- 像素回读：50% 红色矩形为 `BGRA(0,0,128,128)`（预乘精确值），未触及区域全透明；点阵字 "WGPUMETAL" 产生 408 个覆盖像素；合成到 BGRA8 目标后 `(50,25) = (38,25,141,255)`，与 `src.rgb + dst.rgb*(1-src.a)` 的理论值一致。
- `MacMetalOSDRenderer.mm` / `MacMetalOSDDraw.cpp` / `MacMetalOSDShaders.metal` 在 `-Wall -Wextra` 与 `-std=macos-metal1.1` 下干净编译。
- **真实 Lua 驱动的端到端测试**（`documentation/lua-api/examples/osd-shapes.lua`）：用 LuaJIT 2.1 跑 `gui.register` + `emu.frameadvance` 90 帧，中途抓 3 张 PNG。绘制状态机修好后所有图元（矩形/描边/圆角矩形/椭圆/三角形/星形/弧/贝塞尔曲线/线宽阶梯/滑动条/脉冲圆角矩形）与 34 份字体中的 4 份采样、阴影文字全部出现，坐标正确。
- 多帧与空帧：帧 0 非透明像素 900，帧 1（含动画）1380，说明双缓冲每帧重录生效；完全不绘制的帧 `drew=0`，command buffer 无 error。

### 18.5 未验证项与边界

- 真实 GUI 窗口（旋转 × 布局 × 滤镜 × Retina 缩放、OpenEmu 路径）未实机验证；截图/录像路径共用同一个 `renderForCommandBuffer:`，理论上会自动覆盖。
- cocoa 的 FPS/输入 HUD 仍是原有 FreeType 字形图集路径，本次没有改动；`gui.*` 也仍是原 256×384 双缓冲路径。两套 OSD 与现有 HUD 的层序是：NDS 屏幕 → Lua `gui.*` overlay → Metal OSD（agg.* / gui.osdtext）→ FPS HUD。
- 核心 `BaseDriver::AddLine()` 在 cocoa 上没有调用方，未接入；`OSDCLASS` 仍为空壳。
- 非默认 blend、仿射变换、裁剪框、矢量字体依旧不支持（与 WGPU 版一致）。

### 18.6 首轮测试暴露的 bug（已修）

Lua 端到端测试第一次跑出来只有矩形、圆角矩形和文字，椭圆/三角/线/星/弧/曲线/半透明全部消失。根因是绘制状态语义与 AGG 不一致：

- **`fillColor()` / `lineColor()` 没有重新打开填充/描边。** `Agg2D` 与 `WGPUOSDAggTarget` 在这两个函数里都会置位 `m_hasFill` / `m_hasStroke`，而 `MacMetalOSDDrawTarget` 只赋值颜色，于是脚本一旦调用过 `noFill()` / `noLine()`，之后即使再设颜色也永远画不出来——恰好 `agg.noFill()` 在「只描边」的画法里很常见。已在两个函数（以及 `fillLinearGradient()` / `lineLinearGradient()`）里补上置位。
- **`arc()` / `curve()` 未检查描边状态。** 它们是开放路径，只描边不填充，`noLine()` 状态下应直接跳过；原实现会照样塞实例，与 AGG 行为不同。已加 early-return。
- **`MacMetalOSD_AddFixedText()` 未保存/恢复描边状态**，会污染后续 `agg.*` 调用。已改为存旧值—设置—还原。

修复后同一脚本的非透明像素数从 8647 涨到 16648，`documentation/lua-api/examples/osd-shapes.lua` 里专门留了一个 paint-state 回归小部件：先 `noFill()` 画空心矩形，再 `fillColor()` 画实心矩形，两个都得出现才算过。


## 19. Windows 链接修复：无条件链接 libagg（2026-11-10）

### 19.1 现象

Windows x64 默认配置（`UseNativeOsd` 未指定，即 GDI+ 后端）链接时 `wgpu_draw.obj` 报 32 条 `LNK2001`，全是 `agg::*` 符号：`curve4_inc` / `curve3_inc` / `curve3_div` / `curve4_div`、`rounded_rect`、`vcgen_stroke`、`trans_affine`、`image_filter_lut`、`bezier_arc` / `arc` / `bezier_arc_svg`，以及 `gse4x6` / `g_elder_bit_table` / `g_sqrt_table`。

### 19.2 根因

- §12.4 决定 `wgpu_draw.cpp` **无条件**参与编译（保持持续编译检查），且 `WGPUOSDAggTarget : public T_AGG_RGBA`（`wgpu_draw.h:193`）。基类 `Agg2D` 的内联构造函数与回退路径会把 `agg::*` 引用写进 `wgpu_draw.obj`。
- 路线 1 落地时 `frontend/windows/desmume.props` 把 `agg-2.5*.lib` 收窄为**仅** `UseNativeOsd=false` 才链接，注释写的是「libagg is only needed by the legacy AGG backend」。这对 `aggdraw.cpp` 成立（`DESMUME_NATIVE_DRAW` 下改用 `NativeDrawTarget`，且 `AGG_FONT(x)` 展开为 `NULL`，`aggdraw.cpp:79`），但对 `wgpu_draw.obj` 不成立。
- 两者叠加：默认构建和 `/p:UseWgpuOsd=true /p:UseGpuHudOverlay=true`（阶段 4）都会缺 AGG 符号；Win32 同样受影响。这是链接配置遗漏，不是运行时或像素问题，录像/截图/输出行为不受影响。

### 19.3 修复

`frontend/windows/desmume.props`：把 x64 的 `agg-2.5-x64.lib`、Win32 的 `agg-2.5.lib` 改为**无条件**链接，并更新注释说明两个消费者（legacy AGG 后端 + 始终编译的 `wgpu_draw.cpp`）。这与 §16 中 POSIX「无条件静态编译仓库内 AGG 源码」是同一个结论：只要 `WGPUOSDAggTarget` 还以 AGG 为基类，AGG 就是这个构建的一部分。

- 未改 `AggDrawTarget`、`agg_osd.cpp`、`lua-engine.cpp`，未改任何开关默认值；`UseNativeOsd=false` 原本就链接该库，现在只保留一份链接行，不会重复。
- `frontend/interface/windows/desmume.props` 本来就无条件链接 AGG，无需改动。

### 19.4 静态复核

- 报错涉及的全部 `agg::` 符号名在 `agg/agg-2.5-x64.lib` 中命中（31 组前缀、0 缺失）；`agg-2.5.lib` 也含 `gse4x6` / `g_sqrt_table`。两份库的修饰名分别对应 x64 与 Win32。
- 库的 `/DEFAULTLIB` 指令只有 `uuid.lib`，没有 `LIBCMT` / `MSVCRT`，不会与工程的 `/MD` 运行时冲突。
- `desmume.props` XML 解析通过，两条链接行分别带 x64 / Win32 条件。

### 19.5 仍待用户执行 / 备选方案

本机不做 Windows 完整构建；请用原命令重新 `msbuild` 确认 0 error。若坚持「默认 exe 完全不含 AGG」的目标，备选方案是在 `UseWgpuOsd != true` 时把 `wgpu_draw.cpp` 设为 `ExcludedFromBuild`，并把库链接条件改为 `UseNativeOsd=false || UseWgpuOsd=true`；代价是默认构建不再编译检查该文件，与 §12.4 的取舍冲突，本次未采用。

> **本节已被 §20 取代**：§20 把 WGPU 录制器改造成 AGG-free 后，`agg-2.5*.lib` 已改回仅 `UseNativeOsd=false` 时链接。§19 的无条件链接不再需要。


## 20. WGPU 原生 2D 后端：AGG-free 录制器（2026-11-10）

按「⑤ 想让 WGPU 也原生」实现：把 §18 的 Metal AGG-free 录制器反向移植到 WGPU 侧，让 `wgpu_draw.obj` 不再引用 `agg::*`，并撤销 §19 的无条件 libagg 链接。Windows GUI 与像素验证仍由用户执行。

### 20.1 设计决策：继承抽象接口，但不实例化 AGG

⑤ 建议「不复用 `AggDrawTarget` 类型」，那是 cocoa 的约束（cocoa 不编译 `osd/agg`，没有这个类型）。Windows 不同：`display.cpp` 阶段 4 通过 `Agg_setHudTarget(AggDrawTarget*)` 安装目标，`agg_osd.cpp` 与 `lua-engine.cpp` 经 `aggDraw.hud`（同样是 `AggDrawTarget*`）调用它。若做成完全独立的类，就得改业务层。

因此 `WGPUOSDDrawTarget : public AggDrawTarget`，但**只继承纯虚接口**，不实例化 `Agg2DBase` / `AggDrawTargetImplementation<T>` / `T_AGG_RGBA`。接口签名里的 `AggColor`(`agg::rgba8`)、`RectD`(`rect_d`)、`Affine`(`trans_affine`)、`Transformations`、各枚举与 `agg::rendering_buffer` 全是 header-only POD，不产生 libagg 符号。探针（20.4）证明这条路径成立。

### 20.2 新增 / 改造文件

| 文件 | 动作 |
| --- | --- |
| `frontend/modules/osd/wgpu/wgpu_osd_draw.h/.cpp` | **新增** `WGPUOSDDrawTarget`（替代 `WGPUOSDAggTarget`）。POD 颜色 `WGPUOSDColor`、自有 256×384 预乘 BGRA8 CPU 层、名字→字体表、复用 `WGPUOSDRenderer`。实现 `AggDrawTarget` 全部纯虚：约 30 个走录制，其余为 no-op / 默认状态 |
| `frontend/modules/osd/wgpu/wgpu_draw.h/.cpp` | **改造**：删掉 `#include ../agg/aggdraw.h` 与整个 `WGPUOSDAggTarget`（约 549 行），只剩 POD 结构、`WGPUOSDRenderer` 与 WGSL。两种配置下都 AGG-free |
| `frontend/windows/display.cpp` | 阶段 4 改 include `wgpu_osd_draw.h`，类型名 `WGPUOSDAggTarget` → `WGPUOSDDrawTarget`。`beginGPU/setFonts/render/textureView/targetWidth/targetHeight` 接口不变，`Agg_setHudTarget()` 接线不变 |
| `frontend/windows/desmume.props` | **撤销 §19 的无条件链接**：`agg-2.5-x64.lib` / `agg-2.5.lib` 改回仅 `UseNativeOsd=false`。注释说明两个 WGPU obj 已无 `agg::*` 符号 |
| `DeSmuME.vcxproj(.filters)` | 新增 `wgpu_osd_draw.cpp/.h` 两个项，归入 `frontend\modules\osd\wgpu` 过滤器 |
| `frontend/posix/meson.build` | `libdesmume_src` 加入 `wgpu_osd_draw.cpp` |
| `aggdraw.h` / `wgpu_draw.cpp` / Metal 注释 | 同步目标名，避免文档与代码不一致 |

没有改 `agg_osd.cpp`、`lua-engine.cpp`、`Agg_setHudTarget()` 的签名，也没有改 `WGPUOSDRenderer`（⑤ 预判正确：它的接口本来就是 `void*` + POD）。

### 20.3 相对旧录制器的行为变化

| 项 | 旧 `WGPUOSDAggTarget` | 新 `WGPUOSDDrawTarget` |
| --- | --- | --- |
| 基类 | `T_AGG_RGBA`（Agg2D CPU 光栅器） | 抽象 `AggDrawTarget`，无 CPU 光栅器 |
| 非单位仿射 / 非 `BlendAlpha` / clip / radial gradient / 路径命令 / 图像变换 | 回退 AGG | no-op / 默认状态；`agg_osd.cpp` 与 Lua `agg.*` 不调用 |
| `roundedRect(rx≠ry)`、四角不同半径 | 回退 AGG | 取较小半径近似，与 §18 Metal 一致 |
| 8 参数 `curve()`（三次贝塞尔） | 回退 AGG | 16 段开放折线，走 `sdPolyline` |
| 未注册字体 / 空串文字 | 回退 AGG 位图字 | 直接跳过（用 `hasFont()` 判断） |
| 字体查找 | `AggDrawTarget::lookupFont()` | `setFonts()` 注册的名字表；名称/顺序仍来自 `fonts_list[]`，兼容旧 INI 与 Lua |
| `clearAll` / `clearClipBox` | AGG 清屏 | 写自有 CPU 层（预乘 BGRA8） |
| `renderTextDropshadowed` | 继承基类默认实现 | 覆盖，保存/恢复完整描边状态（含渐变端点），规避 §18.6 的 paint-state bug |
| CPU 层上传 | 整屏每帧上传 | 不变（§13.3 的收尾优化仍未做） |

`gui.*` 仍通过 `buf()` 直接写 CPU 层，`prepare_drawing()` 的 `aggDraw.hud->buf().buf()` / `stride_abs()` 契约不变。

### 20.4 可复现的静态验证

本机工具链 `C:\msys64\clang64\bin\g++`（clang 前端）+ `nm`，不需要安装任何东西。

1. **接口探针**：只 `#include "aggdraw.h"` 并引用 `AggDrawTarget*`，编译出的 obj 用 `nm -uC` 查 `agg::` / `Agg2D` → 0 命中。证明继承抽象接口不拉 libagg 符号。
2. **两种配置编译**：`g++ -std=c++11 -Wall -Wextra -c` 编译 `wgpu_draw.cpp` 与 `wgpu_osd_draw.cpp`，分别带 `-DENABLE_WGPU` 与不带 → 4/4 0 error；每个 obj 的 `agg::*` 未定义符号均为 **0**。（include 路径：`modules/osd/wgpu`、`modules/osd/agg`、`windows/agg/include`、`windows/agg/examples`、`windows/agg/font_freetype`、`3rdparty/wgpu`。）
3. **无 libagg 链接 + 运行**：`tools/osd_link_test/main.cpp`（已入库，可重跑）实例化 `WGPUOSDDrawTarget(256,384)`，经 `AggDrawTarget*` 安装并调用 `clear/fillColor/rectangle/renderText`、检查 `buf().buf()`，再调用 `pixelBuffer()`、矩形/圆角矩形/椭圆/线/弧/星形/曲线/三角形/`renderTextDropshadowed`、`hasContent()`、`pixelStrideBytes()`。命令行**不含任何 AGG 库**，在仓库根目录直接跑：

   ```
   g++ -std=c++11 -Wno-deprecated-register \
       -Idesmume/src/frontend/modules/osd/wgpu \
       -Idesmume/src/frontend/modules/osd/agg \
       -Idesmume/src/frontend/windows/agg/include \
       -Idesmume/src/frontend/windows/agg/examples \
       -Idesmume/src/frontend/windows/agg/font_freetype \
       tools/osd_link_test/main.cpp \
       desmume/src/frontend/modules/osd/wgpu/wgpu_osd_draw.cpp \
       desmume/src/frontend/modules/osd/wgpu/wgpu_draw.cpp \
       -o tools/osd_link_test/osd_link_test
   ```

   结果 `BUILD OK`、`exit=0`。若哪天录制器又拉回 `agg::*`，这一步会在链接期直接失败。
4. **接口完整性**：`aggdraw.h` 的 `AGG2D_USE_VECTORFONTS` 分支只有 2 个额外纯虚（`:414` 的 `setVectorFont`、`:436` 的 `renderVectorFontText`），新头用 `#ifdef` 条件覆盖。首次编译曾因 `cubicCurveTo/Rel` 实际是 6/4 参数重载而报「abstract class」，已修正。
5. `DeSmuME.vcxproj`、`DeSmuME.vcxproj.filters`、`desmume.props` 三个 XML 均解析通过。

### 20.5 结论与遗留

§2.1 的「去 libagg」目标在 Windows 默认构建与阶段 4 构建上都真正落地：`UseNativeOsd=false`（legacy AGG 后端）是唯一还需要 `agg-2.5*.lib` 的配置。§19 只是链接层的临时兜底，已被本节替代。§12.4 的「`wgpu_draw.cpp` 始终参与编译、持续被编译检查」保持不变（现在它更干净：纯渲染器）。§18.2 的对比表描述的是当时状态；WGPU 侧现在同样 AGG-free，区别只剩「Windows 版仍继承抽象接口以复用 `aggDraw.hud` 接线」。

**未验证**：Windows 完整 `msbuild` 全链接、GUI 实机（旋转 × 布局 × 滤镜）、WGPU 离屏像素 golden、POSIX 完整 meson 构建（本机无 freetype/fontconfig，`AGG2D_USE_VECTORFONTS` 分支只做了静态核对）。这些由用户在各自平台执行。本机也没有重跑 §12.5 / §13.5 / §17.3 的 WGPU 离屏对拍——旧 `WGPUOSDAggTarget` 已移除，对拍脚本需要改为实例化 `WGPUOSDDrawTarget`；渲染管线本身（`WGPUOSDRenderer` + WGSL）未改动，理论上结果不变，但仍建议实跑一次。


## 21. POSIX Lua 脚本支持：PR #969 现状与缺口（2026-11-11）

本节记录 `TASEmulators/desmume#969`（标题 “added lua support for ubuntu”）应用到当前工作树后的事实。所有改动均未提交。**GTK2 已决定淘汰**：相关代码只保留到工作树可编译为止，不再继续修，也不计入剩余工作量。

### 21.1 PR #969 提供了什么

- 状态：open、未合并；作者 `marko-kriskovic`；7 个提交；11 个文件，**+1,382 / −10**。
- 改动文件：

  | 文件 | 行数 | 内容 |
  | --- | --- | --- |
  | `posix/gtk/tools/luaScriptConsole.cpp/.h` | +521 / +33 | GTK3 脚本控制台 |
  | `posix/gtk2/tools/luaScriptConsole.cpp/.h` | +532 / +33 | GTK2 脚本控制台 |
  | `posix/gtk/main.cpp` | +37 / −1 | app action、菜单回调、`EmuLoop` 前后 Lua 回调 |
  | `posix/gtk/menu.ui` | +13 | Tools → Lua Scripts → Add new script |
  | `posix/gtk/meson.build` | +5 | 控制台源文件 + `--dynamic-list lua-exports.txt` |
  | `posix/gtk2/main.cpp` | +43 | GTK2 菜单/回调 |
  | `posix/gtk2/meson.build` | +5 | 控制台源文件 |
  | `posix/meson.build` | +12 / −9 | `dep_lua`（`lua5.1` / `lua-5.1`）、`HAVE_LUA`、`lua-engine.cpp` |
  | `posix/lua-exports.txt` | +148 | 导出 Lua C API 符号，供 `--dynamic-list` 使用 |

- 功能范围：
  - **每个脚本一个独立控制台窗口**：路径输入、Browse、Edit/Create/View、Run/Stop/Restart、stdout 开关、250,000 字符输出上限、拖放 `.lua` 文件。
  - **inotify 自动重载**：脚本文件保存后停止并重新运行。
  - **菜单入口**：GTK3 与 GTK2 的 Tools 菜单都能打开控制台。
  - **运行钩子**：GTK3 在 `desmume_cycle()` 前后调用 `LUACALL_BEFOREEMULATION` / `AFTEREMULATION` / `AFTEREMULATIONGUI`。
  - **构建**：Meson 探测系统 Lua 开发包并链接；没有 Lua 时整个 Lua 支持不编译。
- PR 自述限制：需要用户自行安装 Lua 5.1；只在 Ubuntu 上、只用一个脚本测过；没有验证 Windows 回归。

### 21.2 当前工作树相对 PR 改了什么

| 主题 | PR #969 | 当前实现 |
| --- | --- | --- |
| Lua 依赖 | 系统 `lua5.1`/`lua-5.1` 开发包，构建期链接 | 仓库内 `frontend/windows/lua/include` 头文件；运行期 `dlopen`，**优先 LuaJIT，回退 Lua 5.1**；不链接 Lua |
| 符号导出 | `lua-exports.txt` + `-Wl,--dynamic-list` | 删除；复用 `frontend/cocoa/LuaJITApiSymbols.h` 的函数指针表 |
| 构建系统 | 只改 Meson | Meson + autotools；`HAVE_LUA` 无条件定义，缺运行库时仍能启动并解释原因 |
| 缺失运行库 | 没有 Lua 就不编译/不可用 | 控制台照常打开，Run 时在输出里报告加载失败原因；About 显示库路径 |
| 引擎回调线程 | 回调里直接操作 GTK 控件 | 全部用高于 `EMULOOP_PRIO` 的 idle 回主线程（`G_PRIORITY_DEFAULT`），按 uid 重查控制台；窗口已销毁时安全丢弃 |
| 文件监视 | `volatile bool`；每次 start 都 join/重启线程 | `g_atomic_int` 标志 + `watched_path`；路径未变时不重启 |
| 路径状态 | 输入框清空后仍保留旧 `filename` | 清空/文件不存在时清除；Run 前重新读取并检查存在性 |
| 关闭窗口 | 运行中直接 destroy，`onstop` 里可能再次 destroy | `closeOnStop` 延迟到 onstop idle，避免回调重入和 use-after-free |
| About | 无 | 新增 “About Lua Scripting”，复用 Windows/macOS 的 `GetLuaEngineInfo()` + 新 `GetLuaEngineLibraryPath()` |
| M1 帧推进 | 无 | `GtkDriver::EMU_StepMainLoop()` + `EmuStepFrame()` + `LuaPumpEvents()`；见 21.3 |
| M1 UI 钩子 | 无 | `SetLuaPopupHandler()` / `SetLuaOpenScriptHandler()`，共享 `lua-engine.cpp` 不引入 GTK 依赖 |
| M2 输入 | 无 | `SetLuaInputStatusHandler()` / `SetLuaGetControllerStatusHandler()`；GTK3 实现键鼠 + SDL 手柄，见 21.7 |
| M3 菜单 | 无 | `LuaMenuHooks` + GTK3 `GMenu` 动态模型、逐项 action、section 分隔符、延迟点击；见 21.8 |
| GTK2 | 完整实现 | 同步修复，但**前端已决定淘汰**，不再投入 |

- 当前工作树相对上游：16 个已跟踪文件 **+1,997 / −137**（不含本节文档），另有 4 个新控制台文件共 **1,485 行**（GTK3 760、GTK2 725）。GTK2 退役后，有效新增约为 **760 行控制台 + 1,995 行接线/构建/驱动/输入/菜单**。
- 已验证：Meson GTK3 与 GTK2 构建成功（M1/M2/M3 后再次验证）；`autoreconf -fi` 与 out-of-tree `configure` 成功；运行期加载器实际加载 `/usr/lib64/libluajit-5.1.so.2`；强制覆盖路径与加载失败路径均按预期报错；GTK 产物不链接 Lua，只链接 `libdl`。GTK3 已在真实 X11 窗口里用 Castlevania: Dawn of Sorrow (USA) ROM 跑通 M1：`loop.lua`（`while true do emu.frameadvance() end`）能启动，Stop 能中止并显示 `terminated by user`，Browse/Stop/Run 状态正确复位；`emu.openscript` 的自动启动路径也能真正执行脚本；`osd-shapes.lua` 的 `agg.*` 图形已能显示并逐帧动画（见 21.6）。M2 也已实测：模拟 `W` 键后 `input.get()` 返回 `{W=true}`；指针映射函数对上半屏中心返回 `(128,-96)`、下半屏中心返回 `(128,96)`、屏外返回 false；SDL 虚拟手柄验证了按钮、轴和对角 hat 语义（见 21.7）。M3 已验证：`emu.addmenu()` 能建立顶层菜单/嵌套子菜单/分隔符，`setmenuiteminfo()` 能改名、启用/禁用、首次显式 `checked` 后转为复选菜单项，`registermenustart()` 按 250 ms 定时触发，菜单点击会延迟到帧边界执行；初始化完成后菜单模型不再重建（见 21.8）。
- 未验证：完整 autotools 构建被 `desmume/src/types.h` 的 `std::memory_order_relaxed`（缺少 `c++14` 标志）挡住，这是既有问题；Windows/macOS 未做运行期回归；GTK2 已退役，未做运行期回归。

### 21.3 PR 没提供、POSIX 仍缺什么

| 能力 | 当前 POSIX 行为 | 影响 | 估算 |
| --- | --- | --- | --- |
| `emu.frameadvance()` / `emu.emulateframe*()` / `emu.wait()` / `emu.pause()` | **已实现**：`GtkDriver::EMU_StepMainLoop()` 返回 `ESTEP_DONE` / `ESTEP_CALL_AGAIN`，遵守 `allowSleep` / `allowPause` / `frameSkip` / `disableUser` / `disableCore` | 常规帧推进脚本可以运行 | 完成 |
| `emu.addmenu()` / `emu.setmenuiteminfo()` / `emu.registermenustart()` | **已实现**：共享引擎通过 `LuaMenuHooks` 调用 GTK3；GTK3 维护自己的菜单树并渲染成 `GMenu` 模型。每个叶子项对应一个 `app.lua<ID>` action，分隔符用 section 边界表示，`registermenustart` 由 250 ms 定时器和帧边界触发，点击排队到帧边界执行 | 脚本可以创建/修改菜单并响应点击 | 完成 |
| `emu.openscript()` | **已实现**：新建控制台并把脚本排到下一个 idle 启动；相对路径先按调用脚本目录解析；文件不存在返回可捕获的 Lua 错误 | 子脚本可以启动，且 `pcall(emu.openscript, ...)` 会先返回 | 完成 |
| `input.get()` | **已实现**：GTK3 跟踪前台键盘事件（GDK keyval → Windows `s_keyToName` 词汇），鼠标按钮取自 `gdk_window_get_device_position()` 的 `GdkModifierType`，坐标用现有 cairo 矩阵换算成上屏负 y / 下屏 0..191；无钩子的前端仍返回空表 | 键鼠脚本可读真实输入 | 完成 |
| `controller.get()` | **已实现**：复用 `open_joysticks[]`，经 `get_joystick_raw_state()` 读取 hat/按钮/轴；对角 hat 默认不返回，`returnDiagonals=true` 时返回；无手柄返回空表 | 手柄脚本可读真实输入 | 完成 |
| `gui.popup()` / `input.popup()` | **已实现**：GTK 模态对话框，按钮顺序和返回字符串与 Windows `MessageBox` 一致（`ok` / `yes` / `no` / `cancel` / `abort` / `retry` / `ignore`） | 用户可以选择，脚本按选择分支 | 完成 |
| About Lua Scripting | 已实现 | — | 完成 |
| 脚本控制台（Run/Stop/Edit/监视/拖放/stdout） | 已实现 | — | 完成 |

M1 帧推进的实现要点：

1. `EMU_StepMainLoop()` 在每次调用时先摘掉 `regMainLoop`，用 `gtk_main_iteration_do(FALSE)` 泵事件，执行一帧后再挂回去。这样脚本的 `while true do emu.frameadvance() end` 不会和正常的 `EmuLoop` 叠加成双倍帧，脚本结束后普通模拟立即恢复。
2. 暂停时（`!desmume_running() && allowPause`）泵事件并 `usleep(10ms)`，返回 `ESTEP_CALL_AGAIN`，与 Windows `StepRunLoop_Paused()` 的契约一致；Run/Stop/关闭窗口仍然可点。
3. 正常速度下用绝对 16.667 ms 截止时间限速，落后超过 100 ms 时重新锚定；等待期间继续泵事件。快进/不睡眠模式只泵一次事件。
4. `EMU_IsAtFrameBoundary()` 在 `desmume_cycle()` 期间为 false，防止在模拟回调里递归推进帧。
5. Stop 仍走引擎已有的跨线程请求机制：UI 线程只置 `stopRequested` / `closeRequested` 并 `RaiseAbortRequest`，真正的销毁在引擎线程处理，事件泵不会在脚本执行中直接释放 `lua_State`。
6. 控制台的 idle 回调必须显式使用高于 `EMULOOP_PRIO` 的优先级（当前用 `G_PRIORITY_DEFAULT`）。`EmuLoop` 是常驻就绪的 idle source，默认 `G_PRIORITY_DEFAULT_IDLE`（200）的回调在模拟运行时永远轮不到；这会让 Stop 后 Browse 仍禁用、`script stopped.` 不显示，也会让 `emu.openscript` 打开的脚本永远不启动。已在 GTK3 控制台的全部 idle 队列上修正。

### 21.4 改动量评估

| 里程碑 | 内容 | 代码量 | 人日 |
| --- | --- | --- | --- |
| M1 可用（**已完成**） | `EMU_StepMainLoop` + 事件泵 + 原子停止标志；popup 对话框；`emu.openscript` | 已完成 | 已完成 |
| M2 输入（**已完成**） | `input.get` / `controller.get`（GTK 键鼠 + SDL 手柄状态） | 已完成 | 已完成 |
| M3 菜单（**已完成**） | `emu.addmenu` / `setmenuiteminfo` / `registermenustart`（GMenu 动态模型） | 已完成 | 已完成 |
| 测试/文档 | 手动脚本矩阵、失败路径、Windows/macOS 回归 | — | 1–2 |
| **合计（M3 之后）** | 功能已齐，剩跨平台回归与文档 | **0 行功能代码** | **1–2** |

- M1/M2/M3 完成后，`api-smoke.lua` 的帧推进、弹窗、`emu.openscript`、`input.get()`、`controller.get()`、`emu.addmenu()` / `setmenuiteminfo()` / `registermenustart()` 路径都已通了。
- M3 的实际改动约 **800 行**（`lua-engine.h/.cpp` 的通用 POSIX 分支 + `gtk/main.cpp` 的菜单树/模型/action/定时器），落在原估 200–400 行之上，主要多出的是 action 生命周期、动态复选状态、延迟点击和避免模型重建的细节。
- 已投入的 PR + 当前修正约 **3,500 行新增**（含 1,485 行控制台），所以剩余工作只剩验证与文档。

### 21.5 结论

PR #969 提供的是一个**能打开控制台、能运行不依赖帧推进的简单脚本**的 Linux 原型；它没有实现 POSIX 的帧推进，也没有菜单、输入、弹窗和 About。当前工作树已经把构建、运行期加载、线程安全、About、autotools 和 **M1 帧推进/弹窗/子脚本**、**M2 键鼠/手柄输入**、**M3 脚本菜单** 补齐。功能层面已无阻断项；剩余是跨平台运行期回归（Windows/macOS 未做）和测试/文档，GTK2 不计入。M1 已在真实 GTK3 窗口中验证（帧推进、Stop、`emu.openscript`）；M2 的键盘、鼠标坐标和手柄读取也已在真实窗口中验证（见 21.7）；M3 的菜单创建/更新/定时回调/点击已在真实窗口中验证（见 21.8）；控制台 idle 优先级导致的 Stop 后状态不刷新问题已修复。

### 21.6 GTK3 OSD 合成顺序修复（2026-11-11）

**现象**：`documentation/lua-api/examples/osd-shapes.lua` 能运行（`AnyLuaActive()==true`），但 `agg.*` 画的图形完全看不见。

**根因**：GTK3 的显示链路是「GPU 输出 → `video->GetSrcBufferPtr()` → `RunFilter()` 拷贝到 `video->GetDstBufferPtr()` → 显示读 dst」。`agg.*` 通过 `aggDraw.hud` 画进 src，但旧代码在 `Gtk3GPUEventHandler::DidFrameEnd()` 里、也就是 `desmume_cycle()` 内部就调用了 `RedrawScreen()`，顺序是：

```
DidFrameEnd: 转换 GPU 输出到 src → 画内置 HUD → RunFilter(src→dst) → queue_draw
回到 EmuLoop/EmuStepFrame: CallRegisteredLuaFunctions(LUACALL_AFTEREMULATIONGUI)  // agg.* 此时才画进 src
```

Lua 回调执行时 src 已经过滤完毕，之后也没有第二次 `RunFilter()`，所以像素只留在 src，永远到不了显示读取的 dst。gdb 实测：`src[60*256+20] == 0xFF4078E8`（脚本的蓝色矩形），而 `dst[60*256+20] == 0xFF180029`（游戏原像素），且 `aggDraw.hud->buf().buf() == video->GetSrcBufferPtr()`。

**修复**（`posix/gtk/main.cpp`）：把原来一体的 `RedrawScreen()` 拆成准备和呈现两段，并按 Windows 的层序排列。

1. `PrepareHudTarget()`：帧转换进 src 后立即调用。负责 OSD 缩放/字体更新，并**每帧**把 `aggDraw.hud` 重新 attach 到 `video->GetSrcBufferPtr()`——即使所有内置 HUD 选项都关闭也要 attach，因为 Lua 无条件通过 `aggDraw.hud` 绘图。此段不画内置 HUD，也不跑滤镜。
2. `DidFrameEnd()` 只做「转换 → `PrepareHudTarget()` → `sFramePresentPending = true`」，不再直接呈现。
3. `PresentFrame()`：在 `LUACALL_AFTEREMULATIONGUI` 之后调用（`EmuStepFrame()` 和 `EmuLoop()` 各一处）。顺序为「内置 HUD（`osd->update()` / `DrawHUD()` / `osd->clear()`）→ `video->RunFilter()` → `gtk_widget_queue_draw()`」，即 Lua 先画、内置 HUD 后覆盖，与 Windows `display.cpp` 一致。
4. `RedrawScreen()` 保留给非帧路径（菜单、缩放、暂停等）：先 `PrepareHudTarget()`，再强制 `sFramePresentPending = true` 并立即 `PresentFrame()`，行为与旧版一致。

**验证**（真实 GTK3 窗口 + Castlevania ROM，脚本经控制台自动启动）：

- `aggDraw.hud->buf().buf() == video->GetSrcBufferPtr()`，`src[60*256+20] == dst[60*256+20] == 0xFF4078E8`（蓝色矩形）；paint-state 控件的填充矩形同样出现在 dst。
- 间隔 1 秒抓取两次 dst：动画绿色条从 x≈144–158 移到 x≈61–75，约 46,935 个像素发生变化，证明脚本不是只呈现一帧。
- 未启用任何内置 HUD 时也走同一条路径（`aggDraw.hud` 始终指向 src）。
- 合成层序现在是：NDS 画面 → Lua `agg.*`/`gui.*` → 内置 FPS/输入 HUD → 视频滤镜 → 显示。

### 21.7 GTK3 `input.get()` / `controller.get()` 实现与验证（2026-11-11）

**共享接口**（`lua-engine.h` / `lua-engine.cpp`）：新增两个纯 C POD 结构和函数指针钩子，共享引擎仍然不包含 GTK/SDL 头文件。

- `LuaInputStatus`：`keyNames[64][16]`、`keyCount`、`mouseValid`、`mouseX/Y`；`LuaGetInputStatusHandler` 返回 false 时 `input.get()` 输出空表。
- `LuaControllerStatus`：`dpad[4]`（up/right/down/left）、`buttonCount`、`buttons[33]`、`axisCount`、`axes[6]`（x/y/z/r/u/v，−1..1）；`LuaGetControllerStatusHandler` 返回 false 表示该手柄不存在，引擎输出空表。
- `SetLuaInputStatusHandler()` / `SetLuaGetControllerStatusHandler()` 由前端在启动时安装。POSIX 的 `#else` 分支在无钩子时保持旧行为（空表），CLI 与已退役的 GTK2 不受影响。

**键盘**（`gtk/main.cpp`）：`std::set<guint> sLuaPressedKeys` 在 `Key_Press` / `Key_Release` 记录**所有** GDK keyval，而不只是绑定到 DS 按键的那些；`GtkLua_KeyName()` 映射到 Windows `s_keyToName[]` 词汇（A–Z、0–9、F1–F24、numpad、方向键、符号键、鼠标键）。`focus-out-event` 清空集合，避免失焦后收不到 release 而把按键永远当成按住。语义对应 Windows 非 `allowBackgroundInput` 路径：只报告前台窗口收到的按键。

**鼠标**：`gdk_window_get_device_position()` 必须传入真实 `GdkDevice`（本 GTK 构建对 NULL 会触发 `gdk_window_get_device_position_double: assertion 'GDK_IS_DEVICE (device)' failed`），因此通过 `gdk_display_get_default_seat()` + `gdk_seat_get_pointer()` 取指针设备。按钮取自 `GdkModifierType` 的 `GDK_BUTTON1/2/3_MASK`，名称与键盘共用命名空间（`leftclick` / `rightclick` / `middleclick`）。坐标用显示路径已经维护的 `topscreen_matrix` / `touch_matrix`（以及 hybrid 变体）换算：上屏 y 为负、下屏 0..191。与 macOS 一致，只有指针位于任一屏幕内才输出 `xmouse`/`ymouse`（Windows 会无条件输出，越界值也可能出现）。

**手柄**（`posix/shared/ctrlssdl.cpp/.h`）：新增 `joystick_raw_state` 与 `get_joystick_raw_state()`，直接复用已打开的 `open_joysticks[]`，避免第二次 `SDL_JoystickOpen`；用 `SDL_JoystickGetHat/GetButton/GetAxis` 读取状态，轴按 −32768..32767 → −1..1。`GtkLua_GetControllerStatus()` 把 SDL hat 位映射到四个方向；对角 hat 在 `returnDiagonals=false` 时全部为 false，`true` 时返回两个方向，与 Windows `JOY_RETURNPOV` 语义一致。无手柄或索引越界返回 false，引擎输出空表。

**验证**（真实 GTK3 窗口 + Castlevania ROM，脚本经控制台启动，结果写入文件）：

- 键盘：`gdk_test_simulate_key()` 注入 `W` 按下 → `input.get()` 返回 `{W=true}`；释放后 `sLuaPressedKeys` 为空。
- 鼠标坐标：直接调用 `GtkLua_PointerToScreen()`：`(154,96) → (128,-96)`、`(154,288) → (128,96)`、`(5,5) → false`；真实指针位于窗口内时 `input.get()` 返回 `xmouse=51, ymouse=-48`，修复设备参数后不再出现 `Gdk-CRITICAL`。
- 鼠标按钮：在 `GtkLua_GetInputStatus()` 的 mask 判定处断点注入 `GDK_BUTTON1_MASK`，脚本输出 `{leftclick=true}`。
- 手柄：`SDL_JoystickAttachVirtual()` 创建 6 轴/15 键/1 hat 虚拟手柄，按下 button 1、x=16384、hat=up|right：`controller.get()` 返回 `1=true`、`x≈0.500015`、dpad 全 false；`controller.get(0,true)` 返回 `up=true,right=true`。Lua 端到端输出一致。
- 无手柄：`controller.get()` / `controller.get(0,true)` 返回 `{}`，不报错。

**范围/限制**：

- GTK2 与 CLI 未安装钩子，保持空表（GTK2 已退役）；Windows/macOS 代码未改动，也未做运行期回归。
- 本会话是 Wayland/COSMIC + XWayland，合成器不响应 `XWarpPointer` / `gdk_device_warp`，所以鼠标按钮改用断点注入 mask 验证；坐标映射本身通过直接函数调用确定性验证。
- M2 实际改动约 **400 行**（原估 100–200 行），多出的部分主要是 GDK→Windows 键名映射和旋转/交换/hybrid 坐标换算。

### 21.8 GTK3 `emu.addmenu()` / `setmenuiteminfo()` / `registermenustart()` 实现与验证（2026-11-11）

**共享接口**（`lua-engine.h` / `lua-engine.cpp`）：新增纯 C 的 `LuaMenuHooks` 函数表与 `SetLuaMenuHooks()`；共享引擎仍然不包含 GTK 头文件。

- 钩子覆盖：分配/释放菜单项 ID、判断 ID 归属、取根菜单、查找子菜单、添加子菜单/普通项/分隔符、删除项、判断空菜单、设置启用/复选/标题、请求一次 `registermenustart` 刷新。
- POSIX 的 `#else` 分支不再走空实现：`IsLuaMenuItem()`、`SearchFreeMenuItem()`、`AddSubMenu()`、`AddMenuEntries()`、`emu_addmenu()`、`emu_setmenuiteminfo()`、`emu_registermenustart()` 以及 `StopLuaScript()` 的清理都改为通过钩子调用。CLI 与已退役的 GTK2 不安装钩子，行为保持为空操作。
- `emu.setmenuiteminfo()` 的 POSIX 分支与 Win32 契约一致：按回调函数找到菜单项，只应用 `enabled` / `checked` / `name` 中实际给出的字段。

**GTK3 实现**（`gtk/main.cpp`）：

- `GtkLuaMenu` / `GtkLuaMenuEntry` / `GtkLuaAction` 维护一棵独立于静态 `menu.ui` 的脚本菜单树；顶层菜单追加到 `GtkApplication` 的 app menubar，脚本停止时移除。
- 每个叶子项对应一个 `app.lua<ID>` action，ID 池为 40000–40511。`GtkLua_EnsureAction()` 创建/复用 action，`GtkLua_SetEnabled()` / `SetChecked()` / `SetTitle()` 就地更新。
- `GMenu` 没有独立的分隔符项：分隔符在重建模型时转成 section 边界，`GtkMenuTracker` 会把 section 边界画成菜单分隔线。
- 标题里的 `_` 转义成 `__`，避免 GTK 把 Lua 标题当作助记符。
- 菜单点击不直接调用 Lua：`GtkLua_MenuItemActivated()` 只把 item ID 放进 `sLuaPendingMenuClicks`，由 `GtkLua_DrainMenuCalls()` 在帧边界（`LUACALL_AFTEREMULATION` 之后）统一执行，避免在 Lua 已运行时重入 `lua_State`。
- `registermenustart` 由 250 ms 定时器置 pending，再在帧边界调用 `LUACALL_ONINITMENU`；没有 Lua 在运行时定时器会立即 drain。定时器**不重建模型**。

**两处实测暴露并修复的问题**：

1. **hover 闪烁 / 子菜单一开就关**：最初每次 `LUACALL_ONINITMENU` 后都无条件 `GtkLua_RebuildAll()`。菜单打开时清空并重填 `GMenu` 模型会重置 hover 状态并关闭已展开的子菜单，表现为约 4 Hz 闪烁。修复：`ONINITMENU` 只让 `setmenuiteminfo()` 就地更新 action，不重建模型；只有增删项/改标题才重建。
2. **所有菜单项都显示复选框**：最初每个 action 都是 stateful boolean，`GtkMenuTracker` 因此把所有项都画成 check item。修复：普通项先用无状态 action；某菜单项第一次收到显式的 `checked` 字段（true 或 false）时，才替换成 stateful boolean action。复选性是粘性的（之后取消勾选仍保留空复选框）。以首次显式 `checked`（而不是首次 `true`）作为切换点，可以让 `api-smoke.lua` 在脚本启动后的第一次 `registermenustart` 里就把四个图层项全部转成复选项，之后菜单打开期间不再重建模型。

**验证**（真实 GTK3 窗口 + Castlevania ROM，脚本经控制台启动，结果写文件；因合成器不提供截图协议，用 gdb 在定时器/action 层取证）：

- `emu.addmenu("LuaTest", {...})` 成功；嵌套 `Sub → Nested`、`nil` 分隔符、禁用项都建立成功；`emu.setmenuiteminfo()` 的改名/`checked=true` 生效；`registermenustart` 连续触发 3 次并写入日志。
- 初始化后的 ID 与 action 状态：40000 = 顶层子菜单（无 action）、40001 = 复选项（stateful `b`，state=`true`）、40002 = 子菜单（无 action）、40003 = 嵌套普通项（无状态 action）、40004 = 分隔符（无 action）。普通项不再带复选框。
- 初始化完成后连续 10 个 250 ms tick，`GtkLua_RebuildAll()` 调用次数为 **0**，因此定时刷新不会造成 hover 闪烁或子菜单关闭。
- 用用户提供的 `documentation/lua-api/examples/api-smoke.lua`（无限 `frameadvance` 循环 + 每 1.5 s 切换图层预设）复测：普通项 40001 保持无状态 action；`Layer masks` 下的 40006 在第一次 `registermenustart` 后即为 stateful boolean（即使当前 `state=false` 也仍是复选项）；初始化完成后连续 tick 的模型重建次数为 **0**。
- 通过 `g_action_activate()` 触发 40001 / 40003，脚本分别记录 `clicked Item 1 checked=true` 和 `clicked Nested`，证明点击排队与 `CallRegisteredLuaMenuHandlers()` 路径可用。
- 关闭主窗口时 gdb 的 `run` 正常返回，进程退出；当时脚本控制台仍然打开（见 21.9）。

**与 Windows/macOS 的刻意偏差**：

- Windows 在 `WM_INITMENUPOPUP` 时调用 `CallRegisteredLuaMenuHandlers(LUACALL_ONINITMENU)`，macOS 在菜单即将打开时排队；`GMenuModel` 没有“即将显示”信号，所以 GTK3 用 **250 ms 定时器 + 帧边界**近似。`registermenustart` 会在菜单未打开时也执行，频率约 4 Hz，而不是每次弹出执行一次。这是 GTK3 的已知行为差异。
- `GMenu` 没有分隔符项，只能靠 section 边界表达；外观一致，但模型结构不同。

**范围/限制**：

- GTK2 与 CLI 未安装菜单钩子，`emu.addmenu()` 保持空操作（GTK2 已退役）；Windows/macOS 代码未改动，也未做运行期回归。
- 本环境无法截图，复选框外观和 hover 效果是通过 action 的 statefulness/state 间接验证的；`GtkMenuTracker` 正是根据 action 的 state 类型决定是否画 check item。
- `setmenuiteminfo(name=...)` 改标题时仍然必须重建 `GMenu` 模型（`GMenu` 没有就地改 label 的 API）；相同标题会跳过重建，但如果脚本在每次 `registermenustart` 里都改标题，仍可能出现与旧版相同的 hover/子菜单重置。`api-smoke.lua` 只改 `checked`/`enabled`，不会触发重建。
- M3 实际改动约 **800 行**（原估 200–400 行）。

### 21.9 GTK3 关闭主窗口 / 退出修复（2026-11-11）

**现象**：关闭主窗口后 Lua 控制台不会跟着关闭，进程也不退出；必须把所有控制台窗口逐个关闭进程才结束。

**根因**：控制台是独立的 `GtkWindow` toplevel，不是 `GtkApplicationWindow`，因此不受主窗口销毁影响；主窗口原来的 `destroy` 处理器只停止模拟循环，没有关闭控制台，也没有显式结束 `GtkApplication`。

**修复**（`gtk/main.cpp`）：

- 新增 `QuitApplication()`：`emu_halt()`、摘掉 `regMainLoop`、`lua_script_close_all()`，然后 `g_application_quit()`。
- File → Quit 的 `DoQuit()` 与主窗口 `destroy` 的 `OnMainWindowDestroy()` 都走 `QuitApplication()`；用 `sQuitRequested` 防止两者互相递归。
- 主窗口的 `destroy` 信号改接到 `OnMainWindowDestroy()`（原来误接 `DoQuit` 的 action 签名）。

**验证**：打开 Lua 控制台并运行脚本后，通过 gdb 调用 `gtk_window_close(pWindow)`；`gdb run` 正常返回，进程退出，控制台未单独关闭。
