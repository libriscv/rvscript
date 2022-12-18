# micro

An implementation of syscall-accelerated C++ binaries for the RISC-V scripting backend. CMake is used to make it easy to add new binaries.

Check the CMakeLists.txt for build options, although the only really relevant one is `RTTI_EXCEPT` which enables C++ RTTI and exceptions should you want that.


## Getting started

You need the RISC-V compiler specified in the main README to be able to compile this. It's set up so that if the compiler is installed into `~/riscv` it will just work.

Remember to add the compiler to your PATH variable. If you edit `~/.bashrc` you can add this line:
```
export PATH=$PATH:$HOME/riscv/bin
```
This gives you access to the compiler anywhere in the terminal. Just remember to close and reopen it after adding the line.


## Code structure

The `api` folder contains the public API and its inlined implementation. There is a tiny libc in the `libc` folder, as well as some overridden C++ headers to fast-path new/delete. System call wrappers are implemented in `libc/include/syscall.hpp`, while the numbers are in `api/syscalls.h`.

This build system will take the libc, some compiler libraries and produce binaries which are defined in the games folder. You can find these in `engine/mods/hello_world`, and most of it in the scripts folder.


## Calling convention

See: https://riscv.org/wp-content/uploads/2015/01/riscv-calling.pdf

You can also use `libc/include/syscall.hpp` as a cheat sheet as system calls and function calls use the same registers, except A7 is used for the system call number.

## Troubleshooting

- Can I build these binaries on Windows?
	- Yes, with WSL2 you can build these binaries without making any changes to the build system or code. It just works.
- I'm unable to execute a specific function on a remote machine.
	- You have to remotely execute a function that exists on that machine, and not the machine you are executing from.
- After using `vmcall()` on a machine it stops working
	- If that machine was in the middle of something when getting called into, then the registers would be clobbered and so when you try to resume the machine later it would be in an exited state. That is, it would be at the point where the vmcall ended, instead of the point where it left off the last time. You can use `preempt()` to call into a machine which is running some long-term operation.
- Why can't I block the main thread?
	- Many reasons. Most importantly because the main thread is going to get clobbered by other functions calling into the machine. It was a design decision (that I have revisited many times) to only allow blocking other threads. It makes everything simple in both the implementation and for the user too, when you think about it.
- Why don't you have real coroutines?
	- Well, they are arriving now in all relevant compilers, so you can probably use them right now. But. They are not good for driving script in game engines, even though they are compiler-assisted and insanely performant. The reason is that only the game engine knows when events happen, and because of this the game engine needs the ability to resume any coroutine at any point in time, by suspending anything that was happening before and then waking up the right thread. Yes, we need threads for that, and there are native micro-threads that are written for this task.
- I'm unable to throw a bog-standard C++ exception
	- The C++ standard library is not linked in, since it requires basically a standard C library, which we don't want to provide. It would be slow and inefficient for a myriad of reasons. If you want a fast C++ library use EASTL. I have a fork of it where I change 4 lines to make it build on RISC-V. You can still use most C++ containers that are inline-able, such as std::vector.
- Do you support any sanitizers?
	- No, but I could drop in support for ubsan. However, personally I think it's not going to provide much value in this particular environment. See my barebones repository for how it's done.
- Backtraces?
	- Possible. I have read that when linking in libgcc you should have access to `__builtin_return_address (N > 0)`, which would make it possible to implement backtraces inside the environment. You have some limited access to backtraces from outside the machine, which automatically gets printed any time a CPU exception happens.
- Should I enable `-gc-sections`?
	- Probably not, it prevents some optimizations, and the size of the binaries matters much less than the locality of the code being executed. If you are good with hot/cold attributes and other things that make the code stay inside the same page as much as possible, you will not lose any performance.
- How do I check if a particular binary has a public function?
	- Use `riscv32-unknown-elf-readelf -a <mybinary>`. The long list of symbols at the end is the symbol table. Functions are FUNC unless there's a missing function then its UND (undefined), which could be the problem.
- Is it possible to strip these binaries so that only the public API is visible?
	- Enable the STRIP_SYMBOLS option, and make sure that your symbols file is up to date with your public functions.

