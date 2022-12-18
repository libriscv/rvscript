#pragma once
#include <array>
#include <string>
#include <string_view>

template <size_t MaxLength = 64>
struct EmbeddedString
{
	uint16_t length = 0;
	std::array<char, MaxLength> data;

	size_t size() const noexcept { return this->length; }

	auto& operator = (std::string_view newval)
	{
		if (newval.size() <= MaxLength) {
			std::memcpy(data.begin(), newval.begin(), newval.size());
			this->length = newval.size();
		} else throw std::out_of_range("Value for embedded string too long to fit");
		return *this;
	}

	std::string_view view() const noexcept { return {data.begin(), this->length}; }
	std::string to_string() const noexcept { return {data.begin(), data.begin() + length}; }

	EmbeddedString() = default;
	EmbeddedString(std::string_view initval)
	{
		this->operator=(initval);
	}
};
