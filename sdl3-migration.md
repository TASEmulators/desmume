# DeSmuME SDL2 → SDL3 迁移计划

> 状态：**已实施，已提交**（分两批：`d3bf5ab4` 源码/构建/CI/文档 + `effecdde` Windows SDL3 SDK，2026-11-11）
> 日期：2026-11-11
> 范围：全仓统一 SDL3

## 1. 已确认的决策

| 项 | 决定 |
|---|---|
| 升级范围 | **全仓统一 SDL3**。共享源码不做 SDL2/SDL3 双版本条件编译，也不拆 Windows 副本 |
| 兼容层 | **不用** `sdl2-compat`，也**不启用** `SDL_ENABLE_OLD_NAMES`。全部改成 SDL3 原生 API |
| SDK 接入 | Windows：内嵌（沿用现有做法）；Linux/macOS：系统库（`pkg-config sdl3`） |
| SDK 来源 | `C:\Users\zhang\SDL`（SDL3 源码仓库）。头文件直接取用，lib/dll 由源码编译，**不作为阻塞项** |
| SDL1 残留 | `draw_sdl_window.cpp` 的 `INCLUDE_OPENGL_2D` 分支一并重写为 SDL3 原生窗口 + GL context |
| 依赖安装 | 不自行下载/安装任何工具链或二进制 |

## 2. 现状基线（实测）

### 2.1 依赖体量

| 项 | 数据 |
|---|---|
| vendored SDK | `frontend/interface/windows/SDL/` — **97 个跟踪文件 / 6,912,996 B (6.59 MB)** |
| 版本 | `SDL_version.h:60-62` = **SDL 2.0.14** |
| 头文件 | `include/` 下 **89 个** |
| 二进制 | x64 / x86 各 `SDL2.dll`、`SDL2.lib`、`SDL2main.lib`、`SDL2test.lib`；DLL x64 1,561,088 B、x86 1,303,552 B |
| 链接配置 | `interface/windows/desmume.props:94`（`.\\SDL\\include`）、`:169/:173`（`.\\SDL\\lib\\x64|x86`）、`:180`（`SDL2main.lib;SDL2.lib`） |
| CI | `.github/workflows/build_interface.yml:62-87` Windows job 用 MSBuild 构建 `DeSmuME_Interface.vcxproj`（x64 + x86 矩阵） |

### 2.2 实际使用面

Windows interface 工程（`vcxproj` 的 ClCompile）只编译 4 个 SDL 消费单元：

| 文件 | 行数 | 含 SDL 的行 | distinct SDL 符号 |
|---|---:|---:|---:|
| `posix/shared/ctrlssdl.cpp` | 982 | 120 | 67 |
| `interface/draw_sdl_window.cpp` | 322 | 41 | 35 |
| `posix/shared/sndsdl.cpp` | 239 | 25 | 11 |
| `interface/interface.cpp` | 690 | 4 | 6 |

加上 `ctrlssdl.h` / `sndsdl.h`，Windows 侧合计 **251 次符号出现、108 个不同符号、196 行涉及 SDL**。
全仓非 vendored 源码：**162 个 `SDL_*` 符号 + 29 个 `SDLK_*` / `AUDIO_*` 常量**。

### 2.3 共享源码的牵连范围（为什么必须全仓升级）

同一份 `ctrlssdl.cpp` / `sndsdl.cpp` / `mic_sdl.cpp` / `sdl_3Demu.cpp` 被 8 处构建引用：

```
posix/meson.build:317            posix/Makefile.am:241
posix/cli/meson.build:7,8        posix/cli/Makefile.am:7
posix/gtk/meson.build:16,17,40   interface/meson.build:142,143,208
windows/DeSmuME_Interface.vcxproj:298,299,302
posix/codeblocks/desmume.cbp:1633,1659,1660,1672
```

### 2.4 可用环境

| 项 | 数据 |
|---|---|
| WSL | **SDL3 3.4.18**，`/usr/include/SDL3/SDL.h`、`/usr/lib/pkgconfig/sdl3.pc`、`-lSDL3`、`SDL_oldnames.h`（1339 行，默认不生效） |
| `sdl3-config` | 不存在 → autotools 必须改用 `PKG_CHECK_MODULES(SDL, sdl3)` |
| 本地源码 | `C:\Users\zhang\SDL`，commit `e7a4fa6`，`include/SDL3/` **88 个头文件**，无预编译产物 |
| 官方最新 | SDL3 3.4.20（2026-11-03） |

