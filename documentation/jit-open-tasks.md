# JIT 迁移待办追踪

> 本文把 `asmjit-jit-migration-plan.md`（下称「主文档」）中散落的未完成项集中成一份可勾选清单。
> **主文档仍是权威来源**，本文是索引视图；状态变化时两处同步更新。
>
> 快照：2026-11-20，HEAD `7246f4bd` + 本轮未提交改动（族 6 迁移与 bank 串值修复，见主文档 §S4 族 6）。

## 当前进度快照

| 项 | 状态 |
|---|---|
| 迁移主线（571 组 `diff-pass`） | **293 / 571**：族 1 数据处理 28 + 族 2 ARM 移位 224 + 族 5 乘法/乘加 28 + CLZ + **族 6 分支/状态/异常 12**；余 278 组 |
| P0 访存 ABI（arm64 `call_mem`） | **已落地提交**：`call_mem` / `_r7` / `_r2` 三变体，24 处调用点中 15 处转轻量（commits `b7225baa`、`b1f5b455`）；**原语级真机执行已验证**（`a64_memcall_probe` 15/15），块级语义待 macOS |
| 族3 代码迁移 | **x86 结构 + arm64 结构级已完成**；卡 `diff-pass` 的是 arm64 接入 `ArmRegCache`（§9.5 P3）+ macOS 三方差分 |
| 访存差分安全网 | **本机已加固**（主文档 §10.8）：`kProbes` 新增 20 条访存定向探针，38/38 全绿；此前 `--full` 只测 `LDR r0,[r0]` 退化形式 |
| **arm64 handler 级真机验证** | `a64_block_probe` 在 Termux aarch64 上跑真实的 `compile_basicblock()` 生成块（68 个桩符号补齐核心）；族 5、族 6 都是"迁移前后逐字节相同 + 少数刻意变化"的判据 |
| **bank 搬移不变量** | 本轮确立：`armcpu_switchMode()` 靠读内存完成 bank 搬移 → **调用前必须 `flush()`**。6 处调用点全部满足（`S_DST_R15` 原本就有）；真机两指令块证明不修会双向串值 |
| x64 fastmem | **已评估放弃**（主文档 §10.6），x64 保持现状 |
| 本机验证 | x86 `jit_diff --full` 81920 次比较功能桶全 0；arm64 编码层 `check_a64` 4/241 已知基线差异；**真机**：`a64_exec_probe` 21/21、`a64_memcall_probe` 15/15、`a64_block_probe` 38/38 |
| 未跟踪文件 | `issue.md`（会话开始前既有，不属本次迁移） |

---

## 一、验证类 —— 需 macOS 整机，本机无法完成（最高优先）

本类直接关系刚提交的 arm64 `call_mem` 改动（15 个调用点），是唯一可能引入功能回归的未验证面。

- [x] **§10.8 访存差分安全网（本机）**：差分台 `kProbes` 新增 20 条访存定向探针（写回 / Rd≠Rn / 宽度 / 符号扩展 / 寄存器偏移 / Thumb SP 相对 / PUSH-POP），38/38 探针 0 mismatch，`--full` 功能桶全 0。这是本机侧能做的加固；**不替代**下面的 macOS arm64 语义验证。
- [x] **`call_mem` 原语级真机验证（Termux aarch64，本轮）**：`tools/jit_matrix/a64_memcall_probe.cpp` 15/15。负对照（同函数去掉 stp/ldp）证明 helper 真的把 x12/x15/x2 毁成 `BEEF0000`/`F00D0000`/`DEAD0000`，正例证明 `call_mem` 保 x12/x15、`call_mem_r2` 保 x2+x12+x15，x21 靠 AAPCS64 callee-saved 免费保住；1e6 次调用验 `call_mem_r2` 的栈收支与 canary。
- [x] **handler 级真机验证通道（族 5 建立，族 6 扩展）**：`tools/jit_matrix/a64_block_probe.cpp` 直接 include `arm_jit_arm.cpp`、用 68 个桩符号补齐核心，在真机上执行 `compile_basicblock()` 生成的块。族 6 把其中的 `armcpu_switchMode` 桩换成**真正搬移 bank 的实现**（否则"缓存没 flush"这类 bug 一条都测不出来），并支持两指令块。**新增族时优先扩这个探针，而不是等 macOS。**
- [ ] **§10.5/§10.7 访存差分（macOS / arm64）**：`build_diff_macos.sh` 重建后跑 `jit_diff --full`，确认访存槽位功能桶（R0–R14 / CPSR / `instruct_adr` / 内存写集）全 0。重点覆盖：LDR/STR 的 Rd==15 与写回变体、**LDRD/STRD 的 W 位写回、Thumb PUSH/POP、SWP、LDM/STM**。注意 macOS 的 `kProbes` 与本机同步后同样是 38 条。
- [ ] **§10.5 代码体积**：`jit_diff --block --slot` 实测 LDR/STR 边际指令数，与 §10.5 推算的「下降约 25–40%」对照。
- [ ] **§10.5 整机 ROM**：花屏 / DMA 提前退出路径回归，确认缓存脏值在 helper 调用后未丢失。
  - 回退预案：任一族差分报错，把该族调用点单独换回 `callR3`/`callR7`（改动按族分点，回退粒度细）。
