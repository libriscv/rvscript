#!/usr/bin/env bash
set -e
# We need a recent g++ version for <source_location>
sudo apt install g++-12-riscv64-linux-gnu
sudo apt install ccache cmake ninja-build libglfw3-dev libglew-dev
git submodule update --init --depth 2
git submodule update --init --recursive ext/nanogui
git submodule update --init --recursive ext/library

pushd engine
./build.sh
popd
