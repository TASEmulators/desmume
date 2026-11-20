# JIT 迁移工单表

> 由 `tools/jit_matrix/gen_opcode_matrix.py` 自动生成，**不要手改**。
> 重新生成：`python tools/jit_matrix/gen_opcode_matrix.py`

共 **591** 条指令（ARM 521 + Thumb 70），覆盖 5120 个分派槽位。

其中 **20** 条在两个后端都是 `#define OP_X NULL`：`emit_armop_call` 发现编译器为空就直接调解释器，**本来就不生成代码**，既不会分叉也不需要迁移。
真正要迁的是 **571** 条。

三方共用同一张分派表（`instruction_tabdef.inc` / `thumb_tabdef.inc`），所以「哪个槽由哪个 handler 负责」三方不存在分歧——分歧只可能在实现里。

**状态**：`todo` → `x86-done` → `a64-struct-done` → `diff-pass` → `perf-ok`；回退解释器的置 `interp-fallback`（无需处理）；有争议置 `disputed`。

## 进度

| 族 | ARM | Thumb | 合计 | 其中回退解释器 | 已 diff-pass |
|---|---|---|---|---|---|
| 数据处理 | 28 | 26 | 54 | 0 | 28 |
| 移位 | 224 | 10 | 234 | 0 | 0 |
| 访存 | 174 | 17 | 191 | 2 | 0 |
| LDM/STM | 32 | 6 | 38 | 0 | 0 |
| 乘法/乘加 | 28 | 1 | 29 | 0 | 0 |
| 协处理器 | 17 | 0 | 17 | 15 | 0 |
| 状态寄存器 | 6 | 0 | 6 | 0 | 0 |
| 饱和/计数 | 5 | 0 | 5 | 0 | 0 |
| 分支 | 5 | 8 | 13 | 1 | 0 |
| 异常/未定义 | 2 | 2 | 4 | 2 | 0 |
| **合计** | **521** | **70** | **591** | **20** | **28** |

## 回退解释器的指令（两后端一致）

这些 handler 在 `arm_jit.cpp` 和 `arm_jit_arm.cpp` 里都被 `#define` 成 `NULL`，编译时根本不进入 `compile_basicblock`。迁移期间它们**不需要改**，但如果某天有人给其中一个补上实现，就必须同时补两个后端，否则两个后端会在同一份游戏上表现不同。

| ISA | handler | 槽位 | 族 |
|---|---|---|---|
| ARM | `OP_STREX` | `0x189` | 访存 |
| ARM | `OP_LDREX` | `0x199` | 访存 |
| ARM | `OP_STC_OPTION` | `0xC00..0xC0F,0xC40..0xC4F,0xC80..0xC8F,0xCC0..0xCCF` | 协处理器 |
| ARM | `OP_LDC_OPTION` | `0xC10..0xC1F,0xC50..0xC5F,0xC90..0xC9F,0xCD0..0xCDF` | 协处理器 |
| ARM | `OP_STC_M_POSTIND` | `0xC20..0xC2F,0xC60..0xC6F` | 协处理器 |
| ARM | `OP_LDC_M_POSTIND` | `0xC30..0xC3F,0xC70..0xC7F` | 协处理器 |
| ARM | `OP_STC_P_POSTIND` | `0xCA0..0xCAF,0xCE0..0xCEF` | 协处理器 |
| ARM | `OP_LDC_P_POSTIND` | `0xCB0..0xCBF,0xCF0..0xCFF` | 协处理器 |
| ARM | `OP_STC_M_IMM_OFF` | `0xD00..0xD0F,0xD40..0xD4F` | 协处理器 |
| ARM | `OP_LDC_M_IMM_OFF` | `0xD10..0xD1F,0xD50..0xD5F` | 协处理器 |
| ARM | `OP_STC_M_PREIND` | `0xD20..0xD2F,0xD60..0xD6F` | 协处理器 |
| ARM | `OP_LDC_M_PREIND` | `0xD30..0xD3F,0xD70..0xD7F` | 协处理器 |
| ARM | `OP_STC_P_IMM_OFF` | `0xD80..0xD8F,0xDC0..0xDCF` | 协处理器 |
| ARM | `OP_LDC_P_IMM_OFF` | `0xD90..0xD9F,0xDD0..0xDDF` | 协处理器 |
| ARM | `OP_STC_P_PREIND` | `0xDA0..0xDAF,0xDE0..0xDEF` | 协处理器 |
| ARM | `OP_LDC_P_PREIND` | `0xDB0..0xDBF,0xDF0..0xDFF` | 协处理器 |
| ARM | `OP_CDP` | `0xE00,0xE02,0xE04,0xE06,0xE08,0xE0A,…（共 128 槽）` | 协处理器 |
| ARM | `OP_UND` | `0x02B,0x02D,0x02F,0x03B,0x03D,0x03F,…（共 348 槽）` | 异常/未定义 |
| Thumb | `OP_BKPT_THUMB` | `0x2F8..0x2FB` | 分支 |
| Thumb | `OP_UND_THUMB` | `0x2C4..0x2CF,0x2D8..0x2EF,0x2FC..0x2FF` | 异常/未定义 |

## ARM

### 数据处理（28）

| id | 槽位 | handler | 解释器 | x86 | x86 宏体 | arm64 | 状态 | 偏差 |
|---|---|---|---|---|---|---|---|---|
| A-0512 | `0x200..0x20F` | `OP_AND_IMM_VAL` | arm_instructions.cpp:340 | arm_jit.cpp:815 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1158 | diff-pass | |
| A-0528 | `0x210..0x21F` | `OP_AND_S_IMM_VAL` | arm_instructions.cpp:395 | arm_jit.cpp:876 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1219 | diff-pass | |
| A-0544 | `0x220..0x22F` | `OP_EOR_IMM_VAL` | arm_instructions.cpp:479 | arm_jit.cpp:825 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1168 | diff-pass | |
| A-0560 | `0x230..0x23F` | `OP_EOR_S_IMM_VAL` | arm_instructions.cpp:534 | arm_jit.cpp:886 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1229 | diff-pass | |
| A-0576 | `0x240..0x24F` | `OP_SUB_IMM_VAL` | arm_instructions.cpp:619 | arm_jit.cpp:855 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1198 | diff-pass | |
| A-0592 | `0x250..0x25F` | `OP_SUB_S_IMM_VAL` | arm_instructions.cpp:682 | arm_jit.cpp:916 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1259 | diff-pass | |
| A-0608 | `0x260..0x26F` | `OP_RSB_IMM_VAL` | arm_instructions.cpp:768 | arm_jit.cpp:865 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1208 | diff-pass | |
| A-0624 | `0x270..0x27F` | `OP_RSB_S_IMM_VAL` | arm_instructions.cpp:831 | arm_jit.cpp:926 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1269 | diff-pass | |
| A-0640 | `0x280..0x28F` | `OP_ADD_IMM_VAL` | arm_instructions.cpp:917 | arm_jit.cpp:845 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1188 | diff-pass | |
| A-0656 | `0x290..0x29F` | `OP_ADD_S_IMM_VAL` | arm_instructions.cpp:980 | arm_jit.cpp:906 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1249 | diff-pass | |
| A-0672 | `0x2A0..0x2AF` | `OP_ADC_IMM_VAL` | arm_instructions.cpp:1076 | arm_jit.cpp:936 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1279 | diff-pass | |
| A-0688 | `0x2B0..0x2BF` | `OP_ADC_S_IMM_VAL` | arm_instructions.cpp:1139 | arm_jit.cpp:946 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1289 | diff-pass | |
| A-0704 | `0x2C0..0x2CF` | `OP_SBC_IMM_VAL` | arm_instructions.cpp:1236 | arm_jit.cpp:956 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1299 | diff-pass | |
| A-0720 | `0x2D0..0x2DF` | `OP_SBC_S_IMM_VAL` | arm_instructions.cpp:1299 | arm_jit.cpp:966 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1309 | diff-pass | |
| A-0736 | `0x2E0..0x2EF` | `OP_RSC_IMM_VAL` | arm_instructions.cpp:1396 | arm_jit.cpp:976 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1319 | diff-pass | |
| A-0752 | `0x2F0..0x2FF` | `OP_RSC_S_IMM_VAL` | arm_instructions.cpp:1459 | arm_jit.cpp:986 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1329 | diff-pass | |
| A-0784 | `0x310..0x31F` | `OP_TST_IMM_VAL` | arm_instructions.cpp:1527 | arm_jit.cpp:1025 | `OP_TST_` | arm_jit_arm.cpp:1374 | diff-pass | |
| A-0816 | `0x330..0x33F` | `OP_TEQ_IMM_VAL` | arm_instructions.cpp:1594 | arm_jit.cpp:1051 | `OP_TEQ_` | arm_jit_arm.cpp:1396 | diff-pass | |
| A-0848 | `0x350..0x35F` | `OP_CMP_IMM_VAL` | arm_instructions.cpp:1662 | arm_jit.cpp:1070 | `OP_CMP` | arm_jit_arm.cpp:1418 | diff-pass | |
| A-0880 | `0x370..0x37F` | `OP_CMN_IMM_VAL` | arm_instructions.cpp:1730 | arm_jit.cpp:1099 | `OP_CMN` | arm_jit_arm.cpp:1451 | diff-pass | |
| A-0896 | `0x380..0x38F` | `OP_ORR_IMM_VAL` | arm_instructions.cpp:1816 | arm_jit.cpp:835 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1178 | diff-pass | |
| A-0912 | `0x390..0x39F` | `OP_ORR_S_IMM_VAL` | arm_instructions.cpp:1871 | arm_jit.cpp:896 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1239 | diff-pass | |
| A-0928 | `0x3A0..0x3AF` | `OP_MOV_IMM_VAL` | arm_instructions.cpp:1960 | arm_jit.cpp:1123 | `OP_MOV` | arm_jit_arm.cpp:1480 | diff-pass | |
| A-0944 | `0x3B0..0x3BF` | `OP_MOV_S_IMM_VAL` | arm_instructions.cpp:2017 | arm_jit.cpp:1148 | `OP_MOV_S` | arm_jit_arm.cpp:1514 | diff-pass | |
| A-0960 | `0x3C0..0x3CF` | `OP_BIC_IMM_VAL` | arm_instructions.cpp:2101 | arm_jit.cpp:996 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1339 | diff-pass | |
| A-0976 | `0x3D0..0x3DF` | `OP_BIC_S_IMM_VAL` | arm_instructions.cpp:2156 | arm_jit.cpp:1006 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1349 | diff-pass | |
| A-0992 | `0x3E0..0x3EF` | `OP_MVN_IMM_VAL` | arm_instructions.cpp:2240 | arm_jit.cpp:1161 | `OP_MOV` | arm_jit_arm.cpp:1527 | diff-pass | |
| A-1008 | `0x3F0..0x3FF` | `OP_MVN_S_IMM_VAL` | arm_instructions.cpp:2294 | arm_jit.cpp:1171 | `OP_MOV_S` | arm_jit_arm.cpp:1537 | diff-pass | |

### 移位（224）