## 3. 迁移原则

1. 所有 `#include <SDL.h>` / `<SDL_xxx.h>` → `#include <SDL3/SDL.h>`。
2. 使用 SDL3 原生函数名/枚举，不留 SDL2 别名，不定义 `SDL_ENABLE_OLD_NAMES`。
3. 返回值 bool 化的函数统一用 `if (!SDL_Xxx())`，不再写 `!= 0` / `< 0`。
4. 音频从 SDL2 的 callback + 设备队列模型改为 **SDL3 AudioStream 模型**；SDL3 自动完成设备侧格式转换，因此可删掉旧的"strict open → allowed changes 重试 → 手动 AudioStream 转换"回退逻辑。
5. 逐文件改完立即用 WSL 的 SDL3 3.4.18 做 `-fsyntax-only`，最后全量 `meson compile`。

## 4. API 迁移对照

### 4.1 需改名（57 个，按 SDL3 `SDL_oldnames.h` 实测）

```
AudioStreamAvailable → SDL_GetAudioStreamAvailable     FreeSurface → SDL_DestroySurface
AudioStreamClear     → SDL_ClearAudioStream             GameController → SDL_Gamepad
AudioStreamGet       → SDL_GetAudioStreamData           GameControllerClose → SDL_CloseGamepad
AudioStreamPut       → SDL_PutAudioStreamData           GameControllerGetJoystick → SDL_GetGamepadJoystick
FreeAudioStream      → SDL_DestroyAudioStream           GameControllerGetNumTouchpads → SDL_GetNumGamepadTouchpads
NewAudioStream       → SDL_CreateAudioStream            GameControllerHasSensor → SDL_GamepadHasSensor
GL_DeleteContext     → SDL_GL_DestroyContext            GameControllerOpen → SDL_OpenGamepad
INIT_GAMECONTROLLER  → SDL_INIT_GAMEPAD                 IsGameController → SDL_IsGamepad
GameControllerEventState → SDL_SetGamepadEventsEnabled(bool) / SDL_GamepadEventsEnabled()
JoystickClose        → SDL_CloseJoystick                JoystickGetAxis/Button/Hat → SDL_GetJoystickAxis/Button/Hat
JoystickInstanceID   → SDL_GetJoystickID               JoystickNumAxes/Balls/Buttons/Hats → SDL_GetNumJoystick*
JoystickOpen         → SDL_OpenJoystick                 JoystickUpdate → SDL_UpdateJoysticks
MasksToPixelFormatEnum → SDL_GetPixelFormatForMasks    RenderCopy → SDL_RenderTexture
SDL_TRUE             → true                             SDL_INIT_* 事件/窗口枚举见下
```

事件枚举（`SDL_EVENT_*`）：

```
SDL_KEYDOWN/KEYUP            → SDL_EVENT_KEY_DOWN / KEY_UP
SDL_MOUSEMOTION              → SDL_EVENT_MOUSE_MOTION
SDL_MOUSEBUTTONDOWN/UP       → SDL_EVENT_MOUSE_BUTTON_DOWN / UP
SDL_JOYAXISMOTION            → SDL_EVENT_JOYSTICK_AXIS_MOTION
SDL_JOYBUTTONDOWN/UP         → SDL_EVENT_JOYSTICK_BUTTON_DOWN / UP
SDL_JOYHATMOTION             → SDL_EVENT_JOYSTICK_HAT_MOTION
SDL_JOYDEVICEADDED/REMOVED   → SDL_EVENT_JOYSTICK_ADDED / REMOVED
SDL_CONTROLLERAXISMOTION     → SDL_EVENT_GAMEPAD_AXIS_MOTION
SDL_CONTROLLERBUTTONDOWN/UP  → SDL_EVENT_GAMEPAD_BUTTON_DOWN / UP
SDL_CONTROLLERDEVICEADDED/REMOVED → SDL_EVENT_GAMEPAD_ADDED / REMOVED
SDL_CONTROLLERTOUCHPADDOWN/UP/MOTION → SDL_EVENT_GAMEPAD_TOUCHPAD_*
SDL_QUIT                     → SDL_EVENT_QUIT
SDL_WINDOWEVENT_FOCUS_GAINED/LOST/RESIZED → SDL_EVENT_WINDOW_*
```

`SDL_WINDOWEVENT` 本身已移除：SDL3 把窗口事件拆成独立事件类型，原来 `switch (event.window.event)` 的分支要改成对 `event.type` 直接分派。

