#include "engine.hpp"

void halt()
{
	asm (".insn i SYSTEM, 0, x0, x0, 0x7ff");
}

SharedMemoryArea::SharedMemoryArea()
	: m_shm {*(SharedMemoryArray *)(void*)SHM_BASE}
{}
void* SharedMemoryArea::push(const void* data, size_t size, size_t align)
{
	auto* dst = this->realign(size, align);
	return memcpy(dst, data, size);
}

#define STRINGIFY_HELPER(x) #x
#define STRINGIFY(x) STRINGIFY_HELPER(x)
asm(".section .text\n.align 8\n");

asm(".global sys_write\n"
"sys_write:\n"
"	li a7, " STRINGIFY(ECALL_WRITE) "\n"
"	ecall\n"
"   ret\n");

asm(".global farcall_helper\n"
"farcall_helper:\n"
"	li a7, " STRINGIFY(ECALL_FARCALL) "\n"
"	ecall\n"
"   ret\n");

asm(".global direct_farcall_helper\n"
"direct_farcall_helper:\n"
"	li a7, " STRINGIFY(ECALL_FARCALL_DIRECT) "\n"
"	ecall\n"
"   ret\n");

asm(".global sys_interrupt\n"
"sys_interrupt:\n"
"	li a7, " STRINGIFY(ECALL_INTERRUPT) "\n"
"	ecall\n"
"   ret\n"
""); // The system call handler must jump back to caller
