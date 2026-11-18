# TODO — Windows 已有、macOS (Cocoa) / Linux (GTK3) 尚缺的功能

本文档以 **Windows 前端为基线**，逐项核对 macOS (Cocoa) 与 Linux (GTK3) 两个前端
缺什么，以及补上它大概要动哪里。GTK2 处于维护模式（只修 bug），不在范围内。

## 0. 口径与证据

- 基线：`src/frontend/windows/resources.rc` 的完整菜单树 + Windows 侧各窗口实现文件。
- macOS：`src/frontend/cocoa/translations/English.lproj/MainMenu.xib` 的菜单 + `userinterface/*.mm`
  的源码检索（该前端部分菜单项是运行时用 `installToolMenuItemWithTitle:` 安装的，
  只看 xib 会漏，例如 Memory Viewer / RAM Watch / RAM Search）。
- Linux：`src/frontend/posix/gtk/menu.ui` + `main.cpp` + `tools/*.cpp`。
- 判定"缺失"的标准是**源码里没有对应实现或入口**，不是"菜单里没有"。凡属平台原生差异
  （Windows 的字体/DPI、DirectDraw、AVI 容器）单列，并建议不移植。

状态标记：`☐` 未开始 · `◐` 部分具备 · `☑` 已完成（目前已满足的项只在矩阵里打勾，不再展开）

---

## 1. 能力矩阵

| 能力 | Windows | macOS | GTK3 | 备注 |
|---|---|---|---|---|
| ROM 载入 / 最近列表 | ☑ | ☑ | ☑ | |
| 卸载（Close/Unload ROM） | ☑ | ☑ | ☐ | GTK 只能换 ROM 或退出 |
| ROM Info 对话框 | ☑ | ☑ | ☐ | |
| 载入策略（全部读入 RAM / 从磁盘流式） | ☑ | ☐ | ☐ | |
| 存档导入 / 导出 | ☑ | ☑ | ☑ | |
| 存档类型选择 | ☑ | ☑ | ☑ | |
| ADVANsCEne 数据库（导入 + 自动判存档类型） | ☑ | ☑ | ☐ | |
| 即时存档（10 槽 + 另存/另读） | ☑ | ☑ | ☑ | |
| 金手指：列表 / 搜索 / 禁用 / 数据库 | ☑ | ☑ | ☑ | GTK 的数据库入口在窗口内，不在菜单 |
| 电影（.dsm）播放 / 录制 / 停止 | ☑ | ☑ | ☑ | |
| 电影：元数据、ROM 校验、暂停到某帧、改注释 | ☑ | ☐ | ☐ | |
| DSM 时间线编辑器（逐帧网格 / CSV 剪贴板） | ☑ | ☐ | ☐ | |
| 视频录制 | ☑ MP4/AVI/WAV | ☑ AVFoundation | ☑ FFmpeg | |
| 旧式 AVI / WAV / UserSPU WAV | ☑ | ☐ | ◐ 仅 H.264 / FLAC | 建议不追平，见 §5 |
| 截图：格式选择、目录、独立截图工具 | ☑ | ☑ | ◐ 固定 PNG 到 Pictures | |
| 反汇编器 | ☑ | ☐ | ☐ | |
| 内存视图（hex，可改值，断点） | ☑ | ☑ | ☐ | |
| IO 寄存器视图 | ☑ | ☐ | ☑ | |
| 调色板 / 图块 / 地图 / OAM / 矩阵 / 光照 / 声音状态 / NitroFS 视图 | ☑ | ☐ | ☐ | 8 个窗口，两端全空 |
| GPU 图层开关 | ☑ 含主/子 GPU | ☑ | ◐ 缺主/子 GPU | |
| RAM Watch / RAM Search | ☑ | ☑ | ☑ | 已共享 `shared/ram*_core` |
| GDB stub | ☑ 命令行 | ☑ 控制面板 | ☑ 命令行 | |
| Lua 脚本控制台 | ☑ | ☑ | ☑ | |
| HUD：FPS / 输入 / 图形输入 / 帧 / 延迟 / RTC / 麦克风 | ☑ | ◐ 无"图形输入" | ☑ | |
| HUD 字体、编辑模式 | ☑ 字体+编辑 | ◐ 设置窗 | ◐ 编辑/重置，无字体 | |
| 帧跳 0–9 + 限速 | ☑ | ☑ 档位不同 | ☑ | |
| Turbo（逐按钮连发） | ☑ | ☑ | ☑ | GTK3 已补，见 §3 |
| 帧步进 / 帧跳跃 | ☑ 热键 | ☑ 菜单 | ☑ 步进已补，帧跳跃仍缺 | |
| 后台输入 / 后台暂停 | ☑ | ☐ | ☐ | |
| 热键自定义 | ☑ 热键配置窗 | ☑ 输入映射 + 配置档 | ☐ 仅固定加速键 | |
| Slot 1 配置 / Slot 2 选择 | ☑ | ☑ | ☐ 只能改 config.cfg | |
| 固件设置（语言、MAC、子网等） | ☑ | ☑ | ◐ 仅语言 | |
| 麦克风设置（设备/电平） | ☑ | ☑ | ◐ 仅 Fake mic noise | |
| Wi-Fi 设置 | ☑ | ☑ | ☐ | |
| 3D 设置（含 Edge Marking/Fog/Line Hack 等） | ☑ | ☑ | ◐ 仅基础项 | |
| 音频设置 | ☑ 对话框 | ☑ 首选项 | ◐ 菜单项拼装 | |
| 路径设置（每类文件独立目录） | ☑ | ◐ | ☐ | |
| Always On Top / Lockdown | ☑ | ☐ | ☐ | |
| 屏幕比例 / 垂直居中 / 间距颜色 / 拖动改间距 | ☑ | ◐ 只有间距百分比 | ◐ 只有 px 间距 | |
| 全屏选项（强制窗口化、显示菜单、自动隐藏光标） | ☑ | ◐ | ◐ | |
| 整数倍缩放 / 保持宽高比 | ☑ | ◐ | ☐ | |
| 多个显示窗口 | ☐ | ☑ | ☐ | 反向差异 |
| 调试日志窗口（`printlog`） | ☑ | ☐ NSLog | ☐ stdout | 建议不追平 |

