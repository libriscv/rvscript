#!/bin/bash
set -e
GCC_TRIPLE="riscv64-unknown-elf"
export CC=$GCC_TRIPLE-gcc
export CXX=$GCC_TRIPLE-g++

mkdir -p build
pushd build
cmake .. -DGCC_TRIPLE=$GCC_TRIPLE -DCMAKE_TOOLCHAIN_FILE=../micro/toolchain.cmake
make -j4
popd
