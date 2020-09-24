set(ARCH 64 CACHE STRING "RISC-V architecture")

add_subdirectory(machine)

set(SOURCES
	script.cpp
)

add_library(script STATIC ${SOURCES})
target_link_libraries(script PUBLIC machine riscv)
target_compile_definitions(script PUBLIC RISCV_ARCH=${ARCH})
