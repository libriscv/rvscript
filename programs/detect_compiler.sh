# We prefer a custom-built Newlib compiler (due to small binaries)
if command -v "riscv64-unknown-elf-g++" &> /dev/null; then
	echo "* Building game scripts with Newlib RISC-V compiler"
	GCC_TRIPLE="riscv64-unknown-elf"
	export CXX="ccache $GCC_TRIPLE-g++"
	export CEXT="OFF"

# Then, second a custom-built Linux compiler
elif command -v "riscv64-unknown-linux-gnu-g++" &> /dev/null; then
	echo "* Building game scripts with custom GCC/glibc compiler"
	GCC_TRIPLE="riscv64-unknown-linux-gnu"
	export CXX="ccache $GCC_TRIPLE-g++"
	export CEXT="OFF"

# System-provided GCC variants
elif command -v "riscv64-linux-gnu-g++-13" &> /dev/null; then
	echo "* Building game scripts with system GCC/glibc compiler"
	GCC_TRIPLE="riscv64-linux-gnu"
	export CXX="ccache $GCC_TRIPLE-g++-13"
	export CEXT="ON"

elif command -v "riscv64-linux-gnu-g++-12" &> /dev/null; then
	echo "* Building game scripts with system GCC/glibc compiler"
	GCC_TRIPLE="riscv64-linux-gnu"
	export CXX="ccache $GCC_TRIPLE-g++-12"
	export CEXT="ON"

elif command -v "riscv64-linux-gnu-g++-11" &> /dev/null; then
	echo "* Building game scripts with system GCC/glibc compiler"
	GCC_TRIPLE="riscv64-linux-gnu"
	export CXX="ccache $GCC_TRIPLE-g++-11"
	export CEXT="ON"

elif command -v "riscv64-linux-gnu-g++-10" &> /dev/null; then
	echo "* Building game scripts with system GCC/glibc compiler"
	GCC_TRIPLE="riscv64-linux-gnu"
	export CXX="ccache $GCC_TRIPLE-g++-10"
	export CEXT="ON"

else
	echo "* Error: Could not detect RISC-V compiler. Install one from your package manager."
	echo "* Example: sudo apt install gcc-12-riscv64-linux-gnu"
	exit 1
fi
