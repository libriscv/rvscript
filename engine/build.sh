#!/usr/bin/env bash
set -e
export CXX="ccache $CXX"
export CC="ccache $CC"

# Build the game
mkdir -p build
pushd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DFLTO=ON -DSANITIZE=OFF
make -j4
popd

# Build the script
pushd ../programs
source build.sh
popd

# gsettings set org.gnome.mutter center-new-windows true 

# Run the game
./engine
