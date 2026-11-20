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
| 移位 | 224 | 10 | 234 | 0 | 224 |
| 访存 | 174 | 17 | 191 | 2 | 172 |
| LDM/STM | 32 | 6 | 38 | 0 | 32 |
| 乘法/乘加 | 28 | 1 | 29 | 0 | 28 |
| 协处理器 | 17 | 0 | 17 | 15 | 0 |
| 状态寄存器 | 6 | 0 | 6 | 0 | 6 |
| 饱和/计数 | 5 | 0 | 5 | 0 | 1 |
| 分支 | 5 | 8 | 13 | 1 | 5 |
| 异常/未定义 | 2 | 2 | 4 | 2 | 1 |
| **合计** | **521** | **70** | **591** | **20** | **497** |

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
| A-0512 | `0x200..0x20F` | `OP_AND_IMM_VAL` | arm_instructions.cpp:340 | arm_jit.cpp:922 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1288 | diff-pass | |
| A-0528 | `0x210..0x21F` | `OP_AND_S_IMM_VAL` | arm_instructions.cpp:395 | arm_jit.cpp:983 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1349 | diff-pass | |
| A-0544 | `0x220..0x22F` | `OP_EOR_IMM_VAL` | arm_instructions.cpp:479 | arm_jit.cpp:932 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1298 | diff-pass | |
| A-0560 | `0x230..0x23F` | `OP_EOR_S_IMM_VAL` | arm_instructions.cpp:534 | arm_jit.cpp:993 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1359 | diff-pass | |
| A-0576 | `0x240..0x24F` | `OP_SUB_IMM_VAL` | arm_instructions.cpp:619 | arm_jit.cpp:962 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1328 | diff-pass | |
| A-0592 | `0x250..0x25F` | `OP_SUB_S_IMM_VAL` | arm_instructions.cpp:682 | arm_jit.cpp:1023 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1389 | diff-pass | |
| A-0608 | `0x260..0x26F` | `OP_RSB_IMM_VAL` | arm_instructions.cpp:768 | arm_jit.cpp:972 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1338 | diff-pass | |
| A-0624 | `0x270..0x27F` | `OP_RSB_S_IMM_VAL` | arm_instructions.cpp:831 | arm_jit.cpp:1033 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1399 | diff-pass | |
| A-0640 | `0x280..0x28F` | `OP_ADD_IMM_VAL` | arm_instructions.cpp:917 | arm_jit.cpp:952 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1318 | diff-pass | |
| A-0656 | `0x290..0x29F` | `OP_ADD_S_IMM_VAL` | arm_instructions.cpp:980 | arm_jit.cpp:1013 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1379 | diff-pass | |
| A-0672 | `0x2A0..0x2AF` | `OP_ADC_IMM_VAL` | arm_instructions.cpp:1076 | arm_jit.cpp:1043 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1409 | diff-pass | |
| A-0688 | `0x2B0..0x2BF` | `OP_ADC_S_IMM_VAL` | arm_instructions.cpp:1139 | arm_jit.cpp:1053 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1419 | diff-pass | |
| A-0704 | `0x2C0..0x2CF` | `OP_SBC_IMM_VAL` | arm_instructions.cpp:1236 | arm_jit.cpp:1063 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1429 | diff-pass | |
| A-0720 | `0x2D0..0x2DF` | `OP_SBC_S_IMM_VAL` | arm_instructions.cpp:1299 | arm_jit.cpp:1073 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1439 | diff-pass | |
| A-0736 | `0x2E0..0x2EF` | `OP_RSC_IMM_VAL` | arm_instructions.cpp:1396 | arm_jit.cpp:1083 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1449 | diff-pass | |
| A-0752 | `0x2F0..0x2FF` | `OP_RSC_S_IMM_VAL` | arm_instructions.cpp:1459 | arm_jit.cpp:1093 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1459 | diff-pass | |
| A-0784 | `0x310..0x31F` | `OP_TST_IMM_VAL` | arm_instructions.cpp:1527 | arm_jit.cpp:1132 | `OP_TST_` | arm_jit_arm.cpp:1504 | diff-pass | |
| A-0816 | `0x330..0x33F` | `OP_TEQ_IMM_VAL` | arm_instructions.cpp:1594 | arm_jit.cpp:1158 | `OP_TEQ_` | arm_jit_arm.cpp:1526 | diff-pass | |
| A-0848 | `0x350..0x35F` | `OP_CMP_IMM_VAL` | arm_instructions.cpp:1662 | arm_jit.cpp:1177 | `OP_CMP` | arm_jit_arm.cpp:1548 | diff-pass | |
| A-0880 | `0x370..0x37F` | `OP_CMN_IMM_VAL` | arm_instructions.cpp:1730 | arm_jit.cpp:1206 | `OP_CMN` | arm_jit_arm.cpp:1581 | diff-pass | |
| A-0896 | `0x380..0x38F` | `OP_ORR_IMM_VAL` | arm_instructions.cpp:1816 | arm_jit.cpp:942 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1308 | diff-pass | |
| A-0912 | `0x390..0x39F` | `OP_ORR_S_IMM_VAL` | arm_instructions.cpp:1871 | arm_jit.cpp:1003 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1369 | diff-pass | |
| A-0928 | `0x3A0..0x3AF` | `OP_MOV_IMM_VAL` | arm_instructions.cpp:1960 | arm_jit.cpp:1230 | `OP_MOV` | arm_jit_arm.cpp:1610 | diff-pass | |
| A-0944 | `0x3B0..0x3BF` | `OP_MOV_S_IMM_VAL` | arm_instructions.cpp:2017 | arm_jit.cpp:1255 | `OP_MOV_S` | arm_jit_arm.cpp:1644 | diff-pass | |
| A-0960 | `0x3C0..0x3CF` | `OP_BIC_IMM_VAL` | arm_instructions.cpp:2101 | arm_jit.cpp:1103 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1469 | diff-pass | |
| A-0976 | `0x3D0..0x3DF` | `OP_BIC_S_IMM_VAL` | arm_instructions.cpp:2156 | arm_jit.cpp:1113 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1479 | diff-pass | |
| A-0992 | `0x3E0..0x3EF` | `OP_MVN_IMM_VAL` | arm_instructions.cpp:2240 | arm_jit.cpp:1268 | `OP_MOV` | arm_jit_arm.cpp:1657 | diff-pass | |
| A-1008 | `0x3F0..0x3FF` | `OP_MVN_S_IMM_VAL` | arm_instructions.cpp:2294 | arm_jit.cpp:1278 | `OP_MOV_S` | arm_jit_arm.cpp:1667 | diff-pass | |

### 移位（224）

