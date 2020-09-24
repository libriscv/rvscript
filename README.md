# RVScript

This repository implements a game engine oriented scripting system using [libriscv](https://github.com/fwsGonzo/libriscv) as a backend. By using a fast virtual machine with low call overhead and modern programming techniques like compile-time programming we can have a fast budgeted script that lives in a separate address space.

The environment is freestanding C++17 (or later) with an option for enabling RTTI and exceptions. Several CRT functions have been implemented as system calls, and will have native speeds.

The example C++ environment has some basic example timers and threads, as well as multiple machines to call into and between. The repository is a starting point for anyone who wants to try to use this in their game engine.

In no uncertain terms: This requires compiling ahead of time, and there is no JIT, although that means you can use it on consoles. I have no issues compiling my script on WSL2 or any Linux.

## Benchmarks

https://gist.github.com/fwsGonzo/f874ba58f2bab1bf502cad47a9b2fbed

Note that I update the gist now and then as I make improvements. The key benchmark is showing the low overhead to calling into the script.

The overhead of a system call is around 5ns last time I measured it, so keep that in mind. The threshold for benefiting from using a dedicated system call is very low, but for simple things like reading the position of an entity you could be using shared memory. Read-only to the machine.


## Demonstration

This repository is built as an example on how you could use advanced techniques to speed up and blur the lines between native and emulated modern C++. The main function is in [engine/src](engine/src/main.cpp).

All the host-side code is in the engine folder, and is written as if it was running inside a game engine, kinda.

The output from the program should look like this after completion:

```
engine$ ./engine
>>> [events] says: Entering event loop...
>>> [gameplay1] says: Hello world!
>>> [gameplay1] says: Exception caught!
> median 2ns  		lowest: 2ns     	highest: 9ns
>>> Measurement "VM function call overhead" median: 2 nanos

> median 147ns  		lowest: 139ns     	highest: 262ns
>>> Measurement "Thread creation overhead" median: 147 nanos

>>> [gameplay2] says: Hello Remote World! value = 1234!
>>> [gameplay1] says: Back again in the start() function! Return value: 1234
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
Object is alive? true
>>> [gameplay2] says: Object 'myobject' is dying!
Object is alive? false
```

This particular output is with C++ RTTI and exceptions enabled.


## Getting started

Install cmake, git, clang-10 or 11 (trunk) for your system. Don't use GCC - it's slower on all benchmarks.

Run `setup.sh` to make sure that libriscv is initialized properly. Then go into the engine folder and run:

```bash
bash build.sh
```

The engine itself should have no external dependencies outside of libriscv.

Running the engine is only half the equation as you will also want to be able to modify the scripts themselves. To do that you need a RISC-V compiler. However, the gameplay binary is provided with the repo so that you can run the engine demonstration without having to download and build a RISC-V compiler.

## Getting a RISC-V compiler

There are several ways to do this. However for now one requirement is to install the riscv-gnu-toolchains GCC 10.1.0 for RISC-V. Install it like this:

```
git clone https://github.com/riscv/riscv-gnu-toolchain.git
cd riscv-gnu-toolchain
git submodule update --init riscv-binutils
git submodule update --init riscv-gcc
git submodule update --init riscv-newlib
<install dependencies for GCC on your particular system here>
./configure --prefix=$HOME/riscv --with-arch=rv64g --with-abi=lp64d
make -j8
```

This compiler will be automatically used by the CMake script in the micro folder. Check out `micro/toolchain.cmake` for the details.

It's technically possible to build without any system files at all, but you will need to provide some minimal C++ headers for convenience: All freestanding headers, functional, type_traits and whatever else you need yourself. I recommend just installing the whole thing and just not link against it.

## Building script files

If you have installed the RISC-V compiler above, the rest should be simple enough. Just run `build.sh` in the root folder. It will create a new folder simply called `build` and build all the programs that are defined in `engine/mods/hello_world/scripts/CMakeLists.txt`:

```
add_micro_binary(gameplay.elf "src/gameplay.symbols"
	"src/gameplay.cpp"
	"src/events.cpp"
)
```

It only builds one binary currently, however that binary is used for many purposes.

If you want to build more binaries you can edit `CMakeLists.txt` and add a new binary like so:

```
add_micro_binary(my.elf "src/my.symbols"
	"src/mycode.cpp"
	"src/morecode.cpp"
)
```

You can share symbol file with any other binaries, and if you don't have a particular symbol that is listed that is fine. It should be possible to reuse the same symbol file for all binaries. The symbol file is a list of symbols that are public, and can be called from outside of the machine. In other words, if `start` is made public, by adding it to a symbols file, then you can call the function froom the outside like so: `myscript.call("start")`.

There is a lot of helper functionality built to make it easy to drop in new programs. See `engine/src/main.cpp` for some example code.

## Building with C++ RTTI and exceptions

The root `CMakeLists.txt` is used to build programs. Go into the root `build` folder for programs. Enable the RTTI_EXCEPT CMake option using ccmake or edit the build.sh shell script. You can also edit `build.sh` and add `-DRTTI_EXCEPT=ON` to the arguments passed to cmake. Run `build.sh` in the root to build any changes.

Exceptions and RTTI will bloat the binary by at least 170k according to my measurements. Additionally, you will have to increase the maximum allotted number of instructions to a call by at least 600k instructions, as the first exception thrown will have to run through a massive amount of code. However, any code that does not throw exceptions as part of normal operation will be fine. It could also be fine to just give up when an exception is thrown, although I recommend re-initializing the machine (around 10-20 micros on my hardware).

The binary that comes with the repository for testing does have C++ exceptions enabled. Atomics will probably have to be enabled to be able to catch exceptions. They are on by default and has no known associated performance penalty.


## WSL2 support

Follow this to install WSL2 on Windows 10: https://docs.microsoft.com/en-us/windows/wsl/install-win10

There is nothing different that you have to do on WSL2. Install dependencies for GCC, then clone and install the RISC-V toolchain like above. It will just work.

Install clang-10 if you want a separate compiler that can build RISC-V binaries. Note that you must build the RISC-V toolchain regardless as we need all the system headers that C++ uses.

You must be on the latest Windows insider version for this at the time of writing.


## Using other programming languages

This is not so easy, as you will have to create a tiny freestanding environment for your language (hard), and also implement the system call layer that your API relies on (easy). Both these things require writing inline assembly, although you only have to create the syscall wrappers once. That said, I have a Rust implementation here:
https://github.com/fwsGonzo/script_bench/tree/master/rvprogram/rustbin

You can use any programming language that can output RISC-V binaries. A tiny bit of info about Rust is that I was unable to build anything but rv64gc binaries, so you would need to enable the C extension in the build.sh script (where it is explicitly set to OFF).

The easiest languages to integrate are those that transpile to C or C++, such as Nim and Pythran.

Good luck.


## Details

I have written in detail about this subject here:

1: https://medium.com/@fwsgonzo/adventures-in-game-engine-programming-a3ab1e96dbde

2: https://medium.com/@fwsgonzo/using-c-as-a-scripting-language-part-2-7726f8e13e3

3: https://medium.com/@fwsgonzo/adventures-in-game-engine-programming-part-3-3895a9f5af1d

Part 3 is a good introduction that will among other things answer the 'why'.

## Common Issues

- The emulator is jumping to a misaligned instruction, or faulting on some other thing but I know for a fact that the assembly is fine.
	- You might have to enable an extension such as atomics (RISCV_EXT_A) or compressed (RISCV_EXT_C). These are enabled by default and have to be disabled by something, such as in `engine/build.sh`.
- After I enabled C++ exceptions and ran a try..catch the emulator seems to just stop.
	- When you call into the virtual machine you can give it a budget. A limit on the number of instructions it gets to run for that particular call (or any other limit you impose yourself). If you forget to check if the limit has been reached, then it will just look like it stopped. You can check this by comparing calculating instruction counter + max instructions beforehand, and comparing that to the instruction counter after the call. You can safely resume execution again by calling `machine.simulate()` again, as running out of instructions is not an exception. The first C++ exception thrown uses a gigaton of instructions and will easily blow the limit.
- I can't seem to call a public API function in another machine.
	- The function has to be added to the symbol file for it to not be removed as an optimization, assuming no other function references it. It's also possible that the remote machine you are calling into simply doesn't have that function - if it's running another binary.
- How do I share memory with the engine?
	- Create aligned memory in your engine and use the `machine.memory.insert_non_owned_memory()` function to insert it using the given page attributes. The machine will then create non-owning pages pointing to this memory sequentially. You can do this on as many machines as you want. The challenge then is to be able to use the pages as memory for your objects, and access the readable members in a portable way (the VMs are default 64-bit).
- My thread blocked and when it returned some shared data is stale now.
	- The microthread block function is an inline system call, which is not a full function call. If it was a function call then the compiler will assume memory is clobbered. You can solve this by adding a memory clobber somewhere where you think it makes the most sense. You can see what it looks like in the implementation of `block()` with a condition. You can also make the system call into a function by using a jump trap. See the implementation of `apicall` to see how it's done.
- Passing strings are slow.
	- Yes, use compile-time hashes of strings where you can, but I'm constantly trying new things to make things go fast. CRC32 is excellent for this, and it might be possible to make it use the SSE 4.2 CRC32 instruction both for run-time and compile-time.
- Why is a particular API function implemented as a macro?
	- Most likely that API function uses compile-time string hashes, and they can't be passed to a function. Instead the hash has to be computed before it's passed to the function and the way to do that is using macros. Hashes can be turned back into strings using reverse lookups, so you will be able to give context when logging errors.
- How do I allow non-technical people to compile script?
	- Hard question. If they are only going to write scripts for the actual levels it might be good enough to use a web API endpoint to compile these simpler binaries. You don't have to do the compilation in a Docker container, so it should be fairly straight-forward for a dedicated server. You could also have an editor in the browser, and when you press Save or Compile the resulting binary is sent to you. They can then move it to the folder it's supposed to be in, and restart the level. Something like that. I'm sure something that is even higher iteration can be done with some thought put into it.
- Will you add support for SIMD-like instructions for RISC-V?
	- Definitely. The extension isn't finalized yet, but if I think it's close to the real thing I'll do it. It should make it possible to implement most vector functions in the script, but benchmarking is needed.
- I'm unable to build one of the projects because of missing files.
	- If you're having problems with missing files, you might not have initialized all git submodules in the project. All the `ext` folders contain sub-projects. You can explicitly initialize these folders by going into each of them and running `git submodule update --init`.
- I'm worried about the impact on the performance from the ever-growing larger binaries, especially when using string formatting.
	- So far I haven't noticed any performance degradation from this, although I did notice when I enabled C++ exceptions. Don't use GC-sections as a band-aid - I've never seen it improve performance.
- I have real-time requirements.
	- As long as pausing the script to continue later is an option, you will not have any trouble. Just don't pause the script while it's in a thread and then accidentally vmcall into it from somewhere else. This will clobber all registers and you can't resume the machine later. You can use preempt provided that it returns to the same thread again (although you are able to yield back to a thread manually). There are many options where things will be OK. In my engine all long-running tasks are running on separate machines, alone.
- I need to share more than 2GB memory with my machines.
	- One thing to keep in mind is that the immediate instructions in RISC-V have a certain range that if exceeded requires you to rebuild everything with a separate, less efficient machine model. Not recommended. Instead, you should just be spamming more machines. They cost around 4-5k memory each and you can share anything you want with each machine, even the binary pages. This is already done as an example in this repository.
