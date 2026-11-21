# DeSmuME x86-64 / AArch64 双后端 JIT 迁移到 asmjit master
## —— 按指令迁移 + 三方言语义对账

> 目标项目：`C:\Users\zhang\desmume`（仓库根 `desmume/`，分支 `asmjit-master` @ `d2282292`）
> 目标依赖：`C:\Users\zhang\asmjit` @ `677a10b`（2026-11-04，master）
> 现状依赖：`desmume/src/utils/AsmJit`，版权头 2008–2012，即 **asmjit 1.0**
> 结论日期：2026-11-19（rev4）

---

## 修订记录

| 版本 | 变更 |
|---|---|
| rev1 | 只针对 x86/x64 宿主后端（`arm_jit.cpp`），分 S0–S7 七阶段 |
| rev2 | ① x86-64 与 AArch64 两个后端同时迁移；② ARM32 后端删除；③ 迁移单位改为"一条指令"；④ 新增三方言语义对账 |
| rev3 | ① **arm64 侧改用 `a64::Compiler`**（不再用 Assembler 平移手写编码器）——为统一、为减 bug、为性能；② 新增 §1.4 性能账（带实测证据）；③ 新增 §3.7：`2d4919aa` 块级边界事故复盘，并把它变成可自动化的判据；④ 阶段重排为 S0–S8 |
| **rev4** | 按 rev3 开工后的落地修正：① vendor 目录改用仓库既有约定的 `3rdparty/asmjit`（原文写 `third_party`，但仓库已有 `3rdparty/` 且被 vcxproj 引用）；② 新增 `tools/jit_matrix/gen_asmjit_sources.py`——asmjit 编译清单**由脚本生成**，不手抄；③ S1 拆成 S1.1（删 ARM32，**已完成**）/ S1.2（vendor，**已完成**）/ S1.3（构建接线，推迟到 S2 首个提交）；④ 记录 MSVC 构建验证的实际结论与阻塞点 |
| **rev5** | S2 收官：① macOS arm64 实机构建通过（两处真实阻塞已修，见 §2.6）；② 差分台移植到 POSIX 宿主机，`build_diff_macos.sh` 复用 Xcode 产物——**arm64 后端第一次以"模拟器的调用方式"跑起来**；③ 首次全量扫描发现 2 个真实缺陷，均按 §3.5 以 ARM ARM 仲裁后修复（arm64 Thumb `ASR #32` 的 C、解释器 `NEG INT_MIN` 的 V）；④ §3.5 补充"周期数不可用 ARM ARM 裁决"的边界 |
| rev6 | 只补现状量化，不改代码：① 新增 §9，记录当前 arm64 JIT 的 Guest:Host 指令膨胀率、访存回调路径、`ArmRegCache` 的实际覆盖边界；② 明确"1 条指令块"的 36.9 条/块不能当比例，避免后续拿错基线；③ 给出下一步优化优先级（P0 访存 ABI） |
| rev7 | 落地 §9.5 的 P0 第一刀：① 新增 `call_mem` 轻量 helper 调用（arm64），把访存热点 `OP_LDR_`/`OP_STR_` 的调用开销从 15 条降到 3 条；② 逐调用点论证寄存器存活面，不全局删 `save_call`；③ 编码层 golden 增加 3 个 case，x86 全量差分功能桶仍全 0；④ 记录本机验证边界与 macOS 待验项（§10） |
| rev8 | P0 第二刀：① `call_mem` 扩展到 SWP/LDM-STM/Thumb LDR-STR/SPREL/PCREL，新增 `call_mem_r7` / `call_mem_r2` 变体，24 处调用点中 15 处转轻量（§10.7）；② 记录 x64 fastmem 评估结论——三项硬障碍下主动放弃，两后端以安全方法对齐（§10.6）；③ 编码层 golden 增至 241 case |
| rev9 | 主线验证网加固：① 差分台 `kProbes` 新增 20 个访存定向探针，覆盖写回 / Rd≠Rn / 字节·半字·符号扩展 / 寄存器偏移 / Thumb SP 相对 / PUSH-POP（此前 `--full` 的 `slot<<20` 只测 `LDR r0,[r0]` 退化形式，零覆盖这些变体）；② 38/38 探针全绿、`--full` 功能桶全 0；③ 新增独立待办追踪文档 `jit-open-tasks.md`（§10.8） |
| rev10 | S7.2 收尾：删除旧 `src/utils/AsmJit`（69 文件）并清理 **9 个**构建文件的遗留引用（原计划写 5 个，实测为 9 个）；`build_check.bat` 同步改为编 vendored asmjit + C++20；矩阵行号随代码同步。验证：`arm_jit.cpp` MSVC C++20 编译 exit=0、接线自检全绿、harness 38/38；POSIX 构建待 Linux/macOS 验证 |
| **rev11** | 修复核心库验证盲点：`build_core.ps1` 此前把 vendored asmjit 的 52 个源按 c++14 编译全部失败，导致 `desmume_core.lib` 停在 11/19、不含 11/20 的 x86 修复——过去所有 x86 差分其实在验旧代码。给 asmjit 源加 C++20 特判后重建（194/194，0 失败），重链 `jit_diff.exe` 整体重跑：`--full` 功能桶全 0、`--seq 2/3/4` 19776 比较全 0 mismatch（§10.9） |
| **rev12** | 主线推进 + **handler 级真机验证首次落地**：① 族 5（乘法 28 handler + CLZ）迁到 `ArmRegCache`，为此新增 `read_into()` 入口与 `rc_read`/`rc_store`（§S4 族 5）；② 新增 `tools/jit_matrix/a64_block_probe.cpp`——在 AArch64 真机上直接执行 `compile_basicblock()` 生成的块（68 个桩符号补齐核心），**终结了"arm64 handler 语义只能等 macOS"的不对称**；③ 真机抓到并修复一个既有 bug：`emit_dma_event_check` 在分支内 `flush()` 会清掉编译期 dirty 位，导致延迟回写在落空路径上丢失 |
| **rev13** | 族 6（分支 + 状态寄存器 + 异常，13 handler）：① 确立并落实一条不变量——**任何 `armcpu_switchMode()` 调用前必须先 `flush()`**，因为它靠读 `armcpu_t` 完成 bank 搬移；MSR/SWI/BKPT/LDM 共 4 处补齐（`S_DST_R15` 本来就有）；② `MRS`/`MSR`/`BX` 的源与目的改走缓存；③ `OP_BKPT` 由"打印 + 回退解释器"改为按解释器逐行内联；④ 真机探针把 `armcpu_switchMode` 桩换成**真正搬移 bank 的实现**，用两指令块抓到 bank 串值（§S4 族 6）；⑤ 矩阵账本补齐族 5 漏记的 29 个 handler：293/571 |
| **rev14** | 族 3+4（访存 + LDM/STM，204 handler）：① `OP_LDR_`/`OP_STR_`/`LDRD`/`STRD`/`SWP`/`op_ldm_stm` 的基址与存储值改走 `ArmRegCache`；② 抓到既有 bug：`OP_LDM_STM` 直接读 `cpu->R[]`，而只有基址被 evict，于是 `add r4,#1 ; stmia r0,{r4}` 存的是旧值（族 1 之后一直存在）；③ 抓到第二个既有 bug：LDM 基址在列表里的写回用的是 `call_mem_r7` 之后已失效的 x4；④ 探针新增 36 个访存块（含 6 个两指令块），82/89 逐字节相同、7 个刻意变化；⑤ 矩阵：497/571 |
| **rev15** | 族 7+8，**主线收尾**：① Thumb 全表 68 handler 迁到 `ArmRegCache`（3 位字段的 `rc_t_*` 入口 + 高寄存器组的 `rc_read_at`）；② `OP_QADD`/`QSUB`/`QDADD`/`QDSUB` 从"`printf` + 回退解释器"改为内联实现，操作数顺序按 DDI 0403E 的 `op{Rd}, Rm, Rn`（解释器本来就对；此前记为"与 ARM ARM 相反"是记错了字段，见 rev16）；③ `OP_MUL_REG` 的周期尾部改喂乘数而非乘积，与 ARM 路径一致（`muls r0,r2` 5→2 cycles，状态不变）；④ 探针新增 78 个 Thumb 块与 4 个 Q 指令 + 2 个 cp15 用例：170 块中 164 逐字节相同、6 个刻意变化，126 项自检全绿；⑤ 矩阵 **571/571 `diff-pass`，todo 归零** |
| **rev16** | **更正 rev15 里的一处错误结论**：族 7+8 曾写"Q 系列操作数顺序按解释器、不按 ARM ARM"。查 ARM 官方文档（DDI 0403E 的 `QADD`/`QSUB`/`QDADD`/`QDSUB` Operation 伪代码、DUI 0489 的 Usage 段）后确认：这一族用 `op{Rd}, Rm, Rn` 语法，`Rm` 是 bits 3-0、`Rn` 是 bits 19-16，`QDADD` 双倍的是 `Rn`——`arm_instructions.cpp` 四个 handler 与之逐条相符，**解释器没有 bug**，错的是 rev15 的表述（把字段名记反）。代码只改注释；另把探针的 Q 判据从"迁移前后相同"升级为"对照伪代码参考模型"，768 组操作数全过（§S4 族 7+8） |
| **rev17** | **x86 侧补齐饱和算术**：① `arm_jit.cpp` 的 `OP_QADD`/`QSUB`/`QDADD`/`QDSUB` 从"`printf` + 回退解释器"改为内联，与 arm64/解释器同一份 DDI 0403E 语义（`seto` + `cmovnz`，两段饱和分别置 Q，N/Z/C/V 不动）；② `jit_diff` 新增 6 条 Q 探针（含 `Rd` 别名），704 次比较 0 mismatch，负对照（`cmovz`）证明探针有效，`--full` 81920 次比较功能桶全 0；③ 修掉 x86 侧 `g_TOTALBYTES`/`g_all_block_bytes` 无定义、导致新建核心库链接不出差分台的缺陷（§S4 族 7+8） |

---

## 0. 结论先说

这仍然不是"升个版本"，但现在它有**明确的推进单位**和**明确的判决依据**：

- 推进单位是**一条指令**（一个 handler 及其覆盖的所有分派槽位），共 **591 组**，其中 **571 组要改**、
  **20 组本来就回退解释器**（见 §2.2）；
- 判决依据是**解释器**（`arm_instructions.cpp` / `thumb_instructions.cpp`），两个 JIT 都必须与它逐字段一致；
- 因此这次迁移同时是一次**审计**：凡是两个 JIT 与解释器三方不一致的地方，基本都藏着既有 bug。

### 0.1 四条硬约束

1. **两个后端必须成对改。** 同一条指令的 x86 侧与 arm64 侧在同一个 commit 里改完、同时置 done。
   不允许"x86 全做完再回头做 arm64"——那样两个后端会长期停在不同语义版本上，
   差分台就失去了交叉验证价值（而这正是第 3 章的全部价值来源）。
2. **ARM32 宿主后端先删。** 它已经是死代码（证据见 §2.4），留着只会让"共享文件里到底哪条路径在编译"变得不可判定。
3. **arm64 侧终点是 `a64::Compiler` + 虚拟寄存器。** 不是把 `arm64_emit.cpp` 的手写编码逐条翻译成
   `a64::Assembler`——那只是把"手写机器码"换成"手写 asmjit 调用"，错误率和性能都不会变。
   决定它的三条理由写在 §1.4。
4. **不追求字节级一致。** 两端都会经过寄存器分配，生成的机器码必然不同。
   验收口径是**行为等价 + 指令数在合理区间 + 性能不退化（arm64 要求明显提升）**。

### 0.2 影响面（实测，不是估计）

| 项目 | 数据 |
|---|---|
| **x86/x64 后端** | `src/arm_jit.cpp`，**4536 行** |
| └ asmjit 调用点 | **935** 处 `c.xxx(...)`，共 55 个不同方法（`mov` 260 / `newGpVar` 167 / `add` 60 / `and_` 46 …） |
| └ 虚拟寄存器 | 167 处 `newGpVar(kX86VarTypeGpd/Gpz)` |
| └ 标签 / 调用 / 绝对地址 | 26 `newLabel`、25 `c.call`+45 `setArg`、6 处 `ptr_abs` |
| └ handler 形态 | **571** 个 `static int OP_*(const u32)`：460 个是一行宏包装、**111 个手写**；宏体 **18 个**（19 处 `#define`，`OP_SMxxW_` 在 `#ifdef` 里定义了两次） |
| **AArch64 后端** | `src/utils/arm_jit/arm_jit_arm.cpp`，**4463 行** + `arm64_emit.cpp`，**1950 行** |
| └ handler | **591** 组，其中 **20 组是 `#define OP_X NULL`**（回退解释器），真编译 571 个 |
| └ 后端原语 | `arm64_emit.cpp` 内 **206** 个函数；共享文件里 573 处 `emit_*` 调用、9 处 `genlabel()` |
| └ **访问入口** | **6 个 `get_ptr*` 内联函数 + 27 个访问宏**（§2.2 的"真正的改入口"） |
| └ 编码器 | 全手写：`t_bytes` 字节块 + 全局标签表 + `MOD_JUMP` 回填 + `memcpy` + icache flush |
| **要删的 ARM32 后端** | `src/utils/arm_jit/arm_emit.cpp`，**1317 行 / 190 个函数** |
| **共用件（不动）** | 分派表 `instruction_tabdef.inc`（4096 槽）+ `thumb_tabdef.inc`（1024 槽）；代码缓存 `arm_jit.h`；块级边界契约 `jit_dma_event.h` |
| **基准（不动）** | 解释器 `arm_instructions.cpp`（151KB）+ `thumb_instructions.cpp`（33KB），表 `arm_instructions_set[2][4096]` / `thumb_instructions_set[2][1024]` |
| 构建文件 | 2 个 `.vcxproj`、2 个 `.xcodeproj`、`posix/Makefile.am` + `configure.ac` |

### 0.3 C++20 前提

新版 asmjit **强制 C++20**。DeSmuME 主体是 C++17。**不要**把整个项目提到 C++20。
按文件设语言标准，你自己的 VBA 仓库已有现成模板（`vba-rerecording\win32\VBA_Latest.vcxproj:1363` 起）：

```xml
<ClCompile Include="..\..\..\3rdparty\asmjit\core\jit_runtime.cpp">
  <LanguageStandard>stdcpp20</LanguageStandard>
  <PreprocessorDefinitions>ASMJIT_STATIC;%(PreprocessorDefinitions)</PreprocessorDefinitions>
  <AdditionalIncludeDirectories>..\..\..\3rdparty;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
</ClCompile>
```

（上面这段是按本仓库的实际落盘路径改写的——vendor 在 `desmume/3rdparty/asmjit/`，
include 根指向 `3rdparty`，因为上游写的是 `#include <asmjit/core.h>`。）

### 0.4 构建前置风险（已排查）

rev3 把「能不能构建」列为 S0 的第一风险。实测结论：

| 项 | 结论 |
|---|---|
| MSVC 工具链 | VS **18 Insiders**（`C:\Program Files\Microsoft Visual Studio\18\Insiders`），MSVC **14.52.37025**，SDK **10.0.28000.0** |
| 工程入口 | 只有 `src/frontend/windows/DeSmuME.vcxproj`（x64：Debug / Dev+ / Release / Release Fastbuild）；无 CMake |
| `HAVE_JIT` | `desmume.props:160` 已全局定义 `HAVE_JIT=1`，Windows 侧**必然**编译 `arm_jit.cpp` |
| `arm_jit_arm.cpp` | **不在任何 vcxproj 里**——Windows 构建从来只编 x86 后端，删 ARM32 对 Windows 零影响 |
| `arm_jit.cpp` 语法 | `clang-cl --target=x86_64-pc-windows-msvc -fsyntax-only` + MSVC 14.52 头文件 + `-std:c++14` + `NOMINMAX` → **exit=0** |
| 全量 `msbuild` | **未跑通**：本机沙箱把 `MSBuild.exe` / `cmd.exe` 判为 LOLBin 直接拦掉，`cl.exe` 也在实际编译时被截断。因此"能否出 exe"仍是**未验证项**，需要在沙箱外跑一次 `tools\jit_matrix\build_check.bat`（见下） |

`tools/jit_matrix/build_check.bat` 是替代入口：动态探测 MSVC/SDK 版本，
只编译 20 个 asmjit 1.0 源文件 + `arm_jit.cpp`，不链接、不碰 GUI。
它就是「工具链到底认不认这份代码」的最小可复现判据。

> 顺带一个发现：`arm64_emit.cpp:129` 把 `uint32_t*` 传给 `_BitScanReverse()`
> （其形参在 MSVC 上是 `unsigned long*`）。Apple/Android 走 `__builtin_clzll` 分支，所以
> 现在没暴露；但 **Windows-on-ARM 用 MSVC 编译时会在这里报 C2664**。
> 这不属于本次迁移的改动范围，记在 §6.3 风险清单里，S2 重写编码器时自然消失。

---

## 1. 现状：四份实现，一张表

```
                     ┌──────────────────────────────────────────────┐
                     │  instruction_tabdef.inc  (4096 槽, 521 handler)│
                     │  thumb_tabdef.inc        (1024 槽,  70 handler)│  ← 两个 JIT 共用
                     └───────────────┬──────────────────────────────┘
                     ┌───────────────┴───────────────┐
                     ▼                               ▼
   ┌──────────────────────────┐     ┌──────────────────────────────────┐
   │ x86/x64 宿主             │     │ AArch64 宿主  ← 本次重写         │
   │ src/arm_jit.cpp  4536 行 │     │ arm_jit_arm.cpp 4463 行          │
   │  · x86::Compiler         │     │  · 固定物理寄存器 R2..R9 / X24   │
   │  · 虚拟寄存器 + RA       │     │  · 每次操作数访问重载 CPU 基址   │
   │  · asmjit 1.0            │     │  · arm64_emit.cpp 1950 行手写编码│
   └──────────────────────────┘     └──────────────────────────────────┘
                                                    ↑
                              ┌─────────────────────┴───────────────┐
                              │ ARM32 宿主 arm_emit.cpp 1317 行     │
                              │ 已死（§2.4），本次删除              │
                              └─────────────────────────────────────┘

   ┌──────────────────────────────────────────────────────────────────┐
   │ 解释器  arm_instructions_set[2][4096] / thumb_instructions_set   │  ← 唯一基准
   │         arm_instructions.cpp / thumb_instructions.cpp            │
   └──────────────────────────────────────────────────────────────────┘
```

### 1.1 为什么"按指令迁移"在这个代码库里是可行的

因为它天然存在一张**两边共享的工单**：`instruction_tabdef.inc` / `thumb_tabdef.inc`。

- ARM 表 4096 个槽 → **521 个不同 handler**（其中 `OP_UND` 独占 348 槽）；
- Thumb 表 1024 个槽 → **70 个不同 handler**；
- 两个集合**零重叠** → 合计 **591 组**，其中 **20 组在两个后端都是 `#define OP_X NULL`**（§2.2），
  所以真正要迁的是 **571 组**；
- 两张 `.inc` **被两个后端同时 include**（`arm_jit.cpp:3841/3847`、`arm_jit_arm.cpp:3790/3796`）
  → 「某个槽由哪个 handler 负责」在两个后端之间**不存在分歧**，分歧只可能出现在 handler 实现里。

解释器那张表（`arm_instructions_set[2][4096]`）用的是**完全相同的槽位编号**，
所以差分测试可以按同一个整数 `index` 同时索引三方实现——这是第 3 章能自动化的前提。

### 1.2 两个后端是逐行同构的（这是"成对改"的依据）

`sync_r15` / `emit_branch` / `compile_basicblock` / `emit_armop_call` 在两份文件里是**一一对应**的：

| 组件 | `arm_jit.cpp` | `arm_jit_arm.cpp` | 差异实质 |
|---|---|---|---|
| 入口 | `c.clear()` + `newFunc(hint naked)` | `allocBytes()` + `emit_startfunc()` | 生命周期 API |
| PC 同步 | `sync_r15(op, is_last, force)` | `sync_r15(op, is_last, force)` | **逐行同逻辑** |
| 条件跳 | `emit_branch(cond, Label)` | `emit_branch(cond, uint64_t id)` | 同一张 `cond_bit[]`、同一个 `arm_cond_table` 查表兜底 |
| 分派 | `arm_instruction_compilers[...]` | 同名 | 共享 `.inc` |
| 回退解释器 | `c.call((void*)f)` + `Prototype` | `emit_ptr(f, R3)` + `callR3` | 调用约定 API |
| DMA 提前退出 | `emit_dma_event_check()` → `c.ret()` | 同名 → `emit_endfunc()` | **同一处中途返回**，见 §3.7 |

结论：**一条指令的语义在两个后端里可以用同一段伪代码描述**，
所以"成对迁移"不是行政要求，而是代码结构本身支持的。

### 1.3 两代 asmjit 的结构性差异（x86 侧适用，六项）

| # | 维度 | asmjit 1.0（现在） | master | 影响 |
|---|---|---|---|---|
| 1 | 命名 | `newGpVar` / `newLabel` / `endFunc` | 全量 snake_case | 935 个调用点，机械替换 |
| 2 | 错误 | `kErrorOk` + **粘性** `getError()` | `enum class Error`，**每次返回** + `ASMJIT_PROPAGATE` | 不能只在末尾查一次 |
| 3 | 生命周期 | `Context` 子类 `generate()` + `make()` | `CodeHolder` + `Target` + `finalize()` → `add()` | `StaticCodeGenerator` 必须重写 |
| 4 | 编译器语义 | `newFunc`+`kFuncHintNaked`+`c.ret`+`unuse()` | `add_func`/`end_func`，PEI 自动 prolog/epilog，`ret`=跳 exit，**无 unuse** | 最高风险，见 R1/R2 |
| 5 | 字符串/容器 | `AsmJit::String` / `PodVector` / `Zone` | `axl::String` / `Span` / `Arena` | 本项目只用 logger，影响小 |
| 6 | 内存所有权 | 全局 `MemoryManager` + 手动 `free()` | `JitAllocator` / `Target::_release` | `arm_jit_reset()` 要重写 |

> 指令助记符（`mov` / `add` / `and_` / `jz` …）**完全不变**，命名空间仍是 `asmjit::x86`。

### 1.4 为什么 arm64 必须上 `Compiler`（性能账，不是审美）

先说结论：**当前 arm64 后端的慢，不是"JIT 就该这么快"，而是三个结构性缺陷叠加**。
这三条都能在代码里指到具体行，也直接决定了改法。

**(1) 每个操作数都要从全局槽重载 CPU 基址**

`arm_jit_arm.cpp` 的所有 CPU 访问宏最终都走 6 个 `get_ptr*`：

```cpp
static int inline get_ptr(void *start, int o, int r, int v) {
    emit_read_ptr64_reg(g_out, (uintptr_t)start, r);  // ldr  x<r>, [&bb_cpu]   ← 重载基址
    emit_addimm64(g_out, r, o, r);                    // add  x<r>, x<r>, #off
    if (v) emit_ldr(g_out, r, r);                     // ldr  w<r>, [x<r>]
    return r;
}
```

`bb_cpu` 是个 `uintptr_t` 全局变量，块入口写一次，之后**每次访问都重新 `ldr` 出来**。
于是一个 ARM `ADD r0,r1,r2` 的代价是：

```
ldr x2,[&bb_cpu] / add x2,#r1off / ldr w2,[x2]    读 Rn     3 条
ldr x3,[&bb_cpu] / add x3,#r2off / ldr w3,[x3]    读 Rm     3 条
adds w3,w3,w2                                              1 条
ldr x2,[&bb_cpu] / add x2,#r0off / str w3,[x2]    写 Rd     3 条
ldr flags / and / orr / str flags                 标志     3~4 条
                                                   ───────────
                                                   ≈ 13 条
```

x86 侧同一件事是 `mov eax,[bb_cpu+r1off]` + `add eax,[bb_cpu+r2off]` + `mov [bb_cpu+r0off],eax`，
其中 `bb_cpu` 是**虚拟寄存器**，RA 保证它常驻，而且 x86 允许把内存操作数折叠进 ALU 指令。

**(2) AArch64 没有内存操作数形式的 ALU 指令**

这条是架构性的，不是实现疏漏：x86 可以 `add eax, [mem]`，AArch64 的 `add` 只吃寄存器。
所以"内存型 JIT"这个策略在 x86 上只需 2~3 条，在 AArch64 上天生要 4 条以上。
**arm64 想快，就必须把 ARM 寄存器提升到宿主寄存器**，而这只有 Compiler + 虚拟寄存器 + 块级活跃期分析才能做到。

**(3) 固定寄存器 + 手写编码器，错误只能靠关路径绕过**

`arm64_emit.cpp` 里有两处**被主动关掉的快速路径**：

```cpp
// arm64_emit.cpp:1539 / 1557
}else if(false && genimm2(imm,32,&armval)) {   // 位掩码立即数直编：发现有问题，直接 false 掉
```

`genimm2` 是手写的位掩码立即数编码器，算出来是错的，于是作者干脆让它永远走
"`mov X10, imm` + `and`"的慢路径。这是手写编码器的典型结局：
**错误的编码不会崩，只会静默算错几个位，于是只能整条路径禁用**。
asmjit 的指令数据库有大量测试覆盖，这类错误由上游承担。

**预期收益（估算，S4 族 1 完成后用基准修正）**

| 场景 | 现在 | Compiler + 块级寄存器缓存 | 依据 |
|---|---|---|---|
| ALU 密集（数据处理族） | ≈13 条/指令 | ≈3~4 条/指令 | 基址常驻 + ARM 寄存器常驻 + 标志延迟合成 |
| 访存密集 | 每次访问 ≈3 条寻址 | 基址常驻后 ≈1 条寻址 | `ptr(base, off)` 直接编码 |
| 溢出/回写 | 每指令 1 次 `str` | 块末只回写脏寄存器 | 块级脏位图 |

> **收益来自"块级寄存器缓存"，不来自"把 `emit_*` 换成 `cc->add`"。**
> 只做后者（仍用物理寄存器）生成码与今天几乎一致、性能持平——但那一版仍然值得做，
> 因为它是**可交付的中间态**，见 §4 的两级设计。

---

## 2. 迁移单位：一条指令 = 一张工单

### 2.1 工单字段

```
id            591 条之一，形如 A-0214 / T-0031
槽位          tabdef 里所有指向它的 index（一个 handler 可能覆盖十几个槽）
语义          ARM ARM 条款（如 A8.8.132 STR）
解释器        arm_instructions.cpp:行号 / thumb_instructions.cpp:行号
x86 侧        arm_jit.cpp:行号（宏体 or handler）；改写后 finalize 无错
arm64 侧      arm_jit_arm.cpp:行号 + 用到的 get_ptr*/访问宏 + 是否已用虚拟寄存器
状态          todo / x86-done / a64-struct-done / diff-pass / perf-ok / disputed
已知偏差      三方不一致时的结论与依据（条款号 / armwrestler 用例）
```

### 2.2 工单总量与分组（由 `tools/jit_matrix/gen_opcode_matrix.py` 从分派表算出）

