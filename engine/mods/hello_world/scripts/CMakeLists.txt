#
# This CMake script builds the gameplay.elf binary.
# Also, if the HAVE_NIM CMake option is enabled, we also
# add some Nim code.
#

add_micro_binary(gameplay.elf "src/gameplay.symbols"
	"src/gameplay.cpp"
	"src/events.cpp"
)
