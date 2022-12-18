#include <stdexcept>
#include <cstdio>
#include <cstring>
#include <vector>
#include <unistd.h>
#include <libriscv/machine.hpp>
#include "crc32.hpp"
static constexpr bool VERBOSE_COMPILER = true;
static const std::string DEFAULT_CXX_COMPILER = "riscv64-unknown-elf-g++";

std::string cpp_compile_command(const std::string& cxx,
	const std::string& args, const std::string& outfile)
{
	return cxx + " -std=c++17 -x c++ -o " + outfile + " " + args;
}
std::string env_with_default(const char* var, const std::string& defval) {
	std::string value = defval;
	if (const char* envval = getenv(var); envval) value = std::string(envval);
	return value;
}

std::vector<uint8_t> load_file(const std::string& filename)
{
	size_t size = 0;
	FILE* f = fopen(filename.c_str(), "rb");
	if (f == NULL) throw std::runtime_error("Could not open file: " + filename);

	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);

	std::vector<uint8_t> result(size);
	if (size != fread(result.data(), 1, size, f))
	{
		fclose(f);
		throw std::runtime_error("Error when reading from file: " + filename);
	}
	fclose(f);
	return result;
}

std::vector<std::vector<uint8_t>> binaries;

riscv::Machine<riscv::RISCV64>
	build_and_load(const std::string& code, const std::string& args)
{
	// Create temporary filenames for code and binary
	char code_filename[64];
	strncpy(code_filename, "/tmp/builder-XXXXXX", sizeof(code_filename));
	// Open temporary code file with owner privs
	const int code_fd = mkstemp(code_filename);
	if (code_fd < 0) {
		throw std::runtime_error(
			"Unable to create temporary file for code: " + std::string(code_filename));
	}
	// Write code to temp code file
	const ssize_t code_len = write(code_fd, code.c_str(), code.size());
	if (code_len < (ssize_t) code.size()) {
		unlink(code_filename);
		throw std::runtime_error("Unable to write to temporary file");
	}
	// Compile code to binary file
	char bin_filename[256];
	const uint32_t code_checksum = crc32((const uint8_t *)code.c_str(), code.size());
	const uint32_t final_checksum = crc32(code_checksum, (const uint8_t *)args.c_str(), args.size());
	(void)snprintf(bin_filename, sizeof(bin_filename),
		"/tmp/binary-%08X", final_checksum);

	auto cxx = env_with_default("RCXX", DEFAULT_CXX_COMPILER);
	std::string command = cpp_compile_command(cxx,
		args + " " + std::string(code_filename), bin_filename);
	if constexpr (VERBOSE_COMPILER) {
		printf("Command: %s\n", command.c_str());
	}
	// Compile program
	FILE* f = popen(command.c_str(), "r");
	if (f == nullptr) {
		unlink(code_filename);
		throw std::runtime_error("Unable to compile code");
	}
	pclose(f);
	unlink(code_filename);

	binaries.push_back(load_file(bin_filename));

	riscv::Machine<riscv::RISCV64> machine { binaries.back() };
	riscv::Machine<riscv::RISCV64>::setup_newlib_syscalls();

	return machine;
}
