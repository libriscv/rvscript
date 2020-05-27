# RVScript

This repository implements a game engine scripting system using [libriscv](https://github.com/fwsGonzo/libriscv) as a backend. It has some basic timers and threads, as well as multiple machines to call into and between. The repository is a starting point for anyone who wants to try to use this in their game engine.

# Getting started

Run setup.sh to make sure that libriscv is initialized properly. Then go into the engine folder and run:

```bash
mkdir -p build
cd build && cmake .. && make -j8
./engine
```

The engine itself should have no external dependencies outside of libriscv.

The output from the engine should look like this after completion:

```
>>> [gameplay1] says: I am called before everything else (once on each machine)
>>> [gameplay2] says: I am called before everything else (once on each machine)
>>> [gameplay1] says: Hello world!
>>> [gameplay1] says: I am called before everything else (once on each machine)
>>> Measurement "Function call overhead" average: 15 nanos
>>> [gameplay2] says: Hello Remote World! value = 1234!
>>> [gameplay1] says: Back again in the start() function! Return value: 1234
>>> [gameplay1] says: Hello Microthread World!
>>> [gameplay1] says: Thread complete. Back again in the start() function!
...
>>> [gameplay1] says: Hello Belated Microthread World! 1 second passed.
```


## Getting a RISC-V compiler

There are several ways to do this. However for now one requirement is to install the GCC 8.3.0 system files for RISC-V. Install it like this:

```
xpm install --global @xpack-dev-tools/riscv-none-embed-gcc@latest
```

If you want to use this compiler, edit the the build.sh script in the micro folder so that CC and CXX points to this compiler. It will be installed in `~/opt/xPacks`.


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

Part 3 is a good introduction that will answer the 'why'.

