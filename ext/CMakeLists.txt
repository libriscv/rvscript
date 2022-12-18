
# libriscv
option(RISCV_THREADED "" ON)
option(RISCV_EXPERIMENTAL "" OFF)
option(RISCV_EXT_C "" OFF)
option(RISCV_EXT_V "" ON)
option(RISCV_MULTIPROCESS "" OFF)
option(RISCV_MEMORY_TRAPS "" OFF)

add_subdirectory(libriscv/lib)
# We need to make room for our own system calls, as well as
# the classic Linux system calls (0-500). So we start at 500.
target_compile_definitions(riscv PUBLIC RISCV_SYSCALLS_MAX=600)

add_subdirectory(strf)
target_compile_definitions(strf-header-only INTERFACE STRF_HAS_STD_STRING_VIEW=1)

add_subdirectory(library)

option(NANOGUI_BUILD_EXAMPLES "" OFF)
option(NANOGUI_BUILD_GLFW "" OFF)
option(NANOGUI_BUILD_PYTHON "" OFF)
option(NANOGUI_INSTALL "" OFF)
add_subdirectory(nanogui)
