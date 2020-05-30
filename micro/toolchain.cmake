SET(CMAKE_SYSTEM_NAME Linux)
SET(CMAKE_CROSSCOMPILING 1)
set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")

message(STATUS ${CMAKE_SOURCE_DIR}/libc/override)

set(COMPILER_DIR $ENV{HOME}/riscv/riscv32-unknown-elf)
set(LIBRARY_DIR $ENV{HOME}/riscv/lib/gcc/riscv32-unknown-elf/9.2.0 CACHE STRING "GCC libraries")

include_directories(SYSTEM
	${CMAKE_SOURCE_DIR}/libc/override
	${COMPILER_DIR}/include/c++/9.2.0
	${COMPILER_DIR}/include/c++/9.2.0/riscv-none-embed
	${COMPILER_DIR}/include
	${LIBRARY_DIR}/include-fixed
	${LIBRARY_DIR}/include
)
