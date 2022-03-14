option(LTO         "Link-time optimizations" ON)
option(GCSECTIONS  "Garbage collect empty sections" ON)
set(VERSION_FILE   "symbols.map" CACHE STRING "Retained symbols file")
option(STRIP_SYMBOLS "Remove all symbols except the public API" ON)

#
# Build configuration
#
set (ENGINE_PATH "${CMAKE_SOURCE_DIR}/../engine")
set (APIPATH "${ENGINE_PATH}/api")
set (UTILPATH "${ENGINE_PATH}/src/util")

if (GCC_TRIPLE STREQUAL "riscv32-unknown-elf")
	set(RISCV_ABI "-march=rv32g -mabi=ilp32d")
else()
	set(RISCV_ABI "-march=rv64g -mabi=lp64d")
endif()
set(WARNINGS  "-Wall -Wextra")
set(COMMON    "-fno-math-errno -fno-threadsafe-statics")
set(COMMON    "-O2 -fno-stack-protector ${COMMON}")
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
	set (COMMON "${COMMON} -ggdb3 -O0")
	set(DEBUGGING TRUE)
endif()
set(CMAKE_CXX_FLAGS "${WARNINGS} ${RISCV_ABI} -std=c++20 ${COMMON}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Ttext 0x120000")

if (LTO)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto -ffat-lto-objects")
endif()

if (GCSECTIONS AND NOT DEBUGGING)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffunction-sections -fdata-sections")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-gc-sections")
endif()

set(BUILD_SHARED_LIBS OFF)
set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "") # remove -rdynamic
set(USE_NEWLIB ON)

# enforce correct global include order for our tiny libc
include_directories(libc)
set (BBLIBCPATH "${CMAKE_SOURCE_DIR}/../ext/libriscv/binaries/barebones/libc")
include_directories(${BBLIBCPATH})

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/ext  ext)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/libc libc)
target_include_directories(libc PUBLIC ${APIPATH})
target_include_directories(libc PUBLIC ${UTILPATH})

function (add_verfile NAME VERFILE)
	set_target_properties(${NAME} PROPERTIES LINK_DEPENDS ${VERFILE})
	target_link_libraries(${NAME} "-Wl,--retain-symbols-file=${VERFILE}")
	# certain languages don't allow you to mark functions as used
	# so we need to explicitly keep these functions this way:
	file(STRINGS "${VERFILE}" SYMBOLS)
	foreach(SYMBOL ${SYMBOLS})
		if (NOT ${SYMBOL} STREQUAL "")
			#message(STATUS "Symbol retained: ${SYMBOL}")
			target_link_libraries(${NAME} "-Wl,--undefined=${SYMBOL}")
		endif()
	endforeach()
endfunction()

function (add_micro_binary NAME)
	# Find dyncall API and mark the files as generated
	set(DYNCALL_API
		${CMAKE_BINARY_DIR}/dyncalls/dyncall_api.cpp
		${CMAKE_BINARY_DIR}/dyncalls/dyncall_api.h)
	set_source_files_properties(${DYNCALL_API} PROPERTIES GENERATED TRUE)
	# the micro binary
	add_executable(${NAME} ${ARGN} ${DYNCALL_API})
	target_include_directories(${NAME} PUBLIC ${CMAKE_BINARY_DIR}/dyncalls)
	add_dependencies(${NAME} generate_dyncalls)
	# Add the whole libc directly as source files
	target_link_libraries(${NAME} -static -Wl,--whole-archive libc -Wl,--no-whole-archive)
	target_link_libraries(${NAME} frozen::frozen)
	# place ELF into the sub-projects source folder
	set_target_properties(${NAME}
		PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
	)
	# strip symbols but keep public API
	if (GCSECTIONS OR STRIP_SYMBOLS)
		if (NOT DEBUGGING)
		set(VERFILE "${CMAKE_CURRENT_SOURCE_DIR}/${VERSION_FILE}")
		if (EXISTS "${VERFILE}")
			add_verfile(${NAME} ${VERFILE})
		else()
			add_verfile(${NAME} ${CMAKE_SOURCE_DIR}/${VERSION_FILE})
		endif()
		add_custom_command(TARGET ${NAME} POST_BUILD
		COMMAND ${CMAKE_STRIP} --strip-debug -R .note -R .comment -- ${CMAKE_CURRENT_SOURCE_DIR}/${NAME})
		endif()
	endif()
endfunction()
