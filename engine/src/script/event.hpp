#pragma once
#include "script.hpp"

enum EventUsagePattern : int {
	SharedScript = 0,
	PerThread    = 1,
};

/// @brief Create a wrapper for a function call, matching the
/// function type F into a given Script instance, for the given function.
/// @tparam Usage The usage pattern of the event, either SharedScript or PerThread
/// @tparam F A function type, eg. void(int)
template <typename F, EventUsagePattern Usage = PerThread>
struct Event
{
	Event(Script&, const std::string& func);
	Event(Script&, Script::gaddr_t address);

	/// @brief Call the function with the given arguments
	/// @tparam Args The event function argument types
	/// @param args The event function arguments
	/// @return The std::optional return value of the function, unless void
	template <typename... Args>
	auto call(Args&&... args);

	auto& script() noexcept
	{
		auto* script_ptr = m_pcall.machine().template get_userdata<Script>();
		if constexpr (Usage == EventUsagePattern::PerThread)
			return script_ptr->get_fork();
		else
			return *script_ptr;
	}

	const auto& script() const noexcept
	{
		auto* script_ptr = m_pcall.machine().template get_userdata<Script>();
		if constexpr (Usage == EventUsagePattern::PerThread)
			return script_ptr->get_fork();
		else
			return *script_ptr;
	}

	auto address() const noexcept
	{
		return m_pcall.address();
	}

	// Turn address into function name (as long as it's visible)
	auto function() const
	{
		return script().symbol_name(address());
	}

  private:
    riscv::PreparedCall<Script::MARCH, F, Script::MAX_CALL_INSTR> m_pcall;
};

template <typename F, EventUsagePattern Usage>
inline Event<F, Usage>::Event(Script& script, Script::gaddr_t address)
  : m_pcall((Usage == SharedScript) ? script.machine() : script.get_fork().machine(), address)
{
}

template <typename F, EventUsagePattern Usage>
inline Event<F, Usage>::Event(Script& script, const std::string& func)
  : Event(script, script.address_of(func))
{
	if (address() == 0x0)
		throw std::runtime_error("Function not found: " + func);
}

template <typename F, EventUsagePattern Usage>
template <typename... Args> inline auto Event<F, Usage>::call(Args&&... args)
{
	static_assert(std::is_invocable_v<F, Args...>);
	using Ret = decltype((F*){}(args...));

	auto& script = this->script();
	if (auto res = script.template prepared_call<F, Args&&...>(m_pcall, std::forward<Args>(args)...)) {
		if constexpr (std::is_same_v<void, Ret>)
			return true;
		else if constexpr (std::is_same_v<Script::gaddr_t, Ret>)
			return res;
		else
			return std::optional<Ret> (res.value());
	}
	if constexpr (std::is_same_v<void, Ret>)
		return false;
	else
		return std::optional<Ret>{std::nullopt};
}
