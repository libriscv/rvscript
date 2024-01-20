#include "interface.hpp"
#include <api.h>
using namespace api;

struct SomeStruct
{
	std::string string;
	int value;
};

KEEP() Gameplay gameplay_state;

KEEP() void Gameplay::set_action(bool a)
{
	print("Setting action to ", a, "\n");
	this->action = a;
}

KEEP() bool Gameplay::get_action()
{
	return this->action;
}

KEEP() void Gameplay::set_string(unsigned key, const std::string& val)
{
	this->strings[key] = val;
}

KEEP() void Gameplay::print_string(unsigned key)
{
	print("Key ", key, " has value: ", this->strings[key], "\n");
}

KEEP() void Gameplay::do_nothing() {
	return_fast();
}

/* We can only call this function remotely if it's added to "gameplay.symbols",
   because the symbol files are used in the build system to preserve certain
   functions that would ordinarily get optimized out. It's name also has to
   unmangled, otherwise we can't find it in the ELF string tables. */
extern KEEP() long gameplay_function(float value, SomeStruct& some)
{
	print("Hello Remote World! value = ", value, "!\n");
	print("Some struct string: ", some.string, "\n");
	print("Some struct value: ", some.value, "\n");
	some.string = "Hello 456! This string is very long!";
	return value * 2;
}

KEEP() int gameplay_allowed_function(int value)
{
	print("Hello from the Gameplay machine. Value = ", value, "\n");
	return value * 2;
}

extern KEEP() void gameplay_empty()
{
	// Do nothing
	return_fast();
}

extern KEEP() void gameplay_arg1(int) {return_fast();}
extern KEEP() void gameplay_arg2(int, int) {return_fast();}
extern KEEP() void gameplay_arg3(int, int, int) {return_fast();}
extern KEEP() void gameplay_arg4(int, int, int, int) {return_fast();}
extern KEEP() void gameplay_arg5(int, int, int, int, int) {return_fast();}
extern KEEP() void gameplay_arg6(int, int, int, int, int, int) {return_fast();}
extern KEEP() void gameplay_arg7(int, int, int, int, int, int, int) {return_fast();}
extern KEEP() void gameplay_arg8(int, int, int, int, int, int, int, int) {return_fast();}

extern KEEP() void gameplay_exec(const Function<void()>& func)
{
	func();
	return_fast();
}

extern KEEP() void gameplay_exec_ptr(void (*func)())
{
	func();
	return_fast();
}

asm(".global sys_rpc_recv\n"
"sys_rpc_recv:\n"
"	li a7, 507\n"
"	ecall\n"
"   ret\n");
extern "C" ssize_t sys_rpc_recv(void *, size_t);

extern "C" KEEP() void rpc_waitloop()
{
	std::array<char, 16384> buffer;
	while (true) {
		sys_rpc_recv(buffer.data(), buffer.size());
	}
}

extern KEEP() void gameplay_rpc(ssize_t len)
{
	std::array<char, 16384> buffer;
	sys_rpc_recv(buffer.data(), buffer.size());
}

extern KEEP() void gameplay_data(const void *, size_t)
{

}
