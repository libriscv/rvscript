# RVScript

This repository implements a game engine scripting system using [libriscv](https://github.com/fwsGonzo/libriscv) as a backend. It has some basic timers and threads, as well as multiple machines to call into and between. The repository is a starting point for anyone who wants to try to use this in their game engine.

# Getting started

Run `setup.sh` to make sure that libriscv is initialized properly. Then go into the engine folder and run:

```bash
bash build.sh
```

The engine itself should have no external dependencies outside of libriscv.

The output from the engine should look like this after completion:

```
>>> [events] says: Entering event loop...
>>> [gameplay1] says: Hello world!
> median 3ns  		lowest: 3ns     	highest: 3ns
>>> Measurement "VM function call overhead" median: 3 nanos

> median 105ns  		lowest: 104ns     	highest: 130ns
>>> Measurement "Thread creation overhead" median: 105 nanos

>>> [gameplay2] says: Hello Remote World! value = 1234!
>>> [gameplay1] says: Back again in the start() function! Return value: 1234
>>> [gameplay1] says: Hello Microthread World!
>>> [gameplay1] says: Back again in the start() function!
...
>>> [gameplay1] says: Hello Belated Microthread World! 1 second passed.
>>> [events] says: I am being run on another machine!
```


## Getting a RISC-V compiler

There are several ways to do this. However for now one requirement is to install the riscv-gnu-toolchains GCC 9.2.0 for RISC-V. Install it like this:

```
git clone https://github.com/riscv/riscv-gnu-toolchain.git
cd riscv-gnu-toolchain
./configure --prefix=$HOME/riscv --with-arch=rv32gc --with-abi=ilp32d
make -j8
```

This compiler will be automatically used by the CMake script in the micro folder. Check out toolchain.cmake for the details.

It's technically possible to build without any system files at all, but you will need to provide some minimal C++ headers for convenience: All freestanding headers, functional, type_traits and whatever else you need yourself. I recommend just installing the whole thing and just not link against it.


## Building script files

If you have installed the RISC-V compiler above, the rest should be simple enough. Just run `build.sh` in the micro folder. You can add and edit script files in the `engine/mods/hello_world/scripts` folder.


## Using other scripting languages

This is not so easy, as you will have to create a tiny freestanding environment for your language, and also implement the system call layer that your API relies on. Both these things require writing inline assembly.
That said, I have a Rust implementation here:
https://github.com/fwsGonzo/script_bench/tree/master/rvprogram/rustbin

You can use any programming language that can output RISC-V binaries.

Good luck.


## Details

I have written in detail about this subject here:
https://medium.com/@fwsgonzo/adventures-in-game-engine-programming-a3ab1e96dbde
https://medium.com/@fwsgonzo/using-c-as-a-scripting-language-part-2-7726f8e13e3
https://medium.com/@fwsgonzo/adventures-in-game-engine-programming-part-3-3895a9f5af1d

Part 3 is a good introduction that will among other things answer the 'why'.

