#!/usr/bin/env bash
set -e
sudo apt install ccache cmake ninja-build libglfw3-dev libglew-dev g++-12-riscv64-linux-gnu
git submodule update --init --depth 2
git submodule update --init --recursive ext/nanogui

pushd engine
./build.sh
popd
