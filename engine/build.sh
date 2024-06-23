#!/usr/bin/env bash
set -e
export ENGINE_CXX="ccache $CXX"
export ENGINE_CC="ccache $CC"

# Build the script
pushd ../programs
source build.sh $@
popd

echo "RISC-V C-extension is: $CEXT"

CXX=$ENGINE_CXX
CC=$ENGINE_CC

# Build the game
mkdir -p build
pushd build
if [ ! -f CMakeCache.txt ]; then
	cmake .. -DCMAKE_BUILD_TYPE=Release -DSANITIZE=OFF -DRISCV_EXT_C=$CEXT
fi
make -j4
popd

# gsettings set org.gnome.mutter center-new-windows true 

# Run the game
./engine
