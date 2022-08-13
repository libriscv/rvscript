#include <stddef.h>
#include <stdint.h>

extern void console_print(const char*, size_t);
extern void breakpoint(const char*);
extern void exit(int) __attribute__((noreturn));
extern void sys_measure(const char*, void(*)());