---

## 2. macOS (Cocoa) 待补

macOS 的日常功能已经齐了（存档、金手指、RAM 工具、Lua、录制、Wi-Fi、固件、Slot 1/2 全有），
缺的集中在**开发者视图**和**电影编辑**上。

### P1 — 调试视图族（8 个窗口 + 反汇编器 + IO 寄存器视图）

Windows 侧实现：`disView.cpp`(32 KB)、`IORegView.cpp`(34 KB)、`palView.cpp`(15 KB)、
`tileView.cpp`(31 KB)、`mapView.cpp`(8 KB)、`oamView.cpp`(15 KB)、`matrixView.cpp`(8 KB)、
`lightView.cpp`(5 KB)、`soundView.cpp`(16 KB)、`fsnitroView.cpp`(11 KB)。

- ☐ 反汇编器（ARM9/ARM7，地址跳转、跟随 PC、导出）
- ☐ IO 寄存器视图（分组、实时刷新、可写寄存器）
- ☐ 调色板视图（主/子引擎、扩展调色板）
- ☐ 图块视图（BG/OBJ、4/16/256 色、翻转、调色板索引）
- ☐ 地图视图（BG 拼图预览、尺寸/滚动）
- ☐ OAM 视图（精灵列表、属性、预览）
- ☐ 矩阵视图（位置/投影/纹理矩阵 + 栈）
- ☐ 光照视图（灯光方向/颜色）
- ☐ 声音状态视图（16 通道、波形、ADSR、抓取）
- ☐ NitroFS 视图（浏览 ROM 文件系统、单文件/全部解包）