| id | 槽位 | handler | 解释器 | x86 | x86 宏体 | arm64 | 状态 | 偏差 |
|---|---|---|---|---|---|---|---|---|
| A-0000 | `0x000,0x008` | `OP_AND_LSL_IMM` | arm_instructions.cpp:292 | arm_jit.cpp:807 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1150 | todo | |
| A-0001 | `0x001` | `OP_AND_LSL_REG` | arm_instructions.cpp:298 | arm_jit.cpp:808 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1151 | todo | |
| A-0002 | `0x002,0x00A` | `OP_AND_LSR_IMM` | arm_instructions.cpp:304 | arm_jit.cpp:809 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1152 | todo | |
| A-0003 | `0x003` | `OP_AND_LSR_REG` | arm_instructions.cpp:310 | arm_jit.cpp:810 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1153 | todo | |
| A-0004 | `0x004,0x00C` | `OP_AND_ASR_IMM` | arm_instructions.cpp:316 | arm_jit.cpp:811 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1154 | todo | |
| A-0005 | `0x005` | `OP_AND_ASR_REG` | arm_instructions.cpp:322 | arm_jit.cpp:812 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1155 | todo | |
| A-0006 | `0x006,0x00E` | `OP_AND_ROR_IMM` | arm_instructions.cpp:328 | arm_jit.cpp:813 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1156 | todo | |
| A-0007 | `0x007` | `OP_AND_ROR_REG` | arm_instructions.cpp:334 | arm_jit.cpp:814 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1157 | todo | |
| A-0016 | `0x010,0x018` | `OP_AND_S_LSL_IMM` | arm_instructions.cpp:347 | arm_jit.cpp:868 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1211 | todo | |
| A-0017 | `0x011` | `OP_AND_S_LSL_REG` | arm_instructions.cpp:353 | arm_jit.cpp:869 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1212 | todo | |
| A-0018 | `0x012,0x01A` | `OP_AND_S_LSR_IMM` | arm_instructions.cpp:359 | arm_jit.cpp:870 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1213 | todo | |
| A-0019 | `0x013` | `OP_AND_S_LSR_REG` | arm_instructions.cpp:365 | arm_jit.cpp:871 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1214 | todo | |
| A-0020 | `0x014,0x01C` | `OP_AND_S_ASR_IMM` | arm_instructions.cpp:371 | arm_jit.cpp:872 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1215 | todo | |
| A-0021 | `0x015` | `OP_AND_S_ASR_REG` | arm_instructions.cpp:377 | arm_jit.cpp:873 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1216 | todo | |
| A-0022 | `0x016,0x01E` | `OP_AND_S_ROR_IMM` | arm_instructions.cpp:383 | arm_jit.cpp:874 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1217 | todo | |
| A-0023 | `0x017` | `OP_AND_S_ROR_REG` | arm_instructions.cpp:389 | arm_jit.cpp:875 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1218 | todo | |
| A-0032 | `0x020,0x028` | `OP_EOR_LSL_IMM` | arm_instructions.cpp:431 | arm_jit.cpp:817 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1160 | todo | |
| A-0033 | `0x021` | `OP_EOR_LSL_REG` | arm_instructions.cpp:437 | arm_jit.cpp:818 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1161 | todo | |
| A-0034 | `0x022,0x02A` | `OP_EOR_LSR_IMM` | arm_instructions.cpp:443 | arm_jit.cpp:819 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1162 | todo | |
| A-0035 | `0x023` | `OP_EOR_LSR_REG` | arm_instructions.cpp:449 | arm_jit.cpp:820 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1163 | todo | |
| A-0036 | `0x024,0x02C` | `OP_EOR_ASR_IMM` | arm_instructions.cpp:455 | arm_jit.cpp:821 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1164 | todo | |
| A-0037 | `0x025` | `OP_EOR_ASR_REG` | arm_instructions.cpp:461 | arm_jit.cpp:822 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1165 | todo | |
| A-0038 | `0x026,0x02E` | `OP_EOR_ROR_IMM` | arm_instructions.cpp:467 | arm_jit.cpp:823 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1166 | todo | |
| A-0039 | `0x027` | `OP_EOR_ROR_REG` | arm_instructions.cpp:473 | arm_jit.cpp:824 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1167 | todo | |
| A-0048 | `0x030,0x038` | `OP_EOR_S_LSL_IMM` | arm_instructions.cpp:486 | arm_jit.cpp:878 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1221 | todo | |
| A-0049 | `0x031` | `OP_EOR_S_LSL_REG` | arm_instructions.cpp:492 | arm_jit.cpp:879 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1222 | todo | |
| A-0050 | `0x032,0x03A` | `OP_EOR_S_LSR_IMM` | arm_instructions.cpp:498 | arm_jit.cpp:880 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1223 | todo | |
| A-0051 | `0x033` | `OP_EOR_S_LSR_REG` | arm_instructions.cpp:504 | arm_jit.cpp:881 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1224 | todo | |
| A-0052 | `0x034,0x03C` | `OP_EOR_S_ASR_IMM` | arm_instructions.cpp:510 | arm_jit.cpp:882 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1225 | todo | |
| A-0053 | `0x035` | `OP_EOR_S_ASR_REG` | arm_instructions.cpp:516 | arm_jit.cpp:883 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1226 | todo | |
| A-0054 | `0x036,0x03E` | `OP_EOR_S_ROR_IMM` | arm_instructions.cpp:522 | arm_jit.cpp:884 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1227 | todo | |
| A-0055 | `0x037` | `OP_EOR_S_ROR_REG` | arm_instructions.cpp:528 | arm_jit.cpp:885 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1228 | todo | |
| A-0064 | `0x040,0x048` | `OP_SUB_LSL_IMM` | arm_instructions.cpp:571 | arm_jit.cpp:847 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1190 | todo | |
| A-0065 | `0x041` | `OP_SUB_LSL_REG` | arm_instructions.cpp:577 | arm_jit.cpp:848 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1191 | todo | |
| A-0066 | `0x042,0x04A` | `OP_SUB_LSR_IMM` | arm_instructions.cpp:583 | arm_jit.cpp:849 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1192 | todo | |
| A-0067 | `0x043` | `OP_SUB_LSR_REG` | arm_instructions.cpp:589 | arm_jit.cpp:850 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1193 | todo | |
| A-0068 | `0x044,0x04C` | `OP_SUB_ASR_IMM` | arm_instructions.cpp:595 | arm_jit.cpp:851 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1194 | todo | |
| A-0069 | `0x045` | `OP_SUB_ASR_REG` | arm_instructions.cpp:601 | arm_jit.cpp:852 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1195 | todo | |
| A-0070 | `0x046,0x04E` | `OP_SUB_ROR_IMM` | arm_instructions.cpp:607 | arm_jit.cpp:853 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1196 | todo | |
| A-0071 | `0x047` | `OP_SUB_ROR_REG` | arm_instructions.cpp:613 | arm_jit.cpp:854 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1197 | todo | |
| A-0080 | `0x050,0x058` | `OP_SUB_S_LSL_IMM` | arm_instructions.cpp:626 | arm_jit.cpp:908 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1251 | todo | |
| A-0081 | `0x051` | `OP_SUB_S_LSL_REG` | arm_instructions.cpp:633 | arm_jit.cpp:909 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1252 | todo | |
| A-0082 | `0x052,0x05A` | `OP_SUB_S_LSR_IMM` | arm_instructions.cpp:640 | arm_jit.cpp:910 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1253 | todo | |
| A-0083 | `0x053` | `OP_SUB_S_LSR_REG` | arm_instructions.cpp:647 | arm_jit.cpp:911 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1254 | todo | |
| A-0084 | `0x054,0x05C` | `OP_SUB_S_ASR_IMM` | arm_instructions.cpp:654 | arm_jit.cpp:912 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1255 | todo | |
| A-0085 | `0x055` | `OP_SUB_S_ASR_REG` | arm_instructions.cpp:661 | arm_jit.cpp:913 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1256 | todo | |
| A-0086 | `0x056,0x05E` | `OP_SUB_S_ROR_IMM` | arm_instructions.cpp:668 | arm_jit.cpp:914 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1257 | todo | |
| A-0087 | `0x057` | `OP_SUB_S_ROR_REG` | arm_instructions.cpp:675 | arm_jit.cpp:915 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1258 | todo | |
| A-0096 | `0x060,0x068` | `OP_RSB_LSL_IMM` | arm_instructions.cpp:720 | arm_jit.cpp:857 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1200 | todo | |
| A-0097 | `0x061` | `OP_RSB_LSL_REG` | arm_instructions.cpp:726 | arm_jit.cpp:858 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1201 | todo | |
| A-0098 | `0x062,0x06A` | `OP_RSB_LSR_IMM` | arm_instructions.cpp:732 | arm_jit.cpp:859 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1202 | todo | |
| A-0099 | `0x063` | `OP_RSB_LSR_REG` | arm_instructions.cpp:738 | arm_jit.cpp:860 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1203 | todo | |
| A-0100 | `0x064,0x06C` | `OP_RSB_ASR_IMM` | arm_instructions.cpp:744 | arm_jit.cpp:861 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1204 | todo | |
| A-0101 | `0x065` | `OP_RSB_ASR_REG` | arm_instructions.cpp:750 | arm_jit.cpp:862 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1205 | todo | |
| A-0102 | `0x066,0x06E` | `OP_RSB_ROR_IMM` | arm_instructions.cpp:756 | arm_jit.cpp:863 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1206 | todo | |
| A-0103 | `0x067` | `OP_RSB_ROR_REG` | arm_instructions.cpp:762 | arm_jit.cpp:864 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1207 | todo | |
| A-0112 | `0x070,0x078` | `OP_RSB_S_LSL_IMM` | arm_instructions.cpp:774 | arm_jit.cpp:918 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1261 | todo | |
| A-0113 | `0x071` | `OP_RSB_S_LSL_REG` | arm_instructions.cpp:782 | arm_jit.cpp:919 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1262 | todo | |
| A-0114 | `0x072,0x07A` | `OP_RSB_S_LSR_IMM` | arm_instructions.cpp:789 | arm_jit.cpp:920 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1263 | todo | |
| A-0115 | `0x073` | `OP_RSB_S_LSR_REG` | arm_instructions.cpp:796 | arm_jit.cpp:921 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1264 | todo | |
| A-0116 | `0x074,0x07C` | `OP_RSB_S_ASR_IMM` | arm_instructions.cpp:803 | arm_jit.cpp:922 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1265 | todo | |
| A-0117 | `0x075` | `OP_RSB_S_ASR_REG` | arm_instructions.cpp:810 | arm_jit.cpp:923 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1266 | todo | |
| A-0118 | `0x076,0x07E` | `OP_RSB_S_ROR_IMM` | arm_instructions.cpp:817 | arm_jit.cpp:924 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1267 | todo | |
| A-0119 | `0x077` | `OP_RSB_S_ROR_REG` | arm_instructions.cpp:824 | arm_jit.cpp:925 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1268 | todo | |
| A-0128 | `0x080,0x088` | `OP_ADD_LSL_IMM` | arm_instructions.cpp:869 | arm_jit.cpp:837 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1180 | todo | |
| A-0129 | `0x081` | `OP_ADD_LSL_REG` | arm_instructions.cpp:875 | arm_jit.cpp:838 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1181 | todo | |
| A-0130 | `0x082,0x08A` | `OP_ADD_LSR_IMM` | arm_instructions.cpp:881 | arm_jit.cpp:839 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1182 | todo | |
| A-0131 | `0x083` | `OP_ADD_LSR_REG` | arm_instructions.cpp:887 | arm_jit.cpp:840 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1183 | todo | |
| A-0132 | `0x084,0x08C` | `OP_ADD_ASR_IMM` | arm_instructions.cpp:893 | arm_jit.cpp:841 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1184 | todo | |
| A-0133 | `0x085` | `OP_ADD_ASR_REG` | arm_instructions.cpp:899 | arm_jit.cpp:842 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1185 | todo | |
| A-0134 | `0x086,0x08E` | `OP_ADD_ROR_IMM` | arm_instructions.cpp:905 | arm_jit.cpp:843 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1186 | todo | |
| A-0135 | `0x087` | `OP_ADD_ROR_REG` | arm_instructions.cpp:911 | arm_jit.cpp:844 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1187 | todo | |
| A-0144 | `0x090,0x098` | `OP_ADD_S_LSL_IMM` | arm_instructions.cpp:924 | arm_jit.cpp:898 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1241 | todo | |
| A-0145 | `0x091` | `OP_ADD_S_LSL_REG` | arm_instructions.cpp:931 | arm_jit.cpp:899 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1242 | todo | |
| A-0146 | `0x092,0x09A` | `OP_ADD_S_LSR_IMM` | arm_instructions.cpp:938 | arm_jit.cpp:900 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1243 | todo | |
| A-0147 | `0x093` | `OP_ADD_S_LSR_REG` | arm_instructions.cpp:945 | arm_jit.cpp:901 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1244 | todo | |
| A-0148 | `0x094,0x09C` | `OP_ADD_S_ASR_IMM` | arm_instructions.cpp:952 | arm_jit.cpp:902 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1245 | todo | |
| A-0149 | `0x095` | `OP_ADD_S_ASR_REG` | arm_instructions.cpp:959 | arm_jit.cpp:903 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1246 | todo | |
| A-0150 | `0x096,0x09E` | `OP_ADD_S_ROR_IMM` | arm_instructions.cpp:966 | arm_jit.cpp:904 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1247 | todo | |
| A-0151 | `0x097` | `OP_ADD_S_ROR_REG` | arm_instructions.cpp:973 | arm_jit.cpp:905 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1248 | todo | |
| A-0160 | `0x0A0,0x0A8` | `OP_ADC_LSL_IMM` | arm_instructions.cpp:1028 | arm_jit.cpp:928 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1271 | todo | |
| A-0161 | `0x0A1` | `OP_ADC_LSL_REG` | arm_instructions.cpp:1034 | arm_jit.cpp:929 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1272 | todo | |
| A-0162 | `0x0A2,0x0AA` | `OP_ADC_LSR_IMM` | arm_instructions.cpp:1040 | arm_jit.cpp:930 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1273 | todo | |
| A-0163 | `0x0A3` | `OP_ADC_LSR_REG` | arm_instructions.cpp:1046 | arm_jit.cpp:931 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1274 | todo | |
| A-0164 | `0x0A4,0x0AC` | `OP_ADC_ASR_IMM` | arm_instructions.cpp:1052 | arm_jit.cpp:932 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1275 | todo | |
| A-0165 | `0x0A5` | `OP_ADC_ASR_REG` | arm_instructions.cpp:1058 | arm_jit.cpp:933 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1276 | todo | |
| A-0166 | `0x0A6,0x0AE` | `OP_ADC_ROR_IMM` | arm_instructions.cpp:1064 | arm_jit.cpp:934 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1277 | todo | |
| A-0167 | `0x0A7` | `OP_ADC_ROR_REG` | arm_instructions.cpp:1070 | arm_jit.cpp:935 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1278 | todo | |
| A-0176 | `0x0B0,0x0B8` | `OP_ADC_S_LSL_IMM` | arm_instructions.cpp:1083 | arm_jit.cpp:938 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1281 | todo | |
| A-0177 | `0x0B1` | `OP_ADC_S_LSL_REG` | arm_instructions.cpp:1090 | arm_jit.cpp:939 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1282 | todo | |
| A-0178 | `0x0B2,0x0BA` | `OP_ADC_S_LSR_IMM` | arm_instructions.cpp:1097 | arm_jit.cpp:940 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1283 | todo | |
| A-0179 | `0x0B3` | `OP_ADC_S_LSR_REG` | arm_instructions.cpp:1104 | arm_jit.cpp:941 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1284 | todo | |
| A-0180 | `0x0B4,0x0BC` | `OP_ADC_S_ASR_IMM` | arm_instructions.cpp:1111 | arm_jit.cpp:942 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1285 | todo | |
| A-0181 | `0x0B5` | `OP_ADC_S_ASR_REG` | arm_instructions.cpp:1118 | arm_jit.cpp:943 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1286 | todo | |
| A-0182 | `0x0B6,0x0BE` | `OP_ADC_S_ROR_IMM` | arm_instructions.cpp:1125 | arm_jit.cpp:944 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1287 | todo | |
| A-0183 | `0x0B7` | `OP_ADC_S_ROR_REG` | arm_instructions.cpp:1132 | arm_jit.cpp:945 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1288 | todo | |
| A-0192 | `0x0C0,0x0C8` | `OP_SBC_LSL_IMM` | arm_instructions.cpp:1188 | arm_jit.cpp:948 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1291 | todo | |
| A-0193 | `0x0C1` | `OP_SBC_LSL_REG` | arm_instructions.cpp:1194 | arm_jit.cpp:949 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1292 | todo | |
| A-0194 | `0x0C2,0x0CA` | `OP_SBC_LSR_IMM` | arm_instructions.cpp:1200 | arm_jit.cpp:950 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1293 | todo | |
| A-0195 | `0x0C3` | `OP_SBC_LSR_REG` | arm_instructions.cpp:1206 | arm_jit.cpp:951 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1294 | todo | |
| A-0196 | `0x0C4,0x0CC` | `OP_SBC_ASR_IMM` | arm_instructions.cpp:1212 | arm_jit.cpp:952 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1295 | todo | |
| A-0197 | `0x0C5` | `OP_SBC_ASR_REG` | arm_instructions.cpp:1218 | arm_jit.cpp:953 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1296 | todo | |
| A-0198 | `0x0C6,0x0CE` | `OP_SBC_ROR_IMM` | arm_instructions.cpp:1224 | arm_jit.cpp:954 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1297 | todo | |
| A-0199 | `0x0C7` | `OP_SBC_ROR_REG` | arm_instructions.cpp:1230 | arm_jit.cpp:955 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1298 | todo | |
| A-0208 | `0x0D0,0x0D8` | `OP_SBC_S_LSL_IMM` | arm_instructions.cpp:1243 | arm_jit.cpp:958 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1301 | todo | |
| A-0209 | `0x0D1` | `OP_SBC_S_LSL_REG` | arm_instructions.cpp:1250 | arm_jit.cpp:959 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1302 | todo | |
| A-0210 | `0x0D2,0x0DA` | `OP_SBC_S_LSR_IMM` | arm_instructions.cpp:1257 | arm_jit.cpp:960 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1303 | todo | |
| A-0211 | `0x0D3` | `OP_SBC_S_LSR_REG` | arm_instructions.cpp:1264 | arm_jit.cpp:961 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1304 | todo | |
| A-0212 | `0x0D4,0x0DC` | `OP_SBC_S_ASR_IMM` | arm_instructions.cpp:1271 | arm_jit.cpp:962 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1305 | todo | |
| A-0213 | `0x0D5` | `OP_SBC_S_ASR_REG` | arm_instructions.cpp:1278 | arm_jit.cpp:963 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1306 | todo | |
| A-0214 | `0x0D6,0x0DE` | `OP_SBC_S_ROR_IMM` | arm_instructions.cpp:1285 | arm_jit.cpp:964 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1307 | todo | |
| A-0215 | `0x0D7` | `OP_SBC_S_ROR_REG` | arm_instructions.cpp:1292 | arm_jit.cpp:965 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1308 | todo | |
| A-0224 | `0x0E0,0x0E8` | `OP_RSC_LSL_IMM` | arm_instructions.cpp:1348 | arm_jit.cpp:968 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1311 | todo | |
| A-0225 | `0x0E1` | `OP_RSC_LSL_REG` | arm_instructions.cpp:1354 | arm_jit.cpp:969 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1312 | todo | |
| A-0226 | `0x0E2,0x0EA` | `OP_RSC_LSR_IMM` | arm_instructions.cpp:1360 | arm_jit.cpp:970 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1313 | todo | |
| A-0227 | `0x0E3` | `OP_RSC_LSR_REG` | arm_instructions.cpp:1366 | arm_jit.cpp:971 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1314 | todo | |
| A-0228 | `0x0E4,0x0EC` | `OP_RSC_ASR_IMM` | arm_instructions.cpp:1372 | arm_jit.cpp:972 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1315 | todo | |
| A-0229 | `0x0E5` | `OP_RSC_ASR_REG` | arm_instructions.cpp:1378 | arm_jit.cpp:973 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1316 | todo | |
| A-0230 | `0x0E6,0x0EE` | `OP_RSC_ROR_IMM` | arm_instructions.cpp:1384 | arm_jit.cpp:974 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1317 | todo | |
| A-0231 | `0x0E7` | `OP_RSC_ROR_REG` | arm_instructions.cpp:1390 | arm_jit.cpp:975 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1318 | todo | |
| A-0240 | `0x0F0,0x0F8` | `OP_RSC_S_LSL_IMM` | arm_instructions.cpp:1403 | arm_jit.cpp:978 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1321 | todo | |
| A-0241 | `0x0F1` | `OP_RSC_S_LSL_REG` | arm_instructions.cpp:1410 | arm_jit.cpp:979 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1322 | todo | |
| A-0242 | `0x0F2,0x0FA` | `OP_RSC_S_LSR_IMM` | arm_instructions.cpp:1417 | arm_jit.cpp:980 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1323 | todo | |
| A-0243 | `0x0F3` | `OP_RSC_S_LSR_REG` | arm_instructions.cpp:1424 | arm_jit.cpp:981 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1324 | todo | |
| A-0244 | `0x0F4,0x0FC` | `OP_RSC_S_ASR_IMM` | arm_instructions.cpp:1431 | arm_jit.cpp:982 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1325 | todo | |
| A-0245 | `0x0F5` | `OP_RSC_S_ASR_REG` | arm_instructions.cpp:1438 | arm_jit.cpp:983 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1326 | todo | |
| A-0246 | `0x0F6,0x0FE` | `OP_RSC_S_ROR_IMM` | arm_instructions.cpp:1445 | arm_jit.cpp:984 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1327 | todo | |
| A-0247 | `0x0F7` | `OP_RSC_S_ROR_REG` | arm_instructions.cpp:1452 | arm_jit.cpp:985 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1328 | todo | |
| A-0272 | `0x110,0x118` | `OP_TST_LSL_IMM` | arm_instructions.cpp:1479 | arm_jit.cpp:1017 | `OP_TST_` | arm_jit_arm.cpp:1366 | todo | |
| A-0273 | `0x111` | `OP_TST_LSL_REG` | arm_instructions.cpp:1485 | arm_jit.cpp:1018 | `OP_TST_` | arm_jit_arm.cpp:1367 | todo | |
| A-0274 | `0x112,0x11A` | `OP_TST_LSR_IMM` | arm_instructions.cpp:1491 | arm_jit.cpp:1019 | `OP_TST_` | arm_jit_arm.cpp:1368 | todo | |
| A-0275 | `0x113` | `OP_TST_LSR_REG` | arm_instructions.cpp:1497 | arm_jit.cpp:1020 | `OP_TST_` | arm_jit_arm.cpp:1369 | todo | |
| A-0276 | `0x114,0x11C` | `OP_TST_ASR_IMM` | arm_instructions.cpp:1503 | arm_jit.cpp:1021 | `OP_TST_` | arm_jit_arm.cpp:1370 | todo | |
| A-0277 | `0x115` | `OP_TST_ASR_REG` | arm_instructions.cpp:1509 | arm_jit.cpp:1022 | `OP_TST_` | arm_jit_arm.cpp:1371 | todo | |
| A-0278 | `0x116,0x11E` | `OP_TST_ROR_IMM` | arm_instructions.cpp:1515 | arm_jit.cpp:1023 | `OP_TST_` | arm_jit_arm.cpp:1372 | todo | |
| A-0279 | `0x117` | `OP_TST_ROR_REG` | arm_instructions.cpp:1521 | arm_jit.cpp:1024 | `OP_TST_` | arm_jit_arm.cpp:1373 | todo | |
| A-0304 | `0x130,0x138` | `OP_TEQ_LSL_IMM` | arm_instructions.cpp:1546 | arm_jit.cpp:1043 | `OP_TEQ_` | arm_jit_arm.cpp:1388 | todo | |
| A-0305 | `0x131` | `OP_TEQ_LSL_REG` | arm_instructions.cpp:1552 | arm_jit.cpp:1044 | `OP_TEQ_` | arm_jit_arm.cpp:1389 | todo | |
| A-0306 | `0x132,0x13A` | `OP_TEQ_LSR_IMM` | arm_instructions.cpp:1558 | arm_jit.cpp:1045 | `OP_TEQ_` | arm_jit_arm.cpp:1390 | todo | |
| A-0307 | `0x133` | `OP_TEQ_LSR_REG` | arm_instructions.cpp:1564 | arm_jit.cpp:1046 | `OP_TEQ_` | arm_jit_arm.cpp:1391 | todo | |
| A-0308 | `0x134,0x13C` | `OP_TEQ_ASR_IMM` | arm_instructions.cpp:1570 | arm_jit.cpp:1047 | `OP_TEQ_` | arm_jit_arm.cpp:1392 | todo | |
| A-0309 | `0x135` | `OP_TEQ_ASR_REG` | arm_instructions.cpp:1576 | arm_jit.cpp:1048 | `OP_TEQ_` | arm_jit_arm.cpp:1393 | todo | |
| A-0310 | `0x136,0x13E` | `OP_TEQ_ROR_IMM` | arm_instructions.cpp:1582 | arm_jit.cpp:1049 | `OP_TEQ_` | arm_jit_arm.cpp:1394 | todo | |
| A-0311 | `0x137` | `OP_TEQ_ROR_REG` | arm_instructions.cpp:1588 | arm_jit.cpp:1050 | `OP_TEQ_` | arm_jit_arm.cpp:1395 | todo | |
| A-0336 | `0x150,0x158` | `OP_CMP_LSL_IMM` | arm_instructions.cpp:1614 | arm_jit.cpp:1062 | `OP_CMP` | arm_jit_arm.cpp:1410 | todo | |
| A-0337 | `0x151` | `OP_CMP_LSL_REG` | arm_instructions.cpp:1620 | arm_jit.cpp:1063 | `OP_CMP` | arm_jit_arm.cpp:1411 | todo | |
| A-0338 | `0x152,0x15A` | `OP_CMP_LSR_IMM` | arm_instructions.cpp:1626 | arm_jit.cpp:1064 | `OP_CMP` | arm_jit_arm.cpp:1412 | todo | |
| A-0339 | `0x153` | `OP_CMP_LSR_REG` | arm_instructions.cpp:1632 | arm_jit.cpp:1065 | `OP_CMP` | arm_jit_arm.cpp:1413 | todo | |
| A-0340 | `0x154,0x15C` | `OP_CMP_ASR_IMM` | arm_instructions.cpp:1638 | arm_jit.cpp:1066 | `OP_CMP` | arm_jit_arm.cpp:1414 | todo | |
| A-0341 | `0x155` | `OP_CMP_ASR_REG` | arm_instructions.cpp:1644 | arm_jit.cpp:1067 | `OP_CMP` | arm_jit_arm.cpp:1415 | todo | |
| A-0342 | `0x156,0x15E` | `OP_CMP_ROR_IMM` | arm_instructions.cpp:1650 | arm_jit.cpp:1068 | `OP_CMP` | arm_jit_arm.cpp:1416 | todo | |
| A-0343 | `0x157` | `OP_CMP_ROR_REG` | arm_instructions.cpp:1656 | arm_jit.cpp:1069 | `OP_CMP` | arm_jit_arm.cpp:1417 | todo | |
| A-0368 | `0x170,0x178` | `OP_CMN_LSL_IMM` | arm_instructions.cpp:1682 | arm_jit.cpp:1091 | `OP_CMN` | arm_jit_arm.cpp:1443 | todo | |
| A-0369 | `0x171` | `OP_CMN_LSL_REG` | arm_instructions.cpp:1688 | arm_jit.cpp:1092 | `OP_CMN` | arm_jit_arm.cpp:1444 | todo | |
| A-0370 | `0x172,0x17A` | `OP_CMN_LSR_IMM` | arm_instructions.cpp:1694 | arm_jit.cpp:1093 | `OP_CMN` | arm_jit_arm.cpp:1445 | todo | |
| A-0371 | `0x173` | `OP_CMN_LSR_REG` | arm_instructions.cpp:1700 | arm_jit.cpp:1094 | `OP_CMN` | arm_jit_arm.cpp:1446 | todo | |
| A-0372 | `0x174,0x17C` | `OP_CMN_ASR_IMM` | arm_instructions.cpp:1706 | arm_jit.cpp:1095 | `OP_CMN` | arm_jit_arm.cpp:1447 | todo | |
| A-0373 | `0x175` | `OP_CMN_ASR_REG` | arm_instructions.cpp:1712 | arm_jit.cpp:1096 | `OP_CMN` | arm_jit_arm.cpp:1448 | todo | |
| A-0374 | `0x176,0x17E` | `OP_CMN_ROR_IMM` | arm_instructions.cpp:1718 | arm_jit.cpp:1097 | `OP_CMN` | arm_jit_arm.cpp:1449 | todo | |
| A-0375 | `0x177` | `OP_CMN_ROR_REG` | arm_instructions.cpp:1724 | arm_jit.cpp:1098 | `OP_CMN` | arm_jit_arm.cpp:1450 | todo | |
| A-0384 | `0x180,0x188` | `OP_ORR_LSL_IMM` | arm_instructions.cpp:1768 | arm_jit.cpp:827 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1170 | todo | |
| A-0385 | `0x181` | `OP_ORR_LSL_REG` | arm_instructions.cpp:1774 | arm_jit.cpp:828 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1171 | todo | |
| A-0386 | `0x182,0x18A` | `OP_ORR_LSR_IMM` | arm_instructions.cpp:1780 | arm_jit.cpp:829 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1172 | todo | |
| A-0387 | `0x183` | `OP_ORR_LSR_REG` | arm_instructions.cpp:1786 | arm_jit.cpp:830 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1173 | todo | |
| A-0388 | `0x184,0x18C` | `OP_ORR_ASR_IMM` | arm_instructions.cpp:1792 | arm_jit.cpp:831 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1174 | todo | |
| A-0389 | `0x185` | `OP_ORR_ASR_REG` | arm_instructions.cpp:1798 | arm_jit.cpp:832 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1175 | todo | |
| A-0390 | `0x186,0x18E` | `OP_ORR_ROR_IMM` | arm_instructions.cpp:1804 | arm_jit.cpp:833 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1176 | todo | |
| A-0391 | `0x187` | `OP_ORR_ROR_REG` | arm_instructions.cpp:1810 | arm_jit.cpp:834 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1177 | todo | |
| A-0400 | `0x190,0x198` | `OP_ORR_S_LSL_IMM` | arm_instructions.cpp:1823 | arm_jit.cpp:888 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1231 | todo | |
| A-0401 | `0x191` | `OP_ORR_S_LSL_REG` | arm_instructions.cpp:1829 | arm_jit.cpp:889 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1232 | todo | |
| A-0402 | `0x192,0x19A` | `OP_ORR_S_LSR_IMM` | arm_instructions.cpp:1835 | arm_jit.cpp:890 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1233 | todo | |
| A-0403 | `0x193` | `OP_ORR_S_LSR_REG` | arm_instructions.cpp:1841 | arm_jit.cpp:891 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1234 | todo | |
| A-0404 | `0x194,0x19C` | `OP_ORR_S_ASR_IMM` | arm_instructions.cpp:1847 | arm_jit.cpp:892 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1235 | todo | |
| A-0405 | `0x195` | `OP_ORR_S_ASR_REG` | arm_instructions.cpp:1853 | arm_jit.cpp:893 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1236 | todo | |
| A-0406 | `0x196,0x19E` | `OP_ORR_S_ROR_IMM` | arm_instructions.cpp:1859 | arm_jit.cpp:894 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1237 | todo | |
| A-0407 | `0x197` | `OP_ORR_S_ROR_REG` | arm_instructions.cpp:1865 | arm_jit.cpp:895 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1238 | todo | |
| A-0416 | `0x1A0,0x1A8` | `OP_MOV_LSL_IMM` | arm_instructions.cpp:1907 | arm_jit.cpp:1115 | — | arm_jit_arm.cpp:1472 | todo | |
| A-0417 | `0x1A1` | `OP_MOV_LSL_REG` | arm_instructions.cpp:1916 | arm_jit.cpp:1116 | `OP_MOV` | arm_jit_arm.cpp:1473 | todo | |
| A-0418 | `0x1A2,0x1AA` | `OP_MOV_LSR_IMM` | arm_instructions.cpp:1923 | arm_jit.cpp:1117 | `OP_MOV` | arm_jit_arm.cpp:1474 | todo | |
| A-0419 | `0x1A3` | `OP_MOV_LSR_REG` | arm_instructions.cpp:1929 | arm_jit.cpp:1118 | `OP_MOV` | arm_jit_arm.cpp:1475 | todo | |
| A-0420 | `0x1A4,0x1AC` | `OP_MOV_ASR_IMM` | arm_instructions.cpp:1936 | arm_jit.cpp:1119 | `OP_MOV` | arm_jit_arm.cpp:1476 | todo | |
| A-0421 | `0x1A5` | `OP_MOV_ASR_REG` | arm_instructions.cpp:1942 | arm_jit.cpp:1120 | `OP_MOV` | arm_jit_arm.cpp:1477 | todo | |
| A-0422 | `0x1A6,0x1AE` | `OP_MOV_ROR_IMM` | arm_instructions.cpp:1948 | arm_jit.cpp:1121 | `OP_MOV` | arm_jit_arm.cpp:1478 | todo | |
| A-0423 | `0x1A7` | `OP_MOV_ROR_REG` | arm_instructions.cpp:1954 | arm_jit.cpp:1122 | `OP_MOV` | arm_jit_arm.cpp:1479 | todo | |
| A-0432 | `0x1B0,0x1B8` | `OP_MOV_S_LSL_IMM` | arm_instructions.cpp:1967 | arm_jit.cpp:1140 | `OP_MOV_S` | arm_jit_arm.cpp:1506 | todo | |
| A-0433 | `0x1B1` | `OP_MOV_S_LSL_REG` | arm_instructions.cpp:1973 | arm_jit.cpp:1141 | `OP_MOV_S` | arm_jit_arm.cpp:1507 | todo | |
| A-0434 | `0x1B2,0x1BA` | `OP_MOV_S_LSR_IMM` | arm_instructions.cpp:1980 | arm_jit.cpp:1142 | `OP_MOV_S` | arm_jit_arm.cpp:1508 | todo | |
| A-0435 | `0x1B3` | `OP_MOV_S_LSR_REG` | arm_instructions.cpp:1986 | arm_jit.cpp:1143 | `OP_MOV_S` | arm_jit_arm.cpp:1509 | todo | |
| A-0436 | `0x1B4,0x1BC` | `OP_MOV_S_ASR_IMM` | arm_instructions.cpp:1993 | arm_jit.cpp:1144 | `OP_MOV_S` | arm_jit_arm.cpp:1510 | todo | |
| A-0437 | `0x1B5` | `OP_MOV_S_ASR_REG` | arm_instructions.cpp:1999 | arm_jit.cpp:1145 | `OP_MOV_S` | arm_jit_arm.cpp:1511 | todo | |
| A-0438 | `0x1B6,0x1BE` | `OP_MOV_S_ROR_IMM` | arm_instructions.cpp:2005 | arm_jit.cpp:1146 | `OP_MOV_S` | arm_jit_arm.cpp:1512 | todo | |
| A-0439 | `0x1B7` | `OP_MOV_S_ROR_REG` | arm_instructions.cpp:2011 | arm_jit.cpp:1147 | `OP_MOV_S` | arm_jit_arm.cpp:1513 | todo | |
| A-0448 | `0x1C0,0x1C8` | `OP_BIC_LSL_IMM` | arm_instructions.cpp:2053 | arm_jit.cpp:988 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1331 | todo | |
| A-0449 | `0x1C1` | `OP_BIC_LSL_REG` | arm_instructions.cpp:2059 | arm_jit.cpp:989 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1332 | todo | |
| A-0450 | `0x1C2,0x1CA` | `OP_BIC_LSR_IMM` | arm_instructions.cpp:2065 | arm_jit.cpp:990 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1333 | todo | |
| A-0451 | `0x1C3` | `OP_BIC_LSR_REG` | arm_instructions.cpp:2071 | arm_jit.cpp:991 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1334 | todo | |
| A-0452 | `0x1C4,0x1CC` | `OP_BIC_ASR_IMM` | arm_instructions.cpp:2077 | arm_jit.cpp:992 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1335 | todo | |
| A-0453 | `0x1C5` | `OP_BIC_ASR_REG` | arm_instructions.cpp:2083 | arm_jit.cpp:993 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1336 | todo | |
| A-0454 | `0x1C6,0x1CE` | `OP_BIC_ROR_IMM` | arm_instructions.cpp:2089 | arm_jit.cpp:994 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1337 | todo | |
| A-0455 | `0x1C7` | `OP_BIC_ROR_REG` | arm_instructions.cpp:2095 | arm_jit.cpp:995 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1338 | todo | |
| A-0464 | `0x1D0,0x1D8` | `OP_BIC_S_LSL_IMM` | arm_instructions.cpp:2108 | arm_jit.cpp:998 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1341 | todo | |
| A-0465 | `0x1D1` | `OP_BIC_S_LSL_REG` | arm_instructions.cpp:2114 | arm_jit.cpp:999 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1342 | todo | |
| A-0466 | `0x1D2,0x1DA` | `OP_BIC_S_LSR_IMM` | arm_instructions.cpp:2120 | arm_jit.cpp:1000 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1343 | todo | |
| A-0467 | `0x1D3` | `OP_BIC_S_LSR_REG` | arm_instructions.cpp:2126 | arm_jit.cpp:1001 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1344 | todo | |
| A-0468 | `0x1D4,0x1DC` | `OP_BIC_S_ASR_IMM` | arm_instructions.cpp:2132 | arm_jit.cpp:1002 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1345 | todo | |
| A-0469 | `0x1D5` | `OP_BIC_S_ASR_REG` | arm_instructions.cpp:2138 | arm_jit.cpp:1003 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1346 | todo | |
| A-0470 | `0x1D6,0x1DE` | `OP_BIC_S_ROR_IMM` | arm_instructions.cpp:2144 | arm_jit.cpp:1004 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1347 | todo | |
| A-0471 | `0x1D7` | `OP_BIC_S_ROR_REG` | arm_instructions.cpp:2150 | arm_jit.cpp:1005 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1348 | todo | |
| A-0480 | `0x1E0,0x1E8` | `OP_MVN_LSL_IMM` | arm_instructions.cpp:2192 | arm_jit.cpp:1153 | `OP_MOV` | arm_jit_arm.cpp:1519 | todo | |
| A-0481 | `0x1E1` | `OP_MVN_LSL_REG` | arm_instructions.cpp:2198 | arm_jit.cpp:1154 | `OP_MOV` | arm_jit_arm.cpp:1520 | todo | |
| A-0482 | `0x1E2,0x1EA` | `OP_MVN_LSR_IMM` | arm_instructions.cpp:2204 | arm_jit.cpp:1155 | `OP_MOV` | arm_jit_arm.cpp:1521 | todo | |
| A-0483 | `0x1E3` | `OP_MVN_LSR_REG` | arm_instructions.cpp:2210 | arm_jit.cpp:1156 | `OP_MOV` | arm_jit_arm.cpp:1522 | todo | |
| A-0484 | `0x1E4,0x1EC` | `OP_MVN_ASR_IMM` | arm_instructions.cpp:2216 | arm_jit.cpp:1157 | `OP_MOV` | arm_jit_arm.cpp:1523 | todo | |
| A-0485 | `0x1E5` | `OP_MVN_ASR_REG` | arm_instructions.cpp:2222 | arm_jit.cpp:1158 | `OP_MOV` | arm_jit_arm.cpp:1524 | todo | |
| A-0486 | `0x1E6,0x1EE` | `OP_MVN_ROR_IMM` | arm_instructions.cpp:2228 | arm_jit.cpp:1159 | `OP_MOV` | arm_jit_arm.cpp:1525 | todo | |
| A-0487 | `0x1E7` | `OP_MVN_ROR_REG` | arm_instructions.cpp:2234 | arm_jit.cpp:1160 | `OP_MOV` | arm_jit_arm.cpp:1526 | todo | |
| A-0496 | `0x1F0,0x1F8` | `OP_MVN_S_LSL_IMM` | arm_instructions.cpp:2246 | arm_jit.cpp:1163 | `OP_MOV_S` | arm_jit_arm.cpp:1529 | todo | |
| A-0497 | `0x1F1` | `OP_MVN_S_LSL_REG` | arm_instructions.cpp:2252 | arm_jit.cpp:1164 | `OP_MOV_S` | arm_jit_arm.cpp:1530 | todo | |
| A-0498 | `0x1F2,0x1FA` | `OP_MVN_S_LSR_IMM` | arm_instructions.cpp:2258 | arm_jit.cpp:1165 | `OP_MOV_S` | arm_jit_arm.cpp:1531 | todo | |
| A-0499 | `0x1F3` | `OP_MVN_S_LSR_REG` | arm_instructions.cpp:2264 | arm_jit.cpp:1166 | `OP_MOV_S` | arm_jit_arm.cpp:1532 | todo | |
| A-0500 | `0x1F4,0x1FC` | `OP_MVN_S_ASR_IMM` | arm_instructions.cpp:2270 | arm_jit.cpp:1167 | `OP_MOV_S` | arm_jit_arm.cpp:1533 | todo | |
| A-0501 | `0x1F5` | `OP_MVN_S_ASR_REG` | arm_instructions.cpp:2276 | arm_jit.cpp:1168 | `OP_MOV_S` | arm_jit_arm.cpp:1534 | todo | |
| A-0502 | `0x1F6,0x1FE` | `OP_MVN_S_ROR_IMM` | arm_instructions.cpp:2282 | arm_jit.cpp:1169 | `OP_MOV_S` | arm_jit_arm.cpp:1535 | todo | |
| A-0503 | `0x1F7` | `OP_MVN_S_ROR_REG` | arm_instructions.cpp:2288 | arm_jit.cpp:1170 | `OP_MOV_S` | arm_jit_arm.cpp:1536 | todo | |

