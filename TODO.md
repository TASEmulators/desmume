# TODO — Windows 已有、macOS (Cocoa) / Linux (GTK3/GTK4) 尚缺的功能

本文档以 **Windows 前端为基线**，逐项核对 macOS (Cocoa)、Linux GTK3 与 GTK4 三个前端
缺什么，以及补上它大概要动哪里。GTK2 处于维护模式（只修 bug），不在范围内。

本次为**重新核对**（截至 2026-11-26，覆盖到 `374b6a3e` 等 2026-11-25 的改动），
上一版把 GTK4 混在“GTK3”里、且遗漏了几项最近已补齐的功能，已按当前源码修正。

## 0. 口径与证据

- 基线：`src/frontend/windows/resources.rc` 的完整菜单树 + Windows 侧各窗口实现文件。
- macOS：`src/frontend/cocoa/translations/English.lproj/MainMenu.xib` 的菜单 +
  `userinterface/*.mm` 的源码检索。部分菜单项是运行时用
  `installToolMenuItemWithTitle:` 安装的（Memory Viewer / RAM Watch / RAM Search /
  Disassembler / Path Settings），只看 xib 会漏。
- Linux：**GTK3 与 GTK4 是两套独立前端目录**，源码并不相同，只共享 `libdesmume` 与
  `src/frontend/shared/`。GTK3 看 `gtk/menu.ui` + `gtk/main.cpp` + `gtk/tools/*`；
  GTK4 看 `gtk4/menu.ui` + `gtk4/main.cpp` + `gtk4/configdialog.cpp` + `gtk4/tools/*`。
  `gtk4/menu.ui` 开头明确写着它是 GTK3 菜单的“只列已接线项”的子集。
- 判定"缺失"以**源码里没有对应实现或 UI 入口**为准，不是单纯"菜单里没有"。
  底层有实现但用户无法从 UI 使用记 `◐`。凡属平台原生差异（Windows 的字体/DPI、
  DirectDraw、AVI 容器）单列，并建议不移植。
- 状态标记：`☐` 未开始 · `◐` 部分具备 · `☑` 已完成。

---

## 1. 能力矩阵

### 1.1 统计摘要（43 项）

| 平台 | ☑ | ◐ | ☐ | 说明 |
|---|---:|---:|---:|---|
| Windows（基线） | 39 | 3 | 1 | 自身缺口只有"多显示窗口"；3 项部分：手动存档类型、独立截图工具、帧跳跃 |
| macOS (Cocoa) | 30 | 6 | 7 | 日常功能齐全，缺口集中在开发者视图、DSM 编辑、载入策略与窗口细节 |
| Linux GTK3 | 23 | 12 | 8 | 内存/IO/反汇编已补齐，主要缺 8 个开发者视图、DSM、ROM 生命周期与配置细节 |
| Linux GTK4 | 21 | 11 | 11 | 迁移中的子集前端；2026-11-26 补齐设置类配置与 ROM 生命周期（ROM Info / 存档导入导出 / ADVANsCEne / 存档类型 / GPU 图层），缺录制、HUD 菜单、开发者视图与 DSM |

- **Cocoa 与 GTK3 同时完全缺失（5 项）**：载入策略、DSM 编辑器、8 个开发者视图、
  Always On Top / Lockdown、调试日志窗口。GTK4 也同样缺这 5 项，适合先抽共享 core。
