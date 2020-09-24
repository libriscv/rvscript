#pragma once
#include "script.hpp"

struct Event
{
	Event() = default;
	Event(Script&, const std::string& func);

	template <typename... Args>
	long call(Args&&... args);

	auto& script() const noexcept { return *m_script; }
	auto address() const noexcept { return m_addr; }

	Script* m_script = nullptr;
	Script::gaddr_t m_addr = 0;
};

inline Event::Event(Script& script, const std::string& func)
	: m_script(&script), m_addr(script.resolve_address(func))  {}

template <typename... Args>
inline long Event::call(Args&&... args)
{
	assert(m_script != nullptr && "Script was null");
	if (address() != 0x0) {
		return script().call(address(), std::forward<Args>(args)...);
	}
	return -1;
}
