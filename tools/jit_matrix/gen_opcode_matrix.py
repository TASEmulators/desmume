#!/usr/bin/env python3
"""Generate the JIT migration worklist (one row per ARM/Thumb instruction handler).

The three implementations -- the interpreter, the x86/x64 JIT and the AArch64 JIT --
all build their dispatch table from the same two .inc files:

    src/instruction_tabdef.inc   4096 slots
    src/thumb_tabdef.inc         1024 slots

so "which handler owns which slot" cannot differ between them.  What does differ is
the implementation of each handler, and that is what the migration has to verify one
instruction at a time.  This script turns those two files into that worklist:

    documentation/jit-opcode-matrix.md    human readable, grouped by family
    documentation/jit-opcode-matrix.csv   machine readable, for the diff harness

Two facts it checks that are easy to get wrong by hand:

  * A handler that is `#define OP_X NULL` in a backend is never compiled by that
    backend: `emit_armop_call` sees a null compiler and calls the interpreter
    instead.  Those cost nothing to migrate and cannot diverge -- they are already
    the reference.  Both backends must agree on the set.
  * The real migration surface is therefore smaller than the slot table suggests.

Nothing here touches the emulator; it is pure static analysis of the source tree.
Run it from the repository root:

    python tools/jit_matrix/gen_opcode_matrix.py

Exit code is non-zero if a handler is missing from one of the three implementations,
if the two backends disagree about which handlers they fall back on, or if the family
totals drift from the numbers the migration plan was sized against.
"""

from __future__ import annotations

import csv
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "desmume" / "src"
DOC = ROOT / "documentation"

ARM_TABDEF = SRC / "instruction_tabdef.inc"
THUMB_TABDEF = SRC / "thumb_tabdef.inc"
INTERP_ARM = SRC / "arm_instructions.cpp"
INTERP_THUMB = SRC / "thumb_instructions.cpp"
JIT_X86 = SRC / "arm_jit.cpp"
JIT_ARM64 = SRC / "utils" / "arm_jit" / "arm_jit_arm.cpp"

# Family order is the migration order (plan section S4), so the table doubles as a
# schedule.  "data" + "shift" are one family in the plan's summary table (252); they
# are split here because they are separate batches.
FAMILIES = [
    ("data", "数据处理"),
    ("shift", "移位"),
    ("mem", "访存"),
    ("ldm", "LDM/STM"),
    ("mul", "乘法/乘加"),
    ("coproc", "协处理器"),
    ("psr", "状态寄存器"),
    ("sat", "饱和/计数"),
    ("branch", "分支"),
    ("und", "异常/未定义"),
]

# Measured against the dispatch tables.  If these drift, the plan's batch sizes and
# time estimates need revisiting, so the script fails loudly instead of silently
# producing a slightly different worklist.
EXPECTED_ARM = {
    "data": 28, "shift": 224, "mem": 174, "ldm": 32, "mul": 28,
    "coproc": 17, "psr": 6, "sat": 5, "branch": 5, "und": 2,
}
EXPECTED_ARM_TOTAL = 521
EXPECTED_THUMB_TOTAL = 70
EXPECTED_SLOTS_ARM = 4096
EXPECTED_SLOTS_THUMB = 1024
EXPECTED_NULL_PER_BACKEND = 20

# ARM mnemonics whose 18 (or 9) variants make up the data-processing family.
DP_MNEMONICS = ("ADC", "ADD", "AND", "BIC", "EOR", "MOV", "MVN",
                "ORR", "RSB", "RSC", "SBC", "SUB", "CMN", "CMP", "TEQ", "TST")