| id | 槽位 | handler | 解释器 | x86 | x86 宏体 | arm64 | 状态 | 偏差 |
|---|---|---|---|---|---|---|---|---|
| A-0000 | `0x000,0x008` | `OP_AND_LSL_IMM` | arm_instructions.cpp:292 | arm_jit.cpp:914 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1280 | diff-pass | |
| A-0001 | `0x001` | `OP_AND_LSL_REG` | arm_instructions.cpp:298 | arm_jit.cpp:915 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1281 | diff-pass | |
| A-0002 | `0x002,0x00A` | `OP_AND_LSR_IMM` | arm_instructions.cpp:304 | arm_jit.cpp:916 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1282 | diff-pass | |
| A-0003 | `0x003` | `OP_AND_LSR_REG` | arm_instructions.cpp:310 | arm_jit.cpp:917 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1283 | diff-pass | |
| A-0004 | `0x004,0x00C` | `OP_AND_ASR_IMM` | arm_instructions.cpp:316 | arm_jit.cpp:918 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1284 | diff-pass | |
| A-0005 | `0x005` | `OP_AND_ASR_REG` | arm_instructions.cpp:322 | arm_jit.cpp:919 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1285 | diff-pass | |
| A-0006 | `0x006,0x00E` | `OP_AND_ROR_IMM` | arm_instructions.cpp:328 | arm_jit.cpp:920 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1286 | diff-pass | |
| A-0007 | `0x007` | `OP_AND_ROR_REG` | arm_instructions.cpp:334 | arm_jit.cpp:921 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1287 | diff-pass | |
| A-0016 | `0x010,0x018` | `OP_AND_S_LSL_IMM` | arm_instructions.cpp:347 | arm_jit.cpp:975 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1341 | diff-pass | |
| A-0017 | `0x011` | `OP_AND_S_LSL_REG` | arm_instructions.cpp:353 | arm_jit.cpp:976 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1342 | diff-pass | |
| A-0018 | `0x012,0x01A` | `OP_AND_S_LSR_IMM` | arm_instructions.cpp:359 | arm_jit.cpp:977 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1343 | diff-pass | |
| A-0019 | `0x013` | `OP_AND_S_LSR_REG` | arm_instructions.cpp:365 | arm_jit.cpp:978 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1344 | diff-pass | |
| A-0020 | `0x014,0x01C` | `OP_AND_S_ASR_IMM` | arm_instructions.cpp:371 | arm_jit.cpp:979 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1345 | diff-pass | |
| A-0021 | `0x015` | `OP_AND_S_ASR_REG` | arm_instructions.cpp:377 | arm_jit.cpp:980 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1346 | diff-pass | |
| A-0022 | `0x016,0x01E` | `OP_AND_S_ROR_IMM` | arm_instructions.cpp:383 | arm_jit.cpp:981 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1347 | diff-pass | |
| A-0023 | `0x017` | `OP_AND_S_ROR_REG` | arm_instructions.cpp:389 | arm_jit.cpp:982 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1348 | diff-pass | |
| A-0032 | `0x020,0x028` | `OP_EOR_LSL_IMM` | arm_instructions.cpp:431 | arm_jit.cpp:924 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1290 | diff-pass | |
| A-0033 | `0x021` | `OP_EOR_LSL_REG` | arm_instructions.cpp:437 | arm_jit.cpp:925 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1291 | diff-pass | |
| A-0034 | `0x022,0x02A` | `OP_EOR_LSR_IMM` | arm_instructions.cpp:443 | arm_jit.cpp:926 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1292 | diff-pass | |
| A-0035 | `0x023` | `OP_EOR_LSR_REG` | arm_instructions.cpp:449 | arm_jit.cpp:927 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1293 | diff-pass | |
| A-0036 | `0x024,0x02C` | `OP_EOR_ASR_IMM` | arm_instructions.cpp:455 | arm_jit.cpp:928 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1294 | diff-pass | |
| A-0037 | `0x025` | `OP_EOR_ASR_REG` | arm_instructions.cpp:461 | arm_jit.cpp:929 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1295 | diff-pass | |
| A-0038 | `0x026,0x02E` | `OP_EOR_ROR_IMM` | arm_instructions.cpp:467 | arm_jit.cpp:930 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1296 | diff-pass | |
| A-0039 | `0x027` | `OP_EOR_ROR_REG` | arm_instructions.cpp:473 | arm_jit.cpp:931 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1297 | diff-pass | |
| A-0048 | `0x030,0x038` | `OP_EOR_S_LSL_IMM` | arm_instructions.cpp:486 | arm_jit.cpp:985 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1351 | diff-pass | |
| A-0049 | `0x031` | `OP_EOR_S_LSL_REG` | arm_instructions.cpp:492 | arm_jit.cpp:986 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1352 | diff-pass | |
| A-0050 | `0x032,0x03A` | `OP_EOR_S_LSR_IMM` | arm_instructions.cpp:498 | arm_jit.cpp:987 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1353 | diff-pass | |
| A-0051 | `0x033` | `OP_EOR_S_LSR_REG` | arm_instructions.cpp:504 | arm_jit.cpp:988 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1354 | diff-pass | |
| A-0052 | `0x034,0x03C` | `OP_EOR_S_ASR_IMM` | arm_instructions.cpp:510 | arm_jit.cpp:989 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1355 | diff-pass | |
| A-0053 | `0x035` | `OP_EOR_S_ASR_REG` | arm_instructions.cpp:516 | arm_jit.cpp:990 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1356 | diff-pass | |
| A-0054 | `0x036,0x03E` | `OP_EOR_S_ROR_IMM` | arm_instructions.cpp:522 | arm_jit.cpp:991 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1357 | diff-pass | |
| A-0055 | `0x037` | `OP_EOR_S_ROR_REG` | arm_instructions.cpp:528 | arm_jit.cpp:992 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1358 | diff-pass | |
| A-0064 | `0x040,0x048` | `OP_SUB_LSL_IMM` | arm_instructions.cpp:571 | arm_jit.cpp:954 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1320 | diff-pass | |
| A-0065 | `0x041` | `OP_SUB_LSL_REG` | arm_instructions.cpp:577 | arm_jit.cpp:955 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1321 | diff-pass | |
| A-0066 | `0x042,0x04A` | `OP_SUB_LSR_IMM` | arm_instructions.cpp:583 | arm_jit.cpp:956 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1322 | diff-pass | |
| A-0067 | `0x043` | `OP_SUB_LSR_REG` | arm_instructions.cpp:589 | arm_jit.cpp:957 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1323 | diff-pass | |
| A-0068 | `0x044,0x04C` | `OP_SUB_ASR_IMM` | arm_instructions.cpp:595 | arm_jit.cpp:958 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1324 | diff-pass | |
| A-0069 | `0x045` | `OP_SUB_ASR_REG` | arm_instructions.cpp:601 | arm_jit.cpp:959 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1325 | diff-pass | |
| A-0070 | `0x046,0x04E` | `OP_SUB_ROR_IMM` | arm_instructions.cpp:607 | arm_jit.cpp:960 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1326 | diff-pass | |
| A-0071 | `0x047` | `OP_SUB_ROR_REG` | arm_instructions.cpp:613 | arm_jit.cpp:961 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1327 | diff-pass | |
| A-0080 | `0x050,0x058` | `OP_SUB_S_LSL_IMM` | arm_instructions.cpp:626 | arm_jit.cpp:1015 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1381 | diff-pass | |
| A-0081 | `0x051` | `OP_SUB_S_LSL_REG` | arm_instructions.cpp:633 | arm_jit.cpp:1016 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1382 | diff-pass | |
| A-0082 | `0x052,0x05A` | `OP_SUB_S_LSR_IMM` | arm_instructions.cpp:640 | arm_jit.cpp:1017 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1383 | diff-pass | |
| A-0083 | `0x053` | `OP_SUB_S_LSR_REG` | arm_instructions.cpp:647 | arm_jit.cpp:1018 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1384 | diff-pass | |
| A-0084 | `0x054,0x05C` | `OP_SUB_S_ASR_IMM` | arm_instructions.cpp:654 | arm_jit.cpp:1019 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1385 | diff-pass | |
| A-0085 | `0x055` | `OP_SUB_S_ASR_REG` | arm_instructions.cpp:661 | arm_jit.cpp:1020 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1386 | diff-pass | |
| A-0086 | `0x056,0x05E` | `OP_SUB_S_ROR_IMM` | arm_instructions.cpp:668 | arm_jit.cpp:1021 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1387 | diff-pass | |
| A-0087 | `0x057` | `OP_SUB_S_ROR_REG` | arm_instructions.cpp:675 | arm_jit.cpp:1022 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1388 | diff-pass | |
| A-0096 | `0x060,0x068` | `OP_RSB_LSL_IMM` | arm_instructions.cpp:720 | arm_jit.cpp:964 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1330 | diff-pass | |
| A-0097 | `0x061` | `OP_RSB_LSL_REG` | arm_instructions.cpp:726 | arm_jit.cpp:965 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1331 | diff-pass | |
| A-0098 | `0x062,0x06A` | `OP_RSB_LSR_IMM` | arm_instructions.cpp:732 | arm_jit.cpp:966 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1332 | diff-pass | |
| A-0099 | `0x063` | `OP_RSB_LSR_REG` | arm_instructions.cpp:738 | arm_jit.cpp:967 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1333 | diff-pass | |
| A-0100 | `0x064,0x06C` | `OP_RSB_ASR_IMM` | arm_instructions.cpp:744 | arm_jit.cpp:968 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1334 | diff-pass | |
| A-0101 | `0x065` | `OP_RSB_ASR_REG` | arm_instructions.cpp:750 | arm_jit.cpp:969 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1335 | diff-pass | |
| A-0102 | `0x066,0x06E` | `OP_RSB_ROR_IMM` | arm_instructions.cpp:756 | arm_jit.cpp:970 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1336 | diff-pass | |
| A-0103 | `0x067` | `OP_RSB_ROR_REG` | arm_instructions.cpp:762 | arm_jit.cpp:971 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1337 | diff-pass | |
| A-0112 | `0x070,0x078` | `OP_RSB_S_LSL_IMM` | arm_instructions.cpp:774 | arm_jit.cpp:1025 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1391 | diff-pass | |
| A-0113 | `0x071` | `OP_RSB_S_LSL_REG` | arm_instructions.cpp:782 | arm_jit.cpp:1026 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1392 | diff-pass | |
| A-0114 | `0x072,0x07A` | `OP_RSB_S_LSR_IMM` | arm_instructions.cpp:789 | arm_jit.cpp:1027 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1393 | diff-pass | |
| A-0115 | `0x073` | `OP_RSB_S_LSR_REG` | arm_instructions.cpp:796 | arm_jit.cpp:1028 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1394 | diff-pass | |
| A-0116 | `0x074,0x07C` | `OP_RSB_S_ASR_IMM` | arm_instructions.cpp:803 | arm_jit.cpp:1029 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1395 | diff-pass | |
| A-0117 | `0x075` | `OP_RSB_S_ASR_REG` | arm_instructions.cpp:810 | arm_jit.cpp:1030 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1396 | diff-pass | |
| A-0118 | `0x076,0x07E` | `OP_RSB_S_ROR_IMM` | arm_instructions.cpp:817 | arm_jit.cpp:1031 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1397 | diff-pass | |
| A-0119 | `0x077` | `OP_RSB_S_ROR_REG` | arm_instructions.cpp:824 | arm_jit.cpp:1032 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1398 | diff-pass | |
| A-0128 | `0x080,0x088` | `OP_ADD_LSL_IMM` | arm_instructions.cpp:869 | arm_jit.cpp:944 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1310 | diff-pass | |
| A-0129 | `0x081` | `OP_ADD_LSL_REG` | arm_instructions.cpp:875 | arm_jit.cpp:945 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1311 | diff-pass | |
| A-0130 | `0x082,0x08A` | `OP_ADD_LSR_IMM` | arm_instructions.cpp:881 | arm_jit.cpp:946 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1312 | diff-pass | |
| A-0131 | `0x083` | `OP_ADD_LSR_REG` | arm_instructions.cpp:887 | arm_jit.cpp:947 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1313 | diff-pass | |
| A-0132 | `0x084,0x08C` | `OP_ADD_ASR_IMM` | arm_instructions.cpp:893 | arm_jit.cpp:948 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1314 | diff-pass | |
| A-0133 | `0x085` | `OP_ADD_ASR_REG` | arm_instructions.cpp:899 | arm_jit.cpp:949 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1315 | diff-pass | |
| A-0134 | `0x086,0x08E` | `OP_ADD_ROR_IMM` | arm_instructions.cpp:905 | arm_jit.cpp:950 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1316 | diff-pass | |
| A-0135 | `0x087` | `OP_ADD_ROR_REG` | arm_instructions.cpp:911 | arm_jit.cpp:951 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1317 | diff-pass | |
| A-0144 | `0x090,0x098` | `OP_ADD_S_LSL_IMM` | arm_instructions.cpp:924 | arm_jit.cpp:1005 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1371 | diff-pass | |
| A-0145 | `0x091` | `OP_ADD_S_LSL_REG` | arm_instructions.cpp:931 | arm_jit.cpp:1006 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1372 | diff-pass | |
| A-0146 | `0x092,0x09A` | `OP_ADD_S_LSR_IMM` | arm_instructions.cpp:938 | arm_jit.cpp:1007 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1373 | diff-pass | |
| A-0147 | `0x093` | `OP_ADD_S_LSR_REG` | arm_instructions.cpp:945 | arm_jit.cpp:1008 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1374 | diff-pass | |
| A-0148 | `0x094,0x09C` | `OP_ADD_S_ASR_IMM` | arm_instructions.cpp:952 | arm_jit.cpp:1009 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1375 | diff-pass | |
| A-0149 | `0x095` | `OP_ADD_S_ASR_REG` | arm_instructions.cpp:959 | arm_jit.cpp:1010 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1376 | diff-pass | |
| A-0150 | `0x096,0x09E` | `OP_ADD_S_ROR_IMM` | arm_instructions.cpp:966 | arm_jit.cpp:1011 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1377 | diff-pass | |
| A-0151 | `0x097` | `OP_ADD_S_ROR_REG` | arm_instructions.cpp:973 | arm_jit.cpp:1012 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1378 | diff-pass | |
| A-0160 | `0x0A0,0x0A8` | `OP_ADC_LSL_IMM` | arm_instructions.cpp:1028 | arm_jit.cpp:1035 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1401 | diff-pass | |
| A-0161 | `0x0A1` | `OP_ADC_LSL_REG` | arm_instructions.cpp:1034 | arm_jit.cpp:1036 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1402 | diff-pass | |
| A-0162 | `0x0A2,0x0AA` | `OP_ADC_LSR_IMM` | arm_instructions.cpp:1040 | arm_jit.cpp:1037 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1403 | diff-pass | |
| A-0163 | `0x0A3` | `OP_ADC_LSR_REG` | arm_instructions.cpp:1046 | arm_jit.cpp:1038 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1404 | diff-pass | |
| A-0164 | `0x0A4,0x0AC` | `OP_ADC_ASR_IMM` | arm_instructions.cpp:1052 | arm_jit.cpp:1039 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1405 | diff-pass | |
| A-0165 | `0x0A5` | `OP_ADC_ASR_REG` | arm_instructions.cpp:1058 | arm_jit.cpp:1040 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1406 | diff-pass | |
| A-0166 | `0x0A6,0x0AE` | `OP_ADC_ROR_IMM` | arm_instructions.cpp:1064 | arm_jit.cpp:1041 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1407 | diff-pass | |
| A-0167 | `0x0A7` | `OP_ADC_ROR_REG` | arm_instructions.cpp:1070 | arm_jit.cpp:1042 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1408 | diff-pass | |
| A-0176 | `0x0B0,0x0B8` | `OP_ADC_S_LSL_IMM` | arm_instructions.cpp:1083 | arm_jit.cpp:1045 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1411 | diff-pass | |
| A-0177 | `0x0B1` | `OP_ADC_S_LSL_REG` | arm_instructions.cpp:1090 | arm_jit.cpp:1046 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1412 | diff-pass | |
| A-0178 | `0x0B2,0x0BA` | `OP_ADC_S_LSR_IMM` | arm_instructions.cpp:1097 | arm_jit.cpp:1047 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1413 | diff-pass | |
| A-0179 | `0x0B3` | `OP_ADC_S_LSR_REG` | arm_instructions.cpp:1104 | arm_jit.cpp:1048 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1414 | diff-pass | |
| A-0180 | `0x0B4,0x0BC` | `OP_ADC_S_ASR_IMM` | arm_instructions.cpp:1111 | arm_jit.cpp:1049 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1415 | diff-pass | |
| A-0181 | `0x0B5` | `OP_ADC_S_ASR_REG` | arm_instructions.cpp:1118 | arm_jit.cpp:1050 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1416 | diff-pass | |
| A-0182 | `0x0B6,0x0BE` | `OP_ADC_S_ROR_IMM` | arm_instructions.cpp:1125 | arm_jit.cpp:1051 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1417 | diff-pass | |
| A-0183 | `0x0B7` | `OP_ADC_S_ROR_REG` | arm_instructions.cpp:1132 | arm_jit.cpp:1052 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1418 | diff-pass | |
| A-0192 | `0x0C0,0x0C8` | `OP_SBC_LSL_IMM` | arm_instructions.cpp:1188 | arm_jit.cpp:1055 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1421 | diff-pass | |
| A-0193 | `0x0C1` | `OP_SBC_LSL_REG` | arm_instructions.cpp:1194 | arm_jit.cpp:1056 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1422 | diff-pass | |
| A-0194 | `0x0C2,0x0CA` | `OP_SBC_LSR_IMM` | arm_instructions.cpp:1200 | arm_jit.cpp:1057 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1423 | diff-pass | |
| A-0195 | `0x0C3` | `OP_SBC_LSR_REG` | arm_instructions.cpp:1206 | arm_jit.cpp:1058 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1424 | diff-pass | |
| A-0196 | `0x0C4,0x0CC` | `OP_SBC_ASR_IMM` | arm_instructions.cpp:1212 | arm_jit.cpp:1059 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1425 | diff-pass | |
| A-0197 | `0x0C5` | `OP_SBC_ASR_REG` | arm_instructions.cpp:1218 | arm_jit.cpp:1060 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1426 | diff-pass | |
| A-0198 | `0x0C6,0x0CE` | `OP_SBC_ROR_IMM` | arm_instructions.cpp:1224 | arm_jit.cpp:1061 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1427 | diff-pass | |
| A-0199 | `0x0C7` | `OP_SBC_ROR_REG` | arm_instructions.cpp:1230 | arm_jit.cpp:1062 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1428 | diff-pass | |
| A-0208 | `0x0D0,0x0D8` | `OP_SBC_S_LSL_IMM` | arm_instructions.cpp:1243 | arm_jit.cpp:1065 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1431 | diff-pass | |
| A-0209 | `0x0D1` | `OP_SBC_S_LSL_REG` | arm_instructions.cpp:1250 | arm_jit.cpp:1066 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1432 | diff-pass | |
| A-0210 | `0x0D2,0x0DA` | `OP_SBC_S_LSR_IMM` | arm_instructions.cpp:1257 | arm_jit.cpp:1067 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1433 | diff-pass | |
| A-0211 | `0x0D3` | `OP_SBC_S_LSR_REG` | arm_instructions.cpp:1264 | arm_jit.cpp:1068 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1434 | diff-pass | |
| A-0212 | `0x0D4,0x0DC` | `OP_SBC_S_ASR_IMM` | arm_instructions.cpp:1271 | arm_jit.cpp:1069 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1435 | diff-pass | |
| A-0213 | `0x0D5` | `OP_SBC_S_ASR_REG` | arm_instructions.cpp:1278 | arm_jit.cpp:1070 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1436 | diff-pass | |
| A-0214 | `0x0D6,0x0DE` | `OP_SBC_S_ROR_IMM` | arm_instructions.cpp:1285 | arm_jit.cpp:1071 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1437 | diff-pass | |
| A-0215 | `0x0D7` | `OP_SBC_S_ROR_REG` | arm_instructions.cpp:1292 | arm_jit.cpp:1072 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1438 | diff-pass | |
| A-0224 | `0x0E0,0x0E8` | `OP_RSC_LSL_IMM` | arm_instructions.cpp:1348 | arm_jit.cpp:1075 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1441 | diff-pass | |
| A-0225 | `0x0E1` | `OP_RSC_LSL_REG` | arm_instructions.cpp:1354 | arm_jit.cpp:1076 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1442 | diff-pass | |
| A-0226 | `0x0E2,0x0EA` | `OP_RSC_LSR_IMM` | arm_instructions.cpp:1360 | arm_jit.cpp:1077 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1443 | diff-pass | |
| A-0227 | `0x0E3` | `OP_RSC_LSR_REG` | arm_instructions.cpp:1366 | arm_jit.cpp:1078 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1444 | diff-pass | |
| A-0228 | `0x0E4,0x0EC` | `OP_RSC_ASR_IMM` | arm_instructions.cpp:1372 | arm_jit.cpp:1079 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1445 | diff-pass | |
| A-0229 | `0x0E5` | `OP_RSC_ASR_REG` | arm_instructions.cpp:1378 | arm_jit.cpp:1080 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1446 | diff-pass | |
| A-0230 | `0x0E6,0x0EE` | `OP_RSC_ROR_IMM` | arm_instructions.cpp:1384 | arm_jit.cpp:1081 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1447 | diff-pass | |
| A-0231 | `0x0E7` | `OP_RSC_ROR_REG` | arm_instructions.cpp:1390 | arm_jit.cpp:1082 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1448 | diff-pass | |
| A-0240 | `0x0F0,0x0F8` | `OP_RSC_S_LSL_IMM` | arm_instructions.cpp:1403 | arm_jit.cpp:1085 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1451 | diff-pass | |
| A-0241 | `0x0F1` | `OP_RSC_S_LSL_REG` | arm_instructions.cpp:1410 | arm_jit.cpp:1086 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1452 | diff-pass | |
| A-0242 | `0x0F2,0x0FA` | `OP_RSC_S_LSR_IMM` | arm_instructions.cpp:1417 | arm_jit.cpp:1087 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1453 | diff-pass | |
| A-0243 | `0x0F3` | `OP_RSC_S_LSR_REG` | arm_instructions.cpp:1424 | arm_jit.cpp:1088 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1454 | diff-pass | |
| A-0244 | `0x0F4,0x0FC` | `OP_RSC_S_ASR_IMM` | arm_instructions.cpp:1431 | arm_jit.cpp:1089 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1455 | diff-pass | |
| A-0245 | `0x0F5` | `OP_RSC_S_ASR_REG` | arm_instructions.cpp:1438 | arm_jit.cpp:1090 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1456 | diff-pass | |
| A-0246 | `0x0F6,0x0FE` | `OP_RSC_S_ROR_IMM` | arm_instructions.cpp:1445 | arm_jit.cpp:1091 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1457 | diff-pass | |
| A-0247 | `0x0F7` | `OP_RSC_S_ROR_REG` | arm_instructions.cpp:1452 | arm_jit.cpp:1092 | `OP_ARITHMETIC_R` | arm_jit_arm.cpp:1458 | diff-pass | |
| A-0272 | `0x110,0x118` | `OP_TST_LSL_IMM` | arm_instructions.cpp:1479 | arm_jit.cpp:1124 | `OP_TST_` | arm_jit_arm.cpp:1496 | diff-pass | |
| A-0273 | `0x111` | `OP_TST_LSL_REG` | arm_instructions.cpp:1485 | arm_jit.cpp:1125 | `OP_TST_` | arm_jit_arm.cpp:1497 | diff-pass | |
| A-0274 | `0x112,0x11A` | `OP_TST_LSR_IMM` | arm_instructions.cpp:1491 | arm_jit.cpp:1126 | `OP_TST_` | arm_jit_arm.cpp:1498 | diff-pass | |
| A-0275 | `0x113` | `OP_TST_LSR_REG` | arm_instructions.cpp:1497 | arm_jit.cpp:1127 | `OP_TST_` | arm_jit_arm.cpp:1499 | diff-pass | |
| A-0276 | `0x114,0x11C` | `OP_TST_ASR_IMM` | arm_instructions.cpp:1503 | arm_jit.cpp:1128 | `OP_TST_` | arm_jit_arm.cpp:1500 | diff-pass | |
| A-0277 | `0x115` | `OP_TST_ASR_REG` | arm_instructions.cpp:1509 | arm_jit.cpp:1129 | `OP_TST_` | arm_jit_arm.cpp:1501 | diff-pass | |
| A-0278 | `0x116,0x11E` | `OP_TST_ROR_IMM` | arm_instructions.cpp:1515 | arm_jit.cpp:1130 | `OP_TST_` | arm_jit_arm.cpp:1502 | diff-pass | |
| A-0279 | `0x117` | `OP_TST_ROR_REG` | arm_instructions.cpp:1521 | arm_jit.cpp:1131 | `OP_TST_` | arm_jit_arm.cpp:1503 | diff-pass | |
| A-0304 | `0x130,0x138` | `OP_TEQ_LSL_IMM` | arm_instructions.cpp:1546 | arm_jit.cpp:1150 | `OP_TEQ_` | arm_jit_arm.cpp:1518 | diff-pass | |
| A-0305 | `0x131` | `OP_TEQ_LSL_REG` | arm_instructions.cpp:1552 | arm_jit.cpp:1151 | `OP_TEQ_` | arm_jit_arm.cpp:1519 | diff-pass | |
| A-0306 | `0x132,0x13A` | `OP_TEQ_LSR_IMM` | arm_instructions.cpp:1558 | arm_jit.cpp:1152 | `OP_TEQ_` | arm_jit_arm.cpp:1520 | diff-pass | |
| A-0307 | `0x133` | `OP_TEQ_LSR_REG` | arm_instructions.cpp:1564 | arm_jit.cpp:1153 | `OP_TEQ_` | arm_jit_arm.cpp:1521 | diff-pass | |
| A-0308 | `0x134,0x13C` | `OP_TEQ_ASR_IMM` | arm_instructions.cpp:1570 | arm_jit.cpp:1154 | `OP_TEQ_` | arm_jit_arm.cpp:1522 | diff-pass | |
| A-0309 | `0x135` | `OP_TEQ_ASR_REG` | arm_instructions.cpp:1576 | arm_jit.cpp:1155 | `OP_TEQ_` | arm_jit_arm.cpp:1523 | diff-pass | |
| A-0310 | `0x136,0x13E` | `OP_TEQ_ROR_IMM` | arm_instructions.cpp:1582 | arm_jit.cpp:1156 | `OP_TEQ_` | arm_jit_arm.cpp:1524 | diff-pass | |
| A-0311 | `0x137` | `OP_TEQ_ROR_REG` | arm_instructions.cpp:1588 | arm_jit.cpp:1157 | `OP_TEQ_` | arm_jit_arm.cpp:1525 | diff-pass | |
| A-0336 | `0x150,0x158` | `OP_CMP_LSL_IMM` | arm_instructions.cpp:1614 | arm_jit.cpp:1169 | `OP_CMP` | arm_jit_arm.cpp:1540 | diff-pass | |
| A-0337 | `0x151` | `OP_CMP_LSL_REG` | arm_instructions.cpp:1620 | arm_jit.cpp:1170 | `OP_CMP` | arm_jit_arm.cpp:1541 | diff-pass | |
| A-0338 | `0x152,0x15A` | `OP_CMP_LSR_IMM` | arm_instructions.cpp:1626 | arm_jit.cpp:1171 | `OP_CMP` | arm_jit_arm.cpp:1542 | diff-pass | |
| A-0339 | `0x153` | `OP_CMP_LSR_REG` | arm_instructions.cpp:1632 | arm_jit.cpp:1172 | `OP_CMP` | arm_jit_arm.cpp:1543 | diff-pass | |
| A-0340 | `0x154,0x15C` | `OP_CMP_ASR_IMM` | arm_instructions.cpp:1638 | arm_jit.cpp:1173 | `OP_CMP` | arm_jit_arm.cpp:1544 | diff-pass | |
| A-0341 | `0x155` | `OP_CMP_ASR_REG` | arm_instructions.cpp:1644 | arm_jit.cpp:1174 | `OP_CMP` | arm_jit_arm.cpp:1545 | diff-pass | |
| A-0342 | `0x156,0x15E` | `OP_CMP_ROR_IMM` | arm_instructions.cpp:1650 | arm_jit.cpp:1175 | `OP_CMP` | arm_jit_arm.cpp:1546 | diff-pass | |
| A-0343 | `0x157` | `OP_CMP_ROR_REG` | arm_instructions.cpp:1656 | arm_jit.cpp:1176 | `OP_CMP` | arm_jit_arm.cpp:1547 | diff-pass | |
| A-0368 | `0x170,0x178` | `OP_CMN_LSL_IMM` | arm_instructions.cpp:1682 | arm_jit.cpp:1198 | `OP_CMN` | arm_jit_arm.cpp:1573 | diff-pass | |
| A-0369 | `0x171` | `OP_CMN_LSL_REG` | arm_instructions.cpp:1688 | arm_jit.cpp:1199 | `OP_CMN` | arm_jit_arm.cpp:1574 | diff-pass | |
| A-0370 | `0x172,0x17A` | `OP_CMN_LSR_IMM` | arm_instructions.cpp:1694 | arm_jit.cpp:1200 | `OP_CMN` | arm_jit_arm.cpp:1575 | diff-pass | |
| A-0371 | `0x173` | `OP_CMN_LSR_REG` | arm_instructions.cpp:1700 | arm_jit.cpp:1201 | `OP_CMN` | arm_jit_arm.cpp:1576 | diff-pass | |
| A-0372 | `0x174,0x17C` | `OP_CMN_ASR_IMM` | arm_instructions.cpp:1706 | arm_jit.cpp:1202 | `OP_CMN` | arm_jit_arm.cpp:1577 | diff-pass | |
| A-0373 | `0x175` | `OP_CMN_ASR_REG` | arm_instructions.cpp:1712 | arm_jit.cpp:1203 | `OP_CMN` | arm_jit_arm.cpp:1578 | diff-pass | |
| A-0374 | `0x176,0x17E` | `OP_CMN_ROR_IMM` | arm_instructions.cpp:1718 | arm_jit.cpp:1204 | `OP_CMN` | arm_jit_arm.cpp:1579 | diff-pass | |
| A-0375 | `0x177` | `OP_CMN_ROR_REG` | arm_instructions.cpp:1724 | arm_jit.cpp:1205 | `OP_CMN` | arm_jit_arm.cpp:1580 | diff-pass | |
| A-0384 | `0x180,0x188` | `OP_ORR_LSL_IMM` | arm_instructions.cpp:1768 | arm_jit.cpp:934 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1300 | diff-pass | |
| A-0385 | `0x181` | `OP_ORR_LSL_REG` | arm_instructions.cpp:1774 | arm_jit.cpp:935 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1301 | diff-pass | |
| A-0386 | `0x182,0x18A` | `OP_ORR_LSR_IMM` | arm_instructions.cpp:1780 | arm_jit.cpp:936 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1302 | diff-pass | |
| A-0387 | `0x183` | `OP_ORR_LSR_REG` | arm_instructions.cpp:1786 | arm_jit.cpp:937 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1303 | diff-pass | |
| A-0388 | `0x184,0x18C` | `OP_ORR_ASR_IMM` | arm_instructions.cpp:1792 | arm_jit.cpp:938 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1304 | diff-pass | |
| A-0389 | `0x185` | `OP_ORR_ASR_REG` | arm_instructions.cpp:1798 | arm_jit.cpp:939 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1305 | diff-pass | |
| A-0390 | `0x186,0x18E` | `OP_ORR_ROR_IMM` | arm_instructions.cpp:1804 | arm_jit.cpp:940 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1306 | diff-pass | |
| A-0391 | `0x187` | `OP_ORR_ROR_REG` | arm_instructions.cpp:1810 | arm_jit.cpp:941 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1307 | diff-pass | |
| A-0400 | `0x190,0x198` | `OP_ORR_S_LSL_IMM` | arm_instructions.cpp:1823 | arm_jit.cpp:995 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1361 | diff-pass | |
| A-0401 | `0x191` | `OP_ORR_S_LSL_REG` | arm_instructions.cpp:1829 | arm_jit.cpp:996 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1362 | diff-pass | |
| A-0402 | `0x192,0x19A` | `OP_ORR_S_LSR_IMM` | arm_instructions.cpp:1835 | arm_jit.cpp:997 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1363 | diff-pass | |
| A-0403 | `0x193` | `OP_ORR_S_LSR_REG` | arm_instructions.cpp:1841 | arm_jit.cpp:998 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1364 | diff-pass | |
| A-0404 | `0x194,0x19C` | `OP_ORR_S_ASR_IMM` | arm_instructions.cpp:1847 | arm_jit.cpp:999 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1365 | diff-pass | |
| A-0405 | `0x195` | `OP_ORR_S_ASR_REG` | arm_instructions.cpp:1853 | arm_jit.cpp:1000 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1366 | diff-pass | |
| A-0406 | `0x196,0x19E` | `OP_ORR_S_ROR_IMM` | arm_instructions.cpp:1859 | arm_jit.cpp:1001 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1367 | diff-pass | |
| A-0407 | `0x197` | `OP_ORR_S_ROR_REG` | arm_instructions.cpp:1865 | arm_jit.cpp:1002 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1368 | diff-pass | |
| A-0416 | `0x1A0,0x1A8` | `OP_MOV_LSL_IMM` | arm_instructions.cpp:1907 | arm_jit.cpp:1222 | — | arm_jit_arm.cpp:1602 | diff-pass | |
| A-0417 | `0x1A1` | `OP_MOV_LSL_REG` | arm_instructions.cpp:1916 | arm_jit.cpp:1223 | `OP_MOV` | arm_jit_arm.cpp:1603 | diff-pass | |
| A-0418 | `0x1A2,0x1AA` | `OP_MOV_LSR_IMM` | arm_instructions.cpp:1923 | arm_jit.cpp:1224 | `OP_MOV` | arm_jit_arm.cpp:1604 | diff-pass | |
| A-0419 | `0x1A3` | `OP_MOV_LSR_REG` | arm_instructions.cpp:1929 | arm_jit.cpp:1225 | `OP_MOV` | arm_jit_arm.cpp:1605 | diff-pass | |
| A-0420 | `0x1A4,0x1AC` | `OP_MOV_ASR_IMM` | arm_instructions.cpp:1936 | arm_jit.cpp:1226 | `OP_MOV` | arm_jit_arm.cpp:1606 | diff-pass | |
| A-0421 | `0x1A5` | `OP_MOV_ASR_REG` | arm_instructions.cpp:1942 | arm_jit.cpp:1227 | `OP_MOV` | arm_jit_arm.cpp:1607 | diff-pass | |
| A-0422 | `0x1A6,0x1AE` | `OP_MOV_ROR_IMM` | arm_instructions.cpp:1948 | arm_jit.cpp:1228 | `OP_MOV` | arm_jit_arm.cpp:1608 | diff-pass | |
| A-0423 | `0x1A7` | `OP_MOV_ROR_REG` | arm_instructions.cpp:1954 | arm_jit.cpp:1229 | `OP_MOV` | arm_jit_arm.cpp:1609 | diff-pass | |
| A-0432 | `0x1B0,0x1B8` | `OP_MOV_S_LSL_IMM` | arm_instructions.cpp:1967 | arm_jit.cpp:1247 | `OP_MOV_S` | arm_jit_arm.cpp:1636 | diff-pass | |
| A-0433 | `0x1B1` | `OP_MOV_S_LSL_REG` | arm_instructions.cpp:1973 | arm_jit.cpp:1248 | `OP_MOV_S` | arm_jit_arm.cpp:1637 | diff-pass | |
| A-0434 | `0x1B2,0x1BA` | `OP_MOV_S_LSR_IMM` | arm_instructions.cpp:1980 | arm_jit.cpp:1249 | `OP_MOV_S` | arm_jit_arm.cpp:1638 | diff-pass | |
| A-0435 | `0x1B3` | `OP_MOV_S_LSR_REG` | arm_instructions.cpp:1986 | arm_jit.cpp:1250 | `OP_MOV_S` | arm_jit_arm.cpp:1639 | diff-pass | |
| A-0436 | `0x1B4,0x1BC` | `OP_MOV_S_ASR_IMM` | arm_instructions.cpp:1993 | arm_jit.cpp:1251 | `OP_MOV_S` | arm_jit_arm.cpp:1640 | diff-pass | |
| A-0437 | `0x1B5` | `OP_MOV_S_ASR_REG` | arm_instructions.cpp:1999 | arm_jit.cpp:1252 | `OP_MOV_S` | arm_jit_arm.cpp:1641 | diff-pass | |
| A-0438 | `0x1B6,0x1BE` | `OP_MOV_S_ROR_IMM` | arm_instructions.cpp:2005 | arm_jit.cpp:1253 | `OP_MOV_S` | arm_jit_arm.cpp:1642 | diff-pass | |
| A-0439 | `0x1B7` | `OP_MOV_S_ROR_REG` | arm_instructions.cpp:2011 | arm_jit.cpp:1254 | `OP_MOV_S` | arm_jit_arm.cpp:1643 | diff-pass | |
| A-0448 | `0x1C0,0x1C8` | `OP_BIC_LSL_IMM` | arm_instructions.cpp:2053 | arm_jit.cpp:1095 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1461 | diff-pass | |
| A-0449 | `0x1C1` | `OP_BIC_LSL_REG` | arm_instructions.cpp:2059 | arm_jit.cpp:1096 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1462 | diff-pass | |
| A-0450 | `0x1C2,0x1CA` | `OP_BIC_LSR_IMM` | arm_instructions.cpp:2065 | arm_jit.cpp:1097 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1463 | diff-pass | |
| A-0451 | `0x1C3` | `OP_BIC_LSR_REG` | arm_instructions.cpp:2071 | arm_jit.cpp:1098 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1464 | diff-pass | |
| A-0452 | `0x1C4,0x1CC` | `OP_BIC_ASR_IMM` | arm_instructions.cpp:2077 | arm_jit.cpp:1099 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1465 | diff-pass | |
| A-0453 | `0x1C5` | `OP_BIC_ASR_REG` | arm_instructions.cpp:2083 | arm_jit.cpp:1100 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1466 | diff-pass | |
| A-0454 | `0x1C6,0x1CE` | `OP_BIC_ROR_IMM` | arm_instructions.cpp:2089 | arm_jit.cpp:1101 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1467 | diff-pass | |
| A-0455 | `0x1C7` | `OP_BIC_ROR_REG` | arm_instructions.cpp:2095 | arm_jit.cpp:1102 | `OP_ARITHMETIC` | arm_jit_arm.cpp:1468 | diff-pass | |
| A-0464 | `0x1D0,0x1D8` | `OP_BIC_S_LSL_IMM` | arm_instructions.cpp:2108 | arm_jit.cpp:1105 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1471 | diff-pass | |
| A-0465 | `0x1D1` | `OP_BIC_S_LSL_REG` | arm_instructions.cpp:2114 | arm_jit.cpp:1106 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1472 | diff-pass | |
| A-0466 | `0x1D2,0x1DA` | `OP_BIC_S_LSR_IMM` | arm_instructions.cpp:2120 | arm_jit.cpp:1107 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1473 | diff-pass | |
| A-0467 | `0x1D3` | `OP_BIC_S_LSR_REG` | arm_instructions.cpp:2126 | arm_jit.cpp:1108 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1474 | diff-pass | |
| A-0468 | `0x1D4,0x1DC` | `OP_BIC_S_ASR_IMM` | arm_instructions.cpp:2132 | arm_jit.cpp:1109 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1475 | diff-pass | |
| A-0469 | `0x1D5` | `OP_BIC_S_ASR_REG` | arm_instructions.cpp:2138 | arm_jit.cpp:1110 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1476 | diff-pass | |
| A-0470 | `0x1D6,0x1DE` | `OP_BIC_S_ROR_IMM` | arm_instructions.cpp:2144 | arm_jit.cpp:1111 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1477 | diff-pass | |
| A-0471 | `0x1D7` | `OP_BIC_S_ROR_REG` | arm_instructions.cpp:2150 | arm_jit.cpp:1112 | `OP_ARITHMETIC_S` | arm_jit_arm.cpp:1478 | diff-pass | |
| A-0480 | `0x1E0,0x1E8` | `OP_MVN_LSL_IMM` | arm_instructions.cpp:2192 | arm_jit.cpp:1260 | `OP_MOV` | arm_jit_arm.cpp:1649 | diff-pass | |
| A-0481 | `0x1E1` | `OP_MVN_LSL_REG` | arm_instructions.cpp:2198 | arm_jit.cpp:1261 | `OP_MOV` | arm_jit_arm.cpp:1650 | diff-pass | |
| A-0482 | `0x1E2,0x1EA` | `OP_MVN_LSR_IMM` | arm_instructions.cpp:2204 | arm_jit.cpp:1262 | `OP_MOV` | arm_jit_arm.cpp:1651 | diff-pass | |
| A-0483 | `0x1E3` | `OP_MVN_LSR_REG` | arm_instructions.cpp:2210 | arm_jit.cpp:1263 | `OP_MOV` | arm_jit_arm.cpp:1652 | diff-pass | |
| A-0484 | `0x1E4,0x1EC` | `OP_MVN_ASR_IMM` | arm_instructions.cpp:2216 | arm_jit.cpp:1264 | `OP_MOV` | arm_jit_arm.cpp:1653 | diff-pass | |
| A-0485 | `0x1E5` | `OP_MVN_ASR_REG` | arm_instructions.cpp:2222 | arm_jit.cpp:1265 | `OP_MOV` | arm_jit_arm.cpp:1654 | diff-pass | |
| A-0486 | `0x1E6,0x1EE` | `OP_MVN_ROR_IMM` | arm_instructions.cpp:2228 | arm_jit.cpp:1266 | `OP_MOV` | arm_jit_arm.cpp:1655 | diff-pass | |
| A-0487 | `0x1E7` | `OP_MVN_ROR_REG` | arm_instructions.cpp:2234 | arm_jit.cpp:1267 | `OP_MOV` | arm_jit_arm.cpp:1656 | diff-pass | |
| A-0496 | `0x1F0,0x1F8` | `OP_MVN_S_LSL_IMM` | arm_instructions.cpp:2246 | arm_jit.cpp:1270 | `OP_MOV_S` | arm_jit_arm.cpp:1659 | diff-pass | |
| A-0497 | `0x1F1` | `OP_MVN_S_LSL_REG` | arm_instructions.cpp:2252 | arm_jit.cpp:1271 | `OP_MOV_S` | arm_jit_arm.cpp:1660 | diff-pass | |
| A-0498 | `0x1F2,0x1FA` | `OP_MVN_S_LSR_IMM` | arm_instructions.cpp:2258 | arm_jit.cpp:1272 | `OP_MOV_S` | arm_jit_arm.cpp:1661 | diff-pass | |
| A-0499 | `0x1F3` | `OP_MVN_S_LSR_REG` | arm_instructions.cpp:2264 | arm_jit.cpp:1273 | `OP_MOV_S` | arm_jit_arm.cpp:1662 | diff-pass | |
| A-0500 | `0x1F4,0x1FC` | `OP_MVN_S_ASR_IMM` | arm_instructions.cpp:2270 | arm_jit.cpp:1274 | `OP_MOV_S` | arm_jit_arm.cpp:1663 | diff-pass | |
| A-0501 | `0x1F5` | `OP_MVN_S_ASR_REG` | arm_instructions.cpp:2276 | arm_jit.cpp:1275 | `OP_MOV_S` | arm_jit_arm.cpp:1664 | diff-pass | |
| A-0502 | `0x1F6,0x1FE` | `OP_MVN_S_ROR_IMM` | arm_instructions.cpp:2282 | arm_jit.cpp:1276 | `OP_MOV_S` | arm_jit_arm.cpp:1665 | diff-pass | |
| A-0503 | `0x1F7` | `OP_MVN_S_ROR_REG` | arm_instructions.cpp:2288 | arm_jit.cpp:1277 | `OP_MOV_S` | arm_jit_arm.cpp:1666 | diff-pass | |

