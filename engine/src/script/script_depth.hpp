#include <cstdint>

struct ScriptDepthMeter {
	ScriptDepthMeter(uint8_t& val) : m_val(++val) {}
	~ScriptDepthMeter() { m_val --; }

	uint8_t get() const noexcept { return m_val; }
	bool is_one() const noexcept { return m_val == 1; }

private:
	uint8_t& m_val;
};
