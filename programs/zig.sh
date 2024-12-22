TOOLCHAIN=$PWD/micro/zig-toolchain.cmake

mkdir -p .zig
pushd .zig
cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=$TOOLCHAIN
make -j8
popd
