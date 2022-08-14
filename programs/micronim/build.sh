#!/bin/bash
set -e
ARCH=64
GCC_TRIPLE="riscv$ARCH-unknown-elf"
export CC=$GCC_TRIPLE-gcc
export CXX=$GCC_TRIPLE-g++
NIMCPU="--cpu=riscv$ARCH"
NIMLIST=$PWD/files.txt
TOOLCHAIN="$PWD/../micro/toolchain.cmake"
FILEDIR=$PWD

# find nim and replace /bin/nim with /lib to detect the Nim lib folder
NIM_LIBS=`whereis nim`
NIM_LIBS="${NIM_LIBS##*: }"
NIM_LIBS="${NIM_LIBS/bin*/lib}"

if [[ -z "${DEBUG}" ]]; then
	DMODE="-d:release"
	CDEBUG="-DGCSECTIONS=ON -DCMAKE_BUILD_TYPE=Release"
else
	DMODE="--debugger:native"
	CDEBUG="-DGCSECTIONS=OFF -DCMAKE_BUILD_TYPE=Debug"
fi

mkdir -p $GCC_TRIPLE
pushd $GCC_TRIPLE

CMAKE_LIST=$PWD/files.cmake
: > $CMAKE_LIST

nimbuild() {
  nim c --nimcache:$NIMCACHE $NIMCPU --colors:on --os:linux --gc:arc -d:useMalloc --noMain --app:lib $DMODE -c $FILEDIR/$file
  jq '.compile[] [0]' $NIMCACHE/*.json > $NIMCACHE/buildfiles.txt
  #rm -f $NIMCACHE/*.json
}

# run nim on each file asynchronously
while read file; do
  FILEBASE=`basename --suffix=.nim $file`

  # temporary folder for some build output
  NIMCACHE=$PWD/nimcache_$FILEBASE
  mkdir -p $NIMCACHE

  nimbuild &
done < $NIMLIST

# wait for nim programs to end
wait

# create CMake build scripts
while read file; do
  FILEBASE=`basename --suffix=.nim $file`
  echo "add_micronim_binary($FILEBASE 0x400000" >> "$CMAKE_LIST"
  echo " src/default.symbols" >> "$CMAKE_LIST"
  cat "$NIMCACHE/buildfiles.txt" >> "$CMAKE_LIST"
  echo ")" >> "$CMAKE_LIST"
  echo "target_include_directories($FILEBASE PRIVATE ${NIM_LIBS})" >> "$CMAKE_LIST"
done < $NIMLIST

cmake .. -G Ninja -DGCC_TRIPLE=$GCC_TRIPLE -DNIM_LIBS=$NIM_LIBS $CDEBUG -DCMAKE_TOOLCHAIN_FILE=$TOOLCHAIN
ninja
popd