- **与上一版相比的状态变更**：
  - GTK3 的**热键自定义**（`gtk/main.cpp:5703-5721` 的 `hotkey_defs[]` + 配置对话框）、
    **固件设置**（`gtk/main.cpp:5569-5667`：昵称/消息/颜色/生日/语言/MAC）、
    **3D 实验开关**（`gtk/graphics.ui` 的 `line_hack`/`edgemark`/`fog`/`textures`/
    `shadow_polygons`/`nds_depth_calc`/`depth_lequal_facing`/`color_depth_combo`）
    均已补齐，不再是 ☐。
  - GTK3 的**内存视图**已实现（`gtk/tools/memoryViewer.cpp`），上一版矩阵 ☑ 但 §3 又写缺，自相矛盾，已修正。
  - Windows 的**手动存档类型选择**菜单代码已被注释（`windows/main.cpp:1988-2010`，
    仅保留自动检测），**帧跳跃**全 windows 目录无对应命令，两项从 ☑ 降为 ◐。
  - Windows **无独立截图工具**（Cocoa 有），截图项从 ☑ 降为 ◐。
  - GTK4 单独成列；它有而 GTK3 没有的 **Close ROM** 属迁移差异，不要"对齐掉"。
  - **GTK4 配置功能迁移（2026-11-26）**：路径设置、Slot 1/2、固件、Wi-Fi、麦克风、
    3D 实验开关、截图格式/目录、色彩深度/多重采样 已随统一设置窗口落地，
    `☐`/`◐` 相应上调。
  - **GTK4 ROM 生命周期与图层（2026-11-26，P10）**：ROM Info、存档导入/导出、
    存档类型选择（`Saves 0–6`）、ADVANsCEne 数据库导入、View Layers 10 项全部落地，
    这 5 项从 `☐` 上调为 `☑`。存档类型选择像 GTK3 一样是菜单项，不在设置对话框里。

### 1.2 矩阵