# Handlers whose differential gate has actually been run green. Kept here rather
# than in the generated CSV/MD: both are rebuilt from the dispatch tables on
# every run, so a status edited in the output would vanish on the next regen.
# `diff-pass` is not `perf-ok` -- family 1 passed the semantic sweep but missed
# the 50% code-size gate (see the plan's S4 family-1 section).
DIFF_PASS_HANDLERS = frozenset({
    "OP_AND_IMM_VAL", "OP_AND_S_IMM_VAL", "OP_EOR_IMM_VAL", "OP_EOR_S_IMM_VAL",
    "OP_SUB_IMM_VAL", "OP_SUB_S_IMM_VAL", "OP_RSB_IMM_VAL", "OP_RSB_S_IMM_VAL",
    "OP_ADD_IMM_VAL", "OP_ADD_S_IMM_VAL", "OP_ADC_IMM_VAL", "OP_ADC_S_IMM_VAL",
    "OP_SBC_IMM_VAL", "OP_SBC_S_IMM_VAL", "OP_RSC_IMM_VAL", "OP_RSC_S_IMM_VAL",
    "OP_TST_IMM_VAL", "OP_TEQ_IMM_VAL", "OP_CMP_IMM_VAL", "OP_CMN_IMM_VAL",
    "OP_ORR_IMM_VAL", "OP_ORR_S_IMM_VAL", "OP_MOV_IMM_VAL", "OP_MOV_S_IMM_VAL",
    "OP_BIC_IMM_VAL", "OP_BIC_S_IMM_VAL", "OP_MVN_IMM_VAL", "OP_MVN_S_IMM_VAL",
    # Batch 6 (2026-11-20): PSR access, branches and the exception entries.
    # Verified on AArch64 hardware with tools/jit_matrix/a64_block_probe.cpp,
    # which runs the real compile_basicblock() over these and compares the full
    # architectural state against the pre-batch build.
    "OP_MRS_CPSR", "OP_MRS_SPSR",
    "OP_MSR_CPSR", "OP_MSR_SPSR", "OP_MSR_CPSR_IMM_VAL", "OP_MSR_SPSR_IMM_VAL",
    "OP_B", "OP_BL", "OP_BX", "OP_BLX_REG", "OP_SWI", "OP_BKPT",
    # Batch 5 (commit 5f3ffbc8) was verified the same way but never landed here,
    # so the ledger said 252 while the notes said 280. Recorded now.
    "OP_MUL", "OP_MUL_S", "OP_MLA", "OP_MLA_S",
    "OP_UMULL", "OP_UMULL_S", "OP_UMLAL", "OP_UMLAL_S",
    "OP_SMULL", "OP_SMULL_S", "OP_SMLAL", "OP_SMLAL_S",
    "OP_SMUL_B_B", "OP_SMUL_B_T", "OP_SMUL_T_B", "OP_SMUL_T_T",
    "OP_SMLA_B_B", "OP_SMLA_B_T", "OP_SMLA_T_B", "OP_SMLA_T_T",
    "OP_SMLAL_B_B", "OP_SMLAL_B_T", "OP_SMLAL_T_B", "OP_SMLAL_T_T",
    "OP_SMULW_B", "OP_SMULW_T", "OP_SMLAW_B", "OP_SMLAW_T",
    "OP_CLZ",
})
SHIFT_TOKEN = re.compile(r"_(LSL|LSR|ASR|ROR)_(IMM|REG)$|^OP_(LSL|LSR|ASR|ROR|RRX)")
COPROC = ("OP_MCR", "OP_MRC", "OP_CDP", "OP_LDC", "OP_STC")


