#pragma once
#include <cstdint>
#include <string>

enum class ArgType : int {
	INT64,
	FLOAT32,
	STRING
};

struct ScriptArg
{
	bool is_int64() const noexcept { return type == ArgType::INT64; }
	bool is_float32() const noexcept { return type == ArgType::FLOAT32; }
	bool is_string() const noexcept { return type == ArgType::STRING; }

	ScriptArg(int64_t value) {
		this->i64 = value;
		this->type = ArgType::INT64;
	}
	ScriptArg(float value) {
		this->f32 = value;
		this->type = ArgType::FLOAT32;
	}
	ScriptArg(std::string value) {
		this->string = std::move(value);
		this->type = ArgType::STRING;
	}
	~ScriptArg();

	union {
		int64_t     i64;
		float       f32;
		std::string string;
	};
	enum ArgType type;
};

inline ScriptArg::~ScriptArg()
{
	switch (type) {
		case ArgType::STRING:
			string.~basic_string();
			break;
		default:
			break;
	}
}
