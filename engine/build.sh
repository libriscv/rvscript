#!/usr/bin/env bash
set -e
export CXX="ccache $CXX"
export CC="ccache $CC"

mkdir -p build
pushd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DSANITIZE=OFF
time make -j4
popd

# gsettings set org.gnome.mutter center-new-windows true 

./hubris
