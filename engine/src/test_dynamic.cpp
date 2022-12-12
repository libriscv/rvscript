#include <script/script.hpp>
#include <fmt/core.h>

void setup_dynamic_calls()
{
    Script::set_dynamic_call(
    "Test::void", [] (Script&) {});

	Script::set_dynamic_call(
    "Test::my_dynamic_call", [] (Script& script)
    {
        auto& args = script.dynargs();
        for (size_t i = 0; i < args.size(); i++)
        {
            if (args[i].type() == typeid(std::string))
            {
                fmt::print("Argument {} is a string: {}\n",
                        i, std::any_cast<std::string>(args[i]));
            }
            else if (args[i].type() == typeid(int64_t))
            {
                fmt::print("Argument {} is a 64-bit int: {}\n",
                        i, std::any_cast<int64_t>(args[i]));
            }
            else if (args[i].type() == typeid(float))
            {
                fmt::print("Argument {} is a 32-bit float: {}\n",
                        i, std::any_cast<float>(args[i]));
            }
            else if (args[i].type() == typeid(double))
            {
                fmt::print("Argument {} is a 64-bit float: {}\n",
                        i, std::any_cast<double>(args[i]));
            }
            else
            {
                fmt::print("Argument {} is unknown type: {}\n",
                        i, args[i].type().name());
            }
        }
    });
}