### 访存（174）

| id | 槽位 | handler | 解释器 | x86 | x86 宏体 | arm64 | 状态 | 偏差 |
|---|---|---|---|---|---|---|---|---|
| A-0011 | `0x00B` | `OP_STRH_POS_INDE_M_REG_OFF` | arm_instructions.cpp:2739 | arm_jit.cpp:1962 | `OP_STR_` | arm_jit_arm.cpp:2346 | diff-pass | |
| A-0013 | `0x00D,0x00F,0x04D,0x04F,0x08D,0x08F,…（共 8 槽）` | `OP_LDRD_STRD_POST_INDEX` | arm_instructions.cpp:6008 | arm_jit.cpp:2033 | — | arm_jit_arm.cpp:2420 | diff-pass | |
| A-0027 | `0x01B` | `OP_LDRH_POS_INDE_M_REG_OFF` | arm_instructions.cpp:2631 | arm_jit.cpp:1791 | `OP_LDR_` | arm_jit_arm.cpp:2173 | diff-pass | |
| A-0029 | `0x01D` | `OP_LDRSB_POS_INDE_M_REG_OFF` | arm_instructions.cpp:2957 | arm_jit.cpp:1854 | `OP_LDR_` | arm_jit_arm.cpp:2236 | diff-pass | |
| A-0031 | `0x01F` | `OP_LDRSH_POS_INDE_M_REG_OFF` | arm_instructions.cpp:2848 | arm_jit.cpp:1806 | `OP_LDR_` | arm_jit_arm.cpp:2188 | diff-pass | |
| A-0075 | `0x04B` | `OP_STRH_POS_INDE_M_IMM_OFF` | arm_instructions.cpp:2721 | arm_jit.cpp:1960 | `OP_STR_` | arm_jit_arm.cpp:2344 | diff-pass | |
| A-0091 | `0x05B` | `OP_LDRH_POS_INDE_M_IMM_OFF` | arm_instructions.cpp:2613 | arm_jit.cpp:1789 | `OP_LDR_` | arm_jit_arm.cpp:2171 | diff-pass | |
| A-0093 | `0x05D` | `OP_LDRSB_POS_INDE_M_IMM_OFF` | arm_instructions.cpp:2939 | arm_jit.cpp:1852 | `OP_LDR_` | arm_jit_arm.cpp:2234 | diff-pass | |
| A-0095 | `0x05F` | `OP_LDRSH_POS_INDE_M_IMM_OFF` | arm_instructions.cpp:2830 | arm_jit.cpp:1804 | `OP_LDR_` | arm_jit_arm.cpp:2186 | diff-pass | |
| A-0139 | `0x08B` | `OP_STRH_POS_INDE_P_REG_OFF` | arm_instructions.cpp:2730 | arm_jit.cpp:1961 | `OP_STR_` | arm_jit_arm.cpp:2345 | diff-pass | |
| A-0155 | `0x09B` | `OP_LDRH_POS_INDE_P_REG_OFF` | arm_instructions.cpp:2622 | arm_jit.cpp:1790 | `OP_LDR_` | arm_jit_arm.cpp:2172 | diff-pass | |
| A-0157 | `0x09D` | `OP_LDRSB_POS_INDE_P_REG_OFF` | arm_instructions.cpp:2948 | arm_jit.cpp:1853 | `OP_LDR_` | arm_jit_arm.cpp:2235 | diff-pass | |
| A-0159 | `0x09F` | `OP_LDRSH_POS_INDE_P_REG_OFF` | arm_instructions.cpp:2839 | arm_jit.cpp:1805 | `OP_LDR_` | arm_jit_arm.cpp:2187 | diff-pass | |
| A-0203 | `0x0CB` | `OP_STRH_POS_INDE_P_IMM_OFF` | arm_instructions.cpp:2712 | arm_jit.cpp:1959 | `OP_STR_` | arm_jit_arm.cpp:2343 | diff-pass | |
| A-0219 | `0x0DB` | `OP_LDRH_POS_INDE_P_IMM_OFF` | arm_instructions.cpp:2604 | arm_jit.cpp:1788 | `OP_LDR_` | arm_jit_arm.cpp:2170 | diff-pass | |
| A-0221 | `0x0DD` | `OP_LDRSB_POS_INDE_P_IMM_OFF` | arm_instructions.cpp:2930 | arm_jit.cpp:1851 | `OP_LDR_` | arm_jit_arm.cpp:2233 | diff-pass | |
| A-0223 | `0x0DF` | `OP_LDRSH_POS_INDE_P_IMM_OFF` | arm_instructions.cpp:2821 | arm_jit.cpp:1803 | `OP_LDR_` | arm_jit_arm.cpp:2185 | diff-pass | |
| A-0265 | `0x109` | `OP_SWP` | arm_instructions.cpp:2506 | arm_jit.cpp:2170 | — | arm_jit_arm.cpp:2632 | diff-pass | |
| A-0267 | `0x10B` | `OP_STRH_M_REG_OFF` | arm_instructions.cpp:2668 | arm_jit.cpp:1953 | `OP_STR_` | arm_jit_arm.cpp:2337 | diff-pass | |
| A-0269 | `0x10D,0x10F,0x12D,0x12F,0x14D,0x14F,…（共 16 槽）` | `OP_LDRD_STRD_OFFSET_PRE_INDEX` | arm_instructions.cpp:6049 | arm_jit.cpp:2074 | — | arm_jit_arm.cpp:2488 | diff-pass | |
| A-0283 | `0x11B` | `OP_LDRH_M_REG_OFF` | arm_instructions.cpp:2559 | arm_jit.cpp:1782 | `OP_LDR_` | arm_jit_arm.cpp:2164 | diff-pass | |
| A-0285 | `0x11D` | `OP_LDRSB_M_REG_OFF` | arm_instructions.cpp:2885 | arm_jit.cpp:1845 | `OP_LDR_` | arm_jit_arm.cpp:2227 | diff-pass | |
| A-0287 | `0x11F` | `OP_LDRSH_M_REG_OFF` | arm_instructions.cpp:2776 | arm_jit.cpp:1797 | `OP_LDR_` | arm_jit_arm.cpp:2179 | diff-pass | |
| A-0299 | `0x12B` | `OP_STRH_PRE_INDE_M_REG_OFF` | arm_instructions.cpp:2703 | arm_jit.cpp:1958 | `OP_STR_` | arm_jit_arm.cpp:2342 | diff-pass | |
| A-0315 | `0x13B` | `OP_LDRH_PRE_INDE_M_REG_OFF` | arm_instructions.cpp:2595 | arm_jit.cpp:1787 | `OP_LDR_` | arm_jit_arm.cpp:2169 | diff-pass | |
| A-0317 | `0x13D` | `OP_LDRSB_PRE_INDE_M_REG_OFF` | arm_instructions.cpp:2921 | arm_jit.cpp:1850 | `OP_LDR_` | arm_jit_arm.cpp:2232 | diff-pass | |
| A-0319 | `0x13F` | `OP_LDRSH_PRE_INDE_M_REG_OFF` | arm_instructions.cpp:2812 | arm_jit.cpp:1802 | `OP_LDR_` | arm_jit_arm.cpp:2184 | diff-pass | |
| A-0329 | `0x149` | `OP_SWPB` | arm_instructions.cpp:2519 | arm_jit.cpp:2171 | — | arm_jit_arm.cpp:2633 | diff-pass | |
| A-0331 | `0x14B` | `OP_STRH_M_IMM_OFF` | arm_instructions.cpp:2652 | arm_jit.cpp:1951 | `OP_STR_` | arm_jit_arm.cpp:2335 | diff-pass | |
| A-0347 | `0x15B` | `OP_LDRH_M_IMM_OFF` | arm_instructions.cpp:2543 | arm_jit.cpp:1780 | `OP_LDR_` | arm_jit_arm.cpp:2162 | diff-pass | |
| A-0349 | `0x15D` | `OP_LDRSB_M_IMM_OFF` | arm_instructions.cpp:2869 | arm_jit.cpp:1843 | `OP_LDR_` | arm_jit_arm.cpp:2225 | diff-pass | |
| A-0351 | `0x15F` | `OP_LDRSH_M_IMM_OFF` | arm_instructions.cpp:2760 | arm_jit.cpp:1795 | `OP_LDR_` | arm_jit_arm.cpp:2177 | diff-pass | |
| A-0363 | `0x16B` | `OP_STRH_PRE_INDE_M_IMM_OFF` | arm_instructions.cpp:2685 | arm_jit.cpp:1956 | `OP_STR_` | arm_jit_arm.cpp:2340 | diff-pass | |
| A-0379 | `0x17B` | `OP_LDRH_PRE_INDE_M_IMM_OFF` | arm_instructions.cpp:2576 | arm_jit.cpp:1785 | `OP_LDR_` | arm_jit_arm.cpp:2167 | diff-pass | |
| A-0381 | `0x17D` | `OP_LDRSB_PRE_INDE_M_IMM_OFF` | arm_instructions.cpp:2902 | arm_jit.cpp:1848 | `OP_LDR_` | arm_jit_arm.cpp:2230 | diff-pass | |
| A-0383 | `0x17F` | `OP_LDRSH_PRE_INDE_M_IMM_OFF` | arm_instructions.cpp:2794 | arm_jit.cpp:1800 | `OP_LDR_` | arm_jit_arm.cpp:2182 | diff-pass | |
| A-0393 | `0x189` | `OP_STREX` | arm_instructions.cpp:4380 | `NULL` | — | `NULL` | interp-fallback | |
| A-0395 | `0x18B` | `OP_STRH_P_REG_OFF` | arm_instructions.cpp:2660 | arm_jit.cpp:1952 | `OP_STR_` | arm_jit_arm.cpp:2336 | diff-pass | |
| A-0409 | `0x199` | `OP_LDREX` | arm_instructions.cpp:3793 | `NULL` | — | `NULL` | interp-fallback | |
| A-0411 | `0x19B` | `OP_LDRH_P_REG_OFF` | arm_instructions.cpp:2551 | arm_jit.cpp:1781 | `OP_LDR_` | arm_jit_arm.cpp:2163 | diff-pass | |
| A-0413 | `0x19D` | `OP_LDRSB_P_REG_OFF` | arm_instructions.cpp:2877 | arm_jit.cpp:1844 | `OP_LDR_` | arm_jit_arm.cpp:2226 | diff-pass | |
| A-0415 | `0x19F` | `OP_LDRSH_P_REG_OFF` | arm_instructions.cpp:2768 | arm_jit.cpp:1796 | `OP_LDR_` | arm_jit_arm.cpp:2178 | diff-pass | |
| A-0427 | `0x1AB` | `OP_STRH_PRE_INDE_P_REG_OFF` | arm_instructions.cpp:2694 | arm_jit.cpp:1957 | `OP_STR_` | arm_jit_arm.cpp:2341 | diff-pass | |
| A-0443 | `0x1BB` | `OP_LDRH_PRE_INDE_P_REG_OFF` | arm_instructions.cpp:2586 | arm_jit.cpp:1786 | `OP_LDR_` | arm_jit_arm.cpp:2168 | diff-pass | |
| A-0445 | `0x1BD` | `OP_LDRSB_PRE_INDE_P_REG_OFF` | arm_instructions.cpp:2911 | arm_jit.cpp:1849 | `OP_LDR_` | arm_jit_arm.cpp:2231 | diff-pass | |
| A-0447 | `0x1BF` | `OP_LDRSH_PRE_INDE_P_REG_OFF` | arm_instructions.cpp:2803 | arm_jit.cpp:1801 | `OP_LDR_` | arm_jit_arm.cpp:2183 | diff-pass | |
| A-0459 | `0x1CB` | `OP_STRH_P_IMM_OFF` | arm_instructions.cpp:2644 | arm_jit.cpp:1950 | `OP_STR_` | arm_jit_arm.cpp:2334 | diff-pass | |
| A-0475 | `0x1DB` | `OP_LDRH_P_IMM_OFF` | arm_instructions.cpp:2535 | arm_jit.cpp:1779 | `OP_LDR_` | arm_jit_arm.cpp:2161 | diff-pass | |
| A-0477 | `0x1DD` | `OP_LDRSB_P_IMM_OFF` | arm_instructions.cpp:2861 | arm_jit.cpp:1842 | `OP_LDR_` | arm_jit_arm.cpp:2224 | diff-pass | |
| A-0479 | `0x1DF` | `OP_LDRSH_P_IMM_OFF` | arm_instructions.cpp:2752 | arm_jit.cpp:1794 | `OP_LDR_` | arm_jit_arm.cpp:2176 | diff-pass | |
| A-0491 | `0x1EB` | `OP_STRH_PRE_INDE_P_IMM_OFF` | arm_instructions.cpp:2676 | arm_jit.cpp:1955 | `OP_STR_` | arm_jit_arm.cpp:2339 | diff-pass | |
| A-0507 | `0x1FB` | `OP_LDRH_PRE_INDE_P_IMM_OFF` | arm_instructions.cpp:2567 | arm_jit.cpp:1784 | `OP_LDR_` | arm_jit_arm.cpp:2166 | diff-pass | |
| A-0509 | `0x1FD` | `OP_LDRSB_PRE_INDE_P_IMM_OFF` | arm_instructions.cpp:2893 | arm_jit.cpp:1847 | `OP_LDR_` | arm_jit_arm.cpp:2229 | diff-pass | |
| A-0511 | `0x1FF` | `OP_LDRSH_PRE_INDE_P_IMM_OFF` | arm_instructions.cpp:2784 | arm_jit.cpp:1799 | `OP_LDR_` | arm_jit_arm.cpp:2181 | diff-pass | |
| A-1024 | `0x400..0x40F,0x420..0x42F` | `OP_STR_M_IMM_OFF_POSTIND` | arm_instructions.cpp:4288 | arm_jit.cpp:1940 | `OP_STR_` | arm_jit_arm.cpp:2324 | diff-pass | |
| A-1040 | `0x410..0x41F,0x430..0x43F` | `OP_LDR_M_IMM_OFF_POSTIND` | arm_instructions.cpp:3737 | arm_jit.cpp:1768 | `OP_LDR_` | arm_jit_arm.cpp:2150 | diff-pass | |
| A-1088 | `0x440..0x44F,0x460..0x46F` | `OP_STRB_M_IMM_OFF_POSTIND` | arm_instructions.cpp:4589 | arm_jit.cpp:1986 | `OP_STR_` | arm_jit_arm.cpp:2370 | diff-pass | |
| A-1104 | `0x450..0x45F,0x470..0x47F` | `OP_LDRB_M_IMM_OFF_POSTIND` | arm_instructions.cpp:4000 | arm_jit.cpp:1831 | `OP_LDR_` | arm_jit_arm.cpp:2213 | diff-pass | |
| A-1152 | `0x480..0x48F,0x4A0..0x4AF` | `OP_STR_P_IMM_OFF_POSTIND` | arm_instructions.cpp:4279 | arm_jit.cpp:1939 | `OP_STR_` | arm_jit_arm.cpp:2323 | diff-pass | |
| A-1168 | `0x490..0x49F,0x4B0..0x4BF` | `OP_LDR_P_IMM_OFF_POSTIND` | arm_instructions.cpp:3732 | arm_jit.cpp:1767 | `OP_LDR_` | arm_jit_arm.cpp:2149 | diff-pass | |
| A-1216 | `0x4C0..0x4CF,0x4E0..0x4EF` | `OP_STRB_P_IMM_OFF_POSTIND` | arm_instructions.cpp:4580 | arm_jit.cpp:1985 | `OP_STR_` | arm_jit_arm.cpp:2369 | diff-pass | |
| A-1232 | `0x4D0..0x4DF,0x4F0..0x4FF` | `OP_LDRB_P_IMM_OFF_POSTIND` | arm_instructions.cpp:3991 | arm_jit.cpp:1830 | `OP_LDR_` | arm_jit_arm.cpp:2212 | diff-pass | |
| A-1280 | `0x500..0x50F` | `OP_STR_M_IMM_OFF` | arm_instructions.cpp:4101 | arm_jit.cpp:1919 | `OP_STR_` | arm_jit_arm.cpp:2303 | diff-pass | |
| A-1296 | `0x510..0x51F` | `OP_LDR_M_IMM_OFF` | arm_instructions.cpp:3602 | arm_jit.cpp:1747 | `OP_LDR_` | arm_jit_arm.cpp:2129 | diff-pass | |
| A-1312 | `0x520..0x52F` | `OP_STR_M_IMM_OFF_PREIND` | arm_instructions.cpp:4190 | arm_jit.cpp:1930 | `OP_STR_` | arm_jit_arm.cpp:2314 | diff-pass | |
| A-1328 | `0x530..0x53F` | `OP_LDR_M_IMM_OFF_PREIND` | arm_instructions.cpp:3670 | arm_jit.cpp:1758 | `OP_LDR_` | arm_jit_arm.cpp:2140 | diff-pass | |
| A-1344 | `0x540..0x54F` | `OP_STRB_M_IMM_OFF` | arm_instructions.cpp:4402 | arm_jit.cpp:1965 | `OP_STR_` | arm_jit_arm.cpp:2349 | diff-pass | |
| A-1360 | `0x550..0x55F` | `OP_LDRB_M_IMM_OFF` | arm_instructions.cpp:3813 | arm_jit.cpp:1810 | `OP_LDR_` | arm_jit_arm.cpp:2192 | diff-pass | |
| A-1376 | `0x560..0x56F` | `OP_STRB_M_IMM_OFF_PREIND` | arm_instructions.cpp:4491 | arm_jit.cpp:1976 | `OP_STR_` | arm_jit_arm.cpp:2360 | diff-pass | |
| A-1392 | `0x570..0x57F` | `OP_LDRB_M_IMM_OFF_PREIND` | arm_instructions.cpp:3902 | arm_jit.cpp:1821 | `OP_LDR_` | arm_jit_arm.cpp:2203 | diff-pass | |
| A-1408 | `0x580..0x58F` | `OP_STR_P_IMM_OFF` | arm_instructions.cpp:4093 | arm_jit.cpp:1918 | `OP_STR_` | arm_jit_arm.cpp:2302 | diff-pass | |
| A-1424 | `0x590..0x59F` | `OP_LDR_P_IMM_OFF` | arm_instructions.cpp:3596 | arm_jit.cpp:1746 | `OP_LDR_` | arm_jit_arm.cpp:2128 | diff-pass | |
| A-1440 | `0x5A0..0x5AF` | `OP_STR_P_IMM_OFF_PREIND` | arm_instructions.cpp:4181 | arm_jit.cpp:1929 | `OP_STR_` | arm_jit_arm.cpp:2313 | diff-pass | |
| A-1456 | `0x5B0..0x5BF` | `OP_LDR_P_IMM_OFF_PREIND` | arm_instructions.cpp:3664 | arm_jit.cpp:1757 | `OP_LDR_` | arm_jit_arm.cpp:2139 | diff-pass | |
| A-1472 | `0x5C0..0x5CF` | `OP_STRB_P_IMM_OFF` | arm_instructions.cpp:4394 | arm_jit.cpp:1964 | `OP_STR_` | arm_jit_arm.cpp:2348 | diff-pass | |
| A-1488 | `0x5D0..0x5DF` | `OP_LDRB_P_IMM_OFF` | arm_instructions.cpp:3805 | arm_jit.cpp:1809 | `OP_LDR_` | arm_jit_arm.cpp:2191 | diff-pass | |
| A-1504 | `0x5E0..0x5EF` | `OP_STRB_P_IMM_OFF_PREIND` | arm_instructions.cpp:4482 | arm_jit.cpp:1975 | `OP_STR_` | arm_jit_arm.cpp:2359 | diff-pass | |
| A-1520 | `0x5F0..0x5FF` | `OP_LDRB_P_IMM_OFF_PREIND` | arm_instructions.cpp:3893 | arm_jit.cpp:1820 | `OP_LDR_` | arm_jit_arm.cpp:2202 | diff-pass | |
| A-1536 | `0x600,0x608,0x620,0x628` | `OP_STR_M_LSL_IMM_OFF_POSTIND` | arm_instructions.cpp:4307 | arm_jit.cpp:1942 | `OP_STR_` | arm_jit_arm.cpp:2326 | diff-pass | |
| A-1538 | `0x602,0x60A,0x622,0x62A` | `OP_STR_M_LSR_IMM_OFF_POSTIND` | arm_instructions.cpp:4327 | arm_jit.cpp:1944 | `OP_STR_` | arm_jit_arm.cpp:2328 | diff-pass | |
| A-1540 | `0x604,0x60C,0x624,0x62C` | `OP_STR_M_ASR_IMM_OFF_POSTIND` | arm_instructions.cpp:4347 | arm_jit.cpp:1946 | `OP_STR_` | arm_jit_arm.cpp:2330 | diff-pass | |
| A-1542 | `0x606,0x60E,0x626,0x62E` | `OP_STR_M_ROR_IMM_OFF_POSTIND` | arm_instructions.cpp:4367 | arm_jit.cpp:1948 | `OP_STR_` | arm_jit_arm.cpp:2332 | diff-pass | |
| A-1552 | `0x610,0x618,0x630,0x638` | `OP_LDR_M_LSL_IMM_OFF_POSTIND` | arm_instructions.cpp:3748 | arm_jit.cpp:1770 | `OP_LDR_` | arm_jit_arm.cpp:2152 | diff-pass | |
| A-1554 | `0x612,0x61A,0x632,0x63A` | `OP_LDR_M_LSR_IMM_OFF_POSTIND` | arm_instructions.cpp:3760 | arm_jit.cpp:1772 | `OP_LDR_` | arm_jit_arm.cpp:2154 | diff-pass | |
| A-1556 | `0x614,0x61C,0x634,0x63C` | `OP_LDR_M_ASR_IMM_OFF_POSTIND` | arm_instructions.cpp:3772 | arm_jit.cpp:1774 | `OP_LDR_` | arm_jit_arm.cpp:2156 | diff-pass | |
| A-1558 | `0x616,0x61E,0x636,0x63E` | `OP_LDR_M_ROR_IMM_OFF_POSTIND` | arm_instructions.cpp:3784 | arm_jit.cpp:1776 | `OP_LDR_` | arm_jit_arm.cpp:2158 | diff-pass | |
| A-1600 | `0x640,0x648,0x660,0x668` | `OP_STRB_M_LSL_IMM_OFF_POSTIND` | arm_instructions.cpp:4608 | arm_jit.cpp:1988 | `OP_STR_` | arm_jit_arm.cpp:2372 | diff-pass | |
| A-1602 | `0x642,0x64A,0x662,0x66A` | `OP_STRB_M_LSR_IMM_OFF_POSTIND` | arm_instructions.cpp:4628 | arm_jit.cpp:1990 | `OP_STR_` | arm_jit_arm.cpp:2374 | diff-pass | |
| A-1604 | `0x644,0x64C,0x664,0x66C` | `OP_STRB_M_ASR_IMM_OFF_POSTIND` | arm_instructions.cpp:4648 | arm_jit.cpp:1992 | `OP_STR_` | arm_jit_arm.cpp:2376 | diff-pass | |
| A-1606 | `0x646,0x64E,0x666,0x66E` | `OP_STRB_M_ROR_IMM_OFF_POSTIND` | arm_instructions.cpp:4668 | arm_jit.cpp:1994 | `OP_STR_` | arm_jit_arm.cpp:2378 | diff-pass | |
| A-1616 | `0x650,0x658,0x670,0x678` | `OP_LDRB_M_LSL_IMM_OFF_POSTIND` | arm_instructions.cpp:4019 | arm_jit.cpp:1833 | `OP_LDR_` | arm_jit_arm.cpp:2215 | diff-pass | |
| A-1618 | `0x652,0x65A,0x672,0x67A` | `OP_LDRB_M_LSR_IMM_OFF_POSTIND` | arm_instructions.cpp:4039 | arm_jit.cpp:1835 | `OP_LDR_` | arm_jit_arm.cpp:2217 | diff-pass | |
| A-1620 | `0x654,0x65C,0x674,0x67C` | `OP_LDRB_M_ASR_IMM_OFF_POSTIND` | arm_instructions.cpp:4059 | arm_jit.cpp:1837 | `OP_LDR_` | arm_jit_arm.cpp:2219 | diff-pass | |
| A-1622 | `0x656,0x65E,0x676,0x67E` | `OP_LDRB_M_ROR_IMM_OFF_POSTIND` | arm_instructions.cpp:4079 | arm_jit.cpp:1839 | `OP_LDR_` | arm_jit_arm.cpp:2221 | diff-pass | |
| A-1664 | `0x680,0x688,0x6A0,0x6A8` | `OP_STR_P_LSL_IMM_OFF_POSTIND` | arm_instructions.cpp:4297 | arm_jit.cpp:1941 | `OP_STR_` | arm_jit_arm.cpp:2325 | diff-pass | |
| A-1666 | `0x682,0x68A,0x6A2,0x6AA` | `OP_STR_P_LSR_IMM_OFF_POSTIND` | arm_instructions.cpp:4317 | arm_jit.cpp:1943 | `OP_STR_` | arm_jit_arm.cpp:2327 | diff-pass | |
| A-1668 | `0x684,0x68C,0x6A4,0x6AC` | `OP_STR_P_ASR_IMM_OFF_POSTIND` | arm_instructions.cpp:4337 | arm_jit.cpp:1945 | `OP_STR_` | arm_jit_arm.cpp:2329 | diff-pass | |
| A-1670 | `0x686,0x68E,0x6A6,0x6AE` | `OP_STR_P_ROR_IMM_OFF_POSTIND` | arm_instructions.cpp:4357 | arm_jit.cpp:1947 | `OP_STR_` | arm_jit_arm.cpp:2331 | diff-pass | |
| A-1680 | `0x690,0x698,0x6B0,0x6B8` | `OP_LDR_P_LSL_IMM_OFF_POSTIND` | arm_instructions.cpp:3742 | arm_jit.cpp:1769 | `OP_LDR_` | arm_jit_arm.cpp:2151 | diff-pass | |
| A-1682 | `0x692,0x69A,0x6B2,0x6BA` | `OP_LDR_P_LSR_IMM_OFF_POSTIND` | arm_instructions.cpp:3754 | arm_jit.cpp:1771 | `OP_LDR_` | arm_jit_arm.cpp:2153 | diff-pass | |
| A-1684 | `0x694,0x69C,0x6B4,0x6BC` | `OP_LDR_P_ASR_IMM_OFF_POSTIND` | arm_instructions.cpp:3766 | arm_jit.cpp:1773 | `OP_LDR_` | arm_jit_arm.cpp:2155 | diff-pass | |
| A-1686 | `0x696,0x69E,0x6B6,0x6BE` | `OP_LDR_P_ROR_IMM_OFF_POSTIND` | arm_instructions.cpp:3778 | arm_jit.cpp:1775 | `OP_LDR_` | arm_jit_arm.cpp:2157 | diff-pass | |
| A-1728 | `0x6C0,0x6C8,0x6E0,0x6E8` | `OP_STRB_P_LSL_IMM_OFF_POSTIND` | arm_instructions.cpp:4598 | arm_jit.cpp:1987 | `OP_STR_` | arm_jit_arm.cpp:2371 | diff-pass | |
| A-1730 | `0x6C2,0x6CA,0x6E2,0x6EA` | `OP_STRB_P_LSR_IMM_OFF_POSTIND` | arm_instructions.cpp:4618 | arm_jit.cpp:1989 | `OP_STR_` | arm_jit_arm.cpp:2373 | diff-pass | |
| A-1732 | `0x6C4,0x6CC,0x6E4,0x6EC` | `OP_STRB_P_ASR_IMM_OFF_POSTIND` | arm_instructions.cpp:4638 | arm_jit.cpp:1991 | `OP_STR_` | arm_jit_arm.cpp:2375 | diff-pass | |
| A-1734 | `0x6C6,0x6CE,0x6E6,0x6EE` | `OP_STRB_P_ROR_IMM_OFF_POSTIND` | arm_instructions.cpp:4658 | arm_jit.cpp:1993 | `OP_STR_` | arm_jit_arm.cpp:2377 | diff-pass | |
| A-1744 | `0x6D0,0x6D8,0x6F0,0x6F8` | `OP_LDRB_P_LSL_IMM_OFF_POSTIND` | arm_instructions.cpp:4009 | arm_jit.cpp:1832 | `OP_LDR_` | arm_jit_arm.cpp:2214 | diff-pass | |
| A-1746 | `0x6D2,0x6DA,0x6F2,0x6FA` | `OP_LDRB_P_LSR_IMM_OFF_POSTIND` | arm_instructions.cpp:4029 | arm_jit.cpp:1834 | `OP_LDR_` | arm_jit_arm.cpp:2216 | diff-pass | |
| A-1748 | `0x6D4,0x6DC,0x6F4,0x6FC` | `OP_LDRB_P_ASR_IMM_OFF_POSTIND` | arm_instructions.cpp:4049 | arm_jit.cpp:1836 | `OP_LDR_` | arm_jit_arm.cpp:2218 | diff-pass | |
| A-1750 | `0x6D6,0x6DE,0x6F6,0x6FE` | `OP_LDRB_P_ROR_IMM_OFF_POSTIND` | arm_instructions.cpp:4069 | arm_jit.cpp:1838 | `OP_LDR_` | arm_jit_arm.cpp:2220 | diff-pass | |
| A-1792 | `0x700,0x708` | `OP_STR_M_LSL_IMM_OFF` | arm_instructions.cpp:4118 | arm_jit.cpp:1921 | `OP_STR_` | arm_jit_arm.cpp:2305 | diff-pass | |
| A-1794 | `0x702,0x70A` | `OP_STR_M_LSR_IMM_OFF` | arm_instructions.cpp:4136 | arm_jit.cpp:1923 | `OP_STR_` | arm_jit_arm.cpp:2307 | diff-pass | |
| A-1796 | `0x704,0x70C` | `OP_STR_M_ASR_IMM_OFF` | arm_instructions.cpp:4154 | arm_jit.cpp:1925 | `OP_STR_` | arm_jit_arm.cpp:2309 | diff-pass | |
| A-1798 | `0x706,0x70E` | `OP_STR_M_ROR_IMM_OFF` | arm_instructions.cpp:4172 | arm_jit.cpp:1927 | `OP_STR_` | arm_jit_arm.cpp:2311 | diff-pass | |
| A-1808 | `0x710,0x718` | `OP_LDR_M_LSL_IMM_OFF` | arm_instructions.cpp:3615 | arm_jit.cpp:1749 | `OP_LDR_` | arm_jit_arm.cpp:2131 | diff-pass | |
| A-1810 | `0x712,0x71A` | `OP_LDR_M_LSR_IMM_OFF` | arm_instructions.cpp:3629 | arm_jit.cpp:1751 | `OP_LDR_` | arm_jit_arm.cpp:2133 | diff-pass | |
| A-1812 | `0x714,0x71C` | `OP_LDR_M_ASR_IMM_OFF` | arm_instructions.cpp:3643 | arm_jit.cpp:1753 | `OP_LDR_` | arm_jit_arm.cpp:2135 | diff-pass | |
| A-1814 | `0x716,0x71E` | `OP_LDR_M_ROR_IMM_OFF` | arm_instructions.cpp:3657 | arm_jit.cpp:1755 | `OP_LDR_` | arm_jit_arm.cpp:2137 | diff-pass | |
| A-1824 | `0x720,0x728` | `OP_STR_M_LSL_IMM_OFF_PREIND` | arm_instructions.cpp:4209 | arm_jit.cpp:1932 | `OP_STR_` | arm_jit_arm.cpp:2316 | diff-pass | |
| A-1826 | `0x722,0x72A` | `OP_STR_M_LSR_IMM_OFF_PREIND` | arm_instructions.cpp:4229 | arm_jit.cpp:1934 | `OP_STR_` | arm_jit_arm.cpp:2318 | diff-pass | |
| A-1828 | `0x724,0x72C` | `OP_STR_M_ASR_IMM_OFF_PREIND` | arm_instructions.cpp:4249 | arm_jit.cpp:1936 | `OP_STR_` | arm_jit_arm.cpp:2320 | diff-pass | |
| A-1830 | `0x726,0x72E` | `OP_STR_M_ROR_IMM_OFF_PREIND` | arm_instructions.cpp:4269 | arm_jit.cpp:1938 | `OP_STR_` | arm_jit_arm.cpp:2322 | diff-pass | |
| A-1840 | `0x730,0x738` | `OP_LDR_M_LSL_IMM_OFF_PREIND` | arm_instructions.cpp:3683 | arm_jit.cpp:1760 | `OP_LDR_` | arm_jit_arm.cpp:2142 | diff-pass | |
| A-1842 | `0x732,0x73A` | `OP_LDR_M_LSR_IMM_OFF_PREIND` | arm_instructions.cpp:3697 | arm_jit.cpp:1762 | `OP_LDR_` | arm_jit_arm.cpp:2144 | diff-pass | |
| A-1844 | `0x734,0x73C` | `OP_LDR_M_ASR_IMM_OFF_PREIND` | arm_instructions.cpp:3711 | arm_jit.cpp:1764 | `OP_LDR_` | arm_jit_arm.cpp:2146 | diff-pass | |
| A-1846 | `0x736,0x73E` | `OP_LDR_M_ROR_IMM_OFF_PREIND` | arm_instructions.cpp:3725 | arm_jit.cpp:1766 | `OP_LDR_` | arm_jit_arm.cpp:2148 | diff-pass | |
| A-1856 | `0x740,0x748` | `OP_STRB_M_LSL_IMM_OFF` | arm_instructions.cpp:4419 | arm_jit.cpp:1967 | `OP_STR_` | arm_jit_arm.cpp:2351 | diff-pass | |
| A-1858 | `0x742,0x74A` | `OP_STRB_M_LSR_IMM_OFF` | arm_instructions.cpp:4437 | arm_jit.cpp:1969 | `OP_STR_` | arm_jit_arm.cpp:2353 | diff-pass | |
| A-1860 | `0x744,0x74C` | `OP_STRB_M_ASR_IMM_OFF` | arm_instructions.cpp:4455 | arm_jit.cpp:1971 | `OP_STR_` | arm_jit_arm.cpp:2355 | diff-pass | |
| A-1862 | `0x746,0x74E` | `OP_STRB_M_ROR_IMM_OFF` | arm_instructions.cpp:4473 | arm_jit.cpp:1973 | `OP_STR_` | arm_jit_arm.cpp:2357 | diff-pass | |
| A-1872 | `0x750,0x758` | `OP_LDRB_M_LSL_IMM_OFF` | arm_instructions.cpp:3830 | arm_jit.cpp:1812 | `OP_LDR_` | arm_jit_arm.cpp:2194 | diff-pass | |
| A-1874 | `0x752,0x75A` | `OP_LDRB_M_LSR_IMM_OFF` | arm_instructions.cpp:3848 | arm_jit.cpp:1814 | `OP_LDR_` | arm_jit_arm.cpp:2196 | diff-pass | |
| A-1876 | `0x754,0x75C` | `OP_LDRB_M_ASR_IMM_OFF` | arm_instructions.cpp:3866 | arm_jit.cpp:1816 | `OP_LDR_` | arm_jit_arm.cpp:2198 | diff-pass | |
| A-1878 | `0x756,0x75E` | `OP_LDRB_M_ROR_IMM_OFF` | arm_instructions.cpp:3884 | arm_jit.cpp:1818 | `OP_LDR_` | arm_jit_arm.cpp:2200 | diff-pass | |
| A-1888 | `0x760,0x768` | `OP_STRB_M_LSL_IMM_OFF_PREIND` | arm_instructions.cpp:4510 | arm_jit.cpp:1978 | `OP_STR_` | arm_jit_arm.cpp:2362 | diff-pass | |
| A-1890 | `0x762,0x76A` | `OP_STRB_M_LSR_IMM_OFF_PREIND` | arm_instructions.cpp:4530 | arm_jit.cpp:1980 | `OP_STR_` | arm_jit_arm.cpp:2364 | diff-pass | |
| A-1892 | `0x764,0x76C` | `OP_STRB_M_ASR_IMM_OFF_PREIND` | arm_instructions.cpp:4550 | arm_jit.cpp:1982 | `OP_STR_` | arm_jit_arm.cpp:2366 | diff-pass | |
| A-1894 | `0x766,0x76E` | `OP_STRB_M_ROR_IMM_OFF_PREIND` | arm_instructions.cpp:4570 | arm_jit.cpp:1984 | `OP_STR_` | arm_jit_arm.cpp:2368 | diff-pass | |
| A-1904 | `0x770,0x778` | `OP_LDRB_M_LSL_IMM_OFF_PREIND` | arm_instructions.cpp:3921 | arm_jit.cpp:1823 | `OP_LDR_` | arm_jit_arm.cpp:2205 | diff-pass | |
| A-1906 | `0x772,0x77A` | `OP_LDRB_M_LSR_IMM_OFF_PREIND` | arm_instructions.cpp:3941 | arm_jit.cpp:1825 | `OP_LDR_` | arm_jit_arm.cpp:2207 | diff-pass | |
| A-1908 | `0x774,0x77C` | `OP_LDRB_M_ASR_IMM_OFF_PREIND` | arm_instructions.cpp:3961 | arm_jit.cpp:1827 | `OP_LDR_` | arm_jit_arm.cpp:2209 | diff-pass | |
| A-1910 | `0x776,0x77E` | `OP_LDRB_M_ROR_IMM_OFF_PREIND` | arm_instructions.cpp:3981 | arm_jit.cpp:1829 | `OP_LDR_` | arm_jit_arm.cpp:2211 | diff-pass | |
| A-1920 | `0x780,0x788` | `OP_STR_P_LSL_IMM_OFF` | arm_instructions.cpp:4109 | arm_jit.cpp:1920 | `OP_STR_` | arm_jit_arm.cpp:2304 | diff-pass | |
| A-1922 | `0x782,0x78A` | `OP_STR_P_LSR_IMM_OFF` | arm_instructions.cpp:4127 | arm_jit.cpp:1922 | `OP_STR_` | arm_jit_arm.cpp:2306 | diff-pass | |
| A-1924 | `0x784,0x78C` | `OP_STR_P_ASR_IMM_OFF` | arm_instructions.cpp:4145 | arm_jit.cpp:1924 | `OP_STR_` | arm_jit_arm.cpp:2308 | diff-pass | |
| A-1926 | `0x786,0x78E` | `OP_STR_P_ROR_IMM_OFF` | arm_instructions.cpp:4163 | arm_jit.cpp:1926 | `OP_STR_` | arm_jit_arm.cpp:2310 | diff-pass | |
| A-1936 | `0x790,0x798` | `OP_LDR_P_LSL_IMM_OFF` | arm_instructions.cpp:3608 | arm_jit.cpp:1748 | `OP_LDR_` | arm_jit_arm.cpp:2130 | diff-pass | |
| A-1938 | `0x792,0x79A` | `OP_LDR_P_LSR_IMM_OFF` | arm_instructions.cpp:3622 | arm_jit.cpp:1750 | `OP_LDR_` | arm_jit_arm.cpp:2132 | diff-pass | |
| A-1940 | `0x794,0x79C` | `OP_LDR_P_ASR_IMM_OFF` | arm_instructions.cpp:3636 | arm_jit.cpp:1752 | `OP_LDR_` | arm_jit_arm.cpp:2134 | diff-pass | |
| A-1942 | `0x796,0x79E` | `OP_LDR_P_ROR_IMM_OFF` | arm_instructions.cpp:3650 | arm_jit.cpp:1754 | `OP_LDR_` | arm_jit_arm.cpp:2136 | diff-pass | |
| A-1952 | `0x7A0,0x7A8` | `OP_STR_P_LSL_IMM_OFF_PREIND` | arm_instructions.cpp:4199 | arm_jit.cpp:1931 | `OP_STR_` | arm_jit_arm.cpp:2315 | diff-pass | |
| A-1954 | `0x7A2,0x7AA` | `OP_STR_P_LSR_IMM_OFF_PREIND` | arm_instructions.cpp:4219 | arm_jit.cpp:1933 | `OP_STR_` | arm_jit_arm.cpp:2317 | diff-pass | |
| A-1956 | `0x7A4,0x7AC` | `OP_STR_P_ASR_IMM_OFF_PREIND` | arm_instructions.cpp:4239 | arm_jit.cpp:1935 | `OP_STR_` | arm_jit_arm.cpp:2319 | diff-pass | |
| A-1958 | `0x7A6,0x7AE` | `OP_STR_P_ROR_IMM_OFF_PREIND` | arm_instructions.cpp:4259 | arm_jit.cpp:1937 | `OP_STR_` | arm_jit_arm.cpp:2321 | diff-pass | |
| A-1968 | `0x7B0,0x7B8` | `OP_LDR_P_LSL_IMM_OFF_PREIND` | arm_instructions.cpp:3676 | arm_jit.cpp:1759 | `OP_LDR_` | arm_jit_arm.cpp:2141 | diff-pass | |
| A-1970 | `0x7B2,0x7BA` | `OP_LDR_P_LSR_IMM_OFF_PREIND` | arm_instructions.cpp:3690 | arm_jit.cpp:1761 | `OP_LDR_` | arm_jit_arm.cpp:2143 | diff-pass | |
| A-1972 | `0x7B4,0x7BC` | `OP_LDR_P_ASR_IMM_OFF_PREIND` | arm_instructions.cpp:3704 | arm_jit.cpp:1763 | `OP_LDR_` | arm_jit_arm.cpp:2145 | diff-pass | |
| A-1974 | `0x7B6,0x7BE` | `OP_LDR_P_ROR_IMM_OFF_PREIND` | arm_instructions.cpp:3718 | arm_jit.cpp:1765 | `OP_LDR_` | arm_jit_arm.cpp:2147 | diff-pass | |
| A-1984 | `0x7C0,0x7C8` | `OP_STRB_P_LSL_IMM_OFF` | arm_instructions.cpp:4410 | arm_jit.cpp:1966 | `OP_STR_` | arm_jit_arm.cpp:2350 | diff-pass | |
| A-1986 | `0x7C2,0x7CA` | `OP_STRB_P_LSR_IMM_OFF` | arm_instructions.cpp:4428 | arm_jit.cpp:1968 | `OP_STR_` | arm_jit_arm.cpp:2352 | diff-pass | |
| A-1988 | `0x7C4,0x7CC` | `OP_STRB_P_ASR_IMM_OFF` | arm_instructions.cpp:4446 | arm_jit.cpp:1970 | `OP_STR_` | arm_jit_arm.cpp:2354 | diff-pass | |
| A-1990 | `0x7C6,0x7CE` | `OP_STRB_P_ROR_IMM_OFF` | arm_instructions.cpp:4464 | arm_jit.cpp:1972 | `OP_STR_` | arm_jit_arm.cpp:2356 | diff-pass | |
| A-2000 | `0x7D0,0x7D8` | `OP_LDRB_P_LSL_IMM_OFF` | arm_instructions.cpp:3821 | arm_jit.cpp:1811 | `OP_LDR_` | arm_jit_arm.cpp:2193 | diff-pass | |
| A-2002 | `0x7D2,0x7DA` | `OP_LDRB_P_LSR_IMM_OFF` | arm_instructions.cpp:3839 | arm_jit.cpp:1813 | `OP_LDR_` | arm_jit_arm.cpp:2195 | diff-pass | |
| A-2004 | `0x7D4,0x7DC` | `OP_LDRB_P_ASR_IMM_OFF` | arm_instructions.cpp:3857 | arm_jit.cpp:1815 | `OP_LDR_` | arm_jit_arm.cpp:2197 | diff-pass | |
| A-2006 | `0x7D6,0x7DE` | `OP_LDRB_P_ROR_IMM_OFF` | arm_instructions.cpp:3875 | arm_jit.cpp:1817 | `OP_LDR_` | arm_jit_arm.cpp:2199 | diff-pass | |
| A-2016 | `0x7E0,0x7E8` | `OP_STRB_P_LSL_IMM_OFF_PREIND` | arm_instructions.cpp:4500 | arm_jit.cpp:1977 | `OP_STR_` | arm_jit_arm.cpp:2361 | diff-pass | |
| A-2018 | `0x7E2,0x7EA` | `OP_STRB_P_LSR_IMM_OFF_PREIND` | arm_instructions.cpp:4520 | arm_jit.cpp:1979 | `OP_STR_` | arm_jit_arm.cpp:2363 | diff-pass | |
| A-2020 | `0x7E4,0x7EC` | `OP_STRB_P_ASR_IMM_OFF_PREIND` | arm_instructions.cpp:4540 | arm_jit.cpp:1981 | `OP_STR_` | arm_jit_arm.cpp:2365 | diff-pass | |
| A-2022 | `0x7E6,0x7EE` | `OP_STRB_P_ROR_IMM_OFF_PREIND` | arm_instructions.cpp:4560 | arm_jit.cpp:1983 | `OP_STR_` | arm_jit_arm.cpp:2367 | diff-pass | |
| A-2032 | `0x7F0,0x7F8` | `OP_LDRB_P_LSL_IMM_OFF_PREIND` | arm_instructions.cpp:3911 | arm_jit.cpp:1822 | `OP_LDR_` | arm_jit_arm.cpp:2204 | diff-pass | |
| A-2034 | `0x7F2,0x7FA` | `OP_LDRB_P_LSR_IMM_OFF_PREIND` | arm_instructions.cpp:3931 | arm_jit.cpp:1824 | `OP_LDR_` | arm_jit_arm.cpp:2206 | diff-pass | |
| A-2036 | `0x7F4,0x7FC` | `OP_LDRB_P_ASR_IMM_OFF_PREIND` | arm_instructions.cpp:3951 | arm_jit.cpp:1826 | `OP_LDR_` | arm_jit_arm.cpp:2208 | diff-pass | |
| A-2038 | `0x7F6,0x7FE` | `OP_LDRB_P_ROR_IMM_OFF_PREIND` | arm_instructions.cpp:3971 | arm_jit.cpp:1828 | `OP_LDR_` | arm_jit_arm.cpp:2210 | diff-pass | |

