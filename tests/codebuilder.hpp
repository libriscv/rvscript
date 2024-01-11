#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <script/script.hpp>
#include <memory>

extern std::shared_ptr<std::vector<uint8_t>> build_and_load(
    const std::string& code,
    const std::string& args = "-O2 -static");