@dataclass
class Handler:
    name: str
    isa: str
    slots: list[int] = field(default_factory=list)
    interp_line: int = 0
    interp_file: str = ""
    x86_line: int = 0
    x86_macro: str = ""
    arm64_line: int = 0
    family: str = ""
    null_x86: bool = False
    null_arm64: bool = False

    @property
    def first_slot(self) -> int:
        return self.slots[0] if self.slots else -1

    @property
    def fallback(self) -> bool:
        """Never compiled by either backend; always runs on the interpreter."""
        return self.null_x86 and self.null_arm64

    @property
    def status(self) -> str:
        if self.fallback:
            return "interp-fallback"
        # Family 2 (2026-11-20): every ARM shift handler now routes through
        # the register cache and passed the full + seq 2/3/4 differential.
        # Thumb shifts stay todo (batch 8).
        if self.isa == "ARM" and self.family == "shift":
            return "diff-pass"
        return "diff-pass" if self.name in DIFF_PASS_HANDLERS else "todo"

    @property
    def slot_spec(self) -> str:
        """Compact slot description: contiguous runs collapse to a range."""
        return self._slot_runs(abbrev_limit=None)

    @property
    def slot_spec_brief(self) -> str:
        """Same, but long lists are truncated so the markdown table stays readable.

        The full list is always available in the CSV and in slot_count.
        """
        return self._slot_runs(abbrev_limit=6)

    def _slot_runs(self, abbrev_limit: int | None) -> str:
        s = self.slots
        if not s:
            return "-"
        runs, start, prev = [], s[0], s[0]
        for v in s[1:]:
            if v == prev + 1:
                prev = v
                continue
            runs.append((start, prev))
            start = prev = v
        runs.append((start, prev))

        # 12 bits for ARM, 10 for Thumb, so the widest slot in the table sets the width.
        width = 4 if max(s) > 0xFFF else 3

        def one(a: int, b: int) -> str:
            return f"0x{a:0{width}X}" if a == b else f"0x{a:0{width}X}..0x{b:0{width}X}"

        if abbrev_limit is not None and len(runs) > abbrev_limit:
            head = ",".join(one(a, b) for a, b in runs[:abbrev_limit])
            return f"{head},…（共 {len(s)} 槽）"
        return ",".join(one(a, b) for a, b in runs)


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace")


def parse_tabdef(path: Path) -> list[str]:
    """Return the TABDECL argument for every slot, in file order."""
    return re.findall(r"TABDECL\(\s*(\w+)\s*\)", read_text(path))


def find_definitions(text: str) -> dict[str, int]:
    """Map handler name -> 1-based line of its definition.

    Every implementation spells it the same way, so one pattern covers all three:
        static <ret> [DESMUME_FASTCALL] NAME(const u32 i)
    """
    out: dict[str, int] = {}
    pattern = re.compile(
        r"^[ \t]*(?:TEMPLATE[ \t]+)?static[ \t]+\w+[ \t]+(?:DESMUME_FASTCALL[ \t]+)?(\w+)[ \t]*\([ \t]*const u32 i[ \t]*\)",
        re.M,
    )
    for m in pattern.finditer(text):
        out.setdefault(m.group(1), text.count("\n", 0, m.start()) + 1)
    return out


def find_x86_macros(text: str) -> dict[str, str]:
    """Map handler name -> macro it forwards to, for the one-line macro wrappers.

    460 of the 571 real x86 handlers are exactly `{ OP_SOMETHING(...); }`.  Knowing
    which macro body a handler routes through is what makes the x86 side look like
    ~17 edit sites instead of 571.
    """
    out: dict[str, str] = {}
    pattern = re.compile(
        r"static[ \t]+int[ \t]+(\w+)[ \t]*\([ \t]*const u32 i[ \t]*\)[ \t]*\{[ \t]*(OP_\w+)[ \t]*\("
    )
    for m in pattern.finditer(text):
        out[m.group(1)] = m.group(2)
    return out


def find_null_handlers(text: str) -> set[str]:
    """Handlers the backend does not compile: `#define OP_X NULL`."""
    return set(re.findall(r"^[ \t]*#[ \t]*define[ \t]+(OP_\w+)[ \t]+NULL\b", text, re.M))


def classify_arm(name: str) -> str:
    m = re.match(r"OP_(\w+)", name)
    if not m:
        return "data"
    rest = m.group(1)

    if name.startswith(COPROC):
        return "coproc"
    if name in ("OP_UND", "OP_SWI"):
        return "und"
    if name in ("OP_MRS_CPSR", "OP_MRS_SPSR") or name.startswith("OP_MSR"):
        return "psr"
    if name in ("OP_QADD", "OP_QDADD", "OP_QDSUB", "OP_QSUB", "OP_CLZ"):
        return "sat"
    if name in ("OP_B", "OP_BL", "OP_BX", "OP_BLX_REG", "OP_BKPT"):
        return "branch"
    if name.startswith(("OP_LDM", "OP_STM")):
        return "ldm"
    if name.startswith(("OP_LDR", "OP_STR", "OP_SWP")):
        return "mem"
    if name.startswith(("OP_MUL", "OP_MLA", "OP_UMULL", "OP_SMULL", "OP_UMLAL",
                        "OP_SMLAL", "OP_SMLA", "OP_SMUL", "OP_SMULW", "OP_SMLAW")):
        return "mul"
    if rest.split("_")[0] in DP_MNEMONICS:
        return "shift" if SHIFT_TOKEN.search(name) else "data"
    return "data"


