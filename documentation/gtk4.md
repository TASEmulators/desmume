# DeSmuME GTK3 → GTK4 迁移方案

> 迁移对象：`desmume/src/frontend/posix/gtk/`（GTK3，约 53 万字节 C++，25 个 `.cpp`/`.h`，目录内共 32 个文件）
> 迁移目标：`desmume/src/frontend/posix/gtk4/`（新增目录，GTK4）
> 原则：**`gtk/` 一行不改，保持可编译、可发布**；所有新工作只进 `gtk4/`
> 基线版本：**GTK ≥ 4.10**（理由见 §3 D2），实际验证目标 4.14+
> 状态：P0–P8 全部完成。P4 工具（IO registers / RAM Watch / RAM Search / Play·Record Movie / Lua 控制台）；P4c Windows 内存监视弹窗移植（RAM Watch Add Cheat + RAM Search Add Watch 注释弹窗）；P4d Memory Viewer 内存查看器（对齐 Windows `memView.cpp` / macOS `MemoryViewerWindow`，tool index 4）；P5 金手指列表窗口；P6 性能与打磨（停绘/帧池零拷贝重构/非活动降频/呈现计时）；P7 wgpu（离屏渲染 + buffer 回读 + GPU 滤镜对齐 + GResource WGSL + View→GPU Filter 菜单）；P8 收尾（GTK4 desktop/metainfo/man、安装布局、CI 资产校验、弃用审计）——迁移全部落地（§14）
> 日期：2026-11-20

---

## 0. 结论先行

### 0.1 四个诉求的可行性

| 诉求 | 可行 | 结论 |
|---|---|---|
| 完整迁移到 GTK4 | 是 | 所有 GTK3 API 都有对应物；工作量集中在 44 处 `gtk_dialog_run` 的异步化 |
| UI 明显更好看 | 是，收益大 | 现有代码已经是 GMenu + GAction 架构，换成 `GtkHeaderBar`/`GtkPopoverMenuBar` 主要是改外壳，不是重写 |
| 性能提升 | **部分可以** | 帧呈现、缩放、resize、后台停绘有真实收益；帧缓冲路径零拷贝重构后像素工作量与 GTK3 持平、少了两次全帧 memcpy（不开倒车）；模拟器核心和 RAM Search 算法不会因为 GTK4 变快 |
| KDE Wayland 下原生外观（SSD） | 是，且不需要 hack | 只要**不设置自定义 titlebar**，GTK 就会让 KWin 通过 KDE server-decoration 协议画窗口边框 |

**已确认（2026-11-20）**：§13 的三项推荐全部接受——原生 GTK4 路线、GTK 4.10 底线、首版只做 texture 呈现。
GPU 滤镜（P7）是**必做项**，只是排在最后补齐，不是可选功能。

### 0.2 一句话方案

> GTK4 版不再自己画标题栏，而是把菜单栏、工具条、状态条放进窗口内容区，把窗口装饰交给平台：
> KWin/Mutter/X11 WM 画 SSD，GNOME 没有该协议时回退到 GTK 默认 CSD 标题栏。
> 视频区改成一个自定义 `GtkWidget`，用 `GdkTexture` + `GtkSnapshot` 做 GPU 合成，
> 取代 GTK3 的 Cairo `expose` 绘制和 wgpu 直接 present。

### 0.3 三条硬约束

1. **`gtk/` 冻结**：不重构、不共享源文件、不 `#if GTK_MAJOR_VERSION`。`gtk4/` 从 `gtk/` 复制后再改，两边独立演化。
2. **所有 GTK 调用只在主线程**：现有 `g_idle_add` 模拟循环本身在主线程，但要清理工具窗口里的嵌套主循环。
3. **窗口装饰形态必须在 surface 第一次 map 之前决定**：GTK4 没有运行时重新协商 SSD/CSD 的公开 API，切换需要重建窗口。

---

## 1. 目标与非目标

### 1.1 功能目标（必须逐项对齐）

- 打开 ROM、最近文件、拖放打开、命令行参数、`GApplication` 打开事件
- 运行 / 暂停 / 帧前进 / 重置 / 快进 / 跳帧 / FPS 限制
- 存档状态：即时存档、读档、1–10 槽位、命名槽位
- 截图、录像（x264 / FLAC / FFmpeg）、录音
- 视图：单屏 / 混合横 / 混合竖、上下屏交换、屏幕间隙、0/90/180/270 旋转、整数倍缩放、全屏、工具条/状态条/菜单栏显隐
- 输入：键盘映射、手柄、触屏/触控笔、HUD 编辑器
- 工具：RAM Watch、RAM Search、IO 寄存器、Lua 控制台、Movie Play/Record
- 金手指：内部金手指、Action Replay、Codebreaker、启用/禁用、增删改、持久化
- 设置：模拟设置（dynarec、block size）、图形设置（渲染器、缩放、滤镜）
- 拖放：ROM、Lua 脚本、RAM Watch 文件
- 截图/录像/存档路径、配置读写与 GTK3 版兼容

### 1.2 UI 目标（这次不只是"能跑"）

1. KDE Wayland / X11 下使用系统窗口装饰（SSD），外观与其它 KDE 应用一致
2. GNOME 下使用 `GtkHeaderBar`（CSD），保留现代外观
3. 视频区纯黑、等比、无边框；全屏时自动隐藏菜单/工具条/状态条，鼠标静止后隐藏光标
4. 状态信息从 `GtkStatusbar` 换成更轻的状态行 + 瞬时消息（toast 风格浮层）
5. 工具窗口用 `GtkColumnView`、`GtkSearchEntry`、`GtkPopoverMenu` 右键菜单、`GtkDropTarget`
6. 新增"键盘快捷键"窗口（`GtkShortcutsWindow`），从现有 accel 自动生成
7. 跟随系统深浅色；视频区始终黑底，不随主题变白
8. HiDPI 下文字/图标清晰，无 GTK3 逐 widget `GdkWindow` 造成的错位

### 1.3 性能目标（可量化，先测后改）

| 指标 | 现状 | GTK4 目标 | 测量方式 |
|---|---|---|---|
| 单帧呈现 CPU（1280×960 窗口，2× 缩放） | 待测基线 | ≤ 基线 50% | 自计时 + `perf record` |
| resize / 全屏切换掉帧 | 待测 | 明显减少，无闪烁 | 帧间隔直方图 |
| 窗口最小化/不可见时 CPU | 仍在 blit | 停止呈现，接近 0 | `top` / `powertop` |
| 工具窗口滚动 | 待测 | ≥ GTK3 | 目测 + 帧计时 |
| 冷启动时间 | 待测 | ≤ 基线 + 50 ms | `hyperfine` |
| 常驻内存 | 待测 | ≤ 基线 + 20 MB | `smem` |

### 1.4 非目标

- 不改模拟器核心、不改 wgpu 后端的选择逻辑（只改"呈现到哪里"）
- 不追求 GTK4 版超越 Windows/Cocoa 前端的功能面
- 不在这一轮做 Flatpak 打包
- 不保证 GTK 4.0–4.8 可编译（旧发行版继续用 GTK3 前端）
- 不做移动端/触屏专用布局

---

## 2. 现状盘点

### 2.1 目录与规模

| 文件 | 大小 | 迁移难度 | 说明 |
|---|---:|---|---|
| `main.cpp` | 261.5 KB | 高 | 窗口、绘制、动作、对话框、Lua 菜单、主循环 |
| `cheatsGTK.cpp` | 65.6 KB | 中高 | TreeView/ListStore + 7 处模态对话框 |
| `tools/ramSearch.cpp` | 38.5 KB | 中 | ListStore + 3 处对话框 |
| `tools/ramWatch.cpp` | 38.3 KB | 中 | DnD + TreeView + 窗口几何持久化 |
| `tools/luaScriptConsole.cpp` | 24.3 KB | 中 | DnD + 文件选择 + 对话框 |
| `tools/movieRecord.cpp` | 19.3 KB | 中 | 表单对话框 |
| `tools/moviePlay.cpp` | 18.9 KB | 中 | 表单对话框 |
| `tools/ioregsView.cpp` | 15.0 KB | 低 | 只读表格 |
| `utilsGTK.cpp` | 11.6 KB | 高 | 自定义 cell renderer + `GtkEventBox` 编辑器 |
| `config.cpp` / `config_opts.h` / `config.h` | 15.1 KB | 低 | 纯逻辑，可原样复制 |
| `main.ui` / `menu.ui` / `*.ui` | 60.7 KB | 中 | `menu.ui` 已是 GMenu，基本可用 |
| `desmume.gresource.xml` | 3.4 KB | 低 | 加一行路径调整 |
| `meson.build` | 2.3 KB | 低 | 复制后改依赖与 target 名 |

### 2.2 GTK3 API 使用量（`*.cpp` 实测）

| API | 次数 | GTK4 状态 |
|---|---:|---|
| `gtk_file_chooser_*` | 156 | 保留但 4.10 起弃用；推荐 `GtkFileDialog` |
| `gtk_box_pack_start` | 81 | 删除 → `gtk_box_append` + expand/align |
| **`gtk_dialog_run`** | **44** | **删除**（最大工作量） |
| `gtk_container_add` | 32 | 删除 → `gtk_*_set_child` / `append` |
| `gtk_widget_show_all` | 28 | 删除 → 默认可见 |
| `GtkFileChooserNative` | 15 | 4.10 起弃用 |
| `gtk_recent*` | 12 | `GtkRecentManager` 仍在 |
| `gtk_statusbar` | 6 | 仍在，但 4.10 弃用 |
| `gtk_widget_get_window` | 6 | 删除 → `GdkSurface` |
| `GdkEvent*` 事件结构（`GdkEventKey` 4、`GdkEventButton` 3、`GdkEventFocus` 3、`Configure`/`Expose`/`Motion` 各 1） | 13 | 删除 → `GtkEventController*` |
| `gtk_widget_override_*` | 3 | 删除 → CSS |
| `gtk_events_pending` / `gtk_main_iteration` | 6 | 删除 → `g_main_context_*` |
| `gtk_drag_dest_set` / `gtk_selection_data_get_uris` | 4 | 删除 → `GtkDropTarget` |
| `gtk_menu_popup_at_pointer` | 1 | 删除 → `GtkPopoverMenu` |
| `gdk_window_begin_draw_frame` | 1 | 删除 → `set_draw_func` / `snapshot` |

`gtk_dialog_run` 分布：`main.cpp` 25、`cheatsGTK.cpp` 7、`ramSearch/ramWatch/moviePlay` 各 3、`movieRecord` 2、`luaScriptConsole` 1。

### 2.3 已经"GTK4 友好"的部分

- 整个应用已经是 `GtkApplication` + `GActionMap` + `GSimpleAction` + GMenu 模型（`menu.ui`）。
- 主窗口用 `gtk_application_set_menubar()`，GTK4 里同一行代码会渲染成 `GtkPopoverMenuBar`。
- 模拟循环已经在主线程 `g_idle_add`，没有跨线程 UI 调用。
- 配置层（`config.cpp`）不依赖 GTK 控件。

这意味着**菜单/动作层几乎零改动**，GTK4 迁移的成本集中在：窗口外壳、渲染、对话框、TreeView。

---

## 3. 关键决策

### D1：目录与构建目标

```
src/frontend/posix/
├── gtk/          # 原样保留（GTK3）
├── gtk2/         # 原样保留
└── gtk4/         # 新增
```

- `meson_options.txt`：`frontend-gtk` 由布尔改成单选 combo `auto|gtk4|gtk3|gtk2|none`，默认 `auto`。
  `auto` 按 **gtk4（≥4.10）→ gtk3（≥3.24）→ gtk2（≥2.24）** 顺序探测 pkg-config，取第一个命中的；
  显式写 `-Dfrontend-gtk=gtk3` 之类就锁定版本，缺依赖直接报错而不是静默降级。
  一次配置只构建一个 GTK 前端（两个 target 都叫 `desmume`，需要并存就开两个 build 目录）。
- target 名：`desmume`，与 GTK3 同名。一次配置只构建一个 GTK 前端（见 D1 的选项说明），
  所以同一 build 目录里不会出现两个 `desmume`；跨 build 目录当然可以并存，但不能把两套一起 `meson install` 到同一 prefix。
- app-id：`org.desmume.DeSmuME.GTK4`，桌面文件 `org.desmume.DeSmuME.GTK4.desktop`，`StartupWMClass` 同名
- 图标仍复用 `org.desmume.DeSmuME`；gresource 前缀不变
- `meson.build` 里**移除** `-DGTK_DISABLE_DEPRECATED`（GTK4 下 `GtkTreeView`/`GtkFileChooserNative` 都会直接编译失败），改为默认显示弃用警告

### D2：GTK 版本底线 = 4.10

| 候选 | 得到的 API | 代价 |
|---|---|---|
| 4.6 | 无 `GtkFileDialog`/`GtkAlertDialog` | 44 处对话框要靠已弃用的 `GtkFileChooserNative` + `GtkMessageDialog`，代码更脏 |
| **4.10（推荐）** | `GtkFileDialog`、`GtkAlertDialog`、`GtkColorDialog`、`GtkFontDialog` | 放弃 Debian 12（4.8）/ Ubuntu 22.04（4.6） |
| 4.12 | `gtk_css_provider_load_from_string`、分数缩放改进 | 放弃 Ubuntu 24.04 之前的 LTS，收益不值 |

**理由**：GTK3 前端继续存在，旧发行版用户有退路；4.10 是 Debian 13 / Ubuntu 24.04 / Fedora 39+ 的版本，覆盖面足够。
**如果维护者坚持 4.6**：所有 `GtkFileDialog` 调用改为 `#if GTK_CHECK_VERSION(4,10,0)` 双实现，工作量 +1～2 周，方案其余部分不变。

**状态**：已确认为 4.10（2026-11-20）。`meson.build` 写 `dependency('gtk4', version: '>=4.10')`。

### D3：装饰策略 = 自适应 SSD/CSD（本次新增需求的核心）

事实（GTK 维护者确认的行为）：

- `wayland` → 默认 CSD；**除非 compositor 通过 KDE server-decoration 协议要求 SSD**
- `X11` → 交给窗口管理器
- **设置了自定义 titlebar（`gtk_header_bar_new()` + `gtk_window_set_titlebar()`）→ 一定是 CSD**
- GNOME/Mutter 不实现 KDE 协议 → GNOME 下自然是 CSD
- KWin 实现该协议 → KDE Wayland 下不设 titlebar 就是原生 SSD

因此：

| 形态 | 触发条件 | 外观 |
|---|---|---|
| SSD | 不调用 `gtk_window_set_titlebar()` | KWin/X11 WM 画标题栏、按钮、阴影；跟随 KDE 主题 |
| CSD | 设置 `GtkHeaderBar` | GTK 自己画，GNOME 风格 |

配置项 `decorations = auto | ssd | csd`（默认 `auto`），在**创建窗口之前**解析：

```
auto:
  X11                          -> ssd
  Wayland + XDG_CURRENT_DESKTOP 含 KDE -> ssd
  Wayland + GNOME/Budgie/Pantheon      -> csd
  其它                          -> ssd
```

切换需要重启（写进设置页提示，或切换后提示"重启生效"）。
不要用 `GTK_CSD=0`、不要用 `gtk_window_set_decorated(FALSE)` 来"要 SSD"——后者是去掉所有装饰，只用于全屏。

### D4：UI 路线 = 原生 GTK4（推荐），libadwaita 作为可选项

| | A. 原生 GTK4（推荐） | B. libadwaita |
|---|---|---|
| 依赖 | 仅 GTK4 | + libadwaita ≥ 1.4 |
| 外观 | 现代但中性；KDE 下跟系统 | GNOME 风格最强 |
| SSD | 容易（不设 titlebar） | 困难：Adw 设计上强绑定 CSD |
| 发行版覆盖 | 与 GTK 4.10 一致 | 多一层版本约束 |
| 适用 | 跨桌面、KDE 优先 | 只面向 GNOME |

**结论**：选 A。`GtkHeaderBar`、`GtkPopoverMenuBar`、`GtkShortcutsWindow`、`GtkAboutDialog` 都在 GTK4 核心里，足以做出干净现代的界面；不需要为了圆角付出 libadwaita 的耦合代价。

**状态**：已确认采用 A（2026-11-20）。

### D5：渲染 = 自定义 `GtkWidget` + `GdkTexture`

- 默认路径：CPU 帧缓冲 → `GdkMemoryTexture` → `snapshot()` 里 `gtk_snapshot_append_texture()`
- 缩放/旋转/间隙由 GSK 在 GPU 上完成
- Cairo 光栅路径保留为 `presentation=cairo` 回退项（用于需要严格 nearest 过滤或 GSK 异常时）
- wgpu：见 §6.4，分阶段处理；**P1–P5 期间隐藏 GPU 呈现选项**，P7 必须补齐 GPU 滤镜（离屏渲染 + 回读），不是可选功能。

### D6：列表控件

- GTK4 没有 `GtkTreeView` / `GtkIconView` / `GtkComboBox`（GTK5 更是把 cell renderer 整套移除），**不存在“先保留 TreeView”的过渡期**；
  P4 直接换成 `GtkColumnView` + `GListModel`，下拉用 `GtkDropDown`（见 §4.10）
- `utilsGTK.cpp` 的自定义 cell renderer 属于 `GtkTreeView` 生态；迁到 `ColumnView` 时这 400 行编辑器整体删掉，换成 `GtkEntry`/`GtkText` 委托（GTK4 没有 TreeView，没有保留选项）

### D7：对话框 = 全面异步

44 处 `gtk_dialog_run` 全部改成"上下文结构体 + 回调"。这是本方案**唯一无法回避的大规模机械改动**，也是必须提前统一的模式（§4.6）。

---

## 4. 目标架构

### 4.1 构建布局

`gtk4/meson.build`（P0 实际内容，与 `gtk/meson.build` 的差异）：

