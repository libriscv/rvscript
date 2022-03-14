# RVScript

This repository implements a game engine oriented scripting system using [libriscv](https://github.com/fwsGonzo/libriscv) as a backend. By using a fast virtual machine with low call overhead and modern programming techniques like compile-time programming we can have a fast budgeted script that lives in a separate address space.

The guest environment is modern C++20 using GCC 11.1 with an option for enabling RTTI and exceptions. Several CRT functions have been implemented as system calls, and will have good performance. There is also Nim support and some example code.

The example program has some basic example timers and threads, as well as multiple machines to call into and between. The repository is a starting point for anyone who wants to try to use this in their game engine.

In no uncertain terms: This requires compiling ahead of time, and there is no JIT, although that means you can use it on consoles. I have so far had no issues compiling my script on WSL2 or any Linux.

## Benchmarks

https://gist.github.com/fwsGonzo/f874ba58f2bab1bf502cad47a9b2fbed

Note that I update the gist now and then as I make improvements. The key benchmark is showing the low overhead to calling into the script.

The overhead of a system call is around 5ns last time I measured it, so keep that in mind. The threshold for benefiting from using a dedicated system call is very low, but for simple things like reading the position of an entity you could be using shared memory. Read-only to the machine.

For generally extending the API there is dynamic calls, which have an overhead of around 14ns. Dynamic calls require looking up a hash value in a hash map, and is backed by a std::function with capture storage.


## Demonstration

This repository is built as an example on how you could use advanced techniques to speed up and blur the lines between native and emulated modern C++. The main function is in [engine/src](engine/src/main.cpp).

All the host-side code is in the engine folder, and is written as if it was running inside a game engine, kinda.

The output from the program should look like this after completion:

```
>>> [events] says: Entering event loop...
>>> [gameplay1] says: Hello world!
>>> [gameplay1] says: Multi-process sum = 8192
>>> [gameplay1] says: Multi-process counter = 4
>>> [gameplay1] says: Multi-process result = 0
>>> [gameplay1] says: Forever multiprocessing result: -1
>>> [gameplay1] says: Single-process sum = 8192
>>> [gameplay1] says: Exception caught: This is a test!!
>>> [gameplay1] says: Exception thrown from: void start(), line 185
> median 6ns  		lowest: 6ns     	highest: 7ns
>>> Measurement "VM function call overhead" median: 6 nanos

> median 159ns  		lowest: 159ns     	highest: 161ns
>>> Measurement "Thread creation overhead" median: 159 nanos

> median 25ns  		lowest: 24ns     	highest: 35ns
>>> Measurement "Dynamic call handler" median: 25 nanos

> median 50ns  		lowest: 50ns     	highest: 70ns
>>> Measurement "Farcall lookup" median: 50 nanos

> median 38ns  		lowest: 38ns     	highest: 40ns
>>> Measurement "Farcall direct" median: 38 nanos

> median 7ns  		lowest: 6ns     	highest: 7ns
>>> Measurement "Benchmarking overhead" median: 7 nanos

> median 10183ns  		lowest: 9741ns     	highest: 10914ns
>>> Measurement "Multi-processing overhead" median: 10183 nanos

>>> [gameplay2] says: Hello Remote World! value = 1234!
>>> [gameplay2] says: Some struct string: Hello 123!
>>> [gameplay2] says: Some struct value: 42
>>> [gameplay1] says: Back again in the start() function! Return value: 1234
>>> [gameplay1] says: Some struct string: Hello 456!
Skipped over breakpoint in gameplay1:0x1205A0. Break here with DEBUG=1.
>>> [gameplay1] says: Hello Microthread World!
>>> [gameplay1] says: Back again in the start() function!
...
>>> [gameplay1] says: Tick 0!
>>> [gameplay1] says: I have a 44!
>>> [gameplay1] says: Tick 1!
>>> [gameplay1] says: I have a 45!
>>> [gameplay1] says: Tick 2!
>>> [gameplay1] says: I have a 46!
...
>>> [gameplay1] says: Hello Belated Microthread World! 1 second passed.
>>> [events] says: I am being run on another machine!
>>> [gameplay2] says: Hello C++ World
...
Calling 'myobject_death' in 'gameplay2'
>>> [gameplay2] says: Object 'myobject' is dying!
...
Benchmarking full fork:
> median 1202ns  		lowest: 1166ns     	highest: 1273ns
Benchmarking reset:
> median 1207ns  		lowest: 1198ns     	highest: 1268ns
...
>>> [nim] says: Before debugging
Skipped over breakpoint in nim:0x120234. Break here with DEBUG=1.
>>> [nim] says: Hello Nim World!
{
  "name": "Hello",
  "email": "World",
  "books": [
    "Foundation"
  ]
}
> median 22449ns  		lowest: 22279ns     	highest: 22950ns
```

This particular output is with C++ RTTI and exceptions enabled.


## Getting started

Install cmake, git, clang-12, lld-12 or later for your system. You can also use GCC 11.1 or later.

Run `setup.sh` to make sure that libriscv is initialized properly. Then go into the engine folder and run:

```bash
bash build.sh
```

The engine itself should have no external dependencies outside of libriscv.

Running the engine is only half the equation as you will also want to be able to modify the scripts themselves. To do that you need a RISC-V compiler. However, the gameplay binary is provided with the repo so that you can run the engine demonstration without having to download and build a RISC-V compiler.

## Getting a RISC-V compiler

There are several ways to do this. However for now one requirement is to install the riscv-gnu-toolchains GCC 11 for RISC-V. Install it like this:

```
git clone https://github.com/riscv/riscv-gnu-toolchain.git
cd riscv-gnu-toolchain
git submodule update --depth 1 --init riscv-binutils
git submodule update --depth 1 --init riscv-gcc
git submodule update --depth 1 --init riscv-newlib
<install dependencies for GCC on your particular system here>
./configure --prefix=$HOME/riscv --with-arch=rv64g --with-abi=lp64d
make -j8
```
For 32-bit RISC-V the incantation becomes:
```
./configure --prefix=$HOME/riscv --with-arch=rv32g --with-abi=ilp32d
```


This compiler will be automatically used by the CMake script in the micro folder. Check out `micro/toolchain.cmake` for the details.

It's technically possible to build without any system files at all, but you will need to provide some minimal C++ headers for convenience: All freestanding headers, functional, type_traits and whatever else you need yourself. I recommend just installing the whole thing and just not link against it. The barebones example in libriscv does this pretty well. Check out how it can build a custom environment in 4 different ways, including raw clang builds. Unfortunately, as mentioned above, if we can't even use std::string because there is no standard library, we might risk being hampered. Scripting is better when it's easy and straightforward.

## Building script files

If you have installed the RISC-V compiler above, the rest should be simple enough. Just run `build.sh` in the programs folder. It will create a new folder simply called `build` and build all the programs that are defined in `engine/mods/hello_world/scripts/CMakeLists.txt`:

```
add_micro_binary(gameplay.elf
	"src/gameplay.cpp"
	"src/events.cpp"
)
```

It only builds one binary currently, however that binary is used for many purposes.

If you want to build more binaries you can edit `CMakeLists.txt` and add a new binary like so:

```
add_micro_binary(my.elf
	"src/mycode.cpp"
	"src/morecode.cpp"
)
```

Any functions you want to be callable from outside must be listed in the symbols file if you want them to not be pruned when stripping symbols, usually `programs/symbols.map`. The file is shared between all programs. The symbol file is a text file with a list of symbols that are to be left alone when stripping the script programs. These symbols are usually the ones you want to be made visible so that we can call public functions from the engine. In other words, if the function `start` is made public, by retaining it, then you can call the function from the engine like so: `myscript.call("start")`. Note that even GC-sections will not prune functions that are using the `PUBLIC()` macro. This is because of `__attribute__((used, retain))`. It's only when the `STRIP_SYMBOLS` CMake option is enabled in `programs` that you need to care about the `symbols.map` file.

There is a lot of helper functionality built to make it easy to drop in new programs. See `engine/src/main.cpp` for some example code.

The debug.sh script will produce programs that you can remotely connect to with GDB. Run `DEBUG=1 ./build.sh` in the engine folder to enable remote debugging with GDB. The engine will listen for a remote debugger on each breakpoint dyncall in the code.

Install gdb-multiarch from your distro packaging system and run it like this:
```
gdb-multiarch gameplay.elf
```
Once inside GDB, you will need to connect to the GDB server in the engine:
```
target remote localhost:2159
```

## Building with C++ RTTI and exceptions

The programs `CMakeLists.txt` is used to build programs. Go into the build folder in `programs/build`. Enable the RTTI_EXCEPT CMake option using ccmake or you can also edit `build.sh` and add `-DRTTI_EXCEPT=ON` to the arguments passed to CMake. Run `build.sh` in to build any changes.

Exceptions and RTTI will bloat the binary by at least 170k according to my measurements. Additionally, you will have to increase the maximum allotted number of instructions to a call by at least 600k instructions, as the first exception thrown will have to run through a massive amount of code. However, any code that does not throw exceptions as part of normal operation will be fine. It could also be fine to just give up when an exception is thrown, although I recommend re-initializing the machine (around 10-20 micros on my hardware).

The binary that comes with the repository for testing does have C++ exceptions enabled. Atomics will probably have to be enabled to be able to catch exceptions. They are on by default and has no known associated performance penalty.


## WSL2 support

Follow this to install WSL2 on Windows 10: https://docs.microsoft.com/en-us/windows/wsl/install-win10

There is nothing different that you have to do on WSL2. Install dependencies for GCC, then clone and install the RISC-V toolchain like above. It will just work.

You must be on the latest Windows insider version for this at the time of writing.


## Using other programming languages

This is not so easy, as you will have to create a tiny freestanding environment for your language (hard), and also implement the system call layer that your API relies on (easy). Both these things require writing inline assembly, although you only have to create the syscall wrappers once. That said, I have a Rust implementation here:
https://github.com/fwsGonzo/script_bench/tree/master/rvprogram/rustbin

You can use any programming language that can output RISC-V binaries. A tiny bit of info about Rust is that I was unable to build anything but rv64gc binaries, so you would need to enable the C extension in the build.sh script (where it is sometimes explicitly set to OFF).

The easiest languages to integrate are those that transpile to C or C++, such as Nim, Haxe and Pythran. If you can stomach the extra cost of interpreting JavaScript then QuickJS can work well.

Good luck.


## Nim language support

There is Nim support with the HAVE_NIM boolean CMake option enabled. Once enabled, the `nim` program must be in PATH, and `NIM_LIBS` will be auto-detected to point to the nim lib folder. For example `/home/user/nim-1.6.4/lib`. Nim support is very experimental, especially 32-bit RISC-V as yours truly added that support in Nim and thus it cannot be trusted.

Remember to use `.exportc` to make your Nim entry functions callable from the outside, and also add them to your symbols file. Last, you will need to call `NimMain()` from the `int main()` entry function. All of this is shown in the `gameplay.nim` example as well as `gameplay.cpp`. The example code gets run from `main.cpp` with `another_machine.call("nim_test");`.

You may also have to enable RTTI and exceptions.

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
myscript.set_dynamic_call("lazy", [] (auto&) {
		fmt::print("I'm not doing much, tbh.\n");
	});
```
Will assign the function to the string "lazy". To call this function from inside the VM we have to amend dyncalls.json in the programs/dyncalls folder, like so:

```
"lazy":      "void sys_lazy ()"
```
The build system will see the JSON changed and rebuild some API files (see `programs/dyncalls/generate.py`), and it will expose the callable function `sys_lazy`:

```
sys_lazy();
```

A slightly more complex example, where we take an integer as argument, and return an integer as the result:
```
myscript.set_dynamic_call("object_id",
	[] (auto& script) {
		const auto [id] = script.machine().template sysargs <int> ();
		fmt::print("Object ID: {}\n", id);

		script.machine().set_result(1234);
	});
```

Or, let's take a struct by reference or pointer:
```
myscript.set_dynamic_call("struct_by_ref",
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
myscript.set_dynamic_call("struct_by_ref",
	[] (auto& script) {
		// A Buffer is a list of pointers to fragmented virtual memory,
		// which cannot be guaranteed to be sequential.
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



## Common Issues

- The emulator is jumping to a misaligned instruction, or faulting on some other thing but I know for a fact that the assembly is fine.
	- You might have to enable an extension such as atomics (RISCV_EXT_A) or compressed (RISCV_EXT_C). These are enabled by default and have to be disabled by something, such as in `engine/build.sh`.
- After I enabled C++ exceptions and ran a try..catch the emulator seems to just stop.
	- When you call into the virtual machine you usually give it a budget. A limit on the number of instructions it gets to run for that particular call (or any other limit you impose yourself). If you forget to check if the limit has been reached, then it will just look like it stopped. You can check this by comparing calculating instruction counter + max instructions beforehand, and comparing that to the instruction counter after the call. You can safely resume execution again by calling `machine.simulate()` again, as running out of instructions is not a fatal exception. For example the first C++ exception thrown inside the RISC-V emulator uses a gigaton of instructions and will easily blow the limit.
- I can't seem to call a public API function in another machine.
	- The function has to be added to the symbol file for it to not be removed as an optimization, assuming no other function references it. It's also possible that the remote machine you are calling into simply doesn't have that function if it's running another binary.
- How do I share memory with the engine?
	- Create aligned memory in your engine and use the `machine.memory.insert_non_owned_memory()` function to insert it using the given page attributes. The machine will then create non-owning pages pointing to this memory sequentially. You can do this on as many machines as you want. The challenge then is to be able to use the pages as memory for your objects, and access the readable members in a portable way (the VMs are default 64-bit).
- Passing strings are slow.
	- Use compile-time hashes of strings where you can.
	- Alternatively use memory sharing, riscv::Buffer or buffer gathering for larger memory buffers.
	- Page sharing if intended for communication between machines
- How do I allow non-technical people to compile script?
	- Hard question. If they are only going to write scripts for the actual levels it might be good enough to use a web API endpoint to compile these simpler binaries. You don't have to do the compilation in a Docker container, so it should be fairly straight-forward for a dedicated server. You could also have an editor in the browser, and when you press Save or Compile the resulting binary is sent to you. They can then move it to the folder it's supposed to be in, and restart the level. Something like that. I'm sure something that is even higher iteration can be done with some thought put into it.
- Will you add support for SIMD-like instructions for RISC-V?
	- Definitely. The extension has been finalized, but there is no real implementation that outputs these vector instructions presently. Because of this, it's on hold. I will very likely support 256-bit lanes only.
	- Heavy compute operations is probably best left implemented as specialized system calls or dynamic function calls.
- I have real-time requirements.
	- As long as pausing the script to continue later is an option, you will not have any trouble. Just don't pause the script while it's in a thread and then accidentally vmcall into it from somewhere else. This will clobber all registers and you will have trouble later. You can use preempt provided that it returns to the same thread again (although you are able to yield back to a thread manually). There are many options where things will be OK. In my own engine all long-running tasks are running on separate machines to simplify things.
