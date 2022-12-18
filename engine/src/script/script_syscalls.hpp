#include "script.hpp"

using gaddr_t	= Script::gaddr_t;
using machine_t = riscv::Machine<Script::MARCH>;

struct EphemeralAlloc
{
	EphemeralAlloc(Script& s, size_t len)
	  : script(s), addr(s.guest_alloc(len)), size(len)
	{
	}

	~EphemeralAlloc()
	{
		this->script.guest_free(this->addr);
	}

	Script& script;
	const gaddr_t addr;
	const size_t size;
};

#include "../../api/api_structs.h"
#include "../../api/syscalls.h"
