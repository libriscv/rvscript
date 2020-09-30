#include <libriscv/machine.hpp>
#include <map>
#include <string_view>

template <int W>
struct MachineData
{
	MachineData(std::vector<uint8_t> b, const std::string& syms)
		: binary{std::move(b)}, machine {binary}, sympath {syms}
	{}

	MachineData(std::vector<uint8_t> b, bool, std::string_view syms)
		: binary{std::move(b)}, machine {binary}, sympath {}, symbols(syms)
	{}

	const std::vector<uint8_t> binary;
	const riscv::Machine<W> machine;
	const std::string sympath;
	const std::string_view symbols;
};

template <int W>
struct Blackbox
{
	void insert_binary(const std::string& name,
		const std::string& binpath, const std::string& sympath);

	void insert_embedded_binary(const std::string& name,
		const std::string_view binary, const std::string_view symbols);

	void insert_embedded_binary(const std::string& name,
		const char* bin_start, const char* bin_end,
		const char* sym_start, const char* sym_end);

	const auto& get(const std::string& name) const {
		auto it = m_data.find(name);
		if (it != m_data.end()) {
			return it->second;
		}
		throw std::runtime_error("Could not find: " + name);
	}

	static std::vector<uint8_t> load_file(const std::string&);
private:
	std::map<std::string, MachineData<W>> m_data;
};

template <int W>
inline void Blackbox<W>::insert_binary(const std::string& name,
	const std::string& binpath, const std::string& sympath)
{
	const auto binary = load_file(binpath);
	try {
		// insert into map
		m_data.emplace(std::piecewise_construct,
			std::forward_as_tuple(name),
			std::forward_as_tuple(std::move(binary), sympath));
	} catch (std::exception& e) {
		printf(">>> Exception: %s\n", e.what());
		throw;
	}
}

template <int W>
inline void Blackbox<W>::insert_embedded_binary(const std::string& name,
	const std::string_view binary, const std::string_view symbols)
{
	const std::vector<uint8_t> binvec {binary.begin(), binary.end()};
	try {
		// insert into map
		m_data.emplace(std::piecewise_construct,
			std::forward_as_tuple(name),
			std::forward_as_tuple(std::move(binvec), true, symbols));
	} catch (std::exception& e) {
		printf(">>> Exception: %s\n", e.what());
		throw;
	}
}

template <int W>
inline void Blackbox<W>::insert_embedded_binary(
	const std::string& name,
	const char* bin_start, const char* bin_end,
	const char* sym_start, const char* sym_end)
{
	this->insert_embedded_binary(name,
		{bin_start, (size_t) (bin_end - bin_start)},
		{sym_start, (size_t) (sym_end - sym_start)});
}

#include <unistd.h>
template <int W>
inline std::vector<uint8_t> Blackbox<W>::load_file(const std::string& filename)
{
    size_t size = 0;
    FILE* f = fopen(filename.c_str(), "rb");
    if (f == NULL) throw std::runtime_error("Could not open file: " + filename);

    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);

    std::vector<uint8_t> result(size);
    if (size != fread(result.data(), 1, size, f))
    {
        fclose(f);
        throw std::runtime_error("Error when reading from file: " + filename);
    }
    fclose(f);
    return result;
}
