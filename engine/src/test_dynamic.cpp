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
}