```meson
dep_gtk4 = dependency('gtk4', version: '>=4.10')

# `dependencies` 在 posix/meson.build:503 已经加过 dep_x11，这里不再重复
gtk_dependencies = dependencies + [dep_gtk4]
gtk4_args = []
# 后端特定头文件（gdk/wayland/gdkwayland.h、gdk/x11/gdkx.h），可选
dep_gtk4_wayland = dependency('gtk4-wayland', required: false)
if dep_gtk4_wayland.found()
  gtk_dependencies += [dep_gtk4_wayland]
  gtk4_args += ['-DHAVE_GTK4_WAYLAND']
endif
dep_gtk4_x11 = dependency('gtk4-x11', required: false)
if dep_gtk4_x11.found()
  gtk_dependencies += [dep_gtk4_x11]
  gtk4_args += ['-DHAVE_GTK4_X11']
endif

executable('desmume',
  desmume_src,
  dependencies: gtk_dependencies,
  include_directories: includes,
  link_with: libdesmume,
  cpp_args: gtk4_args,
  install: true,
)
```

两个可选后端依赖缺失时，装饰探测回退到 `G_TYPE_NAME` 前缀判断（`gdk_display_get_default()` 的类型名），不会让构建失败。

`posix/meson.build` 末尾（前端的自动发现）：

```meson
gtk_frontend = get_option('frontend-gtk')

dep_gtk2 = dependency('gtk+-2.0', version: '>=2.24', required: false)
dep_gtk3 = dependency('gtk+-3.0', version: '>=3.24', required: false)
dep_gtk4 = dependency('gtk4',     version: '>=4.10', required: false)

if gtk_frontend == 'auto'
  if dep_gtk4.found()
    gtk_frontend = 'gtk4'
  elif dep_gtk3.found()
    gtk_frontend = 'gtk3'
  elif dep_gtk2.found()
    gtk_frontend = 'gtk2'
  else
    error('No GTK development files found. ...')
  endif
endif

message('GTK frontend: @0@'.format(gtk_frontend))

if gtk_frontend == 'gtk2'
  subdir('gtk2')
elif gtk_frontend == 'gtk3'
  subdir('gtk')
elif gtk_frontend == 'gtk4'
  subdir('gtk4')
endif
```

探测用的是 `required: false`，子目录里再声明一次 `dependency()` 会命中 meson 的缓存，不重复调用 pkg-config；
被强制指定的版本缺依赖时在 `subdir()` 之前就 `error()` 报出人话，而不是让子目录抛裸依赖错误。

安装项（仅 GTK4 版）：

```
desmume
org.desmume.DeSmuME.GTK4.desktop   (Exec=desmume, StartupWMClass=org.desmume.DeSmuME.GTK4)
org.desmume.DeSmuME.metainfo.xml   (若 app-id 变化，需要 id 一致的副本)
org.desmume.DeSmuME.svg
doc/desmume.1                      (与 GTK3 同名，P8 决定是否合并)
```

### 4.2 窗口外壳与 SSD

主窗口内容树只写一次，两种外壳共用：

```
GtkApplicationWindow (title="DeSmuME", app-id=org.desmume.DeSmuME.GTK4)
└── GtkBox(vertical)
    ├── [CSD only] GtkHeaderBar        # 仅 csd 模式挂到 set_titlebar()
    ├── GtkPopoverMenuBar              # 内容区里的显式 PopoverMenuBar（§4.2 #7）
    ├── GtkBox#toolbar                 # 图标按钮：打开/运行/暂停/重置/全屏
    ├── DesmumeDisplay                 # 视频区，hexpand/vexpand
    └── GtkBox#statusbar               # GtkLabel + 右侧 FPS
```

关键代码：

```c
GtkWidget *win = gtk_application_window_new(app);
gtk_window_set_title(GTK_WINDOW(win), "DeSmuME");
gtk_window_set_child(GTK_WINDOW(win), root);

if (use_csd) {
    GtkWidget *hb = gtk_header_bar_new();
    gtk_header_bar_set_title_widget(GTK_HEADER_BAR(hb), title_label);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(hb), open_button);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(hb), menu_button);
    gtk_window_set_titlebar(GTK_WINDOW(win), hb);   /* 变成 CSD，不可逆 */
}
/* SSD 路径：什么都不做 */
```

细节与陷阱：

1. **必须在 `gtk_window_present()` 之前决定**。窗口 map 后再 `set_titlebar()` 只会在 CSD 内部换标题栏，不会让 KWin 接管。
2. **SSD 下不要显示副标题/自绘标题**，否则出现"两个标题"。ROM 名放到状态栏或窗口 `title`（KWin 会显示）。
3. **全屏**：`gtk_window_fullscreen()`；SSD 由 compositor 自动隐藏。恢复时不要 `gtk_window_move()`（Wayland 无效）。
4. **最大化/平铺**由 KWin 负责；`GtkWindow:maximized` 仍可读，用于保存状态。
5. **窗口几何持久化**：Wayland 无法读取窗口位置。`ramWatch` 里的 `gtk_window_get_position/move` 直接删掉，只保留尺寸（`gtk_window_get_default_size` / 控件 `get_width`），位置交给 compositor。
6. **app-id 与桌面文件必须匹配**（Wayland 用 app_id 关联图标/窗口规则，X11 用 `WM_CLASS`）。两处都写成 `org.desmume.DeSmuME.GTK4`。
7. **菜单栏不进 `show-menubar`**：P0 用 `gtk_application_window_set_show_menubar(FALSE)` + 内容区里显式的
   `gtk_popover_menu_bar_new_from_model()`。原因是 `GtkApplicationWindow:show-menubar` 在 CSD 下会把菜单塞进
   header    bar，两套外壳的布局就会分叉；显式 PopoverMenuBar 让 SSD/CSD 共用同一棵内容树，菜单永远贴在 WM 标题栏下方
   （见 §0.2）。P1 已从 `gtk4/menu.ui` 加载模型替换手写的 `BuildMenuModel()`：只列已接线的动作，
   每阶段再把新实现的条目加进去，避免出现一排灰掉的菜单项。
8. 工具窗口（RAM Watch 等）用普通 `GtkWindow`，也**不要设 titlebar**，让它们同样获得系统装饰；父窗口用 `gtk_window_set_transient_for()`。

**验收**：KDE Wayland 下标题栏由 KWin 绘制、可使用 KDE 窗口规则/平铺/阴影；GNOME Wayland 下是 `GtkHeaderBar`；X11 下由 WM 装饰；三种情况下内容区布局不破。

### 4.3 视频区：`DesmumeDisplay`

新增文件 `gtk4/display.cpp` / `display.h`，替代 `ExposeDrawingArea` + `WGPU_PresentFrame` 的 GTK 部分。

```c
struct _DesmumeDisplay { GtkWidget parent; GdkTexture *texture; ... };

static void desmume_display_snapshot(GtkWidget *w, GtkSnapshot *snap)
{
    /* 1. 黑底 */
    gtk_snapshot_append_color(snap, &black, &rect);

    /* 2. 旋转 + 缩放矩阵（与 GTK3 的 cairo_matrix 等价） */
    gtk_snapshot_save(snap);
    gtk_snapshot_translate(snap, &GRAPHENE_POINT_INIT(cx, cy));
    gtk_snapshot_rotate(snap, angle);
    gtk_snapshot_scale(snap, ratio, ratio);
    gtk_snapshot_translate(snap, &GRAPHENE_POINT_INIT(-lw / 2, -lh / 2));

    /* 3. 两屏 + 间隙 */
    gtk_snapshot_append_texture(snap, texture_main, &main_rect);
    gtk_snapshot_append_texture(snap, texture_sub,  &sub_rect);
    if (gap_visible)
        gtk_snapshot_append_color(snap, &gap_color, &gap_rect);

    /* 4. HUD（如果用 GPU OSD，这里是第二个 texture） */
    gtk_snapshot_pop(snap);
}
```

实现要点：

- 纹理格式：`GDK_MEMORY_B8G8R8A8_PREMULTIPLIED`。
  帧缓冲内存序是 B,G,R,X——这一点由 GTK3 现有代码反证：`drawScreen()` 直接把它交给
  `cairo_image_surface_create_for_data(..., CAIRO_FORMAT_RGB24, ...)`，而 cairo 在小端机上
  的 RGB24 就是 BGRX。alpha 恒为 0xFF，所以预乘格式与直通格式等价。
- 两块屏共用一个 BGRA 缓冲（上下各一半）。不要复制：用
  `g_bytes_new_from_bytes(parent, offset, length)` 从同一个 `GBytes` 切出上下半屏，
  再各自 `gdk_memory_texture_new()`；子 `GBytes` 共享父内存，零拷贝。
- 父 `GBytes` 用 `g_bytes_new_with_free_func()` 包住双缓冲帧，纹理上传后旧缓冲下一帧才能复用。
- 尺寸：用 `gtk_widget_get_width/height`，不要再用 `configure_event`。
- 重绘：`gtk_widget_queue_draw()`；缩放因子用 `gtk_widget_get_scale_factor()`。
- **过滤方式差异**：GSK 的纹理节点只做线性过滤，没有 Cairo 的 `CAIRO_FILTER_NEAREST`。整数倍 + 90° 旋转时两者像素等价；非整数倍时线性更好看但与 GTK3 有差异。需要严格 nearest 时用 `presentation=cairo` 回退路径。
- 需要"点对点"或自定义 shader 时（upscaler），才走 wgpu。

### 4.4 输入

| GTK3 | GTK4 |
|---|---|
| `"button_press_event"` + `GdkEventButton` | `GtkGestureClick`（`pressed` 信号，`gtk_gesture_single_get_current_button`） |
| `"button_release_event"` | 同一个 `GtkGestureClick` 的 `released` |
| `"motion_notify_event"` + `GdkEventMotion` | `GtkEventControllerMotion`（`motion` 信号） |
| `"key_press_event"` + `GdkEventKey` | `GtkEventControllerKey` |
| `"configure_event"` | 删除；用 `GtkWidget::size-allocate` 或 `get_width/height` |
| `"draw"` | `snapshot()` 虚函数或 `gtk_drawing_area_set_draw_func` |
| `gtk_menu_popup_at_pointer` | `GtkPopoverMenu` + `gtk_popover_set_pointing_to()` + `popup()` |
| `gtk_widget_set_events` | 删除，控制器自动拿事件 |

触控笔优先用 `GtkGestureStylus`（信号是 `down/up/motion`，坐标用 `gtk_gesture_stylus_get_axis()`），鼠标/触摸回退到 `GtkGestureClick`。`DesmumeDisplay` 已按 GTK3 的矩阵链把 widget 坐标映射成 NDS 逻辑坐标（0..255 / 0..191，含 hybrid 大屏的额外变换），P3 直接交给 `NDS_setTouchPos()` / HUD 编辑器即可（P2b 已落地，见 §14）。

右键菜单：`GtkPopoverMenu` 从 `GMenu` 构建，`gtk_popover_set_parent()` 后 `popup()`；不再需要 `GtkMenu`。

### 4.5 菜单与动作

- `menu.ui` 原样复制（GMenu XML 在 GTK4 完全有效）
- `gtk_application_set_menubar()` / `gtk_application_set_accels_for_action()` 不变
- Lua 动态菜单（`GtkLua_MenuRefreshTimer`）改的只是 `GMenu` 的 append/remove，继续可用
- 右键上下文菜单从 `GtkMenu` 改为 `GtkPopoverMenu`（`GtkPopoverMenu` 接受 `GMenuModel`，`gtk_popover_menu_new_from_model()`）
- 新增"键盘快捷键"窗口：`GtkShortcutsWindow`，内容从一个小的 `builder` XML 或手写 section 生成

### 4.6 对话框与异步模式

统一模式（`gtk4/dialogs.h`）：

```c
typedef struct {
    GtkWindow *parent;
    /* 业务字段：路径、选项、回调 */
    void (*done)(MyCtx *ctx, gboolean accepted);
} MyCtx;

static void on_file_selected(GObject *src, GAsyncResult *res, gpointer data)
{
    MyCtx *ctx = data;
    GFile *file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(src), res, NULL);
    ...
    ctx->done(ctx, file != NULL);
}
```

对照表：

| GTK3 | GTK4（4.10+） |
|---|---|
| `gtk_file_chooser_native_new` + `gtk_native_dialog_run` | `gtk_file_dialog_new()` + `gtk_file_dialog_open/save/open_multiple()` |
| `gtk_message_dialog_new` + `gtk_dialog_run` | `gtk_alert_dialog_new()` + `gtk_alert_dialog_choose()` |
| `gtk_dialog_run(GTK_DIALOG(...))` | `GtkWindow` + 异步 `response` 风格回调 |
| `GtkDialog` + action area | `GtkWindow` + `GtkHeaderBar`（或底部按钮行） |
| `gtk_dialog_get_content_area()` | `gtk_window_set_child()` |
| 模态阻塞等待结果 | 结果放进 context，主循环不阻塞 |

**注意事项**：
- 现有代码大量依赖"`gtk_dialog_run` 返回后继续用栈变量"。改成异步后，这些变量必须移进 context，且要防止父窗口在回调前销毁（`g_object_add_weak_pointer` 或 `GCancellable`）。
- 现有 `ramSearch` 的注释明确说"alert runs a nested loop; the frame tick keeps firing"。异步化后模拟循环继续正常跑，语义更好，但要保证"自动搜索暂停"标志仍能阻止重入。
- 文件过滤器：`GtkFileFilter` 在 GTK4 仍可用；`gtk_file_dialog_set_filters()` 接收 `GListStore`。

### 4.7 工具窗口框架

`dTool.h` / `dToolsList.cpp` 的 `open/update/close` 结构不变，只改实现：

- `gtk_window_new(GTK_WINDOW_TOPLEVEL)` 保留；`gtk_window_set_child()`
- `"delete-event"` → `"close-request"`（返回 `TRUE` 阻止关闭）
- `"destroy"` 仍在，但建议统一用 `close-request` + 显式清理
- 工具窗口也要走 §4.2 的"不设 titlebar"策略
- `update()` 由主循环每帧调用；GTK4 下如果窗口 `!gtk_widget_get_mapped()`，直接返回，省掉无谓的模型更新

### 4.8 Lua 控制台与主循环

- `LuaPumpEvents()` 里的 `gtk_events_pending()` / `gtk_main_iteration()` → `g_main_context_pending(NULL)` / `g_main_context_iteration(NULL, FALSE)`
- 但要加**重入保护**：Lua 脚本可能触发菜单动作，而动作又回到 Lua。GTK3 版本靠 `gtk_main_iteration` 的语义勉强工作；GTK4 下建议只在"模拟帧之间"泵一次，并加 `static bool in_pump`。
- Lua 控制台是独立 `GtkWindow`（不注册到 `GtkApplication`），保持现状；注意主窗口关闭时 `GtkApplication` 退出行为需要显式测试。

### 4.9 配置

- `config.cpp` / `config_opts.h` 原样复制
- 新增两个键：`decorations`（`auto|ssd|csd`）、`presentation`（`auto|texture|cairo|wgpu`）
- 保持配置文件路径与键名兼容，GTK3/GTK4 可共用同一份配置（新增键有默认值）

### 4.10 面向 GTK 5：迁移注意事项（2026-11-21）

依据 GTK 官方《Preparing for GTK 5》（https://docs.gtk.org/gtk4/migrating-4to5.html）。
GTK 5 会同时破坏 API 和 ABI，因此**这次 GTK4 迁移就按 GTK5 的约束写**，不引入任何"能在 4.x 跑、5.x 再改"的代码。
总原则：不使用任何带弃用标记的符号；`-Wall` 下 GTK4 的弃用警告视为待修项，不累积。

| 官方注意事项 | 本项目的执行规则 | 当前状态 |
|---|---|---|
| Cell renderer 类 widget 全部移除 | 禁止 `GtkTreeView` / `GtkIconView` / `GtkComboBox`（GTK4 本身就没有 TreeView）；P4 的 cheats、RAM Watch/Search、IO 寄存器一律 `GtkColumnView` + `GListModel`，下拉用 `GtkDropDown`，无列需求时用 `GtkListView`/`GtkGridView`；`utilsGTK.cpp` 的 cell renderer 编辑器直接删，改 `GtkEntry`/`GtkText` 委托 | 方案已收紧，见 §5.3/§8（原“先保留 TreeView”的过渡写法已删除） |
| 主题渲染 API 与 `GtkSnapshot` 渲染变体移除（`gtk_render_*`、`gtk_snapshot_render_*`、`gtk_style_context_save()`） | 不在自定义 widget 里模仿 GTK 控件外观；文本/图标/边框用子 widget（如 `GtkLabel`），自绘图表只准用 `gtk_widget_get_color()` 取当前主题前景色 | `DesmumeDisplay` 只画黑底/间隙/纹理，无主题渲染调用 |
| 局部样式表移除 | 只用全局 `gtk_style_context_add_provider_for_display()` + 语义 CSS class；禁止 `gtk_style_context_add_provider()` | P1 已是全局 provider |
| `@define-color` 移除 | 用 `:root { --name: … }` + `var(--name)` | 未使用 `@define-color` |
| `mix()` / `alpha()` / `shade()` / `lighter()` / `darker()` 移除 | 用 `color-mix()`、`rgb(from … / calc(alpha * …))`、`hsl(from …)`；`style.css` 的 toast 背景已改成标准写法，GTK 4.10 用第二条 `alpha()` 作解析器回退（GTK5 会忽略它） | 已改；P6 提高底线后可删回退行 |
| Chooser 接口及其 dialog/button 实现移除 | 只用 `GtkFileDialog` / `GtkColorDialog` / `GtkFontDialog`（4.10+），按钮变体用 `GtkFileDialogButton` 等；禁止 `GtkFileChooser*` / `GtkColorChooser*` / `GtkFontChooser*` / `GtkAppChooser*` | P0 `GtkAlertDialog` 已按此写；P3 方案已是 async 族 |
| `GtkMessageDialog` → `GtkAlertDialog` | 44 处 `gtk_dialog_run()` 全部走 `GtkAlertDialog`，不用 `GtkMessageDialog` | 方案已定，P3 执行 |
| `GtkDialog` 移除（`gtk_dialog_run()` 已不在） | 设置/工具窗口用普通 `GtkWindow` + 自放按钮，不用 action area / internal child | P1 的 About/Shortcuts 已是普通窗口 |
| `GtkInfoBar` 移除 | 瞬时消息用 `GtkRevealer`（必要时包在 `GtkOverlay` 里），不用 `GtkInfoBar` | §5.4 的 toast 改为 Revealer |
| `gtk_show_uri()` → `GtkUriLauncher` / `GtkFileLauncher` | 打开网站用 `GtkUriLauncher`，打开 ROM/目录用 `GtkFileLauncher`；About 的 website 链接由 `GtkAboutDialog` 自己处理 | 无 `gtk_show_uri` 调用；P3 文件动作按此 |
| `GtkStatusbar` 移除 | 状态行用 `GtkBox` + `GtkLabel`（§5.4） | P0 已是 Box + Label |
| `GtkLockButton` / `GtkVolumeButton` 移除 | 需要时由应用自己用普通 `GtkToggleButton`/`GtkScale` 实现，不引入这两个 widget | 未使用 |
| 尺寸 API 变更 | 只用 `gtk_widget_get_width()` / `get_height()`（**内容区**，不是 GTK3 的 CSS border 区）；需要 border 区用 `gtk_widget_compute_bounds()`；baseline 用 `gtk_widget_get_baseline()`；禁止 `get_allocated_*` / `get_allocation()` | `display.cpp` 已用 `get_width/height`，并在 `map_touch()` 里按内容区坐标反算 |
| 停止使用 `GdkPixbuf` | 图像传输一律 `GdkTexture` / `GdkPaintable`；图标用 icon-name + `GtkImage`，不让 GTK 手工缩放 Pixbuf | P2 已是 `GdkMemoryTexture`；不复制 GTK3 的 Pixbuf 缩放代码 |

