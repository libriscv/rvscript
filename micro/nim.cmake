# Build functions for Nim language
set(NIM_LIBS "$ENV{HOME}/nim-1.2.6/lib" CACHE STRING "Nim library folder")

function(add_nim_sources NAME)
	set(NIMCACHE ${CMAKE_CURRENT_BINARY_DIR}/nimcache)
	foreach (FILENAME ${ARGN})
		get_filename_component(SHORTNAME ${FILENAME} NAME_WE)
		list(APPEND C_SOURCES "${NIMCACHE}/@m${SHORTNAME}.nim.cpp")
		list(APPEND J_SOURCES "${NIMCACHE}/${SHORTNAME}.json")
	endforeach()
	add_custom_command(
    	COMMAND "nim" "cpp" "--nimcache:${NIMCACHE}" "--os:any" "--gc:arc" "-d:useMalloc" "--exceptions:cpp" "--cpu:riscv64" "--noMain" "-c" ${ARGN}
    	DEPENDS ${ARGN}
		OUTPUT  ${J_SOURCES}
				${C_SOURCES}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	)
	list(APPEND C_SOURCES
		"${NIMCACHE}/stdlib_assertions.nim.cpp"
		"${NIMCACHE}/stdlib_parseutils.nim.cpp"
		"${NIMCACHE}/stdlib_strformat.nim.cpp"
		"${NIMCACHE}/stdlib_strutils.nim.cpp"
		"${NIMCACHE}/stdlib_system.nim.cpp"
		"${NIMCACHE}/stdlib_unicode.nim.cpp"
	)
	set_source_files_properties(${C_SOURCES}
		PROPERTIES GENERATED TRUE
		COMPILE_FLAGS -Wno-write-strings)
	message(STATUS "Nim sources for ${NAME}: ${C_SOURCES}")
	target_sources(${NAME} PUBLIC ${C_SOURCES})
	target_include_directories(${NAME} PRIVATE ${NIM_LIBS})
	target_include_directories(${NAME} PRIVATE ${APIPATH})
endfunction()
