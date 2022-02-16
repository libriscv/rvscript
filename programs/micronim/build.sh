#!/bin/bash
set -e
GCC_TRIPLE="riscv64-unknown-elf"
export CC=$GCC_TRIPLE-gcc
export CXX=$GCC_TRIPLE-g++
NIMCPU="--cpu=riscv64"
NIMFILE="$PWD/${1:-src/hello.nim}"

mkdir -p $GCC_TRIPLE
pushd $GCC_TRIPLE

# find nim and replace /bin/nim with /lib to detect the Nim lib folder
NIM_LIBS=`whereis nim`
NIM_LIBS="${NIM_LIBS##*: }"
NIM_LIBS="${NIM_LIBS/bin*/lib}"

# temporary folder for some build output
NIMCACHE=$PWD/nimcache
mkdir -p $NIMCACHE

if [[ -z "${DEBUG}" ]]; then
	DMODE="-d:release"
	CDEBUG="-DGCSECTIONS=ON -DDEBUGGING=OFF -DCMAKE_BUILD_TYPE=Release"
else
	DMODE="--debugger:native"
	CDEBUG="-DGCSECTIONS=OFF -DDEBUGGING=ON -DCMAKE_BUILD_TYPE=Debug"
fi

nim c --nimcache:$NIMCACHE $NIMCPU --colors:on --os:linux --gc:arc -d:useMalloc --noMain --app:lib $DMODE -c ${NIMFILE}
jq '.compile[] [0]' $NIMCACHE/*.json > buildfiles.txt

cmake .. -G Ninja -DGCC_TRIPLE=$GCC_TRIPLE -DNIM_LIBS=$NIM_LIBS $CDEBUG -DCMAKE_TOOLCHAIN_FILE=../../micro/toolchain.cmake
ninja
popd
