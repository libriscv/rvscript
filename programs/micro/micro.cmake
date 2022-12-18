option(LTO         "Link-time optimizations" ON)
option(GCSECTIONS  "Garbage collect empty sections" ON)
set(VERSION_FILE   "symbols.map" CACHE STRING "Retained symbols file")
option(STRIP_SYMBOLS "Remove all symbols except the public API" OFF)

#
# Build configuration
#
set (ENGINE_PATH "${CMAKE_SOURCE_DIR}/../engine")
set (APIPATH "${ENGINE_PATH}/api")
set (UTILPATH "${ENGINE_PATH}/src/util")

set(WARNINGS  "-Wall -Wextra -ggdb3 -include strf.hpp")
set(COMMON    "-fno-math-errno -fno-threadsafe-statics")
set(COMMON    "-O2 -fno-stack-protector ${COMMON}")
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
	set (COMMON "${COMMON} -ggdb3 -O0 -fstack-protector")
	set(DEBUGGING TRUE)
endif()
set(CMAKE_CXX_FLAGS "${WARNINGS} -std=gnu++20 ${COMMON}")

if (LTO)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto -ffat-lto-objects")
endif()

if (GCSECTIONS AND NOT DEBUGGING)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffunction-sections -fdata-sections")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-gc-sections")
endif()

set(BUILD_SHARED_LIBS OFF)
set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "") # remove -rdynamic

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
endfunction()

function (add_micro_binary NAME ORG)
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
	target_link_libraries(${NAME} "-Wl,-Ttext-segment=${ORG}")
	# place ELF into the sub-projects source folder
	set_target_properties(${NAME}
		PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
	)
	# strip symbols but keep public API
	if (STRIP_SYMBOLS AND NOT DEBUGGING)
		set(VERFILE "${CMAKE_CURRENT_SOURCE_DIR}/${VERSION_FILE}")
		if (EXISTS "${VERFILE}")
			add_verfile(${NAME} ${VERFILE})
		else()
			add_verfile(${NAME} ${CMAKE_SOURCE_DIR}/${VERSION_FILE})
		endif()
		target_link_libraries(${NAME} "-Wl,-x,-S")
	endif()

endfunction()

# The shared program produces symbols used by --just-symbols
# in order to provide public addresses
function (add_shared_program NAME ORG WILDCARD)
	add_micro_binary(${NAME} ${ORG} ${ARGN})
	add_custom_command(
		TARGET ${NAME} POST_BUILD
		COMMAND ${CMAKE_OBJCOPY} -w --extract-symbol --strip-symbol=!${WILDCARD} --strip-symbol=* ${CMAKE_CURRENT_SOURCE_DIR}/${NAME} ${NAME}.syms
	)
endfunction()

function (add_level NAME ORG)
	add_micro_binary(${NAME} ${ORG} ${ARGN})
endfunction()

function (attach_program NAME PROGRAM)
	set(SYMFILE ${PROGRAM}.syms)
	add_dependencies(${NAME} ${PROGRAM})
	set_property(TARGET ${NAME} PROPERTY LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${PROGRAM})
	target_link_libraries(${NAME} -Wl,--just-symbols=${SYMFILE})
endfunction()
