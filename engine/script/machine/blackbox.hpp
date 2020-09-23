#include <libriscv/machine.hpp>
#include <map>

template <int W>
struct MachineData
{
	MachineData(std::vector<uint8_t> b)
		: binary{std::move(b)}, machine {binary}
	{}

	const std::vector<uint8_t> binary;
	const riscv::Machine<W> machine;
};

template <int W>
struct Blackbox
{
	void insert_binary(const std::string& name, const std::string& binpath);

	const auto* get(const std::string& name) const {
		auto it = m_data.find(name);
		if (it != m_data.end()) {
			return &it->second.machine;
		}
		return (const riscv::Machine<W>*) nullptr;
	}

private:
	std::map<std::string, MachineData<W>> m_data;
	std::vector<uint8_t> load_file(const std::string& filename);
};

template <int W>
inline void Blackbox<W>::insert_binary(
	const std::string& name, const std::string& binpath)
{
	const auto binary = load_file(binpath);
	try {
		// insert into map
		m_data.emplace(std::piecewise_construct,
			std::forward_as_tuple(name),
			std::forward_as_tuple(std::move(binary)));
	} catch (std::exception& e) {
		printf(">>> Exception: %s\n", e.what());
		throw;
	}
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
