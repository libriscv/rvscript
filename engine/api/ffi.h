#include <api.h>

inline void sys_print(const char* data, size_t len)
{
	psyscall(ECALL_WRITE, data, len);
}
