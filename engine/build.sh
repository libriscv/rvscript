#!/usr/bin/env bash
set -e

mkdir -p build
pushd build
cmake .. -DRISCV_EXT_A=ON -DRISCV_EXT_C=OFF
make -j16
popd

export CFLAGS="-O2 -ffast-math"
time ./engine
