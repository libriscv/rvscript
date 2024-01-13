cmake_minimum_required(VERSION 3.10)
project(rvscript_unittests C CXX)

set(CMAKE_CXX_FLAGS "-Wall -Wextra -Og -ggdb3 -fsanitize=address,undefined")

option(RISCV_MEMORY_TRAPS "" OFF)
option(RISCV_EXT_C "" ON)
add_subdirectory(../ext ext)
add_subdirectory(../engine/src/script script)
target_compile_definitions(script PUBLIC TESTING_FRAMEWORK=1)

add_subdirectory(Catch2)

enable_testing()

configure_file(${CMAKE_SOURCE_DIR}/codebuilder.sh
	${CMAKE_CURRENT_BINARY_DIR}/codebuilder.sh COPYONLY)

function(add_unit_test NAME)
	add_executable(${NAME}
		${ARGN}
		codebuilder.cpp
	)
	target_compile_definitions(${NAME} PUBLIC SRCDIR="${CMAKE_CURRENT_SOURCE_DIR}")
	target_link_libraries(${NAME} script Catch2WithMain)
	add_test(
		NAME test_${NAME}
		COMMAND ${NAME}
	)
endfunction()

add_unit_test(basic    basic.cpp)
add_unit_test(evloop   event_loop.cpp)
