
set(LIBC_SOURCES
	assert.cpp
	engine.cpp
	write.cpp
	${BBLIBCPATH}/heap.cpp
	${BBLIBCPATH}/libc.cpp
	${BBLIBCPATH}/libcxx.cpp
	${BBLIBCPATH}/microthread.cpp
  )

set_source_files_properties(${BBLIBCPATH}/libc.cpp
	PROPERTIES COMPILE_FLAGS -fno-builtin)


add_library(libc STATIC ${LIBC_SOURCES})
target_include_directories(libc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(libc PRIVATE
	NATIVE_MEM_SYSCALLS=1
)
target_compile_definitions(libc PUBLIC
	NATIVE_SYSCALLS_BASE=570
	THREAD_SYSCALLS_BASE=590
)
target_compile_definitions(libc PUBLIC
	USE_NEWLIB=1
)
target_link_libraries(libc PUBLIC strf-header-only)
