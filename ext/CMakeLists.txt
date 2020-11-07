
# libriscv
add_subdirectory(libriscv/lib)
target_compile_definitions(riscv PUBLIC
	RISCV_EBREAK_MEANS_STOP=1
	RISCV_SYSCALLS_MAX=256
	RISCV_INBOUND_JUMPS_ONLY=1
)

add_subdirectory(fmt)