### 4.2 已移除（23 个）与替代方案

| 移除的符号 | 替代 |
|---|---|
| `SDL_OpenAudio` / `SDL_CloseAudio` | `SDL_OpenAudioDeviceStream()` / `SDL_DestroyAudioStream()` |
| `SDL_PauseAudio` | `SDL_PauseAudioStreamDevice()` / `SDL_ResumeAudioStreamDevice()` |
| `SDL_LockAudio` / `SDL_UnlockAudio` | `SDL_LockAudioStream()` / `SDL_UnlockAudioStream()`；SDL3 的 stream 本身线程安全，多数场景可直接去掉 |
| `SDL_QueueAudio` 系列 / `SDL_DequeueAudio` / `SDL_ClearQueuedAudio` | `SDL_PutAudioStreamData()` / `SDL_GetAudioStreamData()` / `SDL_ClearAudioStream()` |
| `SDL_AUDIO_ALLOW_*_CHANGE` | 删掉；`SDL_OpenAudioDeviceStream(devid, spec, NULL, NULL)` 的 `spec` 即应用侧格式，SDL3 内部自动转换设备侧 |
| `SDL_MIX_MAXVOLUME` | `SDL_SetAudioStreamGain(stream, gain)`，`gain` 为 `float`，`1.0f` = 原 128；对外接口仍保留 0..128 语义，内部换算 |
| `SDL_INIT_TIMER` | 删掉，SDL3 定时器子系统始终可用 |
| `SDL_JoystickEventState(SDL_QUERY/ENABLE/IGNORE)` | `SDL_JoystickEventsEnabled()` / `SDL_SetJoystickEventsEnabled(bool)` |
| `SDL_NumJoysticks` / `SDL_JoystickNameForIndex` / `SDL_GameControllerNameForIndex` | `SDL_GetJoysticks(&count)` / `SDL_GetJoystickNameForID(id)` / `SDL_GetGamepadNameForID(id)`（改为按 instance ID 取） |
| `SDL_CreateRGBSurfaceFrom` | `SDL_CreateSurfaceFrom()` |
| `SDL_SetVideoMode` / `SDL_OPENGL` / `SDL_RESIZABLE` / `SDL_SWSURFACE` / `SDL_GL_SwapBuffers` | `SDL_CreateWindow(title, w, h, SDL_WINDOW_OPENGL \| SDL_WINDOW_RESIZABLE)` + `SDL_GL_CreateContext()` + `SDL_GL_SwapWindow()` |
| `SDL_3DEMU_H`（本仓库自己的 include guard） | 保留，非 SDL API |

### 4.3 名字保留但签名/语义变化

| API | 变化 |
|---|---|
| `SDL_Init` / `SDL_InitSubSystem` | 返回 `bool`（true = 成功） |
| `SDL_PollEvent` / `SDL_WaitEvent` / `SDL_PushEvent` | 返回 `bool` |
| `SDL_CreateWindow` | `(title, x, y, w, h, flags)` → `(title, w, h, flags)`，**删掉 x/y** |
| `SDL_CreateRenderer` | `(window, index, flags)` → `(window, const char *name)`，`name = NULL` 取默认；`SDL_RENDERER_ACCELERATED` 废弃 |
| `SDL_SetWindowFullscreen` | 第二个参数是 `bool fullscreen` |
| `SDL_GetWindowSize` / `SDL_GetCurrentDisplayMode` | 返回 `bool`；DisplayMode 需要 `SDL_DisplayID` |
| `SDL_AudioSpec` | 字段只剩 `format / channels / freq`，顺序与 SDL2 不同；`samples / callback / userdata / size` 全部消失 |
| `SDL_MixAudio` | 第 5 参 `volume` 由 `int 0..128` 改为 `float 0..1` |
| `SDL_GetTicks` | 返回 `Uint64` |
| `SDL_zero` | 保留 |
| `SDL_GL_*` 上下文 API | 保留，`SDL_GL_DeleteContext` → `SDL_GL_DestroyContext` |

### 4.4 键码与音频格式常量（29 个）

- `SDLK_a/x/z/q/w/s/o/p/m` → **`SDLK_A/X/Z/Q/W/S/O/P/M`**（SDL3 字母键码改大写），其余 `SDLK_RETURN/RIGHT/LEFT/UP/DOWN/RSHIFT/LSHIFT/BACKSPACE/ESCAPE/F1..F10` 保持原名。
- `AUDIO_S8` → `SDL_AUDIO_S8`；`AUDIO_S16SYS` → `SDL_AUDIO_S16`（本机字节序别名）。