配套改动：§5.3 的 IO 寄存器行、§8 的四个工具行、§7 的风险 R10 不再允许 TreeView 过渡；
§5.4 的 toast 用 `GtkRevealer`；`style.css` 按标准 CSS 写并在末尾保留 GTK 4.10 回退。

---

## 5. UI 现代化方案

### 5.1 设计原则

1. **装饰交给平台**：不在窗口里重复画标题；SSD 下内容从菜单栏开始。
2. **视频优先**：工具条/状态栏可一键隐藏；全屏时全部隐藏。
3. **少而清晰**：工具条只保留高频动作（打开、运行/暂停、重置、全屏），其余进菜单。
4. **系统主题优先**：只加少量语义 CSS，不硬编码颜色（视频区除外）。
5. **可发现性**：右键菜单、快捷键窗口、搜索框；不靠记忆。

### 5.2 主窗口布局

SSD（KDE / X11）：

```
┌─ KWin 绘制：DeSmuME — pokemon.nds ─────────────────┐
│ 文件  模拟  视图  工具  帮助                        │  ← GtkPopoverMenuBar
│ [打开] [运行] [重置] [⏭]                    [⛶]    │  ← 工具条（可隐藏）
│ ┌───────────────────────────────────────────────┐ │
│ │                                               │ │
│ │              视频区（纯黑，等比）              │ │
│ │                                               │ │
│ └───────────────────────────────────────────────┘ │
│ Running ...                              FPS 60.0 │  ← 状态行
└───────────────────────────────────────────────────┘
```

CSD（GNOME）：

```
┌───────────────────────────────────────────────────┐
│ [打开]      DeSmuME            [☰]  [⛶]           │  ← GtkHeaderBar
│ 文件  模拟  视图  工具  帮助                        │  ← 可选，config.view_menu
│ ┌───────────────────────────────────────────────┐ │
│ │              视频区                            │ │
│ └───────────────────────────────────────────────┘ │
│ Running ...                              FPS 60.0 │
└───────────────────────────────────────────────────┘
```

两套外壳共用同一个 `root` 内容树；切壳只影响 `set_titlebar()` 和第 2 行菜单栏是否显示。

### 5.3 逐窗口改造

| 窗口 | 现状 | GTK4 目标 |
|---|---|---|
| 模拟设置 | Glade `GtkDialog` + `GtkButtonBox` | `GtkWindow` + 分组 `GtkListBox` 行（开关/数值），底部 `取消/确定`；不再用 action area |
| 图形设置 | Glade `GtkDialog` + 多个 `GtkComboBoxText` | 同上；`GtkDropDown`，实时预览缩放结果（可选） |
| 金手指 | TreeView + `GtkMenuButton`+`GtkMenu` | `GtkColumnView`（启用开关列、类型、代码、描述）+ `GtkMenuButton`+`GMenu` + 搜索框 |
| Movie Play/Record | 表单 + `GtkFileChooserNative` | 紧凑表单 + `GtkFileDialog`；错误用 `GtkAlertDialog` |
| RAM Watch | TreeView + DnD + 文件选择 | `GtkColumnView` + `GtkDropTarget` + 右键 `GtkPopoverMenu` |
| RAM Search | TreeView + 大量 ListStore | `GtkColumnView` + 懒加载 `GListModel`；顶部 `GtkSearchBar` |
| IO 寄存器 | 只读 TreeView | `GtkColumnView`（只读），无 TreeView 回退 |
| Lua 控制台 | TextView + 按钮行 | TextView（等宽 CSS）+ 紧凑按钮行 + `GtkDropTarget` |
| 关于 | `gtk_show_about_dialog` | `GtkAboutDialog` 实例 + `gtk_window_present()` |
| 快捷键 | 无 | 新增 `GtkShortcutsWindow` |

### 5.4 状态与反馈

- 状态行：`GtkLabel`（`ellipsize=end`）+ 右侧 FPS/速度标签；只在文本变化时 `gtk_label_set_text()`
- 瞬时消息：`GtkRevealer`（必要时放在 `GtkOverlay` 底部）包一个 `GtkLabel`（"已保存到 ..."、"录像已开始"），3 秒后 reveal=FALSE 淡出；不用 `GtkInfoBar`，不引入 libadwaita 的 toast
- 错误：`GtkAlertDialog`（4.10+），带"详情"展开
- 忙碌：文件对话框/加载 ROM 时用 `GtkSpinner`，不阻塞主循环

### 5.5 CSS / 主题 / HiDPI

一份 `style.css` 打进 gresource：

```css
.desmume-display { background-color: #000; }
.desmume-status  { font-size: 0.9em; opacity: 0.72; padding: 2px 8px; }
.desmume-toast   { background-color: color-mix(in srgb, currentColor 12%, transparent);
                   /* GTK 4.10 fallback; GTK 5 ignores this declaration */
                   background-color: alpha(currentColor, 0.12);
                   border-radius: 8px; padding: 8px 14px; }
```

- 用 `gtk_css_provider_load_from_data()`（4.10 可用；4.12 起有 `load_from_string`）
- 不再用 `gtk_widget_override_background_color` / `override_color`；视频区黑底靠 CSS class
- 深色模式：视频区永远黑；其余跟随系统。不要强行设 `prefer-dark`
- HiDPI：所有尺寸用逻辑像素；图标用 `GtkImage` + icon-name，让 GTK 按 scale 取图；不要再用 GdkPixbuf 手工缩放

### 5.6 可访问性

- 所有图标按钮加 `gtk_accessible_update_property(..., GTK_ACCESSIBLE_PROPERTY_LABEL, ...)`
- 工具条按钮带 tooltip + accel
- 表格列有可读标题；`GtkColumnView` 自带更好的 a11y 语义（相对 cell renderer）
- 键盘可达：菜单、对话框、工具窗口的 Tab 顺序；全屏退出用 `Esc`

---

## 6. 性能方案

### 6.1 呈现路径对比

| 路径 | 每帧工作 | CPU | 备注 |
|---|---|---|---|
| GTK3 Cairo（回退路径） | CPU 缩放 + 合成 | 高 | `cairo_scale` + bilinear，大窗口很贵 |
| GTK3 wgpu（默认） | 上传 BGRA → GPU 缩放/滤镜 → 直接 present | 低 | 依赖 drawing area 自己的 `GdkWindow` |
| **GTK4 texture（推荐默认）** | 上传 BGRA → GSK GPU 缩放 → 合成到 toplevel | 低 | 多一次合成 pass；无原生窗口依赖 |
| GTK4 cairo | 同 GTK3 Cairo，但走 `snapshot` | 高 | 只在需要 closest 过滤时用 |
| GTK4 wgpu offscreen | GPU 渲染/滤镜 → 回读 → texture | 中高 | 回读成本随分辨率线性增长 |

结论：**默认走 texture 路径**，它在"CPU 成本"和"可移植性"之间最优；wgpu 只在需要 GPU 滤镜时启用。

### 6.2 具体优化项

1. **GPU 缩放代替 CPU 缩放**（最大收益）
   GTK3 Cairo 在大窗口下每帧对 393 KB 帧做 CPU bilinear；GTK4 texture 路径把这一步交给 GSK。
   目标：1280×960 窗口下呈现 CPU 降低 ≥ 50%。

2. **不可见时停止呈现**
   `!gtk_widget_get_mapped()` 时不建纹理、不 `queue_draw`；模拟继续跑（音频不断）。
   可选：监听 `GtkWindow::notify::visible` / `is-active`，非活动时降低呈现频率。

3. **纹理双缓冲 + 零拷贝 GBytes**
   避免每帧 `gdk_memory_texture_new()` 内部再复制一次；用 `g_bytes_new_with_free_func()` 管理两块帧缓冲。

4. **resize / 全屏不再走 `configure_event`**
   GTK3 每次 configure 都重算矩阵并可能触发重新分配；GTK4 单 surface，`snapshot()` 里按当前尺寸即时计算，天然无 configure 风暴。
   目标：resize 过程无闪烁、无黑边。

5. **列表模型**
   - `GtkColumnView` 只创建可见行，滚动更平滑
   - RAM Search：结果集本身在 `RAMSearchCore`，`update()` 已按 150/900 ms 节流；**GTK4 不会让 `SignalNewFrame()` 变快**，不要把"搜索速度"写进性能承诺
   - 若要做，单独优化核心（SIMD/并行），与 GTK4 迁移解耦

6. **状态栏去抖**
   现有 `UpdateStatusBar` 只在事件时调用，已经够好；GTK4 下继续保持"文本不变不 set"。

7. **启动**
   gresource 体积不变；`menu.ui` 39 KB 解析一次；不要把工具窗口在启动时构建（现状是懒加载，保持）。目标 ≤ +50 ms。

### 6.3 测量方法

- ✅ **已实现**：`DesmumeDisplay::push_frame()` 的纹理上传与 `snapshot()` 的 GSK 合成各自计时，`DESMUME_GTK4_TIMING=1` 时每 120 个呈现帧向 stderr 打印一次均值/p50/p95/max（默认关闭，热路径零时钟读取）；详见 §14 P6 小节
- `perf record -g ./desmume`，看 `cairo_*`、`gsk_*`、`gdk_texture_*` 占比
- `hyperfine --warmup 3 './desmume rom.nds'`（配合自动退出参数）
- resize：`xdotool`/`kdotool` 脚本循环改窗口大小，记录帧间隔
- 内存：`smem -P desmume`、`valgrind --tool=massif`（短会话）

### 6.4 wgpu 路径（分阶段，风险最高）

GTK4 里 `GtkDrawingArea` 不再有独立 `GdkWindow`，`WGPU_PresentFrame()` 的 `gdk_x11_window_get_xid()` / `gdk_wayland_window_get_wl_surface()` 路径失效（只能拿到整个 toplevel 的 surface，直接 present 会盖住菜单栏）。

处理顺序：

- **P1–P6**：`presentation=texture` 为默认；配置里的 GPU 呈现选项置灰并提示"GTK4 版暂不可用"
- **P7a（已落地）**：wgpu 离屏渲染 + 回读
  - `WGPUPresent` 增加 `beginOffscreen()` / `presentOffscreen()`：`present()` 的渲染主体把目标从 surface 换成自建 BGRA8Unorm 纹理（RENDER_ATTACHMENT | COPY_SRC），滤镜链和 HUD overlay 完全复用
  - 渲染后 `wgpuCommandEncoderCopyTextureToBuffer` → `wgpuBufferMapAsync`（AllowProcessEvents）+ `wgpuDevicePoll` 同步等待 → 按 256 字节对齐的行距解开到调用者紧密排列的 BGRA 缓冲，回到 GSK 的 `GdkMemoryTexture` 路径
  - 只在"选了非 Off 的 GPU 滤镜"时启用（`config.presentationWGPU && config.gpuFilter`），默认 Off 保持纯 texture 路径；回读缓冲按需增长，尺寸变化重建离屏 target
  - 无 wgpu 构建下这些方法都是 stub、`beginOffscreen()` 返回 false，自动回退到未滤镜帧
- **P7b（可选/实验，未做）**：原生子 surface
  - X11：在 toplevel XID 上 `XCreateWindow` 子窗口覆盖视频区
  - Wayland：`gdk_wayland_surface_get_wl_surface()` + `wl_subcompositor` 子 surface（现有 `WGPULinuxDisplay::beginWayland` 已是这个思路）
  - 定位用 `gtk_widget_compute_point(widget, toplevel, ...)` × scale
  - 明确标注"实验性"：GSK 不知道这个窗口，堆叠/裁剪/全屏切换都可能出问题
- **P7c（已落地，2026-11-21）**：3D 渲染后端 + Screen presentation 下拉框，对齐 GTK3 图形设置
  - `core3DList` 由写死的 Null/SoftRasterizer 改为动态构建：加入 `WGPURender_GetBackendInterface(i)` 枚举的各 WebGPU 后端，以及 `ENABLE_OPENGL_*` 下的 OpenGL 后端；无 adapter 时回退 `gpu3DWGPU`，无 `ENABLE_WGPU` 时不列 stub。RendererID 仍是数组下标，Null=0 / SoftRasterizer=1 保持稳定
  - Config 图形页新增 "Presentation:" 下拉框（GTK3 `Screen presentation` 的对应物）：每行一个 WebGPU 后端（掩码 `WGPUInstanceBackend`）+ 末行 "Software (Cairo)"；选软件行时 GPU filter 置灰并强制 Off
  - 后端掩码经 `desmume_emu_set_presentation_backend()` 传入 `WGPUPresent::beginOffscreen(w, h, mask)`，切换时重建设备，真正落地而非摆设
  - GPU OSD（`presentationWGPUHud`）未做：GTK4 前端没有整套 AGG HUD 绘制管线，列为后续独立任务

### 6.5 不会变快的部分（写清楚，避免预期落空）

- 模拟器核心（CPU/GPU 模拟、JIT）——与前端无关
- RAM Search 的候选扫描——瓶颈在 `RAMSearchCore::SignalNewFrame()`
- 游戏兼容性、音频同步
- 启动时间大概率略增（GTK4 依赖更多）

---

## 7. 分阶段实施

| 阶段 | 内容 | 交付 | 验收 | 估时 |
|---|---|---|---|---|
| **P0** | 目录、meson（gtk4→gtk3→gtk2 自动发现）、CI、空窗口 | `-Dfrontend-gtk=gtk4` 与 `auto` 都能构建；`desmume` 启动显示空窗口；GTK3 不受影响 | 两种选法分别构建成功（一次配置只出一个 GTK 前端） | 2–3 天 |
| **P1** | 窗口外壳 + 装饰策略 | SSD/CSD 自适应；菜单栏、工具条、状态行；关于/快捷键；CSS | KDE Wayland 是 KWin 装饰；GNOME 是 header bar；X11 正常 | 1 周 |
| **P2** | 视频区 + 输入 | `DesmumeDisplay`；纹理呈现；旋转/缩放/间隙/混合布局；手势；全屏 | 与 GTK3 Cairo 路径逐布局像素/坐标对齐 | 2 周 |
| **P3** | 动作、对话框、文件选择 | `main.cpp` 25 处异步化；`GtkFileDialog`；最近文件；录制/截图/存档 | 所有菜单动作可用；无嵌套主循环 | 2–3 周 |
| **P4** | 工具窗口 | ramWatch/ramSearch/moviePlay/movieRecord/ioregs/luaScriptConsole；DnD | 每个工具可开/关/更新；运行中不崩 | 3–4 周 |
| **P5** | 金手指 | 列表、编辑、启用/禁用、导入导出 | 与 GTK3 版行为一致 | 1–2 周 |
| **P6** | 性能与打磨 | 停绘、双缓冲、ColumnView、测量报告 | 达到 §1.3 指标 | 1–2 周 |
| **P7** | **wgpu（必做，最后补齐）** | 离屏 + 回读；滤镜对齐；GPU 选项恢复 | 滤镜输出与 GTK3 wgpu 一致（或明确差异） | 2–4 周 |
| **P8** | 收尾 | 文档、桌面文件、man、CI 矩阵、弃用清理 | 发布检查表全过 | 1 周 |

总计约 **12–18 周**（单人、熟悉 GTK 的前提下）。P0–P3 完成即可日常使用；P4–P5 决定能否替代 GTK3；P7 不补齐则 GPU 滤镜功能缺位，不能宣布完全对齐。

**每个阶段结束都必须**：
- GTK3 target 仍能构建、启动、跑 ROM（防止误改共享文件）
- `gtk4` 分支可构建，无新增编译警告
- 更新本文档的"已完成/遗留"状态

---

## 8. GTK3 → GTK4 API 映射表

### 8.1 容器与布局

| GTK3 | GTK4 |
|---|---|
| `gtk_container_add(c, w)` | `gtk_window_set_child()` / `gtk_box_append()` / `gtk_scrolled_window_set_child()` |
| `gtk_box_pack_start(b, w, expand, fill, pad)` | `gtk_box_append()` + `gtk_widget_set_hexpand/vexpand` |
| `gtk_box_pack_end` | `gtk_box_prepend()` 或 `gtk_box_append` + 反向顺序 |
| `gtk_container_set_border_width` | CSS `margin` 或 `GtkBox` 的 `margin-*` |
| `gtk_widget_show_all()` | 删除（GTK4 默认可见）；隐藏的控件显式 `gtk_widget_set_visible(TRUE)` |
| `gtk_widget_set_margin_*` | 保留 |
| `GtkEventBox` | 删除；需要事件时用 `GtkBox` + 事件控制器 |

### 8.2 事件