### LDM/STM（32）

| id | 槽位 | handler | 解释器 | x86 | x86 宏体 | arm64 | 状态 | 偏差 |
|---|---|---|---|---|---|---|---|---|
| A-2048 | `0x800..0x80F` | `OP_STMDA` | arm_instructions.cpp:5660 | arm_jit.cpp:2430 | — | arm_jit_arm.cpp:2899 | diff-pass | |
| A-2064 | `0x810..0x81F` | `OP_LDMDA` | arm_instructions.cpp:4820 | arm_jit.cpp:2421 | — | arm_jit_arm.cpp:2890 | diff-pass | |
| A-2080 | `0x820..0x82F` | `OP_STMDA_W` | arm_instructions.cpp:5731 | arm_jit.cpp:2434 | — | arm_jit_arm.cpp:2903 | diff-pass | |
| A-2096 | `0x830..0x83F` | `OP_LDMDA_W` | arm_instructions.cpp:5024 | arm_jit.cpp:2425 | — | arm_jit_arm.cpp:2894 | diff-pass | |
| A-2112 | `0x840..0x84F` | `OP_STMDA2` | arm_instructions.cpp:5827 | arm_jit.cpp:2511 | — | arm_jit_arm.cpp:2985 | diff-pass | |
| A-2128 | `0x850..0x85F` | `OP_LDMDA2` | arm_instructions.cpp:5255 | arm_jit.cpp:2502 | — | arm_jit_arm.cpp:2976 | diff-pass | |
| A-2144 | `0x860..0x86F` | `OP_STMDA2_W` | arm_instructions.cpp:5941 | arm_jit.cpp:2515 | — | arm_jit_arm.cpp:2989 | diff-pass | |
| A-2160 | `0x870..0x87F` | `OP_LDMDA2_W` | arm_instructions.cpp:5501 | arm_jit.cpp:2506 | — | arm_jit_arm.cpp:2980 | diff-pass | |
| A-2176 | `0x880..0x88F` | `OP_STMIA` | arm_instructions.cpp:5626 | arm_jit.cpp:2428 | — | arm_jit_arm.cpp:2897 | diff-pass | |
| A-2192 | `0x890..0x89F` | `OP_LDMIA` | arm_instructions.cpp:4710 | arm_jit.cpp:2419 | — | arm_jit_arm.cpp:2888 | diff-pass | |
| A-2208 | `0x8A0..0x8AF` | `OP_STMIA_W` | arm_instructions.cpp:5694 | arm_jit.cpp:2432 | — | arm_jit_arm.cpp:2901 | diff-pass | |
| A-2224 | `0x8B0..0x8BF` | `OP_LDMIA_W` | arm_instructions.cpp:4914 | arm_jit.cpp:2423 | — | arm_jit_arm.cpp:2892 | diff-pass | |
| A-2240 | `0x8C0..0x8CF` | `OP_STMIA2` | arm_instructions.cpp:5769 | arm_jit.cpp:2509 | — | arm_jit_arm.cpp:2983 | diff-pass | |
| A-2256 | `0x8D0..0x8DF` | `OP_LDMIA2` | arm_instructions.cpp:5134 | arm_jit.cpp:2500 | — | arm_jit_arm.cpp:2974 | diff-pass | |
| A-2272 | `0x8E0..0x8EF` | `OP_STMIA2_W` | arm_instructions.cpp:5883 | arm_jit.cpp:2513 | — | arm_jit_arm.cpp:2987 | diff-pass | |
| A-2288 | `0x8F0..0x8FF` | `OP_LDMIA2_W` | arm_instructions.cpp:5386 | arm_jit.cpp:2504 | — | arm_jit_arm.cpp:2978 | diff-pass | |
| A-2304 | `0x900..0x90F` | `OP_STMDB` | arm_instructions.cpp:5677 | arm_jit.cpp:2431 | — | arm_jit_arm.cpp:2900 | diff-pass | |
| A-2320 | `0x910..0x91F` | `OP_LDMDB` | arm_instructions.cpp:4867 | arm_jit.cpp:2422 | — | arm_jit_arm.cpp:2891 | diff-pass | |
| A-2336 | `0x920..0x92F` | `OP_STMDB_W` | arm_instructions.cpp:5750 | arm_jit.cpp:2435 | — | arm_jit_arm.cpp:2904 | diff-pass | |
| A-2352 | `0x930..0x93F` | `OP_LDMDB_W` | arm_instructions.cpp:5079 | arm_jit.cpp:2426 | — | arm_jit_arm.cpp:2895 | diff-pass | |
| A-2368 | `0x940..0x94F` | `OP_STMDB2` | arm_instructions.cpp:5856 | arm_jit.cpp:2512 | — | arm_jit_arm.cpp:2986 | diff-pass | |
| A-2384 | `0x950..0x95F` | `OP_LDMDB2` | arm_instructions.cpp:5322 | arm_jit.cpp:2503 | — | arm_jit_arm.cpp:2977 | diff-pass | |
| A-2400 | `0x960..0x96F` | `OP_STMDB2_W` | arm_instructions.cpp:5972 | arm_jit.cpp:2516 | — | arm_jit_arm.cpp:2990 | diff-pass | |
| A-2416 | `0x970..0x97F` | `OP_LDMDB2_W` | arm_instructions.cpp:5560 | arm_jit.cpp:2507 | — | arm_jit_arm.cpp:2981 | diff-pass | |
| A-2432 | `0x980..0x98F` | `OP_STMIB` | arm_instructions.cpp:5643 | arm_jit.cpp:2429 | — | arm_jit_arm.cpp:2898 | diff-pass | |
| A-2448 | `0x990..0x99F` | `OP_LDMIB` | arm_instructions.cpp:4772 | arm_jit.cpp:2420 | — | arm_jit_arm.cpp:2889 | diff-pass | |
| A-2464 | `0x9A0..0x9AF` | `OP_STMIB_W` | arm_instructions.cpp:5713 | arm_jit.cpp:2433 | — | arm_jit_arm.cpp:2902 | diff-pass | |
| A-2480 | `0x9B0..0x9BF` | `OP_LDMIB_W` | arm_instructions.cpp:4968 | arm_jit.cpp:2424 | — | arm_jit_arm.cpp:2893 | diff-pass | |
| A-2496 | `0x9C0..0x9CF` | `OP_STMIB2` | arm_instructions.cpp:5798 | arm_jit.cpp:2510 | — | arm_jit_arm.cpp:2984 | diff-pass | |
| A-2512 | `0x9D0..0x9DF` | `OP_LDMIB2` | arm_instructions.cpp:5194 | arm_jit.cpp:2501 | — | arm_jit_arm.cpp:2975 | diff-pass | |
| A-2528 | `0x9E0..0x9EF` | `OP_STMIB2_W` | arm_instructions.cpp:5914 | arm_jit.cpp:2514 | — | arm_jit_arm.cpp:2988 | diff-pass | |
| A-2544 | `0x9F0..0x9FF` | `OP_LDMIB2_W` | arm_instructions.cpp:5442 | arm_jit.cpp:2505 | — | arm_jit_arm.cpp:2979 | diff-pass | |

