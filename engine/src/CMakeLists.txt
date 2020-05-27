add_subdirectory(machine)

set(SOURCES
	script.cpp
	script_functions.cpp
	timers.cpp
)

add_library(libengine STATIC ${SOURCES})
target_link_libraries(libengine machine riscv)
