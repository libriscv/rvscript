# RVScript

RVScript is a game engine oriented scripting system backed by a [low latency RISC-V sandbox](https://github.com/fwsGonzo/libriscv). By using a fast virtual machine with low call overhead and memory usage, combined with modern programming techniques we can have a type-safe and memory-safe script that is able to call billions of functions within a limited frame budget.

[![Build Engine w/scripts](https://github.com/fwsGonzo/rvscript/actions/workflows/engine.yml/badge.svg)](https://github.com/fwsGonzo/rvscript/actions/workflows/engine.yml) [![Unit Tests](https://github.com/fwsGonzo/rvscript/actions/workflows/unittests.yml/badge.svg)](https://github.com/fwsGonzo/rvscript/actions/workflows/unittests.yml)

## Introduction

This project aims to change how scripting is done in game engines. Lua, Luau and even LuaJIT have fairly substantial overheads when making function calls into the script, especially when many arguments are involved. The same is true for WebAssembly emulators that I have measured, eg. wasmtime. I have yet to find another low latency emulator, actually. As a result, script functions are considered expensive to call, regardless of how little or how much they do, especially when used from a game engine where there is a tight deadline every frame. That changes thinking and design in projects accordingly. This repository is an attempt at making game scripting low latency, so that even automation games where interactions between complex machinery requires billions of script function calls, can still be achieved in a timely manner.


## Demonstration

This repository is built as a demonstration of how you could use advanced techniques to speed up and blur the lines between native and emulated modern C++. The main function is in [engine/src](engine/src/main.cpp). See also the [unit tests](/tests/).

All the host-side code is in the engine folder, and is written as if it was running inside a tiny fictional game framework.

The script programs are using modern C++20 using a GNU RISC-V compiler with RTTI and exceptions being optional. Several CRT functions have been implemented as system calls, and will have native performance. There is also Nim support and some example code.

The example programs have some basic timers and threads, as well as some examples making calls between machines.


## Getting started

Install cmake, git, GCC or Clang for your system.

Run [setup.sh](/setup.sh) to make sure that libriscv is initialized properly. Then go into the engine folder and run:

```sh
bash build.sh
```

There are also some benchmarks that can be performed with `BENCHMARK=1 ./build.sh`.

The scripting system itself is its own CMake project, and it has no external dependencies outside of libriscv and strf. libriscv has no dependencies, and strf can be replaced with libfmt.

Running the engine is only half the equation as you will also want to be able to modify the scripts themselves. To do that you need a RISC-V compiler.


## Using a RISC-V toolchain from system package

This is the simplest option.

```sh
sudo apt install g++-12-riscv64-linux-gnu
cd engine
./build.sh
```

The build script will detect which version of the GNU toolchain you have installed. Any RISC-V GCC compiler version 10 and later should be compatible.

The C-library that is used by this toolchain, glibc, will use its own POSIX multi-threading, and it will be required that it works in order for C++ exceptions to work. So, be careful with mixing microthread and C++ exceptions.

## Getting a newlib RISC-V compiler

If you want to produce performant and nimble executables, use riscvXX-unknown-elf from the RISC-V GNU toolchain. You can install it like this:

```sh
git clone https://github.com/riscv/riscv-gnu-toolchain.git
cd riscv-gnu-toolchain
git submodule update --depth 1 --init
<install dependencies for GCC on your particular system here>
./configure --prefix=$HOME/riscv --with-arch=rv64g_zba_zbb_zbc_zbs --with-abi=lp64d
make -j8
```

Add `$HOME/riscv` to your PATH by adding `export PATH=$PATH:$HOME/riscv/bin` to your `~/.bashrc` file. After you have done this, close and reopen your terminals. You should be able to tab-complete `riscv64-unknown-elf-` now.

This compiler will be preferred by the build script in the programs folder because it is more performant. Check out the [compiler detection script](/programs/detect_compiler.sh) for the selection process.

```sh
$ riscv64-unknown-elf-g++ --version
riscv64-unknown-elf-g++ (gc891d8dc23e) 13.2.0
```

## Building and running

If you have installed any RISC-V compiler the rest should be simple: Run [build.sh](/engine/build.sh) in the engine folder. It will also automatically start building script programs from the [programs folder](/programs). The actual script programs are located in [the scripts folder](/engine/scripts).

```sh
cd engine
./build.sh
```

If you want to select a specific compiler, you can edit [detect_compiler.sh](/programs/detect_compiler.sh) to make it prioritize another compiler.


## Live-debugging

Running `DEBUG=1 ./build.sh` in the [programs](programs) folder will produce programs that are easy to debug with GDB. Run `DEBUG=1 ./build.sh` in the engine folder to enable remote debugging with GDB. The engine will listen for a remote debugger on each breakpoint in the code. It will also try to start GDB automatically and connect for you. Remote GDB is a little bit wonky and doesn't like microthreads much but for the most part it works well.

Install gdb-multiarch from your distro packaging system:
```sh
sudo apt install gdb-multiarch
```

Connecting manually:
```
gdb-multiarch myprogram.elf
target remote :2159
```


## WSL2 support

Follow this to install WSL2 on Windows 10: https://docs.microsoft.com/en-us/windows/wsl/install-win10

There is nothing different that you have to do on WSL2. Install dependencies for GCC, then clone and install the RISC-V toolchain like above. It will just work.


## Creating an API

The general API to adding new functionality inside the VM is to add more system calls, or use dynamic calls. System calls can only capture a single pointer, require hard-coding a number (the system call number), and is invoked from inline assembly inside the guest. Performant, but clearly not very conducive to auto-generated APIs or preventing binding bugs.

Dynamic calls are string names that when invoked from inside the script will call a std::function on the host side, outside of the script. An example:

```C++
Script::set_dynamic_call("void sys_lazy ()", [] (auto&) {
		strf::to(stdout)("I'm not doing much, tbh.\n");
	});
```
Will assign the function to the dynamic call "void sys_lazy ()", which is done to help minimize ABI mistakes like passing wrong argument types. To call this function from inside the script programs we have to amend [dynamic_calls.json](/programs/dynamic_calls.json), like so:

```json
"lazy":      "void sys_lazy ()"
```
The build system will see the JSON changed and rebuild some API files (see [generate.py](/programs/dyncalls/generate.py)), and it will expose the callable function `sys_lazy`:

```C++
sys_lazy();
```

A slightly more complex example, where we take an integer as argument, and return an integer as the result:
```C++
Script::set_dynamic_call("object_id",
	[] (Script& script) {
		const auto [id] = script.args <int> ();
		strf::to(stdout)("Object ID: ", id, "\n");

		script.machine().set_result(1234);
	});
```

Or, let's take a struct by reference or pointer:
```C++
Script::set_dynamic_call("struct_by_ref",
	[] (Script& script) {
		struct Something {
			int a, b, c, d;
		};
		const auto [s] = script.args <Something> ();
		strf::to(stdout)("Struct A: ", s.a, "\n");
	});
```

Also, let's take a `char* buffer, size_t length` pair as argument:
```C++
Script::set_dynamic_call("void sys_big_data (const char*, size_t)",
	[] (Script& script) {
		// A Buffer is a general-purpose container for fragmented virtual memory.
		// The <riscv::Buffer> consumes two registers (A0: pointer, A1: length).
		const auto [buffer] = script.args <riscv::Buffer> ();
		handle_buffer(buffer.to_string());
		// Or, alternatively (also consumes two registers):
		const auto [view] = script.args <std::string_view> ();
		handle_buffer(view);
	});
```

In this case we would add this to [dynamic_calls.json](/programs/dynamic_calls.json):
```json
"big_data":      "void sys_big_data (const char*, size_t)"
```
See also [memory.hpp](/ext/libriscv/lib/libriscv/memory.hpp) for a list of helper functions, each with a specific purpose. The helper functions exist to simplify string and memory operations.