| 能力 | Windows | macOS | GTK3 | GTK4 | 备注 |
|---|---|---|---|---|---|
| ROM 载入 / 最近列表 | ☑ | ☑ | ☑ | ☑ | |
| 卸载（Close/Unload ROM） | ☑ | ☑ | ☐ | ☑ | GTK3 只能换 ROM 或退出；GTK4 `app.close` 已有 |
| ROM Info 对话框 | ☑ | ☑ | ☑ | ☑ | GTK3 `gtk/main.cpp:5418-5465`；GTK4 `app.rominfo`（桥接 `desmume_emu_rom_info()`） |
| 载入策略（全部读入 RAM / 从磁盘流式） | ☑ | ☐ | ☐ | ☐ | 三端全缺，core 已支持 |
| 存档导入 / 导出 | ☑ | ☑ | ☑ | ☑ | GTK4 File 菜单；导入支持 .sav/.dsv/.duc/.dss |
| 存档类型选择 | ◐ | ☑ | ☑ | ☑ | Windows 仅自动检测；GTK3/GTK4 `Saves 0–6`；Cocoa 有 ROM Save Type 子菜单 |
| ADVANsCEne 数据库（导入 + 自动判存档类型） | ☑ | ☑ | ☐ | ☑ | GTK4 File 菜单导入 XML → `~/.config/desmume/desmume.ddb`；GTK3 仍缺入口 |
| 即时存档（10 槽 + 另存/另读） | ☑ | ☑ | ☑ | ☑ | |
| 金手指：列表 / 搜索 / 禁用 / 数据库 | ☑ | ☑ | ☑ | ◐ | GTK4 有 Cheat list + Disable，无搜索/数据库 |
| 电影（.dsm）播放 / 录制 / 停止 | ☑ | ☑ | ☑ | ☑ | |
| 电影：元数据、ROM 校验、暂停到某帧、改注释 | ☑ | ☑ | ☑ | ☑ | 三端共用 `shared/movie_info_core`；GTK4 `tools/movie.cpp` 已接 |
| DSM 时间线编辑器（逐帧网格 / CSV 剪贴板） | ☑ | ☐ | ☐ | ☐ | 四端共同空白，见 §4 |
| 视频录制 | ☑ MP4/AVI/WAV | ☑ AVFoundation | ☑ FFmpeg | ☐ | |
| 旧式 AVI / WAV / UserSPU WAV | ☑ | ◐ 仅 MP4/MOV UI | ◐ 仅 H.264 / FLAC | ☐ | 建议不追平，见 §5 |
| 截图：格式选择、目录、独立截图工具 | ◐ | ☑ | ◐ | ◐ | Windows 无独立工具；GTK3 有格式/目录无另存对话框；GTK4 有格式/目录/文件名模板，仍无另存对话框 |
| 反汇编器 | ☑ 单窗口 + CPU 下拉 | ☑ | ☑ | ☑ | 三端共用 `shared/disassembly_core`，GTK3/GTK4 各一份宿主 |
| 内存视图（hex，可改值，断点） | ☑ | ☑ | ☑ | ☑ | GTK3/GTK4 均已实现 |
| IO 寄存器视图 | ☑ | ☐ | ☑ | ☑ | `tools/ioregsView.cpp` / `gtk4/tools/ioregs.cpp` |
| 调色板 / 图块 / 地图 / OAM / 矩阵 / 光照 / 声音状态 / NitroFS 视图 | ☑ | ☐ | ☐ | ☐ | 8 个窗口，其余三端全空 |
| GPU 图层开关 | ☑ 含主/子 GPU | ☑ | ◐ 缺主/子 GPU 总开关 | ☑ 10 项，主/子各 5 | GTK4 View > View Layers；状态随会话，不写 config |
| RAM Watch / RAM Search | ☑ | ☑ | ☑ | ☑ | 已共享 `shared/ram*_core` |
| GDB stub | ☑ 命令行 | ☑ 控制面板 | ☑ 命令行 | ☑ 命令行 | Cocoa 控制面板是反向优势 |
| Lua 脚本控制台 | ☑ | ☑ | ☑ | ☑ | |
| HUD：FPS / 输入 / 图形输入 / 帧 / 延迟 / RTC / 麦克风 | ☑ | ◐ 无"图形输入" | ☑ | ◐ 有 config 项无 UI | GTK4 `config_opts.h:50-57` 有 7 项开关但菜单无入口 |
| HUD 字体、编辑模式 | ☑ 字体+编辑 | ◐ 设置窗 | ◐ 编辑/重置，无字体 | ☐ | |
| 帧跳 0–9 + 限速 | ☑ | ☑ 档位不同 | ☑ | ☑ | |
| Turbo（逐按钮连发） | ☑ | ☑ | ☑ | ☐ | GTK4 未编入 `ctrlssdl.cpp` |
| 帧步进 / 帧跳跃 | ◐ 仅步进 | ☑ | ◐ 仅步进 | ◐ 仅步进 | Windows/GTK 均无 Frame Jump |
| 后台输入 / 后台暂停 | ☑ | ☐ | ◐ SDL hint，无开关 | ☐ | |
| 热键自定义 | ☑ 热键配置窗 | ☑ 输入映射 + 配置档 | ☑ 16 项可重绑定 | ☐ 固定 accel | GTK3 已补，见 §3 |
| Slot 1 配置 / Slot 2 选择 | ☑ | ☑ | ☑ | ☑ | GTK4 在设置窗 "Slots" 页（设备/描述/FAT 目录/调试目录/CFlash/GBAROM） |
| 固件设置（语言、MAC、子网等） | ☑ | ☑ | ◐ 缺子网等 | ◐ 缺子网等 | 三端都是昵称/消息/颜色/生日/语言/MAC；GTK4 "Firmware" 页 |
| 麦克风设置（设备/电平） | ☑ | ☑ | ◐ | ◐ | GTK3/GTK4 有 4 种输入模式 + WAV 样本，缺设备/电平 |
| Wi-Fi 设置 | ☑ | ☑ | ◐ | ◐ | GTK3/GTK4 有启用/兼容模式 + PCAP 网卡，缺 MAC/AP 子网 |
| 3D 设置（含 Edge Marking/Fog/Line Hack 等） | ☑ | ☑ | ☑ 9 项实验开关齐全 | ☑ 9 项 + 色彩深度 + 多重采样 | Linux 差异见 §3.2 |
| 音频设置 | ☑ 对话框 | ☑ 首选项 | ☑ 单对话框 | ☑ 配置页 | 上一版把 GTK3 记为 ◐ 属低估 |
| 路径设置（每类文件独立目录） | ☑ | ☑ | ☑ | ☑ | 共用 `shared/path_settings_core.h`；GTK 10 项、Cocoa 9 项 |
| Always On Top / Lockdown | ☑ | ☐ | ☐ | ☐ | |
| 屏幕比例 / 垂直居中 / 间距颜色 / 拖动改间距 | ☑ | ◐ 只有间距百分比 | ◐ 只有 px 间距 | ◐ 只有间距开关 | |
| 全屏选项（强制窗口化、显示菜单、自动隐藏光标） | ☑ | ◐ | ◐ | ◐ | 三端只有全屏切换 |
| 整数倍缩放 / 保持宽高比 | ☑ | ◐ | ◐ | ◐ | |
| 多个显示窗口 | ☐ | ☑ | ☐ | ☐ | 反向差异 |
| 调试日志窗口（`printlog`） | ☑ | ☐ NSLog | ☐ stdout | ☐ stdout | 建议不追平 |

