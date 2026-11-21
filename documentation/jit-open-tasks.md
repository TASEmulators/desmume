# JIT 迁移待办追踪

> 本文把 `asmjit-jit-migration-plan.md`（下称「主文档」）中散落的未完成项集中成一份可勾选清单。
> **主文档仍是权威来源**，本文是索引视图；状态变化时两处同步更新。
>
> 快照：2026-11-21，HEAD `beda713b` + 本轮未提交改动（x64 差分台移植 arm64 探针形状 §10.11、
> x64 补上 ARM `BKPT` §10.12、主存快路径实验与 `--bench` §10.13、callee-saved 提示实验 §10.14）。

## 当前进度快照

| 项 | 状态 |
|---|---|
| 迁移主线（571 组 `diff-pass`） | **571 / 571，todo 归零**：族 1 数据处理 28 + 族 2 ARM 移位 224 + 族 3+4 访存 172 + LDM/STM 32 + 族 5 乘法/乘加 28 + CLZ + 族 6 分支/状态/异常 12 + **族 7+8 Thumb 68 + 饱和 4 + 协处理器 2**。余下 20 个 `interp-fallback`（`OP_UND`/`OP_CDP`/LDC-STC 全系/`LDREX`/`STREX`/Thumb 的 `BKPT`+`UND`）是有意不编译的 |
| **两后端回退集合审计（2026-11-21）** | 逐函数比对 571 个 `OP_*`、20 行 `#define ... NULL` 与各函数体里的 `return 0`：**唯一差异是 ARM `OP_BKPT`**（x64 只有 `printf` + `return 0`，arm64 已内联 abort）。已按 arm64 补齐，两边现在一致；`OP_MCR`/`OP_MRC` 的 `return 2` 不发射代码，但解释器在同一条路径上也是空操作，三者等价（主文档 §10.12） |
| **主存快路径实验（2026-11-21，已回退）** | 照 §10.13 做了"专用 helper + cache 命中常量 cycle"两条：功能**逐位等价**（132 探针 0 差异、`--full` 仍 12067），但 `--bench` 实测**读慢 12%、写慢 20%**（100% 命中的最好情况也不赢）。原因是成本在"每条访存一次间接 call"，不在 helper 指令数；写路径还因为写 miss 不更新 `m_cacheCache` 而永远进不去。两条都没进仓库，保留 `jit_diff --bench` |
| **callee-saved 提示实验（2026-11-21，已回退）** | 把 `bb_cpu`/`bb_total_cycles`/`bb_dma_pending` 用 asmjit 的 home-id 提示放进 r14/r15/r13：代码体积 **−34%（Thumb LDR）/ −23%（STR）**，但 `--bench` 速度 **−1%~−4%**（序言/尾声的 push/pop 在 call-return 关键路径上，省掉的 spill/reload 被乱序执行完全遮住）。未进仓库。两个坑记在主文档 §10.14：invoke 参数**不能**是物理寄存器（`InvalidVirtId`，且块编译失败在探针里是静默的）；物理寄存器在 asmjit 里没有存活区间，会被分配给临时值 |
| P0 访存 ABI（arm64 `call_mem`） | **已落地提交**：`call_mem` / `_r7` / `_r2` 三变体，24 处调用点中 15 处转轻量（commits `b7225baa`、`b1f5b455`）；原语级 + 块级真机均已验证 |
| 族3 代码迁移 | **arm64 缓存接入完成**；x86 侧结构早已完成 |
| **饱和算术（Q 系列）** | **两端都已内联**：arm64 见 §S4 族 7+8；x86 `arm_jit.cpp` 用同一份 DDI 0403E 语义（`seto` + `cmovnz`，两段饱和分别置 Q）。x86 侧 6 条 Q 探针 × 8 组操作数 × 2 种标志 = 704 次比较 0 mismatch，负对照（`cmovz`）确认探针有效 |
| 访存差分安全网 | **x64 侧已扩到 132 条探针**（§10.8 的 20 条访存 + §10.11 的 91 条，覆盖 LDRD/STRD/SWP/LDM/STM/乘法/PSR/分支/Thumb 各宽度）：2112 次比较 **0 功能差异**，176 条周期记录（异常收尾、长乘法、映射区字读三类，均属 §3.5 边界） |
| **arm64 handler 级真机验证** | `a64_block_probe` **904 项自检全绿**（35 乘法 + 2 store + 1 两指令 + 15 PSR/分支 + **41 访存** + 78 Thumb + 4 Q 指令 + 2 cp15）；其中访存表本轮新增 4 例（LDRD/STRD/SWP 的脏 Rm/基址），改前改后输出**逐字节相同** |
| **helper 读 `armcpu_t` 的不变量** | `armcpu_switchMode()`（族 6）与 `OP_LDM_STM`/`OP_LDRD`/`op_swp`/Thumb 的 `PUSH`/`POP`/`STM`/`LDM`（族 7）都直接读写 `cpu->R[]`：**调用前必须把涉及的寄存器落盘**。族 7 把同一条规则套到 Thumb 的寄存器列表上（8 位掩码 + LR/PC 位） |
| x64 fastmem | **已评估放弃**（主文档 §10.6），x64 保持现状 |
| 本机验证 | x86 `jit_diff --full` 81920 次比较功能桶全 0（`R0-R14`/`CPSR`/`instruct_adr`/异常/不变量），仅周期差 12067 与既有 3 条 `jit not repeatable`；**132 条定向探针** 2112 次比较 0 功能差异（176 条周期记录，其中 ARM `BKPT` 已从"回退解释器"改成内联 abort，功能仍 0 差异、周期由 jit=4 变 jit=0）；`--seq 2` 6848 次 0 mismatch。差分台现在支持 `--opcode <hex>`，可测 `slot<<20` 造不出来的编码（如 `bkpt`）。arm64 编码层 `check_a64` 4/241 已知基线差异（全部是 save/restore/startfunc/endfunc 的既有基线）；**真机**：`a64_exec_probe` 21/21、`a64_memcall_probe` 15/15、`a64_block_probe` **904/904**（含 768 组 Q 对伪代码的模型比对）；arm64 后端另可在本机做 `clang++ -fsyntax-only` 语法门（x86_64 主机 + `-DASMJIT_NO_X86`） |
| 未跟踪文件 | `issue.md`（会话开始前既有，不属本次迁移） |

