#!/bin/bash
set -e
GCC_TRIPLE="riscv64-unknown-elf"
export CC=$GCC_TRIPLE-gcc
export CXX=$GCC_TRIPLE-g++
NIMCPU="--cpu=riscv64"
NIMFILE="$PWD/${1:-src/hello.nim}"

mkdir -p $GCC_TRIPLE
pushd $GCC_TRIPLE

NIMCACHE=$PWD/nimcache
mkdir -p $NIMCACHE

nim c --nimcache:$NIMCACHE $NIMCPU --colors:on --os:any --gc:arc -d:useMalloc --noMain --app:lib -d:release -c ${NIMFILE}
jq '.compile[] [0]' $NIMCACHE/*.json > buildfiles.txt

cmake .. -G Ninja -DGCC_TRIPLE=$GCC_TRIPLE -DCMAKE_TOOLCHAIN_FILE=../../micro/toolchain.cmake
ninja
popd