---

## 2. macOS (Cocoa) 待补

Cocoa 的日常功能已经齐了（存档、金手指、RAM 工具、反汇编器、Lua、录制、Wi-Fi、
固件、Slot 1/2、路径设置全有），缺的集中在**开发者视图**、**电影编辑**和**窗口细节**。
`DisassemblerWindow` 与 `PathSettingsWindowController` 已由 `9794d9e6` / `374b6a3e`
补上，不再列入待补。

### P1 — 调试视图族（IO 寄存器 + 8 个窗口）

Windows 侧实现：`IORegView.cpp`(34 KB)、`palView.cpp`(15 KB)、`tileView.cpp`(31 KB)、
`mapView.cpp`(8 KB)、`oamView.cpp`(15 KB)、`matrixView.cpp`(8 KB)、`lightView.cpp`(5 KB)、
`soundView.cpp`(16 KB)、`fsnitroView.cpp`(11 KB)；反汇编器已完成，共用 `shared/disassembly_core`。

- ☐ IO 寄存器视图（分组、实时刷新、可写寄存器）
- ☐ 调色板视图（主/子引擎、扩展调色板）
- ☐ 图块视图（BG/OBJ、4/16/256 色、翻转、调色板索引）
- ☐ 地图视图（BG 拼图预览、尺寸/滚动）
- ☐ OAM 视图（精灵列表、属性、预览）
- ☐ 矩阵视图（位置/投影/纹理矩阵 + 栈）
- ☐ 光照视图（灯光方向/颜色）
- ☐ 声音状态视图（16 通道、波形、ADSR、抓取）
- ☐ NitroFS 视图（浏览 ROM 文件系统、单文件/全部解包）

现状：Cocoa 只有 `MemoryViewerWindow`、`DisassemblerWindow` 与 `CocoaGraphicsController`（图层），
上面的视图在 cocoa 全树无任何匹配。

做法建议：先**抽共享 core**再写宿主，和 `shared/ramwatch_core.cpp` / `ram_search_core.cpp`
已经走过的路一致 —— 把地址解码、图块解压、OAM 解析、音轨状态快照这些纯逻辑放进
`src/frontend/shared/`，各前端只做列表/网格控件。

验收：任取一款 retail ROM，暂停后在两个平台上打开同一视图，图块/调色板/精灵内容一致。

### P1 — DSM 电影编辑器

- ☐ 逐帧网格编辑（13 个按键位 + 触屏 X/Y + 麦克风 + 盒盖 + Reset）
- ☐ 插入/删除/剪切/复制/粘贴整段帧，标记与跳转
- ☐ CSV 剪贴板互操作（`frame,R,L,D,U,S,T,B,A,Y,X,W,E,G,TouchX,TouchY,Touch, Mic,Lid,Reset`）
- ☐ 原格式回写（文本/二进制 .dsm）

Windows 侧：`dsmeditor.cpp`(73 KB) + `dsmcsv.cpp`(11 KB)。Cocoa 只做了播放/录制，无编辑。

### P2 — 电影对话框的元数据部分（已完成）

- ☑ Play 前预览 ROM 序列号/CRC、作者、注释、录制时间、核心版本、帧数、rerecord 数
  （`MoviePlayWindowController` + `MovieInfoPreview`，共用 `shared/movie_info_core`）