---

## 一、验证类 —— 需 macOS 整机，本机无法完成（最高优先）

本类直接关系刚提交的 arm64 `call_mem` 改动（15 个调用点），是唯一可能引入功能回归的未验证面。

- [x] **§10.8 访存差分安全网（本机）**：差分台 `kProbes` 新增 20 条访存定向探针（写回 / Rd≠Rn / 宽度 / 符号扩展 / 寄存器偏移 / Thumb SP 相对 / PUSH-POP），38/38 探针 0 mismatch，`--full` 功能桶全 0。这是本机侧能做的加固；**不替代**下面的 macOS arm64 语义验证。
- [x] **`call_mem` 原语级真机验证（Termux aarch64，本轮）**：`tools/jit_matrix/a64_memcall_probe.cpp` 15/15。负对照（同函数去掉 stp/ldp）证明 helper 真的把 x12/x15/x2 毁成 `BEEF0000`/`F00D0000`/`DEAD0000`，正例证明 `call_mem` 保 x12/x15、`call_mem_r2` 保 x2+x12+x15，x21 靠 AAPCS64 callee-saved 免费保住；1e6 次调用验 `call_mem_r2` 的栈收支与 canary。
- [x] **handler 级真机验证通道（族 5 建立，族 6 扩展）**：`tools/jit_matrix/a64_block_probe.cpp` 直接 include `arm_jit_arm.cpp`、用 68 个桩符号补齐核心，在真机上执行 `compile_basicblock()` 生成的块。族 6 把其中的 `armcpu_switchMode` 桩换成**真正搬移 bank 的实现**（否则"缓存没 flush"这类 bug 一条都测不出来），并支持两指令块。**新增族时优先扩这个探针，而不是等 macOS。**
- [ ] **§10.5/§10.7 访存差分（macOS / arm64）**：`build_diff_macos.sh` 重建后跑 `jit_diff --full`，确认访存槽位功能桶（R0–R14 / CPSR / `instruct_adr` / 内存写集）全 0。重点覆盖：LDR/STR 的 Rd==15 与写回变体、**LDRD/STRD 的 W 位写回、Thumb PUSH/POP、SWP、LDM/STM**。注意 macOS 的 `kProbes` 与本机同步后是 **132 条**（§10.11 把 arm64 探针的形状搬到了 x64）。
- [ ] **`ldmda`/`stmda` 的有效地址对解释器核对（新增，需 oracle）**：探针里这两条只做了 pin（迁移前后相同），没有断言——它们的地址推进跟 ARM ARM 的写法对不上，而块级探针链不到解释器，判不了谁对。macOS 上拿 `jit_diff` 的 LDM/STM 槽位对比即可定性；若 JIT 与解释器一致而与 ARM ARM 不一致，那是既有语义，另开 issue。
- [x] **带移位寄存器偏移的 LDR/STR 已接缓存（2026-11-21 完成）**：移位 `arg` 宏（10 个
  IMM/REG 变体）的 Rm/Rs 改用 `read_into()`——槽里有值就 `mov`，不占槽、不 evict；同一
  形状的 `LDRD/STRD` 寄存器偏移与 `SWP/SWPB` 基址一并改掉。真机 904/904，改前改后输出
  逐字节相同；`--marginal` 实测 `ldrd` 脏 Rm 从 31 → 30 条。详见主文档 §10.10。
