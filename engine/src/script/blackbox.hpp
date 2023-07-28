#include <libriscv/machine.hpp>
#include <map>
#include <stdexcept>
#include <string_view>

template <int W> struct MachineData
{
	static inline const riscv::MachineOptions<W> options {
		.use_memory_arena = false,
		//.default_exit_function = "fast_exit"
		};

	MachineData(
		std::vector<uint8_t> b, const std::string& fn)
	  : binary {std::move(b)}, machine {binary, options}, filename {fn}
	{
	}

	MachineData(
		std::vector<uint8_t> b, const std::string& fn, bool)
	  : binary {std::move(b)}, machine {binary, options}, filename {fn}
	{
	}

	const std::vector<uint8_t> binary;
	const riscv::Machine<W> machine;
	const std::string filename;
};

template <int W> struct Blackbox
{
	void insert_binary(
		const std::string& name, const std::string& filename);

	void insert_embedded_binary(
		const std::string& name, const std::string& filename,
		const std::string_view binary);

	void insert_embedded_binary(
		const std::string& name, const std::string& filename,
		const char* bin_start, const char* bin_end);

	const auto& get(const std::string& name) const
	{
		auto it = m_data.find(name);
		if (it != m_data.end())
		{
			return it->second;
		}
		throw std::runtime_error("Could not find: " + name);
	}

	static std::vector<uint8_t> load_file(const std::string&);

  private:
	std::map<std::string, MachineData<W>> m_data;
};

template <int W>
inline void Blackbox<W>::insert_binary(
	const std::string& name, const std::string& binpath)
{
	const auto binary = load_file(binpath);
	// insert into map
	m_data.emplace(
		std::piecewise_construct, std::forward_as_tuple(name),
		std::forward_as_tuple(std::move(binary), binpath));
}

template <int W>
inline void Blackbox<W>::insert_embedded_binary(
	const std::string& name, const std::string& filename,
	const std::string_view binary)
{
	const std::vector<uint8_t> binvec {binary.begin(), binary.end()};
	// insert into map
	m_data.emplace(
		std::piecewise_construct, std::forward_as_tuple(name),
		std::forward_as_tuple(std::move(binvec), filename, true));
}

template <int W>
inline void Blackbox<W>::insert_embedded_binary(
	const std::string& name, const std::string& filename,
	const char* bin_start, const char* bin_end)
{
	this->insert_embedded_binary(
		name, filename, {bin_start, (size_t)(bin_end - bin_start)});
}

#include <unistd.h>

template <int W>
inline std::vector<uint8_t> Blackbox<W>::load_file(const std::string& filename)
{
	size_t size = 0;
	FILE* f		= fopen(filename.c_str(), "rb");
	if (f == NULL)
		throw std::runtime_error("Could not open file: " + filename);

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
