
set(LIBC_SOURCES
	start.cpp
	libc.cpp
	heap.cpp
	assert.cpp
	libcxx.cpp
	write.cpp
	microthread.cpp
	engine.cpp
  )

add_library(libc STATIC ${LIBC_SOURCES})
target_include_directories(libc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(libc PRIVATE NATIVE_MEM_SYSCALLS=1)
target_link_libraries(libc PUBLIC strf)