| GTK3 信号/结构 | GTK4 |
|---|---|
| `"button_press_event"` / `"button_release_event"` | `GtkGestureClick::pressed/released` |
| `"motion_notify_event"` | `GtkEventControllerMotion::motion` |
| `"key_press_event"` / `"key_release_event"` | `GtkEventControllerKey` |
| `"focus_in_event"` / `"focus_out_event"` | `GtkEventControllerFocus::enter/leave` |
| `"scroll_event"` | `GtkEventControllerScroll` |
| `"configure_event"` | 删除；用尺寸查询 |
| `"delete-event"` | `GtkWindow::close-request` |
| `"destroy"` | 保留（或 `GObject::dispose`） |
| `gdk_window_get_device_position` | `gtk_gesture_get_point()` / 控制器当前事件 |
| `e->x` / `e->y` | `gtk_gesture_get_point()` / `gdk_event_get_position()` |

### 8.3 绘制与窗口

| GTK3 | GTK4 |
|---|---|
| `"draw"` + `GdkEventExpose` | `GtkWidgetClass::snapshot()` 或 `gtk_drawing_area_set_draw_func()` |
| `gdk_window_begin_draw_frame` + `GdkDrawingContext` | 删除；在 `snapshot` 里构图 |
| `gdk_window_get_clip_region` | `gdk_cairo_get_clip_rectangle()`（draw func 内） |
| `gtk_widget_get_window(w)` | `gtk_native_get_surface(gtk_widget_get_native(w))` |
| `gdk_window_get_scale_factor` | `gdk_surface_get_scale_factor()` / `gtk_widget_get_scale_factor()` |
| `gdk_x11_window_get_xid` | `gdk_x11_surface_get_xid()` |
| `gdk_wayland_window_get_wl_surface` | `gdk_wayland_surface_get_wl_surface()` |
| `gtk_window_get_position` / `gtk_window_move` | 删除（Wayland 无窗口定位） |
| `gtk_window_get_size` | `gtk_widget_get_width/height` 或 `gtk_window_get_default_size` |
| `gtk_widget_translate_coordinates` | 仍在，但跨 surface 用 `gtk_widget_compute_point()` |
| `gtk_widget_override_background_color` | CSS provider |
| `gtk_widget_override_color` | CSS provider |
| `g_object_ref_sink(gtk_window_new())` 接管 floating 引用 | GTK4 窗口引用**已非 floating**：不要再 `ref_sink`，否则多出的引用会让 `gtk_window_destroy()` 后对象不析构、`"destroy"` 信号不触发（单例窗口全局指针悬空）。见 §14 的 Config 对话框修复 |

### 8.4 菜单与动作

| GTK3 | GTK4 |
|---|---|
| `GtkMenu` / `GtkMenuItem` / `gtk_menu_shell_append` | 删除 → `GMenu` + `GtkPopoverMenu` |
| `gtk_menu_popup_at_pointer` | `gtk_popover_set_pointing_to()` + `gtk_popover_popup()` |
| `GtkMenuButton` + `gtk_menu_button_set_popup` | `gtk_menu_button_set_menu_model()`（`GMenuModel`）或 `GtkPopoverMenu` |
| `GtkToolbar` / `GtkToolButton` | 删除 → `GtkBox` + `GtkButton`（icon-name + tooltip） |
| `GtkStatusbar` | 保留但弃用 → `GtkLabel` |
| `gtk_show_about_dialog` | `GtkAboutDialog` 实例 |
| `GtkRecentChooserMenu` | 删除；继续用 `GtkRecentManager` 手工构造子菜单（现状就是这么做的） |

### 8.5 对话框与文件

| GTK3 | GTK4 |
|---|---|
| `gtk_dialog_run` / `gtk_native_dialog_run` | 删除 → 异步回调 |
| `GtkFileChooserNative` / `GtkFileChooserDialog` | `GtkFileDialog`（4.10+） |
| `GtkMessageDialog` | `GtkAlertDialog`（4.10+） |
| `gtk_dialog_get_content_area` | `gtk_window_set_child` |
| `gtk_dialog_add_button` | `GtkHeaderBar` 按钮或底部 `GtkBox` |
| `gtk_file_filter_add_pattern` | 保留 |

### 8.6 杂项

| GTK3 | GTK4 |
|---|---|
| `gtk_events_pending` / `gtk_main_iteration` | `g_main_context_pending` / `g_main_context_iteration` |
| `gtk_drag_dest_set` + `gtk_selection_data_get_uris` | `GtkDropTarget`（`GDK_TYPE_FILE_LIST`）+ `gdk_file_list_get_files()` |
| `gtk_drag_finish` | 删除（`GdkDrag` 异步 API） |
| `GtkClipboard` | `gdk_display_get_clipboard()` + 异步读写 |
| `gtk_style_context_*` | CSS / `gtk_widget_add_css_class()` |
| `gtk_widget_set_events` / `add_events` | 删除 |
| `gtk_widget_get_realized` | 保留（或看 `GdkSurface`） |
| `gtk_window_set_default_size` | 保留 |
| `gtk_widget_set_size_request` | 保留 |
| `GtkComboBoxText` | `GtkDropDown`（无保留选项，GTK4/GTK5 移除 ComboBox） |
| `GtkTreeView` / `GtkListStore` / `GtkCellRenderer` | `GtkColumnView` + `GListModel`（无保留选项；GTK4 已移除 TreeView） |

### 8.7 `.ui` 文件

| 项 | 处理 |
|---|---|
| `<requires lib="gtk+" version="3.24"/>` | `<requires lib="gtk" version="4.10"/>` |
| `GtkToolButton` / `GtkToolbar` | 删除，改 `GtkBox` + `GtkButton` |
| `GtkStatusbar` | 删除，改 `GtkBox` + `GtkLabel` |
| `GtkDialog` + `internal-child` | 改成 `GtkWindow` + 普通 `GtkBox`；不再有 action area |
| `GtkButtonBox layout-style` | `GtkBox` + `halign=end` |
| `visible=False` 的控件 | GTK4 默认不显示子控件；需要显示时保留 `visible=True` |
| `packing` 子元素 | 删除；用 `hexpand/vexpand/halign/valign` |
| `GtkComboBoxText` | 可保留；或改 `GtkDropDown`（需改代码） |

工具：`gtk4-builder-tool simplify --3to4 file.ui` 可以自动处理一部分，**必须人工复核**；`gtk4-builder-tool validate` 用于 CI。

`menu.ui` 是 GMenu，不需要转换；`emulation_settings.ui` / `graphics.ui` 建议直接重写而不是转换（反正要做 UI 改版）。

### 8.8 逐文件任务

| 文件 | 工作 |
|---|---|
| `main.cpp` | 拆出 `display.cpp`（绘制）、`dialogs.cpp`（异步对话框）、`window.cpp`（外壳）；动作层保留；25 处 `gtk_dialog_run` 异步化；Lua 菜单保留；事件控制器替换 |
| `cheatsGTK.cpp` | 直接 `GtkColumnView`（无 TreeView 过渡）；`GtkMenuButton`+`GMenu`；7 处对话框；搜索框 |
| `utilsGTK.cpp` | `GtkEventBox` → `GtkBox`；cell renderer / `GtkCellEditable` 编辑器整体删除，换 `GtkEntry`/`GtkText` 委托 |
| `tools/ramWatch.cpp` | DnD → `GtkDropTarget`；`GtkColumnView`；删除窗口位置持久化；3 处对话框 |
| `tools/ramSearch.cpp` | `GtkColumnView` + `GListModel`；3 处对话框；`sInAutoSearchAlert` 重入逻辑改为显式状态 |
| `tools/moviePlay/Record.cpp` | 表单对话框；`GtkFileDialog`；错误 `GtkAlertDialog` |
| `tools/ioregsView.cpp` | `gtk_container_add`/`box_pack` 替换；只读列 |
| `tools/luaScriptConsole.cpp` | DnD、文件选择、1 处对话框；等宽 CSS |
| `config.cpp/.h` | 复制；新增 `decorations`、`presentation` |
| `desmume.cpp/.h` | 复制（无 GTK 依赖） |
| `meson.build` | 新建（§4.1） |
| `desmume.gresource.xml` | 复制 + 增加 `style.css` |

---

## 9. 构建、CI 与打包

### 9.1 CI

`.github/workflows/build.yml` 增加一个 job（已落地，见 §14）：

```yaml
build_gtk4:
  name: Build DeSmuME (Linux/GTK4)
  runs-on: ubuntu-26.04
  steps:
    - uses: actions/checkout@v4
    - run: sudo apt update && sudo apt install meson libgtk-4-dev libsdl3-dev libpcap-dev
    # 不传 -Dfrontend-gtk：默认 auto，必须优先挑 gtk4
    - run: |
        cd desmume/src/frontend/posix/
        meson setup build-gtk4
    - run: ninja -C desmume/src/frontend/posix/build-gtk4
    - run: test -x desmume/src/frontend/posix/build-gtk4/gtk4/desmume   # 证明 auto 选了 gtk4
```

原来的 GTK3 job 显式写 `-Dfrontend-gtk=gtk3`，否则 runner 上装了 libgtk-4-dev 时会不知不觉改成构建 gtk4。

### 9.2 本地构建

```bash
# 默认 auto：gtk4 → gtk3 → gtk2，取第一个装了的
meson setup build-gtk4
# 想锁定版本
meson setup build-gtk3 -Dfrontend-gtk=gtk3
meson compile -C build-gtk4
meson install -C build-gtk4 --destdir /tmp/gtk4-root
```

### 9.3 打包注意

- `.desktop` 文件名 / `Exec` / `StartupWMClass` / app-id 四处一致
- metainfo 的 `<id>` 如果改 app-id，需要同步（否则 AppStream 校验失败）
- man page 改名或复用；`man desmume` 不应同时指向两个二进制
- 长期看，如果 GTK4 版稳定，把 `frontend-gtk` 的默认值从 `auto` 收到 `gtk4`（或干脆 `auto` 保持现状、在发行版打包脚本里写死），
  届时把 app-id/desktop 统一回 `org.desmume.DeSmuME`

---

## 10. 测试矩阵

| 维度 | 取值 |
|---|---|
| 后端 | X11、Wayland+KWin、Wayland+Mutter、Wayland+Weston（嵌套）、XWayland |
| 缩放 | 1×、2×、1.5×（分数） |
| 布局 | 单屏、混合横、混合竖、上下交换、gap 0/1、旋转 0/90/180/270 |
| 窗口 | 普通、最大化、全屏、平铺、多显示器、最小化/恢复 |
| 装饰 | SSD、CSD、切换后重启 |
| 输入 | 鼠标、触摸屏、触控笔、手柄、键盘重映射对话框 |
| 文件 | portal 文件对话框、DnD、最近文件、非 ASCII 路径、只读目录、缺失文件 |
| 工具 | 每个工具：打开/关闭、运行中、暂停时、录像中、Lua 脚本运行时 |
| 金手指 | 三种类型、启停、编辑、删除、导入导出、跨 ROM 持久化 |
| 录像 | x264、FLAC、FFmpeg、失败路径、停止/再开始 |
| 存档 | 即时存档/读档、10 槽、命名槽、快速存档 |
| 主题 | 浅色、深色、高对比度、KDE Breeze、Adwaita |
| 语言 | 中文、日文、阿拉伯语（RTL）、德语（长字符串） |
| 稳定性 | 连续运行 30 分钟、ASAN、valgrind（短会话） |

自动化程度：布局/缩放/输入可以做冒烟脚本（`xdotool`/`kdotool` + 截图对比），其余靠人工清单。

---

## 11. 风险登记

| # | 风险 | 概率 | 影响 | 缓解 |
|---|---|---|---|---|
| R1 | wgpu 呈现无法在 GTK4 直接 present | 高 | 中 | P1–P5 默认 texture；P7 离屏回读；原生子 surface 标实验 |
| R2 | 44 处 `gtk_dialog_run` 异步化引入生命周期 bug | 高 | 高 | 统一 context 模式；weak pointer；逐文件小步提交 |
| R3 | SSD/CSD 双外壳导致布局分叉 | 中 | 中 | 共用同一内容树，仅外壳不同；三平台截图基线 |
| R4 | GSK 无 nearest 过滤，像素风缩放观感变化 | 中 | 低 | 整数倍下等价；提供 cairo 回退 |
| R5 | 非整数缩放/分数缩放下纹理模糊 | 中 | 低 | 允许"整数倍 + 黑边"模式；记录已知差异 |
| R6 | GTK 4.10 底线排除旧发行版 | 高 | 低 | GTK3 前端保留；文档写清 |
| R7 | KDE 下 SSD 与全屏/平铺交互异常 | 中 | 中 | 在 KWin 6.x 实测最大化/平铺/全屏/多屏 |
| R8 | 工具窗口在主窗口关闭后阻止退出 | 中 | 中 | 显式 `close-request` 处理；退出测试 |
| R9 | Lua 泵事件重入 | 中 | 中 | `in_pump` 保护；只在帧间泵一次 |
| R10 | TreeView → ColumnView 改动过大 | 中 | 中 | 无过渡方案（GTK4/GTK5 都没有 TreeView）；按窗口分批直接改 ColumnView |
| R11 | 性能目标不达标（大窗口） | 低 | 中 | 先测量再优化；texture 路径已解决主要成本 |
| R12 | 翻译字符串/`.ui` 重写导致翻译丢失 | 中 | 低 | 保留 msgid；`xgettext` diff 检查 |

---

## 12. 验收标准

> 下面标 `[x]` 的是本环境（Arch/WSL，GTK 4.24.1）已验证项；标 `[实机]` 的需在目标
> 硬件/桌面环境验收（本环境无真实 ROM、无真 GPU、无 KDE/GNOME Wayland 会话）。

**功能**
- [实机] 所有 §1.1 条目可用，行为与 GTK3 版一致（无 ROM，逐项交互待实机）
- [x] GTK3 与 GTK4 可分别构建；destdir 先后安装到同一 prefix，desktop/metainfo 共存不冲突、二进制/图标共享名各一份（§14 P8）
- [x] 配置文件互通；GTK4 新增键有默认值，不影响 GTK3（GTK3+CLI 回归构建通过）

**外观**
- [实机] KDE Wayland：窗口装饰由 KWin 绘制（SSD），最大化/平铺/全屏正常
- [实机] GNOME Wayland：`GtkHeaderBar`，无重复标题
- [实机] X11：WM 装饰，无自绘标题栏
- [实机] 视频区纯黑、等比、无闪烁；全屏自动隐藏 UI（测试帧合成路径已验证无 CRITICAL）
- [实机] 深浅色主题下文字/控件可读

**性能**
- [实机] §1.3 表格指标全部达标（或给出书面解释）——无 GTK3 Cairo 基线，见 §6.3/P6 说明
- [x] 最小化/非活动时无持续呈现（P6 不可见停绘 + ~10 fps 降频）
- [实机] resize 无黑边/闪烁

**质量**
- [x] `gtk4-builder-tool validate menu.ui / shortcuts.ui` 通过
- [x] `desktop-file-validate` / `appstreamcli validate` 通过
- [x] gtk4/ 前端目录零编译警告（全项目清洁重建后扫描确认；核心/3rdparty 历史警告与本次无关）
- [实机] 30 分钟连续运行无泄漏（RSS 稳定）
- [实机] ASAN 短会话无错误
- [实机] CI 双前端 job 绿（workflow 已配置 build/build_gtk4/build_gtk2，YAML 校验通过，需推到 GitHub 实跑）

---

## 13. 决策记录与遗留问题

### 已确认（2026-11-20）

| # | 决策 | 结论 |
|---|---|---|
| 1 | UI 路线 | **原生 GTK4**（不引入 libadwaita） |
| 2 | GTK 底线 | **4.10**（Debian 12 / Ubuntu 22.04 继续用 GTK3 前端） |
| 3 | 呈现优先级 | **首版只做 `GdkTexture` + `snapshot()`**；GPU 滤镜在 **P7 必做补齐** |
| 4 | target / app-id | `desmume` + `org.desmume.DeSmuME.GTK4`（本文档按此实施） |
| 5 | 工具条 | 保留独立工具条，可隐藏；SSD 下不重复标题 |

### 仍待确认

1. **谁来测 KDE**：需要至少一台 KWin 6.x Wayland 环境做 SSD 验收（最大化/平铺/全屏/多屏）。
2. **像素过滤差异**：非整数倍时 GSK 只能线性过滤；是否需要提供"强制 nearest（走 cairo）"的全局开关。
3. **GTK4 版是否最终取代 GTK3**：影响 app-id/desktop 文件是否要合并回 `org.desmume.DeSmuME`。

---

## 14. 实施进度

| 阶段 | 状态 | 备注 |
|---|---|---|
| P0 骨架 | **进行中** | 目录、`frontend-gtk` 自动发现（gtk4→gtk3→gtk2）、最小窗口、CI |
| P1 外壳/装饰 | **已完成** | gresource 菜单/快捷键/CSS、三个可见性开关、SSD/CSD 共用内容树；编译链接通过（§14） |
| P2 显示/输入 | **已完成** | `DesmumeDisplay` + GdkTexture 呈现；混合布局/旋转/间隙逐项对齐 GTK3；帧池双缓冲；触控笔 + 键鼠输入。真实帧与主循环已在 P3a 接入 |
| P3a 核心/主循环 | **已完成** | `emulation.cpp` 桥接核心：异步 ROM 选择、60 Hz 主线程帧回调、真实帧推送、键盘/触摸输入 |
| P3b 动作/最近文件 | **核心动作已完成** | Reset / Close ROM / 10 槽即时存读档（F1–F10、Shift+F1–F10）/ 截图（Ctrl+S）；GTask 后台加载 ROM；自维护最近 ROM（GKeyFile）。录制/电影回放仍在后续阶段 |
| P3c 视图控制 | **已完成** | 5 种 LCD 布局（Ctrl+0/1/2/3/4）、交换屏幕（Space）、4 档旋转、屏幕间隙；从 config 恢复并持久化；P2b 的 `DesmumeDisplay` setter 经菜单动作首次暴露给用户 |
| P4 工具 | 已完成 | IO registers / RAM Watch / RAM Search / Play·Record Movie / Lua 控制台全部落地；P4c 补齐 Windows 内存监视弹窗（RAM Watch Add Cheat 按钮 + RAM Search Add Watch 注释弹窗）；P4d 新增 Memory Viewer 内存查看器（tool index 4，见对应小节）；金手指窗口属 P5 |
| P5 金手指 | **已完成** | `tools/cheats.{h,cpp}` 金手指列表窗口（tool index 3）：ColumnView + 勾选列、三类型增删改/启停/上下移、搜索过滤、R4 数据库导入、save/discard/cancel 关闭确认、全局禁用开关；详见 §14 P5 小节 |
| P6 性能 | **已完成** | 不可见停绘 + 帧池双缓冲零拷贝；性能底线（GTK4 帧路径不低于 GTK3）已通过"颜色转换直写池化缓冲 + GBytes 所有权转移"重构达成——消除 `VideoFilter` 与 `wrap_frame` 两次全帧 memcpy 及逐像素 alpha 写，像素工作量与 GTK3 持平且少两次拷贝；非活动窗口 ~10 fps 降频、`DESMUME_GTK4_TIMING=1` 计时（upload 均值 124→16 µs）；状态栏去抖已在位。详见 §14 P6 小节 |
| P7 wgpu | **已完成** | P7a 离屏渲染 + buffer 回读落地：`WGPUPresent::beginOffscreen/presentOffscreen`，emulation 桥接 `desmume_emu_set_gpu_filter()`，View→GPU Filter 菜单（13 档），GResource 打包 WGSL；gdb 自测 4 种滤镜渲染+回读成功。P7b 原生子 surface 未做（实验性，见 §6.4） |
| P8 收尾 | **已完成** | GTK4 专属 desktop/metainfo（独立 app-id，与 GTK3 共存）+ 复用共享图标 + GTK4 版 man；`gtk4/meson.build` 安装项接线、destdir 安装与共存验证；CI build_gtk4 job 加资产校验 + 安装冒烟；全项目弃用审计（gtk4/ 零警告）。详见 §14 P8 小节 |

