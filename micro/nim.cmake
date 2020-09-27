# Build functions for Nim language
set(NIM_LIBS "$ENV{HOME}/nim-1.2.6/lib" CACHE STRING "Nim library folder")

function(add_nim_sources NAME FILENAME)
	set(NIMCACHE ${CMAKE_CURRENT_BINARY_DIR}/nimcache)
	get_filename_component(SHORTNAME ${FILENAME} NAME_WE)
	set(C_SOURCES "${NIMCACHE}/@m${SHORTNAME}.nim.cpp")
	set(J_SOURCES "${NIMCACHE}/${SHORTNAME}.json")
	add_custom_command(
    	COMMAND "nim" "cpp" "--nimcache:${NIMCACHE}" "--os:any" "--gc:arc" "-d:useMalloc" "--exceptions:cpp" "--cpu:riscv64" "--noMain" "-p:${APIPATH}" "-c" ${FILENAME}
    	DEPENDS ${ARGN}
		OUTPUT  ${J_SOURCES}
				${C_SOURCES}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	)
	file(GLOB ALL_SOURCES "${NIMCACHE}/*.cpp")
	list(APPEND C_SOURCES ${ALL_SOURCES})
	set_source_files_properties(${C_SOURCES}
		PROPERTIES GENERATED TRUE
		COMPILE_FLAGS -Wno-write-strings)
	message(STATUS "Nim sources for ${NAME}: ${C_SOURCES}")
	target_sources(${NAME} PUBLIC ${C_SOURCES})
	target_include_directories(${NAME} PRIVATE ${NIM_LIBS})
	target_include_directories(${NAME} PRIVATE ${APIPATH})
endfunction()