现状：macOS 只有 `MemoryViewerWindow`（内存）与 `CocoaGraphicsController`（图层）。上面的
视图在 cocoa 全树无任何匹配（`Disassembler`/`PalViewer`/`TileViewer` 等均为 0 命中）。

做法建议：先**抽共享 core**再写宿主，和 `shared/ramwatch_core.cpp` / `ram_search_core.cpp`
已经走过的路一致 —— 把地址解码、图块解压、OAM 解析、音轨状态快照这些纯逻辑放进
`src/frontend/shared/`，两个前端各自只做列表/网格控件。

验收：任取一款 retail ROM，暂停后在两个平台上打开同一视图，图块/调色板/精灵内容一致；
反汇编器在 ARM9 与 ARM7 分别能跟随 PC 且与 Windows 输出同一条指令。

### P1 — DSM 电影编辑器

- ☐ 逐帧网格编辑（13 个按键位 + 触屏 X/Y + 麦克风 + 盒盖 + Reset）
- ☐ 插入/删除/剪切/复制/粘贴整段帧，标记与跳转
- ☐ CSV 剪贴板互操作（`frame,R,L,D,U,S,T,B,A,Y,X,W,E,G,TouchX,TouchY,Touch, Mic,Lid,Reset`）
- ☐ 原格式回写（文本/二进制 .dsm）

Windows 侧：`dsmeditor.cpp`(73 KB) + `dsmcsv.cpp`(11 KB)。macOS 只做了播放/录制，无编辑。

### P2 — 电影对话框的元数据部分

- ☐ Play 前预览 ROM 序列号/CRC、作者、注释、录制时间、核心版本、帧数、rerecord 数
- ☐ ROM / 核心版本不匹配时红色高亮并提示 desync
- ☐ 播放暂停到指定帧（默认末帧）
- ☐ 录制时可写注释，并可只改头部的注释行
- ☐ 录制起点三选一：开机 / Reset+SRAM / 当前状态

### P2 — 窗口与显示细节

- ☐ Always On Top（`NSWindow` level）
- ☐ Lockdown（锁住输入，防误触）
- ☐ 屏幕比例 1.0:1.0 … 1.5:0.5、垂直居中、间距颜色、拖动边框改间距
- ☐ 全屏选项：强制窗口化全屏 / 全屏显示菜单 / 自动隐藏光标
- ☐ Pad To Integer（整数倍内边距）与"保持宽高比"的完整语义
- ☐ HUD "Display Graphical Input"（DS 按键贴图叠加）
- ☐ 载入策略：全部读入 RAM / 从磁盘流式

### P3 — 不建议单独做

- 旧式 AVI/WAV 录制：macOS 已有 AVFoundation，容器/编码覆盖面反而更宽。
- 调试日志窗口：`NSLog`/Console.app 已覆盖。

---

## 3. Linux / GTK3 待补

GTK3 这半年补得很快（RAM Watch/Search、金手指重写、FFmpeg 录制、图层、IO 寄存器），
但**日常可用性与开发者工具都还有明显缺口**，尤其是输入侧（Turbo / 帧步进）和配置类对话框。

### P0 — 输入与播放控制（日常手感）

- ☑ **Turbo（逐按钮连发）**：注意这不是"加速"，而是 DeSmuME 的 autofire ——
  勾上某个 DS 按钮后，按住它就会隔帧松开／按下（Windows 的 `Turbo`/`TurboTime`
  与 macOS 的 per-input turbo 是同一件事）。GTK3 的实现：
  `shared/ctrlssdl.cpp` 的 `turbo_cfg[]` + `ApplyTurbo()`（在 `update_keypad()`
  里、antipodal 限制之后套用，和 Windows 的 `SetManualTurbo()` 同序）；
  每按钮的开关放在 Config > Edit controls 对话框里，随 `config` 的 `[TURBO]`
  组持久化。因为开关挂在 DS 按钮上，键盘和手柄同时生效。
  另注：真正的"快进"GTK3 早就有 —— `Boost` 键（默认 `O`，`keys_latch` 的
  `KEY_BOOST` 位）在解码循环里绕过帧率限制。
