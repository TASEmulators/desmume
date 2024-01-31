#!/bin/bash
source=$1
pushd $source/desmume/src/frontend/posix
make -j12
popd
