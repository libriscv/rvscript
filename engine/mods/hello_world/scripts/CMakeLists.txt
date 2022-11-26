#
# This CMake script builds the gameplay.elf binary.
# You can build as many programs as you need (including scripts for maps) here.
# All programs are incrementally built.
#
# add_micro_binary(program.name 0xBaseAddress [source ... files]):
#
# 1. The first argument is the program name
# 2. The second argument is the programs base address, which we can use to put
# special programs on different addresses so that we can use memory sharing
# between programs in a 1:1 manner, using simple programming.
# All level scripts can have the same base address, while helper programs
# that we call into can have other base addresses.
# 3. The remaining arguments are source files.

add_shared_program(gameplay.elf 0x50000000
	*[Gg]ameplay*
	"src/gameplay.cpp"
	"src/events.cpp"
)

add_level(level1.elf 0x400000
	"src/level1.cpp"
	"src/events.cpp"
)
attach_program(level1.elf gameplay.elf)
