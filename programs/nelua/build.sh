#!/bin/bash
set -e
GCC_TRIPLE="riscv64-unknown-elf"
export CC=$GCC_TRIPLE-gcc
export CXX=$GCC_TRIPLE-g++
MAINFILE="$PWD/${1:-src/main.nelua}"

mkdir -p $GCC_TRIPLE
pushd $GCC_TRIPLE

if [[ -z "${DEBUG}" ]]; then
	DMODE="--release"
	CDEBUG="-DGCSECTIONS=ON -DDEBUGGING=OFF -DCMAKE_BUILD_TYPE=Release"
else
	DMODE="--debug"
	CDEBUG="-DGCSECTIONS=OFF -DDEBUGGING=ON -DCMAKE_BUILD_TYPE=Debug"
fi

BASENAME=`basename $MAINFILE`
nelua --print-code $DMODE $MAINFILE > $BASENAME.c

cmake .. -G Ninja -DGCC_TRIPLE=$GCC_TRIPLE $CDEBUG -DCMAKE_TOOLCHAIN_FILE=../../micro/toolchain.cmake
ninja
popd