### 访存（174）

| id | 槽位 | handler | 解释器 | x86 | x86 宏体 | arm64 | 状态 | 偏差 |
|---|---|---|---|---|---|---|---|---|
| A-0011 | `0x00B` | `OP_STRH_POS_INDE_M_REG_OFF` | arm_instructions.cpp:2739 | arm_jit.cpp:1851 | `OP_STR_` | arm_jit_arm.cpp:2187 | todo | |
| A-0013 | `0x00D,0x00F,0x04D,0x04F,0x08D,0x08F,…（共 8 槽）` | `OP_LDRD_STRD_POST_INDEX` | arm_instructions.cpp:6008 | arm_jit.cpp:1922 | — | arm_jit_arm.cpp:2261 | todo | |
| A-0027 | `0x01B` | `OP_LDRH_POS_INDE_M_REG_OFF` | arm_instructions.cpp:2631 | arm_jit.cpp:1680 | `OP_LDR_` | arm_jit_arm.cpp:2014 | todo | |
| A-0029 | `0x01D` | `OP_LDRSB_POS_INDE_M_REG_OFF` | arm_instructions.cpp:2957 | arm_jit.cpp:1743 | `OP_LDR_` | arm_jit_arm.cpp:2077 | todo | |
| A-0031 | `0x01F` | `OP_LDRSH_POS_INDE_M_REG_OFF` | arm_instructions.cpp:2848 | arm_jit.cpp:1695 | `OP_LDR_` | arm_jit_arm.cpp:2029 | todo | |
| A-0075 | `0x04B` | `OP_STRH_POS_INDE_M_IMM_OFF` | arm_instructions.cpp:2721 | arm_jit.cpp:1849 | `OP_STR_` | arm_jit_arm.cpp:2185 | todo | |
| A-0091 | `0x05B` | `OP_LDRH_POS_INDE_M_IMM_OFF` | arm_instructions.cpp:2613 | arm_jit.cpp:1678 | `OP_LDR_` | arm_jit_arm.cpp:2012 | todo | |
| A-0093 | `0x05D` | `OP_LDRSB_POS_INDE_M_IMM_OFF` | arm_instructions.cpp:2939 | arm_jit.cpp:1741 | `OP_LDR_` | arm_jit_arm.cpp:2075 | todo | |
| A-0095 | `0x05F` | `OP_LDRSH_POS_INDE_M_IMM_OFF` | arm_instructions.cpp:2830 | arm_jit.cpp:1693 | `OP_LDR_` | arm_jit_arm.cpp:2027 | todo | |
| A-0139 | `0x08B` | `OP_STRH_POS_INDE_P_REG_OFF` | arm_instructions.cpp:2730 | arm_jit.cpp:1850 | `OP_STR_` | arm_jit_arm.cpp:2186 | todo | |
| A-0155 | `0x09B` | `OP_LDRH_POS_INDE_P_REG_OFF` | arm_instructions.cpp:2622 | arm_jit.cpp:1679 | `OP_LDR_` | arm_jit_arm.cpp:2013 | todo | |
| A-0157 | `0x09D` | `OP_LDRSB_POS_INDE_P_REG_OFF` | arm_instructions.cpp:2948 | arm_jit.cpp:1742 | `OP_LDR_` | arm_jit_arm.cpp:2076 | todo | |
| A-0159 | `0x09F` | `OP_LDRSH_POS_INDE_P_REG_OFF` | arm_instructions.cpp:2839 | arm_jit.cpp:1694 | `OP_LDR_` | arm_jit_arm.cpp:2028 | todo | |
| A-0203 | `0x0CB` | `OP_STRH_POS_INDE_P_IMM_OFF` | arm_instructions.cpp:2712 | arm_jit.cpp:1848 | `OP_STR_` | arm_jit_arm.cpp:2184 | todo | |
| A-0219 | `0x0DB` | `OP_LDRH_POS_INDE_P_IMM_OFF` | arm_instructions.cpp:2604 | arm_jit.cpp:1677 | `OP_LDR_` | arm_jit_arm.cpp:2011 | todo | |
| A-0221 | `0x0DD` | `OP_LDRSB_POS_INDE_P_IMM_OFF` | arm_instructions.cpp:2930 | arm_jit.cpp:1740 | `OP_LDR_` | arm_jit_arm.cpp:2074 | todo | |
| A-0223 | `0x0DF` | `OP_LDRSH_POS_INDE_P_IMM_OFF` | arm_instructions.cpp:2821 | arm_jit.cpp:1692 | `OP_LDR_` | arm_jit_arm.cpp:2026 | todo | |
| A-0265 | `0x109` | `OP_SWP` | arm_instructions.cpp:2506 | arm_jit.cpp:2059 | — | arm_jit_arm.cpp:2446 | todo | |
| A-0267 | `0x10B` | `OP_STRH_M_REG_OFF` | arm_instructions.cpp:2668 | arm_jit.cpp:1842 | `OP_STR_` | arm_jit_arm.cpp:2178 | todo | |
| A-0269 | `0x10D,0x10F,0x12D,0x12F,0x14D,0x14F,…（共 16 槽）` | `OP_LDRD_STRD_OFFSET_PRE_INDEX` | arm_instructions.cpp:6049 | arm_jit.cpp:1963 | — | arm_jit_arm.cpp:2314 | todo | |
| A-0283 | `0x11B` | `OP_LDRH_M_REG_OFF` | arm_instructions.cpp:2559 | arm_jit.cpp:1671 | `OP_LDR_` | arm_jit_arm.cpp:2005 | todo | |
| A-0285 | `0x11D` | `OP_LDRSB_M_REG_OFF` | arm_instructions.cpp:2885 | arm_jit.cpp:1734 | `OP_LDR_` | arm_jit_arm.cpp:2068 | todo | |
| A-0287 | `0x11F` | `OP_LDRSH_M_REG_OFF` | arm_instructions.cpp:2776 | arm_jit.cpp:1686 | `OP_LDR_` | arm_jit_arm.cpp:2020 | todo | |
| A-0299 | `0x12B` | `OP_STRH_PRE_INDE_M_REG_OFF` | arm_instructions.cpp:2703 | arm_jit.cpp:1847 | `OP_STR_` | arm_jit_arm.cpp:2183 | todo | |
| A-0315 | `0x13B` | `OP_LDRH_PRE_INDE_M_REG_OFF` | arm_instructions.cpp:2595 | arm_jit.cpp:1676 | `OP_LDR_` | arm_jit_arm.cpp:2010 | todo | |
| A-0317 | `0x13D` | `OP_LDRSB_PRE_INDE_M_REG_OFF` | arm_instructions.cpp:2921 | arm_jit.cpp:1739 | `OP_LDR_` | arm_jit_arm.cpp:2073 | todo | |
| A-0319 | `0x13F` | `OP_LDRSH_PRE_INDE_M_REG_OFF` | arm_instructions.cpp:2812 | arm_jit.cpp:1691 | `OP_LDR_` | arm_jit_arm.cpp:2025 | todo | |
| A-0329 | `0x149` | `OP_SWPB` | arm_instructions.cpp:2519 | arm_jit.cpp:2060 | — | arm_jit_arm.cpp:2447 | todo | |
| A-0331 | `0x14B` | `OP_STRH_M_IMM_OFF` | arm_instructions.cpp:2652 | arm_jit.cpp:1840 | `OP_STR_` | arm_jit_arm.cpp:2176 | todo | |
| A-0347 | `0x15B` | `OP_LDRH_M_IMM_OFF` | arm_instructions.cpp:2543 | arm_jit.cpp:1669 | `OP_LDR_` | arm_jit_arm.cpp:2003 | todo | |
| A-0349 | `0x15D` | `OP_LDRSB_M_IMM_OFF` | arm_instructions.cpp:2869 | arm_jit.cpp:1732 | `OP_LDR_` | arm_jit_arm.cpp:2066 | todo | |
| A-0351 | `0x15F` | `OP_LDRSH_M_IMM_OFF` | arm_instructions.cpp:2760 | arm_jit.cpp:1684 | `OP_LDR_` | arm_jit_arm.cpp:2018 | todo | |
| A-0363 | `0x16B` | `OP_STRH_PRE_INDE_M_IMM_OFF` | arm_instructions.cpp:2685 | arm_jit.cpp:1845 | `OP_STR_` | arm_jit_arm.cpp:2181 | todo | |
| A-0379 | `0x17B` | `OP_LDRH_PRE_INDE_M_IMM_OFF` | arm_instructions.cpp:2576 | arm_jit.cpp:1674 | `OP_LDR_` | arm_jit_arm.cpp:2008 | todo | |
| A-0381 | `0x17D` | `OP_LDRSB_PRE_INDE_M_IMM_OFF` | arm_instructions.cpp:2902 | arm_jit.cpp:1737 | `OP_LDR_` | arm_jit_arm.cpp:2071 | todo | |
| A-0383 | `0x17F` | `OP_LDRSH_PRE_INDE_M_IMM_OFF` | arm_instructions.cpp:2794 | arm_jit.cpp:1689 | `OP_LDR_` | arm_jit_arm.cpp:2023 | todo | |
| A-0393 | `0x189` | `OP_STREX` | arm_instructions.cpp:4380 | `NULL` | — | `NULL` | interp-fallback | |
| A-0395 | `0x18B` | `OP_STRH_P_REG_OFF` | arm_instructions.cpp:2660 | arm_jit.cpp:1841 | `OP_STR_` | arm_jit_arm.cpp:2177 | todo | |
| A-0409 | `0x199` | `OP_LDREX` | arm_instructions.cpp:3793 | `NULL` | — | `NULL` | interp-fallback | |
| A-0411 | `0x19B` | `OP_LDRH_P_REG_OFF` | arm_instructions.cpp:2551 | arm_jit.cpp:1670 | `OP_LDR_` | arm_jit_arm.cpp:2004 | todo | |
| A-0413 | `0x19D` | `OP_LDRSB_P_REG_OFF` | arm_instructions.cpp:2877 | arm_jit.cpp:1733 | `OP_LDR_` | arm_jit_arm.cpp:2067 | todo | |
| A-0415 | `0x19F` | `OP_LDRSH_P_REG_OFF` | arm_instructions.cpp:2768 | arm_jit.cpp:1685 | `OP_LDR_` | arm_jit_arm.cpp:2019 | todo | |
| A-0427 | `0x1AB` | `OP_STRH_PRE_INDE_P_REG_OFF` | arm_instructions.cpp:2694 | arm_jit.cpp:1846 | `OP_STR_` | arm_jit_arm.cpp:2182 | todo | |
| A-0443 | `0x1BB` | `OP_LDRH_PRE_INDE_P_REG_OFF` | arm_instructions.cpp:2586 | arm_jit.cpp:1675 | `OP_LDR_` | arm_jit_arm.cpp:2009 | todo | |
| A-0445 | `0x1BD` | `OP_LDRSB_PRE_INDE_P_REG_OFF` | arm_instructions.cpp:2911 | arm_jit.cpp:1738 | `OP_LDR_` | arm_jit_arm.cpp:2072 | todo | |
| A-0447 | `0x1BF` | `OP_LDRSH_PRE_INDE_P_REG_OFF` | arm_instructions.cpp:2803 | arm_jit.cpp:1690 | `OP_LDR_` | arm_jit_arm.cpp:2024 | todo | |
| A-0459 | `0x1CB` | `OP_STRH_P_IMM_OFF` | arm_instructions.cpp:2644 | arm_jit.cpp:1839 | `OP_STR_` | arm_jit_arm.cpp:2175 | todo | |
| A-0475 | `0x1DB` | `OP_LDRH_P_IMM_OFF` | arm_instructions.cpp:2535 | arm_jit.cpp:1668 | `OP_LDR_` | arm_jit_arm.cpp:2002 | todo | |
| A-0477 | `0x1DD` | `OP_LDRSB_P_IMM_OFF` | arm_instructions.cpp:2861 | arm_jit.cpp:1731 | `OP_LDR_` | arm_jit_arm.cpp:2065 | todo | |
| A-0479 | `0x1DF` | `OP_LDRSH_P_IMM_OFF` | arm_instructions.cpp:2752 | arm_jit.cpp:1683 | `OP_LDR_` | arm_jit_arm.cpp:2017 | todo | |
| A-0491 | `0x1EB` | `OP_STRH_PRE_INDE_P_IMM_OFF` | arm_instructions.cpp:2676 | arm_jit.cpp:1844 | `OP_STR_` | arm_jit_arm.cpp:2180 | todo | |
| A-0507 | `0x1FB` | `OP_LDRH_PRE_INDE_P_IMM_OFF` | arm_instructions.cpp:2567 | arm_jit.cpp:1673 | `OP_LDR_` | arm_jit_arm.cpp:2007 | todo | |
| A-0509 | `0x1FD` | `OP_LDRSB_PRE_INDE_P_IMM_OFF` | arm_instructions.cpp:2893 | arm_jit.cpp:1736 | `OP_LDR_` | arm_jit_arm.cpp:2070 | todo | |
| A-0511 | `0x1FF` | `OP_LDRSH_PRE_INDE_P_IMM_OFF` | arm_instructions.cpp:2784 | arm_jit.cpp:1688 | `OP_LDR_` | arm_jit_arm.cpp:2022 | todo | |
| A-1024 | `0x400..0x40F,0x420..0x42F` | `OP_STR_M_IMM_OFF_POSTIND` | arm_instructions.cpp:4288 | arm_jit.cpp:1829 | `OP_STR_` | arm_jit_arm.cpp:2165 | todo | |
| A-1040 | `0x410..0x41F,0x430..0x43F` | `OP_LDR_M_IMM_OFF_POSTIND` | arm_instructions.cpp:3737 | arm_jit.cpp:1657 | `OP_LDR_` | arm_jit_arm.cpp:1991 | todo | |
| A-1088 | `0x440..0x44F,0x460..0x46F` | `OP_STRB_M_IMM_OFF_POSTIND` | arm_instructions.cpp:4589 | arm_jit.cpp:1875 | `OP_STR_` | arm_jit_arm.cpp:2211 | todo | |
| A-1104 | `0x450..0x45F,0x470..0x47F` | `OP_LDRB_M_IMM_OFF_POSTIND` | arm_instructions.cpp:4000 | arm_jit.cpp:1720 | `OP_LDR_` | arm_jit_arm.cpp:2054 | todo | |
| A-1152 | `0x480..0x48F,0x4A0..0x4AF` | `OP_STR_P_IMM_OFF_POSTIND` | arm_instructions.cpp:4279 | arm_jit.cpp:1828 | `OP_STR_` | arm_jit_arm.cpp:2164 | todo | |
| A-1168 | `0x490..0x49F,0x4B0..0x4BF` | `OP_LDR_P_IMM_OFF_POSTIND` | arm_instructions.cpp:3732 | arm_jit.cpp:1656 | `OP_LDR_` | arm_jit_arm.cpp:1990 | todo | |
| A-1216 | `0x4C0..0x4CF,0x4E0..0x4EF` | `OP_STRB_P_IMM_OFF_POSTIND` | arm_instructions.cpp:4580 | arm_jit.cpp:1874 | `OP_STR_` | arm_jit_arm.cpp:2210 | todo | |
| A-1232 | `0x4D0..0x4DF,0x4F0..0x4FF` | `OP_LDRB_P_IMM_OFF_POSTIND` | arm_instructions.cpp:3991 | arm_jit.cpp:1719 | `OP_LDR_` | arm_jit_arm.cpp:2053 | todo | |
| A-1280 | `0x500..0x50F` | `OP_STR_M_IMM_OFF` | arm_instructions.cpp:4101 | arm_jit.cpp:1808 | `OP_STR_` | arm_jit_arm.cpp:2144 | todo | |
| A-1296 | `0x510..0x51F` | `OP_LDR_M_IMM_OFF` | arm_instructions.cpp:3602 | arm_jit.cpp:1636 | `OP_LDR_` | arm_jit_arm.cpp:1970 | todo | |
| A-1312 | `0x520..0x52F` | `OP_STR_M_IMM_OFF_PREIND` | arm_instructions.cpp:4190 | arm_jit.cpp:1819 | `OP_STR_` | arm_jit_arm.cpp:2155 | todo | |
| A-1328 | `0x530..0x53F` | `OP_LDR_M_IMM_OFF_PREIND` | arm_instructions.cpp:3670 | arm_jit.cpp:1647 | `OP_LDR_` | arm_jit_arm.cpp:1981 | todo | |
| A-1344 | `0x540..0x54F` | `OP_STRB_M_IMM_OFF` | arm_instructions.cpp:4402 | arm_jit.cpp:1854 | `OP_STR_` | arm_jit_arm.cpp:2190 | todo | |
| A-1360 | `0x550..0x55F` | `OP_LDRB_M_IMM_OFF` | arm_instructions.cpp:3813 | arm_jit.cpp:1699 | `OP_LDR_` | arm_jit_arm.cpp:2033 | todo | |
| A-1376 | `0x560..0x56F` | `OP_STRB_M_IMM_OFF_PREIND` | arm_instructions.cpp:4491 | arm_jit.cpp:1865 | `OP_STR_` | arm_jit_arm.cpp:2201 | todo | |
| A-1392 | `0x570..0x57F` | `OP_LDRB_M_IMM_OFF_PREIND` | arm_instructions.cpp:3902 | arm_jit.cpp:1710 | `OP_LDR_` | arm_jit_arm.cpp:2044 | todo | |
| A-1408 | `0x580..0x58F` | `OP_STR_P_IMM_OFF` | arm_instructions.cpp:4093 | arm_jit.cpp:1807 | `OP_STR_` | arm_jit_arm.cpp:2143 | todo | |
| A-1424 | `0x590..0x59F` | `OP_LDR_P_IMM_OFF` | arm_instructions.cpp:3596 | arm_jit.cpp:1635 | `OP_LDR_` | arm_jit_arm.cpp:1969 | todo | |
| A-1440 | `0x5A0..0x5AF` | `OP_STR_P_IMM_OFF_PREIND` | arm_instructions.cpp:4181 | arm_jit.cpp:1818 | `OP_STR_` | arm_jit_arm.cpp:2154 | todo | |
| A-1456 | `0x5B0..0x5BF` | `OP_LDR_P_IMM_OFF_PREIND` | arm_instructions.cpp:3664 | arm_jit.cpp:1646 | `OP_LDR_` | arm_jit_arm.cpp:1980 | todo | |
| A-1472 | `0x5C0..0x5CF` | `OP_STRB_P_IMM_OFF` | arm_instructions.cpp:4394 | arm_jit.cpp:1853 | `OP_STR_` | arm_jit_arm.cpp:2189 | todo | |
| A-1488 | `0x5D0..0x5DF` | `OP_LDRB_P_IMM_OFF` | arm_instructions.cpp:3805 | arm_jit.cpp:1698 | `OP_LDR_` | arm_jit_arm.cpp:2032 | todo | |
| A-1504 | `0x5E0..0x5EF` | `OP_STRB_P_IMM_OFF_PREIND` | arm_instructions.cpp:4482 | arm_jit.cpp:1864 | `OP_STR_` | arm_jit_arm.cpp:2200 | todo | |
| A-1520 | `0x5F0..0x5FF` | `OP_LDRB_P_IMM_OFF_PREIND` | arm_instructions.cpp:3893 | arm_jit.cpp:1709 | `OP_LDR_` | arm_jit_arm.cpp:2043 | todo | |
| A-1536 | `0x600,0x608,0x620,0x628` | `OP_STR_M_LSL_IMM_OFF_POSTIND` | arm_instructions.cpp:4307 | arm_jit.cpp:1831 | `OP_STR_` | arm_jit_arm.cpp:2167 | todo | |
| A-1538 | `0x602,0x60A,0x622,0x62A` | `OP_STR_M_LSR_IMM_OFF_POSTIND` | arm_instructions.cpp:4327 | arm_jit.cpp:1833 | `OP_STR_` | arm_jit_arm.cpp:2169 | todo | |
| A-1540 | `0x604,0x60C,0x624,0x62C` | `OP_STR_M_ASR_IMM_OFF_POSTIND` | arm_instructions.cpp:4347 | arm_jit.cpp:1835 | `OP_STR_` | arm_jit_arm.cpp:2171 | todo | |
| A-1542 | `0x606,0x60E,0x626,0x62E` | `OP_STR_M_ROR_IMM_OFF_POSTIND` | arm_instructions.cpp:4367 | arm_jit.cpp:1837 | `OP_STR_` | arm_jit_arm.cpp:2173 | todo | |
| A-1552 | `0x610,0x618,0x630,0x638` | `OP_LDR_M_LSL_IMM_OFF_POSTIND` | arm_instructions.cpp:3748 | arm_jit.cpp:1659 | `OP_LDR_` | arm_jit_arm.cpp:1993 | todo | |
| A-1554 | `0x612,0x61A,0x632,0x63A` | `OP_LDR_M_LSR_IMM_OFF_POSTIND` | arm_instructions.cpp:3760 | arm_jit.cpp:1661 | `OP_LDR_` | arm_jit_arm.cpp:1995 | todo | |
| A-1556 | `0x614,0x61C,0x634,0x63C` | `OP_LDR_M_ASR_IMM_OFF_POSTIND` | arm_instructions.cpp:3772 | arm_jit.cpp:1663 | `OP_LDR_` | arm_jit_arm.cpp:1997 | todo | |
| A-1558 | `0x616,0x61E,0x636,0x63E` | `OP_LDR_M_ROR_IMM_OFF_POSTIND` | arm_instructions.cpp:3784 | arm_jit.cpp:1665 | `OP_LDR_` | arm_jit_arm.cpp:1999 | todo | |
| A-1600 | `0x640,0x648,0x660,0x668` | `OP_STRB_M_LSL_IMM_OFF_POSTIND` | arm_instructions.cpp:4608 | arm_jit.cpp:1877 | `OP_STR_` | arm_jit_arm.cpp:2213 | todo | |
| A-1602 | `0x642,0x64A,0x662,0x66A` | `OP_STRB_M_LSR_IMM_OFF_POSTIND` | arm_instructions.cpp:4628 | arm_jit.cpp:1879 | `OP_STR_` | arm_jit_arm.cpp:2215 | todo | |
| A-1604 | `0x644,0x64C,0x664,0x66C` | `OP_STRB_M_ASR_IMM_OFF_POSTIND` | arm_instructions.cpp:4648 | arm_jit.cpp:1881 | `OP_STR_` | arm_jit_arm.cpp:2217 | todo | |
| A-1606 | `0x646,0x64E,0x666,0x66E` | `OP_STRB_M_ROR_IMM_OFF_POSTIND` | arm_instructions.cpp:4668 | arm_jit.cpp:1883 | `OP_STR_` | arm_jit_arm.cpp:2219 | todo | |
| A-1616 | `0x650,0x658,0x670,0x678` | `OP_LDRB_M_LSL_IMM_OFF_POSTIND` | arm_instructions.cpp:4019 | arm_jit.cpp:1722 | `OP_LDR_` | arm_jit_arm.cpp:2056 | todo | |
| A-1618 | `0x652,0x65A,0x672,0x67A` | `OP_LDRB_M_LSR_IMM_OFF_POSTIND` | arm_instructions.cpp:4039 | arm_jit.cpp:1724 | `OP_LDR_` | arm_jit_arm.cpp:2058 | todo | |
| A-1620 | `0x654,0x65C,0x674,0x67C` | `OP_LDRB_M_ASR_IMM_OFF_POSTIND` | arm_instructions.cpp:4059 | arm_jit.cpp:1726 | `OP_LDR_` | arm_jit_arm.cpp:2060 | todo | |
| A-1622 | `0x656,0x65E,0x676,0x67E` | `OP_LDRB_M_ROR_IMM_OFF_POSTIND` | arm_instructions.cpp:4079 | arm_jit.cpp:1728 | `OP_LDR_` | arm_jit_arm.cpp:2062 | todo | |
| A-1664 | `0x680,0x688,0x6A0,0x6A8` | `OP_STR_P_LSL_IMM_OFF_POSTIND` | arm_instructions.cpp:4297 | arm_jit.cpp:1830 | `OP_STR_` | arm_jit_arm.cpp:2166 | todo | |
| A-1666 | `0x682,0x68A,0x6A2,0x6AA` | `OP_STR_P_LSR_IMM_OFF_POSTIND` | arm_instructions.cpp:4317 | arm_jit.cpp:1832 | `OP_STR_` | arm_jit_arm.cpp:2168 | todo | |
| A-1668 | `0x684,0x68C,0x6A4,0x6AC` | `OP_STR_P_ASR_IMM_OFF_POSTIND` | arm_instructions.cpp:4337 | arm_jit.cpp:1834 | `OP_STR_` | arm_jit_arm.cpp:2170 | todo | |
| A-1670 | `0x686,0x68E,0x6A6,0x6AE` | `OP_STR_P_ROR_IMM_OFF_POSTIND` | arm_instructions.cpp:4357 | arm_jit.cpp:1836 | `OP_STR_` | arm_jit_arm.cpp:2172 | todo | |
| A-1680 | `0x690,0x698,0x6B0,0x6B8` | `OP_LDR_P_LSL_IMM_OFF_POSTIND` | arm_instructions.cpp:3742 | arm_jit.cpp:1658 | `OP_LDR_` | arm_jit_arm.cpp:1992 | todo | |
| A-1682 | `0x692,0x69A,0x6B2,0x6BA` | `OP_LDR_P_LSR_IMM_OFF_POSTIND` | arm_instructions.cpp:3754 | arm_jit.cpp:1660 | `OP_LDR_` | arm_jit_arm.cpp:1994 | todo | |
| A-1684 | `0x694,0x69C,0x6B4,0x6BC` | `OP_LDR_P_ASR_IMM_OFF_POSTIND` | arm_instructions.cpp:3766 | arm_jit.cpp:1662 | `OP_LDR_` | arm_jit_arm.cpp:1996 | todo | |
| A-1686 | `0x696,0x69E,0x6B6,0x6BE` | `OP_LDR_P_ROR_IMM_OFF_POSTIND` | arm_instructions.cpp:3778 | arm_jit.cpp:1664 | `OP_LDR_` | arm_jit_arm.cpp:1998 | todo | |
| A-1728 | `0x6C0,0x6C8,0x6E0,0x6E8` | `OP_STRB_P_LSL_IMM_OFF_POSTIND` | arm_instructions.cpp:4598 | arm_jit.cpp:1876 | `OP_STR_` | arm_jit_arm.cpp:2212 | todo | |
| A-1730 | `0x6C2,0x6CA,0x6E2,0x6EA` | `OP_STRB_P_LSR_IMM_OFF_POSTIND` | arm_instructions.cpp:4618 | arm_jit.cpp:1878 | `OP_STR_` | arm_jit_arm.cpp:2214 | todo | |
| A-1732 | `0x6C4,0x6CC,0x6E4,0x6EC` | `OP_STRB_P_ASR_IMM_OFF_POSTIND` | arm_instructions.cpp:4638 | arm_jit.cpp:1880 | `OP_STR_` | arm_jit_arm.cpp:2216 | todo | |
| A-1734 | `0x6C6,0x6CE,0x6E6,0x6EE` | `OP_STRB_P_ROR_IMM_OFF_POSTIND` | arm_instructions.cpp:4658 | arm_jit.cpp:1882 | `OP_STR_` | arm_jit_arm.cpp:2218 | todo | |
| A-1744 | `0x6D0,0x6D8,0x6F0,0x6F8` | `OP_LDRB_P_LSL_IMM_OFF_POSTIND` | arm_instructions.cpp:4009 | arm_jit.cpp:1721 | `OP_LDR_` | arm_jit_arm.cpp:2055 | todo | |
| A-1746 | `0x6D2,0x6DA,0x6F2,0x6FA` | `OP_LDRB_P_LSR_IMM_OFF_POSTIND` | arm_instructions.cpp:4029 | arm_jit.cpp:1723 | `OP_LDR_` | arm_jit_arm.cpp:2057 | todo | |
| A-1748 | `0x6D4,0x6DC,0x6F4,0x6FC` | `OP_LDRB_P_ASR_IMM_OFF_POSTIND` | arm_instructions.cpp:4049 | arm_jit.cpp:1725 | `OP_LDR_` | arm_jit_arm.cpp:2059 | todo | |
| A-1750 | `0x6D6,0x6DE,0x6F6,0x6FE` | `OP_LDRB_P_ROR_IMM_OFF_POSTIND` | arm_instructions.cpp:4069 | arm_jit.cpp:1727 | `OP_LDR_` | arm_jit_arm.cpp:2061 | todo | |
| A-1792 | `0x700,0x708` | `OP_STR_M_LSL_IMM_OFF` | arm_instructions.cpp:4118 | arm_jit.cpp:1810 | `OP_STR_` | arm_jit_arm.cpp:2146 | todo | |
| A-1794 | `0x702,0x70A` | `OP_STR_M_LSR_IMM_OFF` | arm_instructions.cpp:4136 | arm_jit.cpp:1812 | `OP_STR_` | arm_jit_arm.cpp:2148 | todo | |
| A-1796 | `0x704,0x70C` | `OP_STR_M_ASR_IMM_OFF` | arm_instructions.cpp:4154 | arm_jit.cpp:1814 | `OP_STR_` | arm_jit_arm.cpp:2150 | todo | |
| A-1798 | `0x706,0x70E` | `OP_STR_M_ROR_IMM_OFF` | arm_instructions.cpp:4172 | arm_jit.cpp:1816 | `OP_STR_` | arm_jit_arm.cpp:2152 | todo | |
| A-1808 | `0x710,0x718` | `OP_LDR_M_LSL_IMM_OFF` | arm_instructions.cpp:3615 | arm_jit.cpp:1638 | `OP_LDR_` | arm_jit_arm.cpp:1972 | todo | |
| A-1810 | `0x712,0x71A` | `OP_LDR_M_LSR_IMM_OFF` | arm_instructions.cpp:3629 | arm_jit.cpp:1640 | `OP_LDR_` | arm_jit_arm.cpp:1974 | todo | |
| A-1812 | `0x714,0x71C` | `OP_LDR_M_ASR_IMM_OFF` | arm_instructions.cpp:3643 | arm_jit.cpp:1642 | `OP_LDR_` | arm_jit_arm.cpp:1976 | todo | |
| A-1814 | `0x716,0x71E` | `OP_LDR_M_ROR_IMM_OFF` | arm_instructions.cpp:3657 | arm_jit.cpp:1644 | `OP_LDR_` | arm_jit_arm.cpp:1978 | todo | |
| A-1824 | `0x720,0x728` | `OP_STR_M_LSL_IMM_OFF_PREIND` | arm_instructions.cpp:4209 | arm_jit.cpp:1821 | `OP_STR_` | arm_jit_arm.cpp:2157 | todo | |
| A-1826 | `0x722,0x72A` | `OP_STR_M_LSR_IMM_OFF_PREIND` | arm_instructions.cpp:4229 | arm_jit.cpp:1823 | `OP_STR_` | arm_jit_arm.cpp:2159 | todo | |
| A-1828 | `0x724,0x72C` | `OP_STR_M_ASR_IMM_OFF_PREIND` | arm_instructions.cpp:4249 | arm_jit.cpp:1825 | `OP_STR_` | arm_jit_arm.cpp:2161 | todo | |
| A-1830 | `0x726,0x72E` | `OP_STR_M_ROR_IMM_OFF_PREIND` | arm_instructions.cpp:4269 | arm_jit.cpp:1827 | `OP_STR_` | arm_jit_arm.cpp:2163 | todo | |
| A-1840 | `0x730,0x738` | `OP_LDR_M_LSL_IMM_OFF_PREIND` | arm_instructions.cpp:3683 | arm_jit.cpp:1649 | `OP_LDR_` | arm_jit_arm.cpp:1983 | todo | |
| A-1842 | `0x732,0x73A` | `OP_LDR_M_LSR_IMM_OFF_PREIND` | arm_instructions.cpp:3697 | arm_jit.cpp:1651 | `OP_LDR_` | arm_jit_arm.cpp:1985 | todo | |
| A-1844 | `0x734,0x73C` | `OP_LDR_M_ASR_IMM_OFF_PREIND` | arm_instructions.cpp:3711 | arm_jit.cpp:1653 | `OP_LDR_` | arm_jit_arm.cpp:1987 | todo | |
| A-1846 | `0x736,0x73E` | `OP_LDR_M_ROR_IMM_OFF_PREIND` | arm_instructions.cpp:3725 | arm_jit.cpp:1655 | `OP_LDR_` | arm_jit_arm.cpp:1989 | todo | |
| A-1856 | `0x740,0x748` | `OP_STRB_M_LSL_IMM_OFF` | arm_instructions.cpp:4419 | arm_jit.cpp:1856 | `OP_STR_` | arm_jit_arm.cpp:2192 | todo | |
| A-1858 | `0x742,0x74A` | `OP_STRB_M_LSR_IMM_OFF` | arm_instructions.cpp:4437 | arm_jit.cpp:1858 | `OP_STR_` | arm_jit_arm.cpp:2194 | todo | |
| A-1860 | `0x744,0x74C` | `OP_STRB_M_ASR_IMM_OFF` | arm_instructions.cpp:4455 | arm_jit.cpp:1860 | `OP_STR_` | arm_jit_arm.cpp:2196 | todo | |
| A-1862 | `0x746,0x74E` | `OP_STRB_M_ROR_IMM_OFF` | arm_instructions.cpp:4473 | arm_jit.cpp:1862 | `OP_STR_` | arm_jit_arm.cpp:2198 | todo | |
| A-1872 | `0x750,0x758` | `OP_LDRB_M_LSL_IMM_OFF` | arm_instructions.cpp:3830 | arm_jit.cpp:1701 | `OP_LDR_` | arm_jit_arm.cpp:2035 | todo | |
| A-1874 | `0x752,0x75A` | `OP_LDRB_M_LSR_IMM_OFF` | arm_instructions.cpp:3848 | arm_jit.cpp:1703 | `OP_LDR_` | arm_jit_arm.cpp:2037 | todo | |
| A-1876 | `0x754,0x75C` | `OP_LDRB_M_ASR_IMM_OFF` | arm_instructions.cpp:3866 | arm_jit.cpp:1705 | `OP_LDR_` | arm_jit_arm.cpp:2039 | todo | |
| A-1878 | `0x756,0x75E` | `OP_LDRB_M_ROR_IMM_OFF` | arm_instructions.cpp:3884 | arm_jit.cpp:1707 | `OP_LDR_` | arm_jit_arm.cpp:2041 | todo | |
| A-1888 | `0x760,0x768` | `OP_STRB_M_LSL_IMM_OFF_PREIND` | arm_instructions.cpp:4510 | arm_jit.cpp:1867 | `OP_STR_` | arm_jit_arm.cpp:2203 | todo | |
| A-1890 | `0x762,0x76A` | `OP_STRB_M_LSR_IMM_OFF_PREIND` | arm_instructions.cpp:4530 | arm_jit.cpp:1869 | `OP_STR_` | arm_jit_arm.cpp:2205 | todo | |
| A-1892 | `0x764,0x76C` | `OP_STRB_M_ASR_IMM_OFF_PREIND` | arm_instructions.cpp:4550 | arm_jit.cpp:1871 | `OP_STR_` | arm_jit_arm.cpp:2207 | todo | |
| A-1894 | `0x766,0x76E` | `OP_STRB_M_ROR_IMM_OFF_PREIND` | arm_instructions.cpp:4570 | arm_jit.cpp:1873 | `OP_STR_` | arm_jit_arm.cpp:2209 | todo | |
| A-1904 | `0x770,0x778` | `OP_LDRB_M_LSL_IMM_OFF_PREIND` | arm_instructions.cpp:3921 | arm_jit.cpp:1712 | `OP_LDR_` | arm_jit_arm.cpp:2046 | todo | |
| A-1906 | `0x772,0x77A` | `OP_LDRB_M_LSR_IMM_OFF_PREIND` | arm_instructions.cpp:3941 | arm_jit.cpp:1714 | `OP_LDR_` | arm_jit_arm.cpp:2048 | todo | |
| A-1908 | `0x774,0x77C` | `OP_LDRB_M_ASR_IMM_OFF_PREIND` | arm_instructions.cpp:3961 | arm_jit.cpp:1716 | `OP_LDR_` | arm_jit_arm.cpp:2050 | todo | |
| A-1910 | `0x776,0x77E` | `OP_LDRB_M_ROR_IMM_OFF_PREIND` | arm_instructions.cpp:3981 | arm_jit.cpp:1718 | `OP_LDR_` | arm_jit_arm.cpp:2052 | todo | |
| A-1920 | `0x780,0x788` | `OP_STR_P_LSL_IMM_OFF` | arm_instructions.cpp:4109 | arm_jit.cpp:1809 | `OP_STR_` | arm_jit_arm.cpp:2145 | todo | |
| A-1922 | `0x782,0x78A` | `OP_STR_P_LSR_IMM_OFF` | arm_instructions.cpp:4127 | arm_jit.cpp:1811 | `OP_STR_` | arm_jit_arm.cpp:2147 | todo | |
| A-1924 | `0x784,0x78C` | `OP_STR_P_ASR_IMM_OFF` | arm_instructions.cpp:4145 | arm_jit.cpp:1813 | `OP_STR_` | arm_jit_arm.cpp:2149 | todo | |
| A-1926 | `0x786,0x78E` | `OP_STR_P_ROR_IMM_OFF` | arm_instructions.cpp:4163 | arm_jit.cpp:1815 | `OP_STR_` | arm_jit_arm.cpp:2151 | todo | |
| A-1936 | `0x790,0x798` | `OP_LDR_P_LSL_IMM_OFF` | arm_instructions.cpp:3608 | arm_jit.cpp:1637 | `OP_LDR_` | arm_jit_arm.cpp:1971 | todo | |
| A-1938 | `0x792,0x79A` | `OP_LDR_P_LSR_IMM_OFF` | arm_instructions.cpp:3622 | arm_jit.cpp:1639 | `OP_LDR_` | arm_jit_arm.cpp:1973 | todo | |
| A-1940 | `0x794,0x79C` | `OP_LDR_P_ASR_IMM_OFF` | arm_instructions.cpp:3636 | arm_jit.cpp:1641 | `OP_LDR_` | arm_jit_arm.cpp:1975 | todo | |
| A-1942 | `0x796,0x79E` | `OP_LDR_P_ROR_IMM_OFF` | arm_instructions.cpp:3650 | arm_jit.cpp:1643 | `OP_LDR_` | arm_jit_arm.cpp:1977 | todo | |
| A-1952 | `0x7A0,0x7A8` | `OP_STR_P_LSL_IMM_OFF_PREIND` | arm_instructions.cpp:4199 | arm_jit.cpp:1820 | `OP_STR_` | arm_jit_arm.cpp:2156 | todo | |
| A-1954 | `0x7A2,0x7AA` | `OP_STR_P_LSR_IMM_OFF_PREIND` | arm_instructions.cpp:4219 | arm_jit.cpp:1822 | `OP_STR_` | arm_jit_arm.cpp:2158 | todo | |
| A-1956 | `0x7A4,0x7AC` | `OP_STR_P_ASR_IMM_OFF_PREIND` | arm_instructions.cpp:4239 | arm_jit.cpp:1824 | `OP_STR_` | arm_jit_arm.cpp:2160 | todo | |
| A-1958 | `0x7A6,0x7AE` | `OP_STR_P_ROR_IMM_OFF_PREIND` | arm_instructions.cpp:4259 | arm_jit.cpp:1826 | `OP_STR_` | arm_jit_arm.cpp:2162 | todo | |
| A-1968 | `0x7B0,0x7B8` | `OP_LDR_P_LSL_IMM_OFF_PREIND` | arm_instructions.cpp:3676 | arm_jit.cpp:1648 | `OP_LDR_` | arm_jit_arm.cpp:1982 | todo | |
| A-1970 | `0x7B2,0x7BA` | `OP_LDR_P_LSR_IMM_OFF_PREIND` | arm_instructions.cpp:3690 | arm_jit.cpp:1650 | `OP_LDR_` | arm_jit_arm.cpp:1984 | todo | |
| A-1972 | `0x7B4,0x7BC` | `OP_LDR_P_ASR_IMM_OFF_PREIND` | arm_instructions.cpp:3704 | arm_jit.cpp:1652 | `OP_LDR_` | arm_jit_arm.cpp:1986 | todo | |
| A-1974 | `0x7B6,0x7BE` | `OP_LDR_P_ROR_IMM_OFF_PREIND` | arm_instructions.cpp:3718 | arm_jit.cpp:1654 | `OP_LDR_` | arm_jit_arm.cpp:1988 | todo | |
| A-1984 | `0x7C0,0x7C8` | `OP_STRB_P_LSL_IMM_OFF` | arm_instructions.cpp:4410 | arm_jit.cpp:1855 | `OP_STR_` | arm_jit_arm.cpp:2191 | todo | |
| A-1986 | `0x7C2,0x7CA` | `OP_STRB_P_LSR_IMM_OFF` | arm_instructions.cpp:4428 | arm_jit.cpp:1857 | `OP_STR_` | arm_jit_arm.cpp:2193 | todo | |
| A-1988 | `0x7C4,0x7CC` | `OP_STRB_P_ASR_IMM_OFF` | arm_instructions.cpp:4446 | arm_jit.cpp:1859 | `OP_STR_` | arm_jit_arm.cpp:2195 | todo | |
| A-1990 | `0x7C6,0x7CE` | `OP_STRB_P_ROR_IMM_OFF` | arm_instructions.cpp:4464 | arm_jit.cpp:1861 | `OP_STR_` | arm_jit_arm.cpp:2197 | todo | |
| A-2000 | `0x7D0,0x7D8` | `OP_LDRB_P_LSL_IMM_OFF` | arm_instructions.cpp:3821 | arm_jit.cpp:1700 | `OP_LDR_` | arm_jit_arm.cpp:2034 | todo | |
| A-2002 | `0x7D2,0x7DA` | `OP_LDRB_P_LSR_IMM_OFF` | arm_instructions.cpp:3839 | arm_jit.cpp:1702 | `OP_LDR_` | arm_jit_arm.cpp:2036 | todo | |
| A-2004 | `0x7D4,0x7DC` | `OP_LDRB_P_ASR_IMM_OFF` | arm_instructions.cpp:3857 | arm_jit.cpp:1704 | `OP_LDR_` | arm_jit_arm.cpp:2038 | todo | |
| A-2006 | `0x7D6,0x7DE` | `OP_LDRB_P_ROR_IMM_OFF` | arm_instructions.cpp:3875 | arm_jit.cpp:1706 | `OP_LDR_` | arm_jit_arm.cpp:2040 | todo | |
| A-2016 | `0x7E0,0x7E8` | `OP_STRB_P_LSL_IMM_OFF_PREIND` | arm_instructions.cpp:4500 | arm_jit.cpp:1866 | `OP_STR_` | arm_jit_arm.cpp:2202 | todo | |
| A-2018 | `0x7E2,0x7EA` | `OP_STRB_P_LSR_IMM_OFF_PREIND` | arm_instructions.cpp:4520 | arm_jit.cpp:1868 | `OP_STR_` | arm_jit_arm.cpp:2204 | todo | |
| A-2020 | `0x7E4,0x7EC` | `OP_STRB_P_ASR_IMM_OFF_PREIND` | arm_instructions.cpp:4540 | arm_jit.cpp:1870 | `OP_STR_` | arm_jit_arm.cpp:2206 | todo | |
| A-2022 | `0x7E6,0x7EE` | `OP_STRB_P_ROR_IMM_OFF_PREIND` | arm_instructions.cpp:4560 | arm_jit.cpp:1872 | `OP_STR_` | arm_jit_arm.cpp:2208 | todo | |
| A-2032 | `0x7F0,0x7F8` | `OP_LDRB_P_LSL_IMM_OFF_PREIND` | arm_instructions.cpp:3911 | arm_jit.cpp:1711 | `OP_LDR_` | arm_jit_arm.cpp:2045 | todo | |
| A-2034 | `0x7F2,0x7FA` | `OP_LDRB_P_LSR_IMM_OFF_PREIND` | arm_instructions.cpp:3931 | arm_jit.cpp:1713 | `OP_LDR_` | arm_jit_arm.cpp:2047 | todo | |
| A-2036 | `0x7F4,0x7FC` | `OP_LDRB_P_ASR_IMM_OFF_PREIND` | arm_instructions.cpp:3951 | arm_jit.cpp:1715 | `OP_LDR_` | arm_jit_arm.cpp:2049 | todo | |
| A-2038 | `0x7F6,0x7FE` | `OP_LDRB_P_ROR_IMM_OFF_PREIND` | arm_instructions.cpp:3971 | arm_jit.cpp:1717 | `OP_LDR_` | arm_jit_arm.cpp:2051 | todo | |

