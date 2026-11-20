# DeSmuME GTK3 → GTK4 迁移方案

> 迁移对象：`desmume/src/frontend/posix/gtk/`（GTK3，约 53 万字节 C++，25 个 `.cpp`/`.h`，目录内共 32 个文件）
> 迁移目标：`desmume/src/frontend/posix/gtk4/`（新增目录，GTK4）
> 原则：**`gtk/` 一行不改，保持可编译、可发布**；所有新工作只进 `gtk4/`
> 基线版本：**GTK ≥ 4.10**（理由见 §3 D2），实际验证目标 4.14+
> 状态：P0/P1 已完成；P2a（显示 widget）已落地，P2b/P3 待做（§14）
> 日期：2026-11-20

---

## 0. 结论先行

### 0.1 四个诉求的可行性

| 诉求 | 可行 | 结论 |
|---|---|---|
| 完整迁移到 GTK4 | 是 | 所有 GTK3 API 都有对应物；工作量集中在 44 处 `gtk_dialog_run` 的异步化 |
| UI 明显更好看 | 是，收益大 | 现有代码已经是 GMenu + GAction 架构，换成 `GtkHeaderBar`/`GtkPopoverMenuBar` 主要是改外壳，不是重写 |
| 性能提升 | **部分可以** | 帧呈现、缩放、resize、后台停绘有真实收益；模拟器核心和 RAM Search 算法不会因为 GTK4 变快 |
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

触控笔优先用 `GtkGestureStylus`（提供 `stylus-down/up/motion` 和设备信息），鼠标/触摸回退到 `GtkGestureClick`。坐标变换继续复用现有的 `nds_screen.touch_matrix`（`cairo_matrix_t` 原样保留）。

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

- 在 `DesmumeDisplay::snapshot()` 与纹理创建处加 `g_get_monotonic_time()` 计时，`DESMUME_GTK4_TIMING=1` 时每 120 帧打印一次均值/p95
- `perf record -g ./desmume`，看 `cairo_*`、`gsk_*`、`gdk_texture_*` 占比
- `hyperfine --warmup 3 './desmume rom.nds'`（配合自动退出参数）
- resize：`xdotool`/`kdotool` 脚本循环改窗口大小，记录帧间隔
- 内存：`smem -P desmume`、`valgrind --tool=massif`（短会话）

### 6.4 wgpu 路径（分阶段，风险最高）

GTK4 里 `GtkDrawingArea` 不再有独立 `GdkWindow`，`WGPU_PresentFrame()` 的 `gdk_x11_window_get_xid()` / `gdk_wayland_window_get_wl_surface()` 路径失效（只能拿到整个 toplevel 的 surface，直接 present 会盖住菜单栏）。

处理顺序：

- **P1–P5**：`presentation=texture` 为默认；配置里的 GPU 呈现选项置灰并提示"GTK4 版暂不可用"
- **P7a（推荐）**：wgpu 离屏渲染 + 回读
  - `WGPUPresent` 增加"渲染到 offscreen texture"模式，HUD/OSD 合成进同一 target
  - `wgpuBufferMapAsync` 回读 → `GdkMemoryTexture` → 正常 `snapshot()`
  - 只在"选了 GPU 滤镜"且输出 ≤ 1080p 时启用；4K 下回读带宽约 1.3 GB/s，不划算
- **P7b（可选/实验）**：原生子 surface
  - X11：在 toplevel XID 上 `XCreateWindow` 子窗口覆盖视频区
  - Wayland：`gdk_wayland_surface_get_wl_surface()` + `wl_subcompositor` 子 surface（现有 `WGPULinuxDisplay::beginWayland` 已是这个思路）
  - 定位用 `gtk_widget_compute_point(widget, toplevel, ...)` × scale
  - 明确标注"实验性"：GSK 不知道这个窗口，堆叠/裁剪/全屏切换都可能出问题

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

**功能**
- [ ] 所有 §1.1 条目可用，行为与 GTK3 版一致
- [ ] GTK3 与 GTK4 两个 target 可同时构建、同时安装、互不影响
- [ ] 配置文件互通；GTK4 新增键有默认值，不影响 GTK3

**外观**
- [ ] KDE Wayland：窗口装饰由 KWin 绘制（SSD），最大化/平铺/全屏正常
- [ ] GNOME Wayland：`GtkHeaderBar`，无重复标题
- [ ] X11：WM 装饰，无自绘标题栏
- [ ] 视频区纯黑、等比、无闪烁；全屏自动隐藏 UI
- [ ] 深浅色主题下文字/控件可读

**性能**
- [ ] §1.3 表格指标全部达标（或给出书面解释）
- [ ] 最小化时无持续呈现
- [ ] resize 无黑边/闪烁

**质量**
- [ ] `gtk4-builder-tool validate` 通过
- [ ] 无新增编译警告（弃用警告单独统计，不阻断）
- [ ] 30 分钟连续运行无泄漏（RSS 稳定）
- [ ] ASAN 短会话无错误
- [ ] CI 双前端 job 绿

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
| P2 显示/输入 | **P2a 已完成** | `DesmumeDisplay` + GdkTexture 呈现 + 基本布局/输入控制器；P2b（混合布局精调、触控笔、逐像素对齐）待做 |
| P3 对话框 | 未开始 | |
| P4 工具 | 未开始 | |
| P5 金手指 | 未开始 | |
| P6 性能 | 未开始 | |
| P7 wgpu | 未开始（必做） | |
| P8 收尾 | 未开始 | |

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

P2a 的边界（P2b/P3 再做）：

- 混合布局（hybrid-equal/vertical）目前是近似放置，没有照搬 GTK3 `calc_hybrid_vertical_params()` 的比例运算，
  也没有 `hybridBigScreenScale` 与两屏触控矩阵的逐像素对齐；
- 纹理每帧多一次拷贝（为把 BGRX 的 X 强制成 `0xFF`）；§6.2 的双缓冲 `GBytes` 池留到 P2b；
- 没有 `GtkGestureStylus`，触控笔走不到；
- 帧来源只是测试钩子，真实帧缓冲与主循环在 P3 接入；`nds_screen.touch_matrix` 的转换同样留在 P3。

验证：Arch/WSL GTK 4.24.1 下完整链接通过；编译期无 `display.cpp`/`main.cpp` 警告。
视觉效果（KWin/GNOME/X11、旋转、混合布局）还需要带显示环境的人工验收。

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
