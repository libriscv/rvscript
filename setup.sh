#!/usr/bin/env bash
set -e
# we have to do it this way because of EASTLs idiotic repo setup
git submodule update --init
pushd ext/libriscv
git submodule update --init
pushd lib/EASTL
git submodule update --init
popd
popd
