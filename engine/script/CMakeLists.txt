set(ARCH 64 CACHE STRING "RISC-V architecture")

add_subdirectory(machine)

set(SOURCES
	script.cpp
)
if (RISCV_DEBUG)
	list(APPEND SOURCES
		script_debug.cpp
	)
endif()

add_library(script STATIC ${SOURCES})
target_link_libraries(script PUBLIC machine fmt riscv)
target_compile_definitions(script PUBLIC RISCV_ARCH=${ARCH})