| 族 | handler 数 | 说明 | x86 侧改入口 | arm64 侧改入口 |
|---|---|---|---|---|
| 数据处理（AND/EOR/SUB/RSB/ADD/ADC/SBC/TST/TEQ/CMP/CMN/ORR/MOV/BIC/MVN 的 `*_IMM_VAL` 形式） | **28** | 不带移位的 ALU，先把标志语义打准 | `OP_ARITHMETIC(3)` / `OP_MOV(2)` / `OP_TST_` / `OP_TEQ_` / `OP_CMP` / `OP_CMN` | 6 个 `get_ptr*` + 标志宏 |
| 移位（同一批助记符 × LSL/LSR/ASR/ROR × IMM/REG × 带不带 S） | **224** | 条数最多，但背后只有 **8 个移位辅助**（4 种 × IMM/REG），S 变体只是多算 C | `OP_ARITHMETIC(3)` 的 arg 分支 | `emit_lsl/lsr/asr/ror/rrx` + `*s` |
| 访存（LDR/STR/LDRB/STRB/LDRH/STRH/LDRSB/LDRSH/LDRD/STRD/SWP/SWPB + 前/后索引 × 5 种偏移） | **174** | 语义坑最密（对齐、写回、PC 基址） | `OP_LDR_` / `OP_STR_` 两个宏体覆盖大半 | `dword_ptr`/`byte_ptr`/`word_ptr` 族 + `emit_write_ptr32_*` |
| LDM/STM（4 方向 × 2 模式 × 写回） | **32** | 基址寄存器在列表内的边界 | 手写 | `op_ldm_stm*` + 写回宏 |
| 乘法/乘加（MUL/MLA/UMULL/SMULL/UMLAL/SMLAL + B/T 半字 + W） | **28** | 标志位、S 位、64 位累加 | `OP_MUL_` / `OP_MULxy_` / `OP_SMxxW_` | `emit_mul/mla/smull/...` |
| 协处理器（MCR/MRC/CDP/LDC/STC） | **17** | **15 个回退解释器**，只有 MCR/MRC 真编译 | 手写 | `cp15_ptr` 族 |
| 状态寄存器（MRS/MSR × CPSR/SPSR × reg/imm） | **6** | 模式切换副作用 | `OP_MSR_` 宏体 | `OP_MSR_` 宏体 |
| 饱和/计数（QADD/QSUB/QDADD/QDSUB/CLZ） | **5** | 两端都是手写 | 手写 | 手写 |
| 分支（B/BL/BX/BLX/BKPT） | **5** | PC 写入与流水线 | 手写 | `op_b` / `op_bx` |
| 异常/未定义（SWI/UND） | **2** | 走 `JIT_BYPASS` | 手写 | 手写 |
| **ARM 小计** | **521** | | | |
| **Thumb 小计** | **70** | 全部手写，无宏；其中 3 个宏体（`OP_LOGIC` / `OP_SHIFTS_IMM/REG`）只有 Thumb 用 | | |
| **合计** | **591** | | | |

**20 个 handler 是 `#define OP_X NULL`**：`emit_armop_call` 看到编译器指针为空就直接调解释器，
它们**本来就不生成代码**，既不会分叉也不需要迁移。两个后端的这 20 个集合**完全一致**（脚本每次生成都会校验）。
明细见 `documentation/jit-opcode-matrix.md` 的"回退解释器的指令"一节。

> **关键判断**：要迁的是 **571** 条，两边的真实改动面又都**远小于 571**。
> x86 是 18 个宏体 + 111 个手写 handler；arm64 是 6 个 `get_ptr*` + 27 个访问宏 + 需要临时寄存器的地方。
> 571 是**验收粒度**，不是**编辑粒度**——按验收粒度推进才不会漏，按编辑粒度动手才快。

### 2.3 每张工单的完成定义（DoD，逐条勾）

- [ ] 解释器实现已读懂，并把 **条款号 + 副作用清单**（改哪些寄存器/标志/内存/模式）写进矩阵
- [ ] **x86 侧**改写完成，`finalize()` 返回 `kOk`，反汇编与旧版逐条配对一致
- [ ] **arm64 侧结构完成**：走 asmjit 发射，反汇编与旧版逐条配对一致
- [ ] **arm64 侧寄存器完成**：该 handler 不再用 `tmp`/`tmp2` 这类固定临时寄存器，改为虚拟寄存器 + 状态缓存
- [ ] 差分通过：该 handler 覆盖的**全部槽位** × 16 组条件标志 × 8 组边界向量，三方逐字段相等
- [ ] cycle 记账相等（含条件执行与不执行两种情况）
- [ ] 若该 handler 是 store：`instr_is_store()` 判定一致（§3.7）
- [ ] 若出现三方不一致：已定位并记录"谁错、为什么错、依据是什么"
- [ ] 矩阵状态置 `diff-pass`（性能族再置 `perf-ok`）

> **反面判据**：只做到"能编译、能跑、游戏不崩"不算完成。JIT 的错误不会在编译期暴露，
> 只会在"某游戏某处花屏"时暴露——而那时你已经改过 300 条指令，无法二分。

### 2.4 ARM32 后端已经是死代码（删除的依据，实测）

`arm_jit_arm.cpp` 通过 `#ifdef __aarch64__ → arm64_emit.cpp / #else → arm_emit.cpp` 二选一 include。
按两边出现过的原语 token 做差集，正好 **18 对 18**：

- **arm32 有、arm64 完全没有（18 个）**：
  `emit_movz`（含 `_lsl16`）、`emit_movk`（含 `_lsl16` / `_lslnum` / `_lslnum64`）、
  `emit_movn`（含 `_lsl16`）、`emit_movz64`、`emit_ldr64`、`emit_str64`、`emit_cmpx64`、
  `emit_testimm64`、`emit_or64`、`emit_lsr64`、`emit_asr64`、`emit_lsl_reg_quick`、`emit_lsr_reg_quick`。
- **同名但语义退化的（2 个）**：`emit_mov64` / `emit_add64` 在 `arm_emit.cpp` 里是
  `#define emit_mov64(o,r1,r2) emit_mov(o,r1,r2)` 的 32 位别名——名字是 64 位，行为是 32 位。
- 共享文件对这些 64 位原语共 **10 处**无条件调用（`emit_mov64` 4 处、`emit_add64` 1 处，其余为零散）。

更关键的是：**arm_emit.cpp 独有的 18 个原语在共享代码里引用数全部为 0**
（`emit_push` / `emit_pop` / `emit_movw` / `emit_movt` / `emit_mrs_cpsr` / `emit_subcs` /
`emit_smulltemp` / `emit_set_cond` / `emit_mov_asl|asr|lsl|lsr|ror` / `emit_*_cond` /
`emit_addimm_and_set_flags` / `emit_loadlp`）。

**结论**：ARM32 宿主（armv7 Android / 老 iOS）这条路径既没有测试覆盖，也已经半失修——
一半的原语是空引用，另一半靠退化别名假装存在。删除它**不改变任何仍被编译的代码路径**，
却让每周的验证矩阵少一维。

> **状态：已删除（S1.1）。** 落地时补了一次符号闭合检查：
> `arm_jit_arm.cpp` 引用的 128 个 `emit_*`，123 个由 `arm64_emit.cpp` 提供、
> 5 个是该文件自己的成员，**无悬空符号**。详见 §S1.1。

---

## 3. 三方言语义对账（本次迁移的核心价值）

### 3.1 三方是谁，谁说了算

| 角色 | 实现 | 地位 |
|---|---|---|
| 基准 | 解释器 `arm_instructions_set` / `thumb_instructions_set` | **唯一权威**。有疑义时以它为准 |
| 受测 1 | x86/x64 JIT | 受测 |
| 受测 2 | AArch64 JIT | 受测 |

**为什么必须三方而不是两方**：
两个 JIT 一致**不能**证明正确（可能一起错，因为它们的历史实现互相借鉴）；
两个 JIT 不一致时，光看两个 JIT 无法判断谁对。解释器是唯一独立实现，必须是裁判。

**但基准也可能是错的**。解释器同样有历史 bug（尤其 CP15、非对齐访问、某些未定义指令的副作用）。
所以判决流程是三步仲裁（§3.5），不是"无脑信解释器"。

### 3.2 跨机器的三方对比怎么落地

x86 宿主机跑不了 arm64 JIT 生成的代码，反之亦然。所以三方对比必须拆成**本地两方 + 离线第三方**：

```
每台宿主机（x86-64 / aarch64）各自跑同一份测试程序：
   向量 (index, opcode, 输入状态)
      ├─ 解释器执行 → 规范化状态 S_interp
      └─ 本机 JIT 执行（jit_max_block_size = 1）→ 规范化状态 S_jit
   比较 S_interp vs S_jit        ← 本地自动判定，失败即断
   若相等，落盘一行：  index opcode vec_id  hash(S_interp)  hash(S_jit)  cycles

跨机器：
   两边各自的 dump 文件按 (index, opcode, vec_id) 对齐
   hash(S_interp) 必须两边相同（同一个解释器！）——不同说明向量生成或状态规范化有不确定性
   hash(S_jit) 两边都等于 hash(S_interp) 才算三方通过
```

这个设计有三个好处：

1. 任何一台机器上都能独立跑，不需要交叉架构；
2. `hash(S_interp)` 天然成为**测试台自检**——两边不一致就是测试台本身有问题（未初始化内存、地址相关值、时间相关值）；
3. dump 文件可以进 CI artifact，回归时只比 hash 列表，秒级完成。

### 3.3 状态快照与比较面（哪些能比，哪些不能）

**能比的（canonical 面）**

| 项 | 说明 |
|---|---|
| `R[0..14]` | 全部 15 个通用寄存器 |
| `CPSR` 全字段 | N/Z/C/V/T/模式位/I/F，以及 banked 寄存器（模式切换后） |
| `SPSR` | 仅当指令写 SPSR（`MSR SPSR`、异常） |
| `next_instruction` | **仅在单指令块下有效**，见 §3.4 |
| 目标内存写集 | 用写前快照 XOR 写后快照，比较**差异页集合**，不是全内存 |
| `bb_total_cycles` | 见 §3.4 的记账公式 |
| 协处理器副作用 | CP15 的特定寄存器（如 cache 控制），按白名单比 |

**不能比的**

| 项 | 为什么 |
|---|---|
| `R[15]`（PC） | 解释器里 R15 = 当前指令地址 + 8（ARM）/ +4（Thumb），JIT 里只有在 `instr_uses_r15(opcode)` 时才落内存，否则是脏值 |
| `instruct_adr` | 只在 `JIT_BYPASS` / `BRANCH_SWI` / 块末才同步，单指令块下才确定 |
| `cpu->cycles` 全局计数 | 由外层调度累加，不属于单条指令语义 |
| MMU 缓存 / TLB 状态 | 两个实现的查表路径不同，但语义等价；比较会导致假阳性 |
| `armcpu_t` 里未使用的填充 | 未初始化字节 |

### 3.4 四个必须处理的技术坑

**坑 1：PC / next_instruction 依赖块是否结束**

`sync_r15(opcode, is_last, force)` 的同步条件是
`force || JIT_BYPASS || BRANCH_SWI || (is_last && !instr_is_branch)`。
若用默认块大小（15）跑差分，`next_instruction` 根本不会被写，比较结果全是假阳性。

→ **差分台必须强制 `CommonSettings.jit_max_block_size = 1`**，让每条指令都是块内最后一条。
→ 只有在 `jit_max_block_size = 1` 下，"一条指令"才等于"一个块"，本地两方比较才成立。

**坑 2：cycle 记账公式（条件执行）**

编译期累加：`bb_constant_cycles += instr_is_conditional(opcode) ? 1 : cycles`；
运行期对条件执行再补 `bb_total_cycles += (bb_cycles - 1)`（仅在条件成立时）。

合起来的契约是：

```
条件执行指令:  cycles_total = 1 + (条件成立 ? 实际 cycles - 1 : 0)
非条件指令:    cycles_total = cycles
```

而解释器在条件不成立时由 `armcpu_exec` 直接给 **1**（见坑 4）。这条必须在 S4 的第一批里对齐——
对不齐的话单条指令看不出来，整机表现是帧率缓慢漂移，极难定位。

**坑 3：DMA 提前退出是"块级"语义，单指令差分测不到**

`emit_dma_event_check()` 会在块中间返回，并把 `instruct_adr` / `next_instruction` 改写成 `resume_adr`。
这是块级控制流，不能用单指令差分覆盖。详见 §3.7。

**坑 4：条件判断不在指令函数里**

`arm_instructions_set[proc][idx](opcode)` 这个调用**不做条件判断**——条件检查在
`armcpu_exec<PROCNUM>()`（`armcpu.cpp:673-692`）里，先 `TEST_COND(CONDITION(...), CODE(...), CPSR)`，
不成立就 `cExecute = 1; // If condition=false: 1S cycle`，**根本不调用指令函数**。

→ 差分的解释器一侧必须自己复刻这段：条件不成立时不调用指令函数，直接记 `cycles = 1` 且状态不变；
→ 直接复用 `armcpu.h` 的 `TEST_COND` / `CONDITION` / `CODE` 宏，不要重写判断（重写就是引入第四个实现）；
→ 忘了这一条的话，**所有条件指令**（数据处理族 252 条里占绝大多数）都会报假阳性。

### 3.5 三方不一致时的仲裁流程

```
三方不一致
  ├─ 只有 x86 偏 → 先假设 x86 侧迁移写错了；用旧版 arm_jit.cpp 生成的码跑同一向量对照
  ├─ 只有 arm64 偏 → 同上，用旧版 arm64_emit.cpp 对照
  ├─ 两个 JIT 都偏、且偏法相同 → 高度怀疑解释器，但也有可能是旧 bug 被两个 JIT 一起继承
  └─ 两个 JIT 偏法不同 → 至少一个错，且很可能两个都错
```

仲裁依据按优先级：

1. **ARM ARM 条款**（指令的 pseudocode 是最终裁判）；
2. **`tools/ds_tests/armwrestler`**（仓库里已有现成的单指令一致性测试 ROM，
   `source/armwrestler-ds.s` / `thumbwrestler-ds.s`）——它在真机上跑过，是"真实硬件行为"的代理；
3. **游戏回归**（`jit-bug.md` 的《恶魔城》流程）作为最后的否决权。

把结论写进工单的"已知偏差"字段。**不要为了让测试变绿而去改解释器**——
除非能引用条款或 armwrestler 用例证明解释器确实错了；那种情况必须单独开一个 commit，
并在 commit message 里标注"解释器行为变更（附依据）"，方便将来 bisect。

**哪些东西 ARM ARM 裁不了**：只有**架构可见状态**（R0–R15、CPSR、内存写入、`instruct_adr`）
能用条款裁决。周期数是实现定义的——ARM ARM 不规定一条指令几拍，`OC`/`SH` 的具体时序取决于
核的微架构。所以差分台里的"周期数不一致"是**记录项**，不进仲裁、不据此改代码；
真要收口那是 S4 的性能门槛（§1.4）和 S5 的收尾项，判据是"整机帧率不退化"，不是"逐槽位拍数相等"。

**已按此流程裁定的先例**（写在这里是为了下次遇到时不用重新发明判断）：

| 分歧 | 三方表现 | ARM 条款 | 结论 |
|---|---|---|---|
| Thumb `ASR #0` 的 C | 解释器 = bit31；arm64 后端 = bit30；x86 后端 = bit31（借 `sets` 从 SF 取，恰好对） | `DecodeImmShift`：`imm5 == 0 → shift_n = 32`；`ASR_C`：`x[Min(shift-1, N-1)]` | arm64 错，改后端 |
| Thumb `NEG` 的 V（操作数 `0x80000000`） | 解释器（clang -O3）= 0；两个 JIT = 1 | `RSB (immediate)` T1 = `AddWithCarry(NOT(Rn), Zeros(32), '1')`；E1.2.2：减法有符号溢出则 `nzcv[0] = 1` | 解释器错，改解释器（单独 commit） |

两者的教训是同一条：**"多数一致"不是判据**。第二例里"两个 JIT 都偏、且偏法相同"按上面的流程
本该优先怀疑解释器——事实也确实如此，但别停在这个启发式上，条款才是终点。

### 3.6 逐族语义契约清单（写测试用例时按这个查）

**数据处理 / 移位**

| 检查点 | 常见错法 |
|---|---|
| `MOV`/`MVN` 的 S 位：不写 Rd 只写标志 | 只写标志却误写了 Rd（或反之） |
| 逻辑运算的 C 标志 = 移位器进位输出（不是旧 C） | 直接保留旧 C |
| `LSR #0` 实际是 `LSR #32`；`ASR #0` 是 `ASR #32`；`ROR #0` 是 `RRX` | 当成 0 位移处理 |
| 寄存器移位量为 0 时 C 不变；非 0 时 C = 最后移出的位 | 一律更新 C |
| `ADC`/`SBC` 的 C 是**输入**也是**输出** | 用错顺序（先加再取旧 C） |
| `RSB` 的操作数顺序（`Rn - Rm` vs `Rm - Rn`） | 两端历史上都错过 |
| `TEQ`/`TST` 不写 Rd，`CMP`/`CMN` 不写 Rd | 误写 Rd |
| ARM 的 `V` 标志定义（有符号溢出） | 用无符号进位的判据 |

**访存**

| 检查点 | 常见错法 |
|---|---|
| 前索引 / 后索引的写回时机（写回是"基址 + 偏移"，不是有效地址） | 用有效地址写回（P=0 时错） |
| `STR` 的写回在 store 之前还是之后；Rd == Rn 时的优先级 | 未定义行为，三方最易分歧 |
| 非对齐访问：ARM9 对 LDR 做循环右移，LDRH 对奇地址做字节拼接 | 直接按对齐处理 |
| `LDRSB`/`LDRSH` 的符号扩展宽度（byte→32 / half→32） | 扩到 16 位就停 |
| PC 作为基址时的取值（`+8` / `+4`） | 用 `bb_adr` 而不是 `bb_r15` |
| `LDRD/STRD` 要求 4 字节对齐，否则行为未定义 | 未检查 |
| `SWP`/`SWPB` 的原子性与总线锁定副作用 | 当成普通 load+store |

**LDM/STM**

| 检查点 | 常见错法 |
|---|---|
| 基址在寄存器列表里时的写回值（ARM 规定"已写回"或"原始值"，取决于版本） | 两端各自发挥 |
| 空列表（`reg_list == 0`）：ARM7 与 ARM9 行为不同 | 未区分 PROCNUM |
| `LDM` 含 PC 时的模式切换与 `SPSR` 恢复 | 漏掉 `armcpu_switchMode` |
| `STM` 的写序与 DMA 交互（§3.7） | 顺序错导致 DMA 抢先 |

**乘法 / 饱和**

| 检查点 | 常见错法 |
|---|---|
| `MUL`/`MLA` 的 N/Z 由 32 位结果定，C 由**旧实现**决定（ARM7 上 C 破坏 / ARM9 上 C 保留） | 两端统一成一种 |
| `UMULL`/`SMULL` 的 N/Z 由 **64 位**结果的高 32 位定 | 只看低 32 位 |
| `SMLAL`/`UMLAL` 的累加在标志计算之前 | 顺序反了 |
| `QADD`/`QSUB` 的饱和条件与 Q 标志（CPSR 的 sticky Q） | 忘记置 Q |
| `SMULxy` 的 B/T 半字选择（`x` 选 Rm，`y` 选 Rs） | 两端弄反 |

**状态寄存器 / 异常 / 分支**

| 检查点 | 常见错法 |
|---|---|
| `MSR` 的 4 个字节掩码位（bit16..19），USR 模式下只有 flags 字节可写 | 全写 |
| 模式切换后 banked 寄存器组切换（`armcpu_switchMode`） | 漏调或调错顺序 |
| `BX`/`BLX` 的 bit0 决定切换到 Thumb | 忘记改 `CPSR.T` |
| `BL` 的 LR 值（返回地址 = 当前 + 4，Thumb 下置 bit0） | 漏 bit0 |
| `SWI` 后 `instruct_adr` / `next_instruction` 的落点 | 与解释器不一致 |

### 3.7 块级边界语义：`2d4919aa` 事故复盘（迁移的高危回归点）

**事故**：`2d4919aa` *JIT: Leave the block when a DMA channel is armed*（2026-11-18）。
《恶魔城 苍月十字架》在 `jit_max_block_size >= 16` 时 HUD、伤害数字、对话框边框丢图块。

**根因**（不是某条指令算错，是**指令之间**的语义丢了）：

- 写 `DMAxCNT` 且模式为立即启动时，`DmaController::doSchedule()` 把 `nextEvent` 设为 `nds_timer`，
  即**这笔传输在当前时间戳就到期**；
- 解释器每执行一条指令就回到 sequencer，所以传输一定在下一条指令之前跑完；
- JIT 只在**块之间**检查 `sequencer.reschedule`。当两次对同一通道的写落在同一个块里，
  第二次写覆盖了第一次锁存的参数，传输就再也不会发生；
- 游戏的上传是两段式：先 `0x84000001` 填 512 字节暂存区，紧接 `0x81400001` 清通道。
  块大小 15 时中间有事件边界，16 时两条进同一块 → 2048 次填充退化成 16 位 0→0 拷贝。

**修复的契约（迁移必须逐字保留）**

1. `DmaController::write32()` 只在**CPU 自己的编程写**时置 `mmu_dma_event_pending`；
   传输结束时的内部 reschedule 不得置位（那时事件在 `time_elapsed` 之后，不是"现在"）。
2. 每条写内存的指令之后检查该标志，置位则**立即结束块**，并且：
   ```
   mmu_dma_event_pending = 0
   cpu->instruct_adr     = resume_adr
   cpu->next_instruction = resume_adr
   bb_total_cycles      += bb_constant_cycles     // 只加"已发射指令"的常量部分
   return bb_total_cycles
   ```
3. 检查的位置**必须在 `bb_total_cycles += bb_cycles` 之后**——否则 mid-block 返回时的周期数是错的。
4. 哪些指令算 store，由 `src/jit_dma_event.h` 的 `instr_is_store()` 统一判定。
   这是全仓唯一一处"块级语义必须跨后端一致"的地方，头文件注释里写清楚了原因
   （两个后端互相看不见对方的块编译器，所以分类只能共享）。

**为什么这次迁移一定会碰它**

| 迁移动作 | 风险 |
|---|---|
| `c.ret()` → `FuncRetNode`（x86） | 中途 ret 汇合到统一 exit，`bb_total_cycles` 的累加顺序与恢复动作可能变化 |
| `emit_endfunc()` → `cc.end_func()`（arm64） | 同上；且 prolog/epilog 由 PEI 生成后，中途返回点的栈状态要对 |
| 重写 `compile_basicblock` | `instr_is_store()` 的调用点位置（相对 cycle 累加、相对 `emit_armop_call`）极易搬错 |
| 引入块级寄存器缓存（§4 S4） | **块中途返回前必须先 flush 脏寄存器**，否则 CPU 内存状态是旧的 |

最后一条是 Compiler 方案独有的新坑：以前每条指令都直接写内存，天然没有"寄存器里还有脏值"的问题；
引入缓存后，**任何 `ret` / 任何 C 调用之前都必须 flush**。

**怎么验证（三层，缺一不可）**

1. **块级用例**：一条会 arming DMA 的 STR + 后跟 3 条指令，断言两个后端都在同一条 `resume_adr` 退出、
   `bb_total_cycles` 相等、`mmu_dma_event_pending` 被清零。
2. **块大小扫描**：《恶魔城》HUD 在 15 / 16 / 100 三档全部正常；并记录"每次重建的 2048 次暂存写"计数，
   与基线逐字节对比（`jit-bug.md` §4.1 的取证方法）。
3. **分类一致性（新增，可自动化）**：对 4096 + 1024 个槽位逐个跑 handler，观察是否发生内存写，
   与 `instr_is_store(opcode, thumb)` 比对。
   > 这条能防住"以后有人加了新 store handler 却忘了更新分类表"——那种 bug 只会表现为
   > "某个游戏在某个块大小下丢图块"，几乎不可能靠人眼回归抓到。

### 3.8 产物：`documentation/jit-opcode-matrix.md`（已生成）

整个迁移只有一张进度表，就是它。**由脚本生成，不要手改**：

```bash
python tools/jit_matrix/gen_opcode_matrix.py
```

脚本解析两张 `.inc` 得到 591 组的槽位分布，再在三个实现文件里定位每个 handler 的定义行，
输出两份：

| 文件 | 用途 |
|---|---|
| `documentation/jit-opcode-matrix.md` | 人看：按族分组，含解释器 / x86 / arm64 三处行号、x86 宏体、状态 |
| `documentation/jit-opcode-matrix.csv` | 机器看：差分台按 `first_slot` / `slots` 生成向量 |

一行长这样（真实输出）：

```
| A-0512 | `0x200..0x20F` | `OP_AND_IMM_VAL` | arm_instructions.cpp:340 | arm_jit.cpp:846 | `OP_ARITHMETIC` | arm_jit_arm.cpp:811 | todo | |
```

**脚本每次运行都会断言**下面几件事，任何一条不成立就非零退出——这样"表和代码不同步"会立刻暴露：

1. 槽位数 4096 / 1024；
2. handler 数 521 / 70 / 591；
3. 每个族的条数与 §2.2 一致（28 / 224 / 174 / 32 / 28 / 17 / 6 / 5 / 5 / 2）；
4. 每个 handler 在解释器、x86、arm64 三处都有定义；
5. `#define OP_X NULL` 的集合在两个后端**完全相同**（不一致就说明两边会对同一份游戏表现不同）；
6. 两边的 NULL 集合各 20 个。

它同时是三样东西：排期表、进度表、以及最重要的——
**一份被验证过的"DeSmuME ARM 语义清单"**，迁移结束后仍然值钱。

---

## 4. 阶段方案

每一步都给「动作 / 验收 / 回退点」。**S0 不能跳**——否则后面无法判断"改坏了"还是"本来就坏"。

### S0 · 基线 + 差分台（约 2 天）

这一步产出**整个迁移的度量工具**，不是可选的前置工作。

1. 开分支 `asmjit-master`；`src/utils/AsmJit`（1.0）**原地保留**，只读参照、不参与编译——
   不要再复制一份到别处，避免同一份代码出现两个"权威副本"。
2. 新建 `src/utils/jit_diff/`（独立 TU，不进生产构建）：
   - `jit_diff_gen.cpp`：向量生成器。对 591 组（其中 571 组真编译）覆盖的**每一个槽位**，
     生成 opcode（高位置为该槽位的模式位，低位随机）+ 8 组输入状态
     （寄存器取 `0 / 1 / 0x7FFFFFFF / 0x80000000 / 0xFFFFFFFF / 0x0000FFFF / 0xFF000000 / 随机`，
     内存用 `hash(slot, vec_id)` 作种子预填），条件指令额外遍历 16 组 NZCV。
   - `jit_diff_run.cpp`：对每个向量，跑（a）解释器、（b）本机 JIT（强制 `jit_max_block_size = 1`），
     按 §3.3 的 canonical 面比较，落盘 `index opcode vec_id hash_interp hash_jit cycles`。
   - `jit_diff_cmp.cpp`：两份 dump 对齐比较（跨机器用）。
3. 记录三组基线：
   - **行为基线**：上面那份 dump（旧实现产出的 `hash_jit`）——S3 之后的每一步都对照它；
   - **整机基线**：《恶魔城》HUD，block size 15 / 16 / 100 各一次 + 暂存写计数；
   - **性能基线**：冷启动 JIT 编译耗时、平均帧率，**以及一个可复现的 ALU 密集场景**（S4 的性能门槛要用）。

**验收**：一条命令产出 `baseline/` 目录（行为 dump 一份 + 整机现象 + 性能数字）。
且**老实现自己也过不了差分台**——这是正常的：先把它过不了的清单记下来，
那就是"既有 bug 清单"，是本次迁移的额外产出（注意：这些**不要**在迁移里顺手修，单独开 PR，
否则无法区分"迁移引入"和"顺手修引入"）。

#### S0.1 已完成：差分台可跑，首轮基线落盘

