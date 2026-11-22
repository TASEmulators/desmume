#!/bin/bash
# Disassemble the block a `jit_diff --dump` run printed.
#   tools/jit_matrix/disasm_dump.sh .build/dump-ldrd.log
#
# The dump is the JIT's own host code: reading it is how you tell "the handler
# emitted the wrong thing" from "the block never ran", and it is the only way to
# name a fault address that landed inside emitted code.
set -eu
LOG="${1:?usage: disasm_dump.sh <dump.log>}"
DIR="$(dirname "$LOG")"
BIN="$DIR/block.bin"
TXT="$DIR/block.hex"

HEX="$(awk '/-> [0-9]+ bytes/ { f = 1; next } f && /^[0-9A-Fa-f]+$/ { printf "%s", $0 }' "$LOG")"
if command -v xxd >/dev/null 2>&1; then
    printf '%s' "$HEX" | xxd -r -p > "$BIN"
else
    # Termux has no xxd; python3 is always present.
    python3 -c 'import sys,binascii; open(sys.argv[2],"wb").write(binascii.unhexlify(sys.argv[1].strip()))' \
        "$HEX" "$BIN"
fi
echo "bytes: $(wc -c < "$BIN")  -> $BIN"

# binutils objdump is not installed on Termux (its `objdump` is llvm-objdump,
# which cannot read a raw binary), so fall back to llvm-mc over 4-byte words.
if objdump -D -b binary -m aarch64 "$BIN" >/dev/null 2>&1; then
    objdump -D -b binary -m aarch64 "$BIN" | sed -n '7,$p'
else
    python3 - "$BIN" "$TXT" <<'PY'
import sys
data = open(sys.argv[1], "rb").read()
with open(sys.argv[2], "w") as f:
    for i in range(0, len(data) - 3, 4):
        f.write(" ".join("0x%02x" % b for b in data[i:i + 4]) + "\n")
PY
    llvm-mc --disassemble --triple=aarch64 < "$TXT"
fi