### LDM/STM（32）

| id | 槽位 | handler | 解释器 | x86 | x86 宏体 | arm64 | 状态 | 偏差 |
|---|---|---|---|---|---|---|---|---|
| A-2048 | `0x800..0x80F` | `OP_STMDA` | arm_instructions.cpp:5660 | arm_jit.cpp:2319 | — | arm_jit_arm.cpp:2700 | todo | |
| A-2064 | `0x810..0x81F` | `OP_LDMDA` | arm_instructions.cpp:4820 | arm_jit.cpp:2310 | — | arm_jit_arm.cpp:2691 | todo | |
| A-2080 | `0x820..0x82F` | `OP_STMDA_W` | arm_instructions.cpp:5731 | arm_jit.cpp:2323 | — | arm_jit_arm.cpp:2704 | todo | |
| A-2096 | `0x830..0x83F` | `OP_LDMDA_W` | arm_instructions.cpp:5024 | arm_jit.cpp:2314 | — | arm_jit_arm.cpp:2695 | todo | |
| A-2112 | `0x840..0x84F` | `OP_STMDA2` | arm_instructions.cpp:5827 | arm_jit.cpp:2400 | — | arm_jit_arm.cpp:2782 | todo | |
| A-2128 | `0x850..0x85F` | `OP_LDMDA2` | arm_instructions.cpp:5255 | arm_jit.cpp:2391 | — | arm_jit_arm.cpp:2773 | todo | |
| A-2144 | `0x860..0x86F` | `OP_STMDA2_W` | arm_instructions.cpp:5941 | arm_jit.cpp:2404 | — | arm_jit_arm.cpp:2786 | todo | |
| A-2160 | `0x870..0x87F` | `OP_LDMDA2_W` | arm_instructions.cpp:5501 | arm_jit.cpp:2395 | — | arm_jit_arm.cpp:2777 | todo | |
| A-2176 | `0x880..0x88F` | `OP_STMIA` | arm_instructions.cpp:5626 | arm_jit.cpp:2317 | — | arm_jit_arm.cpp:2698 | todo | |
| A-2192 | `0x890..0x89F` | `OP_LDMIA` | arm_instructions.cpp:4710 | arm_jit.cpp:2308 | — | arm_jit_arm.cpp:2689 | todo | |
| A-2208 | `0x8A0..0x8AF` | `OP_STMIA_W` | arm_instructions.cpp:5694 | arm_jit.cpp:2321 | — | arm_jit_arm.cpp:2702 | todo | |
| A-2224 | `0x8B0..0x8BF` | `OP_LDMIA_W` | arm_instructions.cpp:4914 | arm_jit.cpp:2312 | — | arm_jit_arm.cpp:2693 | todo | |
| A-2240 | `0x8C0..0x8CF` | `OP_STMIA2` | arm_instructions.cpp:5769 | arm_jit.cpp:2398 | — | arm_jit_arm.cpp:2780 | todo | |
| A-2256 | `0x8D0..0x8DF` | `OP_LDMIA2` | arm_instructions.cpp:5134 | arm_jit.cpp:2389 | — | arm_jit_arm.cpp:2771 | todo | |
| A-2272 | `0x8E0..0x8EF` | `OP_STMIA2_W` | arm_instructions.cpp:5883 | arm_jit.cpp:2402 | — | arm_jit_arm.cpp:2784 | todo | |
| A-2288 | `0x8F0..0x8FF` | `OP_LDMIA2_W` | arm_instructions.cpp:5386 | arm_jit.cpp:2393 | — | arm_jit_arm.cpp:2775 | todo | |
| A-2304 | `0x900..0x90F` | `OP_STMDB` | arm_instructions.cpp:5677 | arm_jit.cpp:2320 | — | arm_jit_arm.cpp:2701 | todo | |
| A-2320 | `0x910..0x91F` | `OP_LDMDB` | arm_instructions.cpp:4867 | arm_jit.cpp:2311 | — | arm_jit_arm.cpp:2692 | todo | |
| A-2336 | `0x920..0x92F` | `OP_STMDB_W` | arm_instructions.cpp:5750 | arm_jit.cpp:2324 | — | arm_jit_arm.cpp:2705 | todo | |
| A-2352 | `0x930..0x93F` | `OP_LDMDB_W` | arm_instructions.cpp:5079 | arm_jit.cpp:2315 | — | arm_jit_arm.cpp:2696 | todo | |
| A-2368 | `0x940..0x94F` | `OP_STMDB2` | arm_instructions.cpp:5856 | arm_jit.cpp:2401 | — | arm_jit_arm.cpp:2783 | todo | |
| A-2384 | `0x950..0x95F` | `OP_LDMDB2` | arm_instructions.cpp:5322 | arm_jit.cpp:2392 | — | arm_jit_arm.cpp:2774 | todo | |
| A-2400 | `0x960..0x96F` | `OP_STMDB2_W` | arm_instructions.cpp:5972 | arm_jit.cpp:2405 | — | arm_jit_arm.cpp:2787 | todo | |
| A-2416 | `0x970..0x97F` | `OP_LDMDB2_W` | arm_instructions.cpp:5560 | arm_jit.cpp:2396 | — | arm_jit_arm.cpp:2778 | todo | |
| A-2432 | `0x980..0x98F` | `OP_STMIB` | arm_instructions.cpp:5643 | arm_jit.cpp:2318 | — | arm_jit_arm.cpp:2699 | todo | |
| A-2448 | `0x990..0x99F` | `OP_LDMIB` | arm_instructions.cpp:4772 | arm_jit.cpp:2309 | — | arm_jit_arm.cpp:2690 | todo | |
| A-2464 | `0x9A0..0x9AF` | `OP_STMIB_W` | arm_instructions.cpp:5713 | arm_jit.cpp:2322 | — | arm_jit_arm.cpp:2703 | todo | |
| A-2480 | `0x9B0..0x9BF` | `OP_LDMIB_W` | arm_instructions.cpp:4968 | arm_jit.cpp:2313 | — | arm_jit_arm.cpp:2694 | todo | |
| A-2496 | `0x9C0..0x9CF` | `OP_STMIB2` | arm_instructions.cpp:5798 | arm_jit.cpp:2399 | — | arm_jit_arm.cpp:2781 | todo | |
| A-2512 | `0x9D0..0x9DF` | `OP_LDMIB2` | arm_instructions.cpp:5194 | arm_jit.cpp:2390 | — | arm_jit_arm.cpp:2772 | todo | |
| A-2528 | `0x9E0..0x9EF` | `OP_STMIB2_W` | arm_instructions.cpp:5914 | arm_jit.cpp:2403 | — | arm_jit_arm.cpp:2785 | todo | |
| A-2544 | `0x9F0..0x9FF` | `OP_LDMIB2_W` | arm_instructions.cpp:5442 | arm_jit.cpp:2394 | — | arm_jit_arm.cpp:2776 | todo | |