- ☐ **帧跳跃**（向前跳 N 帧）：macOS 有 `Frame Jump`，GTK3 没有。
- ☑ **帧步进**：`File > Frame advance`（`Ctrl+N`，同 Windows 的 `N`）。
  运行中按下先暂停，暂停后每按一次走一帧；步进走完整的单帧管线
  （输入 → 核心 → Lua 回调 → HUD/呈现），帧跳过被强制关闭。

### P0 — 开发者视图（缺 10 个）

- ☐ 内存视图（可改值、断点、区域导出）— macOS 有 `MemoryViewerWindow`，Windows 有 `memView.cpp`
- ☐ 反汇编器
- ☐ 调色板 / 图块 / 地图 / OAM / 矩阵 / 光照 / 声音状态 / NitroFS 视图
- ◐ IO 寄存器视图已有（`tools/ioregsView.cpp`），可作为其余视图的宿主范式
- ◐ GPU 图层开关已有，缺"Main GPU / Sub GPU"两级总开关

### P1 — 文件与 ROM 生命周期

- ☐ ROM Info 对话框（标题、代号、容量、CRC、存档类型、当前引擎）
- ☐ 卸载 ROM（Close ROM），现在只能重新打开一个 ROM
- ☐ 路径设置：截图 / 存档 / 录像 / 金手指 / 电影各自独立目录（现在写死在 XDG 目录）
- ☐ ADVANsCEne 数据库导入 + "从数据库自动判定存档类型"

### P1 — 配置对话框

- ☐ Slot 1（零售卡 / 自制卡 / 下载播放）配置
- ☐ Slot 2（GBA 卡带、RAM 扩展、Rumble、PassMe…）选择界面；现在只能手改 `config.cfg`
- ☐ 麦克风：设备选择与输入电平（`mic_sdl.cpp` 已用默认设备）
- ☐ Wi-Fi 设置（MAC、AP 子网掩码、模拟 AP 数量）
- ☐ 固件设置补全：现在只有语言，缺 MAC/子网/用户信息等
- ☐ 热键自定义：现在所有命令是写死的 `GtkAccel`，用户改不了
- ☐ 3D 设置的实验性开关：Edge Marking、Fog、Enable Textures、Line Hack、
      GPU Color Depth、Shadow Polygons、Zero Alpha、NDS Depth、Depth L-Equal

### P1 — 电影与录像

- ☐ DSM 编辑器（同 macOS，共用同一套 core）
- ☐ 电影元数据 / ROM 校验 / 暂停到帧 / 改注释
- ☐ 录像暂停（`Pause Recording`），现在只能开始/停止

### P2 — 截图与显示

- ☐ 截图：选格式（PNG/BMP/JPEG…）、选目录、另存为对话框；现在固定
      `~/Pictures/desmume-screenshot-N.png`，重名靠自增序号
- ☐ Always On Top、Lockdown
- ☐ 屏幕比例 / 垂直居中 / 间距颜色（现有 `Screen Gap` 只有像素间距）
- ☐ 整数倍缩放 / 保持宽高比
- ☐ 全屏选项与"全屏时显示菜单栏"开关

### P3 — 低优先

- 旧式 AVI/WAV 录制（FFmpeg 路径已覆盖）
- 调试日志窗口（stdout 已够用）

---

## 4. 两端共同的空白（一次设计，两处实现）

这些是"Windows 有、其余全都缺"，最适合先抽 core：

