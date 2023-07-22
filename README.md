# RVScript

This repository implements a game engine oriented scripting system using [libriscv](https://github.com/fwsGonzo/libriscv) as a backend. By using a fast userspace emulator with low call overhead and memory usage, combined with modern programming techniques we can have a fast budgeted script that lives separately in its own address space.

The guest environment is modern C++20 using GCC 11.1 with RTTI and exceptions enabled. Several CRT functions have been implemented as system calls, and will have good performance. There is also Nim support and some example code.

The example programs have some basic example timers and threads, as well as multiple machines to call into and between. The repository is a starting point for anyone who wants to try to use this in their game engine.

In no uncertain terms: This requires compiling ahead of time, and there is no JIT, although that means you can use it on consoles. I have so far had no issues compiling my script on WSL2 or any Linux.

## Benchmarks

https://gist.github.com/fwsGonzo/2f4518b66b147ee657d64496811f9edb

Note that I update the gist now and then as I make improvements. The key benchmark is showing the low overhead to calling into the script.

The overhead of a system call is around 1ns last time I measured it, so you can basically pick and choose what kind of overhead you are comfortable with. Raw system calls are harder to maintain, but faster, while dynamic calls have slight overheads in lookups.

For generally extending the API there is dynamic calls, which have an overhead of around 14ns (on my laptop). Dynamic calls require looking up a hash value, and is backed by a std::function with capture storage. See eg. [timers](/engine/src/timers_setup.cpp).


## Demonstration

This repository is built as an example on how you could use advanced techniques to speed up and blur the lines between native and emulated modern C++. The main function is in [engine/src](engine/src/main.cpp).

All the host-side code is in the engine folder, and is written as if it was running inside a game engine, kinda.

## Getting started

Install cmake, git, clang-12, lld-12 or later for your system. You can also use GCC 11.1 or later.

Run [setup.sh](/setup.sh) to make sure that libriscv is initialized properly. Then go into the engine folder and run:

```bash
bash build.sh
```

This project has no external dependencies outside of libriscv and libfmt. libriscv has no dependencies.

Running the engine is only half the equation as you will also want to be able to modify the scripts themselves. To do that you need a RISC-V compiler.

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

If you have installed the RISC-V compiler above, the rest should be simple enough. Just run `build.sh` in the programs folder. It will create a new folder simply called `build` and build all the programs that are defined in `engine/scripts/CMakeLists.txt`:

```
add_level(hello.elf 0x400000
	"src/hello.cpp"
)
```

The usual suspects will work with the build system such as ccache and ninja/n2.

There is a lot of helper functionality built to make it easy to drop in new programs. See `engine/src/main.cpp` for some example code.


## Stripping symbols

Any functions you want to be callable from outside should be listed in the symbols file if you want them to not be pruned when stripping symbols. The symbol file is `programs/symbols.map`, and the file is shared between all programs. It is a text file with a list of symbols that are to be left alone when stripping the script programs. It is only relevant when the `STRIP_SYMBOLS` CMake option is enabled. It is disabled by default.

These symbols are usually the ones you want to be made visible so that we can call public functions from the engine. In other words, if the function `start` is made public, by retaining it, then you can call the function from the engine like so: `myscript.call("start")`. Note that even GC-sections will not prune functions that are using the `PUBLIC()` macro. This is because of `__attribute__((used, retain))`.


## Live-debugging

Running `DEBUG=1 ./build.sh` in the [programs](programs) folder will produce programs that are easy to debug with GDB. Run `DEBUG=1 ./build.sh` in the engine folder to enable remote debugging with GDB. The engine will listen for a remote debugger on each breakpoint in the code. It will also try to start GDB automatically and connect for you. Remote GDB is a little bit wonky and doesn't like microthreads much but for the most part it works well.

Install gdb-multiarch from your distro packaging system:
```
sudo apt install gdb-multiarch
```

Connecting manually:
```
gdb-multiarch myprogram.elf
target remote localhost:2159
```


## C++ RTTI and exceptions

Exceptions and RTTI are currently always enabled, and will bloat the binary by at least 170k according to my measurements. Additionally, you will have to increase the maximum allotted number of instructions to a call by at least 600k instructions, as the first exception thrown will have to run through a massive amount of code. However, any code that does not throw exceptions as part of normal operation will be fine performance-wise.


## WSL2 support

Follow this to install WSL2 on Windows 10: https://docs.microsoft.com/en-us/windows/wsl/install-win10

There is nothing different that you have to do on WSL2. Install dependencies for GCC, then clone and install the RISC-V toolchain like above. It will just work.

You must be on the latest Windows insider version for this at the time of writing.


## Using other programming languages

This is not so easy, as you will have to create a tiny freestanding environment for your language (hard), and also implement the system call layer that your API relies on (easy). Both these things require writing inline assembly, although you only have to create the syscall wrappers once. That said, I have a Rust implementation here:
https://github.com/fwsGonzo/script_bench/tree/master/rvprogram/rustbin

You can use any programming language that can output RISC-V binaries. A tiny bit of info about Rust is that I was unable to build anything but rv64gc binaries, so you would need to enable the C extension in the build.sh script (where it is sometimes explicitly set to OFF).

The easiest languages to integrate are those that transpile to C or C++, such as Nim, Haxe and Pythran. If you can stomach the extra cost of interpreting JavaScript then QuickJS can work well. Any language on the [list of compilers targetting C](https://github.com/dbohdan/compilers-targeting-c) would work.

Good luck.


## Nim language support

There is Nim support and a few examples are in the [micronim folder](/programs/micronim). The `nim` program must be in PATH, and `NIM_LIBS` will be auto-detected to point to the nim lib folder. For example `$HOME/nim-1.6.6/lib`. Nim support is experimental and the API is fairly incomplete.

The Nim build system is not easily worked with to build in parallel, but I have made an effort with the build scripts. Nim programs are built in parallel and the CMake build after is also that. There is still a lot of shared generated code, but those issues need to be solved by the Nim developers.

Remember to use `.exportc` to make your Nim entry functions callable from the outside, and also add them to the [symbols file](/programs/micronim/src/default.symbols). If your function is listed in the symbols file as well as exported with `.exportc`, you should be able to straight up call it using `script.call("myfunction")`.

There is example code on how to load Nim programs at the bottom of [main.cpp](/engine/src/main.cpp).

Nim code can be live-debugged just like other programs by running the engine with `DEBUG=1`. The Nim programs should be built with `DEBUG=1` also, to disable optimizations and generate richer debug information.

## Details

I have written in detail about this subject here:

1: https://medium.com/@fwsgonzo/adventures-in-game-engine-programming-a3ab1e96dbde

2: https://medium.com/@fwsgonzo/using-c-as-a-scripting-language-part-2-7726f8e13e3

3: https://medium.com/@fwsgonzo/adventures-in-game-engine-programming-part-3-3895a9f5af1d

Part 3 is a good introduction that will among other things answer the 'why'.


## Creating an API

The general API to adding new functionality inside the VM is to add more system calls, or use dynamic calls. System calls can only capture a single pointer, require hard-coding a number (the system call number), and is invoked from inline assembly inside the guest. Performance, but clearly not very conducive to auto-generated APIs or preventing binding bugs.

Dynamic calls are string names that when invoked will call a std::function on the host side. An example:

```
Script::set_dynamic_call("lazy", [] (auto&) {
		fmt::print("I'm not doing much, tbh.\n");
	});
```
Will assign the function to the string "lazy". To call this function from inside the RISC-V programs we have to amend [dyncalls.json](/programs/dyncalls/dyncalls.json) in the [dyncalls folder](/programs/dyncalls), like so:

```
"lazy":      "void sys_lazy ()"
```
The build system will see the JSON changed and rebuild some API files (see [generate.py](/programs/dyncalls/generate.py)), and it will expose the callable function `sys_lazy`:

```
sys_lazy();
```

A slightly more complex example, where we take an integer as argument, and return an integer as the result:
```
Script::set_dynamic_call("object_id",
	[] (auto& script) {
		const auto [id] = script.machine().template sysargs <int> ();
		fmt::print("Object ID: {}\n", id);

		script.machine().set_result(1234);
	});
```

Or, let's take a struct by reference or pointer:
```
Script::set_dynamic_call("struct_by_ref",
	[] (auto& script) {
		struct Something {
			int a, b, c, d;
		};
		const auto [s] = script.machine().template sysargs <Something> ();
		fmt::print("Struct A: {}\n", s.a);
	});
```

Also, let's take a `char* buffer, size_t length` pair as argument:
```
Script::set_dynamic_call("big_data",
	[] (auto& script) {
		// A Buffer is a list of pointers to fragmented virtual memory,
		// which cannot always be guaranteed to be sequential.
		// riscv::Buffer consumes two system call arguments (pointer + length).
		const auto [buffer] = script.machine().template sysargs <riscv::Buffer> ();

		// But we can check if it is, as a fast-path:
		if (buffer.is_sequential()) {
			// NOTE: The buffer is read-only
			handle_buffer(buffer.data(), buffer.size());
		}
		else {
			handle_buffer(buffer.to_string().c_str(), buffer.size());
			// NOTE: you can also copy to a sequential destination
			//buffer.copy_to(dest, dest_size);
		}
	});
```

In this case we would add this to `dyncalls.json`:
```
"big_data":      "void sys_big_data (const char*, size_t)"
```
See `libriscv/memory.hpp` for a list of helper functions, each with a specific purpose.

## Common Issues

- After I throw a C++ exception the emulator seems to just stop.
	- When you call into the virtual machine you usually give it a budget. A limit on the number of instructions it gets to run for that particular call (or any other limit you impose yourself). If you forget to check if the limit has been reached, then it will just look like it stopped. You can check this with `script.machine().instruction_limit_reached()`. You can safely resume execution again by calling `script.resume()` again, as running out of instructions is not exceptional. For example the first C++ exception thrown inside the RISC-V emulator uses a gigaton of instructions and can blow the default limit.
- How do I share memory with the engine?
	- Create aligned memory in your engine and use the `machine.memory.insert_non_owned_memory()` function to insert it using the given page attributes. The machine will then create non-owning pages pointing to this memory sequentially. You can do this on as many machines as you want. The challenge then is to be able to use the pages as memory for your objects, and access the readable members in a portable way (the VMs are default 64-bit).
- Passing long strings and big structures is slow.
	- Use compile-time hashes of strings where you can.
	- Alternatively use memory sharing, riscv::Buffer or buffer gathering for larger memory buffers.
	- Page sharing if intended for communication between machines
- How do I allow non-technical people to compile script?
	- Hard question. If they are only going to write scripts for the actual levels it might be good enough to use a web API endpoint to compile these simpler binaries. You don't have to do the compilation in a Docker container, so it should be fairly straight-forward for a dedicated server. You could also have an editor in the browser, and when you press Save or Compile the resulting binary is sent to you. They can then move it to the folder it's supposed to be in, and restart the level. Something like that. I'm sure something that is even higher iteration can be done with some thought put into it.
- Will you add support for SIMD-like instructions for RISC-V?
	- Definitely. I already support a few 256-bit operations, but it is not near completion.
- I have real-time requirements.
	- As long as pausing the script to continue later is an option, you will not have any trouble. Just don't pause the script while it's in a thread and then accidentally vmcall into it from somewhere else. This will clobber all registers and you will have trouble later. You can use preempt provided that it returns to the same thread again (although you are able to yield back to a thread manually). There are many options where things will be OK. In my own engine all long-running tasks are running on separate machines to simplify things.
- My program is jumping to a misaligned instruction. Something is very wrong!
	- Try enabling the RISCV_EXT_C CMake option and see if perhaps your RISC-V programs are built with compressed instructions enabled. They are the most performant in libriscv, but they are pretty standard. For example the standard 64-bit RISC-V architecture is RV64GC, while the most performant in libriscv is RV64G.