### 乘法/乘加（28）

| id | 槽位 | handler | 解释器 | x86 | x86 宏体 | arm64 | 状态 | 偏差 |
|---|---|---|---|---|---|---|---|---|
| A-0009 | `0x009` | `OP_MUL` | arm_instructions.cpp:2317 | arm_jit.cpp:1351 | `OP_MUL_` | arm_jit_arm.cpp:1739 | diff-pass | |
| A-0025 | `0x019` | `OP_MUL_S` | arm_instructions.cpp:2333 | arm_jit.cpp:1358 | `OP_MUL_` | arm_jit_arm.cpp:1746 | diff-pass | |
| A-0041 | `0x029` | `OP_MLA` | arm_instructions.cpp:2325 | arm_jit.cpp:1352 | `OP_MUL_` | arm_jit_arm.cpp:1740 | diff-pass | |
| A-0057 | `0x039` | `OP_MLA_S` | arm_instructions.cpp:2344 | arm_jit.cpp:1359 | `OP_MUL_` | arm_jit_arm.cpp:1747 | diff-pass | |
| A-0137 | `0x089` | `OP_UMULL` | arm_instructions.cpp:2371 | arm_jit.cpp:1353 | `OP_MUL_` | arm_jit_arm.cpp:1741 | diff-pass | |
| A-0153 | `0x099` | `OP_UMULL_S` | arm_instructions.cpp:2396 | arm_jit.cpp:1360 | `OP_MUL_` | arm_jit_arm.cpp:1748 | diff-pass | |
| A-0169 | `0x0A9` | `OP_UMLAL` | arm_instructions.cpp:2382 | arm_jit.cpp:1354 | `OP_MUL_` | arm_jit_arm.cpp:1742 | diff-pass | |
| A-0185 | `0x0B9` | `OP_UMLAL_S` | arm_instructions.cpp:2410 | arm_jit.cpp:1361 | `OP_MUL_` | arm_jit_arm.cpp:1749 | diff-pass | |
| A-0201 | `0x0C9` | `OP_SMULL` | arm_instructions.cpp:2445 | arm_jit.cpp:1355 | `OP_MUL_` | arm_jit_arm.cpp:1743 | diff-pass | |
| A-0217 | `0x0D9` | `OP_SMULL_S` | arm_instructions.cpp:2473 | arm_jit.cpp:1362 | `OP_MUL_` | arm_jit_arm.cpp:1750 | diff-pass | |
| A-0233 | `0x0E9` | `OP_SMLAL` | arm_instructions.cpp:2456 | arm_jit.cpp:1356 | `OP_MUL_` | arm_jit_arm.cpp:1744 | diff-pass | |
| A-0249 | `0x0F9` | `OP_SMLAL_S` | arm_instructions.cpp:2487 | arm_jit.cpp:1363 | `OP_MUL_` | arm_jit_arm.cpp:1751 | diff-pass | |
| A-0264 | `0x108` | `OP_SMLA_B_B` | arm_instructions.cpp:3353 | arm_jit.cpp:1419 | `OP_MULxy_` | arm_jit_arm.cpp:1789 | diff-pass | |
| A-0266 | `0x10A` | `OP_SMLA_T_B` | arm_instructions.cpp:3380 | arm_jit.cpp:1421 | `OP_MULxy_` | arm_jit_arm.cpp:1793 | diff-pass | |
| A-0268 | `0x10C` | `OP_SMLA_B_T` | arm_instructions.cpp:3366 | arm_jit.cpp:1420 | `OP_MULxy_` | arm_jit_arm.cpp:1792 | diff-pass | |
| A-0270 | `0x10E` | `OP_SMLA_T_T` | arm_instructions.cpp:3394 | arm_jit.cpp:1422 | `OP_MULxy_` | arm_jit_arm.cpp:1794 | diff-pass | |
| A-0296 | `0x128` | `OP_SMLAW_B` | arm_instructions.cpp:3494 | arm_jit.cpp:1466 | `OP_SMxxW_` | arm_jit_arm.cpp:1823 | diff-pass | |
| A-0298 | `0x12A` | `OP_SMULW_B` | arm_instructions.cpp:3468 | arm_jit.cpp:1464 | `OP_SMxxW_` | arm_jit_arm.cpp:1821 | diff-pass | |
| A-0300 | `0x12C` | `OP_SMLAW_T` | arm_instructions.cpp:3511 | arm_jit.cpp:1467 | `OP_SMxxW_` | arm_jit_arm.cpp:1824 | diff-pass | |
| A-0302 | `0x12E` | `OP_SMULW_T` | arm_instructions.cpp:3479 | arm_jit.cpp:1465 | `OP_SMxxW_` | arm_jit_arm.cpp:1822 | diff-pass | |
| A-0328 | `0x148` | `OP_SMLAL_B_B` | arm_instructions.cpp:3412 | arm_jit.cpp:1427 | `OP_MULxy_` | arm_jit_arm.cpp:1799 | diff-pass | |
| A-0330 | `0x14A` | `OP_SMLAL_T_B` | arm_instructions.cpp:3438 | arm_jit.cpp:1429 | `OP_MULxy_` | arm_jit_arm.cpp:1801 | diff-pass | |
| A-0332 | `0x14C` | `OP_SMLAL_B_T` | arm_instructions.cpp:3425 | arm_jit.cpp:1428 | `OP_MULxy_` | arm_jit_arm.cpp:1800 | diff-pass | |
| A-0334 | `0x14E` | `OP_SMLAL_T_T` | arm_instructions.cpp:3451 | arm_jit.cpp:1430 | `OP_MULxy_` | arm_jit_arm.cpp:1802 | diff-pass | |
| A-0360 | `0x168` | `OP_SMUL_B_B` | arm_instructions.cpp:3316 | arm_jit.cpp:1411 | `OP_MULxy_` | arm_jit_arm.cpp:1781 | diff-pass | |
| A-0362 | `0x16A` | `OP_SMUL_T_B` | arm_instructions.cpp:3333 | arm_jit.cpp:1413 | `OP_MULxy_` | arm_jit_arm.cpp:1783 | diff-pass | |
| A-0364 | `0x16C` | `OP_SMUL_B_T` | arm_instructions.cpp:3325 | arm_jit.cpp:1412 | `OP_MULxy_` | arm_jit_arm.cpp:1782 | diff-pass | |
| A-0366 | `0x16E` | `OP_SMUL_T_T` | arm_instructions.cpp:3341 | arm_jit.cpp:1414 | `OP_MULxy_` | arm_jit_arm.cpp:1784 | diff-pass | |

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
| A-3585 | `0xE01,0xE03,0xE05,0xE07,0xE09,0xE0B,…（共 64 槽）` | `OP_MCR` | arm_instructions.cpp:6197 | arm_jit.cpp:2642 | — | arm_jit_arm.cpp:3131 | todo | |
| A-3601 | `0xE11,0xE13,0xE15,0xE17,0xE19,0xE1B,…（共 64 槽）` | `OP_MRC` | arm_instructions.cpp:6215 | arm_jit.cpp:2849 | — | arm_jit_arm.cpp:3338 | todo | |

