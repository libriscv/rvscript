
# libriscv
option(RISCV_EXPERIMENTAL "" ON)
option(RISCV_BINARY_TRANSLATION "" ON)

add_subdirectory(libriscv/lib)
target_compile_definitions(riscv PUBLIC
	RISCV_EBREAK_MEANS_STOP=1
	RISCV_SYSCALLS_MAX=256
)

add_subdirectory(fmt)