P0 检查表：

- [x] `posix/gtk4/` 目录与 `meson.build`
- [x] `posix/meson_options.txt`：`frontend-gtk` 改为 `auto|gtk4|gtk3|gtk2|none`
- [x] `posix/meson.build` 自动发现，按 gtk4 → gtk3 → gtk2 顺序 `subdir()`
- [x] 最小 `GtkApplication` 窗口（无自定义 titlebar → SSD 路径）
- [x] 菜单（GMenu）、工具条、状态行、黑色显示区、SSD/CSD 选择逻辑
- [x] CI 增加 GTK4 job（`build_gtk4`，与 GTK3 job 并行）
- [x] GTK4 源码在 GTK 4.24.1 下编译通过（`config.cpp`、`main.cpp` 两个对象）
- [x] `desmume` 完整链接通过（POSIX meson 已接上 vendored asmjit，见下）

### P0 构建验证（2026-11-20/21，Arch/WSL，GTK 4.24.1 / meson 1.12.1）

| 检查 | 结果 |
|---|---|
| `meson setup -Dfrontend-gtk=auto`（三套 GTK 都装了） | `Message: GTK frontend: gtk4` ✅ 优先 gtk4 |
| `-Dfrontend-gtk=gtk4` / `gtk3` / `gtk2` | 分别输出 `gtk4` / `gtk3` / `gtk2` ✅ |
| `-Dfrontend-gtk=none` | `GTK frontend: none`，不进入任何 GTK 子目录 ✅ |
| `ninja gtk4/desmume.p/{main,config}.cpp.o` | 编译通过，无警告 ✅ |
| `ninja gtk4/desmume`（完整链接，含 `libdesmume`） | ✅ 221/221，链接成功 |
| `-Dfrontend-gtk=gtk3`：`ninja gtk/desmume cli/desmume-cli` | ✅ 248/248，GTK3 与 CLI 无回归 |

第一次完整链接的失败（`arm_jit.cpp:35: asmjit/x86.h: 没有那个文件或目录`）不是文件缺失：
`desmume/3rdparty/asmjit/` 的 54 个源、127 个头都在。真正原因是 POSIX meson 从来没接过 asmjit——
`includes` 没有指向 `3rdparty`（上游 include 写的是 `<asmjit/core.h>`，根必须是 `3rdparty` 这一层），
没有 `-DASMJIT_STATIC`，没有给 JIT 那几个 TU 开 C++20，也没有把 54 个 `.cpp` 放进任何 target。
GTK3 前端同样起不来。已在 `posix/meson.build` 修好，见 §14“已落地文件”。

### P0 静态核对（2026-11-20）

逐项对着源码数出来的结果（编译结果见下面的“P0 构建验证”）：

| 核对项 | 文档值 | 实测 | 结论 |
|---|---|---|---|
| `gtk_dialog_run` 总数 | 44 | 44 | 一致 |
| 分布（main/cheats/ramSearch/ramWatch/moviePlay/movieRecord/lua） | 25/7/3/3/3/2/1 | 同 | 一致 |
| `gtk_file_chooser*` | 156 | 156 | 一致 |
| `gtk_box_pack_start` / `gtk_container_add` / `gtk_widget_show_all` | 81 / 32 / 28 | 同 | 一致 |
| `GtkFileChooserNative` / `gtk_recent*` / `gtk_statusbar` | 15 / 12 / 6 | 同 | 一致 |
| `gtk_events_pending` + `gtk_main_iteration` | 6 | 3 + 3 | 一致 |
| `GdkEvent*` 结构 | 9 | 13 | 已按实测修正 |
| `gtk/` 规模 | 37 万字节 / 31 文件 | 53.7 万字节 C++ / 25 个 `.cpp`+`.h`、共 32 文件 | 已按实测修正 |
| `config.cpp` / `config.h` 与 `gtk/` 的 diff | 原样复制 | 空 diff | 一致 |
| `config_opts.h` | 仅新增 `Window/Decorations` | 仅 +5 行 | 一致 |
| `gtk/meson.build` 的 `-DGTK_DISABLE_DEPRECATED` | GTK4 版必须不带 | gtk4 只加后端宏 | 一致 |
| `libdesmume` 可见性 | gtk4 需 `link_with` | 定义在 posix/meson.build:505，早于 subdir | 一致 |

顺带确认的架构前提：`gtk_application_set_menubar`（main.cpp:7121）、`g_idle_add_full` 主线程模拟循环（1034/6137/6194）、
`drawScreen()` 的 `CAIRO_FORMAT_RGB24`（2365）、`menu.ui` 是纯 GMenu XML——都与 §2.3/§4.3 的描述相符。


### P0 已落地文件

| 路径 | 说明 |
|---|---|
| `posix/gtk4/main.cpp` | 窗口外壳：AppWindow + PopoverMenuBar + 工具条 + 显示区 + 状态行；装饰策略解析；`GtkAlertDialog` 异步示例 |
| `posix/gtk4/config.{h,cpp}` | 从 `gtk/` 原样复制，纯逻辑、无 GTK 依赖 |
| `posix/gtk4/config_opts.h` | 复制 + 新增 `Window/Decorations`（0=auto, 1=SSD, 2=CSD） |
| `posix/gtk4/meson.build` | `gtk4 >= 4.10`；可选 `gtk4-x11` / `gtk4-wayland` 后端依赖；target `desmume` |
| `posix/meson_options.txt` | `frontend-gtk` 改为 combo：`auto|gtk4|gtk3|gtk2|none`，默认 `auto` |
| `posix/meson.build` | gtk4 → gtk3 → gtk2 自动发现，按结果 `subdir()`；强制版本缺依赖时 `error()`；另接上 vendored asmjit（见下） |
| `.github/workflows/build.yml` | 新增 `build_gtk4` job（走 `auto` 并断言产出 `desmume`），GTK3 job 显式 `gtk3` |

POSIX 侧的 JIT 接线（与 MSVC/Xcode 保持一致）：

- include 根 `desmume/3rdparty`（上游写的是 `<asmjit/core.h>`），`ASMJIT_STATIC`，JIT/asmjit 的 TU 用 `cpp_std=c++20`，其余代码保持 C++14；
- 源文件表直接调用 `tools/jit_matrix/gen_asmjit_sources.py --format txt`，不手抄 54 个 `.cpp`（`ujit/` 实验目录不编）；
- `arm_jit.cpp`（x86）或 `arm_jit_arm.cpp`（arm）与 `libasmjit` 各自先编成静态库，再用 `link_whole` 合进 `libdesmume`：
  JIT 与核心互相调用，普通 `link_with` 会留下循环静态库依赖。

P0 刻意不做的事：不装 desktop/metainfo/man（app-id 与安装布局在 P1/P8 定稿），不复制 `main.ui`/`menu.ui`（P1 重写外壳时再加），不引入 wgpu。

### P1 已落地内容

| 路径 | 说明 |
|---|---|
| `posix/gtk4/menu.ui` | `GMenuModel`，按阶段列出已实现的动作；动作名与 `gtk/menu.ui` 对齐（`app.view-menu` / `app.view-toolbar` / `app.view-statusbar`），后续阶段可直接加条目 |
| `posix/gtk4/shortcuts.ui` | `GtkShortcutsWindow`，`app.shortcuts` action；关闭只隐藏，重复打开复用同一窗口 |
| `posix/gtk4/style.css` | §5.5 的三条语义类；`gtk_css_provider_load_from_resource()` 在激活时一次性挂到默认 display |
| `posix/gtk4/desmume.gresource.xml` | 打包上面三个文件，前缀仍是 `/org/desmume/DeSmuME` |
| `posix/gtk4/meson.build` | `gnome.compile_resources()` 生成源并入 target |
| `posix/gtk4/main.cpp` | 菜单改为从资源加载；新增 `view-menu`/`view-toolbar`/`view-statusbar`/`shortcuts` action；工具条按钮加 a11y label；状态行只在文本变化时更新，并挂 `desmume-status` CSS 类 |

验证：Arch/WSL GTK 4.24.1 下 `ninja gtk4/desmume` 224/224 链接通过；
`gtk4-builder-tool validate` 对 `menu.ui`/`shortcuts.ui` 均无报错；二进制内三个资源路径均已嵌入。
CSD/SSD 的实际观感仍需在 KWin/GNOME/X11 上人工验收（§10）。

P1 刻意不做的事：不装 desktop/metainfo/man（留在 P8），不引入视频/模拟逻辑（P2/P3），不把 GTK3 的 70 多个尚未实现的动作塞进菜单。

### P2a 已落地内容

| 路径 | 说明 |
|---|---|
| `posix/gtk4/display.h` / `display.cpp` | `DesmumeDisplay : GtkWidget`：两块 `GdkMemoryTexture`（B8G8R8A8_PREMULTIPLIED），`snapshot()` 画黑底 + 两屏 + 间隙；布局/旋转/间隙/交换由 setter 控制 |
| 同上 | `GtkGestureClick` / `GtkEventControllerMotion` / `GtkEventControllerKey` 转成 `touch-press` / `touch-release` / `touch-move` / `key-event` 信号；`desmume_display_map_touch()` 用 cairo_matrix 逆变换映射到下屏帧缓冲坐标 |
| `posix/gtk4/main.cpp` | 用 `DesmumeDisplay` 替掉 `GtkDrawingArea`；`DESMUME_GTK4_TEST_FRAME=1` 时喂合成帧（默认仍是黑屏） |
| `posix/gtk4/meson.build` | 把 `display.cpp` 并入 target |

P2a 的边界（P2b/P3 再做）→ 已在 P2b 收掉，见下。

验证：Arch/WSL GTK 4.24.1 下完整链接通过；编译期无 `display.cpp`/`main.cpp` 警告。
视觉效果（KWin/GNOME/X11、旋转、混合布局）还需要带显示环境的人工验收。

### P2b 已落地内容（2026-11-21）

对齐基准是 `gtk/main.cpp` 的 `draw_params` / `ExposeDrawingArea` / `drawTopScreen` /
`drawBottomScreen` / `calc_hybrid_vertical_params()`：

| 项 | 做法 | GTK3 对应 |
|---|---|---|
| hybrid-equal 大屏比例 | `2 + gap/h`，不再默认 1.0 | `main.cpp:3168` |
| hybrid-vertical 大屏比例 | 按窗口（90/270 时交换宽高）算 `ratio2/ratio1`；`<=0` 覆盖值表示自动 | `main.cpp:2444` |
| 混合布局两屏位置 | 大屏 `(0,0,w·hs,h·hs)`；小屏 top 在 `(w·hs,0)`、bottom 在 `(w·hs,h+gap)` | `2390` / `2424` |
| gap 矩形 | vertical 在 `(0,h)`；hybrid 在 `(w·hs,h)`；颜色 0.3/0.3/0.3；horizontal/single 不画 | `3176` / `3219` |
| 旋转后的中心平移 | 90/270 用交换后的 `fit_w/fit_h`，与 GTK3 `-imgH/2,-imgW/2` 一致 | `3209` |
| 触控坐标 | 逆矩阵 + hybrid 大屏的 `h·(2-hs)+gap` 偏移，输出 NDS 逻辑 0..255 / 0..191；P3 可直接 `NDS_setTouchPos()` | `2428` / `3225` / `3527` |
| 帧缓冲 | emulation 层 `FramePool` 直写：颜色转换直接写入池化 BGRA 缓冲（Opaque 转换器已写 alpha=0xFF），以 `GBytes` 移交 widget 建两张零拷贝 `GdkMemoryTexture`；无 `memcpy`、无 alpha 修正循环。GPU 滤镜的放大帧按尺寸分桶独立回收 | §6.2 双缓冲 |
| 触控笔 | `GtkGestureStylus`（`stylus-only=TRUE`，`get_axis(GDK_AXIS_X/Y)`），与 click/motion 用 `stylus_active` 互斥 | §4.4 |

颜色转换函数 `ColorspaceConvertBuffer*To8888Opaque` 已把每像素 alpha 写成 0xFF，产物本身就是合法的 `GDK_MEMORY_B8G8R8A8_PREMULTIPLIED`，因此不需要 `GDK_MEMORY_B8G8R8X8`（4.14+）也不需要 alpha 修正拷贝；这条路径比 GTK3 Cairo 少一次全帧 `memcpy`。
布局几何与 GTK3 对齐；非整数倍缩放下 GSK 线性过滤与 GTK3 默认 `CAIRO_FILTER_NEAREST` 的差异仍属已知项（§4.3/R4）。
`get_coordinates()` 已从当前 GTK4 API 移除，触控笔坐标走 `gtk_gesture_stylus_get_axis()`。

顺带清掉 P1 遗留的 `gtk_widget_hide()` 弃用警告（换成 `gtk_widget_set_visible()`），§4.10 的"不累积弃用警告"保持成立。

验证：Arch/WSL GTK 4.24.1 下 `ninja gtk4/desmume` 全绿无警告；切到 `-Dfrontend-gtk=gtk3` 后
`gtk/desmume` + `cli/desmume-cli` 26/26 构建通过（只有既有警告），再切回 gtk4 重新链接通过。

### P3a 已落地内容（2026-11-21）

新增 `posix/gtk4/emulation.{h,cpp}`，把核心从 GTK 层隔离出来。`main.cpp` 只保留
窗口/动作/对话框，不再直接 include 模拟器头文件。

| 项 | 做法 | GTK3 对应 |
|---|---|---|
| 核心初始化 | `NDS_Init()` + `Change3DRendererByID(RENDERID_SOFTRASTERIZER)`；只注册 `SNDDummy`，不开音频设备 | `main.cpp:6657` / `gtk/desmume.cpp:33` |
| 主循环 | `g_timeout_add(1000/60, OnFrameTick)`，主线程每 tick 一次 `desmume_emu_step()`；无 ROM 时空转 | `EmuLoop()` / `EmuSingleFrame()` |
| 单帧 | `apply_keys()` → `NDS_begin/endProcessingInput()` → `NDS_exec<false>()` → `SPU_Emulate_user()` | `EmuProcessFrameInput()` / `desmume_cycle()` |
| 取帧 | `GPU->GetDisplayInfo().masterCustomBuffer` → `ColorspaceConvertBuffer555xTo8888Opaque<true,false,BESwapNone>`（888 源走 `ColorspaceConvertBuffer888xTo8888Opaque<true,false>`）→ `VideoFilter(None)` → `DesmumeDisplay` | `Gtk3GPUEventHandler::DidFrameEnd()` `main.cpp:7663` / `PresentFrame()` `3369` |
| ROM 选择 | `GtkFileDialog` 异步打开，过滤 `*.nds/*.rom/*.zip`；也接受命令行第一个普通文件；错误走 `GtkAlertDialog` | §4.6/D7 |
| 键盘 | `DesmumeDisplay` 的 `key-event` 信号 → 15 键 latch → `NDS_setPad()`，默认键位与 GTK3 `gtk_kb_cfg` 一致 | `Key_Press/Release` `4500` / `update_keypad()` `477` |
| 触摸 | `touch-press/move` → `NDS_setTouchPos(x,y)`，`touch-release` → `NDS_releaseTouch()`；坐标已由 P2b 映射为 NDS 逻辑值 | `EmuSingleFrame()` `5939` |

P3a 的边界（留给 P3b/P4-P8）：

- 没有帧速限制/跳帧补偿、声音、Lua、OSD/HUD、录像、电影回放；当前帧率由 60 Hz GLib timeout 决定；
- 没有手柄、按键重映射对话框、最近文件、ROM 卸载/重置动作；工具栏 Reset 仍是禁用占位；
- ROM 解析发生在文件对话框回调里（同步），大 ROM 会短暂无响应；P3b 再用 `GTask` 挪到后台；
- `VideoFilter` 固定 `VideoFilterTypeID_None`，GPU 帧缓冲保持原生 256×192、BGR555、单页；滤镜/缩放设置留到 P4/P7；
- 没有可运行 ROM 与显示环境，本轮只做编译/链接验证与 `--help` 启动冒烟，画面需人工验收。

验证：Arch/WSL GTK 4.24.1 下 `ninja gtk4/desmume` 链接通过；`gtk4/desmume --help` 正常退出；
`-Dfrontend-gtk=gtk3` 下 GTK3+CLI 无回归，再切回 gtk4 重新链接通过。
`emulation.cpp` 的警告来自核心头文件（`texcache.h` / `matrix.h`），GTK3 `main.cpp` 同样存在，非本次引入。

#### P3a 实机修复：关闭窗口与 Ctrl+C 都能干净退出（2026-11-21）

实机测试暴露两个生命周期问题，均已修复：

