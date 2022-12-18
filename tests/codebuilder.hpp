#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <script/script.hpp>

extern riscv::Machine<riscv::RISCV64> build_and_load(
    const std::string& code,
    const std::string& args = "-O2 -static");