代码在 `src/utils/jit_diff/`（两文件：`jit_diff_main.cpp` + `jit_diff_stubs.cpp`），
由 `tools/jit_matrix/build_diff.ps1` 编译链接到 `build_core/desmume_core.lib`。
两条命令：

```
pwsh -File tools/jit_matrix/build_core.ps1     # 核心静态库（169 TU）
pwsh -File tools/jit_matrix/build_diff.ps1     # 差分台
build_core/jit_diff.exe                        # 18 条定向探针
build_core/jit_diff.exe --sweep                # 5120 槽 × 1 向量 × 2 组标志
build_core/jit_diff.exe --full --all           # 5120 槽 × 8 向量 × 2 组标志，全量列表
```

**跑通差分台踩到的三个坑，都是"测试台自己错"而不是 JIT 错**，已写进代码注释：

1. **解释器执行的是预取字**（`ARMPROC.instruction`），JIT 从 `instruct_adr` 重新读内存。
   只写内存、不写预取字，解释器实际执行的是 `ANDEQ r0,r0,r0`（条件假＝空操作）。
2. **入口状态必须是"流水线中间"**：`R15 = codeAdr + 2*size`、`next_instruction = codeAdr + size`。
   把 R15 设成指令地址，`B #0` 的 target 会算成 codeAdr（应为 codeAdr+8），看着像 JIT 分支错。
3. **标志变体不能碰 T 位**：`0x000000F3` 是"看起来像 NZCV 全置位"的写法，实际置的是 I/F/T/模式位；
   T=1 会让 ARM 扫描走进 `thumb_instructions_set[opcode>>6]`，索引 0x4000 直接越界崩溃
   （`thumb_instructions_set` 只有 1024 项且解释器不做掩码）。正确值是 `0xF00000D3`。

**比较面收窄到契约**：`R0-R14`、`CPSR`、`cycles`、`instruct_adr`。
`R15` 与 `next_instruction` **不比**，理由是代码里写死的：

- `sync_r15()` 只在"块末且不跳转"时写 `next_instruction`；跳转结尾只写 `instruct_adr`；
- `R15` 仅在指令读 PC 时以编译期常量 `bb_r15` 回写，编译块平时不维护 `R[15]`；
- 两个引擎的 `next_instruction` 差一拍（解释器已预取到下一条，JIT 指向下一条本身）。

`instruct_adr` 是两边共同的续跑点，比它就够了；指令读 PC 的正确性由"它写出的寄存器"覆盖。
另外保留一条解释器自检不变量：**发生顺序执行时** `R15 == next_instruction + size`。

**环境问题（一次性排掉 880 个假故障）**：`NDS_Init()` 里 `SPU_Init(SNDCORE_DUMMY)` 按 id 在
`SNDCoreList[]` 里查表，桩文件给的是空表 → 查不到 → 返回 -1 → `NDS_Init` 提前返回，
`wifiHandler` 没有创建。于是每条未定义指令走到 `emu_halt() → wifiHandler->CommStop()` 就是空指针
访问（880 次全崩在同一地址、同一偏移）。用真实核心表
`{ &SNDDummy, NULL }` / `{ &gpu3DNull, &gpu3DRasterize, NULL }` 后故障归零，
并且 `wifiHandler` 由 `NDS_Init` 正常创建。

**首轮基线（`--full`，81920 次比较，两次运行逐字节一致）**：

| 项 | 数量 |
|---|---|
| 引擎异常（访问冲突） | 0 |
| `R0-R14` 不一致 | 0 |
| `instruct_adr` 不一致 | 0 |
| `CPSR` 不一致 | **3**（全是 Thumb `ROR r0,r0`） |
| cycle 数不一致（值合理） | 6869 |
| cycle 数不一致（返回值根本不是周期数） | 2192 |

**这一轮暴露了三个缺陷**，全部定位并修掉（根因、证据和上游位置见 `ISSUE.md`，
代码在 `1f24820b`）：

1. `bb_cycles` 只声明不初始化 —— `INSTR_CYCLES_V` 且 handler 不写它时（`op_b` / `op_swi`），
   块返回的是寄存器分配器留下的垃圾（实测就是 opcode 本身）。
2. 条件指令固定周期 `N>1` 取支时记 `-1` 而不是 `N-1`（上游 `440c6665` 的回归），
   一条取支条件分支记 0 周期，解释器记 3。
3. Thumb `ROR` 的 N/Z 取自 x86 `ror` 留下的标志 —— 而 Intel SDM 规定
   RCL/RCR/ROL/ROR **从不修改 SF/ZF/AF/PF**，所以读到的是上一条指令的残值。

**为什么破例修了它们**：方案原本约定"迁移中不顺手修既有 bug，单独开 PR"。但这三个
正好压在差分台的比较面上——不修，之后每一步迁移都要在 6869 + 2192 条噪声里分辨
"是我改坏了"还是"本来就错"，差分台就失去了作为回归门槛的作用。修完之后噪声只剩
**已知的模型差异**，新出现的差异才能一眼认出来。这三条仍然按"独立缺陷"记录，
没有混进迁移的改动里。

**第二轮（修复后）**：`probes` 288 次 0 不一致；`--full` 81920 次 →
**0 异常 / 0 `R0-R14` / 0 `instruct_adr` / 0 `CPSR` / 12067 cycle 差**，
且这 12067 条全部是"JIT 用 `emit_MMU_aluMemCycles`、解释器用 `MMU_aluMemAccessCycles`"
的模型差异（多为 +1，load/store multiple 是 8-vs-3），属于设计层，不是缺陷。

**尚未完成**：

- **整机基线**（《恶魔城》HUD，block size 15/16/100 + 暂存写计数）：需要 ROM。仓库里
  有前端 exe 和 `__bins/StateSlots/0121 - Castlevania*` 的即时存档，但没有 `.nds`，
  要等用户给出 ROM 路径才能跑。差分台只覆盖单条指令，DMA 事件早退那类**块级**语义
  仍然只能靠整机回归。
- **性能基线**：**推迟到 S2 结束再取**。S2 会把落地路径、prolog/epilog 和寄存器分配
  整套换掉，现在测出的编译耗时与帧率对 S4 的性能门槛没有参考价值。到时候在同一天、
  同一台机器上取"解释器 / 当前 JIT / 迁移后 JIT"三组数字。

**回退点**：无（不改任何生产代码）。

### S1 · 删 ARM32 + 换依赖 + 构建系统（约 1.5 天）

拆成三个可独立 revert 的提交，**S1.3 故意推迟到 S2 的首个提交**（理由见下）。

#### S1.1 · 删 ARM32 ✅ 已完成

- 删 `src/utils/arm_jit/arm_emit.cpp`（40.7 KB）；
- `arm_jit_arm.cpp` 的 include 改成只走 `arm64_emit.cpp`，并加一条硬保护：
  `#ifndef __aarch64__ → #error`，避免以后有人把它误编到别的宿主上；
- 清掉只为 arm32 存在的退化：`JIT_DMA_FLAG_REG` 的 `#ifdef` 改为无条件定义，
  `emit_dma_event_flag_ptr()` 的 `#else`（用 R3 重算地址）整段删除；
- `configure.ac`：JIT 的 `host_cpu` 列表去掉 `arm`，`host_cpu_kind_arm` 只留 `arm64|aarch64`。
  32 位 ARM 宿主**仍然可以构建**，只是不再定义 `HAVE_JIT`，退回纯解释器——这是刻意的；
- `posix/Makefile.am` 无需改：`if HOST_CPU_KIND_ARM` 分支自然只剩 aarch64；
- `DeSmuME (Latest).xcodeproj`：删掉 `arm_emit.cpp` 的 `PBXFileReference` 与 group 子项。
  另外核实了那 3 个 `arm_jit_arm.cpp in Sources` 条目同属一个 phase，
  而该 phase 属于 **"DeSmuME (macOS App; AppleSilicon -- Latest Xcode)"**，即 arm64-only target——
  所以删除 ARM32 不影响任何 x86_64 macOS 目标；
- `DeSmuME (XCode 3).xcodeproj` 本来就没有这两个文件，不用改。

**删除安全性怎么验证的**（没有 ARM 机器，所以做的是静态闭合）：

```
arm_jit_arm.cpp 里引用的 emit_* 符号 : 128
arm64_emit.cpp 提供的定义            : 184
未被 arm64_emit.cpp 满足            : 5 —— 全部是 arm_jit_arm.cpp 自己的成员
                                       (emit_armop_call / emit_branch / emit_dma_event_check /
                                        emit_dma_event_flag_ptr / emit_MMU_aluMemCycles)
→ 闭合，无悬空符号
```

外加 x86 侧回归：`arm_jit.cpp` 用 MSVC 14.52 头文件 + clang-cl 做 `-fsyntax-only`，
**exit=0**（改动只有一行注释，这条是防止误伤）。

#### S1.2 · vendor asmjit master ✅ 已完成

- 落在 **`desmume/3rdparty/asmjit`**（rev3 写的 `third_party` 与仓库既有约定冲突，
  仓库本来就有 `3rdparty/{libs,wgpu}` 且 vcxproj 已在用，故改名）；
- 固定 commit `677a10b`（2026-11-04），`VERSION.txt` 记录 commit / 日期 / 许可 / 复制范围；
- 复制上游 `src/asmjit` 全部头文件（127）与非测试源文件（54），**37 个 `*_test.cpp` 不复制**；
- `ujit/` 的头文件保留、源文件不编译（实验性的统一编译器，DeSmuME 不用）。

#### S1.3 · 构建接线（推迟到 S2 首个提交）

rev3 把「换 vcxproj 里的 asmjit 源文件列表」放在 S1。实际开工后判断应当**推迟**：

> 一旦列表换成 master，`arm_jit.cpp`（还在用 1.0 的 `c.newGpVar` / `Var` 体系）立刻编不过。
> 也就是说 S1 一做完，仓库就进入"必须 `#if 0` 掉整个 JIT 才能构建"的状态，
> 而 S2 要 4 天。中间任何一次回归都无法用"JIT 关掉"来区分是迁移错还是本来错。

改为：**代码迁移（S2）与构建接线同一个提交**，先让两端都能编，再删旧目录。
清单不手抄，由脚本产出：

```bash
python tools/jit_matrix/gen_asmjit_sources.py --check      # 54 源文件 / 127 头文件
python tools/jit_matrix/gen_asmjit_sources.py --format vcxproj
python tools/jit_matrix/gen_asmjit_sources.py --format make
```

接线时要遵守的约束（未变）：

- **不能**定义 `ASMJIT_NO_JIT`（要 `JitRuntime`）、**不要**定义 `ASMJIT_NO_COMPILER`；
- x86 宿主可加 `ASMJIT_NO_AARCH64`，aarch64 宿主可加 `ASMJIT_NO_X86`，各减一半编译时间；
- `arm_jit.cpp`、`arm_jit_arm.cpp` 与 asmjit 源文件单独开 C++20，统一 `ASMJIT_STATIC` + include 路径；
- include 路径指向 **`3rdparty`**（不是 `3rdparty/asmjit`），因为上游用的是 `#include <asmjit/core.h>`；
- 旧目录 `src/utils/AsmJit` 保留但**不编译**，方便 A/B。

**验收**：x86-64 与 aarch64 两个宿主的空项目都能编过 asmjit 静态库；
`arm_jit.cpp` / `arm_jit_arm.cpp` 先 `#if 0` 掉，主程序能链接并运行解释器模式。

**实际落地（2026-11-19，提交 `29f2a3fb`）**：S2 只把 `build_core.ps1` / `build_diff.ps1`
这两条离线脚本接上了，两个**出货工程**一直没动——`DeSmuME.vcxproj` 还列着 AsmJit 1.0 的 29 个源，
Xcode 工程里 asmjit 出现 0 次。两套类的名字还是重叠的（`asmjit::Assembler` 等），
所以"两套都编"不是多几个符号而是直接链接失败。

现在两个工程都由脚本生成，不再手抄：

| 脚本 | 作用 |
|---|---|
| `tools/jit_matrix/wire_asmjit_vcxproj.py` | 摘掉 29 个旧源，换 54 个 vendored，逐文件 `<LanguageStandard>stdcpp20</LanguageStandard>`；`arm_jit.cpp` 同样；`desmume.props` 加 `..\..\..\3rdparty` 与 `ASMJIT_STATIC` |
| `tools/jit_matrix/wire_asmjit_xcode.py` | 同样的活，落到 `project.pbxproj`：文件引用、每个 target 的 build file（带 `COMPILER_FLAGS`）、include 根、`ASMJIT_STATIC`、`GCC_PRECOMPILE_PREFIX_HEADER = NO` |
| `tools/jit_matrix/check_xcode_asmjit.py` | 不开 Xcode 也能验：括号配平、54 个源都引用且存在、每个 JIT target 带 `-std=c++20`、每个配置都有三项设置 |

两个"逐文件"设定都是被逼出来的：

- 工程默认标准是 `gnu++0x` / `stdcpp17`，asmjit 要 C++17；但把整个 target 提到 C++20 会把
  Cocoa 端几百个 `.cpp/.mm` 一起拖进去，风险远大于收益，所以只在 JIT 与 asmjit 这几个 TU 上开。
- 前缀头必须继续**被包含**（`HAVE_JIT` 就在里面），但不能再**预编译**：PCH 是用 `gnu++0x` 生成的，
  clang 拒绝把它交给 `-std=c++20` 的 TU。

范围是"已经在编 JIT 的 10 个 target"（3 个 arm64 的编 `arm_jit_arm.cpp`，7 个 Intel 的编 `arm_jit.cpp`）。
另外两个 target（Universal、OpenEmu Plug-in）原本靠 `UniversalBuildInterface.cpp` 在单个 TU 里
`#include` 旧 AsmJit 1.0 + 对应架构的后端 TU 来获得 JIT——这正是它们的 Sources 里没有 JIT 文件的原因。

**Universal / OpenEmu 已接入（2026-11-20）**：该 shim 已退场（清空为空 TU，保留 build file 条目），
两个 target 现在和其余 target 一样直接登记 54 个 asmjit 源 + 两个后端 TU；
`wire_asmjit_xcode.py` 的 target 筛选增加 `uses_shim`（名称含 `Universal` 或 `OpenEmu Plug-in`）。
后端 TU 各自用 `__aarch64__` 守卫，让错架构的切片编译成空 TU（x86 侧条件为
`!defined(__aarch64__) || defined(__x86_64__)`，arm64 侧为 `defined(__aarch64__)`）。
没有采用 Xcode 的 `EXCLUDED_SOURCE_FILE_NAMES[arch=...]`——实测 `-showBuildSettings -arch arm64`
会忽略这个条件 key，仍显示无条件值，方案不成立。

验证：Universal target 分别以 `-arch arm64` 和 `-arch x86_64` 构建均 **BUILD SUCCEEDED**，
两切片二进制都带 JIT 符号（x86 切片 `__GLOBAL__sub_I_arm_jit.cpp`、arm64 切片
`__GLOBAL__sub_I_arm_jit_arm.cpp`，各自只含自己的后端）；dev+ 全量差分仍全绿。
OpenEmu target 仍编不过，但失败点是 `filter/xbrz19/xbrz.cpp` 的 C++14 语法（该 target 自身的
语言标准设置），与 JIT 无关。本机 Xcode 的 `buildArchitectures = Automatic` 只产出活动架构；
显式传多个 `-arch` 仍只生成 arm64 产物，fat 打包需在 Xcode 里 Archive。

**回退点**：三个提交彼此独立（删 arm32 / vendor / 接线）。
**风险**：xcodeproj 手改易错，建议先用 Xcode 打开让 IDE 重新生成引用。

### S2 · 骨架：两个后端都换成 Compiler（约 4 天）

**S2.0 已完成（2026-11-19）**：`tools/jit_matrix/asmjit_probe.cpp` 用 vendored
asmjit `677a10b` 把骨架假设逐条跑了一遍，**21/21 通过**。它不碰生产代码，单独构建：

```
pwsh -File tools/jit_matrix/build_asmjit.ps1   # 54 个 TU -> asmjit_master.lib，再编探针
build_core/asmjit_probe.exe                    # 21 项断言，exit=0
```

| 断言 | 结果 |
|---|---|
| vendored master 编成静态库（C++20 + `ASMJIT_STATIC`） | 54/54 TU |
| x86 `CodeHolder` + `x86::Compiler` 落地后可执行 | `ret 0`、`mov eax,42`、`f(a,b)=a*3+b` 全部正确 |
| `code.reinit()` 复用同一 `CodeHolder` 产出第二块 | 两块地址不同，分别返回 11 / 22 |
| 未绑定 label 被拦住、不留悬空指针 | `finalize()` → `InvalidState`，`*out == nullptr` |
| a64 `Compiler` 在 x86 宿主上能编出正确编码 | 8 字节，末条 `0xD65F03C0` = `RET X30` |
| `VirtMem::ProtectJitReadWriteScope` 存在且析构 flush | 签名与行为核对 `core/virt_mem.h:379-417` |

**这一轮改掉了 rev4 的三处纸面错误**：`Target` 没有 `_add` 虚函数（那是 `JitRuntime` 的）、
落地流程漏了 `resolve_cross_section_fixups()` 与 `has_unresolved_fixups()`、
函数参数是 `FuncNode::set_arg` 而不是 `Compiler::set_arg`。详见 §2.1 / §2.2。

**没验证到的**（需要 arm64 机器或真实块）：a64 块的实际执行、icache flush 关掉后能否
复现崩溃、573 个 `emit_*` 调用点用 `int` 重载后能否原样编译。这三条留到 S2 收尾。

**x86 侧骨架迁移已完成（2026-11-19）**：`arm_jit.cpp` 已从 asmjit 1.0 切到
vendored master + `CodeHolder` / `x86::Compiler` / 手动落地，生产构建脚本与
差分台都已接线。与迁移前逐条对照：

| 项 | 迁移前 | 迁移后 |
|---|---|---|
| `probes` 288 次 | 0 mismatch | 0 mismatch |
| `--full` 81920 次 R0-R14 | 0 | 0 |
| `CPSR` / `instruct_adr` / 异常 | 0 / 0 / 0 | 0 / 0 / 0 |
| cycle 差异 | 12067 | 12067 |
| jit-not-repeatable | 3 | 3 |

`asmjit_probe` 扩到 24/24（新增 `(u32,u64,int)` 参数槽位用例）。

**迁移中实测出来的五个坑**（照着旧 API 直译必然踩，已写进代码注释）：

1. **`ASMJIT_X64` 在 master 上不存在**。4 处 `#ifdef` 全落到 32 位分支，把
   `u64` 参数拆成两个 `u32`；`OP_LDM_STM` 的 `n` 于是变成 0，Thumb POP/LDM
   一个寄存器都不写，差分台报 140 处 R0-R14 差异。改用项目自己的 `HOST_64`。
2. **void 签名 + `set_ret` 不再产生返回值**。旧代码有 5 处把返回 `void` 的
   `mem_op` / `LDR` / `STR` 也用 `setReturn(bb_cycles)` 取周期；1.0 会读 eax，
   master 不会，这些指令的 JIT 周期因此变成 0。签名改成 `u32` 后与基线逐条一致。
3. **`ret(var)` 拆成 `mov eax, var; ret`**——master 的 `ret()` 不带返回值。
   mid-block 的 DMA 早退由 PEI 统一生成 epilog，不再需要 naked function。
4. `newGpVar(kX86VarTypeGpd/Gpz)` → `new_gp32()/new_gpz()`；`newLabel` →
   `new_label`；`r8Lo()` → `r8_lo()`；`unuse()` 删除（新 RA 自己算活跃期）；
   `kScaleNone` → `0`；`sysint_t` → `int32_t`；`ptr_abs` 基址显式转 `uint64_t`。
5. **25 处 `X86CompilerFuncCall` → `InvokeNode`**：原型从 `setPrototype` 挪进
   `FuncSignature::build<Ret, Args...>()`，参数用 `set_arg`，返回值用 `set_ret`；
   周期返回值先落临时 `gp32` 再 `movzx` 进 `bb_cycles`，不直接写活跃虚拟寄存器。

**arm64 侧骨架与原语层已完成（2026-11-19）**：`arm_jit_arm.cpp` **一行未改**，
改动全部落在 `arm64_emit.cpp`（原语层）与 `emit_core.cpp`（块生命周期）。

| 项 | 改法 |
|---|---|
| 编码器 | 手写 `output_w32(魔数)` → asmjit 指令；**236 个原语调用点全部翻译完，零 `embed` 残留**（除两处见下） |
| `t_bytes` | `typedef asmjit::a64::Compiler`——保留类型名，573 个 `emit_*(g_out, ...)` 调用点零改动 |
| 块生命周期 | 链表式 `t_bytes` 分块 + `MOD_JUMP` 回填 + `memcpy` → `CodeHolder` + `a64::Compiler` + `flatten → resolve_cross_section_fixups → relocate_to_base → copy_flattened_data` |
| 标签 | 全局 `g_LABELS`（id → 字节偏移）→ `std::map<uint64_t, asmjit::Label>` |
| 落地 | 仍是 `armjitarm_alloc_func()` 的 40MB bump 块（加了 16 字节对齐），icache flush 保留 |
| 函数骨架 | `emit_startfunc` / `emit_endfunc` 保持手写 prolog/epilog：PEI 的 `FuncFrame` 为空，不生成任何指令，两者不冲突 |

**这一层怎么验证的（x86 宿主上没有 arm64 机器，后来补了真机，见下）**：比对**生成码**，不比字节比语义。

```
pwsh -File tools/jit_matrix/check_a64.ps1
```

1. `tools/jit_matrix/a64_golden.txt` —— 迁移前用手写编码器跑 236 个原语样例（含每个原语内部分支的边界值）
   冻结下来的指令字，作为金标准；
2. `a64_dump_new.exe` 用 asmjit 跑同一份语料（`a64_corpus.inc`），`check_a64.py` 用 capstone
   反汇编两侧再逐条比对助记符与操作数；
3. `a64_shape_probe.cpp` 单独验证**块中途 `ret`** 这个结构（§3.7 的 DMA 早退形状）：
   早退之后的尾巴没有被 asmjit 的 CFG 优化吃掉。

结果：**236/236 一致**（其中 2 例见下面的"预期差异"）。MSVC 14.52 + C++20 编译该文件也通过
（`cl /c` 产出 654KB obj，无 error），§6.3 那条 `_BitScanReverse` 的 C2664 已随之修掉。

**预期差异（三条，都是语义等价，不是 bug）**

| 现象 | 原因 |
|---|---|
| `b.al`（`0x54..E`）被编成 `b`（`0x14..`） | asmjit 把 AL 条件规范化成无条件分支；`b_al()` 也一样 |
| `emit_mov_cond` / `emit_str_cond` 在 `cond=AL` 时少两条分支 | asmjit 建 CFG，删掉不可达的那一半；原写法里那两条本来就必有一条是死码 |
| 函数末尾多一条 `ret` | `end_func()` 为没有 fallthrough 的函数补出口；实际块都以 `emit_endfunc` 结尾，不会多 |

`check_a64.py` 显式处理这三类（去尾 `ret`、`b.al→b`、白名单两条死分支），其余任何差异都判失败。

**迁移中发现的既有缺陷（按约定不修，只记录）**

1. `emit_movs(rs, rt)` 的常量 `0x2A0003E0` 已经把 Rn 写成 `wzr`，`rs<<5` 于是 OR 进一个已满的字段
   ——这条指令**一直是 `mov w<rt>, w0`，`rs` 从未生效**。当前没有 handler 用它。
2. `emit_umlal` / `emit_umlals` 的注释写 UMADDL，实际常量 `0x9B200000` 是 **SMADDL**
   ——`OP_UMLAL` / `OP_UMLAL_S` 走的是有符号乘加。
3. `emit_ldrsw` 的常量 `0x79C00000` 是 **LDRSH**（32 位半字符号扩展），不是 LDRSW。
   名字错了，行为被 `get_ptrSL/SH` 依赖。

三条都按"迁移不改语义"原样保留，`check_a64.py` 因此能锁住它们；修它们要单独开 PR 并配差分用例。

**asmjit 的一个校验缺口**（`emit_movimm` 的 bitmask 分支）：`orr wD, wzr, #imm` 是
`mov wD, #imm` 的标准编码，但 asmjit 的 validator 拒绝 Rn = WZR，返回 `InvalidPhysId`。
该分支暂时保留一处 `embed()`（`emit_orr_imm_wzr`），注释里写明了原因与上游位置。

**aarch64 真机验证（2026-11-19，Android Termux）**

编码等价只能说明"生成的字节一样"，说明不了"跑得对"。当晚拿到一台 aarch64 设备
（Android 16 / clang 21.1.8 / 8 核，`ssh -p8022 192.168.1.4`），用
`tools/jit_matrix/build_a64_termux.sh` 把整套离线工具原生编了一遍，于是多了两层证据：

| 层次 | 工具 | 结果 |
|---|---|---|
| 编码等价 | `a64_dump_new` + `check_a64.py` | **236/236**（与 x86 宿主上的结论一致） |
| **执行等价** | `a64_exec_probe.cpp` | **25/25** |
| 块中途 `ret` | `a64_shape_probe` | 通过 |

`a64_exec_probe` 用与后端相同的 `emit_*` 原语拼出单块函数，经 `emit_startfunc` /
`emit_endfunc` 包好、`createFunc()` 落地后**直接调用**，比对返回值：ALU、移位、`movimm`
的两种构造分支、`cmp`+`cset`、`emit_mov_cond` 的取支与跳过、`mul`、`str/ldr`、`strb/ldrb`。
它同时验证了落地的四步（flatten / relocate / copy / icache flush）——这是纯字节比对覆盖不到的。

写这个探针时踩到的三个点，也是后端的使用约束：

1. `createFunc()` 里的 `end_func()` **只补 `ret`，不回滚栈**。真实块必须自己发
   `emit_endfunc()`（先 `restore()` 再 `ret`），否则返回时 SP 少 64 字节，
   表现为"跑几条之后在栈地址上 SEGV_ACCERR"。
2. 三寄存器原语（`emit_add/sub/and/or/xor`）的**第三个参数才是目的寄存器**：
   `add w_rt, w_rs1, w_rs2`。`emit_mul(rs1, rs2)` 是例外，写回 `rs1`。
3. `emit_lsl` 把移位量放进 `SHIFT_REG`，也就是 **w0**，因此它会覆盖 w0；
   `emit_lsr/asr/ror` 在 `n < 32` 时走立即数编码，不动 w0。测它们时源寄存器要避开 0。

**尚未开始**：arm64 的寄存器级（`ArmRegCache`，属 S4）与 `emit_*` 的虚拟寄存器重载。

目标只有一个：**两个后端都用 `CodeHolder` + `Compiler` 跑通最小块**。
这一步不改任何 handler 语义，因此可以用差分台立刻验证"骨架对不对"。

#### 2.1 落地路径（不是 Target 子类）

> **2026-11-19 实测修正**（`tools/jit_matrix/asmjit_probe.cpp`，21/21 通过）：
> rev4 这里写的是"继承 `asmjit::Target` 并 override `_add`/`_release`"，
> 在 vendored `677a10b` 上**编译不过**。`Target` 现在只描述目标环境
> （`environment()` / `cpu_features()` / `cpu_hints()`），**没有任何虚函数**；
> `_add`/`_release` 是 `JitRuntime` 的虚函数（`core/jit_runtime.h:102/105`）。
> DeSmuME 不用 `JitAllocator`（自管 scratchpad），所以**不继承任何类**，
> 直接调 `CodeHolder` 的方法落地；x86 与 arm64 共用同一个落地函数。

x86：`StaticCodeGenerator`（`arm_jit.cpp:213`）从 `Context` 改成这套落地路径，继续往 32MB 静态 scratchpad 里写：

