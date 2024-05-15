set(ARCH 64 CACHE STRING "RISC-V architecture")

set(SOURCES
	script.cpp
	script_bench.cpp
	script_debug.cpp
	script_fork.cpp
	script_remote.cpp
	script_syscalls.cpp
)

add_library(script STATIC ${SOURCES})
target_link_libraries(script PUBLIC riscv strf-header-only)
target_include_directories(script INTERFACE ..)
target_compile_definitions(script PUBLIC
	RISCV_ARCH=${ARCH}
)