- ☑ ROM / 核心版本不匹配时红色高亮并提示 desync
- ☑ 播放暂停到指定帧（默认末帧）
- ☑ 录制时可写注释，并可只改头部的注释行（`MovieInfo::WriteComments`）
- ☑ 录制起点三选一：开机 / Reset+SRAM（含 `.dsv` 选择）/ 当前状态

### P2 — 载入策略与窗口细节

- ☐ 载入策略：全部读入 RAM / 从磁盘流式（core 已支持，缺持久化开关）
- ☐ 后台输入 / 后台暂停
- ☐ Always On Top（`NSWindow` level）、Lockdown（锁住输入）
- ☐ 屏幕比例 1.0:1.0 … 1.5:0.5、垂直居中、间距颜色、拖动边框改间距
- ☐ 全屏选项：强制窗口化全屏 / 全屏显示菜单 / 自动隐藏光标
- ☐ Pad To Integer（整数倍内边距）与"保持宽高比"的完整语义
- ☐ HUD "Display Graphical Input"（DS 按键贴图叠加）
- ☐ HUD 字体选择与编辑模式（现为固定 Bundle 字体 + 设置窗）

### P3 — 不建议单独做

- 旧式 AVI/WAV 录制：已有 AVFoundation，容器/编码覆盖面反而更宽（底层 muxer 也已注册）。
- 调试日志窗口：`NSLog`/Console.app 已覆盖。

---

## 3. Linux / GTK 待补

### 3.1 GTK3

GTK3 近期补得很快：RAM Watch/Search、金手指重写、FFmpeg 录制、图层、IO 寄存器、
反汇编器、Slot 1/2、ROM Info、麦克风、Wi-Fi、路径设置、热键/3D/固件都已就位。
剩下的主要是**8 个开发者视图**、**DSM 编辑**、**ROM 生命周期**和显示细节。

#### P0 — 输入与播放控制

- ☑ **Turbo（逐按钮连发）**：不是"加速"，而是 autofire。
  `shared/ctrlssdl.cpp` 的 `turbo_cfg[]` + `ApplyTurbo()`，每按钮开关在
  Config > Edit controls，随 `[TURBO]` 持久化。真正的快进是 `Boost` 键。
- ☑ **帧步进**：`File > Frame advance`（`Ctrl+N`），走完整单帧管线。
- ☐ **帧跳跃**（向前跳 N 帧）：Cocoa 有 `Frame Jump`，Windows 与 GTK3 都没有。

#### P0 — 开发者视图（缺 8 个）

- ☑ 内存视图（`gtk/tools/memoryViewer.cpp`）、反汇编器（`gtk/tools/disassembler.cpp`）
- ☑ IO 寄存器视图（`gtk/tools/ioregsView.cpp`），可作为其余视图的宿主范式
- ☐ 调色板 / 图块 / 地图 / OAM / 矩阵 / 光照 / 声音状态 / NitroFS 视图
- ◐ GPU 图层开关已有 10 项，缺"Main GPU / Sub GPU"两级总开关

#### P1 — 文件与 ROM 生命周期

- ☐ 卸载 ROM（Close ROM）：GTK3 只能重新打开一个 ROM（GTK4 已有，可直接抄 action）
- ☐ ADVANsCEne 数据库导入 + "从数据库自动判定存档类型"
- ☑ ROM Info、路径设置（10 项，空值保留旧默认并作为占位提示）

#### P1 — 配置对话框剩余项

- ☐ Slot 2 Guitar Grip / Easy Piano / Paddle / HCV-1000 的专用按键、条码配置
- ◐ 麦克风：4 种输入模式 + WAV 样本已可设置，缺录音设备选择、输入电平与 `_0.wav` 切换
- ◐ Wi-Fi：启用/兼容模式与 PCAP 网卡已移植，缺 MAC、AP 子网掩码和模拟 AP 数量
- ◐ 固件：昵称/消息/颜色/生日/语言/MAC 已有，缺子网掩码等高级项
- ☑ 热键自定义（`gtk/main.cpp:5703-5721` 的 16 项 `hotkey_defs[]` + Load/Save/Apply）
- ☑ 3D 设置的 9 个实验开关（`graphics.ui` + `gtk/main.cpp:7008-7042`）

