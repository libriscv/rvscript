#pragma once
#include <libriscv/util/crc32.hpp>
#include <script/script.hpp>

struct Scripts
{
	static Script& create(
		const std::string& name, const std::string& blackbox_name,
		bool debug = false);

	static Script& get(uint32_t machine_hash, const char*);

	/* Used by system calls to access other machines. */
	static Script* get(uint32_t machine_hash);

	static void load_binary(
		const std::string& name, const std::string& filename);

	static void embedded_binary(
		const std::string& name, const std::string& filename,
		std::string_view binary);
};

#define SCRIPT(x) Scripts::get(riscv::crc32(x), x)
