# macOS arm64 实机验证清单（MBP Apple Silicon）

> 目的：在真机上确认 S2 之后 arm64 后端可用、且 §3.7 的块级语义没有退化。
> 前置状态：x86 与 arm64 两个后端都已在 asmjit master 上（提交 `49e5fe6c` / `da517591`），
> 两个构建工程刚接线完成（`29f2a3fb`），**尚未在任何 Apple 编译器上构建过**。

## 0. 已经验证到哪一步（今晚，Android Termux / aarch64）

设备：`192.168.1.4:8022`，Android 16，clang 21.1.8，8 核。

| 检查 | 结果 |
|---|---|
| asmjit + 后端在真实 aarch64 上编译 | 45 个 asmjit TU + `a64_dump_new` / `a64_shape_probe` / `a64_exec_probe` 全部通过 |
| 编码等价（236 个原语样例，capstone 逐条反汇编比对） | **236/236 一致**（2 例为 asmjit 的 dead-branch 消除，已登记） |
| 生成代码**真的执行**（25 个用例：ALU / 移位 / 立即数构造 / cmp+cset / 条件分支 / 乘法 / 存取） | **25/25 通过** |
| 块中途 `ret` 形状 | 通过（早退后的尾巴没被吃掉） |

也就是说：`emit_*` 生成的代码在 AArch64 硬件上跑得对，落地路径（flatten → relocate →
copy → icache flush）也是通的。**还没有验证的**是 Apple clang + libc++、Metal/Xcode 工程本身、
以及整机行为（下面这些步骤）。

## 1. 构建前自检（30 秒）

```bash
python3 tools/jit_matrix/check_xcode_asmjit.py
```

最后一行必须是 `wiring looks complete`。它会检查：括号配平、54 个 asmjit 源都被引用且文件存在、
每个编 JIT 的 target 都带 `-std=c++20`、每个配置都有 include 根 / `ASMJIT_STATIC` / PCH 关闭。

## 2. 构建

**target 必须选 AppleSilicon 那几个，不能选 Universal。**

| target | 编 JIT 吗 | 说明 |
|---|---|---|
| `DeSmuME (macOS App; AppleSilicon dev+ -- Latest Xcode)` | **是**（`arm_jit_arm.cpp`） | 推荐：Release 有优化 + GDB stub + Lua |
| `DeSmuME (macOS App; AppleSilicon -- Latest Xcode)` | 是 | 最朴素的 arm64 构建 |
| `DeSmuME (macOS App; Release Stage Final AppleSilicon -- Latest Xcode)` | 是 | 发布用，LTO |
| `DeSmuME (macOS App; Universal -- Latest Xcode)` | **否** | 两个 slice 会重复定义 `arm_jit_*`，属于遗留缺口；用它测不到 JIT |

```bash
cd desmume/src/frontend/cocoa
xcodebuild -project "DeSmuME (Latest).xcodeproj" \
  -target "DeSmuME (macOS App; AppleSilicon dev+ -- Latest Xcode)" \
  -configuration Release build
```

GUI 也可以：打开工程，scheme 选同一个 target，Configuration 选 Release，⌘B。

## 3. 构建失败对照

| 症状 | 原因 | 处理 |
|---|---|---|
| `Undefined symbols: asmjit::...` | asmjit 源没进 target | 重跑 `python3 tools/jit_matrix/wire_asmjit_xcode.py --apply` |
| `'asmjit/core.h' file not found` | include 根没加上 | 同上（会补 `$(SRCROOT)/../../../3rdparty`） |
| `PCH file was compiled with a different language standard` | 前缀头还在预编译 | 同上（会设 `GCC_PRECOMPILE_PREFIX_HEADER = NO`） |
| `#error "arm_jit_arm.cpp is the AArch64 host backend only"` | 选到了 Intel / Universal target | 换 AppleSilicon target |
| `asmjit::Assembler` 重复定义 | 旧 `src/utils/AsmJit` 又被编进去了 | 同上（会把旧源从 Sources 里摘掉） |

接线脚本是幂等的，重跑不会重复插入。要整体回退：
`git checkout -- "desmume/src/frontend/cocoa/DeSmuME (Latest).xcodeproj/project.pbxproj"`。

## 4. 运行设置

1. **引擎**：默认是 Interpreter，必须切到 **Dynamic Recompiler**（Emulation 菜单里的引擎选择）。
   没切的话测的是解释器，JIT 一行都没跑。
2. **Block Size**：在设置弹窗里（`MainMenu.xib` 里绑定的是 `selection.maxJITBlockSize`，标签 "Block Size:"），
   范围 1–100，默认 12。
3. **确认 JIT 生效**：About 窗口在非 `PUBLIC_RELEASE` 构建里会显示 JIT 状态。

## 5. 整机验收：block size 三档扫描（对应方案 §3.7）

《恶魔城 苍月十字架》：

1. 读档到血条出现（或新游戏到 HUD 出现）；
2. **开菜单 → 关菜单**（这一步才会触发 HUD 重建）；
3. block size 分别设 **15 / 16 / 100**，每档重复一次 1–2。

判据（缺任何一个都算失败）：

- 血条外框在，内部**深色底板**在
- “120” 数字、红色 HP 段、绿色 MP 段都在
- 伤害数字、对话框边框不缺块

失败特征就是 `2d4919aa` 那次的样子：HUD 内部整块消失，露出背景，外框还在。
注意：**调回 15 不会自动恢复**，只有再次开关菜单才会重建——所以每次都要重新开关菜单。

## 6. 字节级证据（可选，但这是唯一能"逐字节对比"的一层）

脚本：`tools/hud_probe/castlevania_hudprobe.lua`（从 `__bins/Lua/` 里那份复制来的，已改成读 `HUD_OUT`）。

- 输出目录用环境变量给，从终端直接启动 app 里的可执行文件才带得上环境变量
  （`open` 不传递环境变量）：
  ```bash
  mkdir -p ~/hudlogs
  HUD_OUT=$HOME/hudlogs /path/to/DeSmuME.app/Contents/MacOS/DeSmuME
  ```
- 脚本第 3 帧读 **即时存档 2**：需要把
  `desmume/src/frontend/windows/__bins/StateSlots/0121 - Castlevania - Dawn of Sorrow (U)(Legacy).ds2`
  放到 macOS 端的存档目录并确保它是 slot 2；没有就手动读到 HUD 再跑脚本（把 `savestate.load(2)` 那行去掉）
- 判据：关菜单后对暂存区 `0x02100358..0x02100557` 的 **32 位写入 = 2048 次，覆盖 128 个地址**。
  其它数字（16 位写入 1952 次等）见 `jit-bug.md` §4.1。

## 7. 性能基线（S4 的门槛要用）

同一台机器、同一天、同一个场景（HUD 打开静止），记三组：

| 组 | 引擎 | block size | 冷启动 JIT 编译耗时 | 平均帧率 |
|---|---|---|---|---|
| A | Interpreter | — | — | |
| B | Dynamic Recompiler | 12（默认） | | |
| C | Dynamic Recompiler | 100 | | |

## 8. 带回来的东西

- `xcodebuild` 的完整输出（失败时尤其）
- 三档的截图（HUD 正常/异常）
- 上表的性能数字
- 任何崩溃的 backtrace（`Console.app` 里搜 DeSmuME，或终端里跑二进制直接看 stderr）
- 如果跑了 §6：`HUD_OUT` 目录下的 `hud7_*.log` 和 `stage_*.bin`