- [ ] **§8 三方差分**：571 条 × 16 条件组 × 8 边界向量，`hash(interp) == hash(x86 JIT) == hash(arm64 JIT)`。
- [ ] **§8 跨机器自检**：`hash(interp)` 逐条相同（差分台自检）。
- [ ] **§8 条件 cycle**：条件执行的 cycle 公式（§3.4 坑 2）在所有条件指令上验证通过。
- [ ] **§8 块级 DMA 用例**：两个后端退出点、`instruct_adr`、`bb_total_cycles`、`mmu_dma_event_pending` 完全一致。
- [ ] **§8 `instr_is_store` 分类**：4096 + 1024 个槽位逐一比对「是否真写内存」，零差异。
- [ ] **§8 整机回归**：《恶魔城》HUD block size 15 / 16 / 100 全部正常，暂存写计数与基线逐字节一致。
- [ ] **§8 游戏冒烟**：3~5 个不同类型游戏（2D / 3D / 声音密集）无回归；自修改代码路径不崩。

## 二、构建 / 工具链类

- [ ] **§0.3 全量 msbuild**：本机沙箱把 `MSBuild.exe`/`cmd.exe` 判为 LOLBin 拦掉，能否出 exe 仍未验证。需在沙箱外跑 `tools\jit_matrix\build_check.bat`。
- [ ] **§8 多平台编译**：Win32 / Win64 / macOS arm64 / Linux x86-64 / Linux aarch64 全部编译通过（Windows 与 Xcode 工程已接线；macOS 实测清单见 `macos-arm64-test-plan.md`）。
- [x] **§8 矩阵生成自检**：`gen_opcode_matrix.py --check` 退出码 0（2026-11-20 重跑）。
- [x] **x86 JIT 全量重跑（新核心库，2026-11-20）**：修复 `build_core.ps1` 让 vendored asmjit 源用 C++20 编译（此前 52 个 asmjit 文件按 c++14 编译失败，核心库停在 11/19），重建出含 11/20 全部 x86 修复的 `desmume_core.lib`（194/194 源，0 失败）。重链 `jit_diff.exe` 后：`--full` 81920 比较功能桶全 0（12067 是 cycle 记录项）；`--seq 2/3/4` 共 19776 比较全 0 mismatch；38 探针全绿。接线自检（vcxproj/xcode/gen_sources）全绿。
- [ ] **§8 x86 运行时补测**：6 处 `ptr_abs` 改造现已在**真实 x86 运行时**覆盖——`jit_diff.exe` 是 x86 后端且跑通全量/序列差分（不再是 `-fsyntax-only`）；若需 GUI 整机层面确认，仍可在 Windows / Intel Mac 补做。

## 三、迁移主线进度（核心，剩 278 组）

- [ ] **§8 矩阵状态**：571 组全部 `diff-pass`，其余 20 组保持 `interp-fallback`（当前 293 组，见 §S4）。
- 按 §7 分组的未完成批次（数字取自重新生成的 `documentation/jit-opcode-matrix.csv`）：