| 主题 | Windows 参考 | 可抽的共享层 | 宿主工作量 |
|---|---|---|---|
| 调试视图族 | `disView` / `palView` / `tileView` / `mapView` / `oamView` / `matrixView` / `lightView` / `soundView` / `fsnitroView` | `shared/debug_views/`：地址解码、图块解压、OAM 解析、音轨快照、NitroFS 目录 | 每个视图一个控件 + 一个菜单项 |
| DSM 编辑 | `dsmeditor.cpp` + `dsmcsv.cpp` | `shared/dsm_editor_core`：帧模型、选区、CSV 编解码、回写 | 网格控件（NSTableView / GtkTreeView） |
| 路径模型 | `pathsettings.cpp` | 一个"类别 → 目录"的配置结构 + 解析函数 | 设置页 |
| 热键模型 | `hotkey.cpp`（命令枚举 + 绑定表） | 命令枚举与默认绑定表 | 键捕获控件 |
| ROM 载入策略 | `ID_LOADTOORAM` / `ID_STREAMFROMDISK` | 已由 core 提供，缺的是每前端的持久化开关 | 一行菜单 |
| 屏幕布局模型 | 比例 / 居中 / 间距 / 整数缩放 | 与 `ScreenLayout` 相关的计算 | 菜单 + 绘制路径 |

注意：`shared/` 里已有 `ram_debug_core.cpp`、`ramwatch_core.cpp`、`ram_search_core.cpp`，
RAM 工具的三端一致性就是这么来的 —— 新工具沿用同一模式，避免出现三份实现。

---

## 5. 反向差异（不要"对齐掉"）

写这份清单时容易只朝一个方向看。以下能力 macOS / GTK3 **比 Windows 更全**，改动时应保留：

- macOS：多显示窗口（`New Display Window`）、Replay 的"回滚到已保存"、
  独立截图/AV 捕获工具、Hybrid 2:1/16:9/16:10 布局、自定义旋转角度、
  GDB stub 控制面板、HUD 里 3D FPS 与 CPU 负载。
- GTK3：FFmpeg 录制器（编码器按容器过滤、双屏合成、旋转/间距）、
  WebGPU presentation 后端选择、`Screen Gap` 独立对话框。
- 平台相关、建议明确"不移植"：Windows 的现代 UI 字体与 per-monitor DPI、
  DirectDraw 显示方式、旧式 AVI/WAV、`printlog` 日志窗口。

---

## 6. 建议顺序

1. ~~**GTK3 P0**：Turbo + 帧步进~~ —— 已完成（2026-11-18）。
2. **共享层**：`shared/debug_views/` 骨架 + 内存视图，先让 GTK3 有一个可用的十六进制查看器，
   同时验证"core + 两种宿主"的模式。
3. **调试视图族**：按 图块 → 调色板 → OAM → 地图 → 其余 的顺序推；每个视图独立可交付。
4. **DSM 编辑器**：core 先行，macOS 与 GTK3 各自接网格控件。
5. **配置类对话框**：GTK3 的 Slot 1/2、麦克风、Wi-Fi、热键、路径（数量多但都很小）。
6. **细节**：截图设置、ROM Info、卸载 ROM、Always On Top、屏幕比例。

每完成一项，同步更新 `documentation/FEATURES.md` 对应段落，并在此文件把 `☐` 改成 `☑`。

---

## 7. 本次核对的局限

- 核对本身是静态比对（菜单定义 + 源码检索）。GTK3 侧的增量改动已用 WSL(Arch) 的
  meson/ninja 编译通过（GTK3、GTK2、CLI 全部链接成功），Turbo 的连发时序与
  `[TURBO]` 配置解析有一次性测试程序覆盖；但**菜单点击与帧步进的 GUI 行为没有在
  真实显示器上跑过**，上线前建议人工过一遍：运行中按 `Ctrl+N` 应立刻暂停，再按逐帧前进，
  恢复运行后不应出现"追赶"式快进。
- Cocoa 侧没有编译验证（需要 macOS + Xcode）。
- GTK2 按维护模式处理，未纳入对比；它的功能集是 GTK3 的子集减去近期新增项。
- "缺失"以入口/实现的存在性判断。若某项在别处以非常规路径存在（例如只由 Lua API 暴露），
  需要再确认后调整。
