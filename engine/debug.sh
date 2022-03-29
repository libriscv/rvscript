#!/usr/bin/env bash
set -e

mkdir -p build
pushd build
cmake .. -DFLTO=OFF -DCMAKE_BUILD_TYPE=Debug -DSANITIZE=ON
make -j4
popd

gdb ./engine