| 组 | 内容 | 组数 | 状态 |
|---|---|---|---|
| C | 访存 + LDM/STM（批次 3 + 4） | 204 | 未开始语义迁移（ABI 优化已先行；缓存接入属 §9.5 P3）。**注意**：`op_ldm_stm2` 的两处 `switchMode` 已按本批不变量补了 `flush()`，但还没被探针覆盖 |
| D | 乘法 + 分支 + 状态 + 异常（批次 5 + 6） | 41 | **完成**（`UND` 除外，它一直 `interp-fallback`）：乘法 28 + CLZ（§S4 族 5）、分支/状态/异常 12（§S4 族 6） |
| E | 协处理器 + 饱和 + Thumb（批次 7 + 8） | 74 | 未开始（coproc 2 + sat 4 + Thumb 68） |

- 本轮抓到并修掉两个既有缺陷（都是"缓存与外部世界"的边界）：
  1. `emit_dma_event_check` 在分支内 `flush()` 会丢延迟回写（主文档 §S4 族 5）；
  2. `armcpu_switchMode()` 之前没有 `flush()`，导致 bank 双向串值（主文档 §S4 族 6）。
  共同点：**单指令块测不出来**，都要靠 `a64_block_probe` 里的两指令块。后续族一律先加两指令用例。
- 后续批次纪律（§S4）：族 1/2 的**整机帧率门槛仍未验证**，代码尺寸达标不等于帧率达标；若 ALU 族只拿到 1.0~1.1×，应停下来重定目标，而非继续把 571 条全改完。

## 四、性能优化类

- [ ] **§9.5 P1**：Thumb 批次 8 —— Thumb 整体比 ARM 贵，覆盖缺口最大（70 组）。
- [ ] **§9.5 P2**：死标志位消除 / 惰性标志 —— S 指令急切写标志，ALU 密集代码仍有冗余。
- [ ] **§9.5 P3**：缓存扩到 4–6 槽，并覆盖 load/store 基址与数据 —— 当前 3 槽只够 ALU/移位，访存前必须 `evict`。
- [ ] **§8/§S4 性能门槛**：ALU 密集场景 ≥1.3×（族 1 门槛）、≥1.5×（全量目标）——未测，代码尺寸门槛先失败。
- [ ] **§8/§S4 帧率与编译**：x86 帧率 ±5%；冷启动 JIT 编译耗时 ≤1.2×；族 2 仍等整机帧率实测。
- [ ] **§8 scratchpad/回收**：32MB scratchpad 行为未变（x86）/ 40MB 块回收路径未变（arm64）。
- [ ] **§S8 可选优化（独立 PR，不混入迁移）**：
  1. x86 改用 `JitRuntime` + `JitAllocator::CreateParams::near_address`，取代 32MB 静态 scratchpad + `mprotect` hack；
  2. arm64 连续两条 `ldr` 合并成 `ldp`、块级标志延迟合成（一次 `cset` 批量写 NZCV）；
  3. 两后端共享块级 cycle 记账与 `sync_r15` 策略到头文件（仿 `jit_dma_event.h`）。

## 五、收尾类（S7 / §8）

- [x] **§S7.1** `asmjit_compat.h` 已不存在（此前已完成）；`*_ptr` 宏已直接写成 `x86::ptr`。
- [x] **§S7.2** 删除 `src/utils/AsmJit`（69 文件）+ 清理 **9 个**构建文件（不是 5 个：
  `meson.build`×2、`Makefile.am`、`desmume.cbp`、`windows/Makefile`、
  `DeSmuME_Interface.vcxproj(.filters)`、`DeSmuME.vcxproj(.filters)`）——本轮完成。
  `build_check.bat` 同步改为编 vendored asmjit（C++20）；`VERSION.txt` / `wire_asmjit_xcode.py`
  注释已更新。验证：`arm_jit.cpp` MSVC C++20 编译 exit=0、接线自检全绿、harness 38/38。
  **POSIX 构建本机无法编译，待 Linux/macOS 侧验证**。
- [ ] **§S7.3** 删除 `arm64_emit.cpp` 里被 asmjit 取代的编码函数（`output_w32`/`MOD_JUMP`/手写 imm 编码/`genimm2`），保留纯语义辅助。（本机无 aarch64 编译能力，留待 macOS）
- [ ] **§S7.4** CI 增加「仅编译 arm_jit.cpp + arm_jit_arm.cpp + asmjit」档，防 C++17 配置破坏。
- [ ] **§S7.5** 把 §3.6 契约清单、§3.7 分类测试与差分 dump 归档到 `documentation/jit-semantics-baseline/`。
- [ ] **§8** 旧实现遗留失败项归档为「既有 bug 清单」，且不混入迁移 PR。
- [ ] **§8** 三份验收报告进 `documentation/` 并附在 PR 里。

