set(ARCH 32 CACHE STRING "RISC-V architecture")

add_subdirectory(machine)

set(SOURCES
	script.cpp
	script_functions.cpp
	timers.cpp
)

add_library(libengine STATIC ${SOURCES})
target_link_libraries(libengine machine riscv)
target_compile_definitions(libengine PUBLIC RISCV_ARCH=${ARCH})