### 乘法/乘加（28）

| id | 槽位 | handler | 解释器 | x86 | x86 宏体 | arm64 | 状态 | 偏差 |
|---|---|---|---|---|---|---|---|---|
| A-0009 | `0x009` | `OP_MUL` | arm_instructions.cpp:2317 | arm_jit.cpp:1244 | `OP_MUL_` | arm_jit_arm.cpp:1602 | todo | |
| A-0025 | `0x019` | `OP_MUL_S` | arm_instructions.cpp:2333 | arm_jit.cpp:1251 | `OP_MUL_` | arm_jit_arm.cpp:1609 | todo | |
| A-0041 | `0x029` | `OP_MLA` | arm_instructions.cpp:2325 | arm_jit.cpp:1245 | `OP_MUL_` | arm_jit_arm.cpp:1603 | todo | |
| A-0057 | `0x039` | `OP_MLA_S` | arm_instructions.cpp:2344 | arm_jit.cpp:1252 | `OP_MUL_` | arm_jit_arm.cpp:1610 | todo | |
| A-0137 | `0x089` | `OP_UMULL` | arm_instructions.cpp:2371 | arm_jit.cpp:1246 | `OP_MUL_` | arm_jit_arm.cpp:1604 | todo | |
| A-0153 | `0x099` | `OP_UMULL_S` | arm_instructions.cpp:2396 | arm_jit.cpp:1253 | `OP_MUL_` | arm_jit_arm.cpp:1611 | todo | |
| A-0169 | `0x0A9` | `OP_UMLAL` | arm_instructions.cpp:2382 | arm_jit.cpp:1247 | `OP_MUL_` | arm_jit_arm.cpp:1605 | todo | |
| A-0185 | `0x0B9` | `OP_UMLAL_S` | arm_instructions.cpp:2410 | arm_jit.cpp:1254 | `OP_MUL_` | arm_jit_arm.cpp:1612 | todo | |
| A-0201 | `0x0C9` | `OP_SMULL` | arm_instructions.cpp:2445 | arm_jit.cpp:1248 | `OP_MUL_` | arm_jit_arm.cpp:1606 | todo | |
| A-0217 | `0x0D9` | `OP_SMULL_S` | arm_instructions.cpp:2473 | arm_jit.cpp:1255 | `OP_MUL_` | arm_jit_arm.cpp:1613 | todo | |
| A-0233 | `0x0E9` | `OP_SMLAL` | arm_instructions.cpp:2456 | arm_jit.cpp:1249 | `OP_MUL_` | arm_jit_arm.cpp:1607 | todo | |
| A-0249 | `0x0F9` | `OP_SMLAL_S` | arm_instructions.cpp:2487 | arm_jit.cpp:1256 | `OP_MUL_` | arm_jit_arm.cpp:1614 | todo | |
| A-0264 | `0x108` | `OP_SMLA_B_B` | arm_instructions.cpp:3353 | arm_jit.cpp:1312 | `OP_MULxy_` | arm_jit_arm.cpp:1662 | todo | |
| A-0266 | `0x10A` | `OP_SMLA_T_B` | arm_instructions.cpp:3380 | arm_jit.cpp:1314 | `OP_MULxy_` | arm_jit_arm.cpp:1666 | todo | |
| A-0268 | `0x10C` | `OP_SMLA_B_T` | arm_instructions.cpp:3366 | arm_jit.cpp:1313 | `OP_MULxy_` | arm_jit_arm.cpp:1665 | todo | |
| A-0270 | `0x10E` | `OP_SMLA_T_T` | arm_instructions.cpp:3394 | arm_jit.cpp:1315 | `OP_MULxy_` | arm_jit_arm.cpp:1667 | todo | |
| A-0296 | `0x128` | `OP_SMLAW_B` | arm_instructions.cpp:3494 | arm_jit.cpp:1359 | `OP_SMxxW_` | arm_jit_arm.cpp:1696 | todo | |
| A-0298 | `0x12A` | `OP_SMULW_B` | arm_instructions.cpp:3468 | arm_jit.cpp:1357 | `OP_SMxxW_` | arm_jit_arm.cpp:1694 | todo | |
| A-0300 | `0x12C` | `OP_SMLAW_T` | arm_instructions.cpp:3511 | arm_jit.cpp:1360 | `OP_SMxxW_` | arm_jit_arm.cpp:1697 | todo | |
| A-0302 | `0x12E` | `OP_SMULW_T` | arm_instructions.cpp:3479 | arm_jit.cpp:1358 | `OP_SMxxW_` | arm_jit_arm.cpp:1695 | todo | |
| A-0328 | `0x148` | `OP_SMLAL_B_B` | arm_instructions.cpp:3412 | arm_jit.cpp:1320 | `OP_MULxy_` | arm_jit_arm.cpp:1672 | todo | |
| A-0330 | `0x14A` | `OP_SMLAL_T_B` | arm_instructions.cpp:3438 | arm_jit.cpp:1322 | `OP_MULxy_` | arm_jit_arm.cpp:1674 | todo | |
| A-0332 | `0x14C` | `OP_SMLAL_B_T` | arm_instructions.cpp:3425 | arm_jit.cpp:1321 | `OP_MULxy_` | arm_jit_arm.cpp:1673 | todo | |
| A-0334 | `0x14E` | `OP_SMLAL_T_T` | arm_instructions.cpp:3451 | arm_jit.cpp:1323 | `OP_MULxy_` | arm_jit_arm.cpp:1675 | todo | |
| A-0360 | `0x168` | `OP_SMUL_B_B` | arm_instructions.cpp:3316 | arm_jit.cpp:1304 | `OP_MULxy_` | arm_jit_arm.cpp:1654 | todo | |
| A-0362 | `0x16A` | `OP_SMUL_T_B` | arm_instructions.cpp:3333 | arm_jit.cpp:1306 | `OP_MULxy_` | arm_jit_arm.cpp:1656 | todo | |
| A-0364 | `0x16C` | `OP_SMUL_B_T` | arm_instructions.cpp:3325 | arm_jit.cpp:1305 | `OP_MULxy_` | arm_jit_arm.cpp:1655 | todo | |
| A-0366 | `0x16E` | `OP_SMUL_T_T` | arm_instructions.cpp:3341 | arm_jit.cpp:1307 | `OP_MULxy_` | arm_jit_arm.cpp:1657 | todo | |

