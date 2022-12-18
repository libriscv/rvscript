cmake_minimum_required(VERSION 3.10)
project(libriscv_unittests C CXX)

set(CMAKE_CXX_FLAGS "-Wall -Wextra -Og -ggdb3 -fsanitize=address,undefined")

option(RISCV_MULTIPROCESS "" OFF)
option(RISCV_MEMORY_TRAPS "" OFF)
add_subdirectory(../ext ext)
add_subdirectory(../engine/src/script script)

add_subdirectory(Catch2)

enable_testing()

function(add_unit_test NAME)
	add_executable(${NAME}
		${ARGN}
		codebuilder.cpp
	)
	target_compile_definitions(${NAME} PUBLIC SRCDIR="${CMAKE_CURRENT_SOURCE_DIR}")
	target_link_libraries(${NAME} script -static-libsan Catch2WithMain)
	add_test(
		NAME test_${NAME}
		COMMAND ${NAME}
	)
endfunction()

add_unit_test(basic    basic.cpp)