### 状态寄存器（6）

| id | 槽位 | handler | 解释器 | x86 | x86 宏体 | arm64 | 状态 | 偏差 |
|---|---|---|---|---|---|---|---|---|
| A-0256 | `0x100` | `OP_MRS_CPSR` | arm_instructions.cpp:2970 | arm_jit.cpp:1472 | — | arm_jit_arm.cpp:1835 | diff-pass | |
| A-0288 | `0x120` | `OP_MSR_CPSR` | arm_instructions.cpp:3017 | arm_jit.cpp:1604 | `OP_MSR_` | arm_jit_arm.cpp:1973 | diff-pass | |
| A-0320 | `0x140` | `OP_MRS_SPSR` | arm_instructions.cpp:2977 | arm_jit.cpp:1480 | — | arm_jit_arm.cpp:1843 | diff-pass | |
| A-0352 | `0x160` | `OP_MSR_SPSR` | arm_instructions.cpp:3054 | arm_jit.cpp:1605 | `OP_MSR_` | arm_jit_arm.cpp:1974 | diff-pass | |
| A-0800 | `0x320..0x32F` | `OP_MSR_CPSR_IMM_VAL` | arm_instructions.cpp:3063 | arm_jit.cpp:1606 | `OP_MSR_` | arm_jit_arm.cpp:1975 | diff-pass | |
| A-0864 | `0x360..0x36F` | `OP_MSR_SPSR_IMM_VAL` | arm_instructions.cpp:3071 | arm_jit.cpp:1607 | `OP_MSR_` | arm_jit_arm.cpp:1976 | diff-pass | |