### 协处理器（17）

| id | 槽位 | handler | 解释器 | x86 | x86 宏体 | arm64 | 状态 | 偏差 |
|---|---|---|---|---|---|---|---|---|
| A-3072 | `0xC00..0xC0F,0xC40..0xC4F,0xC80..0xC8F,0xCC0..0xCCF` | `OP_STC_OPTION` | arm_instructions.cpp:6140 | `NULL` | — | `NULL` | interp-fallback | |
| A-3088 | `0xC10..0xC1F,0xC50..0xC5F,0xC90..0xC9F,0xCD0..0xCDF` | `OP_LDC_OPTION` | arm_instructions.cpp:6187 | `NULL` | — | `NULL` | interp-fallback | |
| A-3104 | `0xC20..0xC2F,0xC60..0xC6F` | `OP_STC_M_POSTIND` | arm_instructions.cpp:6134 | `NULL` | — | `NULL` | interp-fallback | |
| A-3120 | `0xC30..0xC3F,0xC70..0xC7F` | `OP_LDC_M_POSTIND` | arm_instructions.cpp:6181 | `NULL` | — | `NULL` | interp-fallback | |
| A-3232 | `0xCA0..0xCAF,0xCE0..0xCEF` | `OP_STC_P_POSTIND` | arm_instructions.cpp:6126 | `NULL` | — | `NULL` | interp-fallback | |
| A-3248 | `0xCB0..0xCBF,0xCF0..0xCFF` | `OP_LDC_P_POSTIND` | arm_instructions.cpp:6175 | `NULL` | — | `NULL` | interp-fallback | |
| A-3328 | `0xD00..0xD0F,0xD40..0xD4F` | `OP_STC_M_IMM_OFF` | arm_instructions.cpp:6108 | `NULL` | — | `NULL` | interp-fallback | |
| A-3344 | `0xD10..0xD1F,0xD50..0xD5F` | `OP_LDC_M_IMM_OFF` | arm_instructions.cpp:6157 | `NULL` | — | `NULL` | interp-fallback | |
| A-3360 | `0xD20..0xD2F,0xD60..0xD6F` | `OP_STC_M_PREIND` | arm_instructions.cpp:6120 | `NULL` | — | `NULL` | interp-fallback | |
| A-3376 | `0xD30..0xD3F,0xD70..0xD7F` | `OP_LDC_M_PREIND` | arm_instructions.cpp:6169 | `NULL` | — | `NULL` | interp-fallback | |
| A-3456 | `0xD80..0xD8F,0xDC0..0xDCF` | `OP_STC_P_IMM_OFF` | arm_instructions.cpp:6102 | `NULL` | — | `NULL` | interp-fallback | |
| A-3472 | `0xD90..0xD9F,0xDD0..0xDDF` | `OP_LDC_P_IMM_OFF` | arm_instructions.cpp:6151 | `NULL` | — | `NULL` | interp-fallback | |
| A-3488 | `0xDA0..0xDAF,0xDE0..0xDEF` | `OP_STC_P_PREIND` | arm_instructions.cpp:6114 | `NULL` | — | `NULL` | interp-fallback | |
| A-3504 | `0xDB0..0xDBF,0xDF0..0xDFF` | `OP_LDC_P_PREIND` | arm_instructions.cpp:6163 | `NULL` | — | `NULL` | interp-fallback | |
| A-3584 | `0xE00,0xE02,0xE04,0xE06,0xE08,0xE0A,…（共 128 槽）` | `OP_CDP` | arm_instructions.cpp:6352 | `NULL` | — | `NULL` | interp-fallback | |
| A-3585 | `0xE01,0xE03,0xE05,0xE07,0xE09,0xE0B,…（共 64 槽）` | `OP_MCR` | arm_instructions.cpp:6197 | arm_jit.cpp:2531 | — | arm_jit_arm.cpp:2919 | todo | |
| A-3601 | `0xE11,0xE13,0xE15,0xE17,0xE19,0xE1B,…（共 64 槽）` | `OP_MRC` | arm_instructions.cpp:6215 | arm_jit.cpp:2738 | — | arm_jit_arm.cpp:3126 | todo | |

