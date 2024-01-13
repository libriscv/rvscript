cmake_minimum_required (VERSION 3.0.2)
cmake_policy(SET CMP0069 NEW)
project (engine C CXX)

option(FLTO          "Enable LTO builds" OFF)
option(SANITIZE      "Enable sanitizers" OFF)
option(GPROF         "Enable profiling with Gprof" OFF)
option(EMBEDDED_MODE "Embed programs in binary" OFF)
option(USE_MOLD      "Use Mold linker" OFF)

set(COMMON "-Wall -Wextra -march=native -ggdb3")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMMON}")

if (SANITIZE)
 	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fsanitize=undefined")
endif()

if (GPROF)
 	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")
endif()

if (FLTO)
	include(CheckIPOSupported)
	check_ipo_supported(RESULT supported OUTPUT error)
	if (supported)
		message(STATUS "IPO / LTO enabled")
		set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
	else()
		message(STATUS "IPO / LTO not supported: <${error}>")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto")
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld")
	endif()
endif()
if (USE_MOLD)
	if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --ld-path=/usr/local/bin/mold")
	else()
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=/usr/local/bin/mold")
	endif()
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld")
endif()

add_subdirectory(../ext ext)
add_subdirectory(src/script script)

add_executable(engine
	src/main.cpp
	src/main_screen.cpp
	src/renderer.cpp
	src/test_dynamic.cpp
	src/timers.cpp
	src/setup_gui.cpp
	src/setup_timers.cpp
)

target_include_directories(engine PUBLIC .)
target_link_libraries(engine script nanogui library)
set_target_properties(engine PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

## embedding programs into executable ##

function (embed_file NAME PATH BINARY)
	get_filename_component(FILENAME ${BINARY} NAME)
	set(OUTFILE ${CMAKE_CURRENT_BINARY_DIR}/${FILENAME}.o)
	add_custom_command(
		WORKING_DIRECTORY ${PATH}
		COMMAND ${CMAKE_OBJCOPY} -Ibinary -Oelf64-x86-64 -Bi386 ${BINARY} ${OUTFILE}
		OUTPUT  ${OUTFILE}
		DEPENDS ${PATH}/${BINARY}
	)
	set_source_files_properties(${OUTFILE} PROPERTIES GENERATED TRUE)
	target_sources(${NAME} PRIVATE ${OUTFILE})
endfunction()

if (EMBEDDED_MODE)
	embed_file(engine "${CMAKE_SOURCE_DIR}/mods/hello_world/scripts" "gameplay.elf")
	embed_file(engine "${CMAKE_SOURCE_DIR}/../programs" "symbols.map")
	target_compile_definitions(engine PUBLIC EMBEDDED_MODE=1)
endif()

if (SANITIZE)
	if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
		target_link_libraries(engine --rtlib=compiler-rt -lgcc_s)
	endif()
endif()