#### P1 — 电影与录像

- ☐ DSM 编辑器（共用同一套 core）
- ☐ 录像暂停（`Pause Recording`），现在只能开始/停止
- ☑ 电影元数据 / ROM 校验 / 暂停到帧 / 改注释（2026-11-19 接入 `shared/movie_info_core`）

#### P2 — 截图与显示

- ◐ 截图：格式与目录已在 Path settings 可设，文件名支持 `%f/%r/%t` + strftime；
  仍缺独立的"另存为"对话框
- ☐ Always On Top、Lockdown
- ☐ 屏幕比例 / 垂直居中 / 间距颜色（现有 `Screen Gap` 只有像素间距）
- ☐ 整数倍缩放 / 保持宽高比（Pad To Integer）
- ☐ 全屏选项与"全屏时显示菜单栏"开关

#### P3 — 低优先

- 旧式 AVI/WAV 录制（FFmpeg 路径已覆盖）
- 调试日志窗口（stdout 已够用）

### 3.2 GTK4（迁移中的子集前端）

GTK4 **不是 GTK3 的换皮**：`gtk4/` 有独立的 `main.cpp`、`configdialog.cpp`、`display.cpp`、
`tools.cpp` 与自己的 `menu.ui`（文件头注明"只列已接线 action"）。每次 meson 配置只编一个 GTK 前端。

- **已具备**：Open/最近列表、Close ROM、ROM Info、存档导入/导出、存档类型 `Saves 0–6`、
  ADVANsCEne 导入、即时存档、Frame advance/帧跳/限速、全屏、
  Layout/Rotation/Screen Gap、View Layers（主/子各 5 项）、WebGPU 呈现与 GPU 滤镜、
  IO 寄存器、RAM Watch/Search、Cheat list、Memory Viewer、Disassembler、
  电影播放/录制/停止（含元数据）、Lua 控制台、
  **统一设置窗的 8 页**（Emulation / Graphic / Sound / Firmware / Slots / Wi-Fi /
  Microphone / Paths，含 GTK3 的全部设置项）、截图格式/目录/文件名模板。
- **相对 GTK3 的主要缺口**：视频/音频录制、热键自定义、Turbo、HUD 菜单、
  8 个开发者视图、DSM 编辑器。
- **ROM 生命周期与图层（2026-11-26，P10）**：`emulation.{h,cpp}` 新增
  `desmume_emu_rom_info()`（九行键值对）、`import_backup` / `export_backup`、
  `set_save_type` / `get_save_type`、`import_advanscene()`、`set_layer_enabled` /
  `get_layer_enabled`；`main.cpp` 接上 ROM Info 对话框、导入/导出对话框
  （暂停 → 选文件 → 恢复）、`Saves` 单选菜单、ADVANsCEne 导入与 10 个图层开关
  （`stateful` action，按会话生效）。
- **配置功能迁移（2026-11-26，P9）**：`configdialog.cpp` 从 3 页扩到 8 页，
  `config_opts.h` 补齐 GTK3 的 3D 实验开关、Emulation/BIOS/固件镜像、固件用户数据、
  Wi-Fi、Slot 1/2、麦克风与 PathSettings 选项；`emulation.{h,cpp}` 新增对应桥接
  （CPU/BIOS/固件、3D、Slot、Wi-Fi、麦克风），并把 18 bit 输出的 BGR666 转换补进
  呈现与截图路径。需要重启内核的项在 OK 时比对签名并询问是否 Reset。
  Config 菜单同时给出逐页快捷入口（GTK3 是每个主题一个对话框）。
- **GTK4 有而 GTK3 没有**：Close ROM（`gtk4/menu.ui:21-24` + `app.close`）。
  这是迁移顺序造成的，不要把 GTK4 的入口删掉去"对齐"GTK3；应反过来给 GTK3 补上。