```cpp
// 照 core/jit_runtime.cpp 的 JitRuntime::_add() 逐步实现，只把 JitAllocator
// 换成 bump。DeSmuME 的 scratchpad 是整块回收，不需要 _release。
static asmjit::Error jit_commit(asmjit::CodeHolder& code, void** out) {
  *out = nullptr;

  ASMJIT_PROPAGATE(code.flatten());
  ASMJIT_PROPAGATE(code.resolve_cross_section_fixups());   // ← rev4 漏了这一步

  if (code.has_unresolved_fixups())                        // ← rev4 也漏了
    return asmjit::Error::kUnresolvedFixups;

  size_t estimated = code.code_size();
  if (estimated == 0)
    return asmjit::Error::kNoCodeGenerated;

  if (estimated > (size_t)(scratchpad + sizeof(scratchpad) - scratchptr)) {
    arm_jit_reset(1);
    return asmjit::Error::kOutOfMemory;
  }

  // 必须是最终运行地址：PC 相对分支与常量池引用都按这个基址重定位。
  asmjit::CodeHolder::RelocationSummary summary;
  ASMJIT_PROPAGATE(code.relocate_to_base((uint64_t)scratchptr, &summary));

  // 重定位可能缩小代码（某些引用不再需要地址表条目），asmjit 自己也是这么算的。
  size_t final_size = estimated - summary.code_size_reduction;

  ASMJIT_PROPAGATE(code.copy_flattened_data(scratchptr, final_size)); // 这一步才落地
  *out = scratchptr;
  scratchptr += final_size;
  return asmjit::Error::kOk;
}
```

顺序不能变：`flatten()` → `resolve_cross_section_fixups()` → `relocate_to_base()` →
`copy_flattened_data()`，且前三者各自只能调一次。

arm64 侧落地函数完全一样，只多一层：往 40MB RWX 块（`emit_core.cpp:86` 的
`armjitarm_alloc_func`）里 bump 时，拷贝窗口必须可写、拷贝完必须切回可执行并 flush icache：

```cpp
// jit_commit() 的 arm64 版本，只把"拷贝"这一步包起来。
{
  asmjit::VirtMem::ProtectJitReadWriteScope scope(ptr, final_size);
  ASMJIT_PROPAGATE(code.copy_flattened_data(ptr, final_size));
}
// 若改成 CachePolicy::kNeverFlush，就必须自己补 FlushInstructionCache /
// sys_icache_invalidate，否则「小向量全过、一进游戏随机崩」。
```

> 实测签名：`VirtMem::ProtectJitReadWriteScope(void* rx_ptr, size_t size,
> CachePolicy policy = kDefault)`；析构里调 `protect_jit_memory(kReadExecute)`，
> 并且 `_policy != kNeverFlush` 时才 `flush_instruction_cache()`
> （`core/virt_mem.h:379-417`）。与 rev4 的描述一致，可以直接用。

> **icache 三种写法只能选一种，不能都不做**：① scope 覆盖精确区间（析构自动 flush）；
> ② 自己 flush；③ `kNeverFlush` + 确保后续有统一 flush 点。
> `copy_flattened_data()` **本身不 flush**——这是从手写编码器迁到 asmjit 最容易漏的一步，
> 漏了以后"小向量测试全过、一进游戏就随机崩"。

#### 2.2 编译器与函数骨架（两个后端同形）

```cpp
// x86 —— 旧: static X86Compiler c(&codegen); c.clear(); c.newFunc(...); ... c.make()
asmjit::CodeHolder code;
ASMJIT_PROPAGATE(code.init(asmjit::Environment::host()));  // 实测：没有 codegen 对象可借了
asmjit::x86::Compiler cc(&code);

code.reinit();                                          // ≈ 老 c.clear()，在 add_func 之前
asmjit::FuncNode* fn = cc.add_func(asmjit::FuncSignature::build<int32_t>());
// 有参数时：x86::Gp a = cc.new_gp32("a"); fn->set_arg(0, a);
...
cc.end_func();
ASMJIT_PROPAGATE(cc.finalize());
void* f = nullptr;
ASMJIT_PROPAGATE(jit_commit(code, &f));

// arm64 —— 旧: g_out = allocBytes(); emit_startfunc(g_out); ... createFunc(g_out)
asmjit::CodeHolder code;
ASMJIT_PROPAGATE(code.init(asmjit::Environment(asmjit::Arch::kAArch64)));
asmjit::a64::Compiler cc(&code);
asmjit::FuncNode* fn = cc.add_func(asmjit::FuncSignature::build<uint32_t>());
...
cc.end_func();
ASMJIT_PROPAGATE(cc.finalize());
ASMJIT_PROPAGATE(jit_commit(code, &f));
```

> **实测注记（asmjit_probe）**：
> - `code.init(Environment::host())` 可用；`code.reinit()` 之后同一个 `CodeHolder`
>   能再产出一个块，两个块地址不同、各自可执行（11 / 22 两个返回值都验过）。
> - 函数参数**不是** `Compiler::set_arg`——`x86::Compiler` 没有这个成员。要用
>   `FuncNode::set_arg(index, reg)`（`x86_compiler.h` 的示例也是这么写的）。
> - `ptr(base, index, shift)` 的第三参是 **log2 缩放**（0..3），rev4 引用的
>   `kScale2Times` 在 master 上不存在；`a*3` 写 `ptr(a, a, 1)`。
> - unbound label 不需要等到落地：`finalize()` 就以 `InvalidState` 失败，
>   随后 `code_size()==0`，`jit_commit()` 返回 `kNoCodeGenerated`，`*out` 保持 `nullptr`。
>   所以 **`finalize()` 之后必须立刻查错并回退到解释器**，不能拖到 commit。

prolog/epilog 由 PEI 生成，`emit_startfunc` / `emit_endfunc` 消失。
若某块被判定需要保存寄存器，prolog 会多出 `stp/ldp`——必须确认不吃掉"块内不碰 callee-saved"的既有前提（R1）。

#### 2.3 错误处理

老代码只在最后查 `c.getError()`。新 API 每次调用都可能失败：

- 生成函数里用 `ASMJIT_PROPAGATE(...)`；
- 原语层（arm64 有 206 个、签名是 `void`）统一包一层：

  ```cpp
  #define ASMJIT_ASSUME_OK(expr) do { \
      asmjit::Error _e = (expr); \
      if (ASMJIT_UNLIKELY(_e != asmjit::Error::kOk)) { \
        fprintf(stderr, "asmjit: %s @ %s:%d\n", asmjit::stringify_error(_e), __FILE__, __LINE__); \
        ASMJIT_ASSERT(false); \
      } \
    } while (0)
  ```
  > `ASMJIT_ASSERT` 在 release 下是空操作，所以 `fprintf` 那行必须留——否则错误又被吞了。

#### 2.4 arm64 原语层：`Gp` 泛型 + `int` 兼容重载（关键设计）

这一步决定后面能不能"逐条指令"平滑推进。

`a64::Gp` 对物理寄存器和虚拟寄存器是**同一个类型**（`a64::x2` 和 `cc.new_gp32()` 都是 `Gp`）。
所以原语层可以同时提供两种签名，互不干扰：

```cpp
// 新：泛型版（虚拟寄存器也能传）
static inline void emit_add(a64::Compiler* cc, a64::Gp rd, a64::Gp rn, a64::Gp rm) {
  ASMJIT_ASSUME_OK(cc->add(rd, rn, rm));
}
// 过渡：物理寄存器版（573 个调用点一行都不用改）
static inline void emit_add(a64::Compiler* cc, int rd, int rn, int rm) {
  emit_add(cc, a64::x(rd), a64::x(rn), a64::x(rm));
}
```

于是迁移可以**两级推进**，中间态始终可编译、可测试：

| 级别 | handler 写法 | 拿到什么 | 什么时候做 |
|---|---|---|---|
| **结构级** | `emit_add(cc, R2, R3, R2)` —— 物理寄存器 | 统一生命周期/标签/编码器；**消灭手写编码器错误** | S2 一次性，机械替换 |
| **寄存器级** | `emit_add(cc, vd, vn, vm)` —— 虚拟寄存器 | **性能**（基址常驻、ARM 寄存器常驻、脏位回写） | S4 按族推进 |

> 这个设计的价值：S2/S3 结束时你就有一个**功能等价、可交付、可回滚**的 asmjit 版本；
> 即使 S4 因收益不达标而中止，迁移本身已经完成。反过来，如果一开始就同时改寄存器模型，
> 失败时无法区分"编码错"还是"分配错"。

#### 2.5 块级寄存器缓存骨架（S2 建好，S4 逐族启用）

```cpp
// 把 R0..R15 提升为虚拟寄存器：按需加载、写时标脏、块末只回写脏的
struct ArmRegCache {
  a64::Compiler& cc;
  a64::Gp  base;                 // &ARMPROC，块内常驻（替代每次 ldr [&bb_cpu]）
  a64::Gp  r[16];
  uint32_t loaded = 0, dirty = 0;

  a64::Gp read(int n) {          // 读：命中就直接用，否则 ldr 一次
    if (!(loaded & (1u << n))) { r[n] = cc.new_gp32(); cc.ldr(r[n], ptr(base, R_OFF + 4*n)); loaded |= 1u << n; }
    return r[n];
  }
  a64::Gp write(int n) {         // 写：拿可写虚拟寄存器并标脏
    if (!(loaded & (1u << n))) r[n] = cc.new_gp32();
    loaded |= 1u << n; dirty |= 1u << n; return r[n];
  }
  void flush() {                 // 只回写脏的；R15 不参与
    for (int n = 0; n < 15; n++) if (dirty & (1u << n)) cc.str(r[n], ptr(base, R_OFF + 4*n));
    dirty = 0;
  }
  void invalidate_all() { loaded = 0; dirty = 0; }   // 模式切换后必须调
};
```

四条铁律（写进代码注释，别再靠记忆）：

1. **`R15` 永不进缓存**。PC 语义由 `sync_r15` 独占，缓存会让它变成脏值。
2. **任何 C 调用（`invoke`）之前 `flush()`**。被调函数（`op_ldr` / `armcpu_switchMode`）会直接读写
   `armcpu_t` 内存，缓存里的值它看不见。
3. **任何 `ret`（包括 DMA 提前退出）之前 `flush()`**。否则 mid-block 返回时 CPU 内存状态是旧的——
   这正好接上 §3.7 的那条新坑。
4. **写 `CPSR` 模式位之后 `invalidate_all()`**。banked 寄存器组整套换了，缓存全失效。

**验收**（S2 结束）：
- 两个后端都能编译出"只有 `ret 0`"的块并执行返回 0；都能编译 `mov reg, 42; ret` 并返回 42；
- arm64 侧 573 个调用点未改动即编译通过（靠 `int` 重载）；
- 故意造一个错误（绑定未定义 label / 非法立即数），确认被打印而不是崩溃；
- **故意把 icache flush 关掉**，确认测试能复现"执行到旧指令"的崩溃——验证用例真的抓得住这类错误。

**回退点**：S2 结束时功能应与改前完全等价（差分 dump 应逐行相同），可整段 revert。

#### 2.6 macOS arm64 实机验收（已完成，2026-11-20）

S2 的"能在真机上编、能按模拟器的方式跑"两项，在 Apple Silicon（Xcode 27 / Apple clang）上补齐。

**构建**：`DeSmuME (macOS App; AppleSilicon dev+ -- Latest Xcode)`，Release，`BUILD SUCCEEDED`。
失败过两次，都不是环境问题：

| 症状 | 根因 | 修法 |
|---|---|---|
| `no member named 'memory_order_relaxed' in 'std::memory_order'`（12 处） | `types.h` 写成 `std::memory_order::memory_order_relaxed`——用枚举类型限定自己的枚举项，C++17 及以前是扩展，clang 在 C++20 下直接拒绝 | 改为 `std::memory_order_relaxed`（任何标准都合法，MSVC 侧行为不变） |
| `use of undeclared identifier 'sys_icache_invalidate'` | `emit_core.cpp` 的 `__APPLE__` 分支用了它，却没 include `<libkern/OSCacheControl.h>` | 补 include |

> **环境注意**：该 target 把 `MACOSX_DEPLOYMENT_TARGET` 钉在 11.5，Xcode 27 的最低支持是 12.0。
> 这是工程设置不是代码问题，验收时用命令行覆盖 `MACOSX_DEPLOYMENT_TARGET=12.0`，
> **没有改 pbxproj**——发布部署版本是维护者的决定，不该被一次验证顺手改掉。

**差分（这是 arm64 后端第一次在 sim 语义下被逐槽位对照）**：

```bash
tools/jit_matrix/build_diff_macos.sh          # 复用 Xcode 产物打包成 .a，再链差分台
.build/diff-mac/jit_diff --full --all
```

| 指标 | 首次（修复前） | 修复后 |
|---|---|---|
| 比较次数 | 81920（5120 槽 × 8 向量 × 2 标志组） | 同左 |
| 引擎异常（崩溃/访问冲突） | **0** | **0** |
| R0–R14 | **0** | **0** |
| CPSR | **6** | **0** |
| `instruct_adr` | 0 | 0 |
| 周期数 | 14755 | 14755 |
| JIT 自身不可重复 | 3（与 x86 基线同址同数） | 3 |

也就是说：**arm64 后端在 5120 个分派槽位上算出的寄存器结果与解释器完全一致**，
唯一的功能性差异是 6 条 CPSR，归并为 2 个根因。两个都按 §3.5 用 ARM ARM 裁定：

1. **arm64 后端 Thumb `ASR #0`（= `ASR #32`）进位错**。原实现移位 31 次。
   `DecodeImmShift()`（DDI 0597 共享伪代码）明写 `imm5 == '00000'` 时 `shift_n = 32`；
   `ASR_C()`（同手册标准库）为 `(ASR{N}(x, shift), x[Min(shift-1, N-1)])`，
   即 #32 的进位取 **bit31**、#31 取 bit30。两者结果相同（`ASR{}` 把 shift 钳到 `N-1`），
   所以只有 C 错——解释器本来就是对的。改 `emit_asrs(..., 32)` 走"count ≥ 32"分支。
2. **解释器 `NEG INT_MIN` 丢 V**。`(s32)0 - (s32)Rm` 在 `Rm == 0x80000000` 时是有符号溢出（UB），
   clang -O3 据此把 `OverflowFromSUB` 折叠成常量假，**V 整个从编译结果里消失**（源码里那行还在，
   所以肉眼审不出来）。RSB (immediate) T1 是 `AddWithCarry(NOT(R[n]), Zeros(32), '1')`，
   ARM ARM E1.2.2 说 `nzcv[0] == '1'` 当减法发生有符号溢出——`0 - INT_MIN` 溢出，故 V=1。
   arm64 与 x86 两个 JIT 都对，只有 clang 编出来的解释器错；MSVC 不利用这个 UB，所以 x86 基线从未暴露。

**周期数 14755 条不做仲裁**：ARM ARM 不规定指令执行拍数（实现定义），
这一项没有"真实硬件行为"可对照，属于两个后端各自的周期模型（§3.3 的不可比项）。
它是**记录项不是缺陷**，S4 的性能门槛另算（§1.4）。

**这一步的价值**：S2 之前 arm64 侧的所有证据都停在"生成的指令字与旧编码器一致"（236 例）
和"25 个原语能在 AArch64 上跑对"——都没经过模拟器的条件执行、周期记账和块级边界。
现在补上了，而且第一次跑就抓到 2 个既有 bug，其中一个是解释器的、还是只在 clang 下存在的那种。

### S3 · x86 机械替换（约 3–5 天）

按"一类一 commit"，**每类改完立刻编译**。不要一次性全改完再编译——4500 行里会淹没真正的语义错误。

| 顺序 | 替换 | 数量 | 备注 |
|---|---|---|---|
| 1 | `newGpVar(kX86VarTypeGpd)` → `cc.new_gp32()`；`Gpz` → `cc.new_gp_ptr()` | 167 | 用 `new_gpz(cc)` 包装更省事 |
| 2 | `newLabel()` → `new_label()`；`bind()` 不变 | 26 | |
| 3 | `c.call(...)` + `setPrototype(...)` → `cc.invoke(Out(node), (uint64_t)target, FuncSignature::build<...>())` | 25 | 见下方示例 |
| 4 | `setArg(i, v)` → `set_arg(i, v)` | 45 | |
| 5 | `dword_ptr/word_ptr/byte_ptr/qword_ptr(...)` → `x86::ptr(base, off, size)` | 22+ | 可先用兼容层顶着 |
| 6 | `c.x86inst(id, ops...)` → `cc.emit(id, ops...)` | 15 | id 变 `x86::Inst::kIdXxx` |
| 7 | `c.unuse(v)` → **直接删掉** | 10 | 新 RA 自动算活跃期 |
| 8 | 其余多词方法 snake_case | 余量 | `getFunc()`→`func()` 等 |

```cpp
// 旧
X86CompilerFuncCall* ctx = c.call((void*)armcpu_switchMode);
ctx->setPrototype(kX86FuncConvDefault, FuncBuilder2<void, void*, u8>());
ctx->setArg(0, cpu_reg); ctx->setArg(1, mode);

// 新
asmjit::InvokeNode* ctx;
ASMJIT_PROPAGATE(cc.invoke(asmjit::Out(ctx), (uint64_t)&armcpu_switchMode,
                           asmjit::FuncSignature::build<void, void*, uint8_t>()));
ctx->set_arg(0, cpu_reg);
ctx->set_arg(1, mode);
```

> x86 侧的 `emit_branch` **不需要** ARM 条件码映射——它是"查 CPSR 内存 + `jz/jnz`"，不是 x86 条件跳。
> 只有 arm64 侧需要（见 §5.2）。

**验收**：整个 `arm_jit.cpp` 编译零错误；差分 dump 与 S2 结束时逐行相同（只应有权重差异，不应有行为差异）。

### S4 · 按族迁移（主体，约 10–14 天）

**推进方式**：按 §2.2 的族顺序，一族一个 PR；同一族内 x86 与 arm64 **成对改**。
每族里：x86 改宏体/handler（结构 + 正确性），arm64 把该族 handler 从物理寄存器改成虚拟寄存器（性能）。

| 批次 | 族 | handler | x86 侧 | arm64 侧（结构级 → 寄存器级） |
|---|---|---|---|---|
| 1 | 数据处理（`*_IMM_VAL`，不含移位） | **28** | `OP_ARITHMETIC*` / `OP_MOV*` / `OP_TST_` / `OP_TEQ_` / `OP_CMP` / `OP_CMN` 宏体 | `emit_and/or/eor/add/sub/cmp/tst` + 标志宏 → `ArmRegCache` |
| 2 | 移位（4 种 × IMM/REG × 带不带 S） | **224** | `OP_ARITHMETIC*` 的 arg 分支 | 8 个移位辅助 `emit_lsl/lsr/asr/ror/rrx` + `*s` 变体（C 标志） |
| 3 | 访存（LDR/STR 族） | 174 | `OP_LDR_` / `OP_STR_` 宏体 | `get_ptr*` 六件套 → `ptr(base, off)`；写回宏 → `cache.write()` |
| 4 | LDM/STM | 32 | 手写 | `op_ldm_stm*`（基址在列表内时禁止缓存） |
| 5 | 乘法/乘加 | 28 | `OP_MUL_` / `OP_MULxy_` / `OP_SMxxW_` | `emit_mul/mla/smull/...` |
| 6 | 分支 + 状态寄存器 + 异常 | 13 | 手写 | `op_b` / `op_bx` / `OP_MSR_`（模式切换 → `invalidate_all()`） |
| 7 | 协处理器 + 饱和 | 22 | 手写 | `OP_MCR/MRC/op_swp_` / `Q*` |
| 8 | Thumb 全部 | 70 | 手写（多数复用批 1–6 的宏） | 手写 |

**每批的固定动作**：

1. 先改 x86，编译通过；
2. 立刻改同一批的 arm64，编译通过；
3. 跑该批的差分子集（只跑这批 handler 覆盖的槽位），必须全绿；
4. 跑 armwrestler 的对应测试组（`tools/ds_tests/armwrestler`）；
5. 若该批含 store：跑 §3.7 的分类一致性测试；
6. 本批的矩阵状态统一置 `diff-pass`，一个 PR 合入。

**性能门槛（族 1 之后设卡）**

族 1 完成后立刻测一次，不达标就停下来重新评估，不要闷头推到族 8：

| 指标 | 门槛 | 不达标时先查 |
|---|---|---|
| ALU 密集场景（S0 的基准场景）帧率 | **≥ 改前 1.3×** | `base` 是否真的常驻；标志是否每条都存内存 |
| 生成码指令数（族 1 的 handler 平均） | ≤ 改前 50% | `get_ptr*` 是否还有残余、`read()` 是否重复加载 |
| 冷启动 JIT 编译耗时 | ≤ 改前 1.2× | `finalize()` 频率、`CodeHolder` 复用（`reinit`） |

#### 族 1 第一步：`base` 常驻（已完成，提交 `1c9db8a3`）

arm64 侧原先每次访问 CPU 字段都用 movz/movk 重建 `&ARMPROC + offset`（2–4 条），
而 x86 侧一直是一个块装一次（`c.mov(bb_cpu, (uintptr_t)&ARMPROC)`）。现在 `x25` 常驻
`&ARMPROC`：序言装一次，`save()`/`restore()` 负责保存，值加载变成 `ldr w{r}, [x25, #off]`，
地址计算变成 `add x{r}, x25, #off`。

**实测**（族 1 的 28 个 handler，取 8 个样本，单位：字节 / host 指令数，用
`jit_diff --slot N --block N` 量）：

| 块大小 | 改前 | 改后 | 比率 |
|---|---|---|---|
| 1 条 | 264 / 66 | 236 / 59 | 0.89 |
| 12 条 | 1012 / 253 | 720 / 180 | 0.71 |

折算到**每条被模拟指令**：84 → 60 字节（21 → 15 条 host 指令），**降 29%**。

**离 50% 门槛还差什么**：这一步只消掉了地址运算；剩下的是「每条指令仍要把 emulated
寄存器从内存读出来、再写回内存」。那正是 `ArmRegCache` 要消掉的部分，所以下一步是它，
而不是继续在 `get_ptr*` 上抠。

**一个必须知道的隐式契约**（做缓存时会撞上）：`CACHED_PTR()` 展开成常量 `PTR_STORE_REG`（x11），
而且 `VALUE` 模式的访问会**顺带把字段地址留在 x11 里**——有 14 处 handler 在读完寄存器后
直接 `str` 到 x11。也就是说「上一次 VALUE 访问的字段地址」被当成了隐式状态在用。
这一步刻意保留了这个副作用（少一条 `add` 就会静默写错地址），并在代码里写明了；
真正的修法是让那 14 处改走 `emit_str_off(...)`，属于族 3（访存）的活。

#### 族 1 第二步：周期计数器常驻基址（已完成，提交 `a291736c`）

反汇编一条 ADD 之后才看清第二块开销：寄存器访问已经是一条 `ldr`，但周期记账仍在用
movz/movk 反复重建 `bb_cycles` / `bb_total_cycles` 的地址——它们是不动的宿主全局。

两个计数器合成一个对象（`BbCounters`），用第二个常驻基址 `x26` 一次寻址两者
（x26 本来就和 x25 成对入栈，零额外开销），31 处 `emit_read_ptr32_reg` /
`emit_write_ptr32_reg` / `emit_inc_ptr*` 改成 `ldr/str [x26, #field]`；
加立即数时直接用 `add/sub` 的立即数形式，不再先把操作数装进寄存器。

**全量扫描的代码总字节**（5120 槽 × 8 向量 × 2 标志组，`jit_diff --full`）：

| 状态 | 总字节 |
|---|---|
| 族 1 开始前 | 15,888,584 |
| `base` 常驻 | 15,888,584 |
| `base` + 计数器常驻 | **12,252,360（−23%）** |

`base` 常驻在这一列上是 0，不是没效果，而是这个指标由**序言开销主导**：扫描故意用
1 条指令的块，序言里多装一个基址要 3 条、每条访问省下的 2–3 条摊不开。所以两个口径要分开看：
单块/大块的比值看 `--block`，整体代码量看这个总数。

**一个测出来的偏差，别被它骗了**：扫描植入的 ARM 操作码是 `slot << 20`，cond 字段因此**永远不是 AL**
（slot 640 → `0x28000000`，cond = CS）。每条指令因此都要付一次"读 emulated CPSR 的标志位、
按条件跳过"的开销（反汇编里那 4 条 `ldrb/tst/b.eq`）。真实代码里条件指令只占约一成，
所以扫描量出来的"每条 11 条宿主指令"里，有 4 条是扫描自己造的。用 Thumb 槽位量可以避开
（Thumb 无条件码），结论一致：每条约 11 条，其中固定开销约 37 条/块。

#### 族 1 第三步：`ArmRegCache`（语义完成，性能门槛**未过**）

第二步留下的问题是「每条指令仍要把 emulated 寄存器从内存读出来、再写回内存」。
方案 §2.5 原本设想用 asmjit 虚拟寄存器交给 RA 托管，**实测否决**：

- 探针 A：用 `add_unavailable_regs()` 把引脚寄存器钉住后，RA 正常工作（生成 `ldr w14, [x25, #8]`），
  说明机制本身可用，问题不在 asmjit 版本；
- 探针 B：把可用池压到两个，RA 立刻插入自己的 `sub sp, sp, #32` / `add sp, sp, #32`，
  与后端手写的 `ldp x19, x20, [sp], #16` 抢同一个栈指针——PEI 不知道这份手写栈帧；
- 另外 RA 不会在 `callR3()` 发出的裸 `blr` 处切断活跃区间，而 `save_call()`/`restore_call()`
  是手写的，放在 caller-saved 虚拟寄存器里的值会被调用悄悄改掉。

所以缓存自己管三个**物理**寄存器：`x12 / x15 / x21`。选法是把全文件扫一遍，这三个寄存器
只出现在自己的 `#define` 里，不与手写临时寄存器、立即数寄存器或 x23–x26 常驻基址冲突；
三者都是 caller-saved，因此 `save()`/`restore()` 和冻结的编码器 dump
（`tools/jit_matrix/a64_golden.txt`）都不用改。

**五条铁律**（前四条来自 §2.5，第五条是这一轮踩坑后加的）：

1. R15 永不进缓存，PC 归 `sync_r15()` 管；块末指令改写 PC 后缓存里的旧值必然是错的。
2. 调用解释器函数（`emit_armop_call()`）之前 `flush()`。
3. 块中间的 `ret`（`emit_dma_event_check()`）之前 `flush()`。
4. 任何**仍走内存**的访问路径，先 `evict(被访问的 emulated 寄存器)` 再读写内存，
   否则延迟回写会把旧值盖回内存；`CACHED_PTR()` 留在 x11 的隐式地址副作用保持不变。
5. 条件指令的 merge point 上，两条路径必须在**内存**这个唯一共同状态上会合：
   先 `if (any_dirty()) flush(); else invalidate();`，再 `emit_label(skip)`。
   曾经把回写放在 `skip` 之后，于是「条件为假、body 从未执行的寄存器」也被写回，
   `ANDS r0,r0,#0` 在条件为假时把分配器留在 x12 里的垃圾写进了 R0。

同一轮还修掉一个更隐蔽的错误：`need()` 命中时重新 `ldr`，把尚未回写的结果丢掉——
`ANDS r0,r0,#0; EOR r0,r0,#0` 得到 `R0 = R0`。现在只有 miss 才加载。

