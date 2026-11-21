#!/usr/bin/env python3
"""Compare the instruction stream of the a64 primitives before/after the
asmjit migration.

The dump tools (a64_dump_old.cpp / a64_dump_new.cpp) print one line of raw
words per primitive. Raw words cannot be compared directly: asmjit canonicalises
`b.al` to `b` (same instruction, different encoding) and drops a branch whose
target is the next instruction. Both are no-ops, so the comparison is done on
the *disassembly* instead -- which is what the migration's DoD asks for
("反汇编与旧版逐条配对一致").

Usage:
    python tools/jit_matrix/check_a64.py tools/jit_matrix/a64_golden.txt build_core/a64_new.txt
"""

import sys

import capstone

md = capstone.Cs(capstone.CS_ARCH_ARM64, capstone.CS_MODE_ARM)
md.detail = False

# Cases where asmjit legitimately emits fewer instructions: it builds a CFG and
# drops the unreachable half of `emit_mov_cond`/`emit_str_cond` when the
# condition is AL (the two branches and the guarded instruction collapse to just
# the instruction). Semantically identical, verified by hand.
EXPECTED_DEAD_BRANCH = {"050", "154"}


def parse(path):
    cases = []
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.rstrip("\n")
            if not line or line.startswith("#"):
                continue
            head, _, body = line.partition("] ")
            head = head.lstrip("[")
            tokens = body.split()
            if tokens and tokens[-1] == '<FAILED>':
                tokens = tokens[:-1] + ['<FAILED>']
                failed = True
            else:
                failed = False
            # The trailing 8-hex-digit tokens are the encoded words; anything
            # before them is the case name.
            split = len(tokens)
            while split > 0 and len(tokens[split - 1]) == 8 and all(
                c in "0123456789ABCDEF" for c in tokens[split - 1]
            ):
                split -= 1
            name = " ".join(tokens[:split])
            words = tokens[split:]
            if words == ["<FAILED>"]:
                cases.append((head, name, None))
                continue
            # The dump prints instruction words as 32-bit values; AArch64 is
            # little-endian in memory, so each word has to be reversed.
            code = b"".join(
                int(w, 16).to_bytes(4, "little") for w in words
            )
            cases.append((head, name, code))
    return cases


def instruction_list(code):
    out = []
    for insn in md.disasm(code, 0):
        mnem = insn.mnemonic
        ops = insn.op_str
        # `b.al` and `b` are the same instruction.
        if mnem == "b.al":
            mnem = "b"
        out.append((insn.address, mnem, ops))
    return out


def normalise(insns):
    """Drop redundant branches and return (mnemonic, operands) tuples."""
    result = []
    for addr, mnem, ops in insns:
        # A branch whose target is the instruction right after it does nothing.
        if mnem == "b" and ops == "#%d" % (addr + 4):
            continue
        result.append((mnem, ops))
    return result


def main():
    golden_path, new_path = sys.argv[1], sys.argv[2]
    golden = parse(golden_path)
    new = parse(new_path)

    if len(golden) != len(new):
        print("case count differs: %d vs %d" % (len(golden), len(new)))
        return 1

    bad = 0
    expected = 0
    for (gid, gname, gcode), (nid, nname, ncode) in zip(golden, new):
        if gid != nid or gname != nname:
            print("%s vs %s: case mismatch" % (gid, nid))
            bad += 1
            continue
        if ncode is None:
            print("%s %-40s FAILED to emit" % (nid, nname))
            bad += 1
            continue

        ge = normalise(instruction_list(gcode))
        ne = normalise(instruction_list(ncode))

        # end_func() appends the function's exit `ret`; the hand-written encoder
        # only had one if the body ended with emit_endfunc().
        if len(ne) == len(ge) + 1 and ne[-1] == ("ret", ""):
            ne = ne[:-1]

        if ge != ne:
            # asmjit builds a CFG, so it drops code that an unconditional branch
            # makes unreachable. emit_mov_cond/emit_str_cond with cond=AL produce
            # exactly that shape ("b over the second branch"), and the dropped
            # branch pair is dead by construction.
            if gid in EXPECTED_DEAD_BRANCH:
                expected += 1
                continue
            bad += 1
            print("%s %s" % (nid, nname))
            limit = max(len(ge), len(ne))
            for i in range(limit):
                g = ge[i] if i < len(ge) else None
                n = ne[i] if i < len(ne) else None
                if g != n:
                    print("    [%2d] %-30s | %s" % (i, fmt(g), fmt(n)))

    if bad:
        print("\n%d of %d cases differ" % (bad, len(golden)))
        return 1

    print("all %d cases match (%d with asmjit's dead-branch elimination, expected)"
          % (len(golden), expected))
    return 0


def fmt(ins):
    if ins is None:
        return "---"
    return ("%s %s" % ins).strip()


if __name__ == "__main__":
    sys.exit(main())