- **建议**：GTK4 的 UI 迁移按"已共享 core 的工具优先"推进（disassembler / memory viewer
  已经这么做了）；设置类与 ROM 生命周期已搬完，剩下的按"录制 → HUD 菜单 →
  开发者视图"排。HUD 需要先有 `agg_osd` 那套绘制管线，属独立工程。

---

## 4. 两端共同的空白（一次设计，三处实现）

这些是"Windows 有、其余三端全缺"，最适合先抽 core：

| 主题 | Windows 参考 | 可抽的共享层 | 宿主工作量 |
|---|---|---|---|
| 调试视图族 | `IORegView` / `palView` / `tileView` / `mapView` / `oamView` / `matrixView` / `lightView` / `soundView` / `fsnitroView` | `shared/debug_views/`：地址解码、图块解压、OAM 解析、音轨快照、NitroFS 目录 | 每个视图一个控件 + 一个菜单项 |
| DSM 编辑 | `dsmeditor.cpp` + `dsmcsv.cpp` | `shared/dsm_editor_core`：帧模型、选区、CSV 编解码、回写 | 网格控件（NSTableView / GtkTreeView） |
| 载入策略 | `ID_LOADTOORAM` / `ID_STREAMFROMDISK` | core 已提供，缺的是每前端的持久化开关 | 一行菜单 |
| 屏幕布局模型 | 比例 / 居中 / 间距 / 整数缩放 | 与 `ScreenLayout` 相关的计算 | 菜单 + 绘制路径 |
| 热键模型 | `hotkey.cpp`（命令枚举 + 绑定表） | 命令枚举与默认绑定表 | 键捕获控件（GTK3/Cocoa 已各有方案，GTK4 仍缺） |

已落地、可直接复用的共享层：`shared/path_settings_core.h`（13 类 INI 键/标签/说明 + 目录归一化，
Cocoa 9 类、GTK3 10 类）、`shared/disassembly_core.cpp`、`shared/movie_info_core.cpp`、
`shared/ram_debug_core.cpp`、`ramwatch_core.cpp`、`ram_search_core.cpp`、`memory_viewer_core.cpp`。
新工具沿用同一模式，避免出现四份实现。

注意：路径设置与反汇编器已经完成，不要再把它们列进"共同空白"。

---

## 5. 反向差异（不要"对齐掉"）

写这份清单时容易只朝一个方向看。以下能力 Cocoa / GTK **比 Windows 更全**，改动时应保留：

- macOS：多显示窗口（`New Display Window`）、Replay 的"回滚到已保存"、
  独立截图/AV 捕获工具（Windows 没有独立截图工具）、Hybrid 2:1/16:9/16:10 布局、
  自定义旋转角度、GDB stub 控制面板、HUD 里 3D FPS 与 CPU 负载。
- GTK3：FFmpeg 录制器（编码器按容器过滤、双屏合成、旋转/间距）、
  WebGPU presentation 后端选择、`Screen Gap` 独立对话框。
- GTK4 相对 GTK3：Close ROM；GTK4 的菜单模型只列已接线项，没有 GTK3 的灰显入口，
  迁移时不要为了"菜单一致"把未实现的项加回去。
- 平台相关、建议明确"不移植"：Windows 的现代 UI 字体与 per-monitor DPI、
  DirectDraw 显示方式、旧式 AVI/WAV、`printlog` 日志窗口。

---

## 6. 建议顺序

1. ~~**GTK3 P0**：Turbo + 帧步进~~ —— 已完成（2026-11-18）。
2. ~~**共享层**：Path Settings / 反汇编器 core~~ —— 已完成（2026-11-25）。
3. **调试视图族**：`shared/debug_views/` 骨架，按 图块 → 调色板 → OAM → 地图 → 其余 推；
   每个视图独立可交付，Cocoa / GTK3 / GTK4 各自接宿主（GTK4 的两个工具已验证该模式）。
4. **DSM 编辑器**：core 先行，Cocoa 与 GTK3 各自接网格控件。
5. **补 GTK3 的 Close ROM**（GTK4 实现可直接移植），再做 GTK3 剩余的 Slot 2 专用控制、
   麦克风设备/电平、Wi-Fi 高级项、固件子网。