### 状态寄存器（6）

| id | 槽位 | handler | 解释器 | x86 | x86 宏体 | arm64 | 状态 | 偏差 |
|---|---|---|---|---|---|---|---|---|
| A-0256 | `0x100` | `OP_MRS_CPSR` | arm_instructions.cpp:2970 | arm_jit.cpp:1365 | — | arm_jit_arm.cpp:1705 | todo | |
| A-0288 | `0x120` | `OP_MSR_CPSR` | arm_instructions.cpp:3017 | arm_jit.cpp:1497 | `OP_MSR_` | arm_jit_arm.cpp:1827 | todo | |
| A-0320 | `0x140` | `OP_MRS_SPSR` | arm_instructions.cpp:2977 | arm_jit.cpp:1373 | — | arm_jit_arm.cpp:1713 | todo | |
| A-0352 | `0x160` | `OP_MSR_SPSR` | arm_instructions.cpp:3054 | arm_jit.cpp:1498 | `OP_MSR_` | arm_jit_arm.cpp:1828 | todo | |
| A-0800 | `0x320..0x32F` | `OP_MSR_CPSR_IMM_VAL` | arm_instructions.cpp:3063 | arm_jit.cpp:1499 | `OP_MSR_` | arm_jit_arm.cpp:1829 | todo | |
| A-0864 | `0x360..0x36F` | `OP_MSR_SPSR_IMM_VAL` | arm_instructions.cpp:3071 | arm_jit.cpp:1500 | `OP_MSR_` | arm_jit_arm.cpp:1830 | todo | |

### 饱和/计数（5）

| id | 槽位 | handler | 解释器 | x86 | x86 宏体 | arm64 | 状态 | 偏差 |
|---|---|---|---|---|---|---|---|---|
| A-0261 | `0x105` | `OP_QADD` | arm_instructions.cpp:3209 | arm_jit.cpp:1177 | — | arm_jit_arm.cpp:1543 | todo | |
| A-0293 | `0x125` | `OP_QSUB` | arm_instructions.cpp:3230 | arm_jit.cpp:1178 | — | arm_jit_arm.cpp:1544 | todo | |
| A-0325 | `0x145` | `OP_QDADD` | arm_instructions.cpp:3251 | arm_jit.cpp:1179 | — | arm_jit_arm.cpp:1545 | todo | |
| A-0353 | `0x161` | `OP_CLZ` | arm_instructions.cpp:3173 | arm_jit.cpp:2465 | — | arm_jit_arm.cpp:2856 | todo | |
| A-0357 | `0x165` | `OP_QDSUB` | arm_instructions.cpp:3280 | arm_jit.cpp:1180 | — | arm_jit_arm.cpp:1546 | todo | |

### 分支（5）

| id | 槽位 | handler | 解释器 | x86 | x86 宏体 | arm64 | 状态 | 偏差 |
|---|---|---|---|---|---|---|---|---|
| A-0289 | `0x121` | `OP_BX` | arm_instructions.cpp:3084 | arm_jit.cpp:2459 | — | arm_jit_arm.cpp:2850 | todo | |
| A-0291 | `0x123` | `OP_BLX_REG` | arm_instructions.cpp:3099 | arm_jit.cpp:2460 | — | arm_jit_arm.cpp:2851 | todo | |
| A-0295 | `0x127` | `OP_BKPT` | arm_instructions.cpp:6306 | arm_jit.cpp:2992 | — | arm_jit_arm.cpp:3360 | todo | |
| A-2560 | `0xA00..0xAFF` | `OP_B` | arm_instructions.cpp:3116 | arm_jit.cpp:2429 | — | arm_jit_arm.cpp:2812 | todo | |
| A-2816 | `0xB00..0xBFF` | `OP_BL` | arm_instructions.cpp:3140 | arm_jit.cpp:2430 | — | arm_jit_arm.cpp:2813 | todo | |