### 饱和/计数（5）

| id | 槽位 | handler | 解释器 | x86 | x86 宏体 | arm64 | 状态 | 偏差 |
|---|---|---|---|---|---|---|---|---|
| A-0261 | `0x105` | `OP_QADD` | arm_instructions.cpp:3209 | arm_jit.cpp:1284 | — | arm_jit_arm.cpp:1673 | todo | |
| A-0293 | `0x125` | `OP_QSUB` | arm_instructions.cpp:3230 | arm_jit.cpp:1285 | — | arm_jit_arm.cpp:1674 | todo | |
| A-0325 | `0x145` | `OP_QDADD` | arm_instructions.cpp:3251 | arm_jit.cpp:1286 | — | arm_jit_arm.cpp:1675 | todo | |
| A-0353 | `0x161` | `OP_CLZ` | arm_instructions.cpp:3173 | arm_jit.cpp:2576 | — | arm_jit_arm.cpp:3068 | diff-pass | |
| A-0357 | `0x165` | `OP_QDSUB` | arm_instructions.cpp:3280 | arm_jit.cpp:1287 | — | arm_jit_arm.cpp:1676 | todo | |

### 分支（5）

| id | 槽位 | handler | 解释器 | x86 | x86 宏体 | arm64 | 状态 | 偏差 |
|---|---|---|---|---|---|---|---|---|
| A-0289 | `0x121` | `OP_BX` | arm_instructions.cpp:3084 | arm_jit.cpp:2570 | — | arm_jit_arm.cpp:3062 | diff-pass | |
| A-0291 | `0x123` | `OP_BLX_REG` | arm_instructions.cpp:3099 | arm_jit.cpp:2571 | — | arm_jit_arm.cpp:3063 | diff-pass | |
| A-0295 | `0x127` | `OP_BKPT` | arm_instructions.cpp:6306 | arm_jit.cpp:3103 | — | arm_jit_arm.cpp:3585 | diff-pass | |
| A-2560 | `0xA00..0xAFF` | `OP_B` | arm_instructions.cpp:3116 | arm_jit.cpp:2540 | — | arm_jit_arm.cpp:3018 | diff-pass | |
| A-2816 | `0xB00..0xBFF` | `OP_BL` | arm_instructions.cpp:3140 | arm_jit.cpp:2541 | — | arm_jit_arm.cpp:3019 | diff-pass | |

### 异常/未定义（2）