6. ~~**GTK4 UI 迁移（设置部分）**~~ —— 已完成（2026-11-26）：GTK3 已有的设置对话框
   逐页搬到 `configdialog.cpp`（路径 / Slot 1&2 / 固件 / 麦克风 / Wi-Fi / 3D 实验开关 /
   色彩深度与多重采样）。剩下的 GTK4 项见 §3.2。
7. ~~**GTK4 ROM 生命周期 + 图层**~~ —— 已完成（2026-11-26）：ROM Info、存档导入/导出、
   存档类型、ADVANsCEne 导入、View Layers。
8. **GTK4 录制**：GTK3 的 FFmpeg 录制器（`gtk/` 侧按容器过滤编码器、双屏合成、旋转/间距）
   可抽成 `shared/` 的录制 core，再接 GTK4 的对话框；工作量比前两项大。
9. **GTK4 HUD 菜单**：需要先移植 `agg_osd.cpp` / `DrawHUD` / 字体与 `Agg_setHudTarget`
   这套绘制管线，之后 `config_opts.h` 里已有的 7 个开关才有效。
10. **细节**：帧跳跃（Windows/GTK）、截图另存为、Always On Top、屏幕比例、
    录像暂停、后台输入/暂停、HUD 图形输入。

每完成一项，同步更新 `documentation/FEATURES.md` 对应段落，并在此文件把 `☐` 改成 `☑`。

---

## 7. 本次核对的局限

- 核对本身是静态比对（菜单定义 + 源码检索 + 近期提交 diff）。GTK3 侧的增量改动此前已用
  WSL(Arch) 的 meson/ninja 编译通过（GTK3、GTK2、CLI 全部链接成功），Turbo 的连发时序与
  `[TURBO]` 配置解析有一次性测试程序覆盖；但**菜单点击与帧步进的 GUI 行为没有在
  真实显示器上跑过**，上线前建议人工过一遍：运行中按 `Ctrl+N` 应立刻暂停，再按逐帧前进，
  恢复运行后不应出现"追赶"式快进。
- 本次重新核对**没有重新编译**：GTK4 的菜单/action、GTK3 最近补齐的热键/固件/3D、
  Cocoa 的运行时菜单项只做了源码确认，未做构建验证。
- **2026-11-26 的 GTK4 配置迁移**：`ninja gtk4/desmume`（Arch/WSL，GTK 4.24.2）通过，
  `gtk4-builder-tool validate menu.ui` 通过；桥接层用临时测试程序（已删除）验证了
  CPU/BIOS/固件用户数据（含 UTF-16 与 MAC 解析）、3D 开关、Slot 列表与行开关、
  `path.setpath` 与截图文件名模板，全部通过。**设置窗口本身没有在真实显示器上打开过**
  （本环境无 X/Wayland，Broadway 后端也连不上），8 页的控件树、逐页切换与 OK 应用
  路径只有静态核对，上线前建议人工过一遍：路径页改目录后打开 ROM 应落到新目录、
  R4/调试设备要求填目录、改 BIOS 后提示 Reset。
- **2026-11-26 的 GTK4 ROM 生命周期与图层**：`ninja gtk4/desmume` 与
  `gtk4-builder-tool validate menu.ui` 均通过；临时测试程序（已删除）覆盖了
  无 ROM 时的拒绝路径、存档类型往返与越界回退、图层设置在核心启动前被拒绝、
  ADVANsCEne XML → `.ddb` 的导入（1 条记录，落在 `~/.config/desmume/desmume.ddb`）。
  **对话框本身仍未在真实显示器上打开过**：ROM Info 的九行内容、导入/导出对话框的
  暂停→恢复、`Saves` 菜单的单选标记、图层开关的实际画面裁剪只有静态核对。
- Cocoa 侧没有编译验证（需要 macOS + Xcode）。
- GTK2 按维护模式处理，未纳入对比；它的功能集是 GTK3 的子集减去近期新增项。
- "缺失"以入口/实现的存在性判断。若某项在别处以非常规路径存在（例如只由 Lua API 暴露），
  需要再确认后调整。
