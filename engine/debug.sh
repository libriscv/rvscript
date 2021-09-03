#!/usr/bin/env bash
set -e

mkdir -p build
pushd build
cmake .. -DRISCV_EXT_A=ON -DRISCV_EXT_C=OFF -DRISCV_DEBUG=ON -DFLTO=OFF -DCMAKE_BUILD_TYPE=Debug -DSANITIZE=ON
make -j4
popd

time ./engine