| id | 槽位 | handler | 解释器 | x86 | x86 宏体 | arm64 | 状态 | 偏差 |
|---|---|---|---|---|---|---|---|---|
| A-0043 | `0x02B,0x02D,0x02F,0x03B,0x03D,0x03F,…（共 348 槽）` | `OP_UND` | arm_instructions.cpp:255 | `NULL` | — | `NULL` | interp-fallback | |
| A-3840 | `0xF00..0xFFF` | `OP_SWI` | arm_instructions.cpp:6259 | arm_jit.cpp:3098 | — | arm_jit_arm.cpp:3572 | diff-pass | |

## Thumb

### 数据处理（26）

| id | 槽位 | handler | 解释器 | x86 | x86 宏体 | arm64 | 状态 | 偏差 |
|---|---|---|---|---|---|---|---|---|
| T-0096 | `0x060..0x067` | `OP_ADD_REG` | thumb_instructions.cpp:250 | arm_jit.cpp:3362 | — | arm_jit_arm.cpp:3810 | todo | |
| T-0104 | `0x068..0x06F` | `OP_SUB_REG` | thumb_instructions.cpp:327 | arm_jit.cpp:3451 | — | arm_jit_arm.cpp:3896 | todo | |
| T-0112 | `0x070..0x077` | `OP_ADD_IMM3` | thumb_instructions.cpp:211 | arm_jit.cpp:3328 | — | arm_jit_arm.cpp:3776 | todo | |
| T-0120 | `0x078..0x07F` | `OP_SUB_IMM3` | thumb_instructions.cpp:297 | arm_jit.cpp:3425 | — | arm_jit_arm.cpp:3870 | todo | |
| T-0128 | `0x080..0x09F` | `OP_MOV_IMM8` | thumb_instructions.cpp:346 | arm_jit.cpp:3500 | — | arm_jit_arm.cpp:3952 | todo | |
| T-0160 | `0x0A0..0x0BF` | `OP_CMP_IMM8` | thumb_instructions.cpp:373 | arm_jit.cpp:3556 | — | arm_jit_arm.cpp:4007 | todo | |
| T-0192 | `0x0C0..0x0DF` | `OP_ADD_IMM8` | thumb_instructions.cpp:236 | arm_jit.cpp:3355 | — | arm_jit_arm.cpp:3803 | todo | |
| T-0224 | `0x0E0..0x0FF` | `OP_SUB_IMM8` | thumb_instructions.cpp:312 | arm_jit.cpp:3444 | — | arm_jit_arm.cpp:3889 | todo | |
| T-0256 | `0x100` | `OP_AND` | thumb_instructions.cpp:415 | arm_jit.cpp:3302 | `OP_LOGIC` | arm_jit_arm.cpp:3750 | todo | |
| T-0257 | `0x101` | `OP_EOR` | thumb_instructions.cpp:427 | arm_jit.cpp:3304 | `OP_LOGIC` | arm_jit_arm.cpp:3752 | todo | |
| T-0261 | `0x105` | `OP_ADC_REG` | thumb_instructions.cpp:440 | arm_jit.cpp:3474 | — | arm_jit_arm.cpp:3921 | todo | |
| T-0262 | `0x106` | `OP_SBC_REG` | thumb_instructions.cpp:465 | arm_jit.cpp:3487 | — | arm_jit_arm.cpp:3937 | todo | |
| T-0264 | `0x108` | `OP_TST` | thumb_instructions.cpp:523 | arm_jit.cpp:3594 | — | arm_jit_arm.cpp:4050 | todo | |
| T-0265 | `0x109` | `OP_NEG` | thumb_instructions.cpp:536 | arm_jit.cpp:3310 | — | arm_jit_arm.cpp:3758 | todo | |
| T-0266 | `0x10A` | `OP_CMP` | thumb_instructions.cpp:385 | arm_jit.cpp:3563 | — | arm_jit_arm.cpp:4016 | todo | |
| T-0267 | `0x10B` | `OP_CMN` | thumb_instructions.cpp:571 | arm_jit.cpp:3582 | — | arm_jit_arm.cpp:4037 | todo | |
| T-0268 | `0x10C` | `OP_ORR` | thumb_instructions.cpp:587 | arm_jit.cpp:3303 | `OP_LOGIC` | arm_jit_arm.cpp:3751 | todo | |
| T-0270 | `0x10E` | `OP_BIC` | thumb_instructions.cpp:601 | arm_jit.cpp:3305 | `OP_LOGIC` | arm_jit_arm.cpp:3753 | todo | |
| T-0271 | `0x10F` | `OP_MVN` | thumb_instructions.cpp:615 | arm_jit.cpp:3524 | — | arm_jit_arm.cpp:3975 | todo | |
| T-0272 | `0x110..0x113` | `OP_ADD_SPE` | thumb_instructions.cpp:264 | arm_jit.cpp:3388 | — | arm_jit_arm.cpp:3835 | todo | |
| T-0276 | `0x114..0x117` | `OP_CMP_SPE` | thumb_instructions.cpp:397 | arm_jit.cpp:3572 | — | arm_jit_arm.cpp:4026 | todo | |
| T-0280 | `0x118..0x11B` | `OP_MOV_SPE` | thumb_instructions.cpp:355 | arm_jit.cpp:3508 | — | arm_jit_arm.cpp:3961 | todo | |
| T-0640 | `0x280..0x29F` | `OP_ADD_2PC` | thumb_instructions.cpp:279 | arm_jit.cpp:3403 | — | arm_jit_arm.cpp:3849 | todo | |
| T-0672 | `0x2A0..0x2BF` | `OP_ADD_2SP` | thumb_instructions.cpp:286 | arm_jit.cpp:3410 | — | arm_jit_arm.cpp:3857 | todo | |
| T-0704 | `0x2C0..0x2C1` | `OP_ADJUST_P_SP` | thumb_instructions.cpp:834 | arm_jit.cpp:3766 | — | arm_jit_arm.cpp:4241 | todo | |
| T-0706 | `0x2C2..0x2C3` | `OP_ADJUST_M_SP` | thumb_instructions.cpp:841 | arm_jit.cpp:3767 | — | arm_jit_arm.cpp:4242 | todo | |

### 移位（10）

| id | 槽位 | handler | 解释器 | x86 | x86 宏体 | arm64 | 状态 | 偏差 |
|---|---|---|---|---|---|---|---|---|
| T-0000 | `0x000` | `OP_LSL_0` | thumb_instructions.cpp:51 | arm_jit.cpp:3175 | — | arm_jit_arm.cpp:3653 | todo | |
| T-0001 | `0x001..0x01F` | `OP_LSL` | thumb_instructions.cpp:60 | arm_jit.cpp:3189 | `OP_SHIFTS_IMM` | arm_jit_arm.cpp:3671 | todo | |
| T-0032 | `0x020` | `OP_LSR_0` | thumb_instructions.cpp:105 | arm_jit.cpp:3191 | — | arm_jit_arm.cpp:3674 | todo | |
| T-0033 | `0x021..0x03F` | `OP_LSR` | thumb_instructions.cpp:115 | arm_jit.cpp:3200 | `OP_SHIFTS_IMM` | arm_jit_arm.cpp:3684 | todo | |
| T-0064 | `0x040` | `OP_ASR_0` | thumb_instructions.cpp:159 | arm_jit.cpp:3202 | — | arm_jit_arm.cpp:3687 | todo | |
| T-0065 | `0x041..0x05F` | `OP_ASR` | thumb_instructions.cpp:169 | arm_jit.cpp:3219 | `OP_SHIFTS_IMM` | arm_jit_arm.cpp:3720 | todo | |
| T-0258 | `0x102` | `OP_LSL_REG` | thumb_instructions.cpp:71 | arm_jit.cpp:3190 | `OP_SHIFTS_REG` | arm_jit_arm.cpp:3672 | todo | |
| T-0259 | `0x103` | `OP_LSR_REG` | thumb_instructions.cpp:126 | arm_jit.cpp:3201 | `OP_SHIFTS_REG` | arm_jit_arm.cpp:3685 | todo | |
| T-0260 | `0x104` | `OP_ASR_REG` | thumb_instructions.cpp:180 | arm_jit.cpp:3220 | — | arm_jit_arm.cpp:3721 | todo | |
| T-0263 | `0x107` | `OP_ROR_REG` | thumb_instructions.cpp:492 | arm_jit.cpp:3257 | — | arm_jit_arm.cpp:3735 | todo | |

### 访存（17）

| id | 槽位 | handler | 解释器 | x86 | x86 宏体 | arm64 | 状态 | 偏差 |
|---|---|---|---|---|---|---|---|---|
| T-0288 | `0x120..0x13F` | `OP_LDR_PCREL` | thumb_instructions.cpp:821 | arm_jit.cpp:3713 | — | arm_jit_arm.cpp:4177 | todo | |
| T-0320 | `0x140..0x147` | `OP_STR_REG_OFF` | thumb_instructions.cpp:786 | arm_jit.cpp:3674 | — | arm_jit_arm.cpp:4123 | todo | |
| T-0328 | `0x148..0x14F` | `OP_STRH_REG_OFF` | thumb_instructions.cpp:734 | arm_jit.cpp:3668 | — | arm_jit_arm.cpp:4117 | todo | |
| T-0336 | `0x150..0x157` | `OP_STRB_REG_OFF` | thumb_instructions.cpp:685 | arm_jit.cpp:3662 | — | arm_jit_arm.cpp:4111 | todo | |
| T-0344 | `0x158..0x15F` | `OP_LDRSB_REG_OFF` | thumb_instructions.cpp:705 | arm_jit.cpp:3664 | — | arm_jit_arm.cpp:4113 | todo | |
| T-0352 | `0x160..0x167` | `OP_LDR_REG_OFF` | thumb_instructions.cpp:794 | arm_jit.cpp:3675 | — | arm_jit_arm.cpp:4124 | todo | |
| T-0360 | `0x168..0x16F` | `OP_LDRH_REG_OFF` | thumb_instructions.cpp:742 | arm_jit.cpp:3669 | — | arm_jit_arm.cpp:4118 | todo | |
| T-0368 | `0x170..0x177` | `OP_LDRB_REG_OFF` | thumb_instructions.cpp:693 | arm_jit.cpp:3663 | — | arm_jit_arm.cpp:4112 | todo | |
| T-0376 | `0x178..0x17F` | `OP_LDRSH_REG_OFF` | thumb_instructions.cpp:754 | arm_jit.cpp:3670 | — | arm_jit_arm.cpp:4119 | todo | |
| T-0384 | `0x180..0x19F` | `OP_STR_IMM_OFF` | thumb_instructions.cpp:766 | arm_jit.cpp:3672 | — | arm_jit_arm.cpp:4121 | todo | |
| T-0416 | `0x1A0..0x1BF` | `OP_LDR_IMM_OFF` | thumb_instructions.cpp:774 | arm_jit.cpp:3673 | — | arm_jit_arm.cpp:4122 | todo | |
| T-0448 | `0x1C0..0x1DF` | `OP_STRB_IMM_OFF` | thumb_instructions.cpp:668 | arm_jit.cpp:3660 | — | arm_jit_arm.cpp:4109 | todo | |
| T-0480 | `0x1E0..0x1FF` | `OP_LDRB_IMM_OFF` | thumb_instructions.cpp:676 | arm_jit.cpp:3661 | — | arm_jit_arm.cpp:4110 | todo | |
| T-0512 | `0x200..0x21F` | `OP_STRH_IMM_OFF` | thumb_instructions.cpp:717 | arm_jit.cpp:3666 | — | arm_jit_arm.cpp:4115 | todo | |
| T-0544 | `0x220..0x23F` | `OP_LDRH_IMM_OFF` | thumb_instructions.cpp:725 | arm_jit.cpp:3667 | — | arm_jit_arm.cpp:4116 | todo | |
| T-0576 | `0x240..0x25F` | `OP_STR_SPREL` | thumb_instructions.cpp:805 | arm_jit.cpp:3677 | — | arm_jit_arm.cpp:4126 | todo | |
| T-0608 | `0x260..0x27F` | `OP_LDR_SPREL` | thumb_instructions.cpp:813 | arm_jit.cpp:3695 | — | arm_jit_arm.cpp:4151 | todo | |

### LDM/STM（6）

| id | 槽位 | handler | 解释器 | x86 | x86 宏体 | arm64 | 状态 | 偏差 |
|---|---|---|---|---|---|---|---|---|
| T-0720 | `0x2D0..0x2D3` | `OP_PUSH` | thumb_instructions.cpp:852 | arm_jit.cpp:3794 | — | arm_jit_arm.cpp:4273 | todo | |
| T-0724 | `0x2D4..0x2D7` | `OP_PUSH_LR` | thumb_instructions.cpp:869 | arm_jit.cpp:3795 | — | arm_jit_arm.cpp:4274 | todo | |
| T-0752 | `0x2F0..0x2F3` | `OP_POP` | thumb_instructions.cpp:890 | arm_jit.cpp:3796 | — | arm_jit_arm.cpp:4275 | todo | |
| T-0756 | `0x2F4..0x2F7` | `OP_POP_PC` | thumb_instructions.cpp:914 | arm_jit.cpp:3797 | — | arm_jit_arm.cpp:4276 | todo | |
| T-0768 | `0x300..0x31F` | `OP_STMIA_THUMB` | thumb_instructions.cpp:944 | arm_jit.cpp:3761 | — | arm_jit_arm.cpp:4236 | todo | |
| T-0800 | `0x320..0x33F` | `OP_LDMIA_THUMB` | thumb_instructions.cpp:975 | arm_jit.cpp:3760 | — | arm_jit_arm.cpp:4235 | todo | |

### 乘法/乘加（1）

| id | 槽位 | handler | 解释器 | x86 | x86 宏体 | arm64 | 状态 | 偏差 |
|---|---|---|---|---|---|---|---|---|
| T-0269 | `0x10D` | `OP_MUL_REG` | thumb_instructions.cpp:641 | arm_jit.cpp:3538 | — | arm_jit_arm.cpp:3988 | todo | |

### 分支（8）

| id | 槽位 | handler | 解释器 | x86 | x86 宏体 | arm64 | 状态 | 偏差 |
|---|---|---|---|---|---|---|---|---|
| T-0284 | `0x11C..0x11D` | `OP_BX_THUMB` | thumb_instructions.cpp:1129 | arm_jit.cpp:3895 | — | arm_jit_arm.cpp:4395 | todo | |
| T-0286 | `0x11E..0x11F` | `OP_BLX_THUMB` | thumb_instructions.cpp:1169 | arm_jit.cpp:3896 | — | arm_jit_arm.cpp:4396 | todo | |
| T-0760 | `0x2F8..0x2FB` | `OP_BKPT_THUMB` | thumb_instructions.cpp:1012 | `NULL` | — | `NULL` | interp-fallback | |
| T-0832 | `0x340..0x37B` | `OP_B_COND` | thumb_instructions.cpp:1080 | arm_jit.cpp:3802 | — | arm_jit_arm.cpp:4281 | todo | |
| T-0896 | `0x380..0x39F` | `OP_B_UNCOND` | thumb_instructions.cpp:1090 | arm_jit.cpp:3818 | — | arm_jit_arm.cpp:4298 | todo | |
| T-0928 | `0x3A0..0x3BF` | `OP_BLX` | thumb_instructions.cpp:1106 | arm_jit.cpp:3825 | — | arm_jit_arm.cpp:4306 | todo | |
| T-0960 | `0x3C0..0x3DF` | `OP_BL_10` | thumb_instructions.cpp:1115 | arm_jit.cpp:3838 | — | arm_jit_arm.cpp:4324 | todo | |
| T-0992 | `0x3E0..0x3FF` | `OP_BL_11` | thumb_instructions.cpp:1121 | arm_jit.cpp:3845 | — | arm_jit_arm.cpp:4331 | todo | |

### 异常/未定义（2）

| id | 槽位 | handler | 解释器 | x86 | x86 宏体 | arm64 | 状态 | 偏差 |
|---|---|---|---|---|---|---|---|---|
| T-0708 | `0x2C4..0x2CF,0x2D8..0x2EF,0x2FC..0x2FF` | `OP_UND_THUMB` | thumb_instructions.cpp:40 | `NULL` | — | `NULL` | interp-fallback | |
| T-0892 | `0x37C..0x37F` | `OP_SWI_THUMB` | thumb_instructions.cpp:1031 | arm_jit.cpp:3898 | — | arm_jit_arm.cpp:4398 | todo | |

