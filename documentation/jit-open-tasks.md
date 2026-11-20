# JIT 迁移待办追踪

> 本文把 `asmjit-jit-migration-plan.md`（下称「主文档」）中散落的未完成项集中成一份可勾选清单。
> **主文档仍是权威来源**，本文是索引视图；状态变化时两处同步更新。
>
> 快照：2026-11-20，HEAD `b1f5b455`（访存探针改动见主文档 §10.8，待提交）。

## 当前进度快照

| 项 | 状态 |
|---|---|
| 迁移主线（571 组 `diff-pass`） | **252 / 571**：族 1 数据处理 28 + 族 2 ARM 移位 224；余 319 组 |
| P0 访存 ABI（arm64 `call_mem`） | **已落地提交**：`call_mem` / `_r7` / `_r2` 三变体，24 处调用点中 15 处转轻量（commits `b7225baa`、`b1f5b455`）；语义待 macOS 验证 |
| 族3 代码迁移 | **x86 结构 + arm64 结构级已完成**；卡 `diff-pass` 的是 arm64 接入 `ArmRegCache`（§9.5 P3）+ macOS 三方差分 |
| 访存差分安全网 | **本机已加固**（主文档 §10.8）：`kProbes` 新增 20 条访存定向探针，38/38 全绿；此前 `--full` 只测 `LDR r0,[r0]` 退化形式 |
| x64 fastmem | **已评估放弃**（主文档 §10.6），x64 保持现状 |
| 本机验证 | x86 `jit_diff --full` 81920 次比较功能桶全 0；arm64 编码层 `check_a64` 4/241 已知基线差异、新增 case 全匹配 |
| 未跟踪文件 | `ISSUE-dma-block-exit.md`、`ISSUE.md`、`jit-bug.md`（会话开始前既有，不属本次迁移） |

---

## 一、验证类 —— 需 macOS 整机，本机无法完成（最高优先）

本类直接关系刚提交的 arm64 `call_mem` 改动（15 个调用点），是唯一可能引入功能回归的未验证面。

- [x] **§10.8 访存差分安全网（本机）**：差分台 `kProbes` 新增 20 条访存定向探针（写回 / Rd≠Rn / 宽度 / 符号扩展 / 寄存器偏移 / Thumb SP 相对 / PUSH-POP），38/38 探针 0 mismatch，`--full` 功能桶全 0。这是本机侧能做的加固；**不替代**下面的 macOS arm64 语义验证。
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
- [ ] **§8 矩阵生成自检**：`python tools/jit_matrix/gen_opcode_matrix.py` 退出码为 0（含 §3.8 六项断言）。
- [ ] **§8 x86 运行时补测**：6 处 `ptr_abs` 改造的运行时验证需在 Windows / Intel Mac 补做（本机无 x86 运行环境；`-fsyntax-only` 已 0 错误）。

## 三、迁移主线进度（核心，剩 319 组）

- [ ] **§8 矩阵状态**：571 组全部 `diff-pass`，其余 20 组保持 `interp-fallback`（当前 252 组，见 §S4）。
- 按 §7 分组的未完成批次：

| 组 | 内容 | 组数 | 状态 |
|---|---|---|---|
| C | 访存 + LDM/STM（批次 3 + 4） | 206 | 未开始语义迁移（ABI 优化已先行） |
| D | 乘法 + 分支 + 状态 + 异常（批次 5 + 6） | 41 | 未开始 |
| E | 协处理器 + 饱和 + Thumb（批次 7 + 8） | 92 | 未开始 |

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

- [ ] **§S7.1** 删除 `asmjit_compat.h`（最后 22 处 `*_ptr` 宏直接写成 `x86::ptr`）。
- [ ] **§S7.2** 删除 `src/utils/AsmJit`，清理 5 个构建文件里的遗留条目。
- [ ] **§S7.3** 删除 `arm64_emit.cpp` 里被 asmjit 取代的编码函数（`output_w32`/`MOD_JUMP`/手写 imm 编码/`genimm2`），保留纯语义辅助。
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
