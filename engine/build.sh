#!/usr/bin/env bash
set -e

mkdir -p build
pushd build
cmake .. -DRISCV_EXT_A=ON -DRISCV_EXT_C=OFF -DRISCV_DEBUG=OFF -DFLTO=ON -DCMAKE_BUILD_TYPE=Release -DSANITIZE=OFF
make -j4
popd

export CFLAGS="-O2 -ffast-math"
time ./engine
