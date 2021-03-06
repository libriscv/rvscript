
# libriscv
option(RISCV_EXPERIMENTAL "" ON)
option(RISCV_SHARED_PT "" ON)
#option(RISCV_BINARY_TRANSLATION "" ON)

add_subdirectory(libriscv/lib)
target_compile_definitions(riscv PUBLIC
	RISCV_SYSCALLS_MAX=128
)

add_subdirectory(fmt)
