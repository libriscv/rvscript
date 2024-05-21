#include <script/script.hpp>
#include <strf/to_cfile.hpp>

void setup_dynamic_calls()
{
	Script::set_dynamic_call("empty",
		"void sys_empty ()", [](Script&) {});

	Script::set_dynamic_call("Test::void",
		[](Script&) {});

	Script::set_dynamic_call(
		"Test::my_dynamic_call",
		[](Script& script)
		{
			auto& args = script.dynargs();
			for (size_t i = 0; i < args.size(); i++)
			{
				if (args[i].type() == typeid(std::string))
				{
					strf::to(stdout)(
						"Argument ", i,
						" is a string: ", std::any_cast<std::string>(args[i]),
						"\n");
				}
				else if (args[i].type() == typeid(int64_t))
				{
					strf::to(stdout)(
						"Argument ", i,
						" is a 64-bit int: ", std::any_cast<int64_t>(args[i]),
						"\n");
				}
				else if (args[i].type() == typeid(float))
				{
					strf::to(stdout)(
						"Argument ", i,
						" is a 32-bit float: ", std::any_cast<float>(args[i]),
						"\n");
				}
				else if (args[i].type() == typeid(double))
				{
					strf::to(stdout)(
						"Argument ", i,
						" is a 64-bit float: ", std::any_cast<double>(args[i]),
						"\n");
				}
				else
				{
					strf::to(stdout)(
						"Argument ", i,
						" is of unknown type: ", args[i].type().name(), "\n");
				}
			}
		});

	Script::set_dynamic_call("test_array", "void sys_test_array (const TestData*)",
		[](Script& script) {
			typedef struct { int a, b, c; float x, y, z; } TestData;

			auto [array] = script.args<std::array<TestData, 64>>();

			if ((array[0].a != 1) ||
				(array[0].b != 2) ||
				(array[0].c != 3) ||
				(array[0].x != 4.0f) ||
				(array[0].y != 5.0f) ||
				(array[0].z != 6.0f))
			{
				throw std::runtime_error("Array test failed");
			}
		});

}
