#pragma once
#include <script/script.hpp>
#include <libriscv/util/crc32.hpp>

struct Scripts {
	static Script& create(const std::string& name,
		const std::string& blackbox_name, bool debug = false);

	static Script* get(uint32_t machine_hash);
	static Script& get(uint32_t machine_hash, const char*);

	static void load_binary(const std::string& name,
		const std::string& filename, const std::string& symbols);

	static void embedded_binary(const std::string& name,
		std::string_view binary, std::string_view symbols);
};


#define SCRIPT(x) Scripts::get(riscv::crc32(#x), #x)