def classify_thumb(name: str) -> str:
    """Thumb names carry no macro structure; bucket them for reporting only."""
    if name in ("OP_UND_THUMB", "OP_SWI_THUMB"):
        return "und"
    if name == "OP_BKPT_THUMB":
        return "branch"
    if re.match(r"^OP_(B_COND|B_UNCOND|BL|BLX|BX)", name):
        return "branch"
    if re.match(r"^OP_(LDMIA|STMIA|PUSH|POP)", name):
        return "ldm"
    if re.match(r"^OP_(LDR|STR)", name):
        return "mem"
    if re.match(r"^OP_(MUL|MLA)", name):
        return "mul"
    if re.match(r"^OP_(LSL|LSR|ASR|ROR)", name):
        return "shift"
    return "data"


def build(thumb: bool) -> tuple[list[Handler], list[str]]:
    tabdef = THUMB_TABDEF if thumb else ARM_TABDEF
    interp = INTERP_THUMB if thumb else INTERP_ARM
    isa = "Thumb" if thumb else "ARM"
    classify = classify_thumb if thumb else classify_arm

    names = parse_tabdef(tabdef)
    handlers: dict[str, Handler] = {}
    for slot, name in enumerate(names):
        h = handlers.get(name)
        if h is None:
            h = handlers[name] = Handler(name=name, isa=isa)
        h.slots.append(slot)

    for h in handlers.values():
        h.family = classify(h.name)

    interp_defs = find_definitions(read_text(interp))
    x86_text = read_text(JIT_X86)
    a64_text = read_text(JIT_ARM64)
    x86_defs = find_definitions(x86_text)
    a64_defs = find_definitions(a64_text)
    x86_macros = find_x86_macros(x86_text)
    x86_null = find_null_handlers(x86_text)
    a64_null = find_null_handlers(a64_text)

    for h in handlers.values():
        h.interp_line = interp_defs.get(h.name, 0)
        h.interp_file = interp.name
        h.x86_line = x86_defs.get(h.name, 0)
        h.x86_macro = x86_macros.get(h.name, "")
        h.arm64_line = a64_defs.get(h.name, 0)
        h.null_x86 = h.name in x86_null
        h.null_arm64 = h.name in a64_null

    order = [k for k, _ in FAMILIES]
    return sorted(handlers.values(), key=lambda h: (order.index(h.family), h.first_slot)), names


def verify(arm: list[Handler], thumb: list[Handler],
           arm_slots: list[str], thumb_slots: list[str]) -> list[str]:
    problems: list[str] = []

    if len(arm_slots) != EXPECTED_SLOTS_ARM:
        problems.append(f"ARM tabdef has {len(arm_slots)} slots, expected {EXPECTED_SLOTS_ARM}")
    if len(thumb_slots) != EXPECTED_SLOTS_THUMB:
        problems.append(f"Thumb tabdef has {len(thumb_slots)} slots, expected {EXPECTED_SLOTS_THUMB}")
    if len(arm) != EXPECTED_ARM_TOTAL:
        problems.append(f"ARM handlers: {len(arm)}, expected {EXPECTED_ARM_TOTAL}")
    if len(thumb) != EXPECTED_THUMB_TOTAL:
        problems.append(f"Thumb handlers: {len(thumb)}, expected {EXPECTED_THUMB_TOTAL}")

    for fam, want in EXPECTED_ARM.items():
        got = sum(1 for h in arm if h.family == fam)
        if got != want:
            problems.append(f"family {fam}: {got} handlers, expected {want}")

    for h in arm + thumb:
        if not h.fallback:
            if h.x86_line == 0:
                problems.append(f"{h.name}: no x86 JIT definition and not NULL")
            if h.arm64_line == 0:
                problems.append(f"{h.name}: no arm64 JIT definition and not NULL")
        if h.interp_line == 0:
            problems.append(f"{h.name}: no interpreter definition")
        if h.null_x86 != h.null_arm64:
            problems.append(
                f"{h.name}: backend disagreement -- x86 {'NULL' if h.null_x86 else 'compiles'}, "
                f"arm64 {'NULL' if h.null_arm64 else 'compiles'}"
            )

    n_x86 = sum(1 for h in arm + thumb if h.null_x86)
    n_a64 = sum(1 for h in arm + thumb if h.null_arm64)
    if n_x86 != EXPECTED_NULL_PER_BACKEND:
        problems.append(f"x86 NULL handlers: {n_x86}, expected {EXPECTED_NULL_PER_BACKEND}")
    if n_a64 != EXPECTED_NULL_PER_BACKEND:
        problems.append(f"arm64 NULL handlers: {n_a64}, expected {EXPECTED_NULL_PER_BACKEND}")

    return problems


