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
LIBS=$(ls obj/*.o | grep -v -e 'a64_dump_new.o' -e 'a64_shape_probe.o' -e 'a64_exec_probe.o')
$CXX $LIBS obj/a64_dump_new.o -o a64_dump_new
$CXX $LIBS obj/a64_shape_probe.o -o a64_shape_probe
$CXX $LIBS obj/a64_exec_probe.o -o a64_exec_probe

./a64_dump_new > a64_new_arm64.txt
echo "dump lines: $(wc -l < a64_new_arm64.txt)"
./a64_shape_probe | tail -2
echo
./a64_exec_probe