- [x] **§10.5 代码体积（arm64 侧已完成测量）**：`a64_block_probe --marginal` 在真机上量出
  `ldr [r1,#4]` 17 条、`ldr [r1,r2]` 18 条（冷/脏同价）、`str [r1,r2]` 44 条（含 DMA 冷
  路径）。§10.5 推算的「29 → 17~21」在 arm64 上成立。x86 侧仍需 `jit_diff --block --slot`。
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

## 三、迁移主线进度（核心，已完成）

- [x] **§8 矩阵状态**：571 / 571 全部 `diff-pass`，`todo` 归零；其余 20 组是 `interp-fallback`（`OP_UND`/`OP_CDP`/LDC-STC 全系/`OP_LDREX`/`OP_STREX`/Thumb 的 `BKPT`+`UND`），有意不编译。见 §S4。
- [x] **"已实现"与"会编译"对齐（2026-11-21）**：`emit_armop_call()` 把 `fc(opcode) == 0` 也当回退处理，所以矩阵上的 `diff-pass` 并不保证不跳解释器。逐函数审计后 571 组里只剩 ARM `OP_BKPT` 一处，x64 已按 arm64 内联 abort 异常补齐；`--full` 与 132 条探针的功能差异仍为 0（主文档 §10.12）。
- 按 §7 分组的批次状态（数字取自重新生成的 `documentation/jit-opcode-matrix.csv`）：

| 组 | 内容 | 组数 | 状态 |
|---|---|---|---|
| A/B | 数据处理 + ARM 移位（批次 1 + 2） | 252 | **完成**（§S4 族 1/2）；性能门槛未过 |
| C | 访存 + LDM/STM（批次 3 + 4） | 204 | **完成**（§S4 族 3+4）；遗留两条 DA 形式待 macOS 对解释器 |
| D | 乘法 + 分支 + 状态 + 异常（批次 5 + 6） | 41 | **完成**（`UND` 一直 `interp-fallback`） |
| E | 协处理器 + 饱和 + Thumb（批次 7 + 8） | 74 | **完成**（§S4 族 7+8）：Thumb 68 + 饱和 4 + 协处理器 2 |

