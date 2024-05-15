#include "script.hpp"

// Per-thread map of forked scripts
thread_local std::unordered_map<uint32_t, Script> forks;

Script& Script::create_fork()
{
	auto it = forks.find(this->m_hash);
	// Create thread-local fork on-demand
	if (UNLIKELY(it == forks.end()))
	{
		auto fit = forks.emplace(std::piecewise_construct,
			std::forward_as_tuple(this->m_hash),
			std::forward_as_tuple(this->m_binary, this->m_name, this->m_filename, this->m_is_debug, this->m_userptr));
		return fit.first->second;
	}
	// Return forked program
	return it->second;
}

Script& Script::get_fork()
{
	thread_local Script* last_fork = nullptr;

	if (last_fork != nullptr && last_fork->hash() == this->hash())
		return *last_fork;

	last_fork = &this->create_fork();
	return *last_fork;
}

