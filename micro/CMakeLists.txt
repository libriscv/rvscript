cmake_minimum_required(VERSION 3.1.0)
project(modbuilder CXX)

option(LTO         "Link-time optimizations" ON)
option(GCSECTIONS  "Garbage collect empty sections" OFF)
option(DEBUGGING   "Add debugging information" OFF)
option(RTTI_EXCEPT "C++ RTTI and exceptions" OFF)
set(VERSION_FILE   "symbols.map" CACHE STRING "Retained symbols file")

#
# Mod settings
#
set (MODNAME "hello_world" CACHE STRING "Mod name")

#
# Build configuration
#
set (ENGINE_PATH "${CMAKE_SOURCE_DIR}/../engine")
set (MODPATH "${ENGINE_PATH}/mods/${MODNAME}")
set (APIPATH "${ENGINE_PATH}/api")
set (UTILPATH "${ENGINE_PATH}/src/util")

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	set(RISCV_ABI "-target riscv32 -march=rv32imfd")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld")
else()
	set(RISCV_ABI "-march=rv32imfd -mabi=ilp32d")
endif()

set(WARNINGS  "-Wall -Wextra")
set(COMMON    "-O2 -fstack-protector -nostdlib -fno-builtin -fno-math-errno")
if (DEBUGGING)
	set (COMMON "-g ${COMMON}")
endif()
set(CMAKE_C_FLAGS "${WARNINGS} ${RISCV_ABI} -std=c11 ${COMMON}")
set(CMAKE_CXX_FLAGS "${WARNINGS} ${RISCV_ABI} -std=c++17 ${COMMON}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Ttext 0x120000")

if (NOT RTTI_EXCEPT)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -fno-exceptions")
endif()

if (LTO)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -flto -ffat-lto-objects")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto -ffat-lto-objects")
endif()

if (GCSECTIONS)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffunction-sections -fdata-sections")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffunction-sections -fdata-sections")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-gc-sections")
endif()

set(BUILD_SHARED_LIBS OFF)
set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "") # remove -rdynamic
set(LIBGCC ${LIBRARY_DIR}/libgcc.a)
set(SUPCPP ${CRTLIB_DIR}/libsupc++.a)

add_subdirectory(ext)
add_subdirectory(libc)
target_include_directories(libc PUBLIC ${APIPATH})
target_include_directories(libc PUBLIC ${UTILPATH})

function (add_verfile NAME VERFILE)
	set_target_properties(${NAME} PROPERTIES LINK_DEPENDS ${VERFILE})
	target_link_libraries(${NAME} "-Wl,--retain-symbols-file=${VERFILE}")
	if (GCSECTIONS)
		file(STRINGS "${VERFILE}" SYMBOLS)
		foreach(SYMBOL ${SYMBOLS})
			if (NOT ${SYMBOL} STREQUAL "")
				#message(STATUS "Symbol retained: ${SYMBOL}")
				target_link_libraries(${NAME} "-Wl,--undefined=${SYMBOL}")
			endif()
		endforeach()
	endif()
endfunction()

function (add_micro_binary NAME VERFILE)
	add_executable(${NAME} ${ARGN})
	target_include_directories(${NAME} PRIVATE ${CMAKE_SOURCE_DIR}/libc)
	target_link_libraries(${NAME} -static -Wl,--whole-archive libc -Wl,--no-whole-archive supc++ gcc)
	target_link_libraries(${NAME} frozen::frozen strf)
	# place ELF into the sub-projects source folder
	set_target_properties(${NAME}
		PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
	)
	# public API support
	if (GCSECTIONS)
		set(VERFILE "${CMAKE_CURRENT_SOURCE_DIR}/${VERFILE}")
		if (EXISTS "${VERFILE}")
			add_verfile(${NAME} ${VERFILE})
		else()
			add_verfile(${NAME} ${CMAKE_SOURCE_DIR}/symbols.map)
		endif()
	endif()
endfunction()

add_subdirectory (${MODPATH} mod)

add_custom_command(
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/api/*.h ${APIPATH}
	DEPENDS ${CMAKE_SOURCE_DIR}/api/api.h
			${CMAKE_SOURCE_DIR}/api/api_impl.h
			${CMAKE_SOURCE_DIR}/api/api_structs.h
			${CMAKE_SOURCE_DIR}/api/shared_memory.h
			${CMAKE_SOURCE_DIR}/api/syscalls.h
	OUTPUT  ${APIPATH}/api.h
			${APIPATH}/api_impl.h
			${APIPATH}/api_structs.h
			${APIPATH}/shared_memory.h
			${APIPATH}/syscalls.h
)
add_custom_target(install_headers
	DEPENDS ${APIPATH}/api.h
			${APIPATH}/api_impl.h
			${APIPATH}/api_structs.h
			${APIPATH}/shared_memory.h
			${APIPATH}/syscalls.h
)
add_dependencies(libc install_headers)
