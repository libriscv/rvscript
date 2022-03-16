#!/usr/bin/env bash
set -e

mkdir -p build
pushd build
cmake .. -DRISCV_DEBUG=OFF -DFLTO=ON -DCMAKE_BUILD_TYPE=Release -DSANITIZE=OFF
make -j4
popd

time ./engine
