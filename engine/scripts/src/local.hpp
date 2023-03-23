#include <functional>

struct Local {
	void local_cpp_function();
};

extern void local_function();
extern Local local_struct;
extern void local_function_ptr(void(*func)());
extern void local_std_function(std::function<void()> callback);