def write_csv(path: Path, arm: list[Handler], thumb: list[Handler]) -> None:
    with path.open("w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow([
            "id", "isa", "family", "handler", "slots", "first_slot", "slot_count",
            "interp_file", "interp_line", "x86_line", "x86_macro", "arm64_line",
            "fallback", "status", "note",
        ])
        for h in arm + thumb:
            w.writerow([
                f"{h.isa[0]}-{h.first_slot:04d}", h.isa, h.family, h.name, h.slot_spec,
                h.first_slot, len(h.slots), h.interp_file, h.interp_line,
                h.x86_line, h.x86_macro, h.arm64_line,
                int(h.fallback), h.status, "",
            ])


def write_md(path: Path, arm: list[Handler], thumb: list[Handler]) -> None:
    allh = arm + thumb
    total = len(allh)
    real = sum(1 for h in allh if not h.fallback)
    fb = total - real
    rows: list[str] = []
    a = rows.append

    a("# JIT 迁移工单表")
    a("")
    a("> 由 `tools/jit_matrix/gen_opcode_matrix.py` 自动生成，**不要手改**。")
    a("> 重新生成：`python tools/jit_matrix/gen_opcode_matrix.py`")
    a("")
    a(f"共 **{total}** 条指令（ARM {len(arm)} + Thumb {len(thumb)}），"
      f"覆盖 {EXPECTED_SLOTS_ARM + EXPECTED_SLOTS_THUMB} 个分派槽位。")
    a("")
    a(f"其中 **{fb}** 条在两个后端都是 `#define OP_X NULL`："
      "`emit_armop_call` 发现编译器为空就直接调解释器，**本来就不生成代码**，"
      "既不会分叉也不需要迁移。")
    a(f"真正要迁的是 **{real}** 条。")
    a("")
    a("三方共用同一张分派表（`instruction_tabdef.inc` / `thumb_tabdef.inc`），"
      "所以「哪个槽由哪个 handler 负责」三方不存在分歧——分歧只可能在实现里。")
    a("")
    a("**状态**：`todo` → `x86-done` → `a64-struct-done` → `diff-pass` → `perf-ok`；"
      "回退解释器的置 `interp-fallback`（无需处理）；有争议置 `disputed`。")
    a("")

    a("## 进度")
    a("")
    a("| 族 | ARM | Thumb | 合计 | 其中回退解释器 | 已 diff-pass |")
    a("|---|---|---|---|---|---|")
    for key, label in FAMILIES:
        ga = [h for h in arm if h.family == key]
        gt = [h for h in thumb if h.family == key]
        if not ga and not gt:
            continue
        g = ga + gt
        a(f"| {label} | {len(ga)} | {len(gt)} | {len(g)} | {sum(1 for h in g if h.fallback)} | {sum(1 for h in g if h.status == 'diff-pass')} |")
    a(f"| **合计** | **{len(arm)}** | **{len(thumb)}** | **{total}** | **{fb}** | **{sum(1 for h in allh if h.status == 'diff-pass')}** |")
    a("")

    a("## 回退解释器的指令（两后端一致）")
    a("")
    a("这些 handler 在 `arm_jit.cpp` 和 `arm_jit_arm.cpp` 里都被 `#define` 成 `NULL`，"
      "编译时根本不进入 `compile_basicblock`。迁移期间它们**不需要改**，"
      "但如果某天有人给其中一个补上实现，就必须同时补两个后端，否则两个后端会在同一份游戏上表现不同。")
    a("")
    a("| ISA | handler | 槽位 | 族 |")
    a("|---|---|---|---|")
    for h in allh:
        if h.fallback:
            fam = dict(FAMILIES)[h.family]
            a(f"| {h.isa} | `{h.name}` | `{h.slot_spec_brief}` | {fam} |")
    a("")

    for isa, handlers in (("ARM", arm), ("Thumb", thumb)):
        a(f"## {isa}")
        a("")
        for key, label in FAMILIES:
            group = [h for h in handlers if h.family == key]
            if not group:
                continue
            a(f"### {label}（{len(group)}）")
            a("")
            a("| id | 槽位 | handler | 解释器 | x86 | x86 宏体 | arm64 | 状态 | 偏差 |")
            a("|---|---|---|---|---|---|---|---|---|")
            for h in group:
                hid = f"{h.isa[0]}-{h.first_slot:04d}"
                if h.fallback:
                    x86 = "`NULL`"
                    a64 = "`NULL`"
                    macro = "—"
                else:
                    x86 = f"arm_jit.cpp:{h.x86_line}" if h.x86_line else "**缺失**"
                    a64 = f"arm_jit_arm.cpp:{h.arm64_line}" if h.arm64_line else "**缺失**"
                    macro = f"`{h.x86_macro}`" if h.x86_macro else "—"
                interp = f"{h.interp_file}:{h.interp_line}" if h.interp_line else "**缺失**"
                a(f"| {hid} | `{h.slot_spec_brief}` | `{h.name}` | {interp} | {x86} | {macro} | {a64} | {h.status} | |")
            a("")

    # Match the CSV (csv.writer's default) and the checked-in file: CRLF. Writing
    # LF here made every regeneration look like a whole-file diff on Windows.
    path.write_bytes(("\r\n".join(rows) + "\r\n").encode("utf-8"))


