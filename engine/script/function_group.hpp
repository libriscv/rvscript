#pragma once
#include <libriscv/machine.hpp>
class Script;

/**
 *  Function groups allow you to allocate a group
 *  to a single purpose, hand it to a subsystem,
 *  and let that subsystem allocate its own system
 *  calls at the same speed as regular handlers.
 *
 *  The primary purpose is to set aside unique group
 *  ids to each subsystem that needs to add handlers.
 *  The guest will be able to call functions in each
 *  group given by an index, so, example:
 *
 *  Subsystem: "Some custom allocator"
 *  Group ID: 44 (It doesn't matter, it's all dynamic)
 *  Index 0: Allocate
 *  Index 1: Reallocate
 *  Index 2: Deallocate
 *
 *  Let's install the deallocate handler:
 *
 *  script.set_dynamic_function(44, 2,
 *     [] (auto& machine) {
 *        printf("Deallocating in ... 3.. 2.. 1..\n");
 *        exit(1);
 *     }
 *
 *  This function will be called from a dynamically
 *  allocated system call, which is written into the
 *  guests memory.
 *  Using the information above, the guest can call
 *  this function group like this:
 *
 *  void custom_deallocate(void* p) {
 *      api::groupcall<44> (2, p);
 *  }
 *
 *  When the function is called, a system call handler is
 *  invoked on the host, which calls the given function.
 *
 *  There are thousands of groups, which are automatically freed
 *  on group deletion, so everything is dynamically managed.
 *
 *  There is extra cost due to the abstraction
 *  of having a std::function wrapper, but it
 *  allows capturing arbitrary things into the handler.
 *  Inside the guest there is extra cost in having to
 *  do a function call (which clobbers memory), although
 *  it should be negligible.
 *  This works by writing instructions into a custom area
 *  in guest memory, which loads the correct system call number
 *  and then invokes a system call.
**/

struct FunctionGroup
{
	using ghandler_t = std::function<void(Script&)>;
	static constexpr size_t GROUP_SIZE = 64; // functions in group
	static constexpr size_t GROUP_BYTES = GROUP_SIZE * 8; // 2 instr

	// installs the 'idx' entry for this group
	void install(unsigned idx, ghandler_t);
	// returns true if all set bit-indices have a corresponding handler
	bool check(uint64_t bits) const;

	// base address in guest memory
	static uint64_t group_area() noexcept;
	// calculate group/index from PC
	static size_t calculate_group(uint64_t) noexcept;
	static size_t calculate_group_index(uint64_t) noexcept;

	FunctionGroup(int gid, Script& s);
	~FunctionGroup();

private:
	using datatype_t = std::array<uint32_t, GROUP_BYTES>;
	datatype_t&  create_dataref(int gid, Script&);
	int  request_number();
	void free_number(int);

	Script&     m_script;
	datatype_t& m_data;
	int         m_sysno;
	eastl::vector<ghandler_t> m_syscall_handlers;
};
