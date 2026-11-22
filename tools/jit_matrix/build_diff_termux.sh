#!/bin/bash
# Build the JIT differential harness natively on an aarch64 POSIX host
# (Termux / Android, or a normal Linux arm64 box).
#
# This is the Linux/Termux counterpart of build_diff_macos.sh, and it exists for
# the same reason: the arm64 backend emits instructions that only an arm64 host
# can execute, so "the backend produces the right words" (check_a64.py) is not
# the same claim as "the backend computes the right answer" (this harness).
#
# Unlike the macOS script it does not reuse Xcode objects -- there are none. It
# reuses the *meson* build tree the front end already produced, which keeps the
# harness on exactly the same translation units and preprocessor definitions as
# the shipped binary. Run `meson setup build ... && ninja -C build` first.
#
# Usage:
#   tools/jit_matrix/build_diff_termux.sh                # build + link
#   BUILD=path/to/meson-build tools/jit_matrix/build_diff_termux.sh
#
# Output: .build/diff-a64/jit_diff

set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SRC="$REPO/desmume/src"
BUILD="${BUILD:-$SRC/frontend/posix/build}"
OUT="$REPO/.build/diff-a64"

CXX="${CXX:-clang++}"

if [ ! -f "$BUILD/libdesmume.a" ]; then
    echo "missing $BUILD/libdesmume.a - configure and build the posix front end first" >&2
    exit 1
fi

mkdir -p "$OUT"

# Same macros the meson build uses for the core. HOST_64 and HAVE_JIT change the
# layouts in types.h / arm_jit.h; getting them wrong links cleanly and then
# compares the wrong fields.
DEFINES=(
    -DHOST_64
    -DHAVE_JIT
    -DNDEBUG
    -DASMJIT_STATIC
)
INCLUDES=(
    -I"$SRC"
    -I"$SRC/libretro-common/include"
    -I"$SRC/frontend"
    -I"$REPO/desmume/3rdparty"
)
# asmjit core, the arm64 backend and the harness itself are C++20; the rest of
# the archive was compiled with the project default and is already an archive.
FLAGS=(-std=c++20 -O2 "${DEFINES[@]}" "${INCLUDES[@]}")

echo "archive : $BUILD/libdesmume.a"
$CXX "${FLAGS[@]}" -c "$SRC/utils/jit_diff/jit_diff_main.cpp"     -o "$OUT/jit_diff_main.o"
$CXX "${FLAGS[@]}" -c "$REPO/tools/jit_matrix/jit_diff_stubs_posix.cpp" -o "$OUT/jit_diff_stubs_posix.o"

EXE="$OUT/jit_diff"
# --start-group: libdesmume.a references back into libdesmume_jit.a (the JIT is
# link_whole'd into the front-end binary, not into the core archive).
$CXX -o "$EXE" "$OUT/jit_diff_main.o" "$OUT/jit_diff_stubs_posix.o" \
    -Wl,--start-group \
        "$BUILD/libdesmume.a" "$BUILD/libdesmume_jit.a" "$BUILD/libasmjit.a" \
    -Wl,--end-group \
    $(pkg-config --libs glib-2.0 sdl3 zlib libpcap freetype2 fontconfig) \
    -lm -lpthread

echo "built   : $EXE"