| 症状 | 根因 | 修复 |
|---|---|---|
| 关闭主窗口后进程不退出 | 60 Hz `g_timeout_add` 从未移除，窗口销毁后继续 `desmume_emu_step()`；frame sink 还持有已销毁的 `pDisplayArea` 裸指针 | `main.cpp` 记录 `sFrameTickSource`/`sTestFrameSource`；新增幂等的 `StopEmulation()`（停 running、清 sink、移除两个 source）；窗口 `close-request` 先 `StopEmulation()` 再 `g_application_quit()`；`main()` 在 `g_application_run()` 返回后也兜底调用 |
| 关窗瞬间 `DESMUME_IS_DISPLAY` 断言失败 | frame sink 的 user_data 是 widget 裸指针，晚到的 tick 用到已释放对象 | sink 的 user_data 改为 `&pDisplayArea`，并在 `BuildWindow()` 用 `g_object_add_weak_pointer()` 监护；widget 销毁时自动置 NULL，`PushFrameToDisplay()`/`PushTestFrame()` 先判空 |
| 终端 Ctrl+C 杀不掉进程 | `NDS_Init()→MMU_Init()→Mic_Init()` 调用 `SDL_InitSubSystem(SDL_INIT_AUDIO)`，SDL3 默认安装 SIGINT/SIGTERM handler 并转成 `SDL_QUIT`；GTK4 前端无 SDL 事件循环，信号被吞 | 在任何 SDL 初始化之前（`main()` 开头 + `desmume_emu_init()`）`g_setenv("SDL_NO_SIGNAL_HANDLERS","1",TRUE)` |

SIGINT 根因已用独立小程序验证（系统 SDL3 3.4.20）：不设 hint 时 `SDL_InitSubSystem(AUDIO)` 会把 SIGINT handler 从 `SIG_DFL` 改掉（default=1→0）；设 hint=1 后 handler 保持默认。
GTK4 关窗时序的运行时验证受限于当前环境无显示服务/无 ROM，仍需实机复测确认。

### P3b 已落地内容（2026-11-21）

GTK4 前端是从零写的，并没有 GTK3 `main.cpp` 里那批 `gtk_dialog_run` 调用——它们主要集中在
`cheatsGTK.cpp` / `tools/*`（由 P4/P5 处理）。本阶段补齐日常使用所需的动作与后台加载：

| 项 | 做法 |
|---|---|
| Reset | 工具栏与 Emulation 菜单接线 `app.reset` → `NDS_Reset()`（保留 ROM，不重载） |
| Close ROM | `app.close` → `NDS_FreeROM()`，复位标题/按钮/动作灵敏度，状态栏提示 |
| 即时存/读档 | `app.save-state(u)` / `app.load-state(u)` 覆盖槽位 0..9（标签 1..10）；`savestate_slot()` / `loadstate_slot()`；快捷键 Shift+F1–F10 / F1–F10；加载 ROM 后 `scan_savestates()` 汇总槽位占用 |
| 截图 | `app.screenshot`（Ctrl+S）从 `masterCustomBuffer` 转 BGRA8888 → `GdkTexture` → `gdk_texture_save_to_png()`，存到 XDG 图片目录，文件名带时间戳；无需新增依赖 |
| 后台加载 | `GtkFileDialog` 选中后用 `GTask` 把 `desmume_emu_load_rom()`（含解压与解析）挪到 worker 线程；加载期置 `sLoadingRom`，60 Hz tick 跳过，避免与 `NDS_LoadROM` 换内存映射竞态；`StopEmulation()` 用 `GCond` 等所有 in-flight worker 退出后才让 `main()` 析构 core |
| 最近文件 | GTK4 已移除 `GtkRecentManager`，改为自维护 `GKeyFile`（`~/.config/desmume/recent-roms.ini`，最多 10 条）；`app.recent(s)` 接收 URI，动态重建 `open_recent` 子菜单；加载失败的路径被移除 |
| 动作灵敏度 | `UpdateRomActions()` 在无 ROM 时置灰 run/reset/save-state/load-state/screenshot/close；命令行 ROM 仍走同步 `LoadRomAndReport()`（主循环尚未启动） |

**已知边界**：录像/电影回放、声音设备、手柄、按键重映射、FS 兼容性仍属后续阶段；最近文件不写 `shared-mime-info` 的 `recently-used.xbel`；
截图以 GPU 原生复合缓冲（两屏纵向叠加）保存，未含 HUD/OSD 叠加层（该层在 P4/P7 的渲染路径里）。

验证：Arch/WSL GTK 4.24.1 下 `ninja gtk4/desmume` 编译链接全绿无警告；`gtk4/desmume --help` 正常退出；
`-Dfrontend-gtk=gtk3` 下 GTK3+CLI 26/26 无回归，再切回 gtk4 重新链接通过。
动作的实际触发（存读档、截图落盘、最近菜单、后台加载进度）仍需带显示环境 + ROM 实机验收。

### P3c 已落地内容（2026-11-21）

P2b 在 `DesmumeDisplay` 上按 GTK3 语义实现了布局/旋转/交换/间隙的几何运算，并用 `DEFINE_SETTER`
宏暴露了 `desmume_display_set_layout/rotation/gap/swap/hybrid_scale`，但前端从未调用它们——视图
配置字段（`view_orient`/`view_swap`/`view_rot`/`view_gap`）躺在 config 里不生效，用户无法切换视图。
P3c 把这条链路接通：

| 项 | 做法 |
|---|---|
| LCD 布局 | `app.orient(s)` 取 vertical/horizontal/single/hybrid_equal/hybrid_vertical，映射到 `DesmumeLayout` 0..4（与 GTK3 `orientation_enum` 同号）；Ctrl+1/2/0/3/4 |
| 交换屏幕 | `app.swapscreens` 布尔 state action → `desmume_display_set_swap()`；Space |
| 旋转 | `app.rotate(s)` 取 "0"/"90"/"180"/"270"，校验后 → `set_rotation()` |
| 屏幕间隙 | `app.gap` 布尔 state action → `set_gap(view_gap ? GAP_SIZE : 0)`；`GAP_SIZE=64` 与 GTK3 同值，按 framebuffer 像素传入 |
| 配置同步 | `ApplyViewSettings()` 在窗口构建后把 config 推给 display；每个动作回调更新 config 并 `save()`；菜单 state 在 OnActivate 里从 config 回填，radio/toggle 勾选状态与实际视图一致 |

`orient`/`rotate` 是带字符串 target 的动作，GTK 只为 application menubar 自动装 accel，
PopoverMenuBar 需要在 `main()` 里用 `gtk_application_set_accels_for_action()` 显式注册
（`app.orient('vertical')` 形式）；`swapscreens`/`gap` 是无参数 state action，走默认注册。
`menu.ui` 的菜单项沿用 GTK3 的 target 命名，便于逐项对照。

验证：`gtk4-builder-tool validate menu.ui` 通过；`ninja gtk4/desmume` 编译链接全绿无警告；
`--help` 正常退出且无 GVariant/action 解析 critical；切 GTK3 +CLI 26/26 无回归，切回 gtk4 重链通过。
布局/旋转/交换/间隙的实际画面变化仍需带显示环境 + ROM 实机验收。

### P4a 已落地内容（2026-11-21）

工具窗口框架 + 前三个只读/调试工具。所有工具共用 `tools.cpp` 注册表（`dTools_list` 同序）、
`desmume_tool_window_new()`（不设 titlebar → SSD 路径，父窗口用 `desmume_tools_set_parent()`），
以及 `rowmodel.cpp` 的 `DesmumeRow` + `desmume_column_view_new()`（ColumnView + MultiSelection + monospace CSS）。

| 路径 | 说明 |
|---|---|
| `posix/gtk4/tools.{h,cpp}` | `DesmumeTool` 接口（open/update/close/is-open）、按索引开工具、每帧 `update()`（未映射窗口直接跳过）、全局父窗口 |
| `posix/gtk4/tools/rowmodel.{h,cpp}` | 5 列 GObject 行 + GListStore + ColumnView 工厂；`desmume_column_view_selected()` 取选中位置 |
| `posix/gtk4/tools/ioregs.cpp` | ARM9/ARM7 `GtkDropDown` 切换；6 个寄存器只读表（值 + 二进制位）；`emulation.cpp` 的 `desmume_emu_read_ioreg()` 桥接核心 MMU |
| `posix/gtk4/tools/ramwatch.cpp` | `RAMWatchCore` 文档模型；新/编辑/删除/复制/分隔/上下移按钮；GtkFileDialog 打开/另存 + DnD（`GtkDropTarget`/`GDK_TYPE_FILE_LIST`）；`GtkPopoverMenu` 文件菜单 + 最近文件 GActions；窗口尺寸状态栏；自动加载偏好；**Add Cheat 按钮**（对齐 Windows `IDC_C_ADDCHEAT`：选中项的地址/缓存值/字节数/注释预填 Add Cheat 窗口，仅主 RAM + 含 ROM 时可用，见 P4c） |
| `posix/gtk4/tools/ramsearch.cpp` | `RAMSearchCore` 引擎；4 类搜索 × 8 种比较 + Value/Parameter 输入；Size/Display `GtkDropDown`；misaligned/little-endian/auto-search 开关；Search/Reset/Clear/Undo/Redo/Eliminate/Poke/Add Watch/Add Cheat；结果表 50000 行上限 + 节流刷新（150/900 ms）；auto-search 耗尽走 `GtkAlertDialog` 三按钮异步回调（无嵌套循环）；Add Cheat 走 `desmume_emu_add_cheat()` 桥接（仅主 RAM，含 ROM 时）；**Add Watch 注释弹窗**（对齐 Windows `IDD_PROMPT`：单选弹窗输入注释，多选全部空注释，见 P4c） |
| `posix/gtk4/emulation.{h,cpp}` | 新增 `desmume_emu_read_ioreg()`（IO 工具）与 `desmume_emu_add_cheat()`（RAM Search → `CHEATS::add` + `save`） |

**GTK 4.24 API 变更**：本机 GTK 4.24.1 已移除 `gtk_entry_get_text/set_text/set_width_chars`
（编译报 not declared），统一改用 `gtk_editable_get_text/set_text/set_width_chars(GTK_EDITABLE(...))`。
这符合 §4.10 的"按 GTK5 约束写"——4.10 起 `GtkEditable` 就是正式接口，4.24 把旧的 entry 便捷函数删掉。
ramwatch 的 editor 两处 entry 已全部切过去。

**已知边界**：
- RAM Watch 的 dirty-close 目前静默保存到已关联文件，没有 GTK3 的"Save changes?"三态确认；
- RAM Search 的 row 缓存仍按 `kMaxDisplayedResults` 截断（50000），但 ColumnView 只实例化可见行，滚动流畅度优于 GTK3；
- 内部金手指（Add Cheat）写入 + 持久化桥接已由 P4c 接入 RAM Watch / RAM Search 的 UI，
  但添加后不自动跳到 P5 主 Cheats 窗口刷新（需重开该窗口）；
- 三个工具的运行时行为（搜索结果、DnD、对话框交互）仍需带显示环境 + ROM 实机验收。

验证：Arch/WSL GTK 4.24.1 下 `ninja gtk4/desmume` 全绿（234/234，编译链接无新增警告）；
切 `-Dfrontend-gtk=gtk3` 后 GTK3 + CLI 248/248 无回归，再切回 gtk4 重链通过。

### P4b 已落地内容（2026-11-21）

| 路径 | 说明 |
|---|---|
| `posix/gtk4/tools/movie.{h,cpp}` | Play/Record 两个模态式表单窗口；`GtkFileDialog` 异步选文件；`GtkAlertDialog` 报错；共享 `movie_info_core.cpp` 做 DSM 头解析/比对/评论改写； GTK 4.24 已移除 `gtk_radio_button_*`（用 `GtkCheckButton` + `set_group`）与 `gtk_calendar_select_month/day`（用 `gtk_calendar_set_date(GDateTime*)`） |
| `posix/gtk4/tools/luaScriptConsole.{h,cpp}` | Lua 控制台：多窗口（uid→console map）、inotify 文件监听 + 重载、`GtkDropTarget` 拖入脚本、`GtkFileDialog` 浏览、`GtkAlertDialog` About；`OpenLuaContext` 的 print/onstart/onstop 回调经 `G_PRIORITY_DEFAULT` idle 回主线程；引擎 UI hooks（popup/openscript/输入/菜单）未安装，走 `lua-engine.h` 的平台中立 fallback（CLI/GTK2 同款） |
| `posix/gtk4/emulation.{h,cpp}` | 新增 `desmume_emu_load_movie/save_movie/stop_movie/movie_active/rom_base_name` 桥接；播放前自动暂停；`FCEUI_SaveMovie` 的 UTF-8→`std::wstring` 转换复用 `utils/xstring.h` 的 `mbstowcs()` |
| `posix/gtk4/main.{cpp}`, `menu.ui` | `app.playmovie/recordmovie/stopmovie` 与 `app.luaconsole/closeluascripts/aboutlua`（后者 `#if HAVE_LUA`）；`stopmovie` 随 movieMode 启停；Tools 菜单加两组条目 |

**Lua 控制台边界**：`emu.openscript()`、`gui.*` 弹窗、Lua 动态菜单、输入状态查询走引擎 fallback，
没有 GTK4 专属实现（GTK3 的 `GtkLua_*` 主机回调未迁移）。脚本的 Run/Stop/print/文件监听/DnD 均可用。
这部分若要对齐 GTK3，需要先补 §4.8 的主循环泵与输入/菜单桥接，属独立后续项。

**Movie 工具边界**：`Import Backup Memory`（导入 .dsv）未迁移；表只读字段按 GTK4 精简为 ROM/Firmware 两项。
播放/录制需要带 ROM 与显示环境实机验收；本机无 ROM，未做运行时验证。

### P4c Windows 内存监视弹窗移植（2026-11-21）

用户要求把 Windows 端 "Memory Watch"（即 RAM Watch）弹窗相关功能移植到 GTK4。
代码里没有 "Memory Watch" 字面量，对照 `windows/ramwatch.cpp`、`ram_search.cpp`、`resources.rc`
后确认 Windows 端比 GTK4 多两个交互，均已补齐（对齐 Windows 行为）：

| 路径 | 说明 |
|---|---|
| `posix/gtk4/tools/ramwatch.cpp` | **Add Cheat 按钮**（对齐 `IDD_RAMWATCH` 的 `IDC_C_ADDCHEAT`）：选中非分隔行后用 `watch.Address` / `RAMWatchCore::ReadValue(watch)`（缓存值，等价于 Windows 的 `watch.CurValue`）/ `SizeInBytes(watch.Size)` / `watch.Comment` 预填一个普通 `GtkWindow` 表单，确认走 `desmume_emu_add_cheat()`；`WatchCheatCtx` 用 `g_object_set_data_full()` 绑定窗口生命周期；侧栏按钮 `update_control_states()` 随选中态 + `desmume_emu_has_rom()` 更新敏感状态 |
| `posix/gtk4/tools/ramsearch.cpp` | **Add Watch 注释弹窗**（对齐 `ram_search.cpp` 从搜索结果加监视时的 `IDD_PROMPT`）：单选弹 "Input Prompt" 让用户输入注释（默认 `RAM Search <addr>`），多选一次性全部以空注释添加（Windows 对每行调 `InsertWatch(tempWatch, "")`）；选中位置先汇总到 `std::vector` 再弹窗，避免 modeless 窗口期间选择变化；`AddWatchCtx` 同样绑定窗口生命周期 |

**边界与校验**：Add Cheat 仅在 main RAM（0x02000000–0x023FFFFF）且已载入 ROM 时可用，
否则报错——与 RAM Search 的 Add Cheat 桥接约束一致；分隔行/无选中直接忽略。
两个弹窗都复用 `desmume_tool_window_new()`（不设 titlebar，跟随 SSD 策略）。

验证：`ninja gtk4/desmume` 全绿；gdb 在 `desmume_tools_update` 断点命中后（GUI 已初始化）
依次调用 `desmume_tools_open(1/2/3)` 打开 RAM Watch / RAM Search / Cheats，
三个窗口均成功构建、3 个新线程对应三次 open、全程无 CRITICAL / 断言 / 崩溃。
无 ROM 环境下未走"添加成功"的完整数据路径，该路径与已验证的 RAM Search Add Cheat 桥接同构。

### P4d Memory Viewer 内存查看器（2026-11-21）

用户要求把 Windows `memView.cpp` / macOS `MemoryViewerWindow` 的 **Memory Viewer**
弹窗移植到 GTK4。两端共用平台中立核心，GTK4 复用同一份核心，只重写视图层。

| 路径 | 说明 |
|---|---|
| `shared/memory_viewer_core.{h,cpp}` | 从 `cocoa/userinterface/` 复制的平台中立核心（include 路径 `../../../`→`../../`）：5 个区域（ARM9/ARM7/Firmware/CartROM/Full）、字节/半字/字读写、主 RAM 读/写断点列表（`memReadBreakPoints` / `memWriteBreakPoints`）、区域导出尺寸；核心层不依赖任何 GUI 工具包，Cocoa 原文件未改动 |
| `posix/gtk4/tools/memoryviewer.{h,cpp}` | `DesmumeTool`（tool index 4）：`GtkDrawingArea` + Cairo 等宽 hex dump（16 字节/行 + ASCII 列，字节/半字/字视图、大端、Full chars、断点红底、选中高亮、正在输入的位用 `.` 占位）；`GtkEventControllerKey` 处理方向键/Home/End/PageUp/PageDown + 十六进制原地编辑（写回 `MemoryViewerWrite*`，写完自动前进一格）；`GtkGestureClick` 定位选中；`GtkEventControllerScroll` 按行滚动；工具栏含区域 `GtkDropDown`、地址输入 + 历史下拉 + Go、视图模式 `GtkDropDown`、选中地址标签、`GtkMenuButton` 导出菜单；断点栏含地址输入、R+/W+/−/Clear、汇总、Full/BE 复选；`Update()` 每 150 ms 节流刷新、仅可见行读总线（4 GB Full 视图瞬时打开） |
| `posix/gtk4/tools.cpp` / `menu.ui` / `meson.build` | 注册 tool index 4、Tools 菜单 `_Memory Viewer…`、编译 `memoryviewer.cpp` 与 `shared/memory_viewer_core.cpp` |

