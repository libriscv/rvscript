#include <api.h>
#define USE_NEWLIB_REENT
#ifdef USE_NEWLIB_REENT
static struct _reent reent;
struct _reent* _impure_ptr = &reent;
#endif

asm(".global _exit\n"
	"_exit:\n"
	"\tebreak;\n"
	"\tj _exit;\n");

extern "C" {
	__attribute__((noreturn))
	void _exit(int);
	void __register_frame(const void*);
}

static void
init_stdlib(void* eh_frame [[maybe_unused]])
{
#ifdef USE_NEWLIB_REENT
	_REENT_INIT_PTR_ZEROED(_impure_ptr);
#endif

#ifdef __EXCEPTIONS
	__register_frame(eh_frame);
#endif

	// call global C/C++ constructors
	extern void(*__preinit_array_start [])();
	extern void(*__preinit_array_end [])();
	const int pcount = __preinit_array_end - __preinit_array_start;
	for (int i = 0; i < pcount; i++) {
		__preinit_array_start[i]();
	}

	extern void(*__init_array_start [])();
	extern void(*__init_array_end [])();
	const int count = __init_array_end - __init_array_start;
	for (int i = 0; i < count; i++) {
		__init_array_start[i]();
	}

}

extern "C" __attribute__((visibility("hidden"), used))
void libc_start(MapFile* map_file, void* eh_frame)
{
	// 1. zero-initialize .bss section
	/*
	extern char __bss_start;
	extern char __BSS_END__;
#ifdef __clang__
	extern char _end;
	memset(&__bss_start, 0, &_end - &__bss_start);
#else
	memset(&__bss_start, 0, &__BSS_END__ - &__bss_start);
#endif
	asm("" ::: "memory");
	*/

	init_stdlib(eh_frame);

	// call map_main(MapFile*), but only if map file provided :)
	if (map_file) map_main(map_file);
	while (true) _exit(0);
}

__attribute__((weak))
void map_main(MapFile*) { }

// initialize GP to __global_pointer
// NOTE: have to disable relaxing first
asm(".global _start             \t\n\
_start:                         \t\n\
     .option push 				\t\n\
	 .option norelax 			\t\n\
	 1:auipc gp, %pcrel_hi(__global_pointer$) \t\n\
	 addi  gp, gp, %pcrel_lo(1b) \t\n\
	.option pop					\t\n\
	call libc_start				\t\n\
");
