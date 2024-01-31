#!/bin/bash
source=$1
pushd $source/desmume/src/frontend/posix
make -j12
#meson build -C debian-linux-build -j 12 --verbose
popd
