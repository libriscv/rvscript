cmake_minimum_required (VERSION 3.0.2)
project (engine C CXX)

option(ENABLE_FLTO "Enable LTO builds" OFF)
option(DEBUGGING   "Enable debugging with GDB" OFF)
option(SANITIZE    "Enable sanitizers" OFF)
option(GPROF       "Enable profiling with Gprof" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-Wall -Wextra -O2 -msse3 -mfpmath=sse")
set(BUILD_SHARED_LIBRARIES OFF)

if (ENABLE_FLTO)
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto=thin")
	 	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -flto=thin")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld")
	else()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto=8")
	 	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -flto=8")
    endif()
endif()

if (DEBUGGING)
 	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb3 -O0")
endif()

if (SANITIZE)
 	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fsanitize=undefined")
endif()

if (GPROF)
 	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")
endif()

add_subdirectory(../ext ext)
add_subdirectory(src)

add_executable(engine src/main.cpp)
target_link_libraries(engine libengine -static-libstdc++ -static-libgcc)
