#pragma once
#include <include/libc.hpp>
#include <cassert>
#include <string>
#include <type_traits>
#define SHM_GCC_BUGFIX() __attribute__ ((noinline))

struct SharedMemoryArea
{
	SHM_GCC_BUGFIX()
	auto* operator() (const std::string& str) {
		return (const char*) push(str.c_str(), str.size()+1, 1);
	}
	SHM_GCC_BUGFIX()
	auto* operator() (const char* str) {
		const size_t len = __builtin_strlen(str)+1;
		return (const char*) push(str, len, 1);
	}
	// Non-const struct
	template <typename T> SHM_GCC_BUGFIX()
	auto& operator() (const T& data) {
		static_assert(std::is_standard_layout_v<T>, "Must be a POD type");
		static_assert(!std::is_pointer_v<T>, "Should not be a pointer");
		return *(T*) push(&data, sizeof(T), alignof(T));
	}

	void* push(const void* data, size_t size, size_t align);

	// An 8kb memory area shared between all machines
	using SharedMemoryArray = std::array<char, 8192>;
	SharedMemoryArea();
private:
	void* realign(size_t size, size_t align) {
		m_current = (char*)(((uintptr_t)m_current - size) & -align);
		assert(m_current >= m_shm.begin());
		return m_current;
	}
	static constexpr uintptr_t ALIGN = 8;
	static constexpr uintptr_t SHM_BASE = 0x2000;
	// NOTE: This is a stack that goes upwards because GCC
	// failed to detect memcpy boundries, and complained a lot.
	SharedMemoryArray& m_shm;
	char* m_current = &m_shm[m_shm.size()];
};
