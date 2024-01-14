#pragma once
#include "script.hpp"

/// @brief Create a wrapper for a function call, matching the
/// function type F into a given Script instance, for the given function.
/// @tparam F A function type, eg. void(int)
template <typename F>
struct Event
{
	Event() = default;
	Event(Script&, const std::string& func);
	Event(Script&, Script::gaddr_t address);

	template <typename... Args> auto call(Args&&... args);

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

template <typename F>
inline Event<F>::Event(Script& script, const std::string& func)
  : m_script(&script), m_addr(script.address_of(func))
{
}

template <typename F>
inline Event<F>::Event(Script& script, Script::gaddr_t address)
  : m_script(&script), m_addr(address)
{
}

template <typename F>
template <typename... Args> inline auto Event<F>::call(Args&&... args)
{
	static_assert(std::is_invocable_v<F, Args...>);
	using Ret = decltype((F*){}(args...));

	if (is_callable())
	{
		script().call(address(), std::forward<Args>(args)...);
		if constexpr (std::is_same_v<void, Ret>)
			return;
		else
			return script().machine().template return_value<Ret>();
	}
	if constexpr (!std::is_same_v<void, Ret>)
		return Ret(-1);
}
