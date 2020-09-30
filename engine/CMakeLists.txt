cmake_minimum_required (VERSION 3.0.2)
project (engine CXX)

option(ENABLE_FLTO   "Enable LTO builds" ON)
option(DEBUGGING     "Enable debugging with GDB" OFF)
option(SANITIZE      "Enable sanitizers" OFF)
option(GPROF         "Enable profiling with Gprof" OFF)
option(EMBEDDED_MODE "Embed programs in binary" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-Wall -Wextra -O2 -march=native")
set(BUILD_SHARED_LIBRARIES OFF)

if (ENABLE_FLTO)
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto=thin")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld")
	else()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto=8")
    endif()
endif()

if (DEBUGGING)
 	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb3 -O0")
endif()

if (SANITIZE)
 	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fsanitize=undefined")
endif()

if (GPROF)
 	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")
endif()

add_subdirectory(../ext ext)
add_subdirectory(script)

add_executable(engine
    src/main.cpp
    src/script_functions.cpp
    src/timers.cpp
)

target_include_directories(engine PUBLIC .)
target_link_libraries(engine script -static-libstdc++ -static-libgcc)
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
    )
    set_source_files_properties(${OUTFILE} PROPERTIES GENERATED TRUE)
    target_sources(${NAME} PRIVATE ${OUTFILE})

    add_custom_target(${NAME}.${FILENAME} DEPENDS ${OUTFILE})
    add_dependencies(${NAME} ${NAME}.${FILENAME})
endfunction()

if (EMBEDDED_MODE)
    embed_file(engine "${CMAKE_SOURCE_DIR}/mods/hello_world/scripts" "gameplay.elf")
    embed_file(engine "${CMAKE_SOURCE_DIR}/mods/hello_world/scripts/src" "gameplay.symbols")
    target_compile_definitions(engine PUBLIC EMBEDDED_MODE=1)
endif()
