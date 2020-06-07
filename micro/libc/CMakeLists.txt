
set(LIBC_SOURCES
	start.cpp
	assert.cpp
	engine.cpp
	write.cpp
	${BBLIBCPATH}/heap.cpp
	${BBLIBCPATH}/libc.cpp
	${BBLIBCPATH}/libcxx.cpp
	${BBLIBCPATH}/microthread.cpp
  )

add_library(libc STATIC ${LIBC_SOURCES})
target_include_directories(libc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(libc PRIVATE
	NATIVE_MEM_SYSCALLS=1
)
target_compile_definitions(libc PUBLIC
	USE_THREADCALLS=1
)
target_link_libraries(libc PUBLIC strf)
