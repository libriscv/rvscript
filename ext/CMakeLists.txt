
# libriscv
add_subdirectory(libriscv/lib)
target_compile_definitions(riscv PUBLIC
	RISCV_EBREAK_MEANS_STOP=1
	RISCV_SYSCALLS_MAX=256
	RISCV_SI_SYSCALLS=1
)

add_subdirectory(fmt)