**导出**：复用 macOS 的三种模式——Text Dump（追加，按当前视图宽度格式化 + ASCII）、
Raw Dump（追加，可见窗口原始字节）、Dump All（整区二进制，CartROM/Full 无有效尺寸时提示改选区域）。
全部走异步 `GtkFileDialog::save`，文件 I/O 在回调中进行，不阻塞主循环。

**验证**：`gtk4-builder-tool validate menu.ui` 通过；`ninja gtk4/desmume` 全绿；
gdb 在 `desmume_tools_update` 断点后 `desmume_tools_open(4)`，两次 `is_open()` 均返回 1，
窗口成功构建、全程无 CRITICAL / 断言 / 崩溃；切 `-Dfrontend-gtk=gtk3` 后 GTK3 + CLI 无回归，
再切回 gtk4 重链通过。无 ROM 环境下断点与导出写盘的完整数据路径未做实机验收。

### P4 运行时修复（2026-11-21）

实机打开 RAM Search 曾触发 SIGSEGV，根因与修复：

| 问题 | 根因 | 修复 |
|---|---|---|
| 打开任意 ColumnView 工具崩溃（`g_object_new` / `g_list_model_get_n_items` 断言） | `rowmodel.cpp` 里 `gtk_column_view_column_new()` 对 factory 是 **transfer full**，之后又 `g_object_unref(factory)` → 双重释放；view bind 时用到已释放的 factory | 移除多余的 `g_object_unref(factory)`；`gtk_column_view_append_column()` 是 transfer none，保留 column 的 unref |
| selection 接管 model 后 `clear_state()` 再次释放 | `gtk_multi_selection_new(model)` 是 **transfer full** | 传 `g_object_ref(model)` 给 selection，调用方保留自己的引用由 `clear_state()` 释放 |
| `invalid cast from 'GtkMenuButton' to 'GtkButton'` | GTK4 中 `GtkMenuButton` 不再是 `GtkButton` 子类 | ramwatch 的 File 按钮改 `gtk_menu_button_set_label()`；movie 的日期按钮同样处理 |

验证：gdb 中 `break ramsearch.cpp:Open` 自动 continue，实际打开窗口并持续运行 20 秒，
无 CRITICAL / SIGSEGV / GObject 断言。量产构建 `ninja gtk4/desmume` 全绿。

**编译警告修复**（核心头文件，行为等价）：
- `utils/datetime.h`：删除 `CalculateTicks()` 中已无用处的 `overflow` 变量（异常抛出早已注释掉）。
- `rasterize.h`：`SoftRasterizerTexture` 加 `using TextureStore::Load;` 消除 `-Woverloaded-virtual`。
- `cheatSystem.h`：`CHEATS_LIST` 构造函数由 `memset(this,...)` 改为成员初始化列表（消除 `-Wclass-memaccess`）。
- `matrix.h`：SIMD helper 加局部 `MATRIX_UNUSED` 属性，消除 `-Wunused-function`。

### RAM Search 虚拟滚动与打开性能（2026-11-21）

**问题**：打开弹窗卡顿好一会。根因有两层：
1. `rebuild_model()` 一次性物化最多 50000 个 `DesmumeRow` GObject，每个都做
   `FormatAddress` + 2× `FormatValue` 字符串格式化；ColumnView 的 widget 虽是虚拟的，
   但数据层不是，滚动 50000 行对象照样全部建好。
2. 首次打开 `Reset()` 枚举 ~420 万候选地址（1 字节步长 × 3 个 RAM 区，约 67 MB），
   同步执行在主线程上，窗口要等它跑完才出现。

**修复**：
- `tools/ramsearch.cpp`：用自定义 `RamSearchModel`（`G_DEFINE_TYPE_WITH_CODE` +
  `G_IMPLEMENT_INTERFACE(G_TYPE_LIST_MODEL)`）替换 `GListStore`。`get_n_items()` 返回行数，
  `get_item()` 只在 GTK 请求时从 `RAMSearchCore::ResultAt()` 格式化单行；
  `items-changed` 通知 GTK 重新 bind 可见行。行数存在 model 对象自身（不读全局），
  避免窗口销毁时全局被清零导致 `gtk_list_item_manager_clear_model` 断言。
  删除 `sRowCache` 与逐行刷新逻辑——现在 widget 层和数据层都是真虚拟滚动。
- 首次 `Reset()` 移到 `g_idle_add(deferred_first_reset)`，窗口先呈现再后台枚举，
  用户不再对着冻结的主窗口干等。
- `tools/rowmodel.cpp`：`desmume_column_view_selected()` 改用 `gtk_bitset_get_size/get_nth`
  只遍历选中项（原先 O(50000) 全表扫描），并补上 `gtk_bitset_unref()`（`get_selection`
  是 transfer full，原先泄漏）。

验证：gdb 中 Open → 呈现 → deferred Reset → 持续帧刷新 → 正常退出，全程无 CRITICAL /
SIGSEGV / GObject 断言；GTK4 与 GTK3/CLI 均编译通过。

### P4 运行时修复：关闭 IO registers / RAM Search 时 SIGABRT（2026-11-21）

**现象**：关闭 IO registers 与 RAM Search 窗口时，先刷 7 条
`g_list_model_get_n_items: assertion 'G_IS_LIST_MODEL (list)' failed`，随后
`gtk_list_item_manager_clear_model: assertion failed: (gtk_rb_tree_get_root (self->items) == NULL)`
→ `Bail out!` → SIGABRT。关闭 RAM Watch 窗口则完全正常。

**共同点与差异点**：

| 工具 | `GtkDropDown` 数量 | 关闭行为 |
|---|---:|---|
| IO registers | 1（ARM9/ARM7 选择） | 崩溃 |
| RAM Search | 4（Search Type / Comparison / Size / Display） | 崩溃 |
| RAM Watch | 0（只有菜单按钮和普通按钮） | 正常 |

崩溃与 `GtkColumnView`、行数、模型类型都无关（把行数改成 0 仍崩；RAM Watch 带 watch 条目也正常），
只跟窗口里有没有 `GtkDropDown` 有关。

**根因**：`gtk_drop_down_new(model, expression)` 的 `model` 是 **`(transfer full)`**，GTK 源码里
显式 `/* we're consuming the references */ g_clear_object (&model);`。而四个调用点在创建之后又
`g_object_unref()` 了一次 → 字符串列表被提前释放，GtkDropDown 内部的 `GtkFilterListModel` /
`GtkSingleSelection` / 弹层 `GtkListView` 全部持有悬空指针。窗口销毁时
`gtk_list_item_manager_clear_model()` 的第一步是
`g_list_model_get_n_items (G_LIST_MODEL (self->model))`，拿到已释放对象 → 断言失败 → 返回 0 →
`remove_items` 一个 tile 都不删 → 末尾 `g_assert (gtk_rb_tree_get_root (self->items) == NULL)` 命中。

**修复**：删除 4 处多余的 `g_object_unref()`（`ioregs.cpp` 1 处、`ramsearch.cpp` 的 `add_dropdown()` 1 处、
`ramwatch.cpp` 的 watch 编辑器 2 处——**New/Edit Watch 对话框同样会崩，只是用户没点到**）。

⚠️ 别顺手删 `rowmodel.cpp` 里的 `g_object_ref(model)`：`gtk_multi_selection_new()` 也是 `(transfer full)`，
那里先 ref 再传是正确写法（`gtk_column_view_set_model()` 则是 transfer none）。

**验证**（Arch/WSL，GTK 4.24.1）：

| 测试 | 结果 |
|---|---|
| 最小复现：裸 `GtkDropDown` + 字符串列表 + 多余 `g_object_unref()`，关窗 | 稳定复现 7 条 CRITICAL + `clear_model` 断言 + SIGABRT |
| 同上，去掉 `g_object_unref()` 或改用 `gtk_drop_down_new_from_strings()` | 正常退出 |
| 真机：gdb 脚本开 IO registers + RAM Search + RAM Watch，跑 120 帧后 `desmume_tools_close_all()` | 修复前：7 CRITICAL + `Bail out!` + SIGABRT；修复后：0 CRITICAL，正常退出 |

#### 复现与回归手法（可复用）

**1. 最小宿主：把崩溃缩到一个控件。** 不碰模拟器核心，只依赖 `gtk4`：

```c
GtkWidget *win = gtk_window_new();
GtkStringList *cpus = gtk_string_list_new(NULL);
gtk_string_list_append(cpus, "ARM9");
gtk_string_list_append(cpus, "ARM7");
GtkWidget *dd = gtk_drop_down_new(G_LIST_MODEL(cpus), NULL);  /* transfer full */
g_object_unref(cpus);                                          /* ← 就这一行是 bug */
gtk_window_set_child(GTK_WINDOW(win), dd);
gtk_window_present(GTK_WINDOW(win));
/* 800 ms 后 gtk_window_close() → 稳定 SIGABRT */
```

`gcc p.c -o p $(pkg-config --cflags --libs gtk4)`。判定只看三件事：有没有
`G_IS_LIST_MODEL` 断言、有没有 `clear_model` 断言、退出码是否 -6（SIGABRT）。
反向验证：去掉 `g_object_unref()`，或改用 `gtk_drop_down_new_from_strings()`，两者都应正常退出。

**2. 把真实工具代码挂进宿主。** `tools/ioregs.cpp` 只依赖 `emulation.h`（纯 GTK 声明）和
`desmume_tool_window_new()`；写个 stub 提供 `desmume_emu_read_ioreg()` /
`desmume_tool_window_new()` 就能和 `rowmodel.cpp` 一起单独链接，再加一个 16 ms 的 tick 调
`update()` 模拟真机帧循环。改一行代码就能重跑，不用起模拟器。

**3. 真机回归：gdb 脚本代替手点菜单。** WSLg 没有 WM，xdotool 点菜单不可靠（且强制
`GDK_BACKEND=x11` 会引入 `BadDrawable` 假故障），直接调 C 接口最稳：

```gdb
set pagination off
set confirm off
break gtk_window_present
run
disable 1                          # 必须 disable，否则 call 内部的 present 会再命中
call (void)desmume_tools_open(0)   # IO registers
call (void)desmume_tools_open(2)   # RAM Search
call (void)desmume_tools_open(1)   # RAM Watch
break desmume_tools_update
ignore 2 120                       # 跑 ~120 帧，让窗口真正 map / realize
continue
call (void)desmume_tools_close_all()
call (void)exit(0)
```

`gdb -q -batch -x drive.gdb ./gtk4/desmume | grep -cE "CRITICAL|assertion|Bail out"`：修复前 7，修复后 0。

⚠️ 两个坑：(a) `call` 之前必须 `disable` 掉 `gtk_window_present` 断点；(b) 不要用 `timeout`
强杀进程看结果——强杀产生的 teardown 断言是假象，必须走 `close_all()` 的正常销毁路径。

### P4 运行时修复：Gdk「Duplicate call to gdk_frame_timings_presented()」警告（2026-11-21）

**现象**：运行中偶发一行

```
(desmume:29457): Gdk-WARNING **: Duplicate call to gdk_frame_timings_presented() with different values:
presentation time is 16.358ms off from expected
```

**结论：这不是 gtk4/ 里的 bug，是 GDK 4.24 Wayland 后端的记账问题**，且无功能影响。

**根因链**（全部在 GTK 侧，源码按 `gh api repos/GNOME/gtk/contents/...?ref=gtk-4-24` 核对）：

| 位置 | 行为 |
|---|---|
| `gdk/wayland/gdksurface-wayland.c` `gdk_wayland_surface_request_frame()` | 每次请求一帧就建一个 `wp_presentation_feedback`，只按 `(frame clock, frame counter)` 记账 |
| 同上，`gdk_wayland_surface_handle_empty_frame()` | 只提交状态（opaque/input region、scale、viewport…）的“空帧”也会 `request_frame()`，不带 buffer |
| `gdk/wayland/gdkpopup-wayland.c:1040` | **popup 直接继承父 toplevel 的 frame clock** |
| `gdk/gdkframetimings.c` `gdk_frame_timings_presented()` | 同一帧第二次上报且时间不同 → `g_warning_once`（只打一行） |

于是同一个 frame counter 上会挂两个 feedback（父窗口 + popover/dropdown/tooltip，或正常帧 + 空帧），
两者实际呈现时间差一个刷新周期（60 Hz ≈ 16.4 ms），第二个上报就成了“重复且不一致”。

**上游状态**：`6b0b5ff7`（"wayland: Don't duplicate complete frames"，commit message 里点名
"in particular in frames when popovers were shown/hidden"）已修常见情况，**4.24.1 里已包含**；
`2da76eb2`（"wayland: Rewrite frame callback handling to use frames"）把记账重写成 per-surface frame，
彻底消除这一类警告，但**没有进 4.24 分支**。所以 4.24.1 上仍会偶发。

**影响**：无。`gdk_frame_timings_presented()` 对重复上报直接 `return`，该帧保留第一次的时间戳；
警告本身是 `g_warning_once`，一个进程只输出一行。

**处理**：`main.cpp` 装一个 `g_log_set_writer_func()`，只丢弃 `GLIB_DOMAIN=Gdk` 且消息以
`Duplicate call to gdk_frame_timings_presented()` 开头的这一条，其余（包括其它 Gdk 警告）原样交给
`g_log_writer_default()`。这样既不让上游噪音冒充 DeSmuME 的 bug，也不掩盖真问题。

**验证**：独立小程序装同一个 writer 后发三条结构化日志——目标消息（Gdk + 该前缀）被吞；
`Gdk` 的其它警告照常打印；`Gtk` domain 下同样的文本照常打印（证明匹配条件够窄）。
带过滤器的 `gtk4/desmume` 跑 8 秒测试帧，输出只剩 Mesa 的 dzn 提示。

### P5 已落地内容（2026-11-21）

金手指列表窗口（`gtk/cheatsGTK.cpp` 的 `CheatList`）迁到 GTK4，作为一个工具注册进
`tools.cpp` 的表（index 3，菜单 `app.tool` target=3）。Cheat 文档本身仍是核心
`CHEATS` 对象（`cheatSystem.h`），与 `ramwatch.cpp`/`ramsearch.cpp` 直接 include 核心
C++ 头同构；GTK 层只负责 UI。

| 路径 | 说明 |
|---|---|
| `posix/gtk4/tools/cheats.{h,cpp}` | 金手指列表窗口：`CheatRow` GObject + 两个 `GListStore`（Internal / AR+CB） fed into 两个 `GtkColumnView`（`GtkMultiSelection`）；"On" 列是带 `toggled` 工厂的 `GtkCheckButton`（多选时整组启停）；`GtkNotebook` 两页；`GtkSearchEntry` 按描述/代码/地址/值即时过滤；Add 用 `GtkMenuButton`+`GMenu`（`win.add-internal`/`add-ar`/`add-cb`）；Edit/Remove/Move/数据库按钮；打开时暂停核心并复制 `CHEATS` 快照，关闭走 save/discard/cancel 三态确认（Discard 恢复快照），destroy 时按需要 resume |
| 内部金手指对话框 | grid 表单（地址/十进制值/1–4 字节 `GtkDropDown`/描述/启用开关）+ 实时校验（地址 1–8 位 hex ≤0x0FFFFFFF、值按宽度取范围且接受负数补码）+ 等价 AR code 预览；OK 调 `CHEATS::add/update` |
| AR / Codebreaker 对话框 | `GtkTextView`（等宽）+ 描述 + 启用开关 + 实时校验（`CHEATS::XXCodeFromString`）；OK 调 `CHEATS::add_AR/CB`、`update_AR/CB` |
| Cheat Database 导入 | `GtkFileDialog` 选 `*.dat`（路径存 `config.cheats_database_path`）+ `CHEATSEXPORT::load()` 按当前 ROM 游戏码查找 + 带勾选列的 `GtkColumnView` 多选 + `CHEATS::add_AR_Direct`；错误按 `CheatSystemError` 分类提示 |
| `emulation.{h,cpp}` | 新增 `desmume_emu_set/get_cheats_disabled()`（读写 `CommonSettings.cheatsDisable`）；暂停/恢复复用 `desmume_emu_set/get_running()` |
| `main.cpp` / `menu.ui` | `app.cheatsdisable` stateful action（写 config 并桥接核心），OnActivate 时从 config 恢复；Tools 菜单新增 `_Cheat list…`（`app.tool` target 3）与 `Disable _cheats` |

**GTK4 API 陷阱（本次踩到并修复）**：`GtkMenuButton` 在 GTK4 不是 `GtkButton` 子类，
不能用 `gtk_button_set_label()`（运行时 GObject-CRITICAL），必须用
`gtk_menu_button_set_label()`；`GtkEditable` 有 `get/set_text`、`set_width_chars`，
但 `max-length` 仍是 `gtk_entry_set_max_length(GTK_ENTRY(...))`（无 editable 版本）；
`gtk_widget_set_placeholder_text` → `gtk_search_entry_set_placeholder_text()`；
`g_list_model_get_item()` 返回 `gpointer`，C++ 下需显式 `G_OBJECT(...)` cast。

**范围说明**：`gtk/cheatsGTK.cpp` 里的 `CheatSearch`（基于 `CHEATSEARCH` 的简单内存搜索）
未单独迁移——P4 的 RAM Search 工具已提供同类搜索且功能更强（4 类 × 8 比较 + Add Cheat
桥接），重复迁移只会让 Tools 菜单出现两个入口。需要时可在后续阶段补一个薄封装工具。

验证：Arch/WSL GTK 4.24.1 下 `ninja gtk4/desmume` 全绿（编译链接无新增警告）。gdb 注入
`desmume_emu_init()` + 强制 ROM 标志后 `desmume_tools_open(3)` 打开/关闭金手指窗口全程无
CRITICAL/ERROR/Gtk 警告（仅 Mesa dzn 提示）。切 `-Dfrontend-gtk=gtk3` 后 GTK3 + CLI
构建无回归，再切回 gtk4 重链通过。

### P6 已落地内容（2026-11-21）

P6 把 §6.2 的呈现侧优化收口。其中两项在 P2b 就已随 `display.cpp` 落地，本次补齐剩余
的可量化部分：

