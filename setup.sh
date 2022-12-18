#!/usr/bin/env bash
set -e
sudo apt install ccache cmake ninja-build libsdl2-dev
git submodule update --init
