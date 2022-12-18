#pragma once
#include <include/function.hpp>
#include <map>
#include <string>
#include <vector>

struct Gameplay
{
	bool action = false;

	std::map<unsigned, std::string> strings;
	std::vector<Function<void()>> functions;

	void set_action(bool a);
	bool get_action();

	void set_string(unsigned, const std::string&);
	void print_string(unsigned);

	void do_nothing();
};

extern Gameplay gameplay_state;

extern void gameplay_exec(const Function<void()>& func);
extern void gameplay_exec_ptr(void (*func)());
