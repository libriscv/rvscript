#!/usr/bin/env bash
set -e

mkdir -p build
pushd build
cmake .. -DRISCV_EXT_A=ON -DRISCV_EXT_C=OFF -DRISCV_DEBUG=OFF -DCMAKE_BUILD_TYPE=Release -DENABLE_FLTO=ON
make -j16
popd

export CFLAGS="-O2 -ffast-math"
time ./engine