**差分验证**（缓存版 vs 解释器）：`--full` 81920 次比较，R0–R14 / CPSR / `instruct_adr` /
invariant / 异常全部 0；`--full --seq 2/3/4` 分别 27392 / 26368 / 25344 次比较，0 mismatch。
尾部那 3 条 `jit not repeatable` 在改前的基线上完全相同，不是本轮引入。
全量代码总字节 `12,252,360` → `12,233,928`（−0.15%；1 条指令的块被序言主导，看不出收益）。

**块体积实测**（`jit_diff --block`，族 1 全部 28 个 handler，单位：host 指令 / 被模拟指令）。
表中「初次」是只做 `ArmRegCache` 时的读数，「最终」是下面「第四步：参考 x64 的标志改造」之后的读数：

| 口径 | 改前 | 初次 | 最终 | 比率 | 门槛 |
|---|---|---|---|---|---|
| `n=1` 块总计 | 1379 | 1343 | 1255 | 0.910 | — |
| `n=16` 块总计 | 6524 | 5228 | 3820 | 0.586 | — |
| **边际（`(n=16 − n=1)/15`），28 个 handler 平均** | **12** | 9 | **6** | **0.499** | ≤ 0.50 ✅ |

全量代码总字节最终 `12,117,704`（相对第二步基线 12,252,360 降 1.1%）。
简单的 `AND/ORR/EOR/MOV/BIC` 早已达标：`n=16` 从 117 降到 71，边际 5 → 2（**40%**）。
反汇编显示 4 条指令只付一次 `ldr w12, [x25, #16]` 和一次 `str`，跨指令复用确实生效。

#### 族 1 第四步：参考 x64 的标志位改造（已完成，代码尺寸门槛通过）

回看 x86 侧这次迁移做了什么，才知道第三步剩下的瓶颈是**历史设计**而非 asmjit 迁移引入：
x86 的 `GET_CARRY` 是 `bt flags_ptr, 5`（+ `cmc`），`SET_NZC` 用 `setcc` + `lea`
从 host 标志直接拼字节；迁移前后 arm64 侧的对应宏是逐条等价的，只是把机器码常量
换成了 asmjit API，一条没少。所以按 x64 的做法重做 arm64 标志路径：

1. **`GET_CARRY` 去分支**。原来 `ands` + `b.ne` + `mrs`/`bic|orr`/`msr` + `b`（6–10 条，
   每次带一个分支）改为 `ldrb` + `and #0x20` + `cmp #0x20`（3 条、无分支）。
   `cmp` 正好在 bit5 置位时设 host C，极性对 `adcs` 和 `sbcs` 都对。
   注意 arm64 版的 `invert` 参数本来就是 no-op：AArch64 的 `sbc` 和 ARM 一样把 C 当
   "no borrow"，与 x86 的 `sbb` 相反，所以不需要 `cmc` 的对应物。
2. **`SET_NZCV/SET_NZC/SET_NZ/SET_N` 用 `bfxil` 合并位域**。原来 `and 高位` +
   `and 低位` + `orr` 三条压成一条 `bfxil`，宽度分别 4/5/6/7 位。
   `SET_Z` 因为要保留 bit7 只换 bit6（非连续位），保持原样。
3. **标志字节读写走直接基址偏移**。原来 `flags_ptr(..., VALUE)` 会顺带发一条
   `add x11, x25, #83` 供 `CACHED_PTR()` 用；这些宏只读写、不复用地址，改成
   `VALUE_NO_CACHE`（`cpu_get_ptrB` 里直接用 `emit_ldrb_off`）+ `emit_strb_off`，
   每条 S 指令再省 1–2 条。`SET_Q` 仍依赖 x11 契约，保持 `VALUE` 不动。

最终：`--full` 81920 次比较全部功能项 0；`--full --seq 2/3/4` 全绿。
边际指令数 9 → **6**，28 个 handler 平均 **0.499 ≤ 0.50，代码尺寸门槛通过**。

**仍未验证的门槛**：ALU 密集场景帧率 ≥1.3×、冷启动编译耗时 ≤1.2×、x86 帧率 ±5%。
这些需要整机跑 ROM，差分台测不了。代码尺寸达标不等于帧率达标，族 2 仍需等实测。

**两个工具修正**（否则上面的数字不可信）：

- `jit_diff --block` 原来在第二次 `execJit()` 之后才读 `g_TOTALBYTES`，而那次调用会编译
  「块尾之后的下一个块」，打印的是别人的体积（所有槽位都是同一串数）。现在在第二次调用
  之前同时抓 `g_TOTALBYTES` 和 `JIT_COMPILED_FUNC(kCodeBase, 0)`。
- `build_diff_macos.sh` 原来用 `find | head -1` 挑对象目录，会在两个 AppleSilicon target 之间
  随机切换；现在用 `TARGET_NAME`（默认 dev+，与脚本头的 xcodebuild 前提一致）显式指定。

> 如果 ALU 族只能拿到 1.0~1.1×，说明瓶颈不在寄存器分配（可能在 MMU 或调度），
> 这时应该停下来重新定目标，而不是继续把 571 条都改完。

#### 族 2：移位（224 个 ARM handler，已完成语义迁移）

**纪律说明**：按 §S4 原计划，族 2 应等族 1 的整机帧率数字出来再推。本次是用户明确要求推进主体迁移，
故照做；**帧率门槛仍未验证**，这一点不因语义迁移完成而改变。

**做法**：移位 `arg` 宏（`LSL_IMM`/`LSL_REG`/`S_LSL_IMM`/…）把移位后的操作数留在物理 `rhs`(w6)，
所以缓存池只需再提供 Rn/Rd 两个槽——3 槽够用，不必扩容。新增三个宏：

- `OP_ARITHMETIC_SHIFT_RC` —— 正向 ALU（AND/EOR/ORR/ADD/SUB + 算术 S 的 ADDS/SUBS + 带进位的 ADC/SBC）
- `OP_ARITHMETIC_S_SHIFT_RC` —— 逻辑 S（ANDS/EORS/ORRS/BICS），保留 `SET_NZC`
- `OP_ARITHMETIC_R_SHIFT_RC` —— 反向（RSB/RSC），`Rd = rhs - Rn`

TST/TEQ/CMP/CMN/MOV/MOV_S 这类单源/比较指令则直接把原有的 `if(rhs_is_imm)` 缓存门控
改成 `if(REG_POS != 15)`——族 1 时缓存分支只服务 IMM_VAL，移位路径走内存；现在两条路径共用缓存。
**R15 始终走内存路径**（铁律 1）。

**覆盖范围**：224 个 ARM shift handler 中 223 个走缓存；唯一例外是 `OP_MOV_LSL_IMM` 的 NOP 特例
（`i == 0xE1A00000` 直接返回，无需缓存）。10 个 Thumb shift 留待批次 8。

**实测**：

| 口径 | 改前 | 改后 | 降幅 |
|---|---|---|---|
| 16 指令块（8 个槽位样本） | 852–1044 字节 | 788–980 字节 | 约 −64 字节/块（−7.5%） |
| 全量代码总字节 | 12,117,704 | 12,101,320 | −16,384（−0.14%） |

收益小于族 1 是设计使然：本批只让 Rn/Rd 享受跨指令复用，Rm/Rs 当时仍走内存（`arg`
宏未改）。**这一项已在 §10.10 补齐**（`read_into()` 不占槽，所以 3 槽仍够用）。

**验证**：每一批（无 S IMM → 无 S REG → 算术 S → 逻辑 S → ADC/SBC/RSC → 比较/移动类）
都重建并跑 `--full`，功能桶（R0–R14 / CPSR / `instruct_adr` / invariant / engine fault）全 0；
最终 `--full --seq 2/3/4` 全绿（0 mismatch）。周期差异仍是记录项，不进仲裁。

#### 族 5：乘法（28 个 ARM handler + CLZ，已完成语义迁移）

**纪律说明**：族 2 收尾时约定了"族 1/2 的整机帧率门槛未验证就不要再闷头推"，本次继续是因为用户明确要求推进主线；**帧率门槛仍未验证**，这一点不因语义迁移完成而改变。

**为什么不能照抄族 1/2 的写法**：乘法族的操作数必须落在**固定 scratch 寄存器**里——`MUL_Mxx_END` 算周期时会原地改写它的入参（`asr`/`xor` 求有效位宽），所以被乘数不能是一段缓存槽的生命周期。而如果退回 `reg_pos_ptr`，族 1/2 攒下来的"值在槽里"就会被换成一次 store+load。

为此给 `ArmRegCache` 加第五个入口：

```cpp
void read_into(int n, int host)   // 槽里有最新值 → mov；否则 ldr
```

它**不占槽位**（不标 `touched`、不改 `dirty`），也不像 `read()` 那样把待回写值刷出去。于是三个宏改成源 `rc_read(...)`、目的 `rc_store(...)`：

| 形状 | 同时驻留的 emulated 寄存器 | 槽位占用 |
|---|---|---|
| `MUL` / `SMULxy` / `SMULWy` | Rm, Rs, Rd | **3**（正好用满） |
| `MLA` / `SMLAxy` / `SMLAWy` | Rm, Rs, Rn(累加), Rd | 源不占槽，只有 Rd 1 槽 |
| `UMULL` / `SMULL` / `SMLAL`（含 S 变体） | 源不占槽，RdLo+RdHi 2 槽 | 2 |

`rc_store` 的意义是目的寄存器不再立刻落内存：下一条 ALU/移位指令直接从槽里读，下一条**未迁移**的 handler 走 `reg_pos_ptr` 时会先 evict（一次 store），与改动前的成本相同。`OP_MUL_` / `OP_MULxy_` 里那两组 `if(flags)/else` 分支体原本完全一样，一并合并成 `if(width)`。

**真机验证：`tools/jit_matrix/a64_block_probe.cpp`（本轮新增）**

此前的死结是"arm64 handler 语义只能等 macOS 整核"。这个探针换了条路：直接 `#include arm_jit_arm.cpp`，再用 `nm -u` 报出来的 **68 个符号**把核心补齐（4MB 主存 + 主存镜像的假 MMU、空解释器表、`armcpu_switchMode` 之类的空实现），于是 `compile_basicblock()` 真的会跑，生成的块也真的在 AArch64 上执行。

- 解释器表指向"什么都不做、返回 1 cycle"的桩：`compile_basicblock()` 会一边发射一边用解释器走一遍（周期就是这么累计的），真表会把指令先执行掉一次。用桩之后只有流水线寄存器在动，探针在调块之前把状态复原，块从用例的初值开始跑。
- 覆盖 35 个单指令块（MUL/MLA/UMULL/SMULL/SMLAL/UMULLS/SMLALS/SMULxy/SMLAxy/SMLALxy/SMULW/SMLAW/CLZ × 三组边界操作数，含 `C+V` 预置的 `MULS`）、2 条 store（含 `mmu_dma_event_pending` 置位的提前退出路径）、1 个两指令块。
- **迁移前后 emulated 状态逐字节相同**（R0–R14 / CPSR / cycles / 内存 / resume 地址），23 项自检全绿。

**顺带抓到一个既有 bug：`emit_dma_event_check` 会丢延迟回写**

`emit_dma_event_check()` 的 armed 分支是一次块中途返回，原来在里面调 `g_regs.flush()`。`flush()` 除了发 store，还会清掉**编译期**的 `dirty` 位——而这段代码长在一个分支的一侧：

```
ldr flag; tst; b.eq skip
   <armed: 写回 + 提前返回>
skip:
```

于是落空路径上那些"待回写"的值就再也没人写了：块尾的 `flush()` 认为没有脏数据，一条 store 都不发。复现是两指令块 `mul r0,r1,r2` + `str r3,[r4]`（store 刻意不碰 R0，否则 store handler 自己的 evict 会掩盖问题）：

| 版本 | R0 | 内存 |
|---|---|---|
| 迁移 + 旧 `flush()` | **00000000**（丢） | CAFEF00D |
| 迁移 + `write_back()` | FFFFFFFE ✅ | CAFEF00D |
| 迁移前（立即 store） | FFFFFFFE | CAFEF00D |

修法是只发写回、不清 dirty：

```cpp
for (int i = 0; i < ARM_RC_SLOTS; i++)
    g_regs.write_back(i);
```

两条路径都成立：armed 路径拿到完整 `armcpu_t` 再返回，落空路径仍由块尾的 `flush()` 负责。这个 bug 不是本次引入的——族 1/2 之后就一直存在，只是此前没有任何"延迟回写 + store 指令"同块出现的用例（store handler 读数据寄存器时正好会 evict 同一个寄存器，把问题遮住了）。

#### 族 6：分支 + 状态寄存器 + 异常（13 handler）

覆盖 `MRS`×2、`MSR`×4（`_c`/`_f` × CPSR/SPSR，含 `IMM_VAL` 形式）、`B`/`BL`/`BX`/`BLX_REG`、`SWI`、`BKPT`。`UND` 一直是 `#define OP_UND NULL`，不在本批。

**这一批的真正收获不是缓存，而是一条不变量**

`armcpu_switchMode()` 不是"改一个字段"：它先把**当前** bank 的 R13/R14/SPSR 读出 `armcpu_t` 写进对应 bank 字段（FIQ 还要 swap R8–R12），再把新 bank 的值读回 `R[]`。也就是说它是**靠内存里的 R13/R14 完成搬移**的。而寄存器缓存把"最近一次写"留在宿主寄存器里——于是：

- switchMode 读到的 R13/R14 是**指令执行前**的旧值 → 存进了旧 bank；
- 块尾 `flush()` 把新值写回 `R[13]`，此时已经在新 bank → **旧 bank 的值被写进新 bank**。

两个方向同时错，而且都不是"少存一次"这种精度问题。所以本批确立的规则是：

> **任何 `armcpu_switchMode()` 调用之前必须 `g_regs.flush()`。**

审计全部 6 处调用点：`S_DST_R15`（异常返回）本来就有 flush（族 4 时加的），其余 4 处补齐——`OP_MSR_` 的控制字节分支与 `byte_mask` 分支、`op_ldm_stm2` 的临时 SYS 切换（进出各一次）、`op_swi`。`OP_BKPT` 是本批新内联的，一开始就带着。

**缓存化本身**

| handler | 改动 | 收益 |
|---|---|---|
| `MRS CPSR/SPSR` | 结果 `rc_store` 而非立即写内存 | `mrs r0,cpsr; tst r0,#0x80` 这类中断守卫不再 store+load |
| `MSR CPSR/SPSR` | 源用 `REG_OFF_CACHED`（`rc_read`），`REG_OFF` 本身不动（LDR/STR 族还在用它） | 前一条指令写的 Rs 直接从槽里读 |
| `BX` / `BLX_REG` | 源 `rc_read` | `mov r0,r1; bx r0` 不再落内存 |
| `B` / `BL` | 不改 | 唯一写的寄存器是 LR，且块在这里结束，最终值本来就必须进 `armcpu_t`；走缓存只会多一条 `mov` |

`BX` 切到 Thumb 时**不需要** flush：`CPSR.T` 不在 bank 里，寄存器文件不动。

**`OP_BKPT`：从"打印 + 回退"改成内联**

原来是 `printf("JIT: unimplemented OP_BKPT"); return 0;` ——也就是每次编译到 BKPT 就往 stdout 打一行，然后退回解释器。现在按 `arm_instructions.cpp:OP_BKPT` 逐行内联：进 ABT 模式、`R14_abt = 指令地址+4`、`SPSR = 旧 CPSR`、清 T、置 I、跳到 `intVector+0x0C`。它在属性表里是 `JIT_BYPASS`，块在这里结束，`next_instruction` 由块尾拷进 `instruct_adr`。

**真机验证：探针补上"真的会搬 bank 的 switchMode"**

上一轮探针里的 `armcpu_switchMode` 桩只写了 `CPSR.bits.mode`，**根本测不出上面那类 bug**——它不读不写 `R[]`，缓存脏不脏都一样。本轮把它换成 `armcpu.cpp` 的逐行副本（只省掉 `armcpu_changeCPSR()`，那个要整核），并给每个 bank 填可区分的哨兵值，于是"值进了哪个 bank"直接可读。

新增 15 个用例（MRS/MSR 六种、B/BL/BX/BLX、SWI、BKPT，以及三个"先 `add sp,#4` 再切换" 的两指令块）：

| 用例 | 迁移前 | 迁移后 |
|---|---|---|
| `add sp,#4 ; msr cpsr_c,#0x12` | `R13_svc=023FF000`（旧值）**且** `R13=023FF004`（SVC 的 SP 被写进 IRQ bank） | `R13_svc=023FF004`，`R13=55555000`（IRQ bank 原值）✅ |
| `add sp,#4 ; swi #0` | `R13_svc=023FF000` | `R13_svc=023FF004` ✅ |
| `bkpt #0` | 走解释器桩（状态不变），并打印一行 | 进 ABT：`cpsr=00000097`、`R14=指令+4`、`instruct_adr=0x0C` ✅ |

按用例逐条比对 53 个块：**49 个逐字节相同**，只有上表 4 个（2 个 bank 串值 + 2 个 BKPT）变化。族 5 的 35 个乘法块、2 条 store、`mul`+`str` 两指令块都在"相同"里，没有被这批改动碰到。

**两点必须说清楚的边界**

1. `cpu->swi_tab` 在 `NDSSystem.cpp` 里是被赋成 `ARM_swi_tab[...]` 的，所以 `op_swi` 的内联路径**在真机上根本不会走**（`if(cpu->swi_tab) return 0;` → 回退解释器）。那处 flush 是正确性保险，不是修了一个线上 bug。
2. `op_ldm_stm2` 的两处 flush 属于族 4（LDM/STM，矩阵里仍是 `todo`），本批只是把不变量补上；它的独立验证要等到族 C 的探针用例。

#### 族 3 + 4：访存 + LDM/STM（172 + 32 handler）

**为什么这两个族一起做**：它们共用同一条不变量——**helper 直接读写 `armcpu_t`**。`OP_LDR`/`OP_STR` 拿的是 `u32* dstreg`（`&cpu->R[Rd]`），`OP_LDM_STM` 拿的是一串寄存器号，`op_swp` 拿的也是 `u32* Rd`。换句话说，寄存器缓存里的"最新值"对这些 helper 是不可见的，凡是被它们碰到的寄存器都必须先落盘。

**改动**

| 位置 | 改动 | 说明 |
|---|---|---|
| `OP_LDR_` 基址 | `reg_pos_ptr(16,…)` → `rc_read` | `add r1,#4 ; ldr r0,[r1]` 不再 store+load |
| `OP_LDR_` 目的 | 保持 `reg_pos_ptr(12, dst, ADDRESS)` | ABI 要求 `&R[Rd]`，这个寄存器没法留在槽里；evict 保证脏值先落盘 |
| `OP_LDR_` 写回 | `LDR_WB`：Rn≠Rd 走 `rc_store`，Rn==Rd 立即存 | Rn==Rd 时 helper 会覆写 `R[Rd]`，延迟回写会盖掉载入值 |
| `OP_STR_` 值/基址 | 均 `rc_read` | `add r0,#1 ; str r0,[r1]` 的值直接来自槽 |
| `OP_STR_` 写回 | `rc_store` | store 不写 `R[]`，任何情况下都可以延迟 |
| `LDRD`/`STRD` | 基址 `rc_read`；目的对 `evict`/`discard` | helper 按 `Rnum` 索引直接访问 `cpu->R[]` |
| `SWP` | Rs 改 `rc_read` | SWPB 的 helper 自己截低字节，槽里的 dword 等价 |
| `op_ldm_stm` | 基址 `rc_read`；**列表内每个寄存器** `evict`（store）/`discard`（load） | 见下面两个 bug |

`ArmRegCache` 为此加了 `discard(n)`：丢掉槽位但不回写。载入型 helper 马上会覆盖这个寄存器，把待回写的值写进内存纯属浪费。store 型必须用 `evict`—— helper 要读的就是那个值。

**抓到的既有 bug 1：STM 存的是旧值**

`OP_LDM_STM` 通过寄存器号列表直接读 `cpu->R[Rd]`，而 `op_ldm_stm` 原来只 evict 了**基址**。于是族 1 之后，任何"刚算出来的值 + 立即 STM"都存旧值：

| 块 | 迁移前内存 | 迁移后 |
|---|---|---|
| `add r4,#1 ; stmia r0,{r4}` | `40000000`（old r4） | `40000001` ✅ |
| `add r4,#1 ; stmdb r0!,{r4}` | `40000000` | `40000001` ✅ |

`push {r4}` 紧跟一条改 r4 的指令就会踩到，这是**线上会出错**的一类，不是理论问题。

**抓到的既有 bug 2：基址在列表里的 LDM 写回是野值**

`op_ldm_stm` 里"基址也在列表且列表里有更高寄存器"的写回，原来在 helper 调用**之后**用 x4 算新基址。而 `call_mem_r7` 只保存 x12/x15，x4 早被 helper 用掉了：

```
ldmia r0!,{r0,r1}   x4 被毁 → R0 = BEB3DF20（每次运行都不一样）
```

改成**调用前**算好、放进缓存槽（`rc_store`），尾部 flush 落盘——顺序也正好对上解释器（它最后才赋 `R[Rn]`）。修好后 `R0 = 02003008`（= 原基址 + 2*4）。

**探针：37 个访存块**

新增 `--dump-mem N`。覆盖：LDR/STR 的立即数（正负）、PC 相对、寄存器偏移、前/后索引写回；LDRB/LDRH/LDRSB/LDRSH/STRB/STRH；LDRD/STRD（含高字）；SWP/SWPB；LDMIA/STMIA/STMDB/LDMIA!/LDMDB/STMIB/LDMDA/STMDA；以及 6 个两指令块（脏基址 + LDR/STR/LDM、脏数据 + STR、脏源 + STM×2）。

结果：90 个块里 **83 个逐字节相同**，7 个变化——4 个是族 6 的（已记录），3 个是上面两个 bug + 基址在列表的写回。71 项自检全绿。

**三条边界**

1. **LDR 的目的寄存器不可能留在缓存里**：helper 的 ABI 是 `u32* dstreg`。要改这一点得先改 ABI（让 helper 把值返回在寄存器里），不在本批范围。
2. 带**移位寄存器偏移**的 LDR/STR（`ldr r0,[r1,r2,lsl #2]`）仍然用 `reg_pos_ptr` 读 Rm——那些 `arg` 宏与族 1/2 共用，本批没动。所以这一族是"基址缓存化"，不是"全寄存器缓存化"。
3. **DA 方向的两条（`ldmda`/`stmda`）只做了 pin（pre == post），没有断言**：它们的有效地址跟 ARM ARM 的写法对不上，要判定得拿解释器当 oracle，而这个探针链不到解释器。已在 `jit-open-tasks.md` 里列为待 macOS 核对项。

#### 族 7 + 8：Thumb 全表 + 饱和 + 协处理器（68 + 4 + 2 handler）

这是迁移主线的收尾批，做完后矩阵 **571/571 全部 `diff-pass`**，剩下 20 个 `interp-fallback`（`OP_UND`/`OP_CDP`/LDC-STC 全系/`OP_LDREX`/`OP_STREX`），它们本来就是有意不编译的。

**缓存入口怎么扩到 Thumb**

Thumb 的操作数在 3 位字段里（bit 0/3/6/8），所以 `reg_pos_thumb(x, …)` 的替换需要自己的拼写：

| 宏 | 等价于 |
|---|---|
| `rc_t_read(bit, host)` | `reg_pos_thumb(bit, host, VALUE)`，`read_into` 不占槽 |
| `rc_t_rw(bit)` | 单寄存器读改写（移位、`adds #imm8`、`neg` 等）用 |
| `rc_t_write(bit)` | 只写不读的目的寄存器 |
| `reg_pos_thumb_dst(x, reg)` | LDR 的 `u32* dstreg`：`discard` 而不是 `evict` |

"特殊"形式（`add/cmp/mov/bx/blx` 的高寄存器组）把第四位放在字段外（H1=bit 7，H2=bit 6），这几个 handler 用 `rc_read_at(REG_POS(i,3), …)` 按 4 位字段取寄存器号。**这是本批最容易写错的地方**：`_T3(3)` 只取 3 位，会把 `bx r12` 读成 `bx r4`。

**Q 系列：从"不可编译"到内联**

`OP_QADD`/`QSUB`/`QDADD`/`QDSUB` 原来只有 `printf` + `return 0`，意味着**任何含饱和指令的块整体退回解释器**。现在内联实现，两条语义必须写清楚：

1. **操作数顺序按 ARM ARM；解释器本来就对，是这份文档此前记错了**。这一族的语法是 `op{Rd}, Rm, Rn`：`Rm` 在 bits 3-0（第一个源），`Rn` 在 bits 19-16（第二个），和数据处理的 `Rd, Rn, Rm` 印出来的顺序相反——所以 `QDADD r0, r1, r2` 双倍的是 **R2**，不是 R1。按 DDI 0403E 的 Operation 伪代码：`QADD` = `SAT(Rm + Rn)`、`QSUB` = `SAT(Rm - Rn)`、`QDADD` = `SAT(Rm + SAT(2*Rn))`、`QDSUB` = `SAT(Rm - SAT(2*Rn))`——逐条对上 `arm_instructions.cpp` 的 `R[bits0] ± sat(R[bits16]<<1)`，四个都对。此前写成"解释器与 ARM ARM 相反、JIT 照抄解释器"，是把字段名记反了（把 bits 3-0 当成了 `Rn`）；解释器的代码一行没改，`arm_jit_arm.cpp` 与本段已改正。
2. **饱和是 `Rd = 0x80000000 - BIT31(res)`，Q 是粘性的**。落地成三对 XOR/AND 而不是分支：`dst = res ^ ((res ^ sat) & -ovf)`，`ovf` 由 `adds`/`subs` 的 V 经 `cset` 取出，Q 用 `OR` 写回 CPSR 第 3 字节（bit 27）。

写这段时踩了一个自造的坑：先把公式写成 `0x7FFFFFFF + BIT31(res)`，正好是正确值的反相，四个饱和用例全部失败（`0x80000000` vs `0x7FFFFFFF`）。探针的自检当场抓住了它——这也说明**饱和这类"值错了但状态看着正常"的指令必须带断言**，不能只靠 pre/post 相同。

**x86 侧补齐（同一份语义，另一套原语）**

`arm_jit.cpp` 里这四个 handler 之前也是 `printf("JIT: unimplemented …") + return 0`，所以在 x86 上含饱和指令的块同样整块退回解释器。现在按同一份伪代码实现，只是把 arm64 的 `cset`/`cmov` 换成 x86 的 `seto` + `cmovnz`：

- 饱和值仍是 `0x80000000 - BIT31(res)`：`shr 31` → `xor 1` → `add 0x7FFFFFFF`，再按 OF 用 `cmovnz` 选回；
- `QDADD`/`QDSUB` 分两段饱和，两段的 OF 分别取、累加成 0/1 的 `anyq`，最后 `shl 3` 一次性 OR 进 CPSR 第 3 字节（bit 27）；N/Z/C/V 一个都不碰，与解释器一致；
- 一个必须记住的细节：`seto` 只写 8 位，而且**任何插在中间的其他指令都会清 OF**。所以它必须紧贴 `add`/`sub`，再用 `and 0xFF` 把上位垃圾清掉、得到干净的 0/1（第一版先 `xor` 再 `seto`，读到的是被 `xor` 清掉的 OF）。

**验证（本机 x86 差分台，oracle 是解释器）**：`jit_diff` 的 `kProbes` 加了 6 条（四条基础形式 + `Rd==Rn` / `Rd==Rm` 两种别名，源寄存器 R1/R2 刻意分开），44 条探针 × 8 组操作数 × 2 种标志 = **704 次比较 0 mismatch**；`--full` 81920 次比较，`R0-R14` / `CPSR` / `instruct_adr` / 异常 / harness 不变量**全 0**，只剩 12067 条周期差（实现定义，不仲裁）和既有的 3 条 `jit not repeatable`。负对照：把 `cmovnz` 改成 `cmovz` 重编，Q 探针立刻全线报错（`QADD r0,r2,r1 vec0: interp=7FFFFFFF jit=80000000`），证明这 6 条探针确实在跑编译后的代码，而不是悄悄回退。