### 异常/未定义（2）

| id | 槽位 | handler | 解释器 | x86 | x86 宏体 | arm64 | 状态 | 偏差 |
|---|---|---|---|---|---|---|---|---|
| A-0043 | `0x02B,0x02D,0x02F,0x03B,0x03D,0x03F,…（共 348 槽）` | `OP_UND` | arm_instructions.cpp:255 | `NULL` | — | `NULL` | interp-fallback | |
| A-3840 | `0xF00..0xFFF` | `OP_SWI` | arm_instructions.cpp:6259 | arm_jit.cpp:2987 | — | arm_jit_arm.cpp:3355 | todo | |

## Thumb

### 数据处理（26）

| id | 槽位 | handler | 解释器 | x86 | x86 宏体 | arm64 | 状态 | 偏差 |
|---|---|---|---|---|---|---|---|---|
| T-0096 | `0x060..0x067` | `OP_ADD_REG` | thumb_instructions.cpp:250 | arm_jit.cpp:3251 | — | arm_jit_arm.cpp:3560 | todo | |
| T-0104 | `0x068..0x06F` | `OP_SUB_REG` | thumb_instructions.cpp:327 | arm_jit.cpp:3340 | — | arm_jit_arm.cpp:3646 | todo | |
| T-0112 | `0x070..0x077` | `OP_ADD_IMM3` | thumb_instructions.cpp:211 | arm_jit.cpp:3217 | — | arm_jit_arm.cpp:3526 | todo | |
| T-0120 | `0x078..0x07F` | `OP_SUB_IMM3` | thumb_instructions.cpp:297 | arm_jit.cpp:3314 | — | arm_jit_arm.cpp:3620 | todo | |
| T-0128 | `0x080..0x09F` | `OP_MOV_IMM8` | thumb_instructions.cpp:346 | arm_jit.cpp:3389 | — | arm_jit_arm.cpp:3702 | todo | |
| T-0160 | `0x0A0..0x0BF` | `OP_CMP_IMM8` | thumb_instructions.cpp:373 | arm_jit.cpp:3445 | — | arm_jit_arm.cpp:3757 | todo | |
| T-0192 | `0x0C0..0x0DF` | `OP_ADD_IMM8` | thumb_instructions.cpp:236 | arm_jit.cpp:3244 | — | arm_jit_arm.cpp:3553 | todo | |
| T-0224 | `0x0E0..0x0FF` | `OP_SUB_IMM8` | thumb_instructions.cpp:312 | arm_jit.cpp:3333 | — | arm_jit_arm.cpp:3639 | todo | |
| T-0256 | `0x100` | `OP_AND` | thumb_instructions.cpp:415 | arm_jit.cpp:3191 | `OP_LOGIC` | arm_jit_arm.cpp:3500 | todo | |
| T-0257 | `0x101` | `OP_EOR` | thumb_instructions.cpp:427 | arm_jit.cpp:3193 | `OP_LOGIC` | arm_jit_arm.cpp:3502 | todo | |
| T-0261 | `0x105` | `OP_ADC_REG` | thumb_instructions.cpp:440 | arm_jit.cpp:3363 | — | arm_jit_arm.cpp:3671 | todo | |
| T-0262 | `0x106` | `OP_SBC_REG` | thumb_instructions.cpp:465 | arm_jit.cpp:3376 | — | arm_jit_arm.cpp:3687 | todo | |
| T-0264 | `0x108` | `OP_TST` | thumb_instructions.cpp:523 | arm_jit.cpp:3483 | — | arm_jit_arm.cpp:3800 | todo | |
| T-0265 | `0x109` | `OP_NEG` | thumb_instructions.cpp:536 | arm_jit.cpp:3199 | — | arm_jit_arm.cpp:3508 | todo | |
| T-0266 | `0x10A` | `OP_CMP` | thumb_instructions.cpp:385 | arm_jit.cpp:3452 | — | arm_jit_arm.cpp:3766 | todo | |
| T-0267 | `0x10B` | `OP_CMN` | thumb_instructions.cpp:571 | arm_jit.cpp:3471 | — | arm_jit_arm.cpp:3787 | todo | |
| T-0268 | `0x10C` | `OP_ORR` | thumb_instructions.cpp:587 | arm_jit.cpp:3192 | `OP_LOGIC` | arm_jit_arm.cpp:3501 | todo | |
| T-0270 | `0x10E` | `OP_BIC` | thumb_instructions.cpp:601 | arm_jit.cpp:3194 | `OP_LOGIC` | arm_jit_arm.cpp:3503 | todo | |
| T-0271 | `0x10F` | `OP_MVN` | thumb_instructions.cpp:615 | arm_jit.cpp:3413 | — | arm_jit_arm.cpp:3725 | todo | |
| T-0272 | `0x110..0x113` | `OP_ADD_SPE` | thumb_instructions.cpp:264 | arm_jit.cpp:3277 | — | arm_jit_arm.cpp:3585 | todo | |
| T-0276 | `0x114..0x117` | `OP_CMP_SPE` | thumb_instructions.cpp:397 | arm_jit.cpp:3461 | — | arm_jit_arm.cpp:3776 | todo | |
| T-0280 | `0x118..0x11B` | `OP_MOV_SPE` | thumb_instructions.cpp:355 | arm_jit.cpp:3397 | — | arm_jit_arm.cpp:3711 | todo | |
| T-0640 | `0x280..0x29F` | `OP_ADD_2PC` | thumb_instructions.cpp:279 | arm_jit.cpp:3292 | — | arm_jit_arm.cpp:3599 | todo | |
| T-0672 | `0x2A0..0x2BF` | `OP_ADD_2SP` | thumb_instructions.cpp:286 | arm_jit.cpp:3299 | — | arm_jit_arm.cpp:3607 | todo | |
| T-0704 | `0x2C0..0x2C1` | `OP_ADJUST_P_SP` | thumb_instructions.cpp:834 | arm_jit.cpp:3655 | — | arm_jit_arm.cpp:3991 | todo | |
| T-0706 | `0x2C2..0x2C3` | `OP_ADJUST_M_SP` | thumb_instructions.cpp:841 | arm_jit.cpp:3656 | — | arm_jit_arm.cpp:3992 | todo | |

### 移位（10）

| id | 槽位 | handler | 解释器 | x86 | x86 宏体 | arm64 | 状态 | 偏差 |
|---|---|---|---|---|---|---|---|---|
| T-0000 | `0x000` | `OP_LSL_0` | thumb_instructions.cpp:51 | arm_jit.cpp:3064 | — | arm_jit_arm.cpp:3403 | todo | |
| T-0001 | `0x001..0x01F` | `OP_LSL` | thumb_instructions.cpp:60 | arm_jit.cpp:3078 | `OP_SHIFTS_IMM` | arm_jit_arm.cpp:3421 | todo | |
| T-0032 | `0x020` | `OP_LSR_0` | thumb_instructions.cpp:105 | arm_jit.cpp:3080 | — | arm_jit_arm.cpp:3424 | todo | |
| T-0033 | `0x021..0x03F` | `OP_LSR` | thumb_instructions.cpp:115 | arm_jit.cpp:3089 | `OP_SHIFTS_IMM` | arm_jit_arm.cpp:3434 | todo | |
| T-0064 | `0x040` | `OP_ASR_0` | thumb_instructions.cpp:159 | arm_jit.cpp:3091 | — | arm_jit_arm.cpp:3437 | todo | |
| T-0065 | `0x041..0x05F` | `OP_ASR` | thumb_instructions.cpp:169 | arm_jit.cpp:3108 | `OP_SHIFTS_IMM` | arm_jit_arm.cpp:3470 | todo | |
| T-0258 | `0x102` | `OP_LSL_REG` | thumb_instructions.cpp:71 | arm_jit.cpp:3079 | `OP_SHIFTS_REG` | arm_jit_arm.cpp:3422 | todo | |
| T-0259 | `0x103` | `OP_LSR_REG` | thumb_instructions.cpp:126 | arm_jit.cpp:3090 | `OP_SHIFTS_REG` | arm_jit_arm.cpp:3435 | todo | |
| T-0260 | `0x104` | `OP_ASR_REG` | thumb_instructions.cpp:180 | arm_jit.cpp:3109 | — | arm_jit_arm.cpp:3471 | todo | |
| T-0263 | `0x107` | `OP_ROR_REG` | thumb_instructions.cpp:492 | arm_jit.cpp:3146 | — | arm_jit_arm.cpp:3485 | todo | |

### 访存（17）

| id | 槽位 | handler | 解释器 | x86 | x86 宏体 | arm64 | 状态 | 偏差 |
|---|---|---|---|---|---|---|---|---|
| T-0288 | `0x120..0x13F` | `OP_LDR_PCREL` | thumb_instructions.cpp:821 | arm_jit.cpp:3602 | — | arm_jit_arm.cpp:3927 | todo | |
| T-0320 | `0x140..0x147` | `OP_STR_REG_OFF` | thumb_instructions.cpp:786 | arm_jit.cpp:3563 | — | arm_jit_arm.cpp:3873 | todo | |
| T-0328 | `0x148..0x14F` | `OP_STRH_REG_OFF` | thumb_instructions.cpp:734 | arm_jit.cpp:3557 | — | arm_jit_arm.cpp:3867 | todo | |
| T-0336 | `0x150..0x157` | `OP_STRB_REG_OFF` | thumb_instructions.cpp:685 | arm_jit.cpp:3551 | — | arm_jit_arm.cpp:3861 | todo | |
| T-0344 | `0x158..0x15F` | `OP_LDRSB_REG_OFF` | thumb_instructions.cpp:705 | arm_jit.cpp:3553 | — | arm_jit_arm.cpp:3863 | todo | |
| T-0352 | `0x160..0x167` | `OP_LDR_REG_OFF` | thumb_instructions.cpp:794 | arm_jit.cpp:3564 | — | arm_jit_arm.cpp:3874 | todo | |
| T-0360 | `0x168..0x16F` | `OP_LDRH_REG_OFF` | thumb_instructions.cpp:742 | arm_jit.cpp:3558 | — | arm_jit_arm.cpp:3868 | todo | |
| T-0368 | `0x170..0x177` | `OP_LDRB_REG_OFF` | thumb_instructions.cpp:693 | arm_jit.cpp:3552 | — | arm_jit_arm.cpp:3862 | todo | |
| T-0376 | `0x178..0x17F` | `OP_LDRSH_REG_OFF` | thumb_instructions.cpp:754 | arm_jit.cpp:3559 | — | arm_jit_arm.cpp:3869 | todo | |
| T-0384 | `0x180..0x19F` | `OP_STR_IMM_OFF` | thumb_instructions.cpp:766 | arm_jit.cpp:3561 | — | arm_jit_arm.cpp:3871 | todo | |
| T-0416 | `0x1A0..0x1BF` | `OP_LDR_IMM_OFF` | thumb_instructions.cpp:774 | arm_jit.cpp:3562 | — | arm_jit_arm.cpp:3872 | todo | |
| T-0448 | `0x1C0..0x1DF` | `OP_STRB_IMM_OFF` | thumb_instructions.cpp:668 | arm_jit.cpp:3549 | — | arm_jit_arm.cpp:3859 | todo | |
| T-0480 | `0x1E0..0x1FF` | `OP_LDRB_IMM_OFF` | thumb_instructions.cpp:676 | arm_jit.cpp:3550 | — | arm_jit_arm.cpp:3860 | todo | |
| T-0512 | `0x200..0x21F` | `OP_STRH_IMM_OFF` | thumb_instructions.cpp:717 | arm_jit.cpp:3555 | — | arm_jit_arm.cpp:3865 | todo | |
| T-0544 | `0x220..0x23F` | `OP_LDRH_IMM_OFF` | thumb_instructions.cpp:725 | arm_jit.cpp:3556 | — | arm_jit_arm.cpp:3866 | todo | |
| T-0576 | `0x240..0x25F` | `OP_STR_SPREL` | thumb_instructions.cpp:805 | arm_jit.cpp:3566 | — | arm_jit_arm.cpp:3876 | todo | |
| T-0608 | `0x260..0x27F` | `OP_LDR_SPREL` | thumb_instructions.cpp:813 | arm_jit.cpp:3584 | — | arm_jit_arm.cpp:3901 | todo | |

### LDM/STM（6）

| id | 槽位 | handler | 解释器 | x86 | x86 宏体 | arm64 | 状态 | 偏差 |
|---|---|---|---|---|---|---|---|---|
| T-0720 | `0x2D0..0x2D3` | `OP_PUSH` | thumb_instructions.cpp:852 | arm_jit.cpp:3683 | — | arm_jit_arm.cpp:4023 | todo | |
| T-0724 | `0x2D4..0x2D7` | `OP_PUSH_LR` | thumb_instructions.cpp:869 | arm_jit.cpp:3684 | — | arm_jit_arm.cpp:4024 | todo | |
| T-0752 | `0x2F0..0x2F3` | `OP_POP` | thumb_instructions.cpp:890 | arm_jit.cpp:3685 | — | arm_jit_arm.cpp:4025 | todo | |
| T-0756 | `0x2F4..0x2F7` | `OP_POP_PC` | thumb_instructions.cpp:914 | arm_jit.cpp:3686 | — | arm_jit_arm.cpp:4026 | todo | |
| T-0768 | `0x300..0x31F` | `OP_STMIA_THUMB` | thumb_instructions.cpp:944 | arm_jit.cpp:3650 | — | arm_jit_arm.cpp:3986 | todo | |
| T-0800 | `0x320..0x33F` | `OP_LDMIA_THUMB` | thumb_instructions.cpp:975 | arm_jit.cpp:3649 | — | arm_jit_arm.cpp:3985 | todo | |

### 乘法/乘加（1）

| id | 槽位 | handler | 解释器 | x86 | x86 宏体 | arm64 | 状态 | 偏差 |
|---|---|---|---|---|---|---|---|---|
| T-0269 | `0x10D` | `OP_MUL_REG` | thumb_instructions.cpp:641 | arm_jit.cpp:3427 | — | arm_jit_arm.cpp:3738 | todo | |

### 分支（8）

| id | 槽位 | handler | 解释器 | x86 | x86 宏体 | arm64 | 状态 | 偏差 |
|---|---|---|---|---|---|---|---|---|
| T-0284 | `0x11C..0x11D` | `OP_BX_THUMB` | thumb_instructions.cpp:1129 | arm_jit.cpp:3784 | — | arm_jit_arm.cpp:4145 | todo | |
| T-0286 | `0x11E..0x11F` | `OP_BLX_THUMB` | thumb_instructions.cpp:1169 | arm_jit.cpp:3785 | — | arm_jit_arm.cpp:4146 | todo | |
| T-0760 | `0x2F8..0x2FB` | `OP_BKPT_THUMB` | thumb_instructions.cpp:1012 | `NULL` | — | `NULL` | interp-fallback | |
| T-0832 | `0x340..0x37B` | `OP_B_COND` | thumb_instructions.cpp:1080 | arm_jit.cpp:3691 | — | arm_jit_arm.cpp:4031 | todo | |
| T-0896 | `0x380..0x39F` | `OP_B_UNCOND` | thumb_instructions.cpp:1090 | arm_jit.cpp:3707 | — | arm_jit_arm.cpp:4048 | todo | |
| T-0928 | `0x3A0..0x3BF` | `OP_BLX` | thumb_instructions.cpp:1106 | arm_jit.cpp:3714 | — | arm_jit_arm.cpp:4056 | todo | |
| T-0960 | `0x3C0..0x3DF` | `OP_BL_10` | thumb_instructions.cpp:1115 | arm_jit.cpp:3727 | — | arm_jit_arm.cpp:4074 | todo | |
| T-0992 | `0x3E0..0x3FF` | `OP_BL_11` | thumb_instructions.cpp:1121 | arm_jit.cpp:3734 | — | arm_jit_arm.cpp:4081 | todo | |

### 异常/未定义（2）

| id | 槽位 | handler | 解释器 | x86 | x86 宏体 | arm64 | 状态 | 偏差 |
|---|---|---|---|---|---|---|---|---|
| T-0708 | `0x2C4..0x2CF,0x2D8..0x2EF,0x2FC..0x2FF` | `OP_UND_THUMB` | thumb_instructions.cpp:40 | `NULL` | — | `NULL` | interp-fallback | |
| T-0892 | `0x37C..0x37F` | `OP_SWI_THUMB` | thumb_instructions.cpp:1031 | arm_jit.cpp:3787 | — | arm_jit_arm.cpp:4148 | todo | |