def main() -> int:
    for p in (ARM_TABDEF, THUMB_TABDEF, INTERP_ARM, INTERP_THUMB, JIT_X86, JIT_ARM64):
        if not p.exists():
            print(f"missing: {p}", file=sys.stderr)
            return 2

    arm, arm_slots = build(thumb=False)
    thumb, thumb_slots = build(thumb=True)
    problems = verify(arm, thumb, arm_slots, thumb_slots)

    DOC.mkdir(parents=True, exist_ok=True)
    write_csv(DOC / "jit-opcode-matrix.csv", arm, thumb)
    write_md(DOC / "jit-opcode-matrix.md", arm, thumb)

    macros_arm = sorted({h.x86_macro for h in arm if h.x86_macro})
    macros_all = sorted({h.x86_macro for h in arm + thumb if h.x86_macro})
    allh = arm + thumb
    fb = [h for h in allh if h.fallback]

    print(f"ARM   : {len(arm):4d} handlers / {len(arm_slots)} slots")
    print(f"Thumb : {len(thumb):4d} handlers / {len(thumb_slots)} slots")
    print(f"total : {len(allh):4d} handlers, {len(fb)} fall back to the interpreter, "
          f"{len(allh) - len(fb)} to migrate")
    print(f"x86 macro bodies reachable from ARM handlers  : {len(macros_arm)}")
    print(f"x86 macro bodies reachable from ARM + Thumb   : {len(macros_all)}")
    for m in macros_all:
        where = "ARM+Thumb" if m in macros_arm else "Thumb only"
        print(f"        {m:20s} {where}")
    for key, label in FAMILIES:
        na = sum(1 for h in arm if h.family == key)
        nt = sum(1 for h in thumb if h.family == key)
        nf = sum(1 for h in allh if h.family == key and h.fallback)
        print(f"  {label:12s} ARM {na:4d}  Thumb {nt:3d}  (fallback {nf})")

    print(f"\nwrote {DOC / 'jit-opcode-matrix.md'}")
    print(f"wrote {DOC / 'jit-opcode-matrix.csv'}")

    if problems:
        print("\nPROBLEMS:", file=sys.stderr)
        for p in problems:
            print(f"  - {p}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