**顺带修掉一个链接缺陷**：`jit_diff_main.cpp` 里的 `extern int g_TOTALBYTES` / `g_all_block_bytes` 只有 arm64 的 `emit_core.cpp` 定义，x86 侧没有定义——所以**用新建的 `desmume_core.lib` 根本链接不出 `jit_diff.exe`**（一直在用 AArch64 核心拆分之前留下的旧二进制，这也是为什么这个缺口一直没被发现）。现在由 `arm_jit.cpp` 自己维护这两个计数，并在 `jit_commit()` 里记录真实块大小，`--dump` 与代码总量统计在 x86 上终于有了意义。

**MCR / MRC**

这两个 handler 早就实现了，只是没进账本。本批只把数据寄存器接进缓存（`rc_read` / `rc_store`）。未识别的 `CRn` 仍是 `return 1` 空操作，与解释器一致。

**顺便修掉一处 Thumb 乘法周期**

`OP_MUL_REG` 原来把**乘积**喂给 `MUL_Mxx_END()`（它用 CLZ 估算操作数宽度来算周期），而 ARM 侧一律喂乘数 `Rs`。改成喂 `Rs` 后与 ARM 路径一致：

| 用例 | 迁移前 | 迁移后 |
|---|---|---|
| `muls r0,r2`（Rd=0x11111111, Rs=2） | cyc=5 | cyc=2 |

寄存器状态逐字节相同，只有周期变了。解释器的 Thumb 公式是 `MUL_Mxx_END_THUMB`（按 `v = R[bits 3..5]` 分档），也不看乘积——所以这是**修正**，不是回归。

**探针：78 个 Thumb 块 + 7 个新 ARM 用例**

新增 `--dump-t N`。覆盖 Thumb 全部 68 个 handler（移位 10、数据处理 26、访存 17、块传输 6、乘法 1、分支 7、`svc` 1），外加 5 个两指令块（`movs`+`str`、`adds`+`str`、`adds`+`push`、`sub sp`+`push`、`adds r1`+`ldr`）。ARM 侧补了 4 个 Q 指令 + `mrc p15` / `mcr p15`。

结果：**170 个块里 164 个逐字节相同，6 个变化**——5 个是 Q 系列（迁移前块里是解释器桩调用，`R0` 保持初值、Q 不清零），1 个是上面 `muls` 的周期。126 项自检全绿。另外，Q 系列不再只靠"迁移前后相同"判定：探针内置了上面那段伪代码的参考模型，4 条指令 × 64 组操作数 × 3 种目的寄存器（Rd 与两个源分别别名）共 **768 个单指令块**，逐个比对 `R[Rd]` 与 `CPSR.Q`，**768/768 通过**（总计 894 项自检全绿）。

**探针自己踩的坑（记下来免得下次再花时间）**

`_MMU_MAIN_MEM_MASK32` 的定义是 `0x3FFFFF & ~3`——它会把低两位清掉。于是 `(adr) & MASK32` 和 `(adr+2) & MASK32` 落到了同一个字节，两指令 Thumb 块的第一条指令被第二条覆盖，表现为"块里只有第二条指令"。修法是先算 `base = adr & MASK32` 再 `+2*k`。

**三条边界**

1. Thumb 的 `bl` / `blx` 是**两半**编码（`0xF000` 前后缀 + `0xF800`/`0xE800`），所以探针用两指令块覆盖；单独一条 `OP_BL_11` 在真机上不会出现。
2. `OP_BLX_THUMB` 的 `test_thumb` 参数仍然按 3 位字段判断 `REG_POS(i,3)==15`——这与解释器一致，但没有独立的真机用例（`blx r15` 属于未定义行为）。
3. 族 3+4 留下的 `ldmda`/`stmda` 待核对项仍然挂着，本批没有改变它的状态。

### S5 · 语义收尾（约 3 天）



先过 §3.6 的逐族契约清单与 §3.7 的块级边界，再过下面 7 项 x86 风险（R1–R7）
与 §6.3 的 arm64 风险。

**R1 · naked 函数没有直接对应物**（x86）
老代码 `kFuncHintNaked` + `c.ret()` 表示"不要 prolog/epilog"。新版 PEI **无条件插入** prolog/epilog，
但 `emit_prolog` 在该帧没有保存寄存器、没有栈参数时**不生成任何指令**，`emit_epilog` 只有一条 `ret`。
→ 对 DeSmuME 的块（无栈帧、只返回 `bb_total_cycles`）语义等价，**但必须用反汇编验证**。
若某块被 RA 判定需要保存寄存器，prolog 会多出 `push/pop`，必须确认不吃掉 ABI 前提。
兜底：该块改用 `x86::Assembler` + 手写帧，或把 `FuncNode::frame()` 的属性显式清零。
（arm64 侧同理会多出 `stp/ldp`，判据相同。）

**R2 · `ret` 的语义变了**
新版 `cc.ret(x)` 生成 `FuncRetNode`：把 x 放进返回值寄存器并跳到函数 exit 节点。
- 块内多处 `ret`（`emit_dma_event_check` 一处，`compile_basicblock` 末尾一处）在新版是**汇合到同一个 exit**；
- **重点**：`emit_dma_event_check` 的 ret 在块中间，且 ret 之前改了 `instruct_adr` / `next_instruction`。
  汇合后必须确认 exit 里的恢复动作不会覆盖这些写入，且 `bb_total_cycles += bb_constant_cycles`
  仍然只在"已发射指令"上生效（§3.7 的契约 2/3）；
- arm64 侧引入寄存器缓存后，**这个 ret 之前必须先 `flush()`**——否则 CPU 内存状态是旧的。

**R3 · `ptr_abs` 在 x64 上只有 32 位**
这条已在 `jit-bug.md` §8 踩过：`dword_ptr_abs(&global, imm)` 在 x64 下地址被截断，运行时 `0xc0000005`。
新 API 的 `x86::ptr_abs()` 仍是同一编码路径。**6 处 `ptr_abs` 全部改成"寄存器持地址 + `[reg+disp]`"**。

**R4 · 寄存器分配结果不同**（两端）
RA 是全新实现，同样的虚拟寄存器图会分配出不同的物理寄存器，机器码**必然不同**。
→ 不要试图字节级 diff，改成「行为等价 + 指令数在合理区间 + 性能不退化」。
→ 若某条路径溢出（`kNoMorePhysRegs`），说明老代码靠 `unuse()` 或固定临时寄存器强行压低了压力，
需要重新划分临时寄存器作用域。

**R5 · 绝对地址调用**（x86）
25 处 `c.call((void*)fn)` 在新版是 `invoke(..., (uint64_t)&fn, ...)`。x64 上 `call imm64` 不可编码，
asmjit 会用 address table（`call [rip+addrtab]`）兜底，代价是多一次内存间接跳转，
且要求代码与表在同一 ±2GB 内。
→ 由 `HAVE_STATIC_CODE_BUFFER` 的 32MB scratchpad 保证；更干净的方案见 S8。

**R6 · 内存所有权**
`arm_jit_reset()` 里的 `AsmJit::MemoryManager::getGlobal()->free(...)` 必须删掉（scratchpad 是静态数组）；
arm64 侧 `freeFuncs()` 的 `munmap`/`VirtualFree` 整块回收**保留**（`ArmJitTarget` 只管 bump，不管释放）。

**R7 · 32 位（Win32）配置**
`Gpz` → `new_gp_ptr()` 自动适配；但 `r64()` 宏 hack 与 `ASMJIT_X64` 判断都要替换成
`Environment::is_64bit()` 或 `ASMJIT_TARGET_ARCH_BITS`。

**验收**：以上各项各写一条注释 + 一句验证结果，附在 PR 描述里。

### S6 · 验证（约 2–3 天）

**第一层：生成码（离线，可自动化）**
- 该批 handler 的指令条数（族 1–2 要求 ≤ 改前 50%，其余允许 ±20%）；
- 用 `x86::Formatter` / `a64::Formatter` 反汇编后比较**语义序列**；
- 检查 6 处 `ptr_abs` 位置是否都变成 `[reg+disp]`。

**第二层：三方差分（S0 的台子）**
- 571 条 × 16 条件组 × 8 边界向量，**全部**跑过；
- 跨机器对齐 `hash(interp)`：两边必须相同（测试台自检）；
- 旧实现遗留的失败项单独归档，不进迁移 PR。

**第三层：整机回归**
- 《恶魔城》HUD：block size **15 / 16 / 100** 三档 + 暂存写计数与基线逐字节对比（§3.7）；
- 换 3~5 个不同类型的游戏冒烟（2D / 3D / 声音密集）；
- 自修改代码路径：让游戏运行中改写指令区，确认 `arm_jit_reset()` → 重编译不崩；
- 性能：冷启动 JIT 编译耗时 ≤ 1.2×；x86 帧率 ±5%；**arm64 帧率 ≥ 1.3×（族 1 门槛）/ ≥ 1.5×（全量目标）**。

**验收**：三份报告进 `documentation/`，附在 PR 里。

### S7 · 收尾（约 1 天）

1. 删除 `asmjit_compat.h`（把最后 22 处 `*_ptr` 宏直接写成 `x86::ptr`）。
2. 删除 `src/utils/AsmJit`，清理 5 个构建文件里的遗留条目。
3. 删掉 `arm64_emit.cpp` 里已被 asmjit 取代的编码函数（`output_w32` / `MOD_JUMP` / 手写 imm 编码 / `genimm2`），
   保留纯语义辅助（`count_leading_zeros` 之类）。
4. CI 增加一档"仅编译 arm_jit.cpp + arm_jit_arm.cpp + asmjit"，防止将来有人用 C++17 配置把它编译坏。
5. 把 §3.6 的契约清单、§3.7 的分类测试和差分 dump 一起归档到 `documentation/jit-semantics-baseline/`。

> **S7.2 已落地（rev10，2026-11-20）**：上面第 2 条原文估计"5 个构建文件"，实测是
> **9 个**——`interface/meson.build`、`posix/meson.build`、`posix/Makefile.am`、
> `posix/codeblocks/desmume.cbp`、`windows/Makefile` 各自仍把旧 AsmJit 的 29 个 `.cpp`
> 列进编译源（`DeSmuME.vcxproj` 主体已在早前摘掉 ClCompile，但两个 `.vcxproj.filters`
> 和 `DeSmuME_Interface.vcxproj` 仍有残留条目）。本轮全部清除并删除旧目录（69 文件）。
> 附带更新：`build_check.bat` 改为调用 `build_asmjit.ps1 -LibOnly` + `arm_jit.cpp` 用
> `/std:c++20`（旧版编 asmjit 1.0、用 c++14，迁移后已双重过时）；`VERSION.txt` 与
> `wire_asmjit_xcode.py` 里"旧目录保留作参照"的注释改为"已删除"。
> 本机证据：旧目录零残留追踪文件、`arm_jit.cpp` MSVC C++20 编译 exit=0、
> `wire_asmjit_vcxproj.py` / `gen_asmjit_sources.py` / `wire_asmjit_xcode.py` 三个自检全绿、
> harness 重建后 38/38 探针 0 mismatch。**POSIX 构建本机无法编译，需 Linux/macOS 侧确认**
> （旧 29 源此前被编进二进制但无人引用，删除后只会减少编译负担）。

### S8 · 可选优化（独立 PR，不要混在迁移里）

1. **x86：改用 `JitRuntime` + `JitAllocator::CreateParams::near_address = (uintptr_t)&arm_jit_compile`**
   取代 32MB 静态 scratchpad + `mprotect` hack，让 `call` 退化成 `call rel32`，省掉 32MB 常驻。
2. **arm64：寄存器缓存深化** —— 连续两条 `ldr` 合并成 `ldp`；块级标志改成延迟合成
   （一次 `cset` 批量写 NZCV，只在 block 末或用时物化）。
3. **两个后端共享块级语义**：`jit_dma_event.h` 已经证明"跨后端语义放头文件"可行。
   迁移完成后，把 cycle 记账、`sync_r15` 策略也收进共享头，让两个后端不可能再分叉。

---

## 5. API 映射表

### 5.1 x86：asmjit 1.0 → master

| asmjit 1.0 | master | 说明 |
|---|---|---|
| `#include <AsmJit/AsmJit.h>` / `x86.h` | `#include <asmjit/x86.h>` | include 根目录即 `asmjit/src` |
| `X86Compiler c(&ctx)` | `x86::Compiler cc(&code)` | 构造参数从 `Context*` 变 `CodeHolder*` |
| `X86Assembler a(&code)` | `x86::Assembler a(&code)` | |
| `GpVar` / `XmmVar` | `x86::Gp` / `x86::Vec` | Compiler 返回的就是寄存器对象 |
| `c.newGpVar(kX86VarTypeGpd)` | `cc.new_gp32()` | |
| `c.newGpVar(kX86VarTypeGpz)` | `cc.new_gp_ptr()` | 指针宽度，32/64 自适应 |
| `c.newGpVar(kX86VarTypeGpq)` | `cc.new_gp64()` | |
| `c.newXmmVar()` | `cc.new_xmm()` / `new_vec128()` | |
| `c.newLabel()` | `cc.new_label()` | |
| `c.bind(L)` | `cc.bind(L)` | 不变 |
| `c.clear()` | `code.reinit()` | 或 `reset()` + `init()` |
| `c.newFunc(conv, FuncBuilder0<int>())` | `cc.add_func(FuncSignature::build<int>())` | 返回 `FuncNode*` |
| `c.getFunc()->setHint(kFuncHintNaked, true)` | **无直接对应** | 见 R1 |
| `c.endFunc()` | `cc.end_func()` | |
| `c.make()` | `cc.finalize()` + `target.add(&p, &code)` | |
| `c.getError()` / `getErrorString()` | 返回值 `Error` / `stringify_error()` | 不再粘性 |
| `c.ret(v)` | `cc.ret(v)` | 语义变 FuncRetNode，见 R2 |
| `c.call((void*)fn)` | `cc.invoke(Out(node), (uint64_t)&fn, sig)` | |
| `n->setPrototype(conv, FuncBuilder2<...>())` | `invoke(..., FuncSignature::build<...>())` | 签名在 invoke 时给 |
| `n->setArg(i, v)` | `n->set_arg(i, v)` | |
| `c.unuse(v)` | 删除 | RA 自动 |
| `c.x86inst(id, ops...)` | `cc.emit(id, ops...)` | id 用 `x86::Inst::kIdXxx` |
| `dword_ptr(base, off)` | `x86::ptr(base, off, 4)` | 宽度必须显式 |
| `word_ptr` / `byte_ptr` / `qword_ptr` | `ptr(..., 2 / 1 / 8)` | |
| `ptr_abs(addr, idx, kScale2Times)` | `x86::ptr_abs(addr, idx, /*shift*/1)` | 见 R3 |
| `Context::generate(void**, Assembler*)` | `Target::_add(void**, CodeHolder*)` | |
| `assembler->relocCode(p)` | `flatten()` + `relocate_to_base()` + `copy_flattened_data()` | |
| `AsmJit::MemoryManager::getGlobal()->free(p)` | `Target::_release(p)` / `rt.reset()` | |
| `ASMJIT_CALL_CONV` / `kX86FuncConvDefault` | `CallConvId::kCDecl` | 即宿主 ABI |
| `Void` | `void` | |

### 5.2 AArch64：手写原语 → `a64::Compiler`

> 与 rev2 的根本区别：**不再"保持 `t_bytes*` 签名只换实现"，而是换到 Compiler + `Gp`。**
> 原语先提供 `int` 重载保证 573 个调用点能编译（S2），再按族改成虚拟寄存器（S4）。

| 现有原语（`arm64_emit.cpp`） | 结构级（物理寄存器） | 寄存器级（虚拟寄存器） |
|---|---|---|
| `output_w32(out, word)` | 删除（asmjit 编码）；万不得已 `cc->embed()` | 同左 |
| `emit_startfunc` / `emit_endfunc` | **删除**，改用 `cc.add_func()` / `cc.end_func()`（PEI 生成 prolog/epilog） | 同左 |
| `emit_mov(out, rs, rt)` | `cc->mov(x(rt), x(rs))` | `cc->mov(Gp rt, Gp rs)` |
| `emit_movimm(out, imm, rt)` | `cc->mov(x(rt), Imm(imm))`（自动展开 movz/movk） | `cc->mov(Gp rt, Imm(imm))` |
| `emit_movz/movk/movn` | `cc->movz/movk/movn` | 同左（有些地方依赖"恰好一条指令"） |
| `emit_add/adds/sub/subs/subsimm/cmp/cmpimm/test/testimm` | `add/adds/sub/subs/cmp/tst` | 同左 |
| `emit_and/ands/or/orr/eor/xor` | `and_/ands/orr/eor` | 同左 |
| `emit_andimm/emit_orimm/emit_xorimm/emit_andsimm` | 保留 `genimm2` 回退：asmjit 对非 bitmask 立即数返回 `kInvalidArgument`，**不会自动展开** | 同左 |
| `emit_lsl/lsr/asr/ror`（立即数与寄存器） | `lsl/lsr/asr/ror` | 同左 |
| `emit_rrx*` | 无对应指令，保留 `adc` 合成序列 | 同左 |
| `emit_mul/mla` | `mul/madd` | 同左 |
| `emit_smulbb/smulbt/.../smlal/umull/...` | `smaddl/smull/umaddl/umull` + `sbfx/ubfx` 取半字 | 同左；**B/T 的 x/y 选哪个寄存器，最易弄反** |
| `emit_ldr/str/ldrb/strb/ldrh/strh/ldrsb/ldrsh/ldrsw` | `cc->ldr/str/...` | 同左，但基址用常驻的 `base` |
| `get_ptr*/dword_ptr/byte_ptr/...`（27 个访问宏） | 保留原样（`int` 版） | **改为 `cache.read(n)` / `cache.write(n)`** |
| `emit_read_ptr64_reg(out, addr, r)` | `cc->mov(x(r), Imm(addr))` | 删除（基址常驻） |
| `emit_ptr(out, imm, rt)` | `cc->mov(x(rt), Imm(imm))` | 同左 |
| `emit_branch_label(out, id, cond)` | `cc->b(arm_cc(cond), L)`；`0xE` 走 `cc->b(L)` | 同左 |
| `emit_label(out, id)` | `cc->bind(L)` | 同左 |
| `genlabel()` | `cc->new_label()` | 同左 |
| `callR3(out)` | `cc->blr(x3)`（过渡）；最终 **`cc->invoke(Out(n), target, sig)`** | `invoke` + 调用前 `cache.flush()` |
| `emit_setc/setnc/sets/setnz/seto` | `cc->cset(w, CondCode)` | 同左（或延迟合成） |
| `emit_clz` / `emit_neg/negs/not/mvns` | `clz` / `neg/negs/mvn/mvns` | 同左 |
| `emit_zeroreg` | `cc->mov(x(r), xzr)` | 同左 |
| `emit_inc_ptr*/emit_inc_reg` | `ldr` + `add` + `str` | `add` on cached Gp |
| `createFunc()` 回填 + memcpy | `Target::_add` + `copy_flattened_data` | 同左（icache 见 §2.1） |
| `MOD_JUMP` | 删除 | 同左 |

**条件码映射（必须手写，不能强转）**

ARM 的 `0..0xD` 到 `CondCode` 恰好是 **+2**（`kEQ=0x02` … `kLE=0x0F`），
而 ARM 的 **AL=`0xE` 对应 `kAL=0x00`**。`(CondCode)0xE` 会得到 `kGT`（0x0E）——
无条件分支变成条件分支，**不报错、不崩**，只在特定 CPSR 下走错。

```cpp
static inline asmjit::CondCode arm_cc(u8 c) {
  switch (c) {
    case 0x0: return asmjit::CondCode::kEQ;  case 0x1: return asmjit::CondCode::kNE;
    case 0x2: return asmjit::CondCode::kCS;  case 0x3: return asmjit::CondCode::kCC;
    case 0x4: return asmjit::CondCode::kMI;  case 0x5: return asmjit::CondCode::kPL;
    case 0x6: return asmjit::CondCode::kVS;  case 0x7: return asmjit::CondCode::kVC;
    case 0x8: return asmjit::CondCode::kHI;  case 0x9: return asmjit::CondCode::kLS;
    case 0xA: return asmjit::CondCode::kGE;  case 0xB: return asmjit::CondCode::kLT;
    case 0xC: return asmjit::CondCode::kGT;  case 0xD: return asmjit::CondCode::kLE;
    case 0xE: return asmjit::CondCode::kAL;
    default:  ASMJIT_ASSERT(false); return asmjit::CondCode::kAL; // 0xF = NV
  }
}
```

### 5.3 共用

| 概念 | 迁移后 |
|---|---|
| 标签 | `asmjit::Label`（`new_label()` / `bind()`） |
| 错误 | `asmjit::Error`（每调用返回）+ `stringify_error()` |
| 目标内存 | 自定义 `Target` 子类（x86: 静态 scratchpad；arm64: 40MB 块） |
| 条件码 | ARM `0x0..0xE` → `CondCode` **显式映射**（`kAL` 是 0，不是 0xE） |
| 反汇编验证 | `x86::Formatter` / `a64::Formatter` |
| 块级边界 | `jit_dma_event.h` 不动；`emit_dma_event_check` 的 emit 顺序逐字保留 |

---

## 6. 风险清单

### 6.1 两个后端共有

| 级别 | 风险 | 症状 | 对策 |
|---|---|---|---|
| 高 | 三方语义分歧（§3.6） | 某游戏某处花屏/DMA 错位/帧率漂移 | 差分台 + armwrestler + 条款仲裁 |
| 高 | **块级边界丢失**（§3.7） | HUD/图块随机丢失，只在特定块大小出现 | 块级用例 + 块大小扫描 + 分类一致性测试 |
| 高 | 差分台本身有不确定性 | 两边 `hash(interp)` 不同 | 内存固定种子预填；快照清零 |
| 中 | cycle 记账偏移 | 帧率缓慢漂移，单条指令看不出 | §3.4 坑 2 的公式，作为独立断言 |
| 中 | 错误被吞 | 某个块静默生成半截代码 → 随机崩溃 | `ASMJIT_PROPAGATE` / `ASMJIT_ASSUME_OK` |
| 中 | 构建文件漏改 | macOS/Linux 构建挂 | 5 个构建文件逐一过 |

### 6.2 x86 侧专属

| 级别 | 风险 | 症状 | 对策 |
|---|---|---|---|
| 高 | `ptr_abs` 在 x64 的 32 位截断（R3） | 运行到某条指令直接 `0xc0000005` | 全部改寄存器持地址；反汇编确认 |
| 高 | naked 语义（R1） | 块入口多出 `push/pop`，返回地址错乱 | 反汇编核对 prolog 为空；必要时改 Assembler |
| 高 | `ret` 汇合到 exit（R2） | DMA 提前退出返回到错误位置 | 块级专项用例（§3.7） |
| 中 | RA 溢出（R4） | `kNoMorePhysRegs` 或性能骤降 | 缩小临时寄存器作用域，替代老 `unuse` |
| 中 | 32 位构建（R7） | 仅 Win32 配置失败 | CI 增加 Win32 编译档 |

### 6.3 arm64 侧专属

| 级别 | 风险 | 症状 | 对策 |
|---|---|---|---|
| 高 | **缓存未 flush 就 `ret` / 调用 C** | CPU 内存状态是旧的 → 花屏、状态错乱，且**只在引入缓存后才出现** | 铁律 2/3 写进注释；`ret` 与 `invoke` 前强制 `flush()`；单指令差分抓不到，靠 §3.7 块级用例 |
| 高 | **icache 未 flush** | 执行到旧指令 / 随机崩溃，小向量不复现 | `ProtectJitReadWriteScope`（析构自带 flush）或自己 flush，不能都不做 |
| 高 | **条件码强转**（ARM `0xE`=AL vs `CondCode::kGT`=0x0E） | 无条件分支变成条件分支，逻辑错乱但不崩 | `arm_cc()` 显式映射 + 单测遍历 0..0xE |
| 高 | **立即数编码限制**（逻辑立即数 / `add imm12`） | asmjit 返回 `kInvalidArgument`，被吞或崩 | 保留 `genimm2` 回退；对 `~0xFF00` 这类值专门测 |
| 中 | **banked 寄存器缓存失效** | 模式切换后读写到旧寄存器组 | 写 CPSR 模式位后 `invalidate_all()` |
| 中 | **R15 被误缓存** | `PC` 语义与 `sync_r15` 打架，分支目标错 | R15 永不进缓存；`read(15)` 直接报错 |
| 中 | Apple hardened runtime + MAP_JIT | 写入被拒绝 / 段错误 | `VirtMem::ProtectJitReadWriteScope` 包住 copy |
| 中 | `x18` 在 Win/macOS/Android 被保留 | 用了 x18 的路径在特定平台崩 | 现有 `X18_REG=23` 重映射保留，别"清理"它 |
| 中 | `a64::Compiler` 本身较新 | RA 断言 / 生成码异常 | 版本钉死 `677a10b`；遇到可疑立即降级到 Assembler 路径做对照 |
| 中 | **`_BitScanReverse` 形参类型**（既有问题，**已修**） | `arm64_emit.cpp` 把 `uint32_t*` 传给形参为 `unsigned long*` 的 intrinsic；Apple/Android 走 `__builtin_clzll` 所以没暴露，**Windows-on-ARM + MSVC 会 C2664** | S2 重写时改成 `unsigned long` 临时变量并转回 `uint32_t`，MSVC 实测编译通过 |

---

## 7. 工作量与并行切分

单人大约 **24–32 个工作日**（rev2 的 20–26 天上调，因为 arm64 从"平移编码器"变成"重写寄存器模型"）。

| 阶段 | 天 | 说明 |
|---|---|---|
| S0 基线 + 差分台 | 2 | 一次性投入，后面全靠它 |
| S1 删 arm32 + 构建 | 1.5 | 机械 |
| S2 骨架（两后端 Compiler） | 4 | 含 arm64 原语层 `Gp` 化 + 缓存骨架 |
| S3 x86 机械替换 | 3–5 | 与 S2 可并行（两个人） |
| S4 按族迁移 | 10–14 | 主体；族 1 后设性能门 |
| S5 语义收尾 | 3 | §3.6 + §3.7 + R1–R7 |
| S6 验证 | 2–3 | 三层 |
| S7 收尾 | 1 | |

**并行切分**：按 §2.2 的族切，但有三条铁律：

1. **S2 必须由一个人独立完成**（它是所有分段的公共前置）；
2. **同一族的 x86 与 arm64 必须由同一个人改**——否则"成对"无从保证；
3. **语义仲裁（§3.5）由一个人统一做**——否则 571 条会被几个人各自判断一遍，标准不一。

| 并行任务 | 范围 | handler |
|---|---|---|
| A · 骨架与工具 | S2 全部 + `compile_basicblock` 首尾 + `emit_dma_event_check` + `sync_r15` + `ArmRegCache` | — |
| B · 数据通路 + 移位 | 批次 1 + 2 | 252 |
| C · 访存 + LDM/STM | 批次 3 + 4 | 206 |
| D · 乘法 + 分支 + 状态 + 异常 | 批次 5 + 6 | 41 |
| E · 协处理器 + 饱和 + Thumb | 批次 7 + 8 | 92 |

---

## 8. 验收清单（贴在 PR 描述里逐条勾）

