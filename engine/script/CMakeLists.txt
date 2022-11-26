set(ARCH 64 CACHE STRING "RISC-V architecture")

set(SOURCES
	script.cpp
	script_bench.cpp
	script_remote.cpp
)

add_library(script STATIC ${SOURCES})
target_link_libraries(script PUBLIC fmt riscv)
target_compile_definitions(script PUBLIC
	RISCV_ARCH=${ARCH}
)
