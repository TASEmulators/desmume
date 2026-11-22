#!/bin/sh
# Build the AArch64 encoder dump natively and compare it with the frozen
# hand-encoder output. Run from ~/a64 (the extracted tarball root).
set -e
cd "$(dirname "$0")"

CXX=${CXX:-clang++}
FLAGS="-std=c++20 -O1 -w -I desmume/3rdparty -DASMJIT_STATIC -DASMJIT_NO_X86"

mkdir -p obj
i=0
for f in desmume/3rdparty/asmjit/core/*.cpp \
         desmume/3rdparty/asmjit/axl/*.cpp \
         desmume/3rdparty/asmjit/arm/*.cpp; do
	o="obj/$(basename "$f" .cpp).o"
	( $CXX $FLAGS -c "$f" -o "$o" ) &
	i=$((i + 1))
	if [ $((i % 8)) -eq 0 ]; then wait; fi
done
wait

echo "asmjit objects: $(ls obj/*.o | wc -l)"

# The dump tool includes arm64_emit.cpp relative to tools/jit_matrix.
$CXX $FLAGS -c tools/jit_matrix/a64_dump_new.cpp -o obj/a64_dump_new.o
$CXX $FLAGS -c tools/jit_matrix/a64_shape_probe.cpp -o obj/a64_shape_probe.o
$CXX $FLAGS -c tools/jit_matrix/a64_exec_probe.cpp -o obj/a64_exec_probe.o
$CXX $FLAGS -c tools/jit_matrix/a64_memcall_probe.cpp -o obj/a64_memcall_probe.o
PROBES='a64_dump_new.o|a64_shape_probe.o|a64_exec_probe.o|a64_memcall_probe.o|a64_block_probe.o'
LIBS=$(ls obj/*.o | grep -v -E "$PROBES")
$CXX $LIBS obj/a64_dump_new.o -o a64_dump_new
$CXX $LIBS obj/a64_shape_probe.o -o a64_shape_probe
$CXX $LIBS obj/a64_exec_probe.o -o a64_exec_probe
$CXX $LIBS obj/a64_memcall_probe.o -o a64_memcall_probe

# a64_block_probe includes arm_jit_arm.cpp itself, so it needs the emulator
# headers (sync desmume/src/*.h too) and must be compiled and linked in one go:
# the stub core it defines has to live in the same translation unit.
# -DHAVE_JIT is not optional here: armcpu_t and the interpreter tables are only
# declared with it, and without it the probe fails at `armcpu_t NDS_ARM9;`.
$CXX $FLAGS -DHAVE_JIT -I desmume/src -I desmume/src/utils -I desmume/src/libretro-common/include \
	tools/jit_matrix/a64_block_probe.cpp $LIBS -o a64_block_probe

./a64_dump_new > a64_new_arm64.txt
echo "dump lines: $(wc -l < a64_new_arm64.txt)"
./a64_shape_probe | tail -2
echo
./a64_exec_probe
echo
./a64_memcall_probe
echo
./a64_block_probe
