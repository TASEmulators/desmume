#!/bin/bash
# Build the JIT differential harness on a macOS host.
#
# This is the counterpart of build_core.ps1 + build_diff.ps1 for Apple
# Silicon. It exists because the arm64 backend has to be measured on a machine
# whose native instruction set is the one it emits -- an x86 host can compare
# the *words* the AArch64 backend produces (that is what check_a64.py does) but
# it cannot execute them, so it cannot tell a wrong instruction from a wrong
# handler.
#
# Instead of recompiling the core with guessed flags, this reuses the object
# files the Xcode build already produced: they are the exact translation units,
# with the exact preprocessor definitions, that ship in the app. The objects are
# packed into a static archive so the linker pulls only what the harness
# reaches; the Cocoa front end happens to supply the handful of globals the core
# references (execute / paused / driver ...), which is why there is no stub file
# here, unlike the Windows harness.
#
# Usage:
#   tools/jit_matrix/build_diff_macos.sh              # build
#   tools/jit_matrix/build_diff_macos.sh --clean      # drop the object cache first
#   DERIVED=.build/mac-arm64-derived tools/jit_matrix/build_diff_macos.sh
#
# Prerequisite (once, or after touching core sources):
#   xcodebuild -project "desmume/src/frontend/cocoa/DeSmuME (Latest).xcodeproj" \
#     -scheme "DeSmuME (macOS App; AppleSilicon dev+ -- Latest Xcode)" \
#     -configuration Release -derivedDataPath .build/mac-arm64-derived \
#     CODE_SIGNING_ALLOWED=NO MACOSX_DEPLOYMENT_TARGET=12.0 build
#
# Output:
#   .build/diff-mac/jit_diff

set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SRC="$REPO/desmume/src"
OUT="$REPO/.build/diff-mac"
DERIVED="${DERIVED:-$REPO/.build/mac-arm64-derived}"

if [ "${1:-}" = "--clean" ] && [ -d "$OUT" ]; then
    rm -rf "$OUT"
fi
mkdir -p "$OUT"

# --- find the object directory ---------------------------------------------
# The derived data usually holds objects for several AppleSilicon targets (the
# plain one and the dev+ one). They are not interchangeable: the plain target
# can contain translation units whose stack-probe attributes the harness linker
# rejects, and picking whichever directory `find` reaches first made the harness
# silently switch engines after an unrelated xcodebuild. Use the target named in
# the prerequisite above unless the caller overrides it.
TARGET_NAME="${TARGET_NAME:-DeSmuME (macOS App; AppleSilicon dev+ -- Latest Xcode)}"
OBJDIR="$DERIVED/Build/Intermediates.noindex/DeSmuME (Latest).build/Release/$TARGET_NAME.build/Objects-normal/arm64"
if [ ! -d "$OBJDIR" ]; then
    echo "no object directory for target '$TARGET_NAME' under $DERIVED" >&2
    echo "build that AppleSilicon target first (see the header of this script)" >&2
    exit 1
fi
echo "objects : $OBJDIR"

# --- pack the core into an archive -----------------------------------------
# main.o is excluded: the harness provides main(), and pulling the app's would
# be a duplicate symbol. Everything else stays in the archive and is only
# linked if the harness actually needs it.
find "$OBJDIR" -name '*.o' ! -name 'main.o' > "$OUT/filelist.txt"
if [ ! -s "$OUT/filelist.txt" ]; then
    echo "object directory is empty" >&2
    exit 1
fi

CORE_A="$OUT/core.a"
if [ ! -f "$CORE_A" ] || [ -n "$(find "$OBJDIR" -name '*.o' -newer "$CORE_A" 2>/dev/null | head -1)" ]; then
    rm -f "$CORE_A"
    # libtool warns about the handful of translation units that are compiled to
    # nothing on this platform (sse_optimized, disc...); that is expected.
    libtool -static -o "$CORE_A" -filelist "$OUT/filelist.txt" 2>&1 | grep -v 'has no symbols' || true
    echo "archive : $CORE_A"
else
    echo "archive : $CORE_A (up to date)"
fi

# --- compile the harness ---------------------------------------------------
# Same macros the Cocoa prefix header defines, because types.h derives HOST_64
# from DESMUME_COCOA -- and HOST_64 changes arm_jit.h's structures. Getting this
# wrong links cleanly and then compares the wrong fields.
DEFINES=(
    -DHOST_DARWIN
    -DDESMUME_COCOA
    -DHAVE_JIT
    -DNDEBUG
    -DASMJIT_STATIC
)
INCLUDES=(
    -I"$SRC"
    -I"$SRC/libretro-common/include"
    -I"$REPO/desmume/3rdparty"
)

HARNESS_O="$OUT/jit_diff_main.o"
HARNESS_SRC="$SRC/utils/jit_diff/jit_diff_main.cpp"
if [ ! -f "$HARNESS_O" ] || [ "$HARNESS_SRC" -nt "$HARNESS_O" ]; then
    clang++ -std=c++17 -O2 -c "${DEFINES[@]}" "${INCLUDES[@]}" "$HARNESS_SRC" -o "$HARNESS_O"
    echo "compiled: $HARNESS_O"
fi

# --- link ------------------------------------------------------------------
# zlib and pcap are what the core references from outside itself (ROM loading,
# the wifi emulation); the frameworks are what the Cocoa front end pulls in once
# its objects are linked.
EXE="$OUT/jit_diff"
clang++ -o "$EXE" \
    "$HARNESS_O" "$CORE_A" \
    -lz -lpcap \
    -framework Foundation \
    -framework AppKit \
    -framework Metal \
    -framework MetalKit \
    -framework AudioToolbox \
    -framework AVFoundation \
    -framework CoreAudio \
    -framework OpenGL \
    -framework QuartzCore \
    -framework CoreVideo \
    -framework CoreGraphics \
    -framework IOKit \
    -framework Carbon \
    -framework CoreServices \
    -framework Security \
    -framework UniformTypeIdentifiers \
    -framework CoreMedia

echo "built   : $EXE"
