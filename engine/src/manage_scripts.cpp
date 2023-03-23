#include "manage_scripts.hpp"
#include <script/blackbox.hpp>
Blackbox<Script::MARCH> blackbox;

/* List of initialized machines ready to go. The Script class
   is a fully initialized machine with a hashed public API and
   a lot of helper functions to simplify usage. */
static std::map<uint32_t, Script> scripts;

Script&
Scripts::create(const std::string& name, const std::string& bbname, bool debug)
{
	const auto& box = blackbox.get(bbname);
	/* This will fork the original machine using copy-on-write
	   and memory sharing mechanics to save memory. */
	auto it = scripts.emplace(
		std::piecewise_construct,
		std::forward_as_tuple(riscv::crc32(name.c_str())),
		std::forward_as_tuple(
			box.machine, nullptr, name, box.filename, debug));

	auto& script = it.first->second;
	script.initialize();
	return script;
}

/* Retrieve machines based on name (hashed), used by system calls.
   With this all our APIs use the same source for machines. */
Script* Scripts::get(uint32_t machine_hash)
{
	auto it = scripts.find(machine_hash);
	if (it != scripts.end())
	{
		return &it->second;
	}
	return nullptr;
}

/* Retrieve machines based on hash and name, throws on failure */
Script& Scripts::get(uint32_t machine_hash, const char* name)
{
	auto it = scripts.find(machine_hash);
	if (it != scripts.end())
	{
		return it->second;
	}
	throw std::runtime_error("Unable to find machine: " + std::string(name));
}

void Scripts::load_binary(
	const std::string& name, const std::string& filename)
{
	blackbox.insert_binary(name, filename);
}

void Scripts::embedded_binary(
	const std::string& name, const std::string& origin_filename, std::string_view binary)
{
	blackbox.insert_embedded_binary(name, origin_filename, binary);
}
