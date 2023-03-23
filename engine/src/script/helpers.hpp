#pragma once
#include "script.hpp"
#include <array>
#include <stdexcept>

struct CaptureStorage
{
	using gaddr_t						= Script::gaddr_t;
	static constexpr size_t CaptureSize = 32;

	static std::array<uint8_t, CaptureSize>
	get(Script::machine_t& machine, gaddr_t data, gaddr_t size)
	{
		std::array<uint8_t, CaptureSize> capture;
		if (UNLIKELY(size > sizeof(capture)))
		{
			throw std::runtime_error(
				"Capture storage data must be 32-bytes or less");
		}
		machine.memory.memcpy_out(capture.data(), data, size);
		return capture;
	}
};