## 5. 逐文件改动清单

| 文件 | 改动 |
|---|---|
| `posix/shared/mic_sdl.cpp` | **已重写**：`SDL_OpenAudioDeviceStream(DEFAULT_RECORDING, &want, NULL, NULL)` + `SDL_GetAudioStreamData()`；删掉 strict-open 回退、手动转换 stream、scratch buffer；`SDL_Pause/ResumeAudioStreamDevice`、`SDL_ClearAudioStream`、`SDL_DestroyAudioStream` |
| `posix/shared/sndsdl.cpp` | 从 callback + `stereodata/soundoff` 预缓冲改成 `SDL_OpenAudioDeviceStream(DEFAULT_PLAYBACK, ...)` push 模型：`SDL_PutAudioStreamData()` 推数据、`SDL_GetAudioStreamQueued()` 计算 `SNDSDLGetAudioSpace()` 背压、`SDL_SetAudioStreamGain()` 实现音量、`SDL_Pause/ResumeAudioStreamDevice()` 实现静音；删除 `MixAudio` callback 与两个手工缓冲区。`_XBOX` 分支保留 |
| `posix/shared/ctrlssdl.cpp/.h` | 事件枚举全量改名；joystick/gamepad API 改名；`SDL_NumJoysticks`/`...NameForIndex` 改成按 instance ID 查询；键码大写；`SDL_WINDOWEVENT` 分支按新事件类型重写；`SDL_INIT_TIMER` 删除 |
| `posix/shared/sdl_3Demu.cpp` | `SDL_CreateWindow` 去 x/y、`SDL_CreateRenderer(window, NULL)`、`SDL_GL_*` 改名、`SDL_SetWindowFullscreen` bool 化、`SDL_GetCurrentDisplayMode` 新签名 |
| `posix/shared/gdksdl.cpp` | 无 SDL 调用，仅需核对 include；若引用 SDL2 类型则同步改名 |
| `posix/cli/main.cpp` | `SDL_SetVideoMode` 注释与 SDL1 flags 注释清理；`SDL_Init` 返回值、`SDL_CreateWindow`、`SDLK_*`、音量接口调用 |
| `posix/gtk/main.cpp` | 45 处 SDL 调用：事件处理、`SDL_GetTicks`、`SDL_Delay`、窗口尺寸、音量接口；`SNDSDLGet/SetAudioVolume` 保持 0..128 语义不变 |
| `interface/interface.cpp` | `SDL_Init` / `SDL_Quit` 返回值、include 路径 |
| `interface/draw_sdl_window.cpp` | **重写 SDL1 遗留分支**：`sdl_videoFlags`（SDL1 flags）改成 `SDL_WindowFlags`；`SDL_CreateWindow` 用 `SDL_WINDOW_OPENGL \| SDL_WINDOW_RESIZABLE`；补 `SDL_GL_CreateContext` + `SDL_GL_MakeCurrent`；`SDL_GL_SwapBuffers()` → `SDL_GL_SwapWindow(window)`；`SDL_CreateRGBSurfaceFrom` → `SDL_CreateSurfaceFrom`；`SDL_FreeSurface` → `SDL_DestroySurface`；`SDL_RenderCopy` → `SDL_RenderTexture`；`SDLK_x` 等键码大写；`SDL_CreateRenderer` 第二参数改 `NULL` |

## 6. 构建系统与 CI

| 文件 | 现状 → 目标 |
|---|---|
| `posix/meson.build:17` | `dependency('sdl2')` → `dependency('sdl3')`；注释里的 "SDL2" 同步 |
| `interface/meson.build:14` | 同上 |
| `posix/configure.ac:87-95` | `AC_PATH_PROGS(SDLCONFIG, [sdl2-config])` → `PKG_CHECK_MODULES(SDL, sdl3)`。`SDL_CFLAGS` / `SDL_LIBS` 变量名保持不变，因此 `Makefile.am`、`cli/Makefile.am` 的引用无需改动 |
| `posix/Makefile.am:3`、`cli/Makefile.am:4,8` | 仅注释文字 SDL2 → SDL3 |
| `posix/codeblocks/desmume.cbp` | `/usr/include/SDL2` → `/usr/include/SDL3`；`libSDL2.so` → `libSDL3.so`（5 处） |
| `.github/workflows/build.yml:17` | `libsdl2-dev` → `libsdl3-dev`（Ubuntu 24.04 无此包，见 §10） |
| `.github/workflows/build_interface.yml:16,40` | `libsdl2-dev` → `libsdl3-dev`；`brew install sdl2` → `brew install sdl3` |
| `.github/workflows/build.yml:27-55` MinGW job | 编译的是 `frontend/windows/`（DirectX 主前端，不用 SDL），**不受影响** |

