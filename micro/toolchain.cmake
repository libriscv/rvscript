SET(CMAKE_SYSTEM_NAME Linux)
SET(CMAKE_CROSSCOMPILING 1)
set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")

message(STATUS ${CMAKE_SOURCE_DIR}/libc/override)

set(GCC_VERSION 10.1.0)
set(COMPILER_DIR $ENV{HOME}/riscv/riscv32-unknown-elf)
set(LIBRARY_DIR $ENV{HOME}/riscv/lib/gcc/riscv32-unknown-elf/${GCC_VERSION} CACHE STRING "GCC libraries")

include_directories(SYSTEM
	${CMAKE_SOURCE_DIR}/libc/override
	${COMPILER_DIR}/include/c++/${GCC_VERSION}
	${COMPILER_DIR}/include/c++/${GCC_VERSION}/riscv32-unknown-elf
	${COMPILER_DIR}/include
	${LIBRARY_DIR}/include-fixed
	${LIBRARY_DIR}/include
)
