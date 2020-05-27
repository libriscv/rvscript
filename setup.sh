#!/usr/bin/env bash
pushd ext/libriscv
git submodule update --init
pushd lib/EASTL
git submodule update --init
popd
popd
