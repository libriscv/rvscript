#!/bin/bash
set -e
GCC_TRIPLE="riscv64-unknown-elf"
export CXX="ccache $GCC_TRIPLE-g++"
CDEBUG="-DGCSECTIONS=ON -DLTO=OFF -DCMAKE_BUILD_TYPE=Release"

for i in "$@"; do
	case $i in
#    -e=*|--extension=*)
#      EXTENSION="${i#*=}"
#      shift # past argument=value
#      ;;
		--debug)
			CDEBUG="-DGCSECTIONS=OFF -DLTO=OFF -DCMAKE_BUILD_TYPE=Debug"
			shift # past argument with no value
			;;
		--glibc)
			echo "Building game scripts with GCC/glibc"
			GCC_VERSION=12
			GCC_TRIPLE="riscv64-linux-gnu"
			export CXX="ccache $GCC_TRIPLE-g++-$GCC_VERSION"
			shift # past argument with no value
			;;
		-*|--*)
			echo "Unknown option $i"
			exit 1
			;;
		*)
		;;
	esac
done

mkdir -p $GCC_TRIPLE
pushd $GCC_TRIPLE
cmake .. -DGCC_TRIPLE=$GCC_TRIPLE $CDEBUG -DCMAKE_TOOLCHAIN_FILE=../micro/toolchain.cmake
make -j4
popd
