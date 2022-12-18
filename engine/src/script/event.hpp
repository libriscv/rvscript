#pragma once
#include "script.hpp"

struct Event
{
	Event() = default;
	Event(Script&, const std::string& func);

	template <typename... Args> long call(Args&&... args);

	bool is_callable() const noexcept
	{
		return m_script != nullptr && m_addr != 0x0;
	}

	auto& script() noexcept
	{
		assert(m_script != nullptr);
		return *m_script;
	}

	const auto& script() const noexcept
	{
		assert(m_script != nullptr);
		return *m_script;
	}

	auto address() const noexcept
	{
		return m_addr;
	}

	// Turn address into function name (as long as it's visible)
	auto function() const
	{
		return script().symbol_name(address());
	}

  private:
	Script* m_script	   = nullptr;
	Script::gaddr_t m_addr = 0;
};

inline Event::Event(Script& script, const std::string& func)
  : m_script(&script), m_addr(script.address_of(func))
{
}

template <typename... Args> inline long Event::call(Args&&... args)
{
	if (is_callable())
	{
		return script().call(address(), std::forward<Args>(args)...);
	}
	return -1;
}