## 7. Windows 内嵌 SDK

目录结构（替换现有 `interface/windows/SDL/`）：

```
interface/windows/SDL3/
├── include/SDL3/*.h          ← 与 DLL 同版本：本地仓库切到 release-3.4.20 tag 后，从 C:\Users\zhang\SDL\include\SDL3 同步（86 个头文件）
└── lib/
    ├── x64/   SDL3.dll  SDL3.lib
    ├── x86/   SDL3.dll  SDL3.lib
    └── arm64/ SDL3.dll  SDL3.lib   ← 官方 3.4.20 预编译运行时包；arm64 仅占位，构建矩阵目前只有 x64/x86（见下）
```

**导入库来源**：官方 3.4.20 发布包是**运行时包**（只含 `SDL3.dll`，无 `SDL3.lib`/头）。因此 `SDL3.lib` 用本机 VS 的 `dumpbin.exe /exports` + `lib.exe /def` **从对应的 `SDL3.dll` 直接生成**，与运行时 DLL 严格匹配，无需下载 devel 包或编译源码。头文件则与本地 `release-3.4.20` tag 对齐（之前误用了 3.5.0-dev 头，已重新同步）。

`desmume.props` 改动：

- `:94` `.\\SDL\\include` → `.\\SDL3\\include`
- `:169/:173` `.\\SDL\\lib\\x64|x86` → `.\\SDL3\\lib\\x64|x86`
- `:180` `SDL2main.lib;SDL2.lib` → `SDL3.lib`（SDL3 已无 SDL2main，改为 header-only 的 `SDL_main.h`；本工程产出的是 DLL，不需要入口包装）

`DeSmuME_Interface.vcxproj` 的源码单元清单不变。运行时只需一个 `SDL3.dll`。

旧 `interface/windows/SDL/` 目录（97 文件 / 6.59 MB）整体删除。

## 8. 验证方案

1. **逐文件语法检查**（WSL，SDL3 3.4.18）：
   `g++ -std=c++14 -fsyntax-only $(pkg-config --cflags sdl3) -I<src> -I<libretro-common/include> <file>`
2. **全量编译**：`/home/carter/desmume-build` reconfigure + `meson compile -j 4`，确认 GTK/CLI 通过、零 error。
3. **interface 目标**：另建 `interface/` 的 meson build dir，编译出 `libdesmume.so`。
4. **链接检查**：`ldd gtk/desmume`、`ldd libdesmume.so` 应只出现 `libSDL3.so`，不再有 `libSDL2`。
5. **残留检查**：全仓 grep `sdl2`、`AUDIO_S16SYS`、`SDLK_[a-z]`、`SDL_JoystickOpen`、`SDL_WINDOWEVENT`、`SDL_SetVideoMode` 等，确认只剩历史文档/ChangeLog 与第三方 vendored AGG 注释。
6. **MSVC 工程**：本机无 VS 命令行，做 XML 合法性 + 路径引用核对，不做实际编译（Windows CI 覆盖）。
7. **autotools**：`autoreconf -fi` + out-of-tree `configure --disable-dependency-tracking`（/mnt/c drvfs 上必须加该参数，是既有问题）。

## 9. 实施顺序

| 阶段 | 内容 | 验收点 |
|---|---|---|
| 0 | 现状盘点、符号差异基线 | 本文档 §2/§4 数据可复现 |
| 1 | `posix/shared/` 四个 SDL 后端（mic 已完成） | 逐文件 `-fsyntax-only` 通过 |
| 2 | CLI / GTK / interface 前端源码 + SDL1 渲染分支重写 | 逐文件 `-fsyntax-only` 通过 |
| 3 | 构建系统与 CI 切到 sdl3 | meson reconfigure 成功 |
| 4 | Windows 内嵌 SDK 头文件 + props/vcxproj | XML 合法、路径引用一致 |
| 5 | WSL 全量编译 + 链接/残留检查 | `BUILD_OK`，`ldd` 只有 libSDL3 |
| 6 | 工作日志；**不提交，等用户指令** | 工作树状态清晰 |

## 10. 风险与边界

