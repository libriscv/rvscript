# hubris

A game about making yourself useful for your people, or face the prospect of waning powers.

... or not. It depends on what happens in the years ahead!

## Getting a RISC-V compiler

There are several ways to do this. However for now one requirement is to build the newlib variant in the riscv-gnu-toolchain for RISC-V. Install it like this:

```sh
git clone https://github.com/riscv/riscv-gnu-toolchain.git
cd riscv-gnu-toolchain
git submodule update --depth 1 --init
<install dependencies for GCC on your particular system here>
./configure --prefix=$HOME/riscv --with-arch=rv64g --with-abi=lp64d
make -j8
```
For 32-bit RISC-V the incantation becomes:
```
./configure --prefix=$HOME/riscv --with-arch=rv32g --with-abi=ilp32d
```

Add `$HOME/riscv` to your PATH by adding `export PATH=$PATH:$HOME/riscv/bin` to your `~/.bashrc` file. After you have done this, close and reopen your terminals. You should be able to tab-complete `riscv64-unknown-elf-` now.

This compiler will be used by the CMake script in the micro folder. Check out `micro/toolchain.cmake` for the details.

```
$ riscv64-unknown-elf-g++ --version
riscv64-unknown-elf-g++ (g5964b5cd7) 11.1.0
```

While 32-bit RISC-V is faster to emulate than 64-bit, I prefer it when the sizes match between the address spaces.

## Building script files

If you have installed the RISC-V compiler above, the rest should be simple enough. Just run `build.sh` in the programs folder. It will create a new folder simply called `build` and build all the programs that are defined in `engine/mods/hello_world/scripts/CMakeLists.txt`:

```
add_micro_binary(gameplay.elf
	"src/gameplay.cpp"
	"src/events.cpp"
)
```

It only builds three programs currently, all used for demonstration purposes.

If you want to build more binaries you can edit `CMakeLists.txt` and add a new binary like so:

```
add_micro_binary(my.elf
	"src/mycode.cpp"
	"src/morecode.cpp"
)
```

The usual suspects will work with the build system such as ccache and ninja/n2.

There is a lot of helper functionality built to make it easy to drop in new programs. See `engine/src/main.cpp` for some example code.
