
set(LIBC_SOURCES
	assert.cpp
	engine.cpp
	write.cpp
	${BBLIBCPATH}/heap.cpp
	${BBLIBCPATH}/libc.cpp
	${BBLIBCPATH}/libcxx.cpp
	${BBLIBCPATH}/microthread.cpp
  )
if (NOT USE_NEWLIB)
	list(APPEND LIBC_SOURCES start.cpp)
endif()

set_source_files_properties(${BBLIBCPATH}/libc.cpp
	PROPERTIES COMPILE_FLAGS -fno-builtin)


add_library(libc STATIC ${LIBC_SOURCES})
target_include_directories(libc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(libc PRIVATE
	NATIVE_MEM_SYSCALLS=1
)
target_compile_definitions(libc PUBLIC
	USE_THREADCALLS=1
	NATIVE_SYSCALLS_BASE=70
	THREAD_SYSCALLS_BASE=90
)
if (USE_NEWLIB)
	target_compile_definitions(libc PUBLIC
		USE_NEWLIB=1
	)
endif()
target_link_libraries(libc PUBLIC strf)