- **CI 的 Ubuntu 24.04 没有 `libsdl3-dev`**：优先把 Linux job 的 runner 升到自带该包的镜像；若 runner 镜像仍不提供，则在 CI 里用 SDL3 官方源码 tarball + CMake 构建并通过 `PKG_CONFIG_PATH` 暴露，或改用 meson subproject。**实施阶段先验证 runner 可用性再定，不猜测。**
- **音频语义**：SDL2 的手工预缓冲改成 SDL3 stream 自带缓冲后，`SNDSDLGetAudioSpace()` 的背压阈值需要用 `SDL_GetAudioStreamQueued()` 重新标定，否则可能出现延迟变化；对外音量接口保持 0..128 不变，避免牵动 GTK 滑块。
- **事件结构**：`SDL_WINDOWEVENT` 拆分影响 `ctrlssdl.cpp` 与 `gtk/main.cpp` 的焦点/缩放逻辑，是本次最容易出错的地方，改完必须实机逻辑复核。
- **本机无法验证 MSVC 工程**：Windows 侧只做配置与 XML 核对，实际编译依赖 `build_interface.yml` 的 Windows job。
- **不改动**：`frontend/windows/`（DirectX 主前端）、cocoa、vendored AGG、ChangeLog / `wgpu-agg.md` 等历史文档。

## 11. 实施结果

阶段 0–5 全部完成，**改动尚未提交**。

- **源码**：10 个 SDL 消费文件全部迁到 SDL3 原生 API；`draw_sdl_window.cpp` 的 SDL 1.2 遗留分支重写为 SDL3 窗口 + 显式 GL context；`ctrlssdl.cpp` 的热插拔按 SDL3 的无符号 `SDL_JoystickID` 改成显式 `opened` 槽位。音频换成 `SDL_OpenAudioDeviceStream()` push/pull 模型，音量对外仍保持 0..128（`SNDSDL_MAX_VOLUME`）。
- **构建**：meson 两处 `dependency('sdl3')`；autotools 改 `PKG_CHECK_MODULES(SDL, [sdl3])`（`sdl3-config` 不存在）；CodeBlocks 5 处路径；README.LIN 同步。
- **CI**：查证 Ubuntu 24.04 无 `libsdl3-dev`（26.04 才有 3.4.2）、GitHub Actions `ubuntu-26.04` 已于 2026-09-17 GA，故两个 Linux job 升到 `ubuntu-26.04` + `libsdl3-dev`；macOS job 改 `brew install sdl3`；MinGW job 仍用 24.04（不依赖 SDL）。
- **Windows SDK**：`interface/windows/SDL3/include/SDL3/` 用官方 3.4.20 预编译运行时包 + 本地 `release-3.4.20` tag 对齐的头文件（86 个 `.h`）；`lib/{x64,x86,arm64}/` 放入官方 3.4.20 `SDL3.dll`，并用 VS 的 `dumpbin /exports` + `lib /def` 从各自 DLL **生成匹配的 `SDL3.lib` 导入库**（运行时包不含导入库，此法免下载 devel 包、免编译源码）。`SDL3.lib` 与 `SDL3.dll` 同版本严格匹配，链接不再有阻塞项。旧 `interface/windows/SDL/`（97 文件 / 6.59 MB）已删除。`desmume.props` 的 include/lib/链接项已改为 SDL3。arm64 二进制已就位但未接入构建矩阵（当前仅 x64/x86）。

**验证**：WSL `meson setup --reconfigure` 报 `sdl3 found: YES 3.4.18`；`-Dfrontend-cli=true` 全量编译 198 步 `BUILD_OK`、0 error；另建 interface build dir 编译 144 步产出 `libdesmume.so`，0 error；`gtk/desmume`、`cli/desmume-cli`、`libdesmume.so` 的 `ldd` 均只有 `libSDL3.so.0`，`build.ninja` 中 SDL2 引用为 0；`autoreconf -fi` + out-of-tree `configure --disable-dependency-tracking` 成功（`checking for sdl3... yes`，`SDL_LIBS = -lSDL3`）；CodeBlocks `.cbp` 与 MSVC `.props` XML 均合法。所有迁移后的源码还额外用 SDL3 3.5.0 头文件做了 `-fsyntax-only`，全部通过。

`git diff --check` 的告警均为 CRLF 文件的假阳性（`-c core.whitespace=cr-at-eol` 后为空）。

MSVC 工程未在本机实际编译（无 VS 命令行），Windows CI 需要 `lib/{x64,x86}/SDL3.lib` 就位后才能链接。
