#!/usr/bin/env bash
set -e
sudo apt install ccache cmake ninja-build libglew-dev
git submodule update --init --recursive
