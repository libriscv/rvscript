
# libriscv
# C-extension is supported, but is not very performant
option(RISCV_EXT_C        "" OFF)
option(RISCV_EXPERIMENTAL "" ON)
option(RISCV_MULTIPROCESS "" ON)

add_subdirectory(libriscv/lib)
# We need to make room for our own system calls, as well as
# the classic Linux system calls (0-500). So we start at 500.
target_compile_definitions(riscv PUBLIC RISCV_SYSCALLS_MAX=600)

add_subdirectory(fmt)
