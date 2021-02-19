
# libriscv
option(RISCV_EXPERIMENTAL "" ON)
option(RISCV_BINARY_TRANSLATION "" ON)
option(RISCV_SHARED_PT "" ON)

add_subdirectory(libriscv/lib)
target_compile_definitions(riscv PUBLIC
	RISCV_EBREAK_MEANS_STOP=1
	RISCV_SYSCALLS_MAX=128
)

add_subdirectory(fmt)