- 四条"缓存 vs 外部世界"的既有缺陷都是这一路抓出来并修掉的，共同点是**单指令块测不出来**：
  1. `emit_dma_event_check` 在分支内 `flush()` 会丢延迟回写（§S4 族 5）；
  2. `armcpu_switchMode()` 之前没有 `flush()`，bank 双向串值（§S4 族 6）；
  3. `OP_LDM_STM` 只 evict 基址，`add r4,#1 ; stmia {r4}` 存旧值（§S4 族 3+4）；
  4. Thumb 的 `PUSH`/`POP`/`STM`/`LDM` 同一个列表问题（§S4 族 7+8，本批按同一条规则一次改对）。
- 迁移本身到此收口。剩下的都是**验证类**（macOS 三方差分、整机 ROM/帧率）与**性能类**，见第一、四节。
- 批次纪律（§S4）：族 1/2 的**整机帧率门槛仍未验证**，代码尺寸达标不等于帧率达标；若 ALU 族只拿到 1.0~1.1×，应停下来重定目标，而不是继续加特性。

## 四、性能优化类

> **代码尺寸基线（2026-11-21，真机 `a64_block_probe --marginal`）**：这是 x86
> `--block --slot` 的 arm64 对应物，能量的是「该指令的边际 host 代码尺寸」。当前数字：
> `add` 4 / `adds` 9 / `cmp` 8 / `ldr [r1,#4]` 17 / `ldr [r1,r2]` 18（冷脏同价）/
> `str [r1,r2]` 44（含 DMA 冷路径，实际执行 3 条）/ `thumb adds` 8 / `thumb ldr` 14。
> 完整表与三条结论见主文档 §10.10。
>
> **x64 侧的对应测量（2026-11-21 新增）**：`jit_diff --bench <iters> --block <n> --slot <s>`
> 直接给 ns/块与 ns/条（`--block` 只看代码尺寸，看不见只改 C helper 的改动）。
> 当前基线（Thumb，块内 16 条）：`add` 0.62 ns、`ldr [r1]` 3.5 ns、`str [r1]` 3.5 ns。
> 差值就是"每条访存一次间接 call"的固定成本，见 §10.13。

- [ ] **§9.5 P1**：Thumb 已全部走缓存（族 7+8），`--marginal` 已有同语义对照：
  `thumb adds` 8 条 ≈ `arm adds` 9 条，**同语义下 Thumb 并不更贵**；差距来自 Thumb 的
  ALU 一律带 S。剩下要补的是整机帧率（不是代码尺寸）。
- [ ] **§9.5 P2**：死标志位消除 / 惰性标志 —— S 指令急切写标志。`--marginal` 给出收益
  上限：`add` 4 vs `adds` 9，即每条 S 最多可省 5 条 host 指令（`SET_NZCV` = `mrs` +
  `ldr` 标志字节 + `lsr` + `bfxil` + `strb`）。
- [x] **§9.5 P3（部分）**：load/store 的**基址与数据**已在族 3+4 接入 `rc_read`/`rc_store`，
  寄存器偏移 Rm 与 SWP 基址在 §10.10 补齐；**不再需要扩到 4–6 槽**（`read_into()` 不占
  槽）。仍走内存的只有 Rd(helper 经 `u32*` 写回)、R15 与列表类写回。
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
2. **继续迁移主线**：从族 3 访存语义迁移起步（C 组 206 组），但需先解决「x86 可差分、arm64 语义只能等 macOS」的验证不对称。**该不对称已部分缓解**：`a64_block_probe` + `--marginal` 能在真机上给出等价性与代码尺寸，剩三方差分与整机帧率两块。
3. **帧率门槛实测**：在族 1/2 上取解释器 / 旧 JIT / 迁移后 JIT 三组数字，确认 ALU 族真的 ≥1.3×，再决定是否继续推 571 条。代码尺寸一侧已可先用 `--marginal` 预判（§10.10 表）。
4. **P1/P2/P3 优化** 与 **S7 收尾** 收口：P3 已按「不扩槽、用 `read_into()`」收口；P2 的收益上限由 `--marginal` 的 `add` 4 vs `adds` 9 直接给出。

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
