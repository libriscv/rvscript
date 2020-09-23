#
# We need the native libc and threads implementations,
# so build them as a static library w/includes
#
set(PATH ${CMAKE_SOURCE_DIR}/../ext/libriscv/emulator/syscalls)
set(MACHINE_SOURCES
	${PATH}/src/native_libc.cpp
	${PATH}/src/native_threads.cpp
)

add_library(machine STATIC ${MACHINE_SOURCES})
target_include_directories(machine PUBLIC ${PATH})
target_compile_definitions(machine PRIVATE
	NATIVE_SYSCALLS_BASE=70
	THREADS_SYSCALL_BASE=90
)
target_link_libraries(machine riscv)
