cmake_minimum_required(VERSION 3.11.0)
project(programs C)

# Auto-detected if Nim is in PATH
set(NIM_LIBS "$ENV{HOME}/nim-1.4.8/lib" CACHE STRING "Nim library folder")

include(micronim.cmake)

file(READ "${CMAKE_BINARY_DIR}/buildfiles.txt" BUILD_LIST)
string(REPLACE "\"" "" BUILD_LIST ${BUILD_LIST})
string(REPLACE "\n" ";" BUILD_LIST ${BUILD_LIST})

add_micronim_binary(hello_nim
	src/default.symbols
	${BUILD_LIST}
)
target_include_directories(hello_nim PRIVATE ${NIM_LIBS})
