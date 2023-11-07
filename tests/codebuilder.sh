#!/usr/bin/env bash
set -e

TESTS=$1
FILE=$2
FINALFILE=$3

PROGRAMS="$TESTS/../programs"
TOOLCHAIN="$PROGRAMS/micro/toolchain.cmake"

CODETMPDIR="$(mktemp -d)"
trap 'rm -rf -- "$CODETMPDIR"' EXIT

pushd $CODETMPDIR
ln -s $PROGRAMS/dynamic_calls.json .
ln -s $FILE program.cpp

cat <<\EOT >CMakeLists.txt
cmake_minimum_required(VERSION 3.12.0)
project(modbuilder CXX)

add_subdirectory(${PROGRAMS}/dyncalls dyncalls)
include(${PROGRAMS}/micro/micro.cmake)

add_level(program 0x400000
	program.cpp
)
EOT

source $PROGRAMS/detect_compiler.sh

cmake -G Ninja . -DCMAKE_TOOLCHAIN_FILE=$TOOLCHAIN -DPROGRAMS=$PROGRAMS -DLTO=OFF -DGCSECTIONS=OFF
ninja
popd

mv $CODETMPDIR/program $FINALFILE
echo "Moved $CODETMPDIR/program to $FINALFILE"
