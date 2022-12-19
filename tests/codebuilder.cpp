#include <stdexcept>
#include <cstdio>
#include <cstring>
#include <vector>
#include <unistd.h>
#include <libriscv/machine.hpp>
#include "crc32.hpp"
static constexpr bool VERBOSE_COMPILER = true;

std::string env_with_default(const char* var, const std::string& defval) {
	std::string value = defval;
	if (const char* envval = getenv(var); envval) value = std::string(envval);
	return value;
}

struct TemporaryFile
{
	TemporaryFile(std::string contents)
	{
		// Create temporary filenames for code and binary
		this->filename = "/tmp/builder-XXXXXX";
		// Open temporary code file with owner privs
		this->fd = mkstemp(&filename[0]);
		if (this->fd < 0)
		{
			throw std::runtime_error(
				"Unable to create temporary file: " + this->filename);
		}

		// Write code to temp code file
		const ssize_t len = write(this->fd, contents.c_str(), contents.size());
		if (len < (ssize_t)contents.size())
		{
			throw std::runtime_error("Unable to write to temporary file");
		}
	}
	~TemporaryFile()
	{
		if (fd > 0)
			unlink(this->filename.c_str());
	}

	int fd;
	std::string filename;
};

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
	TemporaryFile code_tmpfile { code };
	// Compile code to binary file
	char bin_filename[256];
	const uint32_t code_checksum = crc32((const uint8_t *)code.c_str(), code.size());
	const uint32_t final_checksum = crc32(code_checksum, (const uint8_t *)args.c_str(), args.size());
	(void)snprintf(bin_filename, sizeof(bin_filename),
		"/tmp/binary-%08X", final_checksum);

	char current_dir[256];
	getcwd(current_dir, sizeof(current_dir));

	char command[1024];
	snprintf(command, sizeof(command),
		"exec ./codebuilder.sh \"%s/..\" %s %s",
		current_dir,
		code_tmpfile.filename.c_str(),
		bin_filename);

	if constexpr (VERBOSE_COMPILER) {
		printf("Command: %s\n", command);
	}
	// Compile program
	FILE* f = popen(command, "r");
	if (f == nullptr) {
		throw std::runtime_error("Unable to compile code");
	}
	ssize_t r = 0;
	do {
		r = fread(command, 1, sizeof(command), f);
		if (r > 0)
		{
			printf("Compilation:\n%.*s\n", (int)r, command);
		}
	} while (r > 0);
	pclose(f);

	binaries.push_back(load_file(bin_filename));
	unlink(bin_filename);

	riscv::Machine<riscv::RISCV64> machine { binaries.back() };
	riscv::Machine<riscv::RISCV64>::setup_newlib_syscalls();

	return machine;
}