---

## 建议执行顺序

1. **先 macOS 补测访存 15 个调用点**（§10.7）——当前唯一可能引入回归的未验证改动，优先排除风险。
2. **继续迁移主线**：从族 3 访存语义迁移起步（C 组 206 组），但需先解决「x86 可差分、arm64 语义只能等 macOS」的验证不对称。
3. **帧率门槛实测**：在族 1/2 上取解释器 / 旧 JIT / 迁移后 JIT 三组数字，确认 ALU 族真的 ≥1.3×，再决定是否继续推 571 条。
4. **P1/P2/P3 优化** 与 **S7 收尾** 收口。

---

## 附：编译失败诊断（2026-11-20 新增）

`JIT error at ARM9-XXXXXXXX: InvalidInstruction` 以前只给错误名，无法定位指令。
现在默认行尾多两项信息：**块内指令数** 和 **该地址所在存储体**（`jit_bank_name`，
把 0x01xxxxxx 这类 `UNUSED_RAM` 直接标成 `NOT MAPPED`，一眼看出是野 PC 而不是代码
生成 bug）。

要定位到具体指令，设环境变量 `DESMUME_JIT_DIAG=1` 再跑：

```
DESMUME_JIT_DIAG=1 DeSmuME.exe rom.nds
```

此时每个被模拟指令在 asmjit 日志里都有一个 `>> ARM9 <地址>: opcode <原码>` 标记，块
编译失败会把日志尾部打出来——**最后一条标记就是 asmjit 拒绝的那条指令**，其后的行是
它已经编好的那部分 x86；随后还会打印整块的原始指令字（含 opcode matrix 槽位，可直接
对 `jit-opcode-matrix.csv` 的 `first_slot` 列）。注意该模式会格式化整块日志，只能用来
诊断，不能正常玩。

已知案例（本次修复）：ARM `LDR Rd=PC` 的发射代码用 `new_gpz()` 承接 32 位临时值，
`mov r64, r32` 在 asmjit master 下无此编码 → 整块 finalize 失败 → 静默回退解释器。
假 BIOS 的 IRQ 入口 `0xFFFF0274..0xFFFF028C` 末尾正是 `LDR PC,[R0,#-4]`，所以每次
中断都会报一次。现已改为全 32 位序列。

注意两个既有坑（本次未改）：

- `kValidateIntermediate` 抓不到「虚拟寄存器尺寸错配」，asmjit 的 RA 阶段失败也不会
  调用 `ErrorHandler`；只有挂在 `CodeHolder` 上的 logger 能看到失败前的指令流。
- `JIT_COMMENT(...)` 宏用的是 `c.comment(fmt, ...)`，而 asmjit master 的
  `comment()` 只接受 `(const char*, size_t)`。目前 `LOG_JIT_LEVEL == 0` 所以没人发现，
  真要打开 `LOG_JIT` 需要先把宏改成 `snprintf` + `c.comment(buf)`。

### 自定义 Logger 必踩：`size == SIZE_MAX`

`Logger::log()` 是内联转发，**不会**替你把 `SIZE_MAX` 解析成 `strlen`：

```cpp
Error log(const char* data, size_t size = SIZE_MAX) noexcept { return _log(data, size); }
```

而 `BaseBuilder::serialize_to()` 回放注释节点时正是以 `SIZE_MAX` 调用：

```cpp
err = dst->comment(node->inline_comment());   // comment(const char*, size_t = SIZE_MAX)
```

于是 `_log` 里任何形如 `if(size > N) { data += size - N; size = N; }` 的截断都会把
`data` 变成一个**指针减法溢出后的野地址**（`SIZE_MAX - N` 回绕），随后 `memcpy` 直接
AV。第一版 `JitDiagLogger` 就是这么崩的：只在 `DESMUME_JIT_DIAG=1`（有注释节点）时
触发，且崩溃点在 `serialize_to → comment → log → _log`，看起来像 asmjit 的问题。
现在的实现先判 `data == NULL`、再判 `size == SIZE_MAX`，最后按 `drop` 量 `memmove`。

> 验证提醒：`probe_block_at.exe` 之类的探针是**静态链接** `desmume_core.lib` 的。
> 改了核心源却只重编库、不重链 exe，跑的仍是旧代码——会得到假回归。改库后必须重链。