**构建**

- [ ] Win32 / Win64 / macOS(arm64) / Linux(x86-64) / Linux(aarch64) 全部编译通过
      （Windows 与 Xcode 两个工程已接线，见 §S1.3；macOS 待 MBP 实机构建，
      清单见 `documentation/macos-arm64-test-plan.md`）
- [x] `arm_jit.cpp`、`arm_jit_arm.cpp` 与 asmjit 用 C++20，其余仍 C++17（两个工程都按单文件设标准）
- [x] 不再引用 `src/utils/AsmJit` 任何头文件，两个工程也不再编译它（旧的 29 个源已摘掉）
- [x] `arm_emit.cpp` 已删除，`configure.ac` 不再为 `arm` 打开 `HAVE_JIT`（S1.1）
- [x] asmjit master `677a10b` 已 vendor 到 `desmume/3rdparty/asmjit`，`VERSION.txt` 记录 pin（S1.2）
- [x] `python tools/jit_matrix/gen_asmjit_sources.py --check` 退出码为 0（54 源 / 127 头，已通过）
- [x] `pwsh -File tools/jit_matrix/check_a64.ps1` 退出码为 0（236 个原语 + 块中途 `ret` 形状，S2）
- [x] `python tools/jit_matrix/check_xcode_asmjit.py` 退出码为 0（Xcode 工程接线自检）
- [x] aarch64 真机：`a64_exec_probe` 25/25，`a64_dump_new` 236/236（Termux，见 §S2）

**代码**

- [x] 935 个 x86 调用点全部改完，零 `c.getError()` / `getErrorString()` 残留（2026-11-20 实测均为 0）
- [x] 所有 `finalize()` 返回值被检查；arm64 原语全部走 `ASMJIT_ASSUME_OK`
      （仅存两处 `finalize()`：a64 `emit_core.cpp:184`、x86 `arm_jit.cpp:4276`，均检查了返回值）
- [x] 6 处 `ptr_abs` 全部改为寄存器间接寻址（2026-11-20）。3 处活跃的对齐掩码改为
      `mov reg, 0xFFFFFFFC` + `lea reg, [reg + bit<<1]`，语义不变、走相对寻址；
      2 处 `LOG_JIT` 调试宏与 1 处 32 位分支也一并消除。源码 `ptr_abs` 仅剩注释提及。
      x86_64 / i386 双架构 `-fsyntax-only` 均 0 错误；**本机 arm64 无法运行 x86 代码，
      运行时验证需在 Windows / Intel Mac 补做**。
- [ ] 591 组中 571 组的矩阵状态全部 `diff-pass`，其余 20 组保持 `interp-fallback`
      （当前 252 组 `diff-pass`：族 1 的 28 + 族 2 的 224 ARM 移位；见 §S4）
- [x] `arm_cc()` 映射有单测，遍历 0x0..0xE —— **该项已作废**：`arm_cc()` 是 rev5 拟定的辅助函数，
      实现中从未出现；现有后端统一走 `emit_branch_label()` 直接编码条件，无此映射层。
- [x] `emit_andimm` 等立即数原语保留回退，且有非法立即数用例 —— `emit_andimm` 现已不使用
      `genimm2`（直接 `mov imm → X10; and`，对任意 32 位立即数成立，无非法分支）；
      保留 `genimm2` 回退的是 `emit_andsimm`。非法立即数（`0x000FF085` 等）已在差分台
      81920 次比较中覆盖，无需独立用例。
- [x] arm64 的 571 个 handler 不再使用 `tmp`/`tmp2` 这类固定临时寄存器（或已确认为"必须固定"并注释原因）
      —— **已确认为必须固定**（2026-11-20）。`tmp`/`tmp2` 是宿主 w2/w3（R0..R11 手写暂存量），不是模拟寄存器。
      本轮两个探针实测否决了 asmjit RA：手写 `save()/restore()` 帧与 PEI 冲突会产生不配对的 `ldp`，
      且 RA 不在 `callR3()` 的裸 `blr` 处切活跃区间。两项依据详见 `arm_jit_arm.cpp`
      "Why this does *not* use asmjit virtual registers"，`arm64_emit.cpp` 的 R0..R11 定义处已加指引注释。
- [ ] `python tools/jit_matrix/gen_opcode_matrix.py` 退出码为 0（含 §3.8 的六项断言）

**语义（本次迁移的主产出）**

- [ ] 三方差分：571 条 × 16 条件组 × 8 边界向量，`hash(interp) == hash(x86 JIT) == hash(arm64 JIT)`
- [ ] 跨机器 `hash(interp)` 逐条相同（测试台自检通过）
- [ ] 条件执行的 cycle 公式（§3.4 坑 2）在所有条件指令上验证通过
- [ ] **块级 DMA 用例**：两个后端退出点、`instruct_adr`、`bb_total_cycles`、`mmu_dma_event_pending` 完全一致
- [ ] **`instr_is_store` 分类一致性**：4096 + 1024 个槽位逐一比对"是否真写内存"，零差异
- [ ] 旧实现遗留的失败项已归档为"既有 bug 清单"，且**未**混入迁移 PR

**性能**

- [x] `ArmRegCache` **五条**铁律（R15 不进缓存 / 调用前 flush / ret 前 flush /
      仍走内存的路径先 evict / 条件指令 merge point 先在内存上会合）都有注释与差分用例
- [ ] ALU 密集场景 ≥ 1.3×（族 1 门槛）、≥ 1.5×（全量目标）——**未测，代码尺寸门槛先失败**
- [ ] 族 1 的 handler 平均生成指令数 ≤ 改前 50% ——**已过**：实测 0.499（边际 12 → 6），
      见 S4 族 1 第四步；`ArmRegCache` 之后再按 x64 的做法改造标志路径才达标。
      族 2 仍等整机帧率实测（代码尺寸达标 ≠ 帧率达标）。
- [ ] x86 帧率在 ±5% 内；冷启动 JIT 编译耗时 ≤ 1.2×
- [ ] 32MB scratchpad 行为未变（x86）／40MB 块回收路径未变（arm64）

**整机**

- [ ] 《恶魔城》HUD：block size 15 / 16 / 100 全部正常，暂存写计数与基线逐字节一致
- [ ] 3~5 个游戏冒烟无回归；自修改代码路径不崩
- [ ] 兼容层 `asmjit_compat.h` 已删除

---

## 9. 现状量化：指令膨胀率、访存路径与寄存器缓存（2026-11-20）

本节只记录**当前代码的实测现状**，作为后续优化的基线；不改变任何生产代码，也不把结论混入验收项。

### 9.1 测量对象与口径

- 代码：HEAD `a78abf4b`，macOS Apple Silicon，AArch64 后端。
- 工具：`.build/diff-mac/jit_diff --block N --slot S`，连续种 `N` 条相同指令，
  host 指令数 = 生成代码字节数 `/ 4`。
- **边际成本** = `(N=16 的 host 指令数 − N=1 的 host 指令数) / 15`，用于剔除块序言/尾声。
- **n=12 块折算** = `N=12` 的 host 指令数 `/ 12`，对应默认 `jit_max_block_size = 12`。
- 注意：`slot << 20` 会清零槽位低位，**只有 ALU、字 LDR/STR 和 Thumb 槽位**适合用该模式代表；
  LDRH/STRH/LDM/STM 等槽位不能用本节数字外推。
- 单条指令块的全量结果是 `12,101,320 bytes / 81920 ≈ 36.9 host 指令/块`。
  这是**固定开销占主导**的数字，不能当作 Guest:Host 比例使用。

### 9.2 实测比例

| 指令（连续 N 条） | 边际 Host/Guest（N=16） | n=12 块折算 Host/Guest |
|---|---:|---:|
| ARM `ADD` imm | 2 | 5.25 |
| ARM `ORR` imm | 2 | — |
| ARM `MOV` imm | 2 | — |
| ARM `AND` reg | 6 | 9.1 |
| ARM `EOR` reg | 6 | — |
| ARM `ADDS` imm | 7 | 10.25 |
| ARM `CMP` imm | 7 | — |
| ARM `LDR` word | 29 | 31.8 |
| ARM `STR` word | 49 | 52.1 |
| ARM `SWP` | 27 | — |
| Thumb `ADD` reg | 11 | 14.1 |
| Thumb `MOV` imm | 10 | — |
| Thumb `LDR` word | 26 | 28.8 |
| Thumb `STR` word | 47 | 50.1 |
| Thumb `PUSH` | 48 | — |

`N=1` 的固定成本（host 指令/块）：ARM `ADD` 41、`AND` 43、`ADDS` 46、`LDR` 63、`STR` 86、`SWP` 67；
Thumb `ADD` 48、`MOV` 47、`LDR` 60、`STR` 84、`PUSH` 88。
块越小，这部分占比越高，所以上表同时给出边际值与 n=12 折算值。

**结论：**

- 纯 ALU 大约 **1:2 ~ 1:7**（Thumb 约 **1:10 ~ 1:11**）。
- 字访存大约 **1:26 ~ 1:52**，是目前最贵的一类。
- 按真实混合负载估计，整体约 **1:10 ~ 1:20**，**不是 1:3 ~ 1:5**。
  引用 1:3 ~ 1:5 时必须限定为"寄存器已缓存的纯 ALU"，否则会低估访存与调用开销。

### 9.3 访存路径：仍是 C++ 回调，没有 fastmem

当前 `OP_LDR_` / `OP_STR_` 的生成路径是：

1. `emit_ptr(... LDR_tab/STR_tab[PROCNUM][classify_adr(...)], R3)` 选 helper；
2. `callR3` 调用；
3. helper 内的 `OP_LDR` / `OP_STR` 调 `READ32` / `WRITE32`；
4. 最终落到 `cpu->mem_if->read32/write32`，这是**第二次间接调用**。

`classify_adr` 只按 MAIN / DTCM / ERAM / SWIRAM 选择 5 份**内容相同**的 helper 副本，
目的是让分支预测器区分区域；它不是运行时地址映射，也没有 mmap/页表直映射。

`callR3` 每次调用的固定开销至少 15 条 host 指令：

- `save_call`：6×`stp` + 1×`str`；
- `blr`；
- `restore_call`：1×`ldr` + 6×`ldp`。

`STR` 还额外执行 `emit_dma_event_check`，这解释了同宽度下 `STR` 比 `LDR` 更贵（49 vs 29）。

### 9.4 寄存器缓存：存在，但只覆盖 ARM ALU/移位

`ArmRegCache`（`arm_jit_arm.cpp`，约 413 行起）已有基础块内缓存：

- 3 个物理槽：`x12` / `x15` / `x21`；
- `dirty` / `touched` 状态，惰性回写；
- `read` / `read_write` / `write` / `flush` / `evict`；
- 块入口 `g_regs.reset()`。

覆盖范围是 **252 组 `diff-pass`**：族 1 的 28 个 ARM 数据处理指令，
加族 2 的 224 个 ARM 移位（其中 223 个走缓存，1 个 NOP 特例）。

**未覆盖**（仍走 `reg_ptr` / `reg_pos_ptr`，并在访问前 `evict`）：

- 174 个 ARM 访存；
- 32 个 LDM/STM；
- 28 个乘法；
- 13 个分支/状态/异常；
- 22 个协处理器/饱和；
- 70 个 Thumb。

此外，当前没有完整活性分析，也没有死标志位消除；S 指令的标志是**急切计算并写回内存**；
条件执行用 branch-over，不是 `CSEL`。因此"已有基础块内寄存器缓存"成立，
但"缓存已覆盖热点路径"不成立。

### 9.5 下一步优先级

| 优先级 | 事项 | 理由 |
|---|---|---|
| P0 | 访存 fastmem / 自定义 helper ABI，减少 `save_call`/`restore_call` | 访存占真实负载大头，当前 1:26 ~ 1:52 |
| P1 | Thumb 批次 8 | Thumb 目前整体比 ARM 贵，且覆盖缺口最大（70 组） |
| P2 | 死标志位消除 / 惰性标志 | S 指令急切写标志，ALU 密集代码仍有明显冗余 |
| P3 | 缓存扩到 4–6 槽，并覆盖 load/store 基址与数据 | 当前 3 槽只够 ALU/移位，访存前必须 `evict` |

以上数字来自"连续相同指令"的微基准，不含分支、寄存器压力和真实游戏指令混合；
它们适合做**相对比较**和回归门槛，不应直接外推为整机帧率。

---

## 10. P0 第一刀：`call_mem` 轻量 helper 调用（2026-11-20，已落地）

### 10.1 改了什么

§9.3 指出访存是当前最贵的一类（字访问 1:26 ~ 1:52），其中每次 helper 调用的
固定开销是 `save_call`(6×`stp` 存 x1..x12 + 1×`str` 存 x23) + `blr` +
`restore_call`(1×`ldr` + 6×`ldp`) = **15 条**。这一刀只动调用序列，不动访存语义、
`classify_adr`、MMU 回调或 DMA 检查。

在 `arm64_emit.cpp` 新增：

```cpp
static void call_mem(t_bytes *out) {
    stp x12, x15, [sp, #-16]!;   // 只存两个易失缓存槽
    blr x3;                       // helper 指针仍在 x3
    ldp x12, x15, [sp], #16;
}
```

即 **3 条**。用它替换 `OP_LDR_` / `OP_STR_` 两个宏体里的 `callR3`（`arm_jit_arm.cpp:2031` /
`:2223`）——这两个宏覆盖 174 个 ARM 访存 handler 的绝大多数，是 P0 要打的热点。
其余 22 处 `callR3` / `callR7` / `callR9` 调用点**原样保留**。

### 10.2 为什么不直接全局精简 `save_call`

逐调用点核对 24 处 helper 调用后发现，`save_call` 保存的 x1..x12 不是无用功：

- `S_DST_R15`（`:793`）调用 `armcpu_switchMode` 后紧接着用 R9 / R10；
- `OP_MSR_`（`:1891`、`:1827`）调用后用 R1 作地址、用 `operand` / `xPSR`；
- `LDRD/STRD` 写回（`:2441`）、SWP（`:2518`）、LDM/STM（`:2715`）调用后仍持有 `addr` / `Rs`。

这些点若换成 `call_mem` 会读到被 helper 破坏的寄存器。所以优化必须**按调用点存活面定制**，
不能全局替换。这是本刀只覆盖 LDR/STR 两宏的原因。

### 10.3 存活面论证（call_mem 为什么对 LDR/STR 安全）

AAPCS64 下 x19–x28 是 callee-saved，C helper 自动保留——所以 x21（缓存槽）、
x23（DMA flag 别名）、x24/x25/x26（三个常驻基址）无需在调用点保存。
`save_call` 里那条 `str x23` 对这些热点是多余的。

 caller-saved 一侧逐项核对 `OP_LDR_` / `OP_STR_` 调用之后的指令：

| 寄存器 | 调用后是否被用 | 结论 |
|---|---|---|
| x0 (R0) | 是，helper 返回的 cycles | 返回值，不保存（会被 helper 覆盖） |
| x1 (R1) | Rd==15 分支用，但经 `cpu_ptr(...,ADDRESS)` 重算 | 不依赖调用前值 |
| x2/x3 (tmp/tmp2) | Rd==15 分支用，但先 `reg_ptr`+`emit_mov` 重算 | 不依赖 |
| x6 (rhs) / x8 / x9 (adr/dst) | 调用后不再用 | 死值 |
| x11 (CACHED_PTR) | 调用点后无 `CACHED_PTR()` 消费 | 死值，不保存 |
| **x12 / x15** | **ArmRegCache 易失槽，可能持有前一条指令的脏值** | **必须保存** |

关键铁律：x12/x15 里若有前序指令缓存的脏 emulated 寄存器，helper 一旦破坏它们就会
丢值——这正是 `call_mem` 唯一保留的保存项。x11 在这两个宏里不参与跨指令状态，故省下。

### 10.4 验证证据（本机）

- **编码层**：`a64_corpus.inc` / `a64_golden.txt` 新增 3 个 case，`a64_dump_new` 实测
  `call_mem = A9BF3FEC D63F0060 A8C13FEC`（`stp x12,x15` / `blr x3` / `ldp x12,x15`）= 3 条；
  `callR3` = 15 条。`check_a64.py` 的 4 个已知基线差异（save/restore/startfunc/endfunc，
  x25/x26 常驻基址改造后 golden 未刷新）与本次改动无关，新增 3 case 全匹配。
- **x86 回归**：`jit_diff --full` 81920 次比较，R0-R14 / CPSR / instruct_adr / 功能不变量
  全部 0 mismatch；仅周期数与已知 `jit not repeatable` 记录项非零（§3.5 已定性为记录项，
  且改动未涉及 x86 后端与 cycle 逻辑）。

### 10.5 预期收益与待验证项

**静态可算的部分**：每次 LDR/STR helper 调用省 12 条 host 指令。用 §9.2 的边际口径，
`LDR word` 边际 29、边际 49，其中各含 1 次 `callR3`。若这 12 条全部计入边际（单指令块
的序言摊薄口径下保守取 8–12 条），字访存边际有望从 29→17~21、49→37~41，
即访存膨胀率下降约 25–40%。这是**按编码推算**，不是实测。

**本机测不了、需 macOS 补测**：

- [ ] `build_diff_macos.sh` 重建后跑 `jit_diff --full`，确认访存槽位 R0-R14 / CPSR /
      `instruct_adr` / 内存写集全 0（尤其 LDR/STR 的 Rd==15 与写回变体）；
- [ ] `jit_diff --block --slot` 实测 LDR/STR 边际指令数，与 10.5 的推算对照；
- [ ] 整机 ROM 回归（花屏 / DMA 提前退出路径，§3.7），确认缓存脏值在 helper 调用后未丢失；
- [ ] 帧率门槛（§S4：ALU ≥1.3×、访存明显提升）仍只有这一刀之后才能谈，尚未验证。

> 若 macOS 差分在访存槽位报错，第一嫌疑是 x12/x15 之外的存活寄存器漏保存——
> 按 §10.2 的调用点清单回查，必要时把该调用点单独换回 `callR3`。这是本刀设计成
> "只动两宏、可快速回退"的原因。

### 10.6 为什么 x64 侧不做 fastmem（2026-11-20 评估，决定放弃）

曾评估在 x64 后端也做 MAIN_MEM 内联快路径，调研后**主动放弃**，理由是可复现的硬约束，
不是 x64 后端慢：

1. **地址转换不是线性的**。`_MMU_read32` 走两级查表 `MMU_MEM[proc][adr>>20][adr&MASK]`，
   没有预留的线性窗口；MAIN_MEM 用 `adr & _MMU_MAIN_MEM_MASK32` 索引，而 mask 随
   debug console / DSi 模式变化（`MMU.cpp:1045-1049`），生成码里硬编码会在模式切换后读错。
2. **写操作必须触发 JIT 自修改失效**。每次写 MAIN_MEM 都要执行
   `JIT_COMPILED_FUNC_KNOWNBANK(...) = 0`（`MMU.h:930/988/1043-1044`），否则自修改代码
   继续执行旧块。内联直写会绕过它，这是会花屏的静默错误。
3. **cycle 记账无法内联**。`OP_LDR`/`OP_STR` 把数据读取和 cycle 封装在同一次 `c.invoke`
   里，cycle 经 `MMU_aluMemAccessCycles` → `FetchAccessUnit::Fetch`（`MMU_timing.h:195-224`），
   依赖 `m_lastAddress` 非顺序检测、`arm9dataCache.Cached` 命中模拟和 debug/Lua 内存 hook；
   `advanced_timing` 默认 true（`NDSSystem.cpp:3409`）。内联数据但留 helper 算 cycle 不省
   调用（等于白做），全内联 cycle 则与解释器不一致。

**结论**：x64 侧的 `c.invoke` 由 asmjit 按标准 Windows x64 ABI 生成，没有 arm64
`save_call` 那种"无条件存 x1–x12+x23"的固定冗余；x64 已有虚拟寄存器 + 内存操作数折叠
（§1.4），访存热点本身不算慢。两后端"对齐"的正确含义是**各自采用安全范围内更优的方法**：
arm64 做 `call_mem`（§10.1），x64 保持现状。本决策不改任何 x64 生产代码。

**工具链附注**：评估过程中发现 `jit_diff_main.cpp` 无条件 extern 了 arm64 专有的
`g_TOTALBYTES` / `g_all_block_bytes`（定义在 arm_jit/emit_core.cpp），而 x64 的
`build_core.ps1` 不编该文件——这是既有的工具链不同步：`--block`/`--sweep` 的代码体积统计
在 x64 上恒为 0，但功能差分正常。`build_core/x64_harness_stubs.cpp` 是本地链接 stub
（不在版本控制内），仅用于让 x64 的 jit_diff.exe 链接通过；它不改动生产代码，也不影响
功能比较的结论。

### 10.7 第二刀：把轻量调用扩展到其余访存热点（2026-11-20，已落地）

§10.1 的 `call_mem` 只覆盖了 `OP_LDR_`/`OP_STR_`。本刀按同一套存活面论证，把其余
**调用后仅用 R0(cycles)** 的访存点也切过去，并针对两类特殊点加了变体：

| 变体 | 编码 | 用在哪 | 为什么多存/换寄存器 |
|---|---|---|---|
| `call_mem` | stp/ldp x12,x15 + blr x3 = **3 条** | ARM LDR/STR、SWP/SWPB、Thumb STR/LDR/SPREL/PCREL | 函数指针在 x3；调用后只有 R0 活着 |
| `call_mem_r7` | stp/ldp x12,x15 + blr x7 = **3 条** | LDM/STM（`call_ldm_stm`） | 函数指针在 x7（4 个参数占了 x0–x3） |
| `call_mem_r2` | stp x2,x12 + str x15 + blr x3 + 恢复 = **5 条** | LDRD/STRD 三处 | x2 是 effective address，W 位写回在调用后（`arm_jit_arm.cpp:2446/2452`）要用 |

**覆盖统计**：24 处 helper 调用点中 **15 处**换成轻量变体（call_mem 11 + r2 3 + r7 1），
**9 处保留 `callR3`**——它们确实有 caller-saved 值跨调用存活：

- `:744/:793/:1827/:1891` —— `changeCPSR` / `S_DST_R15` / `OP_MSR_` 调用后用 R9/R10/R1/operand；
- `:2813/:2824` —— LDM/STM 的 `armcpu_switchMode` 前后用 `oldmode`；
- `:3001` —— `_maskPrecalc`；`:3410/:3421` —— SWI 用 R4；`:4461` —— 解释器回退 `emit_armop_call`。

**逐点复核的调用后依赖**（本刀正确性的核心）：SWP/Thumb 各点的 `reg_pos*`/`g_regs.evict`
都在 emit_ptr **之前**展开，调用后只剩 `emit_st_counters`(R0) 和
`emit_MMU_aluMemCycles`（后者只写 x_reg/R2 临时，不读旧值）；LDRD/STRD 的 x2 由
`call_mem_r2` 显式保存。缓存槽 x12/x15 在所有点都保存——这些访存 handler 与族 1/2
共用块级缓存，可能有前序指令的脏值。

**验证（本机）**：

- 编码层：`a64_corpus.inc` / golden 新增 `call_mem_r7` / `call_mem_r2` 两个 case，
  实测 `call_mem_r7 = A9BF3FEC D63F00E0 A8C13FEC`、`call_mem_r2 = A9BF33E2 F81F0FEF
  D63F0060 F84107EF A8C133E2`，解码与预期一致（栈 push/pop 各 32 字节平衡）；
  `check_a64.py` 仍是 4/241 已知基线差异，新增 case 全匹配。
- x86 回归：`jit_diff --full` 81920 次比较，功能桶全 0（与基线一致，12067 是周期记录项）。
- **语义层仍需 macOS**：本刀改了 15 个调用点，覆盖 SWP/LDM/STM/Thumb/LDRD/STRD，
  §10.5 的待验清单应扩展到这些族的槽位（尤其 LDRD/STRD 的 W 位写回、Thumb PUSH/POP）。
  回退粒度：任一族差分报错，把该族调用点单独换回 `callR3`/`callR7` 即可。

### 10.8 访存差分安全网：定向探针 + 待办追踪（2026-11-20）

**问题**：族 3 的 x86 结构迁移与 arm64 结构级 + `call_mem` ABI 均已完成，
但差分台对访存的覆盖存在一个结构性空洞——`runSweep()` 用 `opcode = slot << 20`，
低 20 位恒为 0，所以每个访存槽位只测到 `LDR r0,[r0]` 这一退化形式
（基址=目的、零偏移、无写回）。原有 18 个 `kProbes` 定向探针里**没有一条访存指令**。
于是写回（前/后索引）、Rd≠Rn、字节/半字/符号扩展宽度、寄存器偏移、
Thumb SP 相对与 PUSH/POP 这些访存语义坑最密的变体，本机差分从未覆盖。

**做法**：在 `jit_diff_main.cpp` 的 `kProbes` 增加 20 条访存定向探针，共用已有的
`compareOne()` 比较路径（与 sweep 同等强度），以 R13（primeState 设定的合法栈基址
`kStackTop`）作访存基址，保证两引擎访问同一映射 RAM。STR 探针写入幂等值（同一份
prime 后的 R0），harness 只比寄存器状态，不受内存副作用干扰。覆盖清单：

| 类别 | 探针 |
|---|---|
| 字 + 偏移 | `LDR/STR r0,[r13,#±4]`（Rd≠Rn、正/负偏移） |
| 写回 | `LDR/STR ...#4!` 前索引、`...],#4` 后索引（4 条） |
| 宽度/符号 | `LDRB`/`LDRH`/`LDRSB`/`LDRSH`、`STRB`/`STRH` |
| 寄存器偏移 | `LDR r0,[r13,r1]` |
| Thumb | `LDR/STR r0,[sp,#4]`、`LDR r0,[r1,r2]`、`PUSH/POP {r0,r1}` |

> 一条 `LDR r0,[r1,#4]`（Rn 为普通操作数、可能未映射）曾产生 5 个 **cycle-only**
> 差异（vec5–7，MMU 路径随时隙值变化），不是语义缺陷；已换成 SP 基址消除该噪声。
> 这再次印证 §3.5 的边界：周期数不进仲裁。

**验证**：
- 38/38 探针（18 原有 + 20 访存）× 8 向量 × 2 标志 = **0 mismatch**；
- `--full` 81920 次比较功能桶全 0（R0–R14 / CPSR / `instruct_adr` / invariant / fault），
  12067 仍是可忽略的周期记录项，与基线一致；
- 重建说明：`jit_diff.exe` 依赖 `desmume_core.lib`（旧版），其未导出 arm64 专有的
  `g_TOTALBYTES`/`g_all_block_bytes`，靠 `build_core/x64_harness_stubs.cpp`（gitignore，
  本地链接 stub）补齐；改 harness 后需重编 `diff_jit_diff_main.obj` 并带该 stub 重新链接。

**独立待办追踪**：把本文散落的未完成项按验证 / 构建 / 迁移 / 性能 / 收尾分类，
整理为 `documentation/jit-open-tasks.md`，作为可勾选的索引视图；本文仍是权威来源。

**边界不变**：这些探针加固的是 **x86 侧**差分网；arm64 访存语义（尤其 `call_mem`
两刀改的 15 个调用点）仍需按 §10.7 在 macOS 上验证，本机无法替代。族 3 的 174 组
`diff-pass` 仍要等 macOS 三方差分 + §9.5 P3（访存接入 `ArmRegCache`）之后才能置位。

### 10.9 核心库验证盲点与 x64 全量重跑（2026-11-20）