| 优化项 | 状态 / 实现 |
|---|---|
| §6.2-1 GPU 缩放代替 CPU 缩放 | P2 已落地：texture 路径把缩放交给 GSK，无 `cairo_scale` per-frame |
| §6.2-2 不可见时停止呈现 | P2b 已落地：`push_frame()` 里 `!gtk_widget_get_mapped()` 直接返回，不建纹理、不 `queue_draw`；模拟/音频独立继续 |
| §6.2-2b 非活动窗口降频 | **本次新增**：`display.cpp` 懒连接 toplevel 的 `notify::is-active`（首帧才连，避免无 WM 环境误判失焦黑屏）；失焦时呈现节流到 ~10 fps（`INACTIVE_PRESENT_INTERVAL_US=100000`），模拟/音频全速 |
| §6.2-3 纹理双缓冲 + 零拷贝 GBytes | **本次重构**：`FramePool` 从 display 移到 emulation 层并按字节尺寸分桶；颜色转换直接写入池化缓冲（消除 `VideoFilter::RunFilter` 的 full-frame memcpy），`g_bytes_new_with_free_func()` 移交所有权给 display，`g_bytes_new_from_bytes()` 切出上下屏两个零拷贝子视图。每帧像素工作量与 GTK3 相同，额外拷贝为 0（GTK3 仍有 filter 的一次 memcpy） |
| §6.2-4 resize 无 configure 风暴 | P2 已落地：单 surface，`snapshot()` 按当前尺寸即时算几何 |
| §6.2-6 状态栏去抖 | 已在位：`SetStatus()` / `UpdateSpeedLabel()` 只在文本变化时 `gtk_label_set_text()` |
| §6.3 呈现计时 | `DESMUME_GTK4_TIMING=1` 时记录 upload（两张零拷贝子 `GBytes` + 两张 `GdkMemoryTexture`，颜色转换与池化在 emulation 层另有计时）与 snapshot（GSK 合成准备）两条序列，每 120 呈现帧打印均值/p50/p95/max 到 stderr；默认关闭，热路径无时钟读取/无分配 |

**设计要点**：计时开关由 `timing_init()` 经 `g_getenv()` 一次性读取（`class_init` 触发），
关闭时 `push_frame`/`snapshot` 里的 `g_get_monotonic_time()` 调用被短路，不留运行开销。
两条序列独立采样计数（GTK 调 `snapshot()` 的次数可能不等于 `push_frame()` 次数），
各报各自 `n`；p95 用排序后 `n*95/100` 下标，小样本也安全。

**实测**（Arch/WSL GTK 4.24.1，`DESMUME_GTK4_TEST_FRAME=1` 合成帧，6 秒）：
零拷贝重构后 upload 均值约 16–19 µs、p50 约 15–17 µs、p95 约 26–35 µs；snapshot 均值约 5 µs、
p50 约 5 µs、p95 约 6 µs。重构前同一测试 upload 均值约 124–131 µs、p95 约 174–190 µs，
本次消除 full-frame `memcpy` + 逐像素 alpha 写后下降约 7–8 倍。每窗口 120 帧稳定输出，
无 CRITICAL/断言。该数字是合成测试帧基线，真实 ROM 的颜色转换成本量级不变，但不再有
GTK3 `VideoFilter` 与旧 GTK4 `wrap_frame` 的两次全帧拷贝，GSK 合成成本随窗口尺寸变化——
§1.3 的"≤ 基线 50%"需在目标硬件上用同一工具对 GTK3 Cairo 路径做 A/B 才能定论，
本环境无基线数据，不做虚假承诺。

验证：`ninja gtk4/desmume` 无新警告；`DESMUME_GTK4_TIMING=1` 输出符合预期且关闭时零输出；
切 `-Dfrontend-gtk=gtk3` 后 GTK3 + CLI 构建无回归，再切回 gtk4 重链通过。

### P7 已落地内容（2026-11-21）

P7 是文档标注的必做项：GTK4 的视频区不是原生 surface，`WGPU_PresentFrame()` 的
X11 XID / Wayland wl_surface 直写路径失效。采用 P7a（离屏渲染 + 回读），避开 P7b
原生子 surface 与 GSK 堆叠/裁剪冲突的实验性风险。

| 路径 | 说明 |
|---|---|
| `src/frontend/shared/wgpu_present.{h,cpp}` | 新增 `beginOffscreen(w,h,backends)` / `presentOffscreen(...)`。`present()` 内部把"拿 surface texture + present"抽象成离屏分支：目标换成自建 BGRA8Unorm 纹理（`RENDER_ATTACHMENT \| COPY_SRC`），其余渲染（帧上传、upscaler chain、HUD overlay、gap、旋转、顶点）完全复用；提交后 `copyTextureToBuffer` → `mapAsync(AllowProcessEvents)` → `devicePoll` 等待 → 按 256B 行距解包到紧密 BGRA。离屏资源在 `Destroy()` 统一释放；无 wgpu 构建下均为 stub |
| `gtk4/emulation.{h,cpp}` | 新增 `desmume_emu_set/get_gpu_filter()`、`desmume_emu_gpu_filter_active()`、`desmume_emu_self_test_gpu_filter()`（诊断）。`present_current_frame()` 在 `VideoFilter::RunFilter()` 之后，若滤镜激活则把帧交给 wgpu 离屏渲染，回读放大帧再推给同一个 display sink（布局/旋转/间隙不变，仍走 GSK texture）；任何失败回退到未滤镜帧，不黑屏。桥接用纯 int/float，GTK 层无需 WebGPU 头 |
| `gtk4/desmume.gresource.xml` | 打包 23 个 libretro upscaler WGSL pass（与 GTK3/Windows 相同 alias），`WGPUCreateLinuxUpscalerShaderSource()` 按 alias 从 `/org/desmume/DeSmuME/` 按需读取、惰性编译 |
| `gtk4/main.cpp` / `menu.ui` | OnActivate 从 `config.presentationWGPU && config.gpuFilter` 恢复滤镜；`app.gpufilter` 字符串 radio action（13 档：Off/XBRZ/Lanczos3/Bicubic/XBR LV2/LV3/SuperXBR/ScaleFX/XBR2x/MLV4/PT）+ 写回 config；View 菜单新增「GPU Filter」子菜单。缩放系数取 `config.gpuScaleFactor`（1–4×，钳制在桥接层） |

**与 GTK3 的差异（明确记录）**：GTK3 的 GPU 滤镜作用在已按 `gpuScaleFactor` 放大的核心帧缓冲上，
且直接 present 到 drawing area 的原生 surface；GTK4 的核心帧保持原生 256×192（未改
`SetFramebufferDimensions`），滤镜在 present 阶段按 scale 放大到离屏 target 后回读，
再交给 GSK 合成。像素输出在整数倍缩放下与 GTK3 接近，但非整数倍/旋转组合因回读路径
不同可能有细微差异——当前环境无真实 ROM 与真 GPU，未做逐像素 A/B。

**验证**（Arch/WSL GTK 4.24.1 + wgpu-native dzn 软件 Vulkan）：
- `glib-compile-resources` 成功产出 80 KB gresource；`gtk4-builder-tool validate menu.ui` 通过
- `ninja gtk4/desmume` 全绿无新警告
- gdb 注入 `desmume_emu_set_gpu_filter()` 后调用 `desmume_emu_self_test_gpu_filter()`：
  XBRZ(1) / Lanczos3(2) / SuperXBR(8) / ScaleFX(9) 均返回 1，证明渲染→map→回读→非空像素
  全链路成功，无 "shader is missing"/CRITICAL/SIGSEGV；禁用滤镜后恢复默认路径
- 默认无滤镜路径启动干净（仅 Mesa dzn 无关提示）
- 切 GTK3 + CLI 构建无回归，切回 gtk4 重链通过

`desmume_emu_self_test_gpu_filter()` 是保留的诊断入口，供自动化冒烟测试；不接显示 sink。

---

### P8 已落地内容（2026-11-21）

P8 完成发布收尾。原则仍是 `gtk/` 一行不改——所有新资产都在 `gtk4/`。

| 资产 | 说明 |
|---|---|
| `gtk4/org.desmume.DeSmuME.GTK4.desktop` | GTK4 专属启动器：`Name=DeSmuME (GTK4)`、`Exec=desmume`、`TryExec=desmume`、`StartupWMClass=org.desmume.DeSmuME.GTK4`、`Icon=org.desmume.DeSmuME`（复用图标）、MIME 同 GTK3。独立 app-id 使 GTK3/GTK4 的 desktop 可在同一 prefix 共存 |
| `gtk4/org.desmume.DeSmuME.GTK4.metainfo.xml` | `<id>`/`<launchable>` 与 desktop 一致；现代 `<developer><name>`、https 主页、release 2.7.10。`appstreamcli validate --no-net` 普通模式全绿；pedantic 下仅剩 app-id 含大写（D1 既定，与 GTK3 一致，不改） |
| `gtk4/doc/desmume.1` | GTK4 版 man：保留共享核心选项，新增 `DESMUME_GTK4_TEST_FRAME` / `DESMUME_GTK4_TIMING` / `GDK_BACKEND` 环境变量说明与 GPU 滤镜描述。`groff`/`man -l` 渲染无警告 |
| `gtk4/meson.build` | 用 `install_data` 安装 GTK4 desktop/metainfo、复用 `../gtk/org.desmume.DeSmuME.svg`、`install_man('doc/desmume.1')`。一次配置只装一个前端，man page 与二进制共享名不会同时指向两个二进制 |
| `.github/workflows/build.yml` | `build_gtk4` job 依赖加 `desktop-file-utils`/`appstream`，构建后新增 validate 步骤（desktop/metainfo/menu.ui/shortcuts.ui）与 destdir 安装冒烟（检查二进制、desktop、metainfo、man） |

**弃用审计**：全项目清洁重建（238 目标）后扫描——`gtk4/` 前端目录零编译警告；无 `GTK_DISABLE_DEPRECATED`、无 `GtkTreeView`/`GtkFileChooserNative`/`GtkStatusbar` 等实际调用（`luaScriptConsole.cpp` 的 `gtk_button_set_label(GTK_BUTTON(...))` 作用在真正的 `GtkButton`，正确）。核心/`3rdparty` 的历史警告与本次迁移无关。

**安装共存验证**（同一 destdir 先后安装 GTK3、GTK4）：
- `share/applications/`：同时存在 `org.desmume.DeSmuME.desktop` 与 `org.desmume.DeSmuME.GTK4.desktop`
- `share/metainfo/`：两份 metainfo 共存
- `bin/desmume`、`share/icons/.../org.desmume.DeSmuME.svg`：共享名各一份（后装覆盖，始终只有一个二进制对应 `man desmume`）

**验证**：CI 校验命令本地逐条跑通；workflow YAML 解析通过（5 个 job 齐全）；`gtk4-builder-tool validate` / `desktop-file-validate` / `appstreamcli validate` 全通过；destdir 安装布局与共存正确；GTK3 + CLI 回归通过，切回 gtk4 重链通过。

**仍未实机验收**（本环境限制，非代码缺口）：KDE/GNOME Wayland 外观、真 ROM 下的逐项功能与像素 A/B、30 分钟 RSS 泄漏、ASAN、GitHub CI 实跑——见 §12 标 `[实机]` 项与 §13 仍待确认事项。

### 运行时修复：Config 对话框关闭后再打开不显示（2026-11-21）

**现象**：打开 Config → 关闭 → 再打开，对话框不再出现；控制台报
`(desmume:…): Gtk-WARNING **: A window is shown after it has been destroyed. This will leave the window in an inconsistent state.`

**根因**（最小 GTK4 程序逐行复刻生命周期、同一版本 4.24.1 上 A/B 复现确认，非推测）：
`configdialog.cpp` 创建窗口后调用了 `g_object_ref_sink(d->window)`。GTK4 里
`gtk_window_new()` 返回的引用**已经不是 floating**，`ref_sink` 等于额外 `+1` 引用。
于是关闭时 `gtk_window_destroy()` 只释放 GTK 自己那份，对象因多出的引用没有 finalize，
**`"destroy"` 信号根本不触发**（实测 `destroy_fired=0`）。`OnDialogDestroy` 从不执行，
全局 `sDialog` 一直指向已销毁的窗口；回调里那句 `g_object_unref(widget)` 也永远轮不到。
第二次打开时 `sDialog != NULL`，直接对已销毁窗口 `gtk_window_present()` → 警告 + 不显示。

**修复**（`configdialog.cpp`）：删除创建时的 `g_object_ref_sink()`，删除
`OnDialogDestroy` 里的 `g_object_unref(widget)`，让 GTK 独占窗口唯一引用，`destroy`
得以正常同步发出；两处都加注释说明为何不能再加回引用，防止回归。

**教训（GTK3 → GTK4 引用语义）**：
- GTK3 里 `gtk_window_new()` 是 floating，`g_object_ref_sink()` 是标准接管手法；GTK4 里窗口构造出来就不是 floating，照搬这句话会凭空多一个引用，是最容易踩的迁移坑之一。
- 判断一个窗口/对象是否该 `ref_sink`，看构造函数文档里的返回值传递标注（transfer floating / full），不要凭 GTK3 肌肉记忆。
- `"destroy"` 信号依赖引用归零触发 finalize；任何多余引用都会让它静默不触发，且不产生任何警告——这类 bug 只在"关闭后重开"路径暴露，首次打开完全正常。
- 单例窗口的清理回调（置空全局指针）优先用 `g_object_add_weak_pointer` 自动置空（见 `ioregs.cpp` 等工具窗口），比手动连 `destroy` 更稳健。

**验证**：最小复现变体 A（保留 ref_sink）稳定复现警告且 `destroy_fired=0`；变体 B/C（去掉 ref_sink）两轮"打开→销毁→重开"零警告、`sDialog` 正确置空、ASAN 零错误；真实 desmume 由用户实机确认开关正常。`ninja gtk4/desmume` 干净编译无警告。

### P7c：WGPU/OpenGL 3D 后端 + Screen presentation 下拉框（2026-11-21）

**目标**：把 GTK3 图形设置里有、GTK4 缺的两块补齐——(1) 3D Core 下拉框能选 WGPU/OpenGL 后端；(2) 独立的 "Screen presentation" 渲染器下拉框。

**改动**：
- `emulation.cpp`：`core3DList` 从写死两项改为 `build_3d_renderer_list()` 动态构建（幂等、lazy），把 GTK3 的 `Build3DRendererList()` 逻辑搬过来——固定核心 → OpenGL 核心（条件编译）→ wgpu-native 枚举的各后端 → 无 adapter 回退 `gpu3DWGPU`。关键点：
  - 必须保持**全局符号**（`render3D.h` 里 `extern`，核心 `Change3DRendererByID()` 直接按下标索引），加 `static` 会链接失败
  - `OGLRender.h` 在无 OpenGL 构建下直接 `#error`，`/include/` 必须放进 `ENABLE_OPENGL_*` 条件里（GTK3 用的是 `OGLRender_3_2.h` / `OGLRender_ES3.h`）
  - lazy 构建挂在 `renderer_count()` 和 `desmume_emu_init()` 上，覆盖"配置对话框在核心初始化前打开"的场景
- `emulation.{h,cpp}`：新增 presentation backend 桥接——`presentation_backend_count/label/mask()` 暴露 wgpu 后端枚举（GTK 层不碰 `WGPURender.h`），`set/get_presentation_backend(mask)` 把后端掩码存进 `sGpuFilterBackend`，切换时重建离屏设备。
- `configdialog.cpp`：图形页 "WebGPU presentation" 段加 "Presentation:" 行（WebGPU 各后端 + Software (Cairo)），与 GPU filter 做 sensitivity 联动；Apply/Populate 双向同步 `presentationWGPU` / `presentationWGPUBackend` / `gpuFilter`。
- `main.cpp`：启动恢复时把 `presentationWGPUBackend` 灌进桥接层，再设 GPU filter。

**GTK3 → GTK4 的适配差异**：GTK3 的 presentation 是原生 subsurface（直接 present 到子窗口），GTK4 视频区不是原生 surface（P7a），所以下拉框的 WebGPU 项落地到**离屏滤镜设备的 backend 掩码**（`beginOffscreen` 第三参数）而非子 surface；软件项等价于"filter Off + 走 GdkTexture"。语义一致，底层路径不同。

**验证**：三种构建全部 `ninja gtk4/desmume` 通过且无警告——默认 `wgpu=true`、`-Dwgpu=false`、`-Dopengl=true`。无 GPU 的 headless 环境里 wgpu-native 的 Vulkan 枚举会在进程启动阶段挂起（环境问题，与本次改动无关），故未做 GUI 实机截图；bridge 的列表构建逻辑此前已用 gdb 在无 wgpu 构建上 dump 验证（Null + SoftRasterizer，下标稳定）。

**未做**：GPU OSD（`presentationWGPUHud`）——依赖整套 AGG HUD 绘制管线（`agg_osd.cpp` / `DrawHUD` / 字体 / `Agg_setHudTarget` 钩子 / Lua `agg.*`），GTK4 前端完全没有这套子系统，属独立大工程，单列为后续任务。

---

## 附录 A：迁移顺序速查

```
复制 gtk/ → gtk4/
  ├─ config.*, desmume.*, dTool.h, dToolsList.cpp   原样
  ├─ meson.build                                   改 target/依赖
  ├─ main.ui                                       重写外壳
  ├─ menu.ui                                       原样
  ├─ emulation_settings.ui / graphics.ui           重写
  └─ *.cpp                                         按 §8.8 逐文件改
先让 P0 空窗口跑起来 → P1 外壳（SSD 优先）→ P2 显示/输入
→ P3 对话框 → P4 工具 → P5 金手指 → P6 性能 → P7 wgpu（必做）
```

## 附录 B：关键参考

- GTK4 迁移指南：<https://docs.gtk.org/gtk4/migrating-3to4.html>
- `GtkFileDialog`：<https://docs.gtk.org/gtk4/class.FileDialog.html>
- `GtkAlertDialog`：<https://docs.gtk.org/gtk4/class.AlertDialog.html>
- `gtk_snapshot_append_texture`：<https://docs.gtk.org/gtk4/method.Snapshot.append_texture.html>
- `GdkMemoryTexture`：<https://docs.gtk.org/gdk4/class.MemoryTexture.html>
- Wayland 后端专用 API：<https://docs.gtk.org/gdk4/wayland.html>
- 装饰协商行为（维护者说明）：<https://discourse.gnome.org/t/server-side-decorations-in-gtk-a-proposal/16029>
