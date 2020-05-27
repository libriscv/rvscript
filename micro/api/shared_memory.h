#pragma once
#include <include/libc.hpp>
#include <string>
#include <type_traits>

struct SharedMemoryArea
{
	auto* push(const std::string& str) {
		dst -= str.size()+1;
		dst &= ALIGN_MASK; // maintain alignment
		memcpy((void*) dst, str.c_str(), str.size()+1);
		return (const char*) dst;
	}
	auto* push(const char* str) {
		const size_t len = __builtin_strlen(str)+1;
		dst = (dst - len) & ALIGN_MASK;
		memcpy((void*) dst, str, len);
		return (const char*) dst;
	}
	template <typename T>
	auto* push(const T& data) {
		static_assert(std::is_standard_layout_v<T>, "Must be a POD type");
		static_assert(!std::is_pointer_v<T>, "Should not be a pointer");
		dst = (dst - sizeof(T)) & ALIGN_MASK;
		memcpy((void*) dst, &data, sizeof(T));
		return (const T*) dst;
	}

private:
	static constexpr uint32_t ALIGN_MASK = ~0x3;
	uint32_t dst = 0x4000;
};