**发现**：整体重跑 x64 验证前核对二进制时间戳，发现 `desmume_core.lib` 停在
**11/19 21:56**，而 arm_jit.cpp 此后有 5 个提交（`f5722fb6` JitRuntime、
`dbc5ab53` cycle 修正、`4d1d5545` Universal 接线、`15e8409c` 移除 ptr_abs）。
也就是说 §10.5–§10.8 里"x86 `--full` 功能桶全 0"验证的其实是**旧 JIT 代码**，
不包含 11/20 的 x86 修复。这是个真实的验证盲点：harness 源码是新的，但它链接的
核心库是旧的。

**根因**：`build_core.ps1` 只对 `arm_jit.cpp` 设了 `/std:c++20`，vendored asmjit 的
52 个源仍按 c++14 编译，撞上 axl 头里的 `requires` / `std::is_const_v` 全部失败
（见旧 `build_core/failures.txt`）。结果核心库一直没能在迁移后重建。

**修复**：`build_core.ps1` 里把 C++20 特判从"仅 arm_jit.cpp"扩到"arm_jit.cpp 或
路径含 `3rdparty/asmjit`"，两者都带 `/DASMJIT_STATIC`。`-Force` 重建：
**194/194 源编译、0 失败**，新库 23.6 MB / 223 objects，含全部 11/20 x86 修复。

**整体重跑结果**（新库重链 `jit_diff.exe` 后）：

| 验证 | 规模 | 结果 |
|---|---|---|
| `--full` 单指令 sweep | 81920 比较 | 功能桶（R0–R14 / CPSR / `instruct_adr` / invariant / fault）全 0；12067 为 cycle 记录项 |
| `--seq 2` | 6848 比较 | 0 mismatch（4012 跳过为有内存副作用的槽） |
| `--seq 3` | 6592 比较 | 0 mismatch |
| `--seq 4` | 6336 比较 | 0 mismatch |
| 默认 38 探针 | 38×8×2 | 0 mismatch |
| `wire_asmjit_vcxproj` / `wire_asmjit_xcode` / `gen_asmjit_sources` / `gen_opcode_matrix` `--check` | — | 全 exit=0 |

**结论**：`15e8409c` 等 11/20 的 x86 修复在新的核心库下功能正确；上一轮 S7.2 删除
旧 AsmJit 后，x86 后端完全自足（新 asmjit + C++20），无回归。
`--block`/`--sweep` 的代码体积统计仍依赖 arm64 专有的 `g_TOTALBYTES`，在 x64 上恒为 0，
本节的结论只针对功能差分，不含性能数字。

**经验（写进流程）**：差分前必须核对 `desmume_core.lib` 的时间戳 ≥ 所有 JIT 源的最后
提交时间；否则"全绿"验证的是过期二进制。改 harness 或核心源后要同时重编核心库、
重链 harness。

### 10.10 族 2 补刀：Rm/Rs 进缓存 + 边际成本基线（2026-11-21）

**背景**：§S4 族 2 的收益只有族 1 的一半，原因写在该小节里——`arg` 宏仍用
`reg_pos_ptr()` 读 Rm（寄存器移位形式还有 Rs），而 3 槽缓存容不下 Rd/Rn/Rm/Rs 四个。
但 Rm/Rs 和前两者形状不同：它们只被读一次就进固定 scratch（`rhs_reg` / `imm_reg`），
不需要跨指令驻留。`ArmRegCache::read_into()`（族 5 为乘法加的入口）正好是这个形状：
槽里有最新值就 `mov`，否则发与内存路径相同的 `ldr`，且**不占槽**。

**改法**：

| 位置 | 改前 | 改后 |
|---|---|---|
| `LSL/LSR/ASR/ROR_IMM`、`REG_OFF` 及 S 变体（10 个宏） | `reg_pos_ptr(0, rhs_reg, VALUE)` | `rc_read(REG_POS(i,0), rhs_reg)` |
| `LSX_REG` / `S_LSX_REG` / `ROR_REG` / `S_ROR_REG`（4 个宏） | `reg_pos_ptrB(8, imm_reg, VALUE)` | `rc_read(REG_POS(i,8), imm_reg)` |
| `LDRD/STRD` 寄存器偏移（POST/PRE 两处） | `reg_pos_ptr(0, idx, VALUE)` | `rc_read(REG_POS(i,0), idx)` |
| `SWP/SWPB` 基址 | `reg_pos_ptr(16, addr, VALUE)` | `rc_read(REG_POS(i,16), addr)` |

寄存器移位量原来按**字节**读（`reg_pos_ptrB`），现在按 dword 读：`emit_lsl_reg()` 等
在用它之前都做了 `& 0xff` / `& 0x1f`，高位是死值。Rd 仍走内存（helper 通过 `u32*`
写回），R15 仍走内存（规则 1），写回/列表类写操作仍先 evict（规则 4）。

**验证（真机 aarch64 / Termux）**：

| 项 | 结果 |
|---|---|
| `a64_block_probe` 全量 | **904 checks / 0 failed**（访存表新增 4 例） |
| 改前 vs 改后的整份输出 | **逐字节相同**（`diff` 无输出；含 R0–R14 / CPSR / cycles / 内存 / resume 地址） |
| `--marginal`：`ldrd r0,[r1,r2]`（R2 脏） | 124 → **120 字节（31 → 30 条）** |
| `--marginal`：`swp r0,r2,[r1]`（R1 脏） | 224 → 224；转储里 `ldr w2,[x25,#4]` 变成 `mov w2,w12`，块尺寸被同一块的别处抵消 |
| 本机语法门 | `clang++ -fsyntax-only`（x86_64 主机，`-DASMJIT_NO_X86`）exit=0 |

**新增工具：`--marginal`**（`tools/jit_matrix/a64_block_probe.cpp`）。同一块编译两次，
第二次多一条目标指令，差值就是该指令的边际 host 代码尺寸——x86 侧 `--block --slot`
的 arm64 对应物，而且能在真机上跑。基线（2026-11-21）：

| 用例 | 边际 | 说明 |
|---|---|---|
| `arm add r0,#1` | 4 条 | 无 S：只有 Rm 拷贝 + ALU |
| `arm adds r0,#1` | 9 条 | +`SET_NZCV`（`mrs`/`ldr`/`lsr`/`bfxil`/`strb`） |
| `arm cmp r0,#0` | 8 条 | 同上，少一条写目的 |
| `thumb adds r0,#1` | 8 条 | Thumb 的 ALU 一律带 S |
| `thumb cmp r0,#0` | 7 条 | |
| `arm ldr r0,[r1,#4]` | 17 条 | |
| `arm ldr r0,[r1,r2]`（R2 冷 / 脏） | 18 / 18 条 | 脏 Rm 不再多花 store+load |
| `arm ldr r0,[r1,r2,lsl #2]`（脏） | 20 条 | |
| `arm str r0,[r1,r2]`（脏） | 44 条 | 约 30 条是 `emit_dma_event_check` 的内联冷路径，实际只执行 3 条 |
| `arm add r0,r0,r2,lsl #2`（脏） | 7 条 | |
| `arm ldrd r0,[r1,r2]`（脏） | 30 条 | 本轮改动后 |
| `arm swp r0,r2,[r1]`（脏） | 56 条 | |
| `thumb ldr r0,[r1,#4]` | 14 条 | |
| `thumb ldr r0,[r1,r2]`（脏） | 15 条 | |

三条可直接用的结论：

1. **S 位是 ALU 的主要成本**：`add` 4 条 vs `adds` 9 条，差值就是标志导出。§9.5 P2
   （惰性标志）的收益上限由此可算——每条 S 指令最多能省 5 条 host 指令。
2. **Thumb 的同语义 ALU 并不比 ARM 贵**：`thumb adds` 8 ≈ `arm adds` 9。Thumb 的差距
   来自"每条都带 S"，不是编码导致。§9.5 P1 里"Thumb 比 ARM 贵"必须按同语义
   （`adds` vs `adds`）比对，否则会高估。
3. **`STR` 的 44 条不能当成执行成本**：冷路径只在 DMA 真的被触发时才执行。看性能要
   把内联冷路径和热路径分开算。

**未覆盖**：§10.5 的 macOS 三方差分仍待补——本节只增加真机块级等价性与代码尺寸证据，
不替代 §8 的三方哈希；`--marginal` 量的是代码尺寸，不是执行成本。

### 10.11 把 arm64 块级探针的形状移植到 x64 差分台（2026-11-21）

**动机**：arm64 的块级探针（§S4 族 5 起）只能证明"改动前后一致 + 手写期望成立"，
它没有解释器当 oracle；而 macOS 机器与 x86 机器不在一处，三方对账排不上期。x64 差分台
（`jit_diff`）链着**真解释器**，于是把 arm64 探针覆盖的形状搬过去，先在 x64 上把每条
形状与解释器逐一对照。这是"能证明的一半"和"要等另一半"之间唯一能立刻做的事。

**改法**（`desmume/src/utils/jit_diff/jit_diff_main.cpp`）：

- `Probe` 增加两个字段：`ops`（操作数初值表，`nullptr` = 沿用 `kOperands` 轮转）与
  `sp`（把 R13 从 `kStackTop` 下移进主存）。原因有两个：`kStackTop` = 0x02400000 正好
  是主存**上界之后**，升序访问全部落在映射外；寄存器偏移形式的 Rm 又取自向量表
  （`0x7FFFFFFF`），照搬只会测到 fault 路径。
- 新增 **91** 条探针（总计 132）：

| 组 | 条数 | 覆盖 |
|---|---|---|
| LDRD / STRD | 10 | 立即数 / 后索引 / 前索引 W 位 / 寄存器偏移 |
| SWP / SWPB | 2 | 地址与数据按值、目的走 `u32*` |
| LDM / STM | 10 | IA/IB/DA/DB × 读/写 + 列表含 PC |
| 带移位的寄存器偏移 | 5 | `[r13,r1,lsl #2]`、`[r13,-r1]`、`asr #2` |
| LDR Rd=PC | 1 | 块以分支收尾 |
| 乘法族 | 12 | UMULL/SMULL/UMLAL/SMLAL、SMULxy/SMLAWy/SMLALxy、CLZ |
| 寄存器移位 | 4 | LSR/ASR/ROR by Rs、`orr ...,lsl r3` |
| PSR | 5 | MRS/MSR、`msr cpsr_c` 走模式切换 |
| 分支 / 异常 | 6 | B/BL/BX/BLX/SWI/BKPT |
| Thumb 访存 | 15 | 各宽度的立即数与寄存器偏移、PC 相对、PUSH/POP、LDM/STM |
| Thumb 其它 | 21 | B/BEQ/SWI/BKPT/BX/BLX、高寄存器形态、立即数 ALU |

全部编码用 `clang --target=armv5te` 汇编、`llvm-objdump` 反查过——上一轮 arm64 探针就有
两条编码写反（`ldrd/strd` 的 0xD/0xF 互换），断言对着错的指令"通过"。

**判定口径**：探针原先"任何差异都算失败"，而 sweep（`--full`）一直把周期差记成 bucket、
不进仲裁（§3.5）。本次让两者一致：以 `cycles:` 开头的差异记入记录行，其余
（R0–R14 / CPSR / `instruct_adr` / 异常 / 不变量 / 解释器可重复性）仍然判失败。

**结果**：

| 运行 | 规模 | 功能差异 | 周期记录 |
|---|---|---|---|
| 默认（探针） | 132 × 8 向量 × 2 标志 = 2112 | **0** | 176 |
| `--full` | 81920 | 0（各功能桶全 0） | 12067（与基线一致） |
| `--seq 2` | 6848 | 0（4012 跳过） | 28 |

176 条周期记录集中在三类：

| 类别 | 探针 | 典型值 |
|---|---|---|
| 异常 / 分支收尾 | SWI/BKPT、`LDR pc,[r13,#4]`、`LDMIA r13!,{r0,pc}`、`POP {r0,pc}` | interp=8，jit=2~4（`BKPT` 在 §10.12 改成内联后为 0） |
| 长乘法 | UMULL/SMULL/UMLAL/SMLAL | interp 随操作数 3~7，jit 固定 7~8 |
| 映射区字读 | `THUMB LDR r0,[r1,#4]`、`[r1,r2]` | interp=7，jit=3 |

第三类与 §10.8 记的那条 `LDR r0,[r1,#4]` 同源：`MMU_timing` 的顺序访问模型带历史状态，
两个引擎调用 helper 的次数不同，`m_lastAddress` 看到的不是同一条历史。前两类是 JIT 侧
有意的近似（异常/分支不累加流水线重填，长乘法按最坏情况算）。三者都不改体系结构状态，
属 §3.5 的既有边界——但这组数字第一次把它们的规模量化了。

**仍未覆盖**：arm64 自己还没跑同一套（需要 macOS，或 arm64 设备上能链解释器的
`jit_diff`）；`ldmda`/`stmda` 的写回值、`LDM ...^`（用户态 bank 传送）本轮没有进探针。

### 10.12 后端覆盖对齐：x64 唯一的 "已实现却不编译" 是 ARM `BKPT`（2026-11-21）

**动机**：`jit-opcode-matrix.md` 把 571 组标成 `diff-pass`，但"函数存在"不等于"函数会编译"。
`emit_armop_call()` 的规则是 `if (fc && fc(opcode)) return;` —— **函数返回 0 就当场退回解释器**，
比 `#define OP_X NULL` 更隐蔽：矩阵里看起来是已实现，运行时却是一句 `call`。

**审计方法**（纯静态，可重复）：

1. 抽两个后端的 `static int OP_*(const u32)` 定义集合 —— **各 571 个，逐名相同**；
2. 比对 `#define OP_* NULL` 空槽块 —— **都是 20 行，逐字相同**；
3. 对 571 个函数体做 `return 0` 匹配（含单行 `{ ... return 0; }` 这种 grep 行首会漏的写法）
   —— x64 命中 5 个，arm64 命中 4 个，差集只有 **`OP_BKPT`**。

**结论**：x64 的 `OP_BKPT` 是一句 `printf("JIT: unimplemented OP_BKPT")` + `return 0`，
arm64 则是完整的内联 abort 异常。其余 570 组的返回行为一致（`OP_MCR`/`OP_MRC` 的 ARM7 分支、
`LDRD/STRD` 的 Rd==14/奇数 Rd、带 `swi_tab` 的 `OP_SWI` 两边同样返回 0）。

**改法**（`arm_jit.cpp`，照着 arm64 与 `arm_instructions.cpp:OP_BKPT` 的次序写）：

| 步骤 | 说明 |
|---|---|
| 保存 CPSR | 进 ABT 前先读进临时寄存器（解释器的 `tmp`） |
| `armcpu_switchMode(cpu, ABT)` | 与 `op_swi()` 同一套调用约定；x64 的寄存器是即时落盘的，没有 flush 要做 |
| `R14_abt = bb_adr + 4` | `instruct_adr` 已由 `sync_r15()` 按 `JIT_BYPASS` 写成 `bb_adr` |
| `SPSR_abt = 旧 CPSR` | 必须在 switchMode 之后写（前面那步会把 `SPSR_abt` 覆盖进 `SPSR`） |
| CPSR 清 T、置 I | 与解释器一致；不调 `armcpu_changeCPSR()`（那只是 `NDS_Reschedule()`，两个后端都没调） |
| `R15 = next_instruction = intVector + 0x0C` | `BKPT` 不在 `instr_does_prefetch()` 里，块尾会把 `next_instruction` 复制进 `instruct_adr` |

**验证**：

| 项 | 改前 | 改后 |
|---|---|---|
| `jit_diff` 132 探针 × 8 向量 × 2 标志 = 2112 | 0 功能差异 | **0 功能差异**（ARM `BKPT #0` 与 `THUMB BKPT` 都在表内） |
| `ARM BKPT` 的周期记录 | `interp=8 jit=4`（回退时把解释器的 4 周期搬了回来） | `interp=8 jit=0`（内联后按两个后端的惯例不记异常周期） |
| 周期记录总数 | 176 | **176**（BKPT 本来就在记，只是数值变了） |
| `--full` | 81920 比较 / 12067 周期差 / 功能桶全 0 | **完全相同** |
| `JIT: unimplemented OP_BKPT` 打印 | 每次编译该槽位一次 | **0** |

**留下的一条已知差异**：BKPT 的周期。解释器 `return 4`，两个 JIT 的内联版本都记 0 —— 与
`SWI` 的处理一致（`INSTR_CYCLES_V` = 0，异常处理函数不碰 `bb_cycles`）。这条不影响体系结构
状态，按 §3.5 的边界不进仲裁；要修就得两个后端一起改，属性能/时序项，不在本轮范围。

**顺带确认的两个"看起来像回退、其实不是"**：`OP_MCR`/`OP_MRC` 对未分配的协处理器
`return 2` 且不发射任何代码 —— 解释器在这条路径上也只是 `INFO(...); return 2;`（什么都不做），
所以三个引擎等价；ARM7 上两者都返回 0 退回解释器，也是对称的。

### 10.13 主存快路径实验：把 helper 改短没有收益（2026-11-21，已回退）

**动机**：§10.6 判定 x64 不做 fastmem，但 §10.12 的审计表明"已实现"不等于"会编译"；
访存又是最高频的指令族。于是试了一条比 fastmem 保守得多的中间路线，先量清楚
"到底贵在哪"。

**设计**（两条，都不改发射代码）：

1. **专用 helper**：给 ARM9 的 LDR/LDRB/LDRH/LDRSB/LDRSH 与 STR/STRH/STRB 各加一个
   `*_hot` 版本，先用几条比较确认这是主存访问，再直接读 `MMU.MAIN_MEM`，跳过通用
   路径的调试钩子、断点扫描、DTCM 判定与两级查表；不满足条件就尾调用回通用版本。
2. **cache 命中常量 cycle**：ARM9 主存数据访问命中 cache 时 `memCycles` 恒为 1
   （`MMU_timing.h:_MMU_accesstime`），而 ARM9 的流水线模型取 `max(alu, mem)`
   （`MMU_aluMemCycles`），所以 load 恒返回 3、store 恒返回 2，cycle 模型整段可以跳过。
   为了不改 `FetchAccessUnit` 的私有状态，加了 `setLastAddress()` 与
   `CacheController::IsLastBlock()` 两个访问器（都是"把已有语义显式化"，不新增状态）。

guard 复刻的是通用路径进入该分支的全部前提：主存窗口、DTCM 未覆盖、断点表为空、
`advanced_timing` 为真、Lua/接口内存钩子未注册、以及"当前 1 KB 块就是 cache 控制器
上次看的那个"（`IsLastBlock`，即 `Cached()` 的第一条比较；从 tag 数组命中的另一条
路径会更新 `m_cacheCache`，必须走通用版本）。

**功能正确性（先证明，再谈性能）**：

| 检查 | 结果 |
|---|---|
| `jit_diff` 132 探针 × 8 向量 × 2 标志 | **0 功能差异**，176 条周期记录（与改前完全相同） |
| `--full` 81920 次比较 | 周期差 **12067**（与基线一字不差），功能桶全 0 |
| 快路径命中计数（临时插桩，`--full` 期间） | LDR **2560** 次、STR **320** 次 |
| 快路径的机器码 | 30 条、**无栈帧**（编译器把回落做成了尾跳转） |

也就是说"提前返回"与通用路径逐位等价，不是猜的。

**性能（新增 `--bench`，同一进程反复调用同一块）**：

| 形状（Thumb，块内 16 条） | 快路径 | 通用 | 结论 |
|---|---|---|---|
| `add r0,#0`（对照，不碰内存） | 0.616 ns | 0.615 ns | 两二进制只差在内存路径 |
| `ldr r0,[r1]`（100% 命中，最好情况） | **3.92 ns** | **3.49 ns** | **慢 12%**，6/6 轮稳定 |
| `ldr r0,[r1]`（块内 64 条） | 3.77~4.01 | 3.54~3.87 | 仍然不赢 |
| `str r0,[r1]`（纯写，最坏情况） | **4.27 ns** | **3.54 ns** | **慢 20%** |

**为什么**：

1. **写路径的快路径永远进不去**。`CachedInternal<WRITE>` 只在 `DIR == MMU_AD_READ`
   时分配 tag 并更新 `m_cacheCache`（`MMU_timing.h`），纯写序列里 `IsLastBlock()` 恒假
   —— 于是每次写都先付一遍 guard 再走通用路径。这是设计错误，不是调优问题。
2. **读路径即使在 100% 命中的最好情况下也不赢**。把 helper 从约 65 条指令砍到 30 条，
   墙钟时间反而升了 12%。对照 `add` 的 0.62 ns/条 说明块本身每条只要约 2 个周期，
   而一次访存要 11~12 个周期：**成本在"每条访存一次间接 call"上，不在 helper 本体**。
   helper 的指令数被乱序执行吃掉了，而 guard 的 6 个分支是纯新增。
3. 这个结论与 §10.6 并不矛盾，反而解释了它：x64 的访存热点不是"翻译太慢"，
   而是"每条指令一次调用"的固定成本。arm64 的 `call_mem`（§10.1）省的也是这个
   固定成本（调用前后少存几个寄存器），不是 helper 的指令数。

**回退**：两条路线都没有进仓库。保留的工具是 `jit_diff --bench`：

```
jit_diff.exe --thumb --bench 500000 --block 16 --slot 416
jit_diff.exe --arm   --bench 500000 --block 16 --slot 1424
```

它编译一个由 `n` 条同一指令组成的块，跑 `iters` 次编译后的函数，报 ns/块与 ns/条。
`--block` 量的是代码尺寸，看不见"只改 C helper"的改动；这个模式补上了那一半。

**下一步（如果还要做）**：唯一可能赢的是把"guard + 直接 load + `m_lastAddress` + 常量
cycle"内联进生成代码，也就是真 fastmem-lite —— 代价是每条访存多约 40 字节的块体积和
6 个分支，且必须放弃 `HAVE_LUA` 构建下的内存钩子。按上面 11~12 周期/次的基线，
乐观估计能省 5~7 个周期（约 40%），但要先解决块体积与钩子两条约束。
另一条更小的路（把 `bb_cpu`/`bb_cycles` 放进 callee-saved 寄存器）已试过，见 §10.14。

### 10.14 块内状态放进 callee-saved 寄存器：代码少 34%，速度不涨（2026-11-21，已回退）

**动机**：§10.13 的最后一条待办。每条模拟指令都要调一次 C helper，而 `bb_cpu`（CPU
结构体指针）和 `bb_total_cycles`（周期累加器）要跨调用存活。asmjit 给跨调用的虚拟
寄存器分配栈槽，于是每次调用前后各多一次 `mov [rsp+0x28], r9` / `mov r9, [rsp+0x28]`；
块内 10 条发射指令里有 2 条是这个。如果它们落到被调用者保存的寄存器里，helper 按 ABI
替我们保住，一次都不用存。

**第一次尝试：直接指定物理寄存器 —— 错两处，都值得记住**

1. `ctx->set_arg(0, bb_cpu)`。invoke 的参数必须是虚拟寄存器：asmjit 的分配器要重写它，
   拿到物理寄存器会算出一个越界的虚拟 id，`finalize()` 直接失败
   （`JIT error ... InvalidVirtId`）。ARM `BKPT` 是唯一把 `bb_cpu` 直接传给 invoke 的
   handler，因此只有它编译不出来——而差分台在块编译失败时会退回解释器，两个引擎于是
   "结果一致"，探针照样全绿。**块编译失败在探针里是静默的**，这是本轮最值得记住的一条。
2. 物理寄存器在 asmjit 里**没有存活区间**。把 `bb_cpu` 写成 `x86::r14` 之后，分配器
   会在块的中间把 r14 分给某个临时值。实测一个 8 条 `adds` 的块里出现
   `mov r14d, [rsp+0x3c]`，随后 `movzbl 0x53(%r14), %r14d` 拿被踩坏的指针去读 CPSR，
   进程崩在 `--bench` 里（`--block` 只执行一次所以看不出来）。

**第二次尝试：虚拟寄存器 + home-id 提示（正确做法）**

```cpp
static x86::Gp new_persistent_gp(uint32_t hint_reg_id)
{
	x86::Gp reg = c.new_gpz();
	c.virt_reg_by_reg(reg)->set_home_id_hint(hint_reg_id);	// r14 / r15 / r13 / r12
	return reg;
}
```

值仍是虚拟寄存器，存活区间归分配器管（拿不到就正常溢出，不会静默踩坏），只是"优先
放在这个物理寄存器"。`bin_pack` 会先满足带 hint 的 work-reg，所以实际都落在了
callee-saved 上，不再需要 `frame.add_dirty_regs()`。

**代码体积（`--block 16`，与本轮改动前对比）**

| 形状 | 改前 | 改后 | 变化 |
|---|---|---|---|
| Thumb `ldr r0,[r1]` | 704 B / 176 条 | **464 B / 116 条** | **−34%** |
| Thumb `str r0,[r0]` | 1576 B / 394 条 | **1216 B / 304 条** | **−23%** |
| Thumb `adds r0,#0` | 1037 B / 259 条 | 1013 B / 253 条 | −2% |

发射序列变成每次调用 5 条（原来是 7 条）：只少了 spill/reload，参数准备与 `call` 不变。
代价是序言/尾声多一对 `push r14` / `pop r14`（以及 r15）。

**速度（`--bench`，best-of-N，两个二进制交替跑）**

| 形状 | 块内指令数 | 改前 | 改后 | 变化 |
|---|---|---|---|---|
| Thumb `ldr` | 4 | 3.304 ns | 3.406 ns | +3.1% |
| Thumb `ldr` | 16 | 3.049 ns | 3.178 ns | +4.2% |
| Thumb `ldr` | 64 | 3.106 ns | 3.137 ns | +1.0% |
| Thumb `str` | 16 | 3.462 ns | 3.493 ns | +0.9% |
| Thumb `adds`（对照） | 16 | 0.611 ns | 0.605 ns | −1.0%（噪声） |

**为什么省了指令反而更慢**：省掉的是一次 store 加一次 load，它们和 helper 的 60 条
指令完全独立，乱序执行能把它们整个藏进调用的影子里——删掉它们不改变墙钟。而多出来的
`push`/`pop` 在"调用→块→返回"的关键路径上，每个块多约 4 个周期；16 条 `ldr` 的块
本身约 200 个周期，正好 2%。这与 §10.13 的结论是同一件事的两面：**x64 访存的成本在
"每条指令一次调用"这个固定开销上，块内发射的指令数（在这个量级上）不构成瓶颈。**

**结论**：代码体积 −23%~−34% 是真的，速度不涨反跌 1~4% 也是真的。这条路线**没有进
仓库**，工作区里只留下两件与它无关的工具改动（见下）。如果哪天代码体积本身成为目标
（x64 侧目前没有这个门槛，§10.5 的代码体积门槛只针对 arm64），把上面那个
`new_persistent_gp` 加回来、四个 `c.new_gpz()` 换成它即可，10 行以内。

**保留的工具改动**

- `jit_diff --opcode <hex>`：`--block`/`--bench` 原本只能按 `slot<<20`（Thumb 是
  `slot<<6`）合成指令，低 4 位有内容的编码根本造不出来——`bkpt` 就是其一
  （slot 0x127 合成出来是 `0x12700000`，另一条指令）。本轮就是靠它才看到 BKPT 的块
  其实编译失败。
- 探针的周期记录加 `fflush(stdout)`：不刷的情况下 stdout 是块缓冲、解释器的
  `stderr` 是无缓冲，重定向到管道后两类输出会粘在一起，同一份输出两次运行看起来不一样。

**顺带观察（未深究）**：hint 版本在探针里周期记录从 176 条降到 144 条，而 `--full`
81920 次比较的周期差 **12067 一条不差**。也就是说某些探针的周期值会随二进制的寄存器
分配变化。周期按 §3.5 不进仲裁，这里只记录现象。
